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

static void Node_print(Node* node, FILE* file, size_t indent)
{
    if (!node) return;
    for (size_t i = 0; i < indent * 2; i ++)
        fputc(' ', file);
    fprintf(file, "%s ", NodeKind_str[node->kind]);
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
    Node_print(children[0], file, indent + 1);
    Node_print(children[1], file, indent + 1);
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

    Node* n = malloc(sizeof(Node));
    n->kind = NodeChain;
    n->chain.a = a;
    n->chain.b = b;
    return n;
}

static Node* oneOf(Node** nodes, size_t len) {
    if (len == 0) return NULL;
    if (len == 1) return nodes[0];
    Node* rhs = oneOf(nodes + 1, len - 1);
    Node* self = malloc(sizeof(Node));
    self->kind = NodeOr;
    self->or.a = nodes[0];
    self->or.b = rhs;
    return self;
}

static Node* genMatch(tpre_pattern_t pat) {
    Node* self = malloc(sizeof(Node));
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

static Node* parse(TkL toks) {
    if (TkL_len(&toks) == 0) return NULL;

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

            Node* self = malloc(sizeof(Node));
            if (op.ty == OrNot) {
                self->kind = NodeMaybe;
                self->maybe = lhs;
            } else if (op.ty == RepeatLeast0) {
                self->kind = NodeRepeatLeast0;
                self->repeat = lhs;
            } else if (op.ty == RepeatLeast1) {
                self->kind = NodeRepeatLeast1;
                self->repeat = lhs;
            } else if (op.ty == RepeatLazyLeast0) {
                self->kind = NodeLazyRepeatLeast0;
                self->repeat = lhs;
            } else if (op.ty == RepeatLazyLeast1) {
                self->kind = NodeLazyRepeatLeast1;
                self->repeat = lhs;
            }

            fold = self;
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

        Node* self = malloc(sizeof(Node));
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
            Node* new = malloc(sizeof(Node));
            new->kind = NodeNot;
            new->not = self;
            self = new;
        }

        return maybeChain(self, rem);
    }

/*
    OrElse,
    StartOfStr,
*/

    TkL_free(&toks);
    assert(false);
    return NULL;
}

int main() {
    TkL li; li.tokens = lexe("ab(cd.*)?[a-c]");
    Node* nd = parse(li);
    Node_print(nd, stdout, 0);
}
