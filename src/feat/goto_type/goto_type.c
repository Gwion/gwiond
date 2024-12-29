#include "gwion_util.h"
#include "gwion_ast.h" // IWYU pragma: export
#include "gwion_env.h" // IWYU pragma: export
#include "vm.h" // IWYU pragma: export
#include "gwion.h" // IWYU pragma: export
#include "lsp.h"
#include "util.h"
#include "feat/get_value.h"

void lsp_goto_type(const Gwion gwion, int id, const cJSON *params_json) {
  const Value value = get_value(gwion, params_json);
  if(!value) {
    lsp_send_response(id, NULL);
    return;
  }
  cJSON *result = cJSON_CreateObject();
  // TODO: check if that's in the gwion source code
  // as builtin point to their source location
  // NOTE: should check if it's a class
  const Value value_type = value->type->info->value;
  json_location(result, value_type->from->filename, value_type->from->loc);
  lsp_send_response(id, result);
}
