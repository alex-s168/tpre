#ifndef _TPREC_UTILS_H
#define _TPREC_UTILS_H

#include <stddef.h>
#include "include/tpre_compiler.h"

char* tprec_strdup(char const* s);


void tpre_errs_free(tpre_errs_t errs);
void tprec_add_err(
    tpre_errs_t* errs, size_t byte_loc, char const* fmt, ...);


void tprec_re_setnode(
    tpre_re_t* re, tpre_nodeid_t id, tpre_re_node_t nd);
tpre_nodeid_t tprec_re_addnode(tpre_re_t* re, tpre_re_node_t nd);
tpre_nodeid_t tprec_re_resvnode(tpre_re_t* re);

#endif
