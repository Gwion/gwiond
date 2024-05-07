#include <cjson/cJSON.h>
#include "err_codes.h"
#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "vm.h"
#include "gwion.h"
#include "lsp.h"
#include "gwiond.h"
#include "gwiond_defs.h"
#include "mp_vector.h"
#include "hover.h"

#define MAX_HEADER_FIELD_LEN 100

static inline char *get_string(const cJSON* json, const char *key) {
  const cJSON *val = cJSON_GetObjectItem(json, key);
  return cJSON_GetStringValue(val);
}

void lsp_event_loop(const Gwion gwion) {
  for(;;) {
    unsigned long content_length = lsp_parse_header();
    cJSON *request = lsp_parse_content(content_length);
    json_rpc(gwion, request);
    cJSON_Delete(request);
  }
}

unsigned long lsp_parse_header(void) {
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

void lsp_signatureHelp(const Gwion, int id, const cJSON *params_json);
void lsp_foldingRange(int id, const cJSON *params_json);
void json_rpc(const Gwion gwion, const cJSON *request) {
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
  else if(!strcmp(method, "textDocument/completion"))
    lsp_completion(gwion, id, params_json);
  else if(!strcmp(method, "textDocument/formatting"))
    lsp_format(gwion, id, params_json);
  else if(!strcmp(method, "textDocument/signatureHelp"))
    lsp_signatureHelp(gwion, id, params_json);
  else if(!strcmp(method, "textDocument/foldingRange"))
    lsp_foldingRange(id, params_json);
}

// *********************
// LSP helper functions:
// *********************

Document_LOCATION lsp_parse_document(const cJSON *params_json) {
  Document_LOCATION document;

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

Context get_context(void);

void lsp_sync_open(const Gwion gwion, const cJSON *params_json) {
  const cJSON *text_document_json = cJSON_GetObjectItem(params_json, "textDocument");
  char *const uri = get_string(text_document_json, "uri");
  const char *text = get_string(text_document_json, "text");

  if(!uri || !text) exit(EXIT_CONTENT_INCOMPLETE);

  Buffer *buffer = open_buffer(gwion->mp, uri, text);
  lsp_gwfmt(gwion, uri + 7, buffer);
  Context context = get_context();
  buffer->context = context;
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
  lsp_gwfmt(gwion, uri + 7, buffer);
  Context context = get_context();
  buffer->context = context;
}

void lsp_sync_close(const Gwion gwion, const cJSON *params_json) {
  const cJSON *text_document_json = cJSON_GetObjectItem(params_json, "textDocument");
  const char *uri = get_string(text_document_json, "uri");
  if(!uri) {
    message("incomplete content", LSP_ERROR);
    return;
  }
  close_buffer(gwion, uri);
  lsp_gwfmt_clear(uri);
}

static bool get_parse_uri_files(cJSON *files, const char *uri) {
  for(int i = 0; i < cJSON_GetArraySize(files); i++) {
    cJSON *target = cJSON_GetArrayItem(files, i);
    const char *file = cJSON_GetStringValue(target);
    if(!strcmp(realpath(file, NULL), uri)) return true;
  }
  return false;
}

static const char* get_parse_uri_workspace(cJSON *workspaces, const char *uri) {
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

static char* get_parse_uri(char *uri) {
  for(int i = 0; i < cJSON_GetArraySize(workspaces); i++) {
    cJSON *workspace = cJSON_GetArrayItem(workspaces, i);
    cJSON *roots      = cJSON_GetObjectItem(workspace, "roots");
    if(!roots) return uri;
    const char *root = get_parse_uri_workspace(roots, uri);
    if(root) {
      if(!strcmp(root, uri)) return uri;
      return realpath(root, NULL);
    }
  }
  return uri;
}

void lsp_gwfmt(const Gwion gwion, char *const request, Buffer *buffer) {
  MP_Vector *infos = new_mp_vector(gwion->mp, DiagnosticInfo, 0);
  char *uri = get_parse_uri(request);
  gwiond_parse(gwion, &infos, uri, buffer->content);
  if(uri != request) free(uri); // we could have a threadlocal string[PATH_MAX]
  if(!infos->len) lsp_gwfmt_clear(uri - 7);
  else {
    for(uint32_t i = 0; i < infos->len; i++) {
      DiagnosticInfo *info = mp_vector_at(infos, DiagnosticInfo, i);
      lsp_send_notification("textDocument/publishDiagnostics", info->cjson);
    }
  }
  free_mp_vector(gwion->mp, DiagnosticInfo, infos);
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

ANN static Value value_pass(const Gwion gwion, const Context context ,const char *uri, Document_LOCATION document) {
  Hover fv = {
    .filename = { 
      .target = document.uri + 7,
      .source = uri + 7,
    },
    .gwion = gwion,
    .pos = (pos_t) {
      .line = document.line,
      .column = document.character + 1, // WHY?????
    },
  };
  if(hover_ast(&fv, context->tree))
    return fv.value;
  return NULL;
}

ANN static Value get_value(const Gwion gwion, const cJSON *params_json) {
  Document_LOCATION document = lsp_parse_document(params_json);
  char *uri = get_parse_uri(document.uri);
  Buffer buffer = get_buffer(uri);
  const Value value = value_pass(gwion, buffer.context, uri, document);
  if(uri != document.uri) free(uri);
  return value;
} 
ANN static char* value_info(const Gwion gwion, const Value v) {
  char *str = NULL;
  char *name = v->name;
  gw_asprintf(gwion->mp, &str,
"```markdown\n"
"## (%s) `%s`\n"
"```md\n-------------\n"
"```markdown\n### of type: `%s`\n"
"```md\n------------\n"
"```markdown\n### in file: *%s*\n",
// TODO: closures, enum and stuff?
v->type ?              // 
is_class(gwion, v->type) ? "class" :
is_func(gwion, v->type) ? "function" :
"variable" : "unknown",
name, v->type->name, v->from->filename);
  return str;
}

void lsp_hover(const Gwion gwion, int id, const cJSON *params_json) {
  const Value value = get_value(gwion, params_json);
  if(!value) return;
  cJSON *result = cJSON_CreateObject();
  cJSON *array = cJSON_AddArrayToObject(result, "contents");
  char *str = value_info(gwion, value);
  cJSON *contents = cJSON_CreateString(str); 
  free_mstr(gwion->mp, str);
  cJSON_AddItemToArray(array, contents);
  lsp_send_response(id, result);
}

/*static*/ cJSON* json_range(cJSON *base, const loc_t loc);

void lsp_goto_definition(const Gwion gwion, int id, const cJSON *params_json) {
  const Value value = get_value(gwion, params_json);
  if(!value) return;
  // do we need to rebuild uri here?
  // for value's filename
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "uri", value->from->filename);
  json_range(result, value->from->loc); 
  lsp_send_response(id, result);
}

void lsp_completion(const Gwion gwion, int id, const cJSON *params_json) {
  Document_LOCATION document = lsp_parse_document(params_json);

  Buffer buffer = get_buffer(document.uri);
  char *text = strdup(buffer.content);
  truncate_string(text, document.line, document.character);
  const char *symbol_name_part  = extract_last_symbol(text);
  cJSON *result = symbol_completion(gwion, symbol_name_part, text);
  free(text);

  lsp_send_response(id, result);
}
