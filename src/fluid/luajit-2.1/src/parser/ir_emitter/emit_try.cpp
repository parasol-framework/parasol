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

   fs->try_blocks.push_back(TryBlockDesc{
      0,             // Place-holder; set correctly after try body
      handler_count,
      0  // padding
   });

   // Track try depth for break/continue cleanup

   uint8_t saved_try_depth = fs->try_depth;
   fs->try_depth++;

   // Emit BC_TRYENTER with try block index
   bcemit_AD(fs, BC_TRYENTER, base_reg, BCReg(try_block_index));

   // Emit try body inline (not in closure!).  Nested try blocks will add their handlers to try_handlers during this phase.

   auto body_result = this->emit_block(*Payload.try_block, FuncScopeFlag::None);
   if (not body_result.ok()) {
      fs->try_depth = saved_try_depth;
      return body_result;
   }

   bcemit_AD(fs, BC_TRYLEAVE, base_reg, BCReg(0)); // Emit BC_TRYLEAVE after try body (normal exit path)

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
