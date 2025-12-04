// Copyright (C) 2025 Paul Manias

#include "parser/ast_builder.h"

#include <format>
#include <utility>

#include "parser/token_types.h"
#include "parser/parse_types.h"
#include "runtime/lj_str.h"

// Extracts the function payload from an expression node if it's a function expression, otherwise returns null.

static FunctionExprPayload* function_payload_from(ExprNode &Node)
{
   if (Node.kind != AstNodeKind::FunctionExpr) return nullptr;
   return std::get_if<FunctionExprPayload>(&Node.data);
}

// Moves the function payload data out of an expression node, transferring ownership of parameters and body.

static std::unique_ptr<FunctionExprPayload> move_function_payload(ExprNodePtr &Node)
{
   auto *payload = function_payload_from(*Node);
   if (payload IS nullptr) return std::make_unique<FunctionExprPayload>();

   std::unique_ptr<FunctionExprPayload> result = std::make_unique<FunctionExprPayload>();
   result->parameters = std::move(payload->parameters);
   result->is_vararg = payload->is_vararg;
   result->body = std::move(payload->body);
   return result;
}

// Checks if a token kind is a statement keyword that can be used in conditional shorthand syntax (e.g., value ?? return).

static bool is_shorthand_statement_keyword(TokenKind Kind)
{
   switch (Kind) {
      case TokenKind::ReturnToken:
      case TokenKind::BreakToken:
      case TokenKind::ContinueToken:
         return true;
      default:
         return false;
   }
}

// Checks if an expression node is a presence check expression (the ?? operator).

static bool is_presence_expr(const ExprNodePtr &Expr)
{
   return Expr and Expr->kind IS AstNodeKind::PresenceExpr;
}

static ParserResult<StmtNodePtr> make_control_stmt(ParserContext& Context, AstNodeKind Kind, const Token& Token)
{
   StmtNodePtr node = std::make_unique<StmtNode>();
   node->kind = Kind;
   node->span = Token.span();
   if (Kind IS AstNodeKind::BreakStmt) node->data.emplace<BreakStmtPayload>();
   else node->data.emplace<ContinueStmtPayload>();
   Context.tokens().advance();
   return ParserResult<StmtNodePtr>::success(std::move(node));
}

AstBuilder::AstBuilder(ParserContext &Context)
   : ctx(Context)
{
}

//********************************************************************************************************************
// Main entry point for parsing a chunk (entire source file).

ParserResult<std::unique_ptr<BlockStmt>> AstBuilder::parse_chunk()
{
   const TokenKind terms[] = { TokenKind::EndOfFile };
   return this->parse_block(terms);
}

//********************************************************************************************************************
// Parses a block of statements, stopping when a terminator token or end of file is encountered.

ParserResult<std::unique_ptr<BlockStmt>> AstBuilder::parse_block(std::span<const TokenKind> terminators)
{
   std::unique_ptr<BlockStmt> block = std::make_unique<BlockStmt>();
   Token start = this->ctx.tokens().current();
   while (not this->at_end_of_block(terminators)) {
      auto stmt = this->parse_statement();
      if (not stmt.ok()) return ParserResult<std::unique_ptr<BlockStmt>>::failure(stmt.error_ref());
      if (stmt.value_ref()) block->statements.push_back(std::move(stmt.value_ref()));
   }
   Token end = this->ctx.tokens().current();
   block->span = this->span_from(start, end);
   return ParserResult<std::unique_ptr<BlockStmt>>::success(std::move(block));
}

//********************************************************************************************************************
// Parses a single statement by examining the current token and dispatching to the appropriate statement parser.

ParserResult<StmtNodePtr> AstBuilder::parse_statement()
{
   Token current = this->ctx.tokens().current();
   switch (current.kind()) {
      case TokenKind::Local:       return this->parse_local();
      case TokenKind::Function:    return this->parse_function_stmt();
      case TokenKind::If:          return this->parse_if();
      case TokenKind::WhileToken:  return this->parse_while();
      case TokenKind::Repeat:      return this->parse_repeat();
      case TokenKind::For:         return this->parse_for();
      case TokenKind::DoToken:     return this->parse_do();
      case TokenKind::DeferToken:  return this->parse_defer();
      case TokenKind::ReturnToken: return this->parse_return();

      case TokenKind::BreakToken: {
         return make_control_stmt(this->ctx, AstNodeKind::BreakStmt, current);
      }

      case TokenKind::ContinueToken: {
         return make_control_stmt(this->ctx, AstNodeKind::ContinueStmt, current);
      }

      case TokenKind::Semicolon:
         this->ctx.tokens().advance();
         return ParserResult<StmtNodePtr>::success(nullptr);

      default:
         return this->parse_expression_stmt();
   }
}

//********************************************************************************************************************
// Parses local variable declarations and local function statements.

ParserResult<StmtNodePtr> AstBuilder::parse_local()
{
   Token local_token = this->ctx.tokens().current();
   this->ctx.tokens().advance();
   if (this->ctx.check(TokenKind::Function)) {
      Token function_token = this->ctx.tokens().current();
      this->ctx.tokens().advance();
      auto name_token = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
      if (not name_token.ok()) return ParserResult<StmtNodePtr>::failure(name_token.error_ref());
      auto fn = this->parse_function_literal(function_token);
      if (not fn.ok()) return ParserResult<StmtNodePtr>::failure(fn.error_ref());
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
   if (not names.ok()) return ParserResult<StmtNodePtr>::failure(names.error_ref());

   ExprNodeList values;
   if (this->ctx.match(TokenKind::Equals).ok()) {
      auto rhs = this->parse_expression_list();
      if (not rhs.ok()) return ParserResult<StmtNodePtr>::failure(rhs.error_ref());
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

//********************************************************************************************************************
// Parses function declarations, including method definitions with colon syntax.

ParserResult<StmtNodePtr> AstBuilder::parse_function_stmt()
{
   Token func_token = this->ctx.tokens().current();
   this->ctx.tokens().advance();
   FunctionNamePath path;
   auto name_token = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
   if (not name_token.ok()) return ParserResult<StmtNodePtr>::failure(name_token.error_ref());
   path.segments.push_back(make_identifier(name_token.value_ref()));

   bool method = false;
   while (this->ctx.match(TokenKind::Dot).ok()) {
      auto seg = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
      if (not seg.ok()) return ParserResult<StmtNodePtr>::failure(seg.error_ref());
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

   auto fn = this->parse_function_literal(func_token);
   if (not fn.ok()) return ParserResult<StmtNodePtr>::failure(fn.error_ref());
   ExprNodePtr function_expr = std::move(fn.value_ref());

   if (method and path.method.has_value()) {
      auto* payload = function_payload_from(*function_expr);
      FunctionParameter self_param;
      self_param.name = path.method.value();
      self_param.name.symbol = lj_str_newlit(&this->ctx.lua(), "self");
      self_param.name.is_blank = false;
      self_param.is_self = true;
      if (payload) payload->parameters.insert(payload->parameters.begin(), self_param);
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

//********************************************************************************************************************
// Parses if-then-else conditional statements with support for elseif chains.

ParserResult<StmtNodePtr> AstBuilder::parse_if()
{
   Token if_token = this->ctx.tokens().current();
   this->ctx.tokens().advance();
   std::vector<IfClause> clauses;
   auto condition = this->parse_expression();
   if (not condition.ok()) return ParserResult<StmtNodePtr>::failure(condition.error_ref());

   this->ctx.consume(TokenKind::ThenToken, ParserErrorCode::ExpectedToken);
   auto then_block = this->parse_scoped_block({ TokenKind::ElseIf, TokenKind::Else, TokenKind::EndToken });
   if (not then_block.ok()) return ParserResult<StmtNodePtr>::failure(then_block.error_ref());

   IfClause clause;
   clause.condition = std::move(condition.value_ref());
   clause.block = std::move(then_block.value_ref());
   clauses.push_back(std::move(clause));

   while (this->ctx.check(TokenKind::ElseIf)) {
      this->ctx.tokens().advance();
      auto cond = this->parse_expression();
      if (not cond.ok()) return ParserResult<StmtNodePtr>::failure(cond.error_ref());
      this->ctx.consume(TokenKind::ThenToken, ParserErrorCode::ExpectedToken);
      auto block = this->parse_scoped_block({ TokenKind::ElseIf, TokenKind::Else, TokenKind::EndToken });
      if (not block.ok()) return ParserResult<StmtNodePtr>::failure(block.error_ref());
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

//********************************************************************************************************************
// Parses while-do loop statements.

ParserResult<StmtNodePtr> AstBuilder::parse_while()
{
   Token token = this->ctx.tokens().current();
   this->ctx.tokens().advance();
   auto condition = this->parse_expression();
   if (not condition.ok()) return ParserResult<StmtNodePtr>::failure(condition.error_ref());
   this->ctx.consume(TokenKind::DoToken, ParserErrorCode::ExpectedToken);
   auto body = this->parse_scoped_block({ TokenKind::EndToken });
   if (not body.ok()) return ParserResult<StmtNodePtr>::failure(body.error_ref());
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

//********************************************************************************************************************
// Parses repeat-until loop statements.

ParserResult<StmtNodePtr> AstBuilder::parse_repeat()
{
   Token token = this->ctx.tokens().current();
   this->ctx.tokens().advance();
   const TokenKind terms[] = { TokenKind::Until };
   auto body = this->parse_block(terms);
   if (not body.ok()) return ParserResult<StmtNodePtr>::failure(body.error_ref());
   this->ctx.consume(TokenKind::Until, ParserErrorCode::ExpectedToken);
   auto condition = this->parse_expression();
   if (not condition.ok()) return ParserResult<StmtNodePtr>::failure(condition.error_ref());

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

//********************************************************************************************************************
// Parses for loops, handling both numeric (for i=start,stop,step) and generic (for k,v in iterator) forms.

ParserResult<StmtNodePtr> AstBuilder::parse_for()
{
   Token token = this->ctx.tokens().current();
   this->ctx.tokens().advance();
   auto name_token = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
   if (not name_token.ok()) return ParserResult<StmtNodePtr>::failure(name_token.error_ref());

   if (this->ctx.match(TokenKind::Equals).ok()) {
      auto start = this->parse_expression();
      if (not start.ok()) return ParserResult<StmtNodePtr>::failure(start.error_ref());
      this->ctx.consume(TokenKind::Comma, ParserErrorCode::ExpectedToken);
      auto stop = this->parse_expression();
      if (not stop.ok()) return ParserResult<StmtNodePtr>::failure(stop.error_ref());
      ExprNodePtr step_expr;
      if (this->ctx.match(TokenKind::Comma).ok()) {
         auto step = this->parse_expression();
         if (not step.ok()) return ParserResult<StmtNodePtr>::failure(step.error_ref());
         step_expr = std::move(step.value_ref());
      }
      this->ctx.consume(TokenKind::DoToken, ParserErrorCode::ExpectedToken);
      auto body = this->parse_scoped_block({ TokenKind::EndToken });
      if (not body.ok()) return ParserResult<StmtNodePtr>::failure(body.error_ref());
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
      if (not extra.ok()) return ParserResult<StmtNodePtr>::failure(extra.error_ref());
      names.push_back(make_identifier(extra.value_ref()));
   }

   this->ctx.consume(TokenKind::InToken, ParserErrorCode::ExpectedToken);
   auto iterators = this->parse_expression_list();
   if (not iterators.ok()) return ParserResult<StmtNodePtr>::failure(iterators.error_ref());
   this->ctx.consume(TokenKind::DoToken, ParserErrorCode::ExpectedToken);
   auto body = this->parse_scoped_block({ TokenKind::EndToken });
   if (not body.ok()) return ParserResult<StmtNodePtr>::failure(body.error_ref());
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

//********************************************************************************************************************
// Parses do-end block statements that create a new scope.

ParserResult<StmtNodePtr> AstBuilder::parse_do()
{
   Token token = this->ctx.tokens().current();
   this->ctx.tokens().advance();
   const TokenKind terms[] = { TokenKind::EndToken };
   auto block = this->parse_block(terms);
   if (not block.ok()) return ParserResult<StmtNodePtr>::failure(block.error_ref());

   this->ctx.consume(TokenKind::EndToken, ParserErrorCode::ExpectedToken);

   StmtNodePtr stmt = std::make_unique<StmtNode>();
   stmt->kind = AstNodeKind::DoStmt;
   stmt->span = token.span();

   DoStmtPayload payload;
   payload.block = std::move(block.value_ref());
   stmt->data = std::move(payload);
   return ParserResult<StmtNodePtr>::success(std::move(stmt));
}

//********************************************************************************************************************
// Parses defer statements that execute code when the current scope exits.

ParserResult<StmtNodePtr> AstBuilder::parse_defer()
{
   Token token = this->ctx.tokens().current();
   this->ctx.tokens().advance();
   bool has_params = this->ctx.check(TokenKind::LeftParen);
   ParameterListResult param_info;
   if (has_params) {
      auto parsed = this->parse_parameter_list(true);
      if (not parsed.ok()) return ParserResult<StmtNodePtr>::failure(parsed.error_ref());
      param_info = std::move(parsed.value_ref());
   }
   const TokenKind body_terms[] = { TokenKind::EndToken };
   auto body = this->parse_block(body_terms);
   if (not body.ok()) return ParserResult<StmtNodePtr>::failure(body.error_ref());
   this->ctx.consume(TokenKind::EndToken, ParserErrorCode::ExpectedToken);

   ExprNodeList args;
   if (this->ctx.match(TokenKind::LeftParen).ok()) {
      if (not this->ctx.check(TokenKind::RightParen)) {
         auto parsed_args = this->parse_expression_list();
         if (not parsed_args.ok()) return ParserResult<StmtNodePtr>::failure(parsed_args.error_ref());
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

//********************************************************************************************************************
// Parse return payload shared by explicit returns and conditional shorthand returns.

ParserResult<ReturnStmtPayload> AstBuilder::parse_return_payload(const Token& return_token, bool same_line_only)
{
   ExprNodeList values;
   bool forwards_call = false;
   Token current = this->ctx.tokens().current();
   bool is_terminator = this->ctx.check(TokenKind::EndToken) or this->ctx.check(TokenKind::Else) or
      this->ctx.check(TokenKind::ElseIf) or this->ctx.check(TokenKind::Until) or
      this->ctx.check(TokenKind::EndOfFile) or this->ctx.check(TokenKind::Semicolon);
   bool same_line = same_line_only
      ? ((current.kind() IS TokenKind::EndOfFile) ? false : (current.span().line IS return_token.span().line))
      : true;
   bool parse_values = not is_terminator and (not same_line_only or same_line);

   if (parse_values) {
      auto exprs = this->parse_expression_list();
      if (not exprs.ok()) return ParserResult<ReturnStmtPayload>::failure(exprs.error_ref());
      if (exprs.value_ref().size() IS 1 and exprs.value_ref()[0]->kind IS AstNodeKind::CallExpr) {
         forwards_call = true;
      }
      values = std::move(exprs.value_ref());
   }

   if (this->ctx.match(TokenKind::Semicolon).ok()) {
      // Optional separator consumed.
   }

   ReturnStmtPayload payload;
   payload.values = std::move(values);
   payload.forwards_call = forwards_call;
   return ParserResult<ReturnStmtPayload>::success(std::move(payload));
}

//********************************************************************************************************************
// Parses return statements with optional return values.

ParserResult<StmtNodePtr> AstBuilder::parse_return()
{
   Token token = this->ctx.tokens().current();
   this->ctx.tokens().advance();
   auto payload = this->parse_return_payload(token, false);
   if (not payload.ok()) return ParserResult<StmtNodePtr>::failure(payload.error_ref());

   StmtNodePtr stmt = std::make_unique<StmtNode>();
   stmt->kind = AstNodeKind::ReturnStmt;
   stmt->span = token.span();

   stmt->data = std::move(payload.value_ref());
   return ParserResult<StmtNodePtr>::success(std::move(stmt));
}

//********************************************************************************************************************
// Parses expression statements, handling assignments, compound assignments, conditional shorthands, and standalone expressions.

ParserResult<StmtNodePtr> AstBuilder::parse_expression_stmt()
{
   auto first = this->parse_expression();
   if (not first.ok()) return ParserResult<StmtNodePtr>::failure(first.error_ref());

   ExprNodeList targets;
   targets.push_back(std::move(first.value_ref()));
   while (this->ctx.match(TokenKind::Comma).ok()) {
      auto extra = this->parse_expression();
      if (not extra.ok()) return ParserResult<StmtNodePtr>::failure(extra.error_ref());
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
      if (not values.ok()) return ParserResult<StmtNodePtr>::failure(values.error_ref());
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

   // Conditional shorthand pattern: value ?? return/break/continue
   if (targets.size() IS 1 and is_presence_expr(targets[0])) {
      Token next = this->ctx.tokens().current();
      if (is_shorthand_statement_keyword(next.kind())) {
         auto* presence_payload = std::get_if<PresenceExprPayload>(&targets[0]->data);
         if (presence_payload and presence_payload->value) {
            ExprNodePtr condition = std::move(presence_payload->value);
            StmtNodePtr body;

            if (next.kind() IS TokenKind::ReturnToken) {
               Token return_token = next;
               this->ctx.tokens().advance();

               auto payload = this->parse_return_payload(return_token, true);
               if (not payload.ok()) return ParserResult<StmtNodePtr>::failure(payload.error_ref());

               StmtNodePtr node = std::make_unique<StmtNode>();
               node->kind = AstNodeKind::ReturnStmt;
               node->span = return_token.span();

               node->data = std::move(payload.value_ref());
               body = std::move(node);
            }
            else if (next.kind() IS TokenKind::BreakToken) {
               auto control = make_control_stmt(this->ctx, AstNodeKind::BreakStmt, next);
               if (not control.ok()) return ParserResult<StmtNodePtr>::failure(control.error_ref());
               body = std::move(control.value_ref());
            }
            else if (next.kind() IS TokenKind::ContinueToken) {
               auto control = make_control_stmt(this->ctx, AstNodeKind::ContinueStmt, next);
               if (not control.ok()) return ParserResult<StmtNodePtr>::failure(control.error_ref());
               body = std::move(control.value_ref());
            }

            if (body) {
               SourceSpan span = combine_spans(condition->span, body->span);
               StmtNodePtr stmt = std::make_unique<StmtNode>();
               stmt->kind = AstNodeKind::ConditionalShorthandStmt;
               stmt->span = span;
               ConditionalShorthandStmtPayload payload;
               payload.condition = std::move(condition);
               payload.body = std::move(body);
               stmt->data = std::move(payload);
               return ParserResult<StmtNodePtr>::success(std::move(stmt));
            }
         }
      }
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

//********************************************************************************************************************
// Parses expressions using precedence climbing for binary operators, ternary conditionals, and pipe operators.

ParserResult<ExprNodePtr> AstBuilder::parse_expression(uint8_t precedence)
{
   auto left = this->parse_unary();
   if (not left.ok()) return left;

   while (true) {
      Token next = this->ctx.tokens().current();

      // Pipe operator: precedence 2, right-associative (left=2, right=1)
      // Higher than logical operators, lower than comparison

      if (next.kind() IS TokenKind::Pipe) {
         constexpr uint8_t pipe_left = 2;
         if (pipe_left <= precedence) break;

         // Extract limit from token payload (0 = unlimited)
         uint32_t limit = 0;
         if (next.payload().has_value()) {
            double limit_val = next.payload().as_number();
            if (limit_val >= 1) limit = uint32_t(limit_val);
         }

         this->ctx.tokens().advance();

         // Parse RHS as a primary expression with suffixes (to allow call expressions)

         auto rhs = this->parse_unary();
         if (not rhs.ok()) return rhs;

         // Apply suffixes to get the complete RHS expression

         rhs = this->parse_suffixed(std::move(rhs.value_ref()));
         if (not rhs.ok()) return rhs;

         // Validate that RHS is a call expression

         if (rhs.value_ref()->kind != AstNodeKind::CallExpr and rhs.value_ref()->kind != AstNodeKind::SafeCallExpr) {
            Token bad = this->ctx.tokens().current();
            this->ctx.emit_error(ParserErrorCode::UnexpectedToken, bad, "pipe operator requires function call on right-hand side");
            ParserError error;
            error.code = ParserErrorCode::UnexpectedToken;
            error.token = next;
            error.message = "pipe operator requires function call on right-hand side";
            return ParserResult<ExprNodePtr>::failure(error);
         }

         SourceSpan span = combine_spans(left.value_ref()->span, rhs.value_ref()->span);
         left = ParserResult<ExprNodePtr>::success(make_pipe_expr(span, std::move(left.value_ref()), std::move(rhs.value_ref()), limit));
         continue;
      }

      if (next.kind() IS TokenKind::Question) {
         // Ternary operator has priority 1 (lowest). Only process if current
         // precedence level allows it, otherwise let higher-priority operators
         // complete first (e.g., x > 0 ? ... should parse as (x > 0) ? ...)

         if (1 <= precedence) break;
         this->ctx.tokens().advance();
         auto true_branch = this->parse_expression();
         if (not true_branch.ok()) return true_branch;
         this->ctx.consume(TokenKind::TernarySep, ParserErrorCode::ExpectedToken);
         auto false_branch = this->parse_expression();
         if (not false_branch.ok()) return false_branch;
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
      if (not right.ok()) return right;
      SourceSpan span = combine_spans(left.value_ref()->span, right.value_ref()->span);
      left = ParserResult<ExprNodePtr>::success(make_binary_expr(span, op_info->op, std::move(left.value_ref()), std::move(right.value_ref())));
   }

   return left;
}

//********************************************************************************************************************
// Parses unary expressions (not, negation, length, bit not, prefix increment).

ParserResult<ExprNodePtr> AstBuilder::parse_unary()
{
   Token current = this->ctx.tokens().current();
   if (current.kind() IS TokenKind::NotToken) {
      this->ctx.tokens().advance();
      auto operand = this->parse_unary();
      if (not operand.ok()) return operand;

      return ParserResult<ExprNodePtr>::success(make_unary_expr(current.span(), AstUnaryOperator::Not, std::move(operand.value_ref())));
   }

   if (current.kind() IS TokenKind::Minus) {
      this->ctx.tokens().advance();
      auto operand = this->parse_unary();
      if (not operand.ok()) return operand;

      return ParserResult<ExprNodePtr>::success(make_unary_expr(current.span(), AstUnaryOperator::Negate, std::move(operand.value_ref())));
   }

   if (current.raw() IS '#') {
      this->ctx.tokens().advance();
      auto operand = this->parse_unary();
      if (not operand.ok()) return operand;
      return ParserResult<ExprNodePtr>::success(make_unary_expr(current.span(), AstUnaryOperator::Length, std::move(operand.value_ref())));
   }

   if (current.raw() IS '~') {
      this->ctx.tokens().advance();
      auto operand = this->parse_unary();
      if (not operand.ok()) return operand;

      return ParserResult<ExprNodePtr>::success(make_unary_expr(current.span(), AstUnaryOperator::BitNot, std::move(operand.value_ref())));
   }
   if (current.kind() IS TokenKind::PlusPlus) {
      this->ctx.tokens().advance();
      auto operand = this->parse_unary();
      if (not operand.ok()) return operand;

      return ParserResult<ExprNodePtr>::success(make_update_expr(current.span(), AstUpdateOperator::Increment, false, std::move(operand.value_ref())));
   }
   return this->parse_primary();
}

//********************************************************************************************************************
// Parses primary expressions (literals, identifiers, varargs, functions, tables, parenthesised expressions) and their suffixes.

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
         Token function_token = this->ctx.tokens().current();
         this->ctx.tokens().advance();
         auto fn = this->parse_function_literal(function_token);
         if (not fn.ok()) return fn;

         node = std::move(fn.value_ref());
         break;
      }

      case TokenKind::LeftBrace: {
         auto table = this->parse_table_literal();
         if (not table.ok()) return table;

         node = std::move(table.value_ref());
         break;
      }

      case TokenKind::LeftParen: {
         this->ctx.tokens().advance();
         auto expr = this->parse_expression();
         if (not expr.ok()) return expr;

         this->ctx.consume(TokenKind::RightParen, ParserErrorCode::ExpectedToken);
         node = std::move(expr.value_ref());
         break;
      }

      case TokenKind::LeftBracket: {
         // Result filter prefix syntax: [_*]func()
         return this->parse_result_filter_expr(current);
      }

      case TokenKind::DeferredOpen: {
         // Deferred expression: <{ expr }>
         Token start = this->ctx.tokens().current();
         this->ctx.tokens().advance();
         auto inner = this->parse_expression();
         if (not inner.ok()) return inner;
         if (not this->ctx.match(TokenKind::DeferredClose).ok()) {
            this->ctx.emit_error(ParserErrorCode::ExpectedToken, this->ctx.tokens().current(),
               "Expected '}>' to close deferred expression");
            ParserError error;
            error.code = ParserErrorCode::ExpectedToken;
            error.token = this->ctx.tokens().current();
            error.message = "Expected '}>' to close deferred expression";
            return ParserResult<ExprNodePtr>::failure(error);
         }
         node = make_deferred_expr(span_from(start, this->ctx.tokens().current()), std::move(inner.value_ref()));
         break;
      }

      default: {
         auto msg = std::format("Expected expression, got '{}'", this->ctx.lex().token2str(current.raw()));
         this->ctx.emit_error(ParserErrorCode::UnexpectedToken, current, msg);
         ParserError error;
         error.code = ParserErrorCode::UnexpectedToken;
         error.token = current;
         error.message = msg;
         return ParserResult<ExprNodePtr>::failure(error);
      }
   }
   return this->parse_suffixed(std::move(node));
}

//********************************************************************************************************************
// Parses suffix operations on expressions (field access, indexing, method calls, function calls, postfix increment, presence checks).

ParserResult<ExprNodePtr> AstBuilder::parse_suffixed(ExprNodePtr base)
{
   while (true) {
      Token token = this->ctx.tokens().current();
      if (token.kind() IS TokenKind::Dot) {
         this->ctx.tokens().advance();
         auto name_token = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
         if (not name_token.ok()) return ParserResult<ExprNodePtr>::failure(name_token.error_ref());

         base = make_member_expr(span_from(token, name_token.value_ref()), std::move(base),
            make_identifier(name_token.value_ref()), false);
         continue;
      }

      if (token.kind() IS TokenKind::SafeField) {
         this->ctx.tokens().advance();
         auto name_token = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
         if (not name_token.ok()) return ParserResult<ExprNodePtr>::failure(name_token.error_ref());

         base = make_safe_member_expr(span_from(token, name_token.value_ref()), std::move(base),
            make_identifier(name_token.value_ref()));
         continue;
      }

      if (token.kind() IS TokenKind::LeftBracket) {
         this->ctx.tokens().advance();
         auto index = this->parse_expression();
         if (not index.ok()) return index;

         this->ctx.consume(TokenKind::RightBracket, ParserErrorCode::ExpectedToken);
         SourceSpan span = combine_spans(base->span, index.value_ref()->span);
         base = make_index_expr(span, std::move(base), std::move(index.value_ref()));
         continue;
      }

      if (token.kind() IS TokenKind::SafeIndex) {
         this->ctx.tokens().advance();
         auto index = this->parse_expression();
         if (not index.ok()) return index;

         this->ctx.consume(TokenKind::RightBracket, ParserErrorCode::ExpectedToken);
         SourceSpan span = combine_spans(base->span, index.value_ref()->span);
         base = make_safe_index_expr(span, std::move(base), std::move(index.value_ref()));
         continue;
      }

      if (token.kind() IS TokenKind::Colon) {
         this->ctx.tokens().advance();
         auto name_token = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
         if (not name_token.ok()) return ParserResult<ExprNodePtr>::failure(name_token.error_ref());

         bool forwards = false;
         auto args = this->parse_call_arguments(&forwards);
         if (not args.ok()) return ParserResult<ExprNodePtr>::failure(args.error_ref());

         SourceSpan span = combine_spans(base->span, name_token.value_ref().span());
         base = make_method_call_expr(span, std::move(base),
            make_identifier(name_token.value_ref()), std::move(args.value_ref()), forwards);
         continue;
      }

      if (token.kind() IS TokenKind::SafeMethod) {
         this->ctx.tokens().advance();
         auto name_token = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
         if (not name_token.ok()) return ParserResult<ExprNodePtr>::failure(name_token.error_ref());

         bool forwards = false;
         auto args = this->parse_call_arguments(&forwards);
         if (not args.ok()) return ParserResult<ExprNodePtr>::failure(args.error_ref());

         SourceSpan span = combine_spans(base->span, name_token.value_ref().span());
         base = make_safe_method_call_expr(span, std::move(base),
            make_identifier(name_token.value_ref()), std::move(args.value_ref()), forwards);
         continue;
      }

      if (token.kind() IS TokenKind::LeftParen or token.kind() IS TokenKind::LeftBrace or
         token.kind() IS TokenKind::String) {
         bool forwards = false;
         auto args = this->parse_call_arguments(&forwards);
         if (not args.ok()) return ParserResult<ExprNodePtr>::failure(args.error_ref());
         SourceSpan span = combine_spans(base->span, token.span());
         base = make_call_expr(span, std::move(base), std::move(args.value_ref()), forwards);
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

//********************************************************************************************************************
// Parses function literals (anonymous functions) with parameters and body.

ParserResult<ExprNodePtr> AstBuilder::parse_function_literal(const Token &function_token)
{
   auto params = this->parse_parameter_list(false);
   if (not params.ok()) return ParserResult<ExprNodePtr>::failure(params.error_ref());

   const TokenKind terms[] = { TokenKind::EndToken };
   auto body = this->parse_block(terms);
   if (not body.ok()) return ParserResult<ExprNodePtr>::failure(body.error_ref());

   this->ctx.consume(TokenKind::EndToken, ParserErrorCode::ExpectedToken);
   ExprNodePtr node = make_function_expr(function_token.span(), std::move(params.value_ref().parameters),
      params.value_ref().is_vararg, std::move(body.value_ref()));
   return ParserResult<ExprNodePtr>::success(std::move(node));
}

//********************************************************************************************************************
// Parses table constructor expressions with array and record fields.

ParserResult<ExprNodePtr> AstBuilder::parse_table_literal()
{
   Token token = this->ctx.tokens().current();
   this->ctx.tokens().advance();
   bool has_array = false;
   auto fields = this->parse_table_fields(&has_array);
   if (not fields.ok()) return ParserResult<ExprNodePtr>::failure(fields.error_ref());

   this->ctx.consume(TokenKind::RightBrace, ParserErrorCode::ExpectedToken);
   ExprNodePtr node = make_table_expr(token.span(), std::move(fields.value_ref()), has_array);
   return ParserResult<ExprNodePtr>::success(std::move(node));
}

//********************************************************************************************************************
// Parses comma-separated lists of expressions.

ParserResult<ExprNodeList> AstBuilder::parse_expression_list()
{
   ExprNodeList nodes;
   auto first = this->parse_expression();
   if (not first.ok()) return ParserResult<ExprNodeList>::failure(first.error_ref());

   nodes.push_back(std::move(first.value_ref()));
   while (this->ctx.match(TokenKind::Comma).ok()) {
      auto next = this->parse_expression();
      if (not next.ok()) return ParserResult<ExprNodeList>::failure(next.error_ref());
      nodes.push_back(std::move(next.value_ref()));
   }
   return ParserResult<ExprNodeList>::success(std::move(nodes));
}

// Parses comma-separated lists of identifiers with optional attributes (e.g., <close>).

ParserResult<std::vector<Identifier>> AstBuilder::parse_name_list()
{
   std::vector<Identifier> names;

   auto parse_named_identifier = [&]() -> ParserResult<Identifier> {
      auto token = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
      if (not token.ok()) return ParserResult<Identifier>::failure(token.error_ref());

      Identifier identifier = make_identifier(token.value_ref());

      if (this->ctx.tokens().current().raw() IS '<') {
         this->ctx.tokens().advance();

         auto attribute = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
         if (not attribute.ok()) return ParserResult<Identifier>::failure(attribute.error_ref());

         bool is_close_attribute = false;
         if (GCstr *attr_name = attribute.value_ref().identifier()) {
            std::string_view view(strdata(attr_name), attr_name->len);
            if (view IS std::string_view("close")) {
               is_close_attribute = true;
            }
         }

         if (not this->ctx.lex_opt('>')) {
            Token current = this->ctx.tokens().current();
            std::string message("expected '>' after attribute");
            this->ctx.emit_error(ParserErrorCode::ExpectedToken, current, message);

            ParserError error;
            error.code = ParserErrorCode::ExpectedToken;
            error.token = current;
            error.message = message;
            return ParserResult<Identifier>::failure(error);
         }

         if (is_close_attribute) identifier.has_close = true;
         else this->ctx.emit_error(ParserErrorCode::UnexpectedToken, attribute.value_ref(), "unknown attribute");
      }

      return ParserResult<Identifier>::success(std::move(identifier));
   };

   auto first = parse_named_identifier();
   if (not first.ok()) return ParserResult<std::vector<Identifier>>::failure(first.error_ref());

   names.push_back(std::move(first.value_ref()));
   while (this->ctx.match(TokenKind::Comma).ok()) {
      auto name = parse_named_identifier();
      if (not name.ok()) return ParserResult<std::vector<Identifier>>::failure(name.error_ref());
      names.push_back(std::move(name.value_ref()));
   }
   return ParserResult<std::vector<Identifier>>::success(std::move(names));
}

//********************************************************************************************************************
// Parses function parameter lists with optional type annotations and varargs.

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
         if (not name.ok()) return ParserResult<ParameterListResult>::failure(name.error_ref());

         FunctionParameter param;
         param.name = make_identifier(name.value_ref());

         if (this->ctx.check(TokenKind::Colon)) {
            this->ctx.tokens().advance();

            Token type_token = this->ctx.tokens().current();
            std::string_view type_view;

            auto kind = type_token.kind();
            if (kind IS TokenKind::Identifier) {
               this->ctx.tokens().advance();
               GCstr *type_symbol = type_token.identifier();
               if (type_symbol) type_view = std::string_view(strdata(type_symbol), type_symbol->len);
            }
            else if (kind IS TokenKind::Function or kind IS TokenKind::Nil) {
               this->ctx.tokens().advance();
               type_view = token_kind_name_constexpr(kind);
            }
            else {
               constexpr std::string_view expected_type_name = "Expected type name after ':'";
               this->ctx.emit_error(ParserErrorCode::ExpectedTypeName, type_token, expected_type_name);

               ParserError error;
               error.code = ParserErrorCode::ExpectedTypeName;
               error.token = type_token;
               error.message.assign(expected_type_name.begin(), expected_type_name.end());
               return ParserResult<ParameterListResult>::failure(error);
            }

            param.type = parse_type_name(type_view);
            // If parse_type_name returns an invalid type, emit error
            if (param.type IS FluidType::Unknown) {
               auto message = std::format("Unknown type name '{}'; expected a valid type name", type_view);
               this->ctx.emit_error(ParserErrorCode::UnknownTypeName, type_token, message);

               ParserError error;
               error.code = ParserErrorCode::UnknownTypeName;
               error.token = type_token;
               error.message = message;
               return ParserResult<ParameterListResult>::failure(error);
            }
         }
         result.parameters.push_back(param);
      } while (this->ctx.match(TokenKind::Comma).ok());
   }
   this->ctx.consume(TokenKind::RightParen, ParserErrorCode::ExpectedToken);
   return ParserResult<ParameterListResult>::success(std::move(result));
}

//********************************************************************************************************************
// Parses the fields inside table constructors, distinguishing between array, record, and computed key forms.

ParserResult<std::vector<TableField>> AstBuilder::parse_table_fields(bool *has_array_part)
{
   std::vector<TableField> fields;
   bool array = false;
   while (not this->ctx.check(TokenKind::RightBrace)) {
      TableField field;
      Token current = this->ctx.tokens().current();
      if (current.kind() IS TokenKind::LeftBracket) {
         this->ctx.tokens().advance();
         auto key = this->parse_expression();
         if (not key.ok()) return ParserResult<std::vector<TableField>>::failure(key.error_ref());
         this->ctx.consume(TokenKind::RightBracket, ParserErrorCode::ExpectedToken);
         this->ctx.consume(TokenKind::Equals, ParserErrorCode::ExpectedToken);
         auto value = this->parse_expression();
         if (not value.ok()) return ParserResult<std::vector<TableField>>::failure(value.error_ref());

         field.kind = TableFieldKind::Computed;
         field.key = std::move(key.value_ref());
         field.value = std::move(value.value_ref());
      }
      else if (current.kind() IS TokenKind::Identifier and this->ctx.tokens().peek(1).kind() IS TokenKind::Equals) {
         this->ctx.tokens().advance();
         this->ctx.tokens().advance();
         auto value = this->parse_expression();
         if (not value.ok()) return ParserResult<std::vector<TableField>>::failure(value.error_ref());

         field.kind = TableFieldKind::Record;
         field.name = make_identifier(current);
         field.value = std::move(value.value_ref());
      }
      else {
         auto value = this->parse_expression();
         if (not value.ok()) return ParserResult<std::vector<TableField>>::failure(value.error_ref());

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

//********************************************************************************************************************
// Parses function call arguments, handling parenthesised expressions, table constructors, and string literals.

ParserResult<ExprNodeList> AstBuilder::parse_call_arguments(bool *ForwardsMultret)
{
   ExprNodeList args;
   *ForwardsMultret = false;
   if (this->ctx.check(TokenKind::LeftParen)) {
      this->ctx.tokens().advance();
      if (not this->ctx.check(TokenKind::RightParen)) {
         auto parsed = this->parse_expression_list();
         if (not parsed.ok()) return ParserResult<ExprNodeList>::failure(parsed.error_ref());

         args = std::move(parsed.value_ref());
         if (not args.empty()) {
            const ExprNodePtr& tail = args.back();
            if (tail and (tail->kind IS AstNodeKind::CallExpr or tail->kind IS AstNodeKind::VarArgExpr)) {
               *ForwardsMultret = true;
            }
         }
      }
      this->ctx.consume(TokenKind::RightParen, ParserErrorCode::ExpectedToken);
      return ParserResult<ExprNodeList>::success(std::move(args));
   }

   if (this->ctx.check(TokenKind::LeftBrace)) {
      auto table = this->parse_table_literal();
      if (not table.ok()) return ParserResult<ExprNodeList>::failure(table.error_ref());

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

//********************************************************************************************************************
// Parses a scoped block with a specified set of terminator tokens, automatically adding end-of-file as a terminator.

ParserResult<std::unique_ptr<BlockStmt>> AstBuilder::parse_scoped_block(std::initializer_list<TokenKind> terminators)
{
   std::vector<TokenKind> merged(terminators);
   merged.push_back(TokenKind::EndOfFile);
   return this->parse_block(merged);
}

// Checks if the current token indicates the end of a block by matching against terminator tokens.

bool AstBuilder::at_end_of_block(std::span<const TokenKind> terminators) const
{
   TokenKind kind = this->ctx.tokens().current().kind();
   if (kind IS TokenKind::EndOfFile) return true;
   for (TokenKind term : terminators) {
      if (kind IS term) return true;
   }
   return false;
}

// Checks if a token kind can begin a statement.

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

// Creates an identifier structure from a token, extracting its symbol and source span.

Identifier AstBuilder::make_identifier(const Token &Token)
{
   Identifier id;
   id.symbol   = Token.identifier();
   id.span     = Token.span();
   id.is_blank = id.symbol IS NAME_BLANK;
   return id;
}

// Creates a literal value structure from a token, extracting the appropriate value based on token type.

LiteralValue AstBuilder::make_literal(const Token &token)
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

//********************************************************************************************************************
// Parses the result filter pattern inside brackets: [_*], [*_], [_**_], etc.
// The pattern consists of '_' (drop) and '*' (keep) characters.
// The last character determines the trailing behaviour for excess values.

ParserResult<AstBuilder::ResultFilterInfo> AstBuilder::parse_result_filter_pattern()
{
   ResultFilterInfo info;
   info.keep_mask = 0;
   info.explicit_count = 0;
   info.trailing_keep = false;

   uint8_t position = 0;
   Token current = this->ctx.tokens().current();

   while (current.kind() != TokenKind::RightBracket) {
      if (position >= 64) {
         this->ctx.emit_error(ParserErrorCode::UnexpectedToken, current,
            "result filter pattern too long (max 64 positions)");
         ParserError error;
         error.code = ParserErrorCode::UnexpectedToken;
         error.token = current;
         error.message = "result filter pattern too long (max 64 positions)";
         return ParserResult<ResultFilterInfo>::failure(error);
      }

      if (current.kind() IS TokenKind::Multiply) {  // *
         info.keep_mask |= (1ULL << position);
         info.trailing_keep = true;
         position++;
      }
      else if (current.kind() IS TokenKind::Identifier) {
         // Check for underscore identifier - may contain multiple underscores (e.g. "__")
         GCstr *id = current.identifier();
         if (id) {
            const char* data = strdata(id);
            MSize len = id->len;
            bool all_underscores = true;
            for (MSize i = 0; i < len; i++) {
               if (data[i] != '_') {
                  all_underscores = false;
                  break;
               }
            }
            if (all_underscores and len > 0) {
               // Each underscore counts as one "drop" position
               for (MSize i = 0; i < len; i++) {
                  if (position >= 64) {
                     this->ctx.emit_error(ParserErrorCode::UnexpectedToken, current, "result filter pattern too long (max 64 positions)");
                     ParserError error;
                     error.code = ParserErrorCode::UnexpectedToken;
                     error.token = current;
                     error.message = "result filter pattern too long (max 64 positions)";
                     return ParserResult<ResultFilterInfo>::failure(error);
                  }
                  info.trailing_keep = false;
                  position++;
                  if (position == 64) {
                     this->ctx.emit_error(ParserErrorCode::UnexpectedToken, current, "result filter pattern too long (max 64 positions)");
                     ParserError error;
                     error.code = ParserErrorCode::UnexpectedToken;
                     error.token = current;
                     error.message = "result filter pattern too long (max 64 positions)";
                     return ParserResult<ResultFilterInfo>::failure(error);
                  }
               }
            }
            else {
               this->ctx.emit_error(ParserErrorCode::UnexpectedToken, current, "result filter pattern expects '_' or '*'");
               ParserError error;
               error.code = ParserErrorCode::UnexpectedToken;
               error.token = current;
               error.message = "result filter pattern expects '_' or '*'";
               return ParserResult<ResultFilterInfo>::failure(error);
            }
         }
         else {
            this->ctx.emit_error(ParserErrorCode::UnexpectedToken, current, "result filter pattern expects '_' or '*'");
            ParserError error;
            error.code = ParserErrorCode::UnexpectedToken;
            error.token = current;
            error.message = "result filter pattern expects '_' or '*'";
            return ParserResult<ResultFilterInfo>::failure(error);
         }
      }
      else {
         this->ctx.emit_error(ParserErrorCode::UnexpectedToken, current, "result filter pattern expects '_' or '*'");
         ParserError error;
         error.code = ParserErrorCode::UnexpectedToken;
         error.token = current;
         error.message = "result filter pattern expects '_' or '*'";
         return ParserResult<ResultFilterInfo>::failure(error);
      }

      this->ctx.tokens().advance();
      current = this->ctx.tokens().current();
   }

   info.explicit_count = position;
   return ParserResult<ResultFilterInfo>::success(info);
}

//********************************************************************************************************************
// Parses result filter expressions: [_*]func(), [*_]obj:method(), etc.
// This syntax allows selective extraction of return values from multi-value function calls.

ParserResult<ExprNodePtr> AstBuilder::parse_result_filter_expr(const Token &StartToken)
{
   this->ctx.tokens().advance();  // Consume '['

   auto filter = this->parse_result_filter_pattern();
   if (not filter.ok()) return ParserResult<ExprNodePtr>::failure(filter.error_ref());

   this->ctx.consume(TokenKind::RightBracket, ParserErrorCode::ExpectedToken);

   // Parse the expression to filter (must be followed by a callable)
   auto expr = this->parse_unary();
   if (not expr.ok()) return expr;

   expr = this->parse_suffixed(std::move(expr.value_ref()));
   if (not expr.ok()) return expr;

   // Validate that result is a call expression
   AstNodeKind kind = expr.value_ref()->kind;
   if (kind != AstNodeKind::CallExpr and kind != AstNodeKind::SafeCallExpr) {
      this->ctx.emit_error(ParserErrorCode::UnexpectedToken, StartToken,
         "result filter requires a function call");
      ParserError error;
      error.code = ParserErrorCode::UnexpectedToken;
      error.token = StartToken;
      error.message = "result filter requires a function call";
      return ParserResult<ExprNodePtr>::failure(error);
   }

   // Optimisation: If the filter keeps all values (trailing_keep=true and all explicit
   // positions are kept), skip the filter wrapper entirely. This handles [*], [**], [***], etc.
   // A mask of all 1s up to explicit_count means (1 << count) - 1

   auto &f = filter.value_ref();
   uint64_t all_kept_mask = (f.explicit_count > 0) ? ((1ULL << f.explicit_count) - 1) : 0;
   if (f.trailing_keep and f.keep_mask IS all_kept_mask) {
      return expr;  // No filtering needed - just return the call expression
   }

   SourceSpan span = combine_spans(StartToken.span(), expr.value_ref()->span);
   return ParserResult<ExprNodePtr>::success(
      make_result_filter_expr(span, std::move(expr.value_ref()),
         f.keep_mask, f.explicit_count, f.trailing_keep));
}

//********************************************************************************************************************
// Matches a token to a binary operator and returns its precedence information, or returns empty if not a binary operator.

std::optional<AstBuilder::BinaryOpInfo> AstBuilder::match_binary_operator(const Token &token) const
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
         // Only treat ?? as binary if-empty when lookahead indicates binary usage
         if (not this->ctx.lex().should_emit_presence()) {
            info.op = AstBinaryOperator::IfEmpty;
            info.left = 1;
            info.right = 1;
            return info;
         }
         break;  // Not a binary operator, will be handled as postfix
      case TokenKind::ShiftLeft:
         info.op = AstBinaryOperator::ShiftLeft;
         info.left = 5;   // C precedence: shifts bind looser than +/- (6)
         info.right = 5;  // Left-associative: 1 << 2 << 3 = (1 << 2) << 3
         return info;
      case TokenKind::ShiftRight:
         info.op = AstBinaryOperator::ShiftRight;
         info.left = 5;   // C precedence: shifts bind looser than +/- (6)
         info.right = 5;  // Left-associative
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
      info.left = 4;  // Lower than shifts (5) per C precedence
      info.right = 4;  // Left-associative: a & b & c = (a & b) & c
      return info;
   }

   if (token.raw() IS '|') {
      info.op = AstBinaryOperator::BitOr;
      info.left = 2;  // Lower than XOR (3) per C precedence: AND > XOR > OR
      info.right = 2;  // Left-associative: a | b | c = (a | b) | c
      return info;
   }

   if (token.raw() IS '~') {
      info.op = AstBinaryOperator::BitXor;
      info.left = 3;  // Lower than AND (4) per C precedence: AND > XOR > OR
      info.right = 3;  // Left-associative: a ~ b ~ c = (a ~ b) ~ c
      return info;
   }
   return std::nullopt;
}
