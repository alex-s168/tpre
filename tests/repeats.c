#include "testing.h"

int main()
{
  tpre_match_t m;

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
