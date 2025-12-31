// Copyright (C) 2025 Paul Manias
// IR emitter implementation: try...except...end statement emission
// This file is #included from ir_emitter.cpp

//********************************************************************************************************************
// Emit bytecode for try...except...end exception handling blocks.
//
// Transforms:
//   try
//      <try body>
//   except e when { ERR_A, ERR_B }
//      <handler1>
//   except e
//      <handler2>
//   end
//
// Into:
//   __try(0, function() <try body> end, function(e) <handler1> end, filter1, function(e) <handler2> end, 0)
//
// The first argument is the expected result count (0 for statement form).
// The try body is wrapped in a function closure.
// Each except handler is wrapped in a function closure with optional exception parameter.
// Filters are packed as 64-bit integers with up to 4 error codes (16 bits each), or 0 for catch-all.

ParserResult<IrEmitUnit> IrEmitter::emit_try_except_stmt(const TryExceptPayload &Payload)
{
   FuncState *fs = &this->func_state;

   // 1. Emit load of __try global function
   ExpDesc try_func(ExpKind::Global);
   try_func.u.sval = lj_str_newlit(this->lex_state.L, "__try");
   this->materialise_to_next_reg(try_func, "try call function");
   BCReg base = BCReg(try_func.u.s.info);

   // Reserve register for frame link (FR2 mode)
   RegisterAllocator allocator(fs);
   allocator.reserve(BCReg(1));

   // 2. Emit result count (0 for statement form - no LHS assignment)
   // For expression form this would be the number of LHS variables
   ExpDesc count(0.0);  // Statement form - no return values
   this->materialise_to_next_reg(count, "try result count");

   // 3. Emit try body as function closure
   if (not Payload.try_block) {
      return this->unsupported_stmt(AstNodeKind::TryExceptStmt, SourceSpan{});
   }

   // Emit the try body function by creating a child function state and emitting the try block directly.
   {
      FuncState child_state;
      ParserAllocator child_allocator = ParserAllocator::from(this->lex_state.L);
      ParserConfig inherited = this->ctx.config();
      ParserContext child_ctx = ParserContext::from(this->lex_state, child_state, child_allocator, inherited);
      ParserSession session(child_ctx, inherited);

      ptrdiff_t oldbase = fs->bcbase - this->lex_state.bcstack;
      this->lex_state.fs_init(&child_state);
      FuncStateGuard fs_guard(&this->lex_state, &child_state);

      child_state.declared_globals = fs->declared_globals;

      // Set linedefined to the earliest line that bytecode might reference.
      // Use the minimum of lastline and the first statement's line to ensure
      // linedefined is always <= any line number in the emitted bytecode.
      BCLine body_first_line = this->lex_state.lastline;
      if (not Payload.try_block->statements.empty()) {
         const StmtNodePtr& first_stmt = Payload.try_block->statements.front();
         if (first_stmt) body_first_line = first_stmt->span.line;
      }
      child_state.linedefined = std::min(this->lex_state.lastline, body_first_line);

      child_state.bcbase = fs->bcbase + fs->pc;
      child_state.bclim = fs->bclim - fs->pc;
      bcemit_AD(&child_state, BC_FUNCF, 0, 0);

      FuncScope scope;
      ScopeGuard scope_guard(&child_state, &scope, FuncScopeFlag::None);

      child_state.numparams = 0;

      IrEmitter child_emitter(child_ctx);
      auto body_result = child_emitter.emit_block(*Payload.try_block, FuncScopeFlag::None);
      if (not body_result.ok()) return body_result;

      fs_guard.disarm();
      GCproto *pt = this->lex_state.fs_finish(Payload.try_block->span.line);
      scope_guard.disarm();
      fs->bcbase = this->lex_state.bcstack + oldbase;
      fs->bclim = BCPos(this->lex_state.sizebcstack - oldbase).raw();

      ExpDesc try_fn;
      try_fn.init(ExpKind::Relocable, bcemit_AD(fs, BC_FNEW, 0, const_gc(fs, obj2gco(pt), LJ_TPROTO)));
      this->materialise_to_next_reg(try_fn, "try body function");

      if (not (fs->flags & PROTO_CHILD)) {
         if (fs->flags & PROTO_HAS_RETURN) fs->flags |= PROTO_FIXUP_RETURN;
         fs->flags |= PROTO_CHILD;
      }
   }

   // 4. For each except clause, emit handler function and filter
   int arg_count = 2;  // result_count and try_body so far

   for (const auto& clause : Payload.except_clauses) {
      // Emit handler function with optional exception parameter
      {
         FuncState child_state;
         ParserAllocator child_allocator = ParserAllocator::from(this->lex_state.L);
         ParserConfig inherited = this->ctx.config();
         ParserContext child_ctx = ParserContext::from(this->lex_state, child_state, child_allocator, inherited);
         ParserSession session(child_ctx, inherited);

         ptrdiff_t oldbase = fs->bcbase - this->lex_state.bcstack;
         this->lex_state.fs_init(&child_state);
         FuncStateGuard fs_guard(&this->lex_state, &child_state);

         child_state.declared_globals = fs->declared_globals;

         // Set linedefined to the earliest line that bytecode might reference.
         // Use the minimum of lastline, clause.span.line, and the first statement's line
         // to ensure linedefined is always <= any line number in the emitted bytecode.
         BCLine body_first_line = this->lex_state.lastline;
         body_first_line = std::min(body_first_line, clause.span.line);
         if (clause.block and not clause.block->statements.empty()) {
            const StmtNodePtr& first_stmt = clause.block->statements.front();
            if (first_stmt) body_first_line = std::min(body_first_line, first_stmt->span.line);
         }
         child_state.linedefined = body_first_line;

         child_state.bcbase = fs->bcbase + fs->pc;
         child_state.bclim = fs->bclim - fs->pc;
         bcemit_AD(&child_state, BC_FUNCF, 0, 0);

         FuncScope scope;
         ScopeGuard scope_guard(&child_state, &scope, FuncScopeFlag::None);

         // Set up exception variable parameter if present
         if (clause.exception_var.has_value()) {
            GCstr *symbol = clause.exception_var->symbol;
            if (symbol) {
               BCLine param_line = std::min(body_first_line, clause.exception_var->span.line);
               this->lex_state.var_new(BCReg(0), symbol, param_line, clause.exception_var->span.column);
               child_state.numparams = 1;
               this->lex_state.var_add(BCReg(1));
               RegisterAllocator child_reg_alloc(&child_state);
               child_reg_alloc.reserve(BCReg(1));
            }
         }
         else {
            // No exception variable - still receives one arg but ignores it
            this->lex_state.var_new(BCReg(0), NAME_BLANK, body_first_line, 0);
            child_state.numparams = 1;
            this->lex_state.var_add(BCReg(1));
            RegisterAllocator child_reg_alloc(&child_state);
            child_reg_alloc.reserve(BCReg(1));
         }

         IrEmitter child_emitter(child_ctx);

         // Update local binding for exception variable
         if (clause.exception_var.has_value() and clause.exception_var->symbol) {
            child_emitter.update_local_binding(clause.exception_var->symbol, BCReg(0));
         }

         if (clause.block) {
            auto body_result = child_emitter.emit_block(*clause.block, FuncScopeFlag::None);
            if (not body_result.ok()) return body_result;
         }

         fs_guard.disarm();
         GCproto *pt = this->lex_state.fs_finish(body_first_line);
         scope_guard.disarm();
         fs->bcbase = this->lex_state.bcstack + oldbase;
         fs->bclim = BCPos(this->lex_state.sizebcstack - oldbase).raw();

         ExpDesc handler_fn;
         handler_fn.init(ExpKind::Relocable, bcemit_AD(fs, BC_FNEW, 0, const_gc(fs, obj2gco(pt), LJ_TPROTO)));
         this->materialise_to_next_reg(handler_fn, "except handler function");

         if (not (fs->flags & PROTO_CHILD)) {
            if (fs->flags & PROTO_HAS_RETURN) fs->flags |= PROTO_FIXUP_RETURN;
            fs->flags |= PROTO_CHILD;
         }
      }
      arg_count++;

      // Emit filter value (packed 64-bit integer or 0 for catch-all)
      if (not clause.filter_codes.empty()) {
         // Pack up to 4 error codes into a 64-bit integer
         // Each code is 16 bits, stored from low to high bits
         uint64_t packed = 0;
         int shift = 0;
         for (const auto& code_expr : clause.filter_codes) {
            if (not code_expr or shift >= 64) break;

            // Emit the expression and get its value
            auto code_result = this->emit_expression(*code_expr);
            if (not code_result.ok()) {
               // If we can't evaluate at compile time, emit 0 (catch-all fallback)
               break;
            }

            ExpDesc code = code_result.value_ref();

            // We need the numeric value - if it's a constant, extract it
            if (code.k IS ExpKind::Num) {
               uint16_t code_val = uint16_t(code.number_value());
               packed |= (uint64_t(code_val) << shift);
               shift += 16;
            }
            else {
               // Not a compile-time constant - we need to evaluate at runtime
               // For now, emit the expression and build the filter dynamically
               // This is more complex, so we'll just emit 0 for catch-all if not constant
               this->materialise_to_next_reg(code, "except filter code");
               // TODO: Build filter at runtime for non-constant codes
            }
         }

         lua_Number filter_val = lua_Number(packed);
         ExpDesc filter(filter_val);
         this->materialise_to_next_reg(filter, "except filter");
      }
      else {
         // Catch-all: emit 0
         ExpDesc filter(0.0);
         this->materialise_to_next_reg(filter, "except catch-all filter");
      }
      arg_count++;
   }

   // 5. Emit CALL instruction
   // BC_CALL A=base, B=nresults+1, C=nargs+1
   // For statement form, B=1 means 0 results (discard all)
   // C operand: freereg - base - 1 (this accounts for the frame link slot properly)
   bcemit_ABC(fs, BC_CALL, base, BCReg(1), BCReg(fs->freereg - base.raw() - 1));

   // Reset freereg to base after call (statement form, no results kept)
   fs->freereg = base.raw();

   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}
