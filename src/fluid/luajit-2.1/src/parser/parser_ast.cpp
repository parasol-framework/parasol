#include "parser/parser_ast.h"

AstBuilder::AstBuilder(ParserContext& Context)
   : context(&Context)
{
}

ParserResult<AstPrimaryExpression> AstBuilder::parse_primary_expression()
{
   lj_assertX(this->context != nullptr, "AST builder requires a parser context");
   auto identifier = this->context->expect_identifier(ParserErrorCode::IdentifierExpected);
   if (!identifier) {
      ParserError error = identifier.get_error();
      return ParserResult<AstPrimaryExpression>::failure(error);
   }

   AstPrimaryExpression expression;
   expression.prefix.kind = AstPrimaryPrefixKind::Identifier;
   expression.prefix.token = identifier.get();
   return ParserResult<AstPrimaryExpression>::success(expression);
}

ParserResult<AstLocalStatement> AstBuilder::parse_local_statement()
{
   lj_assertX(this->context != nullptr, "AST builder requires a parser context");
   auto local_token = this->context->consume(TokenKind::ReservedLocal, ParserErrorCode::UnexpectedToken);
   if (!local_token) {
      ParserError error = local_token.get_error();
      return ParserResult<AstLocalStatement>::failure(error);
   }

   AstLocalStatement statement;
   statement.local_token = local_token.get();

   for (;;) {
      auto name_token = this->context->expect_identifier(ParserErrorCode::IdentifierExpected);
      if (!name_token) {
         ParserError error = name_token.get_error();
         return ParserResult<AstLocalStatement>::failure(error);
      }
      statement.bindings.push_back({name_token.get()});
      if (!this->context->match(TokenKind::Comma))
         break;
   }

   statement.has_initializer = this->context->match(TokenKind::Equal);
   return ParserResult<AstLocalStatement>::success(statement);
}

