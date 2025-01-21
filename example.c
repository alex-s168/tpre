#include "tpre.h"

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
