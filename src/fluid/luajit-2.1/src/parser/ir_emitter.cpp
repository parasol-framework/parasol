#include "parser/ir_emitter.h"

IrEmitter::IrEmitter(ParserContext& Context)
   : context(&Context)
{
}

ParserResult<ExpDesc> IrEmitter::emit_primary_expression(const AstPrimaryExpression& Expression)
{
   lj_assertX(this->context != nullptr, "IR emitter requires a parser context");
   ExpDesc result{};
   auto& prefix = Expression.prefix;
   switch (prefix.kind) {
   case AstPrimaryPrefixKind::Identifier: {
      GCstr* name = prefix.token.identifier();
      if (!name)
         name = NAME_BLANK;
      this->context->lex().var_lookup_named(name, &result);
      break;
   }
   default: {
      ParserError error;
      error.code = ParserErrorCode::InternalError;
      error.message = "unsupported primary expression";
      error.token = prefix.token;
      this->context->emit_error(error.code, error.message, prefix.token);
      return ParserResult<ExpDesc>::failure(error);
   }
   }
   return ParserResult<ExpDesc>::success(result);
}

