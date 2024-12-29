/*typedef struct CompleteItem {*/
/*  union {*/
/*  Value value;*/
/*// Func func;*/
/*//  Type  type;*/
/*// Trait trait;*/
/*  char *label;*/
/*  } data;*/
/*  CompleteType type;*/
/*} CompleteItem;*/
typedef struct {
  ValueList *values;
  MemPool mp;
  int line;
  int character;
  Class_Def class_def;
  Exp *last; // is that useful?
  uint32_t scope;
} Complete;

typedef enum {
	CompKind_Text = 1,
	CompKind_Method = 2,
	CompKind_Function = 3,
	CompKind_Constructor = 4,
	CompKind_Field = 5,
	CompKind_Variable = 6,
	CompKind_Class = 7,
	CompKind_Interface = 8,
	CompKind_Module = 9,
	CompKind_Property = 10,
	CompKind_Unit = 11,
	CompKind_Value = 12,
	CompKind_Enum = 13,
	CompKind_Keyword = 14,
	CompKind_Snippet = 15,
	CompKind_Color = 16,
	CompKind_File = 17,
	CompKind_Reference = 18,
	CompKind_Folder = 19,
	CompKind_EnumMember = 20,
	CompKind_Constant = 21,
	CompKind_Struct = 22,
	CompKind_Event = 23,
	CompKind_Operator = 24,
	CompKind_TypeParameter = 25,
} CompletionKind;

ANN static bool complete_exp(Complete *a, const Exp* b);
ANN static bool complete_stmt(Complete *a, const Stmt* b);
ANN static bool complete_stmt_list(Complete *a, const StmtList *b);
ANN bool complete_ast(Complete *a, const Ast b);
