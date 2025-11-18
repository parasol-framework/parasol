#pragma once

#include "parser/ast_nodes.h"
#include "parser/parser_context.h"

ParserResult<ExprNodePtr> parse_expression_ast(ParserContext &, uint8_t = 0);
ParserResult<ExprNodeList> parse_expression_list_ast(ParserContext &);
