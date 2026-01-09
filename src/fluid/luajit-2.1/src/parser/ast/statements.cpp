// AST Builder - Statement Parsers
// Copyright (C) 2025 Paul Manias
//
// This file contains parsers for statement constructs:
// - Local/global variable declarations
// - Function declarations
// - Control flow statements (if, while, repeat, do)
// - Defer statements
// - Return statements
// - Expression statements

//********************************************************************************************************************
// Parses local variable declarations, local function statements and local thunk function statements.
// Supports both explicit 'local' keyword and implicit local declarations with <const>/<close> attributes.

ParserResult<StmtNodePtr> AstBuilder::parse_local()
{
   Token local_token = this->ctx.tokens().current();
   bool implicit_local = (local_token.kind() IS TokenKind::Identifier);

   if (not implicit_local) {
      this->ctx.tokens().advance();  // Consume the 'local' keyword

      bool is_thunk = false;
      if (this->ctx.check(TokenKind::ThunkToken)) {
         is_thunk = true;
         this->ctx.tokens().advance();
      }

      if (this->ctx.check(TokenKind::Function) or is_thunk) {
         if (not is_thunk) {
            this->ctx.tokens().advance();
         }
         Token function_token = local_token;  // Use local_token as span start
         auto name_token = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
         if (not name_token.ok()) return ParserResult<StmtNodePtr>::failure(name_token.error_ref());
         auto fn = this->parse_function_literal(function_token, is_thunk);
         if (not fn.ok()) return ParserResult<StmtNodePtr>::failure(fn.error_ref());
         ExprNodePtr function_expr = std::move(fn.value_ref());
         auto stmt = std::make_unique<StmtNode>(AstNodeKind::LocalFunctionStmt, this->span_from(local_token, name_token.value_ref()));
         LocalFunctionStmtPayload payload(make_identifier(name_token.value_ref()), move_function_payload(function_expr));
         stmt->data = std::move(payload);
         return ParserResult<StmtNodePtr>::success(std::move(stmt));
      }
   }

   auto names = this->parse_name_list();
   if (not names.ok()) return ParserResult<StmtNodePtr>::failure(names.error_ref());

   ExprNodeList values;
   AssignmentOperator assign_op = AssignmentOperator::Plain;

   // Check for plain = or conditional ?=/??= assignment
   if (this->ctx.match(TokenKind::Equals).ok()) {
      auto rhs = this->parse_expression_list();
      if (not rhs.ok()) return ParserResult<StmtNodePtr>::failure(rhs.error_ref());
      values = std::move(rhs.value_ref());
   }
   else if (this->ctx.match(TokenKind::CompoundIfEmpty).ok()) {
      assign_op = AssignmentOperator::IfEmpty;
      auto rhs = this->parse_expression_list();
      if (not rhs.ok()) return ParserResult<StmtNodePtr>::failure(rhs.error_ref());
      values = std::move(rhs.value_ref());
   }
   else if (this->ctx.match(TokenKind::CompoundIfNil).ok()) {
      assign_op = AssignmentOperator::IfNil;
      auto rhs = this->parse_expression_list();
      if (not rhs.ok()) return ParserResult<StmtNodePtr>::failure(rhs.error_ref());
      values = std::move(rhs.value_ref());
   }

   // Check if any trailing expressions beyond the name count are bare identifiers,
   // and if so, convert them to additional variable names.

   auto& name_list = names.value_ref();
   size_t name_count = name_list.size();

   if (values.size() > name_count) {
      for (size_t i = name_count; i < values.size(); ++i) {
         ExprNodePtr& expr = values[i];
         if (expr and expr->kind IS AstNodeKind::IdentifierExpr) {
            auto* name_ref = std::get_if<NameRef>(&expr->data);
            if (name_ref) { // Convert this identifier expression to a variable name
               name_list.push_back(name_ref->identifier);
            }
         }
         else {
            // Non-identifier expression in trailing position - this is an error
            return this->fail<StmtNodePtr>(ParserErrorCode::ExpectedIdentifier, this->ctx.tokens().current(),
               "expected identifier after values in local declaration");
         }
      }
      // Remove the converted identifiers from the values list
      values.resize(name_count);
   }

   auto stmt = std::make_unique<StmtNode>(AstNodeKind::LocalDeclStmt, local_token.span());
   stmt->data.emplace<LocalDeclStmtPayload>(assign_op, std::move(name_list), std::move(values));
   return ParserResult<StmtNodePtr>::success(std::move(stmt));
}

//********************************************************************************************************************
// Parses global variable declarations, forcing variables to be stored in the global table.

ParserResult<StmtNodePtr> AstBuilder::parse_global()
{
   Token global_token = this->ctx.tokens().current();
   this->ctx.tokens().advance();

   // Handle `global function name()` and `global thunk name()` syntax

   bool is_thunk = false;
   if (this->ctx.check(TokenKind::ThunkToken)) {
      is_thunk = true;
      this->ctx.tokens().advance();
   }

   if (this->ctx.check(TokenKind::Function) or is_thunk) {
      if (not is_thunk) this->ctx.tokens().advance();

      Token function_token = global_token;
      auto name_token = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
      if (not name_token.ok()) return ParserResult<StmtNodePtr>::failure(name_token.error_ref());
      auto fn = this->parse_function_literal(function_token, is_thunk);
      if (not fn.ok()) return ParserResult<StmtNodePtr>::failure(fn.error_ref());
      ExprNodePtr function_expr = std::move(fn.value_ref());

      // Build a FunctionStmt with a simple name path (will store to global)

      auto stmt = std::make_unique<StmtNode>(AstNodeKind::FunctionStmt, this->span_from(global_token, name_token.value_ref()));
      FunctionNamePath name;
      name.segments.push_back(make_identifier(name_token.value_ref()));
      name.is_explicit_global = true;  // Mark as explicitly global
      FunctionStmtPayload payload(std::move(name), move_function_payload(function_expr));
      stmt->data = std::move(payload);
      return ParserResult<StmtNodePtr>::success(std::move(stmt));
   }

   auto names = this->parse_name_list();
   if (not names.ok()) return ParserResult<StmtNodePtr>::failure(names.error_ref());

   ExprNodeList values;
   AssignmentOperator assign_op = AssignmentOperator::Plain;

   // Check for plain = or conditional ?=/??= assignment
   if (this->ctx.match(TokenKind::Equals).ok()) {
      auto rhs = this->parse_expression_list();
      if (not rhs.ok()) return ParserResult<StmtNodePtr>::failure(rhs.error_ref());
      values = std::move(rhs.value_ref());
   }
   else if (this->ctx.match(TokenKind::CompoundIfEmpty).ok()) {
      assign_op = AssignmentOperator::IfEmpty;
      auto rhs = this->parse_expression_list();
      if (not rhs.ok()) return ParserResult<StmtNodePtr>::failure(rhs.error_ref());
      values = std::move(rhs.value_ref());
   }
   else if (this->ctx.match(TokenKind::CompoundIfNil).ok()) {
      assign_op = AssignmentOperator::IfNil;
      auto rhs = this->parse_expression_list();
      if (not rhs.ok()) return ParserResult<StmtNodePtr>::failure(rhs.error_ref());
      values = std::move(rhs.value_ref());
   }

   // Check if any trailing expressions beyond the name count are bare identifiers,
   // and if so, convert them to additional variable names.

   auto &name_list = names.value_ref();
   size_t name_count = name_list.size();

   if (values.size() > name_count) {
      for (size_t i = name_count; i < values.size(); ++i) {
         ExprNodePtr& expr = values[i];
         if (expr and expr->kind IS AstNodeKind::IdentifierExpr) {
            if (auto *name_ref = std::get_if<NameRef>(&expr->data)) {
               // Convert this identifier expression to a variable name
               name_list.push_back(name_ref->identifier);
            }
         }
         else {
            // Non-identifier expression in trailing position - this is an error
            return this->fail<StmtNodePtr>(ParserErrorCode::ExpectedIdentifier, this->ctx.tokens().current(),
               "expected identifier after values in global declaration");
         }
      }
      // Remove the converted identifiers from the values list
      values.resize(name_count);
   }

   auto stmt = std::make_unique<StmtNode>(AstNodeKind::GlobalDeclStmt, global_token.span());
   GlobalDeclStmtPayload payload(assign_op, std::move(name_list), std::move(values));
   stmt->data = std::move(payload);
   return ParserResult<StmtNodePtr>::success(std::move(stmt));
}

//********************************************************************************************************************
// Parses function declarations, including method definitions with colon syntax and thunk functions.

ParserResult<StmtNodePtr> AstBuilder::parse_function_stmt()
{
   Token func_token = this->ctx.tokens().current();
   bool is_thunk = (func_token.kind() IS TokenKind::ThunkToken);
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
      if (is_thunk) {
         return this->fail<StmtNodePtr>(ParserErrorCode::UnexpectedToken, this->ctx.tokens().current(),
            "thunk functions do not support method syntax");
      }
      method = true;
      auto seg = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
      if (not seg.ok()) {
         return ParserResult<StmtNodePtr>::failure(seg.error_ref());
      }
      path.method = make_identifier(seg.value_ref());
   }

   auto fn = this->parse_function_literal(func_token, is_thunk);
   if (not fn.ok()) return ParserResult<StmtNodePtr>::failure(fn.error_ref());
   ExprNodePtr function_expr = std::move(fn.value_ref());

   if (method and path.method.has_value()) {
      auto* payload = function_payload_from(*function_expr);
      FunctionParameter self_param;
      self_param.name = Identifier(&this->ctx.lua(), "self", path.method.value().span);
      self_param.is_self = true;
      if (payload) payload->parameters.insert(payload->parameters.begin(), self_param);
   }

   auto stmt = std::make_unique<StmtNode>(AstNodeKind::FunctionStmt, this->span_from(func_token, name_token.value_ref()));
   FunctionStmtPayload payload(std::move(path), move_function_payload(function_expr));
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

   auto stmt = std::make_unique<StmtNode>(AstNodeKind::IfStmt, if_token.span());
   IfStmtPayload payload(std::move(clauses));
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

   auto stmt = std::make_unique<StmtNode>(AstNodeKind::WhileStmt, token.span());
   LoopStmtPayload payload(LoopStyle::WhileLoop, std::move(condition.value_ref()), std::move(body.value_ref()));
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

   auto stmt = std::make_unique<StmtNode>(AstNodeKind::RepeatStmt, token.span());
   LoopStmtPayload payload(LoopStyle::RepeatUntil, std::move(condition.value_ref()), std::move(body.value_ref()));
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

   auto stmt = std::make_unique<StmtNode>(AstNodeKind::DoStmt, token.span());
   DoStmtPayload payload(std::move(block.value_ref()));
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

   auto stmt = std::make_unique<StmtNode>(AstNodeKind::DeferStmt, token.span());
   DeferStmtPayload payload(make_function_payload(std::move(param_info.parameters), param_info.is_vararg,
      std::move(body.value_ref())), std::move(args));
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

   ReturnStmtPayload payload(std::move(values), forwards_call);
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

   auto stmt = std::make_unique<StmtNode>(AstNodeKind::ReturnStmt, token.span());
   stmt->data = std::move(payload.value_ref());
   return ParserResult<StmtNodePtr>::success(std::move(stmt));
}

//********************************************************************************************************************
// Parses try...except...end exception handling blocks.
//
// Syntax:
//   try
//      <body>
//   except [e] [when { ERR_A, ERR_B } | when ERR_C]
//      <handler>
//   [except ...]
//   end

ParserResult<StmtNodePtr> AstBuilder::parse_try()
{
   Token try_token = this->ctx.tokens().current();
   this->ctx.tokens().advance();  // consume 'try'

   // Parse optional <trace> attribute
   bool enable_trace = false;
   if (this->ctx.tokens().current().raw() IS '<') {
      this->ctx.tokens().advance();  // consume '<'
      auto attribute = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
      if (not attribute.ok()) return ParserResult<StmtNodePtr>::failure(attribute.error_ref());

      if (GCstr *attr_name = attribute.value_ref().identifier()) {
         std::string_view view(strdata(attr_name), attr_name->len);
         if (view IS std::string_view("trace")) {
            enable_trace = true;
         }
         else {
            return this->fail<StmtNodePtr>(ParserErrorCode::UnexpectedToken,
               attribute.value_ref(), "unknown try attribute, expected 'trace'");
         }
      }

      if (not this->ctx.lex_opt('>')) {
         return this->fail<StmtNodePtr>(ParserErrorCode::ExpectedToken,
            this->ctx.tokens().current(), "expected '>' after try attribute");
      }
   }

   // Parse try block body - terminates on 'except', 'success', or 'end'
   const TokenKind try_terms[] = { TokenKind::ExceptToken, TokenKind::SuccessToken, TokenKind::EndToken };
   auto try_body = this->parse_block(try_terms);
   if (not try_body.ok()) return ParserResult<StmtNodePtr>::failure(try_body.error_ref());

   std::vector<ExceptClause> clauses;
   bool has_catch_all = false;

   // Parse except clauses
   while (this->ctx.check(TokenKind::ExceptToken)) {
      if (has_catch_all) {
         // Error: catch-all must be last
         return this->fail<StmtNodePtr>(ParserErrorCode::UnexpectedToken, this->ctx.tokens().current(),
            "catch-all 'except' must be the last clause");
      }

      Token except_token = this->ctx.tokens().current();
      this->ctx.tokens().advance();  // consume 'except'

      ExceptClause clause;
      clause.span = this->ctx.tokens().current().span();

      // Check for optional exception variable
      // Patterns: `except e when ...`, `except e`, `except when ...`, `except`
      // The exception variable must be on the same line as 'except'
      if (this->ctx.check(TokenKind::Identifier)) {
         Token name_token = this->ctx.tokens().current();
         // Only treat as exception variable if on same line as 'except'
         if (name_token.span().line IS except_token.span().line) {
            this->ctx.tokens().advance();
            clause.exception_var = make_identifier(name_token);
         }
      }

      // Optional when clause for filtering
      // Check for unexpected tokens on the same line after 'except [var]' (e.g., 'where' instead of 'when')
      if (this->ctx.check(TokenKind::Identifier)) {
         Token unexpected = this->ctx.tokens().current();
         if (unexpected.span().line IS except_token.span().line) {
            GCstr *ident = unexpected.identifier();
            return this->fail<StmtNodePtr>(ParserErrorCode::UnexpectedToken, unexpected,
               "expected 'when' or newline after 'except', not '" + std::string(strdata(ident), ident->len) + "'");
         }
      }

      if (this->ctx.check(TokenKind::When)) {
         Token when_token = this->ctx.tokens().current();
         this->ctx.tokens().advance();  // consume 'when'

         // Filter code(s) must be on the same line as 'when'
         Token next_token = this->ctx.tokens().current();
         if (next_token.span().line != when_token.span().line) {
            return this->fail<StmtNodePtr>(ParserErrorCode::ExpectedToken, when_token,
               "expected error code(s) after 'when' on the same line");
         }

         // Parse error code filter(s): when ERR_A or when ERR_A, ERR_B
         auto first_code = this->parse_expression();
         if (not first_code.ok()) return ParserResult<StmtNodePtr>::failure(first_code.error_ref());
         clause.filter_codes.push_back(std::move(first_code.value_ref()));

         // Continue parsing comma-separated codes on the same line as 'when'
         while (this->ctx.check(TokenKind::Comma)) {
            Token comma_token = this->ctx.tokens().current();
            if (comma_token.span().line != when_token.span().line) break;
            this->ctx.tokens().advance();  // consume ','

            Token code_token = this->ctx.tokens().current();
            if (code_token.span().line != when_token.span().line) {
               return this->fail<StmtNodePtr>(ParserErrorCode::ExpectedToken, comma_token,
                  "expected error code after ',' on the same line as 'when'");
            }

            auto next_code = this->parse_expression();
            if (not next_code.ok()) return ParserResult<StmtNodePtr>::failure(next_code.error_ref());
            clause.filter_codes.push_back(std::move(next_code.value_ref()));
         }
      }
      else {
         has_catch_all = true;  // No 'when' = catch-all
      }

      // Parse except block body - terminates on next 'except', 'success', or 'end'
      const TokenKind except_terms[] = { TokenKind::ExceptToken, TokenKind::SuccessToken, TokenKind::EndToken };
      auto except_body = this->parse_block(except_terms);
      if (not except_body.ok()) return ParserResult<StmtNodePtr>::failure(except_body.error_ref());
      clause.block = std::move(except_body.value_ref());

      clauses.push_back(std::move(clause));
   }

   // Parse optional success clause
   std::unique_ptr<BlockStmt> success_block;
   if (this->ctx.check(TokenKind::SuccessToken)) {
      this->ctx.tokens().advance();  // consume 'success'

      // Parse success block body - terminates on 'end'
      const TokenKind success_terms[] = { TokenKind::EndToken };
      auto success_body = this->parse_block(success_terms);
      if (not success_body.ok()) return ParserResult<StmtNodePtr>::failure(success_body.error_ref());
      success_block = std::move(success_body.value_ref());
   }

   this->ctx.consume(TokenKind::EndToken, ParserErrorCode::ExpectedToken);

   // Build the statement
   auto stmt = std::make_unique<StmtNode>(AstNodeKind::TryExceptStmt, try_token.span());
   TryExceptPayload payload;
   payload.try_block = std::move(try_body.value_ref());
   payload.except_clauses = std::move(clauses);
   payload.success_block = std::move(success_block);
   payload.enable_trace = enable_trace;
   stmt->data = std::move(payload);

   return ParserResult<StmtNodePtr>::success(std::move(stmt));
}

//********************************************************************************************************************
// Parses raise statements: raise expression [, message]
//
// The raise keyword always triggers an exception with the given error code.

ParserResult<StmtNodePtr> AstBuilder::parse_raise()
{
   Token raise_token = this->ctx.tokens().current();
   this->ctx.tokens().advance();  // consume 'raise'

   // Parse error code expression (required)
   auto error_code = this->parse_expression();
   if (not error_code.ok()) return ParserResult<StmtNodePtr>::failure(error_code.error_ref());

   RaiseStmtPayload payload;
   payload.error_code = std::move(error_code.value_ref());

   // Check for optional message
   if (this->ctx.check(TokenKind::Comma)) {
      this->ctx.tokens().advance();  // consume ','
      auto message = this->parse_expression();
      if (not message.ok()) return ParserResult<StmtNodePtr>::failure(message.error_ref());
      payload.message = std::move(message.value_ref());
   }

   auto stmt = std::make_unique<StmtNode>(AstNodeKind::RaiseStmt, raise_token.span());
   stmt->data = std::move(payload);
   return ParserResult<StmtNodePtr>::success(std::move(stmt));
}

//********************************************************************************************************************
// Parses check statements: check expression
//
// The check keyword raises an exception only if the error code >= ERR::ExceptionThreshold.

ParserResult<StmtNodePtr> AstBuilder::parse_check()
{
   Token check_token = this->ctx.tokens().current();
   this->ctx.tokens().advance();  // consume 'check'

   // Parse error code expression (required)
   auto error_code = this->parse_expression();
   if (not error_code.ok()) return ParserResult<StmtNodePtr>::failure(error_code.error_ref());

   CheckStmtPayload payload;
   payload.error_code = std::move(error_code.value_ref());

   auto stmt = std::make_unique<StmtNode>(AstNodeKind::CheckStmt, check_token.span());
   stmt->data = std::move(payload);
   return ParserResult<StmtNodePtr>::success(std::move(stmt));
}
