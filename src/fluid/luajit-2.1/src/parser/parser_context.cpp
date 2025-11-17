#include "parser/parser_context.h"

#include "lj_parse.h"

ParserAllocator ParserAllocator::from(lua_State* state)
{
   ParserAllocator allocator;
   allocator.userdata = (void*)state;
   return allocator;
}

ParserContext ParserContext::from(LexState& lex_state, FuncState& func_state, ParserAllocator allocator,
   ParserConfig config)
{
   return ParserContext(lex_state, func_state, *lex_state.L, allocator, config);
}

ParserContext::ParserContext(LexState& lex_state, FuncState& func_state, lua_State& lua_state,
   ParserAllocator allocator, ParserConfig config)
   : lex_state(&lex_state)
   , func_state(&func_state)
   , lua_state(&lua_state)
   , allocator(allocator)
   , current_config(config)
   , token_stream(lex_state)
{
   this->diag.set_limit(config.max_diagnostics);
}

LexState& ParserContext::lex() const
{
   return *this->lex_state;
}

FuncState& ParserContext::func() const
{
   return *this->func_state;
}

lua_State& ParserContext::lua() const
{
   return *this->lua_state;
}

ParserDiagnostics& ParserContext::diagnostics()
{
   return this->diag;
}

const ParserDiagnostics& ParserContext::diagnostics() const
{
   return this->diag;
}

TokenStreamAdapter& ParserContext::tokens()
{
   return this->token_stream;
}

const TokenStreamAdapter& ParserContext::tokens() const
{
   return this->token_stream;
}

ParserConfig ParserContext::config() const
{
   return this->current_config;
}

void ParserContext::override_config(const ParserConfig& config)
{
   this->current_config = config;
   this->diag.set_limit(config.max_diagnostics);
}

void ParserContext::restore_config(const ParserConfig& config)
{
   this->current_config = config;
   this->diag.set_limit(config.max_diagnostics);
}

ParserResult<Token> ParserContext::match(TokenKind kind)
{
   Token current = this->tokens().current();
   if (current.is(kind)) {
      this->token_stream.advance();
      return ParserResult<Token>::success(current);
   }

   if (this->current_config.trace_expectations) {
      std::string expectation = this->format_expected_message(kind);
      ParserDiagnostic diagnostic;
      diagnostic.severity = ParserDiagnosticSeverity::Info;
      diagnostic.code = ParserErrorCode::ExpectedToken;
      diagnostic.message = expectation;
      diagnostic.token = current;
      this->diag.report(diagnostic);
   }

   ParserError error = this->make_error(ParserErrorCode::ExpectedToken, current, std::string_view{});
   return ParserResult<Token>::failure(error);
}

ParserResult<Token> ParserContext::consume(TokenKind kind, ParserErrorCode code)
{
   auto result = this->match(kind);
   if (result.ok()) return result;

   std::string expectation = this->format_expected_message(kind);
   Token current = this->tokens().current();
   this->emit_error(code, current, expectation);
   return ParserResult<Token>::failure(this->make_error(code, current, expectation));
}

bool ParserContext::check(TokenKind kind) const
{
   return this->tokens().current().is(kind);
}

ParserResult<Token> ParserContext::expect_identifier(ParserErrorCode code)
{
   Token current = this->tokens().current();
   if (current.is_identifier()) {
      this->token_stream.advance();
      return ParserResult<Token>::success(current);
   }
   this->emit_error(code, current, "expected identifier");
   return ParserResult<Token>::failure(this->make_error(code, current, "expected identifier"));
}

std::string ParserContext::format_expected_message(TokenKind kind) const
{
   const char* name = token_kind_name(kind, this->lex());
   std::string message = std::string("expected ") + name;
   if ((LexToken)kind <= TK_OFS) {
      lua_pop(this->lua_state, 1);
   }
   return message;
}

ParserError ParserContext::make_error(ParserErrorCode code, const Token& token, std::string_view message)
{
   ParserError error;
   error.code = code;
   error.token = token;
   error.message.assign(message.begin(), message.end());
   return error;
}

void ParserContext::emit_error(ParserErrorCode code, const Token& token, std::string_view message)
{
   ParserDiagnostic diagnostic;
   diagnostic.severity = ParserDiagnosticSeverity::Error;
   diagnostic.code = code;
   diagnostic.message.assign(message.begin(), message.end());
   diagnostic.token = token;
   this->diag.report(diagnostic);
   if (this->current_config.abort_on_error) {
      this->lex_state->err_token(token.raw());
   }
}

ParserSession::ParserSession(ParserContext& context, ParserConfig config)
   : ctx(&context)
   , previous(context.config())
{
   this->ctx->override_config(config);
}

ParserSession::~ParserSession()
{
   this->ctx->restore_config(this->previous);
}

