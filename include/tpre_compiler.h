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

  // `^`/start anchor and `$`/end anchor refer to line start / line end
  bool startend_is_line;

  bool ignore_case;

  // ignore whitespaces in regex pattern. If yo want to a match a whitespace, use `\s`, `\ `, `\n`...
  bool ignore_whitespace_in_pat;

  // input is utf8. If false (default), is only ASCII.
  // This is a bit slower than without unicode.
  //
  // This mainly affects the behaviour of `*`, matching any unicode character when enabled.
  bool utf8;

  // all greedy quanitifers will be lazy
  bool ungreedy;

  // dot matches newline
  bool single_line;
} tpre_opts_t;

typedef enum
{
  TPRE_OPT_ANCHORED,
  TPRE_OPT_START_ANCHORED,
  TPRE_OPT_END_ANCHORED,
  TPRE_OPT_STARTEND_IS_LINE,
  TPRE_OPT_IGNORE_CASE,
  TPRE_OPT_IGNORE_PAT_WHITESPACE,
  TPRE_OPT_UTF8,
  TPRE_OPT_UNGREEDY,
  TPRE_OPT_SINGLE_LINE,
} tpre_opt_key_t;

bool tpre_opt_getb(tpre_opts_t const* opts, tpre_opt_key_t key);
void tpre_opt_setb(tpre_opts_t* opts, tpre_opt_key_t key, bool val);

int tpre_opt_parsekey(tpre_opt_key_t* out, char const* name);
int tpre_opt_parse(tpre_opts_t* out, char const* opts);

typedef enum
{
  TPRE_FSM_PAT_START,
  TPRE_FSM_PAT_END,
  TPRE_FSM_PAT_ONEOF,
  TPRE_FSM_PAT_ANY_ASCII_EXCEPT,
  // only succeeds if there is a second byte
  TPRE_FSM_PAT_ANY_UTF8_BYTE0,
  // only succeeds if there is a third byte
  TPRE_FSM_PAT_ANY_UTF8_BYTE1,
  // only succeeds if there is a fourth byte
  TPRE_FSM_PAT_ANY_UTF8_BYTE2,
} tpre_fsm_patkind;

typedef struct
{
  tpre_fsm_patkind kind;
  union
  {
    // when ONEOF, this is list that is allowed. when ANY_ASCII_EXCEPT, this is list of unallowed
    struct
    {
      size_t len;
      uint8_t* items;
    } ascii;
  } v;
} tpre_fsm_pat_t;

void tpre_fsm_pat_free(tpre_fsm_pat_t pat);

typedef struct tpre_fsm_node tpre_fsm_node_t;

typedef struct
{
  tpre_fsm_pat_t pat;
  tpre_fsm_node_t* then;
  uint16_t group;
} tpre_fsm_case_t;

struct tpre_fsm_node
{
  /* private: */
  bool _gc_flag;

  struct
  {
    size_t len;
    tpre_fsm_case_t* items;
  } cases;

  tpre_fsm_node_t* els;

  struct
  {
    bool known;
    uint32_t by;
  } backtrack;
};

typedef struct
{
  /* ref to special node that marks end of pattern, with success status */
  tpre_fsm_node_t* nd_ok;
  /* ref to special node that marks end of pattern, with fail status */
  tpre_fsm_node_t* nd_err;
  tpre_fsm_node_t* first;

  uint16_t total_num_groups, num_named_groups;
  uint16_t first_named_group;
  char const** named_groups;
} tpre_fsm_t;

void tpre_fsm_init(tpre_fsm_t* fsm);
void tpre_fsm_gc(tpre_fsm_t* fsm);
void tpre_fsm_free(tpre_fsm_t* fsm);
tpre_fsm_node_t* tpre_fsm_mknd(tpre_fsm_t* fsm);

int tpre2fsm(
    tpre_fsm_t* out,
    char const* str,
    tpre_errs_t* errs_out,
    tpre_opts_t opts);

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
