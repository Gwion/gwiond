#include "gwion_util.h" // IWYU pragma: export
#include "gwion_ast.h"  // IWYU pragma: export
#include "gwion_env.h"  // IWYU pragma: export
#include "pp.h"         // IWYU pragma: export
#include "vm.h"         // IWYU pragma: export
#include "instr.h"      // IWYU pragma: export
#include "emit.h"       // IWYU pragma: export
#include "compile.h"
#include "gwion.h"
#include "pass.h"
#include "gwiond_defs.h"
#include "fold_pass.h"
#include "io.h"
#include "gwiond.h"
#include "util.h"

struct Gwion_ gwion = {};

static DiagnosticList **_diagnostics = NULL; // thread_local?

static cJSON *get_diagnostics(const Gwion gwion, const char *filename) {
  for(uint32_t i = 0; i < (*_diagnostics)->len; i++) {
    const Diagnostic info = diagnosticlist_at((*_diagnostics), i);
    if(!strcmp(filename, info.filename))
      return cJSON_GetObjectItem(info.cjson, "diagnostics");
  }
  Diagnostic info = {
    .filename = filename, 
    .cjson = cJSON_CreateObject()
  };
  diagnosticlist_add(gwion->mp, _diagnostics, info);
  cJSON_AddStringToObject(info.cjson, "uri", filename);
  return cJSON_AddArrayToObject(info.cjson, "diagnostics");
}

static cJSON* mk_diagnostic(const char* msg, const loc_t loc, const int severity) {
  cJSON *diagnostic = cJSON_CreateObject();
  json_range(diagnostic, loc);
  cJSON_AddNumberToObject(diagnostic, "severity", severity);
  cJSON_AddStringToObject(diagnostic, "message", msg);
  return diagnostic;
}

static void gwiond_error(const char *main, const char *secondary,
  const char *filename, const loc_t loc,  const uint errno NUSED,
                         const enum libprettyerr_errtype error_code) {
  char msg[256];
  tcol_snprintf(msg, 256, "%s.%s", main, secondary ?: "");
  cJSON *diagnostic = mk_diagnostic(msg, loc, LSP_ERROR);
  if(error_code)
    cJSON_AddNumberToObject(diagnostic, "code", error_code);
  cJSON *diagnostics = get_diagnostics(&gwion, filename);
  cJSON_AddItemToArray(diagnostics, diagnostic);
}

static void gwiond_warning(const char *main, const char *filename,
                         const loc_t loc) {
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
  json_location(related, filename, loc); // NOTE: may be a good thing to add uri
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
  char msg[256];
  tcol_snprintf(msg, 256, main);
  cJSON *diagnostic = mk_diagnostic(msg, loc, LSP_HINT);
  cJSON *diagnostics = get_diagnostics(&gwion, filename);
  cJSON_AddItemToArray(diagnostics, diagnostic);
}

void gwiond_parse(const Gwion gwion, DiagnosticList **diagnostics, const char *filename, Buffer *buffer) {
  _diagnostics = diagnostics;
  const Symbol sym = insert_symbol(gwion->st, "@lsp_buffer");
  const loc_t loc = {};
  const Tag tag = MK_TAG(sym, loc);
  Value value = new_value(gwion->env, gwion->type[et_int], tag);
  value->d.ptr = (void*)buffer;
  ValueList *values = new_valuelist(gwion->mp, 1);
  valuelist_set(values, 0, value);
  compile_string_xid_values_comments(gwion, (char*)filename, buffer->content, NULL, 0, values, &buffer->comments);
  free_valuelist(gwion->mp, values);
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

ANN static void gwiond_passes(const Gwion gwion) {
  pass_register(gwion, "documentation", doc_pass);
  struct Vector_ passes;
  vector_init(&passes);
  vector_add(&passes, (m_uint)"sema");
  vector_add(&passes, (m_uint)"check");
  vector_add(&passes, (m_uint)"documentation");
  pass_set(gwion, &passes);
  vector_release(&passes);
}

// should probably take gwion as arg instead
Gwion gwiond_ini(void) {
  CliArg arg = { .arg = {.argc = 0, .argv = NULL }, .uargs = true};
  const bool ini = gwion_ini(&gwion, &arg);
  arg_release(&arg);
  gwiond_passes(&gwion);
  gwlog_set_func(gwiond_error, gwiond_warning,
                 gwiond_related, gwiond_hint);
  gwion_parser_set_default_pos((pos_t) { 0, 0 });
  return &gwion;
}

void gwiond_end(void) {
  gwion_end(&gwion);
}
