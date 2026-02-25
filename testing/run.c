#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "minirent.h"
#include "cJSON.h"
#include "../tpre.h"

struct cjson_buf_pair {
  cJSON* json;
  char*  buf;
};

static
void
cjson_buf_pair_free(struct cjson_buf_pair p)
{
  cJSON_Delete(p.json);
  free(p.buf);
}

static
struct cjson_buf_pair
read_json_file(char const* path)
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

int main() {
  DIR* d = opendir("test-cases/test-cases");
  assert(d);

  struct dirent* ent;
  while ( (ent = readdir(d)) ) {
    if (!strcmp(ent->d_name, "..")) {
      continue;
    }

    if (!strcmp(ent->d_name, ".")) {
      continue;
    }

    char buf[512];

    sprintf(buf, "test-cases/test-cases/%s", ent->d_name);
    struct cjson_buf_pair test_case = read_json_file(buf);

    sprintf(buf, "test-cases/test-results/re2/%s", ent->d_name);
    struct cjson_buf_pair result = read_json_file(buf);

    char const *test_name = cJSON_GetStringValue(cJSON_GetObjectItem(test_case.json, "name"));
    assert(test_name);

    cJSON* strs = cJSON_GetObjectItem(test_case.json, "strs");
    assert(strs);

    cJSON* regexs = cJSON_GetObjectItem(test_case.json, "regexs");
    assert(regexs);

    assert(cJSON_GetArraySize(result.json) == cJSON_GetArraySize(regexs));

    for (size_t regex_i = 0; regex_i < cJSON_GetArraySize(regexs); regex_i ++)
    {
      char const* regex_src = cJSON_GetStringValue(cJSON_GetArrayItem(regexs, regex_i));
      assert(regex_src);

      cJSON* values = cJSON_GetArrayItem(result.json, regex_i);
      assert(values);

      assert(cJSON_GetArraySize(values) == cJSON_GetArraySize(strs));

      printf("pat: %s\n", regex_src);

      tpre_re_t re;
      int nok = tpre_compile(&re, regex_src, NULL);

      for (size_t value_i = 0; value_i < cJSON_GetArraySize(values); value_i ++)
      {
        char const* str = cJSON_GetStringValue(cJSON_GetArrayItem(strs, value_i));
        cJSON* val = cJSON_GetArrayItem(values, value_i);
        // val is an array of [re2 anchored at start and end,
        //                     re2 unanchored,
        //                     re_longest anchored at start and end
        //                     re_longest unanchored]
        cJSON* expected_anchored = cJSON_GetArrayItem(val, 0);
        assert(expected_anchored);
      }
    }

    cjson_buf_pair_free(test_case);
    cjson_buf_pair_free(result);
  }

  closedir(d);
}
