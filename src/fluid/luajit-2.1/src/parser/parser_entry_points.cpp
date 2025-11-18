#include "parser/parser_entry_points.h"

#include "parser/ast_builder.h"

/*
TODO: Consider deprecating?

The wrapper functions in parse_expression_ast and parse_expression_list_ast add little value over calling AstBuilder::parse_expression_entry 
and AstBuilder::parse_expression_list_entry directly. They simply create an AstBuilder, call the method, and rewrap the result in the 
same ParserResult type. Consider either:

Removing these wrappers and having callers use AstBuilder directly, or
Adding meaningful logic (e.g., initialization, cleanup, validation) to justify the wrapper layer.
*/

[[maybe_unused]] inline ParserResult<ExprNodePtr> parse_expression_ast(ParserContext& context, uint8_t precedence)
{
   AstBuilder builder(context);
   auto expression = builder.parse_expression_entry(precedence);
   if (not expression.ok()) return ParserResult<ExprNodePtr>::failure(expression.error_ref());
   ExprNodePtr node = std::move(expression.value_ref());
   return ParserResult<ExprNodePtr>::success(std::move(node));
}

[[maybe_unused]] inline ParserResult<ExprNodeList> parse_expression_list_ast(ParserContext& context)
{
   AstBuilder builder(context);
   auto list = builder.parse_expression_list_entry();
   if (not list.ok()) return ParserResult<ExprNodeList>::failure(list.error_ref());
   ExprNodeList nodes = std::move(list.value_ref());
   return ParserResult<ExprNodeList>::success(std::move(nodes));
}
