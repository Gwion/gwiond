#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "io.h"
#include "gwiond.h"
#include "gwiond_pass.h"

ANN static void add_foldrange(GwiondInfo *a, const FoldRange fr) {
  mp_vector_add(a->mp, &a->foldranges, FoldRange, fr);
}

ANN static void gwiond_symbol(GwiondInfo *a, Symbol b) {
  (void)a;
  (void)b;
}

ANN static void gwiond_loc(GwiondInfo *a, loc_t b) {
  (void)a;
  (void)b;
}

ANN static void gwiond_tag(GwiondInfo *a, Tag *b) {
  if(b->sym) gwiond_symbol(a, b->sym);
  gwiond_loc(a, b->loc);
}

ANN static void gwiond_array_sub(GwiondInfo *a, Array_Sub b) {
  if(b->exp) gwiond_exp(a, b->exp);
}

ANN static void gwiond_id_list(GwiondInfo *a, ID_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Symbol c = *mp_vector_at(b, Symbol, i);
    gwiond_symbol(a, c);
  }
}

ANN static void gwiond_specialized(GwiondInfo *a, Specialized *b) {
  gwiond_tag(a, &b->tag);
  if(b->traits) gwiond_id_list(a, b->traits);
}

ANN static void gwiond_specialized_list(GwiondInfo *a, Specialized_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Specialized * c = mp_vector_at(b, Specialized  , i);
    gwiond_specialized(a, c);
  }
}

ANN static void gwiond_tmplarg(GwiondInfo *a, TmplArg *b) {
  if (b->type == tmplarg_td) gwiond_type_decl(a, b->d.td);
  else gwiond_exp(a, b->d.exp);
}

ANN static void gwiond_tmplarg_list(GwiondInfo *a, TmplArg_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    TmplArg * c = mp_vector_at(b, TmplArg, i);
    gwiond_tmplarg(a, c);
  }
}

ANN static void gwiond_tmpl(GwiondInfo *a, Tmpl *b) {
  if(b->list) gwiond_specialized_list(a, b->list);
  if(b->call) gwiond_tmplarg_list(a, b->call);
}

ANN static void gwiond_range(GwiondInfo *a, Range *b) {
  if(b->start) gwiond_exp(a, b->start);
  if(b->end) gwiond_exp(a , b->end);
}

ANN static void gwiond_type_decl(GwiondInfo *a, Type_Decl *b) {
  gwiond_tag(a, &b->tag);
  if(b->array) gwiond_array_sub(a, b->array);
  if(b->types) gwiond_tmplarg_list(a, b->types);
}

ANN static void gwiond_prim_id(GwiondInfo *a, Symbol *b) {
  gwiond_symbol(a, *b);
}

ANN static void gwiond_prim_num(GwiondInfo *a, m_uint *b) {
  (void)a;
  (void)b;
}

ANN static void gwiond_prim_float(GwiondInfo *a, m_float *b) {
  (void)a;
  (void)b;
}

ANN static void gwiond_prim_str(GwiondInfo *a, m_str *b) {
  (void)a;
  (void)b;
}

ANN static void gwiond_prim_array(GwiondInfo *a, Array_Sub *b) {
  gwiond_array_sub(a, *b);
}

ANN static void gwiond_prim_range(GwiondInfo *a, Range* *b) {
  gwiond_range(a, *b);
}

ANN static void gwiond_prim_dict(GwiondInfo *a, Exp* *b) {
  gwiond_exp(a, *b);
}

ANN static void gwiond_prim_hack(GwiondInfo *a, Exp* *b) {
  gwiond_exp(a, *b);
}

ANN static void gwiond_prim_interp(GwiondInfo *a, Exp* *b) {
  gwiond_exp(a, *b);
}

ANN static void gwiond_prim_char(GwiondInfo *a, m_str *b) {
  (void)a;
  (void)b;
}

ANN static void gwiond_prim_nil(GwiondInfo *a, void *b) {
  (void)a;
  (void)b;
}

ANN static void gwiond_prim_perform(GwiondInfo *a, Symbol *b) {
  gwiond_symbol(a, *b);
}

ANN static void gwiond_prim_locale(GwiondInfo *a, Symbol *b) {
  gwiond_symbol(a, *b);
}

DECL_PRIM_FUNC(gwiond, void, GwiondInfo *)
ANN static void gwiond_prim(GwiondInfo *a, Exp_Primary *b) {
  gwiond_prim_func[b->prim_type](a, &b->d);
}

ANN static void gwiond_var_decl(GwiondInfo *a, Var_Decl *b) {
  gwiond_tag(a, &b->tag);
}

ANN static void gwiond_variable(GwiondInfo *a, Variable *b) {
  if(b->td) gwiond_type_decl(a, b->td);
  gwiond_var_decl(a, &b->vd);
}

ANN static void gwiond_exp_decl(GwiondInfo *a, Exp_Decl *b) {
  gwiond_variable(a, &b->var);
}

ANN static void gwiond_exp_binary(GwiondInfo *a, Exp_Binary *b) {
  gwiond_exp(a, b->lhs);
  gwiond_exp(a, b->rhs);
  gwiond_symbol(a, b->op);
}

ANN static void gwiond_capture(GwiondInfo *a, Capture *b) {
  gwiond_tag(a, &b->var.tag);
}

ANN static void gwiond_captures(GwiondInfo *a, Capture_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Capture *c = mp_vector_at(b, Capture, i);
    gwiond_capture(a, c);
  }
}

ANN static void gwiond_exp_unary(GwiondInfo *a, Exp_Unary *b) {
  gwiond_symbol(a, b->op);
  const enum unary_type type = b->unary_type;
  if(type == unary_exp) gwiond_exp(a, b->exp);
  else if(type == unary_td) gwiond_type_decl(a, b->ctor.td);
  else {
    gwiond_stmt_list(a, b->code);
    if(b->captures)gwiond_captures(a, b->captures);
  }
}

ANN static void gwiond_exp_cast(GwiondInfo *a, Exp_Cast *b) {
  gwiond_type_decl(a, b->td);
  gwiond_exp(a, b->exp);
}

ANN static void gwiond_exp_post(GwiondInfo *a, Exp_Postfix *b) {
  gwiond_symbol(a, b->op);
  gwiond_exp(a, b->exp);
}

ANN static void gwiond_exp_call(GwiondInfo *a, Exp_Call *b) {
  gwiond_exp(a, b->func);
  if(b->args) gwiond_exp(a, b->args);
  if(b->tmpl) gwiond_tmpl(a, b->tmpl);
}

ANN static void gwiond_exp_array(GwiondInfo *a, Exp_Array *b) {
  gwiond_exp(a, b->base);
  gwiond_array_sub(a, b->array);
}

ANN static void gwiond_exp_slice(GwiondInfo *a, Exp_Slice *b) {
  gwiond_exp(a, b->base);
  gwiond_range(a, b->range);
}

ANN static void gwiond_exp_if(GwiondInfo *a, Exp_If *b) {
  gwiond_exp(a, b->cond);
  if(b->if_exp) gwiond_exp(a, b->if_exp);
  gwiond_exp(a, b->else_exp);
}

ANN static void gwiond_exp_dot(GwiondInfo *a, Exp_Dot *b) {
  gwiond_exp(a, b->base);
  gwiond_symbol(a,  b->xid);
}

ANN static void gwiond_exp_lambda(GwiondInfo *a, Exp_Lambda *b) {
  gwiond_func_def(a, b->def);
}

ANN static void gwiond_exp_td(GwiondInfo *a, Type_Decl *b) {
  gwiond_type_decl(a, b);
}

DECL_EXP_FUNC(gwiond, void, GwiondInfo*)
ANN static void gwiond_exp(GwiondInfo *a, Exp* b) {
  gwiond_exp_func[b->exp_type](a, &b->d);
  if(b->next) return gwiond_exp(a, b ->next);
}

ANN static void gwiond_stmt_exp(GwiondInfo *a, Stmt_Exp b) {
  if(b->val) gwiond_exp(a,  b->val);
}

ANN static void gwiond_stmt_while(GwiondInfo *a, Stmt_Flow b) {
  gwiond_exp(a, b->cond);
  gwiond_stmt(a, b->body);
}

ANN static void gwiond_stmt_until(GwiondInfo *a, Stmt_Flow b) {
  gwiond_exp(a, b->cond);
  gwiond_stmt(a, b->body);
}

ANN static void gwiond_stmt_for(GwiondInfo *a, Stmt_For b) {
  gwiond_stmt(a, b->c1);
  if(b->c2) gwiond_stmt(a, b->c2);
  if(b->c3) gwiond_exp(a, b->c3);
  gwiond_stmt(a, b->body);
}

ANN static void gwiond_stmt_each(GwiondInfo *a, Stmt_Each b) {
  gwiond_var_decl(a, &b->var);
  if(b->idx.tag.sym) gwiond_var_decl(a, &b->idx);
  gwiond_exp(a, b->exp);
  gwiond_stmt(a, b->body);
}

ANN static void gwiond_stmt_loop(GwiondInfo *a, Stmt_Loop b) {
  gwiond_exp(a,  b->cond);
  gwiond_stmt(a, b->body);
}

ANN static void gwiond_stmt_if(GwiondInfo *a, Stmt_If b) {
  gwiond_exp(a,  b->cond);
  gwiond_stmt(a, b->if_body);
  if(b->else_body) gwiond_stmt(a, b->else_body);
}

ANN static void gwiond_stmt_code(GwiondInfo *a, Stmt_Code b) {
  if(b->stmt_list) gwiond_stmt_list(a, b->stmt_list);
}

ANN static void gwiond_stmt_break(GwiondInfo *a, Stmt_Index b) {
  gwiond_stmt_index(a, b);
}

ANN static void gwiond_stmt_continue(GwiondInfo *a, Stmt_Index b) {
  gwiond_stmt_index(a, b);
}

ANN static void gwiond_stmt_return(GwiondInfo *a, Stmt_Exp b) {
  if(b->val) gwiond_exp(a, b-> val);
}

ANN static void gwiond_case_list(GwiondInfo *a, Stmt_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Stmt* c = mp_vector_at(b, Stmt, i);
    gwiond_stmt_case(a, &c->d.stmt_match);
  }
}

ANN static void gwiond_stmt_match(GwiondInfo *a, Stmt_Match b) {
  gwiond_exp(a, b->cond);
  gwiond_case_list(a, b->list);
  if(b->where) gwiond_stmt(a, b->where);
}

ANN static void gwiond_stmt_case(GwiondInfo *a, Stmt_Match b) {
  gwiond_exp(a, b->cond);
  gwiond_stmt_list(a, b->list);
  if(b->when) gwiond_exp(a, b->when);
}

ANN static void gwiond_stmt_index(GwiondInfo *a, Stmt_Index b) {
  (void)a;
  (void)b;
}

ANN static void fold_ini(GwiondInfo *a, Stmt_PP b, const char *kind) {
  a->last = (FoldRange){ .kind = kind, .loc = stmt_self(b)->loc };
}

ANN static bool fold_is(GwiondInfo *a, const char *kind) {
  assert(a->last.kind);
  if(!strcmp(a->last.kind, kind)) 
    return true;
  return false;
}

ANN static bool fold_can_continue(GwiondInfo *a, const Stmt_PP b) {
  return stmt_self(b)->loc.first.line - 1 == a->last.loc.last.line;
}

ANN static void fold_continue(GwiondInfo *a, const Stmt_PP b) {
  a->last.loc.last = stmt_self(b)->loc.last;
}

ANN static void fold_end(GwiondInfo *a, const Stmt_PP b, const char *kind) {
  if(fold_is(a, kind) && fold_can_continue(a, b))
    fold_continue(a, b);
  else {
    add_foldrange(a, a->last); 
    a->last.kind = NULL;
  }
}

ANN static void fold(GwiondInfo *a, const Stmt_PP b, const char *kind) {
    if(!a->last.kind) fold_ini(a, b, kind);
    else fold_end(a, b, kind);
}

ANN static void gwiond_stmt_pp(GwiondInfo *a, Stmt_PP b) {
  if(b->pp_type == ae_pp_comment) fold(a, b, "comment");
  if(b->pp_type == ae_pp_import) fold(a, b, "imports");
}

ANN static void gwiond_stmt_retry(GwiondInfo *a, Stmt_Exp b) {
  (void)a;
  (void)b;
}

ANN static void gwiond_handler(GwiondInfo *a, Handler *b) {
  gwiond_tag(a, &b->tag);
  gwiond_stmt(a, b->stmt);
}

ANN static void gwiond_handler_list(GwiondInfo *a, Handler_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Handler *handler = mp_vector_at(b, Handler, i);
    gwiond_handler(a, handler);
  }
}

ANN static void gwiond_stmt_try(GwiondInfo *a, Stmt_Try b) {
  gwiond_stmt(a, b->stmt);
  gwiond_handler_list(a, b->handler);
}

ANN static void gwiond_stmt_defer(GwiondInfo *a, Stmt_Defer b) {
  gwiond_stmt(a, b->stmt);
}

ANN static void gwiond_stmt_spread(GwiondInfo *a, Spread_Def b) {
  gwiond_tag(a, &b->tag);
  gwiond_id_list(a, b->list);
}
DECL_STMT_FUNC(gwiond, void, GwiondInfo*)
ANN static void gwiond_stmt(GwiondInfo *a, Stmt* b) {
  gwiond_stmt_func[b->stmt_type](a, &b->d);
}

ANN static void gwiond_arg(GwiondInfo *a, Arg *b) {
  gwiond_variable(a, &b->var);
}

ANN static void gwiond_arg_list(GwiondInfo *a, Arg_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Arg *c = mp_vector_at(b, Arg, i);
    gwiond_arg(a, c);
  }
}

ANN static void gwiond_variable_list(GwiondInfo *a, Variable_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Variable *c = mp_vector_at(b, Variable, i);
    gwiond_variable(a, c);
  }
}

ANN static void gwiond_stmt_list(GwiondInfo *a, Stmt_List b) {
  if(!b->len) return;
  for(uint32_t i = 0; i < b->len; i++) {
    Stmt* c = mp_vector_at(b, Stmt, i);
    gwiond_stmt(a, c);
  }
}

ANN static void gwiond_func_base(GwiondInfo *a, Func_Base *b) {
  if(b->td) gwiond_type_decl(a, b->td);
  gwiond_tag(a, &b->tag);
  if(b->args) gwiond_arg_list(a, b->args);
  if(b->tmpl) gwiond_tmpl(a, b->tmpl);
}

ANN static void gwiond_func_def(GwiondInfo *a, Func_Def b) {
  gwiond_func_base(a, b->base);
  if(b->d.code) gwiond_stmt_list(a, b->d.code);
  if(b->captures) gwiond_captures(a, b->captures);
}

ANN static void gwiond_class_def(GwiondInfo *a, Class_Def b) {
  gwiond_type_def( a, &b->base);
  if(b->body) gwiond_ast(a, b->body);
}

ANN static void gwiond_trait_def(GwiondInfo *a, Trait_Def b) {
  if(b->body) gwiond_ast(a, b->body);
}

ANN static void gwiond_enumvalue(GwiondInfo *a, EnumValue *b) {
  gwiond_tag(a, &b->tag);
  // gwint, set
}

ANN static void gwiond_enum_list(GwiondInfo *a, EnumValue_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    EnumValue *c = mp_vector_at(b, EnumValue, i);
    gwiond_enumvalue(a, c);
  }
}

ANN static void gwiond_enum_def(GwiondInfo *a, Enum_Def b) {
  gwiond_enum_list(a, b->list);
  gwiond_tag(a, &b->tag);
}

ANN static void gwiond_union_def(GwiondInfo *a, Union_Def b) {
  gwiond_variable_list(a, b->l);
  gwiond_tag(a, &b->tag);
  if(b->tmpl) gwiond_tmpl(a, b->tmpl);
}

ANN static void gwiond_fptr_def(GwiondInfo *a, Fptr_Def b) {
  gwiond_func_base(a, b->base);
}

ANN static void gwiond_type_def(GwiondInfo *a, Type_Def b) {
  if(b->ext) gwiond_type_decl(a, b->ext);
  gwiond_tag(a, &b->tag);
  if(b->tmpl) gwiond_tmpl(a, b->tmpl);
}

ANN static void gwiond_extend_def(GwiondInfo *a, Extend_Def b) {
  gwiond_type_decl(a, b->td);
  gwiond_id_list(a, b->traits);
}

ANN static void gwiond_prim_def(GwiondInfo *a, Prim_Def b) {
  gwiond_tag(a, &b->tag);
}

DECL_SECTION_FUNC(gwiond, void, GwiondInfo*)
ANN static void gwiond_section(GwiondInfo *a, Section *b) {
  add_foldrange(a, (FoldRange){"region", b->loc});
  gwiond_section_func[b->section_type](a, *(void**)&b->d);
}

ANN void gwiond_ast(GwiondInfo *a, Ast b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Section *c = mp_vector_at(b, Section, i);
    gwiond_section(a, c);
  }
  // catch up remaining comments or imports foldranges
  if(a->last.kind) add_foldrange(a, a->last);
}
