#include "defs.h"
#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "map.h"
#include "vm.h"
#include "gwion.h"
#include "io.h"

typedef struct Doc {
  MemPool      mp;
  CommentList *comments;
  Map          map;
  void        *last;
  pos_t        pos;
  uint32_t     index;
} Doc;

#define CHECK_RET(a, ret) \
do {                      \
  if(!(a))                \
    ret = false;          \
} while(0) 

#define PUSH_SCOPE(a)     \
//  void *last = (a)->last;

#define POP_SCOPE(a, loc) \
/*  doc_handle(a, a->last, (loc_t){ loc.last, loc.last });*/ \
/*  if(last)(a)->last = last;*/

ANN static bool doc_ast(Doc *a, const Ast);
ANN static bool doc_stmt(Doc *a, const Stmt *);
ANN static bool doc_stmt_list(Doc *a, const StmtList *);
ANN static bool doc_exp(Doc *a, const Exp *);
ANN static bool doc_type_decl(Doc *a, const Type_Decl* b);
ANN static bool doc_func_def(Doc *a, const Func_Def b);

ANN static bool is_before(const loc_t a, const loc_t b) {
  if(a.last.line < b.first.line)
    return true; 
  if(a.last.line > b.first.line)
    return false;
  return a.last.column < b.first.column;
}

static void doc_handle(Doc *a, void *data, const loc_t loc) {
  uint32_t i;
  // first loop the `after` comments
  //
  //
  // store the loc in some `last` thing?
  // so we can now if it's on same line?
  for(i = a->index; i < a->comments->len; i++) {
    Comment *c = commentlist_ptr_at(a->comments, i);

    if(c->loc.last.line == loc.first.line ||
      a->pos.line == c->loc.first.line)
      c->alone = true;

    if(c->type == comment_normal) continue;
    if(!is_before(c->loc, loc)) break;
    if(c->type == comment_before) {
      GwText *text = (GwText*)map_get(a->map, (vtype)a->last);
      if(!text) {
        text = new_text(a->mp);
        map_set(a->map, (vtype)a->last, (vtype)text);
      }
      text_add(text, c->str);
    } else break;
  }
  a->index = i;
  for(i = a->index; i < a->comments->len; i++) {
    Comment c = commentlist_at(a->comments, i);
    if(is_before(c.loc, loc)) {
      if(c.type == comment_normal) continue;
      if(c.type == comment_after) {
        GwText *text = (GwText*)map_get(a->map, (vtype)data);
        if(!text) {
          text = new_text(a->mp);
          map_set(a->map, (vtype)data, (vtype)text);
        }
        text_add(text, c.str);
    } else break;

      //if(c.type == comment_after) break;
  } else break;
    }
  a->index = i;
  a->last = data;
  a->pos = loc.last;
}

static void doc_handle2(Doc *a, void *data, const loc_t loc) {
  uint32_t i;
  for(i = a->index; i < a->comments->len; i++) {
    Comment c = commentlist_at(a->comments, i);
    if(!is_before(loc, c.loc)) break;
    if(c.type == comment_before) {
        GwText *text = (GwText*)map_get(a->map, (vtype)data);
        if(!text) {
          text = new_text(a->mp);
          map_set(a->map, (vtype)data, (vtype)text);
        }
        text_add(text, c.str);
    }
  }
  a->index = i;
  a->last = data;
}

ANN static bool doc_symbol(Doc *a NUSED, const Symbol b NUSED) {
  return true;
}

ANN static bool doc_tag(Doc *a, const Tag *b) {
  return b->sym ? doc_symbol(a, b->sym) : true;
}

ANN static bool doc_array_sub(Doc *a, const Array_Sub b) {
  return b->exp ? doc_exp(a, b->exp) : true;
}

ANN static bool doc_taglist(Doc *a, const TagList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Tag c = taglist_at(b, i);
    CHECK_RET(doc_tag(a, &c), ret);
  }
  return ret;
}

ANN static bool doc_specialized(Doc *a, const Specialized *b) {
  bool ret = true;
  CHECK_RET(doc_tag(a, &b->tag), ret);
  if(b->traits) CHECK_RET(doc_taglist(a, b->traits), ret);
  return ret;
}

ANN static bool doc_specialized_list(Doc *a, const SpecializedList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Specialized c = specializedlist_at(b, i);
    CHECK_RET(doc_specialized(a, &c), ret);
  }
  return ret;
}

ANN static bool doc_tmplarg(Doc *a, const TmplArg *b) {
  bool ret = true;
  if (b->type == tmplarg_td) CHECK_RET(doc_type_decl(a, b->d.td), ret);
  else CHECK_RET(doc_exp(a, b->d.exp), ret);
  return ret;
}

ANN static bool doc_tmplarg_list(Doc *a, const TmplArgList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const TmplArg c = tmplarglist_at(b, i);
    CHECK_RET(doc_tmplarg(a, &c), ret);
  }
  return ret;
}

ANN static bool doc_tmpl(Doc *a, const Tmpl *b) {
  bool ret = true;
  if(b->list) CHECK_RET(doc_specialized_list(a, b->list), ret);
  if(b->call) CHECK_RET(doc_tmplarg_list(a, b->call), ret);
  return ret;
}

ANN static bool doc_range(Doc *a, const Range *b) {
  bool ret = true;
  if(b->start) CHECK_RET(doc_exp(a, b->start), ret);
  if(b->end) CHECK_RET(doc_exp(a , b->end), ret);
  return ret;
}

ANN static bool doc_type_decl(Doc *a, const Type_Decl *b) {
  bool ret = true;
  CHECK_RET(doc_tag(a, &b->tag), ret);
  if(b->array) CHECK_RET(doc_array_sub(a, b->array), ret);
  if(b->types) CHECK_RET(doc_tmplarg_list(a, b->types), ret);
  return ret;
}

ANN static bool doc_prim_id(Doc *a, const Symbol *b) {
  return doc_symbol(a, *b);
}

ANN static bool doc_prim_num(Doc *a NUSED, const m_uint *b NUSED) {
  return true;
}

ANN static bool doc_prim_float(Doc *a NUSED, const m_float *b NUSED) {
  return true;
}

ANN static bool doc_prim_str(Doc *a NUSED, const m_str *b NUSED) {
  return true;
}

ANN static bool doc_prim_array(Doc *a, const Array_Sub *b) {
  return doc_array_sub(a, *b);
}

ANN static bool doc_prim_range(Doc *a, const Range* *b) {
  return doc_range(a, *b);
}

ANN static bool doc_prim_dict(Doc *a, const Exp* *b) {
  return doc_exp(a, *b);
}

ANN static bool doc_prim_hack(Doc *a, const Exp* *b) {
  return doc_exp(a, *b);
}

ANN static bool doc_prim_interp(Doc *a, const Exp* *b) {
  return doc_exp(a, *b);
}

ANN static bool doc_prim_char(Doc *a NUSED, const m_str *b NUSED) {
  return true;
}

ANN static bool doc_prim_nil(Doc *a NUSED, const bool *b NUSED) {
  return true;
}

ANN static bool doc_prim_perform(Doc *a, const Symbol *b) {
  return doc_symbol(a, *b);
}

ANN static bool doc_prim_locale(Doc *a, const Symbol *b) {
  return doc_symbol(a, *b);
}

DECL_PRIM_FUNC(doc, bool, Doc *, const)
ANN static bool doc_prim(Doc *a, const Exp_Primary *b) {
  return doc_prim_func[b->prim_type](a, &b->d);
}

ANN static bool doc_var_decl(Doc *a, const Var_Decl *b) {
  doc_handle(a, b->value, b->tag.loc);
  return doc_tag(a, &b->tag);
}

ANN static bool doc_variable(Doc *a, const Variable *b) {
  bool ret = true;
  if(b->td) CHECK_RET(doc_type_decl(a, b->td), ret);
  CHECK_RET(doc_var_decl(a, &b->vd), ret);
  return ret;
}

ANN static bool doc_exp_decl(Doc *a, const Exp_Decl *b) {
  bool ret = true;
  CHECK_RET(doc_variable(a, &b->var), ret);
  if(b->args) CHECK_RET(doc_exp(a, b->args), ret);
  return ret;
}

ANN static bool doc_exp_binary(Doc *a, const Exp_Binary *b) {
  bool ret = true;
  CHECK_RET(doc_exp(a, b->lhs), ret);
  CHECK_RET(doc_exp(a, b->rhs), ret);
  CHECK_RET(doc_symbol(a, b->op), ret);
  return ret;
}

ANN static bool doc_capture(Doc *a, const Capture *b) {
  return doc_tag(a, &b->var.tag);
}

ANN static bool doc_captures(Doc *a, const CaptureList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Capture c = capturelist_at(b, i);
    CHECK_RET(doc_capture(a, &c), ret);
  }
  return ret;
}

ANN static bool doc_exp_unary(Doc *a, const Exp_Unary *b) {
  bool ret = true;
  CHECK_RET(doc_symbol(a, b->op), ret);
  const enum unary_type type = b->unary_type;
  if(type == unary_exp) CHECK_RET(doc_exp(a, b->exp), ret);
  else if(type == unary_td) CHECK_RET(doc_type_decl(a, b->ctor.td), ret);
  else {
    CHECK_RET(doc_stmt_list(a, b->code), ret);
    if(b->captures)CHECK_RET(doc_captures(a, b->captures), ret);
  }
  return ret;
}

ANN static bool doc_exp_cast(Doc *a, const Exp_Cast *b) {
  bool ret = true;
  CHECK_RET(doc_type_decl(a, b->td), ret);
  CHECK_RET(doc_exp(a, b->exp), ret);
  return ret;
}

ANN static bool doc_exp_post(Doc *a, const Exp_Postfix *b) {
  bool ret = true;
  CHECK_RET(doc_symbol(a, b->op), ret);
  CHECK_RET(doc_exp(a, b->exp), ret);
  return ret;
}

ANN static bool doc_exp_call(Doc *a, const Exp_Call *b) {
  bool ret = true;
  CHECK_RET(doc_exp(a, b->func), ret);
  if(b->args) CHECK_RET(doc_exp(a, b->args), ret);
  if(b->tmpl) CHECK_RET(doc_tmpl(a, b->tmpl), ret);
  return ret;
}

ANN static bool doc_exp_array(Doc *a, const Exp_Array *b) {
  bool ret = true;
  CHECK_RET(doc_exp(a, b->base), ret);
  CHECK_RET(doc_array_sub(a, b->array), ret);
  return ret;
}

ANN static bool doc_exp_slice(Doc *a, const Exp_Slice *b) {
  bool ret = true;
  CHECK_RET(doc_exp(a, b->base), ret);
  CHECK_RET(doc_range(a, b->range), ret);
  return ret;
}

ANN static bool doc_exp_if(Doc *a, const Exp_If *b) {
  bool ret = true;
  CHECK_RET(doc_exp(a, b->cond), ret);
  if(b->if_exp) CHECK_RET(doc_exp(a, b->if_exp), ret);
  CHECK_RET(doc_exp(a, b->else_exp), ret);
  return ret;
}

ANN static bool doc_exp_dot(Doc *a, const Exp_Dot *b) {
  bool ret = true;
  CHECK_RET(doc_exp(a, b->base), ret);
  CHECK_RET(doc_var_decl(a, &b->var), ret);
  return ret;
}

ANN static bool doc_exp_lambda(Doc *a, const Exp_Lambda *b) {
  return doc_func_def(a, b->def);
}

ANN static bool doc_exp_td(Doc *a, const Type_Decl *b) {
  return doc_type_decl(a, b);
}

ANN static bool doc_exp_named(Doc *a, const Exp_Named *b) {
  return doc_exp(a, b->exp);
}

DECL_EXP_FUNC(doc, bool, Doc*, const)
ANN static bool doc_exp(Doc *a, const Exp* b) {
  bool ret = doc_exp_func[b->exp_type](a, &b->d);
  if(b->next) CHECK_RET(doc_exp(a, b ->next), ret);
  return ret;
}

ANN static bool doc_stmt_exp(Doc *a, const Stmt_Exp b) {
  return b->val ? doc_exp(a,  b->val) : true;
}

ANN static bool doc_stmt_while(Doc *a, const Stmt_Flow b) {
  bool ret = true;
  CHECK_RET(doc_exp(a, b->cond), ret);
  CHECK_RET(doc_stmt(a, b->body), ret);
  return ret;
}

ANN static bool doc_stmt_until(Doc *a, const Stmt_Flow b) {
  bool ret = true;
  CHECK_RET(doc_exp(a, b->cond), ret);
  CHECK_RET(doc_stmt(a, b->body), ret);
  return ret;
}

ANN static bool doc_stmt_for(Doc *a, const Stmt_For b) {
  bool ret = true;
  CHECK_RET(doc_stmt(a, b->c1), ret);
  if(b->c2) CHECK_RET(doc_stmt(a, b->c2), ret);
  if(b->c3) CHECK_RET(doc_exp(a, b->c3), ret);
  CHECK_RET(doc_stmt(a, b->body), ret);
  return ret;
}

ANN static bool doc_stmt_each(Doc *a, const Stmt_Each b) {
  bool ret = true;
  if(b->idx.tag.sym) CHECK_RET(doc_var_decl(a, &b->idx), ret);
  CHECK_RET(doc_var_decl(a, &b->var), ret);
  CHECK_RET(doc_exp(a, b->exp), ret);
  CHECK_RET(doc_stmt(a, b->body), ret);
  return ret;
}

ANN static bool doc_stmt_loop(Doc *a, const Stmt_Loop b) {
  bool ret = true;
  if(b->idx.tag.sym) CHECK_RET(doc_var_decl(a, &b->idx), ret);
  CHECK_RET(doc_exp(a,  b->cond), ret);
  CHECK_RET(doc_stmt(a, b->body), ret);
  return ret;
}

ANN static bool doc_stmt_if(Doc *a, const Stmt_If b) {
  bool ret = true;
  CHECK_RET(doc_exp(a,  b->cond), ret);
  CHECK_RET(doc_stmt(a, b->if_body), ret);
  if(b->else_body) doc_stmt(a, b->else_body);
  return ret;
}

ANN static bool doc_stmt_code(Doc *a, const Stmt_Code b) {
  if(unlikely(!b->stmt_list)) return true;
  PUSH_SCOPE(a);
  const bool ret = doc_stmt_list(a, b->stmt_list);
  POP_SCOPE(a, stmt_self(b)->loc);
  return ret;
}

ANN static bool doc_stmt_index(Doc *a NUSED, const Stmt_Index b NUSED) {
  return true;
}

ANN static bool doc_stmt_break(Doc *a, const Stmt_Index b) {
  return doc_stmt_index(a, b);
}

ANN static bool doc_stmt_continue(Doc *a, const Stmt_Index b) {
  return doc_stmt_index(a, b);
}

ANN static bool doc_stmt_return(Doc *a, const Stmt_Exp b) {
  return b->val ? doc_exp(a, b-> val) : true;
}

ANN static bool doc_stmt_case(Doc *a, const struct Match *b) {
  bool ret = true;
  PUSH_SCOPE(a);
  CHECK_RET(doc_exp(a, b->cond), ret);
  CHECK_RET(doc_stmt_list(a, b->list), ret);
  if(b->when) CHECK_RET(doc_exp(a, b->when), ret);
  POP_SCOPE(a, stmt_self(b)->loc);
  return ret;
}

ANN static bool doc_case_list(Doc *a, const StmtList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Stmt c = stmtlist_at(b, i);
    CHECK_RET(doc_stmt_case(a, &c.d.stmt_match), ret);
  }
  return ret;
}

ANN static bool doc_stmt_match(Doc *a, const Stmt_Match b) {
  bool ret = true;
  PUSH_SCOPE(a);
  doc_exp(a, b->cond);
  doc_case_list(a, b->list);
  if(b->where) doc_stmt(a, b->where);
  POP_SCOPE(a, stmt_self(b)->loc);
  return ret;
}

ANN static bool doc_stmt_pp(Doc *a NUSED, const Stmt_PP b NUSED) {
  return true;
}

ANN static bool doc_stmt_retry(Doc *a NUSED, const Stmt_Exp b NUSED) {
  return true;
}

ANN static bool doc_handler(Doc *a, const Handler *b) {
  bool ret = true;
  PUSH_SCOPE(a);
  CHECK_RET(doc_tag(a, &b->tag), ret);
  CHECK_RET(doc_stmt(a, b->stmt), ret);
  POP_SCOPE(a, stmt_self(b)->loc);
  return ret;
}

ANN static bool doc_handler_list(Doc *a, const HandlerList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Handler c = handlerlist_at(b, i);
    CHECK_RET(doc_handler(a, &c), ret);
  }
  return ret;
}

ANN static bool doc_stmt_try(Doc *a, const Stmt_Try b) {
  bool ret = true;
  CHECK_RET(doc_stmt(a, b->stmt), ret);
  PUSH_SCOPE(a);
  CHECK_RET(doc_handler_list(a, b->handler), ret);
  POP_SCOPE(a, stmt_self(b)->loc);
  return ret;
}

ANN static bool doc_stmt_defer(Doc *a, const Stmt_Defer b) {
  return doc_stmt(a, b->stmt);
}

ANN static bool doc_stmt_spread(Doc *a, const Spread_Def b) {
  bool ret = true;
  CHECK_RET(doc_tag(a, &b->tag), ret);
  CHECK_RET(doc_taglist(a, b->list), ret);
  return ret;
}

ANN static bool doc_stmt_using(Doc *a, const Stmt_Using b) {
  bool ret = true;
  if(b->tag.sym) {
    CHECK_RET(doc_tag(a, &b->tag), ret);
    CHECK_RET(doc_exp(a, b->d.exp), ret);
  } else
    CHECK_RET(doc_type_decl(a, b->d.td), ret);
  return ret;
}

ANN static bool doc_stmt_import(Doc *a, const Stmt_Import b) {
  bool ret = true;
  CHECK_RET(doc_tag(a, &b->tag), ret);
  if(b->selection) {
    for(uint32_t i = 0; i < b->selection->len; i++) {
      const UsingStmt c = usingstmtlist_at(b->selection, i);
      CHECK_RET(doc_tag(a, &c.tag), ret);
      if(c.tag.sym)
        CHECK_RET(doc_exp(a, c.d.exp), ret);
      else
        CHECK_RET(doc_type_decl(a, c.d.td), ret);
    }
  }
  return ret;
}

DECL_STMT_FUNC(doc, bool, Doc*, const)
ANN static bool doc_stmt(Doc *a, const Stmt* b) {
  return doc_stmt_func[b->stmt_type](a, &b->d);
}

ANN static bool doc_arg(Doc *a, const Arg *b) {
  return doc_variable(a, &b->var);
}

ANN static bool doc_arg_list(Doc *a, const ArgList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Arg c = arglist_at(b, i);
    CHECK_RET(doc_arg(a, &c), ret);
  }
  return ret;
}

ANN static bool doc_variablelist(Doc *a, const VariableList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Variable c = variablelist_at(b, i);
    CHECK_RET(doc_variable(a, &c), ret);
  }
  return ret;
}

ANN static bool doc_stmt_list(Doc *a, const StmtList *b) {
  if(!b->len) return true;
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const Stmt c = stmtlist_at(b, i);
    CHECK_RET(doc_stmt(a, &c), ret);
  }
  return ret;
}

ANN static bool doc_func_base(Doc *a, const Func_Base *b) {
  bool ret = true;
  if(b->td) CHECK_RET(doc_type_decl(a, b->td), ret);
  if(b->func) // we may be better off using func!
    doc_handle(a, b->func->value_ref, b->tag.loc);
  CHECK_RET(doc_tag(a, &b->tag), ret);
  if(b->args) {
    CHECK_RET(doc_arg_list(a, b->args), ret);
  }
  if(b->tmpl) CHECK_RET(doc_tmpl(a, b->tmpl), ret);
  return ret;
}

ANN static bool doc_func_def(Doc *a, const Func_Def b) {
  bool ret = true;
  CHECK_RET(doc_func_base(a, b->base), ret);
//  if(b->base->func)
//  a->last = b->base->func->value_ref;
  // what about captures
  // and if comment is after argument?
  if(b->captures) CHECK_RET(doc_captures(a, b->captures), ret);
  if(b->d.code) {
    Stmt first = stmtlist_at(b->d.code, 0);
  if(b->base->func)
    doc_handle(a, a->last, first.loc); 
  if(b->base->func)
    a->last = b->base->func->value_ref;
    CHECK_RET(doc_stmt_list(a, b->d.code), ret);
  if(b->base->func)
    a->last = b->base->func->value_ref;
  }
  return ret;
}

ANN static bool doc_type_def(Doc *a, const Type_Def b) {
  bool ret = true;
  if(b->ext) CHECK_RET(doc_type_decl(a, b->ext), ret);
  CHECK_RET(doc_tag(a, &b->tag), ret);
  if(b->tmpl) CHECK_RET(doc_tmpl(a, b->tmpl), ret);
  return ret;
}

ANN static bool doc_class_def(Doc *a, const Class_Def b) {
  bool ret = true;
  CHECK_RET(doc_type_def( a, &b->base), ret);
  if(b->body) CHECK_RET(doc_ast(a, b->body), ret);
  return ret;
}

ANN static bool doc_trait_def(Doc *a, const Trait_Def b) {
  return b->body ? doc_ast(a, b->body) : true;
}

ANN static bool doc_enumvalue(Doc *a, const EnumValue *b) {
  // TODO: get trait then
  return doc_tag(a, &b->tag);
}

ANN static bool doc_enum_list(Doc *a, const EnumValueList *b) {
  bool ret = true;
  for(uint32_t i = 0; i < b->len; i++) {
    const EnumValue c = enumvaluelist_at(b, i);
    // TODO: get value then
    CHECK_RET(doc_enumvalue(a, &c), ret);
  }
  return ret;
}

ANN static bool doc_enum_def(Doc *a, const Enum_Def b) {
  bool ret = true;
  CHECK_RET(doc_enum_list(a, b->list), ret);
  CHECK_RET(doc_tag(a, &b->tag), ret);
  return ret;
}

ANN static bool doc_union_def(Doc *a, const Union_Def b) {
  bool ret = true;
  CHECK_RET(doc_variablelist(a, b->l), ret);
  CHECK_RET(doc_tag(a, &b->tag), ret);
  if(b->tmpl) CHECK_RET(doc_tmpl(a, b->tmpl), ret);
  return ret;
}

ANN static bool doc_fptr_def(Doc *a, const Fptr_Def b) {
  return doc_func_base(a, b->base);
}

ANN static bool doc_extend_def(Doc *a, const Extend_Def b) {
  bool ret = true;
  CHECK_RET(doc_type_decl(a, b->td), ret);
  CHECK_RET(doc_taglist(a, b->traits), ret);
  return ret;
}

ANN static bool doc_prim_def(Doc *a, const Prim_Def b) {
  return doc_tag(a, &b->tag);
}

DECL_SECTION_FUNC(doc, bool, Doc*, const)
ANN static bool doc_section(Doc *a, const Section *b) {
  return doc_section_func[b->section_type](a, *(void**)&b->d);
}

ANN static bool doc_ast(Doc *a, const Ast b) {
  bool ret = true;
  Section c;
  for(uint32_t i = 0; i < b->len; i++) {
    PUSH_SCOPE(a);
    c = sectionlist_at(b, i);
    CHECK_RET(doc_section(a, &c), ret);
    POP_SCOPE(a, c.loc);
  }
  if(!a->last)exit(20);
  doc_handle2(a, a->last, (loc_t){});
  return ret;
}

ANN bool doc_pass(const Env env, Ast *b) {
  const Symbol sym = insert_symbol(env->gwion->st, "@lsp_buffer");
  Value value = nspc_lookup_value0(env->curr, sym);
  if(!value) return NULL;
  Buffer *buffer = (Buffer*)value->d.ptr;
  buffer->context = env->context;
  Doc doc = {
    .mp = env->gwion->mp,
    .comments = buffer->comments,
    .map = &buffer->docs
  };
  return doc_ast(&doc, *b);
}

ANN m_str get_doc(Context context, void *data) {
  Buffer buffer = get_buffer(context->name);
  const GwText *text = (GwText*)map_get(&buffer.docs, (vtype)data);
  return text->str;
}
