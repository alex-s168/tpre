#ifndef _TPRE_H
#define _TPRE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

typedef uint8_t tpre_groupid_t;
typedef int16_t tpre_nodeid_t;
typedef uint8_t tpre_backtrack_t;
typedef struct {
    uint8_t is_special;
    uint8_t val;
} tpre_pattern_t;

typedef struct {
    size_t pat_byte_loc;
    char* message;
} tpre_err_t;

typedef struct {
    tpre_err_t *items;
    size_t      len;
} tpre_errs_t;

typedef struct {
    bool free;
    tpre_nodeid_t num_nodes;
    tpre_nodeid_t first_node;
    tpre_groupid_t max_group;

    tpre_pattern_t  * i_pat;
    tpre_nodeid_t   * i_ok;
    tpre_nodeid_t   * i_err;
    tpre_backtrack_t* i_backtrack;
    tpre_groupid_t  * i_group;
} tpre_re_t;

typedef int32_t tpre_src_loc_t;

typedef struct {
    tpre_src_loc_t begin;
    /** if is 0, then no match */
    size_t len;
} tpre_group_t;

typedef struct {
    bool found;
    size_t ngroups;
    tpre_group_t * groups;
} tpre_match_t;

void tpre_match_free(tpre_match_t match);
tpre_match_t tpre_match(tpre_re_t const* re, const char * str);
void tpre_match_dump(tpre_match_t match, char const * matched_str, FILE* out);

/** 0 = ok; errsOut can be null */
int tpre_compile(tpre_re_t* out, char const * str, tpre_errs_t* errs_out);
void tpre_free(tpre_re_t re);

void tpre_errs_free(tpre_errs_t errs);

#ifdef __cplusplus
}
#endif

#endif
