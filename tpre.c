#include <hs/hs_runtime.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include "allib/dynamic_list/dynamic_list.h"
#include "allib/kallok/kallok.h"
#define CREFLECT(args, ...) __VA_ARGS__

#define STORE_GROUPS  (1)

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
#if STORE_GROUPS
    size_t ngroups;
    /** will have null terminators ONLY IF LEN > 0 */
    DynamicList TYPES(char) * groups;
#endif
} re_match_t;

void regex_match_free(re_match_t match)
{
#if STORE_GROUPS
    for (size_t i = 0; i < match.ngroups; i ++)
        DynamicList_clear(&match.groups[i]);
    free(match.groups);
#endif
}

#if STORE_GROUPS
static void regex_match_group_put(re_match_t* match, group_id_t group, char c)
{
    if (group == 0) return;
    DynamicList_add(&match->groups[group], &c);
}
#endif

static bool pattern_match(pattern_t pat, char src)
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

re_match_t regex_match(regex_t const* re, const char * str)
{
    re_match_t match;
#if STORE_GROUPS
    match.ngroups = re->max_group + 1;
    match.groups = malloc(sizeof(*match.groups) * match.ngroups);
    for (size_t i = 0; i < match.ngroups; i ++) {
        DynamicList_init(&match.groups[i], sizeof(char), getLIBCAlloc(), 0);
    }
#endif

    node_id_t cursor = re->first_node;
    while (cursor >= 0)
    {
        if (pattern_match(re->i_pat[cursor], *str)) {
#if STORE_GROUPS
            regex_match_group_put(&match, re->i_group[cursor], *str);
#endif
            cursor = re->i_ok[cursor];
            if (*str) str ++;
        } else {
            backtrack_t bt = re->i_backtrack[cursor];
            str -= bt;
#if STORE_GROUPS
            group_id_t g = re->i_group[cursor];
            if (bt > 0) {
                size_t glen = match.groups[g].fixed.len;
                if (match.groups[g].fixed.len >= bt)
                    match.groups[g].fixed.len -= bt;
            }
#endif
            cursor = re->i_err[cursor];
        }
    }

    match.found = cursor == NODE_DONE;

#if STORE_GROUPS
    for (size_t i = 0; i < match.ngroups; i ++)
        if (match.groups[i].fixed.len > 0)
            DynamicList_add(&match.groups[i], (char[]) { '\0' });
#endif

    return match;
}

void regex_match_print(re_match_t match, FILE* out)
{
    if (match.found) {
        fprintf(out, "does match\n");
#if STORE_GROUPS
        for (size_t i = 0; i < match.ngroups; i ++) {
            fprintf(out, "  group %zu: %s\n", i, match.groups[i].fixed.len > 0 ? ((const char *) match.groups[i].fixed.data) : "");
        }
#endif
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
        pattern_t match;
        char group_name[20];
        struct { char from, to; } range;
    };
} ReTk;

void ReTk_dump(ReTk tk, FILE* out)
{
    fprintf(out, "%s", ReTkTy_str[tk.ty]);
    if (tk.ty == Match) {
        pattern_t pat = tk.match;
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
        pattern_t m;
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
        pattern_t match;

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

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <time.h>

int main()
{
    const char * resrc = "\\s*?(red|green|blue)?\\s*?(car|train)\\s*?";

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

    {
        re_match_t match = regex_match(&re, " green car ");
        regex_match_print(match, stdout);
        regex_match_free(match);
    }

    size_t niter = 1000000;

    const char * str;
    double start, end;

    str = " green car ";
    size_t strl = strlen(str);

    {
        int errnu;
        PCRE2_SIZE errof;
        pcre2_code* pre = pcre2_compile((PCRE2_SPTR) resrc, PCRE2_ZERO_TERMINATED, PCRE2_ANCHORED, &errnu, &errof, NULL);
        if (pre == NULL) {
            PCRE2_UCHAR buffer[256];
            pcre2_get_error_message(errnu, buffer, sizeof(buffer));
            printf("PCRE2 compilation failed at offset %d: %s\n", (int)errof, buffer);
            return 1;
        }

        start = (double) clock() / CLOCKS_PER_SEC;
        for (size_t i = 0; i < niter; i ++) {
            pcre2_match_data *volatile match_data = pcre2_match_data_create_from_pattern(pre, NULL);
            if ( pcre2_match(pre, (PCRE2_SPTR) str, strl, 0, PCRE2_NO_JIT | PCRE2_ANCHORED, match_data, NULL) < 1)
                return 1;
            pcre2_match_data_free(match_data);
        }
        end = (double) clock() / CLOCKS_PER_SEC;
        printf("pcre2 took %f ms\n", (end - start) / 1000);
    }

    {
        int errnu;
        PCRE2_SIZE errof;
        pcre2_code* pre = pcre2_compile((PCRE2_SPTR) resrc, PCRE2_ZERO_TERMINATED, PCRE2_ANCHORED, &errnu, &errof, NULL);
        if (pre == NULL) {
            PCRE2_UCHAR buffer[256];
            pcre2_get_error_message(errnu, buffer, sizeof(buffer));
            printf("PCRE2 compilation failed at offset %d: %s\n", (int)errof, buffer);
            return 1;
        }
        pcre2_match_data *match_data;
        pcre2_match_context *mcontext;
        pcre2_jit_stack *jit_stack;
        int rc = pcre2_jit_compile(pre, PCRE2_JIT_COMPLETE);
        mcontext = pcre2_match_context_create(NULL);
        jit_stack = pcre2_jit_stack_create(32*1024, 512*1024, NULL);
        pcre2_jit_stack_assign(mcontext, NULL, jit_stack);

        start = (double) clock() / CLOCKS_PER_SEC;
        for (size_t i = 0; i < niter; i ++) {
            pcre2_match_data *volatile match_data = pcre2_match_data_create_from_pattern(pre, NULL);
            rc = pcre2_jit_match(pre, (PCRE2_SPTR) str, strl, 0, PCRE2_ANCHORED, match_data, NULL);
            if (rc < 1) return 1;
            PCRE2_SIZE * volatile ovector = pcre2_get_ovector_pointer(match_data);
            (void) ovector;
            pcre2_match_data_free(match_data);
        }
        end = (double) clock() / CLOCKS_PER_SEC;
        printf("pcre2 (jit) took %f ms\n", (end - start) / 1000);
    }

    {
        start = (double) clock() / CLOCKS_PER_SEC;
        for (size_t i = 0; i < niter; i ++) {
            re_match_t volatile match = regex_match(&re, str);
            regex_match_free(match);
        }
        end = (double) clock() / CLOCKS_PER_SEC;
        printf("this lib took %f ms\n", (end - start) / 1000);
    }
}
