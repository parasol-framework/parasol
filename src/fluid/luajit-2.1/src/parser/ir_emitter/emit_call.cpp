// Copyright (C) 2025 Paul Manias
// IR emitter implementation: call expression emission
//
// #included from ir_emitter.cpp

constexpr auto HASH_ASSERT  = pf::strhash("assert");
constexpr auto HASH_MSG     = pf::strhash("msg");
constexpr auto HASH_INCLUDE = pf::strhash("include");

//********************************************************************************************************************
// Pipe expression: lhs |> rhs_call()
// Prepends the LHS result(s) as argument(s) to the RHS function call.
// The RHS must be a CallExpr node.
//
// Register layout for calls:
//   R(base)   = function
//   R(base+1) = frame link (FR2, 64-bit mode)
//   R(base+2) = first argument (LHS piped value)
//   R(base+3...) = remaining arguments from RHS call

ParserResult<ExpDesc> IrEmitter::emit_pipe_expr(const PipeExprPayload &Payload)
{
   if (not Payload.lhs or not Payload.rhs_call) return this->unsupported_expr(AstNodeKind::PipeExpr, SourceSpan{});

   // The RHS must be a call expression - this was validated in the parser
   if (Payload.rhs_call->kind != AstNodeKind::CallExpr and Payload.rhs_call->kind != AstNodeKind::SafeCallExpr) {
      return this->unsupported_expr(AstNodeKind::PipeExpr, Payload.rhs_call->span);
   }

   BCLine call_line = this->lex_state.lastline;
   FuncState *fs = &this->func_state;

   const CallExprPayload &call_payload = std::get<CallExprPayload>(Payload.rhs_call->data);

   // Emit the callee (function) FIRST to establish base register

   ExpDesc callee;
   BCReg base(0);
   if (const auto* direct = std::get_if<DirectCallTarget>(&call_payload.target)) {
      if (not direct->callable) return this->unsupported_expr(AstNodeKind::PipeExpr, SourceSpan{});
      auto callee_result = this->emit_expression(*direct->callable);
      if (not callee_result.ok()) return callee_result;
      callee = callee_result.value_ref();
      this->materialise_to_next_reg(callee, "pipe call callee");
      RegisterAllocator allocator(fs);
      allocator.reserve(BCReg(1)); // Frame link (FR2)
      base = BCReg(callee.u.s.info);
   }
   else if (const auto *method = std::get_if<MethodCallTarget>(&call_payload.target)) {
      if (not method->receiver or method->method.symbol IS nullptr) {
         return this->unsupported_expr(AstNodeKind::PipeExpr, SourceSpan{});
      }
      auto receiver_result = this->emit_expression(*method->receiver);
      if (not receiver_result.ok()) return receiver_result;
      callee = receiver_result.value_ref();
      ExpDesc key(ExpKind::Str);
      key.u.sval = method->method.symbol;
      bcemit_method(fs, &callee, &key);
      base = BCReg(callee.u.s.info);
   }
   else return this->unsupported_expr(AstNodeKind::PipeExpr, SourceSpan{});

   // Emit LHS expression as the first argument(s)

   auto lhs_result = this->emit_expression(*Payload.lhs);
   if (not lhs_result.ok()) return lhs_result;
   ExpDesc lhs = lhs_result.value_ref();

   // Determine if LHS is a multi-value expression (function call)

   bool lhs_is_call = lhs.k IS ExpKind::Call;
   bool forward_multret = false;

   if (lhs_is_call) {
      // Multi-value case: discharge the call result to registers
      // If limit > 0, we want exactly 'limit' results
      // If limit == 0, we want all results (multi-return)

      if (Payload.limit > 0) {
         // Set BC_CALL B field to request exactly 'limit' return values
         // B = limit + 1 means "expect limit results"

         setbc_b(ir_bcptr(fs, &lhs), Payload.limit + 1);

         // The call results are placed starting at lhs.u.s.aux (the call base)
         // Update freereg to reflect the limited number of results

         fs->freereg = lhs.u.s.aux + Payload.limit;
      }
      else { // Forward all return values - keep B=0 for CALLM pattern
         setbc_b(ir_bcptr(fs, &lhs), 0);
         forward_multret = true;
      }
   }
   else { // Single value case - materialize to next register
      this->materialise_to_next_reg(lhs, "pipe LHS value");
   }

   // Emit remaining RHS arguments

   BCReg arg_count(0);
   ExpDesc args(ExpKind::Void);
   if (not call_payload.arguments.empty()) {
      auto args_result = this->emit_expression_list(call_payload.arguments, arg_count);
      if (not args_result.ok()) return ParserResult<ExpDesc>::failure(args_result.error_ref());
      args = args_result.value_ref();
   }

   // Emit the call instruction

   BCIns ins;
   if (forward_multret and call_payload.arguments.empty()) {
      // Use CALLM to forward all LHS return values as arguments (no additional args)
      // C field = number of fixed args before the vararg (0 in this case)
      ins = BCINS_ABC(BC_CALLM, base, 2, lhs.u.s.aux - base - 1 - 1);
   }
   else { // Regular CALL with fixed argument count
      if (args.k != ExpKind::Void) this->materialise_to_next_reg(args, "pipe rhs arguments");
      ins = BCINS_ABC(BC_CALL, base, 2, fs->freereg - base - 1);
   }

   this->lex_state.lastline = call_line;

   ExpDesc result;
   result.init(ExpKind::Call, bcemit_INS(fs, ins));
   result.u.s.aux = base;
   fs->freereg = base + 1;
   return ParserResult<ExpDesc>::success(result);
}

//********************************************************************************************************************
// Emit bytecode for a safe call expression (obj:?method()), returning nil if the receiver is nil.

ParserResult<ExpDesc> IrEmitter::emit_safe_call_expr(const CallExprPayload &Payload)
{
   BCLine call_line = this->lex_state.lastline;

   const auto* safe_method = std::get_if<SafeMethodCallTarget>(&Payload.target);
   if (not safe_method or not safe_method->receiver or safe_method->method.symbol IS nullptr) {
      return this->unsupported_expr(AstNodeKind::SafeCallExpr, SourceSpan{});
   }

   auto receiver_result = this->emit_expression(*safe_method->receiver);
   if (not receiver_result.ok()) return receiver_result;

   NilShortCircuitGuard guard(this, receiver_result.value_ref());
   if (not guard.ok()) return guard.error<ExpDesc>();

   // Method dispatch and arguments are evaluated only on non-nil path (short-circuit)

   ExpDesc callee = guard.base_expression();
   ExpDesc key(ExpKind::Str);
   key.u.sval = safe_method->method.symbol;
   bcemit_method(&this->func_state, &callee, &key);

   auto call_base = BCReg(callee.u.s.info);
   auto arg_count = BCReg(0);
   ExpDesc args(ExpKind::Void);
   if (not Payload.arguments.empty()) {
      auto args_result = this->emit_expression_list(Payload.arguments, arg_count);
      if (not args_result.ok()) return ParserResult<ExpDesc>::failure(args_result.error_ref());
      args = args_result.value_ref();
   }

   BCIns ins;
   bool forward_tail = Payload.forwards_multret and (args.k IS ExpKind::Call);
   if (forward_tail) {
      setbc_b(ir_bcptr(&this->func_state, &args), 0);
      ins = BCINS_ABC(BC_CALLM, call_base, 2, args.u.s.aux - call_base - 1  - 1);
   }
   else {
      if (not (args.k IS ExpKind::Void)) this->materialise_to_next_reg(args, "safe call arguments");
      ins = BCINS_ABC(BC_CALL, call_base, 2, this->func_state.freereg - call_base  - 1);
   }

   this->lex_state.lastline = call_line;
   auto call_pc = BCPos(bcemit_INS(&this->func_state, ins));

   return guard.complete_call(call_base, call_pc);
}

//********************************************************************************************************************
// Emit bytecode for a call expression (func(args) or obj:method(args)), handling direct and method calls.

ParserResult<ExpDesc> IrEmitter::emit_call_expr(const CallExprPayload &Payload)
{
   // We save lastline here before it gets overwritten by processing sub-expressions.
   BCLine call_line = this->lex_state.lastline;

   // First pass optimisations: In some cases we can optimise functions during parsing.
   // NOTE: Optimisations may cause confusion during debugging sessions, so we may want to add
   // a way to disable them if tracing/profiling is enabled.

   if (const auto *direct = std::get_if<DirectCallTarget>(&Payload.target)) {
      if (direct->callable and direct->callable->kind IS AstNodeKind::IdentifierExpr) {
         const auto *name_ref = std::get_if<NameRef>(&direct->callable->data);
         if (name_ref and name_ref->identifier.symbol) {
            GCstr *func_name = name_ref->identifier.symbol;

            if (func_name->hash IS HASH_ASSERT) this->optimise_assert(const_cast<ExprNodeList&>(Payload.arguments));
            else if ((func_name->hash IS HASH_MSG) and not glPrintMsg) {
               // msg() is eliminated entirely when debug messaging is disabled at compile time.
               return ParserResult<ExpDesc>::success(ExpDesc(ExpKind::Void));
            }
            else if (func_name->hash IS HASH_INCLUDE) {
               // Intercept include('module_name') to pre-load constants at parse time
               if (not Payload.arguments.empty() and
                   Payload.arguments[0]->kind IS AstNodeKind::LiteralExpr) {
                  const auto *lit = std::get_if<LiteralValue>(&Payload.arguments[0]->data);
                  if (lit and lit->kind IS LiteralKind::String and lit->string_value) {
                     std::string mod_name(strdata(lit->string_value), lit->string_value->len);
                     load_include(this->lex_state.L->script, mod_name.c_str());
                  }
               }
            }
         }
      }
   }

   ExpDesc callee;
   auto base = BCReg(0);
   bool is_safe_callable = false;
   FluidType callee_return_type = FluidType::Unknown;  // First return type of callee (if known)

   if (const auto *direct = std::get_if<DirectCallTarget>(&Payload.target)) {
      if (not direct->callable) return this->unsupported_expr(AstNodeKind::CallExpr, SourceSpan{});

      // Check if the callable is a safe navigation expression (?.field or ?[index])
      // If so, we need to add a nil check on the result before calling

      is_safe_callable = (direct->callable->kind IS AstNodeKind::SafeMemberExpr) or
                         (direct->callable->kind IS AstNodeKind::SafeIndexExpr);

      auto callee_result = this->emit_expression(*direct->callable);
      if (not callee_result.ok()) return callee_result;
      callee = callee_result.value_ref();

      // If callee is a local variable, check if it has known return types
      if (callee.k IS ExpKind::Local) {
         VarInfo* vinfo = &this->lex_state.vstack[callee.u.s.aux];
         callee_return_type = vinfo->result_types[0];
      }

      this->materialise_to_next_reg(callee, "call callee");
      // Reserve register for frame link
      RegisterAllocator allocator(&this->func_state);
      allocator.reserve(BCReg(1));
      base = BCReg(callee.u.s.info);
   }
   else if (const auto *method = std::get_if<MethodCallTarget>(&Payload.target)) {
      if (not method->receiver or method->method.symbol IS nullptr) {
         return this->unsupported_expr(AstNodeKind::CallExpr, SourceSpan{});
      }
      auto receiver_result = this->emit_expression(*method->receiver);
      if (not receiver_result.ok()) return receiver_result;
      callee = receiver_result.value_ref();
      ExpDesc key(ExpKind::Str);
      key.u.sval = method->method.symbol;
      bcemit_method(&this->func_state, &callee, &key);
      base = BCReg(callee.u.s.info);
   }
   else return this->unsupported_expr(AstNodeKind::CallExpr, SourceSpan{});

   // For safe callable expressions (obj?.method()), emit a nil check on the callable.
   // If the callable is nil, skip the call (including argument evaluation) and return nil instead.

   ControlFlowEdge nil_jump;
   if (is_safe_callable) {
      ExpDesc nilv(ExpKind::Nil);
      bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQP, base, const_pri(&nilv)));
      nil_jump = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));
   }

   // Evaluate arguments only after the nil check, so if callable is nil we skip argument evaluation
   auto arg_count = BCReg(0);
   ExpDesc args(ExpKind::Void);
   if (not Payload.arguments.empty()) {
      auto args_result = this->emit_expression_list(Payload.arguments, arg_count);
      if (not args_result.ok()) return ParserResult<ExpDesc>::failure(args_result.error_ref());
      args = args_result.value_ref();
   }

   BCIns ins;
   bool forward_tail = Payload.forwards_multret and (args.k IS ExpKind::Call);
   if (forward_tail) {
      setbc_b(ir_bcptr(&this->func_state, &args), 0);
      ins = BCINS_ABC(BC_CALLM, base, 2, args.u.s.aux - base - 1  - 1);
   }
   else {
      if (not (args.k IS ExpKind::Void)) this->materialise_to_next_reg(args, "call arguments");
      ins = BCINS_ABC(BC_CALL, base, 2, this->func_state.freereg - base  - 1);
   }

   // Restore the saved line number so the CALL instruction gets the correct line

   this->lex_state.lastline = call_line;

   auto call_pc = BCPos(bcemit_INS(&this->func_state, ins));

   // For safe callable: emit the nil path and patch jumps

   if (is_safe_callable) {
      ControlFlowEdge skip_nil = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));

      BCPos nil_path = BCPos(this->func_state.pc);
      nil_jump.patch_to(nil_path);
      bcemit_nil(&this->func_state, base.raw(), 1);

      skip_nil.patch_to(BCPos(this->func_state.pc));
   }

   ExpDesc result;
   result.init(ExpKind::Call, call_pc);
   result.u.s.aux = base;
   result.result_type = callee_return_type;  // Propagate known return type
   this->func_state.freereg = base + 1;
   return ParserResult<ExpDesc>::success(result);
}

//********************************************************************************************************************
// Optimise assert(condition, message) expressions by:
// 1. Wrapping expensive message expressions in an anonymous thunk for lazy evaluation
// 2. Appending line/column as additional arguments for runtime formatting
//
// Transforms: assert(cond, msg)            -> assert(cond, msg, line, col)
// Transforms: assert(cond, expensive())    -> assert(cond, (thunk():str return expensive() end)(), line, col)

void IrEmitter::optimise_assert(ExprNodeList &Args)
{
   // Requires at least 2 arguments: condition (Args[0]) and message (Args[1])
   if (Args.size() < 2) return;

   ExprNodePtr &msg_arg = Args[1];
   SourceSpan span = msg_arg->span;
   AstNodeKind msg_kind = msg_arg->kind;

   // Check if the message is already a thunk call
   bool is_already_thunk = false;
   if (msg_kind IS AstNodeKind::CallExpr) {
      const auto &call_payload = std::get<CallExprPayload>(msg_arg->data);
      if (std::holds_alternative<DirectCallTarget>(call_payload.target)) {
         const auto &target = std::get<DirectCallTarget>(call_payload.target);
         if (target.callable and target.callable->kind IS AstNodeKind::FunctionExpr) {
            const auto &func_payload = std::get<FunctionExprPayload>(target.callable->data);
            is_already_thunk = func_payload.is_thunk;
         }
      }
   }

   // Wrap expensive expressions in thunk for lazy evaluation
   bool needs_thunk = false;
   switch (msg_kind) {
      case AstNodeKind::LiteralExpr:    // String/number literals are cheap
      case AstNodeKind::IdentifierExpr: // Simple variable access is cheap
         break;
      case AstNodeKind::CallExpr:
         if (not is_already_thunk) needs_thunk = true;
         break;
      default:
         needs_thunk = true;
         break;
   }

   if (needs_thunk and not is_already_thunk) {
      // Wrap in thunk: (thunk():str return msg end)()
      ExprNodeList return_values;
      return_values.push_back(std::move(msg_arg));
      StmtNodePtr return_stmt = make_return_stmt(span, std::move(return_values), false);

      StmtNodeList body_stmts;
      body_stmts.push_back(std::move(return_stmt));
      auto body = make_block(span, std::move(body_stmts));

      ExprNodePtr thunk_func = make_function_expr(span, {}, false, std::move(body), true, FluidType::Str);

      ExprNodeList call_args;
      msg_arg = make_call_expr(span, std::move(thunk_func), std::move(call_args), false);
   }

   // Append line and column as literal arguments for runtime formatting
   auto line_num = Args[0]->span.line;
   auto column = Args[0]->span.column;

   Args.push_back(make_literal_expr(span, LiteralValue::number(double(line_num))));
   Args.push_back(make_literal_expr(span, LiteralValue::number(double(column))));
}

//********************************************************************************************************************
// Result filter expression: [_*]func(), [*_]obj:method(), etc.
// Transforms to: __filter(mask, count, trailing_keep, func(...))
// The __filter function is a built-in that selectively returns values based on the filter pattern.

ParserResult<ExpDesc> IrEmitter::emit_result_filter_expr(const ResultFilterPayload &Payload)
{
   if (not Payload.expression) return this->unsupported_expr(AstNodeKind::ResultFilterExpr, SourceSpan{});

   FuncState* fs = &this->func_state;

   // Look up and emit the __filter function
   BCReg base = fs->free_reg();
   ExpDesc filter_fn;
   this->lex_state.var_lookup_symbol(lj_str_newlit(this->lex_state.L, "__filter"), &filter_fn);
   this->materialise_to_next_reg(filter_fn, "filter function");

   // Reserve register for frame link
   RegisterAllocator allocator(fs);
   allocator.reserve(BCReg(1));

   // Emit arguments: mask, count, trailing_keep
   ExpDesc mask_expr(double(Payload.keep_mask));
   this->materialise_to_next_reg(mask_expr, "filter mask");

   ExpDesc count_expr(double(Payload.explicit_count));
   this->materialise_to_next_reg(count_expr, "filter count");

   ExpDesc trail_expr(Payload.trailing_keep);
   this->materialise_to_next_reg(trail_expr, "filter trailing");

   // Emit the call expression

   auto call_result = this->emit_expression(*Payload.expression);
   if (not call_result.ok()) return call_result;
   ExpDesc call = call_result.value_ref();

   // Set B=0 on the inner call to request all return values

   if (call.k IS ExpKind::Call) setbc_b(ir_bcptr(fs, &call), 0);
   this->materialise_to_next_reg(call, "filter input");

   // Emit CALLM to call __filter with variable arguments from the inner call
   // CALLM: base = function, C+1 = fixed args before vararg (3: mask, count, trailing)
   // The varargs come from the inner call's multiple returns

   BCIns ins = BCINS_ABC(BC_CALLM, base, 0, 3);  // 3 fixed args before vararg

   ExpDesc result;
   result.init(ExpKind::Call, bcemit_INS(fs, ins));
   result.u.s.aux = base;
   fs->freereg = base + 1;

   return ParserResult<ExpDesc>::success(result);
}
