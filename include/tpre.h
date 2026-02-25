#ifndef _TPRE_H
#define _TPRE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef uint8_t tpre_groupid_t;
typedef int16_t tpre_nodeid_t;
typedef uint8_t tpre_backtrack_t;
typedef struct
{
  uint8_t is_special;
  uint8_t val;
  uint8_t invert;
} tpre_pattern_t;

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

typedef struct
{
  tpre_pattern_t pat;
  tpre_nodeid_t ok, err;
  tpre_backtrack_t backtrack;
  tpre_groupid_t group;
} tpre_re_node_t;

typedef struct
{
  bool free;
  tpre_nodeid_t num_nodes;
  tpre_nodeid_t first_node;
  tpre_groupid_t max_group;

  tpre_groupid_t first_named_group;
  char** named_groups;
  tpre_groupid_t num_named_groups;

  tpre_re_node_t* i;
} tpre_re_t;

typedef int32_t tpre_src_loc_t;

typedef struct
{
  tpre_src_loc_t begin;
  /** if is 0, then no match */
  size_t len;
} tpre_group_t;

/**
 * depends on lifetime of [tpre_re_t]
 *
 * all named capture groups get their IDs appended to the unnamed capture groups 
 *
 * group 0 currently is always empty, but will contain whole matched string in the future
 */
typedef struct
{
  bool found;

  // including group 0
  size_t ngroups;
  tpre_group_t* groups;
} tpre_match_t;

tpre_match_t tpre_match(tpre_re_t const* re, const char* str);

/** a tiny bit slower than tpre_match() */
tpre_match_t
tpre_matchn(tpre_re_t const* re, const char* str, size_t strl);

/** matched_str does not have to be mull terminated because it only prints the slices of it that match */
void tpre_match_dump(
    tpre_re_t const* re,
    tpre_match_t match,
    char const* matched_str,
    FILE* out);

void tpre_match_free(tpre_match_t match);

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
