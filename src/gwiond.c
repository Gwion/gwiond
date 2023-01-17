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
#include "symtab.h"
#include "gwiond.h"
#include "gwfmt.h"
#include "lsp.h"


static struct Gwion_ gwion = {};


char char_buffer[CHAR_BUFFER_LENGTH];
int severity = ERROR;

cJSON *_diagnostics = NULL;

cJSON* symbol_info(const char *symbol_name, const char *text) {
  //parse(NULL, text);
  char *display = lookup_symbol_type(symbol_name, VAR|PAR|FUN);
  if(!display) return NULL;
  cJSON *info = cJSON_CreateString(display);
  return info;
}

cJSON* symbol_location(const char *filename, const char *symbol_name, char *text) {
  gwiond_parse(NULL, filename, text);

  int idx = lookup_symbol(symbol_name, VAR|PAR|FUN);
  if(idx == -1) {
    return NULL;
  }

  loc_t sym_range = get_range(idx);
  cJSON *range = 

  cJSON_CreateObject();
  // TODO use range here too
  cJSON *start_position = cJSON_AddObjectToObject(range, "start");
  cJSON_AddNumberToObject(start_position, "line", sym_range.first.line - 1);
  cJSON_AddNumberToObject(start_position, "character", sym_range.first.column - 1);
  cJSON *end_position = cJSON_AddObjectToObject(range, "end");
  cJSON_AddNumberToObject(end_position, "line", sym_range.last.line -  1);
  cJSON_AddNumberToObject(end_position, "character", sym_range.last.column - 1);
  return range;
}

cJSON* symbol_completion(const char *symbol_name_part, const char *text) {
//  gwiond_parse(NULL, text);
  int indices[SYMBOL_TABLE_LENGTH];
  int indices_num = lookup_starts_with(indices, symbol_name_part);

  cJSON *results = cJSON_CreateArray();
  for(int i = 0; i < indices_num; i++) {
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "label", get_name(indices[i]));
    char *detail = get_display(indices[i]);
    cJSON_AddStringToObject(item, "detail", detail);
    free(detail);
    cJSON_AddItemToArray(results, item);
  }
  return results;
}

static cJSON *json_position(int line, int character) {
  cJSON *pos  = cJSON_CreateObject();
  cJSON *_line  = cJSON_CreateNumber(line);
  cJSON_AddItemToObject(pos, "line", _line);
  cJSON *_character  = cJSON_CreateNumber(character);
  cJSON_AddItemToObject(pos, "character", _character);
  return pos;
}

static cJSON* json_range(int line1, int character1, 
                    int line2, int character2) {
  cJSON *range = cJSON_CreateObject();
  cJSON *start = json_position(line1, character1);
  cJSON *end   = json_position(line2, character2);
  cJSON_AddItemToObject(range, "start", start);
  cJSON_AddItemToObject(range, "end", end);
  return range;
}
    
static void gwiond_error_basic(const char *main, const char *secondary, const char *fix,
  const char *filename, const loc_t loc,  const uint errno,  const enum libprettyerr_errtype error) {
  if(!_diagnostics) return;
  cJSON *diagnostic = cJSON_CreateObject();
  cJSON *range = json_range(loc.first.line - 1, loc.first.column -1,
                            loc.last.line - 1, loc.last.column - 1);
  cJSON_AddItemToObject(diagnostic, "range", range);
  cJSON_AddNumberToObject(diagnostic, "severity", severity);
  cJSON_AddStringToObject(diagnostic, "message", main);
  cJSON_AddItemToArray(_diagnostics, diagnostic);
  severity = ERROR;
}

static void gwiond_error_secondary(const char *main, const char *filename,
                         const loc_t loc) {}
static char*gw_args[1] = {
  "-gcheck"
};

ANN void gwiond_add_value(const Nspc n, const Symbol s, const Value a) {
  _nspc_add_value(n, s, a);
  gwiond_insert_symbol(strdup(s_name(s)), a->type->name, REG, NO_TYPE, NO_ATR, NO_ATR, a->from->loc);
}
ANN void gwiond_add_value_front(const Nspc n, const Symbol s, const Value a) {
  _nspc_add_value_front(n, s, a);
  gwiond_insert_symbol(strdup(s_name(s)), a->type->name, REG, NO_TYPE, NO_ATR, NO_ATR, a->from->loc);
}

void gwiond_parse(cJSON *diagnostics, const char *filename, char *text) {
  _diagnostics = diagnostics;
  compile_string(&gwion, (char*)filename, text);
  _diagnostics = NULL;
}

static GwText text;
static int gwiond_lint(const char *c, ...) {
  char tmp[strlen(c) + 1];
  if(tcol_snprintf(tmp, strlen(c), c) == TermColorErrorNone && strlen(tmp))
    text_add(&text, tmp);
  else
    text_add(&text, c);
  return 0;
}

void lsp_format(int id, const cJSON *params_json) {
  text.mp=gwion.mp;
  struct PPArg_ ppa = {.lint = 1};
  pparg_ini(gwion.mp, &ppa);
  struct LintState ls = { .color = false, .pretty=false, .show_line = false, .header = false, .nindent = 2};
  tcol_override_color_checks(false);
  Lint l = {.mp = gwion.mp, .st = gwion.st, .ls = &ls, .line = 0 };

  DOCUMENT_LOCATION document = lsp_parse_document(params_json);
  BUFFER buffer = get_buffer(document.uri);
  FILE *f = fmemopen(buffer.content, strlen(buffer.content), "r");
  struct AstGetter_ arg = { (const m_str)document.uri, f, gwion.st, .ppa = &ppa};
  Ast ast = parse(&arg);
  if (!ast) { return;} // would better send an error
  lint_ast(&l, ast);
  fclose(f);
  cJSON *result = cJSON_CreateArray();

  cJSON *range = json_range(0, 0, l.line, l.column - 1);
  cJSON *textedit = cJSON_CreateObject();

  cJSON_AddItemToObject(textedit, "range", range);

  cJSON_AddStringToObject(textedit, "newText", text.str);
  cJSON_AddItemToArray(result, textedit);

  lsp_send_response(id, result);
  text_reset(&text);
}

void gwiond_ini(void) {
  CliArg arg = {.arg = {.argc = 1, .argv = gw_args}, .loop = false};
  const m_bool ini = gwion_ini(&gwion, &arg);
  gwerr_set_func(gwiond_error_basic, gwiond_error_secondary);
  lint_set_func(gwiond_lint);
  nspc_add_value_set_func(gwiond_add_value, gwiond_add_value_front);
  arg_release(&arg);
}

void gwiond_end(void) {
  gwion_end(&gwion);
}
