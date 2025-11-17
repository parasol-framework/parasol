#include "parser/ir_emitter.h"

#include <string>

#include "parser/parse_internal.h"
#include "parser/token_types.h"

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
      const ExprNodePtr& first = payload.values.front();
      SourceSpan span = first ? first->span : SourceSpan{};
      return this->unsupported_stmt(AstNodeKind::ReturnStmt, span);
   }
   snapshot_return_regs(&this->func_state, &ins);
   execute_defers(&this->func_state, 0);
   if (this->func_state.flags & PROTO_CHILD) {
      bcemit_AJ(&this->func_state, BC_UCLO, 0, 0);
   }
   bcemit_INS(&this->func_state, ins);
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

