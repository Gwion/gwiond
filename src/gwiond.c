#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "vm.h"
#include "instr.h"
#include "emit.h"
#include "compile.h"
#include "gwion.h"
#include "pass.h"
#include "gwiond_defs.h"
#include "gwiond.h"
#include "gwfmt.h"
#include "lsp.h"
#include "trie.h"
#include "io.h"

static struct Gwion_ gwion = {};


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

static cJSON* json_range(cJSON *base, const loc_t loc) {
  cJSON *json = json_base(base, "range");
  json_position(json, "start", loc.first);
  json_position(json, "end", loc.last);
  return json;
}
    
cJSON* symbol_info(const char *symbol_name, char *const text) {
  //parse(NULL, text);
  const Vector vec = trie_get((m_str)symbol_name);
  if(!vec) return NULL;
  const Value v = (Value)vector_front(vec);
  if(!v) return NULL;
  return cJSON_CreateString(v->type->name);
}

cJSON* symbol_location(const char *filename, const char *symbol_name, char *text) {
  gwiond_parse(NULL, filename, text);
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

static cJSON *_diagnostics = NULL;
static void gwiond_error_basic(const char *main, const char *secondary, const char *fix,
  const char *filename, const loc_t loc,  const uint errno,  const enum libprettyerr_errtype error) {
  if(!error || !_diagnostics) return;
  cJSON *diagnostic = cJSON_CreateObject();
  cJSON *range = json_range(diagnostic, loc);
  char msg[256];
  snprintf(msg, 256, "%s %s", main, secondary ?: "");
  cJSON_AddNumberToObject(diagnostic, "severity", error);
  cJSON_AddStringToObject(diagnostic, "message", msg);
  cJSON_AddItemToArray(_diagnostics, diagnostic);
}

static void gwiond_error_secondary(const char *main, const char *filename,
                         const loc_t loc) {}
static char*gw_args[1] = {
  "-gcheck"
};

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

void gwiond_parse(cJSON *diagnostics, const char *filename, char *text) {
  _diagnostics = diagnostics;
  trie_clear();
  compile_string(&gwion, (char*)filename, text);
  _diagnostics = NULL;
}

void lsp_signature(int id, const cJSON *params_json) {
  DOCUMENT_LOCATION document = lsp_parse_document(params_json);

  BUFFER buffer = get_buffer(document.uri);
  char *text = strdup(buffer.content);
  truncate_string(text, document.line, document.character);
  const char *symbol_name  = extract_last_symbol(text);
  char *str = strndup(symbol_name, strlen(symbol_name) - 1); //strchr(symbol_name, '(');
  //if(*str) return;
   // what if no result...
  //char *name = strndup(symbol_name, str-symbol_name);
  //gwiond_parse(NULL, "kjhkj", text);

  cJSON *result = cJSON_CreateObject();
  cJSON *signatures = cJSON_AddArrayToObject(result, "signatures");
  /*
  for(int i = 0; i <= 12 ; i++) {
    const Value v = get_value(i,  symbol_name);
    fprintf(stderr, "get func %i %s %p\n", i, symbol_name, v);
    
    if(v) {
      if(is_func(&gwion, v->type)) {
        fprintf(stderr, "got func %s\n", text);
        //
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "label", symbol_name);
        // "documentation"
        // [parameters]
        cJSON_AddItemToArray(result, obj);
      } else if(isa(v->type, gwion.type[et_closure]) > 0) {
//
      }
    }
  }
*/
  Vector vec = trie_get(str);
  if(vec) {
  for(m_uint i = 0; i < vector_size(vec); i++) {
    Value v = (Value)vector_at(vec, i);
    if(v) {
        cJSON *obj = cJSON_CreateObject();
  
        const Func f = v->d.func_ref;
        
  struct GwfmtState ls = {
    .color = false, .pretty=false, .show_line = false, .header = false,
        //.nindent = tabsize,
        .base_column=0,
    //.use_tabs = usetabs
  };
  tcol_override_color_checks(false);
  Gwfmt l = {.mp = gwion.mp, .st = gwion.st, .ls = &ls, .line = 0, .column = 0, .last = cht_nl };
  text_init(&ls.text, gwion.mp);
text_add(&ls.text, str);
      
        //cJSON_AddStringToObject(obj, "label", v->name);
        Stmt_List code = f->def->d.code;
        f->def->d.code = NULL;
        Arg_List args = f->def->base->args;
text_add(&ls.text, "(");
        if(args) {
          //gwfmt_arg_list(&l, args);
        for(uint32_t i = 0; i < mp_vector_len(args); i++) {
          Arg *arg = mp_vector_at(args, struct Arg_, i);
// TODO: full type path
          text_add(&ls.text, s_name(arg->td->xid));
text_add(&ls.text, " ");
text_add(&ls.text, s_name(arg->var_decl.xid));
if(i < mp_vector_len(args) - 1)
text_add(&ls.text, ",");

        }

        }
text_add(&ls.text, ")");
        f->def->d.code = code;


        cJSON_AddStringToObject(obj, "label", ls.text.str);
text_release(&ls.text);

        // "documentation"
        // [parameters]
  // get is func optr closure
        if(args)  {
  cJSON *arguments = cJSON_AddArrayToObject(obj, "parameters");
        for(uint32_t i = 0; i < mp_vector_len(args); i++) {
          Arg *arg = mp_vector_at(args, struct Arg_, i);
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "label", s_name(arg->var_decl.xid));
          cJSON_AddItemToArray(arguments, obj);
        }
        }
          cJSON_AddItemToArray(signatures, obj);
    }
  }
  }
  //cJSON_AddStringToObject(result, "uri", document.uri);
  //cJSON_AddItemToObject(result, "range", range);
  

  //free(text);


  lsp_send_response(id, result);
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
  pos_t pos = { 0, 0 };
  Ast ast = parse_pos(&arg, pos); // is that needed?
  if (!ast) { return;} // would better send an error
  gwfmt_ast(&l, ast);
  fclose(f);
  cJSON *json = cJSON_CreateArray();
  cJSON *textedit = cJSON_CreateObject();
  const loc_t loc = {
    pos,
    { l.line, l.column }
  };
  json_range(textedit, loc);
  cJSON_AddStringToObject(textedit, "newText", ls.text.str);
  cJSON_AddItemToArray(json, textedit);
  lsp_send_response(id, json);
  text_release(&ls.text);
}

void gwiond_ini(void) {
  CliArg arg = {.arg = {.argc = 1, .argv = gw_args}, .loop = false};
  const m_bool ini = gwion_ini(&gwion, &arg);
  io_ini(gwion.mp);
  trie_ini(gwion.mp);
  gwerr_set_func(gwiond_error_basic, gwiond_error_secondary);
  gwion_set_default_pos((pos_t) { 0 , 0 });
  nspc_add_value_set_func(gwiond_add_value, gwiond_add_value_front);
  arg_release(&arg);
}

void gwiond_end(void) {
  gwion_end(&gwion);
}
