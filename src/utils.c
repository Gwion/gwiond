#include "gwion_util.h"  // IWYU pragma: export
#include "gwion_ast.h" // IWYU pragma: export
#include "gwion_env.h" // IWYU pragma: export
#include "vm.h"        // IWYU pragma: export
#include "instr.h"     // IWYU pragma: export
#include "emit.h"      // IWYU pragma: export
#include "gwion.h"
#include "pass.h"
#include "gwiond_defs.h"
#include "fold_pass.h"
#include "lsp.h"
#include "gwiond.h"
#include "gwfmt.h"


static cJSON *json_base(cJSON *base, const char *name) {
  return base
    ? cJSON_AddObjectToObject(base, name)
    : cJSON_CreateObject();
}

static cJSON *json_position(cJSON *base, const char *name,
                            const pos_t pos) {
  cJSON *json  = json_base(base, name);
  cJSON_AddNumberToObject(json, "line", pos.line);
  cJSON_AddNumberToObject(json, "character", pos.column);
  return json;
}

cJSON* json_range(cJSON *base, const loc_t loc) {
  cJSON *json = json_base(base, "range");
  json_position(json, "start", loc.first);
  json_position(json, "end", loc.last);
  return json;
}

cJSON* json_location(cJSON *base, const char *uri, const loc_t loc) {
  cJSON *json = json_range(base, loc);
  cJSON_AddStringToObject(base, "uri", uri);
  return json;
}

Buffer get_buffer_from_params(Gwion gwion, const cJSON *params_json) {
  TextDocumentPosition document = lsp_parse_document(params_json);
  return get_buffer(document.uri);
}

Ast get_ast(Gwion gwion, const cJSON *params_json) {
  TextDocumentPosition document = lsp_parse_document(params_json);
  //Buffer *buffer = get_buffer_ptr(document.uri);
  Buffer buffer = get_buffer(document.uri);
  FILE *f = fmemopen(buffer.content, strlen(buffer.content), "r");
  if(!f) return NULL;
  AstGetter getter = {
    .name = (const m_str)document.uri,
    .f    = f, 
    .st   = gwion->st,
    .ppa  = gwion->ppa,
//    .comments = new_commentlist(gwion->mp, 0)
  };
  Ast ast = parse(&getter);
  fclose(f);
//  buffer->comments = getter.comments;
  return ast;
}

ANN bool get_ast_from_getter(AstGetter *const getter, const cJSON *params_json) {
  TextDocumentPosition document = lsp_parse_document(params_json);
  Buffer buffer = get_buffer(document.uri);
  FILE *f = fmemopen(buffer.content, strlen(buffer.content), "r");
  if(!f) return NULL;
  getter->name = document.uri;
  getter->f = f;
  Ast ast = parse(getter);
  fclose(f);
  return !!ast;
}
