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
   return identifier.is_blank or identifier.symbol == nullptr;
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
      if (count == 1 and not (last.k IS ExpKind::Call)) {
         BCReg reg = expr_toanyreg(&this->func_state, &last);
         ins = BCINS_AD(BC_RET1, reg, 2);
      }
      else {
         const ExprNodePtr& first = payload.values.front();
         SourceSpan span = first ? first->span : SourceSpan{};
         return this->unsupported_stmt(AstNodeKind::ReturnStmt, span);
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
   case AstNodeKind::BinaryExpr:
      return this->emit_binary_expr(std::get<BinaryExprPayload>(expr.data));
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

   BinOpr op;
   switch (payload.op) {
   case AstBinaryOperator::Add:
      op = BinOpr::OPR_ADD;
      break;
   case AstBinaryOperator::Multiply:
      op = BinOpr::OPR_MUL;
      break;
   default:
      return this->unsupported_expr(AstNodeKind::BinaryExpr, payload.left ? payload.left->span : SourceSpan{});
   }

   ExpDesc lhs = lhs_result.value_ref();
   ExpDesc rhs = rhs_result.value_ref();
   bcemit_arith(&this->func_state, op, &lhs, &rhs);
   return ParserResult<ExpDesc>::success(lhs);
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

