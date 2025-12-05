// Copyright (C) 2025 Paul Manias

#include "parser/ir_emitter.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <utility>

#include <parasol/main.h>

#include "lj_debug.h"
#include "lj_tab.h"

#include "parser/parse_internal.h"
#include "parser/parse_value.h"
#include "parser/token_types.h"

#include "../../../defs.h"  // For glPrintMsg

//********************************************************************************************************************
// NilShortCircuitGuard - RAII helper for safe navigation nil-check pattern.
//
// Encapsulates the common control flow for safe navigation operators:
//   1. Discharge operand to register
//   2. Emit BC_ISEQP nil check with conditional jump
//   3. [Caller performs operation on non-nil path]
//   4. complete() emits nil path and patches jumps
//
// Usage:
//   NilShortCircuitGuard guard(emitter, base_expression);
//   if (not guard.ok()) return guard.error<ExpDesc>();
//   // ... perform operation using guard.base_register() ...
//   materialise_to_reg(result, guard.base_register(), "...");
//   return guard.complete();

class NilShortCircuitGuard {
public:
   NilShortCircuitGuard(IrEmitter *Emitter, ExpDesc BaseExpr)
      : emitter(Emitter), register_guard(&Emitter->func_state), allocator(&Emitter->func_state)
   {
      ExpressionValue base_value(&this->emitter->func_state, BaseExpr);
      this->result_reg = base_value.discharge_to_any_reg(this->allocator);
      this->base_expr = base_value.legacy();

      ExpDesc nilv(ExpKind::Nil);
      bcemit_INS(&this->emitter->func_state, BCINS_AD(BC_ISEQP, this->result_reg, const_pri(&nilv)));
      this->nil_jump = this->emitter->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->emitter->func_state)));
      this->setup_ok = true;
   }

   [[nodiscard]] bool ok() const { return this->setup_ok; }

   template<typename T>
   ParserResult<T> error() const {
      ParserError err;
      err.code = ParserErrorCode::InternalInvariant;
      err.message = "nil guard setup failed";
      return ParserResult<T>::failure(err);
   }

   [[nodiscard]] inline BCREG base_register() const { return this->result_reg; }
   [[nodiscard]] inline ExpDesc base_expression() const { return this->base_expr; }
   [[nodiscard]] inline RegisterAllocator& reg_allocator() { return this->allocator; }
   [[nodiscard]] inline ControlFlowEdge& nil_jump_edge() { return this->nil_jump; }

   // Complete the nil short-circuit: emit nil path, patch jumps, return result.
   // The result is stored in base_register() as a NonReloc expression.

   ParserResult<ExpDesc> complete()
   {
      this->allocator.collapse_freereg(BCReg(this->result_reg));

      ControlFlowEdge skip_nil = this->emitter->control_flow.make_unconditional(
         BCPos(bcemit_jmp(&this->emitter->func_state)));

      BCPos nil_path = BCPos(this->emitter->func_state.pc);
      this->nil_jump.patch_to(nil_path);
      bcemit_nil(&this->emitter->func_state, this->result_reg, 1);

      skip_nil.patch_to(BCPos(this->emitter->func_state.pc));

      this->register_guard.disarm();

      ExpDesc result;
      result.init(ExpKind::NonReloc, this->result_reg);
      return ParserResult<ExpDesc>::success(result);
   }

   // Complete with a custom result register (for call expressions where result may differ).
   // Note: Unlike complete(), we don't call collapse_freereg(result_reg) here because CallBase
   // may differ from result_reg after method dispatch setup, and we explicitly set freereg to
   // CallBase + 1 at the end, which is the correct final state for call expressions.

   ParserResult<ExpDesc> complete_call(BCReg CallBase, BCPos CallPc)
   {
      ControlFlowEdge skip_nil = this->emitter->control_flow.make_unconditional(
         BCPos(bcemit_jmp(&this->emitter->func_state)));

      BCPos nil_path = BCPos(this->emitter->func_state.pc);
      this->nil_jump.patch_to(nil_path);
      bcemit_nil(&this->emitter->func_state, CallBase.raw(), 1);

      skip_nil.patch_to(BCPos(this->emitter->func_state.pc));

      this->register_guard.adopt_saved(BCReg(CallBase.raw() + 1));
      this->register_guard.disarm();

      ExpDesc result;
      result.init(ExpKind::Call, CallPc);
      result.u.s.aux = CallBase;
      this->emitter->func_state.freereg = CallBase + 1;
      return ParserResult<ExpDesc>::success(result);
   }

private:
   IrEmitter *emitter;
   RegisterGuard register_guard;
   RegisterAllocator allocator;
   ControlFlowEdge nil_jump;
   ExpDesc base_expr;
   BCReg result_reg = BCReg(0);
   bool setup_ok = false;
};

//********************************************************************************************************************
// Snapshot return register state.
// Used by ir_emitter for return statement handling.

static void snapshot_return_regs(FuncState* fs, BCIns* ins)
{
   BCOp op = bc_op(*ins);

   if (op IS BC_RET1) {
      auto src = BCReg(bc_a(*ins));
      if (src < fs->nactvar) {
         RegisterAllocator allocator(fs);
         auto dst = fs->free_reg();
         allocator.reserve(BCReg(1));
         bcemit_AD(fs, BC_MOV, dst, src);
         setbc_a(ins, dst);
      }
   }
   else if (op IS BC_RET) {
      auto base = BCReg(bc_a(*ins));
      auto nres = BCReg(bc_d(*ins));
      auto top = BCReg(base.raw() + nres.raw() - 1);
      if (top < fs->nactvar) {
         auto dst = fs->free_reg();
         RegisterAllocator allocator(fs);
         allocator.reserve(nres);
         for (auto i = BCReg(0); i < nres; ++i) {
            bcemit_AD(fs, BC_MOV, dst + i, base + i);
         }
         setbc_a(ins, dst);
      }
   }
   else if (op IS BC_RETM) {
      auto base = BCReg(bc_a(*ins));
      auto nfixed = BCReg(bc_d(*ins));
      if (base < fs->nactvar) {
         auto dst = fs->free_reg();
         RegisterAllocator allocator(fs);
         allocator.reserve(nfixed);
         for (auto i = BCReg(0); i < nfixed; ++i) {
            bcemit_AD(fs, BC_MOV, dst + i, base + i);
         }
         setbc_a(ins, dst);
      }
   }
}

//********************************************************************************************************************
// Adjust LHS/RHS of an assignment.
// Exclusively used by ir_emitter for assignment statements, local declarations, and for loops.
// TOOD: May as well be a regular function instead of a LexState method.

void LexState::assign_adjust(BCREG nvars, BCREG nexps, ExpDesc *Expr)
{
   FuncState* fs = this->fs;
   RegisterAllocator allocator(fs);
   int32_t extra = int32_t(nvars) - int32_t(nexps);
   if (Expr->k IS ExpKind::Call) {
      extra++;  // Compensate for the ExpKind::Call itself.
      if (extra < 0) extra = 0;
      setbc_b(bcptr(fs, Expr), extra + 1);  // Fixup call results.
      if (extra > 1) allocator.reserve(BCReg(BCREG(extra) - 1));
   }
   else {
      if (Expr->k IS ExpKind::Void) {
         // Void expression contributes no values, so all LHS variables need nil.
         // This handles cases like `local a, b = assert(...)` where a shadow function might return void.
         extra = int32_t(nvars);
      }
      else {
         ExpressionValue value(fs, *Expr);
         value.to_next_reg(allocator);
         *Expr = value.legacy();
      }

      if (extra > 0) {  // Leftover LHS are set to nil.
         auto reg = fs->free_reg();
         allocator.reserve(BCReg(BCREG(extra)));
         bcemit_nil(fs, reg, BCREG(extra));
      }
   }

   if (nexps > nvars) fs->freereg -= nexps - nvars;  // Drop leftover regs.
}

//********************************************************************************************************************
// Restore the local binding table to the previous scope level by removing bindings added in the current scope.

void LocalBindingTable::pop_scope()
{
   if (this->scope_marks.empty()) {
      this->bindings.clear();
      this->depth = 0;
      return;
   }
   size_t restore = this->scope_marks.back();
   this->scope_marks.pop_back();
   this->bindings.resize(restore);
   if (this->depth > 0) this->depth--;
}

// Add a new local variable binding to the table, associating a symbol with its register slot.

void LocalBindingTable::add(GCstr* symbol, BCReg slot)
{
   if (not symbol) return;
   LocalBindingEntry entry;
   entry.symbol = symbol;
   entry.slot = slot;
   entry.depth = this->depth;
   this->bindings.push_back(entry);
}

//********************************************************************************************************************
// IR emission context implementation

namespace {

constexpr size_t kAstNodeKindCount = size_t(AstNodeKind::ExpressionStmt) + 1;

class UnsupportedNodeRecorder {
public:
   void record(AstNodeKind kind, const SourceSpan& span, const char* stage) {
      size_t index = size_t(kind);
      if (index >= this->counts.size()) return;
      uint32_t total = ++this->counts[index];
      if ((total <= 8) or (total % 32 IS 0)) {
         pf::Log log("Parser");
         log.msg("Unsupported %s node kind=%u hits=%u line=%d column=%d offset=%lld",
            stage, unsigned(kind), unsigned(total), int(span.line), int(span.column),
            (long long)(span.offset));
      }
   }

private:
   std::array<uint32_t, kAstNodeKindCount> counts{};
};

UnsupportedNodeRecorder glUnsupportedNodes;

//********************************************************************************************************************
// Check if an identifier is blank (underscore placeholder) or has no associated symbol.

[[nodiscard]] static bool is_blank_symbol(const Identifier& identifier)
{
   return identifier.is_blank or identifier.symbol IS nullptr;
}

//********************************************************************************************************************
// Map an AST binary operator to its corresponding bytecode binary operator representation.

[[nodiscard]] static std::optional<BinOpr> map_binary_operator(AstBinaryOperator op)
{
   switch (op) {
      case AstBinaryOperator::Add:          return BinOpr::Add;
      case AstBinaryOperator::Subtract:     return BinOpr::Sub;
      case AstBinaryOperator::Multiply:     return BinOpr::Mul;
      case AstBinaryOperator::Divide:       return BinOpr::Div;
      case AstBinaryOperator::Modulo:       return BinOpr::Mod;
      case AstBinaryOperator::Power:        return BinOpr::Pow;
      case AstBinaryOperator::Concat:       return BinOpr::Concat;
      case AstBinaryOperator::NotEqual:     return BinOpr::NotEqual;
      case AstBinaryOperator::Equal:        return BinOpr::Equal;
      case AstBinaryOperator::LessThan:     return BinOpr::LessThan;
      case AstBinaryOperator::GreaterEqual: return BinOpr::GreaterEqual;
      case AstBinaryOperator::LessEqual:    return BinOpr::LessEqual;
      case AstBinaryOperator::GreaterThan:  return BinOpr::GreaterThan;
      case AstBinaryOperator::BitAnd:       return BinOpr::BitAnd;
      case AstBinaryOperator::BitOr:        return BinOpr::BitOr;
      case AstBinaryOperator::BitXor:       return BinOpr::BitXor;
      case AstBinaryOperator::ShiftLeft:    return BinOpr::ShiftLeft;
      case AstBinaryOperator::ShiftRight:   return BinOpr::ShiftRight;
      case AstBinaryOperator::LogicalAnd:   return BinOpr::LogicalAnd;
      case AstBinaryOperator::LogicalOr:    return BinOpr::LogicalOr;
      case AstBinaryOperator::IfEmpty:      return BinOpr::IfEmpty;
      default: return std::nullopt;
   }
}

//********************************************************************************************************************
// Map a compound assignment operator (+=, -=, etc.) to its corresponding binary operator.

[[nodiscard]] static std::optional<BinOpr> map_assignment_operator(AssignmentOperator op)
{
   switch (op) {
      case AssignmentOperator::Add:      return BinOpr::Add;
      case AssignmentOperator::Subtract: return BinOpr::Sub;
      case AssignmentOperator::Multiply: return BinOpr::Mul;
      case AssignmentOperator::Divide:   return BinOpr::Div;
      case AssignmentOperator::Modulo:   return BinOpr::Mod;
      case AssignmentOperator::Concat:   return BinOpr::Concat;
      default: return std::nullopt;
   }
}

//********************************************************************************************************************
// Check if an auxiliary value represents a valid register key (used for indexed expressions).

[[nodiscard]] static bool is_register_key(uint32_t aux)
{
   return (int32_t(aux) >= 0) and (aux <= BCMAX_C);
}

//********************************************************************************************************************
// Convert an AST node kind enumeration to its human-readable string representation for debugging and logging.

[[nodiscard]] static std::string_view describe_node_kind(AstNodeKind kind)
{
   switch (kind) {
      case AstNodeKind::LiteralExpr:    return "LiteralExpr";
      case AstNodeKind::IdentifierExpr: return "IdentifierExpr";
      case AstNodeKind::VarArgExpr:     return "VarArgExpr";
      case AstNodeKind::UnaryExpr:      return "UnaryExpr";
      case AstNodeKind::BinaryExpr:     return "BinaryExpr";
      case AstNodeKind::UpdateExpr:     return "UpdateExpr";
      case AstNodeKind::TernaryExpr:    return "TernaryExpr";
      case AstNodeKind::PresenceExpr:   return "PresenceExpr";
      case AstNodeKind::CallExpr:       return "CallExpr";
      case AstNodeKind::MemberExpr:     return "MemberExpr";
      case AstNodeKind::IndexExpr:      return "IndexExpr";
      case AstNodeKind::ResultFilterExpr: return "ResultFilterExpr";
      case AstNodeKind::TableExpr:      return "TableExpr";
      case AstNodeKind::FunctionExpr:   return "FunctionExpr";
      case AstNodeKind::BlockStmt:      return "BlockStmt";
      case AstNodeKind::AssignmentStmt: return "AssignmentStmt";
      case AstNodeKind::LocalDeclStmt:  return "LocalDeclStmt";
      case AstNodeKind::LocalFunctionStmt: return "LocalFunctionStmt";
      case AstNodeKind::FunctionStmt:   return "FunctionStmt";
      case AstNodeKind::IfStmt:         return "IfStmt";
      case AstNodeKind::WhileStmt:      return "WhileStmt";
      case AstNodeKind::RepeatStmt:     return "RepeatStmt";
      case AstNodeKind::NumericForStmt: return "NumericForStmt";
      case AstNodeKind::GenericForStmt: return "GenericForStmt";
      case AstNodeKind::BreakStmt:      return "BreakStmt";
      case AstNodeKind::ContinueStmt:   return "ContinueStmt";
      case AstNodeKind::ReturnStmt:     return "ReturnStmt";
      case AstNodeKind::DeferStmt:      return "DeferStmt";
      case AstNodeKind::DoStmt:         return "DoStmt";
      case AstNodeKind::ConditionalShorthandStmt: return "ConditionalShorthandStmt";
      case AstNodeKind::ExpressionStmt: return "ExpressionStmt";
      default: return "Unknown";
   }
}

//********************************************************************************************************************
// Create an unresolved name reference from an identifier for later symbol resolution.

[[nodiscard]] static NameRef make_name_ref(const Identifier& identifier)
{
   NameRef ref;
   ref.identifier = identifier;
   ref.resolution = NameResolution::Unresolved;
   ref.slot = 0;
   return ref;
}

//********************************************************************************************************************
// Predict if a bytecode instruction loads the 'pairs' or 'next' iterator, used to optimise generic for loops.

[[nodiscard]] static int predict_next(LexState& lex_state, FuncState &func_state, BCPos pc)
{
   BCIns ins = func_state.bcbase[pc].ins;
   GCstr *name = nullptr;
   cTValue *table_entry = nullptr;

   switch (bc_op(ins)) {
      case BC_MOV:
         name = gco2str(gcref(func_state.var_get(bc_d(ins)).name));
         break;

      case BC_UGET:
         name = gco2str(gcref(func_state.var_get(func_state.uvmap[bc_d(ins)]).name));
         break;

      case BC_GGET:
         table_entry = lj_tab_getstr(func_state.kt, lj_str_newlit(lex_state.L, "pairs"));
         if (table_entry and tvhaskslot(table_entry) and tvkslot(table_entry) IS bc_d(ins)) return 1;
         table_entry = lj_tab_getstr(func_state.kt, lj_str_newlit(lex_state.L, "next"));
         if (table_entry and tvhaskslot(table_entry) and tvkslot(table_entry) IS bc_d(ins)) return 1;
         return 0;

      default:
         return 0;
   }

   if (not name) return 0;
   if (name->len IS 5 and !strcmp(strdata(name), "pairs")) return 1;
   if (name->len IS 4 and !strcmp(strdata(name), "next")) return 1;
   return 0;
}

//********************************************************************************************************************
// Release registers held by an indexed expression's base and key after they are no longer needed.

static void release_indexed_original(FuncState &func_state, const ExpDesc &original)
{
   if (original.k IS ExpKind::Indexed) {
      RegisterAllocator allocator(&func_state);
      uint32_t orig_aux = original.u.s.aux;
      if (is_register_key(orig_aux)) allocator.release_register(BCReg(orig_aux));
      allocator.release_register(BCReg(original.u.s.info));
   }
}

// Get a pointer to the bytecode instruction referenced by an expression descriptor.

[[nodiscard]] static BCIns* ir_bcptr(FuncState* func_state, const ExpDesc* expression)
{
   return &func_state->bcbase[expression->u.s.info].ins;
}

}  // namespace

IrEmitter::IrEmitter(ParserContext& context)
   : ctx(context),
     func_state(context.func()),
     lex_state(context.lex()),
     register_allocator(&this->func_state),
     control_flow(&this->func_state),
     operator_emitter(&this->func_state, &this->register_allocator, &this->control_flow)
{
}

IrEmitter::LoopStackGuard IrEmitter::push_loop_context(BCPos continue_target)
{
   LoopContext loop_context{};
   loop_context.break_edge = this->control_flow.make_break_edge();
   loop_context.continue_edge = this->control_flow.make_continue_edge();
   loop_context.defer_base = this->func_state.active_var_count();
   loop_context.continue_target = continue_target;
   this->loop_stack.push_back(loop_context);
   return LoopStackGuard(this);
}

//********************************************************************************************************************
// Emit bytecode for a complete function chunk, the top-level code block of a function.

ParserResult<IrEmitUnit> IrEmitter::emit_chunk(const BlockStmt& chunk)
{
   this->control_flow.reset(&this->func_state);
   FuncScope chunk_scope;
   ScopeGuard guard(&this->func_state, &chunk_scope, FuncScopeFlag::None);
   auto result = this->emit_block(chunk, FuncScopeFlag::None);
   if (not result.ok()) return result;
   this->control_flow.finalize();

   if (glPrintMsg) {
      // Verify no register leaks at function exit
      RegisterAllocator verifier(&this->func_state);
      verifier.verify_no_leaks("function exit");
   }

   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

// Emit bytecode for a block statement, creating a new scope with the specified flags.

ParserResult<IrEmitUnit> IrEmitter::emit_block(const BlockStmt& block, FuncScopeFlag flags)
{
   return this->emit_block_with_bindings(block, flags, std::span<const BlockBinding>());
}

//********************************************************************************************************************
// Emit bytecode for a block statement with pre-existing local bindings (used for loops and function parameters).

ParserResult<IrEmitUnit> IrEmitter::emit_block_with_bindings(
   const BlockStmt& block, FuncScopeFlag flags, std::span<const BlockBinding> bindings)
{
   FuncScope scope;
   ScopeGuard guard(&this->func_state, &scope, flags);
   LocalBindingScope binding_scope(this->binding_table);
   for (const BlockBinding& binding : bindings) {
      if (binding.symbol) this->update_local_binding(binding.symbol, binding.slot);
   }
   for (const StmtNode& stmt : block.view()) {
      auto status = this->emit_statement(stmt);
      if (not status.ok()) return status;
      this->ensure_register_balance(describe_node_kind(stmt.kind));
   }
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************
// Dispatch statement emission to the appropriate handler based on the AST node kind.

ParserResult<IrEmitUnit> IrEmitter::emit_statement(const StmtNode& stmt)
{
   // Update lexer's last line so bytecode emission uses correct line numbers
   this->lex_state.lastline = stmt.span.line;

   switch (stmt.kind) {
   case AstNodeKind::ExpressionStmt: {
      const auto &payload = std::get<ExpressionStmtPayload>(stmt.data);
      return this->emit_expression_stmt(payload);
   }
   case AstNodeKind::ReturnStmt: {
      const auto &payload = std::get<ReturnStmtPayload>(stmt.data);
      return this->emit_return_stmt(payload);
   }
   case AstNodeKind::LocalDeclStmt: {
      const auto &payload = std::get<LocalDeclStmtPayload>(stmt.data);
      return this->emit_local_decl_stmt(payload);
   }
   case AstNodeKind::LocalFunctionStmt: {
      const auto &payload = std::get<LocalFunctionStmtPayload>(stmt.data);
      return this->emit_local_function_stmt(payload);
   }
   case AstNodeKind::FunctionStmt: {
      const auto &payload = std::get<FunctionStmtPayload>(stmt.data);
      return this->emit_function_stmt(payload);
   }
   case AstNodeKind::AssignmentStmt: {
      const auto &payload = std::get<AssignmentStmtPayload>(stmt.data);
      return this->emit_assignment_stmt(payload);
   }
   case AstNodeKind::IfStmt: {
      const auto &payload = std::get<IfStmtPayload>(stmt.data);
      return this->emit_if_stmt(payload);
   }
   case AstNodeKind::WhileStmt: {
      const auto &payload = std::get<LoopStmtPayload>(stmt.data);
      return this->emit_while_stmt(payload);
   }
   case AstNodeKind::RepeatStmt: {
      const auto &payload = std::get<LoopStmtPayload>(stmt.data);
      return this->emit_repeat_stmt(payload);
   }
   case AstNodeKind::NumericForStmt: {
      const auto &payload = std::get<NumericForStmtPayload>(stmt.data);
      return this->emit_numeric_for_stmt(payload);
   }
   case AstNodeKind::GenericForStmt: {
      const auto &payload = std::get<GenericForStmtPayload>(stmt.data);
      return this->emit_generic_for_stmt(payload);
   }
   case AstNodeKind::DeferStmt: {
      const auto &payload = std::get<DeferStmtPayload>(stmt.data);
      return this->emit_defer_stmt(payload);
   }
   case AstNodeKind::BreakStmt: {
      const auto &payload = std::get<BreakStmtPayload>(stmt.data);
      return this->emit_break_stmt(payload);
   }
   case AstNodeKind::ContinueStmt: {
      const auto &payload = std::get<ContinueStmtPayload>(stmt.data);
      return this->emit_continue_stmt(payload);
   }
   case AstNodeKind::DoStmt: {
      const auto &payload = std::get<DoStmtPayload>(stmt.data);
      if (payload.block) return this->emit_block(*payload.block, FuncScopeFlag::None);
      return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
   }
   case AstNodeKind::ConditionalShorthandStmt: {
      const auto &payload = std::get<ConditionalShorthandStmtPayload>(stmt.data);
      return this->emit_conditional_shorthand_stmt(payload);
   }
   default:
      return this->unsupported_stmt(stmt.kind, stmt.span);
   }
}

//********************************************************************************************************************
// Emit bytecode for an expression statement, evaluating the expression and discarding its result.

ParserResult<IrEmitUnit> IrEmitter::emit_expression_stmt(const ExpressionStmtPayload &payload)
{
   if (not payload.expression) return ParserResult<IrEmitUnit>::success(IrEmitUnit{});

   auto expression = this->emit_expression(*payload.expression);
   if (not expression.ok()) return ParserResult<IrEmitUnit>::failure(expression.error_ref());

   ExpDesc value = expression.value_ref();
   ExpressionValue value_toval(&this->func_state, value);
   value_toval.to_val();
   value = value_toval.legacy();
   release_indexed_original(this->func_state, value);
   this->func_state.reset_freereg();
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************
// Emit bytecode for a conditional shorthand statement (executes body only for falsey values like nil, false, 0, or empty string).

ParserResult<IrEmitUnit> IrEmitter::emit_conditional_shorthand_stmt(const ConditionalShorthandStmtPayload& payload)
{
   if (not payload.condition or not payload.body) {
      return this->unsupported_stmt(AstNodeKind::ConditionalShorthandStmt, SourceSpan{});
   }

   auto condition_result = this->emit_expression(*payload.condition);
   if (not condition_result.ok()) {
      return ParserResult<IrEmitUnit>::failure(condition_result.error_ref());
   }

   RegisterGuard register_guard(&this->func_state);
   RegisterAllocator allocator(&this->func_state);

   ExpDesc condition = condition_result.value_ref();
   ExpressionValue condition_value(&this->func_state, condition);
   auto cond_reg = condition_value.discharge_to_any_reg(allocator);

   ExpDesc nilv(ExpKind::Nil);
   ExpDesc falsev(ExpKind::False);
   ExpDesc zerov(0.0);
   ExpDesc emptyv(this->lex_state.intern_empty_string());

   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQP, cond_reg, const_pri(&nilv)));
   ControlFlowEdge check_nil = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));
   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQP, cond_reg, const_pri(&falsev)));
   ControlFlowEdge check_false = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));
   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQN, cond_reg, const_num(&this->func_state, &zerov)));
   ControlFlowEdge check_zero = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));
   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQS, cond_reg, const_str(&this->func_state, &emptyv)));
   ControlFlowEdge check_empty = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));

   ControlFlowEdge skip_body = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));

   BCPos body_start = BCPos(this->func_state.pc);
   check_nil.patch_to(body_start);
   check_false.patch_to(body_start);
   check_zero.patch_to(body_start);
   check_empty.patch_to(body_start);

   auto body_result = this->emit_statement(*payload.body);
   if (not body_result.ok()) return body_result;

   skip_body.patch_to(BCPos(this->func_state.pc));

   allocator.collapse_freereg(BCReg(cond_reg));
   register_guard.disarm();
   this->func_state.reset_freereg();

   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************
// Emit bytecode for a return statement, handling zero, single, or multiple return values.

ParserResult<IrEmitUnit> IrEmitter::emit_return_stmt(const ReturnStmtPayload &payload)
{
   BCIns ins;
   this->func_state.flags |= PROTO_HAS_RETURN;
   if (payload.values.empty()) {
      ins = BCINS_AD(BC_RET0, 0, 1);
   }
   else {
      auto count = BCReg(0);
      auto list = this->emit_expression_list(payload.values, count);
      if (not list.ok()) return ParserResult<IrEmitUnit>::failure(list.error_ref());

      ExpDesc last = list.value_ref();
      if (count IS 1) {
         if (last.k IS ExpKind::Call) {
            BCIns* ip = ir_bcptr(&this->func_state, &last);
            if (bc_op(*ip) IS BC_VARG) {
               setbc_b(ir_bcptr(&this->func_state, &last), 0);
               ins = BCINS_AD(BC_RETM, this->func_state.nactvar, last.u.s.aux - this->func_state.nactvar);
            }
            else {
               this->func_state.pc--;
               ins = BCINS_AD(bc_op(*ip) - BC_CALL + BC_CALLT, bc_a(*ip), bc_c(*ip));
            }
         }
         else {
            RegisterAllocator allocator(&this->func_state);
            ExpressionValue value(&this->func_state, last);
            auto reg = value.discharge_to_any_reg(allocator);
            ins = BCINS_AD(BC_RET1, reg, 2);
         }
      }
      else {
         if (last.k IS ExpKind::Call) {
            setbc_b(ir_bcptr(&this->func_state, &last), 0);
            ins = BCINS_AD(BC_RETM, this->func_state.nactvar, last.u.s.aux - this->func_state.nactvar);
         }
         else {
            this->materialise_to_next_reg(last, "return tail value");
            ins = BCINS_AD(BC_RET, this->func_state.nactvar, count + 1);
         }
      }
   }

   snapshot_return_regs(&this->func_state, &ins);
   // Both __close and defer handlers must run before returning from function.
   // Order: closes before defers (LIFO - most recently declared runs first).
   execute_closes(&this->func_state, 0);
   execute_defers(&this->func_state, 0);
   if (this->func_state.flags & PROTO_CHILD) bcemit_AJ(&this->func_state, BC_UCLO, 0, 0);
   bcemit_INS(&this->func_state, ins);
   this->func_state.reset_freereg();
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************
// Emit bytecode for a local variable declaration statement, allocating slots and initialising values.

ParserResult<IrEmitUnit> IrEmitter::emit_local_decl_stmt(const LocalDeclStmtPayload& payload)
{
   auto nvars = BCReg(BCREG(payload.names.size()));
   if (nvars IS 0) return ParserResult<IrEmitUnit>::success(IrEmitUnit{});

   for (auto i = BCReg(0); i < nvars; ++i) {
      const Identifier& identifier = payload.names[i.raw()];
      GCstr* symbol = identifier.symbol;
      this->lex_state.var_new(i, is_blank_symbol(identifier) ? NAME_BLANK : symbol);
   }

   ExpDesc tail;
   auto nexps = BCReg(0);
   if (payload.values.empty()) tail = ExpDesc(ExpKind::Void);
   else {
      auto list = this->emit_expression_list(payload.values, nexps);
      if (not list.ok()) return ParserResult<IrEmitUnit>::failure(list.error_ref());
      tail = list.value_ref();
   }

   this->lex_state.assign_adjust(nvars.raw(), nexps.raw(), &tail);
   this->lex_state.var_add(nvars);
   auto base = BCReg(this->func_state.nactvar - nvars.raw());

   for (auto i = BCReg(0); i < nvars; ++i) {
      const Identifier& identifier = payload.names[i.raw()];
      if (not identifier.has_close) continue;

      // Check slot limit for closeslots bitmap (max 64 slots supported)
      uint8_t slot = uint8_t(base.raw() + i.raw());
      if (slot >= 64) {
         return ParserResult<IrEmitUnit>::failure(this->make_error(ParserErrorCode::InternalInvariant,
            "too many local variables with <close> attribute (max 64 slots)"));
      }

      VarInfo* info = &this->func_state.var_get(base.raw() + i.raw());
      info->info |= VarInfoFlag::Close;
   }

   for (auto i = BCReg(0); i < nvars; ++i) {
      const Identifier& identifier = payload.names[i.raw()];
      if (is_blank_symbol(identifier)) continue;
      this->update_local_binding(identifier.symbol, BCReg(base.raw() + i.raw()));
   }
   this->func_state.reset_freereg();
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************
// Emit bytecode for a local function declaration, creating a local variable and assigning a function to it.

ParserResult<IrEmitUnit> IrEmitter::emit_local_function_stmt(const LocalFunctionStmtPayload &payload)
{
   if (not payload.function) return this->unsupported_stmt(AstNodeKind::LocalFunctionStmt, SourceSpan{});

   GCstr* symbol = payload.name.symbol ? payload.name.symbol : NAME_BLANK;
   auto slot = BCReg(this->func_state.freereg);
   this->lex_state.var_new(0, symbol);
   ExpDesc variable;
   variable.init(ExpKind::Local, slot);
   variable.u.s.aux = this->func_state.varmap[slot];
   RegisterAllocator allocator(&this->func_state);
   allocator.reserve(BCReg(1));
   this->lex_state.var_add(1);

   auto function_value = this->emit_function_expr(*payload.function);
   if (not function_value.ok()) return ParserResult<IrEmitUnit>::failure(function_value.error_ref());

   ExpDesc fn = function_value.value_ref();
   this->materialise_to_reg(fn, slot, "local function literal");
   this->func_state.var_get(this->func_state.nactvar - 1).startpc = this->func_state.pc;
   if (payload.name.symbol and not payload.name.is_blank) this->update_local_binding(payload.name.symbol, slot);

   this->func_state.reset_freereg();
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************
// Emit bytecode for a function declaration statement, assigning a function to a global or table field.

ParserResult<IrEmitUnit> IrEmitter::emit_function_stmt(const FunctionStmtPayload& payload)
{
   if (not payload.function) return this->unsupported_stmt(AstNodeKind::FunctionStmt, SourceSpan{});

   auto target_result = this->emit_function_lvalue(payload.name);
   if (not target_result.ok()) return ParserResult<IrEmitUnit>::failure(target_result.error_ref());
   auto function_value = this->emit_function_expr(*payload.function);
   if (not function_value.ok()) return ParserResult<IrEmitUnit>::failure(function_value.error_ref());

   ExpDesc target = target_result.value_ref();
   ExpDesc value = function_value.value_ref();
   bcemit_store(&this->func_state, &target, &value);
   release_indexed_original(this->func_state, target);
   this->func_state.reset_freereg();
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************
// Emit bytecode for an if statement with one or more conditional clauses and an optional else clause.

ParserResult<IrEmitUnit> IrEmitter::emit_if_stmt(const IfStmtPayload& payload)
{
   if (payload.clauses.empty()) return ParserResult<IrEmitUnit>::success(IrEmitUnit{});

   ControlFlowEdge escapelist = this->control_flow.make_unconditional();
   for (size_t i = 0; i < payload.clauses.size(); ++i) {
      const IfClause& clause = payload.clauses[i];
      bool has_next = (i + 1) < payload.clauses.size();
      if (clause.condition) {
         auto condexit_result = this->emit_condition_jump(*clause.condition);
         if (not condexit_result.ok()) return ParserResult<IrEmitUnit>::failure(condexit_result.error_ref());

         ControlFlowEdge condexit = condexit_result.value_ref();

         if (clause.block) {
            auto block_result = this->emit_block(*clause.block, FuncScopeFlag::None);
            if (not block_result.ok()) return block_result;
         }

         if (has_next) {
            escapelist.append(BCPos(bcemit_jmp(&this->func_state)));
            condexit.patch_here();
         }
         else escapelist.append(condexit);
      }
      else if (clause.block) {
         auto block_result = this->emit_block(*clause.block, FuncScopeFlag::None);
         if (not block_result.ok()) return block_result;
      }
   }

   escapelist.patch_here();
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************
// Emit bytecode for a while loop, evaluating the condition before each iteration.

ParserResult<IrEmitUnit> IrEmitter::emit_while_stmt(const LoopStmtPayload& payload)
{
   if (payload.style != LoopStyle::WhileLoop or not payload.condition or not payload.body) {
      return this->unsupported_stmt(AstNodeKind::WhileStmt, SourceSpan{});
   }

   FuncState* fs = &this->func_state;
   BCPos start = BCPos(fs->lasttarget = fs->pc);
   auto loop_stack_guard = this->push_loop_context(start);
   auto condexit_result = this->emit_condition_jump(*payload.condition);
   if (not condexit_result.ok()) return ParserResult<IrEmitUnit>::failure(condexit_result.error_ref());

   ControlFlowEdge condexit = condexit_result.value_ref();

   ControlFlowEdge loop;

   {
      FuncScope loop_scope;
      ScopeGuard guard(fs, &loop_scope, FuncScopeFlag::Loop);
      loop = this->control_flow.make_unconditional(BCPos(bcemit_AD(fs, BC_LOOP, fs->nactvar, 0)));
      auto block_result = this->emit_block(*payload.body, FuncScopeFlag::None);
      if (not block_result.ok()) {
         return block_result;
      }
      ControlFlowEdge body_jump = this->control_flow.make_unconditional(BCPos(bcemit_jmp(fs)));
      body_jump.patch_to(BCPos(start));
   }

   condexit.patch_here();
   loop.patch_head(fs->current_pc());

   this->loop_stack.back().continue_edge.patch_to(start);
   this->loop_stack.back().break_edge.patch_here();
   loop_stack_guard.release();

   if (glPrintMsg) {
      // Verify no register leaks at loop exit
      RegisterAllocator verifier(fs);
      verifier.verify_no_leaks("while loop exit");
   }

   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************
// Emit bytecode for a repeat-until loop, executing the body at least once before testing the condition.

ParserResult<IrEmitUnit> IrEmitter::emit_repeat_stmt(const LoopStmtPayload& payload)
{
   if (payload.style != LoopStyle::RepeatUntil or not payload.condition or not payload.body) {
      return this->unsupported_stmt(AstNodeKind::RepeatStmt, SourceSpan{});
   }

   FuncState* fs = &this->func_state;
   BCPos loop = BCPos(fs->lasttarget = fs->pc);
   BCPos iter = BCPos(NO_JMP);
   ControlFlowEdge condexit;
   bool inner_has_upvals = false;

   auto loop_stack_guard = this->push_loop_context(loop);

   FuncScope outer_scope;
   ScopeGuard loop_guard(fs, &outer_scope, FuncScopeFlag::Loop);
   {
      FuncScope inner_scope;
      ScopeGuard inner_guard(fs, &inner_scope, FuncScopeFlag::None);
      bcemit_AD(fs, BC_LOOP, fs->nactvar, 0);
      auto block_result = this->emit_block(*payload.body, FuncScopeFlag::None);
      if (not block_result.ok()) return block_result;

      iter = fs->current_pc();
      auto cond_result = this->emit_condition_jump(*payload.condition);
      if (not cond_result.ok()) return ParserResult<IrEmitUnit>::failure(cond_result.error_ref());

      condexit = cond_result.value_ref();
      inner_has_upvals = has_flag(inner_scope.flags, FuncScopeFlag::Upvalue);
      if (inner_has_upvals) {
         auto break_result = this->emit_break_stmt(BreakStmtPayload{});
         if (not break_result.ok()) return break_result;
         condexit.patch_here();
      }
   }

   if (inner_has_upvals) condexit = this->control_flow.make_unconditional(BCPos(bcemit_jmp(fs)));

   condexit.patch_to(BCPos(loop));
   ControlFlowEdge loop_head = this->control_flow.make_unconditional(BCPos(loop));
   loop_head.patch_head(fs->current_pc());

   this->loop_stack.back().continue_target = iter;
   this->loop_stack.back().continue_edge.patch_to(BCPos(iter));
   this->loop_stack.back().break_edge.patch_here();
   loop_stack_guard.release();

   if (glPrintMsg) {
      // Verify no register leaks at loop exit
      RegisterAllocator verifier(fs);
      verifier.verify_no_leaks("repeat loop exit");
   }

   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************
// Emit bytecode for a numeric for loop with start, stop, and optional step values.

ParserResult<IrEmitUnit> IrEmitter::emit_numeric_for_stmt(const NumericForStmtPayload& payload)
{
   if (not payload.start or not payload.stop or not payload.body) {
      return this->unsupported_stmt(AstNodeKind::NumericForStmt, SourceSpan{});
   }

   FuncState* fs = &this->func_state;
   auto base = fs->free_reg();
   GCstr* control_symbol = payload.control.symbol ? payload.control.symbol : NAME_BLANK;

   FuncScope outer_scope;
   ScopeGuard loop_guard(fs, &outer_scope, FuncScopeFlag::Loop);

   this->lex_state.var_new_fixed(FORL_IDX, VARNAME_FOR_IDX);
   this->lex_state.var_new_fixed(FORL_STOP, VARNAME_FOR_STOP);
   this->lex_state.var_new_fixed(FORL_STEP, VARNAME_FOR_STEP);
   this->lex_state.var_new(FORL_EXT, control_symbol);

   auto start_expr = this->emit_expression(*payload.start);
   if (not start_expr.ok()) return ParserResult<IrEmitUnit>::failure(start_expr.error_ref());

   ExpDesc start_value = start_expr.value_ref();
   this->materialise_to_next_reg(start_value, "numeric for start");

   auto stop_expr = this->emit_expression(*payload.stop);
   if (not stop_expr.ok()) return ParserResult<IrEmitUnit>::failure(stop_expr.error_ref());

   ExpDesc stop_value = stop_expr.value_ref();
   this->materialise_to_next_reg(stop_value, "numeric for stop");

   if (payload.step) {
      auto step_expr = this->emit_expression(*payload.step);
      if (not step_expr.ok()) return ParserResult<IrEmitUnit>::failure(step_expr.error_ref());
      ExpDesc step_value = step_expr.value_ref();
      this->materialise_to_next_reg(step_value, "numeric for step");
   }
   else {
      RegisterAllocator allocator(fs);
      bcemit_AD(fs, BC_KSHORT, fs->freereg, 1);
      allocator.reserve(BCReg(1));
   }

   this->lex_state.var_add(3);

   auto loop_stack_guard = this->push_loop_context(BCPos(NO_JMP));

   ControlFlowEdge loop = this->control_flow.make_unconditional(BCPos(bcemit_AJ(fs, BC_FORI, base, NO_JMP)));

   {
      FuncScope visible_scope;
      ScopeGuard guard(fs, &visible_scope, FuncScopeFlag::None);
      this->lex_state.var_add(1);
      RegisterAllocator allocator(fs);
      allocator.reserve(BCReg(1));
      std::array<BlockBinding, 1> loop_bindings{};
      std::span<const BlockBinding> binding_span;
      if (payload.control.symbol and not payload.control.is_blank) {
         loop_bindings[0].symbol = payload.control.symbol;
         loop_bindings[0].slot = BCReg(base.raw() + FORL_EXT);
         binding_span = std::span<const BlockBinding>(loop_bindings.data(), 1);
      }
      auto block_result = this->emit_block_with_bindings(*payload.body, FuncScopeFlag::None, binding_span);
      if (not block_result.ok()) return block_result;
   }

   ControlFlowEdge loopend = this->control_flow.make_unconditional(BCPos(bcemit_AJ(fs, BC_FORL, base, NO_JMP)));
   fs->bcbase[loopend.head().raw()].line = payload.body->span.line;
   loopend.patch_head(BCPos(loop.head().raw() + 1));
   loop.patch_head(fs->current_pc());
   this->loop_stack.back().continue_target = loopend.head();
   this->loop_stack.back().continue_edge.patch_to(loopend.head());
   this->loop_stack.back().break_edge.patch_here();
   loop_stack_guard.release();
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************
// Emit bytecode for a generic for loop using iterator functions (e.g., pairs, ipairs).

ParserResult<IrEmitUnit> IrEmitter::emit_generic_for_stmt(const GenericForStmtPayload& payload)
{
   if (payload.names.empty() or payload.iterators.empty() or not payload.body) {
      return this->unsupported_stmt(AstNodeKind::GenericForStmt, SourceSpan{});
   }

   FuncState* fs = &this->func_state;
   auto base = BCReg(fs->freereg + 3);
   auto nvars = BCReg(0);

   FuncScope outer_scope;
   ScopeGuard loop_guard(fs, &outer_scope, FuncScopeFlag::Loop);

   this->lex_state.var_new_fixed(nvars++, VARNAME_FOR_GEN);
   this->lex_state.var_new_fixed(nvars++, VARNAME_FOR_STATE);
   this->lex_state.var_new_fixed(nvars++, VARNAME_FOR_CTL);

   for (const Identifier& identifier : payload.names) {
      GCstr* symbol = identifier.symbol ? identifier.symbol : NAME_BLANK;
      this->lex_state.var_new(nvars++, symbol);
   }

   BCPos exprpc = fs->current_pc();
   auto iterator_count = BCReg(0);
   auto iter_values = this->emit_expression_list(payload.iterators, iterator_count);
   if (not iter_values.ok()) return ParserResult<IrEmitUnit>::failure(iter_values.error_ref());

   ExpDesc tail = iter_values.value_ref();
   this->lex_state.assign_adjust(3, iterator_count.raw(), &tail);

   bcreg_bump(fs, 3  + 1);
   int isnext = (nvars <= 5) ? predict_next(this->lex_state, *fs, exprpc) : 0;
   this->lex_state.var_add(3);

   auto loop_stack_guard = this->push_loop_context(BCPos(NO_JMP));

   ControlFlowEdge loop = this->control_flow.make_unconditional(BCPos(bcemit_AJ(fs, isnext ? BC_ISNEXT : BC_JMP, base, NO_JMP)));

   {
      FuncScope visible_scope;
      ScopeGuard guard(fs, &visible_scope, FuncScopeFlag::None);
      auto visible = BCReg(nvars.raw() - 3);
      this->lex_state.var_add(visible);
      RegisterAllocator allocator(fs);
      allocator.reserve(visible);
      std::vector<BlockBinding> loop_bindings;
      loop_bindings.reserve(visible.raw());
      for (auto i = BCReg(0); i < visible; ++i) {
         const Identifier& identifier = payload.names[i.raw()];
         if (identifier.symbol and not identifier.is_blank) {
            BlockBinding binding;
            binding.symbol = identifier.symbol;
            binding.slot = BCReg(base.raw() + i.raw());
            loop_bindings.push_back(binding);
         }
      }
      std::span<const BlockBinding> binding_span(loop_bindings.data(), loop_bindings.size());
      auto block_result = this->emit_block_with_bindings(*payload.body, FuncScopeFlag::None, binding_span);
      if (not block_result.ok()) {
         return block_result;
      }
   }

   loop.patch_head(fs->current_pc());
   BCPos iter = BCPos(bcemit_ABC(fs, isnext ? BC_ITERN : BC_ITERC, base, nvars - BCREG(3) + BCREG(1), 3));
   ControlFlowEdge loopend = this->control_flow.make_unconditional(BCPos(bcemit_AJ(fs, BC_ITERL, base, NO_JMP)));
   fs->bcbase[loopend.head().raw() - 1].line = payload.body->span.line;
   fs->bcbase[loopend.head().raw()].line = payload.body->span.line;
   loopend.patch_head(BCPos(loop.head().raw() + 1));
   this->loop_stack.back().continue_target = iter;
   this->loop_stack.back().continue_edge.patch_to(iter);
   this->loop_stack.back().break_edge.patch_here();
   loop_stack_guard.release();
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************
// Emit bytecode for a defer statement, registering a function to execute when the current scope exits.

ParserResult<IrEmitUnit> IrEmitter::emit_defer_stmt(const DeferStmtPayload& payload)
{
   if (not payload.callable) return this->unsupported_stmt(AstNodeKind::DeferStmt, SourceSpan{});

   FuncState* fs = &this->func_state;
   auto reg = fs->free_reg();
   this->lex_state.var_new(0, NAME_BLANK);
   RegisterAllocator allocator(fs);
   allocator.reserve(BCReg(1));
   this->lex_state.var_add(1);
   VarInfo* info = &fs->var_get(fs->nactvar - 1);
   info->info |= VarInfoFlag::Defer;

   auto function_value = this->emit_function_expr(*payload.callable);
   if (not function_value.ok()) return ParserResult<IrEmitUnit>::failure(function_value.error_ref());

   ExpDesc fn = function_value.value_ref();
   this->materialise_to_reg(fn, reg, "defer callable");

   auto nargs = BCReg(0);
   for (const ExprNodePtr& argument : payload.arguments) {
      if (not argument) continue;

      auto arg_expr = this->emit_expression(*argument);
      if (not arg_expr.ok()) return ParserResult<IrEmitUnit>::failure(arg_expr.error_ref());

      ExpDesc arg = arg_expr.value_ref();
      this->materialise_to_next_reg(arg, "defer argument");
      nargs++;
   }

   if (nargs > 0) {
      for (auto i = BCReg(0); i < nargs; ++i) {
         this->lex_state.var_new(i, NAME_BLANK);
      }

      this->lex_state.var_add(nargs);

      for (auto i = BCReg(0); i < nargs; ++i) {
         VarInfo* arg_info = &fs->var_get(fs->nactvar - nargs.raw() + i.raw());
         arg_info->info |= VarInfoFlag::DeferArg;
      }
   }

   fs->reset_freereg();
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************
// Emit bytecode for a break statement, exiting the innermost loop after executing close and defer handlers.

ParserResult<IrEmitUnit> IrEmitter::emit_break_stmt(const BreakStmtPayload&)
{
   if (this->loop_stack.empty()) {
      return ParserResult<IrEmitUnit>::failure(this->make_error(ParserErrorCode::InternalInvariant, "break outside loop"));
   }

   LoopContext& loop = this->loop_stack.back();
   // Both __close and defer handlers must run when jumping out of scope via break.
   // Order: closes before defers (LIFO - most recently declared runs first).
   execute_closes(&this->func_state, loop.defer_base);
   execute_defers(&this->func_state, loop.defer_base);
   loop.break_edge.append(BCPos(bcemit_jmp(&this->func_state)));
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************
// Emit bytecode for a continue statement, jumping to the next iteration after executing close and defer handlers.

ParserResult<IrEmitUnit> IrEmitter::emit_continue_stmt(const ContinueStmtPayload&)
{
   if (this->loop_stack.empty()) {
      return ParserResult<IrEmitUnit>::failure(this->make_error(ParserErrorCode::InternalInvariant, "continue outside loop"));
   }

   LoopContext& loop = this->loop_stack.back();
   // Both __close and defer handlers must run when jumping out of scope via continue.
   // Order: closes before defers (LIFO - most recently declared runs first).
   execute_closes(&this->func_state, loop.defer_base);
   execute_defers(&this->func_state, loop.defer_base);
   loop.continue_edge.append(BCPos(bcemit_jmp(&this->func_state)));
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************
// Emit bytecode for an assignment statement, dispatching to plain, compound, or if-empty assignment handlers.

ParserResult<IrEmitUnit> IrEmitter::emit_assignment_stmt(const AssignmentStmtPayload &Payload)
{
   if (Payload.targets.empty()) return this->unsupported_stmt(AstNodeKind::AssignmentStmt, SourceSpan{});

   auto targets_result = this->prepare_assignment_targets(Payload.targets);
   if (not targets_result.ok()) return ParserResult<IrEmitUnit>::failure(targets_result.error_ref());

   std::vector<PreparedAssignment> targets = std::move(targets_result.value_ref());

   if (Payload.op IS AssignmentOperator::Plain) {
      return this->emit_plain_assignment(std::move(targets), Payload.values);
   }

   if (targets.size() != 1) {
      const ExprNodePtr& node = Payload.targets.front();
      const ExprNode* raw = node ? node.get() : nullptr;
      SourceSpan span = raw ? raw->span : SourceSpan{};
      return this->unsupported_stmt(AstNodeKind::AssignmentStmt, span);
   }

   PreparedAssignment target = std::move(targets.front());
   if (Payload.op IS AssignmentOperator::IfEmpty) {
      return this->emit_if_empty_assignment(std::move(target), Payload.values);
   }

   return this->emit_compound_assignment(Payload.op, std::move(target), Payload.values);
}

//********************************************************************************************************************
// Emit bytecode for a plain assignment, storing values into one or more target lvalues.

ParserResult<IrEmitUnit> IrEmitter::emit_plain_assignment(std::vector<PreparedAssignment> targets, const ExprNodeList& values)
{
   auto nvars = BCReg(BCREG(targets.size()));
   if (not nvars) return ParserResult<IrEmitUnit>::success(IrEmitUnit{});

   ExpDesc tail(ExpKind::Void);
   auto nexps = BCReg(0);
   if (not values.empty()) {
      auto list = this->emit_expression_list(values, nexps);
      if (not list.ok()) return ParserResult<IrEmitUnit>::failure(list.error_ref());
      tail = list.value_ref();
   }

   auto assign_from_stack = [&](std::vector<PreparedAssignment>::reverse_iterator first,
      std::vector<PreparedAssignment>::reverse_iterator last)
   {
      for (; first != last; ++first) {
         ExpDesc stack_value;
         stack_value.init(ExpKind::NonReloc, this->func_state.freereg - 1);
         bcemit_store(&this->func_state, &first->storage, &stack_value);
      }
   };

   RegisterAllocator allocator(&this->func_state);

   if (nexps IS nvars) {
      if (tail.k IS ExpKind::Call) {
         if (bc_op(*ir_bcptr(&this->func_state, &tail)) IS BC_VARG) {
            this->func_state.freereg--;
            tail.k = ExpKind::Relocable;
         }
         else {
            tail.u.s.info = tail.u.s.aux;
            tail.k = ExpKind::NonReloc;
         }
      }
      bcemit_store(&this->func_state, &targets.back().storage, &tail);
      if (targets.size() > 1) {
         auto begin = targets.rbegin();
         ++begin;
         assign_from_stack(begin, targets.rend());
      }
      for (PreparedAssignment& prepared : targets) {
         allocator.release(prepared.reserved);
      }
      this->func_state.reset_freereg();
      return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
   }

   this->lex_state.assign_adjust(nvars.raw(), nexps.raw(), &tail);
   assign_from_stack(targets.rbegin(), targets.rend());
   for (PreparedAssignment& prepared : targets) {
      allocator.release(prepared.reserved);
   }
   this->func_state.reset_freereg();
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************
// Emit bytecode for a compound assignment (+=, -=, etc.), combining a binary operation with storage.

ParserResult<IrEmitUnit> IrEmitter::emit_compound_assignment(AssignmentOperator op, PreparedAssignment target, const ExprNodeList &values)
{
   auto mapped = map_assignment_operator(op);
   if (not mapped.has_value()) {
      const ExprNode* raw = nullptr;
      if (not values.empty()) {
         const ExprNodePtr &first = values.front();
         raw = first ? first.get() : nullptr;
      }
      SourceSpan span = raw ? raw->span : SourceSpan{};
      return this->unsupported_stmt(AstNodeKind::AssignmentStmt, span);
   }

   if (values.empty()) return this->unsupported_stmt(AstNodeKind::AssignmentStmt, SourceSpan{});

   auto count = BCReg(0);
   RegisterGuard register_guard(&this->func_state);

   RegisterAllocator allocator(&this->func_state);
   TableOperandCopies copies = allocator.duplicate_table_operands(target.storage);
   ExpDesc working = copies.duplicated;

   ExpDesc rhs;
   if (mapped.value() IS BinOpr::Concat) {
      ExpDesc infix = working;
      // CONCAT compound assignment: use OperatorEmitter for BC_CAT chaining
      this->operator_emitter.prepare_concat(ExprValue(&infix));
      auto list = this->emit_expression_list(values, count);
      if (not list.ok()) return ParserResult<IrEmitUnit>::failure(list.error_ref());

      if (count != 1) {
         const ExprNodePtr& node = values.front();
         SourceSpan span = node ? node->span : SourceSpan{};
         return this->unsupported_stmt(AstNodeKind::AssignmentStmt, span);
      }

      rhs = list.value_ref();
      this->operator_emitter.complete_concat(ExprValue(&infix), rhs);
      bcemit_store(&this->func_state, &target.storage, &infix);
   }
   else {
      this->materialise_to_next_reg(working, "compound assignment base");
      auto list = this->emit_expression_list(values, count);
      if (not list.ok()) return ParserResult<IrEmitUnit>::failure(list.error_ref());

      if (count != 1) {
         const ExprNodePtr& node = values.front();
         SourceSpan span = node ? node->span : SourceSpan{};
         return this->unsupported_stmt(AstNodeKind::AssignmentStmt, span);
      }

      rhs = list.value_ref();
      ExpDesc infix = working;

      // Use OperatorEmitter for arithmetic compound assignments (+=, -=, *=, /=, %=)
      this->operator_emitter.emit_binary_arith(mapped.value(), ExprValue(&infix), rhs);

      bcemit_store(&this->func_state, &target.storage, &infix);
   }

   register_guard.release_to(register_guard.saved());
   allocator.release(target.reserved);
   allocator.release(copies.reserved);
   release_indexed_original(this->func_state, target.storage);
   this->func_state.reset_freereg();
   register_guard.adopt_saved(BCReg(this->func_state.freereg));
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************
// Emit bytecode for an if-empty assignment (?=), assigning only if the target is nil, false, 0, or empty string.

ParserResult<IrEmitUnit> IrEmitter::emit_if_empty_assignment(PreparedAssignment target, const ExprNodeList& values)
{
   if (values.empty() or not vkisvar(target.storage.k)) {
      return this->unsupported_stmt(AstNodeKind::AssignmentStmt, SourceSpan{});
   }

   auto count = BCReg(0);
   RegisterGuard register_guard(&this->func_state);

   // Use RegisterAllocator::duplicate_table_operands()
   RegisterAllocator allocator(&this->func_state);
   TableOperandCopies copies = allocator.duplicate_table_operands(target.storage);
   ExpDesc working = copies.duplicated;

   // Use ExpressionValue for discharge operations
   ExpressionValue lhs_value(&this->func_state, working);
   auto lhs_reg = lhs_value.discharge_to_any_reg(allocator);

   ExpDesc nilv(ExpKind::Nil);
   ExpDesc falsev(ExpKind::False);
   ExpDesc zerov(0.0);
   ExpDesc emptyv(this->lex_state.intern_empty_string());

   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQP, lhs_reg, const_pri(&nilv)));
   ControlFlowEdge check_nil = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));

   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQP, lhs_reg, const_pri(&falsev)));
   ControlFlowEdge check_false = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));

   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQN, lhs_reg, const_num(&this->func_state, &zerov)));
   ControlFlowEdge check_zero = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));

   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQS, lhs_reg, const_str(&this->func_state, &emptyv)));
   ControlFlowEdge check_empty = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));

   ControlFlowEdge skip_assign = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));
   BCPos assign_pos = BCPos(this->func_state.pc);

   auto list = this->emit_expression_list(values, count);
   if (not list.ok()) return ParserResult<IrEmitUnit>::failure(list.error_ref());

   if (count != 1) {
      const ExprNodePtr& node = values.front();
      SourceSpan span = node ? node->span : SourceSpan{};
      return this->unsupported_stmt(AstNodeKind::AssignmentStmt, span);
   }

   ExpDesc rhs = list.value_ref();
   ExpressionValue rhs_value(&this->func_state, rhs);
   rhs_value.discharge();
   this->materialise_to_reg(rhs_value.legacy(), lhs_reg, "assignment RHS");
   bcemit_store(&this->func_state, &target.storage, &rhs);

   check_nil.patch_to(BCPos(assign_pos));
   check_false.patch_to(BCPos(assign_pos));
   check_zero.patch_to(BCPos(assign_pos));
   check_empty.patch_to(BCPos(assign_pos));
   skip_assign.patch_to(BCPos(this->func_state.pc));

   register_guard.release_to(register_guard.saved());
   allocator.release(target.reserved);
   allocator.release(copies.reserved);
   release_indexed_original(this->func_state, target.storage);
   this->func_state.reset_freereg();
   register_guard.adopt_saved(BCReg(this->func_state.freereg));
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************
// Dispatch expression emission to the appropriate handler based on the AST node kind.

ParserResult<ExpDesc> IrEmitter::emit_expression(const ExprNode& expr)
{
   // Update lexer's last line so bytecode emission uses correct line numbers.
   // Complex expressions (like calls) may save/restore this to ensure their
   // final instruction gets the correct line.

   this->lex_state.lastline = expr.span.line;

   switch (expr.kind) {
      case AstNodeKind::LiteralExpr:    return this->emit_literal_expr(std::get<LiteralValue>(expr.data));
      case AstNodeKind::IdentifierExpr: return this->emit_identifier_expr(std::get<NameRef>(expr.data));
      case AstNodeKind::VarArgExpr:     return this->emit_vararg_expr();
      case AstNodeKind::UnaryExpr:      return this->emit_unary_expr(std::get<UnaryExprPayload>(expr.data));
      case AstNodeKind::UpdateExpr:     return this->emit_update_expr(std::get<UpdateExprPayload>(expr.data));
      case AstNodeKind::BinaryExpr:     return this->emit_binary_expr(std::get<BinaryExprPayload>(expr.data));
      case AstNodeKind::TernaryExpr:    return this->emit_ternary_expr(std::get<TernaryExprPayload>(expr.data));
      case AstNodeKind::PresenceExpr:   return this->emit_presence_expr(std::get<PresenceExprPayload>(expr.data));
      case AstNodeKind::PipeExpr:       return this->emit_pipe_expr(std::get<PipeExprPayload>(expr.data));
      case AstNodeKind::MemberExpr:     return this->emit_member_expr(std::get<MemberExprPayload>(expr.data));
      case AstNodeKind::IndexExpr:      return this->emit_index_expr(std::get<IndexExprPayload>(expr.data));
      case AstNodeKind::SafeMemberExpr: return this->emit_safe_member_expr(std::get<SafeMemberExprPayload>(expr.data));
      case AstNodeKind::SafeIndexExpr:  return this->emit_safe_index_expr(std::get<SafeIndexExprPayload>(expr.data));
      case AstNodeKind::SafeCallExpr:   return this->emit_safe_call_expr(std::get<CallExprPayload>(expr.data));
      case AstNodeKind::CallExpr:       return this->emit_call_expr(std::get<CallExprPayload>(expr.data));
      case AstNodeKind::ResultFilterExpr: return this->emit_result_filter_expr(std::get<ResultFilterPayload>(expr.data));
      case AstNodeKind::TableExpr:        return this->emit_table_expr(std::get<TableExprPayload>(expr.data));
      case AstNodeKind::FunctionExpr:     return this->emit_function_expr(std::get<FunctionExprPayload>(expr.data));
      default: return this->unsupported_expr(expr.kind, expr.span);
   }
}

//********************************************************************************************************************
// Emit bytecode for a conditional expression with a jump on false (used in if/while statements).

ParserResult<ControlFlowEdge> IrEmitter::emit_condition_jump(const ExprNode& expr)
{
   auto condition = this->emit_expression(expr);
   if (not condition.ok()) return ParserResult<ControlFlowEdge>::failure(condition.error_ref());
   ExpDesc result = condition.value_ref();
   if (result.k IS ExpKind::Nil) result.k = ExpKind::False;
   bcemit_branch_t(&this->func_state, &result);
   return ParserResult<ControlFlowEdge>::success(this->control_flow.make_false_edge(BCPos(result.f)));
}

//********************************************************************************************************************
// Emit bytecode for a literal expression (nil, boolean, number, string, or CData).

ParserResult<ExpDesc> IrEmitter::emit_literal_expr(const LiteralValue& literal)
{
   ExpDesc expr;
   switch (literal.kind) {
      case LiteralKind::Nil:     expr = ExpDesc(ExpKind::Nil); break;
      case LiteralKind::Boolean: expr = ExpDesc(literal.bool_value); break;
      case LiteralKind::Number:  expr = ExpDesc(literal.number_value); break;
      case LiteralKind::String:  expr = ExpDesc(literal.string_value); break;
      case LiteralKind::CData:
         expr.init(ExpKind::CData, 0);
         expr.u.nval = literal.cdata_value;
         break;
   }
   return ParserResult<ExpDesc>::success(expr);
}

//********************************************************************************************************************
// Emit bytecode for an identifier expression, resolving the name to a local, upvalue, or global variable.

ParserResult<ExpDesc> IrEmitter::emit_identifier_expr(const NameRef& reference)
{
   ExpDesc resolved;
   this->lex_state.var_lookup_symbol(reference.identifier.symbol, &resolved);
   return ParserResult<ExpDesc>::success(resolved);
}

//********************************************************************************************************************
// Emit bytecode for a vararg expression (...), accessing variadic function arguments.

ParserResult<ExpDesc> IrEmitter::emit_vararg_expr()
{
   ExpDesc expr;
   RegisterAllocator allocator(&this->func_state);
   allocator.reserve(BCReg(1));
   auto base = BCReg(this->func_state.freereg) - BCREG(1);
   expr.init(ExpKind::Call, bcemit_ABC(&this->func_state, BC_VARG, base, 2, this->func_state.numparams));
   expr.u.s.aux = base;
   expr.flags |= ExprFlag::HasRhsReg;
   return ParserResult<ExpDesc>::success(expr);
}

//********************************************************************************************************************
// Emit bytecode for a unary expression (negation, not, length, or bitwise not).

ParserResult<ExpDesc> IrEmitter::emit_unary_expr(const UnaryExprPayload& Payload)
{
   if (not Payload.operand) return this->unsupported_expr(AstNodeKind::UnaryExpr, SourceSpan{});
   auto operand_result = this->emit_expression(*Payload.operand);
   if (not operand_result.ok()) return operand_result;
   ExpDesc operand = operand_result.value_ref();

   // Use OperatorEmitter facade for unary operators
   switch (Payload.op) {
      case AstUnaryOperator::Negate: this->operator_emitter.emit_unary(BC_UNM, ExprValue(&operand)); break;
      case AstUnaryOperator::Not: this->operator_emitter.emit_unary(BC_NOT, ExprValue(&operand)); break;
      case AstUnaryOperator::Length: this->operator_emitter.emit_unary(BC_LEN, ExprValue(&operand)); break;
      case AstUnaryOperator::BitNot:
         // BitNot calls bit.bnot library function
         this->operator_emitter.emit_bitnot(ExprValue(&operand));
         break;
   }
   return ParserResult<ExpDesc>::success(operand);
}

//********************************************************************************************************************
// Emit bytecode for an update expression (++, --), incrementing or decrementing a variable in place.

ParserResult<ExpDesc> IrEmitter::emit_update_expr(const UpdateExprPayload& Payload)
{
   if (not Payload.target) return this->unsupported_expr(AstNodeKind::UpdateExpr, SourceSpan{});

   auto target_result = this->emit_lvalue_expr(*Payload.target);
   if (not target_result.ok()) return target_result;

   ExpDesc target = target_result.value_ref();

   // Use RegisterAllocator::duplicate_table_operands()
   RegisterAllocator allocator(&this->func_state);
   TableOperandCopies copies = allocator.duplicate_table_operands(target);
   ExpDesc working = copies.duplicated;

   BinOpr op = (Payload.op IS AstUpdateOperator::Increment) ? BinOpr::Add : BinOpr::Sub;

   // Use ExpressionValue for discharge operations
   ExpressionValue operand_value(&this->func_state, working);
   auto operand_reg = operand_value.discharge_to_any_reg(allocator);

   auto saved_reg = operand_reg;
   if (Payload.is_postfix) {
      saved_reg = BCReg(this->func_state.freereg);
      bcemit_AD(&this->func_state, BC_MOV, saved_reg, operand_reg);
      allocator.reserve(BCReg(1));
   }

   ExpDesc operand = operand_value.legacy();  // Get ExpDesc for subsequent operations
   ExpDesc delta(1.0);
   ExpDesc infix = operand;

   // Use OperatorEmitter for arithmetic operation (operand +/- 1)
   this->operator_emitter.emit_binary_arith(op, ExprValue(&infix), delta);

   bcemit_store(&this->func_state, &target, &infix);
   release_indexed_original(this->func_state, target);

   if (Payload.is_postfix) {
      allocator.collapse_freereg(BCReg(saved_reg));
      ExpDesc result;
      result.init(ExpKind::NonReloc, saved_reg);
      return ParserResult<ExpDesc>::success(result);
   }

   return ParserResult<ExpDesc>::success(infix);
}

//********************************************************************************************************************
// Emit bytecode for a binary expression (arithmetic, comparison, logical, bitwise, or concatenation operators).

ParserResult<ExpDesc> IrEmitter::emit_binary_expr(const BinaryExprPayload& Payload)
{
   auto lhs_result = this->emit_expression(*Payload.left);
   if (not lhs_result.ok()) return lhs_result;

   auto mapped = map_binary_operator(Payload.op);
   if (not mapped.has_value()) {
      SourceSpan span = Payload.left ? Payload.left->span : SourceSpan{};
      return this->unsupported_expr(AstNodeKind::BinaryExpr, span);
   }

   BinOpr opr = mapped.value();
   ExpDesc lhs = lhs_result.value_ref();

   // IF_EMPTY requires special handling - it must emit RHS conditionally like ternary
   // Cannot use the standard prepare/emit RHS/complete pattern

   if (opr IS BinOpr::IfEmpty) return this->emit_if_empty_expr(lhs, *Payload.right);

   // ALL binary operators need binop_left preparation before RHS evaluation

   // This discharges LHS to appropriate form to prevent register clobbering

   if (opr IS BinOpr::LogicalAnd) { // Logical AND: CFG-based short-circuit implementation
      this->operator_emitter.prepare_logical_and(ExprValue(&lhs));
   }
   else if (opr IS BinOpr::LogicalOr) { // Logical OR: CFG-based short-circuit implementation
      this->operator_emitter.prepare_logical_or(ExprValue(&lhs));
   }
   else if (opr IS BinOpr::Concat) { // CONCAT: Discharge to consecutive register for BC_CAT chaining
      this->operator_emitter.prepare_concat(ExprValue(&lhs));
   }
   else { // All other operators use OperatorEmitter facade
      this->operator_emitter.emit_binop_left(opr, ExprValue(&lhs));
   }

   // Now evaluate RHS (safe because binop_left prepared LHS)

   auto rhs_result = this->emit_expression(*Payload.right);
   if (not rhs_result.ok()) return rhs_result;
   ExpDesc rhs = rhs_result.value_ref();

   // Emit the actual operation based on operator type
   if (opr IS BinOpr::LogicalAnd) { // Logical AND: CFG-based short-circuit implementation
      this->operator_emitter.complete_logical_and(ExprValue(&lhs), rhs);
   }
   else if (opr IS BinOpr::LogicalOr) { // Logical OR: CFG-based short-circuit implementation
      this->operator_emitter.complete_logical_or(ExprValue(&lhs), rhs);
   }
   else if (opr >= BinOpr::NotEqual and opr <= BinOpr::GreaterThan) { // Comparison operators (NE, EQ, LT, GE, LE, GT)
      this->operator_emitter.emit_comparison(opr, ExprValue(&lhs), rhs);
   }
   else if (opr IS BinOpr::Concat) { // CONCAT: CFG-based implementation with BC_CAT chaining
      this->operator_emitter.complete_concat(ExprValue(&lhs), rhs);
   }
   else if (opr IS BinOpr::BitAnd or opr IS BinOpr::BitOr or opr IS BinOpr::BitXor or opr IS BinOpr::ShiftLeft or opr IS BinOpr::ShiftRight) {
      // Bitwise operators: Route through OperatorEmitter (emits bit.* library calls)
      this->operator_emitter.emit_binary_bitwise(opr, ExprValue(&lhs), rhs);
   }
   else { // Arithmetic operators (ADD, SUB, MUL, DIV, MOD, POW)
      this->operator_emitter.emit_binary_arith(opr, ExprValue(&lhs), rhs);
   }

   return ParserResult<ExpDesc>::success(lhs);
}

//********************************************************************************************************************
// IF_EMPTY (lhs ?? rhs) with conditional RHS emission for proper short-circuit semantics
// Similar to ternary but with extended falsey checks (nil, false, 0, "")

ParserResult<ExpDesc> IrEmitter::emit_if_empty_expr(ExpDesc lhs, const ExprNode& rhs_ast)
{
   // Use RegisterGuard for automatic register cleanup on all exit paths (RAII)
   RegisterGuard register_guard(&this->func_state);

   RegisterAllocator allocator(&this->func_state);
   ExpressionValue lhs_value(&this->func_state, lhs);
   auto lhs_reg = lhs_value.discharge_to_any_reg(allocator);

   ExpDesc nilv(ExpKind::Nil);
   ExpDesc falsev(ExpKind::False);
   ExpDesc zerov(0.0);
   ExpDesc emptyv(this->lex_state.intern_empty_string());

   // Extended falsey checks - jumps skip to RHS when value is falsey
   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQP, lhs_reg, const_pri(&nilv)));
   ControlFlowEdge check_nil = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));
   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQP, lhs_reg, const_pri(&falsev)));
   ControlFlowEdge check_false = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));
   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQN, lhs_reg, const_num(&this->func_state, &zerov)));
   ControlFlowEdge check_zero = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));
   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQS, lhs_reg, const_str(&this->func_state, &emptyv)));
   ControlFlowEdge check_empty = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));

   // LHS is truthy - it's already in lhs_reg, just skip RHS
   ControlFlowEdge skip_rhs = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));

   // Patch falsey checks to jump here (RHS evaluation)

   BCPos rhs_start = BCPos(this->func_state.pc);
   check_nil.patch_to(rhs_start);
   check_false.patch_to(BCPos(rhs_start));
   check_zero.patch_to(BCPos(rhs_start));
   check_empty.patch_to(BCPos(rhs_start));

   // Emit RHS - only executed when LHS is falsey

   auto rhs_result = this->emit_expression(rhs_ast);
   if (not rhs_result.ok()) return rhs_result;
   ExpressionValue rhs_value(&this->func_state, rhs_result.value_ref());
   rhs_value.discharge();
   this->materialise_to_reg(rhs_value.legacy(), lhs_reg, "if_empty rhs");

   // Clean up any RHS temporaries, but preserve the result register

   allocator.collapse_freereg(BCReg(lhs_reg));

   // Patch skip jump to here (after RHS)

   skip_rhs.patch_to(BCPos(this->func_state.pc));

   // Preserve result register by adjusting what RegisterGuard will restore to
   // Only restore to saved_freereg if it's beyond the result register

   if (register_guard.saved() > BCReg(lhs_reg + 1)) register_guard.adopt_saved(register_guard.saved());
   else register_guard.disarm();  // Keep current freereg (lhs_reg + 1)

   // Result is in lhs_reg

   ExpDesc result;
   result.init(ExpKind::NonReloc, lhs_reg);
   return ParserResult<ExpDesc>::success(result);
}

//********************************************************************************************************************
// Emit bytecode for a ternary expression (condition ? true_value : false_value), with falsey checks.

ParserResult<ExpDesc> IrEmitter::emit_ternary_expr(const TernaryExprPayload &Payload)
{
   if (not Payload.condition or not Payload.if_true or not Payload.if_false) {
      return this->unsupported_expr(AstNodeKind::TernaryExpr, SourceSpan{});
   }

   auto condition_result = this->emit_expression(*Payload.condition);
   if (not condition_result.ok()) return condition_result;

   // Use RegisterGuard for automatic register cleanup on all exit paths (RAII)

   RegisterGuard register_guard(&this->func_state);
   RegisterAllocator allocator(&this->func_state);
   ExpressionValue condition_value(&this->func_state, condition_result.value_ref());
   auto cond_reg = condition_value.discharge_to_any_reg(allocator);

   ExpDesc nilv(ExpKind::Nil);
   ExpDesc falsev(ExpKind::False);
   ExpDesc zerov(0.0);
   ExpDesc emptyv(this->lex_state.intern_empty_string());

   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQP, cond_reg, const_pri(&nilv)));
   ControlFlowEdge check_nil = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));
   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQP, cond_reg, const_pri(&falsev)));
   ControlFlowEdge check_false = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));
   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQN, cond_reg, const_num(&this->func_state, &zerov)));
   ControlFlowEdge check_zero = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));
   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQS, cond_reg, const_str(&this->func_state, &emptyv)));
   ControlFlowEdge check_empty = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));

   auto true_result = this->emit_expression(*Payload.if_true);
   if (not true_result.ok()) return true_result;

   ExpressionValue true_value(&this->func_state, true_result.value_ref());
   true_value.discharge();
   this->materialise_to_reg(true_value.legacy(), cond_reg, "ternary true branch");
   allocator.collapse_freereg(BCReg(cond_reg));

   ControlFlowEdge skip_false = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));

   BCPos false_start = BCPos(this->func_state.pc);
   check_nil.patch_to(false_start);
   check_false.patch_to(BCPos(false_start));
   check_zero.patch_to(BCPos(false_start));
   check_empty.patch_to(BCPos(false_start));

   auto false_result = this->emit_expression(*Payload.if_false);
   if (not false_result.ok()) return false_result;
   ExpressionValue false_value(&this->func_state, false_result.value_ref());
   false_value.discharge();
   this->materialise_to_reg(false_value.legacy(), cond_reg, "ternary false branch");
   allocator.collapse_freereg(BCReg(cond_reg));

   skip_false.patch_to(BCPos(this->func_state.pc));

   // Preserve result register by adjusting what RegisterGuard will restore to
   // Only restore to saved_freereg if it's beyond the result register

   if (register_guard.saved() > BCReg(cond_reg + 1)) register_guard.adopt_saved(register_guard.saved());
   else register_guard.disarm();  // Keep current freereg (cond_reg + 1)

   ExpDesc result;
   result.init(ExpKind::NonReloc, cond_reg);
   return ParserResult<ExpDesc>::success(result);
}

//********************************************************************************************************************
// Emit bytecode for a presence check expression (??), testing if a value is not nil.

ParserResult<ExpDesc> IrEmitter::emit_presence_expr(const PresenceExprPayload &Payload)
{
   SourceSpan span = Payload.value ? Payload.value->span : SourceSpan{};
   if (not Payload.value) return this->unsupported_expr(AstNodeKind::PresenceExpr, span);
   auto value_result = this->emit_expression(*Payload.value);
   if (not value_result.ok()) return value_result;
   ExpDesc value = value_result.value_ref();
   this->operator_emitter.emit_presence_check(ExprValue(&value));
   return ParserResult<ExpDesc>::success(value);
}

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
   if (not Payload.lhs or not Payload.rhs_call) {
      return this->unsupported_expr(AstNodeKind::PipeExpr, SourceSpan{});
   }

   // The RHS must be a call expression - this was validated in the parser
   if (Payload.rhs_call->kind != AstNodeKind::CallExpr and
       Payload.rhs_call->kind != AstNodeKind::SafeCallExpr) {
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
// Emit bytecode for a member access expression (table.field), indexing a table with a string key.

ParserResult<ExpDesc> IrEmitter::emit_member_expr(const MemberExprPayload &Payload)
{
   if (not Payload.table or Payload.member.symbol IS nullptr) {
      return this->unsupported_expr(AstNodeKind::MemberExpr, Payload.member.span);
   }

   auto table_result = this->emit_expression(*Payload.table);
   if (not table_result.ok()) return table_result;

   ExpDesc table = table_result.value_ref();
   RegisterAllocator allocator(&this->func_state);
   ExpressionValue table_value(&this->func_state, table);
   table_value.discharge_to_any_reg(allocator);
   table = table_value.legacy();
   ExpDesc key(Payload.member.symbol);
   expr_index(&this->func_state, &table, &key);
   return ParserResult<ExpDesc>::success(table);
}

//********************************************************************************************************************
// Emit bytecode for an index expression (table[key]), indexing a table with an arbitrary key.

ParserResult<ExpDesc> IrEmitter::emit_index_expr(const IndexExprPayload &Payload)
{
   if (not Payload.table or not Payload.index) return this->unsupported_expr(AstNodeKind::IndexExpr, SourceSpan{});

   auto table_result = this->emit_expression(*Payload.table);
   if (not table_result.ok()) return table_result;
   ExpDesc table = table_result.value_ref();
   // Materialize table BEFORE evaluating key, so nested index expressions emit bytecode in
   // the correct order (table first, then key)
   RegisterAllocator allocator(&this->func_state);
   ExpressionValue table_value(&this->func_state, table);
   table_value.discharge_to_any_reg(allocator);
   table = table_value.legacy();
   auto key_result = this->emit_expression(*Payload.index);
   if (not key_result.ok()) return key_result;
   ExpDesc key = key_result.value_ref();
   ExpressionValue key_toval(&this->func_state, key);
   key_toval.to_val();
   key = key_toval.legacy();
   expr_index(&this->func_state, &table, &key);
   return ParserResult<ExpDesc>::success(table);
}

//********************************************************************************************************************
// Emit bytecode for a safe member access expression (table?.field), returning nil if the table is nil.

ParserResult<ExpDesc> IrEmitter::emit_safe_member_expr(const SafeMemberExprPayload &Payload)
{
   if (not Payload.table or Payload.member.symbol IS nullptr) {
      return this->unsupported_expr(AstNodeKind::SafeMemberExpr, Payload.member.span);
   }

   auto table_result = this->emit_expression(*Payload.table);
   if (not table_result.ok()) return table_result;

   NilShortCircuitGuard guard(this, table_result.value_ref());
   if (not guard.ok()) return guard.error<ExpDesc>();

   ExpDesc table = guard.base_expression();
   ExpDesc key(Payload.member.symbol);
   expr_index(&this->func_state, &table, &key);

   // Materialize the indexed result to a new register.
   // Do NOT reuse base_register() as that would clobber the table variable

   ExpressionValue indexed_value(&this->func_state, table);
   BCReg result_reg = indexed_value.discharge_to_any_reg(guard.reg_allocator());

   // Collapse freereg to include the result register
   guard.reg_allocator().collapse_freereg(result_reg);

   // Emit the nil path
   ControlFlowEdge skip_nil = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));

   BCPos nil_path = BCPos(this->func_state.pc);
   guard.nil_jump_edge().patch_to(nil_path);
   bcemit_nil(&this->func_state, result_reg.raw(), 1);

   skip_nil.patch_to(BCPos(this->func_state.pc));

   ExpDesc result;
   result.init(ExpKind::NonReloc, result_reg);
   return ParserResult<ExpDesc>::success(result);
}

//********************************************************************************************************************
// Emit bytecode for a safe index expression (table?[key]), returning nil if the table is nil.

ParserResult<ExpDesc> IrEmitter::emit_safe_index_expr(const SafeIndexExprPayload &Payload)
{
   if (not Payload.table or not Payload.index) {
      return this->unsupported_expr(AstNodeKind::SafeIndexExpr, SourceSpan{});
   }

   auto table_result = this->emit_expression(*Payload.table);
   if (not table_result.ok()) return table_result;

   NilShortCircuitGuard guard(this, table_result.value_ref());
   if (not guard.ok()) return guard.error<ExpDesc>();

   // Index expression is evaluated only on non-nil path (short-circuit)
   auto key_result = this->emit_expression(*Payload.index);
   if (not key_result.ok()) return key_result;

   ExpDesc key = key_result.value_ref();
   ExpressionValue key_toval(&this->func_state, key);
   key_toval.to_val();
   key = key_toval.legacy();

   ExpDesc table = guard.base_expression();
   expr_index(&this->func_state, &table, &key);

   // Materialize the indexed result to a new register.
   // Do NOT reuse base_register() as that would clobber the table variable,
   // causing issues if the table is referenced again.
   ExpressionValue indexed_value(&this->func_state, table);
   BCReg result_reg = indexed_value.discharge_to_any_reg(guard.reg_allocator());

   // Collapse freereg to include the result register
   guard.reg_allocator().collapse_freereg(result_reg);

   // Emit the nil path
   ControlFlowEdge skip_nil = this->control_flow.make_unconditional(BCPos(bcemit_jmp(&this->func_state)));

   BCPos nil_path = BCPos(this->func_state.pc);
   guard.nil_jump_edge().patch_to(nil_path);
   bcemit_nil(&this->func_state, result_reg.raw(), 1);

   skip_nil.patch_to(BCPos(this->func_state.pc));

   ExpDesc result;
   result.init(ExpKind::NonReloc, result_reg);
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

   if (const auto* direct = std::get_if<DirectCallTarget>(&Payload.target)) {
      if (direct->callable and direct->callable->kind IS AstNodeKind::IdentifierExpr) {
         const auto *name_ref = std::get_if<NameRef>(&direct->callable->data);
         if (name_ref and name_ref->identifier.symbol) {
            GCstr *func_name = name_ref->identifier.symbol;

            static GCstr *assert_str = nullptr; // Global state - this is confirmed as thread safe.
            static GCstr *msg_str = nullptr;
            if (not assert_str) assert_str = lj_str_newlit(this->lex_state.L, "assert");
            if (not msg_str) msg_str = lj_str_newlit(this->lex_state.L, "msg");

            if (func_name IS assert_str) this->optimise_assert(Payload.arguments);
            else if ((func_name IS msg_str) and not glPrintMsg) {
               // msg() is eliminated entirely when debug messaging is disabled at compile time.
               return ParserResult<ExpDesc>::success(ExpDesc(ExpKind::Void));
            }
         }
      }
   }

   ExpDesc callee;
   auto base = BCReg(0);
   bool is_safe_callable = false;

   if (const auto *direct = std::get_if<DirectCallTarget>(&Payload.target)) {
      if (not direct->callable) return this->unsupported_expr(AstNodeKind::CallExpr, SourceSpan{});

      // Check if the callable is a safe navigation expression (?.field or ?[index])
      // If so, we need to add a nil check on the result before calling

      is_safe_callable = (direct->callable->kind IS AstNodeKind::SafeMemberExpr) or
                         (direct->callable->kind IS AstNodeKind::SafeIndexExpr);

      auto callee_result = this->emit_expression(*direct->callable);
      if (not callee_result.ok()) return callee_result;
      callee = callee_result.value_ref();
      this->materialise_to_next_reg(callee, "call callee");
      // Reserve register for frame link
      RegisterAllocator allocator(&this->func_state);
      allocator.reserve(BCReg(1));
      base = BCReg(callee.u.s.info);
   }
   else if (const auto* method = std::get_if<MethodCallTarget>(&Payload.target)) {
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
   this->func_state.freereg = base + 1;
   return ParserResult<ExpDesc>::success(result);
}

//********************************************************************************************************************
// Optimise assert(condition, message) expressions by wrapping the message in an anonymous thunk call.
// This provides short-circuiting: the message is only evaluated when the assertion fails.
//
// Transforms: assert(cond, expensive_expr)
// Into:       assert(cond, (thunk():str return expensive_expr end)())

void IrEmitter::optimise_assert(const ExprNodeList &Args)
{
   if (Args.size() < 2) return;  // No message argument

   // Simple literals and identifiers don't benefit from deferral.
   const ExprNode &msg = *Args[1];

   switch (msg.kind) {
      case AstNodeKind::LiteralExpr:    // String/number literals are cheap
      case AstNodeKind::IdentifierExpr: // Simple variable access is cheap
         return;
      case AstNodeKind::CallExpr: {
         // Check if this is already a thunk call (anonymous thunk being invoked)
         // Pattern: CallExpr with FunctionExpr callee where is_thunk=true
         const auto &call_payload = std::get<CallExprPayload>(msg.data);
         if (std::holds_alternative<DirectCallTarget>(call_payload.target)) {
            const auto &target = std::get<DirectCallTarget>(call_payload.target);
            if (target.callable and target.callable->kind IS AstNodeKind::FunctionExpr) {
               const auto &func_payload = std::get<FunctionExprPayload>(target.callable->data);
               if (func_payload.is_thunk) return;  // Already a thunk call
            }
         }
         break;
      }
      default:
         break;
   }

   // Wrap the message in an anonymous thunk call:
   //   (thunk():str return msg end)()

   ExprNodePtr &msg_arg = const_cast<ExprNodePtr&>(Args[1]);
   SourceSpan span = msg_arg->span;

   // Step 1: Build return statement with the message expression
   ExprNodeList return_values;
   return_values.push_back(std::move(msg_arg));
   StmtNodePtr return_stmt = make_return_stmt(span, std::move(return_values), false);

   // Step 2: Build thunk body containing just the return statement
   StmtNodeList body_stmts;
   body_stmts.push_back(std::move(return_stmt));
   auto body = make_block(span, std::move(body_stmts));

   // Step 3: Build anonymous thunk function (no parameters, is_thunk=true, returns string)
   ExprNodePtr thunk_func = make_function_expr(span, {}, false, std::move(body), true, FluidType::Str);

   // Step 4: Build immediate call to thunk (no arguments)
   ExprNodeList call_args;
   msg_arg = make_call_expr(span, std::move(thunk_func), std::move(call_args), false);
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
   if (call.k IS ExpKind::Call) {
      setbc_b(ir_bcptr(fs, &call), 0);
   }
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

//********************************************************************************************************************
// Emit bytecode for a table constructor expression ({key=value, [expr]=value, value}), optimising constant fields.

ParserResult<ExpDesc> IrEmitter::emit_table_expr(const TableExprPayload &Payload)
{
   FuncState* fs = &this->func_state;
   GCtab* template_table = nullptr;
   int vcall = 0;
   int needarr = 0;
   int fixt = 0;
   uint32_t narr = 0;  // 0-based array indexing
   uint32_t nhash = 0;
   auto freg = fs->free_reg();
   BCPos pc = BCPos(bcemit_AD(fs, BC_TNEW, freg, 0));
   ExpDesc table;
   table.init(ExpKind::NonReloc, freg);
   RegisterAllocator allocator(fs);
   allocator.reserve(BCReg(1));
   freg++;

   for (const TableField &field : Payload.fields) {
      if (not field.value) return this->unsupported_expr(AstNodeKind::TableExpr, field.span);
      RegisterGuard entry_guard(fs);
      ExpDesc key;
      vcall = 0;

      switch (field.kind) {
         case TableFieldKind::Computed: {
            if (not field.key) return this->unsupported_expr(AstNodeKind::TableExpr, field.span);
            auto key_result = this->emit_expression(*field.key);
            if (not key_result.ok()) return key_result;
            key = key_result.value_ref();
            ExpressionValue key_toval(fs, key);
            key_toval.to_val();
            key = key_toval.legacy();
            if (not key.is_constant()) expr_index(fs, &table, &key);
            if (key.is_num_constant() and key.is_num_zero()) needarr = 1;
            else nhash++;
            break;
         }

         case TableFieldKind::Record: {
            if (not field.name.has_value() or field.name->symbol IS nullptr) {
               return this->unsupported_expr(AstNodeKind::TableExpr, field.span);
            }
            key.init(ExpKind::Str, 0);
            key.u.sval = field.name->symbol;
            nhash++;
            break;
         }

         case TableFieldKind::Array:
         default: {
            key.init(ExpKind::Num, 0);
            setintV(&key.u.nval, int(narr));
            narr++;
            needarr = vcall = 1;
            break;
         }
      }

      auto value_result = this->emit_expression(*field.value);
      if (not value_result.ok()) return value_result;

      ExpDesc val = value_result.value_ref();

      bool emit_constant = key.is_constant() and key.k != ExpKind::Nil and (key.k IS ExpKind::Str or val.is_constant_nojump());

      if (emit_constant) {
         TValue k;
         TValue *slot;
         if (not template_table) {
            BCReg kidx;
            template_table = lj_tab_new(fs->L, needarr ? narr : 0, hsize2hbits(nhash));
            kidx = BCReg(const_gc(fs, obj2gco(template_table), LJ_TTAB));
            fs->bcbase[pc.raw()].ins = BCINS_AD(BC_TDUP, freg - BCREG(1), kidx);
         }

         vcall = 0;
         expr_kvalue(fs, &k, &key);
         slot = lj_tab_set(fs->L, template_table, &k);
         lj_gc_anybarriert(fs->L, template_table);
         if (val.is_constant_nojump()) {
            expr_kvalue(fs, slot, &val);
            continue;
         }
         settabV(fs->L, slot, template_table);
         fixt = 1;
         emit_constant = false;
      }

      if (not emit_constant) {
         if (val.k != ExpKind::Call) {
            RegisterAllocator val_allocator(fs);
            ExpressionValue val_value(fs, val);
            val_value.discharge_to_any_reg(val_allocator);
            val = val_value.legacy();
            vcall = 0;
         }
         if (key.is_constant()) expr_index(fs, &table, &key);
         bcemit_store(fs, &table, &val);
      }
   }

   if (vcall) {
      BCInsLine* ilp = &fs->last_instruction();
      ExpDesc en(ExpKind::Num);
      en.u.nval.u32.lo = narr - 1;
      en.u.nval.u32.hi = 0x43300000;
      if (narr > 256) {
         fs->pc--;
         ilp--;
      }
      ilp->ins = BCINS_AD(BC_TSETM, freg, const_num(fs, &en));
      setbc_b(&ilp[-1].ins, 0);
   }

   if (pc IS fs->pc - 1) {
      table.u.s.info = pc;
      fs->freereg--;
      table.k = ExpKind::Relocable;
   }
   else table.k = ExpKind::NonReloc;

   if (not template_table) {
      BCIns *ip = &fs->bcbase[pc].ins;
      if (not needarr) narr = 0;
      else if (narr < 3) narr = 3;
      else if (narr > 0x7ff) narr = 0x7ff;
      setbc_d(ip, narr | (hsize2hbits(nhash) << 11));
   }
   else {
      if (needarr and template_table->asize < narr) {
         lj_tab_reasize(fs->L, template_table, narr - 1);
      }

      if (fixt) {
         Node *node = noderef(template_table->node);
         uint32_t hmask = template_table->hmask;
         for (uint32_t i = 0; i <= hmask; ++i) {
            Node *n = &node[i];
            if (tvistab(&n->val)) setnilV(&n->val);
         }
      }
      lj_gc_check(fs->L);
   }

   return ParserResult<ExpDesc>::success(table);
}

//********************************************************************************************************************
// Emit bytecode for a function expression (function(...) ... end), creating a child function prototype.
// For thunk functions, transforms into a wrapper that returns thunk userdata via AST transformation.

ParserResult<ExpDesc> IrEmitter::emit_function_expr(const FunctionExprPayload &Payload)
{
   if (not Payload.body) return this->unsupported_expr(AstNodeKind::FunctionExpr, SourceSpan{});

   // Handle thunk functions via AST transformation
   if (Payload.is_thunk) {
      // Transform:
      //   thunk compute(x, y):num
      //      return x * y
      //   end
      //
      // Into:
      //   function compute(x, y)
      //      return __create_thunk(function() return x * y end, type_tag)
      //   end

      SourceSpan span = Payload.body->span;

      // Step 1: Create inner closure (no parameters, captures parent's as upvalues)
      // Move original body to inner function
      auto inner_body = std::make_unique<BlockStmt>();
      inner_body->span = span;
      for (auto& stmt : Payload.body->statements) {
         inner_body->statements.push_back(std::move(const_cast<StmtNodePtr&>(stmt)));
      }

      ExprNodePtr inner_fn = make_function_expr(span, {}, false, std::move(inner_body), false, FluidType::Any);

      // Step 2: Create call to __create_thunk(inner_fn, type_tag)
      NameRef create_thunk_ref;
      create_thunk_ref.identifier.symbol = lj_str_newlit(this->lex_state.L, "__create_thunk");
      create_thunk_ref.identifier.span = span;
      create_thunk_ref.resolution = NameResolution::Unresolved;
      ExprNodePtr create_thunk_fn = make_identifier_expr(span, create_thunk_ref);

      // Type tag argument
      LiteralValue type_literal;
      type_literal.kind = LiteralKind::Number;
      type_literal.number_value = double(fluid_type_to_lj_tag(Payload.thunk_return_type));
      ExprNodePtr type_arg = make_literal_expr(span, type_literal);

      // Build argument list
      ExprNodeList call_args;
      call_args.push_back(std::move(inner_fn));
      call_args.push_back(std::move(type_arg));

      // Create call expression
      ExprNodePtr thunk_call = make_call_expr(span, std::move(create_thunk_fn), std::move(call_args), false);

      // Step 3: Create return statement
      ExprNodeList return_values;
      return_values.push_back(std::move(thunk_call));
      StmtNodePtr return_stmt = make_return_stmt(span, std::move(return_values), false);

      // Step 4: Create wrapper body with just the return statement
      auto wrapper_body = std::make_unique<BlockStmt>();
      wrapper_body->span = span;
      wrapper_body->statements.push_back(std::move(return_stmt));

      // Step 5: Create wrapper function payload (same parameters, not a thunk)
      FunctionExprPayload wrapper_payload;
      wrapper_payload.parameters = Payload.parameters;  // Copy parameters
      wrapper_payload.is_vararg = Payload.is_vararg;
      wrapper_payload.is_thunk = false;  // Important: wrapper is not a thunk
      wrapper_payload.body = std::move(wrapper_body);

      // Recursively emit the wrapper function (which is now a regular function)
      return this->emit_function_expr(wrapper_payload);
   }

   // Regular function emission
   FuncState child_state;
   ParserAllocator allocator = ParserAllocator::from(this->lex_state.L);
   ParserConfig inherited = this->ctx.config();
   ParserContext child_ctx = ParserContext::from(this->lex_state, child_state, allocator, inherited);
   ParserSession session(child_ctx, inherited);

   FuncState *parent_state = &this->func_state;
   ptrdiff_t oldbase = parent_state->bcbase - this->lex_state.bcstack;

   this->lex_state.fs_init(&child_state);
   FuncStateGuard fs_guard(&this->lex_state, &child_state);  // Restore ls->fs on error
   // Use lastline which was set to the function expression's line by emit_expression()
   child_state.linedefined = this->lex_state.lastline;
   child_state.bcbase = parent_state->bcbase + parent_state->pc;
   child_state.bclim = parent_state->bclim - parent_state->pc;
   bcemit_AD(&child_state, BC_FUNCF, 0, 0);
   if (Payload.is_vararg) child_state.flags |= PROTO_VARARG;

   FuncScope scope;
   ScopeGuard scope_guard(&child_state, &scope, FuncScopeFlag::None);

   auto param_count = BCReg(BCREG(Payload.parameters.size()));
   for (auto i = BCReg(0); i < param_count; ++i) {
      const FunctionParameter& param = Payload.parameters[i.raw()];
      GCstr *symbol = (param.name.symbol and not param.name.is_blank) ? param.name.symbol : NAME_BLANK;
      this->lex_state.var_new(i, symbol);
   }

   child_state.numparams = uint8_t(param_count.raw());
   this->lex_state.var_add(param_count);
   if (child_state.nactvar > 0) {
      RegisterAllocator child_allocator(&child_state);
      child_allocator.reserve(BCReg(child_state.nactvar));
   }

   IrEmitter child_emitter(child_ctx);
   auto base = BCReg(child_state.nactvar - param_count.raw());
   for (auto i = BCReg(0); i < param_count; ++i) {
      const FunctionParameter& param = Payload.parameters[i.raw()];
      if (param.name.is_blank or param.name.symbol IS nullptr) continue;
      child_emitter.update_local_binding(param.name.symbol, BCReg(base.raw() + i.raw()));
   }

   auto body_result = child_emitter.emit_block(*Payload.body, FuncScopeFlag::None);
   if (not body_result.ok()) {
      return ParserResult<ExpDesc>::failure(body_result.error_ref());
   }

   fs_guard.disarm();  // fs_finish will handle cleanup
   GCproto *pt = this->lex_state.fs_finish(Payload.body->span.line);
   scope_guard.disarm();
   parent_state->bcbase = this->lex_state.bcstack + oldbase;
   parent_state->bclim = BCPos(this->lex_state.sizebcstack - oldbase).raw();

   ExpDesc expr;
   expr.init(ExpKind::Relocable, bcemit_AD(parent_state, BC_FNEW, 0, const_gc(parent_state, obj2gco(pt), LJ_TPROTO)));

#if LJ_HASFFI
   parent_state->flags |= (child_state.flags & PROTO_FFI);
#endif

   if (not (parent_state->flags & PROTO_CHILD)) {
      if (parent_state->flags & PROTO_HAS_RETURN) parent_state->flags |= PROTO_FIXUP_RETURN;
      parent_state->flags |= PROTO_CHILD;
   }

   return ParserResult<ExpDesc>::success(expr);
}

//********************************************************************************************************************
// Emit bytecode for a function declaration path (module.submodule.name or module:method), resolving the lvalue target.

ParserResult<ExpDesc> IrEmitter::emit_function_lvalue(const FunctionNamePath &path)
{
   if (path.segments.empty()) return this->unsupported_expr(AstNodeKind::FunctionExpr, SourceSpan{});

   NameRef base_ref = make_name_ref(path.segments.front());
   auto base_expr = this->emit_identifier_expr(base_ref);
   if (not base_expr.ok()) return base_expr;

   ExpDesc target = base_expr.value_ref();

   size_t traverse_limit = path.method.has_value() ? path.segments.size() : (path.segments.size() > 0 ? path.segments.size() - 1 : 0);
   for (size_t i = 1; i < traverse_limit; ++i) {
      const Identifier &segment = path.segments[i];
      if (not segment.symbol) return this->unsupported_expr(AstNodeKind::FunctionExpr, SourceSpan{});

      ExpDesc key(segment.symbol);
      ExpressionValue target_toval(&this->func_state, target);
      target_toval.to_val();
      target = target_toval.legacy();
      RegisterAllocator allocator(&this->func_state);
      ExpressionValue target_value(&this->func_state, target);
      target_value.discharge_to_any_reg(allocator);
      target = target_value.legacy();
      expr_index(&this->func_state, &target, &key);
   }

   const Identifier *final_name = nullptr;
   if (path.method.has_value()) final_name = &path.method.value();
   else if (path.segments.size() > 1) final_name = &path.segments.back();

   if (not final_name) return ParserResult<ExpDesc>::success(target);

   if (not final_name->symbol) return this->unsupported_expr(AstNodeKind::FunctionExpr, SourceSpan{});

   ExpDesc key(final_name->symbol);
   ExpressionValue target_toval_final(&this->func_state, target);
   target_toval_final.to_val();
   target = target_toval_final.legacy();

   RegisterAllocator allocator(&this->func_state);
   ExpressionValue target_value(&this->func_state, target);
   target_value.discharge_to_any_reg(allocator);
   target = target_value.legacy();
   expr_index(&this->func_state, &target, &key);
   return ParserResult<ExpDesc>::success(target);
}

//********************************************************************************************************************
// Emit bytecode for an lvalue expression (assignable location like identifier, member, or index).

ParserResult<ExpDesc> IrEmitter::emit_lvalue_expr(const ExprNode &Expr)
{
   switch (Expr.kind) {
      case AstNodeKind::IdentifierExpr: {
         auto result = this->emit_identifier_expr(std::get<NameRef>(Expr.data));
         if (not result.ok()) return result;
         ExpDesc value = result.value_ref();
         if (value.k IS ExpKind::Local) value.u.s.aux = this->func_state.varmap[value.u.s.info];
         if (not vkisvar(value.k)) return this->unsupported_expr(Expr.kind, Expr.span);
         return ParserResult<ExpDesc>::success(value);
      }

      case AstNodeKind::MemberExpr: {
         const auto &payload = std::get<MemberExprPayload>(Expr.data);
         if (not payload.table or not payload.member.symbol) return this->unsupported_expr(Expr.kind, Expr.span);

         auto table_result = this->emit_expression(*payload.table);
         if (not table_result.ok()) return table_result;
         ExpDesc table = table_result.value_ref();
         ExpressionValue table_toval(&this->func_state, table);
         table_toval.to_val();
         table = table_toval.legacy();
         RegisterAllocator allocator(&this->func_state);
         ExpressionValue table_value(&this->func_state, table);
         table_value.discharge_to_any_reg(allocator);
         table = table_value.legacy();
         ExpDesc key(ExpKind::Str);
         key.u.sval = payload.member.symbol;
         expr_index(&this->func_state, &table, &key);
         return ParserResult<ExpDesc>::success(table);
      }

      case AstNodeKind::IndexExpr: {
         const auto &payload = std::get<IndexExprPayload>(Expr.data);
         if (not payload.table or not payload.index) return this->unsupported_expr(Expr.kind, Expr.span);

         auto table_result = this->emit_expression(*payload.table);
         if (not table_result.ok()) return table_result;

         ExpDesc table = table_result.value_ref();

         // Materialize table BEFORE evaluating key, so nested index expressions emit bytecode in
         // the correct order (table first, then key)

         ExpressionValue table_toval_idx(&this->func_state, table);
         table_toval_idx.to_val();
         table = table_toval_idx.legacy();
         RegisterAllocator allocator(&this->func_state);
         ExpressionValue table_value(&this->func_state, table);
         table_value.discharge_to_any_reg(allocator);
         table = table_value.legacy();
         auto key_result = this->emit_expression(*payload.index);
         if (not key_result.ok()) return key_result;

         ExpDesc key = key_result.value_ref();
         ExpressionValue key_toval_idx(&this->func_state, key);
         key_toval_idx.to_val();
         key = key_toval_idx.legacy();
         expr_index(&this->func_state, &table, &key);
         return ParserResult<ExpDesc>::success(table);
      }

      case AstNodeKind::SafeMemberExpr:
      case AstNodeKind::SafeIndexExpr:
         return ParserResult<ExpDesc>::failure(this->make_error(ParserErrorCode::InternalInvariant,
            "Safe navigation operators (?. and ?[]) cannot be used as assignment targets"));

      default:
         return this->unsupported_expr(Expr.kind, Expr.span);
   }
}

//********************************************************************************************************************
// Emit bytecode for a list of expressions, materialising intermediate results and returning the last expression.

ParserResult<ExpDesc> IrEmitter::emit_expression_list(const ExprNodeList &expressions, BCReg &count)
{
   count = BCReg(0);
   if (expressions.empty()) return ParserResult<ExpDesc>::success(ExpDesc(ExpKind::Void));

   ExpDesc last(ExpKind::Void);
   bool first = true;
   for (const ExprNodePtr &node : expressions) {
      if (not node) return this->unsupported_expr(AstNodeKind::ExpressionStmt, SourceSpan{});
      if (not first) this->materialise_to_next_reg(last, "expression list baton");
      auto value = this->emit_expression(*node);
      if (not value.ok()) return value;
      ExpDesc expr = value.value_ref();
      ++count;
      last = expr;
      first = false;
   }

   return ParserResult<ExpDesc>::success(last);
}

//********************************************************************************************************************
// Prepare assignment targets by resolving lvalues and duplicating table operands to prevent register clobbering.

ParserResult<std::vector<PreparedAssignment>> IrEmitter::prepare_assignment_targets(const ExprNodeList &Targets)
{
   std::vector<PreparedAssignment> lhs;
   lhs.reserve(Targets.size());
   RegisterAllocator allocator(&this->func_state);

   auto prv = (prvFluid *)this->func_state.ls->L->Script->ChildPrivate;
   bool trace_assignments = (prv->JitOptions & JOF::TRACE_ASSIGNMENTS) != JOF::NIL;

   for (const ExprNodePtr &node : Targets) {
      if (not node) {
         return ParserResult<std::vector<PreparedAssignment>>::failure(this->make_error(
            ParserErrorCode::InternalInvariant, "assignment target missing"));
      }

      auto lvalue = this->emit_lvalue_expr(*node);
      if (not lvalue.ok()) return ParserResult<std::vector<PreparedAssignment>>::failure(lvalue.error_ref());

      ExpDesc slot = lvalue.value_ref();
      PreparedAssignment prepared;
      TableOperandCopies copies = allocator.duplicate_table_operands(slot);
      prepared.storage = copies.duplicated;
      prepared.reserved = std::move(copies.reserved);
      prepared.target = LValue::from_expdesc(&prepared.storage);

      if (trace_assignments and prepared.reserved.count().raw() > 0) {
         auto target_kind = prepared.target.is_indexed() ? "indexed" : "member";
         pf::Log("Parser").msg("[%d] assignment: prepared %s target, duplicated %d registers (R%d..R%d)",
            this->func_state.ls->linenumber, target_kind,
            unsigned(prepared.reserved.count().raw()), unsigned(prepared.reserved.start().raw()),
            unsigned(prepared.reserved.start().raw() + prepared.reserved.count().raw() - 1));
      }

      if (prepared.target.is_local()) {
         for (PreparedAssignment& existing : lhs) {
            bool refresh_table = existing.target.is_indexed()
               and existing.target.get_table_reg() IS prepared.target.get_local_reg();
            bool refresh_key = existing.target.is_indexed() and is_register_key(existing.storage.u.s.aux)
               and existing.target.get_key_reg() IS prepared.target.get_local_reg();
            bool refresh_member = existing.target.is_member()
               and existing.target.get_table_reg() IS prepared.target.get_local_reg();

            if (refresh_table or refresh_key or refresh_member) {
               TableOperandCopies refreshed = allocator.duplicate_table_operands(existing.storage);
               existing.storage = refreshed.duplicated;
               existing.reserved = std::move(refreshed.reserved);
               existing.target = LValue::from_expdesc(&existing.storage);
            }
         }
      }

      lhs.push_back(std::move(prepared));
   }

   return ParserResult<std::vector<PreparedAssignment>>::success(std::move(lhs));
}

//********************************************************************************************************************
// Materialise an expression to the next available register, ensuring it's stored in a concrete location.

void IrEmitter::materialise_to_next_reg(ExpDesc& expression, std::string_view usage)
{
   RegisterAllocator allocator(&this->func_state);
   ExpressionValue value(&this->func_state, expression);
   value.to_next_reg(allocator);
   expression = value.legacy();
   this->ensure_register_floor(usage);
}

// Materialise an expression to a specific register slot.

void IrEmitter::materialise_to_reg(ExpDesc& expression, BCReg slot, std::string_view usage)
{
   RegisterAllocator allocator(&this->func_state);
   ExpressionValue value(&this->func_state, expression);
   value.to_reg(allocator, slot);
   expression = value.legacy();
   this->ensure_register_floor(usage);
}

// Ensure the free register pointer hasn't fallen below the active variable count (register underrun check).

void IrEmitter::ensure_register_floor(std::string_view usage)
{
   if (this->func_state.freereg < this->func_state.nactvar) {
      pf::Log log("Parser");
      log.warning("Register underrun during %.*s (free=%u active=%u)",
         int(usage.size()), usage.data(), unsigned(this->func_state.freereg), unsigned(this->func_state.nactvar));
      this->func_state.reset_freereg();
   }
}

// Ensure registers are balanced (no leaks or underruns) after completing an operation.

void IrEmitter::ensure_register_balance(std::string_view usage)
{
   this->ensure_register_floor(usage);
   if (this->func_state.freereg > this->func_state.nactvar) {
      pf::Log log("Parser");
      int line = this->lex_state.lastline;
      log.warning("Leaked %u registers after %.*s at line %d (free=%u active=%u)",
         unsigned(this->func_state.freereg - this->func_state.nactvar), int(usage.size()), usage.data(),
         line + 1, unsigned(this->func_state.freereg), unsigned(this->func_state.nactvar));
      this->func_state.reset_freereg();
   }
}

// Report an unsupported statement node and return an internal invariant error.

ParserResult<IrEmitUnit> IrEmitter::unsupported_stmt(AstNodeKind kind, const SourceSpan& span)
{
   glUnsupportedNodes.record(kind, span, "stmt");
   std::string message = "IR emitter does not yet support statement kind " + std::to_string(int(kind));
   return ParserResult<IrEmitUnit>::failure(this->make_error(ParserErrorCode::InternalInvariant, message));
}

// Report an unsupported expression node and return an internal invariant error.

ParserResult<ExpDesc> IrEmitter::unsupported_expr(AstNodeKind kind, const SourceSpan& span)
{
   glUnsupportedNodes.record(kind, span, "expr");
   std::string message = "IR emitter does not yet support expression kind " + std::to_string(int(kind));
   return ParserResult<ExpDesc>::failure(this->make_error(ParserErrorCode::InternalInvariant, message));
}

// Create a parser error with the specified error code and message, capturing the current token context.

ParserError IrEmitter::make_error(ParserErrorCode code, std::string_view message) const
{
   ParserError error;
   error.code = code;
   error.message.assign(message.begin(), message.end());
   error.token = Token::from_current(this->lex_state);
   return error;
}
