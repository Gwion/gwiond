#ifndef MINIC_H
#define MINIC_H

#include <cjson/cJSON.h>

/*
 * Parse the `text` string and return info about the specified symbol.
 */
cJSON* symbol_info(char *const symbol_name, char *const text);

/*
 * Parse the `text` string and return definition location of the specified symbol.
 */
cJSON* symbol_location(const char *filename, const char *symbol_name, char *text);

/*
 * Parse the `text` string and return all completions for the specified name part.
 */
cJSON* symbol_completion(const char *symbol_name_part, const char *text);

void gwiond_ini(void);
void gwiond_end(void);

typedef struct {
  const char *filename;
  cJSON *cjson;
} DiagnosticInfo;

void gwiond_parse(MP_Vector **diagnostics, const char *filename, char *text);
#endif /* end of include guard: MINIC_H */
