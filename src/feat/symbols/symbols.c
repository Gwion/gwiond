#include "gwion_util.h" // IWYU pragma: export
#include "gwion_ast.h"  // IWYU pragma: export
#include "gwion_env.h"  // IWYU pragma: export
#include "vm.h"         // IWYU pragma: export
#include "gwion.h"      // IWYU pragma: export
#include "lsp.h"
#include "util.h"
#include "feat/get_value.h"

typedef enum SymbolKind {
  SK_File = 1,
  SK_Module = 2,
  SK_Namespace = 3,
  SK_Package = 4,
  SK_Class = 5,          //
  SK_Method = 6,         //
  SK_Property = 7,
  SK_Field = 8,
  SK_Constructor = 9,    //
  SK_Enum = 10,          //
  SK_Interface = 11,
  SK_Function = 12,      //
  SK_Variable = 13,
  SK_Constant = 14,
  SK_String = 15,        //
  SK_Number = 16,        //
  SK_Boolean = 17,       //
  SK_Array = 18,         //
  SK_Object = 19,        //
  SK_Key = 20,           // N.A.
  SK_Null = 21,          // N.A.
  SK_EnumMember = 22,    //
  SK_Struct = 23,        //
  SK_Event = 24,         //
  SK_Operator = 25,      //
  SK_TypeParameter = 26,
} SymbolKind;

static SymbolKind get_kind(Gwion gwion, const Value v) {
  if(tflag(v->type, tflag_union))
    return SK_Enum;
  if(v->from->owner_class && tflag(v->from->owner_class, tflag_union))
    return SK_EnumMember;
  if(isa(v->type, gwion->type[et_event]))
    return SK_Event;
  if(is_class(gwion, v->type))
     return !tflag(v->type, tflag_struct) ? SK_Class : SK_Struct;
  if(isa(v->type, gwion->type[et_array]))
    return SK_Array;
  if(isa(v->type, gwion->type[et_bool]))
    return SK_Boolean;
  // catch enum before?
  if(isa(v->type, gwion->type[et_int]) || isa(v->type, gwion->type[et_float]))
    return SK_Number;
  if(is_func(gwion, v->type)) {
    if(!strcmp(s_name(v->d.func_ref->def->base->tag.sym), "new"))
      return SK_Constructor;
    if(fbflag(v->d.func_ref->def->base, fbflag_op))
      return SK_Operator;
    return v->from->owner_class ? SK_Method : SK_Function;
  }
  if(isa(v->type, gwion->type[et_string]))
     return SK_String;
  if(isa(v->type, gwion->type[et_object]))
    return SK_Object;
  return SK_Variable;//Field
}

void lsp_symbols(const Gwion gwion, int id, const cJSON *params_json) {
  TextDocumentPosition document = lsp_parse_document(params_json);
  Buffer buffer = get_buffer(document.uri);
  if(!buffer.context) return;
  ValueList *values = get_value_all(gwion->mp, buffer.context->tree);
  cJSON *results = cJSON_CreateArray();
  for(uint32_t i = 0; i < values->len; i++) {
    const Value value = valuelist_at(values, i);
    cJSON *documentSymbol  = cJSON_CreateObject();
    cJSON_AddStringToObject(documentSymbol, "name", value->name);
    // detail?
    const m_str detail = get_doc(value->from->ctx, value);
    if(detail) cJSON_AddStringToObject(documentSymbol, "detail", detail);
    const SymbolKind kind = get_kind(gwion, value);
    cJSON_AddNumberToObject(documentSymbol, "kind", kind);
    // tag?
    json_range(documentSymbol, value->from->loc);
    cJSON *selectionRange = json_range(NULL, value->from->loc);
    cJSON_AddItemToObject(documentSymbol, "selectionRange", selectionRange);
    // children?
    //
    cJSON_AddItemToArray(results, documentSymbol);
  }
  lsp_send_response(id, results);
  free_valuelist(gwion->mp, values);
}
