#ifndef _TPRE_COMMON_H
#define _TPRE_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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


#ifdef __cplusplus
}
#endif

#endif
