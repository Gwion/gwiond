#include "gwion_env.h"
typedef struct {
  char *uri;
  loc_t loc;
} Reference;
MK_VECTOR_TYPE(Reference, reference)

typedef struct {
  MemPool mp;
  char *filename;
  Value value;
  ReferenceList *list;
} References;

ANN static bool references_exp(References *a,  const Exp* b);
ANN static bool references_stmt(References *a, const Stmt* b);
ANN static bool references_stmt_list(References *a, const StmtList *b);
ANN /*static*/ bool references_ast(References *a, const Ast b);
