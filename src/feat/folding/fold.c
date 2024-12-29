#include "gwion_util.h"
#include "gwion_ast.h" // IWYU pragma: export
#include "gwion_env.h" // IWYU pragma: export
#include "vm.h"        // IWYU pragma: export
#include "instr.h"     // IWYU pragma: export
#include "emit.h"      // IWYU pragma: export
#include "gwion.h"
#include "fold_pass.h"
#include "lsp.h"


ANN static void fold_comments(Fold *fold, const loc_t loc) {
  FoldRange fr = {
    .loc = loc,
    .kind = "comment"
  };
  foldlist_add(fold->mp, &fold->foldranges, fr);
}

ANN static void comments(CommentList *comments, Fold* fold) {
  if(!comments->len) return; // move me!
  Comment c = commentlist_at(comments, 0);
  loc_t loc = c.loc; // may be pos, or just line
  comment_t type = c.type;
  for(uint32_t i = 0; i < comments->len; i++) {
    Comment c = commentlist_at(comments, i);
    //if(c.alone        ||
    if(
       type != c.type ||
       loc.last.line + 1 < c.loc.first.line) {
      fold_comments(fold, loc);
      loc = c.loc;
    }
    else loc.last = c.loc.last;
    type = c.type;
  }
  fold_comments(fold, loc);
}

void lsp_foldingRange(const Gwion gwion, int id, const cJSON *params_json) {
  TextDocumentPosition document = lsp_parse_document(params_json);
  Buffer buffer = get_buffer(document.uri);
  Fold fold = {};
  fold_ini(gwion->mp, &fold);
  if(buffer.context) fold_ast(&fold, buffer.context->tree);
  comments(buffer.comments, &fold);

  cJSON *json = cJSON_CreateArray();
  for(uint32_t i = 0; i < fold.foldranges->len; i++) {
    const FoldRange fr = foldlist_at(fold.foldranges, i);
    cJSON *foldrange = cJSON_CreateObject();
    cJSON_AddNumberToObject(foldrange, "startLine", fr.loc.first.line);
    cJSON_AddNumberToObject(foldrange, "startCharacter", fr.loc.first.column);
    cJSON_AddNumberToObject(foldrange, "endLine", fr.loc.last.line);
    cJSON_AddNumberToObject(foldrange, "endCharacter", fr.loc.last.column);
    cJSON_AddStringToObject(foldrange, "kind", fr.kind);
    cJSON_AddItemToArray(json, foldrange);
  }
  lsp_send_response(id, json);	
  fold_end(gwion->mp, &fold);
}
