#include "err_codes.h"
#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "vm.h"
#include "gwion.h"
#include "lsp.h"
#include "util.h"
#include "gwiond_defs.h"

void lsp_sync_open(const Gwion gwion, const cJSON *params_json) {
  const cJSON *text_document_json = cJSON_GetObjectItem(params_json, "textDocument");
  char *const uri = get_string(text_document_json, "uri");
  const char *text = get_string(text_document_json, "text");
  if(!uri || !text) {
    message("incomplete content", LSP_ERROR);
    return;
  }
  Buffer *buffer = open_buffer(gwion->mp, uri, text);
  lsp_diagnostics(gwion, uri, buffer);
}

void lsp_sync_change(const Gwion gwion, const cJSON *params_json) {
  const cJSON *text_document_json = cJSON_GetObjectItem(params_json, "textDocument");
  char *const uri = get_string(text_document_json, "uri");
  const cJSON *content_changes_json = cJSON_GetObjectItem(params_json, "contentChanges");
  const cJSON *content_change_json = cJSON_GetArrayItem(content_changes_json, 0);
  const char *text = get_string(content_change_json, "text");
  if(!uri || !text) {
    message("incomplete content", LSP_ERROR);
    return;
  }
  Buffer *buffer = update_buffer(gwion->mp, uri, text);
  //  const Nspc global = buffer->context->nspc->parent;
 // free_context(buffer->context, gwion);

  //  context_remref(buffer->context, gwion);

  //  if(buffer->context->global)
//    free_nspc(global, gwion);
  lsp_diagnostics(gwion, uri, buffer); // ???
}

void lsp_sync_close(const Gwion gwion, const cJSON *params_json) {
  const cJSON *text_document_json = cJSON_GetObjectItem(params_json, "textDocument");
  const char *uri = get_string(text_document_json, "uri");
  if(!uri) {
    message("incomplete content", LSP_ERROR);
    return;
  }
  close_buffer(gwion, uri);
}


