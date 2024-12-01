#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include "allib/dynamic_list/dynamic_list.h"
#include "allib/kallok/kallok.h"
#include "tpre.h"
#define CREFLECT(args, ...) __VA_ARGS__

#define NODE_DONE ((tpre_nodeid_t) -2)
#define NODE_ERR  ((tpre_nodeid_t) -1)

#define SPECIAL_ANY   (0)
#define SPECIAL_SPACE (1)
#define SPECIAL_END   (2)
#define NO(c) ((tpre_pattern_t) {.is_special = 0,.val=(uint8_t)c})
#define SP(c) ((tpre_pattern_t) {.is_special = 1,.val=(uint8_t)c})

static tpre_nodeid_t tpre_re_addnode(
        tpre_re_t* re,
        tpre_pattern_t   pat,
        tpre_nodeid_t   ok,
        tpre_nodeid_t   err,
        tpre_backtrack_t backtrack,
        tpre_groupid_t  group)
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

void tpre_match_free(tpre_match_t match)
{
    for (size_t i = 0; i < match.ngroups; i ++)
        DynamicList_clear(&match.groups[i]);
    free(match.groups);
}

static void tpre_match_group_put(tpre_match_t* match, tpre_groupid_t group, char c)
{
    if (group == 0) return;
    DynamicList_add(&match->groups[group], &c);
}

static bool pattern_match(tpre_pattern_t pat, char src)
{
    if (!pat.is_special) 
        return src == (char) pat.val;

    switch (pat.val)
    {
        case SPECIAL_ANY:
            return true;

        case SPECIAL_SPACE:
            return src == ' ' || src == '\n' || src == '\t' || src == '\r';

        case SPECIAL_END:
            return src == '\0';

        default:
            return false;
    }
}

tpre_match_t tpre_match(tpre_re_t const* re, const char * str)
{
    tpre_match_t match;
    match.ngroups = re->max_group + 1;
    match.groups = malloc(sizeof(*match.groups) * match.ngroups);
    for (size_t i = 0; i < match.ngroups; i ++) {
        DynamicList_init(&match.groups[i], sizeof(char), getLIBCAlloc(), 0);
    }

    tpre_nodeid_t cursor = re->first_node;
    while (cursor >= 0)
    {
        if (pattern_match(re->i_pat[cursor], *str)) {
            tpre_match_group_put(&match, re->i_group[cursor], *str);
            cursor = re->i_ok[cursor];
            if (*str) str ++;
        } else {
            tpre_backtrack_t bt = re->i_backtrack[cursor];
            str -= bt;
            tpre_groupid_t g = re->i_group[cursor];
            if (bt > 0) {
                size_t glen = match.groups[g].fixed.len;
                if (match.groups[g].fixed.len >= bt)
                    match.groups[g].fixed.len -= bt;
            }
            cursor = re->i_err[cursor];
        }
    }

    match.found = cursor == NODE_DONE;

    for (size_t i = 0; i < match.ngroups; i ++)
        if (match.groups[i].fixed.len > 0)
            DynamicList_add(&match.groups[i], (char[]) { '\0' });

    return match;
}

void tpre_match_dump(tpre_match_t match, FILE* out)
{
    if (match.found) {
        fprintf(out, "does match\n");
        for (size_t i = 0; i < match.ngroups; i ++) {
            fprintf(out, "  group %zu: %s\n", i, match.groups[i].fixed.len > 0 ? ((const char *) match.groups[i].fixed.data) : "");
        }
    }
    else {
        fprintf(out, "does not match\n");
    }
}


CREFLECT((),
typedef enum {
    Match,
    MatchRange,
    RepeatLazyLeast0,
    RepeatLeast0,
    RepeatLazyLeast1,
    RepeatLeast1,
    OrNot,
    CaptureGroupOpen,
    CaptureGroupOpenNoCapture,
    CaptureGroupOpenNamed,
    CaptureGroupClose,
    OneOfOpen,
    OneOfOpenInvert,
    OneOfClose,
    OrElse,
    StartOfStr,
} ReTkTy);
extern const char * ReTkTy_str[];

typedef struct {
    ReTkTy ty;
    union {
        tpre_pattern_t match;
        char group_name[20];
        struct { char from, to; } range;
    };
} ReTk;

static void ReTk_dump(ReTk tk, FILE* out)
{
    fprintf(out, "%s", ReTkTy_str[tk.ty]);
    if (tk.ty == Match) {
        tpre_pattern_t pat = tk.match;
        if (pat.is_special) fprintf(out, "(special: %u)", pat.val);
        else fprintf(out, "(%c)", (char) pat.val);
    } else if (tk.ty == CaptureGroupOpenNamed) {
        fprintf(out, "(%s)", tk.group_name);
    } else if (tk.ty == MatchRange) {
        fprintf(out, "(%c-%c)", tk.range.from, tk.range.to);
    }
}

static bool lex(ReTk* tkOut, char const* * reader)
{
    if (!**reader) return false;

    if ((*reader)[1] == '-') {
        tkOut->range.from = **reader;
        (*reader)+=2;
        tkOut->range.to = **reader;
        (*reader)++;
        tkOut->ty = MatchRange;
        return true;
    }

    if (**reader == '.') {
        (*reader)++;
        tkOut->ty = Match;
        tkOut->match = SP(SPECIAL_ANY);
        return true;
    }

    if (**reader == '\\') {
        (*reader)++;
        char c = **reader;
        (*reader)++;
        tpre_pattern_t m;
        switch (c) {
            case 't': m = NO('\t'); break;
            case 'r': m = NO('\r'); break;
            case 'n': m = NO('\n'); break;
            case 'f': m = NO('\f'); break;

            case 's': m = SP(SPECIAL_SPACE); break;

            default:  m = NO(c); break;
        }
        tkOut->ty = Match;
        tkOut->match = m;
        return true;
    }

    if (**reader == '*') {
        (*reader)++;
        if (**reader == '?') {
            (*reader)++;
            tkOut->ty = RepeatLazyLeast0;
        } else {
            tkOut->ty = RepeatLeast0;
        }
        return true;
    }

    if (**reader == '+') {
        (*reader)++;
        if (**reader == '?') {
            (*reader)++;
            tkOut->ty = RepeatLazyLeast1;
        } else {
            tkOut->ty = RepeatLeast1;
        }
        return true;
    }

    if (**reader == '?') {
        (*reader)++;
        tkOut->ty = OrNot;
        return true;
    }

    if (**reader == '(') {
        (*reader)++;
        if (**reader == '?') {
            (*reader)++;
            if (**reader == ':') {
                (*reader)++;
                tkOut->ty = CaptureGroupOpenNoCapture;
                return true;
            }

            if (**reader == '\'') {
                (*reader)++;
                const char * begin = *reader;
                tkOut->ty = CaptureGroupOpenNamed;
                for (; **reader && **reader != '\''; reader ++);
                size_t len = *reader - begin;
                if (**reader) (*reader)++;
                if (len > 19) len = 19;
                memcpy(tkOut->group_name, begin, len);
                tkOut->group_name[len] = '\0';
                return true;
            }

            return false;
        }

        tkOut->ty = CaptureGroupOpen;
        return true;
    }

    if (**reader == ')') {
        (*reader)++;
        tkOut->ty = CaptureGroupClose;
        return true;
    }

    if (**reader == '[') {
        (*reader)++;
        if (**reader == '^') {
            (*reader)++;
            tkOut->ty = OneOfOpenInvert;
            return true;
        }
        tkOut->ty = OneOfOpen;
        return true;
    }

    if (**reader == ']') {
        (*reader)++;
        tkOut->ty = OneOfClose;
        return true;
    }

    if (**reader == '|') {
        (*reader)++;
        tkOut->ty = OrElse;
        return true;
    }

    if (**reader == '^') {
        (*reader)++;
        tkOut->ty = StartOfStr;
        return true;
    }

    if (**reader == '$') {
        (*reader)++;
        tkOut->ty = Match;
        tkOut->match = SP(SPECIAL_END);
        return true;
    }

    tkOut->match = NO(**reader);
    (*reader)++;
    tkOut->ty = Match;
    return true;
}

static DynamicList TYPES(ReTk) lexe(const char * src)
{
    DynamicList out; DynamicList_init(&out, sizeof(ReTk), getLIBCAlloc(), 0);

    ReTk tok;
    const char* reader = src;
    while (lex(&tok, &reader)) {
        DynamicList_add(&out, &tok);
    }

    if (*reader) {
        fprintf(stderr, "error at arround %zu\n", reader - src);
        exit(1);
    }

    return out;
}

CREFLECT((remove_prefix(Node)),
typedef enum {
    NodeMatch,
    NodeChain,
    NodeOr,
    NodeMaybe,
    NodeRepeatLeast0,
    NodeRepeatLeast1,
    NodeLazyRepeatLeast0,
    NodeLazyRepeatLeast1,
    NodeCaptureGroup,
    NodeNamedCaptureGroup,
} NodeKind);

typedef struct Node Node;
struct Node {
    NodeKind kind;
    union {
        tpre_pattern_t match;

        struct {
            Node* a;
            Node* b;
        } chain;

        struct {
            Node* a;
            Node* b;
        } or;

        Node* maybe;

        Node* repeat;

        Node* capture;

        struct {
            char name[20];
            Node* group;
        } named_capture;
    };
};

static void Node_children(Node* nd, Node* childrenOut[2]) {
    childrenOut[0] = NULL;
    childrenOut[1] = NULL;

    switch (nd->kind) {
        case NodeMatch:
            break;

        case NodeChain:
            childrenOut[0] = nd->chain.a;
            childrenOut[1] = nd->chain.b;
            break;

        case NodeOr:
            childrenOut[0] = nd->or.a;
            childrenOut[1] = nd->or.b;
            break;

        case NodeMaybe:
            childrenOut[0] = nd->maybe;
            break;

        case NodeRepeatLeast0:
        case NodeRepeatLeast1:
        case NodeLazyRepeatLeast0:
        case NodeLazyRepeatLeast1:
            childrenOut[0] = nd->repeat;
            break;

        case NodeCaptureGroup:
            childrenOut[0] = nd->capture;
            break;

        case NodeNamedCaptureGroup:
            childrenOut[0] = nd->named_capture.group;
            break;
    }
}

static void Node_free(Node* node)
{
    if (!node) return;
    Node* children[2];
    Node_children(node, children);
    Node_free(children[0]);
    Node_free(children[1]);
    free(node);
}

typedef struct {
    DynamicList TYPES(ReTk) tokens;
} TkL;

static size_t TkL_len(TkL const* li) {
    return li->tokens.fixed.len;
}

static bool TkL_peek(ReTk* out, TkL* li) {
    if (TkL_len(li) == 0) return false;
    *out = * (ReTk*) FixedList_get(li->tokens.fixed, 0);
    return true;
}

static ReTk TkL_get(TkL const* li, size_t i) {
    return * (ReTk*) FixedList_get(li->tokens.fixed, i);
}

static bool TkL_take(ReTk* out, TkL* li) {
    if (TkL_len(li) == 0) return false;
    *out = TkL_get(li, 0);
    DynamicList_removeAt(&li->tokens, 0);
    return true;
}

static void TkL_free(TkL* li) {
    DynamicList_clear(&li->tokens);
}

static TkL TkL_copy_view(TkL const* li, size_t first, size_t num) {
    TkL out;
    DynamicList_init(&out.tokens, sizeof(ReTk), getLIBCAlloc(), num);
    for (size_t i = 0; i < num; i ++) {
        ReTk t = TkL_get(li, i + first);
        DynamicList_add(&out.tokens, &t);
    }
    return out;
}
