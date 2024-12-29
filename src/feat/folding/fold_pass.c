#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "fold_pass.h"

ANN static void add_foldrange(Fold *a, const FoldRange fr) {
  if(fr.loc.first.line != fr.loc.last.line)
    foldlist_add(a->mp, &a->foldranges, fr);
}

ANN static void fold_symbol(Fold *a, const Symbol b) {
  (void)a;
  (void)b;
}

ANN static void fold_loc(Fold *a, const loc_t b) {
  (void)a;
  (void)b;
}

ANN static void fold_tag(Fold *a, const Tag *b) {
  if(b->sym) fold_symbol(a, b->sym);
  fold_loc(a, b->loc);
}

ANN static void fold_array_sub(Fold *a, const Array_Sub b) {
  if(b->exp) fold_exp(a, b->exp);
}

ANN static void fold_id_list(Fold *a, const TagList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Tag c = taglist_at(b, i);
    fold_tag(a, &c);
  }
}

ANN static void fold_specialized(Fold *a, const Specialized *b) {
  fold_tag(a, &b->tag);
  if(b->traits) fold_id_list(a, b->traits);
}

ANN static void fold_specialized_list(Fold *a, const SpecializedList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Specialized c = specializedlist_at(b, i);
    fold_specialized(a, &c);
  }
}

ANN static void fold_tmplarg(Fold *a, const TmplArg *b) {
  if (b->type == tmplarg_td) fold_type_decl(a, b->d.td);
  else fold_exp(a, b->d.exp);
}

ANN static void fold_tmplarg_list(Fold *a, const TmplArgList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const TmplArg c = tmplarglist_at(b, i);
    fold_tmplarg(a, &c);
  }
}

ANN static void fold_tmpl(Fold *a, const Tmpl *b) {
  if(b->list) fold_specialized_list(a, b->list);
  if(b->call) fold_tmplarg_list(a, b->call);
}

ANN static void fold_range(Fold *a, const Range *b) {
  if(b->start) fold_exp(a, b->start);
  if(b->end) fold_exp(a , b->end);
}

ANN static void fold_type_decl(Fold *a, const Type_Decl *b) {
  fold_tag(a, &b->tag);
  if(b->array) fold_array_sub(a, b->array);
  if(b->types) fold_tmplarg_list(a, b->types);
}

ANN static void fold_prim_id(Fold *a, const Symbol *b) {
  fold_symbol(a, *b);
}

ANN static void fold_prim_num(Fold *a, const m_uint *b) {
  (void)a;
  (void)b;
}

ANN static void fold_prim_float(Fold *a, const m_float *b) {
  (void)a;
  (void)b;
}

ANN static void fold_prim_str(Fold *a, const m_str *b) {
  (void)a;
  (void)b;
}

ANN static void fold_prim_array(Fold *a, const Array_Sub *b) {
  fold_array_sub(a, *b);
}

ANN static void fold_prim_range(Fold *a, const Range* *b) {
  fold_range(a, *b);
}

ANN static void fold_prim_dict(Fold *a, const Exp* *b) {
  fold_exp(a, *b);
}

ANN static void fold_prim_hack(Fold *a, const Exp* *b) {
  fold_exp(a, *b);
}

ANN static void fold_prim_interp(Fold *a, const Exp* *b) {
  fold_exp(a, *b);
}

ANN static void fold_prim_char(Fold *a, const m_str *b) {
  (void)a;
  (void)b;
}

ANN static void fold_prim_nil(Fold *a, const void *b) {
  (void)a;
  (void)b;
}

ANN static void fold_prim_perform(Fold *a, const Symbol *b) {
  fold_symbol(a, *b);
}

ANN static void fold_prim_locale(Fold *a, const Symbol *b) {
  fold_symbol(a, *b);
}

DECL_PRIM_FUNC(fold, void, Fold *, const)
ANN static void fold_prim(Fold *a, const Exp_Primary *b) {
  fold_prim_func[b->prim_type](a, &b->d);
}

ANN static void fold_var_decl(Fold *a, const Var_Decl *b) {
  fold_tag(a, &b->tag);
}

ANN static void fold_variable(Fold *a, const Variable *b) {
  if(b->td) fold_type_decl(a, b->td);
  fold_var_decl(a, &b->vd);
}

ANN static void fold_exp_decl(Fold *a, const Exp_Decl *b) {
  fold_variable(a, &b->var);
}

ANN static void fold_exp_binary(Fold *a, const Exp_Binary *b) {
  fold_exp(a, b->lhs);
  fold_exp(a, b->rhs);
  fold_symbol(a, b->op);
}

ANN static void fold_capture(Fold *a, const Capture *b) {
  fold_tag(a, &b->var.tag);
}

ANN static void fold_captures(Fold *a, const CaptureList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Capture c = capturelist_at(b, i);
    fold_capture(a, &c);
  }
}

ANN static void fold_exp_unary(Fold *a, const Exp_Unary *b) {
  fold_symbol(a, b->op);
  const enum unary_type type = b->unary_type;
  if(type == unary_exp) fold_exp(a, b->exp);
  else if(type == unary_td) fold_type_decl(a, b->ctor.td);
  else {
    fold_stmt_list(a, b->code);
    if(b->captures)fold_captures(a, b->captures);
  }
}

ANN static void fold_exp_cast(Fold *a, const Exp_Cast *b) {
  fold_type_decl(a, b->td);
  fold_exp(a, b->exp);
}

ANN static void fold_exp_post(Fold *a, const Exp_Postfix *b) {
  fold_symbol(a, b->op);
  fold_exp(a, b->exp);
}

ANN static void fold_exp_call(Fold *a, const Exp_Call *b) {
  fold_exp(a, b->func);
  if(b->args) fold_exp(a, b->args);
  if(b->tmpl) fold_tmpl(a, b->tmpl);
}

ANN static void fold_exp_array(Fold *a, const Exp_Array *b) {
  fold_exp(a, b->base);
  fold_array_sub(a, b->array);
}

ANN static void fold_exp_slice(Fold *a, const Exp_Slice *b) {
  fold_exp(a, b->base);
  fold_range(a, b->range);
}

ANN static void fold_exp_if(Fold *a, const Exp_If *b) {
  fold_exp(a, b->cond);
  if(b->if_exp) fold_exp(a, b->if_exp);
  fold_exp(a, b->else_exp);
}

ANN static void fold_exp_dot(Fold *a, const Exp_Dot *b) {
  fold_exp(a, b->base);
  fold_var_decl(a, &b->var);
}

ANN static void fold_exp_lambda(Fold *a, const Exp_Lambda *b) {
  fold_func_def(a, b->def);
}

ANN static void fold_exp_td(Fold *a, const Type_Decl *b) {
  fold_type_decl(a, b);
}

ANN static void fold_exp_named(Fold *a, const Exp_Named *b) {
  fold_exp(a, b->exp);
}

DECL_EXP_FUNC(fold, void, Fold*, const)
ANN static void fold_exp(Fold *a, const Exp* b) {
  fold_exp_func[b->exp_type](a, &b->d);
  if(b->next) return fold_exp(a, b ->next);
}

ANN static void fold_stmt_exp(Fold *a, const Stmt_Exp b) {
  if(b->val) fold_exp(a,  b->val);
}

ANN static void fold_stmt_while(Fold *a, const Stmt_Flow b) {
  fold_exp(a, b->cond);
  fold_stmt(a, b->body);
}

ANN static void fold_stmt_until(Fold *a, const Stmt_Flow b) {
  fold_exp(a, b->cond);
  fold_stmt(a, b->body);
}

ANN static void fold_stmt_for(Fold *a, const Stmt_For b) {
  fold_stmt(a, b->c1);
  if(b->c2) fold_stmt(a, b->c2);
  if(b->c3) fold_exp(a, b->c3);
  fold_stmt(a, b->body);
}

ANN static void fold_stmt_each(Fold *a, const Stmt_Each b) {
  fold_var_decl(a, &b->var);
  if(b->idx.tag.sym) fold_var_decl(a, &b->idx);
  fold_exp(a, b->exp);
  fold_stmt(a, b->body);
}

ANN static void fold_stmt_loop(Fold *a, const Stmt_Loop b) {
  fold_exp(a,  b->cond);
  fold_stmt(a, b->body);
}

ANN static void fold_stmt_if(Fold *a, const Stmt_If b) {
  fold_exp(a,  b->cond);
  fold_stmt(a, b->if_body);
  if(b->else_body) fold_stmt(a, b->else_body);
}

ANN static void fold_stmt_code(Fold *a, const Stmt_Code b) {
  if(b->stmt_list) {
    loc_t loc = a->loc;
//    a->loc = b->loc;
    fold_stmt_list(a, b->stmt_list); 
    add_foldrange(a, (FoldRange){"region", a->loc});
    a->loc = loc;
  }
}

ANN static void fold_stmt_index(Fold *a, const Stmt_Index b) {
  (void)a;
  (void)b;
}

ANN static void fold_stmt_break(Fold *a, const Stmt_Index b) {
  fold_stmt_index(a, b);
}

ANN static void fold_stmt_continue(Fold *a, const Stmt_Index b) {
  fold_stmt_index(a, b);
}

ANN static void fold_stmt_return(Fold *a, const Stmt_Exp b) {
  if(b->val) fold_exp(a, b-> val);
}

ANN static void fold_stmt_case(Fold *a, const struct Match *b) {
  fold_exp(a, b->cond);
  fold_stmt_list(a, b->list);
  if(b->when) fold_exp(a, b->when);
}

ANN static void fold_case_list(Fold *a, const StmtList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Stmt c = stmtlist_at(b, i);
    fold_stmt_case(a, &c.d.stmt_match);
  }
}

ANN static void fold_stmt_match(Fold *a, const Stmt_Match b) {
  fold_exp(a, b->cond);
  fold_case_list(a, b->list);
  if(b->where) fold_stmt(a, b->where);
}

ANN static void foldrange_ini(Fold *a, const Stmt_PP b, const char *kind) {
  //a->last = (FoldRange){ .kind = kind, .loc = stmt_self(b)->loc };
  const pos_t pos = stmt_self(b)->loc.last;
  a->last = (FoldRange){ .kind = kind, .loc = (loc_t){ .first = pos, .last = pos } };
}

ANN static bool fold_is(Fold *a, const char *kind) {
  assert(a->last.kind);
  if(!strcmp(a->last.kind, kind)) 
    return true;
  return false;
}

ANN static bool fold_can_continue(Fold *a, const Stmt_PP b) {
  return stmt_self(b)->loc.first.line - 1 == a->last.loc.last.line;
//  return stmt_self(b)->loc.first.line == a->last.loc.last.line;
}

ANN static void fold_continue(Fold *a, const Stmt_PP b) {
  a->last.loc.last = stmt_self(b)->loc.last;
}

ANN static void foldrange_end(Fold *a, const Stmt_PP b, const char *kind) {
  if(fold_is(a, kind) && fold_can_continue(a, b))
    fold_continue(a, b);
  else {
    add_foldrange(a, a->last); 
    a->last.kind = NULL;
  }
}

ANN static void fold(Fold *a, const Stmt_PP b, const char *kind) {
    if(!a->last.kind) foldrange_ini(a, b, kind);
    else foldrange_end(a, b, kind);
}

ANN static void fold_stmt_pp(Fold *a, const Stmt_PP b) {
  (void)a;
  (void)b;
}

ANN static void fold_stmt_retry(Fold *a, const Stmt_Exp b) {
  (void)a;
  (void)b;
}

ANN static void fold_handler(Fold *a, const Handler *b) {
  fold_tag(a, &b->tag);
  fold_stmt(a, b->stmt);
}

ANN static void fold_handler_list(Fold *a, const HandlerList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Handler handler = handlerlist_at(b, i);
    fold_handler(a, &handler);
  }
}

ANN static void fold_stmt_try(Fold *a, const Stmt_Try b) {
  fold_stmt(a, b->stmt);
  fold_handler_list(a, b->handler);
}

ANN static void fold_stmt_defer(Fold *a, const Stmt_Defer b) {
  fold_stmt(a, b->stmt);
}

ANN static void fold_stmt_spread(Fold *a, const Spread_Def b) {
  fold_tag(a, &b->tag);
  fold_id_list(a, b->list);
}

ANN static void fold_stmt_using(Fold *a, const Stmt_Using b) {
  if(b->tag.sym)
    fold_exp(a, b->d.exp);
  else
   fold_type_decl(a, b->d.td);
}

ANN static void fold_stmt_import(Fold *a, const Stmt_Import b) {
  a->last.loc.last = stmt_self(b)->loc.last;
  fold_tag(a, &b->tag);
  if(b->selection) {
    for(uint32_t i = 0; i < b->selection->len; i++) {
      const UsingStmt using = usingstmtlist_at(b->selection, i);
      fold_tag(a, &using.tag);
      if(!using.tag.sym)
        fold_type_decl(a, using.d.td);
      else
        fold_exp(a, using.d.exp);
    }
  }
}


DECL_STMT_FUNC(fold, void, Fold*, const)
ANN static void fold_stmt(Fold *a, const Stmt* b) {
  fold_stmt_func[b->stmt_type](a, &b->d);
}

ANN static void fold_arg(Fold *a, const Arg *b) {
  fold_variable(a, &b->var);
}

ANN static void fold_arg_list(Fold *a, const ArgList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Arg c = arglist_at(b, i);
    fold_arg(a, &c);
  }
}

ANN static void fold_variable_list(Fold *a, const VariableList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Variable c = variablelist_at(b, i);
    fold_variable(a, &c);
  }
}

ANN static void fold_stmt_list(Fold *a, const StmtList *b) {
  const char *kind = NULL;
  for(uint32_t i = 0; i < b->len; i++) {
    const Stmt c = stmtlist_at(b, i);
    const char *new_kind = NULL;
/*    if(c.stmt_type == ae_stmt_pp && c.d.stmt_pp.pp_type == ae_pp_comment) {
      new_kind = "comment";
    }
    else */if(c.stmt_type == ae_stmt_import) {
      new_kind = "import";
    }
    if(kind != new_kind) { // check pos exists? huh
      add_foldrange(a, (FoldRange){ .kind = kind, .loc = a->last.loc }); 
      a->last.loc.first = c.loc.first;
    }
    fold_stmt(a, &c);
    kind = new_kind;
  }
}

ANN static void fold_func_base(Fold *a, const Func_Base *b) {
  if(b->td) fold_type_decl(a, b->td);
  fold_tag(a, &b->tag);
  if(b->args) fold_arg_list(a, b->args);
  if(b->tmpl) fold_tmpl(a, b->tmpl);
}

ANN static void fold_func_def(Fold *a, const Func_Def b) {
  fold_func_base(a, b->base);
  if(b->d.code) fold_stmt_list(a, b->d.code);
  if(b->captures) fold_captures(a, b->captures);
}

ANN static void fold_type_def(Fold *a, const Type_Def b) {
  if(b->ext) fold_type_decl(a, b->ext);
  fold_tag(a, &b->tag);
  if(b->tmpl) fold_tmpl(a, b->tmpl);
}

ANN static void fold_class_def(Fold *a, const Class_Def b) {

  fold_type_def(a, &b->base);
  if(b->body) fold_ast(a, b->body);
}

ANN static void fold_trait_def(Fold *a, const Trait_Def b) {
  if(b->body) fold_ast(a, b->body);
}

ANN static void fold_enumvalue(Fold *a, const EnumValue *b) {
  fold_tag(a, &b->tag);
  // gwint, set
}

ANN static void fold_enum_list(Fold *a, const EnumValueList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const EnumValue c = enumvaluelist_at(b, i);
    fold_enumvalue(a, &c);
  }
}

ANN static void fold_enum_def(Fold *a, const Enum_Def b) {
  fold_enum_list(a, b->list);
  fold_tag(a, &b->tag);
}

ANN static void fold_union_def(Fold *a, const Union_Def b) {
  fold_variable_list(a, b->l);
  fold_tag(a, &b->tag);
  if(b->tmpl) fold_tmpl(a, b->tmpl);
}

ANN static void fold_fptr_def(Fold *a, const Fptr_Def b) {
  fold_func_base(a, b->base);
}

ANN static void fold_extend_def(Fold *a, const Extend_Def b) {
  fold_type_decl(a, b->td);
  fold_id_list(a, b->traits);
}

ANN static void fold_prim_def(Fold *a, const Prim_Def b) {
  fold_tag(a, &b->tag);
}
// using loc as the new system
DECL_SECTION_FUNC(fold, void, Fold*, const)
ANN static void fold_section(Fold *a, const Section *b) {
  loc_t loc = a->loc;
  if(b->section_type != ae_section_stmt)
    a->loc = b->loc;
  fold_section_func[b->section_type](a, *(void**)&b->d);
  if(b->section_type != ae_section_stmt)
    add_foldrange(a, (FoldRange){"region", a->loc});
  a->loc = loc;
}

ANN void fold_ast(Fold *a, const Ast b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Section c = sectionlist_at(b, i);
    fold_section(a, &c);
  }
  // catch up remaining comments or imports foldranges
//  if(a->last.kind) add_foldrange(a, a->last);
}
