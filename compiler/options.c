#include "tpre_compiler.h"

bool tpre_opt_getb(tpre_opts_t const* opts, tpre_opt_key_t key)
{
  switch (key)
  {
    case TPRE_OPT_ANCHORED:
      return !opts->start_unanchored && !opts->end_unanchored;
    case TPRE_OPT_START_ANCHORED: return !opts->start_unanchored;
    case TPRE_OPT_END_ANCHORED:   return !opts->end_unanchored;
    case TPRE_OPT_STARTEND_IS_LINE:
      return opts->startend_is_line;
    case TPRE_OPT_IGNORE_CASE: return opts->ignore_case;
    case TPRE_OPT_IGNORE_PAT_WHITESPACE:
      return opts->ignore_whitespace_in_pat;
    case TPRE_OPT_UTF8:        return opts->utf8;
    case TPRE_OPT_UNGREEDY:    return opts->ungreedy;
    case TPRE_OPT_SINGLE_LINE: return opts->single_line;
  }
}

void tpre_opt_setb(tpre_opts_t* opts, tpre_opt_key_t key, bool val)
{
  switch (key)
  {
    case TPRE_OPT_ANCHORED:
      opts->start_unanchored = !val;
      opts->end_unanchored = !val;
      break;
    case TPRE_OPT_START_ANCHORED:
      opts->start_unanchored = !val;
      break;
    case TPRE_OPT_END_ANCHORED:
      opts->end_unanchored = !val;
      break;
    case TPRE_OPT_STARTEND_IS_LINE:
      opts->startend_is_line = val;
      break;
    case TPRE_OPT_IGNORE_CASE: opts->ignore_case = val; break;
    case TPRE_OPT_IGNORE_PAT_WHITESPACE:
      opts->ignore_whitespace_in_pat = val;
      break;
    case TPRE_OPT_UTF8:        opts->utf8 = val; break;
    case TPRE_OPT_UNGREEDY:    opts->ungreedy = val; break;
    case TPRE_OPT_SINGLE_LINE: opts->single_line = val; break;
  }
}

int tpre_opt_parsekey(tpre_opt_key_t* out, char const* name)
{
  if (!out || !name || !*name)
    return 1;
  if (name[1])
    return 1;
  switch (*name)
  {
    case 'm': *out = TPRE_OPT_STARTEND_IS_LINE; return 0;
    case 'i': *out = TPRE_OPT_IGNORE_CASE; return 0;
    case 'x': *out = TPRE_OPT_IGNORE_PAT_WHITESPACE; return 0;
    case 's': *out = TPRE_OPT_SINGLE_LINE; return 0;
    case 'u': *out = TPRE_OPT_UTF8; return 0;
    case 'U': *out = TPRE_OPT_UNGREEDY; return 0;
    case 'A': *out = TPRE_OPT_ANCHORED; return 0;
    default:  return 1;
  }
}

int tpre_opt_parse(tpre_opts_t* out, char const* opts)
{
  if (!opts || !out)
    return 1;
  tpre_opt_key_t k;
  char s[2] = { 0 };
  for (; *opts; opts++)
  {
    s[0] = *opts;
    if (tpre_opt_parsekey(&k, s))
      return 1;
    tpre_opt_setb(out, k, true);
  }
  return 0;
}
