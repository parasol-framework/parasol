
static void expr(LexState *State, ExpDesc* v);
static void parse_args(LexState *State, ExpDesc* e);
static void bcemit_presence_check(FuncState* fs, ExpDesc* e);
static void bcemit_binop(FuncState* fs, BinOpr op, ExpDesc* e1, ExpDesc* e2);
static void bcemit_unop(FuncState* fs, BCOp op, ExpDesc* e);
static void bcemit_arith(FuncState* fs, BinOpr opr, ExpDesc* e1, ExpDesc* e2);
static void bcemit_shift_call_at_base(FuncState* fs, const char* fname, MSize fname_len, ExpDesc* lhs, ExpDesc* rhs, BCReg base);
static void bcemit_unary_bit_call(FuncState* fs, const char* fname, MSize fname_len, ExpDesc* arg);
static void bcemit_binop_left(FuncState* fs, BinOpr op, ExpDesc* e);
static void expr_collapse_freereg(FuncState* fs, BCReg result_reg);

//********************************************************************************************************************
// Return string expression.

static void expr_str(LexState *State, ExpDesc* e)
{
   expr_init(e, ExpKind::Str, 0);
   e->u.sval = lex_str(State);
}

//********************************************************************************************************************
// Return index expression.

static void expr_index(FuncState* fs, ExpDesc* t, ExpDesc* e)
{
   // Already called: expr_toval(fs, e).
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
      if (checku8(k) and n == lua_Number(k)) {
         t->u.s.aux = BCMAX_C + 1 + uint32_t(k);  // 256..511: const byte key
         return;
      }
#endif
   }
   else if (expr_isstrk(e)) {
      BCReg idx = const_str(fs, e);
      if (idx <= BCMAX_C) {
         t->u.s.aux = ~idx;  // -256..-1: const string key
         return;
      }
   }
   t->u.s.aux = expr_toanyreg(fs, e);  // 0..255: register
}

// Parse index expression with named field.

static void expr_field(LexState *State, ExpDesc* v)
{
   FuncState* fs = State->fs;
   ExpDesc key;
   expr_toanyreg(fs, v);
   lj_lex_next(State);  // Skip dot or colon.
   expr_str(State, &key);
   expr_index(fs, v, &key);
}

//********************************************************************************************************************
// Parse index expression with brackets.

static void expr_bracket(LexState *State, ExpDesc* v)
{
   lj_lex_next(State);  // Skip '['.
   expr(State, v);
   expr_toval(State->fs, v);
   lex_check(State, ']');
}

//********************************************************************************************************************

static void expr_collapse_freereg(FuncState* fs, BCReg result_reg)
{
   BCReg target = result_reg + 1;
   if (target < fs->nactvar) target = BCReg(fs->nactvar);
   if (fs->freereg > target) fs->freereg = target;
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

static int should_emit_presence(LexState *State)
{
   BCLine token_line = State->lastline;
   BCLine operator_line = State->linenumber;
   LexToken lookahead = (State->lookahead != TK_eof) ? State->lookahead : lj_lex_lookahead(State);
   if (operator_line > token_line) return 1;
   return !token_starts_expression(lookahead);
}

//********************************************************************************************************************
// Get value of constant expression.

static void expr_kvalue(FuncState* fs, TValue* v, ExpDesc* e)
{
   UNUSED(fs);
   if (e->k <= ExpKind::True) {
      setpriV(v, ~uint32_t(e->k));
   }
   else if (e->k == ExpKind::Str) {
      setgcVraw(v, obj2gco(e->u.sval), LJ_TSTR);
   }
   else {
      lj_assertFS(tvisnumber(expr_numtv(e)), "bad number constant");
      *v = *expr_numtv(e);
   }
}

//********************************************************************************************************************
// Parse table constructor expression.

static void expr_table(LexState *State, ExpDesc* e)
{
   FuncState* fs = State->fs;
   BCLine line = State->linenumber;
   GCtab* t = nullptr;
   int vcall = 0, needarr = 0, fixt = 0;
   uint32_t narr = 1;  // First array index.
   uint32_t nhash = 0;  // Number of hash entries.
   BCReg freg = fs->freereg;
   BCPos pc = bcemit_AD(fs, BC_TNEW, freg, 0);
   expr_init(e, ExpKind::NonReloc, freg);
   bcreg_reserve(fs, 1);
   freg++;
   lex_check(State, '{');
   while (State->tok != '}') {
      ExpDesc key, val;
      vcall = 0;
      if (State->tok == '[') {
         expr_bracket(State, &key);  // Already calls expr_toval.
         if (!expr_isk(&key)) expr_index(fs, e, &key);
         if (expr_isnumk(&key) and expr_numiszero(&key)) needarr = 1; else nhash++;
         lex_check(State, '=');
      }
      else if (State->tok == TK_name and lj_lex_lookahead(State) == '=') {
         expr_str(State, &key);
         lex_check(State, '=');
         nhash++;
      }
      else {
         expr_init(&key, ExpKind::Num, 0);
         setintV(&key.u.nval, int(narr));
         narr++;
         needarr = vcall = 1;
      }

      expr(State, &val);

      if (expr_isk(&key) and key.k != ExpKind::Nil &&
         (key.k == ExpKind::Str or expr_isk_nojump(&val))) {
         TValue k, * v;
         if (!t) {  // Create template table on demand.
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
         if (expr_isk(&key)) expr_index(fs, e, &key);
         bcemit_store(fs, e, &val);
      }
      fs->freereg = freg;
      if (!lex_opt(State, ',') and !lex_opt(State, ';')) break;
   }

   lex_match(State, '}', '{', line);

   if (vcall) {
      BCInsLine* ilp = &fs->bcbase[fs->pc - 1];
      ExpDesc en;
      lj_assertFS(bc_a(ilp->ins) == freg &&
         bc_op(ilp->ins) == (narr > 256 ? BC_TSETV : BC_TSETB),
         "bad CALL code generation");
      expr_init(&en, ExpKind::Num, 0);
      en.u.nval.u32.lo = narr - 1;
      en.u.nval.u32.hi = 0x43300000;  // Biased integer to avoid denormals.
      if (narr > 256) { fs->pc--; ilp--; }
      ilp->ins = BCINS_AD(BC_TSETM, freg, const_num(fs, &en));
      setbc_b(&ilp[-1].ins, 0);
   }

   if (pc == fs->pc - 1) {  // Make expr relocable if possible.
      e->u.s.info = pc;
      fs->freereg--;
      e->k = ExpKind::Relocable;
   }
   else {
      e->k = ExpKind::NonReloc;  // May have been changed by expr_index.
   }

   if (!t) {  // Construct TNEW RD: hhhhhaaaaaaaaaaa.
      BCIns* ip = &fs->bcbase[pc].ins;
      if (!needarr) narr = 0;
      else if (narr < 3) narr = 3;
      else if (narr > 0x7ff) narr = 0x7ff;
      setbc_d(ip, narr | (hsize2hbits(nhash) << 11));
   }
   else {
      if (needarr and t->asize < narr)
         lj_tab_reasize(fs->L, t, narr - 1);
      if (fixt) {  // Fix value for dummy keys in template table.
         Node* node = noderef(t->node);
         uint32_t i, hmask = t->hmask;
         for (i = 0; i <= hmask; i++) {
            Node* n = &node[i];
            if (tvistab(&n->val)) {
               lj_assertFS(tabV(&n->val) == t, "bad dummy key in template table");
               setnilV(&n->val);  // Turn value into nil.
            }
         }
      }
      lj_gc_check(fs->L);
   }
}

//********************************************************************************************************************
// Parse function parameters.

[[nodiscard]] static BCReg parse_params(LexState *State, int needself)
{
   FuncState* fs = State->fs;
   BCReg nparams = 0;
   lex_check(State, '(');
   if (needself)
      var_new_lit(State, nparams++, "self", sizeof("self")-1);
   if (State->tok != ')') {
      do {
         if (State->tok == TK_name) {
            var_new(State, nparams++, lex_str(State));
         }
         else if (State->tok == TK_dots) {
            lj_lex_next(State);
            fs->flags |= PROTO_VARARG;
            break;
         }
         else {
            err_syntax(State, LJ_ERR_XPARAM);
         }
      } while (lex_opt(State, ','));
   }
   var_add(State, nparams);
   lj_assertFS(fs->nactvar == nparams, "bad regalloc");
   bcreg_reserve(fs, nparams);
   lex_check(State, ')');
   return nparams;
}

//********************************************************************************************************************

static void parse_chunk(LexState* ls);

static void parse_body_impl(LexState *State, ExpDesc* e, int needself,
   BCLine line, int optparams)
{
   FuncState fs, * pfs = State->fs;
   FuncScope bl;
   GCproto* pt;
   ptrdiff_t oldbase = pfs->bcbase - State->bcstack;
   fs_init(State, &fs);
   fscope_begin(&fs, &bl, 0);
   fs.linedefined = line;
   if (optparams and State->tok != '(') {
      lj_assertLS(!needself, "optional parameters require explicit self");
      fs.numparams = 0;
   }
   else {
      fs.numparams = uint8_t(parse_params(State, needself));
   }
   fs.bcbase = pfs->bcbase + pfs->pc;
   fs.bclim = pfs->bclim - pfs->pc;
   bcemit_AD(&fs, BC_FUNCF, 0, 0);  // Placeholder.
   parse_chunk(State);
   if (State->tok != TK_end) lex_match(State, TK_end, TK_function, line);
   pt = fs_finish(State, (State->lastline = State->linenumber));
   pfs->bcbase = State->bcstack + oldbase;  // May have been reallocated.
   pfs->bclim = BCPos(State->sizebcstack - oldbase);
   // Store new prototype in the constant array of the parent.
   expr_init(e, ExpKind::Relocable, bcemit_AD(pfs, BC_FNEW, 0, const_gc(pfs, obj2gco(pt), LJ_TPROTO)));

#if LJ_HASFFI
   pfs->flags |= (fs.flags & PROTO_FFI);
#endif

   if (!(pfs->flags & PROTO_CHILD)) {
      if (pfs->flags & PROTO_HAS_RETURN)
         pfs->flags |= PROTO_FIXUP_RETURN;
      pfs->flags |= PROTO_CHILD;
   }
   lj_lex_next(State);
}

//********************************************************************************************************************
// Parse body of a function.

static void parse_body(LexState *State, ExpDesc* e, int needself, BCLine line)
{
   parse_body_impl(State, e, needself, line, 0);
}

//********************************************************************************************************************
// Parse body of a defer handler where parameter list is optional.

static void parse_body_defer(LexState *State, ExpDesc* e, BCLine line)
{
   parse_body_impl(State, e, 0, line, 1);
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
//   - The caller (parse_args) can then detect args.k == ExpKind::Call and use BC_CALLM
//
// This pattern allows the calling function to receive ALL return values from g(), not just
// the first one, by using BC_CALLM instead of BC_CALL.
//
// Returns: Number of expressions in the list

[[nodiscard]] static BCReg expr_list(LexState *State, ExpDesc* v)
{
   BCReg n = 1;
   expr(State, v);
   while (lex_opt(State, ',')) {
      expr_tonextreg(State->fs, v);  // Discharge previous expressions to registers
      expr(State, v);                // Parse next expression (may be ExpKind::Call)
      n++;
   }
   return n;  // Last expression 'v' is NOT discharged
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
//     expr_list() leaves the last argument undischarged. If args.k == ExpKind::Call after expr_list(),
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

static void parse_args(LexState *State, ExpDesc* e)
{
   FuncState* fs = State->fs;
   ExpDesc args;
   BCIns ins;
   BCReg base;
   BCLine line = State->linenumber;
   if (State->tok == '(') {
#if !LJ_52
      if (line != State->lastline)
         err_syntax(State, LJ_ERR_XAMBIG);
#endif
      lj_lex_next(State);
      if (State->tok == ')') {  // f().
         args.k = ExpKind::Void;
      }
      else {
         (void)expr_list(State, &args);
         if (args.k == ExpKind::Call)  // f(a, b, g()) or f(a, b, ...).
            setbc_b(bcptr(fs, &args), 0);  // Pass on multiple results.
      }
      lex_match(State, ')', '(', line);
   }
   else if (State->tok == '{') {
      expr_table(State, &args);
   }
   else if (State->tok == TK_string) {
      expr_init(&args, ExpKind::Str, 0);
      args.u.sval = strV(&State->tokval);
      lj_lex_next(State);
   }
   else {
      err_syntax(State, LJ_ERR_XFUNARG);
      return;  // Silence compiler.
   }
   lj_assertFS(e->k == ExpKind::NonReloc, "bad expr type %d", static_cast<int>(e->k));
   base = e->u.s.info;  // Base register for call.
   if (args.k == ExpKind::Call) {
      ins = BCINS_ABC(BC_CALLM, base, 2, args.u.s.aux - base - 1 - LJ_FR2);
   }
   else {
      if (args.k != ExpKind::Void)
         expr_tonextreg(fs, &args);
      ins = BCINS_ABC(BC_CALL, base, 2, fs->freereg - base - LJ_FR2);
   }
   expr_init(e, ExpKind::Call, bcemit_INS(fs, ins));
   e->u.s.aux = base;
   fs->bcbase[fs->pc - 1].line = line;
   fs->freereg = base + 1;  // Leave one result by default.
}

static void inc_dec_op(LexState *State, BinOpr op, ExpDesc* v, int isPost);

//********************************************************************************************************************
// Parse primary expression.

static void expr_primary(LexState *State, ExpDesc* v)
{
   FuncState* fs = State->fs;
   // Parse prefix expression.
   if (State->tok == '(') {
      BCLine line = State->linenumber;
      lj_lex_next(State);
      expr(State, v);
      lex_match(State, ')', '(', line);
      expr_discharge(State->fs, v);
   }
   else if (State->tok == TK_name) {
      var_lookup(State, v);
   }
   else {
      err_syntax(State, LJ_ERR_XSYMBOL);
   }
   for (;;) {  // Parse multiple expression suffixes.
      if (State->tok == '.') {
         expr_field(State, v);
      }
      else if (State->tok == '[') {
         ExpDesc key;
         expr_toanyreg(fs, v);
         expr_bracket(State, &key);
         expr_index(fs, v, &key);
      }
      else if (State->tok == ':') {
         ExpDesc key;
         lj_lex_next(State);
         expr_str(State, &key);
         bcemit_method(fs, v, &key);
         parse_args(State, v);
      }
      else if (State->tok == TK_plusplus) {
         lj_lex_next(State);
         inc_dec_op(State, OPR_ADD, v, 1);
      }
      else if (State->tok == TK_if_empty and should_emit_presence(State)) {
         // Postfix presence check operator: x??
         lj_lex_next(State);  // Consume '??'
         bcemit_presence_check(fs, v);
      }
      else if (State->tok == '(' or State->tok == TK_string or State->tok == '{') {
         expr_tonextreg(fs, v);
         if (LJ_FR2) bcreg_reserve(fs, 1);
         parse_args(State, v);
      }
      else {
         break;
      }
   }
}

//********************************************************************************************************************

static void inc_dec_op(LexState *State, BinOpr op, ExpDesc* v, int isPost)
{
   FuncState* fs = State->fs;
   ExpDesc lv, e1, e2;
   BCReg indices;

   if (!v)
      v = &lv;
   indices = fs->freereg;
   expr_init(&e2, ExpKind::Num, 0);
   setintV(&e2.u.nval, 1);
   if (isPost) {
      checkcond(State, vkisvar(v->k), LJ_ERR_XNOTASSIGNABLE);
      lv = *v;
      e1 = *v;
      if (v->k == ExpKind::Indexed)
         bcreg_reserve(fs, 1);
      expr_tonextreg(fs, v);
      // Remember that this expression was consumed as a standalone postfix increment.
      v->flags |= POSTFIX_INC_STMT_FLAG;
      bcreg_reserve(fs, 1);
      bcemit_arith(fs, op, &e1, &e2);
      bcemit_store(fs, &lv, &e1);
      fs->freereg--;
      return;
   }
   expr_primary(State, v);
   checkcond(State, vkisvar(v->k), LJ_ERR_XNOTASSIGNABLE);
   e1 = *v;
   if (v->k == ExpKind::Indexed)
      bcreg_reserve(fs, fs->freereg - indices);
   bcemit_arith(fs, op, &e1, &e2);
   bcemit_store(fs, v, &e1);
   if (v != &lv)
      expr_tonextreg(fs, v);
}

//********************************************************************************************************************
// Parse simple expression.

static void expr_simple(LexState *State, ExpDesc* v)
{
   switch (State->tok) {
   case TK_number:
      expr_init(v, (LJ_HASFFI and tviscdata(&State->tokval)) ? ExpKind::CData : ExpKind::Num, 0);
      copyTV(State->L, &v->u.nval, &State->tokval);
      lj_lex_next(State);
      break;
   case TK_string:
      expr_init(v, ExpKind::Str, 0);
      v->u.sval = strV(&State->tokval);
      lj_lex_next(State);
      break;
   case TK_nil:
      expr_init(v, ExpKind::Nil, 0);
      lj_lex_next(State);
      break;
   case TK_true:
      expr_init(v, ExpKind::True, 0);
      lj_lex_next(State);
      break;
   case TK_false:
      expr_init(v, ExpKind::False, 0);
      lj_lex_next(State);
      break;
   case TK_dots: {  // Vararg.
      FuncState* fs = State->fs;
      BCReg base;
      checkcond(State, fs->flags & PROTO_VARARG, LJ_ERR_XDOTS);
      bcreg_reserve(fs, 1);
      base = fs->freereg - 1;
      expr_init(v, ExpKind::Call, bcemit_ABC(fs, BC_VARG, base, 2, fs->numparams));
      v->u.s.aux = base;
      lj_lex_next(State);
      break;
   }
   case '{':  // Table constructor.
      expr_table(State, v);
      return;
   case TK_function:
      lj_lex_next(State);
      parse_body(State, v, 0, State->linenumber);
      return;
   default:
      expr_primary(State, v);
      return;
   }
}

//********************************************************************************************************************
// Manage syntactic levels to avoid blowing up the stack.

static void synlevel_begin(LexState *State)
{
   if (++State->level >= LJ_MAX_XLEVEL)
      lj_lex_error(State, 0, LJ_ERR_XLEVELS);
}

static inline void synlevel_end(LexState *State) {
   State->level--;
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

// Forward declaration.
static BinOpr expr_binop(LexState *State, ExpDesc* v, uint32_t limit);

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

static BinOpr expr_shift_chain(LexState *State, ExpDesc* lhs, BinOpr op)
{
   FuncState* fs = State->fs;
   ExpDesc rhs;
   BinOpr nextop;
   BCReg base_reg;

   // Parse RHS operand. expr_binop() respects priority levels and will not consume
   // another shift/bitop at the same level due to left-associativity logic in expr_binop().

   nextop = expr_binop(State, &rhs, priority[op].right);

   // Choose the base register for the bit operation call.
   //
   // To avoid orphaning intermediate results (which become extra return values),
   // we prioritize reusing registers that are already at the top of the stack:
   //
   // 1. If LHS is at the top (lhs->u.s.info + 1 == fs->freereg), reuse it.
   //    This happens when chaining across precedence levels: e.g., after "1 & 2"
   //    completes in reg N and freereg becomes N+1, then "| 4" finds LHS at the top.
   // 2. Otherwise, if RHS is at the top, reuse it for compactness.
   // 3. Otherwise, allocate a fresh register.

   if (lhs->k == ExpKind::NonReloc and lhs->u.s.info >= fs->nactvar &&
      lhs->u.s.info + 1 == fs->freereg) {
      // LHS result from previous operation is at the top - reuse it to avoid orphaning
      base_reg = lhs->u.s.info;
   }
   else if (rhs.k == ExpKind::NonReloc and rhs.u.s.info >= fs->nactvar &&
      rhs.u.s.info + 1 == fs->freereg) {
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
   bcemit_shift_call_at_base(fs, priority[op].name, MSize(priority[op].name_len), lhs, &rhs, base_reg);

   // Continue processing chained operators at the same precedence level.
   // Example: for `x << 2 >> 3 << 4`, this loop handles `>> 3 << 4`
   // C-style precedence is enforced by checking that operators have matching precedence before chaining

   while (nextop == OPR_SHL or nextop == OPR_SHR or nextop == OPR_BAND or nextop == OPR_BXOR or nextop == OPR_BOR) {
      BinOpr follow = nextop;
      // Only chain operators with matching left precedence (same precedence level)
      if (priority[follow].left != priority[op].left) break;
      lj_lex_next(State);  // Consume the operator token

      /* Update lhs to point to base_reg where the previous result is stored.
      ** This makes the previous result the input for the next operation. */
      lhs->k = ExpKind::NonReloc;
      lhs->u.s.info = base_reg;

      // Parse the next RHS operand
      nextop = expr_binop(State, &rhs, priority[follow].right);

      // Emit the next operation, reusing the same base register
      bcemit_shift_call_at_base(fs, priority[follow].name, MSize(priority[follow].name_len), lhs, &rhs, base_reg);
   }

   // Return any unconsumed operator for the caller to handle
   return nextop;
}

//********************************************************************************************************************
// Parse unary expression.

static void expr_unop(LexState *State, ExpDesc* v)
{
   BCOp op;
   if (State->tok == TK_not) {
      op = BC_NOT;
   }
   else if (State->tok == '-') {
      op = BC_UNM;
   }
   else if (State->tok == '~') {
      // Unary bitwise not: desugar to bit.bnot(x).
      lj_lex_next(State);
      expr_binop(State, v, UNARY_PRIORITY);
      bcemit_unary_bit_call(State->fs, "bnot", 4, v);
      return;
   }
   else if (State->tok == '#') {
      op = BC_LEN;
   }
   else {
      expr_simple(State, v);
      // Check for postfix presence check operator after simple expressions (constants)
      if (State->tok == TK_if_empty and should_emit_presence(State)) {
         lj_lex_next(State);
         bcemit_presence_check(State->fs, v);
      }
      return;
   }
   lj_lex_next(State);
   expr_binop(State, v, UNARY_PRIORITY);
   bcemit_unop(State->fs, op, v);
}

//********************************************************************************************************************
// Parse binary expressions with priority higher than the limit.

static BinOpr expr_binop(LexState *State, ExpDesc* v, uint32_t limit)
{
   BinOpr op;
   synlevel_begin(State);
   expr_unop(State, v);
   op = token2binop(State->tok);
   while (op != OPR_NOBINOPR) {
      uint8_t lpri = priority[op].left;
      // Special-case: when parsing the RHS of a shift (limit set to
      // the shift right-priority), do not consume another shift here.
      // This enforces left-associativity for chained shifts while still
      // allowing lower-precedence additions on the RHS to bind tighter.

      if (limit == priority[op].right &&
         (op == OPR_SHL or op == OPR_SHR ||
            op == OPR_BOR or op == OPR_BXOR or op == OPR_BAND))
         lpri = 0;

      if (!(lpri > limit)) break;

      lj_lex_next(State);

      if (op == OPR_TERNARY) {
         FuncState* fs = State->fs;
         ExpDesc nilv, falsev, zerov, emptyv;
         BCReg cond_reg, result_reg;
         BCPos check_nil, check_false, check_zero, check_empty;
         BCPos skip_false;

         expr_discharge(fs, v);
         cond_reg = expr_toanyreg(fs, v);
         result_reg = cond_reg;

         State->ternary_depth++;

         expr_init(&nilv, ExpKind::Nil, 0);
         bcemit_INS(fs, BCINS_AD(BC_ISEQP, cond_reg, const_pri(&nilv)));
         check_nil = bcemit_jmp(fs);
         expr_init(&falsev, ExpKind::False, 0);
         bcemit_INS(fs, BCINS_AD(BC_ISEQP, cond_reg, const_pri(&falsev)));
         check_false = bcemit_jmp(fs);
         expr_init(&zerov, ExpKind::Num, 0);
         setnumV(&zerov.u.nval, 0.0);
         bcemit_INS(fs, BCINS_AD(BC_ISEQN, cond_reg, const_num(fs, &zerov)));
         check_zero = bcemit_jmp(fs);
         expr_init(&emptyv, ExpKind::Str, 0);
         emptyv.u.sval = lj_parse_keepstr(State, "", 0);
         bcemit_INS(fs, BCINS_AD(BC_ISEQS, cond_reg, const_str(fs, &emptyv)));
         check_empty = bcemit_jmp(fs);

         {
            ExpDesc v2;
            expr_binop(State, &v2, priority[OPR_IF_EMPTY].right);
            expr_discharge(fs, &v2);
            expr_toreg(fs, &v2, result_reg);
            expr_collapse_freereg(fs, result_reg);
         }

         skip_false = bcemit_jmp(fs);

         lex_check(State, TK_ternary_sep);
         lj_assertLS(State->ternary_depth > 0, "ternary depth underflow");
         State->ternary_depth--;

         {
            BCPos false_start = fs->pc;
            jmp_patch(fs, check_nil, false_start);
            jmp_patch(fs, check_false, false_start);
            jmp_patch(fs, check_zero, false_start);
            jmp_patch(fs, check_empty, false_start);
         }

         {
            ExpDesc fexp; BinOpr nextop3 = expr_binop(State, &fexp, priority[OPR_IF_EMPTY].right);
            expr_discharge(fs, &fexp);
            expr_toreg(fs, &fexp, result_reg);
            expr_collapse_freereg(fs, result_reg);
            jmp_patch(fs, skip_false, fs->pc);
            v->u.s.info = result_reg; v->k = ExpKind::NonReloc; op = nextop3; continue;
         }
      }

      bcemit_binop_left(State->fs, op, v);

      if ((op == OPR_SHL) or (op == OPR_SHR) or (op == OPR_BAND) or (op == OPR_BXOR) or (op == OPR_BOR)) {
         op = expr_shift_chain(State, v, op);
         continue;
      }

      // Parse binary expression with higher priority.

      ExpDesc v2;
      BinOpr nextop;
      nextop = expr_binop(State, &v2, priority[op].right);

      if (op == OPR_IF_EMPTY and State->ternary_depth == 0 &&
         (State->tok == TK_ternary_sep or State->pending_if_empty_colon)) {
         FuncState* fs = State->fs;

         State->pending_if_empty_colon = 0;

         if (v->t != NO_JMP) {
            jmp_patch(fs, v->t, fs->pc);
            v->t = NO_JMP;
         }

         if (v->flags & EXP_HAS_RHS_REG_FLAG) {
            BCReg rhs_reg = BCReg(v->u.s.aux);
            v->flags &= ~EXP_HAS_RHS_REG_FLAG;
            if (rhs_reg >= fs->nactvar and rhs_reg < fs->freereg) {
               fs->freereg = rhs_reg;
            }
         }

         expr_discharge(fs, &v2);
         expr_free(fs, &v2);

         // Emit a runtime error: error('Invalid ternary mix: use '?' with ':>').
         BCReg base = fs->freereg;
         BCReg arg_reg = base + 1 + LJ_FR2;

         bcreg_reserve(fs, 1);
         if (LJ_FR2) bcreg_reserve(fs, 1);
         bcreg_reserve(fs, 1);

         {
            ExpDesc callee;
            expr_init(&callee, ExpKind::Str, 0);
            callee.u.sval = lj_parse_keepstr(State, "error", 5);
            bcemit_INS(fs, BCINS_AD(BC_GGET, base, const_str(fs, &callee)));
         }

         {
            ExpDesc message;
            expr_init(&message, ExpKind::Str, 0);
            message.u.sval = lj_parse_keepstr(State, "Invalid ternary mix: use '?' with ':>'", 39);
            bcemit_INS(fs, BCINS_AD(BC_KSTR, arg_reg, const_str(fs, &message)));
         }

         if (fs->freereg <= arg_reg) fs->freereg = arg_reg + 1;

         v->k = ExpKind::Call;
         v->u.s.info = bcemit_INS(fs, BCINS_ABC(BC_CALL, base, 2, fs->freereg - base - LJ_FR2));
         v->u.s.aux = base;
         fs->freereg = base + 1;
         expr_discharge(fs, v);

         lj_lex_next(State);

         {
            ExpDesc dummy;
            BinOpr after = expr_binop(State, &dummy, priority[OPR_IF_EMPTY].right);
            expr_discharge(fs, &dummy);
            expr_free(fs, &dummy);
            op = after;
         }

         continue;
      }

      bcemit_binop(State->fs, op, v, &v2);
      op = nextop;
   }
   synlevel_end(State);
   if (State->tok == TK_ternary_sep and State->ternary_depth == 0) {
      if (limit == priority[OPR_IF_EMPTY].right) {
         State->pending_if_empty_colon = 1;
         return op;
      }
      err_syntax(State, LJ_ERR_XSYMBOL);
   }
   return op;  // Return unconsumed binary operator (if any).
}

//********************************************************************************************************************
// Parse expression.

static void expr(LexState *State, ExpDesc* v)
{
   expr_binop(State, v, 0);  // Priority 0: parse whole expression.
}

//********************************************************************************************************************
// Assign expression to the next register.

static void expr_next(LexState *State)
{
   ExpDesc e;
   expr(State, &e);
   expr_tonextreg(State->fs, &e);
}

//********************************************************************************************************************
// Parse conditional expression.

[[nodiscard]] static BCPos expr_cond(LexState *State)
{
   ExpDesc v;
   expr(State, &v);
   if (v.k == ExpKind::Nil) v.k = ExpKind::False;
   bcemit_branch_t(State->fs, &v);
   return v.f;
}
