#include "tpre.h"
#include <string.h>

static int test(const char* pats, const char * str, bool should_match, const char* group1, const char* group2)
{
    tpre_re_t pat;
    tpre_errs_t errs;
    if (tpre_compile(&pat, pats, &errs))
        return 1;

    // no nullterm
    static char buf[200];
    memcpy(buf, str, strlen(str));

    tpre_match_t m0 = tpre_match(&pat, str);
    tpre_match_t m1 = tpre_matchn(&pat, buf, strlen(str));

    if (m0.found != should_match)
        return 1;

    if (m1.found != should_match)
        return 1;

    if (group1) {
        if (m0.ngroups < 2)
            return 1;
        if (m1.ngroups < 2)
            return 1;
        if (m0.groups[1].len != strlen(group1))
            return 1;
        if (m1.groups[1].len != strlen(group1))
            return 1;
        if (strncmp(buf + m0.groups[1].begin, group1, m0.groups[1].len))
            return 1;
        if (strncmp(buf + m1.groups[1].begin, group1, m1.groups[1].len))
            return 1;
    }

    if (group2) {
        if (m0.ngroups < 3)
            return 1;
        if (m1.ngroups < 3)
            return 1;
        if (m0.groups[2].len != strlen(group2))
            return 1;
        if (m1.groups[2].len != strlen(group2))
            return 1;
        if (strncmp(buf + m0.groups[2].begin, group2, m0.groups[2].len))
            return 1;
        if (strncmp(buf + m1.groups[2].begin, group2, m1.groups[2].len))
            return 1;
    }

    return 0;
}

int main() {
    size_t tnum = 1;
#define test(...) if (test(__VA_ARGS__)) { return tnum; } tnum ++;

    test("\\s*?(red|green|blue)?\\s*?(car|train)\\s*?", "  red    train  ", true, "red", "train");
    test("\\s*?(red|green|blue)?\\s*?(car|train)\\s*?", "red    train", true, "red", "train");
    test("\\s*?(red|green|blue)?\\s*?(car|train)\\s*?", "  blue  car", true, "blue", "car");
    test("\\s*?(red|green|blue)?\\s*?(car|train)\\s*?", "   car ", true, NULL, "car");
    test("\\s*?(red|green|blue)?\\s*?(car|train)\\s*?", " train", true, NULL, "train");
    test("\\s*?(red|green|blue)?\\s*?(car|train)\\s*?", "bluecar", true, "blue", "car");

    test("\\s*?(?:(red|green|blue)\\s+?)?(car|train)\\s*?", "  red    train  ", true, "red", "train");
    test("\\s*?(?:(red|green|blue)\\s+?)?(car|train)\\s*?", "red    train", true, "red", "train");
    test("\\s*?(?:(red|green|blue)\\s+?)?(car|train)\\s*?", "  blue  car", true, "blue", "car");
    test("\\s*?(?:(red|green|blue)\\s+?)?(car|train)\\s*?", "   car", true, NULL, "car");
    test("\\s*?(?:(red|green|blue)\\s+?)?(car|train)\\s*?", "   train   ", true, NULL, "train");
    test("\\s*?(?:(red|green|blue)\\s+?)?(car|train)\\s*?", "bluecar", false, NULL, NULL);

    return 0;
}
