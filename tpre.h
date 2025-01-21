#ifndef _TPRE_H
#define _TPRE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include "allib/dynamic_list/dynamic_list.h"

typedef uint8_t tpre_groupid_t;
typedef int16_t tpre_nodeid_t;
typedef uint8_t tpre_backtrack_t;
typedef struct {
    uint8_t is_special;
    uint8_t val;
} tpre_pattern_t;

typedef struct {
    bool free;
    tpre_nodeid_t num_nodes;
    tpre_nodeid_t first_node;
    tpre_groupid_t max_group;

    tpre_pattern_t  * i_pat;
    tpre_nodeid_t  * i_ok;
    tpre_nodeid_t  * i_err;
    tpre_backtrack_t* i_backtrack;
    tpre_groupid_t * i_group;
} tpre_re_t;

typedef struct {
    bool found;
    size_t ngroups;
    /** will have null terminators ONLY IF LEN > 0 */
    DynamicList TYPES(char) * groups;
} tpre_match_t;

void tpre_match_free(tpre_match_t match);
tpre_match_t tpre_match(tpre_re_t const* re, const char * str);
void tpre_match_dump(tpre_match_t match, FILE* out);

/** 0 = ok; errsOut can be null */
int tpre_compile(tpre_re_t* out, char const * str, DynamicList TYPES(char *) * errsOut);
void tpre_free(tpre_re_t re);

#ifdef __cplusplus
}
#endif

#endif
