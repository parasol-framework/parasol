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

//********************************************************************************************************************
// TODO: Move small LocalBindingTable functions to the header for inlining.

LocalBindingTable::LocalBindingTable() = default;

void LocalBindingTable::push_scope()
{
   this->scope_marks.push_back(this->bindings.size());
   this->depth++;
}

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

void LocalBindingTable::add(GCstr* symbol, BCReg slot)
{
   if (not symbol) return;
   LocalBindingEntry entry;
   entry.symbol = symbol;
   entry.slot = slot;
   entry.depth = this->depth;
   this->bindings.push_back(entry);
}

std::optional<BCReg> LocalBindingTable::resolve(GCstr* symbol) const
{
   if (not symbol) return std::nullopt;
   for (auto it = this->bindings.rbegin(); it != this->bindings.rend(); ++it) {
      if (it->symbol IS symbol) return it->slot;
   }
   return std::nullopt;
}

LocalBindingScope::LocalBindingScope(LocalBindingTable& table) : table(table)
{
   this->table.push_scope();
}

LocalBindingScope::~LocalBindingScope()
{
   this->table.pop_scope();
}

// IR emission context implementation

IrEmissionContext::IrEmissionContext(FuncState* State)
   : func_state(State),
     register_allocator(State),
     control_flow_graph(State),
     operator_emitter(State, &this->register_allocator, &this->control_flow_graph)
{
}

RegisterAllocator& IrEmissionContext::allocator()
{
   return this->register_allocator;
}

ControlFlowGraph& IrEmissionContext::cfg()
{
   return this->control_flow_graph;
}

OperatorEmitter& IrEmissionContext::operators()
{
   return this->operator_emitter;
}

FuncState* IrEmissionContext::state() const
{
   return this->func_state;
}

namespace {

constexpr size_t kAstNodeKindCount = size_t(AstNodeKind::ExpressionStmt) + 1;
constexpr int kJumpBreak = 0;
constexpr int kJumpContinue = 1;

class UnsupportedNodeRecorder {
public:
   void record(AstNodeKind kind, const SourceSpan& span, const char* stage) {
      size_t index = size_t(kind);
      if (index >= this->counts.size()) return;
      uint32_t total = ++this->counts[index];
      if ((total <= 8) or (total % 32 IS 0)) {
         pf::Log log("Parser");
         log.msg("ast-pipeline unsupported %s node kind=%u hits=%u line=%d column=%d offset=%lld",
            stage, unsigned(kind), unsigned(total), int(span.line), int(span.column),
            static_cast<long long>(span.offset));
      }
   }

private:
   std::array<uint32_t, kAstNodeKindCount> counts{};
};

UnsupportedNodeRecorder glUnsupportedNodes;

//********************************************************************************************************************

[[nodiscard]] static bool is_blank_symbol(const Identifier& identifier)
{
   return identifier.is_blank or identifier.symbol IS nullptr;
}

//********************************************************************************************************************

[[nodiscard]] static std::optional<BinOpr> map_binary_operator(AstBinaryOperator op)
{
   switch (op) {
      case AstBinaryOperator::Add: return BinOpr::OPR_ADD;
      case AstBinaryOperator::Subtract: return BinOpr::OPR_SUB;
      case AstBinaryOperator::Multiply: return BinOpr::OPR_MUL;
      case AstBinaryOperator::Divide: return BinOpr::OPR_DIV;
      case AstBinaryOperator::Modulo: return BinOpr::OPR_MOD;
      case AstBinaryOperator::Power: return BinOpr::OPR_POW;
      case AstBinaryOperator::Concat: return BinOpr::OPR_CONCAT;
      case AstBinaryOperator::NotEqual: return BinOpr::OPR_NE;
      case AstBinaryOperator::Equal: return BinOpr::OPR_EQ;
      case AstBinaryOperator::LessThan: return BinOpr::OPR_LT;
      case AstBinaryOperator::GreaterEqual: return BinOpr::OPR_GE;
      case AstBinaryOperator::LessEqual: return BinOpr::OPR_LE;
      case AstBinaryOperator::GreaterThan: return BinOpr::OPR_GT;
      case AstBinaryOperator::BitAnd: return BinOpr::OPR_BAND;
      case AstBinaryOperator::BitOr: return BinOpr::OPR_BOR;
      case AstBinaryOperator::BitXor: return BinOpr::OPR_BXOR;
      case AstBinaryOperator::ShiftLeft: return BinOpr::OPR_SHL;
      case AstBinaryOperator::ShiftRight: return BinOpr::OPR_SHR;
      case AstBinaryOperator::LogicalAnd: return BinOpr::OPR_AND;
      case AstBinaryOperator::LogicalOr: return BinOpr::OPR_OR;
      case AstBinaryOperator::IfEmpty: return BinOpr::OPR_IF_EMPTY;
      default: return std::nullopt;
   }
}

//********************************************************************************************************************

[[nodiscard]] static std::optional<BinOpr> map_assignment_operator(AssignmentOperator op)
{
   switch (op) {
      case AssignmentOperator::Add: return BinOpr::OPR_ADD;
      case AssignmentOperator::Subtract: return BinOpr::OPR_SUB;
      case AssignmentOperator::Multiply: return BinOpr::OPR_MUL;
      case AssignmentOperator::Divide: return BinOpr::OPR_DIV;
      case AssignmentOperator::Modulo: return BinOpr::OPR_MOD;
      case AssignmentOperator::Concat: return BinOpr::OPR_CONCAT;
      default: return std::nullopt;
   }
}

//********************************************************************************************************************

[[nodiscard]] static bool is_register_key(uint32_t aux)
{
   return (int32_t(aux) >= 0) and (aux <= BCMAX_C);
}

//********************************************************************************************************************

[[nodiscard]] static std::string_view describe_node_kind(AstNodeKind kind)
{
   switch (kind) {
      case AstNodeKind::LiteralExpr: return "LiteralExpr";
      case AstNodeKind::IdentifierExpr: return "IdentifierExpr";
      case AstNodeKind::VarArgExpr: return "VarArgExpr";
      case AstNodeKind::UnaryExpr: return "UnaryExpr";
      case AstNodeKind::BinaryExpr: return "BinaryExpr";
      case AstNodeKind::UpdateExpr: return "UpdateExpr";
      case AstNodeKind::TernaryExpr: return "TernaryExpr";
      case AstNodeKind::PresenceExpr: return "PresenceExpr";
      case AstNodeKind::CallExpr: return "CallExpr";
      case AstNodeKind::MemberExpr: return "MemberExpr";
      case AstNodeKind::IndexExpr: return "IndexExpr";
      case AstNodeKind::TableExpr: return "TableExpr";
      case AstNodeKind::FunctionExpr: return "FunctionExpr";
      case AstNodeKind::BlockStmt: return "BlockStmt";
      case AstNodeKind::AssignmentStmt: return "AssignmentStmt";
      case AstNodeKind::LocalDeclStmt: return "LocalDeclStmt";
      case AstNodeKind::LocalFunctionStmt: return "LocalFunctionStmt";
      case AstNodeKind::FunctionStmt: return "FunctionStmt";
      case AstNodeKind::IfStmt: return "IfStmt";
      case AstNodeKind::WhileStmt: return "WhileStmt";
      case AstNodeKind::RepeatStmt: return "RepeatStmt";
      case AstNodeKind::NumericForStmt: return "NumericForStmt";
      case AstNodeKind::GenericForStmt: return "GenericForStmt";
      case AstNodeKind::BreakStmt: return "BreakStmt";
      case AstNodeKind::ContinueStmt: return "ContinueStmt";
      case AstNodeKind::ReturnStmt: return "ReturnStmt";
      case AstNodeKind::DeferStmt: return "DeferStmt";
      case AstNodeKind::DoStmt: return "DoStmt";
      case AstNodeKind::ExpressionStmt: return "ExpressionStmt";
      default: return "Unknown";
   }
}

//********************************************************************************************************************

[[nodiscard]] static NameRef make_name_ref(const Identifier& identifier)
{
   NameRef ref;
   ref.identifier = identifier;
   ref.resolution = NameResolution::Unresolved;
   ref.slot = 0;
   return ref;
}

//********************************************************************************************************************

[[nodiscard]] static int predict_next(LexState& lex_state, FuncState& func_state, BCPos pc)
{
   BCIns ins = func_state.bcbase[pc].ins;
   GCstr *name = nullptr;
   cTValue *table_entry = nullptr;

   switch (bc_op(ins)) {
      case BC_MOV:
         name = gco2str(gcref(var_get(&lex_state, &func_state, bc_d(ins)).name));
         break;
      case BC_UGET:
         name = gco2str(gcref(lex_state.vstack[func_state.uvmap[bc_d(ins)]].name));
         break;
      case BC_GGET:
         table_entry = lj_tab_getstr(func_state.kt, lj_str_newlit(lex_state.L, "pairs"));
         if (table_entry and tvhaskslot(table_entry) and tvkslot(table_entry) IS bc_d(ins)) {
            return 1;
         }
         table_entry = lj_tab_getstr(func_state.kt, lj_str_newlit(lex_state.L, "next"));
         if (table_entry and tvhaskslot(table_entry) and tvkslot(table_entry) IS bc_d(ins)) {
            return 1;
         }
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
// TODO: This function is now deprecated - use the allocator method instead

static void ir_collapse_freereg(FuncState* func_state, BCReg result_reg)
{
   BCReg target = result_reg + 1;
   if (target < func_state->nactvar) target = BCReg(func_state->nactvar);
   if (func_state->freereg > target) func_state->freereg = target;
}

//********************************************************************************************************************

static void release_indexed_original(FuncState& func_state, const ExpDesc& original)
{
   if (original.k IS ExpKind::Indexed) {
      RegisterAllocator allocator(&func_state);
      uint32_t orig_aux = original.u.s.aux;
      if (is_register_key(orig_aux)) allocator.release_register(BCReg(orig_aux));
      allocator.release_register(BCReg(original.u.s.info));
   }
}

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

//********************************************************************************************************************

ParserResult<IrEmitUnit> IrEmitter::emit_chunk(const BlockStmt& chunk)
{
   this->control_flow.reset(&this->func_state);
   FuncScope chunk_scope;
   ScopeGuard guard(&this->func_state, &chunk_scope, FuncScopeFlag::None);
   auto result = this->emit_block(chunk, FuncScopeFlag::None);
   if (not result.ok()) return result;
   this->control_flow.finalize();

   if (GetResource(RES::LOG_LEVEL) >= 5) {
      // Verify no register leaks at function exit
      RegisterAllocator verifier(&this->func_state);
      verifier.verify_no_leaks("function exit");
   }

   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

ParserResult<IrEmitUnit> IrEmitter::emit_block(const BlockStmt& block, FuncScopeFlag flags)
{
   return this->emit_block_with_bindings(block, flags, std::span<const BlockBinding>());
}

//********************************************************************************************************************

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

ParserResult<IrEmitUnit> IrEmitter::emit_statement(const StmtNode& stmt)
{
   // Update lexer's last line so bytecode emission uses correct line numbers
   this->lex_state.lastline = stmt.span.line;

   switch (stmt.kind) {
   case AstNodeKind::ExpressionStmt: {
      const auto& payload = std::get<ExpressionStmtPayload>(stmt.data);
      return this->emit_expression_stmt(payload);
   }
   case AstNodeKind::ReturnStmt: {
      const auto& payload = std::get<ReturnStmtPayload>(stmt.data);
      return this->emit_return_stmt(payload);
   }
   case AstNodeKind::LocalDeclStmt: {
      const auto& payload = std::get<LocalDeclStmtPayload>(stmt.data);
      return this->emit_local_decl_stmt(payload);
   }
   case AstNodeKind::LocalFunctionStmt: {
      const auto& payload = std::get<LocalFunctionStmtPayload>(stmt.data);
      return this->emit_local_function_stmt(payload);
   }
   case AstNodeKind::FunctionStmt: {
      const auto& payload = std::get<FunctionStmtPayload>(stmt.data);
      return this->emit_function_stmt(payload);
   }
   case AstNodeKind::AssignmentStmt: {
      const auto& payload = std::get<AssignmentStmtPayload>(stmt.data);
      return this->emit_assignment_stmt(payload);
   }
   case AstNodeKind::IfStmt: {
      const auto& payload = std::get<IfStmtPayload>(stmt.data);
      return this->emit_if_stmt(payload);
   }
   case AstNodeKind::WhileStmt: {
      const auto& payload = std::get<LoopStmtPayload>(stmt.data);
      return this->emit_while_stmt(payload);
   }
   case AstNodeKind::RepeatStmt: {
      const auto& payload = std::get<LoopStmtPayload>(stmt.data);
      return this->emit_repeat_stmt(payload);
   }
   case AstNodeKind::NumericForStmt: {
      const auto& payload = std::get<NumericForStmtPayload>(stmt.data);
      return this->emit_numeric_for_stmt(payload);
   }
   case AstNodeKind::GenericForStmt: {
      const auto& payload = std::get<GenericForStmtPayload>(stmt.data);
      return this->emit_generic_for_stmt(payload);
   }
   case AstNodeKind::DeferStmt: {
      const auto& payload = std::get<DeferStmtPayload>(stmt.data);
      return this->emit_defer_stmt(payload);
   }
   case AstNodeKind::BreakStmt: {
      const auto& payload = std::get<BreakStmtPayload>(stmt.data);
      return this->emit_break_stmt(payload);
   }
   case AstNodeKind::ContinueStmt: {
      const auto& payload = std::get<ContinueStmtPayload>(stmt.data);
      return this->emit_continue_stmt(payload);
   }
   case AstNodeKind::DoStmt: {
      const auto& payload = std::get<DoStmtPayload>(stmt.data);
      if (payload.block) return this->emit_block(*payload.block, FuncScopeFlag::None);
      return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
   }
   default:
      return this->unsupported_stmt(stmt.kind, stmt.span);
   }
}

//********************************************************************************************************************

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
   this->func_state.freereg = this->func_state.nactvar;
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************

ParserResult<IrEmitUnit> IrEmitter::emit_return_stmt(const ReturnStmtPayload &payload)
{
   BCIns ins;
   this->func_state.flags |= PROTO_HAS_RETURN;
   if (payload.values.empty()) {
      ins = BCINS_AD(BC_RET0, 0, 1);
   }
   else {
      BCReg count = 0;
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
            BCReg reg = value.discharge_to_any_reg(allocator);
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
   execute_defers(&this->func_state, 0);
   if (this->func_state.flags & PROTO_CHILD) bcemit_AJ(&this->func_state, BC_UCLO, 0, 0);
   bcemit_INS(&this->func_state, ins);
   this->func_state.freereg = this->func_state.nactvar;
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************

ParserResult<IrEmitUnit> IrEmitter::emit_local_decl_stmt(const LocalDeclStmtPayload& payload)
{
   BCReg nvars = BCReg(payload.names.size());
   if (nvars IS 0) return ParserResult<IrEmitUnit>::success(IrEmitUnit{});

   for (BCReg i = 0; i < nvars; ++i) {
      const Identifier& identifier = payload.names[i];
      GCstr* symbol = identifier.symbol;
      this->lex_state.var_new(i, is_blank_symbol(identifier) ? NAME_BLANK : symbol);
   }

   ExpDesc tail;
   BCReg nexps = 0;
   if (payload.values.empty()) tail = make_const_expr(ExpKind::Void);
   else {
      auto list = this->emit_expression_list(payload.values, nexps);
      if (not list.ok()) return ParserResult<IrEmitUnit>::failure(list.error_ref());
      tail = list.value_ref();
   }

   this->lex_state.assign_adjust(nvars, nexps, &tail);
   this->lex_state.var_add(nvars);
   BCReg base = this->func_state.nactvar - nvars;
   for (BCReg i = 0; i < nvars; ++i) {
      const Identifier& identifier = payload.names[i];
      if (is_blank_symbol(identifier)) continue;
      this->update_local_binding(identifier.symbol, base + i);
   }
   this->func_state.freereg = this->func_state.nactvar;
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************

ParserResult<IrEmitUnit> IrEmitter::emit_local_function_stmt(const LocalFunctionStmtPayload& payload)
{
   if (not payload.function) return this->unsupported_stmt(AstNodeKind::LocalFunctionStmt, SourceSpan{});

   GCstr* symbol = payload.name.symbol ? payload.name.symbol : NAME_BLANK;
   BCReg slot = this->func_state.freereg;
   this->lex_state.var_new(0, symbol);
   ExpDesc variable;
   expr_init(&variable, ExpKind::Local, slot);
   variable.u.s.aux = this->func_state.varmap[slot];
   RegisterAllocator allocator(&this->func_state);
   allocator.reserve(1);
   this->lex_state.var_add(1);

   auto function_value = this->emit_function_expr(*payload.function);
   if (not function_value.ok()) {
      return ParserResult<IrEmitUnit>::failure(function_value.error_ref());
   }
   ExpDesc fn = function_value.value_ref();
   this->materialise_to_reg(fn, slot, "local function literal");
   var_get(&this->lex_state, &this->func_state, this->func_state.nactvar - 1).startpc = this->func_state.pc;
   if (payload.name.symbol and not payload.name.is_blank) {
      this->update_local_binding(payload.name.symbol, slot);
   }
   this->func_state.freereg = this->func_state.nactvar;
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

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
   this->func_state.freereg = this->func_state.nactvar;
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

ParserResult<IrEmitUnit> IrEmitter::emit_if_stmt(const IfStmtPayload& payload)
{
   if (payload.clauses.empty()) return ParserResult<IrEmitUnit>::success(IrEmitUnit{});

   ControlFlowEdge escapelist = this->control_flow.make_unconditional();
   for (size_t i = 0; i < payload.clauses.size(); ++i) {
      const IfClause& clause = payload.clauses[i];
      bool has_next = (i + 1) < payload.clauses.size();
      if (clause.condition) {
         auto condexit_result = this->emit_condition_jump(*clause.condition);
         if (not condexit_result.ok()) {
            return ParserResult<IrEmitUnit>::failure(condexit_result.error_ref());
         }
         ControlFlowEdge condexit = condexit_result.value_ref();
         if (clause.block) {
            auto block_result = this->emit_block(*clause.block, FuncScopeFlag::None);
            if (not block_result.ok()) {
               return block_result;
            }
         }
         if (has_next) {
            escapelist.append(bcemit_jmp(&this->func_state));
            condexit.patch_here();
         }
         else {
            escapelist.append(condexit);
         }
      }
      else if (clause.block) {
         auto block_result = this->emit_block(*clause.block, FuncScopeFlag::None);
         if (not block_result.ok()) {
            return block_result;
         }
      }
   }

   escapelist.patch_here();
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

ParserResult<IrEmitUnit> IrEmitter::emit_while_stmt(const LoopStmtPayload& payload)
{
   if (payload.style != LoopStyle::WhileLoop or not payload.condition or not payload.body) {
      return this->unsupported_stmt(AstNodeKind::WhileStmt, SourceSpan{});
   }

   FuncState* fs = &this->func_state;
   BCPos start = fs->lasttarget = fs->pc;
   LoopContext loop_context{};
   loop_context.break_edge = this->control_flow.make_break_edge();
   loop_context.continue_edge = this->control_flow.make_continue_edge();
   loop_context.defer_base = fs->nactvar;
   loop_context.continue_target = start;
   this->loop_stack.push_back(loop_context);
   LoopStackGuard loop_stack_guard(this);
   auto condexit_result = this->emit_condition_jump(*payload.condition);
   if (not condexit_result.ok()) {
      return ParserResult<IrEmitUnit>::failure(condexit_result.error_ref());
   }
   ControlFlowEdge condexit = condexit_result.value_ref();

   ControlFlowEdge loop;
   {
      FuncScope loop_scope;
      ScopeGuard guard(fs, &loop_scope, FuncScopeFlag::Loop);
      loop = this->control_flow.make_unconditional(bcemit_AD(fs, BC_LOOP, fs->nactvar, 0));
      auto block_result = this->emit_block(*payload.body, FuncScopeFlag::None);
      if (not block_result.ok()) {
         return block_result;
      }
      ControlFlowEdge body_jump = this->control_flow.make_unconditional(bcemit_jmp(fs));
      body_jump.patch_to(start);
   }

   condexit.patch_here();
   loop.patch_head(fs->pc);

   this->loop_stack.back().continue_edge.patch_to(loop_context.continue_target);
   this->loop_stack.back().break_edge.patch_here();
   loop_stack_guard.release();

   if (GetResource(RES::LOG_LEVEL) >= 5) {
      // Verify no register leaks at loop exit
      RegisterAllocator verifier(fs);
      verifier.verify_no_leaks("while loop exit");
   }

   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

ParserResult<IrEmitUnit> IrEmitter::emit_repeat_stmt(const LoopStmtPayload& payload)
{
   if (payload.style != LoopStyle::RepeatUntil or not payload.condition or not payload.body) {
      return this->unsupported_stmt(AstNodeKind::RepeatStmt, SourceSpan{});
   }

   FuncState* fs = &this->func_state;
   BCPos loop = fs->lasttarget = fs->pc;
   BCPos iter = NO_JMP;
   ControlFlowEdge condexit;
   bool inner_has_upvals = false;

   LoopContext loop_context{};
   loop_context.break_edge = this->control_flow.make_break_edge();
   loop_context.continue_edge = this->control_flow.make_continue_edge();
   loop_context.defer_base = fs->nactvar;
   loop_context.continue_target = loop;
   this->loop_stack.push_back(loop_context);
   LoopStackGuard loop_stack_guard(this);

   FuncScope outer_scope;
   ScopeGuard loop_guard(fs, &outer_scope, FuncScopeFlag::Loop);
   {
      FuncScope inner_scope;
      ScopeGuard inner_guard(fs, &inner_scope, FuncScopeFlag::None);
      bcemit_AD(fs, BC_LOOP, fs->nactvar, 0);
      auto block_result = this->emit_block(*payload.body, FuncScopeFlag::None);
      if (not block_result.ok()) {
         return block_result;
      }
      iter = fs->pc;
      auto cond_result = this->emit_condition_jump(*payload.condition);
      if (not cond_result.ok()) {
         return ParserResult<IrEmitUnit>::failure(cond_result.error_ref());
      }
      condexit = cond_result.value_ref();
      inner_has_upvals = has_flag(inner_scope.flags, FuncScopeFlag::Upvalue);
      if (inner_has_upvals) {
         auto break_result = this->emit_break_stmt(BreakStmtPayload{});
         if (not break_result.ok()) {
            return break_result;
         }
         condexit.patch_here();
      }
   }
   if (inner_has_upvals) {
      condexit = this->control_flow.make_unconditional(bcemit_jmp(fs));
   }
   condexit.patch_to(loop);
   ControlFlowEdge loop_head = this->control_flow.make_unconditional(loop);
   loop_head.patch_head(fs->pc);

   this->loop_stack.back().continue_target = iter;
   this->loop_stack.back().continue_edge.patch_to(iter);
   this->loop_stack.back().break_edge.patch_here();
   loop_stack_guard.release();

   if (GetResource(RES::LOG_LEVEL) >= 5) {
      // Verify no register leaks at loop exit
      RegisterAllocator verifier(fs);
      verifier.verify_no_leaks("repeat loop exit");
   }

   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

ParserResult<IrEmitUnit> IrEmitter::emit_numeric_for_stmt(const NumericForStmtPayload& payload)
{
   if (not payload.start or not payload.stop or not payload.body) {
      return this->unsupported_stmt(AstNodeKind::NumericForStmt, SourceSpan{});
   }

   FuncState* fs = &this->func_state;
   BCReg base = fs->freereg;
   GCstr* control_symbol = payload.control.symbol ? payload.control.symbol : NAME_BLANK;

   FuncScope outer_scope;
   ScopeGuard loop_guard(fs, &outer_scope, FuncScopeFlag::Loop);

   this->lex_state.var_new_fixed(FORL_IDX, VARNAME_FOR_IDX);
   this->lex_state.var_new_fixed(FORL_STOP, VARNAME_FOR_STOP);
   this->lex_state.var_new_fixed(FORL_STEP, VARNAME_FOR_STEP);
   this->lex_state.var_new(FORL_EXT, control_symbol);

   auto start_expr = this->emit_expression(*payload.start);
   if (not start_expr.ok()) {
      return ParserResult<IrEmitUnit>::failure(start_expr.error_ref());
   }
   ExpDesc start_value = start_expr.value_ref();
   this->materialise_to_next_reg(start_value, "numeric for start");

   auto stop_expr = this->emit_expression(*payload.stop);
   if (not stop_expr.ok()) {
      return ParserResult<IrEmitUnit>::failure(stop_expr.error_ref());
   }
   ExpDesc stop_value = stop_expr.value_ref();
   this->materialise_to_next_reg(stop_value, "numeric for stop");

   if (payload.step) {
      auto step_expr = this->emit_expression(*payload.step);
      if (not step_expr.ok()) {
         return ParserResult<IrEmitUnit>::failure(step_expr.error_ref());
      }
      ExpDesc step_value = step_expr.value_ref();
      this->materialise_to_next_reg(step_value, "numeric for step");
   }
   else {
      RegisterAllocator allocator(fs);
      bcemit_AD(fs, BC_KSHORT, fs->freereg, 1);
      allocator.reserve(1);
   }

   this->lex_state.var_add(3);

   LoopContext loop_context{};
   loop_context.break_edge = this->control_flow.make_break_edge();
   loop_context.continue_edge = this->control_flow.make_continue_edge();
   loop_context.defer_base = fs->nactvar;
   loop_context.continue_target = NO_JMP;
   this->loop_stack.push_back(loop_context);
   LoopStackGuard loop_stack_guard(this);

   ControlFlowEdge loop = this->control_flow.make_unconditional(bcemit_AJ(fs, BC_FORI, base, NO_JMP));

   {
      FuncScope visible_scope;
      ScopeGuard guard(fs, &visible_scope, FuncScopeFlag::None);
      this->lex_state.var_add(1);
      RegisterAllocator allocator(fs);
      allocator.reserve(1);
      std::array<BlockBinding, 1> loop_bindings{};
      std::span<const BlockBinding> binding_span;
      if (payload.control.symbol and not payload.control.is_blank) {
         loop_bindings[0].symbol = payload.control.symbol;
         loop_bindings[0].slot = base + FORL_EXT;
         binding_span = std::span<const BlockBinding>(loop_bindings.data(), 1);
      }
      auto block_result = this->emit_block_with_bindings(*payload.body, FuncScopeFlag::None, binding_span);
      if (not block_result.ok()) {
         return block_result;
      }
   }

   ControlFlowEdge loopend = this->control_flow.make_unconditional(bcemit_AJ(fs, BC_FORL, base, NO_JMP));
   fs->bcbase[loopend.head()].line = payload.body->span.line;
   loopend.patch_head(loop.head() + 1);
   loop.patch_head(fs->pc);
   this->loop_stack.back().continue_target = loopend.head();
   this->loop_stack.back().continue_edge.patch_to(loopend.head());
   this->loop_stack.back().break_edge.patch_here();
   loop_stack_guard.release();
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

ParserResult<IrEmitUnit> IrEmitter::emit_generic_for_stmt(const GenericForStmtPayload& payload)
{
   if (payload.names.empty() or payload.iterators.empty() or not payload.body) {
      return this->unsupported_stmt(AstNodeKind::GenericForStmt, SourceSpan{});
   }

   FuncState* fs = &this->func_state;
   BCReg base = fs->freereg + 3;
   BCReg nvars = 0;

   FuncScope outer_scope;
   ScopeGuard loop_guard(fs, &outer_scope, FuncScopeFlag::Loop);

   this->lex_state.var_new_fixed(nvars++, VARNAME_FOR_GEN);
   this->lex_state.var_new_fixed(nvars++, VARNAME_FOR_STATE);
   this->lex_state.var_new_fixed(nvars++, VARNAME_FOR_CTL);

   for (const Identifier& identifier : payload.names) {
      GCstr* symbol = identifier.symbol ? identifier.symbol : NAME_BLANK;
      this->lex_state.var_new(nvars++, symbol);
   }

   BCPos exprpc = fs->pc;
   BCReg iterator_count = 0;
   auto iter_values = this->emit_expression_list(payload.iterators, iterator_count);
   if (not iter_values.ok()) {
      return ParserResult<IrEmitUnit>::failure(iter_values.error_ref());
   }
   ExpDesc tail = iter_values.value_ref();
   this->lex_state.assign_adjust(3, iterator_count, &tail);

   bcreg_bump(fs, 3 + LJ_FR2);
   int isnext = (nvars <= 5) ? predict_next(this->lex_state, *fs, exprpc) : 0;
   this->lex_state.var_add(3);

   LoopContext loop_context{};
   loop_context.break_edge = this->control_flow.make_break_edge();
   loop_context.continue_edge = this->control_flow.make_continue_edge();
   loop_context.defer_base = fs->nactvar;
   loop_context.continue_target = NO_JMP;
   this->loop_stack.push_back(loop_context);
   LoopStackGuard loop_stack_guard(this);

   ControlFlowEdge loop = this->control_flow.make_unconditional(bcemit_AJ(fs, isnext ? BC_ISNEXT : BC_JMP, base, NO_JMP));

   {
      FuncScope visible_scope;
      ScopeGuard guard(fs, &visible_scope, FuncScopeFlag::None);
      BCReg visible = nvars - 3;
      this->lex_state.var_add(visible);
      RegisterAllocator allocator(fs);
      allocator.reserve(visible);
      std::vector<BlockBinding> loop_bindings;
      loop_bindings.reserve(visible);
      for (BCReg i = 0; i < visible; ++i) {
         const Identifier& identifier = payload.names[i];
         if (identifier.symbol and not identifier.is_blank) {
            BlockBinding binding;
            binding.symbol = identifier.symbol;
            binding.slot = base + i;
            loop_bindings.push_back(binding);
         }
      }
      std::span<const BlockBinding> binding_span(loop_bindings.data(), loop_bindings.size());
      auto block_result = this->emit_block_with_bindings(*payload.body, FuncScopeFlag::None, binding_span);
      if (not block_result.ok()) {
         return block_result;
      }
   }

   loop.patch_head(fs->pc);
   BCPos iter = bcemit_ABC(fs, isnext ? BC_ITERN : BC_ITERC, base, nvars - 3 + 1, 3);
   ControlFlowEdge loopend = this->control_flow.make_unconditional(bcemit_AJ(fs, BC_ITERL, base, NO_JMP));
   fs->bcbase[loopend.head() - 1].line = payload.body->span.line;
   fs->bcbase[loopend.head()].line = payload.body->span.line;
   loopend.patch_head(loop.head() + 1);
   this->loop_stack.back().continue_target = iter;
   this->loop_stack.back().continue_edge.patch_to(iter);
   this->loop_stack.back().break_edge.patch_here();
   loop_stack_guard.release();
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

ParserResult<IrEmitUnit> IrEmitter::emit_defer_stmt(const DeferStmtPayload& payload)
{
   if (not payload.callable) {
      return this->unsupported_stmt(AstNodeKind::DeferStmt, SourceSpan{});
   }

   FuncState* fs = &this->func_state;
   BCReg reg = fs->freereg;
   this->lex_state.var_new(0, NAME_BLANK);
   RegisterAllocator allocator(fs);
   allocator.reserve(1);
   this->lex_state.var_add(1);
   VarInfo* info = &var_get(&this->lex_state, fs, fs->nactvar - 1);
   info->info |= VarInfoFlag::Defer;

   auto function_value = this->emit_function_expr(*payload.callable);
   if (not function_value.ok()) {
      return ParserResult<IrEmitUnit>::failure(function_value.error_ref());
   }
   ExpDesc fn = function_value.value_ref();
   this->materialise_to_reg(fn, reg, "defer callable");

   BCReg nargs = 0;
   for (const ExprNodePtr& argument : payload.arguments) {
      if (not argument) {
         continue;
      }
      auto arg_expr = this->emit_expression(*argument);
      if (not arg_expr.ok()) {
         return ParserResult<IrEmitUnit>::failure(arg_expr.error_ref());
      }
      ExpDesc arg = arg_expr.value_ref();
      this->materialise_to_next_reg(arg, "defer argument");
      nargs++;
   }

   if (nargs > 0) {
      for (BCReg i = 0; i < nargs; ++i) {
         this->lex_state.var_new(i, NAME_BLANK);
      }
      this->lex_state.var_add(nargs);
      for (BCReg i = 0; i < nargs; ++i) {
         VarInfo* arg_info = &var_get(&this->lex_state, fs, fs->nactvar - nargs + i);
         arg_info->info |= VarInfoFlag::DeferArg;
      }
   }

   fs->freereg = fs->nactvar;
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

ParserResult<IrEmitUnit> IrEmitter::emit_break_stmt(const BreakStmtPayload&)
{
   if (this->loop_stack.empty()) {
      return ParserResult<IrEmitUnit>::failure(this->make_error(ParserErrorCode::InternalInvariant, "break outside loop"));
   }

   LoopContext& loop = this->loop_stack.back();
   execute_defers(&this->func_state, loop.defer_base);
   loop.break_edge.append(bcemit_jmp(&this->func_state));
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************

ParserResult<IrEmitUnit> IrEmitter::emit_continue_stmt(const ContinueStmtPayload&)
{
   if (this->loop_stack.empty()) {
      return ParserResult<IrEmitUnit>::failure(this->make_error(ParserErrorCode::InternalInvariant, "continue outside loop"));
   }

   LoopContext& loop = this->loop_stack.back();
   execute_defers(&this->func_state, loop.defer_base);
   loop.continue_edge.append(bcemit_jmp(&this->func_state));
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************

ParserResult<IrEmitUnit> IrEmitter::emit_assignment_stmt(const AssignmentStmtPayload& payload)
{
   if (payload.targets.empty()) {
      return this->unsupported_stmt(AstNodeKind::AssignmentStmt, SourceSpan{});
   }

   auto targets_result = this->prepare_assignment_targets(payload.targets);
   if (not targets_result.ok()) {
      return ParserResult<IrEmitUnit>::failure(targets_result.error_ref());
   }

   std::vector<PreparedAssignment> targets = std::move(targets_result.value_ref());

   if (payload.op IS AssignmentOperator::Plain) {
      return this->emit_plain_assignment(std::move(targets), payload.values);
   }

   if (targets.size() != 1) {
      const ExprNodePtr& node = payload.targets.front();
      const ExprNode* raw = node ? node.get() : nullptr;
      SourceSpan span = raw ? raw->span : SourceSpan{};
      return this->unsupported_stmt(AstNodeKind::AssignmentStmt, span);
   }

   PreparedAssignment target = std::move(targets.front());
   if (payload.op IS AssignmentOperator::IfEmpty) {
      return this->emit_if_empty_assignment(std::move(target), payload.values);
   }

   return this->emit_compound_assignment(payload.op, std::move(target), payload.values);
}

//********************************************************************************************************************

ParserResult<IrEmitUnit> IrEmitter::emit_plain_assignment(std::vector<PreparedAssignment> targets, const ExprNodeList& values)
{
   BCReg nvars = BCReg(targets.size());
   if (nvars IS 0) {
      return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
   }

   RegisterGuard register_guard(&this->func_state);

   ExpDesc tail = make_const_expr(ExpKind::Void);
   BCReg nexps = 0;
   if (not values.empty()) {
      auto list = this->emit_expression_list(values, nexps);
      if (not list.ok()) {
         return ParserResult<IrEmitUnit>::failure(list.error_ref());
      }
      tail = list.value_ref();
   }

   auto assign_from_stack = [&](std::vector<PreparedAssignment>::reverse_iterator first,
      std::vector<PreparedAssignment>::reverse_iterator last)
   {
      for (; first != last; ++first) {
         ExpDesc stack_value;
         expr_init(&stack_value, ExpKind::NonReloc, this->func_state.freereg - 1);
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
      register_guard.release_to(this->func_state.nactvar);
      register_guard.adopt_saved(this->func_state.freereg);
      return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
   }

   this->lex_state.assign_adjust(nvars, nexps, &tail);
   assign_from_stack(targets.rbegin(), targets.rend());
   for (PreparedAssignment& prepared : targets) {
      allocator.release(prepared.reserved);
   }
   register_guard.release_to(this->func_state.nactvar);
   register_guard.adopt_saved(this->func_state.freereg);
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************

ParserResult<IrEmitUnit> IrEmitter::emit_compound_assignment(AssignmentOperator op, PreparedAssignment target, const ExprNodeList& values)
{
   auto mapped = map_assignment_operator(op);
   if (not mapped.has_value()) {
      const ExprNode* raw = nullptr;
      if (not values.empty()) {
         const ExprNodePtr& first = values.front();
         raw = first ? first.get() : nullptr;
      }
      SourceSpan span = raw ? raw->span : SourceSpan{};
      return this->unsupported_stmt(AstNodeKind::AssignmentStmt, span);
   }
   if (values.empty()) {
      return this->unsupported_stmt(AstNodeKind::AssignmentStmt, SourceSpan{});
   }

   BCReg count = 0;
   RegisterGuard register_guard(&this->func_state);

   // Use RegisterAllocator::duplicate_table_operands()
   RegisterAllocator allocator(&this->func_state);
   TableOperandCopies copies = allocator.duplicate_table_operands(target.storage);
   ExpDesc working = copies.duplicated;

   ExpDesc rhs;
   if (mapped.value() IS BinOpr::OPR_CONCAT) {
      ExpDesc infix = working;
      // CONCAT compound assignment: use OperatorEmitter for BC_CAT chaining
      this->operator_emitter.prepare_concat(ValueSlot(&infix));
      auto list = this->emit_expression_list(values, count);
      if (not list.ok()) {
         return ParserResult<IrEmitUnit>::failure(list.error_ref());
      }
      if (count != 1) {
         const ExprNodePtr& node = values.front();
         SourceSpan span = node ? node->span : SourceSpan{};
         return this->unsupported_stmt(AstNodeKind::AssignmentStmt, span);
      }
      rhs = list.value_ref();
      this->operator_emitter.complete_concat(ValueSlot(&infix), ValueUse(&rhs));
      bcemit_store(&this->func_state, &target.storage, &infix);
   }
   else {
      this->materialise_to_next_reg(working, "compound assignment base");
      auto list = this->emit_expression_list(values, count);
      if (not list.ok()) {
         return ParserResult<IrEmitUnit>::failure(list.error_ref());
      }
      if (count != 1) {
         const ExprNodePtr& node = values.front();
         SourceSpan span = node ? node->span : SourceSpan{};
         return this->unsupported_stmt(AstNodeKind::AssignmentStmt, span);
      }
      rhs = list.value_ref();
      ExpDesc infix = working;

      // Use OperatorEmitter for arithmetic compound assignments (+=, -=, *=, /=, %=)
      this->operator_emitter.emit_binary_arith(mapped.value(), ValueSlot(&infix), ValueUse(&rhs));

      bcemit_store(&this->func_state, &target.storage, &infix);
   }

   register_guard.release_to(register_guard.saved());
   allocator.release(target.reserved);
   allocator.release(copies.reserved);
   release_indexed_original(this->func_state, target.storage);
   this->func_state.freereg = this->func_state.nactvar;
   register_guard.adopt_saved(this->func_state.freereg);
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************

ParserResult<IrEmitUnit> IrEmitter::emit_if_empty_assignment(PreparedAssignment target, const ExprNodeList& values)
{
   if (values.empty() or not vkisvar(target.storage.k)) {
      return this->unsupported_stmt(AstNodeKind::AssignmentStmt, SourceSpan{});
   }

   BCReg count = 0;
   RegisterGuard register_guard(&this->func_state);

   // Use RegisterAllocator::duplicate_table_operands()
   RegisterAllocator allocator(&this->func_state);
   TableOperandCopies copies = allocator.duplicate_table_operands(target.storage);
   ExpDesc working = copies.duplicated;

   // Use ExpressionValue for discharge operations
   ExpressionValue lhs_value(&this->func_state, working);
   BCReg lhs_reg = lhs_value.discharge_to_any_reg(allocator);

   ExpDesc nilv = make_nil_expr();
   ExpDesc falsev = make_bool_expr(false);
   ExpDesc zerov = make_num_expr(0.0);
   ExpDesc emptyv = make_interned_string_expr(this->lex_state.intern_empty_string());

   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQP, lhs_reg, const_pri(&nilv)));
   ControlFlowEdge check_nil = this->control_flow.make_unconditional(bcemit_jmp(&this->func_state));

   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQP, lhs_reg, const_pri(&falsev)));
   ControlFlowEdge check_false = this->control_flow.make_unconditional(bcemit_jmp(&this->func_state));

   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQN, lhs_reg, const_num(&this->func_state, &zerov)));
   ControlFlowEdge check_zero = this->control_flow.make_unconditional(bcemit_jmp(&this->func_state));

   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQS, lhs_reg, const_str(&this->func_state, &emptyv)));
   ControlFlowEdge check_empty = this->control_flow.make_unconditional(bcemit_jmp(&this->func_state));

   ControlFlowEdge skip_assign = this->control_flow.make_unconditional(bcemit_jmp(&this->func_state));
   BCPos assign_pos = this->func_state.pc;

   auto list = this->emit_expression_list(values, count);
   if (not list.ok()) {
      return ParserResult<IrEmitUnit>::failure(list.error_ref());
   }
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

   check_nil.patch_to(assign_pos);
   check_false.patch_to(assign_pos);
   check_zero.patch_to(assign_pos);
   check_empty.patch_to(assign_pos);
   skip_assign.patch_to(this->func_state.pc);

   register_guard.release_to(register_guard.saved());
   allocator.release(target.reserved);
   allocator.release(copies.reserved);
   release_indexed_original(this->func_state, target.storage);
   this->func_state.freereg = this->func_state.nactvar;
   register_guard.adopt_saved(this->func_state.freereg);
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

//********************************************************************************************************************

ParserResult<ExpDesc> IrEmitter::emit_expression(const ExprNode& expr)
{
   // Update lexer's last line so bytecode emission uses correct line numbers.
   // Complex expressions (like calls) may save/restore this to ensure their
   // final instruction gets the correct line.
   this->lex_state.lastline = expr.span.line;

   switch (expr.kind) {
   case AstNodeKind::LiteralExpr:
      return this->emit_literal_expr(std::get<LiteralValue>(expr.data));
   case AstNodeKind::IdentifierExpr:
      return this->emit_identifier_expr(std::get<NameRef>(expr.data));
   case AstNodeKind::VarArgExpr:
      return this->emit_vararg_expr();
   case AstNodeKind::UnaryExpr:
      return this->emit_unary_expr(std::get<UnaryExprPayload>(expr.data));
   case AstNodeKind::UpdateExpr:
      return this->emit_update_expr(std::get<UpdateExprPayload>(expr.data));
   case AstNodeKind::BinaryExpr:
      return this->emit_binary_expr(std::get<BinaryExprPayload>(expr.data));
   case AstNodeKind::TernaryExpr:
      return this->emit_ternary_expr(std::get<TernaryExprPayload>(expr.data));
   case AstNodeKind::PresenceExpr:
      return this->emit_presence_expr(std::get<PresenceExprPayload>(expr.data));
   case AstNodeKind::MemberExpr:
      return this->emit_member_expr(std::get<MemberExprPayload>(expr.data));
   case AstNodeKind::IndexExpr:
      return this->emit_index_expr(std::get<IndexExprPayload>(expr.data));
   case AstNodeKind::CallExpr:
      return this->emit_call_expr(std::get<CallExprPayload>(expr.data));
   case AstNodeKind::TableExpr:
      return this->emit_table_expr(std::get<TableExprPayload>(expr.data));
   case AstNodeKind::FunctionExpr:
      return this->emit_function_expr(std::get<FunctionExprPayload>(expr.data));
   default:
      return this->unsupported_expr(expr.kind, expr.span);
   }
}

//********************************************************************************************************************

ParserResult<ControlFlowEdge> IrEmitter::emit_condition_jump(const ExprNode& expr)
{
   auto condition = this->emit_expression(expr);
   if (not condition.ok()) return ParserResult<ControlFlowEdge>::failure(condition.error_ref());
   ExpDesc result = condition.value_ref();
   if (result.k IS ExpKind::Nil) result.k = ExpKind::False;
   bcemit_branch_t(&this->func_state, &result);
   return ParserResult<ControlFlowEdge>::success(this->control_flow.make_false_edge(result.f));
}

//********************************************************************************************************************

ParserResult<ExpDesc> IrEmitter::emit_literal_expr(const LiteralValue& literal)
{
   ExpDesc expr;
   switch (literal.kind) {
      case LiteralKind::Nil:
         expr = make_nil_expr();
         break;
      case LiteralKind::Boolean:
         expr = make_bool_expr(literal.bool_value);
         break;
      case LiteralKind::Number:
         expr = make_num_expr(literal.number_value);
         break;
      case LiteralKind::String:
         expr = make_interned_string_expr(literal.string_value);
         break;
      case LiteralKind::CData:
         expr_init(&expr, ExpKind::CData, 0);
         expr.u.nval = literal.cdata_value;
         break;
   }
   return ParserResult<ExpDesc>::success(expr);
}

//********************************************************************************************************************

ParserResult<ExpDesc> IrEmitter::emit_identifier_expr(const NameRef& reference)
{
   ExpDesc resolved;
   this->lex_state.var_lookup_symbol(reference.identifier.symbol, &resolved);
   return ParserResult<ExpDesc>::success(resolved);
}

//********************************************************************************************************************

ParserResult<ExpDesc> IrEmitter::emit_vararg_expr()
{
   ExpDesc expr;
   RegisterAllocator allocator(&this->func_state);
   allocator.reserve(1);
   BCReg base = this->func_state.freereg - 1;
   expr_init(&expr, ExpKind::Call, bcemit_ABC(&this->func_state, BC_VARG, base, 2, this->func_state.numparams));
   expr.u.s.aux = base;
   expr_set_flag(&expr, ExprFlag::HasRhsReg);
   return ParserResult<ExpDesc>::success(expr);
}

//********************************************************************************************************************

ParserResult<ExpDesc> IrEmitter::emit_unary_expr(const UnaryExprPayload& payload)
{
   if (not payload.operand) return this->unsupported_expr(AstNodeKind::UnaryExpr, SourceSpan{});
   auto operand_result = this->emit_expression(*payload.operand);
   if (not operand_result.ok()) return operand_result;
   ExpDesc operand = operand_result.value_ref();

   // Use OperatorEmitter facade for unary operators
   switch (payload.op) {
      case AstUnaryOperator::Negate:
         this->operator_emitter.emit_unary(BC_UNM, ValueSlot(&operand));
         break;
      case AstUnaryOperator::Not:
         this->operator_emitter.emit_unary(BC_NOT, ValueSlot(&operand));
         break;
      case AstUnaryOperator::Length:
         this->operator_emitter.emit_unary(BC_LEN, ValueSlot(&operand));
         break;
      case AstUnaryOperator::BitNot:
         // BitNot calls bit.bnot library function
         this->operator_emitter.emit_bitnot(ValueSlot(&operand));
         break;
   }
   return ParserResult<ExpDesc>::success(operand);
}

//********************************************************************************************************************

ParserResult<ExpDesc> IrEmitter::emit_update_expr(const UpdateExprPayload& payload)
{
   if (not payload.target) return this->unsupported_expr(AstNodeKind::UpdateExpr, SourceSpan{});

   auto target_result = this->emit_lvalue_expr(*payload.target);
   if (not target_result.ok()) return target_result;

   ExpDesc target = target_result.value_ref();

   // Use RegisterAllocator::duplicate_table_operands()
   RegisterAllocator allocator(&this->func_state);
   TableOperandCopies copies = allocator.duplicate_table_operands(target);
   ExpDesc working = copies.duplicated;

   BinOpr op = (payload.op IS AstUpdateOperator::Increment) ? BinOpr::OPR_ADD : BinOpr::OPR_SUB;

   // Use ExpressionValue for discharge operations
   ExpressionValue operand_value(&this->func_state, working);
   BCReg operand_reg = operand_value.discharge_to_any_reg(allocator);

   BCReg saved_reg = operand_reg;
   if (payload.is_postfix) {
      saved_reg = this->func_state.freereg;
      bcemit_AD(&this->func_state, BC_MOV, saved_reg, operand_reg);
      allocator.reserve(1);
   }

   ExpDesc operand = operand_value.legacy();  // Get ExpDesc for subsequent operations

   ExpDesc delta = make_num_expr(1.0);
   ExpDesc infix = operand;

   // Use OperatorEmitter for arithmetic operation (operand +/- 1)
   this->operator_emitter.emit_binary_arith(op, ValueSlot(&infix), ValueUse(&delta));

   bcemit_store(&this->func_state, &target, &infix);
   release_indexed_original(this->func_state, target);

   if (payload.is_postfix) {
      ir_collapse_freereg(&this->func_state, saved_reg);
      ExpDesc result;
      expr_init(&result, ExpKind::NonReloc, saved_reg);
      return ParserResult<ExpDesc>::success(result);
   }

   return ParserResult<ExpDesc>::success(infix);
}

//********************************************************************************************************************

ParserResult<ExpDesc> IrEmitter::emit_binary_expr(const BinaryExprPayload& payload)
{
   auto lhs_result = this->emit_expression(*payload.left);
   if (not lhs_result.ok()) return lhs_result;

   auto mapped = map_binary_operator(payload.op);
   if (not mapped.has_value()) {
      SourceSpan span = payload.left ? payload.left->span : SourceSpan{};
      return this->unsupported_expr(AstNodeKind::BinaryExpr, span);
   }

   BinOpr opr = mapped.value();
   ExpDesc lhs = lhs_result.value_ref();

   // IF_EMPTY requires special handling - it must emit RHS conditionally like ternary
   // Cannot use the standard prepare/emit RHS/complete pattern

   if (opr IS OPR_IF_EMPTY) return this->emit_if_empty_expr(lhs, *payload.right);

   // ALL binary operators need binop_left preparation before RHS evaluation

   // This discharges LHS to appropriate form to prevent register clobbering

   if (opr IS OPR_AND) { // Logical AND: CFG-based short-circuit implementation
      this->operator_emitter.prepare_logical_and(ValueSlot(&lhs));
   }
   else if (opr IS OPR_OR) { // Logical OR: CFG-based short-circuit implementation
      this->operator_emitter.prepare_logical_or(ValueSlot(&lhs));
   }
   else if (opr IS OPR_CONCAT) { // CONCAT: Discharge to consecutive register for BC_CAT chaining
      this->operator_emitter.prepare_concat(ValueSlot(&lhs));
   }
   else { // All other operators use OperatorEmitter facade
      this->operator_emitter.emit_binop_left(opr, ValueSlot(&lhs));
   }

   // Now evaluate RHS (safe because binop_left prepared LHS)

   auto rhs_result = this->emit_expression(*payload.right);
   if (not rhs_result.ok()) return rhs_result;
   ExpDesc rhs = rhs_result.value_ref();

   // Emit the actual operation based on operator type
   if (opr IS OPR_AND) { // Logical AND: CFG-based short-circuit implementation
      this->operator_emitter.complete_logical_and(ValueSlot(&lhs), ValueUse(&rhs));
   }
   else if (opr IS OPR_OR) { // Logical OR: CFG-based short-circuit implementation
      this->operator_emitter.complete_logical_or(ValueSlot(&lhs), ValueUse(&rhs));
   }
   else if (opr >= OPR_NE and opr <= OPR_GT) { // Comparison operators (NE, EQ, LT, GE, LE, GT)
      this->operator_emitter.emit_comparison(opr, ValueSlot(&lhs), ValueUse(&rhs));
   }
   else if (opr IS OPR_CONCAT) { // CONCAT: CFG-based implementation with BC_CAT chaining
      this->operator_emitter.complete_concat(ValueSlot(&lhs), ValueUse(&rhs));
   }
   else if (opr IS OPR_BAND or opr IS OPR_BOR or opr IS OPR_BXOR or opr IS OPR_SHL or opr IS OPR_SHR) {
      // Bitwise operators: Route through OperatorEmitter (emits bit.* library calls)
      this->operator_emitter.emit_binary_bitwise(opr, ValueSlot(&lhs), ValueUse(&rhs));
   }
   else { // Arithmetic operators (ADD, SUB, MUL, DIV, MOD, POW)
      this->operator_emitter.emit_binary_arith(opr, ValueSlot(&lhs), ValueUse(&rhs));
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
   BCReg lhs_reg = lhs_value.discharge_to_any_reg(allocator);

   ExpDesc nilv = make_nil_expr();
   ExpDesc falsev = make_bool_expr(false);
   ExpDesc zerov = make_num_expr(0.0);
   ExpDesc emptyv = make_interned_string_expr(this->lex_state.intern_empty_string());

   // Extended falsey checks - jumps skip to RHS when value is falsey
   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQP, lhs_reg, const_pri(&nilv)));
   ControlFlowEdge check_nil = this->control_flow.make_unconditional(bcemit_jmp(&this->func_state));
   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQP, lhs_reg, const_pri(&falsev)));
   ControlFlowEdge check_false = this->control_flow.make_unconditional(bcemit_jmp(&this->func_state));
   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQN, lhs_reg, const_num(&this->func_state, &zerov)));
   ControlFlowEdge check_zero = this->control_flow.make_unconditional(bcemit_jmp(&this->func_state));
   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQS, lhs_reg, const_str(&this->func_state, &emptyv)));
   ControlFlowEdge check_empty = this->control_flow.make_unconditional(bcemit_jmp(&this->func_state));

   // LHS is truthy - it's already in lhs_reg, just skip RHS
   ControlFlowEdge skip_rhs = this->control_flow.make_unconditional(bcemit_jmp(&this->func_state));

   // Patch falsey checks to jump here (RHS evaluation)

   BCPos rhs_start = this->func_state.pc;
   check_nil.patch_to(rhs_start);
   check_false.patch_to(rhs_start);
   check_zero.patch_to(rhs_start);
   check_empty.patch_to(rhs_start);

   // Emit RHS - only executed when LHS is falsey

   auto rhs_result = this->emit_expression(rhs_ast);
   if (not rhs_result.ok()) return rhs_result;
   ExpressionValue rhs_value(&this->func_state, rhs_result.value_ref());
   rhs_value.discharge();
   this->materialise_to_reg(rhs_value.legacy(), lhs_reg, "if_empty rhs");

   // Clean up any RHS temporaries, but preserve the result register

   ir_collapse_freereg(&this->func_state, lhs_reg);

   // Patch skip jump to here (after RHS)

   skip_rhs.patch_to(this->func_state.pc);

   // Preserve result register by adjusting what RegisterGuard will restore to
   // Only restore to saved_freereg if it's beyond the result register

   if (register_guard.saved() > lhs_reg + 1) register_guard.adopt_saved(register_guard.saved());
   else register_guard.disarm();  // Keep current freereg (lhs_reg + 1)

   // Result is in lhs_reg

   ExpDesc result;
   expr_init(&result, ExpKind::NonReloc, lhs_reg);
   return ParserResult<ExpDesc>::success(result);
}

//********************************************************************************************************************

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
   BCReg cond_reg = condition_value.discharge_to_any_reg(allocator);

   ExpDesc nilv = make_nil_expr();
   ExpDesc falsev = make_bool_expr(false);
   ExpDesc zerov = make_num_expr(0.0);
   ExpDesc emptyv = make_interned_string_expr(this->lex_state.intern_empty_string());

   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQP, cond_reg, const_pri(&nilv)));
   ControlFlowEdge check_nil = this->control_flow.make_unconditional(bcemit_jmp(&this->func_state));
   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQP, cond_reg, const_pri(&falsev)));
   ControlFlowEdge check_false = this->control_flow.make_unconditional(bcemit_jmp(&this->func_state));
   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQN, cond_reg, const_num(&this->func_state, &zerov)));
   ControlFlowEdge check_zero = this->control_flow.make_unconditional(bcemit_jmp(&this->func_state));
   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQS, cond_reg, const_str(&this->func_state, &emptyv)));
   ControlFlowEdge check_empty = this->control_flow.make_unconditional(bcemit_jmp(&this->func_state));

   auto true_result = this->emit_expression(*Payload.if_true);
   if (not true_result.ok()) {
      return true_result;
   }
   ExpressionValue true_value(&this->func_state, true_result.value_ref());
   true_value.discharge();
   this->materialise_to_reg(true_value.legacy(), cond_reg, "ternary true branch");
   ir_collapse_freereg(&this->func_state, cond_reg);

   ControlFlowEdge skip_false = this->control_flow.make_unconditional(bcemit_jmp(&this->func_state));

   BCPos false_start = this->func_state.pc;
   check_nil.patch_to(false_start);
   check_false.patch_to(false_start);
   check_zero.patch_to(false_start);
   check_empty.patch_to(false_start);

   auto false_result = this->emit_expression(*Payload.if_false);
   if (not false_result.ok()) return false_result;
   ExpressionValue false_value(&this->func_state, false_result.value_ref());
   false_value.discharge();
   this->materialise_to_reg(false_value.legacy(), cond_reg, "ternary false branch");
   ir_collapse_freereg(&this->func_state, cond_reg);

   skip_false.patch_to(this->func_state.pc);

   // Preserve result register by adjusting what RegisterGuard will restore to
   // Only restore to saved_freereg if it's beyond the result register

   if (register_guard.saved() > cond_reg + 1) register_guard.adopt_saved(register_guard.saved());
   else register_guard.disarm();  // Keep current freereg (cond_reg + 1)

   ExpDesc result;
   expr_init(&result, ExpKind::NonReloc, cond_reg);
   return ParserResult<ExpDesc>::success(result);
}

//********************************************************************************************************************

ParserResult<ExpDesc> IrEmitter::emit_presence_expr(const PresenceExprPayload& Payload)
{
   SourceSpan span = Payload.value ? Payload.value->span : SourceSpan{};
   if (not Payload.value) return this->unsupported_expr(AstNodeKind::PresenceExpr, span);
   auto value_result = this->emit_expression(*Payload.value);
   if (not value_result.ok()) return value_result;
   ExpDesc value = value_result.value_ref();
   this->operator_emitter.emit_presence_check(ValueSlot(&value));
   return ParserResult<ExpDesc>::success(value);
}

//********************************************************************************************************************

ParserResult<ExpDesc> IrEmitter::emit_member_expr(const MemberExprPayload& Payload)
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
   ExpDesc key = make_interned_string_expr(Payload.member.symbol);
   expr_index(&this->func_state, &table, &key);
   return ParserResult<ExpDesc>::success(table);
}

//********************************************************************************************************************

ParserResult<ExpDesc> IrEmitter::emit_index_expr(const IndexExprPayload& Payload)
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

ParserResult<ExpDesc> IrEmitter::emit_call_expr(const CallExprPayload& Payload)
{
   // We save lastline here before it gets overwritten by processing sub-expressions.
   BCLine call_line = this->lex_state.lastline;

   ExpDesc callee;
   BCReg base = 0;
   if (const auto* direct = std::get_if<DirectCallTarget>(&Payload.target)) {
      if (not direct->callable) return this->unsupported_expr(AstNodeKind::CallExpr, SourceSpan{});
      auto callee_result = this->emit_expression(*direct->callable);
      if (not callee_result.ok()) return callee_result;
      callee = callee_result.value_ref();
      this->materialise_to_next_reg(callee, "call callee");
#if LJ_FR2
      RegisterAllocator allocator(&this->func_state);
      allocator.reserve(1);
#endif
      base = callee.u.s.info;
   }
   else if (const auto* method = std::get_if<MethodCallTarget>(&Payload.target)) {
      if (not method->receiver or method->method.symbol IS nullptr) {
         return this->unsupported_expr(AstNodeKind::CallExpr, SourceSpan{});
      }
      auto receiver_result = this->emit_expression(*method->receiver);
      if (not receiver_result.ok()) return receiver_result;
      callee = receiver_result.value_ref();
      ExpDesc key = make_interned_string_expr(method->method.symbol);
      bcemit_method(&this->func_state, &callee, &key);
      base = callee.u.s.info;
   }
   else return this->unsupported_expr(AstNodeKind::CallExpr, SourceSpan{});

   BCReg arg_count = 0;
   ExpDesc args = make_const_expr(ExpKind::Void);
   if (not Payload.arguments.empty()) {
      auto args_result = this->emit_expression_list(Payload.arguments, arg_count);
      if (not args_result.ok()) return ParserResult<ExpDesc>::failure(args_result.error_ref());
      args = args_result.value_ref();
   }

   BCIns ins;
   bool forward_tail = Payload.forwards_multret and (args.k IS ExpKind::Call);
   if (forward_tail) {
      setbc_b(ir_bcptr(&this->func_state, &args), 0);
      ins = BCINS_ABC(BC_CALLM, base, 2, args.u.s.aux - base - 1 - LJ_FR2);
   }
   else {
      if (not (args.k IS ExpKind::Void)) this->materialise_to_next_reg(args, "call arguments");
      ins = BCINS_ABC(BC_CALL, base, 2, this->func_state.freereg - base - LJ_FR2);
   }

   // Restore the saved line number so the CALL instruction gets the correct line
   this->lex_state.lastline = call_line;

   ExpDesc result;
   expr_init(&result, ExpKind::Call, bcemit_INS(&this->func_state, ins));
   result.u.s.aux = base;
   this->func_state.freereg = base + 1;
   return ParserResult<ExpDesc>::success(result);
}

//********************************************************************************************************************

ParserResult<ExpDesc> IrEmitter::emit_table_expr(const TableExprPayload &Payload)
{
   FuncState* fs = &this->func_state;
   GCtab* template_table = nullptr;
   int vcall = 0;
   int needarr = 0;
   int fixt = 0;
   uint32_t narr = 1;
   uint32_t nhash = 0;
   BCReg freg = fs->freereg;
   BCPos pc = bcemit_AD(fs, BC_TNEW, freg, 0);
   ExpDesc table;
   expr_init(&table, ExpKind::NonReloc, freg);
   RegisterAllocator allocator(fs);
   allocator.reserve(1);
   freg++;

   for (const TableField& field : Payload.fields) {
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
            if (not expr_isk(&key)) expr_index(fs, &table, &key);
            if (expr_isnumk(&key) and expr_numiszero(&key)) needarr = 1;
            else nhash++;
            break;
         }

         case TableFieldKind::Record: {
            if (not field.name.has_value() or field.name->symbol IS nullptr) {
               return this->unsupported_expr(AstNodeKind::TableExpr, field.span);
            }
            key = make_interned_string_expr(field.name->symbol);
            nhash++;
            break;
         }

         case TableFieldKind::Array:
         default: {
            expr_init(&key, ExpKind::Num, 0);
            setintV(&key.u.nval, int(narr));
            narr++;
            needarr = vcall = 1;
            break;
         }
      }

      auto value_result = this->emit_expression(*field.value);
      if (not value_result.ok()) return value_result;

      ExpDesc val = value_result.value_ref();

      bool emit_constant = expr_isk(&key) and key.k != ExpKind::Nil and (key.k IS ExpKind::Str or expr_isk_nojump(&val));

      if (emit_constant) {
         TValue k;
         TValue* slot;
         if (not template_table) {
            BCReg kidx;
            template_table = lj_tab_new(fs->L, needarr ? narr : 0, hsize2hbits(nhash));
            kidx = const_gc(fs, obj2gco(template_table), LJ_TTAB);
            fs->bcbase[pc].ins = BCINS_AD(BC_TDUP, freg - 1, kidx);
         }

         vcall = 0;
         expr_kvalue(fs, &k, &key);
         slot = lj_tab_set(fs->L, template_table, &k);
         lj_gc_anybarriert(fs->L, template_table);
         if (expr_isk_nojump(&val)) {
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
         if (expr_isk(&key)) expr_index(fs, &table, &key);
         bcemit_store(fs, &table, &val);
      }
   }

   if (vcall) {
      BCInsLine* ilp = &fs->bcbase[fs->pc - 1];
      ExpDesc en = make_const_expr(ExpKind::Num);
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
      BCIns* ip = &fs->bcbase[pc].ins;
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
         Node* node = noderef(template_table->node);
         uint32_t hmask = template_table->hmask;
         for (uint32_t i = 0; i <= hmask; ++i) {
            Node* n = &node[i];
            if (tvistab(&n->val)) {
               setnilV(&n->val);
            }
         }
      }
      lj_gc_check(fs->L);
   }

   return ParserResult<ExpDesc>::success(table);
}

//********************************************************************************************************************

ParserResult<ExpDesc> IrEmitter::emit_function_expr(const FunctionExprPayload& Payload)
{
   if (not Payload.body) return this->unsupported_expr(AstNodeKind::FunctionExpr, SourceSpan{});

   FuncState child_state;
   ParserAllocator allocator = ParserAllocator::from(this->lex_state.L);
   ParserConfig inherited = this->ctx.config();
   ParserContext child_ctx = ParserContext::from(this->lex_state, child_state, allocator, inherited);
   ParserSession session(child_ctx, inherited);

   FuncState* parent_state = &this->func_state;
   ptrdiff_t oldbase = parent_state->bcbase - this->lex_state.bcstack;

   this->lex_state.fs_init(&child_state);
   // Use lastline which was set to the function expression's line by emit_expression()
   child_state.linedefined = this->lex_state.lastline;
   child_state.bcbase = parent_state->bcbase + parent_state->pc;
   child_state.bclim = parent_state->bclim - parent_state->pc;
   bcemit_AD(&child_state, BC_FUNCF, 0, 0);
   if (Payload.is_vararg) child_state.flags |= PROTO_VARARG;

   FuncScope scope;

   // TODO: Should use ScopeGuard, but currently crashes when its destructor calls fscope_end() during stack unwinding.
   //ScopeGuard scope_guard(&child_state, &scope, FuncScopeFlag::None);
   fscope_begin(&child_state, &scope, FuncScopeFlag::None);

   BCReg param_count = BCReg(Payload.parameters.size());
   for (BCReg i = 0; i < param_count; ++i) {
      const FunctionParameter& param = Payload.parameters[i];
      GCstr* symbol = (param.name.symbol and not param.name.is_blank) ? param.name.symbol : NAME_BLANK;
      this->lex_state.var_new(i, symbol);
   }
   child_state.numparams = uint8_t(param_count);
   this->lex_state.var_add(param_count);
   if (child_state.nactvar > 0) {
      RegisterAllocator child_allocator(&child_state);
      child_allocator.reserve(child_state.nactvar);
   }

   IrEmitter child_emitter(child_ctx);
   BCReg base = child_state.nactvar - param_count;
   for (BCReg i = 0; i < param_count; ++i) {
      const FunctionParameter& param = Payload.parameters[i];
      if (param.name.is_blank or param.name.symbol IS nullptr) continue;
      child_emitter.update_local_binding(param.name.symbol, base + i);
   }

   auto body_result = child_emitter.emit_block(*Payload.body, FuncScopeFlag::None);
   if (not body_result.ok()) {
      //fscope_end(&child_state); // Crashes in some cases
      return ParserResult<ExpDesc>::failure(body_result.error_ref());
   }

   GCproto* pt = this->lex_state.fs_finish(Payload.body->span.line);
   parent_state->bcbase = this->lex_state.bcstack + oldbase;
   parent_state->bclim = BCPos(this->lex_state.sizebcstack - oldbase);

   ExpDesc expr;
   expr_init(&expr, ExpKind::Relocable,
      bcemit_AD(parent_state, BC_FNEW, 0, const_gc(parent_state, obj2gco(pt), LJ_TPROTO)));

#if LJ_HASFFI
   parent_state->flags |= (child_state.flags & PROTO_FFI);
#endif

   if (not (parent_state->flags & PROTO_CHILD)) {
      if (parent_state->flags & PROTO_HAS_RETURN) parent_state->flags |= PROTO_FIXUP_RETURN;
      parent_state->flags |= PROTO_CHILD;
   }

   //fscope_end(&child_state); // Crashes in some cases
   return ParserResult<ExpDesc>::success(expr);
}

//********************************************************************************************************************

ParserResult<ExpDesc> IrEmitter::emit_function_lvalue(const FunctionNamePath& path)
{
   if (path.segments.empty()) return this->unsupported_expr(AstNodeKind::FunctionExpr, SourceSpan{});

   NameRef base_ref = make_name_ref(path.segments.front());
   auto base_expr = this->emit_identifier_expr(base_ref);
   if (not base_expr.ok()) {
      return base_expr;
   }
   ExpDesc target = base_expr.value_ref();

   size_t traverse_limit = path.method.has_value() ? path.segments.size() : (path.segments.size() > 0 ? path.segments.size() - 1 : 0);
   for (size_t i = 1; i < traverse_limit; ++i) {
      const Identifier& segment = path.segments[i];
      if (not segment.symbol) {
         return this->unsupported_expr(AstNodeKind::FunctionExpr, SourceSpan{});
      }
      ExpDesc key = make_interned_string_expr(segment.symbol);
      ExpressionValue target_toval(&this->func_state, target);
      target_toval.to_val();
      target = target_toval.legacy();
      RegisterAllocator allocator(&this->func_state);
      ExpressionValue target_value(&this->func_state, target);
      target_value.discharge_to_any_reg(allocator);
      target = target_value.legacy();
      expr_index(&this->func_state, &target, &key);
   }

   const Identifier* final_name = nullptr;
   if (path.method.has_value()) final_name = &path.method.value();
   else if (path.segments.size() > 1) final_name = &path.segments.back();

   if (not final_name) return ParserResult<ExpDesc>::success(target);

   if (not final_name->symbol) return this->unsupported_expr(AstNodeKind::FunctionExpr, SourceSpan{});

   ExpDesc key = make_interned_string_expr(final_name->symbol);
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

ParserResult<ExpDesc> IrEmitter::emit_lvalue_expr(const ExprNode& expr)
{
   switch (expr.kind) {
      case AstNodeKind::IdentifierExpr: {
         auto result = this->emit_identifier_expr(std::get<NameRef>(expr.data));
         if (not result.ok()) return result;
         ExpDesc value = result.value_ref();
         if (value.k IS ExpKind::Local) value.u.s.aux = this->func_state.varmap[value.u.s.info];
         if (not vkisvar(value.k)) return this->unsupported_expr(expr.kind, expr.span);
         return ParserResult<ExpDesc>::success(value);
      }

      case AstNodeKind::MemberExpr: {
         const auto& payload = std::get<MemberExprPayload>(expr.data);
         if (not payload.table or not payload.member.symbol) {
            return this->unsupported_expr(expr.kind, expr.span);
         }

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
         ExpDesc key = make_interned_string_expr(payload.member.symbol);
         expr_index(&this->func_state, &table, &key);
         return ParserResult<ExpDesc>::success(table);
      }

      case AstNodeKind::IndexExpr: {
         const auto& payload = std::get<IndexExprPayload>(expr.data);
         if (not payload.table or not payload.index) {
            return this->unsupported_expr(expr.kind, expr.span);
         }

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

      default:
         return this->unsupported_expr(expr.kind, expr.span);
   }
}

//********************************************************************************************************************

ParserResult<ExpDesc> IrEmitter::emit_expression_list(const ExprNodeList &expressions, BCReg &count)
{
   count = 0;
   if (expressions.empty()) return ParserResult<ExpDesc>::success(make_const_expr(ExpKind::Void));

   ExpDesc last = make_const_expr(ExpKind::Void);
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

ParserResult<std::vector<PreparedAssignment>> IrEmitter::prepare_assignment_targets(const ExprNodeList& targets)
{
   std::vector<PreparedAssignment> lhs;
   lhs.reserve(targets.size());
   RegisterAllocator allocator(&this->func_state);

   auto prv = (prvFluid *)this->func_state.ls->L->Script->ChildPrivate;
   bool trace_assignments = (prv->JitOptions & JOF::TRACE_ASSIGNMENTS) != JOF::NIL;

   for (const ExprNodePtr& node : targets) {
      if (not node) {
         return ParserResult<std::vector<PreparedAssignment>>::failure(this->make_error(
            ParserErrorCode::InternalInvariant, "assignment target missing"));
      }

      auto lvalue = this->emit_lvalue_expr(*node);
      if (not lvalue.ok()) {
         return ParserResult<std::vector<PreparedAssignment>>::failure(lvalue.error_ref());
      }

      ExpDesc slot = lvalue.value_ref();
      PreparedAssignment prepared;
      TableOperandCopies copies = allocator.duplicate_table_operands(slot);
      prepared.storage = copies.duplicated;
      prepared.reserved = std::move(copies.reserved);
      prepared.target = LValue::from_expdesc(&prepared.storage);

      if (trace_assignments and prepared.reserved.count() > 0) {
         const char* target_kind = prepared.target.is_indexed() ? "indexed" : "member";
         pf::Log("Parser").msg("[%d] assignment: prepared %s target, duplicated %d registers (R%d..R%d)",
            this->func_state.ls->linenumber, target_kind,
            prepared.reserved.count(), prepared.reserved.start(),
            prepared.reserved.start() + prepared.reserved.count() - 1);
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

std::optional<BCReg> IrEmitter::resolve_local(GCstr* symbol) const
{
   return this->binding_table.resolve(symbol);
}

void IrEmitter::update_local_binding(GCstr* symbol, BCReg slot)
{
   this->binding_table.add(symbol, slot);
}

void IrEmitter::materialise_to_next_reg(ExpDesc& expression, std::string_view usage)
{
   RegisterAllocator allocator(&this->func_state);
   ExpressionValue value(&this->func_state, expression);
   value.to_next_reg(allocator);
   expression = value.legacy();
   this->ensure_register_floor(usage);
}

void IrEmitter::materialise_to_reg(ExpDesc& expression, BCReg slot, std::string_view usage)
{
   RegisterAllocator allocator(&this->func_state);
   ExpressionValue value(&this->func_state, expression);
   value.to_reg(allocator, slot);
   expression = value.legacy();
   this->ensure_register_floor(usage);
}

void IrEmitter::release_expression(ExpDesc& expression, std::string_view usage)
{
   expr_free(&this->func_state, &expression);
   this->ensure_register_floor(usage);
}

void IrEmitter::ensure_register_floor(std::string_view usage)
{
   if (this->func_state.freereg < this->func_state.nactvar) {
      pf::Log log("Parser");
      log.warning("ast-pipeline register underrun during %.*s (free=%u active=%u)",
         int(usage.size()), usage.data(), unsigned(this->func_state.freereg), unsigned(this->func_state.nactvar));
      this->func_state.freereg = this->func_state.nactvar;
   }
}

void IrEmitter::ensure_register_balance(std::string_view usage)
{
   this->ensure_register_floor(usage);
   if (this->func_state.freereg > this->func_state.nactvar) {
      pf::Log log("Parser");
      int line = this->lex_state.lastline;
      log.warning("ast-pipeline leaked %u registers after %.*s at line %d (free=%u active=%u)",
         unsigned(this->func_state.freereg - this->func_state.nactvar), int(usage.size()), usage.data(),
         line + 1, unsigned(this->func_state.freereg), unsigned(this->func_state.nactvar));
      this->func_state.freereg = this->func_state.nactvar;
   }
}

ParserResult<IrEmitUnit> IrEmitter::unsupported_stmt(AstNodeKind kind, const SourceSpan& span)
{
   glUnsupportedNodes.record(kind, span, "stmt");
   std::string message = "IR emitter does not yet support statement kind " + std::to_string(int(kind));
   return ParserResult<IrEmitUnit>::failure(this->make_error(ParserErrorCode::InternalInvariant, message));
}

ParserResult<ExpDesc> IrEmitter::unsupported_expr(AstNodeKind kind, const SourceSpan& span)
{
   glUnsupportedNodes.record(kind, span, "expr");
   std::string message = "IR emitter does not yet support expression kind " + std::to_string(int(kind));
   return ParserResult<ExpDesc>::failure(this->make_error(ParserErrorCode::InternalInvariant, message));
}

ParserError IrEmitter::make_error(ParserErrorCode code, std::string_view message) const
{
   ParserError error;
   error.code = code;
   error.message.assign(message.begin(), message.end());
   error.token = Token::from_current(this->lex_state);
   return error;
}
