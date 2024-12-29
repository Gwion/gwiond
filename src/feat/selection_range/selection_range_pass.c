#include "defs.h"
#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "feat/selectionrange.h"

#define CHECK_RET(a) \
do {                 \
  if(!(a))           \
    return false;    \
} while(0) 


ANN bool selectionrange_ast(SelectionRange *a, const Ast b);

ANN static bool selectionrange_exp(SelectionRange *a, const Exp* b);
ANN static bool selectionrange_stmt(SelectionRange *a, const Stmt* b);
ANN static bool selectionrange_stmt_list(SelectionRange *a, const StmtList* b);
ANN static bool selectionrange_type_decl(SelectionRange *a, const Type_Decl* b);
ANN static bool selectionrange_func_def(SelectionRange *a, const Func_Def b);

ANN static bool selectionrange_symbol(SelectionRange *a NUSED, const Symbol b NUSED) {
  return true;
}

ANN static bool selectionrange_tag(SelectionRange *a, const Tag *b) {
  a->inside(a, b->loc);
  return b->sym ? selectionrange_symbol(a, b->sym) : true;
}

ANN static bool selectionrange_array_sub(SelectionRange *a, const Array_Sub b) {
  return b->exp ? selectionrange_exp(a, b->exp) : true;
}

ANN static bool selectionrange_taglist(SelectionRange *a, const TagList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Tag c = taglist_at(b, i);
    if(a->inside(a, c.loc))
      CHECK_RET(selectionrange_tag(a, &c));
  }
  return true;
}

ANN static bool selectionrange_specialized(SelectionRange *a, const Specialized *b) {
  CHECK_RET(selectionrange_tag(a, &b->tag));
  if(b->traits) CHECK_RET(selectionrange_taglist(a, b->traits));
  return true;
}

ANN static bool selectionrange_specialized_list(SelectionRange *a, const SpecializedList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Specialized c = specializedlist_at(b, i);
    if(a->inside(a, c.tag.loc))
      CHECK_RET(selectionrange_specialized(a, &c));
  }
  return true;
}

ANN static bool selectionrange_tmplarg(SelectionRange *a, const TmplArg *b) {
  if (b->type == tmplarg_td) CHECK_RET(selectionrange_type_decl(a, b->d.td));
  else CHECK_RET(selectionrange_exp(a, b->d.exp));
  return true;
}

ANN static bool selectionrange_tmplarg_list(SelectionRange *a, const TmplArgList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const TmplArg c = tmplarglist_at(b, i);
//    if(a->inside(a, c.loc)) // move test in selectionrange_tmplarg
    CHECK_RET(selectionrange_tmplarg(a, &c));
  }
  return true;
}

ANN static bool selectionrange_tmpl(SelectionRange *a, const Tmpl *b) {
  if(b->list) CHECK_RET(selectionrange_specialized_list(a, b->list));
  if(b->call) CHECK_RET(selectionrange_tmplarg_list(a, b->call));
  return true;
}

ANN static bool selectionrange_range(SelectionRange *a, const Range *b) {
  if(b->start) CHECK_RET(selectionrange_exp(a, b->start));
  if(b->end) CHECK_RET(selectionrange_exp(a , b->end));
  return true;
}

ANN static bool selectionrange_type_decl(SelectionRange *a, const Type_Decl *b) {
  CHECK_RET(selectionrange_tag(a, &b->tag));
  if(b->array) CHECK_RET(selectionrange_array_sub(a, b->array));
  if(b->types) CHECK_RET(selectionrange_tmplarg_list(a, b->types));
  return true;
}

ANN static bool selectionrange_prim_id(SelectionRange *a, const Symbol *b) {
  return selectionrange_symbol(a, *b);
}

ANN static bool selectionrange_prim_num(SelectionRange *a NUSED, const m_uint *b NUSED) {
  return true;
}

ANN static bool selectionrange_prim_float(SelectionRange *a NUSED, const m_float *b NUSED) {
  return true;
}

ANN static bool selectionrange_prim_str(SelectionRange *a NUSED, const m_str *b NUSED) {
  return true;
}

ANN static bool selectionrange_prim_array(SelectionRange *a, const Array_Sub *b) {
  return selectionrange_array_sub(a, *b);
}

ANN static bool selectionrange_prim_range(SelectionRange *a, const Range* *b) {
  return selectionrange_range(a, *b);
}

ANN static bool selectionrange_prim_dict(SelectionRange *a, const Exp* *b) {
  return selectionrange_exp(a, *b);
}

ANN static bool selectionrange_prim_hack(SelectionRange *a, const Exp* *b) {
  return selectionrange_exp(a, *b);
}

ANN static bool selectionrange_prim_interp(SelectionRange *a, const Exp* *b) {
  return selectionrange_exp(a, *b);
}

ANN static bool selectionrange_prim_char(SelectionRange *a NUSED, const m_str *b NUSED) {
  return true;
}

ANN static bool selectionrange_prim_nil(SelectionRange *a NUSED, const bool *b NUSED) {
  return true;
}

ANN static bool selectionrange_prim_perform(SelectionRange *a, const Symbol *b) {
  return selectionrange_symbol(a, *b);
}

ANN static bool selectionrange_prim_locale(SelectionRange *a, const Symbol *b) {
  return selectionrange_symbol(a, *b);
}

DECL_PRIM_FUNC(selectionrange, bool, SelectionRange *, const)
ANN static bool selectionrange_prim(SelectionRange *a, const Exp_Primary *b) {
  return selectionrange_prim_func[b->prim_type](a, &b->d);
}

ANN static bool selectionrange_var_decl(SelectionRange *a, const Var_Decl *b) {
  return selectionrange_tag(a, &b->tag);
}

ANN static bool selectionrange_variable(SelectionRange *a, const Variable *b) {
  if(b->td) CHECK_RET(selectionrange_type_decl(a, b->td));
  CHECK_RET(selectionrange_var_decl(a, &b->vd));
  return true;
}

ANN static bool selectionrange_exp_decl(SelectionRange *a, const Exp_Decl *b) {
  if(b->args) CHECK_RET(selectionrange_exp(a, b->args));
  CHECK_RET(selectionrange_variable(a, &b->var));
  return true;
}

ANN static bool selectionrange_exp_binary(SelectionRange *a, const Exp_Binary *b) {
  CHECK_RET(selectionrange_exp(a, b->lhs));
  CHECK_RET(selectionrange_exp(a, b->rhs));
  CHECK_RET(selectionrange_symbol(a, b->op));
  return true;
}

ANN static bool selectionrange_capture(SelectionRange *a, const Capture *b) {
  return selectionrange_tag(a, &b->var.tag);
}

ANN static bool selectionrange_captures(SelectionRange *a, const CaptureList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Capture c = capturelist_at(b, i);
    if(a->inside(a, c.var.tag.loc))
      CHECK_RET(selectionrange_capture(a, &c));
  }
  return true;
}

ANN static bool selectionrange_exp_unary(SelectionRange *a, const Exp_Unary *b) {
  CHECK_RET(selectionrange_symbol(a, b->op));
  const enum unary_type type = b->unary_type;
  if(type == unary_exp) CHECK_RET(selectionrange_exp(a, b->exp));
  else if(type == unary_td) CHECK_RET(selectionrange_type_decl(a, b->ctor.td));
  else {
    CHECK_RET(selectionrange_stmt_list(a, b->code));
    if(b->captures)CHECK_RET(selectionrange_captures(a, b->captures));
  }
  return true;
}

ANN static bool selectionrange_exp_cast(SelectionRange *a, const Exp_Cast *b) {
  CHECK_RET(selectionrange_type_decl(a, b->td));
  CHECK_RET(selectionrange_exp(a, b->exp));
  return true;
}

ANN static bool selectionrange_exp_post(SelectionRange *a, const Exp_Postfix *b) {
  CHECK_RET(selectionrange_symbol(a, b->op));
  CHECK_RET(selectionrange_exp(a, b->exp));
  return true;
}

ANN static bool selectionrange_exp_call(SelectionRange *a, const Exp_Call *b) {
  CHECK_RET(selectionrange_exp(a, b->func));
  if(b->args) CHECK_RET(selectionrange_exp(a, b->args));
  if(b->tmpl) CHECK_RET(selectionrange_tmpl(a, b->tmpl));
  return true;
}

ANN static bool selectionrange_exp_array(SelectionRange *a, const Exp_Array *b) {
  CHECK_RET(selectionrange_exp(a, b->base));
  CHECK_RET(selectionrange_array_sub(a, b->array));
  return true;
}

ANN static bool selectionrange_exp_slice(SelectionRange *a, const Exp_Slice *b) {
  CHECK_RET(selectionrange_exp(a, b->base));
  CHECK_RET(selectionrange_range(a, b->range));
  return true;
}

ANN static bool selectionrange_exp_if(SelectionRange *a, const Exp_If *b) {
  CHECK_RET(selectionrange_exp(a, b->cond));
  if(b->if_exp) CHECK_RET(selectionrange_exp(a, b->if_exp));
  CHECK_RET(selectionrange_exp(a, b->else_exp));
  return true;
}

ANN static bool selectionrange_exp_dot(SelectionRange *a, const Exp_Dot *b) {
  CHECK_RET(selectionrange_exp(a, b->base));
  CHECK_RET(selectionrange_var_decl(a, &b->var));
  return true;
}

ANN static bool selectionrange_exp_lambda(SelectionRange *a, const Exp_Lambda *b) {
  return selectionrange_func_def(a, b->def);
}

ANN static bool selectionrange_exp_td(SelectionRange *a, const Type_Decl *b) {
  return selectionrange_type_decl(a, b);
}

ANN static bool selectionrange_exp_named(SelectionRange *a, const Exp_Named *b) {
  return selectionrange_exp(a, b->exp);
}

DECL_EXP_FUNC(selectionrange, bool, SelectionRange*, const)
ANN static bool selectionrange_exp(SelectionRange *a, const Exp* b) {
  a->inside(a, b->loc);
  CHECK_RET(selectionrange_exp_func[b->exp_type](a, &b->d));
  if(b->next) CHECK_RET(selectionrange_exp(a, b ->next));
  return true;
}

ANN static bool selectionrange_stmt_exp(SelectionRange *a, const Stmt_Exp b) {
  return b->val ? selectionrange_exp(a,  b->val) : true;
}

ANN static bool selectionrange_stmt_while(SelectionRange *a, const Stmt_Flow b) {
  CHECK_RET(selectionrange_exp(a, b->cond));
  CHECK_RET(selectionrange_stmt(a, b->body));
  return true;
}

ANN static bool selectionrange_stmt_until(SelectionRange *a, const Stmt_Flow b) {
  CHECK_RET(selectionrange_exp(a, b->cond));
  CHECK_RET(selectionrange_stmt(a, b->body));
  return true;
}

ANN static bool selectionrange_stmt_for(SelectionRange *a, const Stmt_For b) {
  CHECK_RET(selectionrange_stmt(a, b->c1));
  if(b->c2) CHECK_RET(selectionrange_stmt(a, b->c2));
  if(b->c3) CHECK_RET(selectionrange_exp(a, b->c3));
  CHECK_RET(selectionrange_stmt(a, b->body));
  return true;
}

ANN static bool selectionrange_stmt_each(SelectionRange *a, const Stmt_Each b) {
  if(b->idx.tag.sym) CHECK_RET(selectionrange_var_decl(a, &b->idx));
  CHECK_RET(selectionrange_var_decl(a, &b->var));
  CHECK_RET(selectionrange_exp(a, b->exp));
  CHECK_RET(selectionrange_stmt(a, b->body));
  return true;
}

ANN static bool selectionrange_stmt_loop(SelectionRange *a, const Stmt_Loop b) {
  if(b->idx.tag.sym) CHECK_RET(selectionrange_var_decl(a, &b->idx));
  CHECK_RET(selectionrange_exp(a,  b->cond));
  CHECK_RET(selectionrange_stmt(a, b->body));
  return true;
}

ANN static bool selectionrange_stmt_if(SelectionRange *a, const Stmt_If b) {
  CHECK_RET(selectionrange_exp(a,  b->cond));
  CHECK_RET(selectionrange_stmt(a, b->if_body));
  if(b->else_body) selectionrange_stmt(a, b->else_body);
  return true;
}

ANN static bool selectionrange_stmt_code(SelectionRange *a, const Stmt_Code b) {
  return b->stmt_list ? selectionrange_stmt_list(a, b->stmt_list) : true;
}

ANN static bool selectionrange_stmt_index(SelectionRange *a NUSED, const Stmt_Index b NUSED) {
  return true;
}

ANN static bool selectionrange_stmt_break(SelectionRange *a, const Stmt_Index b) {
  return selectionrange_stmt_index(a, b);
}

ANN static bool selectionrange_stmt_continue(SelectionRange *a, const Stmt_Index b) {
  return selectionrange_stmt_index(a, b);
}

ANN static bool selectionrange_stmt_return(SelectionRange *a, const Stmt_Exp b) {
  return b->val ? selectionrange_exp(a, b-> val) : true;
}

ANN static bool selectionrange_stmt_case(SelectionRange *a, const struct Match *b) {
  CHECK_RET(selectionrange_exp(a, b->cond));
  CHECK_RET(selectionrange_stmt_list(a, b->list));
  if(b->when) CHECK_RET(selectionrange_exp(a, b->when));
  return true;
}

ANN static bool selectionrange_case_list(SelectionRange *a, const StmtList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Stmt c = stmtlist_at(b, i);
    if(a->inside(a, c.loc))
      CHECK_RET(selectionrange_stmt_case(a, &c.d.stmt_match));
  }
  return true;
}

ANN static bool selectionrange_stmt_match(SelectionRange *a, const Stmt_Match b) {
  selectionrange_exp(a, b->cond);
  selectionrange_case_list(a, b->list);
  if(b->where) selectionrange_stmt(a, b->where);
  return true;
}

ANN static bool selectionrange_stmt_pp(SelectionRange *a NUSED, const Stmt_PP b NUSED) {
  return true;
}

ANN static bool selectionrange_stmt_retry(SelectionRange *a NUSED, const Stmt_Exp b NUSED) {
  return true;
}

ANN static bool selectionrange_handler(SelectionRange *a, const Handler *b) {
  CHECK_RET(selectionrange_tag(a, &b->tag));
  CHECK_RET(selectionrange_stmt(a, b->stmt));
  return true;
}

ANN static bool selectionrange_handler_list(SelectionRange *a, const HandlerList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Handler c = handlerlist_at(b, i);
    if(a->inside(a, c.tag.loc))
      CHECK_RET(selectionrange_handler(a, &c));
  }
  return true;
}

ANN static bool selectionrange_stmt_try(SelectionRange *a, const Stmt_Try b) {
  CHECK_RET(selectionrange_stmt(a, b->stmt));
  CHECK_RET(selectionrange_handler_list(a, b->handler));
  return true;
}

ANN static bool selectionrange_stmt_defer(SelectionRange *a, const Stmt_Defer b) {
  return selectionrange_stmt(a, b->stmt);
}

ANN static bool selectionrange_stmt_spread(SelectionRange *a, const Spread_Def b) {
  CHECK_RET(selectionrange_tag(a, &b->tag));
  CHECK_RET(selectionrange_taglist(a, b->list));
  return true;
}

ANN static bool selectionrange_stmt_using(SelectionRange *a, const Stmt_Using b) {
  if(b->tag.sym) {
    CHECK_RET(selectionrange_tag(a, &b->tag));
    CHECK_RET(selectionrange_exp(a, b->d.exp));
  } else
    CHECK_RET(selectionrange_type_decl(a, b->d.td));
  return true;
}

ANN static bool selectionrange_stmt_import(SelectionRange *a, const Stmt_Import b) {
  CHECK_RET(selectionrange_tag(a, &b->tag));
  if(b->selection) {
    for(uint32_t i = 0; i < b->selection->len; i++) {
      const UsingStmt c = usingstmtlist_at(b->selection, i);
      if(a->inside(a, c.tag.loc)) {
        CHECK_RET(selectionrange_tag(a, &c.tag));
      }
      if(c.tag.sym && a->inside(a, c.d.exp->loc))
        CHECK_RET(selectionrange_exp(a, c.d.exp));
      else if(a->inside(a, c.d.td->tag.loc))
        CHECK_RET(selectionrange_type_decl(a, c.d.td));
    }
  }
  return true;
}

DECL_STMT_FUNC(selectionrange, bool, SelectionRange*, const)
ANN static bool selectionrange_stmt(SelectionRange *a, const Stmt* b) {
  return selectionrange_stmt_func[b->stmt_type](a, &b->d);
}

ANN static bool selectionrange_arg(SelectionRange *a, const Arg *b) {
  return selectionrange_variable(a, &b->var);
}

ANN static bool selectionrange_arg_list(SelectionRange *a, const ArgList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Arg c = arglist_at(b, i);
    // move test
    CHECK_RET(selectionrange_arg(a, &c));
  }
  return true;
}

ANN static bool selectionrange_variablelist(SelectionRange *a, const VariableList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Variable c = variablelist_at(b, i);
    // move test
    CHECK_RET(selectionrange_variable(a, &c));
  }
  return true;
}

ANN static bool selectionrange_stmt_list(SelectionRange *a, const StmtList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Stmt c = stmtlist_at(b, i);
    if(a->inside(a, c.loc))
      CHECK_RET(selectionrange_stmt(a, &c));
  }
  return true;
}

ANN static bool selectionrange_func_base(SelectionRange *a, const Func_Base *b) {
  if(b->td) CHECK_RET(selectionrange_type_decl(a, b->td));
  CHECK_RET(selectionrange_tag(a, &b->tag));
  if(b->args) CHECK_RET(selectionrange_arg_list(a, b->args));
  if(b->tmpl) CHECK_RET(selectionrange_tmpl(a, b->tmpl));
  return true;
}

ANN static bool selectionrange_func_def(SelectionRange *a, const Func_Def b) {
  CHECK_RET(selectionrange_func_base(a, b->base));
  if(b->d.code) CHECK_RET(selectionrange_stmt_list(a, b->d.code));
  if(b->captures) CHECK_RET(selectionrange_captures(a, b->captures));
  return true;
}

ANN static bool selectionrange_type_def(SelectionRange *a, const Type_Def b) {
  if(b->ext) CHECK_RET(selectionrange_type_decl(a, b->ext));
  CHECK_RET(selectionrange_tag(a, &b->tag));
  if(b->tmpl) CHECK_RET(selectionrange_tmpl(a, b->tmpl));
  return true;
}

ANN static bool selectionrange_class_def(SelectionRange *a, const Class_Def b) {
  CHECK_RET(selectionrange_type_def( a, &b->base));
  if(b->body) CHECK_RET(selectionrange_ast(a, b->body));
  return true;
}

ANN static bool selectionrange_trait_def(SelectionRange *a, const Trait_Def b) {
  return b->body ? selectionrange_ast(a, b->body) : true;
}

ANN static bool selectionrange_enumvalue(SelectionRange *a, const EnumValue *b) {
  return selectionrange_tag(a, &b->tag);
}

ANN static bool selectionrange_enum_list(SelectionRange *a, const EnumValueList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const EnumValue c = enumvaluelist_at(b, i);
    // move test
    CHECK_RET(selectionrange_enumvalue(a, &c));
  }
  return true;
}

ANN static bool selectionrange_enum_def(SelectionRange *a, const Enum_Def b) {
  CHECK_RET(selectionrange_enum_list(a, b->list));
  CHECK_RET(selectionrange_tag(a, &b->tag));
  return true;
}

ANN static bool selectionrange_union_def(SelectionRange *a, const Union_Def b) {
  CHECK_RET(selectionrange_variablelist(a, b->l));
  CHECK_RET(selectionrange_tag(a, &b->tag));
  if(b->tmpl) CHECK_RET(selectionrange_tmpl(a, b->tmpl));
  return true;
}

ANN static bool selectionrange_fptr_def(SelectionRange *a, const Fptr_Def b) {
  return selectionrange_func_base(a, b->base);
}

ANN static bool selectionrange_extend_def(SelectionRange *a, const Extend_Def b) {
  CHECK_RET(selectionrange_type_decl(a, b->td));
  CHECK_RET(selectionrange_taglist(a, b->traits));
  return true;
}

ANN static bool selectionrange_prim_def(SelectionRange *a, const Prim_Def b) {
  return selectionrange_tag(a, &b->tag);
}

DECL_SECTION_FUNC(selectionrange, bool, SelectionRange*, const)
ANN static bool selectionrange_section(SelectionRange *a, const Section *b) {
  return selectionrange_section_func[b->section_type](a, *(void**)&b->d);
}

ANN bool selectionrange_ast(SelectionRange *a, const Ast b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Section c = sectionlist_at(b, i);
    if(a->inside(a, c.loc))
      CHECK_RET(selectionrange_section(a, &c));
  }
  return true;
}
