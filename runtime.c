#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/tpre_runtime.h"
#include "shared.h"

#ifdef __GNUC__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-function"
#endif

#define SLOWARR_FUNC static
#include "slowarr.h"


typedef struct
{
  tpre_nodeid_t cursor;
  tpre_match_t match;
  size_t i;
} bt_stack_ent;

SLOWARR_Header(bt_stack_ent);
SLOWARR_Impl(bt_stack_ent);

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif


void tpre_match_free(tpre_match_t match)
{
  free(match.groups);
}

static tpre_match_t tpre_match_dup(tpre_match_t const* match)
{
  tpre_match_t out;
  out.found = match->found;
  out.ngroups = match->ngroups;
  out.groups = malloc(sizeof(*out.groups) * out.ngroups);
  for (size_t i = 0; i < match->ngroups; i++)
    out.groups[i] = match->groups[i];
  return out;
}

static void tpre_match_group_put(
    tpre_match_t* match,
    tpre_groupid_t group,
    char c,
    tpre_src_loc_t loc)
{
  if (group == 0)
    return;
  tpre_group_t* g = &match->groups[group];
  if (g->len == 0)
    g->begin = loc;
  g->len++;
}

static int
pattern_match(tpre_pattern_t pat, char src, bool is_begin)
{
  if (!pat.is_special)
    return src == (char) pat.val ? 1 : -1;

  switch (pat.val)
  {
    case SPECIAL_ANY: return src != '\0' ? 1 : -1;

    case SPECIAL_SPACE:
      return (src == ' ' || src == '\n' || src == '\t' ||
              src == '\r')
          ? 1
          : -1;

    case SPECIAL_DIGIT:
      return (src >= '0' && src <= '9') ? 1 : -1;

    case SPECIAL_WORDC:
      return ((src >= '0' && src <= '9') ||
              (src >= 'a' && src <= 'z') ||
              (src >= 'A' && src <= 'Z') || src == '_')
          ? 1
          : -1;

    case SPECIAL_BT_PUSH: return -2;

    case SPECIAL_END: return src == '\0' ? 0 : -1;

    case SPECIAL_START: return is_begin ? 0 : -1;

    default: return -1;
  }
}

static tpre_match_t init_match(tpre_re_t const* re)
{
  tpre_match_t match = { 0 };
  match.ngroups = re->max_group + 1;
  match.groups = calloc(sizeof(*match.groups), match.ngroups);

  return match;
}

tpre_match_t
tpre_matchn(tpre_re_t const* re, const char* str, size_t strl)
{
  SLOWARR_MANGLE(bt_stack_ent) bt_stack = { 0 };

  tpre_match_t match = init_match(re);
  size_t i = 0;
  tpre_nodeid_t cursor = re->first_node;

  do
  {
    while (cursor >= 0)
    {
      if (cursor >= re->num_nodes)
        return match;

      int m = pattern_match(
          re->i[cursor].pat, i >= strl ? '\0' : str[i], i == 0);
      if (m == -2)
      {
        // TODO: can do this more efficiently: can reuse memory from dups
        SLOWARR_MANGLE_F(bt_stack_ent, push)(
            &bt_stack,
            (bt_stack_ent) {
              .cursor = re->i[cursor].err,
              .i = i,
              .match = tpre_match_dup(&match) });
        m = 0;
      }
      if (m >= 0)
      {
        for (; m && i < strl; m--, i++)
          tpre_match_group_put(
              &match, re->i[cursor].group, str[i], i);
        cursor = re->i[cursor].ok;
      }
      else
      {
        tpre_backtrack_t bt = re->i[cursor].backtrack;
        i -= bt;
        tpre_groupid_t g = re->i[cursor].group;
        if (bt > 0)
        {
          if (match.groups[g].len >= bt)
          {
            size_t n = bt;
            if (n > match.groups[g].len)
              n = match.groups[g].len;
            match.groups[g].len -= n;
          }
        }
        cursor = re->i[cursor].err;
      }
    }

    if (cursor == NODE_DONE)
    {
      match.found = true;
      break;
    }

    if (!bt_stack.len)
      return match;

    tpre_match_free(match);

    bt_stack_ent e =
        SLOWARR_MANGLE_F(bt_stack_ent, pop)(&bt_stack);
    cursor = e.cursor;
    i = e.i;
    match = e.match;
  } while (1);

  return match;
}

void tpre_match_dump(
    tpre_re_t const* re,
    tpre_match_t match,
    char const* matched_str,
    FILE* out)
{
  if (match.found)
  {
    fprintf(out, "does match\n");
    size_t i;
    for (i = 1; i < match.ngroups; i++)
    {
      tpre_group_t group = match.groups[i];
      char const* name = i >= re->first_named_group
          ? (re->named_groups[i - re->first_named_group])
          : 0;

      if (name)
        fprintf(out, "  group '%s': ", name);
      else
        fprintf(out, "  group %zu: ", i);
      fwrite(matched_str + group.begin, 1, group.len, out);
      fprintf(out, "\n");
    }
  }
  else
  {
    fprintf(out, "does not match\n");
  }
}
