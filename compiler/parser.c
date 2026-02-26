#include "parser.h"
#include <string.h>
#include "../shared.h"

#define USING_TPREC
#include "compiler/lexer.h"

void tprec_Node_children(Node* nd, Node* childrenOut[2])
{
  childrenOut[0] = NULL;
  childrenOut[1] = NULL;

  switch (nd->kind)
  {
    case NodeMatch:
    case NodeBackref:
    case NodeNamedBackref: break;

    case NodeChain:
      childrenOut[0] = nd->chain.a;
      childrenOut[1] = nd->chain.b;
      break;

    case NodeNot: childrenOut[0] = nd->not; break;

    case NodeOr:
      childrenOut[0] = nd->or.a;
      childrenOut[1] = nd->or.b;
      break;

    case NodeMaybe: childrenOut[0] = nd->maybe; break;

    case NodeGreedyRepeatLeast0:
    case NodeGreedyRepeatLeast1:
    case NodeLazyRepeatLeast0:
    case NodeLazyRepeatLeast1:
      childrenOut[0] = nd->repeat;
      break;

    case NodeJustGroup: childrenOut[0] = nd->just_group; break;

    case NodeCaptureGroup: childrenOut[0] = nd->capture; break;

    case NodeNamedCaptureGroup:
      childrenOut[0] = nd->named_capture.group;
      break;
  }
}

void tprec_Node_free(Node* node)
{
  if (!node)
    return;
  Node* children[2];
  tprec_Node_children(node, children);
  tprec_Node_free(children[0]);
  tprec_Node_free(children[1]);
  free(node);
}

Node* tprec_Node_clone(Node* node)
{
  Node* copy = Node_alloc();
  copy->kind = node->kind;
  copy->group = node->group;
  copy->wherePlus1 = node->wherePlus1;

  switch (node->kind)
  {
    case NodeMatch: copy->match = node->match; break;

    case NodeChain:
      copy->chain.a = tprec_Node_clone(node->chain.a);
      copy->chain.b = tprec_Node_clone(node->chain.b);
      break;

    case NodeOr:
      copy->or.a = tprec_Node_clone(node->or.a);
      copy->or.b = tprec_Node_clone(node->or.b);
      break;

    case NodeMaybe:
      copy->maybe = tprec_Node_clone(node->maybe);
      break;

    case NodeNot: copy->not= tprec_Node_clone(node->not); break;

    case NodeGreedyRepeatLeast0:
    case NodeGreedyRepeatLeast1:
    case NodeLazyRepeatLeast0:
    case NodeLazyRepeatLeast1:
      copy->repeat = tprec_Node_clone(node->repeat);
      break;

    case NodeJustGroup:
      copy->just_group = tprec_Node_clone(node->just_group);
      break;

    case NodeCaptureGroup:
      copy->capture = tprec_Node_clone(node->capture);
      break;

    case NodeNamedCaptureGroup:
      copy->named_capture.group =
          tprec_Node_clone(node->named_capture.group);
      memcpy(
          copy->named_capture.name, node->named_capture.name, 20);
      break;

    case NodeNamedBackref:
      memcpy(
          copy->named_backref.name, node->named_backref.name, 20);
      break;

    case NodeBackref: copy->backref = node->backref; break;
  }

  return copy;
}

static const char* NodeKind_str[] = {
  [NodeMatch] = "Match",
  [NodeChain] = "Chain",
  [NodeOr] = "Or",
  [NodeMaybe] = "Maybe",
  [NodeNot] = "Not",
  [NodeGreedyRepeatLeast0] = "GreedyRepeatLeast0",
  [NodeGreedyRepeatLeast1] = "GreedyRepeatLeast1",
  [NodeLazyRepeatLeast0] = "LazyRepeatLeast0",
  [NodeLazyRepeatLeast1] = "LazyRepeatLeast1",
  [NodeJustGroup] = "JustGroup",
  [NodeCaptureGroup] = "CaptureGroup",
  [NodeNamedCaptureGroup] = "NamedCaptureGroup",
  [NodeBackref] = "Backref",
  [NodeNamedBackref] = "NamedBackref",
};

void tprec_Node_print(
    Node* node, FILE* file, size_t indent, bool print_grps)
{
  if (!node)
    return;
  size_t i;
  for (i = 0; i < indent * 2; i++)
    fputc(' ', file);
  fprintf(file, "%s ", NodeKind_str[node->kind]);
  if (print_grps)
    fprintf(file, "@%zu ", (size_t) node->group);
  switch (node->kind)
  {
    case NodeMatch: {
      tpre_pattern_t pat = node->match;
      if (pat.is_special)
        fprintf(file, "(special: %u)", pat.val);
      else
        fprintf(file, "(%c)", (char) pat.val);
    }
    break;

    case NodeNamedCaptureGroup: {
      fprintf(file, "(%s)", node->named_capture.name);
    }
    break;

    case NodeNamedBackref: {
      fprintf(file, "(%s)", node->named_backref.name);
    }
    break;

    case NodeBackref: {
      fprintf(file, "(%u)", node->backref);
    }
    break;

    default: break;
  }
  fputc('\n', file);
  Node* children[2];
  tprec_Node_children(node, children);
  tprec_Node_print(children[0], file, indent + 1, print_grps);
  tprec_Node_print(children[1], file, indent + 1, print_grps);
}

bool tprec_Node_eq(Node* a, Node* b)
{
  if (a->kind != b->kind)
    return false;
  if (a->group != b->group)
    return false;

  switch (a->kind)
  {
    case NodeMatch:
      return a->match.is_special == b->match.is_special &&
          a->match.val == b->match.val;

    case NodeChain:
      return tprec_Node_eq(a->chain.a, b->chain.a) &&
          tprec_Node_eq(a->chain.b, b->chain.b);

    case NodeOr:
      return tprec_Node_eq(a->or.a, b->or.a) &&
          tprec_Node_eq(a->or.b, b->or.b);

    case NodeMaybe: return tprec_Node_eq(a->maybe, b->maybe);

    case NodeNot: return tprec_Node_eq(a->not, b->not);

    case NodeGreedyRepeatLeast0:
    case NodeGreedyRepeatLeast1:
    case NodeLazyRepeatLeast0:
    case NodeLazyRepeatLeast1:
      return tprec_Node_eq(a->repeat, b->repeat);

    case NodeJustGroup:
      return tprec_Node_eq(a->just_group, b->just_group);

    case NodeCaptureGroup:
      return tprec_Node_eq(a->capture, b->capture);

    case NodeNamedCaptureGroup:
      return tprec_Node_eq(
                 a->named_capture.group, b->named_capture.group) &&
          !strcmp(a->named_capture.name, b->named_capture.name);

    case NodeNamedBackref:
      return !strcmp(a->named_backref.name, b->named_backref.name);

    case NodeBackref: return a->backref == b->backref;
  }
}

Node* tprec_maybeChain(Node* a, Node* b)
{
  if (b == NULL)
    return a;

  Node* n = Node_alloc();
  n->wherePlus1 = a->wherePlus1;
  n->kind = NodeChain;
  n->chain.a = a;
  n->chain.b = b;
  return n;
}

Node* tprec_oneOf(Node** nodes, size_t len)
{
  if (len == 0)
    return NULL;
  if (len == 1)
    return nodes[0];
  Node* rhs = tprec_oneOf(nodes + 1, len - 1);
  Node* self = Node_alloc();
  self->wherePlus1 = nodes[0]->wherePlus1;
  self->kind = NodeOr;
  self->or.a = nodes[0];
  self->or.b = rhs;
  return self;
}

Node* tprec_genMatch(size_t where, tpre_pattern_t pat)
{
  Node* self = Node_alloc();
  self->kind = NodeMatch;
  self->match = pat;
  self->wherePlus1 = where + 1;
  return self;
}

#define Node_children tprec_Node_children

static void handle_postfix(Node* node, ReTk op)
{
  if (node->kind == NodeChain)
  {
    Node* children[2];
    Node_children(node, children);

    if (children[1] != NULL)
      return handle_postfix(children[1], op);
    if (children[0] != NULL)
      return handle_postfix(children[0], op);
  }

  Node* copy = Node_alloc();
  memcpy(copy, node, sizeof(Node));

  if (op.ty == OrNot)
  {
    node->kind = NodeMaybe;
    node->maybe = copy;
  }
  else if (op.ty == GreedyRepeatLeast0)
  {
    node->kind = NodeGreedyRepeatLeast0;
    node->repeat = copy;
  }
  else if (op.ty == GreedyRepeatLeast1)
  {
    node->kind = NodeGreedyRepeatLeast1;
    node->repeat = copy;
  }
  else if (op.ty == LazyRepeatLeast0)
  {
    node->kind = NodeLazyRepeatLeast0;
    node->repeat = copy;
  }
  else if (op.ty == LazyRepeatLeast1)
  {
    node->kind = NodeLazyRepeatLeast1;
    node->repeat = copy;
  }
}

static void replaceChainsWithOrs(Node* node)
{
  if (node == NULL)
    return;
  if (node->kind != NodeChain)
    return;
  Node tmp = *node;
  node->kind = NodeOr;
  node->or.a = tmp.chain.a;
  node->or.b = tmp.chain.b;
  Node* children[2];
  Node_children(node, children);
  replaceChainsWithOrs(children[0]);
  replaceChainsWithOrs(children[1]);
}

Node* tprec_parse(TkL toks)
{
  if (TkL_len(&toks) == 0)
    return NULL;

  // weird code for ors
  {
    TkL* seg = NULL;
    size_t seglen = 0;

    // split with nesting at ors

    size_t nesting = 0;
    size_t begin = 0;
    {
      size_t i;
      for (i = 0; i < TkL_len(&toks); i++)
      {
        ReTkTy t = TkL_get(&toks, i).ty;
        if (tk_isCaptureGroupOpen(t))
          nesting++;
        else if (t == CaptureGroupClose)
          nesting--;
        else if (tk_isOneOfOpen(t))
          nesting++;
        else if (t == OneOfClose)
          nesting--;
        if (nesting == 0 && t == OrElse)
        {
          TkL tokl = TkL_copy_range(&toks, begin, i - begin);
          seg = realloc(seg, sizeof(*seg) * (seglen + 1));
          seg[seglen++] = tokl;
          begin = i + 1;
        }
      }
      if (TkL_len(&toks) - begin > 0)
      {
        TkL tokl =
            TkL_copy_range(&toks, begin, TkL_len(&toks) - begin);
        seg = realloc(seg, sizeof(*seg) * (seglen + 1));
        seg[seglen++] = tokl;
      }
    }

    if (seglen >= 2)
    {
      Node* fold = NULL;
      size_t i;
      for (i = 0; i < seglen; i++)
      {
        Node* nd = tprec_parse(seg[i]);
        if (fold == NULL)
          fold = nd;
        else
          fold = tprec_oneOf((Node*[]) { fold, nd }, 2);
      }
      free(seg);
      TkL_free(&toks);
      return fold;
    }

    free(seg);
  }

  // weird code for postfix operators
  {
    char* is_postfix = calloc(TkL_len(&toks), 1);
    void* is_postfix_alloc = is_postfix;

    {
      size_t nesting = 0;
      size_t i;
      for (i = 0; i < TkL_len(&toks); i++)
      {
        ReTkTy t = TkL_get(&toks, i).ty;
        if (tk_isCaptureGroupOpen(t))
          nesting++;
        else if (t == CaptureGroupClose)
          nesting--;
        else if (tk_isOneOfOpen(t))
          nesting++;
        else if (t == OneOfClose)
          nesting--;

        if (nesting == 0 && tk_isPostfix(t))
          is_postfix[i] = true;
      }
    }

    Node* fold = NULL;
    size_t idx;
    for (idx = 0; idx < (volatile size_t) TkL_len(&toks); idx++)
    {
      if (!is_postfix[idx])
        continue;

      ReTk op = TkL_get(&toks, idx);
      Node* lhs = tprec_parse(TkL_copy_range(&toks, 0, idx));

      size_t i;
      for (i = 0; i < idx + 1; i++)
      {
        ReTk ign;
        TkL_take(&ign, &toks);
      }
      is_postfix += idx + 1;

      if (fold)
        lhs = tprec_maybeChain(fold, lhs);
      fold = lhs;

      handle_postfix(fold, op);
    }

    free(is_postfix_alloc);

    if (fold != NULL)
      return tprec_maybeChain(fold, tprec_parse(toks));
  }

  ReTkTy firstTy = TkL_get(&toks, 0).ty;
  size_t firstPos = TkL_get(&toks, 0).where;

  if (tk_isCaptureGroupOpen(firstTy))
  {
    size_t nesting = 0;
    size_t close = 0;
    for (; close < TkL_len(&toks); close++)
    {
      ReTkTy t = TkL_get(&toks, close).ty;
      if (tk_isCaptureGroupOpen(t))
        nesting++;
      else if (t == CaptureGroupClose)
      {
        nesting--;
        if (nesting == 0)
          break;
      }
    }

    Node* inner =
        tprec_parse(TkL_copy_range(&toks, 1, close - 1));
    Node* rem = tprec_parse(TkL_copy_range(
        &toks, close + 1, TkL_len(&toks) - close - 1));

    Node* self = Node_alloc();
    self->wherePlus1 = firstPos + 1;
    if (firstTy == CaptureGroupOpen)
    {
      self->kind = NodeCaptureGroup;
      self->capture = inner;
    }
    else if (firstTy == CaptureGroupOpenNoCapture)
    {
      self->kind = NodeJustGroup;
      self->just_group = inner;
    }
    else
    {
      self->kind = NodeNamedCaptureGroup;
      self->named_capture.group = inner;
      ReTk t = TkL_get(&toks, 0);
      char const* name = t.group_name;
      memcpy(self->named_capture.name, name, 20);
    }

    TkL_free(&toks);
    return tprec_maybeChain(self, rem);
  }

  if (firstTy == Match)
  {
    Node* rem =
        tprec_parse(TkL_copy_range(&toks, 1, TkL_len(&toks) - 1));
    Node* self =
        tprec_genMatch(firstPos + 1, TkL_get(&toks, 0).match);

    TkL_free(&toks);
    return tprec_maybeChain(self, rem);
  }

  if (firstTy == MatchRange)
  {
    Node* rem =
        tprec_parse(TkL_copy_range(&toks, 1, TkL_len(&toks) - 1));

    char from = TkL_get(&toks, 0).range.from;
    char to = TkL_get(&toks, 0).range.to;
    if (from > to)
    {
      char t = from;
      from = to;
      to = t;
    }

    size_t len = to - from + 1;
    Node* nodes[len];
    size_t i;
    for (i = 0; i < len; i++)
      nodes[i] = tprec_genMatch(firstPos, NO(from + i));
    Node* self = tprec_oneOf(nodes, len);

    TkL_free(&toks);
    return tprec_maybeChain(self, rem);
  }

  if (firstTy == BackrefId)
  {
    Node* rem =
        tprec_parse(TkL_copy_range(&toks, 1, TkL_len(&toks) - 1));
    Node* self = Node_alloc();
    self->wherePlus1 = firstPos + 1;
    self->kind = NodeBackref;
    self->backref = TkL_get(&toks, 0).group_id;

    TkL_free(&toks);
    return tprec_maybeChain(self, rem);
  }

  if (firstTy == BackrefName)
  {
    Node* rem =
        tprec_parse(TkL_copy_range(&toks, 1, TkL_len(&toks) - 1));
    Node* self = Node_alloc();
    self->wherePlus1 = firstPos + 1;
    self->kind = NodeNamedBackref;
    memcpy(
        self->named_backref.name, TkL_get(&toks, 0).group_name,
        20);

    TkL_free(&toks);
    return tprec_maybeChain(self, rem);
  }

  if (tk_isOneOfOpen(firstTy))
  {
    size_t nesting = 0;
    size_t i = 0;
    for (; i < TkL_len(&toks); i++)
    {
      ReTkTy t = TkL_get(&toks, i).ty;
      if (tk_isOneOfOpen(t))
        nesting++;
      else if (t == OneOfClose)
      {
        nesting--;
        if (nesting == 0)
          break;
      }
    }

    Node* self = tprec_parse(TkL_copy_range(&toks, 1, i - 1));
    Node* rem = tprec_parse(
        TkL_copy_range(&toks, i + 1, TkL_len(&toks) - i - 1));
    TkL_free(&toks);

    replaceChainsWithOrs(self);
    if (firstTy == OneOfOpenInvert)
    {
      Node* new = Node_alloc();
      new->wherePlus1 = firstPos + 1;
      new->kind = NodeNot;
      new->not= self;
      self = new;
    }

    return tprec_maybeChain(self, rem);
  }

  TkL_free(&toks);
  return NULL;
}
