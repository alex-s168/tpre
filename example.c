#include "tpre.h"

int main() {
    const char * restr = "\\s*?(?:(red|green|blue)\\s+?)?(car|train)\\s*?";

    tpre_re_t re;
    tpre_errs_t errs;
    if (tpre_compile(&re, restr, &errs) != 0) {
        fprintf(stderr, "regex compile failed:\n");
        size_t i;
        for (i = 0; i < errs.len; i ++) {
            fprintf(stderr, "  %s\n", errs.items[i].message);
        }
        tpre_errs_free(errs);
        return 1;
    }

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
