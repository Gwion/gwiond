#ifndef LSP_H
#define LSP_H

#include <cjson/cJSON.h>
#include "io.h"

/*
 * Main event loop.
 */
void lsp_event_loop(const Gwion);

/*
 * Parses message header and returns the content length.
 */
unsigned long lsp_parse_header(void);

/*
 * Converts message body of specified length to cJSON object.
 *
 * WARNING: Caller is responsible to free the result.
 */
cJSON* lsp_parse_content(unsigned long content_length);

/*
 * Parses RPC request and calls the appropriate function.
 */
void json_rpc(const Gwion, const cJSON *request);

// *********************
// LSP helper functions:
// *********************

typedef struct {
  char *uri;
  int line;
  int character;
} Document_LOCATION;

/*
 * Parses document location from LSP request.
 */
Document_LOCATION lsp_parse_document(const cJSON *params_json);

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

/*
 * Parses LSP text sync notifications, and updates buffers and diagnostics.
 */
void lsp_sync_open(const Gwion, const cJSON *params_json);
void lsp_sync_change(const Gwion, const cJSON *params_json);
void lsp_sync_close(const Gwion, const cJSON *params_json);

/*
 * Runs a gwfmter and returns LSP publish diagnostics notification.
 */
void lsp_gwfmt(const Gwion, char *const uri, Buffer *buffer);
void lsp_format(const Gwion, int id, const cJSON *params_json);
/*
 * Clears diagnostics for a file with specified `uri`.
 */
void lsp_gwfmt_clear(const char *uri);

/*
 * Parses LSP hover request, and returns hover information.
 */
void lsp_hover(const Gwion, int id, const cJSON *params_json);

/*
 * Parses LSP definition request, and returns jump position.
 */
void lsp_goto_definition(const Gwion, int id, const cJSON *params_json);

/*
 * Parses LSP completion request, and returns completion results.
 */
void lsp_completion(const Gwion, int id, const cJSON *params_json);

static inline void message(const char* message, int severity) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "type", severity);
    cJSON_AddStringToObject(json, "message", message);
    lsp_send_notification("window/showMessage", json);
}
#endif /* end of include guard: LSP_H */
