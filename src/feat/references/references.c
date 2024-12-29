#include "gwion_util.h" // IWYU pragma: export
#include "gwion_ast.h"  // IWYU pragma: export
#include "gwion_env.h"  // IWYU pragma: export
#include "vm.h"         // IWYU pragma: export
#include "gwion.h"
#include "lsp.h"
#include "util.h"
#include "feat/get_value.h"

#include "references_pass.h"

void lsp_reference(const Gwion gwion, int id, const cJSON *params_json) {
  message("foo", 2);
  TextDocumentPosition document = lsp_parse_document(params_json);
  Buffer buffer = get_buffer(document.uri);
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
  references_ast(&r, buffer.context->tree); 

  cJSON *results = cJSON_CreateArray();
  for(uint32_t i = 0; i < r.list->len; i++) {
    const Reference ref = referencelist_at(r.list, i);
    cJSON *location  = cJSON_CreateObject();
    json_location(location, ref.uri, ref.loc);
     cJSON_AddItemToArray(results, location);
  }
  lsp_send_response(id, results);
  
  free_referencelist(gwion->mp, r.list);
}
