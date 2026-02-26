#ifndef _TPRE_RUNTIME_H
#define _TPRE_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "tpre_common.h"

tpre_match_t
tpre_matchn(tpre_re_t const* re, const char* str, size_t strl);

/** matched_str does not have to be mull terminated because it only prints the slices of it that match */
void tpre_match_dump(
    tpre_re_t const* re,
    tpre_match_t match,
    char const* matched_str,
    FILE* out);

void tpre_match_free(tpre_match_t match);

#ifdef __cplusplus
}
#endif

#endif
