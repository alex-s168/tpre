#include "tpre.h"
#include <stdlib.h>

static void compile(tpre_re_t* out, char const* str)
{
    tpre_errs_t errs;
    if (tpre_compile(out, str, &errs) != 0) {
        fprintf(stderr, "regex compile failed:\n");
        size_t i;
        for (i = 0; i < errs.len; i ++) {
            fprintf(stderr, "  %s\n", errs.items[i].message);
        }
        tpre_errs_free(errs);
        exit(1);
    }
}

static void simple_groups() {
    const char * restr = "\\s*?(?:(red|green|blue)\\s+?)?(car|train)\\s*?";

    tpre_re_t re;
    compile(&re, restr);

    tpre_match_t m;
    char const* str;

#define MATCH \
    m = tpre_match(&re, str); \
    tpre_match_dump(m, str, stdout); \
    tpre_match_free(m); \

    str = "blue car";
    MATCH;

    str = "   red   car ";
    MATCH;

    str = "  green   train    ";
    MATCH;

    str = "    car    ";
    MATCH;

    str = "    train    ";
    MATCH;

    str = "bluecar";
    MATCH;

    tpre_free(re);
}

static void named_groups() {
    const char * restr = "\\s*?(?:(?'color'red|green|blue)\\s+?)?(?'type'car|train)\\s*?";

    tpre_re_t re;
    compile(&re, restr);

    tpre_match_t m;
    char const* str;

#define MATCH \
    m = tpre_match(&re, str); \
    tpre_match_dump(m, str, stdout); \
    tpre_match_free(m); \

    str = "blue car";
    MATCH;

    str = "   red   car ";
    MATCH;

    str = "  green   train    ";
    MATCH;

    str = "    car    ";
    MATCH;

    str = "    train    ";
    MATCH;

    str = "bluecar";
    MATCH;

    tpre_free(re);
}

static void named_and_simple_groups() {
    const char * restr = "\\s*?(?:(?'color'red|green|blue)\\s+?)?(car|train)\\s*?";

    tpre_re_t re;
    compile(&re, restr);

    tpre_match_t m;
    char const* str;

#define MATCH \
    m = tpre_match(&re, str); \
    tpre_match_dump(m, str, stdout); \
    tpre_match_free(m); \

    str = "blue car";
    MATCH;

    str = "   red   car ";
    MATCH;

    str = "  green   train    ";
    MATCH;

    str = "    car    ";
    MATCH;

    str = "    train    ";
    MATCH;

    str = "bluecar";
    MATCH;

    tpre_free(re);
}

int main() {
    puts(" ==== SIMPLE GROUPS ====");
    simple_groups();

    puts(" ==== NAMED GROUPS ====");
    named_groups();

    puts(" ==== NAMED AND SIMPLE GROUPS ====");
    named_and_simple_groups();
}
