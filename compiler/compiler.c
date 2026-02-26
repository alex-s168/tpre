#include <assert.h>
#include <string.h>
#include "../shared.h"
#include "tpre_common.h"

#define USING_TPREC
#include "parser.h"
#include "utils.h"

/** negative on failure */
int tpre_find_group(tpre_re_t const* re, char const* name)
{
  for (tpre_groupid_t i = 0; i < re->num_named_groups; i++)
    if (!strcmp(re->named_groups[i], name))
      return (int) (re->num_named_groups + i);

  return -1;
}

typedef struct
{
  Node** items;
  size_t len;
} NodeLi;

static void NodeLi_add(NodeLi* li, Node* nd)
{
  li->items = realloc(li->items, sizeof(Node*) * (li->len + 1));
  li->items[li->len++] = nd;
}

static void or_cases(Node* or, NodeLi* out)
{
  if (!or || or->kind != NodeOr)
    return;
  if (or->or.a->kind != NodeOr)
    NodeLi_add(out, or->or.a);
  else
    or_cases(or->or.a, out);
  if (or->or.b->kind != NodeOr)
    NodeLi_add(out, or->or.b);
  else
    or_cases(or->or.b, out);
}

static Node* find_trough_rep(Node* node, NodeKind what)
{
  if (node->kind == what)
    return node;

  switch (node->kind)
  {
    case NodeLazyRepeatLeast0:
    case NodeLazyRepeatLeast1:
    case NodeGreedyRepeatLeast0:
    case NodeGreedyRepeatLeast1:
      return find_trough_rep(node->repeat, what);

    default: return NULL;
  }
}

static Node* last_left_chain(Node* node)
{
  if (node->kind == NodeOr)
    return last_left_chain(node->or.a);
  if (node->kind != NodeChain)
    return NULL;
  if (node->chain.a->kind == NodeChain)
    return last_left_chain(node->chain.a);
  return node;
}

static int verify(Node* nd, tpre_errs_t* errs)
{
  if (nd == NULL)
    return 1;
  Node* children[2];
  Node_children(nd, children);
  verify(children[0], errs);
  verify(children[1], errs);

  return 0;
}

static size_t count_groups(Node* nd)
{
  if (nd == NULL)
    return 0;

  Node* children[2];
  Node_children(nd, children);

  size_t count = 0;

  if (nd->kind == NodeCaptureGroup)
    count += 1;

  count += count_groups(children[0]);
  count += count_groups(children[1]);

  return count;
}

static size_t count_named_groups(Node* nd)
{
  if (nd == NULL)
    return 0;

  Node* children[2];
  Node_children(nd, children);

  size_t count = 0;

  if (nd->kind == NodeNamedCaptureGroup)
    count += 1;

  count += count_named_groups(children[0]);
  count += count_named_groups(children[1]);

  return count;
}

static void named_groups(Node* nd, char*** out)
{
  if (nd == NULL)
    return;
  Node* children[2];
  Node_children(nd, children);

  if (nd->kind == NodeNamedCaptureGroup)
  {
    **out = tprec_strdup(nd->named_capture.name);
    (*out)++;
  }

  named_groups(children[0], out);
  named_groups(children[1], out);
}

static void groups(
    Node* nd,
    tpre_groupid_t group,
    tpre_groupid_t* global_next_group_id,
    tpre_groupid_t* next_named_gr)
{
  if (nd == NULL)
    return;
  Node* children[2];
  Node_children(nd, children);

  if (nd->kind == NodeJustGroup)
  {
    memcpy(nd, nd->just_group, sizeof(Node));
    groups(nd, group, global_next_group_id, next_named_gr);
    return;
  }

  if (nd->kind == NodeCaptureGroup)
  {
    memcpy(nd, nd->capture, sizeof(Node));
    groups(
        nd, (*global_next_group_id)++, global_next_group_id,
        next_named_gr);
    return;
  }

  if (nd->kind == NodeNamedCaptureGroup)
  {
    memcpy(nd, nd->named_capture.group, sizeof(Node));
    groups(
        nd, (*next_named_gr)++, global_next_group_id,
        next_named_gr);
    return;
  }

  nd->group = group;
  groups(children[0], group, global_next_group_id, next_named_gr);
  groups(children[1], group, global_next_group_id, next_named_gr);
}

static bool isRepeatLeast0(NodeKind k)
{
  return k == NodeLazyRepeatLeast0 || k == NodeGreedyRepeatLeast0;
}

/** convert RepeatLeast1 to RepeatLeast0 */
static void fix_0(Node* node)
{
  if (node == NULL)
    return;
  Node* children[2];
  Node_children(node, children);

  fix_0(children[0]);
  fix_0(children[1]);

  if (node->kind == NodeLazyRepeatLeast1)
  {
    Node* first = Node_clone(node->repeat);
    Node* rep = Node_alloc();
    rep->group = node->group;
    rep->kind = NodeLazyRepeatLeast0;
    rep->wherePlus1 = node->wherePlus1;
    rep->repeat = node->repeat;
    node->kind = NodeChain;
    node->chain.a = first;
    node->chain.b = rep;
  }

  if (node->kind == NodeGreedyRepeatLeast1)
  {
    Node* first = Node_clone(node->repeat);
    Node* rep = Node_alloc();
    rep->group = node->group;
    rep->kind = NodeGreedyRepeatLeast0;
    rep->wherePlus1 = node->wherePlus1;
    rep->repeat = node->repeat;
    node->kind = NodeChain;
    node->chain.a = first;
    node->chain.b = rep;
  }
}

/** move all code chained to or into all or cases if any or case contains
 * repetition */
static void fix_1(Node* node)
{
  if (node == NULL)
    return;
  Node* children[2];
  Node_children(node, children);

  if (node->kind == NodeChain)
  {
    Node* orr = find_trough_rep(node->chain.a, NodeOr);
    if (orr != NULL)
    {
      NodeLi cases = { 0 };
      or_cases(orr, &cases);
      Node* mov = node->chain.b;
      size_t i;
      for (i = 0; i < cases.len; i++)
      {
        Node* cas = cases.items[i];
        Node* inner = Node_alloc();
        memcpy(inner, cas, sizeof(Node));
        cas->kind = NodeChain;
        cas->chain.a = inner;
        Node* mov2 = Node_clone(mov);
        cas->chain.b = mov2;
      }
      free(cases.items);
      Node_free(mov);
      memcpy(node, node->chain.a, sizeof(Node));
    }
  }

  fix_1(children[0]);
  fix_1(children[1]);
}

/** move duplicate code in beginning of or cases to befre the or; required
 * because otherwise will break engine */
static void fix_2(Node* node)
{
  if (node == NULL)
    return;
  Node* children[2];
  Node_children(node, children);

  fix_2(children[0]);
  fix_2(children[1]);

  if (node->kind == NodeOr)
  {
    Node* a = node->or.a;
    Node* b = node->or.b;

    a = last_left_chain(a);
    b = last_left_chain(b);

    if (a && b && Node_eq(a->chain.a, b->chain.a))
    {
      Node* prefix = a->chain.a;
      free(b->chain.a);
      memcpy(a, a->chain.b, sizeof(Node));
      memcpy(b, b->chain.b, sizeof(Node));

      Node* right = Node_alloc();
      memcpy(right, node, sizeof(Node));
      node->kind = NodeChain;
      node->chain.a = prefix;
      node->chain.b = right;
    }
  }
}

static void lower(
    tpre_re_t* out,
    tpre_nodeid_t this_id,
    tpre_nodeid_t on_ok,
    tpre_nodeid_t on_error,
    tpre_backtrack_t bt,
    size_t* num_match,
    Node* node)
{
  assert(node);
  switch (node->kind)
  {
    // removed/checked by fix_*() and verify()
    case NodeLazyRepeatLeast1:
    case NodeGreedyRepeatLeast1:
#ifdef __GNUC__
      __builtin_unreachable();
#endif
      break;

    case NodeMatch: {
      if (num_match)
        (*num_match)++;
      tprec_re_setnode(
          out, this_id,
          (tpre_re_node_t) {
            node->match, on_ok, on_error, bt, node->group });
    }
    break;

    // parse everything, and backtrack until next pattern matches
    case NodeGreedyRepeatLeast0: {
      // this: bt_push(then: loop, onbt: on_error)
      // loop: bt_push(then: step, onbt: next/on_ok)
      // step: node(ok: loop, err: next/on_ok)
      tpre_nodeid_t step = tprec_re_resvnode(out);
      tpre_nodeid_t loop;
      if (on_error == -1)
      {
        loop = this_id;
      }
      else
      {
        loop = tprec_re_resvnode(out);
        tprec_re_setnode(
            out, this_id,
            (tpre_re_node_t) {
              SP(SPECIAL_BT_PUSH),
              /* then: */ loop,
              /* onbt: */ on_error, 0, 0 });
      }

      tprec_re_setnode(
          out, loop,
          (tpre_re_node_t) {
            SP(SPECIAL_BT_PUSH),
            /* then: */ step,
            /* onbt: */ on_ok, 0, 0 });
      lower(
          out, step, /*on_ok=*/loop, /*on_error=*/on_ok, 0, NULL,
          node->repeat);
    }
    break;

    // parse until next pattern matches
    case NodeLazyRepeatLeast0: {
      tpre_nodeid_t step = tprec_re_resvnode(out);
      lower(
          out, step, /*on_ok=*/this_id, /*on_error=*/on_error,
          /*bt=*/0, NULL, node->repeat);
      tprec_re_setnode(
          out, this_id,
          (tpre_re_node_t) {
            SP(SPECIAL_BT_PUSH), /* then: */ on_ok,
            /* onbt: */ step, 0, 0 });
    }
    break;

    case NodeChain: {
      size_t nimatch = 0;
      tpre_nodeid_t right = tprec_re_resvnode(out);
      assert(right != this_id);
      lower(
          out, this_id, right, on_error, bt, &nimatch,
          node->chain.a);
      if (num_match)
        (*num_match) += nimatch;
      lower(
          out, right, on_ok, on_error, bt + nimatch, num_match,
          node->chain.b);
    }
    break;

    case NodeOr: {
      tpre_nodeid_t right = tprec_re_resvnode(out);
      assert(right != this_id);
      lower(out, this_id, on_ok, right, 0, NULL, node->or.a);
      lower(out, right, on_ok, on_error, 0, NULL, node->or.b);
    }
    break;

    case NodeMaybe: {
      lower(out, this_id, on_ok, on_ok, bt, num_match, node->maybe);
    }
    break;

    default: assert(false && "bruh"); break;
  }
}

/** get rid of outer of nested RepeatLeast0 */
static void fix_3(Node* node)
{
  if (node == NULL)
    return;
  Node* children[2];
  Node_children(node, children);

  fix_3(children[0]);
  fix_3(children[1]);

  if (isRepeatLeast0(node->kind) &&
      isRepeatLeast0(node->repeat->kind))
  {
    node->wherePlus1 = node->repeat->wherePlus1;
    node->group = node->repeat->group;
    Node* old = node->repeat;
    node->repeat = node->repeat->repeat;
    free(old);
  }
}

static Node* leftmost(Node* node)
{
  switch (node->kind)
  {
    case NodeLazyRepeatLeast0:
    case NodeLazyRepeatLeast1:
    case NodeGreedyRepeatLeast0:
    case NodeGreedyRepeatLeast1: return leftmost(node->repeat);

    case NodeChain: return leftmost(node->chain.a);

    case NodeMaybe: return leftmost(node->maybe);

    default: return node;
  }
}

static int check_legal(tpre_errs_t* errs, Node* nd)
{
  if (nd->kind == NodeOr)
  {
    // this is invalid: a*?b|ac
    // this is invalid too: (ab)*|(ac)*

    NodeLi cases = { 0 };
    or_cases(nd, &cases);

    size_t i, j;
    for (i = 0; i < cases.len; i++)
    {
      for (j = 0; j < cases.len; j++)
      {
        if (i == j)
          continue;

        Node* a = leftmost(cases.items[i]);
        Node* b = leftmost(cases.items[j]);

        if (Node_eq(a, b))
        {
          tprec_add_err(
              errs,
              nd->wherePlus1 - 1, "patterns where a case has the same starting pattern in a repeating sequence as in another case are not yet supported");
          return 1;
        }
      }
    }

    free(cases.items);
  }

  Node* children[2];
  Node_children(nd, children);
  for (int i = 0; i < 2; i++)
    if (children[i] && check_legal(errs, children[i]))
      return 1;
  return 0;
}

static void tpre_dump(tpre_re_t out)
{
  printf("start = %i\n", out.first_node);
  printf("nd\tok\terr\tv\tbt\tg\n");
  size_t i;
  for (i = 0; i < out.num_nodes; i++)
  {
    char s[3];
    if (out.i[i].pat.is_special)
    {
      s[0] = '\\';
      switch (out.i[i].pat.val)
      {
        case SPECIAL_ANY:     s[1] = '*'; break;
        case SPECIAL_END:     s[1] = '$'; break;
        case SPECIAL_SPACE:   s[1] = 's'; break;
        case SPECIAL_START:   s[1] = '^'; break;
        case SPECIAL_BT_PUSH: s[1] = '{'; break;
        default:              break;
      }
      s[2] = '\0';
    }
    else
    {
      s[0] = out.i[i].pat.val;
      s[1] = '\0';
    }
    printf(
        "%zu\t%i\t%i\t%s\t%u\t%u\n", i, out.i[i].ok,
        out.i[i].err, s, out.i[i].backtrack, out.i[i].group);
  }
  fflush(stdout);
}

int tpre_compile(
    tpre_re_t* out,
    char const* str,
    tpre_errs_t* errs_out,
    tpre_opts_t opts)
{
  int status = 0;

  memset(out, 0, sizeof(tpre_re_t));

  if (errs_out)
  {
    errs_out->len = 0;
    errs_out->items = NULL;
  }

  TkL li = { 0 };
  if (tprec_lexe(&li, errs_out, str))
    return 1;
  if (li.oom)
    return 1;
  Node* nd = tprec_parse(li);
  if (!nd)
  {
    if (li.len)
      tprec_add_err(
          errs_out, TkL_get(&li, 0).where, "unparsed tokens");
    return 1;
  }
  if (verify(nd, errs_out))
    status = 1;

  do
  {
    size_t num_groups = count_groups(nd);
    size_t num_named_groups = count_named_groups(nd);

    out->first_named_group = num_groups + 1;
    out->num_named_groups = num_named_groups;
    out->named_groups = malloc(sizeof(char*) * num_named_groups);
    char** named_groups_ptr = out->named_groups;
    if (!named_groups_ptr)
    {
      status = 1;
      break;
    }
    named_groups(nd, &named_groups_ptr);

    tpre_groupid_t nextgr = 1;
    tpre_groupid_t next_named_gr = out->first_named_group;
    groups(nd, 0, &nextgr, &next_named_gr);
  } while (0);

  if (opts.start_unanchored)
  {
    Node* any = Node_alloc();
    any->kind = NodeMatch;
    any->match = SP(SPECIAL_ANY);

    Node* rep = Node_alloc();
    rep->kind = NodeLazyRepeatLeast0;
    rep->repeat = any;

    nd = maybeChain(rep, nd);
  }

  fix_0(nd);
  fix_1(nd);
  fix_2(nd);
  fix_3(nd);
  if (check_legal(errs_out, nd))
    status = 1;

  Node_print(nd, stdout, 0, true);

  tpre_nodeid_t nd0 = tprec_re_resvnode(out);
  tpre_nodeid_t err = NODE_ERR;

  if (!opts.start_unanchored)
  {
    tpre_nodeid_t anchor = tprec_re_addnode(
        out,
        (tpre_re_node_t) {
          .pat = SP(SPECIAL_START),
          .ok = nd0,
          .err = err,
          .backtrack = 0,
          .group = 0,
        });
    out->first_node = anchor;
  }

  tpre_nodeid_t last;
  if (opts.end_unanchored)
  {
    last = NODE_DONE;
  }
  else
  {
    tpre_nodeid_t anchor = tprec_re_addnode(
        out,
        (tpre_re_node_t) {
          .pat = SP(SPECIAL_END),
          .ok = NODE_DONE,
          .err = NODE_ERR,
          .backtrack = 0,
          .group = 0,
        });
    last = anchor;
  }

  lower(out, nd0, last, err, 0, NULL, nd);

  tpre_dump(*out);

  Node_free(nd);

  if (status)
  {
    tpre_free(*out);
    memset(out, 0, sizeof(tpre_re_t));
  }
  return status;
}

void tpre_free(tpre_re_t re)
{
  if (re.free)
  {
    for (size_t i = 0; i < re.num_named_groups; i++)
      free(re.named_groups[i]);
    free(re.named_groups);
    free(re.i);
  }
}

// TODO: this will break the enine: a*?b|ac
// TODO: this will break the engine (ab)*|(ac)*
