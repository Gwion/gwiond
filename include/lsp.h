#ifndef LSP_H
#define LSP_H

#include <cjson/cJSON.h>
#include "io.h"

/*
 * Main event loop.
 */
void lsp_event_loop(const Gwion);

// *********************
// LSP helper functions:
// *********************

typedef struct {
  char *uri;
  pos_t pos;
} TextDocumentPosition;

/*
 * Parses document location from LSP request.
 */
TextDocumentPosition lsp_parse_document(const cJSON *params_json);

/*
 * Sends a LSP message response.
 */
void lsp_send_response(int id, cJSON *result);

/*
 * Sends a LSP notification.
 */
void lsp_send_notification(const char *method, cJSON *params);

// **************
// RPC functions:
// **************

/*
 * Parses LSP initialize request, and sends a response accordingly.
 * Response specifies language server's capabilities.
 */
void lsp_initialize(Gwion, int id, const cJSON *params);

/*
 * Parses LSP shutdown request, and sends a response.
 */
void lsp_shutdown(int id);

/*
 * Stops the language server.
 */
void lsp_exit(void);

void lsp_sync_open(const Gwion, const cJSON *params_json);
void lsp_sync_change(const Gwion, const cJSON *params_json);
void lsp_sync_close(const Gwion, const cJSON *params_json);
void lsp_signatureHelp(const Gwion, int id, const cJSON *params_json);
void lsp_foldingRange(Gwion gwion, int id, const cJSON *params_json);
void lsp_selectionRange(Gwion gwion, int id, const cJSON *params_json);
void lsp_diagnostics(const Gwion, char *const uri, Buffer *buffer);
void lsp_format(const Gwion, int id, const cJSON *params_json);
void lsp_hover(const Gwion, int id, const cJSON *params_json);
void lsp_reference(const Gwion, int id, const cJSON *params_json);
void lsp_symbols(const Gwion, int id, const cJSON *params_json);
void lsp_rename(const Gwion, int id, const cJSON *params_json);
void lsp_goto_definition(const Gwion, int id, const cJSON *params_json);
void lsp_goto_type(const Gwion, int id, const cJSON *params_json);
void lsp_completion(const Gwion, int id, const cJSON *params_json);

static inline void message(const char* message, int severity) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "type", severity);
    cJSON_AddStringToObject(json, "message", message);
    lsp_send_notification("window/showMessage", json);
}
ANN m_str get_doc(Context context, void *data);
#endif /* end of include guard: LSP_H */
