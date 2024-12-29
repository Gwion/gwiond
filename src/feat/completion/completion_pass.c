#include "defs.h"
#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include <stdbool.h>
#include "complete.h"

#define CHECK_RET(a, ret) \
do {                      \
  if(!(a))                \
    ret = false;           \
} while(0) 


ANN static bool is_after(Complete *a, const pos_t pos) {
    return a->line > pos.line ||
      (a->line == pos.line && a->character >= pos.column);
}
ANN static bool is_before(Complete *a, const pos_t pos) {
    return a->line < pos.line ||
      (a->line == pos.line && a->character <= pos.column);
}
ANN static bool is_inside(Complete *a, const loc_t loc) {
  return is_after(a, loc.first) && is_before(a, loc.last); 
}

ANN static bool complete_type_decl(Complete *a, const Type_Decl* b);
ANN static bool complete_func_def(Complete *a, const Func_Def b);

ANN static bool complete_tag(Complete *a, const Tag *b) {
  return true;
}

ANN static bool complete_array_sub(Complete *a, const Array_Sub b) {
  return b->exp ? complete_exp(a, b->exp) : true;
}

ANN static bool complete_specialized(Complete *a, const Specialized *b) {
  bool ret = true;
  CHECK_RET(complete_tag(a, &b->tag), ret);
  return ret;
}

ANN static bool complete_specialized_list(Complete *a, const SpecializedList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Specialized c = specializedlist_at(b, i);
    CHECK_RET(complete_specialized(a, &c), ret);
  }
  return ret;
}

ANN static bool complete_tmplarg(Complete *a, const TmplArg *b) {
  bool ret = true;
  if (b->type == tmplarg_td) CHECK_RET(complete_type_decl(a, b->d.td), ret);
  else CHECK_RET(complete_exp(a, b->d.exp), ret);
  return ret;
}

ANN static bool complete_tmplarg_list(Complete *a, const TmplArgList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const TmplArg c = tmplarglist_at(b, i);
    CHECK_RET(complete_tmplarg(a, &c), ret);
  }
  return ret;
}

ANN static bool complete_tmpl(Complete *a, const Tmpl *b) {
  bool ret = true;
  if(b->list) CHECK_RET(complete_specialized_list(a, b->list), ret);
  if(b->call) CHECK_RET(complete_tmplarg_list(a, b->call), ret);
  return ret;
}

ANN static bool complete_range(Complete *a, const Range *b) {
  bool ret = true;
  if(b->start) CHECK_RET(complete_exp(a, b->start), ret);
  if(b->end) CHECK_RET(complete_exp(a , b->end), ret);
  return ret;
}

ANN static bool complete_type_decl(Complete *a, const Type_Decl *b) {
  bool ret = true;
  CHECK_RET(complete_tag(a, &b->tag), ret);
  if(b->array) CHECK_RET(complete_array_sub(a, b->array), ret);
  if(b->types) CHECK_RET(complete_tmplarg_list(a, b->types), ret);
  return ret;
}


ANN static bool complete_prim_id(Complete *a, const Symbol *b) {
  return true;
}

ANN static bool complete_prim_num(Complete *a NUSED, const m_uint *b NUSED) {
  return true;
}

ANN static bool complete_prim_float(Complete *a NUSED, const m_float *b NUSED) {
  return true;
}

ANN static bool complete_prim_str(Complete *a NUSED, const m_str *b NUSED) {
  return true;
}

ANN static bool complete_prim_array(Complete *a, const Array_Sub *b) {
  return complete_array_sub(a, *b);
}

ANN static bool complete_prim_range(Complete *a, const Range* *b) {
  return complete_range(a, *b);
}

ANN static bool complete_prim_dict(Complete *a, const Exp* *b) {
  return complete_exp(a, *b);
}

ANN static bool complete_prim_hack(Complete *a, const Exp* *b) {
  return complete_exp(a, *b);
}

ANN static bool complete_prim_interp(Complete *a, const Exp* *b) {
  return complete_exp(a, *b);
}

ANN static bool complete_prim_char(Complete *a NUSED, const m_str *b NUSED) {
  return true;
}

ANN static bool complete_prim_nil(Complete *a NUSED, const bool *b NUSED) {
  return true;
}

ANN static bool complete_prim_perform(Complete *a, const Symbol *b) {
  return true;
}

ANN static bool complete_prim_locale(Complete *a, const Symbol *b) {
  return true;
}

DECL_PRIM_FUNC(complete, bool, Complete *, const)
ANN static bool complete_prim(Complete *a, const Exp_Primary *b) {
  return complete_prim_func[b->prim_type](a, &b->d);
}

ANN static bool complete_var_decl(Complete *a, const Var_Decl *b) {
  if(a->scope && b->value)
    valuelist_add(a->mp, &a->values, b->value);
  return true;
}

ANN static bool complete_variable(Complete *a, const Variable *b) {
  bool ret = true;
  if(b->td) CHECK_RET(complete_type_decl(a, b->td), ret);
  CHECK_RET(complete_var_decl(a, &b->vd), ret);
  return ret;
}
#include <gwion_env.h>
#include <lsp.h>
ANN static bool complete_exp_decl(Complete *a, const Exp_Decl *b) {
  bool ret = true;
  CHECK_RET(complete_variable(a, &b->var), ret);
  if(b->args) CHECK_RET(complete_exp(a, b->args), ret);
  return ret;
}

ANN static bool complete_exp_binary(Complete *a, const Exp_Binary *b) {
  bool ret = true;
  CHECK_RET(complete_exp(a, b->lhs), ret);
  CHECK_RET(complete_exp(a, b->rhs), ret);
  return ret;
}

ANN static bool complete_capture(Complete *a, const Capture *b) {
  return complete_var_decl(a, &b->var);
}

ANN static bool complete_captures(Complete *a, const CaptureList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Capture c = capturelist_at(b, i);
    CHECK_RET(complete_capture(a, &c), ret);
  }
  return ret;
}

ANN static bool complete_exp_unary(Complete *a, const Exp_Unary *b) {
  bool ret = true;
  const enum unary_type type = b->unary_type;
  if(type == unary_exp) CHECK_RET(complete_exp(a, b->exp), ret);
  else if(type == unary_td) CHECK_RET(complete_type_decl(a, b->ctor.td), ret);
  else {
    CHECK_RET(complete_stmt_list(a, b->code), ret);
    if(b->captures)CHECK_RET(complete_captures(a, b->captures), ret);
  }
  return ret;
}

ANN static bool complete_exp_cast(Complete *a, const Exp_Cast *b) {
  bool ret = true;
  CHECK_RET(complete_type_decl(a, b->td), ret);
  CHECK_RET(complete_exp(a, b->exp), ret);
  return ret;
}

ANN static bool complete_exp_post(Complete *a, const Exp_Postfix *b) {
  bool ret = true;
  CHECK_RET(complete_exp(a, b->exp), ret);
  return ret;
}

ANN static bool complete_exp_call(Complete *a, const Exp_Call *b) {
  bool ret = true;
  CHECK_RET(complete_exp(a, b->func), ret);
  if(b->args) CHECK_RET(complete_exp(a, b->args), ret);
  if(b->tmpl) CHECK_RET(complete_tmpl(a, b->tmpl), ret);
  return ret;
}

ANN static bool complete_exp_array(Complete *a, const Exp_Array *b) {
  bool ret = true;
  CHECK_RET(complete_exp(a, b->base), ret);
  CHECK_RET(complete_array_sub(a, b->array), ret);
  return ret;
}

ANN static bool complete_exp_slice(Complete *a, const Exp_Slice *b) {
  bool ret = true;
  CHECK_RET(complete_exp(a, b->base), ret);
  CHECK_RET(complete_range(a, b->range), ret);
  return ret;
}

ANN static bool complete_exp_if(Complete *a, const Exp_If *b) {
  bool ret = true;
  CHECK_RET(complete_exp(a, b->cond), ret);
  if(b->if_exp) CHECK_RET(complete_exp(a, b->if_exp), ret);
  CHECK_RET(complete_exp(a, b->else_exp), ret);
  return ret;
}

ANN static bool complete_exp_dot(Complete *a, const Exp_Dot *b) {
  bool ret = true;
  CHECK_RET(complete_exp(a, b->base), ret);
  return ret;
}

ANN static bool complete_exp_lambda(Complete *a, const Exp_Lambda *b) {
  // we need do check if the wanted location is inside
  // also maybe refactor so that captures are in expression and not func def?
  return complete_func_def(a, b->def);
}

ANN static bool complete_exp_td(Complete *a, const Type_Decl *b) {
  return complete_type_decl(a, b);
}

ANN static bool complete_exp_named(Complete *a, const Exp_Named *b) {
  return complete_exp(a, b->exp);
}

DECL_EXP_FUNC(complete, bool, Complete*, const)
ANN static bool complete_exp(Complete *a, const Exp* b) {
  a->last = (Exp*)b; // TODO: find a prettier way
  bool ret = complete_exp_func[b->exp_type](a, &b->d);
  CHECK_B(is_before(a, b->loc.last));
  if(b->next) CHECK_RET(complete_exp(a, b ->next), ret);
  return ret;
}

ANN static bool complete_stmt_exp(Complete *a, const Stmt_Exp b) {
  return b->val ? complete_exp(a,  b->val) : true;
}

ANN static bool complete_stmt_while(Complete *a, const Stmt_Flow b) {
  bool ret = true;
  a->scope++;
  CHECK_RET(complete_exp(a, b->cond), ret);
  CHECK_RET(complete_stmt(a, b->body), ret);
  a->scope--;
  return ret;
}

ANN static bool complete_stmt_until(Complete *a, const Stmt_Flow b) {
  bool ret = true;
  a->scope++;
  CHECK_RET(complete_exp(a, b->cond), ret);
  CHECK_RET(complete_stmt(a, b->body), ret);
  a->scope--;
  return ret;
}

ANN static bool complete_stmt_for(Complete *a, const Stmt_For b) {
  bool ret = true;
  a->scope++;
  CHECK_RET(complete_stmt(a, b->c1), ret);
  if(b->c2) CHECK_RET(complete_stmt(a, b->c2), ret);
  if(b->c3) CHECK_RET(complete_exp(a, b->c3), ret);
  CHECK_RET(complete_stmt(a, b->body), ret);
  a->scope--;
  return ret;
}

ANN static bool complete_stmt_each(Complete *a, const Stmt_Each b) {
  bool ret = true;
  if(b->idx.tag.sym) CHECK_RET(complete_var_decl(a, &b->idx), ret);
  CHECK_RET(complete_var_decl(a, &b->var), ret);
  CHECK_RET(complete_exp(a, b->exp), ret);
  CHECK_RET(complete_stmt(a, b->body), ret);
  return ret;
}

ANN static bool complete_stmt_loop(Complete *a, const Stmt_Loop b) {
  bool ret = true;
  a->scope++;
  if(b->idx.tag.sym) CHECK_RET(complete_var_decl(a, &b->idx), ret);
  CHECK_RET(complete_exp(a,  b->cond), ret);
  CHECK_RET(complete_stmt(a, b->body), ret);
  a->scope--;
  return ret;
}

ANN static bool complete_stmt_if(Complete *a, const Stmt_If b) {
  bool ret = true;
  a->scope++;
  CHECK_RET(complete_exp(a,  b->cond), ret);
  CHECK_RET(complete_stmt(a, b->if_body), ret);
  if(b->else_body) complete_stmt(a, b->else_body);
  a->scope--;
  return ret;
}

ANN static bool complete_stmt_code(Complete *a, const Stmt_Code b) {
  if(!b->stmt_list) return true;
  if(!is_inside(a, stmt_self(b)->loc)) return true;
  a->scope++;
  const bool ret = complete_stmt_list(a, b->stmt_list);
  a->scope--;
  return ret;
}

#define complete_stmt_break dummy_func
#define complete_stmt_continue dummy_func

ANN static bool complete_stmt_return(Complete *a, const Stmt_Exp b) {
  return b->val ? complete_exp(a, b-> val) : true;
}

ANN static bool complete_stmt_case(Complete *a, const struct Match *b) {
  bool ret = true;
  CHECK_RET(complete_exp(a, b->cond), ret);
  CHECK_RET(complete_stmt_list(a, b->list), ret);
  if(b->when) CHECK_RET(complete_exp(a, b->when), ret);
  return ret;
}

ANN static bool complete_case_list(Complete *a, const StmtList *b) {
  bool ret = true;
  a->scope++;
  for(uint32_t i = 0; i < b->len; i++) {
    const Stmt c = stmtlist_at(b, i);
    CHECK_RET(complete_stmt_case(a, &c.d.stmt_match), ret);
  }
  a->scope--;
  return ret;
}

ANN static bool complete_stmt_match(Complete *a, const Stmt_Match b) {
  bool ret = true;
  a->scope++;
  complete_exp(a, b->cond);
  complete_case_list(a, b->list);
  if(b->where) complete_stmt(a, b->where);
  a->scope--;
  return ret;
}

#define complete_stmt_pp dummy_func
#define complete_stmt_retry dummy_func

ANN static bool complete_handler(Complete *a, const const Handler *b) {
  bool ret = true;
  a->scope++;
  CHECK_RET(complete_tag(a, &b->tag), ret);
  CHECK_RET(complete_stmt(a, b->stmt), ret);
  a->scope--;
  return ret;
}

ANN static bool complete_handler_list(Complete *a, const HandlerList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Handler handler = handlerlist_at(b, i);
    CHECK_RET(complete_handler(a, &handler), ret);
  }
  return ret;
}

ANN static bool complete_stmt_try(Complete *a, const Stmt_Try b) {
  bool ret = true;
  a->scope++;
  CHECK_RET(complete_stmt(a, b->stmt), ret);
  CHECK_RET(complete_handler_list(a, b->handler), ret);
  a->scope--;
  return ret;
}

ANN static bool complete_stmt_defer(Complete *a, const Stmt_Defer b) {
  a->scope++;
  const bool ret = complete_stmt(a, b->stmt);
  a->scope--;
  return ret;
}

// NOTE: not used, make dummy
ANN static bool complete_stmt_spread(Complete *a, const Spread_Def b) {
  bool ret = true;
  CHECK_RET(complete_tag(a, &b->tag), ret);
  return ret;
}

ANN static bool complete_stmt_using(Complete *a, const Stmt_Using b) {
  bool ret = true;
  a->scope++;
  if(b->tag.sym) {
    CHECK_RET(complete_tag(a, &b->tag), ret);
    CHECK_RET(complete_exp(a, b->d.exp), ret);
  } else
    CHECK_RET(complete_type_decl(a, b->d.td), ret);
  a->scope--;
  return ret;
}

ANN static bool complete_stmt_import(Complete *a, const Stmt_Import b) {
  bool ret = true;
  CHECK_RET(complete_tag(a, &b->tag), ret);
  if(b->selection) {
    for(uint32_t i = 0; i < b->selection->len; i++) {
      const UsingStmt using = usingstmtlist_at(b->selection, i);
      CHECK_RET(complete_tag(a, &using.tag), ret);
      if(using.tag.sym)
        CHECK_RET(complete_exp(a, using.d.exp), ret);
      else
        CHECK_RET(complete_type_decl(a, using.d.td), ret);
    }
  }
  return ret;
}

DECL_STMT_FUNC(complete, bool, Complete*, const)
ANN static bool complete_stmt(Complete *a, const Stmt* b) {
  return complete_stmt_func[b->stmt_type](a, &b->d);
}

ANN static bool complete_arg(Complete *a, const Arg *b) {
  return complete_variable(a, &b->var);
}

ANN static bool complete_arg_list(Complete *a, const ArgList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Arg c = arglist_at(b, i);
    CHECK_RET(complete_arg(a, &c), ret);
  }
  return ret;
}

ANN static bool complete_variable_list(Complete *a, const VariableList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Variable c = variablelist_at(b, i);
    CHECK_RET(complete_variable(a, &c), ret);
  }
  return ret;
}

ANN static bool complete_stmt_list(Complete *a, const StmtList *b) {
  // TODO: only parse if target is between first and last statement
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Stmt c = stmtlist_at(b, i);
    CHECK_RET(complete_stmt(a, &c), ret);
  }
  return ret;
}

ANN static bool complete_func_base(Complete *a, const Func_Base *b) {
  bool ret = true;
  if(b->td) CHECK_RET(complete_type_decl(a, b->td), ret);
  CHECK_RET(complete_tag(a, &b->tag), ret);
  if(b->args) CHECK_RET(complete_arg_list(a, b->args), ret);
  if(b->tmpl) CHECK_RET(complete_tmpl(a, b->tmpl), ret);
  return ret;
}

ANN static bool complete_func_def(Complete *a, const Func_Def b) {
  a->last = NULL;
  bool ret = true;
  a->scope++;
  CHECK_RET(complete_func_base(a, b->base), ret);
  if(b->d.code) CHECK_RET(complete_stmt_list(a, b->d.code), ret);
  if(b->captures) CHECK_RET(complete_captures(a, b->captures), ret);
  a->scope--;
  return ret;
}

ANN static bool complete_type_def(Complete *a, const Type_Def b) {
  bool ret = true;
  if(b->ext) CHECK_RET(complete_type_decl(a, b->ext), ret);
  CHECK_RET(complete_tag(a, &b->tag), ret);
  if(b->tmpl) CHECK_RET(complete_tmpl(a, b->tmpl), ret);
  return ret;
}

ANN static bool complete_class_def(Complete *a, const Class_Def b) {
  const Class_Def former = a->class_def;
  a->class_def = b;
  a->last = NULL;
  bool ret = true;
  CHECK_RET(complete_type_def( a, &b->base), ret);
  if(b->body) CHECK_B(complete_ast(a, b->body));
  a->class_def = former;
  return ret;
}

ANN static bool complete_trait_def(Complete *a, const Trait_Def b) {
  return b->body ? complete_ast(a, b->body) : true;
}

ANN static bool complete_enumvalue(Complete *a, const EnumValue *b) {
  return complete_tag(a, &b->tag);
}

ANN static bool complete_enum_list(Complete *a, const EnumValueList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const EnumValue c = enumvaluelist_at(b, i);
    CHECK_RET(complete_enumvalue(a, &c), ret);
  }
  return ret;
}

ANN static bool complete_enum_def(Complete *a, const Enum_Def b) {
  bool ret = true;
  CHECK_RET(complete_enum_list(a, b->list), ret);
  CHECK_RET(complete_tag(a, &b->tag), ret);
  return ret;
}

ANN static bool complete_union_def(Complete *a, const Union_Def b) {
  bool ret = true;
  CHECK_RET(complete_variable_list(a, b->l), ret);
  CHECK_RET(complete_tag(a, &b->tag), ret);
  if(b->tmpl) CHECK_RET(complete_tmpl(a, b->tmpl), ret);
  return ret;
}

ANN static bool complete_fptr_def(Complete *a, const Fptr_Def b) {
  return complete_func_base(a, b->base);
}

ANN static bool complete_extend_def(Complete *a, const Extend_Def b) {
  bool ret = true;
  CHECK_RET(complete_type_decl(a, b->td), ret);
  return ret;
}

ANN static bool complete_prim_def(Complete *a, const Prim_Def b) {
  return complete_tag(a, &b->tag);
}

DECL_SECTION_FUNC(complete, bool, Complete*, const)

ANN static bool complete_section(Complete *a, const Section *b) {
  if(is_inside(a, b->loc)) {
    CHECK_B(complete_section_func[b->section_type](a, *(void**)&b->d));
    if(a->line < b->loc.last.line)
      return false;
  }
  return true;
}

ANN bool complete_ast(Complete *a, const Ast b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Section c = sectionlist_at(b, i);
    Class_Def former = a->class_def;
    if(a->line < c.loc.first.line && !former) {
      return false;
    }
    if(c.section_type == ae_section_class)
      a->class_def = c.d.class_def;
    if(!complete_section(a, &c)) // can't use macro???? 
      return false;
    if(c.section_type == ae_section_class)
      a->class_def = former;
  }
  return true;
}
