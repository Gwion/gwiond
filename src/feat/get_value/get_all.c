#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"

typedef struct {
  MemPool mp;
  ValueList *values;
} GetDecls;

ANN2(1) static void getdecls_add(GetDecls *a, Value v) {
  if(v) valuelist_add(a->mp, &a->values, v);
}


ANN static void getdecls_exp(GetDecls *a, const Exp* b);
ANN static void getdecls_stmt(GetDecls *a, const Stmt* b);
ANN static void getdecls_stmt_list(GetDecls *a, const StmtList *b);
ANN static void getdecls_func_def(GetDecls *a, const Func_Def b);
ANN static void getdecls_section(GetDecls *a, const Section *b);
ANN static void getdecls_ast(GetDecls *a, const Ast b);

ANN static void getdecls_symbol(GetDecls *a, const Symbol b) {
  return;
}

ANN static void getdecls_tag(GetDecls *a, const Tag *b) {
  if(b->sym) getdecls_symbol(a, b->sym);
  return;
}

ANN2(1, 2) static void getdecls_type_decl(GetDecls *a, const Type_Decl *b);

ANN static void getdecls_array_sub(GetDecls *a, const Array_Sub b) {
  if(b->exp) getdecls_exp(a, b->exp);
}

ANN static void getdecls_id_list(GetDecls *a, const TagList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Tag c = taglist_at(b, i);
    getdecls_tag(a, &c);
  }
}

ANN static void getdecls_specialized(GetDecls *a, const Specialized *b) {
}

ANN static void getdecls_specialized_list(GetDecls *a, const SpecializedList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Specialized c = specializedlist_at(b, i);
    getdecls_specialized(a, &c);
  }
}

ANN static void getdecls_tmplarg(GetDecls *a, const TmplArg *b) {
  if (b->type == tmplarg_td) getdecls_type_decl(a, b->d.td);
  else getdecls_exp(a, b->d.exp);
}

ANN static void getdecls_tmplarg_list(GetDecls *a, const TmplArgList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const TmplArg c = tmplarglist_at(b, i);
    getdecls_tmplarg(a, &c);
  }
}

ANN static void getdecls_tmpl(GetDecls *a, const Tmpl *b) {
  if(b->list) getdecls_specialized_list(a, b->list);
  if(b->call) getdecls_tmplarg_list(a, b->call); 
}

ANN static void getdecls_range(GetDecls *a, const Range *b) {
  if(b->start) getdecls_exp(a, b->start);
  if(b->end) getdecls_exp(a , b->end);
}

ANN2(1, 2) static void getdecls_type_decl(GetDecls *a, const Type_Decl *b) {
  getdecls_tag(a, &b->tag);
  if(b->array) getdecls_array_sub(a, b->array);
  if(b->types) getdecls_tmplarg_list(a, b->types);
}

ANN static void getdecls_prim_id(GetDecls *a, const Symbol *b) {
}

ANN static void getdecls_prim_num(GetDecls *a, const m_uint *b) {
  // check pos
}

ANN static void getdecls_prim_float(GetDecls *a, const m_float *b) {
  // check pos
}

ANN static void getdecls_prim_str(GetDecls *a, const m_str *b) {
  // check pos
}

ANN static void getdecls_prim_array(GetDecls *a, const Array_Sub *b) {
  getdecls_array_sub(a, *b);
}

ANN static void getdecls_prim_range(GetDecls *a, const Range* *b) {
  getdecls_range(a, *b);
}

ANN static void getdecls_prim_dict(GetDecls *a, const Exp* *b) {
  getdecls_exp(a, *b);
}

ANN static void getdecls_prim_hack(GetDecls *a, const Exp* *b) {
  getdecls_exp(a, *b);
}

ANN static void getdecls_prim_interp(GetDecls *a, const Exp* *b) {
  getdecls_exp(a, *b);
}

ANN static void getdecls_prim_char(GetDecls *a, const m_str *b) {
}

ANN static void getdecls_prim_nil(GetDecls *a, const bool *b) {
}

ANN static void getdecls_prim_perform(GetDecls *a, const Symbol *b) {
  // TODO: effect_info
  getdecls_symbol(a, *b);
}

ANN static void getdecls_prim_locale(GetDecls *a, const Symbol *b) {
  // TODO: locale info (would require locale tracking)
  getdecls_symbol(a, *b);
}

DECL_PRIM_FUNC(getdecls, void, GetDecls *, const);
ANN static void getdecls_prim(GetDecls *a, const Exp_Primary *b) {
  getdecls_prim_func[b->prim_type](a, &b->d);
}

ANN static void getdecls_var_decl(GetDecls *a, const Var_Decl *b) {
  getdecls_tag(a, &b->tag);
  getdecls_add(a, b->value);
}

ANN static void getdecls_variable(GetDecls *a, const Variable *b) {
  if(b->td) getdecls_type_decl(a, b->td);
  getdecls_var_decl(a, &b->vd);
}

ANN static void getdecls_exp_decl(GetDecls *a, const Exp_Decl *b) {
  getdecls_variable(a, &b->var);
}

ANN static void getdecls_exp_binary(GetDecls *a, const Exp_Binary *b) {
  getdecls_exp(a, b->lhs);
  getdecls_exp(a, b->rhs);
  getdecls_symbol(a, b->op); // TODO: op_info
}

ANN static void getdecls_capture(GetDecls *a, const Capture *b) {
  getdecls_var_decl(a, &b->var);
}

ANN static void getdecls_captures(GetDecls *a, const CaptureList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Capture c = capturelist_at(b, i);
    getdecls_capture(a, &c);
  }
}

ANN static void getdecls_exp_unary(GetDecls *a, const Exp_Unary *b) {
  getdecls_symbol(a, b->op); // TODO: op_ino
  const enum unary_type type = b->unary_type;
  if(type == unary_exp) getdecls_exp(a, b->exp);
  else if(type == unary_td) getdecls_type_decl(a, b->ctor.td);
  else {
    getdecls_stmt_list(a, b->code);
    if(b->captures) getdecls_captures(a, b->captures);
  }
}

ANN static void getdecls_exp_cast(GetDecls *a, const Exp_Cast *b) {
  getdecls_type_decl(a, b->td);
  getdecls_exp(a, b->exp);
}

ANN static void getdecls_exp_post(GetDecls *a, const Exp_Postfix *b) {
  getdecls_symbol(a, b->op); // TODO: op_ino
  getdecls_exp(a, b->exp);
}

ANN static void getdecls_exp_call(GetDecls *a, const Exp_Call *b) {
  getdecls_exp(a, b->func);
  if(b->args) getdecls_exp(a, b->args);
  if(b->tmpl) getdecls_tmpl(a, b->tmpl);
}

ANN static void getdecls_exp_array(GetDecls *a, const Exp_Array *b) {
  getdecls_exp(a, b->base);
  getdecls_array_sub(a, b->array);
}

ANN static void getdecls_exp_slice(GetDecls *a, const Exp_Slice *b) {
  getdecls_exp(a, b->base);
  getdecls_range(a, b->range);
}

ANN static void getdecls_exp_if(GetDecls *a, const Exp_If *b) {
  getdecls_exp(a, b->cond);
  if(b->if_exp) getdecls_exp(a, b->if_exp);
  getdecls_exp(a, b->else_exp);
}

ANN static void getdecls_exp_dot(GetDecls *a, const Exp_Dot *b) {
  getdecls_exp(a, b->base);
  getdecls_var_decl(a, &b->var);
}

ANN static void getdecls_exp_lambda(GetDecls *a, const Exp_Lambda *b) {
  getdecls_func_def(a, b->def);
}

ANN static void getdecls_exp_td(GetDecls *a, const Type_Decl *b) {
  getdecls_type_decl(a, b);
}

ANN static void getdecls_exp_named(GetDecls *a, const Exp_Named *b) {
  getdecls_exp(a, b->exp);
}


DECL_EXP_FUNC(getdecls, void, GetDecls*, const);
ANN static void getdecls_exp(GetDecls *a, const Exp* b) {
    getdecls_exp_func[b->exp_type](a, &b->d);
  if(b->next) getdecls_exp(a, b->next);
}

ANN static void getdecls_stmt_exp(GetDecls *a, const Stmt_Exp b) {
  if(b->val) getdecls_exp(a,  b->val);
}

ANN static void getdecls_stmt_while(GetDecls *a, const Stmt_Flow b) {
  getdecls_exp(a, b->cond);
  getdecls_stmt(a, b->body);
}

ANN static void getdecls_stmt_until(GetDecls *a, const Stmt_Flow b) {
  getdecls_exp(a, b->cond);
  getdecls_stmt(a, b->body);
}

ANN static void getdecls_stmt_for(GetDecls *a, const Stmt_For b) {
  getdecls_stmt(a, b->c1);
  if(b->c2) getdecls_stmt(a, b->c2);
  if(b->c3) getdecls_exp(a, b->c3);
  getdecls_stmt(a, b->body);
}

ANN static void getdecls_stmt_each(GetDecls *a, const Stmt_Each b) {
  if(b->idx.tag.sym) getdecls_var_decl(a, &b->idx);
  getdecls_var_decl(a, &b->var);
  getdecls_exp(a, b->exp);
  getdecls_stmt(a, b->body);
}

ANN static void getdecls_stmt_loop(GetDecls *a, const Stmt_Loop b) {
  if(b->idx.tag.sym) getdecls_var_decl(a, &b->idx);
  getdecls_exp(a,  b->cond);
  getdecls_stmt(a, b->body);
}

ANN static void getdecls_stmt_if(GetDecls *a, const Stmt_If b) {
  getdecls_exp(a,  b->cond);
  getdecls_stmt(a, b->if_body);
  if(b->else_body) getdecls_stmt(a, b->else_body);
}

ANN static void getdecls_stmt_code(GetDecls *a, const Stmt_Code b) {
  if(b->stmt_list) getdecls_stmt_list(a, b->stmt_list);
}

ANN static void getdecls_stmt_index(GetDecls *a, const Stmt_Index b) {
}

ANN static void getdecls_stmt_break(GetDecls *a, const Stmt_Index b) {
  getdecls_stmt_index(a, b);
}

ANN static void getdecls_stmt_continue(GetDecls *a, const Stmt_Index b) {
  getdecls_stmt_index(a, b);
}

ANN static void getdecls_stmt_return(GetDecls *a, const Stmt_Exp b) {
  if(b->val) getdecls_exp(a, b-> val);
}

ANN static void getdecls_stmt_case(GetDecls *a, const struct Match *b) {
  getdecls_exp(a, b->cond);
  getdecls_stmt_list(a, b->list);
  if(b->when) getdecls_exp(a, b->when);
}

ANN static void getdecls_case_list(GetDecls *a, const StmtList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Stmt c = stmtlist_at(b, i);
    getdecls_stmt_case(a, &c.d.stmt_match);
  }
}

ANN static void getdecls_stmt_match(GetDecls *a, const Stmt_Match b) {
  getdecls_exp(a, b->cond);
  getdecls_case_list(a, b->list);
  if(b->where) getdecls_stmt(a, b->where);
}

ANN static void getdecls_stmt_pp(GetDecls *a, const Stmt_PP b) {
}

ANN static void getdecls_stmt_retry(GetDecls *a, const Stmt_Exp b) {
}

ANN static void getdecls_handler(GetDecls *a, const Handler *b) {
  getdecls_tag(a, &b->tag);  // should return handler or effect ino
  getdecls_stmt(a, b->stmt);
}

ANN static void getdecls_handler_list(GetDecls *a, const HandlerList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Handler handler = handlerlist_at(b, i);
    getdecls_handler(a, &handler);
  }
}

ANN static void getdecls_stmt_try(GetDecls *a, const Stmt_Try b) {
  getdecls_stmt(a, b->stmt);
  getdecls_handler_list(a, b->handler);
}

ANN static void getdecls_stmt_defer(GetDecls *a, const Stmt_Defer b) {
  getdecls_stmt(a, b->stmt);
}

ANN static void getdecls_stmt_spread(GetDecls *a, const Spread_Def b) {
  getdecls_tag(a, &b->tag); // should print info about spread tg
  getdecls_id_list(a, b->list); // should print info about spread list
}

ANN static void getdecls_stmt_using(GetDecls *a, const UsingStmt *b) {
  getdecls_type_decl(a, b->d.td);
  if(b->tag.sym)
    getdecls_exp(a, b->d.exp);
}

ANN static void getdecls_stmt_import(GetDecls *a, const Stmt_Import b) {
  getdecls_tag(a, &b->tag);
  if(b->selection) {
    for(uint32_t i = 0; i < b->selection->len; i++) {
      const UsingStmt using = usingstmtlist_at(b->selection, i);
      getdecls_stmt_using(a, &using);
    }
  }
}


DECL_STMT_FUNC(getdecls, void, GetDecls*, const);
ANN static void getdecls_stmt(GetDecls *a, const Stmt* b) {
  getdecls_stmt_func[b->stmt_type](a, &b->d);
}

ANN static void getdecls_arg(GetDecls *a, const Arg *b) {
  getdecls_variable(a, &b->var);
}

ANN static void getdecls_arg_list(GetDecls *a, const ArgList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Arg c = arglist_at(b, i);
    getdecls_arg(a, &c);
  }
}

ANN static void getdecls_variable_list(GetDecls *a, const VariableList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Variable c = variablelist_at(b, i);
    getdecls_variable(a, &c);
  }
}

ANN static void getdecls_stmt_list(GetDecls *a, const StmtList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Stmt c = stmtlist_at(b, i);
    getdecls_stmt(a, &c);
  }
}

ANN static void getdecls_func_base(GetDecls *a, const Func_Base *b) {
  if(b->td) getdecls_type_decl(a, b->td);
  getdecls_tag(a, &b->tag);
  if(b->func) getdecls_add(a, b->func->value_ref);
  if(b->tmpl) getdecls_tmpl(a, b->tmpl);
  if(b->args) getdecls_arg_list(a, b->args);
}

ANN static void getdecls_func_def(GetDecls *a, const Func_Def b) {
  getdecls_func_base(a, b->base);
  if(b->d.code) getdecls_stmt_list(a, b->d.code);
  if(b->captures) getdecls_captures(a, b->captures);
}

ANN static void getdecls_type_def(GetDecls *a, const Type_Def b) {
  if(b->ext) getdecls_type_decl(a, b->ext);
  getdecls_tag(a, &b->tag);
  if(b->type) getdecls_add(a, b->type->info->value);
  if(b->tmpl) getdecls_tmpl(a, b->tmpl);
}

ANN static void getdecls_class_def(GetDecls *a, const Class_Def b) {
  getdecls_type_def( a, &b->base);
  if(b->body) getdecls_ast(a, b->body);
}

ANN static void getdecls_trait_def(GetDecls *a, const Trait_Def b) {
  if(b->body) getdecls_ast(a, b->body);
}

ANN static void getdecls_enumvalue(GetDecls *a, const EnumValue *b) {
  getdecls_tag(a, &b->tag); // we should print info about that enum
  // gwint, set
}

ANN static void getdecls_enum_list(GetDecls *a, const EnumValueList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const EnumValue c = enumvaluelist_at(b, i);
    getdecls_enumvalue(a, &c);
  }
}

ANN static void getdecls_enum_def(GetDecls *a, const Enum_Def b) {
  getdecls_tag(a, &b->tag);
  if(b->type) getdecls_add(a, b->type->info->value);
  getdecls_enum_list(a, b->list);
}

ANN static void getdecls_union_def(GetDecls *a, const Union_Def b) {
  getdecls_tag(a, &b->tag);
  if(b->type)getdecls_add(a, b->type->info->value);
  getdecls_variable_list(a, b->l);
  if(b->tmpl) getdecls_tmpl(a, b->tmpl);
}

ANN static void getdecls_fptr_def(GetDecls *a, const Fptr_Def b) {
  getdecls_func_base(a, b->base);
}

ANN static void getdecls_extend_def(GetDecls *a, const Extend_Def b) {
  getdecls_type_decl(a, b->td);
  getdecls_id_list(a, b->traits);
}

ANN static void getdecls_prim_def(GetDecls *a, const Prim_Def b) {
  getdecls_tag(a, &b->tag);
}

DECL_SECTION_FUNC(getdecls, void, GetDecls*, const);
ANN static void getdecls_section(GetDecls *a, const Section *b) {
  getdecls_section_func[b->section_type](a, *(void**)&b->d);
}

ANN static void getdecls_ast(GetDecls *a, const Ast b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Section c = sectionlist_at(b, i);
    getdecls_section(a, &c);
  }
}

ANN ValueList *get_value_all(MemPool mp, const Ast b) {
  GetDecls a = {
    .mp = mp,
    .values = new_valuelist(mp, 0),
  };
  getdecls_ast(&a, b);
  return a.values;
}
