#include <string>

#include "parser/parser_context.h"

static void expr_collapse_freereg(FuncState *, BCReg);

static ParserError make_expr_error(ParserErrorCode Code, const Token &Token, std::string_view Message)
{
   ParserError error;
   error.code = Code;
   error.token = Token;
   error.message.assign(Message.begin(), Message.end());
   return error;
}

//********************************************************************************************************************
// Return string expression.

void LexState::expr_str(ExpDesc *Expression)
{
   expr_init(Expression, ExpKind::Str, 0);
   Expression->u.sval = this->lex_str();
}

//********************************************************************************************************************
// Return index expression.

static void expr_index(FuncState *State, ExpDesc *t, ExpDesc *e)
{
   // Already called: expr_toval(State, e).
   t->k = ExpKind::Indexed;
   if (expr_isnumk(e)) {
#if LJ_DUALNUM
      if (tvisint(expr_numtv(e))) {
         int32_t k = intV(expr_numtv(e));
         if (checku8(k)) {
            t->u.s.aux = BCMAX_C + 1 + uint32_t(k);  // 256..511: const byte key
            return;
         }
      }
#else
      lua_Number n = expr_numberV(e);
      int32_t k = lj_num2int(n);
      if (checku8(k) and n IS lua_Number(k)) {
         t->u.s.aux = BCMAX_C + 1 + uint32_t(k);  // 256..511: const byte key
         return;
      }
#endif
   }
   else if (expr_isstrk(e)) {
      BCReg idx = const_str(State, e);
      if (idx <= BCMAX_C) {
         t->u.s.aux = ~idx;  // -256..-1: const string key
         return;
      }
   }

   t->u.s.aux = expr_toanyreg(State, e);  // 0..255: register
}

//********************************************************************************************************************
// Parse index expression with named field.

void LexState::expr_field(ExpDesc* Expression)
{
   FuncState* fs = this->fs;
   ExpDesc key;
   expr_toanyreg(fs, Expression);
   this->next();  // Skip dot or colon.
   this->expr_str(&key);
   expr_index(fs, Expression, &key);
}

//********************************************************************************************************************
// Parse index expression with brackets.

void LexState::expr_bracket(ExpDesc* Expression)
{
   this->next();  // Skip '['.
   auto bracket_expr = this->expr(Expression);
   if (not bracket_expr.ok()) {
      return;
   }
   expr_toval(this->fs, Expression);
   this->lex_check(']');
}

//********************************************************************************************************************

static void expr_collapse_freereg(FuncState *State, BCReg result_reg)
{
   BCReg target = result_reg + 1;
   if (target < State->nactvar) target = BCReg(State->nactvar);
   if (State->freereg > target) State->freereg = target;
}

//********************************************************************************************************************

static int token_starts_expression(LexToken tok)
{
   switch (tok) {
   case TK_number:
   case TK_string:
   case TK_nil:
   case TK_true:
   case TK_false:
   case TK_dots:
   case TK_function:
   case TK_name:
   case '{':
   case '(':
   case TK_not:
   case TK_plusplus:
   case '-':
   case '~':
   case '#':
      return 1;
   default:
      return 0;
   }
}

//********************************************************************************************************************

bool LexState::should_emit_presence()
{
   BCLine token_line = this->lastline;
   BCLine operator_line = this->linenumber;
   LexToken lookahead = (this->lookahead != TK_eof) ? this->lookahead : this->lookahead_token();
   if (operator_line > token_line) return true;
   return !token_starts_expression(lookahead);
}

//********************************************************************************************************************
// Get value of constant expression.

static void expr_kvalue(FuncState* fs, TValue* v, ExpDesc* e)
{
   if (e->k <= ExpKind::True) {
      setpriV(v, ~uint32_t(e->k));
   }
   else if (e->k IS ExpKind::Str) {
      setgcVraw(v, obj2gco(e->u.sval), LJ_TSTR);
   }
   else {
      lj_assertFS(tvisnumber(expr_numtv(e)), "bad number constant");
      *v = *expr_numtv(e);
   }
}

//********************************************************************************************************************
// Parse table constructor expression.

void LexState::expr_table(ExpDesc* Expression)
{
   FuncState* fs = this->fs;
   BCLine line = this->linenumber;
   GCtab* t = nullptr;
   int vcall = 0, needarr = 0, fixt = 0;
   uint32_t narr = 1;  // First array index.
   uint32_t nhash = 0;  // Number of hash entries.
   BCReg freg = fs->freereg;
   BCPos pc = bcemit_AD(fs, BC_TNEW, freg, 0);
   expr_init(Expression, ExpKind::NonReloc, freg);
   bcreg_reserve(fs, 1);
   freg++;
   this->lex_check('{');
   while (this->tok != '}') {
      bool has_more = false;
      {
         RegisterGuard entry_guard(fs);
         ExpDesc key, val;
         vcall = 0;
         if (this->tok IS '[') {
            this->expr_bracket(&key);  // Already calls expr_toval.
            if (not expr_isk(&key)) expr_index(fs, Expression, &key);
            if (expr_isnumk(&key) and expr_numiszero(&key)) needarr = 1; else nhash++;
            this->lex_check('=');
         }
         else if (this->tok IS TK_name and this->lookahead_token() IS '=') {
            this->expr_str(&key);
            this->lex_check('=');
            nhash++;
         }
         else {
            expr_init(&key, ExpKind::Num, 0);
            setintV(&key.u.nval, int(narr));
            narr++;
            needarr = vcall = 1;
         }

         auto value_expr = this->expr(&val);
         if (not value_expr.ok()) {
            return;
         }

         if (expr_isk(&key) and key.k != ExpKind::Nil and
            (key.k IS ExpKind::Str or expr_isk_nojump(&val))) {
            TValue k, * v;
            if (not t) {  // Create template table on demand.
               BCReg kidx;
               t = lj_tab_new(fs->L, needarr ? narr : 0, hsize2hbits(nhash));
               kidx = const_gc(fs, obj2gco(t), LJ_TTAB);
               fs->bcbase[pc].ins = BCINS_AD(BC_TDUP, freg - 1, kidx);
            }
            vcall = 0;
            expr_kvalue(fs, &k, &key);
            v = lj_tab_set(fs->L, t, &k);
            lj_gc_anybarriert(fs->L, t);
            if (expr_isk_nojump(&val)) {  // Add const key/value to template table.
               expr_kvalue(fs, v, &val);
            }
            else {  // Otherwise create dummy string key (avoids lj_tab_newkey).
               settabV(fs->L, v, t);  // Preserve key with table itself as value.
               fixt = 1;   // Fix this later, after all resizes.
               goto nonconst;
            }
         }
         else {
         nonconst:
            if (val.k != ExpKind::Call) { expr_toanyreg(fs, &val); vcall = 0; }
            if (expr_isk(&key)) expr_index(fs, Expression, &key);
            bcemit_store(fs, Expression, &val);
         }
      }
      has_more = this->lex_opt(',');
      if (not has_more) has_more = this->lex_opt(';');
      if (not has_more) break;
   }

   this->lex_match('}', '{', line);

   if (vcall) {
      BCInsLine *ilp = &fs->bcbase[fs->pc - 1];
      ExpDesc en;
      lj_assertFS(bc_a(ilp->ins) IS freg and
         bc_op(ilp->ins) IS (narr > 256 ? BC_TSETV : BC_TSETB),
         "bad CALL code generation");
      expr_init(&en, ExpKind::Num, 0);
      en.u.nval.u32.lo = narr - 1;
      en.u.nval.u32.hi = 0x43300000;  // Biased integer to avoid denormals.
      if (narr > 256) { fs->pc--; ilp--; }
      ilp->ins = BCINS_AD(BC_TSETM, freg, const_num(fs, &en));
      setbc_b(&ilp[-1].ins, 0);
   }

   if (pc IS fs->pc - 1) {  // Make expr relocable if possible.
      Expression->u.s.info = pc;
      fs->freereg--;
      Expression->k = ExpKind::Relocable;
   }
   else {
      Expression->k = ExpKind::NonReloc;  // May have been changed by expr_index.
   }

   if (not t) {  // Construct TNEW RD:
      BCIns *ip = &fs->bcbase[pc].ins;
      if (not needarr) narr = 0;
      else if (narr < 3) narr = 3;
      else if (narr > 0x7ff) narr = 0x7ff;
      setbc_d(ip, narr | (hsize2hbits(nhash) << 11));
   }
   else {
      if (needarr and t->asize < narr)
         lj_tab_reasize(fs->L, t, narr - 1);
      if (fixt) {  // Fix value for dummy keys in template table.
         Node *node = noderef(t->node);
         uint32_t i, hmask = t->hmask;
         for (i = 0; i <= hmask; i++) {
            Node *n = &node[i];
            if (tvistab(&n->val)) {
               lj_assertFS(tabV(&n->val) IS t, "bad dummy key in template table");
               setnilV(&n->val);  // Turn value into nil.
            }
         }
      }
      lj_gc_check(fs->L);
   }
}

//********************************************************************************************************************
// Parse function parameters.

[[nodiscard]] BCReg LexState::parse_params(int NeedSelf)
{
   FuncState* fs = this->fs;
   BCReg nparams = 0;
   this->lex_check('(');
   if (NeedSelf)
      this->var_new_lit(nparams++, "self");
   if (this->tok != ')') {
      do {
         if (this->tok IS TK_name) {
            this->var_new(nparams++, this->lex_str());
         }
         else if (this->tok IS TK_dots) {
            this->next();
            fs->flags |= PROTO_VARARG;
            break;
         }
         else {
            this->err_syntax(LJ_ERR_XPARAM);
         }
      } while (this->lex_opt(','));
   }
   this->var_add(nparams);
   lj_assertFS(fs->nactvar IS nparams, "bad regalloc");
   bcreg_reserve(fs, nparams);
   this->lex_check(')');
   return nparams;
}

//********************************************************************************************************************

[[maybe_unused]] void LexState::parse_body_impl(ExpDesc* Expression, int NeedSelf,
   BCLine Line, int OptionalParams)
{
   FuncState fs, * parent_state = this->fs;
   ParserAllocator allocator = ParserAllocator::from(this->L);
   ParserConfig inherited = this->active_context ? this->active_context->config() : ParserConfig{};
   ParserContext context = ParserContext::from(*this, fs, allocator);
   ParserSession session(context, inherited);
   FuncScope bl;
   GCproto* pt;
   ptrdiff_t oldbase = parent_state->bcbase - this->bcstack;
   this->fs_init(&fs);
   fscope_begin(&fs, &bl, FuncScopeFlag::None);
   fs.linedefined = Line;
   if (OptionalParams and this->tok != '(') {
      this->assert_condition(not NeedSelf, "optional parameters require explicit self");
      fs.numparams = 0;
   }
   else {
      fs.numparams = uint8_t(this->parse_params(NeedSelf));
   }
   fs.bcbase = parent_state->bcbase + parent_state->pc;
   fs.bclim = parent_state->bclim - parent_state->pc;
   bcemit_AD(&fs, BC_FUNCF, 0, 0);  // Placeholder.
   this->parse_chunk(context);
   if (this->tok != TK_end) this->lex_match(TK_end, TK_function, Line);
   pt = this->fs_finish((this->lastline = this->linenumber));
   parent_state->bcbase = this->bcstack + oldbase;  // May have been reallocated.
   parent_state->bclim = BCPos(this->sizebcstack - oldbase);
   // Store new prototype in the constant array of the parent.
   expr_init(Expression, ExpKind::Relocable,
      bcemit_AD(parent_state, BC_FNEW, 0, const_gc(parent_state, obj2gco(pt), LJ_TPROTO)));

#if LJ_HASFFI
   parent_state->flags |= (fs.flags & PROTO_FFI);
#endif

   if (not (parent_state->flags & PROTO_CHILD)) {
      if (parent_state->flags & PROTO_HAS_RETURN)
         parent_state->flags |= PROTO_FIXUP_RETURN;
      parent_state->flags |= PROTO_CHILD;
   }
   this->next();
}

//********************************************************************************************************************
// Parse body of a function.

void LexState::parse_body(ExpDesc* Expression, int NeedSelf, BCLine Line)
{
   this->parse_body_impl(Expression, NeedSelf, Line, 0);
}

//********************************************************************************************************************
// Parse body of a defer handler where parameter list is optional.

void LexState::parse_body_defer(ExpDesc* Expression, BCLine Line)
{
   this->parse_body_impl(Expression, 0, Line, 1);
}

//********************************************************************************************************************
// Parse expression list. Last expression is left open.
//
// This function parses comma-separated expressions but deliberately leaves the last expression
// in its original ExpDesc state without discharging it. This is critical for multi-return
// function call handling.
//
// Key Behavior:
//   f(a, b, g())  where g() returns multiple values
//   - Expressions 'a' and 'b' are discharged via expr_tonextreg() to place them in registers
//   - Expression 'g()' is NOT discharged and remains as ExpKind::Call (k=13)
//   - The caller (parse_args) can then detect args.k IS ExpKind::Call and use BC_CALLM
//
// This pattern allows the calling function to receive ALL return values from g(), not just
// the first one, by using BC_CALLM instead of BC_CALL.
//
// Returns: Number of expressions in the list

ParserResult<BCReg> LexState::expr_list(ExpDesc* Expression)
{
   BCReg n = 1;
   auto first = this->expr(Expression);
   if (not first.ok()) {
      return ParserResult<BCReg>::failure(first.error_ref());
   }
   while (this->lex_opt(',')) {
      expr_tonextreg(this->fs, Expression);  // Discharge previous expressions to registers
      auto next = this->expr(Expression);                // Parse next expression (may be ExpKind::Call)
      if (not next.ok()) {
         return ParserResult<BCReg>::failure(next.error_ref());
      }
      n++;
   }
   return ParserResult<BCReg>::success(n);  // Last expression 'Expression' is NOT discharged
}

//********************************************************************************************************************
// Parse function argument list and emit function call.
//
// BC_CALL vs BC_CALLM - Multi-Return Forwarding:
//
//   BC_CALL is used when argument count is fixed:
//     f(a, b, c)  emits  BC_CALL with C field = 3 (three arguments)
//
//   BC_CALLM is used when the last argument is a multi-return function call:
//     f(a, b, g())  where g() returns multiple values
//     - Emits BC_CALLM instead of BC_CALL
//     - C field = g_base - f_base - 1 - LJ_FR2 (encodes where g()'s results start)
//     - The VM forwards ALL return values from g() to f()
//
//   Example:
//     function g() return 1, 2, 3 end
//     function f(x, y, z) print(x, y, z) end
//     f(10, g())  -- f receives (10, 1, 2, 3), uses first 3: prints "10 1 2"
//
//   Detection:
//     expr_list() leaves the last argument undischarged. If args.k IS ExpKind::Call after expr_list(),
//     we know the last argument can return multiple values, so we:
//     1. Patch the ExpKind::Call's B field to 0 (return all results)
//     2. Use BC_CALLM instead of BC_CALL
//
//   Contrast with Binary Operators:
//     Binary operators (including our bitwise shifts) use expr_binop() which discharges ExpKind::Call
//     to a single value BEFORE the operator executes. This matches standard Lua semantics:
//       x + g()  uses only the first return value of g()
//       x << g() uses only the first return value of g()
//
//     Function calls preserve multi-return:
//       f(g())   passes all return values of g() to f()

void LexState::parse_args(ExpDesc* Expression)
{
   FuncState* fs = this->fs;
   ExpDesc args;
   BCIns ins;
   BCReg base;
   BCLine line = this->linenumber;
   if (this->tok IS '(') {
#if !LJ_52
      if (line != this->lastline)
         this->err_syntax(LJ_ERR_XAMBIG);
#endif
      this->next();
      if (this->tok IS ')') {  // f().
         args.k = ExpKind::Void;
      }
      else {
         auto list = this->expr_list(&args);
         if (not list.ok()) {
            return;
         }
         if (args.k IS ExpKind::Call)  // f(a, b, g()) or f(a, b, ...).
            setbc_b(bcptr(fs, &args), 0);  // Pass on multiple results.
      }
      this->lex_match(')', '(', line);
   }
   else if (this->tok IS '{') {
      this->expr_table(&args);
   }
   else if (this->tok IS TK_string) {
      expr_init(&args, ExpKind::Str, 0);
      args.u.sval = strV(&this->tokval);
      this->next();
   }
   else {
      this->err_syntax(LJ_ERR_XFUNARG);
      return;  // Silence compiler.
   }
   lj_assertFS(Expression->k IS ExpKind::NonReloc, "bad expr type %d", int(Expression->k));
   base = Expression->u.s.info;  // Base register for call.
   if (args.k IS ExpKind::Call) {
      ins = BCINS_ABC(BC_CALLM, base, 2, args.u.s.aux - base - 1 - LJ_FR2);
   }
   else {
      if (args.k != ExpKind::Void)
         expr_tonextreg(fs, &args);
      ins = BCINS_ABC(BC_CALL, base, 2, fs->freereg - base - LJ_FR2);
   }
   expr_init(Expression, ExpKind::Call, bcemit_INS(fs, ins));
   Expression->u.s.aux = base;
   fs->bcbase[fs->pc - 1].line = line;
   fs->freereg = base + 1;  // Leave one result by default.
}

//********************************************************************************************************************

static ParserResult<ExpDesc> expr_primary_with_context(ParserContext &Context, ExpDesc *Expression)
{
   FuncState* fs = &Context.func();
   ExpDesc* v = Expression;
   Token current = Context.tokens().current();

   if (current.is(TokenKind::LeftParen)) {
      BCLine line = Context.lex().linenumber;
      Context.tokens().advance();
      auto inner = Context.lex().expr(v);
      if (not inner.ok()) {
         return inner;
      }
      Context.lex_match(')', '(', line);
      expr_discharge(fs, v);
   }
   else if (current.is_identifier()) {
      Context.lex().var_lookup(v);
   }
   else {
      ParserError error = make_expr_error(ParserErrorCode::UnexpectedToken, current, "expected expression");
      Context.emit_error(ParserErrorCode::UnexpectedToken, current, "expected expression");
      return ParserResult<ExpDesc>::failure(error);
   }

   while (true) {
      current = Context.tokens().current();
      if (current.is(TokenKind::Dot)) {
         Context.lex().expr_field(v);
      }
      else if (current.is(TokenKind::LeftBracket)) {
         ExpDesc key;
         expr_toanyreg(fs, v);
         Context.lex().expr_bracket(&key);
         expr_index(fs, v, &key);
      }
      else if (current.is(TokenKind::Colon)) {
         ExpDesc key;
         Context.tokens().advance();
         Context.lex().expr_str(&key);
         bcemit_method(fs, v, &key);
         Context.lex().parse_args(v);
      }
      else if (current.is(TokenKind::PlusPlus)) {
         Context.tokens().advance();
         Context.lex().inc_dec_op(OPR_ADD, v, 1);
      }
      else if (current.is(TokenKind::Presence) and Context.lex().should_emit_presence()) {
         Context.tokens().advance();
         bcemit_presence_check(fs, v);
      }
      else if (current.is(TokenKind::LeftParen) or current.is(TokenKind::String) or current.is(TokenKind::LeftBrace)) {
         expr_tonextreg(fs, v);
         if (LJ_FR2) bcreg_reserve(fs, 1);
         Context.lex().parse_args(v);
      }
      else {
         break;
      }
   }

   return ParserResult<ExpDesc>::success(*v);
}

//********************************************************************************************************************
// Parse primary expression.

ParserResult<ExpDesc> LexState::expr_primary(ExpDesc *Expression)
{
   if (this->active_context) {
      return expr_primary_with_context(*this->active_context, Expression);
   }
   ParserAllocator allocator = ParserAllocator::from(this->L);
   ParserContext context = ParserContext::from(*this, *this->fs, allocator);
   return expr_primary_with_context(context, Expression);
}

//********************************************************************************************************************

static ParserResult<ExpDesc> expr_simple_with_context(ParserContext &Context, ExpDesc *Expression)
{
   LexState& lex = Context.lex();
   FuncState* fs = lex.fs;
   ExpDesc* v = Expression;
   Token current = Context.tokens().current();

   switch (current.kind()) {
   case TokenKind::Number:
      expr_init(v, (LJ_HASFFI and tviscdata(&lex.tokval)) ? ExpKind::CData : ExpKind::Num, 0);
      copyTV(lex.L, &v->u.nval, &lex.tokval);
      Context.tokens().advance();
      break;
   case TokenKind::String:
      expr_init(v, ExpKind::Str, 0);
      v->u.sval = strV(&lex.tokval);
      Context.tokens().advance();
      break;
   case TokenKind::Nil:
      expr_init(v, ExpKind::Nil, 0);
      Context.tokens().advance();
      break;
   case TokenKind::TrueToken:
      expr_init(v, ExpKind::True, 0);
      Context.tokens().advance();
      break;
   case TokenKind::FalseToken:
      expr_init(v, ExpKind::False, 0);
      Context.tokens().advance();
      break;
   case TokenKind::Dots: {
      BCReg base;
      checkcond(&lex, fs->flags & PROTO_VARARG, LJ_ERR_XDOTS);
      bcreg_reserve(fs, 1);
      base = fs->freereg - 1;
      expr_init(v, ExpKind::Call, bcemit_ABC(fs, BC_VARG, base, 2, fs->numparams));
      v->u.s.aux = base;
      Context.tokens().advance();
      break;
   }
   case TokenKind::LeftBrace:
      lex.expr_table(v);
      return ParserResult<ExpDesc>::success(*v);
   case TokenKind::Function:
      Context.tokens().advance();
      lex.parse_body(v, 0, lex.linenumber);
      return ParserResult<ExpDesc>::success(*v);
   default: {
      auto primary = lex.expr_primary(v);
      if (not primary.ok()) {
         return primary;
      }
      return ParserResult<ExpDesc>::success(*v);
   }
   }

   return ParserResult<ExpDesc>::success(*v);
}

//********************************************************************************************************************

void LexState::inc_dec_op(BinOpr Operator, ExpDesc* Expression, int IsPost)
{
   BinOpr op = Operator;
   ExpDesc* v = Expression;
   int isPost = IsPost;
   FuncState* fs = this->fs;
   ExpDesc lv, e1, e2;
   BCReg indices;

   if (not v)
      v = &lv;
   indices = fs->freereg;
   expr_init(&e2, ExpKind::Num, 0);
   setintV(&e2.u.nval, 1);
   if (isPost) {
      checkcond(this, vkisvar(v->k), LJ_ERR_XNOTASSIGNABLE);
      lv = *v;
      e1 = *v;
      if (v->k IS ExpKind::Indexed)
         bcreg_reserve(fs, 1);
      expr_tonextreg(fs, v);
      // Remember that this expression was consumed as a standalone postfix increment.
      expr_set_flag(v, ExprFlag::PostfixIncStmt);
      bcreg_reserve(fs, 1);
      bcemit_arith(fs, op, &e1, &e2);
      bcemit_store(fs, &lv, &e1);
      fs->freereg--;
      return;
   }
   auto primary = this->expr_primary(v);
   if (not primary.ok()) {
      return;
   }
   checkcond(this, vkisvar(v->k), LJ_ERR_XNOTASSIGNABLE);
   e1 = *v;
   if (v->k IS ExpKind::Indexed)
      bcreg_reserve(fs, fs->freereg - indices);
   bcemit_arith(fs, op, &e1, &e2);
   bcemit_store(fs, v, &e1);
   if (v != &lv)
      expr_tonextreg(fs, v);
}

//********************************************************************************************************************
// Parse simple expression.

ParserResult<ExpDesc> LexState::expr_simple(ExpDesc* Expression)
{
   if (this->active_context) {
      return expr_simple_with_context(*this->active_context, Expression);
   }
   ParserAllocator allocator = ParserAllocator::from(this->L);
   ParserContext context = ParserContext::from(*this, *this->fs, allocator);
   return expr_simple_with_context(context, Expression);
}

//********************************************************************************************************************
// Manage syntactic levels to avoid blowing up the stack.

void LexState::synlevel_begin()
{
   if (++this->level >= LJ_MAX_XLEVEL)
      lj_lex_error(this, 0, LJ_ERR_XLEVELS);
}

void LexState::synlevel_end()
{
   this->level--;
}

//********************************************************************************************************************
// Convert token to binary operator.

[[nodiscard]] static BinOpr token2binop(LexToken tok)
{
   switch (tok) {
   case '+':	return OPR_ADD;
   case '-':	return OPR_SUB;
   case '*':	return OPR_MUL;
   case '/':	return OPR_DIV;
   case '%':	return OPR_MOD;
   case '^':	return OPR_POW;
   case TK_concat: return OPR_CONCAT;
   case TK_ne:	return OPR_NE;
   case TK_eq:	return OPR_EQ;
   case TK_is:	return OPR_EQ;
   case '<':	return OPR_LT;
   case TK_le:	return OPR_LE;
   case '>':	return OPR_GT;
   case TK_ge:	return OPR_GE;
   case '&':	return OPR_BAND;
   case '|':	return OPR_BOR;
   case '~':	return OPR_BXOR;  // Binary XOR; unary handled separately.
   case TK_shl: return OPR_SHL;
   case TK_shr: return OPR_SHR;
   case TK_and:	return OPR_AND;
   case TK_or:	return OPR_OR;
   case TK_if_empty: return OPR_IF_EMPTY;
   case '?':    return OPR_TERNARY;
   default:	return OPR_NOBINOPR;
   }
}

inline constexpr uint32_t UNARY_PRIORITY = 8;  // Priority for unary operators.

//********************************************************************************************************************
// Handle chained bitwise shift and bitwise logical operators with left-to-right associativity.
//
// This function implements left-associative chaining for bitwise operators, allowing expressions
// like `x << 2 << 3` or `x & 0xFF | 0x100` to be evaluated correctly. Without this special
// handling, these operators would be right-associative due to their priority levels.
//
// Left Associativity Examples:
//   1 << 2 << 3  evaluates as  (1 << 2) << 3  = 4 << 3 = 32
//   NOT as  1 << (2 << 3)  = 1 << 8 = 256
//
// Register Reuse Strategy:
//   All operations in the chain use the same base register for intermediate results. This is
//   more efficient than allocating new registers for each operation:
//     x << 2      -> result stored at base_reg
//     result << 3 -> reuses base_reg for both input and output
//
// Why expr_binop() is Used:
//   The RHS of each operator is parsed using expr_binop() with the operator's right priority.
//   This ensures:
//   - Lower-priority operators on the RHS bind correctly (e.g., `1 << 2 + 3` = `1 << (2+3)`)
//   - The special left-associativity logic in expr_binop() prevents consuming subsequent
//     shifts/bitops at the same level, forcing left-to-right evaluation
//
// ExpKind::Call Handling:
//   If the RHS is a ExpKind::Call (multi-return function), expr_binop() returns it as k=ExpKind::Call.
//   The function is then passed to bcemit_shift_call_at_base() which attempts to handle
//   multi-return semantics, though standard Lua binary operator rules apply (first value only).
//
// Parameters:
//   ls  - Lexer state
//   lhs - Left-hand side expression (updated with each operation's result)
//   op  - The current shift/bitwise operator (OPR_SHL, OPR_SHR, OPR_BAND, OPR_BXOR, OPR_BOR)
//        Note: Operators are only chained if they have matching precedence levels,
//        implementing C-style precedence (BAND > BXOR > BOR)
//
// Returns:
//   The next binary operator token (if any) that was not consumed by this chain

ParserResult<BinOpr> LexState::expr_shift_chain(ExpDesc* LeftHandSide, BinOpr Operator)
{
   ExpDesc* lhs = LeftHandSide;
   BinOpr op = Operator;
   FuncState* fs = this->fs;
   ExpDesc rhs;
   BCReg base_reg;

   // Parse RHS operand. expr_binop() respects priority levels and will not consume
   // another shift/bitop at the same level due to left-associativity logic in expr_binop().

   auto nextop_result = this->expr_binop(&rhs, priority[op].right);
   if (not nextop_result.ok()) {
      return nextop_result;
   }
   BinOpr nextop = nextop_result.value_ref();

   // Choose the base register for the bit operation call.
   //
   // To avoid orphaning intermediate results (which become extra return values),
   // we prioritize reusing registers that are already at the top of the stack:
   //
   // 1. If LHS is at the top (lhs->u.s.info + 1 IS fs->freereg), reuse it.
   //    This happens when chaining across precedence levels: e.g., after "1 & 2"
   //    completes in reg N and freereg becomes N+1, then "| 4" finds LHS at the top.
   // 2. Otherwise, if RHS is at the top, reuse it for compactness.
   // 3. Otherwise, allocate a fresh register.

   if (lhs->k IS ExpKind::NonReloc and lhs->u.s.info >= fs->nactvar and
      lhs->u.s.info + 1 IS fs->freereg) {
      // LHS result from previous operation is at the top - reuse it to avoid orphaning
      base_reg = lhs->u.s.info;
   }
   else if (rhs.k IS ExpKind::NonReloc and rhs.u.s.info >= fs->nactvar and
      rhs.u.s.info + 1 IS fs->freereg) {
      // RHS is at the top - reuse it
      base_reg = rhs.u.s.info;
   }
   else {
      // Allocate a fresh register
      base_reg = fs->freereg;
   }

   // Reserve space for: callee (1), frame link if x64 (LJ_FR2), and two arguments (2).
   bcreg_reserve(fs, 1);  // Reserve for callee
   if (LJ_FR2) bcreg_reserve(fs, 1);  // Reserve for frame link on x64
   bcreg_reserve(fs, 2);  // Reserve for arguments

   // Emit the first operation in the chain
   bcemit_shift_call_at_base(fs, std::string_view(priority[op].name, priority[op].name_len), lhs, &rhs, base_reg);

   // Continue processing chained operators at the same precedence level.
   // Example: for `x << 2 >> 3 << 4`, this loop handles `>> 3 << 4`
   // C-style precedence is enforced by checking that operators have matching precedence before chaining

   while (nextop IS OPR_SHL or nextop IS OPR_SHR or nextop IS OPR_BAND or nextop IS OPR_BXOR or nextop IS OPR_BOR) {
      BinOpr follow = nextop;
      // Only chain operators with matching left precedence (same precedence level)
      if (priority[follow].left != priority[op].left) break;
      this->next();  // Consume the operator token

      /* Update lhs to point to base_reg where the previous result is stored.
      ** This makes the previous result the input for the next operation. */
      lhs->k = ExpKind::NonReloc;
      lhs->u.s.info = base_reg;

      // Parse the next RHS operand
      auto chained = this->expr_binop(&rhs, priority[follow].right);
      if (not chained.ok()) {
         return chained;
      }
      nextop = chained.value_ref();

      // Emit the next operation, reusing the same base register
      bcemit_shift_call_at_base(fs, std::string_view(priority[follow].name, priority[follow].name_len), lhs, &rhs, base_reg);
   }

   // Return any unconsumed operator for the caller to handle
   return ParserResult<BinOpr>::success(nextop);
}

//********************************************************************************************************************
// Parse unary expression.

ParserResult<ExpDesc> LexState::expr_unop(ExpDesc* Expression)
{
   ExpDesc* v = Expression;
   BCOp op;
   if (this->tok IS TK_not) {
      op = BC_NOT;
   }
   else if (this->tok IS '-') {
      op = BC_UNM;
   }
   else if (this->tok IS '~') {
      // Unary bitwise not: desugar to bit.bnot(x).
      this->next();
      auto bit_result = this->expr_binop(v, UNARY_PRIORITY);
      if (not bit_result.ok()) {
         return ParserResult<ExpDesc>::failure(bit_result.error_ref());
      }
      bcemit_unary_bit_call(this->fs, "bnot", v);
      return ParserResult<ExpDesc>::success(*v);
   }
   else if (this->tok IS '#') {
      op = BC_LEN;
   }
   else {
      auto simple = this->expr_simple(v);
      if (not simple.ok()) {
        return simple;
      }
      // Check for postfix presence check operator after simple expressions (constants)
      if (this->tok IS TK_if_empty and this->should_emit_presence()) {
         this->next();
         bcemit_presence_check(this->fs, v);
      }
      return ParserResult<ExpDesc>::success(*v);
   }
   this->next();
   auto unary = this->expr_binop(v, UNARY_PRIORITY);
   if (not unary.ok()) {
      return ParserResult<ExpDesc>::failure(unary.error_ref());
   }
   bcemit_unop(this->fs, op, v);
   return ParserResult<ExpDesc>::success(*v);
}

//********************************************************************************************************************
// Parse binary expressions with priority higher than the limit.

ParserResult<BinOpr> LexState::expr_binop(ExpDesc* Expression, uint32_t Limit)
{
   ExpDesc* v = Expression;
   uint32_t limit = Limit;
   BinOpr op;
   this->synlevel_begin();
   auto unary = this->expr_unop(v);
   if (not unary.ok()) {
      this->synlevel_end();
      return ParserResult<BinOpr>::failure(unary.error_ref());
   }
   op = token2binop(this->tok);
   while (op != OPR_NOBINOPR) {
      uint8_t lpri = priority[op].left;
      if (limit IS priority[op].right and
         (op IS OPR_SHL or op IS OPR_SHR or
            op IS OPR_BOR or op IS OPR_BXOR or op IS OPR_BAND))
         lpri = 0;

      if (not (lpri > limit)) break;

      this->next();

      if (op IS OPR_TERNARY) {
         FuncState* fs = this->fs;
         ExpDesc nilv = make_nil_expr();
         ExpDesc falsev = make_bool_expr(false);
         ExpDesc zerov = make_num_expr(0.0);
         ExpDesc emptyv = make_interned_string_expr(this->intern_empty_string());
         BCReg cond_reg, result_reg;
         BCPos check_nil, check_false, check_zero, check_empty;
         BCPos skip_false;

         expr_discharge(fs, v);
         cond_reg = expr_toanyreg(fs, v);
         result_reg = cond_reg;

         this->ternary_depth++;

         bcemit_INS(fs, BCINS_AD(BC_ISNEP, cond_reg, const_pri(&nilv)));
         check_nil = bcemit_jmp(fs);
         bcemit_INS(fs, BCINS_AD(BC_ISNEP, cond_reg, const_pri(&falsev)));
         check_false = bcemit_jmp(fs);
         bcemit_INS(fs, BCINS_AD(BC_ISNEN, cond_reg, const_num(fs, &zerov)));
         check_zero = bcemit_jmp(fs);
         bcemit_INS(fs, BCINS_AD(BC_ISNES, cond_reg, const_str(fs, &emptyv)));
         check_empty = bcemit_jmp(fs);

         {
            ExpDesc v2;
            auto branch = this->expr_binop(&v2, priority[OPR_IF_EMPTY].right);
            if (not branch.ok()) {
               this->synlevel_end();
               return ParserResult<BinOpr>::failure(branch.error_ref());
            }
            expr_discharge(fs, &v2);
            expr_toreg(fs, &v2, result_reg);
            expr_collapse_freereg(fs, result_reg);
         }

         skip_false = bcemit_jmp(fs);

         this->lex_check(TK_ternary_sep);
         this->assert_condition(this->ternary_depth > 0, "ternary depth underflow");
         this->ternary_depth--;

         {
            BCPos false_start = fs->pc;
            JumpListView(fs, check_nil).patch_to(false_start);
            JumpListView(fs, check_false).patch_to(false_start);
            JumpListView(fs, check_zero).patch_to(false_start);
            JumpListView(fs, check_empty).patch_to(false_start);
         }

         {
            ExpDesc fexp;
            auto false_branch = this->expr_binop(&fexp, priority[OPR_IF_EMPTY].right);
            if (not false_branch.ok()) {
               this->synlevel_end();
               return ParserResult<BinOpr>::failure(false_branch.error_ref());
            }
            BinOpr nextop3 = false_branch.value_ref();
            expr_discharge(fs, &fexp);
            expr_toreg(fs, &fexp, result_reg);
            expr_collapse_freereg(fs, result_reg);
            JumpListView(fs, skip_false).patch_to(fs->pc);
            v->u.s.info = result_reg;
            v->k = ExpKind::NonReloc;
            op = nextop3;
            continue;
         }
      }

      bcemit_binop_left(this->fs, op, v);

      if ((op IS OPR_SHL) or (op IS OPR_SHR) or (op IS OPR_BAND) or (op IS OPR_BXOR) or (op IS OPR_BOR)) {
         auto shift = this->expr_shift_chain(v, op);
         if (not shift.ok()) {
            this->synlevel_end();
            return ParserResult<BinOpr>::failure(shift.error_ref());
         }
         op = shift.value_ref();
         continue;
      }

      ExpDesc v2;
      auto nextop_result = this->expr_binop(&v2, priority[op].right);
      if (not nextop_result.ok()) {
         this->synlevel_end();
         return ParserResult<BinOpr>::failure(nextop_result.error_ref());
      }
      BinOpr nextop = nextop_result.value_ref();

      if (op IS OPR_IF_EMPTY and this->ternary_depth IS 0 and
         (this->tok IS TK_ternary_sep or this->pending_if_empty_colon)) {
         FuncState* fs = this->fs;

         this->pending_if_empty_colon = 0;

         if (v->t != NO_JMP) {
            JumpListView(fs, v->t).patch_to(fs->pc);
            v->t = NO_JMP;
         }

         if (expr_consume_flag(v, ExprFlag::HasRhsReg)) {
            BCReg rhs_reg = BCReg(v->u.s.aux);
            if (rhs_reg >= fs->nactvar and rhs_reg < fs->freereg) {
               fs->freereg = rhs_reg;
            }
         }

         expr_discharge(fs, &v2);
         expr_free(fs, &v2);

         BCReg base = fs->freereg;
         BCReg arg_reg = base + 1 + LJ_FR2;

         bcreg_reserve(fs, 1);
         if (LJ_FR2) bcreg_reserve(fs, 1);
         bcreg_reserve(fs, 1);

         {
            ExpDesc callee;
            expr_init(&callee, ExpKind::Str, 0);
            callee.u.sval = this->keepstr("error");
            bcemit_INS(fs, BCINS_AD(BC_GGET, base, const_str(fs, &callee)));
         }

         {
            ExpDesc message;
            expr_init(&message, ExpKind::Str, 0);
            message.u.sval = this->keepstr("Invalid ternary mix: use '?' with ':>'");
            bcemit_INS(fs, BCINS_AD(BC_KSTR, arg_reg, const_str(fs, &message)));
         }

         if (fs->freereg <= arg_reg) fs->freereg = arg_reg + 1;

         v->k = ExpKind::Call;
         v->u.s.info = bcemit_INS(fs, BCINS_ABC(BC_CALL, base, 2, fs->freereg - base - LJ_FR2));
         v->u.s.aux = base;
         fs->freereg = base + 1;
         expr_discharge(fs, v);

         this->next();

         {
            ExpDesc dummy;
            auto after = this->expr_binop(&dummy, priority[OPR_IF_EMPTY].right);
            if (not after.ok()) {
               this->synlevel_end();
               return ParserResult<BinOpr>::failure(after.error_ref());
            }
            expr_discharge(fs, &dummy);
            expr_free(fs, &dummy);
            op = after.value_ref();
         }

         continue;
      }

      bcemit_binop(this->fs, op, v, &v2);
      op = nextop;
   }
   if (this->tok IS TK_ternary_sep and this->ternary_depth IS 0) {
      if (limit IS priority[OPR_IF_EMPTY].right) {
         this->pending_if_empty_colon = 1;
         this->synlevel_end();
         return ParserResult<BinOpr>::success(op);
      }
      this->synlevel_end();
      this->err_syntax(LJ_ERR_XSYMBOL);
   }
   this->synlevel_end();
   return ParserResult<BinOpr>::success(op);
}

//********************************************************************************************************************
// Parse expression.

ParserResult<ExpDesc> LexState::expr(ExpDesc* Expression)
{
   auto result = this->expr_binop(Expression, 0);  // Priority 0: parse whole expression.
   if (not result.ok()) {
      return ParserResult<ExpDesc>::failure(result.error_ref());
   }
   return ParserResult<ExpDesc>::success(*Expression);
}

//********************************************************************************************************************
// Assign expression to the next register.

ParserResult<ExpDesc> LexState::expr_next()
{
   ExpDesc expression;
   auto result = this->expr(&expression);
   if (not result.ok()) {
      return result;
   }
   expr_tonextreg(this->fs, &expression);
    return ParserResult<ExpDesc>::success(expression);
}

//********************************************************************************************************************
// Parse conditional expression.

ParserResult<BCPos> LexState::expr_cond()
{
   ExpDesc condition;
   auto result = this->expr(&condition);
   if (not result.ok()) {
      return ParserResult<BCPos>::failure(result.error_ref());
   }
   if (condition.k IS ExpKind::Nil) condition.k = ExpKind::False;
   bcemit_branch_t(this->fs, &condition);
   return ParserResult<BCPos>::success(condition.f);
}
