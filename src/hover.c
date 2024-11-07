#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "vm.h"
#include "gwion.h"
#include "hover.h"
#include "hover_private.h"
#include <stdbool.h>

#define CK(a)       \
do {                \
  if((a)) return a; \
} while (0)

static bool filematch(Hover *a) {
  return !strcmp(a->filename.source, a->filename.target);
}

static bool before(const pos_t find, const pos_t last) {
  return find.line < last.line ||
         find.column < last.column;
}

ANN static bool inside(Hover *a, const loc_t loc) {
  return loc.first.line   <= a->pos.line    &&
         loc.first.column <= a->pos.column  &&
         a->pos.line      <= loc.last.line  &&
         a->pos.column    <= loc.last.column;
}

ANN static bool hover_symbol(Hover *a, Symbol b) {
  return false;
}

ANN static bool hover_tag(Hover *a, Tag *b) {
  if(b->sym) CK(hover_symbol(a, b->sym));
  return false;
}

ANN2(1, 2) static bool hover_tag2(Hover *a, Tag *b, const Value v) {
  if(filematch(a) && inside(a, b->loc)) {
    a->value = v;
    return true;
  }
  return false;
}


ANN static bool hover_array_sub(Hover *a, Array_Sub b) {
  if(b->exp) CK(hover_exp(a, b->exp));
  return false;
}

// TODO: switch from Symbol list to Tag list
// this way we may be able to remove string checking
ANN static bool hover_id_list(Hover *a, ID_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Symbol c = *mp_vector_at(b, Symbol, i);
    CK(hover_symbol(a, c));
  }
  return false;
}

ANN static bool hover_specialized(Hover *a, Specialized *b) {
  if(inside(a, b->tag.loc))
    return mstrdup(a->gwion->mp, "template argument definition");
  if(b->traits) CK(hover_id_list(a, b->traits));
  return false;
}

ANN static bool hover_specialized_list(Hover *a, Specialized_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Specialized * c = mp_vector_at(b, Specialized  , i);
    CK(hover_specialized(a, c));
  }
  return false;
}

ANN static bool hover_tmplarg(Hover *a, TmplArg *b) {
  if (b->type == tmplarg_td) return hover_type_decl(a, b->d.td, false);
  return hover_exp(a, b->d.exp);
}

ANN static bool hover_tmplarg_list(Hover *a, TmplArg_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    TmplArg * c = mp_vector_at(b, TmplArg, i);
    // TODO: check exp type
//    if(before(a->pos, c->d.exp
    CK(hover_tmplarg(a, c));
  }
  return false;
}

ANN static bool hover_tmpl(Hover *a, Tmpl *b) {
  if(b->list) CK(hover_specialized_list(a, b->list));
  if(b->call) CK(hover_tmplarg_list(a, b->call)); 
  return false;
}

ANN static bool hover_range(Hover *a, Range *b) {
  if(b->start) CK(hover_exp(a, b->start));
  if(b->end) CK(hover_exp(a , b->end));
  return false;
}

ANN2(1, 2) static bool hover_type_decl(Hover *a, Type_Decl *b, const Type t) {
  CK(hover_tag2(a, &b->tag, t ? t->info->value : false));
  if(b->array) CK(hover_array_sub(a, b->array));
  if(b->types) CK(hover_tmplarg_list(a, b->types));
  return false;
}

ANN static bool hover_prim_id(Hover *a, Symbol *b) {
  if(!filematch(a) || !inside(a, prim_pos(b)))
    return false;
  a->value = prim_self(b)->value;
  return true;
}

ANN static bool hover_prim_num(Hover *a, m_uint *b) {
  // check pos
  return false;
}

ANN static bool hover_prim_float(Hover *a, m_float *b) {
  // check pos
  return false;
}

ANN static bool hover_prim_str(Hover *a, m_str *b) {
  // check pos
  return false;
}

ANN static bool hover_prim_array(Hover *a, Array_Sub *b) {
  return hover_array_sub(a, *b);
}

ANN static bool hover_prim_range(Hover *a, Range* *b) {
  return hover_range(a, *b);
}

ANN static bool hover_prim_dict(Hover *a, Exp* *b) {
  return hover_exp(a, *b);
}

ANN static bool hover_prim_hack(Hover *a, Exp* *b) {
  return hover_exp(a, *b);
}

ANN static bool hover_prim_interp(Hover *a, Exp* *b) {
  return hover_exp(a, *b);
}

ANN static bool hover_prim_char(Hover *a, m_str *b) {
  return false;
}

ANN static bool hover_prim_nil(Hover *a, bool *b) {
  return false;
}

ANN static bool hover_prim_perform(Hover *a, Symbol *b) {
  // TODO: effect_info
  return hover_symbol(a, *b);
}

ANN static bool hover_prim_locale(Hover *a, Symbol *b) {
  // TODO: locale info (would require locale tracking)
  return hover_symbol(a, *b);
}

DECL_PRIM_FUNC(hover, bool, Hover *);
ANN static bool hover_prim(Hover *a, Exp_Primary *b) {
  return hover_prim_func[b->prim_type](a, &b->d);
}

ANN static bool hover_var_decl(Hover *a, Var_Decl *b) {
  return hover_tag2(a, &b->tag, b->value);
}

ANN static bool hover_variable(Hover *a, Variable *b) {
  if(b->td) CK(hover_type_decl(a, b->td, b->vd.value ? b->vd.value->type : false));
  if(filematch(a) && inside(a, b->vd.tag.loc)) {
    a->value = b->vd.value;
    return b->vd.value;
  }
  return false;
}

ANN static bool hover_exp_decl(Hover *a, Exp_Decl *b) {
  return hover_variable(a, &b->var);
}

ANN static bool hover_exp_binary(Hover *a, Exp_Binary *b) {
  CK(hover_exp(a, b->lhs));
  CK(hover_exp(a, b->rhs));
  return hover_symbol(a, b->op); // TODO: op_info
}

ANN static bool hover_capture(Hover *a, Capture *b) {
  return hover_tag2(a, &b->var.tag, b->var.value);
}

ANN static bool hover_captures(Hover *a, Capture_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Capture *c = mp_vector_at(b, Capture, i);
 //   if(before(a->pos, c->var.tag.loc.last))
 //     continue;
    CK(hover_capture(a, c));
  }
  return false;
}

ANN static bool hover_exp_unary(Hover *a, Exp_Unary *b) {
  CK(hover_symbol(a, b->op)); // TODO: op_info
  const enum unary_type type = b->unary_type;
  if(type == unary_exp) CK(hover_exp(a, b->exp));
  else if(type == unary_td) CK(hover_type_decl(a, b->ctor.td, exp_self(b)->type));
  else {
    CK(hover_stmt_list(a, b->code));
    if(b->captures) CK(hover_captures(a, b->captures));
  }
  return false;
}

ANN static bool hover_exp_cast(Hover *a, Exp_Cast *b) {
  CK(hover_type_decl(a, b->td, exp_self(b)->type));
  return hover_exp(a, b->exp);
}

ANN static bool hover_exp_post(Hover *a, Exp_Postfix *b) {
  CK(hover_symbol(a, b->op)); // TODO: op_info
  return hover_exp(a, b->exp);
}

ANN static bool hover_exp_call(Hover *a, Exp_Call *b) {
  CK(hover_exp(a, b->func));
  if(b->args) CK(hover_exp(a, b->args));
  if(b->tmpl) CK(hover_tmpl(a, b->tmpl));
  return false;
}

ANN static bool hover_exp_array(Hover *a, Exp_Array *b) {
  CK(hover_exp(a, b->base));
  return hover_array_sub(a, b->array);
}

ANN static bool hover_exp_slice(Hover *a, Exp_Slice *b) {
  CK(hover_exp(a, b->base));
  return hover_range(a, b->range);
}

ANN static bool hover_exp_if(Hover *a, Exp_If *b) {
  CK(hover_exp(a, b->cond));
  if(b->if_exp) CK(hover_exp(a, b->if_exp));
  return hover_exp(a, b->else_exp);
}

ANN static bool hover_exp_dot(Hover *a, Exp_Dot *b) {
  CK(hover_exp(a, b->base));
  // we don't really have a value
  // but we have a name and a type
  return hover_symbol(a,  b->tag.sym);
}

ANN static bool hover_exp_lambda(Hover *a, Exp_Lambda *b) {
  return hover_func_def(a, b->def);
}

ANN static bool hover_exp_td(Hover *a, Type_Decl *b) {
  return hover_type_decl(a, b, exp_self(b)->type);
}

ANN static bool hover_exp_named(Hover *a, Exp_Named *b) {
  return hover_exp(a, b->exp);
}


DECL_EXP_FUNC(hover, bool, Hover*);
ANN static bool hover_exp(Hover *a, Exp* b) {
  CK(hover_exp_func[b->exp_type](a, &b->d));
  if(b->next) CK(hover_exp(a, b ->next));
  return false;
}

ANN static bool hover_stmt_exp(Hover *a, Stmt_Exp b) {
  if(b->val) return hover_exp(a,  b->val);
  return false;
}

ANN static bool hover_stmt_while(Hover *a, Stmt_Flow b) {
  CK(hover_exp(a, b->cond));
  return hover_stmt(a, b->body);
}

ANN static bool hover_stmt_until(Hover *a, Stmt_Flow b) {
  CK(hover_exp(a, b->cond));
  return hover_stmt(a, b->body);
}

ANN static bool hover_stmt_for(Hover *a, Stmt_For b) {
  CK(hover_stmt(a, b->c1));
  if(b->c2) CK(hover_stmt(a, b->c2));
  if(b->c3) CK(hover_exp(a, b->c3));
  return hover_stmt(a, b->body);
}

ANN static bool hover_stmt_each(Hover *a, Stmt_Each b) {
  if(b->idx.tag.sym) CK(hover_var_decl(a, &b->idx));
  CK(hover_var_decl(a, &b->var));
  CK(hover_exp(a, b->exp));
  return hover_stmt(a, b->body);
}

ANN static bool hover_stmt_loop(Hover *a, Stmt_Loop b) {
  if(b->idx.tag.sym) CK(hover_var_decl(a, &b->idx));
  CK(hover_exp(a,  b->cond));
  return hover_stmt(a, b->body);
}

ANN static bool hover_stmt_if(Hover *a, Stmt_If b) {
  CK(hover_exp(a,  b->cond));
  CK(hover_stmt(a, b->if_body));
  if(b->else_body) return hover_stmt(a, b->else_body);
  return false;
}

ANN static bool hover_stmt_code(Hover *a, Stmt_Code b) {
  if(b->stmt_list) return hover_stmt_list(a, b->stmt_list);
  return false;
}

ANN static bool hover_stmt_break(Hover *a, Stmt_Index b) {
  return hover_stmt_index(a, b);
}

ANN static bool hover_stmt_continue(Hover *a, Stmt_Index b) {
  return hover_stmt_index(a, b);
}

ANN static bool hover_stmt_return(Hover *a, Stmt_Exp b) {
  if(b->val) return hover_exp(a, b-> val);
  return false;
}

ANN static bool hover_case_list(Hover *a, Stmt_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Stmt* c = mp_vector_at(b, Stmt, i);
  //  if(before(a->pos, c->loc.last))
  //    continue;
    CK(hover_stmt_case(a, &c->d.stmt_match));
  }
  return false;
}

ANN static bool hover_stmt_match(Hover *a, Stmt_Match b) {
  CK(hover_exp(a, b->cond));
  CK(hover_case_list(a, b->list));
  if(b->where) return hover_stmt(a, b->where);
  return false;
}

ANN static bool hover_stmt_case(Hover *a, Stmt_Match b) {
  CK(hover_exp(a, b->cond));
  CK(hover_stmt_list(a, b->list));
  if(b->when) return hover_exp(a, b->when);
  return false;
}

ANN static bool hover_stmt_index(Hover *a, Stmt_Index b) {
  return false;
}

ANN static bool hover_stmt_pp(Hover *a, Stmt_PP b) {
  if(b->pp_type == ae_pp_include)
    a->filename.source = b->data;
  return false;
}

ANN static bool hover_stmt_retry(Hover *a, Stmt_Exp b) {
  return false;
}

ANN static bool hover_handler(Hover *a, Handler *b) {
  CK(hover_tag(a, &b->tag));  // should return handler or effect info
  return hover_stmt(a, b->stmt);
}

ANN static bool hover_handler_list(Hover *a, Handler_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Handler *handler = mp_vector_at(b, Handler, i);
  //  if(before(a->pos, handler->tag.loc.last))
  //    continue;
    CK(hover_handler(a, handler));
  }
  return false;
}

ANN static bool hover_stmt_try(Hover *a, Stmt_Try b) {
  CK(hover_stmt(a, b->stmt));
  return hover_handler_list(a, b->handler);
}

ANN static bool hover_stmt_defer(Hover *a, Stmt_Defer b) {
  return hover_stmt(a, b->stmt);
}

ANN static bool hover_stmt_spread(Hover *a, Spread_Def b) {
  CK(hover_tag(a, &b->tag)); // should print info about spread tag
  return hover_id_list(a, b->list); // should print info about spread list
}

ANN static bool hover_stmt_using(Hover *a, Stmt_Using b) {
  CK(hover_type_decl(a, b->d.td, known_type(a->gwion->env, b->d.td)));
  if(b->tag.sym)
    return hover_exp(a, b->d.exp);
  const Type type = known_type(a->gwion->env, b->d.td);
  if(type)
    return hover_type_decl(a, b->d.td, type);
  return false;
}

ANN static bool hover_stmt_import(Hover *a, Stmt_Import b) {
  CK(hover_tag(a, &b->tag));
  if(b->selection) {
    for(uint32_t i = 0; i < b->selection->len; i++) {
      Stmt_Using using = *mp_vector_at(b->selection, Stmt_Using, i);
      CK(hover_stmt_using(a, using));
    }
  }
  return true;
}


DECL_STMT_FUNC(hover, bool, Hover*);
ANN static bool hover_stmt(Hover *a, Stmt* b) {
  return hover_stmt_func[b->stmt_type](a, &b->d);
}

ANN static bool hover_arg(Hover *a, Arg *b) {
  return hover_variable(a, &b->var);
}

ANN static bool hover_arg_list(Hover *a, Arg_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Arg *c = mp_vector_at(b, Arg, i);
  //  if(before(a->pos, c->var.vd.tag.loc.last))
  //    continue;
    CK(hover_arg(a, c));
  }
  return false;
}

ANN static bool hover_variable_list(Hover *a, Variable_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Variable *c = mp_vector_at(b, Variable, i);
    // TODO: check if td or vd
    CK(hover_variable(a, c));
  }
  return false;
}

ANN static bool hover_stmt_list(Hover *a, Stmt_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Stmt* c = mp_vector_at(b, Stmt, i);
  //  if(before(a->pos, c->loc.last))
  //    continue;
    CK(hover_stmt(a, c));
  }
  return false;
}

ANN static bool hover_func_base(Hover *a, Func_Base *b) {
  if(b->td) CK(hover_type_decl(a, b->td, b->ret_type));
  CK(hover_tag2(a, &b->tag, b->func ? b->func->value_ref : false));
  if(b->args) CK(hover_arg_list(a, b->args));
  if(b->tmpl) return hover_tmpl(a, b->tmpl);
  return false;
}

ANN static bool hover_func_def(Hover *a, Func_Def b) {
  CK(hover_func_base(a, b->base));
  if(b->d.code) CK(hover_stmt_list(a, b->d.code));
  if(b->captures) return hover_captures(a, b->captures);
  return false;
}

ANN static bool hover_class_def(Hover *a, Class_Def b) {
  CK(hover_type_def( a, &b->base));
  if(b->body) return hover_ast(a, b->body);
  return false;
}

ANN static bool hover_trait_def(Hover *a, Trait_Def b) {
  if(b->body) return hover_ast(a, b->body);
  return false;
}

ANN static bool hover_enumvalue(Hover *a, EnumValue *b) {
  return hover_tag(a, &b->tag); // we should print info about that enum
  // gwint, set
}

ANN static bool hover_enum_list(Hover *a, EnumValue_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    EnumValue *c = mp_vector_at(b, EnumValue, i);
//    if(c->set) {
//      if(!before(c.tag.loc.last))       
//          }
    if(before(a->pos, c->tag.loc.last))
      continue;
    CK(hover_enumvalue(a, c));
  }
  return false;
}

ANN static bool hover_enum_def(Hover *a, Enum_Def b) {
  CK(hover_tag2(a, &b->tag, b->type ? b->type->info->value : false));
  return hover_enum_list(a, b->list);
}

ANN static bool hover_union_def(Hover *a, Union_Def b) {
  CK(hover_tag2(a, &b->tag, b->type ? b->type->info->value : false));
  CK(hover_variable_list(a, b->l));
  if(b->tmpl) CK(hover_tmpl(a, b->tmpl));
  return false;
}

ANN static bool hover_fptr_def(Hover *a, Fptr_Def b) {
  return hover_func_base(a, b->base);
}

ANN static bool hover_type_def(Hover *a, Type_Def b) {
  if(b->ext) CK(hover_type_decl(a, b->ext, b->type ? b->type->info->parent : false));
  CK(hover_tag2(a, &b->tag, b->type ? b->type->info->value : false));
  if(b->tmpl) return hover_tmpl(a, b->tmpl);
  return false;
}

ANN static bool hover_extend_def(Hover *a, Extend_Def b) {
  CK(hover_type_decl(a, b->td, b->type));
  return hover_id_list(a, b->traits);
}

ANN static bool hover_prim_def(Hover *a, Prim_Def b) {
  return hover_tag(a, &b->tag);
}

DECL_SECTION_FUNC(hover, bool, Hover*);
ANN static bool hover_section(Hover *a, Section *b) {
  return hover_section_func[b->section_type](a, *(void**)&b->d);
}

ANN bool hover_ast(Hover *a, Ast b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Section *c = mp_vector_at(b, Section, i);
//    if(before(a->pos, c->loc.last))
//      continue;
    CK(hover_section(a, c));
  }
  return false;
}
