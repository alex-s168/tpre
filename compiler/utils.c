#include "utils.h"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

char* tprec_strdup(char const* s)
{
  if (!s)
    return 0;
  size_t nb = strlen(s) + 1;
  char* out = malloc(nb);
  if (!out)
    return 0;
  memcpy(out, s, nb);
  return out;
}


void tpre_errs_free(tpre_errs_t errs)
{
  size_t i;
  for (i = 0; i < errs.len; i++)
    free(errs.items[i].message);
  free(errs.items);
}

void tprec_add_err(
    tpre_errs_t* errs, size_t byte_loc, char const* fmt, ...)
{
  char buf[256];
  if (!errs)
    return;

  va_list args;
  va_start(args, fmt);

  vsnprintf(buf, sizeof(buf), fmt, args);

  va_end(args);

  void* newptr =
      realloc(errs->items, (errs->len + 1) * sizeof(*errs->items));
  if (!newptr)
    return;
  errs->items = newptr;
  errs->items[errs->len++] = (tpre_err_t) {
    .pat_byte_loc = byte_loc, .message = tprec_strdup(buf)
  };
}


void tprec_re_setnode(
    tpre_re_t* re, tpre_nodeid_t id, tpre_re_node_t nd)
{
  re->i[id] = nd;
  if (nd.group > re->max_group)
    re->max_group = nd.group;
}

tpre_nodeid_t tprec_re_addnode(tpre_re_t* re, tpre_re_node_t nd)
{
  re->i = realloc(re->i, sizeof(*re->i) * (re->num_nodes + 1));
  assert(re->i); // TODO: no
  re->i[re->num_nodes] = nd;
  tprec_re_setnode(re, re->num_nodes, nd);
  re->free = true;
  return re->num_nodes++;
}

tpre_nodeid_t tprec_re_resvnode(tpre_re_t* re)
{
  return tprec_re_addnode(re, (tpre_re_node_t) { 0 });
}
