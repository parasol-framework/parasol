#include "parser/parser_context.h"

#include <algorithm>

#include "lj_err.h"

static ParserDiagnostics make_diagnostics(const ParserConfig& Config)
{
   return ParserDiagnostics(Config.max_diagnostics);
}

ParserDiagnostics::ParserDiagnostics()
   : limit(8)
{
}

ParserDiagnostics::ParserDiagnostics(std::size_t Limit)
   : limit(Limit)
{
}

void ParserDiagnostics::configure(std::size_t Limit)
{
   this->limit = Limit;
   if (this->diagnostics.size() > this->limit)
      this->diagnostics.resize(this->limit);
}

void ParserDiagnostics::report(const ParserDiagnostic& Diagnostic)
{
   if (this->diagnostics.size() >= this->limit)
      return;
   this->diagnostics.push_back(Diagnostic);
}

bool ParserDiagnostics::has_errors() const
{
   return std::any_of(this->diagnostics.begin(), this->diagnostics.end(), [](const ParserDiagnostic& Entry) {
      return Entry.severity IS ParserSeverity::Error;
   });
}

std::span<const ParserDiagnostic> ParserDiagnostics::entries() const
{
   return std::span(this->diagnostics.data(), this->diagnostics.size());
}

ParserContext ParserContext::from(LexState& Lex, FuncState& Func, lua_State& State, ParserConfig Config)
{
   return ParserContext(Lex, Func, State, Config);
}

ParserContext::ParserContext(LexState& Lex, FuncState& Func, lua_State& State, ParserConfig Config)
   : config_data(Config),
     diagnostics_data(make_diagnostics(Config)),
      lex_state(&Lex),
      func_state(&Func),
      lua_state(&State),
      allocator{&State},
      token_stream(&Lex)
{
   this->refresh_configuration();
}

void ParserContext::refresh_configuration()
{
   this->diagnostics_data.configure(this->config_data.max_diagnostics);
   this->trace_data.configure(this->config_data.max_trace_events);
   this->trace_data.set_enabled(this->config_data.enable_tracing);
}

LexState& ParserContext::lex()
{
   return *this->lex_state;
}

FuncState& ParserContext::func()
{
   return *this->func_state;
}

lua_State& ParserContext::state()
{
   return *this->lua_state;
}

const ParserConfig& ParserContext::config() const
{
   return this->config_data;
}

void ParserContext::set_config(const ParserConfig& Config)
{
   this->config_data = Config;
   this->refresh_configuration();
}

TokenStreamAdapter& ParserContext::tokens()
{
   return this->token_stream;
}

ParserDiagnostics& ParserContext::diagnostics()
{
   return this->diagnostics_data;
}

const ParserDiagnostics& ParserContext::diagnostics() const
{
   return this->diagnostics_data;
}

ParserTraceSink& ParserContext::trace()
{
   return this->trace_data;
}

const ParserTraceSink& ParserContext::trace() const
{
   return this->trace_data;
}

void ParserContext::trace_event(ParserTraceEventKind Kind, std::string_view Message, const Token& TokenInfo)
{
   this->trace_data.record(Kind, Message, TokenInfo);
}

bool ParserContext::tracing_enabled() const
{
   return this->trace_data.enabled();
}

void ParserContext::emit_error(ParserErrorCode Code, std::string_view Message, const Token& TokenInfo)
{
   ParserDiagnostic Diagnostic;
   Diagnostic.code = Code;
   Diagnostic.severity = ParserSeverity::Error;
   Diagnostic.message.assign(Message.begin(), Message.end());
   Diagnostic.token = TokenInfo;
   this->diagnostics_data.report(Diagnostic);
}

void ParserContext::emit_legacy_error(ErrMsg MessageId, const Token& TokenInfo)
{
   const char* Message = err2msg(MessageId);
   this->emit_error(ParserErrorCode::LegacySyntaxError, Message, TokenInfo);
}

ParserResult<Token> ParserContext::consume(TokenKind Kind, ParserErrorCode Code)
{
   Token Current = this->tokens().current();
   if (Current.kind IS Kind) {
      this->tokens().advance();
      return ParserResult<Token>::success(Current);
   }
   auto Message = format_unexpected_token_message(Kind, Current.kind);
   ParserError Error;
   Error.code = Code;
   Error.message = Message;
   Error.token = Current;
   this->emit_error(Code, Error.message, Current);
   return ParserResult<Token>::failure(std::move(Error));
}

bool ParserContext::match(TokenKind Kind)
{
   if (this->check(Kind)) {
      this->tokens().advance();
      return true;
   }
   return false;
}

bool ParserContext::check(TokenKind Kind) const
{
   return this->token_stream.current().kind IS Kind;
}

ParserResult<Token> ParserContext::expect_identifier(ParserErrorCode Code)
{
   Token Current = this->tokens().current();
   if (Current.is_identifier()) {
      this->tokens().advance();
      return ParserResult<Token>::success(Current);
   }
   ParserError Error;
   Error.code = Code;
   Error.message = "identifier expected";
   Error.token = Current;
   this->emit_error(Code, Error.message, Current);
   return ParserResult<Token>::failure(std::move(Error));
}

ParserSession::ParserSession(ParserContext& Context, ParserConfig Override)
   : context(&Context), previous(Context.config())
{
   Context.set_config(Override);
}

ParserSession::~ParserSession()
{
   if (this->context)
      this->context->set_config(this->previous);
}

std::string format_unexpected_token_message(TokenKind Expected, TokenKind Actual)
{
   std::string Message = "expected ";
   Message.append(describe_token_kind(Expected));
   Message.append(", got ");
   Message.append(describe_token_kind(Actual));
   return Message;
}

