#include "err_codes.h"
#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "vm.h"
#include "gwion.h"
#include "lsp.h"
#include "util.h"
#include "gwiond.h"
#include "gwiond_defs.h"

#define MAX_HEADER_FIELD_LEN 100

/*
 * Converts message body of specified length to cJSON object.
 *
 * WARNING: Caller is responsible to free the result.
 */
static cJSON* lsp_parse_content(unsigned long content_length) {
  char *buffer = malloc(content_length + 1);
  if(!buffer)
    exit(EXIT_OUT_OF_MEMORY);
  size_t read_elements = fread(buffer, 1, content_length, stdin);
  if(read_elements != content_length) {
    free(buffer);
    exit(EXIT_IO_ERROR);
  }
  buffer[content_length] = '\0';
  cJSON *request = cJSON_Parse(buffer);
  free(buffer);
  if(!request) exit(EXIT_PARSE_ERROR);
  return request;
}

static unsigned long lsp_parse_header(void) {
  char buffer[MAX_HEADER_FIELD_LEN];
  unsigned long content_length = 0;

  for(;;) {
    fgets(buffer, MAX_HEADER_FIELD_LEN, stdin);
    if(!strcmp(buffer, "\r\n")) { // End of header
      if(!content_length) {
        message("incomplete header", LSP_ERROR);
        return 0;
      }
      return content_length;
    }

    char *buffer_part = strtok(buffer, " ");
    if(!strcmp(buffer_part, "Content-Length:")) {
      buffer_part = strtok(NULL, "\n");
      content_length = atoi(buffer_part);
    }
  }
}

static void json_rpc(const Gwion gwion, const cJSON *request) {
  const char *method;
  int id = -1;

  const cJSON *method_json = cJSON_GetObjectItem(request, "method");
  if(!cJSON_IsString(method_json)) {
    exit(EXIT_CONTENT_INCOMPLETE);
  }
  method = method_json->valuestring;

  const cJSON *id_json = cJSON_GetObjectItem(request, "id");
  if(cJSON_IsNumber(id_json)) {
    id = id_json->valueint;
  }

  const cJSON *params_json = cJSON_GetObjectItem(request, "params");

  if(!strcmp(method, "initialize"))
    lsp_initialize(gwion, id, params_json);
  else if(!strcmp(method, "shutdown"))
    lsp_shutdown(id);
  else if(!strcmp(method, "exit"))
    lsp_exit();
  else if(!strcmp(method, "textDocument/didOpen"))
    lsp_sync_open(gwion, params_json);
  else if(!strcmp(method, "textDocument/didChange"))
    lsp_sync_change(gwion, params_json);
  else if(!strcmp(method, "textDocument/didClose"))
    lsp_sync_close(gwion, params_json);
  else if(!strcmp(method, "textDocument/hover"))
    lsp_hover(gwion, id, params_json);
  else if(!strcmp(method, "textDocument/definition"))
    lsp_goto_definition(gwion, id, params_json);
  else if(!strcmp(method, "textDocument/declaration"))
    lsp_goto_definition(gwion, id, params_json);
  else if(!strcmp(method, "textDocument/implementation"))
    lsp_goto_definition(gwion, id, params_json);
  else if(!strcmp(method, "textDocument/typeDefinition"))
    lsp_goto_type(gwion, id, params_json);
  else if(!strcmp(method, "textDocument/completion"))
    lsp_completion(gwion, id, params_json);
  else if(!strcmp(method, "textDocument/formatting"))
    lsp_format(gwion, id, params_json);
  else if(!strcmp(method, "textDocument/signatureHelp"))
    lsp_signatureHelp(gwion, id, params_json);
  else if(!strcmp(method, "textDocument/references"))
    lsp_reference(gwion, id, params_json);
  else if(!strcmp(method, "textDocument/rename"))
    lsp_rename(gwion, id, params_json);
  else if(!strcmp(method, "textDocument/foldingRange"))
    lsp_foldingRange(gwion, id, params_json);
  else if(!strcmp(method, "textDocument/documentSymbol"))
    lsp_symbols(gwion, id, params_json);
  else if(!strcmp(method, "textDocument/selectionRange"))
    lsp_selectionRange(gwion, id, params_json);
}

// *********************
// LSP helper functions:
// *********************

TextDocumentPosition lsp_parse_document(const cJSON *params_json) {
  TextDocumentPosition document;

  const cJSON *text_document_json = cJSON_GetObjectItem(params_json, "textDocument");
  document.uri = get_string(text_document_json, "uri");
  if(!document.uri) exit(EXIT_CONTENT_INCOMPLETE);

  const cJSON *position_json = cJSON_GetObjectItem(params_json, "position");
  const cJSON *line_json = cJSON_GetObjectItem(position_json, "line");
  if(!cJSON_IsNumber(line_json)) {
    exit(EXIT_CONTENT_INCOMPLETE);
  }
  document.pos.line = line_json->valueint;
  const cJSON *character_json = cJSON_GetObjectItem(position_json, "character");
  if(!cJSON_IsNumber(character_json)) {
    exit(EXIT_CONTENT_INCOMPLETE);
  }
  document.pos.column = character_json->valueint;
  return document;
}

static cJSON *rpc_ini(void) {
  cJSON *response = cJSON_CreateObject();
  cJSON_AddStringToObject(response, "jsonrpc", "2.0");
  return response;
}

ANN static void rpc_end(cJSON *response) {
  char *output = cJSON_Print(response);
  if(output) {
    cJSON_Minify(output); 
    printf("Content-Length: %lu\r\n\r\n", strlen(output));
    fwrite(output, 1, strlen(output), stdout);
    fflush(stdout);
    free(output);
  }
  cJSON_Delete(response);
}

void lsp_send_response(int id, cJSON *result) {
  cJSON *response = rpc_ini();
  cJSON_AddNumberToObject(response, "id", id);
  if(result)
    cJSON_AddItemToObject(response, "result", result);
  rpc_end(response);
}

void lsp_send_notification(const char *method, cJSON *params) {
  cJSON *response = rpc_ini();
  cJSON_AddStringToObject(response, "method", method);
  if(params)
    cJSON_AddItemToObject(response, "params", params);
  rpc_end(response);
}

// **************
// RPC functions:

static char* read_file(MemPool mp, const char *filename) {
  FILE *file = fopen(filename, "r");
  if(!file) return NULL;
  fseek(file, 0, SEEK_END); 
  const long size = ftell(file);
  fseek(file, 0, SEEK_SET); 
  char *buffer = mp_malloc2(mp, size + 1);
  fread(buffer, 1, size, file);
  buffer[size] = '\0';
  fclose(file); 
  return buffer;
}

static cJSON *workspaces = NULL; // TODO; clean at exit
void lsp_initialize(Gwion gwion, int id, const cJSON *params) { 
  if(!workspaces) workspaces = cJSON_CreateArray();
  cJSON *workspaceFolders = cJSON_GetObjectItem(params, "workspaceFolders");
  if(workspaceFolders) {
    for(int i = 0; i < cJSON_GetArraySize(workspaceFolders); i++) {
      cJSON *json = cJSON_GetArrayItem(workspaceFolders, i);

      const char* uri = get_string(json, "uri");
      //char *buffer = read_file(gwion->mp, "gwiond.json");
      char *buffer = read_file(gwion->mp, "/mnt/code/Gwion/gwion/plug/Gwylan/gwiond.json");
      if(!buffer) continue;
      cJSON *result = cJSON_Parse(buffer); 
      free_mstr(gwion->mp, buffer);
      if (result) { 
        cJSON_AddStringToObject(result, "uri", uri);
        cJSON_AddItemToArray(workspaces, result);

        cJSON *plugdirs = cJSON_GetObjectItem(result, "plugdirs");
        if(plugdirs) {
          // TODO: could be improved
          struct Vector_ v;
          vector_init(&v); 
          for(int j = 0; j < cJSON_GetArraySize(plugdirs); j++) {
            cJSON *plugdir = cJSON_GetArrayItem(plugdirs, i);
            char *plugdir_str = cJSON_GetStringValue(plugdir);
            message(plugdir_str, 0);
            vector_add(&v, (m_uint)plugdir_str);
          }
          plug_ini(gwion, &v);
          vector_release(&v);
        }
      }
    }
  }
  cJSON *result = cJSON_CreateObject();
  cJSON *capabilities = cJSON_AddObjectToObject(result, "capabilities");
  cJSON_AddNumberToObject(capabilities, "textDocumentSync", 1);
  cJSON_AddBoolToObject(capabilities, "hoverProvider", 1);
  cJSON_AddBoolToObject(capabilities, "definitionProvider", 1);
  cJSON_AddBoolToObject(capabilities, "declarationProvider", 1);
  cJSON_AddBoolToObject(capabilities, "implementationProvider", 1);
  cJSON_AddBoolToObject(capabilities, "typeDefinitionProvider", 1);
  cJSON_AddBoolToObject(capabilities, "documentFormattingProvider", 1);
  cJSON *completion = cJSON_AddObjectToObject(capabilities, "completionProvider");
  cJSON_AddBoolToObject(completion, "resolveProvider", 0);


  cJSON_AddBoolToObject(capabilities, "foldingRangeProvider", 1);

  cJSON_AddBoolToObject(capabilities, "referencesProvider", 1);
  cJSON_AddBoolToObject(capabilities, "renameProvider", 1);
  cJSON_AddBoolToObject(capabilities, "documentSymbolProvider", 1);
  cJSON_AddBoolToObject(capabilities, "selectionRangeProvider", 1);

  cJSON *signature = cJSON_AddObjectToObject(capabilities, "signatureHelpProvider");
  cJSON *triggerCharacters = cJSON_AddArrayToObject(signature, "triggerCharacters");
  cJSON_AddItemToArray(triggerCharacters, cJSON_CreateString("("));
  cJSON *retriggerCharacters = cJSON_AddArrayToObject(signature, "retriggerCharacters");
  cJSON_AddItemToArray(retriggerCharacters, cJSON_CreateString(","));

  lsp_send_response(id, result);
}

void lsp_event_loop(const Gwion gwion) {
  for(;;) {
    unsigned long content_length = lsp_parse_header();
    cJSON *request = lsp_parse_content(content_length);
    json_rpc(gwion, request);
    cJSON_Delete(request);
  }
}

void lsp_shutdown(int id) {
  lsp_send_response(id, NULL);
}

void lsp_exit(void) {
  exit(0);
}
