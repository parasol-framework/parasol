// Copyright (C) 2025 Paul Manias

#include <span>
#include <string>
#include <vector>

#include "parser/parser_context.h"

//********************************************************************************************************************
// Eliminate write-after-read hazards for local variable assignment.

void LexState::assign_hazard(std::span<ExpDesc> Left, const ExpDesc &Var)
{
   FuncState* fs = this->fs;
   BCReg reg = Var.u.s.info;  // Check against this variable.
   BCReg tmp = fs->freereg;  // Rename to this temp. register (if needed).
   int hazard = 0;

   for (auto& entry : Left) {
      if (entry.k IS ExpKind::Indexed) {
         if (entry.u.s.info IS reg) {  // t[i], t = 1, 2
            hazard = 1;
            entry.u.s.info = tmp;
         }

         if (entry.u.s.aux IS reg) {  // t[i], i = 1, 2
            hazard = 1;
            entry.u.s.aux = tmp;
         }
      }
   }

   if (hazard) {
      bcemit_AD(fs, BC_MOV, tmp, reg);  // Rename conflicting variable.
      bcreg_reserve(fs, 1);
   }
}

//********************************************************************************************************************
// Adjust LHS/RHS of an assignment.

void LexState::assign_adjust(BCReg nvars, BCReg nexps, ExpDesc *Expr)
{
   FuncState* fs = this->fs;
   int32_t extra = int32_t(nvars) - int32_t(nexps);
   if (Expr->k IS ExpKind::Call) {
      extra++;  // Compensate for the ExpKind::Call itself.
      if (extra < 0) extra = 0;
      setbc_b(bcptr(fs, Expr), extra + 1);  // Fixup call results.
      if (extra > 1) bcreg_reserve(fs, BCReg(extra) - 1);
   }
   else {
      if (Expr->k != ExpKind::Void) expr_tonextreg(fs, Expr);  // Close last expression.
      if (extra > 0) {  // Leftover LHS are set to nil.
         BCReg reg = fs->freereg;
         bcreg_reserve(fs, BCReg(extra));
         bcemit_nil(fs, reg, BCReg(extra));
      }
   }

   if (nexps > nvars) fs->freereg -= nexps - nvars;  // Drop leftover regs.
}

//********************************************************************************************************************

int LexState::assign_if_empty(ParserContext &Context, ExpDesc* lh)
{
   FuncState* fs = this->fs;
   ExpDesc lhv, lhs_eval, rh;
   ExpDesc nilv   = make_nil_expr();
   ExpDesc falsev = make_bool_expr(false);
   ExpDesc zerov  = make_num_expr(0.0);
   ExpDesc emptyv = make_interned_string_expr(this->intern_empty_string());
   BCReg lhs_reg;
   BCPos check_nil, check_false, check_zero, check_empty;
   BCPos skip_assign, assign_pos;
   BCReg nexps;

   lhv = *lh;

   checkcond(this, vkisvar(lh->k), LJ_ERR_XLEFTCOMPOUND);

   Context.tokens().advance();

   RegisterGuard register_guard(fs);

   if (lh->k IS ExpKind::Indexed) {
      BCReg new_base, new_idx;
      uint32_t orig_aux = lhv.u.s.aux;

      new_base = fs->freereg;
      bcemit_AD(fs, BC_MOV, new_base, lhv.u.s.info);
      bcreg_reserve(fs, 1);

      if (int32_t(orig_aux) >= 0 and orig_aux <= BCMAX_C) {
         new_idx = fs->freereg;
         bcemit_AD(fs, BC_MOV, new_idx, BCReg(orig_aux));
         bcreg_reserve(fs, 1);
         lh->u.s.info = new_base;
         lh->u.s.aux = new_idx;
      }
      else lh->u.s.info = new_base;
   }

   lhs_eval = *lh;
   expr_discharge(fs, &lhs_eval);
   lhs_reg = expr_toanyreg(fs, &lhs_eval);

   bcemit_INS(fs, BCINS_AD(BC_ISEQP, lhs_reg, const_pri(&nilv)));
   check_nil = bcemit_jmp(fs);

   bcemit_INS(fs, BCINS_AD(BC_ISEQP, lhs_reg, const_pri(&falsev)));
   check_false = bcemit_jmp(fs);

   bcemit_INS(fs, BCINS_AD(BC_ISEQN, lhs_reg, const_num(fs, &zerov)));
   check_zero = bcemit_jmp(fs);

   bcemit_INS(fs, BCINS_AD(BC_ISEQS, lhs_reg, const_str(fs, &emptyv)));
   check_empty = bcemit_jmp(fs);

   skip_assign = bcemit_jmp(fs);

   assign_pos = fs->pc;

   auto rhs_list = this->expr_list(&rh);
   if (!rhs_list.ok()) {
      return 0;
   }
   nexps = rhs_list.value_ref();
   checkcond(this, nexps IS 1, LJ_ERR_XRIGHTCOMPOUND);

   expr_discharge(fs, &rh);
   expr_toreg(fs, &rh, lhs_reg);

   bcemit_store(fs, &lhv, &rh);

   JumpListView(fs, check_nil).patch_to(assign_pos);
   JumpListView(fs, check_false).patch_to(assign_pos);
   JumpListView(fs, check_zero).patch_to(assign_pos);
   JumpListView(fs, check_empty).patch_to(assign_pos);
   JumpListView(fs, skip_assign).patch_to(fs->pc);

   // Release temporary duplicates before freeing the original table slots.
   register_guard.release_to(register_guard.saved());

   if (lhv.k IS ExpKind::Indexed) {
      uint32_t orig_aux = lhv.u.s.aux;
      if (int32_t(orig_aux) >= 0 and orig_aux <= BCMAX_C)
         bcreg_free(fs, BCReg(orig_aux));
      bcreg_free(fs, BCReg(lhv.u.s.info));
   }
   return 1;
}

//********************************************************************************************************************

int LexState::assign_compound(ParserContext &Context, ExpDesc *lh, TokenKind OpType)
{
   if (OpType IS TokenKind::CompoundIfEmpty) return this->assign_if_empty(Context, lh);

   FuncState* fs = this->fs;
   ExpDesc lhv, infix, rh;
   int32_t nexps;
   BinOpr op;

   lhv = *lh;

   checkcond(this, vkisvar(lh->k), LJ_ERR_XLEFTCOMPOUND);

   switch (OpType) {
      case TokenKind::CompoundAdd: op = OPR_ADD; break;
      case TokenKind::CompoundSub: op = OPR_SUB; break;
      case TokenKind::CompoundMul: op = OPR_MUL; break;
      case TokenKind::CompoundDiv: op = OPR_DIV; break;
      case TokenKind::CompoundMod: op = OPR_MOD; break;
      case TokenKind::CompoundConcat: op = OPR_CONCAT; break;
      default:
         this->assert_condition(0, "unknown compound operator");
         return 0;
   }
   Context.tokens().advance();

   // Preserve table base/index across RHS evaluation by duplicating them
   // to the top of the stack and discharging using the duplicates. This retains
   // the original registers for the final store and maintains LIFO free order.

   RegisterGuard register_guard(fs);
   if (lh->k IS ExpKind::Indexed) {
      BCReg new_base, new_idx;
      uint32_t orig_aux = lhv.u.s.aux;  // Keep originals for the store.

      // Duplicate base to a fresh register.
      new_base = fs->freereg;
      bcemit_AD(fs, BC_MOV, new_base, lhv.u.s.info);
      bcreg_reserve(fs, 1);

      // If index is a register (0..BCMAX_C), duplicate it, too.
      if (int32_t(orig_aux) >= 0 and orig_aux <= BCMAX_C) {
         new_idx = fs->freereg;
         bcemit_AD(fs, BC_MOV, new_idx, BCReg(orig_aux));
         bcreg_reserve(fs, 1);
         // Discharge using the duplicates; keep lhv pointing to originals.
         lh->u.s.info = new_base;
         lh->u.s.aux = new_idx;
      }
      else {
         // For string/byte keys, only the base needs duplicating.
         lh->u.s.info = new_base;
         // aux remains an encoded constant.
      }
   }

   // For concatenation, fix left operand placement before parsing RHS to
   // maintain BC_CAT stack adjacency and LIFO freeing semantics.

   if (op IS OPR_CONCAT) {
      infix = *lh;
      bcemit_binop_left(fs, op, &infix);
      auto rhs_values = this->expr_list(&rh);
      if (!rhs_values.ok()) {
         return 0;
      }
      nexps = rhs_values.value_ref();
      checkcond(this, nexps IS 1, LJ_ERR_XRIGHTCOMPOUND);
   }
   else {
      // For bitwise ops, avoid pre-pushing LHS to keep call frame contiguous.

      if (!(op IS OPR_BAND or op IS OPR_BOR or op IS OPR_BXOR or op IS OPR_SHL or op IS OPR_SHR))
         expr_tonextreg(fs, lh);
      auto rhs_values = this->expr_list(&rh);
      if (!rhs_values.ok()) {
         return 0;
      }
      nexps = rhs_values.value_ref();
      checkcond(this, nexps IS 1, LJ_ERR_XRIGHTCOMPOUND);
      infix = *lh;
      bcemit_binop_left(fs, op, &infix);
   }
   bcemit_binop(fs, op, &infix, &rh);
   bcemit_store(fs, &lhv, &infix);

   // Drop any RHS temporaries and release original base/index in LIFO order.

   register_guard.release_to(register_guard.saved());

   if (lhv.k IS ExpKind::Indexed) {
      uint32_t orig_aux = lhv.u.s.aux;
      if (int32_t(orig_aux) >= 0 and orig_aux <= BCMAX_C) bcreg_free(fs, BCReg(orig_aux));
      bcreg_free(fs, BCReg(lhv.u.s.info));
   }
   return 1;
}

//********************************************************************************************************************
// Recursively parse assignment statement.

void LexState::parse_assignment(ParserContext &Context, ExpDesc *First)
{
   std::vector<ExpDesc> lhs_vars;
   lhs_vars.reserve(4);
   lhs_vars.push_back(*First);
   BCReg nvars = 1;

   checkcond(this, ExpKind::Local <= First->k and First->k <= ExpKind::Indexed, LJ_ERR_XSYNTAX);

   while (true) {
      auto comma = Context.match(TokenKind::Comma);
      if (!comma.ok()) break;
      ExpDesc next;
      auto next_result = this->expr_primary(&next);
      if (!next_result.ok()) {
         return;
      }
      checkcond(this, ExpKind::Local <= next.k and next.k <= ExpKind::Indexed, LJ_ERR_XSYNTAX);
      if (next.k IS ExpKind::Local) {
         auto existing = std::span(lhs_vars.data(), lhs_vars.size());
         this->assign_hazard(existing, next);
      }
      lhs_vars.push_back(next);
      nvars++;
      checklimit(this->fs, this->level + nvars - 1, LJ_MAX_XLEVEL, "variable names");
   }

   Context.consume(TokenKind::Equals, ParserErrorCode::ExpectedToken);

   ExpDesc e;
   auto expr_values = this->expr_list(&e);
   if (!expr_values.ok()) {
      return;
   }
   BCReg nexps = expr_values.value_ref();

   auto assign_from_stack = [&](std::vector<ExpDesc>::reverse_iterator first_it,
      std::vector<ExpDesc>::reverse_iterator last_it)
   {
      for (; first_it != last_it; ++first_it) {
         ExpDesc stack_value;
         expr_init(&stack_value, ExpKind::NonReloc, this->fs->freereg - 1);
         bcemit_store(this->fs, &(*first_it), &stack_value);
      }
   };

   if (nexps IS nvars) {
      if (e.k IS ExpKind::Call) {
         if (bc_op(*bcptr(this->fs, &e)) IS BC_VARG) {
            this->fs->freereg--;
            e.k = ExpKind::Relocable;
         }
         else {
            e.u.s.info = e.u.s.aux;
            e.k = ExpKind::NonReloc;
         }
      }
      bcemit_store(this->fs, &lhs_vars.back(), &e);
      if (lhs_vars.size() > 1) {
         auto begin = lhs_vars.rbegin();
         ++begin;
         assign_from_stack(begin, lhs_vars.rend());
      }
      return;
   }

   this->assign_adjust(nvars, nexps, &e);
   assign_from_stack(lhs_vars.rbegin(), lhs_vars.rend());
}

static bool is_compound_assignment_token(TokenKind kind)
{
   switch (kind) {
   case TokenKind::CompoundAdd:
   case TokenKind::CompoundSub:
   case TokenKind::CompoundMul:
   case TokenKind::CompoundDiv:
   case TokenKind::CompoundMod:
   case TokenKind::CompoundConcat:
   case TokenKind::CompoundIfEmpty:
      return true;
   default:
      return false;
   }
}

//********************************************************************************************************************
// Parse call statement or assignment.

void LexState::parse_call_assign(ParserContext &Context)
{
   FuncState* fs = this->fs;
   ExpDesc lhs;
   auto lhs_result = this->expr_primary(&lhs);
   if (!lhs_result.ok()) {
      return;
   }
   if (lhs.k IS ExpKind::NonReloc and expr_has_flag(&lhs, ExprFlag::PostfixIncStmt))
      return;
   if (lhs.k IS ExpKind::Call) {  // Function call statement.
      setbc_b(bcptr(fs, &lhs), 1);  // No results.
      return;
   }

   Token current = Context.tokens().current();
   TokenKind kind = current.kind();

   if (is_compound_assignment_token(kind)) {
      this->assign_compound(Context, &lhs, kind);
   }
   else if (kind IS TokenKind::Semicolon) {
      // Postfix increment (++) handled in expr_primary.
      Context.tokens().advance();
   }
   else {  // Start of an assignment.
      this->parse_assignment(Context, &lhs);
   }
}

//********************************************************************************************************************
// Parse 'local' statement.

ParserResult<LocalDeclResult> LexState::parse_local(ParserContext &Context)
{
   LocalDeclResult summary;
   Context.tokens().advance();  // Skip 'local'.

   if (Context.check(TokenKind::Function)) {  // Local function declaration.
      auto fn = Context.consume(TokenKind::Function, ParserErrorCode::ExpectedToken);
      if (!fn.ok()) {
         return ParserResult<LocalDeclResult>::failure(fn.error_ref());
      }
      ExpDesc v, b;
      FuncState* fs = this->fs;
      auto name_token = Context.expect_identifier(ParserErrorCode::ExpectedIdentifier);
      if (!name_token.ok()) {
         return ParserResult<LocalDeclResult>::failure(name_token.error_ref());
      }
      GCstr* func_name = name_token.value_ref().identifier();
      this->var_new(0, func_name ? func_name : NAME_BLANK);
      expr_init(&v, ExpKind::Local, fs->freereg);
      v.u.s.aux = fs->varmap[fs->freereg];
      bcreg_reserve(fs, 1);
      this->var_add(1);
      this->parse_body(&b, 0, this->linenumber);
      expr_free(fs, &b);
      expr_toreg(fs, &b, v.u.s.info);
      var_get(this, fs, fs->nactvar - 1).startpc = fs->pc;
      summary.declared = 1;
      summary.initialised = 1;
      return ParserResult<LocalDeclResult>::success(summary);
   }

   ExpDesc e;
   BCReg nvars = 0;
   BCReg nexps = 0;
   do {  // Collect LHS.
      auto identifier = Context.expect_identifier(ParserErrorCode::ExpectedIdentifier);
      if (!identifier.ok()) {
         return ParserResult<LocalDeclResult>::failure(identifier.error_ref());
      }
      GCstr* name = identifier.value_ref().identifier();
      this->var_new(nvars++, is_blank_identifier(name) ? NAME_BLANK : name);
   } while (Context.match(TokenKind::Comma).ok());

   if (Context.match(TokenKind::Equals).ok()) {  // Optional RHS.
      auto rhs_list = this->expr_list(&e);
      if (!rhs_list.ok()) {
         return ParserResult<LocalDeclResult>::failure(rhs_list.error_ref());
      }
      nexps = rhs_list.value_ref();
   }
   else {  // Or implicitly set to nil.
      e.k = ExpKind::Void;
      nexps = 0;
   }
   this->assign_adjust(nvars, nexps, &e);
   this->var_add(nvars);
   summary.declared = nvars;
   summary.initialised = nexps;
   return ParserResult<LocalDeclResult>::success(summary);
}

//********************************************************************************************************************

static void snapshot_return_regs(FuncState* fs, BCIns* ins)
{
   BCOp op = bc_op(*ins);

   if (op IS BC_RET1) {
      BCReg src = bc_a(*ins);
      if (src < fs->nactvar) {
         BCReg dst = fs->freereg;
         bcreg_reserve(fs, 1);
         bcemit_AD(fs, BC_MOV, dst, src);
         setbc_a(ins, dst);
      }
   }
   else if (op IS BC_RET) {
      BCReg base = bc_a(*ins);
      BCReg nres = bc_d(*ins);

      if (nres > 1) {
         BCReg count = nres - 1;
         BCReg dst = fs->freereg;
         BCReg i;

         bcreg_reserve(fs, count);
         for (i = 0; i < count; i++)
            bcemit_AD(fs, BC_MOV, dst + i, base + i);
         setbc_a(ins, dst);
      }
   }
}

//********************************************************************************************************************

void LexState::parse_defer()
{
   FuncState* fs = this->fs;
   ExpDesc func, arg;
   BCLine line = this->linenumber;
   BCReg reg = fs->freereg;
   BCReg nargs = 0;
   VarInfo* vi;

   this->next();  // Skip 'defer'.
   this->var_new(0, NAME_BLANK);
   bcreg_reserve(fs, 1);
   this->var_add(1);
   vi = &var_get(this, fs, fs->nactvar - 1);
   vi->info |= VarInfoFlag::Defer;

   this->parse_body_defer(&func, line);
   expr_toreg(fs, &func, reg);

   if (this->tok IS '(') {
      BCLine argline = this->linenumber;
      this->next();
      if (this->tok != ')') {
         do {
            auto arg_expr = this->expr(&arg);
            if (!arg_expr.ok()) {
               return;
            }
            expr_tonextreg(fs, &arg);
            nargs++;
         } while (this->lex_opt(','));
      }

      this->lex_match(')', '(', argline);

      if (nargs) {
         BCReg i;
         for (i = 0; i < nargs; i++) this->var_new(i, NAME_BLANK);
         this->var_add(nargs);
         for (i = 0; i < nargs; i++) {
            VarInfo* argi = &var_get(this, fs, fs->nactvar - nargs + i);
            argi->info |= VarInfoFlag::DeferArg;
         }
      }
   }

   fs->freereg = fs->nactvar;
}

//********************************************************************************************************************
// Parse 'function' statement.

void LexState::parse_func(BCLine line)
{
   FuncState* fs;
   ExpDesc v, b;
   int needself = 0;
   this->next();  // Skip 'function'.
   // Parse function name.
   this->var_lookup(&v);
   while (this->tok IS '.')  // Multiple dot-separated fields.
      this->expr_field(&v);
   if (this->tok IS ':') {  // Optional colon to signify method call.
      needself = 1;
      this->expr_field(&v);
   }
   this->parse_body(&b, needself, line);
   fs = this->fs;
   bcemit_store(fs, &v, &b);
   fs->bcbase[fs->pc - 1].line = line;  // Set line for the store.
}

//********************************************************************************************************************
// Check for end of block.

static bool parse_is_end(TokenKind kind)
{
   switch (kind) {
   case TokenKind::Else:
   case TokenKind::ElseIf:
   case TokenKind::EndToken:
   case TokenKind::Until:
   case TokenKind::EndOfFile:
      return true;
   default:
      return false;
   }
}

//********************************************************************************************************************
// Parse 'return' statement.

void LexState::parse_return(ParserContext &Context)
{
   BCIns ins;
   FuncState* fs = this->fs;
   Context.tokens().advance();  // Skip 'return'.
   fs->flags |= PROTO_HAS_RETURN;
   TokenKind next_kind = Context.tokens().current().kind();
   if (parse_is_end(next_kind) or next_kind IS TokenKind::Semicolon) {  // Bare return.
      ins = BCINS_AD(BC_RET0, 0, 1);
   }
   else {  // Return with one or more values.
      ExpDesc e;  // Receives the _last_ expression in the list.
      auto returns = this->expr_list(&e);
      if (!returns.ok()) return;

      BCReg nret = returns.value_ref();
      if (nret IS 1) {  // Return one result.
         if (e.k IS ExpKind::Call) {  // Check for tail call.
            BCIns* ip = bcptr(fs, &e);
            // It doesn't pay off to add BC_VARGT just for 'return ...'.
            if (bc_op(*ip) IS BC_VARG) goto no_tail_call;
            fs->pc--;
            ins = BCINS_AD(bc_op(*ip) - BC_CALL + BC_CALLT, bc_a(*ip), bc_c(*ip));
         }
         else {  // Can return the result from any register.
            ins = BCINS_AD(BC_RET1, expr_toanyreg(fs, &e), 2);
         }
      }
      else {
         if (e.k IS ExpKind::Call) {  // Append all results from a call.
         no_tail_call:
            setbc_b(bcptr(fs, &e), 0);
            ins = BCINS_AD(BC_RETM, fs->nactvar, e.u.s.aux - fs->nactvar);
         }
         else {
            expr_tonextreg(fs, &e);  // Force contiguous registers.
            ins = BCINS_AD(BC_RET, fs->nactvar, nret + 1);
         }
      }
   }
   snapshot_return_regs(fs, &ins);
   execute_defers(fs, 0);
   if (fs->flags & PROTO_CHILD)
      bcemit_AJ(fs, BC_UCLO, 0, 0);  // May need to close upvalues first.
   bcemit_INS(fs, ins);
}

//********************************************************************************************************************
// Parse 'continue' statement.

void LexState::parse_continue()
{
   FuncState* fs = this->fs;
   FuncScope* loop = fs->bl;

   this->next();  // Skip 'continue'.

   while (loop and !has_flag(loop->flags, FuncScopeFlag::Loop))
      loop = loop->prev;
   this->assert_condition(loop != nullptr, "continue outside loop");

   execute_defers(fs, loop->nactvar);
   fs->bl->flags |= FuncScopeFlag::Continue;
   this->gola_new(JUMP_CONTINUE, VarInfoFlag::Jump, bcemit_jmp(fs));
}

//********************************************************************************************************************
// Parse 'break' statement.

void LexState::parse_break()
{
   FuncState *fs = this->fs;
   FuncScope *loop = fs->bl;

   this->next();  // Skip 'break'.

   while (loop and !has_flag(loop->flags, FuncScopeFlag::Loop)) loop = loop->prev;
   this->assert_condition(loop != nullptr, "break outside loop");

   execute_defers(fs, loop->nactvar);
   fs->bl->flags |= FuncScopeFlag::Break;
   this->gola_new(JUMP_BREAK, VarInfoFlag::Jump, bcemit_jmp(fs));
}

//********************************************************************************************************************
// Blocks, loops and conditional statements

// Parse a block.

void LexState::parse_block(ParserContext &Context)
{
   FuncState* fs = this->fs;
   FuncScope bl;
   ScopeGuard scope_guard(fs, &bl, FuncScopeFlag::None);
   this->parse_chunk(Context);
}

//********************************************************************************************************************
// Parse 'while' statement.

void LexState::parse_while(ParserContext &Context, BCLine line)
{
   FuncState* fs = this->fs;
   BCPos start, loop, condexit;
   Context.tokens().advance();  // Skip 'while'.
   start = fs->lasttarget = fs->pc;
   auto while_cond = this->expr_cond();

   if (!while_cond.ok()) return;

   condexit = while_cond.value_ref();
   FuncScope bl;

   {
      ScopeGuard loop_scope(fs, &bl, FuncScopeFlag::Loop);
      Context.consume(TokenKind::DoToken, ParserErrorCode::ExpectedToken);
      loop = bcemit_AD(fs, BC_LOOP, fs->nactvar, 0);
      this->parse_block(Context);
      JumpListView(fs, bcemit_jmp(fs)).patch_to(start);
      this->lex_match(TK_end, TK_while, line);
      fscope_loop_continue(fs, start);
   }

   JumpListView(fs, condexit).patch_to_here();
   JumpListView(fs, loop).patch_head(fs->pc);
}

//********************************************************************************************************************
// Parse 'repeat' statement.

void LexState::parse_repeat(ParserContext &Context, BCLine line)
{
   FuncState* fs = this->fs;
   BCPos loop = fs->lasttarget = fs->pc;
   BCPos condexit, iter;
   FuncScope bl1, bl2;
   ScopeGuard loop_scope(fs, &bl1, FuncScopeFlag::Loop);  // Breakable loop scope.
   bool inner_has_upvals = false;
   {
      ScopeGuard inner_scope(fs, &bl2, FuncScopeFlag::None);  // Inner scope.
      Context.tokens().advance();  // Skip 'repeat'.
      bcemit_AD(fs, BC_LOOP, fs->nactvar, 0);
      this->parse_chunk(Context);
      this->lex_match(TK_until, TK_repeat, line);
      iter = fs->pc;
      auto repeat_cond = this->expr_cond();  // Parse condition (still inside inner scope).
      if (!repeat_cond.ok()) {
         return;
      }
      condexit = repeat_cond.value_ref();
      inner_has_upvals = has_flag(bl2.flags, FuncScopeFlag::Upvalue);
      if (inner_has_upvals) {  // Otherwise generate: cond: UCLO+JMP out, !cond: UCLO+JMP loop.
         this->parse_break();  // Break from loop and close upvalues.
         JumpListView(fs, condexit).patch_to_here();
      }
   }
   if (inner_has_upvals) condexit = bcemit_jmp(fs);
   JumpListView(fs, condexit).patch_to(loop);  // Jump backwards if !cond.
   JumpListView(fs, loop).patch_head(fs->pc);
   fscope_loop_continue(fs, iter); // continue statements jump to condexit.
}

//********************************************************************************************************************
// Parse numeric 'for'.

void LexState::parse_for_num(ParserContext &Context, GCstr* varname, BCLine line)
{
   FuncState* fs = this->fs;
   BCReg base = fs->freereg;
   FuncScope bl;
   BCPos loop, loopend;

   // Hidden control variables.

   this->var_new_fixed(FORL_IDX, VARNAME_FOR_IDX);
   this->var_new_fixed(FORL_STOP, VARNAME_FOR_STOP);
   this->var_new_fixed(FORL_STEP, VARNAME_FOR_STEP);

   // Visible copy of index variable.

   this->var_new(FORL_EXT, varname);
   Context.consume(TokenKind::Equals, ParserErrorCode::ExpectedToken);
   if (!this->expr_next().ok()) return;
   Context.consume(TokenKind::Comma, ParserErrorCode::ExpectedToken);
   if (!this->expr_next().ok()) return;

   if (Context.match(TokenKind::Comma).ok()) {
      if (!this->expr_next().ok()) return;
   }
   else {
      bcemit_AD(fs, BC_KSHORT, fs->freereg, 1);  // Default step is 1.
      bcreg_reserve(fs, 1);
   }

   this->var_add(3);  // Hidden control variables.
   Context.consume(TokenKind::DoToken, ParserErrorCode::ExpectedToken);
   loop = bcemit_AJ(fs, BC_FORI, base, NO_JMP);

   {
      ScopeGuard visible_scope(fs, &bl, FuncScopeFlag::None);  // Scope for visible variables.
      this->var_add(1);
      bcreg_reserve(fs, 1);
      this->parse_block(Context);
   }

   // Perform loop inversion. Loop control instructions are at the end.

   loopend = bcemit_AJ(fs, BC_FORL, base, NO_JMP);
   fs->bcbase[loopend].line = line;  // Fix line for control ins.
   JumpListView(fs, loopend).patch_head(loop + 1);
   JumpListView(fs, loop).patch_head(fs->pc);
   fscope_loop_continue(fs, loopend); // continue statements jump to loopend.
}

//********************************************************************************************************************
// Try to predict whether the iterator is next() and specialize the bytecode.
// Detecting next() and pairs() by name is simplistic, but quite effective.
// The interpreter backs off if the check for the closure fails at runtime.

static int predict_next(LexState *State, FuncState *fs, BCPos pc)
{
   BCIns ins = fs->bcbase[pc].ins;
   GCstr* name;
   cTValue* o;

   switch (bc_op(ins)) {
      case BC_MOV:
         name = gco2str(gcref(var_get(State, fs, bc_d(ins)).name));
         break;
      case BC_UGET:
         name = gco2str(gcref(State->vstack[fs->uvmap[bc_d(ins)]].name));
         break;
      case BC_GGET:
         // There's no inverse index (yet), so lookup the strings.
         o = lj_tab_getstr(fs->kt, lj_str_newlit(State->L, "pairs"));
         if (o and tvhaskslot(o) and tvkslot(o) IS bc_d(ins))
            return 1;
         o = lj_tab_getstr(fs->kt, lj_str_newlit(State->L, "next"));
         if (o and tvhaskslot(o) and tvkslot(o) IS bc_d(ins))
            return 1;
         return 0;
      default:
         return 0;
   }

   return (name->len IS 5 and !strcmp(strdata(name), "pairs")) or
      (name->len IS 4 and !strcmp(strdata(name), "next"));
}

//********************************************************************************************************************
// Parse 'for' iterator.

void LexState::parse_for_iter(ParserContext &Context, GCstr* indexname)
{
   FuncState* fs = this->fs;
   ExpDesc e;
   BCReg nvars = 0;
   BCLine line;
   BCReg base = fs->freereg + 3;
   BCPos loop, loopend, iter, exprpc = fs->pc;
   FuncScope bl;
   int isnext;

   // Hidden control variables.
   this->var_new_fixed(nvars++, VARNAME_FOR_GEN);
   this->var_new_fixed(nvars++, VARNAME_FOR_STATE);
   this->var_new_fixed(nvars++, VARNAME_FOR_CTL);

   // Visible variables returned from iterator.

   this->var_new(nvars++, is_blank_identifier(indexname) ? NAME_BLANK : indexname);
   while (Context.match(TokenKind::Comma).ok()) {
      GCstr* name = Context.lex_str();
      this->var_new(nvars++, is_blank_identifier(name) ? NAME_BLANK : name);
   }

   Context.consume(TokenKind::InToken, ParserErrorCode::ExpectedToken);
   line = this->linenumber;
   auto iter_values = this->expr_list(&e);
   if (!iter_values.ok()) return;

   this->assign_adjust(3, iter_values.value_ref(), &e);

   // The iterator needs another 3 [4] slots (func [pc] | state ctl).

   bcreg_bump(fs, 3 + LJ_FR2);
   isnext = (nvars <= 5 and predict_next(this, fs, exprpc));
   this->var_add(3);  // Hidden control variables.
   Context.consume(TokenKind::DoToken, ParserErrorCode::ExpectedToken);
   loop = bcemit_AJ(fs, isnext ? BC_ISNEXT : BC_JMP, base, NO_JMP);

   {
      ScopeGuard visible_scope(fs, &bl, FuncScopeFlag::None);  // Scope for visible variables.
      this->var_add(nvars - 3);
      bcreg_reserve(fs, nvars - 3);
      this->parse_block(Context);
   }

   // Perform loop inversion. Loop control instructions are at the end.

   JumpListView(fs, loop).patch_head(fs->pc);
   iter = bcemit_ABC(fs, isnext ? BC_ITERN : BC_ITERC, base, nvars - 3 + 1, 2 + 1);
   loopend = bcemit_AJ(fs, BC_ITERL, base, NO_JMP);
   fs->bcbase[loopend - 1].line = line;  // Fix line for control ins.
   fs->bcbase[loopend].line = line;
   JumpListView(fs, loopend).patch_head(loop + 1);
   fscope_loop_continue(fs, iter); // continue statements jump to iter.
}

//********************************************************************************************************************
// Parse 'for' statement.

void LexState::parse_for(ParserContext &Context, BCLine line)
{
   FuncState* fs = this->fs;
   GCstr* varname;
   FuncScope bl;
   ScopeGuard loop_scope(fs, &bl, FuncScopeFlag::Loop);
   Context.tokens().advance();  // Skip 'for'.
   varname = Context.lex_str();  // Get first variable name.
   TokenKind next_kind = Context.tokens().current().kind();
   if (next_kind IS TokenKind::Equals) this->parse_for_num(Context, varname, line);
   else if (next_kind IS TokenKind::Comma or next_kind IS TokenKind::InToken) this->parse_for_iter(Context, varname);
   else this->err_syntax(LJ_ERR_XFOR);
   this->lex_match(TK_end, TK_for, line);
}

//********************************************************************************************************************
// Parse condition and 'then' block.

BCPos LexState::parse_then(ParserContext &Context)
{
   BCPos condexit;
   Context.tokens().advance();  // Skip 'if' or 'elseif'.
   auto if_cond = this->expr_cond();
   if (!if_cond.ok()) return NO_JMP;
   condexit = if_cond.value_ref();
   Context.consume(TokenKind::ThenToken, ParserErrorCode::ExpectedToken);
   this->parse_block(Context);
   return condexit;
}

//********************************************************************************************************************
// Parse 'if' statement.

void LexState::parse_if(ParserContext &Context, BCLine line)
{
   FuncState* fs = this->fs;
   BCPos flist;
   BCPos escapelist = NO_JMP;
   flist = this->parse_then(Context);
   while (Context.tokens().current().is(TokenKind::ElseIf)) {  // Parse multiple 'elseif' blocks.
      escapelist = JumpListView(fs, escapelist).append(bcemit_jmp(fs));
      JumpListView(fs, flist).patch_to_here();
      flist = this->parse_then(Context);
   }

   if (Context.tokens().current().is(TokenKind::Else)) {  // Parse optional 'else' block.
      escapelist = JumpListView(fs, escapelist).append(bcemit_jmp(fs));
      JumpListView(fs, flist).patch_to_here();
      Context.tokens().advance();  // Skip 'else'.
      this->parse_block(Context);
   }
   else escapelist = JumpListView(fs, escapelist).append(flist);

   JumpListView(fs, escapelist).patch_to_here();
   this->lex_match(TK_end, TK_if, line);
}

//********************************************************************************************************************
// Parse a single statement. Returns true if it must be the last one in a chunk.

bool LexState::parse_stmt(ParserContext &Context)
{
   auto line = this->linenumber;

   switch (Context.tokens().current().kind()) {
      case TokenKind::If: this->parse_if(Context, line); break;
      case TokenKind::WhileToken: this->parse_while(Context, line); break;
      case TokenKind::For: this->parse_for(Context, line); break;
      case TokenKind::Repeat: this->parse_repeat(Context, line); break;
      case TokenKind::Function: this->parse_func(line); break;
      case TokenKind::DeferToken: this->parse_defer(); break;
      case TokenKind::Local: this->parse_local(Context); break;
      case TokenKind::ReturnToken: this->parse_return(Context); return true;  // Must be last.
      case TokenKind::ContinueToken: this->parse_continue(); break;
      case TokenKind::BreakToken: this->parse_break(); break;
      case TokenKind::Semicolon: Context.tokens().advance(); break;
      case TokenKind::DoToken:
         Context.tokens().advance();
         this->parse_block(Context);
         this->lex_match(TK_end, TK_do, line);
         break;
      default: this->parse_call_assign(Context); break;
   }
   return false;
}

//********************************************************************************************************************
// Summarise accumulated diagnostics when abort_on_error is disabled.

static void raise_accumulated_diagnostics(ParserContext &Context)
{
   auto entries = Context.diagnostics().entries();
   if (entries.empty()) return;

   std::string summary;
   summary.reserve(entries.size() * 64);
   summary.append("parser reported ");
   summary.append(std::to_string(entries.size()));
   summary.append(entries.size() == 1 ? " error:\n" : " errors:\n");

   for (const auto& diagnostic : entries) {
      SourceSpan span = diagnostic.token.span();
      summary.append("   line ");
      summary.append(std::to_string(span.line));
      summary.append(":");
      summary.append(std::to_string(span.column));
      summary.append(" - ");
      if (!diagnostic.message.empty()) summary.append(diagnostic.message);
      else summary.append("unexpected token");
      summary.push_back('\n');
   }

   lua_State *L = &Context.lua();
   GCstr *message = lj_str_new(L, summary.data(), summary.size());
   setstrV(L, L->top++, message);
   lj_err_throw(L, LUA_ERRSYNTAX);
}

//********************************************************************************************************************
// Parse a chunk (list of statements).

void LexState::parse_chunk(ParserContext &Context)
{
   bool is_last = false;
   this->synlevel_begin();
   while ((not is_last) and (not parse_is_end(Context.tokens().current().kind()))) {
      is_last = this->parse_stmt(Context);
      this->lex_opt(';');
      this->assert_condition(this->fs->framesize >= this->fs->freereg and this->fs->freereg >= this->fs->nactvar, "bad regalloc");
      this->fs->freereg = this->fs->nactvar;  // Free registers after each stmt.
   }
   this->synlevel_end();

   if (!Context.config().abort_on_error and Context.diagnostics().has_errors()) {
      raise_accumulated_diagnostics(Context);
   }
}
