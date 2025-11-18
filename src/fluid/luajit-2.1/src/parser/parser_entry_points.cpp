#include "parser/parser_entry_points.h"

#include "parser/ast_builder.h"

ParserResult<ExprNodePtr> parse_expression_ast(ParserContext& context, uint8_t precedence)
{
   AstBuilder builder(context);
   auto expression = builder.parse_expression_entry(precedence);
   if (not expression.ok()) {
      return ParserResult<ExprNodePtr>::failure(expression.error_ref());
   }
   ExprNodePtr node = std::move(expression.value_ref());
   return ParserResult<ExprNodePtr>::success(std::move(node));
}

ParserResult<ExprNodeList> parse_expression_list_ast(ParserContext& context)
{
   AstBuilder builder(context);
   auto list = builder.parse_expression_list_entry();
   if (not list.ok()) {
      return ParserResult<ExprNodeList>::failure(list.error_ref());
   }
   ExprNodeList nodes = std::move(list.value_ref());
   return ParserResult<ExprNodeList>::success(std::move(nodes));
}
