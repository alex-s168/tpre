#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "tpre.h"

int match_equal(tpre_match_t const* a, tpre_match_t const* b)
{
  if (a->found != b->found)
    return 0;
  if (a->ngroups != b->ngroups)
    return 0;
  for (size_t i = 0; i < a->ngroups; i++)
  {
    if (a->groups[i].begin != b->groups[i].begin)
      return 0;
    if (a->groups[i].len != b->groups[i].len)
      return 0;
  }
  return 1;
}

tpre_match_t match(char const* pat, char const* str)
{
  size_t strl = strlen(str);
  char* buf = malloc(strl + 200);
  memcpy(buf, str, strl);
  memset(buf + strl, 'a', 100);

  tpre_re_t re;
  tpre_errs_t errs;
  if (tpre_compile(&re, pat, &errs))
    assert(false && "compile fail");
  tpre_match_t a = tpre_match(&re, str);
  tpre_match_t b = tpre_matchn(&re, buf, strl);
  if (!match_equal(&a, &b))
    assert(false && "matchn() not same result as match()");
  return a;
}

int main()
{
  tpre_match_t m;
  m = match("[ab(cd)e*+]", "(");
  assert(m.found);
  m = match("[ab(cd)e*+]", ")");
  assert(m.found);
  m = match("[ab(cd)e*+]", "+");
  assert(m.found);
  m = match("[ab(cd)e*+]", "*");
  assert(m.found);
  m = match("[ab(cd)e*+]", "!");
  assert(!m.found);
}
