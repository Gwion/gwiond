#include <wchar.h>
// TODO: rename to Fold
// and Fold to Folder
typedef struct FoldRange {
  const char *kind;
  loc_t loc;
} FoldRange;
MK_VECTOR_TYPE(FoldRange, fold)

typedef struct Fold {
  MemPool mp;
  FoldRangeList *foldranges;
  FoldRange last;
  loc_t loc;
} Fold;

ANN static inline void fold_ini(MemPool mp, Fold *gi) {
  gi->foldranges = new_foldlist(mp, 0);
  gi->mp = mp;
}

ANN static inline void fold_end(MemPool mp, Fold *gi) {
  free_foldlist(mp, gi->foldranges);
}

ANN static void fold_type_decl(Fold *a, const Type_Decl *b);
ANN static void fold_exp(Fold *a, const Exp* b);
ANN static void fold_stmt(Fold *a, const Stmt* b);
ANN static void fold_stmt_list(Fold *a, const StmtList *b);
ANN static void fold_func_def(Fold *a, const Func_Def b);
ANN static void fold_section(Fold *a, const Section *b);
ANN void fold_ast(Fold *a, const Ast b);
