#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "tpre.h"

tpre_match_t
match(char const* pat, char const* str, tpre_opts_t opts)
{
  size_t strl = strlen(str);
  char* buf = malloc(strl + 200);
  memcpy(buf, str, strl);
  memset(buf + strl, 'a', 100);

  tpre_re_t re;
  tpre_errs_t errs;
  if (tpre_compile(&re, pat, &errs, opts))
    assert(false && "compile fail");
  return tpre_matchn(&re, buf, strl);
}

int main()
{
  tpre_match_t m;
  m = match("[ab(cd)e*+]", "a", (tpre_opts_t) { 0 });
  assert(m.found);
  m = match("[ab(cd)e*+]", "(", (tpre_opts_t) { 0 });
  assert(m.found);
  m = match("[ab(cd)e*+]", ")", (tpre_opts_t) { 0 });
  assert(m.found);
  m = match("[ab(cd)e*+]", "+", (tpre_opts_t) { 0 });
  assert(m.found);
  m = match("[ab(cd)e*+]", "*", (tpre_opts_t) { 0 });
  assert(m.found);
  m = match("[ab(cd)e*+]", "!", (tpre_opts_t) { 0 });
  assert(!m.found);

  // regression 1
  m = match("(a)", "", (tpre_opts_t) { 0 });
  assert(!m.found);
  m = match(
      "(a)", "",
      (tpre_opts_t) {
        .start_unanchored = 1, .end_unanchored = 1 });
  assert(!m.found);

  // regression 2

  m = match(
      "((?:(?:a(?:a?)))$)", "bba",
      (tpre_opts_t) {
        .start_unanchored = 1, .end_unanchored = 1 });
  assert(!m.found);

  // anchored
  m = match("abc", "abc  ", (tpre_opts_t) { 0 });
  assert(!m.found);
  m = match("abc", "abc", (tpre_opts_t) { 0 });
  assert(m.found);
  m = match("abc", "abc  ", (tpre_opts_t) { .end_unanchored = 1 });
  assert(m.found);
  m = match("abc", "  abc", (tpre_opts_t) { 0 });
  assert(!m.found);
  m = match(
      "(abc)", "  abc",
      (tpre_opts_t) { .start_unanchored = 1, 0 });
  assert(m.found);
  assert(m.ngroups == 2);
  assert(m.groups[1].begin == 2);
  assert(m.groups[1].len == 3);

  // lazy don't match anything at end, unless anchored
  m = match(
      "(a*?)", "aaaaaa", (tpre_opts_t) { .end_unanchored = 1 });
  assert(m.found);
  assert(m.ngroups == 2);
  assert(m.groups[1].len == 0);
  m = match(
      "(a+?)", "aaaaaa", (tpre_opts_t) { .end_unanchored = 1 });
  assert(m.found);
  assert(m.ngroups == 2);
  assert(m.groups[1].begin == 0);
  assert(m.groups[1].len == 1);
  m = match("(a*?)", "aaaaaa", (tpre_opts_t) { 0 });
  assert(m.found);
  assert(m.ngroups == 2);
  assert(m.groups[1].begin == 0);
  assert(m.groups[1].len == 6);

  // backtracking matches
  m = match("(\\d*?)(\\d+)", "123456", (tpre_opts_t) { 0 });
  assert(m.found);
  assert(m.ngroups == 3);
  assert(m.groups[1].len == 0);
  assert(m.groups[2].begin == 0);
  assert(m.groups[2].len = 6);
  m = match("(\\d*)(\\d+)", "123456", (tpre_opts_t) { 0 });
  assert(m.found);
  assert(m.ngroups == 3);
  assert(m.groups[1].begin == 0);
  assert(m.groups[1].len == 5);
  assert(m.groups[2].begin == 5);
  assert(m.groups[2].len = 1);
}
