#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/tpre.h"
#include "cJSON.h"
#include "minirent.h"

struct cjson_buf_pair
{
  cJSON* json;
  char* buf;
};

static void cjson_buf_pair_free(struct cjson_buf_pair p)
{
  cJSON_Delete(p.json);
  free(p.buf);
}

static struct cjson_buf_pair read_json_file(char const* path)
{
  FILE* file = fopen(path, "rb");
  assert(file);
  fseek(file, 0, SEEK_END);
  size_t len = ftell(file);
  rewind(file);

  char* buf = malloc(len);
  assert(buf);
  fread(buf, 1, len, file);
  fclose(file);

  cJSON* json = cJSON_ParseWithLength(buf, len);
  assert(json);

  return (struct cjson_buf_pair) {
    .json = json,
    .buf = buf,
  };
}

static int read_cmp_result(tpre_match_t res, cJSON* jr)
{
  assert(jr);
  int sz = cJSON_GetArraySize(jr);
  if (sz == 0)
    return !res.found ? 0 : -1;
  long first = cJSON_GetNumberValue(cJSON_GetArrayItem(jr, 0));
  long lastP1 = cJSON_GetNumberValue(cJSON_GetArrayItem(jr, 1));
  if (!res.found)
    return -1;
  assert(res.ngroups >= 2);
  if (res.groups[1].begin != first)
    return -1;
  if (res.groups[1].len != lastP1 - first)
    return -1;
  return 0;
}

int main()
{
  DIR* d = opendir("test-cases/test-cases");
  assert(d);

  struct dirent* ent;
  while ((ent = readdir(d)))
  {
    if (!strcmp(ent->d_name, ".."))
      continue;

    if (!strcmp(ent->d_name, "."))
      continue;

    char buf[512];

    sprintf(buf, "test-cases/test-cases/%s", ent->d_name);
    struct cjson_buf_pair test_case = read_json_file(buf);

    sprintf(buf, "test-cases/test-results/re2/%s", ent->d_name);
    struct cjson_buf_pair result = read_json_file(buf);

    char const* test_name = cJSON_GetStringValue(
        cJSON_GetObjectItem(test_case.json, "name"));
    assert(test_name);

    cJSON* strs = cJSON_GetObjectItem(test_case.json, "strs");
    assert(strs);

    cJSON* regexs = cJSON_GetObjectItem(test_case.json, "regexs");
    assert(regexs);

    assert(
        cJSON_GetArraySize(result.json) ==
        cJSON_GetArraySize(regexs));

    for (size_t regex_i = 0;
         regex_i < cJSON_GetArraySize(regexs); regex_i++)
    {
      char const* regex_src = cJSON_GetStringValue(
          cJSON_GetArrayItem(regexs, regex_i));
      assert(regex_src);

      static char src[1028];
      sprintf(src, "(%s)", regex_src);

      cJSON* values = cJSON_GetArrayItem(result.json, regex_i);
      assert(values);

      assert(
          cJSON_GetArraySize(values) == cJSON_GetArraySize(strs));

      int nok = 0;

      printf("precomp: %s\n", src);
      tpre_re_t unanchored;
      nok |= tpre_compile(
          &unanchored, src, NULL,
          (tpre_opts_t) {
            .start_unanchored = 1, .end_unanchored = 1 });
      tpre_re_t anchored;
      nok |=
          tpre_compile(&anchored, src, NULL, (tpre_opts_t) { 0 });
      printf("postcomp\n");
      fflush(stdout);

      unsigned num_pass = 0;
      unsigned num_fail = 0;
      unsigned total = cJSON_GetArraySize(values);

      if (!nok)
        for (size_t value_i = 0; value_i < total; value_i++)
        {
          char const* str = cJSON_GetStringValue(
              cJSON_GetArrayItem(strs, value_i));
          cJSON* val = cJSON_GetArrayItem(values, value_i);
          // val is an array of [re2 anchored at start and end,
          //                     re2 unanchored,
          //                     re_longest anchored at start and end
          //                     re_longest unanchored]

          size_t strl = strlen(str);

          unsigned old_num_pass = num_pass;
          printf("pat: '%s' \t str: '%s': ", src, str);
          fflush(stdout);
          if (read_cmp_result(
                  tpre_matchn(&anchored, str, strl),
                  cJSON_GetArrayItem(val, 0)))
            num_fail++;
          else
            num_pass++;
          if (read_cmp_result(
                  tpre_matchn(&unanchored, str, strl),
                  cJSON_GetArrayItem(val, 1)))
            num_fail++;
          else
            num_pass++;
          printf("done. pass: %u/2\n", num_pass - old_num_pass);
          fflush(stdout);
        }

      if (nok)
        printf("FAIL compile: '%s'\n", src);
      else if (num_fail)
        printf(
            "FAIL (pass: %u, fail: %u): '%s'\n", num_pass,
            num_fail, src);
      else
        printf("PASS\n");
      fflush(stdout);
    }

    cjson_buf_pair_free(test_case);
    cjson_buf_pair_free(result);
  }

  closedir(d);
}
