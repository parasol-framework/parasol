// AST to bytecode emitter entry points.

#pragma once

#include "parser/parser_ast.h"
#include "parser/parser_context.h"
#include "parser/parse_types.h"

class IrEmitter {
public:
   explicit IrEmitter(ParserContext& Context);

   ParserResult<ExpDesc> emit_primary_expression(const AstPrimaryExpression& Expression);

private:
   ParserContext* context = nullptr;
};

