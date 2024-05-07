typedef struct {
  struct {
    const char *source;
    const char *target;
  } filename;
  Gwion gwion;
  Value value;
  pos_t pos;
} Hover;

ANN bool hover_ast(Hover *a, Ast b);
