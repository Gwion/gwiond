#include <wchar.h>
typedef struct FoldRange {
  const char *kind;
  loc_t loc;
} FoldRange;

typedef struct GwiondInfo {
  MemPool mp;
  MP_Vector *foldranges;
  FoldRange last;
} GwiondInfo;

ANN static inline void gwiondinfo_ini(MemPool mp, GwiondInfo *gi) {
  gi->foldranges = new_mp_vector(mp, FoldRange, 0);
  gi->mp = mp;
}

ANN static inline void gwiondinfo_end(MemPool mp, GwiondInfo *gi) {
  free_mp_vector(mp, FoldRange, gi->foldranges);
}

ANN static void gwiond_loc(GwiondInfo *a, loc_t b);
ANN static void gwiond_symbol(GwiondInfo *a, Symbol b);
ANN static void gwiond_tag(GwiondInfo *a, Tag *tag);
ANN static void gwiond_array_sub(GwiondInfo *a, Array_Sub b);
ANN static void gwiond_specialized(GwiondInfo *a, Specialized *b);
ANN static void gwiond_tmpl(GwiondInfo *a, Tmpl *b);
ANN static void gwiond_range(GwiondInfo *a, Range *b);
ANN static void gwiond_type_decl(GwiondInfo *a, Type_Decl *b);
ANN static void gwiond_prim_id(GwiondInfo *a, Symbol *b);
ANN static void gwiond_prim_num(GwiondInfo *a, m_uint *b);
ANN static void gwiond_prim_float(GwiondInfo *a, m_float *b);
ANN static void gwiond_prim_str(GwiondInfo *a, m_str *b);
ANN static void gwiond_prim_array(GwiondInfo *a, Array_Sub *b);
ANN static void gwiond_prim_range(GwiondInfo *a, Range* *b);
ANN static void gwiond_prim_dict(GwiondInfo *a, Exp* *b);
ANN static void gwiond_prim_hack(GwiondInfo *a, Exp* *b);
ANN static void gwiond_prim_interp(GwiondInfo *a, Exp* *b);
ANN static void gwiond_prim_char(GwiondInfo *a, m_str *b);
ANN static void gwiond_prim_nil(GwiondInfo *a, void *b);
ANN static void gwiond_prim_perform(GwiondInfo *a, Symbol *b);
ANN static void gwiond_prim(GwiondInfo *a, Exp_Primary *b);
ANN static void gwiond_var_decl(GwiondInfo *a, Var_Decl *b);
ANN static void gwiond_variable(GwiondInfo *a, Variable *b);
ANN static void gwiond_exp_decl(GwiondInfo *a, Exp_Decl *b);
ANN static void gwiond_exp_binary(GwiondInfo *a, Exp_Binary *b);
ANN static void gwiond_exp_unary(GwiondInfo *a, Exp_Unary *b);
ANN static void gwiond_exp_cast(GwiondInfo *a, Exp_Cast *b);
ANN static void gwiond_exp_post(GwiondInfo *a, Exp_Postfix *b);
ANN static void gwiond_exp_call(GwiondInfo *a, Exp_Call *b);
ANN static void gwiond_exp_array(GwiondInfo *a, Exp_Array *b);
ANN static void gwiond_exp_slice(GwiondInfo *a, Exp_Slice *b);
ANN static void gwiond_exp_if(GwiondInfo *a, Exp_If *b);
ANN static void gwiond_exp_dot(GwiondInfo *a, Exp_Dot *b);
ANN static void gwiond_exp_lambda(GwiondInfo *a, Exp_Lambda *b);
ANN static void gwiond_exp_td(GwiondInfo *a, Type_Decl *b);
ANN static void gwiond_exp(GwiondInfo *a, Exp* b);
ANN static void gwiond_stmt_exp(GwiondInfo *a, Stmt_Exp b);
ANN static void gwiond_stmt_while(GwiondInfo *a, Stmt_Flow b);
ANN static void gwiond_stmt_until(GwiondInfo *a, Stmt_Flow b);
ANN static void gwiond_stmt_for(GwiondInfo *a, Stmt_For b);
ANN static void gwiond_stmt_each(GwiondInfo *a, Stmt_Each b);
ANN static void gwiond_stmt_loop(GwiondInfo *a, Stmt_Loop b);
ANN static void gwiond_stmt_if(GwiondInfo *a, Stmt_If b);
ANN static void gwiond_stmt_code(GwiondInfo *a, Stmt_Code b);
ANN static void gwiond_stmt_break(GwiondInfo *a, Stmt_Index b);
ANN static void gwiond_stmt_continue(GwiondInfo *a, Stmt_Index b);
ANN static void gwiond_stmt_return(GwiondInfo *a, Stmt_Exp b);
ANN static void gwiond_case_list(GwiondInfo *a, Stmt_List b);
ANN static void gwiond_stmt_match(GwiondInfo *a, Stmt_Match b);
ANN static void gwiond_stmt_case(GwiondInfo *a, Stmt_Match b);
ANN static void gwiond_stmt_index(GwiondInfo *a, Stmt_Index b);
ANN static void gwiond_stmt_pp(GwiondInfo *a, Stmt_PP b);
ANN static void gwiond_stmt_retry(GwiondInfo *a, Stmt_Exp b);
ANN static void gwiond_stmt_try(GwiondInfo *a, Stmt_Try b);
ANN static void gwiond_stmt_defer(GwiondInfo *a, Stmt_Defer b);
ANN static void gwiond_stmt(GwiondInfo *a, Stmt* b);
ANN static void gwiond_arg(GwiondInfo *a, Arg *b);
ANN static void gwiond_variable_list(GwiondInfo *a, Variable_List b);
ANN static void gwiond_stmt_list(GwiondInfo *a, Stmt_List b);
ANN static void gwiond_func_base(GwiondInfo *a, Func_Base *b);
ANN static void gwiond_func_def(GwiondInfo *a, Func_Def b);
ANN static void gwiond_class_def(GwiondInfo *a, Class_Def b);
ANN static void gwiond_trait_def(GwiondInfo *a, Trait_Def b);
ANN static void gwiond_enum_def(GwiondInfo *a, Enum_Def b);
ANN static void gwiond_union_def(GwiondInfo *a, Union_Def b);
ANN static void gwiond_fptr_def(GwiondInfo *a, Fptr_Def b);
ANN static void gwiond_type_def(GwiondInfo *a, Type_Def b);
ANN static void gwiond_extend_def(GwiondInfo *a, Extend_Def b);
ANN static void gwiond_section(GwiondInfo *a, Section *b);
ANN void gwiond_ast(GwiondInfo *a, Ast b);
