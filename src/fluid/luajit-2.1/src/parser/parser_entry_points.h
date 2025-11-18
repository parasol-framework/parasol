#pragma once

#include "parser/ast_nodes.h"
#include "parser/parser_context.h"

ParserResult<ExprNodePtr> parse_expression_ast(ParserContext& context, uint8_t precedence = 0);
ParserResult<ExprNodeList> parse_expression_list_ast(ParserContext& context);
