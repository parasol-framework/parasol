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
   if (not Payload.try_block) {
      return this->unsupported_stmt(AstNodeKind::TryExceptStmt, SourceSpan{});
   }

   if (Payload.except_clauses.empty()) {
      return this->unsupported_stmt(AstNodeKind::TryExceptStmt, Payload.try_block->span);
   }

   if (Payload.except_clauses.size() > UINT8_MAX) {
      return this->unsupported_stmt(AstNodeKind::TryExceptStmt, Payload.try_block->span);
   }

   uint16_t try_block_index = uint16_t(fs->try_blocks.size());
   uint16_t first_handler = uint16_t(fs->try_handlers.size());
   uint8_t handler_count = uint8_t(Payload.except_clauses.size());

   fs->try_blocks.push_back(TryBlockDesc{first_handler, handler_count});

   BCReg base_reg = fs->free_reg();
   bcemit_AD(fs, BC_TRYENTER, base_reg, BCReg(try_block_index));

   auto body_result = this->emit_block(*Payload.try_block, FuncScopeFlag::None);
   if (not body_result.ok()) return body_result;

   bcemit_AD(fs, BC_TRYLEAVE, base_reg, BCReg(0));

   BCPos exit_jmp(bcemit_jmp(fs));
   std::vector<BCPos> handler_exits;
   handler_exits.reserve(Payload.except_clauses.size());

   auto pack_filter_codes = [this](const ExprNodeList &filter_codes) -> uint64_t {
      if (filter_codes.empty()) return 0;

      uint64_t packed = 0;
      int shift = 0;
      for (const auto &code_expr : filter_codes) {
         if (not code_expr or shift >= 64) break;

         auto code_result = this->emit_expression(*code_expr);
         if (not code_result.ok()) break;

         ExpDesc code = code_result.value_ref();
         if (code.k IS ExpKind::Num) {
            uint16_t code_val = uint16_t(code.number_value());
            packed |= (uint64_t(code_val) << shift);
            shift += 16;
         }
      }

      return packed;
   };

   for (const auto &clause : Payload.except_clauses) {
      FuncScope handler_scope;
      ScopeGuard scope_guard(fs, &handler_scope, FuncScopeFlag::None);

      BCReg exception_reg = BCReg(0xFF);
      if (clause.exception_var.has_value() and clause.exception_var->symbol) {
         exception_reg = fs->active_var_count();
         BCLine param_line = clause.exception_var->span.line;
         this->lex_state.var_new(exception_reg, clause.exception_var->symbol, param_line, clause.exception_var->span.column);
         this->lex_state.var_add(BCReg(1));
         RegisterAllocator handler_alloc(fs);
         handler_alloc.reserve(BCReg(1));
         this->update_local_binding(clause.exception_var->symbol, exception_reg);
      }

      BCPos handler_pc = fs->current_pc();
      uint64_t packed_filter = pack_filter_codes(clause.filter_codes);
      fs->try_handlers.push_back(TryHandlerDesc{
         packed_filter,
         handler_pc.raw(),
         BCREG(exception_reg.raw())
      });

      if (clause.block) {
         auto body = this->emit_block(*clause.block, FuncScopeFlag::None);
         if (not body.ok()) return body;
      }

      handler_exits.push_back(BCPos(bcemit_jmp(fs)));
   }

   BCPos try_end = fs->current_pc();
   JumpListView(fs, exit_jmp.raw()).patch_to(try_end.raw());
   for (auto handler_exit : handler_exits) {
      JumpListView(fs, handler_exit.raw()).patch_to(try_end.raw());
   }

   fs->ensure_freereg_at_locals();
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}
