#ifndef GWIOND_H
#define GWIOND_H

#include <cjson/cJSON.h>
/*
 * Parse the `text` string and return all completions for the specified name part.
 */
cJSON* symbol_completion(const Gwion, const char *symbol_name_part, const char *text);

Gwion gwiond_ini(void);
void gwiond_end(void);

typedef struct {
  const char *filename;
  cJSON *cjson;
} DiagnosticInfo;

void gwiond_parse(const Gwion gwion, MP_Vector **diagnostics, const char *filename, char *text);
#endif /* end of include guard: MINIC_H */
