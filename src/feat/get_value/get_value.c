#include "gwion_util.h" // IWYU pragma: export
#include "gwion_ast.h"  // IWYU pragma: export
#include "gwion_env.h"  // IWYU pragma: export
#include "lsp.h"
#include "feat/get_value.h"

ANN Value value_pass(const Gwion gwion, const Context context, TextDocumentPosition document) {
  GetValue gv = {
    .filename = { 
      .target = document.uri,
      .source = document.uri,
    },
    .gwion = gwion,
    .pos = document.pos
  };
  if(getvalue_ast(&gv, context->tree))
    return gv.value;
  return NULL;
}

ANN Value get_value(const Gwion gwion, const cJSON *params_json) {
  const TextDocumentPosition document = lsp_parse_document(params_json);
  const Buffer buffer = get_buffer(document.uri);
  if(!buffer.context || !buffer.context->tree) return NULL;
  return value_pass(gwion, buffer.context, document);
}
