#include "gwion_util.h"
#include "gwion_ast.h" // IWYU pragma: export
#include "gwion_env.h" // IWYU pragma: export
#include "vm.h"        // IWYU pragma: export
#include "instr.h"     // IWYU pragma: export
#include "emit.h"      // IWYU pragma: export
#include "compile.h"
#include "gwion.h"
#include "pass.h"
#include "gwiond_defs.h"
#include "gwiond_pass.h"
#include "gwfmt.h"
#include "lsp.h"
#include "gwiond.h"
#include "trie.h"
#include "io.h"
#include <cjson/cJSON.h>

struct Gwion_ gwion = {};

static cJSON *json_base(cJSON *base, const char *name) {
  return base
    ? cJSON_AddObjectToObject(base, name)
    : cJSON_CreateObject();
}

static cJSON *json_position(cJSON *base, const char *name,
                            const pos_t pos) {
  cJSON *json  = json_base(base, name);
  cJSON_AddNumberToObject(json, "line", pos.line);
  cJSON_AddNumberToObject(json, "character", pos.column - 1);
  return json;
}

/*static*/ cJSON* json_range(cJSON *base, const loc_t loc) {
  cJSON *json = json_base(base, "range");
  json_position(json, "start", loc.first);
  json_position(json, "end", loc.last);
  return json;
}

cJSON* symbol_completion(const Gwion gwion, const char *symbol_name_part, const char *text) {
  cJSON *results = cJSON_CreateArray();
  Vector v = trie_starts_with((char*)symbol_name_part);
  if(v) {
    for(m_uint i = 0; i < vector_size(v); i++) {
      cJSON *item = cJSON_CreateObject();
      const Value val = (Value)vector_at(v, i);
      char *s = strchr(val->name, '@');
      char *ret = strndup(val->name, s-val->name);
      message(val->name, 3);
      cJSON_AddStringToObject(item, "label", ret);
      cJSON_AddStringToObject(item, "detail", val->name);
      cJSON_AddNumberToObject(item, "kind", 6);
      free(ret);
      cJSON_AddItemToArray(results, item);
    }
    free_vector(gwion->mp, v);
  }
  return results;
}

static MP_Vector **_diagnostics = NULL;

static cJSON *get_diagnostics(const Gwion gwion, const char *filename) {
  for(uint32_t i = 0; i < (*_diagnostics)->len; i++) {
    DiagnosticInfo *info = mp_vector_at((*_diagnostics), DiagnosticInfo, i);
    if(!strcmp(filename, info->filename))
      return cJSON_GetObjectItem(info->cjson, "diagnostics");
  }
  DiagnosticInfo info = { .filename = filename, .cjson = cJSON_CreateObject() };
  mp_vector_add(gwion->mp, _diagnostics, DiagnosticInfo, info);
  char *uri = NULL;
  gw_asprintf(gwion->mp, &uri, "file://%s", filename);
  cJSON_AddStringToObject(info.cjson, "uri", uri);
  free_mstr(gwion->mp, uri);
  return cJSON_AddArrayToObject(info.cjson, "diagnostics");
}

static cJSON* mk_diagnostic(const char* msg, const loc_t loc, const int severity) {
  cJSON *diagnostic = cJSON_CreateObject();
  cJSON *range = json_range(diagnostic, loc);
  cJSON_AddItemToObject(diagnostic, "range", range);
  cJSON_AddNumberToObject(diagnostic, "severity", severity);
  cJSON_AddStringToObject(diagnostic, "message", msg);
  return diagnostic;
}

static void gwiond_error(const char *main, const char *secondary,
  const char *filename, const loc_t loc,  const uint errno,  const enum libprettyerr_errtype error_code) {
  if(!_diagnostics) return;
  char msg[256];
  // TODO: concat messages?
  // or make two of them 
  tcol_snprintf(msg, 256, "%s.%s", main, secondary ?: "");
  //tcol_snprintf(msg, 256, main);
  cJSON *diagnostic = mk_diagnostic(msg, loc, LSP_ERROR);
  if(error_code) {
    cJSON_AddNumberToObject(diagnostic, "code", error_code);
    // we could add code uri here
  }
  cJSON *diagnostics = get_diagnostics(&gwion, filename);
  cJSON_AddItemToArray(diagnostics, diagnostic);
}

static void gwiond_warning(const char *main, const char *filename,
                         const loc_t loc) {
  if(!_diagnostics) return;
  char msg[256];
  tcol_snprintf(msg, 256, main);
  cJSON *diagnostic = mk_diagnostic(msg, loc, LSP_WARNING);
  cJSON_AddStringToObject(diagnostic, "message", msg);
  cJSON *diagnostics = get_diagnostics(&gwion, filename);
  cJSON_AddItemToArray(diagnostics, diagnostic);
}

static cJSON* get_relateds(cJSON *base) {
  cJSON *relateds = cJSON_GetObjectItem(base, "relatedInformation");
  if(relateds) return relateds;
  return cJSON_AddArrayToObject(base, "relatedInformation");
}

static void gwiond_related(const char *main, const char *filename,
                         const loc_t loc) {
  if(!_diagnostics) return;
  cJSON *related = cJSON_CreateObject();
  cJSON *location = cJSON_CreateObject();
  json_range(location, loc);
  cJSON_AddStringToObject(location, "uri", filename);
  cJSON_AddItemToObject(related, "location", location);
  char msg[256];
  tcol_snprintf(msg, 256, main);
  cJSON_AddStringToObject(related, "message", msg);
  cJSON *diagnostics = get_diagnostics(&gwion, filename);
  const int sz = cJSON_GetArraySize(diagnostics);
  if(!sz) return; // calls a C file
  cJSON *base = cJSON_GetArrayItem(diagnostics, sz - 1);
  cJSON *relateds = get_relateds(base);
  cJSON_AddItemToArray(relateds, related);
}

static void gwiond_hint(const char *main, const char *filename, const loc_t loc) {
  if(!_diagnostics) return;
  char msg[256];
  tcol_snprintf(msg, 256, main);
  cJSON *diagnostic = mk_diagnostic(msg, loc, LSP_HINT);
  cJSON *diagnostics = get_diagnostics(&gwion, filename);
  cJSON_AddItemToArray(diagnostics, diagnostic);
}


ANN void gwiond_add_value(const Nspc n, const Symbol s, const Value a) {
  _nspc_add_value(n, s, a);
  a->from->loc.first.column--;
  a->from->loc.last.column--;
  char c[256];
  strcpy(c, s_name(s));
  char *tmp = strchr(c, '@');
  if(tmp) {
    c[tmp -c] = 0;
  }
  //trie_add(s_name(s), a);
  trie_add(c, a);
}
ANN void gwiond_add_value_front(const Nspc n, const Symbol s, const Value a) {
  _nspc_add_value_front(n, s, a);
  a->from->loc.first.column--;
  a->from->loc.last.column--;
  char c[256];
  strcpy(c, s_name(s));
  char *tmp = strchr(c, '@');
  if(tmp) {
    c[tmp -c] = 0;
  }
  trie_add(c, a);
}

void gwiond_parse(const Gwion gwion, MP_Vector **diagnostics, const char *filename, char *text) {
  _diagnostics = diagnostics;
  trie_clear();
  compile_string(gwion, (char*)filename, text);
  const Vector known_ctx = &gwion->env->scope->known_ctx;
  if(vector_size(known_ctx)) {
    Context ctx = (Context)vector_pop(known_ctx);
    ctx->ref--;
    if(ctx->global) {
      gwion->env->global_nspc = ctx->nspc->parent->parent;
    }
  }
  _diagnostics = NULL;
}

void lsp_signatureHelp(const Gwion gwion, int id, const cJSON *params_json) {
  Document_LOCATION document = lsp_parse_document(params_json);
  Buffer buffer = get_buffer(document.uri);
  char *text = strdup(buffer.content);
  truncate_string(text, document.line, document.character);
  char *const symbol_name  = extract_last_symbol(text);
//  char *str = symbol_name; //strndup(symbol_name, strlen(symbol_name) - 1); //strchr(symbol_name, '(');
   // what if no result...
  cJSON *result = cJSON_CreateObject();
  cJSON *signatures = cJSON_AddArrayToObject(result, "signatures");

  Vector vec = trie_get(symbol_name);
  if(vec) {
  for(m_uint i = 0; i < vector_size(vec); i++) {
    Value v = (Value)vector_at(vec, i);
    if(v) {
        cJSON *signature = cJSON_CreateObject();
        const Func f = v->d.func_ref;
        struct GwfmtState ls = {};
        gwfmt_state_init(&ls);
//  tcol_override_color_checks(false);
    Gwfmt l = {.mp = gwion->mp, .st = gwion->st, .ls = &ls, .last = cht_nl };
    l.filename = buffer.uri + 7;
    text_init(&ls.text, gwion->mp);
      
        Stmt_List code = f->def->d.code;
        f->def->d.code = NULL;


        Arg_List args = f->def->base->args;
        // remove ;
        gwfmt_func_def(&l, f->def);
        f->def->d.code = code;
        cJSON_AddStringToObject(signature, "label", ls.text.str);
    if(args)  {
      ls.text.len = 0;
      cJSON *arguments = cJSON_AddArrayToObject(signature, "parameters");
      for(uint32_t i = 0; i < mp_vector_len(args); i++) {
        Arg *arg = mp_vector_at(args, struct Arg_, i);
        gwfmt_variable(&l, &arg->var);
        cJSON *argument = cJSON_CreateObject();
        cJSON_AddStringToObject(argument, "label", ls.text.str);
        cJSON_AddItemToArray(arguments, argument);
      }
    } 
        cJSON_AddItemToArray(signatures, signature);
        text_release(&ls.text);
      }
    }
  }
  lsp_send_response(id, result);
}

void lsp_foldingRange(int id, const cJSON *params_json) {
  Document_LOCATION document = lsp_parse_document(params_json);
  Buffer buffer = get_buffer(document.uri);
  FILE *f = fmemopen(buffer.content, strlen(buffer.content), "r");
  struct PPArg_ ppa = {.fmt = 1};
  pparg_ini(gwion.mp, &ppa);
  struct AstGetter_ arg = { (const m_str)document.uri, f, gwion.st, .ppa = &ppa};
  Ast ast = parse(&arg); // is that needed?
  if (!ast) { return;} // would better send an error
  GwiondInfo gi;
  gwiondinfo_ini(gwion.mp, &gi);
  gwiond_ast(&gi, ast);
  fclose(f);

// we doing regions only for now
	// next we'll need to add kinds to the vector
// free the ast?
	// or store it in buffer? 
// send the array
  cJSON *json = cJSON_CreateArray();
  for(uint32_t i = 0; i < gi.foldranges->len; i++) {
    FoldRange fr = *mp_vector_at(gi.foldranges, FoldRange, i);
    cJSON *foldrange = cJSON_CreateObject();
    cJSON_AddNumberToObject(foldrange, "startLine", fr.loc.first.line);
    cJSON_AddNumberToObject(foldrange, "startCharacter", fr.loc.first.column);
    cJSON_AddNumberToObject(foldrange, "endLine", fr.loc.last.line);
    cJSON_AddNumberToObject(foldrange, "endCharacter", fr.loc.last.column);
    cJSON_AddStringToObject(foldrange, "kind", "region");
    cJSON_AddItemToArray(json, foldrange);
  }

  lsp_send_response(id, json);

	
  gwiondinfo_end(gwion.mp, &gi);
}

void lsp_format(const Gwion gwion, int id, const cJSON *params_json) {
  struct PPArg_ ppa = {.fmt = 1};
  pparg_ini(gwion->mp, &ppa);
  const cJSON *options = cJSON_GetObjectItem(params_json, "params");
  const cJSON *tabsize_json = cJSON_GetObjectItem(options, "tabSize");
  const int tabsize = cJSON_GetNumberValue(tabsize_json);
  const cJSON *insert_spaces_json = cJSON_GetObjectItem(options, "insertSpaces");
  const int usetabs = cJSON_IsFalse(tabsize_json);
  struct GwfmtState ls = {
    .color = false, .pretty=false, .show_line = false, .header = false, .nindent = tabsize, .base_column=0,
    .use_tabs = usetabs
  };
  gwfmt_state_init(&ls);
  //tcol_override_color_checks(false);
  Gwfmt l = {.mp = gwion->mp, .st = gwion->st, .ls = &ls, .line = 0, .column = 0 };
  text_init(&ls.text, gwion->mp);

  Document_LOCATION document = lsp_parse_document(params_json);
  Buffer buffer = get_buffer(document.uri);
  l.filename = buffer.uri + 7;
  FILE *f = fmemopen(buffer.content, strlen(buffer.content), "r");
  struct AstGetter_ arg = { (const m_str)document.uri, f, gwion->st, .ppa = &ppa};
  Ast ast = parse(&arg); // is that needed?
  if (!ast) { return;} // would better send an error
  gwfmt_ast(&l, ast);
  fclose(f);
  cJSON *json = cJSON_CreateArray();
  cJSON *textedit = cJSON_CreateObject();
  const loc_t loc = {
    .first = (pos_t){ .line = 0, .column = 1},
    .last = { l.line, l.column }
  };
  json_range(textedit, loc);
  cJSON_AddStringToObject(textedit, "newText", ls.text.str);
  cJSON_AddItemToArray(json, textedit);
  lsp_send_response(id, json);
  text_release(&ls.text);
}

static Context g_context;// thread_local?
Context get_context(void) {
  Context ctx = g_context;
  g_context = NULL;
  return ctx;
}
ANN static bool keep_context(const Env env, Ast *ast) {
  env->context->ref++;
  g_context = env->context;
  return true;
}

static char*gw_args[2] = {
  "gwiond"
  "--color=never"
};

ANN static void gwiond_passes(const Gwion gwion) {
  pass_register(gwion, "keep_context", keep_context);
  struct Vector_ passes;
  vector_init(&passes);
  vector_add(&passes, (m_uint)"keep_context");
  vector_add(&passes, (m_uint)"sema");
  vector_add(&passes, (m_uint)"check");
  pass_set(gwion, &passes);
  vector_release(&passes);
}

// should probably take gwion as arg instead
Gwion gwiond_ini(void) {
  CliArg arg = {.arg = {.argc = 2, .argv = gw_args} };
  const bool ini = gwion_ini(&gwion, &arg);
  arg_release(&arg);
  gwiond_passes(&gwion);
//  io_ini(gwion.mp);
  trie_ini(gwion.mp);
  gwlog_set_func(gwiond_error, gwiond_warning,
                 gwiond_related, gwiond_hint);
  gwion_parser_set_default_pos((pos_t) { 0, 1 });
  nspc_add_value_set_func(gwiond_add_value, gwiond_add_value_front);
  return &gwion;
}

void gwiond_end(void) {
  gwion_end(&gwion);
}
