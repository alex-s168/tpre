#ifndef _TPREC_LEXER_H
#define _TPREC_LEXER_H

#include <stddef.h>
#include "include/tpre_common.h"
#include "include/tpre_compiler.h"

typedef enum
{
  Match,
  MatchRange,
  LazyRepeatLeast0,
  LazyRepeatLeast1,
  GreedyRepeatLeast0,
  GreedyRepeatLeast1,
  OrNot,
  CaptureGroupOpen,
  CaptureGroupOpenNoCapture,
  CaptureGroupOpenNamed,
  CaptureGroupClose,
  OneOfOpen,
  OneOfOpenInvert,
  OneOfClose,
  OrElse,
  BackrefId,
  BackrefName,
} ReTkTy;

typedef struct
{
  ReTkTy ty;
  size_t where;
  union
  {
    tpre_pattern_t match;
    char group_name[20];
    tpre_groupid_t group_id;
    struct
    {
      char from, to;
    } range;
  };
} ReTk;

static inline bool tk_isCaptureGroupOpen(ReTkTy ty)
{
  return ty == CaptureGroupOpen || ty == CaptureGroupOpenNamed ||
      ty == CaptureGroupOpenNoCapture;
}

static inline bool tk_isOneOfOpen(ReTkTy ty)
{
  return ty == OneOfOpen || ty == OneOfOpenInvert;
}

static inline bool tk_isPostfix(ReTkTy ty)
{
  return ty == OrNot || ty == GreedyRepeatLeast0 ||
      ty == GreedyRepeatLeast1 || ty == LazyRepeatLeast0 ||
      ty == LazyRepeatLeast1;
}


typedef struct
{
  int oom;
  void* allocptr;
  ReTk* tokens;
  size_t cap;
  size_t len;
} TkL;

size_t tprec_TkL_len(TkL const* li);
bool tprec_TkL_peek(ReTk* out, TkL* li);
ReTk tprec_TkL_get(TkL const* li, size_t i);
bool tprec_TkL_take(ReTk* out, TkL* li);
void tprec_TkL_free(TkL* li);
TkL tprec_TkL_copy_range(TkL const* li, size_t first, size_t num);
void tprec_TkL_add(TkL* li, ReTk tk);

int tprec_lexe(
    TkL* out,
    tpre_errs_t* errs,
    const char* src,
    tpre_opts_t const* opts);

#endif

#if defined(USING_TPREC) && !defined(TkL_len)
#define TkL_len tprec_TkL_len
#define TkL_peek tprec_TkL_peek
#define TkL_get tprec_TkL_get
#define TkL_take tprec_TkL_take
#define TkL_free tprec_TkL_free
#define TkL_copy_range tprec_TkL_copy_range
#define TkL_add tprec_TkL_add
#endif
