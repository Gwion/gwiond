#include "gwion_util.h" // IWYU pragma: export
#include "gwion_ast.h"  // IWYU pragma: export
#include "gwion_env.h"  // IWYU pragma: export
#include "vm.h"         // IWYU pragma: export
#include "instr.h"      // IWYU pragma: export
#include "emit.h"       // IWYU pragma: export
#include "gwion.h"
#include "fold_pass.h"
#include "gwfmt.h"
#include "lsp.h"
#include "util.h"

void lsp_format(const Gwion gwion, int id, const cJSON *params_json) {
  const cJSON *options = cJSON_GetObjectItem(params_json, "params");
  const cJSON *tabsize_json = cJSON_GetObjectItem(options, "tabSize");
  int tabsize = tabsize_json->valueint;
  const cJSON *insert_spaces_json = cJSON_GetObjectItem(options, "insertSpaces");
  const int usetabs = cJSON_IsFalse(tabsize_json);
  if(tabsize<=0) tabsize = 2;
  struct GwfmtState ls = {
    .nindent = tabsize,
    .check_case = true,
    .use_tabs = usetabs,
  };
  gwfmt_state_init(&ls);
  text_init(&ls.text, gwion->mp);

  Ast ast = get_ast(gwion, params_json);
  if (!ast) {
    lsp_send_response(id, NULL);
    return;
  }
  Gwfmt l = {
    .mp = gwion->mp,
    .st = gwion->st,
    .ls = &ls,
    .last = cht_nl
  }; 
  gwfmt_ast(&l, ast);
  free_ast(gwion->mp, ast);
  cJSON *json = cJSON_CreateArray();
  cJSON *textedit = cJSON_CreateObject();
  loc_t loc = {
    .last = l.pos,
  };
  pos_ini(&loc.first);
  json_range(textedit, loc);
  cJSON_AddStringToObject(textedit, "newText", ls.text.str);
  cJSON_AddItemToArray(json, textedit);
  lsp_send_response(id, json);
  text_release(&ls.text);
}
