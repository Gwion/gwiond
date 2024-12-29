#pragma once
cJSON* json_range(cJSON *base, const loc_t loc);
cJSON* json_location(cJSON *base, const char *uri, const loc_t loc);
Buffer get_buffer_from_params(Gwion gwion, const cJSON *params_json);
Ast get_ast(Gwion gwion, const cJSON *params_json);
static inline char *get_string(const cJSON* json, const char *key) {
  const cJSON *val = cJSON_GetObjectItem(json, key);
  return cJSON_GetStringValue(val);
}


