#include "parser/ir_emitter.h"

#include <utility>

#include "parser/parse_internal.h"

IrEmitter::IrEmitter(ParserContext& Context)
   : context(&Context)
{
}

ParserResult<ExpressionValue> IrEmitter::emit_primary_expression(const AstPrimaryExpression& Expression)
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
      return ParserResult<ExpressionValue>::failure(error);
   }
   }

   FuncState& func_state = this->context->func();
   for (const auto& suffix : Expression.suffixes) {
      switch (suffix.kind) {
      case AstPrimarySuffixKind::Field: {
         GCstr* field = suffix.token.identifier();
         if (!field)
            field = NAME_BLANK;
         ExpDesc key;
         expr_init(&key, ExpKind::Str, 0);
         key.u.sval = field;
         expr_toanyreg(&func_state, &result);
         expr_index(&func_state, &result, &key);
         break;
      }
      case AstPrimarySuffixKind::PresenceCheck: {
         bcemit_presence_check(&func_state, &result);
         break;
      }
      default: {
         ParserError error;
         error.code = ParserErrorCode::InternalError;
         error.message = "unsupported primary suffix";
         error.token = suffix.token;
         this->context->emit_error(error.code, error.message, suffix.token);
         return ParserResult<ExpressionValue>::failure(error);
      }
      }
   }
   ExpressionValue value(*this->context, result);
   return ParserResult<ExpressionValue>::success(std::move(value));
}

