#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "termcolor.h"
#include "vm.h"
#include "instr.h"
#include "emit.h"
#include "compile.h"
#include "gwion.h"
#include "pass.h"
#include "gwiond_defs.h"
#include "gwiond.h"
#include "gwiond_pass.h"
#include "gwfmt.h"
#include "lsp.h"
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

static cJSON* json_range(cJSON *base, const loc_t loc) {
  cJSON *json = json_base(base, "range");
  json_position(json, "start", loc.first);
  json_position(json, "end", loc.last);
  return json;
}

cJSON* symbol_info(char *const symbol_name, char *const text) {
  const Vector vec = trie_get((m_str)symbol_name);
  const Value v = vec ? (Value)vector_front(vec) : nspc_lookup_value1(gwion.env->curr, insert_symbol(gwion.st, symbol_name));
  if(!v) {


    return NULL;
  }
  char *str = NULL;
//  char *name = isa(v->type, gwion.type[et_function]) ? s_name(v->type->info->func->def->base->tag.sym) : v->name;
  char *name = v->name;
  gw_asprintf(gwion.mp, &str,
"```markdown\n"
"## (%s) `%s`\n"
"```md\n-------------\n"
"```markdown\n### of type: `%s`\n"
"```md\n------------\n"
"```markdown\n### in file: *%s*\n",
isa(v->type, gwion.type[et_class]) ? "class" :
isa(v->type, gwion.type[et_function]) ? "function" :
"variable",
name, v->type->name, v->from->filename);
  cJSON *result = cJSON_CreateString(str);
  free_mstr(gwion.mp, str);
  return result;
}

cJSON* symbol_location(const char *filename, const char *symbol_name, char *text) {
  //gwiond_parse(NULL, filename, text);
  Vector vec = trie_get((m_str)symbol_name);
  if(vec) {
    const Value v = (Value)vector_at(vec, 0);
    if(v) return json_range(NULL, v->from->loc);
  }
  return NULL;
}

cJSON* symbol_completion(const char *symbol_name_part, const char *text) {
  cJSON *results = cJSON_CreateArray();
  Vector v = trie_starts_with((char*)symbol_name_part);
  if(v) {
    for(m_uint i = 0; i < vector_size(v); i++) {
      cJSON *item = cJSON_CreateObject();
      const Value val = (Value)vector_at(v, i);
      char *s = strchr(val->name, '@');
      char *ret = strndup(val->name, s-val->name);
      cJSON_AddStringToObject(item, "label", ret);
      cJSON_AddStringToObject(item, "detail", val->name);
      free(ret);
      cJSON_AddItemToArray(results, item);
    }
    free_vector(gwion.mp, v);
  }
  return results;
}

static MP_Vector **_diagnostics = NULL;

static cJSON *get_diagnostics(const char *filename) {
  for(uint32_t i = 0; i < (*_diagnostics)->len; i++) {
    DiagnosticInfo *info = mp_vector_at((*_diagnostics), DiagnosticInfo, i);
    if(!strcmp(filename, info->filename))
      return cJSON_GetObjectItem(info->cjson, "diagnostics");
  }
  DiagnosticInfo info = { .filename = filename, .cjson = cJSON_CreateObject() };
  mp_vector_add(gwion.mp, _diagnostics, DiagnosticInfo, info);
  char *uri = NULL;
  gw_asprintf(gwion.mp, &uri, "file://%s", filename);
  cJSON_AddStringToObject(info.cjson, "uri", uri);
  free_mstr(gwion.mp, uri);
  return cJSON_AddArrayToObject(info.cjson, "diagnostics");
}

static void gwiond_error_basic(const char *main, const char *secondary, const char *fix,
  const char *filename, const loc_t loc,  const uint errno,  const enum libprettyerr_errtype error) {
  // TODO: a function that checks extensions?
  if(!strcmp(filename + strlen(filename) - 2, ".c")) return;
  if(/*!error ||*/ !_diagnostics) return;
  cJSON *diagnostic = cJSON_CreateObject();
  cJSON *range = json_range(diagnostic, loc);
  char msg[256];
  //tcol_snprintf(msg, 256, "%s %s", main, secondary ?: "");
  tcol_snprintf(msg, 256, main);
  cJSON_AddNumberToObject(diagnostic, "severity", ERROR);
  cJSON_AddStringToObject(diagnostic, "message", msg);
  cJSON *diagnostics = get_diagnostics(filename);
  cJSON_AddItemToArray(diagnostics, diagnostic);
}

static void gwiond_error_secondary(const char *main, const char *filename,
                         const loc_t loc) {
  if(!_diagnostics) return;
  // TODO: a function that checks extensions?
  if(!strcmp(filename + strlen(filename) - 2, ".c")) return;
  cJSON *diagnostic = cJSON_CreateObject();
  cJSON *range = json_range(diagnostic, loc);
  char msg[256];
  //tcol_snprintf(msg, 256, "%s %s", main, secondary ?: "");
  tcol_snprintf(msg, 256, main);
  cJSON_AddNumberToObject(diagnostic, "severity", WARNING);
  cJSON_AddStringToObject(diagnostic, "message", msg);
  cJSON *diagnostics = get_diagnostics(filename);
  cJSON_AddItemToArray(diagnostics, diagnostic);

}

static char*gw_args[2] = {
  "-gsema,check", "--color=never"
};

ANN void gwiond_add_value(const Nspc n, const Symbol s, const Value a) {
//if(!s)return;
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

void gwiond_parse(MP_Vector **diagnostics, const char *filename, char *text) {
  _diagnostics = diagnostics;
  trie_clear();
  compile_string(&gwion, (char*)filename, text);
  _diagnostics = NULL;
}
ANN void get_values(const Vector ret, Vector vec);

void lsp_signatureHelp(int id, const cJSON *params_json) {
  DOCUMENT_LOCATION document = lsp_parse_document(params_json);
  BUFFER buffer = get_buffer(document.uri);
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
  tcol_override_color_checks(false);
  Gwfmt l = {.mp = gwion.mp, .st = gwion.st, .ls = &ls, .last = cht_nl };
  text_init(&ls.text, gwion.mp);
      
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
  DOCUMENT_LOCATION document = lsp_parse_document(params_json);
  BUFFER buffer = get_buffer(document.uri);
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

	
  // gwiondinfo_end();
}

void lsp_format(int id, const cJSON *params_json) {
  struct PPArg_ ppa = {.fmt = 1};
  pparg_ini(gwion.mp, &ppa);
  const cJSON *options = cJSON_GetObjectItem(params_json, "params");
  const cJSON *tabsize_json = cJSON_GetObjectItem(options, "tabSize");
  const int tabsize = cJSON_GetNumberValue(tabsize_json);
  const cJSON *insert_spaces_json = cJSON_GetObjectItem(options, "insertSpaces");
  const int usetabs = cJSON_IsFalse(tabsize_json);
  struct GwfmtState ls = {
    .color = false, .pretty=false, .show_line = false, .header = false, .nindent = tabsize, .base_column=0,
    .use_tabs = usetabs
  };
  tcol_override_color_checks(false);
  Gwfmt l = {.mp = gwion.mp, .st = gwion.st, .ls = &ls, .line = 0, .column = 0 };
  text_init(&ls.text, gwion.mp);

  DOCUMENT_LOCATION document = lsp_parse_document(params_json);
  BUFFER buffer = get_buffer(document.uri);
  FILE *f = fmemopen(buffer.content, strlen(buffer.content), "r");
  struct AstGetter_ arg = { (const m_str)document.uri, f, gwion.st, .ppa = &ppa};
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

void gwiond_ini(void) {
  CliArg arg = {.arg = {.argc = 1, .argv = gw_args}, .loop = false };
  const bool ini = gwion_ini(&gwion, &arg);
  io_ini(gwion.mp);
  trie_ini(gwion.mp);
  gwerr_set_func(gwiond_error_basic, gwiond_error_secondary);
  gwion_parser_set_default_pos((pos_t) { 0, 1 });
  nspc_add_value_set_func(gwiond_add_value, gwiond_add_value_front);
  arg_release(&arg);
}

void gwiond_end(void) {
  gwion_end(&gwion);
}
