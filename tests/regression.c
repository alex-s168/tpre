#include <assert.h>
#include "testing.h"

int main()
{
  tpre_match_t m;

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

  // regression 3
  m = match("((?:a*))", "", (tpre_opts_t) { 0 });
  assert(m.found);
  m = match(
      "((?:a*))", "",
      (tpre_opts_t) {
        .start_unanchored = 1, .end_unanchored = 1 });
  assert(m.found);

  // regression 4
  m = match("((?:(?:a*)*))", "", (tpre_opts_t) { 0 });
  assert(m.found);
  m = match(
      "((?:(?:a*)*))", "",
      (tpre_opts_t) {
        .start_unanchored = 1, .end_unanchored = 1 });
  assert(m.found);

  // regression 5
  m = match("(a?a)", "a", (tpre_opts_t) { 0 });
  assert(m.found);
  assert(m.ngroups == 2);
  assert(m.groups[1].begin == 0);
  assert(m.groups[1].len == 1);

  // regression 6
  m = match(
      "((?:(?:(?:a?)a))$)", "aca",
      (tpre_opts_t) {
        .start_unanchored = 1, .end_unanchored = 1 });
  assert(m.found);
  assert(m.ngroups == 2);
  assert(m.groups[1].begin == 2);
  assert(m.groups[1].len == 1);
}
