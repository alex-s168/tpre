#ifndef _TPREC_PARSER_H
#define _TPREC_PARSER_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "compiler/lexer.h"
#include "include/tpre_common.h"

typedef enum
{
  NodeMatch,
  NodeChain,
  NodeOr,
  NodeMaybe,
  NodeNot,
  NodeGreedyRepeatLeast0,
  NodeGreedyRepeatLeast1,
  NodeLazyRepeatLeast0,
  NodeLazyRepeatLeast1,
  NodeJustGroup,
  NodeCaptureGroup,
  NodeNamedCaptureGroup,
  NodeBackref,
  NodeNamedBackref,
} NodeKind;

typedef struct Node Node;
struct Node
{
  NodeKind kind;
  tpre_groupid_t group;
  size_t wherePlus1;
  union
  {
    tpre_pattern_t match;

    struct
    {
      Node* a;
      Node* b;
    } chain;

    struct
    {
      Node* a;
      Node* b;
    } or ;

    Node* just_group;
    Node* maybe;
    Node* repeat;
    Node* capture;
    Node * not;

    struct
    {
      char name[20];
      Node* group;
    } named_capture;

    tpre_groupid_t backref;

    struct
    {
      char name[20];
    } named_backref;
  };
};

static inline Node* Node_alloc(void)
{
  return calloc(1, sizeof(Node));
}

void tprec_Node_free(Node* node);
Node* tprec_Node_clone(Node* node);

void tprec_Node_children(Node* nd, Node* childrenOut[2]);
bool tprec_Node_eq(Node* a, Node* b);
void tprec_Node_print(
    Node* node, FILE* file, size_t indent, bool print_grps);

Node* tprec_maybeChain(Node* a, Node* b);
Node* tprec_oneOf(Node** nodes, size_t len);
Node* tprec_genMatch(size_t where, tpre_pattern_t pat);

Node* tprec_parse(TkL toks);

#endif

#if defined(USING_TPREC) && !defined(Node_free)
#define Node_free tprec_Node_free
#define Node_clone tprec_Node_clone
#define Node_children tprec_Node_children
#define Node_eq tprec_Node_eq
#define Node_print tprec_Node_print
#define maybeChain tprec_maybeChain
#define oneOf tprec_oneOf
#define genMatch tprec_genMatch
#endif
