#ifndef _TPRE_COMPILER_H
#define _TPRE_COMPILER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "tpre_common.h"

typedef struct
{
  size_t pat_byte_loc;
  char* message;
} tpre_err_t;

typedef struct
{
  tpre_err_t* items;
  size_t len;
} tpre_errs_t;

/** negative on failure */
int tpre_find_group(tpre_re_t const* re, char const* name);

// zero initialized is default
typedef struct
{
  // if true, `^` can be used to anchor at start
  bool start_unanchored;
  // if true, `$` can be used to anchor at end
  bool end_unanchored;

  // TODO:
  // bool startend_is_line;
  // bool ignore_case;
} tpre_opts_t;

/** 0 = ok; errsOut can be null */
int tpre_compile(
    tpre_re_t* out,
    char const* str,
    tpre_errs_t* errs_out,
    tpre_opts_t opts);
void tpre_free(tpre_re_t re);

void tpre_errs_free(tpre_errs_t errs);

#ifdef __cplusplus
}
#endif

#endif
