// Copyright (C) 2025 Paul Manias
// IR emitter implementation: try...except...end statement emission (bytecode-level)
// This file is #included from ir_emitter.cpp
//
// This implements bytecode-level exception handling that emits try body and handlers inline (not in closures),
// allowing return/break/continue to work correctly.
//
// Bytecode structure:
//   BC_TRYENTER  base, try_block_index    ; Push exception frame
//   <try body bytecode>                   ; Inline try body
//   BC_TRYLEAVE  base, 0                  ; Pop exception frame (normal exit)
//   JMP          exit_label               ; Jump over handlers
//   handler_1:                            ; Handler entry point (recorded in TryHandlerDesc)
//   <handler1 bytecode>                   ; Inline handler body
//   JMP          exit_label
//   handler_2:
//   <handler2 bytecode>
//   JMP          exit_label
//   exit_label:
//
// Handler metadata (TryBlockDesc, TryHandlerDesc) is stored in the FuncState during compilation and copied to the
// GCproto during fs_finish().

//********************************************************************************************************************
// Emit bytecode for try...except...end exception handling blocks.
//
// This is the bytecode-level implementation that emits try body and handlers inline,
// allowing return/break/continue to correctly affect the enclosing function/loop.

ParserResult<IrEmitUnit> IrEmitter::emit_try_except_stmt(const TryExceptPayload &Payload)
{
   FuncState *fs = &this->func_state;
   BCReg base_reg = BCReg(fs->freereg);

   if (not Payload.try_block) return this->unsupported_stmt(AstNodeKind::TryExceptStmt, SourceSpan{});

   // Reserve a slot in try_blocks for this try block. We'll fill in the details later after emitting the try body
   // (which may contain nested try blocks with their own handlers).  This ensures first_handler_index is correct
   // after nested try blocks have added their handlers.

   uint16_t try_block_index = uint16_t(fs->try_blocks.size());

   // Validate limits

   if (try_block_index >= 0xFFFF) {
      return ParserResult<IrEmitUnit>::failure(this->make_error(
         ParserErrorCode::InternalInvariant, "too many try blocks in function", SourceSpan{}));
   }

   // Determine handler count - if no except clauses, we'll emit a synthetic catch-all handler

   uint8_t handler_count = Payload.except_clauses.empty() ? 1 : uint8_t(Payload.except_clauses.size());

   // Push a placeholder TryBlockDesc - first_handler will be updated after emitting try body

   uint8_t entry_slots = uint8_t(base_reg.raw());
   uint8_t flags = 0;
   if (Payload.enable_trace) flags |= TRY_FLAG_TRACE;

   fs->try_blocks.push_back(TryBlockDesc{
      0,             // Place-holder; set correctly after try body
      handler_count,
      entry_slots,
      flags
   });

   // Track try depth for break/continue cleanup

   uint8_t saved_try_depth = fs->try_depth;
   fs->try_depth++;

   // Emit BC_TRYENTER with try block index
   bcemit_AD(fs, BC_TRYENTER, base_reg, BCReg(try_block_index));

   // Emit try body inline (not in closure!).  Nested try blocks will add their handlers to try_handlers during this phase.
   // We manually manage the scope here so we can emit BC_TRYLEAVE before defer execution.
   // The ScopeGuard destructor will call fscope_end() which executes defers AFTER BC_TRYLEAVE.
   {
      FuncScope try_scope;
      ScopeGuard try_guard(fs, &try_scope, FuncScopeFlag::None);
      LocalBindingScope binding_scope(this->binding_table);

      // Emit all statements in the try body
      for (const StmtNode& stmt : Payload.try_block->view()) {
         auto status = this->emit_statement(stmt);
         if (not status.ok()) {
            fs->try_depth = saved_try_depth;
            return status;
         }
         this->ensure_register_balance(describe_node_kind(stmt.kind));
      }

      // Emit BC_TRYLEAVE BEFORE the scope ends (before defers are executed)
      // This ensures defers run OUTSIDE the try block's exception protection
      bcemit_AD(fs, BC_TRYLEAVE, base_reg, BCReg(0));

      // ScopeGuard destructor runs here, calling fscope_end() which executes defers
   }

   // Emit success block (if present) - runs after defers, before jump over handlers
   if (Payload.success_block) {
      auto success_result = this->emit_block(*Payload.success_block, FuncScopeFlag::None);
      if (not success_result.ok()) {
         fs->try_depth = saved_try_depth;
         return success_result;
      }
   }

   // Jump over handlers (successful completion)
   ControlFlowEdge exit_jmp = this->control_flow.make_unconditional(BCPos(bcemit_jmp(fs)));

   // Correctly determine first_handler_index - nested try blocks have added their handlers
   size_t first_handler_index = fs->try_handlers.size();

   // Update the placeholder TryBlockDesc with the correct first_handler
   fs->try_blocks[try_block_index].first_handler = uint16_t(first_handler_index);

   if (first_handler_index + Payload.except_clauses.size() >= 0xFFFF) {
      return ParserResult<IrEmitUnit>::failure(this->make_error(
         ParserErrorCode::InternalInvariant, "too many exception handlers in function", SourceSpan{}));
   }

   // Emit handlers inline and record metadata

   std::vector<ControlFlowEdge> handler_exits;

   for (const auto &clause : Payload.except_clauses) {
      // Pack filter codes inline (up to 4 16-bit error codes into 64-bit integer)
      uint64_t packed_filter = 0;
      if (not clause.filter_codes.empty()) {
         int shift = 0;
         for (const auto &code_expr : clause.filter_codes) {
            if (not code_expr or shift >= 64) break;

            // Try to evaluate the expression as a constant
            auto code_result = this->emit_expression(*code_expr);
            if (not code_result.ok()) break;

            ExpDesc code = code_result.value_ref();

            // We need the numeric value - if it's a constant, extract it
            if (code.k IS ExpKind::Num) {
               uint16_t code_val = uint16_t(code.number_value());
               packed_filter |= (uint64_t(code_val) << shift);
               shift += 16;
            }
            else { // Non-numeric codes are an error
               return ParserResult<IrEmitUnit>::failure(this->make_error(
                  ParserErrorCode::InternalInvariant, "Non-numeric filter in try block", SourceSpan{}));
            }
         }
      }

      // Determine exception register

      BCREG exception_reg = 0xFF;  // No exception variable by default
      BCReg saved_freereg = BCReg(fs->freereg);
      BCPOS handler_pc = fs->pc;  // Record handler entry PC BEFORE emitting any handler code

      // If there's an exception variable, allocate a register for it.
      // The runtime will place the exception table in this register

      if (clause.exception_var.has_value() and clause.exception_var->symbol) {
         BCREG saved_nactvar = fs->nactvar;  // Save nactvar before adding the exception variable

         // Reserve register space first, then create the variable var_new takes an offset from nactvar, not an
         // absolute register.

         fs->freereg++;
         this->lex_state.var_new(BCReg(0), clause.exception_var->symbol, clause.exception_var->span.line, clause.exception_var->span.column);
         this->lex_state.var_add(BCReg(1));

         exception_reg = saved_nactvar; // The exception register is the slot we just added

         // Update local binding so the variable can be referenced
         this->update_local_binding(clause.exception_var->symbol, BCReg(exception_reg));

         // Emit handler body
         if (clause.block) {
            auto handler_result = this->emit_block(*clause.block, FuncScopeFlag::None);
            if (not handler_result.ok()) {
               fs->try_depth = saved_try_depth;
               return handler_result;
            }
         }

         // Clean up: remove the exception variable and restore freereg
         this->lex_state.var_remove(saved_nactvar);
         fs->freereg = saved_freereg.raw();
      }
      else {
         // No exception variable - emit handler body without variable binding
         if (clause.block) {
            auto handler_result = this->emit_block(*clause.block, FuncScopeFlag::None);
            if (not handler_result.ok()) {
               fs->try_depth = saved_try_depth;
               return handler_result;
            }
         }
         fs->freereg = saved_freereg.raw();
      }

      // Record handler metadata
      fs->try_handlers.push_back(TryHandlerDesc{
         packed_filter,
         handler_pc,
         exception_reg
      });

      // Jump to exit after handler
      handler_exits.push_back(this->control_flow.make_unconditional(BCPos(bcemit_jmp(fs))));
   }

   // If no except clauses were provided, emit a synthetic catch-all handler that silently swallows exceptions.
   // This handler has no body - it just jumps to the exit, effectively discarding the exception.

   if (Payload.except_clauses.empty()) {
      BCPOS handler_pc = fs->pc;  // Record handler entry PC

      // Record handler metadata: packed_filter=0 (catch-all), exception_reg=0xFF (no variable)
      fs->try_handlers.push_back(TryHandlerDesc{
         0,           // packed_filter = 0 means catch-all
         handler_pc,
         0xFF         // exception_reg = 0xFF means no exception variable
      });

      // Jump to exit (this is the "body" of the synthetic handler - just falls through to exit)
      handler_exits.push_back(this->control_flow.make_unconditional(BCPos(bcemit_jmp(fs))));
   }

   // Patch all exits to point to after the try-except block
   exit_jmp.patch_here();
   for (ControlFlowEdge &handler_exit : handler_exits) {
      handler_exit.patch_here();
   }

   fs->try_depth = saved_try_depth; // Restore try depth

   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************
// Emit bytecode for raise statement: raise error_code [, message]
//
// Bytecode structure:
//   <evaluate error_code to register>
//   [<evaluate message to register>]
//   GGET     R(base)   "lj_raise_internal"    ; Get internal raise function
//   MOV      R(base+1) R(error_code)          ; Move error code to arg slot
//   [MOV     R(base+2) R(message)]            ; Move message if present
//   CALL     R(base)   1   2+has_msg          ; Call raise function (no results)

ParserResult<IrEmitUnit> IrEmitter::emit_raise_stmt(const RaiseStmtPayload &Payload, const SourceSpan &Span)
{
   FuncState *fs = &this->func_state;

   if (not Payload.error_code) {
      return ParserResult<IrEmitUnit>::failure(this->make_error(
         ParserErrorCode::InternalInvariant, "raise statement requires error code expression", Span));
   }

   // Save starting register state
   BCREG saved_freereg = fs->freereg;
   bool has_message = (Payload.message != nullptr);

   // Allocate registers for the call: function + frame slot + args
   // Layout: [base]=func, [base+1]=frame, [base+2]=arg1, [base+3]=arg2
   BCREG base = BCReg(fs->freereg);
   fs->freereg += 2 + LJ_FR2 + (has_message ? 1 : 0);  // func + frame + args

   // Load function from global: lj_raise_internal
   GCstr *fname = this->lex_state.keepstr("lj_raise_internal");
   bcemit_AD(fs, BC_GGET, base, const_gc(fs, obj2gco(fname), LJ_TSTR));

   // Evaluate error code expression and move to argument slot
   auto code_result = this->emit_expression(*Payload.error_code);
   if (not code_result.ok()) { fs->freereg = saved_freereg; return ParserResult<IrEmitUnit>::failure(code_result.error_ref()); }
   ExpDesc code_expr = code_result.value_ref();
   expr_toanyreg(fs, &code_expr);
   bcemit_AD(fs, BC_MOV, base + 1 + LJ_FR2, BCReg(code_expr.u.s.info));

   // Evaluate optional message expression and move to argument slot
   if (has_message) {
      auto msg_result = this->emit_expression(*Payload.message);
      if (not msg_result.ok()) { fs->freereg = saved_freereg; return ParserResult<IrEmitUnit>::failure(msg_result.error_ref()); }
      ExpDesc msg_expr = msg_result.value_ref();
      expr_toanyreg(fs, &msg_expr);
      bcemit_AD(fs, BC_MOV, base + 2 + LJ_FR2, BCReg(msg_expr.u.s.info));
   }

   // Emit call: B = number of results + 1 (0 results = B=1), C = number of args + 1
   bcemit_ABC(fs, BC_CALL, base, 1, has_message ? 3 : 2);

   // Restore freereg
   fs->freereg = saved_freereg;

   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************
// Emit bytecode for check statement: check expression
//
// Bytecode structure:
//   <evaluate expression to register>
//   GGET     R(base)   "lj_check_internal"    ; Get internal check function
//   MOV      R(base+1) R(error_code)          ; Move error code to arg slot
//   CALL     R(base)   1   2                  ; Call check function (no results, 1 arg)

ParserResult<IrEmitUnit> IrEmitter::emit_check_stmt(const CheckStmtPayload &Payload, const SourceSpan &Span)
{
   FuncState *fs = &this->func_state;

   if (not Payload.error_code) {
      return ParserResult<IrEmitUnit>::failure(this->make_error(
         ParserErrorCode::InternalInvariant, "check statement requires error code expression", Span));
   }

   // Save starting register state
   BCREG saved_freereg = fs->freereg;

   // Allocate registers for the call: function + frame slot + arg
   // Layout: [base]=func, [base+1]=frame, [base+2]=arg1
   BCREG base = BCReg(fs->freereg);
   fs->freereg += 2 + LJ_FR2;  // func + frame + arg

   // Load function from global: lj_check_internal
   GCstr *check_fname = this->lex_state.keepstr("lj_check_internal");
   bcemit_AD(fs, BC_GGET, base, const_gc(fs, obj2gco(check_fname), LJ_TSTR));

   // Evaluate error code expression and move to argument slot
   auto code_result = this->emit_expression(*Payload.error_code);
   if (not code_result.ok()) { fs->freereg = saved_freereg; return ParserResult<IrEmitUnit>::failure(code_result.error_ref()); }
   ExpDesc code_expr = code_result.value_ref();
   expr_toanyreg(fs, &code_expr);
   bcemit_AD(fs, BC_MOV, base + 1 + LJ_FR2, BCReg(code_expr.u.s.info));

   // Emit call: B = 1 (no results), C = 2 (1 arg + 1)
   bcemit_ABC(fs, BC_CALL, base, 1, 2);

   // Restore freereg
   fs->freereg = saved_freereg;

   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}
