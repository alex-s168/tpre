#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include "allib/dynamic_list/dynamic_list.h"
#include "allib/fixed_list/fixed_list.h"
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


static void tpre_re_setnode(
        tpre_re_t* re,
        tpre_nodeid_t id,
        tpre_pattern_t   pat,
        tpre_nodeid_t   ok,
        tpre_nodeid_t   err,
        tpre_backtrack_t backtrack,
        tpre_groupid_t  group)
{
#define do(k) \
    re->i_##k[id] = k;

    do(pat);
    do(ok);
    do(err);
    do(backtrack);
    do(group);

#undef do

    if (group > re->max_group) re->max_group = group;
}

static tpre_nodeid_t tpre_re_addnode(
        tpre_re_t* re,
        tpre_pattern_t   pat,
        tpre_nodeid_t   ok,
        tpre_nodeid_t   err,
        tpre_backtrack_t backtrack,
        tpre_groupid_t  group)
{
#define realloc(k) \
    re->i_##k = realloc(re->i_##k, sizeof(*re->i_##k) * (re->num_nodes + 1)); \
    assert(re->i_##k); \
    re->i_##k[re->num_nodes] = k;

    realloc(pat);
    realloc(ok);
    realloc(err);
    realloc(backtrack);
    realloc(group);

#undef realloc

    if (group > re->max_group) re->max_group = group;
    re->free = true;
    return re->num_nodes ++;
}

static tpre_nodeid_t tpre_re_resvnode(tpre_re_t* re)
{
    return tpre_re_addnode(re, (tpre_pattern_t) {}, 0, 0, 0, 0);
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
    NodeStartOfStr,
    NodeChain,
    NodeOr,
    NodeMaybe,
    NodeNot,
    NodeRepeatLeast0,
    NodeRepeatLeast1,
    NodeLazyRepeatLeast0,
    NodeLazyRepeatLeast1,
    NodeJustGroup,
    NodeCaptureGroup,
    NodeNamedCaptureGroup,
} NodeKind);
extern const char * NodeKind_str[];

typedef struct Node Node;
struct Node {
    NodeKind kind;
    tpre_groupid_t group;
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

        Node* just_group;
        Node* maybe;
        Node* repeat;
        Node* capture;
        Node* not;

        struct {
            char name[20];
            Node* group;
        } named_capture;
    };
};

static Node* Node_alloc(void) {
    return calloc(1, sizeof(Node));
}

static void Node_children(Node* nd, Node* childrenOut[2]) {
    childrenOut[0] = NULL;
    childrenOut[1] = NULL;

    switch (nd->kind) {
        case NodeMatch:
        case NodeStartOfStr:
            break;

        case NodeChain:
            childrenOut[0] = nd->chain.a;
            childrenOut[1] = nd->chain.b;
            break;

        case NodeNot:
            childrenOut[0] = nd->not;
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

        case NodeJustGroup:
            childrenOut[0] = nd->just_group;
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

static Node* Node_clone(Node* node)
{
    Node* copy = Node_alloc();
    copy->kind = node->kind;
    copy->group = node->group;

    switch (node->kind)
    {
        case NodeMatch:
            copy->match = node->match;
            break;

        case NodeStartOfStr:
            break;

        case NodeChain:
            copy->chain.a = Node_clone(node->chain.a);
            copy->chain.b = Node_clone(node->chain.b);
            break;

        case NodeOr:
            copy->or.a = Node_clone(node->or.a);
            copy->or.b = Node_clone(node->or.b);
            break;

        case NodeMaybe:
            copy->maybe = Node_clone(node->maybe);
            break;

        case NodeNot:
            copy->not = Node_clone(node->not);
            break;

        case NodeRepeatLeast0:
        case NodeRepeatLeast1:
        case NodeLazyRepeatLeast0:
        case NodeLazyRepeatLeast1:
            copy->repeat = Node_clone(node->repeat);
            break;

        case NodeJustGroup:
            copy->just_group = Node_clone(node->just_group);
            break;

        case NodeCaptureGroup:
            copy->capture = Node_clone(node->capture);
            break;

        case NodeNamedCaptureGroup:
            copy->named_capture.group = Node_clone(node->named_capture.group);
            memcpy(copy->named_capture.name, node->named_capture.name, 20);
            break;
    }

    return copy;
}

static void Node_print(Node* node, FILE* file, size_t indent, bool print_grps)
{
    if (!node) return;
    for (size_t i = 0; i < indent * 2; i ++)
        fputc(' ', file);
    fprintf(file, "%s ", NodeKind_str[node->kind]);
    if (print_grps)
        fprintf(file, "@%zu ", (size_t) node->group);
    switch (node->kind) {
        case Match: {
            tpre_pattern_t pat = node->match;
            if (pat.is_special) fprintf(file, "(special: %u)", pat.val);
            else fprintf(file, "(%c)", (char) pat.val);
        } break;

        default: break;
    }
    fputc('\n', file);
    Node* children[2];
    Node_children(node, children);
    Node_print(children[0], file, indent + 1, print_grps);
    Node_print(children[1], file, indent + 1, print_grps);
}

static bool Node_eq(Node* a, Node* b) {
    if (a->kind != b->kind) return false;
    if (a->group != b->group) return false;

    switch (a->kind) {
        case NodeMatch:
            return a->match.is_special == b->match.is_special &&
                a->match.val == b->match.val;

        case NodeStartOfStr:
            return true;

        case NodeChain:
            return Node_eq(a->chain.a, b->chain.a)
                && Node_eq(a->chain.b, b->chain.b);

        case NodeOr:
            return Node_eq(a->or.a, b->or.a)
                && Node_eq(a->or.b, b->or.b);

        case NodeMaybe:
            return Node_eq(a->maybe, b->maybe);
        
        case NodeNot:
            return Node_eq(a->not, b->not);

        case NodeRepeatLeast0:
        case NodeRepeatLeast1:
        case NodeLazyRepeatLeast0:
        case NodeLazyRepeatLeast1:
            return Node_eq(a->repeat, b->repeat);

        case NodeJustGroup:
            return Node_eq(a->just_group, b->just_group);

        case NodeCaptureGroup:
            return Node_eq(a->capture, b->capture);

        case NodeNamedCaptureGroup:
            return Node_eq(a->named_capture.group, b->named_capture.group) &&
                !strcmp(a->named_capture.name, b->named_capture.name);
    }
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

static bool isCaptureGroupOpen(ReTkTy ty) {
    return ty == CaptureGroupOpen ||
           ty == CaptureGroupOpenNamed ||
           ty == CaptureGroupOpenNoCapture;
}

static bool isOneOfOpen(ReTkTy ty) {
    return ty == OneOfOpen ||
           ty == OneOfOpenInvert;
}

static bool isPostfix(ReTkTy ty) {
    return ty == OrNot ||
           ty == RepeatLeast0 ||
           ty == RepeatLeast1 ||
           ty == RepeatLazyLeast0 ||
           ty == RepeatLazyLeast1;
}

static Node* maybeChain(Node* a, Node* b) {
    if (b == NULL) return a;

    Node* n = Node_alloc();
    n->kind = NodeChain;
    n->chain.a = a;
    n->chain.b = b;
    return n;
}

static Node* oneOf(Node** nodes, size_t len) {
    if (len == 0) return NULL;
    if (len == 1) return nodes[0];
    Node* rhs = oneOf(nodes + 1, len - 1);
    Node* self = Node_alloc();
    self->kind = NodeOr;
    self->or.a = nodes[0];
    self->or.b = rhs;
    return self;
}

static Node* genMatch(tpre_pattern_t pat) {
    Node* self = Node_alloc();
    self->kind = NodeMatch;
    self->match = pat;
    return self;
}

static void replaceChainsWithOrs(Node* node) {
    if (node == NULL) return;
    if (node->kind != NodeChain) return;
    Node tmp = *node;
    node->kind = NodeOr;
    node->or.a = tmp.chain.a;
    node->or.b = tmp.chain.b;
    Node* children[2];
    Node_children(node, children);
    replaceChainsWithOrs(children[0]);
    replaceChainsWithOrs(children[1]);
}

static void handle_postfix(Node* node, ReTk op)
{
    if (node->kind == NodeChain) {
        Node* children[2];
        Node_children(node, children);

        if (children[1] != NULL)
            return handle_postfix(children[1], op);
        if (children[0] != NULL)
            return handle_postfix(children[0], op);
    }

    Node* copy = Node_alloc();
    memcpy(copy, node, sizeof(Node));

    if (op.ty == OrNot) {
        node->kind = NodeMaybe;
        node->maybe = copy;
    } else if (op.ty == RepeatLeast0) {
        node->kind = NodeRepeatLeast0;
        node->repeat = copy;
    } else if (op.ty == RepeatLeast1) {
        node->kind = NodeRepeatLeast1;
        node->repeat = copy;
    } else if (op.ty == RepeatLazyLeast0) {
        node->kind = NodeLazyRepeatLeast0;
        node->repeat = copy;
    } else if (op.ty == RepeatLazyLeast1) {
        node->kind = NodeLazyRepeatLeast1;
        node->repeat = copy;
    }
}

static Node* parse(TkL toks) {
    if (TkL_len(&toks) == 0) return NULL;

    {
        DynamicList TYPES(size_t) where;
        DynamicList_init(&where, sizeof(size_t), getLIBCAlloc(), 0);
        size_t nesting = 0;
        for (size_t i = 0; i < TkL_len(&toks); i ++) {
            ReTkTy t = TkL_get(&toks, i).ty;
            if (isCaptureGroupOpen(t)) nesting ++;
            else if (t == CaptureGroupClose) nesting --;
            else if (isOneOfOpen(t)) nesting ++;
            else if (t == OneOfClose) nesting --;
            if (nesting == 0 && t == OrElse) {
                if (where.fixed.len == 0)
                    *(size_t*)DynamicList_addp(&where) = 0;
                *(size_t*)DynamicList_addp(&where) = i-1;
                *(size_t*)DynamicList_addp(&where) = i+1;
            }
        }
        if (where.fixed.len > 0) {
            *(size_t*)DynamicList_addp(&where) = TkL_len(&toks)-1;
            Node* fold = NULL;
            for (size_t i = 0; i < where.fixed.len; i += 2) {
                size_t first = *(size_t*) FixedList_get(where.fixed, i);
                size_t last = *(size_t*) FixedList_get(where.fixed, i + 1);
                Node* nd = parse(TkL_copy_view(&toks, first, last-first+1));
                if (fold == NULL)
                    fold = nd;
                else fold = oneOf((Node*[]) { fold, nd }, 2);
            }
            return fold;
        }
    }



    // weird code for postfix operators
    {
        DynamicList TYPES(size_t) postfixes;
        DynamicList_init(&postfixes, sizeof(size_t), getLIBCAlloc(), 0);

        size_t nesting = 0;
        for (size_t i = 0; i < TkL_len(&toks); i ++) {
            ReTkTy t = TkL_get(&toks, i).ty;
            if (isCaptureGroupOpen(t)) nesting ++;
            else if (t == CaptureGroupClose) nesting --;
            else if (isOneOfOpen(t)) nesting ++;
            else if (t == OneOfClose) nesting --;

            if (nesting == 0 && isPostfix(t)) {
                DynamicList_add(&postfixes, &i);
            }
        }

        Node* fold = NULL;
        while (postfixes.fixed.len > 0) {
            size_t idx = * (size_t*) FixedList_get(postfixes.fixed, 0);
            DynamicList_removeAt(&postfixes, 0);

            ReTk op = TkL_get(&toks, idx);
            Node* lhs = parse(TkL_copy_view(&toks, 0, idx));

            DynamicList_removeRange(&toks.tokens, 0, idx);
            for (size_t i = 0; i < postfixes.fixed.len; i ++) {
                *((size_t *) FixedList_get(postfixes.fixed, i)) -= idx + 1;
            }

            if (fold)
                lhs = maybeChain(fold, lhs);
            fold = lhs;

            handle_postfix(fold, op);
        }

        if (fold != NULL) {
            return maybeChain(fold, parse(toks));
        }
    }

    ReTkTy firstTy = TkL_get(&toks, 0).ty;

    if (isCaptureGroupOpen(firstTy)) {
        size_t nesting = 0;
        size_t i = 0;
        for (; i < TkL_len(&toks); i ++) {
            ReTkTy t = TkL_get(&toks, i).ty;
            if (isCaptureGroupOpen(t)) nesting ++;
            else if (t == CaptureGroupClose) {
                nesting --;
                if (nesting == 0) break;
            }
        }

        Node* inner = parse(TkL_copy_view(&toks, 1, i-1));
        Node* rem = parse(TkL_copy_view(&toks, i+1, TkL_len(&toks)-i-1));

        Node* self = Node_alloc();
        if (firstTy == CaptureGroupOpen) {
            self->kind = NodeCaptureGroup;
            self->capture = inner;
        } else if (firstTy == CaptureGroupOpenNoCapture) {
            self->kind = NodeJustGroup;
            self->just_group = inner;
        } else {
            self->kind = NodeNamedCaptureGroup;
            self->named_capture.group = inner;
            memcpy(self->named_capture.name, TkL_get(&toks, 0).group_name, 20);
        }

        TkL_free(&toks);
        return maybeChain(self, rem);
    }

    if (firstTy == Match) {
        Node* rem = parse(TkL_copy_view(&toks, 1, TkL_len(&toks)-1));
        Node* self = genMatch(TkL_get(&toks, 0).match);

        TkL_free(&toks);
        return maybeChain(self, rem);
    }

    if (firstTy == StartOfStr) {
        Node* rem = parse(TkL_copy_view(&toks, 1, TkL_len(&toks)-1));
        Node* self = Node_alloc();
        self->kind = NodeStartOfStr;

        TkL_free(&toks);
        return maybeChain(self, rem);
    }

    if (firstTy == MatchRange) {
        Node* rem = parse(TkL_copy_view(&toks, 1, TkL_len(&toks)-1));

        char from = TkL_get(&toks, 0).range.from;
        char to = TkL_get(&toks, 0).range.to;
        if (from > to) {
            char t = from;
            from = to;
            to = t;
        }

        size_t len = to - from + 1;
        Node* nodes[len];
        for (size_t i = 0; i < len; i ++)
            nodes[i] = genMatch(NO(from + i));
        Node* self = oneOf(nodes, len);

        TkL_free(&toks);
        return maybeChain(self, rem);
    }

    if (isOneOfOpen(firstTy)) {
        size_t nesting = 0;
        size_t i = 0;
        for (; i < TkL_len(&toks); i ++) {
            ReTkTy t = TkL_get(&toks, i).ty;
            if (isOneOfOpen(t)) nesting ++;
            else if (t == OneOfClose) {
                nesting --;
                if (nesting == 0) break;
            }
        }

        Node* self = parse(TkL_copy_view(&toks, 1, i-1));
        Node* rem = parse(TkL_copy_view(&toks, i+1, TkL_len(&toks)-i-1));
        TkL_free(&toks);

        replaceChainsWithOrs(self);
        if (firstTy == OneOfOpenInvert) {
            Node* new = Node_alloc();
            new->kind = NodeNot;
            new->not = self;
            self = new;
        }

        return maybeChain(self, rem);
    }

    TkL_free(&toks);
    assert(false);
    return NULL;
}

static void or_cases(Node* or, DynamicList TYPES(Node*) * out) {
    if (!or || or->kind != NodeOr) return;
    if (or->or.a->kind != NodeOr)
        DynamicList_add(out, &or->or.a);
    else or_cases(or->or.a, out);
    if (or->or.b->kind != NodeOr)
        DynamicList_add(out, &or->or.b);
    else or_cases(or->or.b, out);
}

static Node* find_trough_rep(Node* node, NodeKind what) {
    if (node->kind == what) return node;

    switch (node->kind) {
        case NodeLazyRepeatLeast0:
        case NodeLazyRepeatLeast1:
        case NodeRepeatLeast0:
        case NodeRepeatLeast1:
            return find_trough_rep(node->repeat, what);

        default:
            return NULL;
    }
}

static Node* last_left_chain(Node* node) {
    if (node->kind == NodeOr) return last_left_chain(node->or.a);
    if (node->kind != NodeChain) return NULL;
    if (node->chain.a->kind == NodeChain) return last_left_chain(node->chain.a);
    return node;
}

static void verify(Node* nd) {
    if (nd == NULL) return;
    Node* children[2];
    Node_children(nd, children);
    verify(children[0]);
    verify(children[1]);

    if (nd->kind == NodeNamedCaptureGroup) {
        fprintf(stderr, "named capture groups not supported\n");
        exit(1);
    }

    if (nd->kind == NodeRepeatLeast0 || nd->kind == NodeRepeatLeast1) {
        fprintf(stderr, "only lazy repeats supported\n");
        exit(1);
    }
}

static void groups(Node* nd, tpre_groupid_t group, tpre_groupid_t* global_next_group_id) {
    if (nd == NULL) return;
    Node* children[2];
    Node_children(nd, children);

    if (nd->kind == NodeJustGroup) {
        memcpy(nd, nd->just_group, sizeof(Node));
        groups(nd, group, global_next_group_id);
        return;
    }

    if (nd->kind == NodeCaptureGroup) {
        memcpy(nd, nd->capture, sizeof(Node));
        groups(nd, (*global_next_group_id)++, global_next_group_id);
        return;
    }

    nd->group = group;
    groups(children[0], group, global_next_group_id);
    groups(children[1], group, global_next_group_id);
}

/** convert RepeatLazyLeast1 to RepeatLazyLeast0 */
static void fix_0(Node* node) {
    if (node == NULL) return;
    Node* children[2];
    Node_children(node, children);

    fix_0(children[0]);
    fix_0(children[1]);

    if (node->kind == NodeLazyRepeatLeast1)
    {
        Node* first = Node_clone(node->repeat);
        Node* rep = Node_alloc();
        rep->group = node->group;
        rep->kind = NodeLazyRepeatLeast0;
        rep->repeat = node->repeat;
        node->kind = NodeChain;
        node->chain.a = first;
        node->chain.b = rep;
    }
}

/** move all code chained to or into all or cases if any or case contains repetition */
static void fix_1(Node* node) {
    if (node == NULL) return;
    Node* children[2];
    Node_children(node, children);

    if (node->kind == NodeChain) {
        Node* orr = find_trough_rep(node->chain.a, NodeOr);
        if (orr != NULL) {
            DynamicList TYPES(Node*) cases;
            DynamicList_init(&cases, sizeof(Node*), getLIBCAlloc(), 8);
            or_cases(orr, &cases);
            Node* mov = node->chain.b;
            for (size_t i = 0; i < cases.fixed.len; i ++) {
                Node* cas = *(Node**)FixedList_get(cases.fixed, i);
                Node* inner = Node_alloc();
                memcpy(inner, cas, sizeof(Node));
                cas->kind = NodeChain;
                cas->chain.a = inner;
                Node* mov2 = Node_clone(mov);
                cas->chain.b = mov2;
            }
            DynamicList_clear(&cases);
            Node_free(mov);
            memcpy(node, node->chain.a, sizeof(Node));
        }
    }

    fix_1(children[0]);
    fix_1(children[1]);
}

/** move duplicate code in beginning of or cases to befre the or; required because otherwise will break engine */
static void fix_2(Node* node) {
    if (node == NULL) return;
    Node* children[2];
    Node_children(node, children);

    fix_2(children[0]);
    fix_2(children[1]);

    if (node->kind == NodeOr) {
        Node* a = node->or.a;
        Node* b = node->or.b;

        a = last_left_chain(a);
        b = last_left_chain(b);

        if (a && b && Node_eq(a->chain.a, b->chain.a)) {
            Node* prefix = a->chain.a;
            free(b->chain.a);
            memcpy(a, a->chain.b, sizeof(Node));
            memcpy(b, b->chain.b, sizeof(Node));

            Node* right = Node_alloc();
            memcpy(right, node, sizeof(Node));
            node->kind = NodeChain;
            node->chain.a = prefix;
            node->chain.b = right;
        }
    }
}

static void lower(tpre_re_t* out, tpre_nodeid_t this_id, tpre_nodeid_t on_ok, tpre_nodeid_t on_error, tpre_backtrack_t bt, size_t* num_match, Node* node)
{
    switch (node->kind)
    {
        case NodeLazyRepeatLeast1:
        case NodeRepeatLeast0:
        case NodeRepeatLeast1:
            __builtin_unreachable();
            break;

        case NodeMatch: {
            if (num_match) (*num_match)++;
            tpre_re_setnode(out, this_id,
                    node->match,
                    on_ok,
                    on_error,
                    bt,
                    node->group);
        } break;

        case NodeLazyRepeatLeast0: {
            lower(out, this_id, this_id, on_ok, 0, NULL, node->repeat);
        } break;

        case NodeChain: {
            size_t nimatch = 0;
            tpre_nodeid_t right = tpre_re_resvnode(out);
            lower(out, this_id, right, on_error, bt, &nimatch, node->chain.a);
            if (num_match) (*num_match) += nimatch;
            lower(out, right, on_ok, on_error, bt + nimatch, num_match, node->chain.b);
        } break;

        case NodeOr: {
            tpre_nodeid_t right = tpre_re_resvnode(out);
            lower(out, this_id, on_ok, right, bt, NULL, node->or.a);
            lower(out, right, on_ok, on_error, bt, NULL, node->or.b);
        } break;

        case NodeMaybe: {
            lower(out, this_id, on_ok, on_ok, bt, num_match, node->maybe);
        } break;

        case NodeStartOfStr:
        default:
            assert(false && "bruh");
            break;
    }
}

/** 0 = ok */
int tpre_compile(tpre_re_t* out, char const * str, DynamicList TYPES(char *) * errsOut)
{
    TkL li; li.tokens = lexe(str);
    Node* nd = parse(li);
    verify(nd);
    tpre_groupid_t nextgr = 1;
    groups(nd, 0, &nextgr);
    fix_0(nd);
    fix_1(nd);
    fix_2(nd);

    memset(out, 0, sizeof(tpre_re_t));
    tpre_nodeid_t nd0 = tpre_re_resvnode(out);
    lower(out, nd0, NODE_DONE, NODE_ERR, 0, NULL, nd);

    return 0;
}

void tpre_free(tpre_re_t re)
{
    if (re.free) {
        free(re.i_ok);
        free(re.i_err);
        free(re.i_pat);
        free(re.i_group);
        free(re.i_backtrack);
    }
}

static void tpre_dump(tpre_re_t out)
{
    printf("nd\tok\terr\tv\tbt\n");
    for (size_t i = 0; i < out.num_nodes; i ++)
    {
        printf("%zu\t%i\t%i\t%c\t%u\n", i, out.i_ok[i], out.i_err[i], out.i_pat[i].val, out.i_backtrack[i]);
    }
}

// TODO: this will break the enine: a*?b|ac
// TODO: this will break the engine (ab)*|(ac)*

int main() {
    const char * str = "\\s*?(red|green|blue)?\\s*?(car|train)\\s*?";

    tpre_re_t re;
    DynamicList TYPES(char*) errs;
    DynamicList_init(&errs, sizeof(char*), getLIBCAlloc(), 0);
    if (tpre_compile(&re, str, &errs) != 0) {
        fprintf(stderr, "regex compile failed:\n");
        for (size_t i = 0; i < errs.fixed.len; i ++) {
            char* reason = *(char**)FixedList_get(errs.fixed, i);
            fprintf(stderr, "  %s\n", reason);
            free(reason);
        }
        return 1;
    }

    tpre_match_t m;

    m = tpre_match(&re, "blue car");
    tpre_match_dump(m, stdout);
    tpre_match_free(m);

    m = tpre_match(&re, "   red   car ");
    tpre_match_dump(m, stdout);
    tpre_match_free(m);

    m = tpre_match(&re, "  green   train    ");
    tpre_match_dump(m, stdout);
    tpre_match_free(m);

    m = tpre_match(&re, "    car    ");
    tpre_match_dump(m, stdout);
    tpre_match_free(m);


    m = tpre_match(&re, "    train    ");
    tpre_match_dump(m, stdout);
    tpre_match_free(m);


}
