#include "gwion_util.h"
#include "gwion_ast.h" // IWYU pragma: export
#include "gwion_env.h" // IWYU pragma: export
#include "vm.h"        // IWYU pragma: export
#include "gwion.h"
#include "lsp.h"
#include "feat/get_value.h"

ANN static char* value_info(const Gwion gwion, const Value v) {
  char *str = NULL;
  const char *name = v->name;
  const m_str doc = get_doc(v->from->ctx, v);
  gw_asprintf(gwion->mp, &str,
"# (%s) `%s`\n"
"-------------\n"
"## of type: `%s`\n"
"-------------\n"
"%s%s%s"
"## in file: *%s*\n",
// TODO: closures, enum and stuff?
  v->type ?              // 
  is_class(gwion, v->type) ? "class" :
  is_func(gwion, v->type) ? "function" :
  "variable" : "unknown",
  name, v->type->name,  
  doc ? "## documentation:\n" : "",
  doc ?: "",
  doc ? "\n--------\n" : "",
  v->from->filename);
  return str;
}

void lsp_hover(const Gwion gwion, int id, const cJSON *params_json) {
  const TextDocumentPosition document = lsp_parse_document(params_json);
  const Buffer buffer = get_buffer(document.uri);
  const Value value = value_pass(gwion, buffer.context, document);
  if(!value) {
    lsp_send_response(id, NULL);
    return;
  }
  cJSON *result = cJSON_CreateObject();
  cJSON *array = cJSON_AddArrayToObject(result, "contents");
  const m_str str = value_info(gwion, value);
  cJSON *contents = cJSON_CreateString(str); 
  free_mstr(gwion->mp, str);
  cJSON_AddItemToArray(array, contents);
  lsp_send_response(id, result);
}
