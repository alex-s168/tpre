#include "testing.h"

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
}
