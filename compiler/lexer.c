#include "lexer.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "../shared.h"
#include "utils.h"
#include "include/tpre_compiler.h"

// TODO "UNDOCUMENTED" SYNTAX:
//
//  backref to group 39:   \39
//  backref to group 39:   \g{39}
//  backref to group hey:  \g{hey}

bool tprec_lex(ReTk* tkOut, bool isOneOf, char const** reader)
{
  if (!**reader)
    return false;

  if ((*reader)[1] == '-')
  {
    tkOut->range.from = **reader;
    (*reader) += 2;
    tkOut->range.to = **reader;
    (*reader)++;
    tkOut->ty = MatchRange;
    return true;
  }

  if (!isOneOf && **reader == '.')
  {
    (*reader)++;
    tkOut->ty = Match;
    tkOut->match = SP(SPECIAL_ANY);
    return true;
  }

  if (**reader == '\\')
  {
    (*reader)++;
    char c = **reader;

    if (isdigit(c))
    {
      long num = strtol(*reader, (char**) reader, 10);
      if (isOneOf)
      {
        // TODO: document this syntax: matches char with this ascii code
        tkOut->ty = Match;
        tkOut->match = (tpre_pattern_t) {
          .invert = 0, .is_special = 0, .val = (char) num
        };
      }
      else
      {
        // backreference
        tkOut->ty = BackrefId;
        tkOut->group_id = (tpre_groupid_t) num;
        return true;
      }
    }
    else if (!isOneOf && c == 'g')
    {
      (*reader)++;
      // also backreference
      if (**reader != '{')
        return false;
      (*reader)++;
      if (**reader == '+' ||
          **reader == '-')  // not yet implemented
        return false;

      char const* begin = *reader;
      char const* close = strchr(*reader, '}');
      if (!close)
        return false;
      *reader = close + 1;
      size_t len = close - begin;

      if (isdigit(*begin))
      {
        char* end;
        long num = strtol(begin, &end, 10);
        if (end != begin + len)
          return false;
        tkOut->ty = BackrefId;
        tkOut->group_id = (tpre_groupid_t) num;
        return true;
      }
      else
      {
        if (len >= sizeof(tkOut->group_name))
          return false;
        tkOut->ty = BackrefName;
        memcpy(tkOut->group_name, begin, len);
        tkOut->group_name[len] = '\0';
        return true;
      }
    }
    else
    {
      (*reader)++;
      tpre_pattern_t m;
      switch (c)
      {
        case 't': m = NO('\t'); break;
        case 'r': m = NO('\r'); break;
        case 'n': m = NO('\n'); break;
        case 'f': m = NO('\f'); break;
        case 's': m = SP(SPECIAL_SPACE); break;
        case 'S':
          m = SP(SPECIAL_SPACE);
          m.invert = 1;
          break;
        case 'd': m = SP(SPECIAL_DIGIT); break;
        case 'D':
          m = SP(SPECIAL_DIGIT);
          m.invert = 1;
          break;
        case 'w': m = SP(SPECIAL_WORDC); break;
        case 'W':
          m = SP(SPECIAL_WORDC);
          m.invert = 1;
          break;
        default: m = NO(c); break;
      }
      tkOut->ty = Match;
      tkOut->match = m;
      return true;
    }
  }

  if (!isOneOf && **reader == '*')
  {
    (*reader)++;
    if (**reader == '?')
    {
      (*reader)++;
      tkOut->ty = LazyRepeatLeast0;
    }
    else
    {
      tkOut->ty = GreedyRepeatLeast0;
    }
    return true;
  }

  if (!isOneOf && **reader == '+')
  {
    (*reader)++;
    if (**reader == '?')
    {
      (*reader)++;
      tkOut->ty = LazyRepeatLeast1;
    }
    else
    {
      tkOut->ty = GreedyRepeatLeast1;
    }
    return true;
  }

  if (!isOneOf && **reader == '?')
  {
    (*reader)++;
    tkOut->ty = OrNot;
    return true;
  }

  if (!isOneOf && **reader == '(')
  {
    (*reader)++;
    if (**reader == '?')
    {
      (*reader)++;
      if (**reader == ':')
      {
        (*reader)++;
        tkOut->ty = CaptureGroupOpenNoCapture;
        return true;
      }
      else if (**reader == '#')
      {
        while (**reader && **reader != ')')
          (*reader)++;
        if (!**reader)
          return false;
        (*reader)++;
      }
      else if (**reader == '\'')
      {
        (*reader)++;
        const char* begin = *reader;
        tkOut->ty = CaptureGroupOpenNamed;
        for (; **reader && **reader != '\''; (*reader)++)
          ;
        size_t len = *reader - begin;
        if (!**reader)
          return false;
        (*reader)++;
        if (len >= sizeof(tkOut->group_name))
          return false;
        memcpy(tkOut->group_name, begin, len);
        tkOut->group_name[len] = '\0';
        return true;
      }

      return false;
    }
    else
    {
      tkOut->ty = CaptureGroupOpen;
      return true;
    }
  }

  if (!isOneOf && **reader == ')')
  {
    (*reader)++;
    tkOut->ty = CaptureGroupClose;
    return true;
  }

  if (!isOneOf && **reader == '[')
  {
    (*reader)++;
    if (**reader == '^')
    {
      (*reader)++;
      tkOut->ty = OneOfOpenInvert;
      return true;
    }
    tkOut->ty = OneOfOpen;
    return true;
  }

  if (isOneOf && **reader == '[')
  {
    // TODO: parse posix character classes
  }

  if (**reader == ']')
  {
    (*reader)++;
    tkOut->ty = OneOfClose;
    return true;
  }

  if (!isOneOf && **reader == '|')
  {
    (*reader)++;
    tkOut->ty = OrElse;
    return true;
  }

  if (!isOneOf && **reader == '^')
  {
    (*reader)++;
    tkOut->ty = Match;
    tkOut->match = SP(SPECIAL_START);
    return true;
  }

  if (!isOneOf && **reader == '$')
  {
    (*reader)++;
    tkOut->ty = Match;
    tkOut->match = SP(SPECIAL_END);
    return true;
  }

  tkOut->match = NO(**reader);
  (*reader)++;
  tkOut->ty = Match;
  return true;
}

int tprec_lexe(TkL* out, tpre_errs_t* errs, const char* src)
{
  memset(out, 0, sizeof(TkL));

  ReTk tok;
  const char* reader = src;
  bool isOneOf = false;
  while (tprec_lex(&tok, isOneOf, &reader))
  {
    tok.where = reader - src;
    tprec_TkL_add(out, tok);
    if (tk_isOneOfOpen(tok.ty))
      isOneOf = true;
    else if (tok.ty == OneOfClose)
      isOneOf = false;
  }

  if (*reader)
  {
    tprec_add_err(errs, reader - src, "lexer error");
    free(out->tokens);
    memset(out, 0, sizeof(TkL));
    return 1;
  }

  return 0;
}


size_t tprec_TkL_len(TkL const* li)
{
  return li->len;
}

bool tprec_TkL_peek(ReTk* out, TkL* li)
{
  if (tprec_TkL_len(li) == 0)
    return false;
  *out = li->tokens[0];
  return true;
}

ReTk tprec_TkL_get(TkL const* li, size_t i)
{
  return li->tokens[i];
}

bool tprec_TkL_take(ReTk* out, TkL* li)
{
  if (tprec_TkL_len(li) == 0)
    return false;
  *out = tprec_TkL_get(li, 0);
  li->tokens++;
  li->len--;
  li->cap--;
  return true;
}

void tprec_TkL_free(TkL* li)
{
  if (li->allocptr && li->cap)
    free(li->allocptr);
  li->allocptr = 0;
  li->cap = 0;
}

TkL tprec_TkL_copy_range(TkL const* li, size_t first, size_t num)
{
  TkL out = { 0 };
  out.len = num;
  out.cap = num;
  out.allocptr = malloc(li->cap * sizeof(ReTk));
  out.tokens = out.allocptr;
  if (out.allocptr)
    memcpy(out.tokens, li->tokens + first, num * sizeof(ReTk));
  return out;
}

void tprec_TkL_add(TkL* li, ReTk tk)
{
  if (li->len + 1 > li->cap)
  {
    size_t newCap = li->cap + 16;
    ReTk* new = realloc(li->allocptr, newCap * sizeof(ReTk));
    if (!new)
    {
      li->oom = 1;
      return;
    }
    li->cap = newCap;
    li->allocptr = new;
    li->tokens = new;
  }
  li->tokens[li->len++] = tk;
}
