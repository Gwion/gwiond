#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "vm.h"
#include "gwion.h"
#include "lsp.h"
#include "feat/get_value.h"

#define CK(a)       \
do {                \
  if((a)) return a; \
} while (0)

ANN static bool getvalue_exp(GetValue *a, const Exp* b);
ANN static bool getvalue_stmt(GetValue *a, const Stmt* b);
ANN static bool getvalue_stmt_list(GetValue *a, const StmtList *b);
ANN static bool getvalue_func_def(GetValue *a, const Func_Def b);
ANN static bool getvalue_section(GetValue *a, const Section *b);

static bool filematch(GetValue *a) {
  return !strcmp(a->filename.source, a->filename.target);
}

static bool before(const pos_t find, const pos_t last) {
  return find.line < last.line ||
         (find.line == last.line &&
         find.column <= last.column);
}

ANN static bool inside(GetValue *a, const loc_t loc) {
  return (loc.first.line < a->pos.line ||
         (loc.first.line == a->pos.line &&
          loc.first.column <= a->pos.column)) && 
         (a->pos.line < loc.last.line ||
         (a->pos.line == loc.last.line &&
          a->pos.column <= loc.last.column));
}

ANN static bool getvalue_symbol(GetValue *a, const Symbol b) {
  return false;
}

ANN static bool getvalue_tag(GetValue *a, const Tag *b) {
  if(b->sym) CK(getvalue_symbol(a, b->sym));
  return false;
}

ANN2(1, 2) static bool getvalue_tag2(GetValue *a, const Tag *b, const Value v) {
  if(filematch(a) && inside(a, b->loc)) {
    a->value = v;
    return true;
  }
  return false;
}

ANN2(1, 2) static bool getvalue_type_decl(GetValue *a, const Type_Decl *b, const Type t);

ANN static bool getvalue_array_sub(GetValue *a, const Array_Sub b) {
  if(b->exp) CK(getvalue_exp(a, b->exp));
  return false;
}

ANN static bool getvalue_id_list(GetValue *a, const TagList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Tag c = taglist_at(b, i);
    CK(getvalue_tag(a, &c));
  }
  return false;
}

ANN static bool getvalue_specialized(GetValue *a, const Specialized *b) {
  if(inside(a, b->tag.loc))
    return true;
//    return mstrdup(a->gwion->mp, "template argument definition");
  if(b->traits) CK(getvalue_id_list(a, b->traits));
  return false;
}

ANN static bool getvalue_specialized_list(GetValue *a, const SpecializedList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Specialized c = specializedlist_at(b, i);
    CK(getvalue_specialized(a, &c));
  }
  return false;
}

ANN static bool getvalue_tmplarg(GetValue *a, const TmplArg *b) {
  if (b->type == tmplarg_td) return getvalue_type_decl(a, b->d.td, false);
  return getvalue_exp(a, b->d.exp);
}

ANN static bool getvalue_tmplarg_list(GetValue *a, const TmplArgList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const TmplArg c = tmplarglist_at(b, i);
    CK(getvalue_tmplarg(a, &c));
  }
  return false;
}

ANN static bool getvalue_tmpl(GetValue *a, const Tmpl *b) {
  if(b->list) CK(getvalue_specialized_list(a, b->list));
  if(b->call) CK(getvalue_tmplarg_list(a, b->call)); 
  return false;
}

ANN static bool getvalue_range(GetValue *a, const Range *b) {
  if(b->start) CK(getvalue_exp(a, b->start));
  if(b->end) CK(getvalue_exp(a , b->end));
  return false;
}

ANN2(1, 2) static bool getvalue_type_decl(GetValue *a, const Type_Decl *b, const Type t) {
  CK(getvalue_tag2(a, &b->tag, t ? t->info->value : false));
  if(b->array) CK(getvalue_array_sub(a, b->array));
  if(b->types) CK(getvalue_tmplarg_list(a, b->types));
  return false;
}

ANN static bool getvalue_prim_id(GetValue *a, const Symbol *b) {
  if(!filematch(a) || !inside(a, prim_pos(b)))
    return false;
  a->value = prim_self(b)->value;
  return true;
}

ANN static bool getvalue_prim_num(GetValue *a, const m_uint *b) {
  // check pos
  return false;
}

ANN static bool getvalue_prim_float(GetValue *a, const m_float *b) {
  // check pos
  return false;
}

ANN static bool getvalue_prim_str(GetValue *a, const m_str *b) {
  // check pos
  return false;
}

ANN static bool getvalue_prim_array(GetValue *a, const Array_Sub *b) {
  return getvalue_array_sub(a, *b);
}

ANN static bool getvalue_prim_range(GetValue *a, const Range* *b) {
  return getvalue_range(a, *b);
}

ANN static bool getvalue_prim_dict(GetValue *a, const Exp* *b) {
  return getvalue_exp(a, *b);
}

ANN static bool getvalue_prim_hack(GetValue *a, const Exp* *b) {
  return getvalue_exp(a, *b);
}

ANN static bool getvalue_prim_interp(GetValue *a, const Exp* *b) {
  return getvalue_exp(a, *b);
}

ANN static bool getvalue_prim_char(GetValue *a, const m_str *b) {
  return false;
}

ANN static bool getvalue_prim_nil(GetValue *a, const bool *b) {
  return false;
}

ANN static bool getvalue_prim_perform(GetValue *a, const Symbol *b) {
  // TODO: effect_info
  return getvalue_symbol(a, *b);
}

ANN static bool getvalue_prim_locale(GetValue *a, const Symbol *b) {
  // TODO: locale info (would require locale tracking)
  return getvalue_symbol(a, *b);
}

DECL_PRIM_FUNC(getvalue, bool, GetValue *, const);
ANN static bool getvalue_prim(GetValue *a, const Exp_Primary *b) {
  return getvalue_prim_func[b->prim_type](a, &b->d);
}

ANN static bool getvalue_var_decl(GetValue *a, const Var_Decl *b) {
  return getvalue_tag2(a, &b->tag, b->value);
}

ANN static bool getvalue_variable(GetValue *a, const Variable *b) {
  if(b->td) CK(getvalue_type_decl(a, b->td, b->vd.value ? b->vd.value->type : false));
  if(filematch(a) && inside(a, b->vd.tag.loc)) {
    a->value = b->vd.value;
    return true;
  }
  return false;
}

ANN static bool getvalue_exp_decl(GetValue *a, const Exp_Decl *b) {
  return getvalue_variable(a, &b->var);
}

ANN static bool getvalue_exp_binary(GetValue *a, const Exp_Binary *b) {
  CK(getvalue_exp(a, b->lhs));
  CK(getvalue_exp(a, b->rhs));
  return getvalue_symbol(a, b->op); // TODO: op_info
}

ANN static bool getvalue_capture(GetValue *a, const Capture *b) {
  return getvalue_tag2(a, &b->var.tag, b->var.value);
}

ANN static bool getvalue_captures(GetValue *a, const CaptureList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Capture c = capturelist_at(b, i);
 //   if(before(a->pos, c->var.tag.loc.last))
 //     continue;
    CK(getvalue_capture(a, &c));
  }
  return false;
}

ANN static bool getvalue_exp_unary(GetValue *a, const Exp_Unary *b) {
  CK(getvalue_symbol(a, b->op)); // TODO: op_info
  const enum unary_type type = b->unary_type;
  if(type == unary_exp) CK(getvalue_exp(a, b->exp));
  else if(type == unary_td) CK(getvalue_type_decl(a, b->ctor.td, exp_self(b)->type));
  else {
    CK(getvalue_stmt_list(a, b->code));
    if(b->captures) CK(getvalue_captures(a, b->captures));
  }
  return false;
}

ANN static bool getvalue_exp_cast(GetValue *a, const Exp_Cast *b) {
  CK(getvalue_type_decl(a, b->td, exp_self(b)->type));
  return getvalue_exp(a, b->exp);
}

ANN static bool getvalue_exp_post(GetValue *a, const Exp_Postfix *b) {
  CK(getvalue_symbol(a, b->op)); // TODO: op_info
  return getvalue_exp(a, b->exp);
}

ANN static bool getvalue_exp_call(GetValue *a, const Exp_Call *b) {
  CK(getvalue_exp(a, b->func));
  if(b->func->type && a->value) {
    a->value = b->func->type->info->value;
    return true;
  }
  if(b->args) CK(getvalue_exp(a, b->args));
  if(b->tmpl) CK(getvalue_tmpl(a, b->tmpl));
  return !!a->value;
//  return false;
}

ANN static bool getvalue_exp_array(GetValue *a, const Exp_Array *b) {
  CK(getvalue_exp(a, b->base));
  return getvalue_array_sub(a, b->array);
}

ANN static bool getvalue_exp_slice(GetValue *a, const Exp_Slice *b) {
  CK(getvalue_exp(a, b->base));
  return getvalue_range(a, b->range);
}

ANN static bool getvalue_exp_if(GetValue *a, const Exp_If *b) {
  CK(getvalue_exp(a, b->cond));
  if(b->if_exp) CK(getvalue_exp(a, b->if_exp));
  return getvalue_exp(a, b->else_exp);
}

ANN static bool getvalue_exp_dot(GetValue *a, const Exp_Dot *b) {
  CK(getvalue_exp(a, b->base));
  CK(getvalue_var_decl(a, &b->var));
//  a->value = b->var.value;
  return false;
}

ANN static bool getvalue_exp_lambda(GetValue *a, const Exp_Lambda *b) {
  return getvalue_func_def(a, b->def);
}

ANN static bool getvalue_exp_td(GetValue *a, const Type_Decl *b) {
  return getvalue_type_decl(a, b, exp_self(b)->type);
}

ANN static bool getvalue_exp_named(GetValue *a, const Exp_Named *b) {
  return getvalue_exp(a, b->exp);
}


DECL_EXP_FUNC(getvalue, bool, GetValue*, const);
ANN static bool getvalue_exp(GetValue *a, const Exp* b) {
  if(inside(a, b->loc))
    CK(getvalue_exp_func[b->exp_type](a, &b->d));
  if(b->next) CK(getvalue_exp(a, b->next));
  return false;
}

ANN static bool getvalue_stmt_exp(GetValue *a, const Stmt_Exp b) {
  if(b->val) return getvalue_exp(a,  b->val);
  return false;
}

ANN static bool getvalue_stmt_while(GetValue *a, const Stmt_Flow b) {
  CK(getvalue_exp(a, b->cond));
  return getvalue_stmt(a, b->body);
}

ANN static bool getvalue_stmt_until(GetValue *a, const Stmt_Flow b) {
  CK(getvalue_exp(a, b->cond));
  return getvalue_stmt(a, b->body);
}

ANN static bool getvalue_stmt_for(GetValue *a, const Stmt_For b) {
  CK(getvalue_stmt(a, b->c1));
  if(b->c2) CK(getvalue_stmt(a, b->c2));
  if(b->c3) CK(getvalue_exp(a, b->c3));
  return getvalue_stmt(a, b->body);
}

ANN static bool getvalue_stmt_each(GetValue *a, const Stmt_Each b) {
  if(b->idx.tag.sym) CK(getvalue_var_decl(a, &b->idx));
  CK(getvalue_var_decl(a, &b->var));
  CK(getvalue_exp(a, b->exp));
  return getvalue_stmt(a, b->body);
}

ANN static bool getvalue_stmt_loop(GetValue *a, const Stmt_Loop b) {
  if(b->idx.tag.sym) CK(getvalue_var_decl(a, &b->idx));
  CK(getvalue_exp(a,  b->cond));
  return getvalue_stmt(a, b->body);
}

ANN static bool getvalue_stmt_if(GetValue *a, const Stmt_If b) {
  CK(getvalue_exp(a,  b->cond));
  CK(getvalue_stmt(a, b->if_body));
  if(b->else_body) return getvalue_stmt(a, b->else_body);
  return false;
}

ANN static bool getvalue_stmt_code(GetValue *a, const Stmt_Code b) {
  if(b->stmt_list) return getvalue_stmt_list(a, b->stmt_list);
  return false;
}

ANN static bool getvalue_stmt_index(GetValue *a, const Stmt_Index b) {
  return false;
}

ANN static bool getvalue_stmt_break(GetValue *a, const Stmt_Index b) {
  return getvalue_stmt_index(a, b);
}

ANN static bool getvalue_stmt_continue(GetValue *a, const Stmt_Index b) {
  return getvalue_stmt_index(a, b);
}

ANN static bool getvalue_stmt_return(GetValue *a, const Stmt_Exp b) {
  if(b->val) return getvalue_exp(a, b-> val);
  return false;
}

ANN static bool getvalue_stmt_case(GetValue *a, const struct Match *b) {
  CK(getvalue_exp(a, b->cond));
  CK(getvalue_stmt_list(a, b->list));
  if(b->when) return getvalue_exp(a, b->when);
  return false;
}

ANN static bool getvalue_case_list(GetValue *a, const StmtList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Stmt c = stmtlist_at(b, i);
  //  if(before(a->pos, c->loc.last))
  //    continue;
    CK(getvalue_stmt_case(a, &c.d.stmt_match));
  }
  return false;
}

ANN static bool getvalue_stmt_match(GetValue *a, const Stmt_Match b) {
  CK(getvalue_exp(a, b->cond));
  CK(getvalue_case_list(a, b->list));
  if(b->where) return getvalue_stmt(a, b->where);
  return false;
}

ANN static bool getvalue_stmt_pp(GetValue *a, const Stmt_PP b) {
  if(b->pp_type == ae_pp_include)
    a->filename.source = b->data;
  return false;
}

ANN static bool getvalue_stmt_retry(GetValue *a, const Stmt_Exp b) {
  return false;
}

ANN static bool getvalue_handler(GetValue *a, const Handler *b) {
  CK(getvalue_tag(a, &b->tag));  // should return handler or effect info
  return getvalue_stmt(a, b->stmt);
}

ANN static bool getvalue_handler_list(GetValue *a, const HandlerList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Handler handler = handlerlist_at(b, i);
  //  if(before(a->pos, handler->tag.loc.last))
  //    continue;
    CK(getvalue_handler(a, &handler));
  }
  return false;
}

ANN static bool getvalue_stmt_try(GetValue *a, const Stmt_Try b) {
  CK(getvalue_stmt(a, b->stmt));
  return getvalue_handler_list(a, b->handler);
}

ANN static bool getvalue_stmt_defer(GetValue *a, const Stmt_Defer b) {
  return getvalue_stmt(a, b->stmt);
}

ANN static bool getvalue_stmt_spread(GetValue *a, const Spread_Def b) {
  CK(getvalue_tag(a, &b->tag)); // should print info about spread tag
  return getvalue_id_list(a, b->list); // should print info about spread list
}

ANN static bool getvalue_stmt_using(GetValue *a, const UsingStmt *b) {
  CK(getvalue_type_decl(a, b->d.td, known_type(a->gwion->env, b->d.td)));
  if(b->tag.sym)
    return getvalue_exp(a, b->d.exp);
  const Type type = known_type(a->gwion->env, b->d.td);
  if(type)
    return getvalue_type_decl(a, b->d.td, type);
  return false;
}

ANN static bool getvalue_stmt_import(GetValue *a, const Stmt_Import b) {
  CK(getvalue_tag(a, &b->tag));
  if(b->selection) {
    for(uint32_t i = 0; i < b->selection->len; i++) {
      const UsingStmt using = usingstmtlist_at(b->selection, i);
      CK(getvalue_stmt_using(a, &using));
    }
  }
  return false;
}


DECL_STMT_FUNC(getvalue, bool, GetValue*, const);
ANN static bool getvalue_stmt(GetValue *a, const Stmt* b) {
  return getvalue_stmt_func[b->stmt_type](a, &b->d);
}

ANN static bool getvalue_arg(GetValue *a, const Arg *b) {
  return getvalue_variable(a, &b->var);
}

ANN static bool getvalue_arg_list(GetValue *a, const ArgList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Arg c = arglist_at(b, i);
    CK(getvalue_arg(a, &c));
  }
  return false;
}

ANN static bool getvalue_variable_list(GetValue *a, const VariableList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Variable c = variablelist_at(b, i);
    // TODO: check if td or vd
    CK(getvalue_variable(a, &c));
  }
  return false;
}

ANN static bool getvalue_stmt_list(GetValue *a, const StmtList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Stmt c = stmtlist_at(b, i);
    if(before(c.loc.last, a->pos))
      continue;
    CK(getvalue_stmt(a, &c));
  }
  return false;
}

ANN static bool getvalue_func_base(GetValue *a, const Func_Base *b) {
  if(b->td) CK(getvalue_type_decl(a, b->td, b->ret_type));
  CK(getvalue_tag2(a, &b->tag, b->func ? b->func->value_ref : false));
  if(b->tmpl) CK(getvalue_tmpl(a, b->tmpl));
  if(b->args) CK(getvalue_arg_list(a, b->args));
  return false;
}

ANN static bool getvalue_func_def(GetValue *a, const Func_Def b) {
  CK(getvalue_func_base(a, b->base));
  if(b->d.code) CK(getvalue_stmt_list(a, b->d.code));
  if(b->captures) return getvalue_captures(a, b->captures);
  return false;
}

ANN static bool getvalue_type_def(GetValue *a, const Type_Def b) {
  if(b->ext) CK(getvalue_type_decl(a, b->ext, b->type ? b->type->info->parent : false));
  CK(getvalue_tag2(a, &b->tag, b->type ? b->type->info->value : false));
  if(b->tmpl) return getvalue_tmpl(a, b->tmpl);
  return false;
}

ANN static bool getvalue_class_def(GetValue *a, const Class_Def b) {
  CK(getvalue_type_def( a, &b->base));
  if(b->body) return getvalue_ast(a, b->body);
  return false;
}

ANN static bool getvalue_trait_def(GetValue *a, const Trait_Def b) {
  if(b->body) return getvalue_ast(a, b->body);
  return false;
}

ANN static bool getvalue_enumvalue(GetValue *a, const EnumValue *b) {
  return getvalue_tag(a, &b->tag); // we should print info about that enum
  // gwint, set
}

ANN static bool getvalue_enum_list(GetValue *a, const EnumValueList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const EnumValue c = enumvaluelist_at(b, i);
//    if(c->set) {
//      if(!before(c.tag.loc.last))       
//          }
//    if(before(c->tag.loc.last, a->pos))
//      continue;
    CK(getvalue_enumvalue(a, &c));
  }
  return false;
}

ANN static bool getvalue_enum_def(GetValue *a, const Enum_Def b) {
  CK(getvalue_tag2(a, &b->tag, b->type ? b->type->info->value : false));
  return getvalue_enum_list(a, b->list);
}

ANN static bool getvalue_union_def(GetValue *a, const Union_Def b) {
  CK(getvalue_tag2(a, &b->tag, b->type ? b->type->info->value : false));
  CK(getvalue_variable_list(a, b->l));
  if(b->tmpl) CK(getvalue_tmpl(a, b->tmpl));
  return false;
}

ANN static bool getvalue_fptr_def(GetValue *a, const Fptr_Def b) {
  return getvalue_func_base(a, b->base);
}

ANN static bool getvalue_extend_def(GetValue *a, const Extend_Def b) {
  CK(getvalue_type_decl(a, b->td, b->type));
  return getvalue_id_list(a, b->traits);
}

ANN static bool getvalue_prim_def(GetValue *a, const Prim_Def b) {
  return getvalue_tag(a, &b->tag);
}

DECL_SECTION_FUNC(getvalue, bool, GetValue*, const);
ANN static bool getvalue_section(GetValue *a, const Section *b) {
  return getvalue_section_func[b->section_type](a, *(void**)&b->d);
}

ANN bool getvalue_ast(GetValue *a, const Ast b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Section c = sectionlist_at(b, i);
    if(filematch(a) && inside(a, c.loc))
      CK(getvalue_section(a, &c));
  }
  return !!a->value;
}
