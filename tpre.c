#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>

typedef uint8_t group_id_t;
typedef int16_t node_id_t;
typedef uint8_t backtrack_t;
typedef struct {
    uint8_t is_special;
    uint8_t val;
} pattern_t;

#define NODE_DONE ((node_id_t) -2)
#define NODE_ERR  ((node_id_t) -1)

#define SPECIAL_ANY   (0)
#define SPECIAL_SPACE (1)
#define SPECIAL_END   (2)
#define NO(c) ((pattern_t) {.is_special = 0,.val=(uint8_t)c})
#define SP(c) ((pattern_t) {.is_special = 1,.val=(uint8_t)c})

typedef struct {
    node_id_t num_nodes;
    node_id_t first_node;
    group_id_t max_group;

    pattern_t  * i_pat;
    node_id_t  * i_ok;
    node_id_t  * i_err;
    backtrack_t* i_backtrack;
    group_id_t * i_group;
} regex_t;

static node_id_t regex_add_node(
        regex_t* re,
        pattern_t   pat,
        node_id_t   ok,
        node_id_t   err,
        backtrack_t backtrack,
        group_id_t  group)
{
#define realloc(k) \
    re->i_##k = realloc(re->i_##k, sizeof(*re->i_##k) * re->num_nodes); \
    re->i_##k[re->num_nodes] = k;

    realloc(pat);
    realloc(ok);
    realloc(err);
    realloc(backtrack);
    realloc(group);

#undef realloc

    if (group > re->max_group) re->max_group = group;

    return re->num_nodes ++;
}

typedef struct {
    bool found;
    size_t ngroups;
    struct {
        size_t len;
        /** null-terminated */
        char * str;
    } * groups;
} re_match_t;

void regex_match_free(re_match_t match)
{
    for (size_t i = 0; i < match.ngroups; i ++)
        free(match.groups[i].str);
    free(match.groups);
}

static void regex_match_group_put(re_match_t* match, group_id_t group, char c)
{
    if (group != 0) regex_match_group_put(match, 0, c);
    match->groups[group].str = realloc(match->groups[group].str, sizeof(char) * (++ match->groups[group].len));
    strcat(match->groups[group].str, (char[]) { c, '\0' });
}

static bool pattern_match(pattern_t pat, char src)
{
    if (!pat.is_special) 
        return src == (char) pat.val;

    switch (pat.val)
    {
        case SPECIAL_ANY:
            return true;

        case SPECIAL_SPACE:
            return isspace(src);

        case SPECIAL_END:
            return src == '\0';

        default:
            assert(false);
            exit(1);
            return false;
    }
}

re_match_t regex_match(regex_t const* re, const char * str)
{
    re_match_t match;
    match.ngroups = re->max_group + 1;
    match.groups = malloc(sizeof(*match.groups) * match.ngroups);
    for (size_t i = 0; i < match.ngroups; i ++) {
        match.groups[i].len = 1;
        match.groups[i].str = calloc(1,1);
    }

    node_id_t cursor = re->first_node;
    while (cursor >= 0)
    {
        if (pattern_match(re->i_pat[cursor], *str)) {
            regex_match_group_put(&match, re->i_group[cursor], *str);
            cursor = re->i_ok[cursor];
            if (*str) str ++;
        } else {
            backtrack_t bt = re->i_backtrack[cursor];
            str -= bt;
            group_id_t g = re->i_group[cursor];
            match.groups[g].len -= bt;
            match.groups[g].str[match.groups[g].len - 1] = '\0';
            cursor = re->i_err[cursor];
        }
    }

    match.found = cursor == NODE_DONE;

    // not including nt
    for (size_t i = 0; i < match.ngroups; i ++)
        match.groups[i].len --;
    return match;
}

void regex_match_print(re_match_t match, FILE* out)
{
    if (match.found) {
        fprintf(out, "matches:\n");
        for (size_t i = 0; i < match.ngroups; i ++) {
            fprintf(out, "  group %zu: %s\n", i, match.groups[i].str);
        }
    }
    else {
        fprintf(out, "does not match\n");
    }
}

int main()
{
    // \s*?(red|green|blue)?\s*?(car|train)\s*?

    regex_t re;
    re.i_ok  = (node_id_t[]) { 4,5,6,0, 7,8,9,15,10,11,12,15,15,15,16,17,18,19,20,23,21,23,-2,23 };
    re.i_err = (node_id_t[]) { 1,2,3,15,1,2,3,1, 2, 3, 2, 3, 2, -1,13,14,13,14,13,14,13,13,-1,22 };
    re.i_pat = (pattern_t[]) {
        NO('r'), NO('g'), NO('b'), SP(SPECIAL_SPACE),
        NO('e'), NO('r'), NO('l'), NO('d'), NO('e'), NO('u'), NO('e'), NO('e'), NO('n'),
        SP(SPECIAL_SPACE),
        NO('t'), NO('c'), NO('r'), NO('a'), NO('a'), NO('r'), NO('i'), NO('n'),
        SP(SPECIAL_END), SP(SPECIAL_SPACE)
    };
    re.i_backtrack = (backtrack_t[]) { 0,0,0,0,1,1,1,2,2,2,3,3,4,0,0,0,1,1,2,2,3,4,0,0 };
    re.i_group = (group_id_t[]) { 1,1,1,0,1,1,1,1,1,1,1,1,1,0,2,2,2,2,2,2,2,2,0,0 };
    re.first_node = 0;
    re.max_group = 2;
    re.num_nodes = 24;

    re_match_t m;

    m = regex_match(&re, " green  car ");
    regex_match_print(m, stdout);
    regex_match_free(m);

    m = regex_match(&re, " bluetrain ");
    regex_match_print(m, stdout);
    regex_match_free(m);

    m = regex_match(&re, " train ");
    regex_match_print(m, stdout);
    regex_match_free(m);
}
