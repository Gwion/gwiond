#ifndef GWIOND_H
#define GWIOND_H

#include <cjson/cJSON.h>
/*
 * Parse the `text` string and return all completions for the specified name part.
 */
cJSON* symbol_completion(const Gwion, const char *symbol_name_part, const char *text);

Gwion gwiond_ini(void);
void gwiond_end(void);

typedef struct Diagnostic {
  const char *filename;
  cJSON *cjson;
} Diagnostic;
MK_VECTOR_TYPE(Diagnostic, diagnostic)

void gwiond_parse(const Gwion gwion, DiagnosticList **diagnostics, const char *filename, Buffer*);

ANN bool doc_pass(const Env env, Ast *b);
Context get_context(void);
#endif /* end of include guard: MINIC_H */
