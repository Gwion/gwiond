#pragma once
ANN Value value_pass(const Gwion gwion, const Context context, TextDocumentPosition document);
ANN Value get_value(const Gwion gwion, const cJSON *params_json);
ANN ValueList *get_value_all(MemPool, const Ast);

typedef struct GetValue {
  struct {
    const char *source;
    const char *target;
  } filename;
  Gwion gwion;
  Value value;
  pos_t pos;
} GetValue;

ANN bool getvalue_ast(GetValue *a, Ast b);
