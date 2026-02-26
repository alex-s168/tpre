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
