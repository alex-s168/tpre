#include <string.h>
#include "../shared.h"
#include "tpre_compiler.h"

#define USING_TPREC
#include "lexer.h"
#include "parser.h"
#include "utils.h"

static bool isRepeatLeast0(NodeKind k)
{
  return k == NodeLazyRepeatLeast0 || k == NodeGreedyRepeatLeast0;
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

static void rewr_repleast1_to_repleast0(Node* node)
{
  if (node == NULL)
    return;
  Node* children[2];
  Node_children(node, children);

  rewr_repleast1_to_repleast0(children[0]);
  rewr_repleast1_to_repleast0(children[1]);

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

static void rewr_nested_repleast0(Node* node)
{
  if (node == NULL)
    return;
  Node* children[2];
  Node_children(node, children);

  rewr_nested_repleast0(children[0]);
  rewr_nested_repleast0(children[1]);

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

int tpre2fsm(
    tpre_fsm_t* out,
    char const* str,
    tpre_errs_t* errs_out,
    tpre_opts_t opts)
{
  tpre_fsm_init(out);

  if (errs_out)
  {
    errs_out->len = 0;
    errs_out->items = NULL;
  }

  TkL li = { 0 };
  if (tprec_lexe(&li, errs_out, str, &opts))
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
  else
  {
    // TODO: opt pass to remove all pat_start if know we are at start (on fsm level)
    Node* start = Node_alloc();
    start->kind = NodeMatch;
    start->match = SP(SPECIAL_START);

    nd = maybeChain(start, nd);
  }

  if (!opts.end_unanchored)
  {
    Node* end = Node_alloc();
    end->kind = NodeMatch;
    end->match = SP(SPECIAL_END);

    nd = maybeChain(nd, end);
  }

  // rewrites:
  rewr_repleast1_to_repleast0(nd);
  rewr_nested_repleast0(nd);

  Node_print(nd, stdout, 0, true);

  if (1)
  {
    size_t num_groups = count_groups(nd);
    size_t num_named_groups = count_named_groups(nd);

    out->first_named_group = num_groups + 1;
    out->num_named_groups = num_named_groups;
    char** named_groupsp =
        malloc(sizeof(char*) * num_named_groups);
    if (!named_groupsp)
    {
      Node_free(nd);
      return 1;
    }
    named_groups(nd, &named_groupsp);
    out->named_groups = (char const**) named_groupsp;

    tpre_groupid_t nextgr = 1;
    tpre_groupid_t next_named_gr = out->first_named_group;
    groups(nd, 0, &nextgr, &next_named_gr);
  };

  // TODO: first lower to fsm without backtrack info, and then figure out known backtracks?

  Node_free(nd);
  return 0;
}
