ANN static bool hover_symbol(Hover *a, Symbol b);
ANN static bool hover_tag(Hover *a, Tag *tag);
ANN static bool hover_array_sub(Hover *a, Array_Sub b);
ANN static bool hover_specialized(Hover *a, Specialized *b);
ANN static bool hover_tmpl(Hover *a, Tmpl *b);
ANN static bool hover_range(Hover *a, Range *b);
ANN2(1,2) static bool hover_type_decl(Hover *a, Type_Decl *b, const Type t);
ANN static bool hover_prim_id(Hover *a, Symbol *b);
ANN static bool hover_prim_num(Hover *a, m_uint *b);
ANN static bool hover_prim_float(Hover *a, m_float *b);
ANN static bool hover_prim_str(Hover *a, m_str *b);
ANN static bool hover_prim_array(Hover *a, Array_Sub *b);
ANN static bool hover_prim_range(Hover *a, Range* *b);
ANN static bool hover_prim_dict(Hover *a, Exp* *b);
ANN static bool hover_prim_hack(Hover *a, Exp* *b);
ANN static bool hover_prim_interp(Hover *a, Exp* *b);
ANN static bool hover_prim_char(Hover *a, m_str *b);
ANN static bool hover_prim_nil(Hover *a, bool *b);
ANN static bool hover_prim_perform(Hover *a, Symbol *b);
ANN static bool hover_prim(Hover *a, Exp_Primary *b);
ANN static bool hover_var_decl(Hover *a, Var_Decl *b);
ANN static bool hover_variable(Hover *a, Variable *b);
ANN static bool hover_exp_decl(Hover *a, Exp_Decl *b);
ANN static bool hover_exp_binary(Hover *a, Exp_Binary *b);
ANN static bool hover_exp_unary(Hover *a, Exp_Unary *b);
ANN static bool hover_exp_cast(Hover *a, Exp_Cast *b);
ANN static bool hover_exp_post(Hover *a, Exp_Postfix *b);
ANN static bool hover_exp_call(Hover *a, Exp_Call *b);
ANN static bool hover_exp_array(Hover *a, Exp_Array *b);
ANN static bool hover_exp_slice(Hover *a, Exp_Slice *b);
ANN static bool hover_exp_if(Hover *a, Exp_If *b);
ANN static bool hover_exp_dot(Hover *a, Exp_Dot *b);
ANN static bool hover_exp_lambda(Hover *a, Exp_Lambda *b);
ANN static bool hover_exp_td(Hover *a, Type_Decl *b);
ANN static bool hover_exp(Hover *a, Exp* b);
ANN static bool hover_stmt_exp(Hover *a, Stmt_Exp b);
ANN static bool hover_stmt_while(Hover *a, Stmt_Flow b);
ANN static bool hover_stmt_until(Hover *a, Stmt_Flow b);
ANN static bool hover_stmt_for(Hover *a, Stmt_For b);
ANN static bool hover_stmt_each(Hover *a, Stmt_Each b);
ANN static bool hover_stmt_loop(Hover *a, Stmt_Loop b);
ANN static bool hover_stmt_if(Hover *a, Stmt_If b);
ANN static bool hover_stmt_code(Hover *a, Stmt_Code b);
ANN static bool hover_stmt_break(Hover *a, Stmt_Index b);
ANN static bool hover_stmt_continue(Hover *a, Stmt_Index b);
ANN static bool hover_stmt_return(Hover *a, Stmt_Exp b);
ANN static bool hover_case_list(Hover *a, Stmt_List b);
ANN static bool hover_stmt_match(Hover *a, Stmt_Match b);
ANN static bool hover_stmt_case(Hover *a, Stmt_Match b);
ANN static bool hover_stmt_index(Hover *a, Stmt_Index b);
ANN static bool hover_stmt_pp(Hover *a, Stmt_PP b);
ANN static bool hover_stmt_retry(Hover *a, Stmt_Exp b);
ANN static bool hover_stmt_try(Hover *a, Stmt_Try b);
ANN static bool hover_stmt_defer(Hover *a, Stmt_Defer b);
ANN static bool hover_stmt(Hover *a, Stmt* b);
ANN static bool hover_arg(Hover *a, Arg *b);
ANN static bool hover_variable_list(Hover *a, Variable_List b);
ANN static bool hover_stmt_list(Hover *a, Stmt_List b);
ANN static bool hover_func_base(Hover *a, Func_Base *b);
ANN static bool hover_func_def(Hover *a, Func_Def b);
ANN static bool hover_class_def(Hover *a, Class_Def b);
ANN static bool hover_trait_def(Hover *a, Trait_Def b);
ANN static bool hover_enum_def(Hover *a, Enum_Def b);
ANN static bool hover_union_def(Hover *a, Union_Def b);
ANN static bool hover_fptr_def(Hover *a, Fptr_Def b);
ANN static bool hover_type_def(Hover *a, Type_Def b);
ANN static bool hover_extend_def(Hover *a, Extend_Def b);
ANN static bool hover_section(Hover *a, Section *b);