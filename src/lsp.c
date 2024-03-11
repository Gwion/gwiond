#include <cjson/cJSON.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "err_codes.h"
#include "gwion_util.h"
#include "lsp.h"
#include "gwiond.h"
#include "mp_vector.h"
#define MAX_HEADER_FIELD_LEN 100

static inline char *const get_string(const cJSON* json, const char *key) {
  const cJSON *val = cJSON_GetObjectItem(json, key);
  return cJSON_GetStringValue(val);
}

void lsp_event_loop(void) {
  for(;;) {
    unsigned long content_length = lsp_parse_header();
    cJSON *request = lsp_parse_content(content_length);
    json_rpc(request);
    cJSON_Delete(request);
  }
}

unsigned long lsp_parse_header(void) {
  char buffer[MAX_HEADER_FIELD_LEN];
  unsigned long content_length = 0;

  for(;;) {
    fgets(buffer, MAX_HEADER_FIELD_LEN, stdin);
    if(strcmp(buffer, "\r\n") == 0) { // End of header
      if(content_length == 0)
        exit(EXIT_HEADER_INCOMPLETE);
      return content_length;
    }

    char *buffer_part = strtok(buffer, " ");
    if(strcmp(buffer_part, "Content-Length:") == 0) {
      buffer_part = strtok(NULL, "\n");
      content_length = atoi(buffer_part);
    }
  }
}

cJSON* lsp_parse_content(unsigned long content_length) {
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

void lsp_signatureHelp(int id, const cJSON *params_json);
void lsp_foldingRange(int id, const cJSON *params_json);
void json_rpc(const cJSON *request) {
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

  if(strcmp(method, "initialize") == 0)
    lsp_initialize(id, params_json);
  else if(strcmp(method, "shutdown") == 0)
    lsp_shutdown(id);
  else if(strcmp(method, "exit") == 0)
    lsp_exit();
  else if(strcmp(method, "textDocument/didOpen") == 0)
    lsp_sync_open(params_json);
  else if(strcmp(method, "textDocument/didChange") == 0)
    lsp_sync_change(params_json);
  else if(strcmp(method, "textDocument/didClose") == 0)
    lsp_sync_close(params_json);
  else if(strcmp(method, "textDocument/hover") == 0)
    lsp_hover(id, params_json);
  else if(strcmp(method, "textDocument/definition") == 0)
    lsp_goto_definition(id, params_json);
  else if(strcmp(method, "textDocument/declaration") == 0)
    lsp_goto_definition(id, params_json);
  else if(strcmp(method, "textDocument/completion") == 0)
    lsp_completion(id, params_json);
  else if(strcmp(method, "textDocument/formatting") == 0)
    lsp_format(id, params_json);
  else if(strcmp(method, "textDocument/signatureHelp") == 0)
    lsp_signatureHelp(id, params_json);
  else if(strcmp(method, "textDocument/foldingRange") == 0)
    lsp_foldingRange(id, params_json);
}

// *********************
// LSP helper functions:
// *********************

DOCUMENT_LOCATION lsp_parse_document(const cJSON *params_json) {
  DOCUMENT_LOCATION document;

  const cJSON *text_document_json = cJSON_GetObjectItem(params_json, "textDocument");
//  document.uri = get_string(text_document_json, "uri");
  document.uri = get_string(text_document_json, "uri");
//  document.uri += 7;
  if(!document.uri) exit(EXIT_CONTENT_INCOMPLETE);

  const cJSON *position_json = cJSON_GetObjectItem(params_json, "position");
  const cJSON *line_json = cJSON_GetObjectItem(position_json, "line");
  if(!cJSON_IsNumber(line_json)) {
    return document;
    exit(EXIT_CONTENT_INCOMPLETE);
  }
  document.line = line_json->valueint;
  const cJSON *character_json = cJSON_GetObjectItem(position_json, "character");
  if(!cJSON_IsNumber(character_json)) {
    exit(EXIT_CONTENT_INCOMPLETE);
  }
  document.character = character_json->valueint;
  return document;
}

void lsp_send_response(int id, cJSON *result) {
  cJSON *response = cJSON_CreateObject();
  cJSON_AddStringToObject(response, "jsonrpc", "2.0");
  cJSON_AddNumberToObject(response, "id", id);
  if(result != NULL)
    cJSON_AddItemToObject(response, "result", result);

  char *output = cJSON_Print(response);
  //cJSON_Minify(output); 
  printf("Content-Length: %lu\r\n\r\n", strlen(output));
  fwrite(output, 1, strlen(output), stdout);
  fflush(stdout);
  free(output);
  cJSON_Delete(response);
  fflush(stdout);
}

void lsp_send_notification(const char *method, cJSON *params) {
  cJSON *response = cJSON_CreateObject();
  cJSON_AddStringToObject(response, "jsonrpc", "2.0");
  cJSON_AddStringToObject(response, "method", method);
  if(params != NULL)
    cJSON_AddItemToObject(response, "params", params);

  char *output = cJSON_Print(response);
  printf("Content-Length: %lu\r\n\r\n", strlen(output));
  fwrite(output, 1, strlen(output), stdout);
  fflush(stdout);
  free(output);
  cJSON_Delete(response);
  fflush(stdout);
}

// **************
// RPC functions:
#include "gwion_ast.h"
#include "gwion_env.h"
#include "vm.h"
#include "gwion.h"

extern struct Gwion_ gwion;
static char* read_file(const char *filename) {
  FILE *file = fopen(filename, "r");
  if(!file) return NULL;
  fseek(file, 0, SEEK_END); 
  long size = ftell(file);
  fseek(file, 0, SEEK_SET); 
  char *buffer = mp_malloc2(gwion.mp, size + 1);
  buffer[size] = '\0';
  int len = fread(buffer, 1, size, file); 
  fclose(file); 
  return buffer;
}

static cJSON *workspaces = NULL; // TODO; clean at exit
void lsp_initialize(int id, const cJSON *params) { 
  if(!workspaces) workspaces = cJSON_CreateArray();
  cJSON *workspaceFolders = cJSON_GetObjectItem(params, "workspaceFolders");
  if(workspaceFolders) {
    for(int i = 0; i < cJSON_GetArraySize(workspaceFolders); i++) {
      cJSON *json = cJSON_GetArrayItem(workspaceFolders, i);
      const char* uri = get_string(json, "uri"); 
      char *config = NULL;
      char *buffer = read_file("gwiond.json");
      if(!buffer) continue;
      cJSON *result = cJSON_Parse(buffer); 
      free_mstr(gwion.mp, buffer);
      if (result) { 
        cJSON_AddStringToObject(result, "uri", uri);
        cJSON_AddItemToArray(workspaces, result);
      }
    }
  }
  cJSON *result = cJSON_CreateObject();
  cJSON *capabilities = cJSON_AddObjectToObject(result, "capabilities");
  cJSON_AddNumberToObject(capabilities, "textDocumentSync", 1);
  cJSON_AddBoolToObject(capabilities, "hoverProvider", 1);
  cJSON_AddBoolToObject(capabilities, "definitionProvider", 1);
  cJSON_AddBoolToObject(capabilities, "declarationProvider", 1);
  cJSON_AddBoolToObject(capabilities, "documentFormattingProvider", 1);
  cJSON *completion = cJSON_AddObjectToObject(capabilities, "completionProvider");
  cJSON_AddBoolToObject(completion, "resolveProvider", 0);


  cJSON_AddBoolToObject(capabilities, "foldingRangeProvider", 1);

    cJSON *signature = cJSON_AddObjectToObject(capabilities, "signatureHelpProvider");
    cJSON *triggerCharacters = cJSON_AddArrayToObject(signature, "triggerCharacter");
    cJSON_AddItemToArray(triggerCharacters, cJSON_CreateString("("));
    cJSON *retriggerCharacters = cJSON_AddArrayToObject(signature, "retriggerCharacter");
    cJSON_AddItemToArray(retriggerCharacters, cJSON_CreateString(","));

  lsp_send_response(id, result);
}

void lsp_shutdown(int id) {
  lsp_send_response(id, NULL);
}

void lsp_exit(void) {
  exit(0);
}

void lsp_sync_open(const cJSON *params_json) {
  const cJSON *text_document_json = cJSON_GetObjectItem(params_json, "textDocument");
  char *const uri = get_string(text_document_json, "uri");
  const char *text = get_string(text_document_json, "text");

  if(!uri || !text) exit(EXIT_CONTENT_INCOMPLETE);

  BUFFER buffer = open_buffer(uri, text);
  lsp_gwfmt(uri + 7, buffer);
}

void lsp_sync_change(const cJSON *params_json) {
  const cJSON *text_document_json = cJSON_GetObjectItem(params_json, "textDocument");
  char *const uri = get_string(text_document_json, "uri");
  const cJSON *content_changes_json = cJSON_GetObjectItem(params_json, "contentChanges");
  const cJSON *content_change_json = cJSON_GetArrayItem(content_changes_json, 0);
  const char *text = get_string(content_change_json, "text");

  if(!uri || !text) exit(EXIT_CONTENT_INCOMPLETE);

  BUFFER buffer = update_buffer(uri, text);
  lsp_gwfmt(uri + 7, buffer);
}

void lsp_sync_close(const cJSON *params_json) {
  const cJSON *text_document_json = cJSON_GetObjectItem(params_json, "textDocument");
  const char *uri = get_string(text_document_json, "uri");

  if(!uri) exit(EXIT_CONTENT_INCOMPLETE);

  close_buffer(uri);
  lsp_gwfmt_clear(uri);
}

static bool get_parse_uri_files(cJSON *files, char *const uri) {
  for(int i = 0; i < cJSON_GetArraySize(files); i++) {
    cJSON *target = cJSON_GetArrayItem(files, i);
    const char *file = cJSON_GetStringValue(target);
    if(!strcmp(realpath(file, NULL), uri)) return true;
  }
  return false;
}

static char *const get_parse_uri_workspace(cJSON *workspaces, char *const uri) {
  for(int i = 0; i < cJSON_GetArraySize(workspaces); i++) {
    cJSON *workspace = cJSON_GetArrayItem(workspaces, i);
    char *const root = get_string(workspace, "base");
    if(!strcmp(uri , root)) return uri;
    cJSON *files    = cJSON_GetObjectItem(workspace, "files");
    if(get_parse_uri_files(files, uri))
      return uri;
  }
  return uri;
}

static char *const get_parse_uri(char *const uri) {
  for(int i = 0; i < cJSON_GetArraySize(workspaces); i++) {
    cJSON *workspace = cJSON_GetArrayItem(workspaces, i);
    cJSON *roots      = cJSON_GetObjectItem(workspace, "roots");
    if(!roots) exit(32);
    char *root = get_parse_uri_workspace(roots, uri);
    if(root) {
      if(!strcmp(root, uri)) return uri;
      return realpath(root, NULL);
    }
  }
  return uri;
}


static bool find_uri(cJSON *workspaces, char *const request) {
  for(uint32_t i = 0; i < cJSON_GetArraySize(workspaces); i++) {
    cJSON *workspace = cJSON_GetArrayItem(workspaces, i);
    char *const uri = get_string(workspace, "uri");
    if(!strcmp(request, uri)) return true;
  }
  return false;
}
void lsp_gwfmt(char *const request, BUFFER buffer) {
  MP_Vector *infos = new_mp_vector(gwion.mp, DiagnosticInfo, 0);
  char *const uri = get_parse_uri(request);

 gwiond_parse(&infos, uri, buffer.content);

  if(!infos->len) lsp_gwfmt_clear(uri - 7);
  else {
    for(uint32_t i = 0; i < infos->len; i++) {
      DiagnosticInfo *info = mp_vector_at(infos, DiagnosticInfo, i);
//      if(find_uri(workspaces, uri))
        lsp_send_notification("textDocument/publishDiagnostics", info->cjson);
    }
  }
 
  // not a mistake, we are comparing the pointers
  if(uri != request) free(uri); // we could have a threadlocal string[PATH_MAX]

  free_mp_vector(gwion.mp, DiagnosticInfo, infos);
}

void lsp_gwfmt_clear(const char *uri) {
// we should send to all files if *workspace*?
//  char *const uri = get_parse_uri(request);
  cJSON *params = cJSON_CreateObject();
  cJSON_AddStringToObject(params, "uri", uri);
  cJSON_AddArrayToObject(params, "diagnostics");
  lsp_send_notification("textDocument/publishDiagnostics", params);
  // not a mistake, we are comparing the pointers
//  if(uri != request) free(uri); // we could have a threadlocal string[PATH_MAX]
}

void lsp_hover(int id, const cJSON *params_json) {
  DOCUMENT_LOCATION document = lsp_parse_document(params_json);

  BUFFER buffer = get_buffer(document.uri);
  char *text = strdup(buffer.content);
  truncate_string(text, document.line, document.character);

  char *const symbol_name  = extract_last_symbol(text);
  cJSON *result = cJSON_CreateObject();
  cJSON *array = cJSON_AddArrayToObject(result, "contents");
  cJSON *contents = symbol_info(symbol_name, text);
  free(text);

  if(contents)
    cJSON_AddItemToArray(array, contents);
  lsp_send_response(id, result);
}

void lsp_goto_definition(int id, const cJSON *params_json) {
  DOCUMENT_LOCATION document = lsp_parse_document(params_json);

  BUFFER buffer = get_buffer(document.uri);
  char *text = strdup(buffer.content);
  truncate_string(text, document.line, document.character);
  const char *symbol_name  = extract_last_symbol(text);
  cJSON *range = symbol_location(document.uri, symbol_name, text);
  free(text);

  if(!range) {
    lsp_send_response(id, NULL);
    return;
  }
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "uri", document.uri);
  cJSON_AddItemToObject(result, "range", range);
  lsp_send_response(id, result);
}

void lsp_completion(int id, const cJSON *params_json) {
  DOCUMENT_LOCATION document = lsp_parse_document(params_json);

  BUFFER buffer = get_buffer(document.uri);
  char *text = strdup(buffer.content);
  truncate_string(text, document.line, document.character);
  const char *symbol_name_part  = extract_last_symbol(text);
  cJSON *result = symbol_completion(symbol_name_part, text);
  free(text);

  lsp_send_response(id, result);
}
