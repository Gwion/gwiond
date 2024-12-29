#include "defs.h"
#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "vm.h"
#include "gwion.h"
#include "feat/selectionrange.h"
#include "lsp.h"
#include "util.h"

ANN static bool is_after(const pos_t a, const pos_t b) {
    return a.line > b.line ||
      (a.line == b.line && a.column >= b.column);
}
ANN static bool is_before(const pos_t a, const pos_t b) {
    return a.line < b.line ||
      (a.line == b.line && a.column <= b.column);
}
ANN static bool is_inside(const pos_t a, const loc_t loc) {
  return is_after(a, loc.first) && is_before(a, loc.last); 
}

typedef cJSON* Loc;
typedef pos_t Pos;
MK_VECTOR_TYPE(Loc, loc);
MK_VECTOR_TYPE(Pos, pos);

typedef struct SelectionRanges {
  MemPool  mp;
  PosList *poss;
  LocList *locs;
} SelectionRanges;

static bool inside(SelectionRange *selectionrange, const loc_t loc) {
  bool ret = false;
  SelectionRanges *sr = (SelectionRanges*)selectionrange->data;
  for(uint32_t i = 0; i < sr->poss->len; i++) {
    const pos_t pos = poslist_at(sr->poss, i);
    if(is_inside(pos, loc)) {
      cJSON *json = cJSON_CreateObject();
      json_range(json, loc);
      cJSON *parent = loclist_at(sr->locs, i);
      if(parent) {
        // TODO: we should check parent is different!
        cJSON_AddItemToObject(json, "parent", parent);
      }
      loclist_set(sr->locs, i, json);
      ret = true;
    }
  } 
  return ret;
}

ANN bool selectionrange_ast(SelectionRange *a, const Ast b);
ANN void selectionrange(const cJSON *params_json, bool (*inside)(SelectionRange*, const loc_t), void *data) {
  TextDocumentPosition document = lsp_parse_document(params_json);
  SelectionRange pass = {
    .pos = document.pos,
    .inside = inside,
    .data = data,
  };
  Buffer buffer = get_buffer(document.uri);
  if(buffer.context && buffer.context->tree)
    selectionrange_ast(&pass, buffer.context->tree);
}

void lsp_selectionRange(Gwion gwion, int id, const cJSON *params_json) {
  cJSON *positions = cJSON_GetObjectItem(params_json, "positions");
  
  const uint32_t len = cJSON_GetArraySize(positions);
  LocList *locs = new_loclist(gwion->mp, len); // maybe we can use a json array instead
  PosList *poss = new_poslist(gwion->mp, len); 
  for(uint32_t i = 0; i < len; i++) {
    cJSON *position = cJSON_GetArrayItem(positions, i);
    cJSON *json_line = cJSON_GetObjectItem(position, "line");
    cJSON *json_char = cJSON_GetObjectItem(position, "character");
    pos_t pos = {
        .line = json_line->valueint,
        .column = json_char->valueint
    };
    poslist_set(poss, i, pos);
  }

  SelectionRanges sr = {
    .mp = gwion->mp,
    .poss = poss, // maybe we can use a json array instead
    .locs = locs,
  };
  selectionrange(params_json, inside, &sr);
  cJSON *results = cJSON_CreateArray();
  for(uint32_t i = 0; i < len; i++) {
    const Loc loc = loclist_at(sr.locs, i);
    if(loc) // what if null????
      cJSON_AddItemToArray(results, loc);
  }
  lsp_send_response(id, results);
  free_loclist(gwion->mp, sr.locs);
  free_poslist(gwion->mp, poss);
}
