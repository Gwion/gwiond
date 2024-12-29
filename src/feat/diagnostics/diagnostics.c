#include "err_codes.h"
#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "vm.h"
#include "gwion.h"
#include "lsp.h"
#include "util.h"
#include "gwiond.h"

static void diagnostic_clear(const char *uri) {
  cJSON *params = cJSON_CreateObject();
  cJSON_AddStringToObject(params, "uri", uri);
  cJSON_AddArrayToObject(params, "diagnostics");
  lsp_send_notification("textDocument/publishDiagnostics", params);
}

void lsp_diagnostics(const Gwion gwion, char *const uri, Buffer *buffer) {
  DiagnosticList *infos = new_diagnosticlist(gwion->mp, 0);
  // we needa make a AstGetter?
  gwiond_parse(gwion, &infos, uri, buffer);
  if(!infos->len) diagnostic_clear(uri);
  else {
    for(uint32_t i = 0; i < infos->len; i++) {
      const Diagnostic info = diagnosticlist_at(infos, i);
      lsp_send_notification("textDocument/publishDiagnostics", info.cjson);
    }
  }
  free_diagnosticlist(gwion->mp, infos);
}
