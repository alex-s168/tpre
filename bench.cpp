#include "tpre.h"
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <regex>

#include <string.h>
#include <string>
#include <time.h>
#include <stdio.h>

#define SPECIAL_ANY   (0)
#define SPECIAL_SPACE (1)
#define SPECIAL_END   (2)
#define NO(c) ((tpre_pattern_t) {.is_special = 0,.val=(uint8_t)c})
#define SP(c) ((tpre_pattern_t) {.is_special = 1,.val=(uint8_t)c})

int main()
{
    const char * resrc = "\\s*?(red|green|blue)?\\s*?(car|train)\\s*?";

    tpre_re_t re;
    if (tpre_compile(&re, resrc, NULL) != 0)
        return 1;

    {
        tpre_match_t match = tpre_match(&re, " green car ");
        tpre_match_dump(match, stdout);
        tpre_match_free(match);
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
        std::string strs(str);
        std::regex pattern(resrc);

        start = (double) clock() / CLOCKS_PER_SEC;
        for (size_t i = 0; i < niter; i ++) {
            std::smatch matches;
            if (!std::regex_search(strs, matches, pattern)) return 1;
            (void) (volatile typeof(matches)) matches;
        }
        end = (double) clock() / CLOCKS_PER_SEC;
        printf("C++ regex took %f ms\n", (end - start) / 1000);
    }

    {
        start = (double) clock() / CLOCKS_PER_SEC;
        for (size_t i = 0; i < niter; i ++) {
            tpre_match_t match = tpre_match(&re, str);
            tpre_match_free(match);
            (void) (volatile tpre_match_t) match;
        }
        end = (double) clock() / CLOCKS_PER_SEC;
        printf("this lib took %f ms\n", (end - start) / 1000);
    }
}
