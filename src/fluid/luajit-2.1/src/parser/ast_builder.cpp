#include "parser/ast_builder.h"

#include <utility>

#include "parser/token_types.h"
#include "parser/parse_types.h"

static SourceSpan combine_spans(const SourceSpan& start, const SourceSpan& end)
{
   SourceSpan span = start;
   span.offset = end.offset;
   span.line = end.line;
   span.column = end.column;
   return span;
}

static FunctionExprPayload* function_payload_from(ExprNode& node)
{
   if (node.kind != AstNodeKind::FunctionExpr) return nullptr;
   return std::get_if<FunctionExprPayload>(&node.data);
}

static std::unique_ptr<FunctionExprPayload> move_function_payload(ExprNodePtr& node)
{
   auto* payload = function_payload_from(*node);
   if (payload == nullptr) {
      return std::make_unique<FunctionExprPayload>();
   }
   std::unique_ptr<FunctionExprPayload> result = std::make_unique<FunctionExprPayload>();
   result->parameters = std::move(payload->parameters);
   result->is_vararg = payload->is_vararg;
   result->body = std::move(payload->body);
   return result;
}

AstBuilder::AstBuilder(ParserContext& context)
   : ctx(context)
{
}

ParserResult<std::unique_ptr<BlockStmt>> AstBuilder::parse_chunk()
{
   const TokenKind terms[] = { TokenKind::EndOfFile };
   return this->parse_block(terms);
}

ParserResult<std::unique_ptr<BlockStmt>> AstBuilder::parse_block(
   std::span<const TokenKind> terminators)
{
   std::unique_ptr<BlockStmt> block = std::make_unique<BlockStmt>();
   Token start = this->ctx.tokens().current();
   while (not this->at_end_of_block(terminators)) {
      auto stmt = this->parse_statement();
      if (not stmt.ok()) {
         return ParserResult<std::unique_ptr<BlockStmt>>::failure(stmt.error_ref());
      }
      if (stmt.value_ref()) {
         block->statements.push_back(std::move(stmt.value_ref()));
      }
   }
   Token end = this->ctx.tokens().current();
   block->span = this->span_from(start, end);
   return ParserResult<std::unique_ptr<BlockStmt>>::success(std::move(block));
}

ParserResult<StmtNodePtr> AstBuilder::parse_statement()
{
   Token current = this->ctx.tokens().current();
   switch (current.kind()) {
   case TokenKind::Local:
      return this->parse_local();
   case TokenKind::Function:
      return this->parse_function_stmt();
   case TokenKind::If:
      return this->parse_if();
   case TokenKind::WhileToken:
      return this->parse_while();
   case TokenKind::Repeat:
      return this->parse_repeat();
   case TokenKind::For:
      return this->parse_for();
   case TokenKind::DoToken:
      return this->parse_do();
   case TokenKind::DeferToken:
      return this->parse_defer();
   case TokenKind::ReturnToken:
      return this->parse_return();
   case TokenKind::BreakToken: {
      StmtNodePtr node = std::make_unique<StmtNode>();
      node->kind = AstNodeKind::BreakStmt;
      node->span = current.span();
      node->data.emplace<BreakStmtPayload>();
      this->ctx.tokens().advance();
      return ParserResult<StmtNodePtr>::success(std::move(node));
   }
   case TokenKind::ContinueToken: {
      StmtNodePtr node = std::make_unique<StmtNode>();
      node->kind = AstNodeKind::ContinueStmt;
      node->span = current.span();
      node->data.emplace<ContinueStmtPayload>();
      this->ctx.tokens().advance();
      return ParserResult<StmtNodePtr>::success(std::move(node));
   }
   case TokenKind::Semicolon:
      this->ctx.tokens().advance();
      return ParserResult<StmtNodePtr>::success(nullptr);
   default:
      return this->parse_expression_stmt();
   }
}

ParserResult<StmtNodePtr> AstBuilder::parse_local()
{
   Token local_token = this->ctx.tokens().current();
   this->ctx.tokens().advance();
   if (this->ctx.check(TokenKind::Function)) {
      this->ctx.tokens().advance();
      auto name_token = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
      if (not name_token.ok()) {
         return ParserResult<StmtNodePtr>::failure(name_token.error_ref());
      }
      auto fn = this->parse_function_literal();
      if (not fn.ok()) {
         return ParserResult<StmtNodePtr>::failure(fn.error_ref());
      }
      ExprNodePtr function_expr = std::move(fn.value_ref());
      StmtNodePtr stmt = std::make_unique<StmtNode>();
      stmt->kind = AstNodeKind::LocalFunctionStmt;
      stmt->span = this->span_from(local_token, name_token.value_ref());
      LocalFunctionStmtPayload payload;
      payload.name = make_identifier(name_token.value_ref());
      payload.function = move_function_payload(function_expr);
      stmt->data = std::move(payload);
      return ParserResult<StmtNodePtr>::success(std::move(stmt));
   }

   auto names = this->parse_name_list();
   if (not names.ok()) {
      return ParserResult<StmtNodePtr>::failure(names.error_ref());
   }

   ExprNodeList values;
   if (this->ctx.match(TokenKind::Equals).ok()) {
      auto rhs = this->parse_expression_list();
      if (not rhs.ok()) {
         return ParserResult<StmtNodePtr>::failure(rhs.error_ref());
      }
      values = std::move(rhs.value_ref());
   }

   StmtNodePtr stmt = std::make_unique<StmtNode>();
   stmt->kind = AstNodeKind::LocalDeclStmt;
   stmt->span = local_token.span();
   LocalDeclStmtPayload payload;
   payload.names = std::move(names.value_ref());
   payload.values = std::move(values);
   stmt->data = std::move(payload);
   return ParserResult<StmtNodePtr>::success(std::move(stmt));
}

ParserResult<StmtNodePtr> AstBuilder::parse_function_stmt()
{
   Token func_token = this->ctx.tokens().current();
   this->ctx.tokens().advance();
   FunctionNamePath path;
   auto name_token = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
   if (not name_token.ok()) {
      return ParserResult<StmtNodePtr>::failure(name_token.error_ref());
   }
   path.segments.push_back(make_identifier(name_token.value_ref()));
   bool method = false;
   while (this->ctx.match(TokenKind::Dot).ok()) {
      auto seg = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
      if (not seg.ok()) {
         return ParserResult<StmtNodePtr>::failure(seg.error_ref());
      }
      path.segments.push_back(make_identifier(seg.value_ref()));
   }
   if (this->ctx.match(TokenKind::Colon).ok()) {
      method = true;
      auto seg = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
      if (not seg.ok()) {
         return ParserResult<StmtNodePtr>::failure(seg.error_ref());
      }
      path.method = make_identifier(seg.value_ref());
   }

   auto fn = this->parse_function_literal();
   if (not fn.ok()) {
      return ParserResult<StmtNodePtr>::failure(fn.error_ref());
   }
   ExprNodePtr function_expr = std::move(fn.value_ref());

   if (method and path.method.has_value()) {
      auto* payload = function_payload_from(*function_expr);
      FunctionParameter self_param;
      self_param.name = path.method.value();
      self_param.is_self = true;
      if (payload) {
         payload->parameters.insert(payload->parameters.begin(), self_param);
      }
   }

   StmtNodePtr stmt = std::make_unique<StmtNode>();
   stmt->kind = AstNodeKind::FunctionStmt;
   stmt->span = this->span_from(func_token, name_token.value_ref());
   FunctionStmtPayload payload;
   payload.name = std::move(path);
   payload.function = move_function_payload(function_expr);
   stmt->data = std::move(payload);
   return ParserResult<StmtNodePtr>::success(std::move(stmt));
}

ParserResult<StmtNodePtr> AstBuilder::parse_if()
{
   Token if_token = this->ctx.tokens().current();
   this->ctx.tokens().advance();
   std::vector<IfClause> clauses;
   auto condition = this->parse_expression();
   if (not condition.ok()) {
      return ParserResult<StmtNodePtr>::failure(condition.error_ref());
   }
   this->ctx.consume(TokenKind::ThenToken, ParserErrorCode::ExpectedToken);
   auto then_block = this->parse_scoped_block({ TokenKind::ElseIf, TokenKind::Else, TokenKind::EndToken });
   if (not then_block.ok()) {
      return ParserResult<StmtNodePtr>::failure(then_block.error_ref());
   }
   IfClause clause;
   clause.condition = std::move(condition.value_ref());
   clause.block = std::move(then_block.value_ref());
   clauses.push_back(std::move(clause));

   while (this->ctx.check(TokenKind::ElseIf)) {
      this->ctx.tokens().advance();
      auto cond = this->parse_expression();
      if (not cond.ok()) {
         return ParserResult<StmtNodePtr>::failure(cond.error_ref());
      }
      this->ctx.consume(TokenKind::ThenToken, ParserErrorCode::ExpectedToken);
      auto block = this->parse_scoped_block({ TokenKind::ElseIf, TokenKind::Else, TokenKind::EndToken });
      if (not block.ok()) {
         return ParserResult<StmtNodePtr>::failure(block.error_ref());
      }
      IfClause elseif_clause;
      elseif_clause.condition = std::move(cond.value_ref());
      elseif_clause.block = std::move(block.value_ref());
      clauses.push_back(std::move(elseif_clause));
   }

   if (this->ctx.match(TokenKind::Else).ok()) {
      auto else_block = this->parse_scoped_block({ TokenKind::EndToken });
      if (not else_block.ok()) {
         return ParserResult<StmtNodePtr>::failure(else_block.error_ref());
      }
      IfClause else_clause;
      else_clause.condition = nullptr;
      else_clause.block = std::move(else_block.value_ref());
      clauses.push_back(std::move(else_clause));
   }

   this->ctx.consume(TokenKind::EndToken, ParserErrorCode::ExpectedToken);

   StmtNodePtr stmt = std::make_unique<StmtNode>();
   stmt->kind = AstNodeKind::IfStmt;
   stmt->span = if_token.span();
   IfStmtPayload payload;
   payload.clauses = std::move(clauses);
   stmt->data = std::move(payload);
   return ParserResult<StmtNodePtr>::success(std::move(stmt));
}

ParserResult<StmtNodePtr> AstBuilder::parse_while()
{
   Token token = this->ctx.tokens().current();
   this->ctx.tokens().advance();
   auto condition = this->parse_expression();
   if (not condition.ok()) {
      return ParserResult<StmtNodePtr>::failure(condition.error_ref());
   }
   this->ctx.consume(TokenKind::DoToken, ParserErrorCode::ExpectedToken);
   auto body = this->parse_scoped_block({ TokenKind::EndToken });
   if (not body.ok()) {
      return ParserResult<StmtNodePtr>::failure(body.error_ref());
   }
   this->ctx.consume(TokenKind::EndToken, ParserErrorCode::ExpectedToken);

   StmtNodePtr stmt = std::make_unique<StmtNode>();
   stmt->kind = AstNodeKind::WhileStmt;
   stmt->span = token.span();
   LoopStmtPayload payload;
   payload.style = LoopStyle::WhileLoop;
   payload.condition = std::move(condition.value_ref());
   payload.body = std::move(body.value_ref());
   stmt->data = std::move(payload);
   return ParserResult<StmtNodePtr>::success(std::move(stmt));
}

ParserResult<StmtNodePtr> AstBuilder::parse_repeat()
{
   Token token = this->ctx.tokens().current();
   this->ctx.tokens().advance();
   const TokenKind terms[] = { TokenKind::Until };
   auto body = this->parse_block(terms);
   if (not body.ok()) {
      return ParserResult<StmtNodePtr>::failure(body.error_ref());
   }
   this->ctx.consume(TokenKind::Until, ParserErrorCode::ExpectedToken);
   auto condition = this->parse_expression();
   if (not condition.ok()) {
      return ParserResult<StmtNodePtr>::failure(condition.error_ref());
   }

   StmtNodePtr stmt = std::make_unique<StmtNode>();
   stmt->kind = AstNodeKind::RepeatStmt;
   stmt->span = token.span();
   LoopStmtPayload payload;
   payload.style = LoopStyle::RepeatUntil;
   payload.body = std::move(body.value_ref());
   payload.condition = std::move(condition.value_ref());
   stmt->data = std::move(payload);
   return ParserResult<StmtNodePtr>::success(std::move(stmt));
}

ParserResult<StmtNodePtr> AstBuilder::parse_for()
{
   Token token = this->ctx.tokens().current();
   this->ctx.tokens().advance();
   auto name_token = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
   if (not name_token.ok()) {
      return ParserResult<StmtNodePtr>::failure(name_token.error_ref());
   }

   if (this->ctx.match(TokenKind::Equals).ok()) {
      auto start = this->parse_expression();
      if (not start.ok()) {
         return ParserResult<StmtNodePtr>::failure(start.error_ref());
      }
      this->ctx.consume(TokenKind::Comma, ParserErrorCode::ExpectedToken);
      auto stop = this->parse_expression();
      if (not stop.ok()) {
         return ParserResult<StmtNodePtr>::failure(stop.error_ref());
      }
      ExprNodePtr step_expr;
      if (this->ctx.match(TokenKind::Comma).ok()) {
         auto step = this->parse_expression();
         if (not step.ok()) {
            return ParserResult<StmtNodePtr>::failure(step.error_ref());
         }
         step_expr = std::move(step.value_ref());
      }
      this->ctx.consume(TokenKind::DoToken, ParserErrorCode::ExpectedToken);
      auto body = this->parse_scoped_block({ TokenKind::EndToken });
      if (not body.ok()) {
         return ParserResult<StmtNodePtr>::failure(body.error_ref());
      }
      this->ctx.consume(TokenKind::EndToken, ParserErrorCode::ExpectedToken);

      StmtNodePtr stmt = std::make_unique<StmtNode>();
      stmt->kind = AstNodeKind::NumericForStmt;
      stmt->span = token.span();
      NumericForStmtPayload payload;
      payload.control = make_identifier(name_token.value_ref());
      payload.start = std::move(start.value_ref());
      payload.stop = std::move(stop.value_ref());
      payload.step = std::move(step_expr);
      payload.body = std::move(body.value_ref());
      stmt->data = std::move(payload);
      return ParserResult<StmtNodePtr>::success(std::move(stmt));
   }

   std::vector<Identifier> names;
   names.push_back(make_identifier(name_token.value_ref()));
   while (this->ctx.match(TokenKind::Comma).ok()) {
      auto extra = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
      if (not extra.ok()) {
         return ParserResult<StmtNodePtr>::failure(extra.error_ref());
      }
      names.push_back(make_identifier(extra.value_ref()));
   }
   this->ctx.consume(TokenKind::InToken, ParserErrorCode::ExpectedToken);
   auto iterators = this->parse_expression_list();
   if (not iterators.ok()) {
      return ParserResult<StmtNodePtr>::failure(iterators.error_ref());
   }
   this->ctx.consume(TokenKind::DoToken, ParserErrorCode::ExpectedToken);
   auto body = this->parse_scoped_block({ TokenKind::EndToken });
   if (not body.ok()) {
      return ParserResult<StmtNodePtr>::failure(body.error_ref());
   }
   this->ctx.consume(TokenKind::EndToken, ParserErrorCode::ExpectedToken);

   StmtNodePtr stmt = std::make_unique<StmtNode>();
   stmt->kind = AstNodeKind::GenericForStmt;
   stmt->span = token.span();
   GenericForStmtPayload payload;
   payload.names = std::move(names);
   payload.iterators = std::move(iterators.value_ref());
   payload.body = std::move(body.value_ref());
   stmt->data = std::move(payload);
   return ParserResult<StmtNodePtr>::success(std::move(stmt));
}

ParserResult<StmtNodePtr> AstBuilder::parse_do()
{
   Token token = this->ctx.tokens().current();
   this->ctx.tokens().advance();
   const TokenKind terms[] = { TokenKind::EndToken };
   auto block = this->parse_block(terms);
   if (not block.ok()) {
      return ParserResult<StmtNodePtr>::failure(block.error_ref());
   }
   this->ctx.consume(TokenKind::EndToken, ParserErrorCode::ExpectedToken);
   StmtNodePtr stmt = std::make_unique<StmtNode>();
   stmt->kind = AstNodeKind::DoStmt;
   stmt->span = token.span();
   DoStmtPayload payload;
   payload.block = std::move(block.value_ref());
   stmt->data = std::move(payload);
   return ParserResult<StmtNodePtr>::success(std::move(stmt));
}

ParserResult<StmtNodePtr> AstBuilder::parse_defer()
{
   Token token = this->ctx.tokens().current();
   this->ctx.tokens().advance();
   bool has_params = this->ctx.check(TokenKind::LeftParen);
   ParameterListResult param_info;
   if (has_params) {
      auto parsed = this->parse_parameter_list(true);
      if (not parsed.ok()) {
         return ParserResult<StmtNodePtr>::failure(parsed.error_ref());
      }
      param_info = std::move(parsed.value_ref());
   }
   const TokenKind body_terms[] = { TokenKind::EndToken };
   auto body = this->parse_block(body_terms);
   if (not body.ok()) {
      return ParserResult<StmtNodePtr>::failure(body.error_ref());
   }
   this->ctx.consume(TokenKind::EndToken, ParserErrorCode::ExpectedToken);

   ExprNodeList args;
   if (this->ctx.match(TokenKind::LeftParen).ok()) {
      if (not this->ctx.check(TokenKind::RightParen)) {
         auto parsed_args = this->parse_expression_list();
         if (not parsed_args.ok()) {
            return ParserResult<StmtNodePtr>::failure(parsed_args.error_ref());
         }
         args = std::move(parsed_args.value_ref());
      }
      this->ctx.consume(TokenKind::RightParen, ParserErrorCode::ExpectedToken);
   }

   StmtNodePtr stmt = std::make_unique<StmtNode>();
   stmt->kind = AstNodeKind::DeferStmt;
   stmt->span = token.span();
   DeferStmtPayload payload;
   payload.callable = make_function_payload(std::move(param_info.parameters), param_info.is_vararg,
      std::move(body.value_ref()));
   payload.arguments = std::move(args);
   stmt->data = std::move(payload);
   return ParserResult<StmtNodePtr>::success(std::move(stmt));
}

ParserResult<StmtNodePtr> AstBuilder::parse_return()
{
   Token token = this->ctx.tokens().current();
   this->ctx.tokens().advance();
   ExprNodeList values;
   bool forwards_call = false;
   if (not this->ctx.check(TokenKind::EndToken) and not this->ctx.check(TokenKind::Else) and
      not this->ctx.check(TokenKind::ElseIf) and not this->ctx.check(TokenKind::Until) and
      not this->ctx.check(TokenKind::EndOfFile) and not this->ctx.check(TokenKind::Semicolon)) {
      auto exprs = this->parse_expression_list();
      if (not exprs.ok()) {
         return ParserResult<StmtNodePtr>::failure(exprs.error_ref());
      }
      if (exprs.value_ref().size() IS 1 and exprs.value_ref()[0]->kind IS AstNodeKind::CallExpr) {
         forwards_call = true;
      }
      values = std::move(exprs.value_ref());
   }
   if (this->ctx.match(TokenKind::Semicolon).ok()) {
      // Optional separator consumed.
   }

   StmtNodePtr stmt = std::make_unique<StmtNode>();
   stmt->kind = AstNodeKind::ReturnStmt;
   stmt->span = token.span();
   ReturnStmtPayload payload;
   payload.values = std::move(values);
   payload.forwards_call = forwards_call;
   stmt->data = std::move(payload);
   return ParserResult<StmtNodePtr>::success(std::move(stmt));
}

ParserResult<StmtNodePtr> AstBuilder::parse_expression_stmt()
{
   auto first = this->parse_expression();
   if (not first.ok()) {
      return ParserResult<StmtNodePtr>::failure(first.error_ref());
   }
   ExprNodeList targets;
   targets.push_back(std::move(first.value_ref()));
   while (this->ctx.match(TokenKind::Comma).ok()) {
      auto extra = this->parse_expression();
      if (not extra.ok()) {
         return ParserResult<StmtNodePtr>::failure(extra.error_ref());
      }
      targets.push_back(std::move(extra.value_ref()));
   }

   Token op = this->ctx.tokens().current();
   AssignmentOperator assignment = AssignmentOperator::Plain;
   bool has_assignment = false;
   switch (op.kind()) {
   case TokenKind::Equals:
      has_assignment = true;
      assignment = AssignmentOperator::Plain;
      break;
   case TokenKind::CompoundAdd:
      has_assignment = true;
      assignment = AssignmentOperator::Add;
      break;
   case TokenKind::CompoundSub:
      has_assignment = true;
      assignment = AssignmentOperator::Subtract;
      break;
   case TokenKind::CompoundMul:
      has_assignment = true;
      assignment = AssignmentOperator::Multiply;
      break;
   case TokenKind::CompoundDiv:
      has_assignment = true;
      assignment = AssignmentOperator::Divide;
      break;
   case TokenKind::CompoundMod:
      has_assignment = true;
      assignment = AssignmentOperator::Modulo;
      break;
   case TokenKind::CompoundConcat:
      has_assignment = true;
      assignment = AssignmentOperator::Concat;
      break;
   case TokenKind::CompoundIfEmpty:
      has_assignment = true;
      assignment = AssignmentOperator::IfEmpty;
      break;
   default:
      break;
   }

   if (has_assignment) {
      this->ctx.tokens().advance();
      auto values = this->parse_expression_list();
      if (not values.ok()) {
         return ParserResult<StmtNodePtr>::failure(values.error_ref());
      }
      StmtNodePtr stmt = std::make_unique<StmtNode>();
      stmt->kind = AstNodeKind::AssignmentStmt;
      stmt->span = op.span();
      AssignmentStmtPayload payload;
      payload.op = assignment;
      payload.targets = std::move(targets);
      payload.values = std::move(values.value_ref());
      stmt->data = std::move(payload);
      return ParserResult<StmtNodePtr>::success(std::move(stmt));
   }

   if (targets.size() > 1) {
      Token bad = this->ctx.tokens().current();
      this->ctx.emit_error(ParserErrorCode::UnexpectedToken, bad,
         "unexpected expression list without assignment");
      ParserError error;
      error.code = ParserErrorCode::UnexpectedToken;
      error.token = bad;
      error.message = "malformed assignment";
      return ParserResult<StmtNodePtr>::failure(error);
   }

   StmtNodePtr stmt = std::make_unique<StmtNode>();
   stmt->kind = AstNodeKind::ExpressionStmt;
   stmt->span = targets[0]->span;
   ExpressionStmtPayload payload;
   payload.expression = std::move(targets[0]);
   stmt->data = std::move(payload);
   return ParserResult<StmtNodePtr>::success(std::move(stmt));
}

ParserResult<ExprNodePtr> AstBuilder::parse_expression(uint8_t precedence)
{
   auto left = this->parse_unary();
   if (not left.ok()) {
      return left;
   }

   while (true) {
      Token next = this->ctx.tokens().current();
      if (next.kind() IS TokenKind::Question) {
         this->ctx.tokens().advance();
         auto true_branch = this->parse_expression();
         if (not true_branch.ok()) {
            return true_branch;
         }
         this->ctx.consume(TokenKind::TernarySep, ParserErrorCode::ExpectedToken);
         auto false_branch = this->parse_expression();
         if (not false_branch.ok()) {
            return false_branch;
         }
         SourceSpan span = combine_spans(left.value_ref()->span, false_branch.value_ref()->span);
         ExprNodePtr ternary = make_ternary_expr(span,
            std::move(left.value_ref()), std::move(true_branch.value_ref()),
            std::move(false_branch.value_ref()));
         left = ParserResult<ExprNodePtr>::success(std::move(ternary));
         continue;
      }

      auto op_info = this->match_binary_operator(next);
      if (not op_info.has_value()) break;
      if (op_info->left <= precedence) break;
      this->ctx.tokens().advance();
      auto right = this->parse_expression(op_info->right);
      if (not right.ok()) {
         return right;
      }
      SourceSpan span = combine_spans(left.value_ref()->span, right.value_ref()->span);
      left = ParserResult<ExprNodePtr>::success(make_binary_expr(span,
         op_info->op, std::move(left.value_ref()), std::move(right.value_ref())));
   }

   return left;
}

ParserResult<ExprNodePtr> AstBuilder::parse_unary()
{
   Token current = this->ctx.tokens().current();
   if (current.kind() IS TokenKind::NotToken) {
      this->ctx.tokens().advance();
      auto operand = this->parse_unary();
      if (not operand.ok()) {
         return operand;
      }
      return ParserResult<ExprNodePtr>::success(make_unary_expr(current.span(),
         AstUnaryOperator::Not, std::move(operand.value_ref())));
   }
   if (current.kind() IS TokenKind::Minus) {
      this->ctx.tokens().advance();
      auto operand = this->parse_unary();
      if (not operand.ok()) {
         return operand;
      }
      return ParserResult<ExprNodePtr>::success(make_unary_expr(current.span(),
         AstUnaryOperator::Negate, std::move(operand.value_ref())));
   }
   if (current.raw() IS '#') {
      this->ctx.tokens().advance();
      auto operand = this->parse_unary();
      if (not operand.ok()) {
         return operand;
      }
      return ParserResult<ExprNodePtr>::success(make_unary_expr(current.span(),
         AstUnaryOperator::Length, std::move(operand.value_ref())));
   }
   if (current.raw() IS '~') {
      this->ctx.tokens().advance();
      auto operand = this->parse_unary();
      if (not operand.ok()) {
         return operand;
      }
      return ParserResult<ExprNodePtr>::success(make_unary_expr(current.span(),
         AstUnaryOperator::BitNot, std::move(operand.value_ref())));
   }
   if (current.kind() IS TokenKind::PlusPlus) {
      this->ctx.tokens().advance();
      auto operand = this->parse_unary();
      if (not operand.ok()) {
         return operand;
      }
      return ParserResult<ExprNodePtr>::success(make_update_expr(current.span(),
         AstUpdateOperator::Increment, false, std::move(operand.value_ref())));
   }
   return this->parse_primary();
}

ParserResult<ExprNodePtr> AstBuilder::parse_primary()
{
   Token current = this->ctx.tokens().current();
   ExprNodePtr node;
   switch (current.kind()) {
   case TokenKind::Number:
   case TokenKind::String:
   case TokenKind::Nil:
   case TokenKind::TrueToken:
   case TokenKind::FalseToken:
      node = make_literal_expr(current.span(), make_literal(current));
      this->ctx.tokens().advance();
      break;
   case TokenKind::Identifier: {
      Identifier id = make_identifier(current);
      NameRef name;
      name.identifier = id;
      node = make_identifier_expr(current.span(), name);
      this->ctx.tokens().advance();
      break;
   }
   case TokenKind::Dots:
      node = make_vararg_expr(current.span());
      this->ctx.tokens().advance();
      break;
   case TokenKind::Function: {
      auto fn = this->parse_function_literal();
      if (not fn.ok()) {
         return fn;
      }
      node = std::move(fn.value_ref());
      break;
   }
   case TokenKind::LeftBrace: {
      auto table = this->parse_table_literal();
      if (not table.ok()) {
         return table;
      }
      node = std::move(table.value_ref());
      break;
   }
   case TokenKind::LeftParen: {
      this->ctx.tokens().advance();
      auto expr = this->parse_expression();
      if (not expr.ok()) {
         return expr;
      }
      this->ctx.consume(TokenKind::RightParen, ParserErrorCode::ExpectedToken);
      node = std::move(expr.value_ref());
      break;
   }
   default:
      this->ctx.emit_error(ParserErrorCode::UnexpectedToken, current, "expected expression");
      ParserError error;
      error.code = ParserErrorCode::UnexpectedToken;
      error.token = current;
      error.message = "expected expression";
      return ParserResult<ExprNodePtr>::failure(error);
   }
   return this->parse_suffixed(std::move(node));
}

ParserResult<ExprNodePtr> AstBuilder::parse_suffixed(ExprNodePtr base)
{
   while (true) {
      Token token = this->ctx.tokens().current();
      if (token.kind() IS TokenKind::Dot) {
         this->ctx.tokens().advance();
         auto name_token = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
         if (not name_token.ok()) {
            return ParserResult<ExprNodePtr>::failure(name_token.error_ref());
         }
         base = make_member_expr(span_from(token, name_token.value_ref()), std::move(base),
            make_identifier(name_token.value_ref()), false);
         continue;
      }
      if (token.kind() IS TokenKind::LeftBracket) {
         this->ctx.tokens().advance();
         auto index = this->parse_expression();
         if (not index.ok()) {
            return index;
         }
         this->ctx.consume(TokenKind::RightBracket, ParserErrorCode::ExpectedToken);
         SourceSpan span = combine_spans(base->span, index.value_ref()->span);
         base = make_index_expr(span, std::move(base), std::move(index.value_ref()));
         continue;
      }
      if (token.kind() IS TokenKind::Colon) {
         this->ctx.tokens().advance();
         auto name_token = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
         if (not name_token.ok()) {
            return ParserResult<ExprNodePtr>::failure(name_token.error_ref());
         }
         bool forwards = false;
         auto args = this->parse_call_arguments(&forwards);
         if (not args.ok()) {
            return ParserResult<ExprNodePtr>::failure(args.error_ref());
         }
         SourceSpan span = combine_spans(base->span, name_token.value_ref().span());
         base = make_method_call_expr(span, std::move(base),
            make_identifier(name_token.value_ref()), std::move(args.value_ref()), forwards);
         continue;
      }
      if (token.kind() IS TokenKind::LeftParen or token.kind() IS TokenKind::LeftBrace or
         token.kind() IS TokenKind::String) {
         bool forwards = false;
         auto args = this->parse_call_arguments(&forwards);
         if (not args.ok()) {
            return ParserResult<ExprNodePtr>::failure(args.error_ref());
         }
         SourceSpan span = combine_spans(base->span, token.span());
         base = make_call_expr(span, std::move(base),
            std::move(args.value_ref()), forwards);
         continue;
      }
      if (token.kind() IS TokenKind::PlusPlus) {
         this->ctx.tokens().advance();
         base = make_update_expr(token.span(), AstUpdateOperator::Increment, true, std::move(base));
         continue;
      }
      if (token.kind() IS TokenKind::Presence and this->ctx.lex().should_emit_presence()) {
         this->ctx.tokens().advance();
         base = make_presence_expr(token.span(), std::move(base));
         continue;
      }
      break;
   }
   return ParserResult<ExprNodePtr>::success(std::move(base));
}

ParserResult<ExprNodePtr> AstBuilder::parse_function_literal()
{
   Token token = this->ctx.tokens().current();
   this->ctx.tokens().advance();
   auto params = this->parse_parameter_list(false);
   if (not params.ok()) {
      return ParserResult<ExprNodePtr>::failure(params.error_ref());
   }
   const TokenKind terms[] = { TokenKind::EndToken };
   auto body = this->parse_block(terms);
   if (not body.ok()) {
      return ParserResult<ExprNodePtr>::failure(body.error_ref());
   }
   this->ctx.consume(TokenKind::EndToken, ParserErrorCode::ExpectedToken);
   ExprNodePtr node = make_function_expr(token.span(), std::move(params.value_ref().parameters),
      params.value_ref().is_vararg, std::move(body.value_ref()));
   return ParserResult<ExprNodePtr>::success(std::move(node));
}

ParserResult<ExprNodePtr> AstBuilder::parse_table_literal()
{
   Token token = this->ctx.tokens().current();
   this->ctx.tokens().advance();
   bool has_array = false;
   auto fields = this->parse_table_fields(&has_array);
   if (not fields.ok()) {
      return ParserResult<ExprNodePtr>::failure(fields.error_ref());
   }
   this->ctx.consume(TokenKind::RightBrace, ParserErrorCode::ExpectedToken);
   ExprNodePtr node = make_table_expr(token.span(), std::move(fields.value_ref()), has_array);
   return ParserResult<ExprNodePtr>::success(std::move(node));
}

ParserResult<ExprNodeList> AstBuilder::parse_expression_list()
{
   ExprNodeList nodes;
   auto first = this->parse_expression();
   if (not first.ok()) {
      return ParserResult<ExprNodeList>::failure(first.error_ref());
   }
   nodes.push_back(std::move(first.value_ref()));
   while (this->ctx.match(TokenKind::Comma).ok()) {
      auto next = this->parse_expression();
      if (not next.ok()) {
         return ParserResult<ExprNodeList>::failure(next.error_ref());
      }
      nodes.push_back(std::move(next.value_ref()));
   }
   return ParserResult<ExprNodeList>::success(std::move(nodes));
}

ParserResult<std::vector<Identifier>> AstBuilder::parse_name_list()
{
   std::vector<Identifier> names;
   auto first = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
   if (not first.ok()) {
      return ParserResult<std::vector<Identifier>>::failure(first.error_ref());
   }
   names.push_back(make_identifier(first.value_ref()));
   while (this->ctx.match(TokenKind::Comma).ok()) {
      auto name = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
      if (not name.ok()) {
         return ParserResult<std::vector<Identifier>>::failure(name.error_ref());
      }
      names.push_back(make_identifier(name.value_ref()));
   }
   return ParserResult<std::vector<Identifier>>::success(std::move(names));
}

ParserResult<AstBuilder::ParameterListResult> AstBuilder::parse_parameter_list(bool allow_optional)
{
   ParameterListResult result;
   if (allow_optional and not this->ctx.check(TokenKind::LeftParen)) {
      return ParserResult<ParameterListResult>::success(result);
   }
   this->ctx.consume(TokenKind::LeftParen, ParserErrorCode::ExpectedToken);
   if (not this->ctx.check(TokenKind::RightParen)) {
      do {
         if (this->ctx.check(TokenKind::Dots)) {
            this->ctx.tokens().advance();
            result.is_vararg = true;
            break;
         }
         auto name = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
         if (not name.ok()) {
            return ParserResult<ParameterListResult>::failure(name.error_ref());
         }
         FunctionParameter param;
         param.name = make_identifier(name.value_ref());
         result.parameters.push_back(param);
      } while (this->ctx.match(TokenKind::Comma).ok());
   }
   this->ctx.consume(TokenKind::RightParen, ParserErrorCode::ExpectedToken);
   return ParserResult<ParameterListResult>::success(std::move(result));
}

ParserResult<std::vector<TableField>> AstBuilder::parse_table_fields(bool* has_array_part)
{
   std::vector<TableField> fields;
   bool array = false;
   while (not this->ctx.check(TokenKind::RightBrace)) {
      TableField field;
      Token current = this->ctx.tokens().current();
      if (current.kind() IS TokenKind::LeftBracket) {
         this->ctx.tokens().advance();
         auto key = this->parse_expression();
         if (not key.ok()) {
            return ParserResult<std::vector<TableField>>::failure(key.error_ref());
         }
         this->ctx.consume(TokenKind::RightBracket, ParserErrorCode::ExpectedToken);
         this->ctx.consume(TokenKind::Equals, ParserErrorCode::ExpectedToken);
         auto value = this->parse_expression();
         if (not value.ok()) {
            return ParserResult<std::vector<TableField>>::failure(value.error_ref());
         }
         field.kind = TableFieldKind::Computed;
         field.key = std::move(key.value_ref());
         field.value = std::move(value.value_ref());
      }
      else if (current.kind() IS TokenKind::Identifier and this->ctx.tokens().peek(1).kind() IS TokenKind::Equals) {
         this->ctx.tokens().advance();
         this->ctx.tokens().advance();
         auto value = this->parse_expression();
         if (not value.ok()) {
            return ParserResult<std::vector<TableField>>::failure(value.error_ref());
         }
         field.kind = TableFieldKind::Record;
         field.name = make_identifier(current);
         field.value = std::move(value.value_ref());
      }
      else {
         auto value = this->parse_expression();
         if (not value.ok()) {
            return ParserResult<std::vector<TableField>>::failure(value.error_ref());
         }
         field.kind = TableFieldKind::Array;
         field.value = std::move(value.value_ref());
         array = true;
      }
      field.span = current.span();
      fields.push_back(std::move(field));
      if (this->ctx.match(TokenKind::Comma).ok()) continue;
      if (this->ctx.match(TokenKind::Semicolon).ok()) continue;
   }
   if (has_array_part) *has_array_part = array;
   return ParserResult<std::vector<TableField>>::success(std::move(fields));
}

ParserResult<ExprNodeList> AstBuilder::parse_call_arguments(bool* forwards_multret)
{
   ExprNodeList args;
   *forwards_multret = false;
   if (this->ctx.check(TokenKind::LeftParen)) {
      this->ctx.tokens().advance();
      if (not this->ctx.check(TokenKind::RightParen)) {
         auto parsed = this->parse_expression_list();
         if (not parsed.ok()) {
            return ParserResult<ExprNodeList>::failure(parsed.error_ref());
         }
         args = std::move(parsed.value_ref());
      }
      this->ctx.consume(TokenKind::RightParen, ParserErrorCode::ExpectedToken);
      return ParserResult<ExprNodeList>::success(std::move(args));
   }
   if (this->ctx.check(TokenKind::LeftBrace)) {
      auto table = this->parse_table_literal();
      if (not table.ok()) {
         return ParserResult<ExprNodeList>::failure(table.error_ref());
      }
      args.push_back(std::move(table.value_ref()));
      return ParserResult<ExprNodeList>::success(std::move(args));
   }
   if (this->ctx.check(TokenKind::String)) {
      Token literal = this->ctx.tokens().current();
      args.push_back(make_literal_expr(literal.span(), make_literal(literal)));
      this->ctx.tokens().advance();
      return ParserResult<ExprNodeList>::success(std::move(args));
   }
   Token bad = this->ctx.tokens().current();
   this->ctx.emit_error(ParserErrorCode::UnexpectedToken, bad, "invalid call arguments");
   ParserError error;
   error.code = ParserErrorCode::UnexpectedToken;
   error.token = bad;
   error.message = "invalid call arguments";
   return ParserResult<ExprNodeList>::failure(error);
}

ParserResult<std::unique_ptr<BlockStmt>> AstBuilder::parse_scoped_block(
   std::initializer_list<TokenKind> terminators)
{
   std::vector<TokenKind> merged(terminators);
   merged.push_back(TokenKind::EndOfFile);
   return this->parse_block(merged);
}

bool AstBuilder::at_end_of_block(std::span<const TokenKind> terminators) const
{
   TokenKind kind = this->ctx.tokens().current().kind();
   if (kind IS TokenKind::EndOfFile) return true;
   for (TokenKind term : terminators) {
      if (kind IS term) return true;
   }
   return false;
}

bool AstBuilder::is_statement_start(TokenKind kind) const
{
   switch (kind) {
   case TokenKind::Local:
   case TokenKind::Function:
   case TokenKind::If:
   case TokenKind::WhileToken:
   case TokenKind::Repeat:
   case TokenKind::For:
   case TokenKind::DoToken:
   case TokenKind::DeferToken:
   case TokenKind::ReturnToken:
   case TokenKind::BreakToken:
   case TokenKind::ContinueToken:
      return true;
   default:
      return false;
   }
}

Identifier AstBuilder::make_identifier(const Token& token)
{
   Identifier id;
   id.symbol = token.identifier();
   id.span = token.span();
   id.is_blank = (id.symbol == NAME_BLANK);
   return id;
}

LiteralValue AstBuilder::make_literal(const Token& token)
{
   LiteralValue literal;
   switch (token.kind()) {
   case TokenKind::Number:
      literal.kind = LiteralKind::Number;
      literal.number_value = token.payload().as_number();
      break;
   case TokenKind::String:
      literal.kind = LiteralKind::String;
      literal.string_value = token.payload().as_string();
      break;
   case TokenKind::Nil:
      literal.kind = LiteralKind::Nil;
      break;
   case TokenKind::TrueToken:
      literal.kind = LiteralKind::Boolean;
      literal.bool_value = true;
      break;
   case TokenKind::FalseToken:
      literal.kind = LiteralKind::Boolean;
      literal.bool_value = false;
      break;
   default:
      break;
   }
   return literal;
}

SourceSpan AstBuilder::span_from(const Token& token)
{
   return token.span();
}

SourceSpan AstBuilder::span_from(const Token& start, const Token& end) const
{
   return combine_spans(start.span(), end.span());
}

std::optional<AstBuilder::BinaryOpInfo> AstBuilder::match_binary_operator(const Token& token) const
{
   BinaryOpInfo info;
   switch (token.kind()) {
   case TokenKind::Plus:
      info.op = AstBinaryOperator::Add;
      info.left = 6;
      info.right = 6;
      return info;
   case TokenKind::Minus:
      info.op = AstBinaryOperator::Subtract;
      info.left = 6;
      info.right = 6;
      return info;
   case TokenKind::Multiply:
      info.op = AstBinaryOperator::Multiply;
      info.left = 7;
      info.right = 7;
      return info;
   case TokenKind::Divide:
      info.op = AstBinaryOperator::Divide;
      info.left = 7;
      info.right = 7;
      return info;
   case TokenKind::Modulo:
      info.op = AstBinaryOperator::Modulo;
      info.left = 7;
      info.right = 7;
      return info;
   case TokenKind::Cat:
      info.op = AstBinaryOperator::Concat;
      info.left = 5;
      info.right = 4;
      return info;
   case TokenKind::Equal:
   case TokenKind::IsToken:
      info.op = AstBinaryOperator::Equal;
      info.left = 3;
      info.right = 3;
      return info;
   case TokenKind::NotEqual:
      info.op = AstBinaryOperator::NotEqual;
      info.left = 3;
      info.right = 3;
      return info;
   case TokenKind::LessEqual:
      info.op = AstBinaryOperator::LessEqual;
      info.left = 3;
      info.right = 3;
      return info;
   case TokenKind::GreaterEqual:
      info.op = AstBinaryOperator::GreaterEqual;
      info.left = 3;
      info.right = 3;
      return info;
   case TokenKind::AndToken:
      info.op = AstBinaryOperator::LogicalAnd;
      info.left = 2;
      info.right = 2;
      return info;
   case TokenKind::OrToken:
      info.op = AstBinaryOperator::LogicalOr;
      info.left = 1;
      info.right = 1;
      return info;
   case TokenKind::Presence:
      info.op = AstBinaryOperator::IfEmpty;
      info.left = 1;
      info.right = 1;
      return info;
   case TokenKind::ShiftLeft:
      info.op = AstBinaryOperator::ShiftLeft;
      info.left = 7;
      info.right = 5;
      return info;
   case TokenKind::ShiftRight:
      info.op = AstBinaryOperator::ShiftRight;
      info.left = 7;
      info.right = 5;
      return info;
   default:
      break;
   }
   if (token.raw() IS '^') {
      info.op = AstBinaryOperator::Power;
      info.left = 10;
      info.right = 9;
      return info;
   }
   if (token.raw() IS '<') {
      info.op = AstBinaryOperator::LessThan;
      info.left = 3;
      info.right = 3;
      return info;
   }
   if (token.raw() IS '>') {
      info.op = AstBinaryOperator::GreaterThan;
      info.left = 3;
      info.right = 3;
      return info;
   }
   if (token.raw() IS '&') {
      info.op = AstBinaryOperator::BitAnd;
      info.left = 5;
      info.right = 4;
      return info;
   }
   if (token.raw() IS '|') {
      info.op = AstBinaryOperator::BitOr;
      info.left = 3;
      info.right = 2;
      return info;
   }
   if (token.raw() IS '~') {
      info.op = AstBinaryOperator::BitXor;
      info.left = 4;
      info.right = 3;
      return info;
   }
   return std::nullopt;
}


