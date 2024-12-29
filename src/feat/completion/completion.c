#include "gwion_util.h"
#include "gwion_ast.h" // IWYU pragma: export
#include "gwion_env.h" // IWYU pragma: export
#include "vm.h"        // IWYU pragma: export
#include "instr.h"     // IWYU pragma: export
#include "emit.h"      // IWYU pragma: export
#include "gwion.h"
#include "lsp.h"
#include "feat/get_value.h"
#include "complete.h"

typedef struct Completer {
  const Gwion gwion;
  cJSON *const results;
  const pos_t pos;
  bool inside;
} Completer;
#include "gwfmt.h"
static void add_signature(const Completer *c, const Func f, const char *name) {
  struct GwfmtState ls = {};
  gwfmt_state_init(&ls);
  Gwfmt l = {.mp = c->gwion->mp, .st = c->gwion->st, .ls = &ls, .last = cht_nl };
  l.filename = name;
  text_init(&ls.text, c->gwion->mp);

        StmtList *code = f->def->d.code;
        f->def->d.code = NULL;
    gwfmt_func_def(&l, f->def);



   f->def->d.code = code;
          cJSON *signature = cJSON_CreateObject();
  ls.text.str[ls.text.len -2] = '\0';
  //message(ls.text.str,1);
  cJSON_AddStringToObject(signature, "label", ls.text.str);

  const m_str doc = get_doc(f->value_ref->from->ctx, f->value_ref);
  if(doc) cJSON_AddStringToObject(signature, "documentation", doc);

  // cJSON_AddStringToObject(signature, "documentation", ???);
  ArgList *args = f->def->base->args;
  if(args)  {
    //ls.text.len = 0;
    cJSON *arguments = cJSON_AddArrayToObject(signature, "parameters");
    for(uint32_t i = 0; i < args->len; i++) {
    ls.text.len = 0;
      const Arg arg = arglist_at(args, i);
      gwfmt_variable(&l, &arg.var);
      cJSON *argument = cJSON_CreateObject();
      cJSON_AddStringToObject(argument, "label", ls.text.str);
      /* cJSON_AddStringToObject(argument, "documentation", ???); */
      cJSON_AddItemToArray(arguments, argument);
    }
  }
  text_release(&ls.text);
  cJSON_AddItemToArray(c->results, signature);
}

static void nspc_signature_map(const Completer *c, const Map m, const char *name) {
  for(m_uint i = 0; i < map_size(m); i++) { // run in reverse?
    const Func f = (Func)map_at(m, i);
    if(!strcmp(name, s_name(f->def->base->tag.sym))) {
      add_signature(c, f, name);
    }
  }
}

void lsp_signatureHelp(const Gwion gwion, int id, const cJSON *params_json) {
  TextDocumentPosition document = lsp_parse_document(params_json);
  //cJSON *context = cJSON_GetObjectItem(params_json, "context");
  //cJSON *retrigger = cJSON_GetObjectItem(context, "isRetrigger");
  // trigger kind
  // active param

  Buffer buffer = get_buffer(document.uri);
  if(!buffer.context) {
    lsp_send_response(id, NULL);
    return;
  }
  const Value value = value_pass(gwion, buffer.context, document);
  if(!value) {
    lsp_send_response(id, NULL);
    return;
  }
  cJSON *result = cJSON_CreateObject();
  Completer c = {
    .gwion = gwion,
    .results = cJSON_AddArrayToObject(result, "signatures"),
    .pos = document.pos
  };
  if(is_func(gwion, value->type))
    add_signature(&c, value->d.func_ref, "test");
  // NOTE: could be also for function pointers
  lsp_send_response(id, result);
}

typedef struct CompletionItem {

	/**
	 * The label of this completion item.
	 *
	 * The label property is also by default the text that
	 * is inserted when selecting this completion.
	 *
	 * If label details are provided the label itself should
	 * be an unqualified name of the completion item.
	 */
	char *label;

	/**
	 * Additional details for the label
	 *
	 * @since 3.17.0
	 */
//	labelDetails?: CompletionItemLabelDetails;


	/**
	 * The kind of this completion item. Based of the kind
	 * an icon is chosen by the editor. The standardized set
	 * of available values is defined in `CompletionItemKind`.
	 */
	CompletionKind kind;

	/**
	 * Tags for this completion item.
	 *
	 * @since 3.15.0
	 */
//	tags?: CompletionItemTag[];

	/**
	 * A human-readable string with additional information
	 * about this item, like type or symbol information.
	 */
//	detail?: string;

	/**
	 * A human-readable string that represents a doc-comment.
	 */
//	documentation?: string | MarkupContent;
        m_str doc;
	/**
	 * Indicates if this item is deprecated.
	 *
	 * @deprecated Use `tags` instead if supported.
	 */
//	deprecated?: boolean;

	/**
	 * Select this item when showing.
	 *
	 * *Note* that only one completion item can be selected and that the
	 * tool / client decides which item that is. The rule is that the *first*
	 * item of those that match best is selected.
	 */
//	preselect?: boolean;

	/**
	 * A string that should be used when comparing this item
	 * with other items. When omitted the label is used
	 * as the sort text for this item.
	 */
//	sortText?: string;

	/**
	 * A string that should be used when filtering a set of
	 * completion items. When omitted the label is used as the
	 * filter text for this item.
	 */
//	filterText?: string;

	/**
	 * A string that should be inserted into a document when selecting
	 * this completion. When omitted the label is used as the insert text
	 * for this item.
	 *
	 * The `insertText` is subject to interpretation by the client side.
	 * Some tools might not take the string literally. For example
	 * VS Code when code complete is requested in this example
	 * `con<cursor position>` and a completion item with an `insertText` of
	 * `console` is provided it will only insert `sole`. Therefore it is
	 * recommended to use `textEdit` instead since it avoids additional client
	 * side interpretation.
	 */
//	insertText?: string;

	/**
	 * The format of the insert text. The format applies to both the
	 * `insertText` property and the `newText` property of a provided
	 * `textEdit`. If omitted defaults to `InsertTextFormat.PlainText`.
	 *
	 * Please note that the insertTextFormat doesn't apply to
	 * `additionalTextEdits`.
	 */
//	insertTextFormat?: InsertTextFormat;

	/**
	 * How whitespace and indentation is handled during completion
	 * item insertion. If not provided the client's default value depends on
	 * the `textDocument.completion.insertTextMode` client capability.
	 *
	 * @since 3.16.0
	 * @since 3.17.0 - support for `textDocument.completion.insertTextMode`
	 */
//	insertTextMode?: InsertTextMode;

	/**
	 * An edit which is applied to a document when selecting this completion.
	 * When an edit is provided the value of `insertText` is ignored.
	 *
	 * *Note:* The range of the edit must be a single line range and it must
	 * contain the position at which completion has been requested.
	 *
	 * Most editors support two different operations when accepting a completion
	 * item. One is to insert a completion text and the other is to replace an
	 * existing text with a completion text. Since this can usually not be
	 * predetermined by a server it can report both ranges. Clients need to
	 * signal support for `InsertReplaceEdit`s via the
	 * `textDocument.completion.completionItem.insertReplaceSupport` client
	 * capability property.
	 *
	 * *Note 1:* The text edit's range as well as both ranges from an insert
	 * replace edit must be a [single line] and they must contain the position
	 * at which completion has been requested.
	 * *Note 2:* If an `InsertReplaceEdit` is returned the edit's insert range
	 * must be a prefix of the edit's replace range, that means it must be
	 * contained and starting at the same position.
	 *
	 * @since 3.16.0 additional type `InsertReplaceEdit`
	 */
//	textEdit?: TextEdit | InsertReplaceEdit;

	/**
	 * The edit text used if the completion item is part of a CompletionList and
	 * CompletionList defines an item default for the text edit range.
	 *
	 * Clients will only honor this property if they opt into completion list
	 * item defaults using the capability `completionList.itemDefaults`.
	 *
	 * If not provided and a list's default range is provided the label
	 * property is used as a text.
	 *
	 * @since 3.17.0
	 */
//	textEditText?: string;

	/**
	 * An optional array of additional text edits that are applied when
	 * selecting this completion. Edits must not overlap (including the same
	 * insert position) with the main edit nor with themselves.
	 *
	 * Additional text edits should be used to change text unrelated to the
	 * current cursor position (for example adding an import statement at the
	 * top of the file if the completion item will insert an unqualified type).
	 */
//	additionalTextEdits?: TextEdit[];

	/**
	 * An optional set of characters that when pressed while this completion is
	 * active will accept it first and then type that character. *Note* that all
	 * commit characters should have `length=1` and that superfluous characters
	 * will be ignored.
	 */
//	commitCharacters?: string[];

	/**
	 * An optional command that is executed *after* inserting this completion.
	 * *Note* that additional modifications to the current document should be
	 * described with the additionalTextEdits-property.
	 */
//	command?: Command;

	/**
	 * A data entry field that is preserved on a completion item between
	 * a completion and a completion resolve request.
	 */
//	data?: LSPAny;
} CompletionItem;



static void add_completion(const Completer *c, const CompletionItem *ci) {
    cJSON *item = cJSON_CreateObject();
//    char *s = strchr(v->name, '@');
//    char *ret = strndup(v->name, s-v->name);
    cJSON_AddStringToObject(item, "label", ci->label);
//    if(ci->detail)
//      cJSON_AddStringToObject(item, "detail", v->type->name);
    if(ci->doc)
      cJSON_AddStringToObject(item, "documentation", ci->doc);
    cJSON_AddNumberToObject(item, "kind", ci->kind);// we could probably have smth better here
//    free(ret);
    cJSON_AddItemToArray(c->results, item);
 }

static void add_completion_value(const Completer *c, const Value v) {
  char *s = strchr(v->name, '@');
  char *ret = strndup(v->name, s-v->name);

  // TODO: complete kind function
  // (even tho we won't use all available LSP types)
  uint kind = v->from->owner_class ? CompKind_Field : CompKind_Variable;
  if(is_func(c->gwion, v->type)) {
    if(strcmp(ret, "new"))
      kind = CompKind_Constructor; // tho we could use `inside` to get methods tho
    else if(!v->from->owner_class)
      kind = CompKind_Method;
    else kind = CompKind_Function;
  } else if (is_class(c->gwion, v->type)) {
    kind = !tflag(v->type, tflag_struct)
      ? CompKind_Class
      : CompKind_Struct;
  } else if(c->inside) {
    if(c->pos.line < v->from->loc.first.line)
      return;
  }
  const m_str doc = get_doc(v->from->ctx, v);
  CompletionItem ci = {
    .label = ret,
    .doc   = doc,
    .kind = kind,
  };
  add_completion(c, &ci);
  free(ret);
}


static void nspc_completion_map(const Completer *c, const Map m) {
  for(m_uint i = 0; i < map_size(m); i++) { // run in reverse?
    const Value v = (Value)map_at(m, i);
    if(c->inside || !GET_FLAG(v, private)) { // we can move that
      if(v->type && is_func(c->gwion, v->type)) {
        const Symbol sym = (Symbol)map_key(m, i);
//        if(strcmp(s_name(sym), v->name)) continue;
      }
      add_completion_value(c, v); // beware funcs and stuff with @
    }
  }
}

static void nspc_completion_simple(const Completer *c, const Nspc nspc) {
  const Map m = &nspc->info->value->map;
  return nspc_completion_map(c, m);
}

static void nspc_completion(Completer *c, Nspc nspc) {
  do {
    nspc_completion_simple(c, nspc);
  } while((nspc = nspc->parent));
}

static void type_completion(Completer *c, Type t) {
  do {
//    message(t->name, 3);
    if(t->nspc) nspc_completion_simple(c, t->nspc);
    c->inside = false;
  } while((t = t->info->parent));
}

void lsp_completion(const Gwion gwion, int id, const cJSON *params_json) {
  TextDocumentPosition document = lsp_parse_document(params_json);

  Buffer buffer = get_buffer(document.uri);

  Complete complete = {
    .values = new_valuelist(gwion->mp, 0),
    .mp = gwion->mp,
    // TODO: use pos_t
    .line = document.pos.line,
    .character = document.pos.line
  };
   if(buffer.context && buffer.context->tree)
    complete_ast(&complete, buffer.context->tree);

  //char *text = strdup(buffer.content);
  char *text = strdup(buffer.content);
  truncate_string(text, document.pos.line, document.pos.column);
  bool isdot;
  const char *symbol_name_part  = extract_last_symbol(text, &isdot);
  message(symbol_name_part, 1);
  Completer c = {
    .gwion = gwion,
    .results = cJSON_CreateArray(),
    .pos = (pos_t){
      .line = complete.line,
      .column = complete.character,
    },
  };
  if(complete.last && isdot) {
    const Type base = complete.last->type;
    if(base) {
      c.inside = false;
      type_completion(&c, base);
    }
  } else {
  if(complete.values->len) {
    for(m_uint i = complete.values->len + 1; --i;) {
      const Value v = valuelist_at(complete.values, i - 1);
      c.inside = true;
      add_completion_value(&c, v);
    }
  }
  if(complete.class_def) {
    Type t = complete.class_def->base.type;
    c.inside = true;
    if(t) type_completion(&c, t);
  }
  if(buffer.context) {
    c.inside = true;
    const Nspc nspc = buffer.context->nspc;
    if(vector_size((Vector)nspc->info->value)) {
      m_uint m = vector_front((Vector)nspc->info->value);
      nspc_completion_map(&c, (Map)&m);
    }
    c.inside = false;
    nspc_completion(&c, nspc->parent);
  }
  }
  free(text);
  lsp_send_response(id, c.results);
}
