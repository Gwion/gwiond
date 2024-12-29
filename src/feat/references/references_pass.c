#include "defs.h"
#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "references_pass.h"

#define CHECK_RET(a, ret) \
do {                      \
  if(!(a))                \
    ret = false;          \
} while(0) 



ANN static void add_ref(References *a, const loc_t loc) {
  Reference ref = {
    .loc = loc,
    .uri = a->filename,
  };
  referencelist_add(a->mp, &a->list, ref);
}

ANN static bool references_type_decl(References *a, const Type_Decl* b);
ANN static bool references_func_def(References *a, const Func_Def b);

ANN static bool references_symbol(References *a NUSED, const Symbol b NUSED) {
  return true;
}

ANN static bool references_tag(References *a, const Tag *b) {
  return b->sym ? references_symbol(a, b->sym) : true;
}

ANN static bool references_array_sub(References *a, const Array_Sub b) {
  return b->exp ? references_exp(a, b->exp) : true;
}

ANN static bool references_taglist(References *a, const TagList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Tag c = taglist_at(b, i);
    CHECK_RET(references_tag(a, &c), ret);
  }
  return ret;
}

ANN static bool references_specialized(References *a, const Specialized *b) {
  bool ret = true;
  CHECK_RET(references_tag(a, &b->tag), ret);
  if(b->traits) CHECK_RET(references_taglist(a, b->traits), ret);
  return ret;
}

ANN static bool references_specialized_list(References *a, const SpecializedList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Specialized c = specializedlist_at(b, i);
    CHECK_RET(references_specialized(a, &c), ret);
  }
  return ret;
}

ANN static bool references_tmplarg(References *a, const TmplArg *b) {
  bool ret = true;
  if (b->type == tmplarg_td) CHECK_RET(references_type_decl(a, b->d.td), ret);
  else CHECK_RET(references_exp(a, b->d.exp), ret);
  return ret;
}

ANN static bool references_tmplarg_list(References *a, const TmplArgList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const TmplArg c = tmplarglist_at(b, i);
    CHECK_RET(references_tmplarg(a, &c), ret);
  }
  return ret;
}

ANN static bool references_tmpl(References *a, const Tmpl *b) {
  bool ret = true;
  if(b->list) CHECK_RET(references_specialized_list(a, b->list), ret);
  if(b->call) CHECK_RET(references_tmplarg_list(a, b->call), ret);
  return ret;
}

ANN static bool references_range(References *a, const Range *b) {
  bool ret = true;
  if(b->start) CHECK_RET(references_exp(a, b->start), ret);
  if(b->end) CHECK_RET(references_exp(a , b->end), ret);
  return ret;
}

ANN static bool references_type_decl(References *a, const Type_Decl *b) {
  bool ret = true;
  CHECK_RET(references_tag(a, &b->tag), ret);
  if(b->array) CHECK_RET(references_array_sub(a, b->array), ret);
  if(b->types) CHECK_RET(references_tmplarg_list(a, b->types), ret);
  return ret;
}

ANN static bool references_prim_id(References *a, const Symbol *b) {
  return references_symbol(a, *b);
}

ANN static bool references_prim_num(References *a NUSED, const m_uint *b NUSED) {
  return true;
}

ANN static bool references_prim_float(References *a NUSED, const m_float *b NUSED) {
  return true;
}

ANN static bool references_prim_str(References *a NUSED, const m_str *b NUSED) {
  return true;
}

ANN static bool references_prim_array(References *a, const Array_Sub *b) {
  return references_array_sub(a, *b);
}

ANN static bool references_prim_range(References *a, const Range* *b) {
  return references_range(a, *b);
}

ANN static bool references_prim_dict(References *a, const Exp* *b) {
  return references_exp(a, *b);
}

ANN static bool references_prim_hack(References *a, const Exp* *b) {
  return references_exp(a, *b);
}

ANN static bool references_prim_interp(References *a, const Exp* *b) {
  return references_exp(a, *b);
}

ANN static bool references_prim_char(References *a NUSED, const m_str *b NUSED) {
  return true;
}

ANN static bool references_prim_nil(References *a NUSED, const bool *b NUSED) {
  return true;
}

ANN static bool references_prim_perform(References *a, const Symbol *b) {
  return references_symbol(a, *b);
}

ANN static bool references_prim_locale(References *a, const Symbol *b) {
  return references_symbol(a, *b);
}

DECL_PRIM_FUNC(references, bool, References *, const)
ANN static bool references_prim(References *a, const Exp_Primary *b) {
  if(b->value == a->value)
    add_ref(a, exp_self(b)->loc);
  return references_prim_func[b->prim_type](a, &b->d);
}

ANN static bool references_var_decl(References *a, const Var_Decl *b) {
  if(b->value == a->value)
    add_ref(a, b->tag.loc);
  return references_tag(a, &b->tag);
}

ANN static bool references_variable(References *a, const Variable *b) {
  bool ret = true;
  if(b->td) CHECK_RET(references_type_decl(a, b->td), ret);
  CHECK_RET(references_var_decl(a, &b->vd), ret);
  return ret;
}

ANN static bool references_exp_decl(References *a, const Exp_Decl *b) {
  bool ret = true;
  if(b->args) CHECK_RET(references_exp(a, b->args), ret);
  CHECK_RET(references_variable(a, &b->var), ret);
  return ret;
}

ANN static bool references_exp_binary(References *a, const Exp_Binary *b) {
  bool ret = true;
  CHECK_RET(references_exp(a, b->lhs), ret);
  CHECK_RET(references_exp(a, b->rhs), ret);
  CHECK_RET(references_symbol(a, b->op), ret);
  return ret;
}

ANN static bool references_capture(References *a, const Capture *b) {
  return references_var_decl(a, &b->var);
}

ANN static bool references_captures(References *a, const CaptureList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Capture c = capturelist_at(b, i);
    CHECK_RET(references_capture(a, &c), ret);
  }
  return ret;
}

ANN static bool references_exp_unary(References *a, const Exp_Unary *b) {
  bool ret = true;
  CHECK_RET(references_symbol(a, b->op), ret);
  const enum unary_type type = b->unary_type;
  if(type == unary_exp) CHECK_RET(references_exp(a, b->exp), ret);
  else if(type == unary_td) CHECK_RET(references_type_decl(a, b->ctor.td), ret);
  else {
    CHECK_RET(references_stmt_list(a, b->code), ret);
    if(b->captures)CHECK_RET(references_captures(a, b->captures), ret);
  }
  return ret;
}

ANN static bool references_exp_cast(References *a, const Exp_Cast *b) {
  bool ret = true;
  CHECK_RET(references_type_decl(a, b->td), ret);
  CHECK_RET(references_exp(a, b->exp), ret);
  return ret;
}

ANN static bool references_exp_post(References *a, const Exp_Postfix *b) {
  bool ret = true;
  CHECK_RET(references_symbol(a, b->op), ret);
  CHECK_RET(references_exp(a, b->exp), ret);
  return ret;
}

ANN static bool references_exp_call(References *a, const Exp_Call *b) {
  bool ret = true;
  CHECK_RET(references_exp(a, b->func), ret);
  if(b->args) CHECK_RET(references_exp(a, b->args), ret);
  if(b->tmpl) CHECK_RET(references_tmpl(a, b->tmpl), ret);
  return ret;
}

ANN static bool references_exp_array(References *a, const Exp_Array *b) {
  bool ret = true;
  CHECK_RET(references_exp(a, b->base), ret);
  CHECK_RET(references_array_sub(a, b->array), ret);
  return ret;
}

ANN static bool references_exp_slice(References *a, const Exp_Slice *b) {
  bool ret = true;
  CHECK_RET(references_exp(a, b->base), ret);
  CHECK_RET(references_range(a, b->range), ret);
  return ret;
}

ANN static bool references_exp_if(References *a, const Exp_If *b) {
  bool ret = true;
  CHECK_RET(references_exp(a, b->cond), ret);
  if(b->if_exp) CHECK_RET(references_exp(a, b->if_exp), ret);
  CHECK_RET(references_exp(a, b->else_exp), ret);
  return ret;
}

ANN static bool references_exp_dot(References *a, const Exp_Dot *b) {
  bool ret = true;
  CHECK_RET(references_exp(a, b->base), ret);
  CHECK_RET(references_var_decl(a, &b->var), ret);
  return ret;
}

ANN static bool references_exp_lambda(References *a, const Exp_Lambda *b) {
  return references_func_def(a, b->def);
}

ANN static bool references_exp_td(References *a, const Type_Decl *b) {
  return references_type_decl(a, b);
}

ANN static bool references_exp_named(References *a, const Exp_Named *b) {
  return references_exp(a, b->exp);
}

DECL_EXP_FUNC(references, bool, References*, const)
ANN static bool references_exp(References *a, const Exp* b) {
  bool ret = references_exp_func[b->exp_type](a, &b->d);
  if(b->next) CHECK_RET(references_exp(a, b ->next), ret);
  return ret;
}

ANN static bool references_stmt_exp(References *a, const Stmt_Exp b) {
  return b->val ? references_exp(a,  b->val) : true;
}

ANN static bool references_stmt_while(References *a, const Stmt_Flow b) {
  bool ret = true;
  CHECK_RET(references_exp(a, b->cond), ret);
  CHECK_RET(references_stmt(a, b->body), ret);
  return ret;
}

ANN static bool references_stmt_until(References *a, const Stmt_Flow b) {
  bool ret = true;
  CHECK_RET(references_exp(a, b->cond), ret);
  CHECK_RET(references_stmt(a, b->body), ret);
  return ret;
}

ANN static bool references_stmt_for(References *a, const Stmt_For b) {
  bool ret = true;
  CHECK_RET(references_stmt(a, b->c1), ret);
  if(b->c2) CHECK_RET(references_stmt(a, b->c2), ret);
  if(b->c3) CHECK_RET(references_exp(a, b->c3), ret);
  CHECK_RET(references_stmt(a, b->body), ret);
  return ret;
}

ANN static bool references_stmt_each(References *a, const Stmt_Each b) {
  bool ret = true;
  if(b->idx.tag.sym) CHECK_RET(references_var_decl(a, &b->idx), ret);
  CHECK_RET(references_var_decl(a, &b->var), ret);
  CHECK_RET(references_exp(a, b->exp), ret);
  CHECK_RET(references_stmt(a, b->body), ret);
  return ret;
}

ANN static bool references_stmt_loop(References *a, const Stmt_Loop b) {
  bool ret = true;
  if(b->idx.tag.sym) CHECK_RET(references_var_decl(a, &b->idx), ret);
  CHECK_RET(references_exp(a,  b->cond), ret);
  CHECK_RET(references_stmt(a, b->body), ret);
  return ret;
}

ANN static bool references_stmt_if(References *a, const Stmt_If b) {
  bool ret = true;
  CHECK_RET(references_exp(a,  b->cond), ret);
  CHECK_RET(references_stmt(a, b->if_body), ret);
  if(b->else_body) references_stmt(a, b->else_body);
  return ret;
}

ANN static bool references_stmt_code(References *a, const Stmt_Code b) {
  return b->stmt_list ? references_stmt_list(a, b->stmt_list) : true;
}

ANN static bool references_stmt_index(References *a NUSED, const Stmt_Index b NUSED) {
  return true;
}

ANN static bool references_stmt_break(References *a, const Stmt_Index b) {
  return references_stmt_index(a, b);
}

ANN static bool references_stmt_continue(References *a, const Stmt_Index b) {
  return references_stmt_index(a, b);
}

ANN static bool references_stmt_return(References *a, const Stmt_Exp b) {
  return b->val ? references_exp(a, b-> val) : true;
}

ANN static bool references_stmt_case(References *a, const struct Match *b) {
  bool ret = true;
  CHECK_RET(references_exp(a, b->cond), ret);
  CHECK_RET(references_stmt_list(a, b->list), ret);
  if(b->when) CHECK_RET(references_exp(a, b->when), ret);
  return ret;
}

ANN static bool references_case_list(References *a, const StmtList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Stmt c = stmtlist_at(b, i);
    CHECK_RET(references_stmt_case(a, &c.d.stmt_match), ret);
  }
  return ret;
}

ANN static bool references_stmt_match(References *a, const Stmt_Match b) {
  bool ret = true;
  references_exp(a, b->cond);
  references_case_list(a, b->list);
  if(b->where) references_stmt(a, b->where);
  return ret;
}

ANN static bool references_stmt_pp(References *a NUSED, const Stmt_PP b NUSED) {
  return true;
}

ANN static bool references_stmt_retry(References *a NUSED, const Stmt_Exp b NUSED) {
  return true;
}

ANN static bool references_handler(References *a, const Handler *b) {
  bool ret = true;
  CHECK_RET(references_tag(a, &b->tag), ret);
  CHECK_RET(references_stmt(a, b->stmt), ret);
  return ret;
}

ANN static bool references_handler_list(References *a, const HandlerList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Handler c = handlerlist_at(b, i);
    CHECK_RET(references_handler(a, &c), ret);
  }
  return ret;
}

ANN static bool references_stmt_try(References *a, const Stmt_Try b) {
  bool ret = true;
  CHECK_RET(references_stmt(a, b->stmt), ret);
  CHECK_RET(references_handler_list(a, b->handler), ret);
  return ret;
}

ANN static bool references_stmt_defer(References *a, const Stmt_Defer b) {
  return references_stmt(a, b->stmt);
}

ANN static bool references_stmt_spread(References *a, const Spread_Def b) {
  bool ret = true;
  CHECK_RET(references_tag(a, &b->tag), ret);
  CHECK_RET(references_taglist(a, b->list), ret);
  return ret;
}

ANN static bool references_stmt_using(References *a, const Stmt_Using b) {
  bool ret = true;
  if(b->tag.sym) {
    CHECK_RET(references_tag(a, &b->tag), ret);
    CHECK_RET(references_exp(a, b->d.exp), ret);
  } else
    CHECK_RET(references_type_decl(a, b->d.td), ret);
  return ret;
}

ANN static bool references_stmt_import(References *a, const Stmt_Import b) {
  bool ret = true;
  CHECK_RET(references_tag(a, &b->tag), ret);
  if(b->selection) {
    for(uint32_t i = 0; i < b->selection->len; i++) {
      const UsingStmt c = usingstmtlist_at(b->selection, i);
      CHECK_RET(references_tag(a, &c.tag), ret);
      if(c.tag.sym)
        CHECK_RET(references_exp(a, c.d.exp), ret);
      else
        CHECK_RET(references_type_decl(a, c.d.td), ret);
    }
  }
  return ret;
}

DECL_STMT_FUNC(references, bool, References*, const)
ANN static bool references_stmt(References *a, const Stmt* b) {
  return references_stmt_func[b->stmt_type](a, &b->d);
}

ANN static bool references_arg(References *a, const Arg *b) {
  return references_variable(a, &b->var);
}

ANN static bool references_arg_list(References *a, const ArgList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Arg c = arglist_at(b, i);
    CHECK_RET(references_arg(a, &c), ret);
  }
  return ret;
}

ANN static bool references_variablelist(References *a, const VariableList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Variable c = variablelist_at(b, i);
    CHECK_RET(references_variable(a, &c), ret);
  }
  return ret;
}

ANN static bool references_stmt_list(References *a, const StmtList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Stmt c = stmtlist_at(b, i);
    CHECK_RET(references_stmt(a, &c), ret);
  }
  return ret;
}

ANN static bool references_func_base(References *a, const Func_Base *b) {
  bool ret = true;
  if(b->td) CHECK_RET(references_type_decl(a, b->td), ret);
  CHECK_RET(references_tag(a, &b->tag), ret);
  if(b->args) CHECK_RET(references_arg_list(a, b->args), ret);
  if(b->tmpl) CHECK_RET(references_tmpl(a, b->tmpl), ret);
  return ret;
}

ANN static bool references_func_def(References *a, const Func_Def b) {
  bool ret = true;
  CHECK_RET(references_func_base(a, b->base), ret);
  if(b->d.code) CHECK_RET(references_stmt_list(a, b->d.code), ret);
  if(b->captures) CHECK_RET(references_captures(a, b->captures), ret);
  return ret;
}

ANN static bool references_type_def(References *a, const Type_Def b) {
  bool ret = true;
  if(b->ext) CHECK_RET(references_type_decl(a, b->ext), ret);
  CHECK_RET(references_tag(a, &b->tag), ret);
  if(b->tmpl) CHECK_RET(references_tmpl(a, b->tmpl), ret);
  return ret;
}

ANN static bool references_class_def(References *a, const Class_Def b) {
  bool ret = true;
  CHECK_RET(references_type_def( a, &b->base), ret);
  if(b->body) CHECK_RET(references_ast(a, b->body), ret);
  return ret;
}

ANN static bool references_trait_def(References *a, const Trait_Def b) {
  return b->body ? references_ast(a, b->body) : true;
}

ANN static bool references_enumvalue(References *a, const EnumValue *b) {
  return references_tag(a, &b->tag);
}

ANN static bool references_enum_list(References *a, const EnumValueList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const EnumValue c = enumvaluelist_at(b, i);
    CHECK_RET(references_enumvalue(a, &c), ret);
  }
  return ret;
}

ANN static bool references_enum_def(References *a, const Enum_Def b) {
  bool ret = true;
  CHECK_RET(references_enum_list(a, b->list), ret);
  CHECK_RET(references_tag(a, &b->tag), ret);
  return ret;
}

ANN static bool references_union_def(References *a, const Union_Def b) {
  bool ret = true;
  CHECK_RET(references_variablelist(a, b->l), ret);
  CHECK_RET(references_tag(a, &b->tag), ret);
  if(b->tmpl) CHECK_RET(references_tmpl(a, b->tmpl), ret);
  return ret;
}

ANN static bool references_fptr_def(References *a, const Fptr_Def b) {
  return references_func_base(a, b->base);
}

ANN static bool references_extend_def(References *a, const Extend_Def b) {
  bool ret = true;
  CHECK_RET(references_type_decl(a, b->td), ret);
  CHECK_RET(references_taglist(a, b->traits), ret);
  return ret;
}

ANN static bool references_prim_def(References *a, const Prim_Def b) {
  return references_tag(a, &b->tag);
}

DECL_SECTION_FUNC(references, bool, References*, const)
ANN static bool references_section(References *a, const Section *b) {
  return references_section_func[b->section_type](a, *(void**)&b->d);
}

ANN bool references_ast(References *a, const Ast b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Section c = sectionlist_at(b, i);
    CHECK_RET(references_section(a, &c), ret);
  }
  return ret;
}
