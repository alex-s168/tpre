#include <stdlib.h>
#include <string.h>
#include "tpre_compiler.h"

void tpre_fsm_pat_free(tpre_fsm_pat_t pat)
{
  switch (pat.kind)
  {
    case TPRE_FSM_PAT_ONEOF: free(pat.v.ascii.items); break;
    default:                 break;
  }
}

void tpre_fsm_gc(tpre_fsm_t* fsm)
{
  // TODO
}

void tpre_fsm_free(tpre_fsm_t* fsm)
{
  // TODO

  for (size_t i = 0; i < (size_t) fsm->num_named_groups; i++)
    free((char*) fsm->named_groups[i]);
  free(fsm->named_groups);
}

tpre_fsm_node_t* tpre_fsm_mknd(tpre_fsm_t* fsm)
{
  tpre_fsm_node_t* out = calloc(1, sizeof(tpre_fsm_node_t));
  if (!out)
    return 0;
  out->els = fsm->nd_err;
  return out;
}

void tpre_fsm_init(tpre_fsm_t* fsm)
{
  memset(fsm, 0, sizeof(tpre_fsm_t));
  fsm->nd_err = calloc(1, sizeof(tpre_fsm_node_t));
  fsm->nd_ok = calloc(1, sizeof(tpre_fsm_node_t));
}
