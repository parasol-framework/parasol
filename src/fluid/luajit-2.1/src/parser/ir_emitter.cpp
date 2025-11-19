#include "parser/ir_emitter.h"

#include <string>
#include <utility>

#include "parser/parse_internal.h"
#include "parser/token_types.h"

namespace {

class LocalBindingScope {
public:
   explicit LocalBindingScope(std::vector<std::pair<GCstr*, BCReg>>& bindings)
      : bindings_(bindings)
      , mark_(bindings.size())
   {
   }

   ~LocalBindingScope()
   {
      bindings_.resize(mark_);
   }

private:
   std::vector<std::pair<GCstr*, BCReg>>& bindings_;
   size_t mark_;
};

[[nodiscard]] static bool is_blank_symbol(const Identifier& identifier)
{
   return identifier.is_blank or identifier.symbol IS nullptr;
}

[[nodiscard]] static std::optional<BinOpr> map_binary_operator(AstBinaryOperator op)
{
   switch (op) {
   case AstBinaryOperator::Add:
      return BinOpr::OPR_ADD;
   case AstBinaryOperator::Subtract:
      return BinOpr::OPR_SUB;
   case AstBinaryOperator::Multiply:
      return BinOpr::OPR_MUL;
   case AstBinaryOperator::Divide:
      return BinOpr::OPR_DIV;
   case AstBinaryOperator::Modulo:
      return BinOpr::OPR_MOD;
   case AstBinaryOperator::Power:
      return BinOpr::OPR_POW;
   case AstBinaryOperator::Concat:
      return BinOpr::OPR_CONCAT;
   case AstBinaryOperator::NotEqual:
      return BinOpr::OPR_NE;
   case AstBinaryOperator::Equal:
      return BinOpr::OPR_EQ;
   case AstBinaryOperator::LessThan:
      return BinOpr::OPR_LT;
   case AstBinaryOperator::GreaterEqual:
      return BinOpr::OPR_GE;
   case AstBinaryOperator::LessEqual:
      return BinOpr::OPR_LE;
   case AstBinaryOperator::GreaterThan:
      return BinOpr::OPR_GT;
   case AstBinaryOperator::BitAnd:
      return BinOpr::OPR_BAND;
   case AstBinaryOperator::BitOr:
      return BinOpr::OPR_BOR;
   case AstBinaryOperator::BitXor:
      return BinOpr::OPR_BXOR;
   case AstBinaryOperator::ShiftLeft:
      return BinOpr::OPR_SHL;
   case AstBinaryOperator::ShiftRight:
      return BinOpr::OPR_SHR;
   case AstBinaryOperator::LogicalAnd:
      return BinOpr::OPR_AND;
   case AstBinaryOperator::LogicalOr:
      return BinOpr::OPR_OR;
   case AstBinaryOperator::IfEmpty:
      return BinOpr::OPR_IF_EMPTY;
   default:
      return std::nullopt;
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
     lex_state(context.lex())
{
}

ParserResult<IrEmitUnit> IrEmitter::emit_chunk(const BlockStmt& chunk)
{
   FuncScope chunk_scope;
   ScopeGuard guard(&this->func_state, &chunk_scope, FuncScopeFlag::None);
   auto result = this->emit_block(chunk, FuncScopeFlag::None);
   if (not result.ok()) {
      return result;
   }
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

ParserResult<IrEmitUnit> IrEmitter::emit_block(const BlockStmt& block, FuncScopeFlag flags)
{
   FuncScope scope;
   ScopeGuard guard(&this->func_state, &scope, flags);
   LocalBindingScope binding_scope(this->local_bindings);
   for (const StmtNode& stmt : block.view()) {
      auto status = this->emit_statement(stmt);
      if (not status.ok()) {
         return status;
      }
   }
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

ParserResult<IrEmitUnit> IrEmitter::emit_statement(const StmtNode& stmt)
{
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
   case AstNodeKind::AssignmentStmt: {
      const auto& payload = std::get<AssignmentStmtPayload>(stmt.data);
      return this->emit_assignment_stmt(payload);
   }
   case AstNodeKind::DoStmt: {
      const auto& payload = std::get<DoStmtPayload>(stmt.data);
      if (payload.block) {
         return this->emit_block(*payload.block, FuncScopeFlag::None);
      }
      return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
   }
   default:
      return this->unsupported_stmt(stmt.kind, stmt.span);
   }
}

ParserResult<IrEmitUnit> IrEmitter::emit_expression_stmt(const ExpressionStmtPayload& payload)
{
   if (not payload.expression) {
      return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
   }
   auto expression = this->emit_expression(*payload.expression);
   if (not expression.ok()) {
      return ParserResult<IrEmitUnit>::failure(expression.error_ref());
   }
   ExpDesc value = expression.value_ref();
   expr_toval(&this->func_state, &value);
   expr_free(&this->func_state, &value);
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

ParserResult<IrEmitUnit> IrEmitter::emit_return_stmt(const ReturnStmtPayload& payload)
{
   BCIns ins;
   this->func_state.flags |= PROTO_HAS_RETURN;
   if (payload.values.empty()) {
      ins = BCINS_AD(BC_RET0, 0, 1);
   }
   else {
      BCReg count = 0;
      auto list = this->emit_expression_list(payload.values, count);
      if (not list.ok()) {
         return ParserResult<IrEmitUnit>::failure(list.error_ref());
      }
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
            BCReg reg = expr_toanyreg(&this->func_state, &last);
            ins = BCINS_AD(BC_RET1, reg, 2);
         }
      }
      else {
         if (last.k IS ExpKind::Call) {
            setbc_b(ir_bcptr(&this->func_state, &last), 0);
            ins = BCINS_AD(BC_RETM, this->func_state.nactvar, last.u.s.aux - this->func_state.nactvar);
         }
         else {
            expr_tonextreg(&this->func_state, &last);
            ins = BCINS_AD(BC_RET, this->func_state.nactvar, count + 1);
         }
      }
   }
   snapshot_return_regs(&this->func_state, &ins);
   execute_defers(&this->func_state, 0);
   if (this->func_state.flags & PROTO_CHILD) {
      bcemit_AJ(&this->func_state, BC_UCLO, 0, 0);
   }
   bcemit_INS(&this->func_state, ins);
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

ParserResult<IrEmitUnit> IrEmitter::emit_local_decl_stmt(const LocalDeclStmtPayload& payload)
{
   BCReg nvars = BCReg(payload.names.size());
   if (nvars == 0) {
      return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
   }

   for (BCReg i = 0; i < nvars; ++i) {
      const Identifier& identifier = payload.names[i];
      GCstr* symbol = identifier.symbol;
      this->lex_state.var_new(i, is_blank_symbol(identifier) ? NAME_BLANK : symbol);
   }

   ExpDesc tail;
   BCReg nexps = 0;
   if (payload.values.empty()) {
      tail = make_const_expr(ExpKind::Void);
   }
   else {
      auto list = this->emit_expression_list(payload.values, nexps);
      if (not list.ok()) {
         return ParserResult<IrEmitUnit>::failure(list.error_ref());
      }
      tail = list.value_ref();
   }

   this->lex_state.assign_adjust(nvars, nexps, &tail);
   this->lex_state.var_add(nvars);
   BCReg base = this->func_state.nactvar - nvars;
   for (BCReg i = 0; i < nvars; ++i) {
      const Identifier& identifier = payload.names[i];
      if (is_blank_symbol(identifier)) {
         continue;
      }
      this->local_bindings.emplace_back(identifier.symbol, base + i);
   }
   this->func_state.freereg = this->func_state.nactvar;
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

ParserResult<IrEmitUnit> IrEmitter::emit_assignment_stmt(const AssignmentStmtPayload& payload)
{
   if (payload.op != AssignmentOperator::Plain or payload.targets.size() != 1 or payload.values.size() != 1) {
      const ExprNodePtr* target_ptr = payload.targets.empty() ? nullptr : &payload.targets.front();
      SourceSpan span = (target_ptr and (*target_ptr)) ? (*target_ptr)->span : SourceSpan{};
      return this->unsupported_stmt(AstNodeKind::AssignmentStmt, span);
   }

   const ExprNodePtr& target = payload.targets.front();
   if (not target or not (target->kind IS AstNodeKind::IdentifierExpr)) {
      SourceSpan span = target ? target->span : SourceSpan{};
      return this->unsupported_stmt(AstNodeKind::AssignmentStmt, span);
   }

   const NameRef& name = std::get<NameRef>(target->data);
   auto slot = this->resolve_local(name.identifier.symbol);
   if (not slot.has_value()) {
      return this->unsupported_stmt(AstNodeKind::AssignmentStmt, target->span);
   }

   auto value_result = this->emit_expression(*payload.values.front());
   if (not value_result.ok()) {
      return ParserResult<IrEmitUnit>::failure(value_result.error_ref());
   }

   ExpDesc value = value_result.value_ref();
   expr_toval(&this->func_state, &value);
   expr_toreg(&this->func_state, &value, slot.value());
   expr_free(&this->func_state, &value);
   this->func_state.freereg = this->func_state.nactvar;
   return ParserResult<IrEmitUnit>::success(IrEmitUnit{});
}

ParserResult<ExpDesc> IrEmitter::emit_expression(const ExprNode& expr)
{
   switch (expr.kind) {
   case AstNodeKind::LiteralExpr:
      return this->emit_literal_expr(std::get<LiteralValue>(expr.data));
   case AstNodeKind::IdentifierExpr:
      return this->emit_identifier_expr(std::get<NameRef>(expr.data));
   case AstNodeKind::VarArgExpr:
      return this->emit_vararg_expr();
   case AstNodeKind::UnaryExpr:
      return this->emit_unary_expr(std::get<UnaryExprPayload>(expr.data));
   case AstNodeKind::BinaryExpr:
      return this->emit_binary_expr(std::get<BinaryExprPayload>(expr.data));
   case AstNodeKind::PresenceExpr:
      return this->emit_presence_expr(std::get<PresenceExprPayload>(expr.data));
   case AstNodeKind::MemberExpr:
      return this->emit_member_expr(std::get<MemberExprPayload>(expr.data));
   case AstNodeKind::IndexExpr:
      return this->emit_index_expr(std::get<IndexExprPayload>(expr.data));
   case AstNodeKind::CallExpr:
      return this->emit_call_expr(std::get<CallExprPayload>(expr.data));
   default:
      return this->unsupported_expr(expr.kind, expr.span);
   }
}

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

ParserResult<ExpDesc> IrEmitter::emit_identifier_expr(const NameRef& reference)
{
   if (reference.identifier.symbol) {
      auto slot = this->resolve_local(reference.identifier.symbol);
      if (slot.has_value()) {
         ExpDesc expr;
         expr_init(&expr, ExpKind::Local, slot.value());
         return ParserResult<ExpDesc>::success(expr);
      }
   }

   ExpDesc expr;
   switch (reference.resolution) {
   case NameResolution::Local:
      expr_init(&expr, ExpKind::Local, reference.slot);
      break;
   case NameResolution::Upvalue:
      expr_init(&expr, ExpKind::Upval, reference.slot);
      break;
   case NameResolution::Global:
   case NameResolution::Environment:
   case NameResolution::Unresolved:
   default:
      expr_init(&expr, ExpKind::Global, 0);
      expr.u.sval = reference.identifier.symbol;
      break;
   }
   return ParserResult<ExpDesc>::success(expr);
}

ParserResult<ExpDesc> IrEmitter::emit_vararg_expr()
{
   ExpDesc expr;
   bcreg_reserve(&this->func_state, 1);
   BCReg base = this->func_state.freereg - 1;
   expr_init(&expr, ExpKind::Call, bcemit_ABC(&this->func_state, BC_VARG, base, 2, this->func_state.numparams));
   expr.u.s.aux = base;
   expr_set_flag(&expr, ExprFlag::HasRhsReg);
   return ParserResult<ExpDesc>::success(expr);
}

ParserResult<ExpDesc> IrEmitter::emit_unary_expr(const UnaryExprPayload& payload)
{
   if (not payload.operand) {
      return this->unsupported_expr(AstNodeKind::UnaryExpr, SourceSpan{});
   }
   auto operand_result = this->emit_expression(*payload.operand);
   if (not operand_result.ok()) {
      return operand_result;
   }
   ExpDesc operand = operand_result.value_ref();
   switch (payload.op) {
   case AstUnaryOperator::Negate:
      bcemit_unop(&this->func_state, BC_UNM, &operand);
      break;
   case AstUnaryOperator::Not:
      bcemit_unop(&this->func_state, BC_NOT, &operand);
      break;
   case AstUnaryOperator::Length:
      bcemit_unop(&this->func_state, BC_LEN, &operand);
      break;
   case AstUnaryOperator::BitNot:
      bcemit_unary_bit_call(&this->func_state, "bnot", &operand);
      break;
   }
   return ParserResult<ExpDesc>::success(operand);
}

ParserResult<ExpDesc> IrEmitter::emit_binary_expr(const BinaryExprPayload& payload)
{
   auto lhs_result = this->emit_expression(*payload.left);
   if (not lhs_result.ok()) {
      return lhs_result;
   }
   auto rhs_result = this->emit_expression(*payload.right);
   if (not rhs_result.ok()) {
      return rhs_result;
   }
   auto mapped = map_binary_operator(payload.op);
   if (not mapped.has_value()) {
      SourceSpan span = payload.left ? payload.left->span : SourceSpan{};
      return this->unsupported_expr(AstNodeKind::BinaryExpr, span);
   }
   ExpDesc lhs = lhs_result.value_ref();
   ExpDesc rhs = rhs_result.value_ref();
   bcemit_binop(&this->func_state, mapped.value(), &lhs, &rhs);
   return ParserResult<ExpDesc>::success(lhs);
}

ParserResult<ExpDesc> IrEmitter::emit_presence_expr(const PresenceExprPayload& payload)
{
   SourceSpan span = payload.value ? payload.value->span : SourceSpan{};
   if (not payload.value) {
      return this->unsupported_expr(AstNodeKind::PresenceExpr, span);
   }
   auto value_result = this->emit_expression(*payload.value);
   if (not value_result.ok()) {
      return value_result;
   }
   ExpDesc value = value_result.value_ref();
   bcemit_presence_check(&this->func_state, &value);
   return ParserResult<ExpDesc>::success(value);
}

ParserResult<ExpDesc> IrEmitter::emit_member_expr(const MemberExprPayload& payload)
{
   if (not payload.table or payload.member.symbol IS nullptr) {
      return this->unsupported_expr(AstNodeKind::MemberExpr, payload.member.span);
   }
   auto table_result = this->emit_expression(*payload.table);
   if (not table_result.ok()) {
      return table_result;
   }
   ExpDesc table = table_result.value_ref();
   expr_toanyreg(&this->func_state, &table);
   ExpDesc key = make_interned_string_expr(payload.member.symbol);
   expr_index(&this->func_state, &table, &key);
   return ParserResult<ExpDesc>::success(table);
}

ParserResult<ExpDesc> IrEmitter::emit_index_expr(const IndexExprPayload& payload)
{
   if (not payload.table or not payload.index) {
      return this->unsupported_expr(AstNodeKind::IndexExpr, SourceSpan{});
   }
   auto table_result = this->emit_expression(*payload.table);
   if (not table_result.ok()) {
      return table_result;
   }
   auto key_result = this->emit_expression(*payload.index);
   if (not key_result.ok()) {
      return key_result;
   }
   ExpDesc table = table_result.value_ref();
   ExpDesc key = key_result.value_ref();
   expr_toanyreg(&this->func_state, &table);
   expr_toval(&this->func_state, &key);
   expr_index(&this->func_state, &table, &key);
   return ParserResult<ExpDesc>::success(table);
}

ParserResult<ExpDesc> IrEmitter::emit_call_expr(const CallExprPayload& payload)
{
   ExpDesc callee;
   BCReg base = 0;
   if (const auto* direct = std::get_if<DirectCallTarget>(&payload.target)) {
      if (not direct->callable) {
         return this->unsupported_expr(AstNodeKind::CallExpr, SourceSpan{});
      }
      auto callee_result = this->emit_expression(*direct->callable);
      if (not callee_result.ok()) {
         return callee_result;
      }
      callee = callee_result.value_ref();
      expr_tonextreg(&this->func_state, &callee);
#if LJ_FR2
      bcreg_reserve(&this->func_state, 1);
#endif
      base = callee.u.s.info;
   }
   else if (const auto* method = std::get_if<MethodCallTarget>(&payload.target)) {
      if (not method->receiver or method->method.symbol IS nullptr) {
         return this->unsupported_expr(AstNodeKind::CallExpr, SourceSpan{});
      }
      auto receiver_result = this->emit_expression(*method->receiver);
      if (not receiver_result.ok()) {
         return receiver_result;
      }
      callee = receiver_result.value_ref();
      ExpDesc key = make_interned_string_expr(method->method.symbol);
      bcemit_method(&this->func_state, &callee, &key);
      base = callee.u.s.info;
   }
   else {
      return this->unsupported_expr(AstNodeKind::CallExpr, SourceSpan{});
   }

   BCReg arg_count = 0;
   ExpDesc args = make_const_expr(ExpKind::Void);
   if (not payload.arguments.empty()) {
      auto args_result = this->emit_expression_list(payload.arguments, arg_count);
      if (not args_result.ok()) {
         return ParserResult<ExpDesc>::failure(args_result.error_ref());
      }
      args = args_result.value_ref();
   }

   BCIns ins;
   if (args.k IS ExpKind::Call) {
      setbc_b(ir_bcptr(&this->func_state, &args), 0);
      ins = BCINS_ABC(BC_CALLM, base, 2, args.u.s.aux - base - 1 - LJ_FR2);
   }
   else {
      if (not (args.k IS ExpKind::Void)) {
         expr_tonextreg(&this->func_state, &args);
      }
      ins = BCINS_ABC(BC_CALL, base, 2, this->func_state.freereg - base - LJ_FR2);
   }

   ExpDesc result;
   expr_init(&result, ExpKind::Call, bcemit_INS(&this->func_state, ins));
   result.u.s.aux = base;
   this->func_state.freereg = base + 1;
   return ParserResult<ExpDesc>::success(result);
}

ParserResult<ExpDesc> IrEmitter::emit_expression_list(const ExprNodeList& expressions, BCReg& count)
{
   count = 0;
   if (expressions.empty()) {
      return ParserResult<ExpDesc>::success(make_const_expr(ExpKind::Void));
   }

   ExpDesc last = make_const_expr(ExpKind::Void);
   bool first = true;
   for (const ExprNodePtr& node : expressions) {
      if (not node) {
         return this->unsupported_expr(AstNodeKind::ExpressionStmt, SourceSpan{});
      }
      auto value = this->emit_expression(*node);
      if (not value.ok()) {
         return value;
      }
      ExpDesc expr = value.value_ref();
      ++count;
      if (not first) {
         expr_tonextreg(&this->func_state, &last);
      }
      last = expr;
      first = false;
   }
   return ParserResult<ExpDesc>::success(last);
}

std::optional<BCReg> IrEmitter::resolve_local(GCstr* symbol) const
{
   if (not symbol) {
      return std::nullopt;
   }
   for (auto it = this->local_bindings.rbegin(); it != this->local_bindings.rend(); ++it) {
      if (it->first == symbol) {
         return it->second;
      }
   }
   return std::nullopt;
}

ParserResult<IrEmitUnit> IrEmitter::unsupported_stmt(AstNodeKind kind, const SourceSpan& span)
{
   (void)span;
   std::string message = "IR emitter does not yet support statement kind " + std::to_string(int(kind));
   return ParserResult<IrEmitUnit>::failure(this->make_error(ParserErrorCode::InternalInvariant, message));
}

ParserResult<ExpDesc> IrEmitter::unsupported_expr(AstNodeKind kind, const SourceSpan& span)
{
   (void)span;
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

