#include "gwion_util.h" // IWYU pragma: export
#include "gwion_ast.h"  // IWYU pragma: export
#include "gwion_env.h"  // IWYU pragma: export
#include "vm.h"         // IWYU pragma: export
#include "gwion.h"
#include "lsp.h"
#include "util.h"
#include "feat/get_value.h"
#include "references_pass.h"
#include <cjson/cJSON.h>

void lsp_rename(const Gwion gwion, int id, const cJSON *params_json) {
  // TODO: check new value is valid!!!!!
  TextDocumentPosition document = lsp_parse_document(params_json);

  char *newName = get_string(params_json, "newName");
  Buffer buffer = get_buffer(document.uri);
  if(!buffer.context || !buffer.context->tree) {
    lsp_send_response(id, NULL);
    return;
  }
  const Value value = value_pass(gwion, buffer.context, document);
  if(!value) {
    lsp_send_response(id, NULL);
    return;
  }
  References r = {
    .mp    = gwion->mp,
    .value = value,
    .filename = document.uri,
    .list  = new_referencelist(gwion->mp, 0),
  };
  if(buffer.context && buffer.context->tree)
    references_ast(&r, buffer.context->tree); 
  cJSON *result = cJSON_CreateObject();
  cJSON *changes = cJSON_AddObjectToObject(result, "changes");
  cJSON *edits = cJSON_AddArrayToObject(changes, document.uri);
  for(uint32_t i = 0; i < r.list->len; i++) {
    const Reference ref = referencelist_at(r.list, i);
    cJSON *location  = cJSON_CreateObject();
    json_range(location, ref.loc);
    cJSON_AddStringToObject(location, "newText", newName);
    cJSON_AddItemToArray(edits, location);
  }
  lsp_send_response(id, result);
  free_referencelist(gwion->mp, r.list);
}
