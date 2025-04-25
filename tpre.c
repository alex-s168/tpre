#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include "tpre.h"

#define NODE_DONE ((tpre_nodeid_t) -2)
#define NODE_ERR  ((tpre_nodeid_t) -1)

#define SPECIAL_ANY   (0)
#define SPECIAL_SPACE (1)
#define SPECIAL_END   (2)
#define SPECIAL_START (3)
#define NO(c) ((tpre_pattern_t) {.is_special = 0,.val=(uint8_t)c})
#define SP(c) ((tpre_pattern_t) {.is_special = 1,.val=(uint8_t)c})


static void tpre_re_setnode(
        tpre_re_t* re,
        tpre_nodeid_t id,
        tpre_re_node_t nd)
{
    re->i[id] = nd;
    if (nd.group > re->max_group) {
        re->max_group = nd.group;
    }
}

static tpre_nodeid_t tpre_re_addnode(
        tpre_re_t* re,
        tpre_re_node_t nd)
{
    re->i = realloc(re->i, sizeof(*re->i) * (re->num_nodes + 1));
    assert(re->i);
    re->i[re->num_nodes] = nd;
    tpre_re_setnode(re, re->num_nodes, nd);
    re->free = true;
    return re->num_nodes ++;
}

static tpre_nodeid_t tpre_re_resvnode(tpre_re_t* re)
{
    return tpre_re_addnode(re, (tpre_re_node_t) {0});
}

void tpre_match_free(tpre_match_t match)
{
    free(match.groups);
}

static void tpre_match_group_put(tpre_match_t* match, tpre_groupid_t group, char c, tpre_src_loc_t loc)
{
    if (group == 0) return;
    tpre_group_t* g = &match->groups[group];
    if (g->len == 0) {
        g->begin = loc;
    }
    g->len ++;
}

static bool pattern_match(tpre_pattern_t pat, char src, bool is_begin)
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

        case SPECIAL_START:
            return is_begin;

        default:
            return false;
    }
}

tpre_match_t tpre_match(tpre_re_t const* re, const char * str)
{
    tpre_match_t match;
    match.ngroups = re->max_group + 1;
    match.groups = calloc(sizeof(*match.groups), match.ngroups);

    const char * begin_str = str;

    tpre_nodeid_t cursor = re->first_node;
    while (cursor >= 0)
    {
        if (pattern_match(re->i[cursor].pat, *str, begin_str == str)) {
            if (*str) {
                tpre_match_group_put(&match, re->i[cursor].group, *str, str - begin_str);
            }
            cursor = re->i[cursor].ok;
            if (*str) {
                str ++;
            }
        } else {
            tpre_backtrack_t bt = re->i[cursor].backtrack;
            str -= bt;
            tpre_groupid_t g = re->i[cursor].group;
            if (bt > 0) {
                if (match.groups[g].len >= bt) {
                    size_t n = bt;
                    if (n > match.groups[g].len)
                        n = match.groups[g].len;
                    match.groups[g].len -= n;
                }
            }
            cursor = re->i[cursor].err;
        }
    }

    match.found = cursor == NODE_DONE;

    return match;
}

static bool pattern_matchn(tpre_pattern_t pat, char src, size_t i, size_t len)
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
            return i+1 == len;

        case SPECIAL_START:
            return i == 0;

        default:
            return false;
    }
}

tpre_match_t tpre_matchn(tpre_re_t const* re, const char * str, size_t strl)
{
    tpre_match_t match;
    match.ngroups = re->max_group + 1;
    match.groups = calloc(sizeof(*match.groups), match.ngroups);

    size_t i = 0;

    tpre_nodeid_t cursor = re->first_node;
    while (cursor >= 0)
    {
        if (pattern_matchn(re->i[cursor].pat, str[i], i, strl)) {
            if (i < strl) {
                tpre_match_group_put(&match, re->i[cursor].group, str[i], i);
            }
            cursor = re->i[cursor].ok;
            if (i < strl) i ++;
        } else {
            tpre_backtrack_t bt = re->i[cursor].backtrack;
            i -= bt;
            tpre_groupid_t g = re->i[cursor].group;
            if (bt > 0) {
                if (match.groups[g].len >= bt) {
                    size_t n = bt;
                    if (n > match.groups[g].len)
                        n = match.groups[g].len;
                    match.groups[g].len -= n;
                }
            }
            cursor = re->i[cursor].err;
        }
    }

    match.found = cursor == NODE_DONE;

    return match;
}

void tpre_match_dump(tpre_match_t match, char const * matched_str, FILE* out)
{
    if (match.found) {
        fprintf(out, "does match\n");
        size_t i;
        for (i = 1; i < match.ngroups; i ++) {
            fprintf(out, "  group %zu: ", i);
            fwrite(matched_str + match.groups[i].begin, 1, match.groups[i].len, out);
            fprintf(out, "\n");
        }
    }
    else {
        fprintf(out, "does not match\n");
    }
}

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
} ReTkTy;

static const char * ReTkTy_str[] = {
    [Match] = "Match",
    [MatchRange] = "MatchRange",
    [RepeatLazyLeast0] = "RepeatLazyLeast0",
    [RepeatLeast0] = "RepeatLeast0",
    [RepeatLazyLeast1] = "RepeatLazyLeast1",
    [RepeatLeast1] = "RepeatLeast1",
    [OrNot] = "OrNot",
    [CaptureGroupOpen] = "CaptureGroupOpen",
    [CaptureGroupOpenNoCapture] = "CaptureGroupOpenNoCapture",
    [CaptureGroupOpenNamed] = "CaptureGroupOpenNamed",
    [CaptureGroupClose] = "CaptureGroupClose",
    [OneOfOpen] = "OneOfOpen",
    [OneOfOpenInvert] = "OneOfOpenInvert",
    [OneOfClose] = "OneOfClose",
    [OrElse] = "OrElse",
};

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
        else if (**reader == '#') {
            while (**reader && **reader != ')') {
                (*reader) ++;
            }
            if (!**reader) {
                return false;
            }
            (*reader) ++;
        }
        else {
            tkOut->ty = CaptureGroupOpen;
            return true;
        }
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
        tkOut->ty = Match;
        tkOut->match = SP(SPECIAL_START);
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

typedef struct {
    void* allocptr;
    ReTk* tokens;
    size_t cap;
    size_t len;
} TkL;

static size_t TkL_len(TkL const* li) {
    return li->len;
}

static bool TkL_peek(ReTk* out, TkL* li) {
    if (TkL_len(li) == 0) return false;
    *out = li->tokens[0];
    return true;
}

static ReTk TkL_get(TkL const* li, size_t i) {
    return li->tokens[i];
}

static bool TkL_take(ReTk* out, TkL* li) {
    if (TkL_len(li) == 0) return false;
    *out = TkL_get(li, 0);
    li->tokens ++;
    li->len --;
    li->cap --;
    return true;
}

static void TkL_free(TkL* li) {
    free(li->allocptr);
}

static TkL TkL_copy_range(TkL const* li, size_t first, size_t num) {
    TkL out = {0};
    out.len = num;
    out.cap = num;
    out.allocptr = malloc(li->cap * sizeof(ReTk));
    out.tokens = out.allocptr;
    memcpy(out.tokens, li->tokens + first, num * sizeof(ReTk));
    return out;
}

static void TkL_add(TkL* li, ReTk tk) {
    if (li->len + 1 > li->cap) {
        li->cap += 16;
        ReTk* new = malloc(li->cap * sizeof(ReTk));
        if (!new) {
            free(li->tokens);
            fprintf(stderr, "out of memory");
            exit(1);
        }
        memcpy(new, li->tokens, li->len * sizeof(ReTk));
        free(li->allocptr);
        li->allocptr = new;
        li->tokens = new;
    }
    li->tokens[li->len ++] = tk;
}

static TkL lexe(const char * src)
{
    TkL out = {0};

    ReTk tok;
    const char* reader = src;
    while (lex(&tok, &reader)) {
        TkL_add(&out, tok);
    }

    if (*reader) {
        fprintf(stderr, "error at arround %zu\n", reader - src);
        free(out.tokens);
        exit(1);
    }

    return out;
}

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
} NodeKind;

static const char * NodeKind_str[] = {
    [NodeMatch] = "Match",
    [NodeChain] = "Chain",
    [NodeOr] = "Or",
    [NodeMaybe] = "Maybe",
    [NodeNot] = "Not",
    [NodeRepeatLeast0] = "RepeatLeast0",
    [NodeRepeatLeast1] = "RepeatLeast1",
    [NodeLazyRepeatLeast0] = "LazyRepeatLeast0",
    [NodeLazyRepeatLeast1] = "LazyRepeatLeast1",
    [NodeJustGroup] = "JustGroup",
    [NodeCaptureGroup] = "CaptureGroup",
    [NodeNamedCaptureGroup] = "NamedCaptureGroup",
};

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
    size_t i;
    for (i = 0; i < indent * 2; i ++)
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

    // weird code for ors
    {
        TkL* seg = NULL;
        size_t seglen = 0;

        // split with nesting at ors
        
        size_t nesting = 0;
        size_t begin = 0;
        {
            size_t i;
            for (i = 0; i < TkL_len(&toks); i ++) {
                ReTkTy t = TkL_get(&toks, i).ty;
                if (isCaptureGroupOpen(t)) nesting ++;
                else if (t == CaptureGroupClose) nesting --;
                else if (isOneOfOpen(t)) nesting ++;
                else if (t == OneOfClose) nesting --;
                if (nesting == 0 && t == OrElse) {
                    TkL tokl = TkL_copy_range(&toks, begin, i - begin);
                    seg = realloc(seg, sizeof(*seg) * (seglen+1));
                    seg[seglen++] = tokl;
                    begin = i + 1;
                }
            }
            if (TkL_len(&toks) - begin > 0) {
                TkL tokl = TkL_copy_range(&toks, begin, TkL_len(&toks) - begin);
                seg = realloc(seg, sizeof(*seg) * (seglen+1));
                seg[seglen++] = tokl;
            }
        }

        if (seglen >= 2) {
            Node* fold = NULL;
            size_t i;
            for (i = 0; i < seglen; i ++) {
                Node* nd = parse(seg[i]);
                if (fold == NULL)
                    fold = nd;
                else fold = oneOf((Node*[]) { fold, nd }, 2);
            }
            free(seg);
            TkL_free(&toks);
            return fold;
        }

        free(seg);
    }

    // weird code for postfix operators
    {
        char* is_postfix = calloc(TkL_len(&toks), 1);
        void* is_postfix_alloc = is_postfix;

        {
            size_t nesting = 0;
            size_t i;
            for (i = 0; i < TkL_len(&toks); i ++) {
                ReTkTy t = TkL_get(&toks, i).ty;
                if (isCaptureGroupOpen(t)) nesting ++;
                else if (t == CaptureGroupClose) nesting --;
                else if (isOneOfOpen(t)) nesting ++;
                else if (t == OneOfClose) nesting --;

                if (nesting == 0 && isPostfix(t)) {
                    is_postfix[i] = true;
                }
            }
        }

        Node* fold = NULL;
        size_t idx;
        for (idx = 0; idx < (volatile size_t) TkL_len(&toks); idx ++) {
            if (!is_postfix[idx])
                continue;

            ReTk op = TkL_get(&toks, idx);
            Node* lhs = parse(TkL_copy_range(&toks, 0, idx));

            size_t i;
            for (i = 0; i < idx + 1; i ++) {
                ReTk ign;
                TkL_take(&ign, &toks);
            }
            is_postfix += idx + 1;

            if (fold)
                lhs = maybeChain(fold, lhs);
            fold = lhs;

            handle_postfix(fold, op);
        }

        free(is_postfix_alloc);

        if (fold != NULL) {
            return maybeChain(fold, parse(toks));
        }
    }

    ReTkTy firstTy = TkL_get(&toks, 0).ty;

    if (isCaptureGroupOpen(firstTy)) {
        size_t nesting = 0;
        size_t close = 0;
        for (; close < TkL_len(&toks); close ++) {
            ReTkTy t = TkL_get(&toks, close).ty;
            if (isCaptureGroupOpen(t)) nesting ++;
            else if (t == CaptureGroupClose) {
                nesting --;
                if (nesting == 0) break;
            }
        }

        Node* inner = parse(TkL_copy_range(&toks, 1, close-1));
        Node* rem = parse(TkL_copy_range(&toks, close+1, TkL_len(&toks)-close-1));

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
            ReTk t = TkL_get(&toks, 0);
            char const* name = t.group_name;
            memcpy(self->named_capture.name, name, 20);
        }

        TkL_free(&toks);
        return maybeChain(self, rem);
    }

    if (firstTy == Match) {
        Node* rem = parse(TkL_copy_range(&toks, 1, TkL_len(&toks)-1));
        Node* self = genMatch(TkL_get(&toks, 0).match);

        TkL_free(&toks);
        return maybeChain(self, rem);
    }

    if (firstTy == MatchRange) {
        Node* rem = parse(TkL_copy_range(&toks, 1, TkL_len(&toks)-1));

        char from = TkL_get(&toks, 0).range.from;
        char to = TkL_get(&toks, 0).range.to;
        if (from > to) {
            char t = from;
            from = to;
            to = t;
        }

        size_t len = to - from + 1;
        Node* nodes[len];
        size_t i;
        for (i = 0; i < len; i ++)
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

        Node* self = parse(TkL_copy_range(&toks, 1, i-1));
        Node* rem = parse(TkL_copy_range(&toks, i+1, TkL_len(&toks)-i-1));
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

    fprintf(stderr, "unparsed tok! %s\n", ReTkTy_str[TkL_get(&toks, 0).ty]);
    TkL_free(&toks);
    exit(1);
}

typedef struct {
    Node** items;
    size_t len;
} NodeLi;

static void NodeLi_add(NodeLi* li, Node* nd) {
    li->items = realloc(li->items, sizeof(Node*) * (li->len + 1));
    li->items[li->len ++] = nd;
}

static void or_cases(Node* or, NodeLi * out) {
    if (!or || or->kind != NodeOr) return;
    if (or->or.a->kind != NodeOr)
        NodeLi_add(out, or->or.a);
    else or_cases(or->or.a, out);
    if (or->or.b->kind != NodeOr)
        NodeLi_add(out, or->or.b);
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
            NodeLi cases = {0};
            or_cases(orr, &cases);
            Node* mov = node->chain.b;
            size_t i;
            for (i = 0; i < cases.len; i ++) {
                Node* cas = cases.items[i];
                Node* inner = Node_alloc();
                memcpy(inner, cas, sizeof(Node));
                cas->kind = NodeChain;
                cas->chain.a = inner;
                Node* mov2 = Node_clone(mov);
                cas->chain.b = mov2;
            }
            free(cases.items);
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
                    (tpre_re_node_t) {
                        node->match,
                        on_ok,
                        on_error,
                        bt,
                        node->group
                    });
        } break;

        case NodeLazyRepeatLeast0: {
            lower(out, this_id, this_id, on_ok, 0, NULL, node->repeat);
        } break;

        case NodeChain: {
            size_t nimatch = 0;
            tpre_nodeid_t right = tpre_re_resvnode(out);
            assert(right != this_id);
            lower(out, this_id, right, on_error, bt, &nimatch, node->chain.a);
            if (num_match) (*num_match) += nimatch;
            lower(out, right, on_ok, on_error, bt + nimatch, num_match, node->chain.b);
        } break;

        case NodeOr: {
            tpre_nodeid_t right = tpre_re_resvnode(out);
            assert(right != this_id);
            lower(out, this_id, on_ok, right, 0, NULL, node->or.a);
            lower(out, right, on_ok, on_error, 0, NULL, node->or.b);
        } break;

        case NodeMaybe: {
            lower(out, this_id, on_ok, on_ok, bt, num_match, node->maybe);
        } break;

        default:
            assert(false && "bruh");
            break;
    }
}

static Node* leftmost(Node* node) {
    switch (node->kind) {
        case NodeLazyRepeatLeast0:
        case NodeLazyRepeatLeast1:
        case NodeRepeatLeast0:
        case NodeRepeatLeast1:
            return leftmost(node->repeat);

        case NodeChain:
            return leftmost(node->chain.a);

        case NodeMaybe:
            return leftmost(node->maybe);

        default:
            return node;
    }
}

static void check_legal(Node* nd) {
    if (nd->kind == NodeOr)
    {
        // this is invalid: a*?b|ac
        // this is invalid too: (ab)*|(ac)*
    
        NodeLi cases = {0};
        or_cases(nd, &cases);

        size_t i, j;
        for (i = 0; i < cases.len; i ++) {
            for (j = 0; j < cases.len; j ++) {
                if (i == j) continue;

                Node* a = leftmost(cases.items[i]);
                Node* b = leftmost(cases.items[j]);

                if (Node_eq(a, b)) {
                    fprintf(stderr, "this pattern is not supported by tpre yet. (check_legal() or cases)\n");
                    exit(1);
                }
            }
        }

        free(cases.items);
    }

    Node* children[2];
    Node_children(nd, children);
    if (children[0]) check_legal(children[0]);
    if (children[1]) check_legal(children[1]);
}

static void tpre_dump(tpre_re_t out)
{
    printf("nd\tok\terr\tv\tbt\n");
    size_t i;
    for (i = 0; i < out.num_nodes; i ++)
    {
        char s[3];
        if (out.i[i].pat.is_special) {
            s[0] = '\\';
            switch (out.i[i].pat.val) {
                case SPECIAL_ANY:
                    s[1] = '*';
                    break;
                case SPECIAL_END:
                    s[1] = '0';
                    break;
                case SPECIAL_SPACE:
                    s[1] = 's';
                    break;
                default:break;
            }
            s[2] = '\0';
        } else {
            s[0] = out.i[i].pat.val;
            s[1] = '\0';
        }
        printf("%zu\t%i\t%i\t%s\t%u\n", i, out.i[i].ok, out.i[i].err, s, out.i[i].backtrack);
    }
}

/** 0 = ok */
int tpre_compile(tpre_re_t* out, char const * str, tpre_errs_t * errs_out)
{
    if (errs_out) {
        errs_out->len = 0;
        errs_out->items = NULL;
    }

    TkL li = lexe(str);
    Node* nd = parse(li);
    verify(nd);
    tpre_groupid_t nextgr = 1;
    groups(nd, 0, &nextgr);
    fix_0(nd);
    fix_1(nd);
    fix_2(nd);
    check_legal(nd);

    //Node_print(nd, stdout, 0, true);

    memset(out, 0, sizeof(tpre_re_t));
    tpre_nodeid_t nd0 = tpre_re_resvnode(out);
    lower(out, nd0, NODE_DONE, NODE_ERR, 0, NULL, nd);

    //tpre_dump(*out);

    return 0;
}

void tpre_free(tpre_re_t re)
{
    if (re.free) {
        free(re.i);
    }
}

void tpre_errs_free(tpre_errs_t errs)
{
    size_t i;
    for (i = 0; i < errs.len; i ++)
        free(errs.items[i].message);
    free(errs.items);
}

// TODO: this will break the enine: a*?b|ac
// TODO: this will break the engine (ab)*|(ac)*
