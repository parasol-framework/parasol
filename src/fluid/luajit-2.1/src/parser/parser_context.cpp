// Copyright (C) 2025 Paul Manias

#include <parasol/main.h>
#include <parasol/modules/fluid.h>
#include "../../../defs.h"
#include "parser/parser_context.h"

#include <cstdio>

#include "parser/parse_types.h"
#include "parser.h"
#ifdef INCLUDE_ADVICE
#include "parser/parser_advice.h"
#endif

//********************************************************************************************************************

ParserContext & ParserContext::operator=(ParserContext&& other) noexcept
{
   if (this != &other) {
      this->detach_from_lex();
      this->lex_state  = other.lex_state;
      this->func_state = other.func_state;
      this->lua_state  = other.lua_state;
      this->allocator  = other.allocator;
      this->current_config = other.current_config;
      this->diag           = std::move(other.diag);
      this->diag.set_limit(this->current_config.max_diagnostics);
      this->token_stream = other.token_stream;
      this->previous_context = other.previous_context;
      if (this->lex_state) this->lex_state->active_context = this;
      other.lex_state  = nullptr;
      other.func_state = nullptr;
      other.lua_state  = nullptr;
      other.previous_context = nullptr;
   }
   return *this;
}

//********************************************************************************************************************

ParserResult<Token> ParserContext::match(TokenKind kind)
{
   Token current = this->tokens().current();
   if (current.is(kind)) {
      this->token_stream.advance();
      return ParserResult<Token>::success(current);
   }

   auto prv = (prvFluid *)this->lua_state->script->ChildPrivate;
   if ((prv->JitOptions & JOF::TRACE_EXPECT) != JOF::NIL) {
      std::string expectation = this->format_expected_message(kind);
      ParserDiagnostic diagnostic;
      diagnostic.severity = ParserDiagnosticSeverity::Info;
      diagnostic.code     = ParserErrorCode::ExpectedToken;
      diagnostic.message  = expectation;
      diagnostic.token    = current;
      this->diag.report(diagnostic);
      this->log_trace(ParserChannel::Expect, current, expectation);
   }

   ParserError error = this->make_error(ParserErrorCode::ExpectedToken, current, std::string_view{});
   return ParserResult<Token>::failure(error);
}

//********************************************************************************************************************

ParserResult<Token> ParserContext::consume(TokenKind kind, ParserErrorCode code)
{
   auto result = this->match(kind);
   if (result.ok()) return result;

   std::string expectation = this->format_expected_message(kind);
   Token current = this->tokens().current();
   this->emit_error(code, current, expectation);
   return ParserResult<Token>::failure(this->make_error(code, current, expectation));
}

//********************************************************************************************************************

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

//********************************************************************************************************************

int ParserContext::lex_opt(LexToken token)
{
   if (this->tokens().current().raw() IS token) {
      this->token_stream.advance();
      return 1;
   }
   return 0;
}

//********************************************************************************************************************

void ParserContext::lex_match(LexToken what, LexToken who, BCLine line)
{
   if (this->lex_opt(what)) return;

   if (line IS this->lex_state->linenumber) {
      this->err_token(what);
      return;
   }

   auto swhat = this->lex_state->token2str(what);
   auto swho = this->lex_state->token2str(who);
   lj_lex_error(this->lex_state, this->lex_state->tok, ErrMsg::XMATCH, swhat, swho, line);
}

//********************************************************************************************************************

GCstr* ParserContext::lex_str()
{
   Token current = this->tokens().current();
   if (not current.is_identifier()) {
      this->err_token(TK_name);
      return NAME_BLANK;
   }

   GCstr *result = current.identifier();
   this->token_stream.advance();
   return result ? result : NAME_BLANK;
}

//********************************************************************************************************************

void ParserContext::err_syntax(ErrMsg message)
{
   Token current = this->tokens().current();
   ParserDiagnostic diagnostic;
   diagnostic.severity = ParserDiagnosticSeverity::Error;
   diagnostic.code     = ParserErrorCode::UnexpectedToken;
   GCstr *text = lj_err_str(this->lua_state, message);
   if (text) diagnostic.message.assign(strdata(text), text->len);
   diagnostic.token = current;
   this->diag.report(diagnostic);
   lj_lex_error(this->lex_state, this->lex_state->tok, message);
}

//********************************************************************************************************************

void ParserContext::err_token(LexToken token)
{
   Token current = this->tokens().current();
   ParserDiagnostic diagnostic;
   diagnostic.severity = ParserDiagnosticSeverity::Error;
   diagnostic.code     = ParserErrorCode::UnexpectedToken;
   diagnostic.message  = this->format_lex_error(token);
   diagnostic.token    = current;
   this->diag.report(diagnostic);
   lj_lex_error(this->lex_state, this->lex_state->tok, ErrMsg::XTOKEN, this->lex_state->token2str(token));
}

//********************************************************************************************************************

void ParserContext::report_limit_error(FuncState& func_state, uint32_t limit, const char* what)
{
   ParserDiagnostic diagnostic;
   diagnostic.severity = ParserDiagnosticSeverity::Error;
   diagnostic.code     = ParserErrorCode::UnexpectedToken;
   diagnostic.message  = std::string("function limit exceeded for ") + what;
   diagnostic.token    = this->tokens().current();
   this->diag.report(diagnostic);
   if (func_state.linedefined IS 0) {
      lj_lex_error(func_state.ls, 0, ErrMsg::XLIMM, limit, what);
      return;
   }
   lj_lex_error(func_state.ls, 0, ErrMsg::XLIMF, func_state.linedefined, limit, what);
}

//********************************************************************************************************************

std::string ParserContext::format_expected_message(TokenKind kind) const
{
   CSTRING name = token_kind_name(kind, this->lex());
   std::string message = std::string("expected ") + name;
   if ((LexToken)kind <= TK_OFS) {
      lua_pop(this->lua_state, 1);
   }
   return message;
}

//********************************************************************************************************************

ParserError ParserContext::make_error(ParserErrorCode code, const Token &token, std::string_view message)
{
   ParserError error;
   error.code = code;
   error.token = token;
   error.message.assign(message.begin(), message.end());
   return error;
}

//********************************************************************************************************************

void ParserContext::trace_token_advance(const Token &previous, const Token &current) const
{
   auto prv = (prvFluid *)this->lua_state->script->ChildPrivate;

   if ((prv->JitOptions & JOF::TRACE_TOKENS) != JOF::NIL) {
      std::string detail = std::string("previous: ") + this->describe_token(previous);
      this->log_trace(ParserChannel::Advance, current, detail);
   }
}

//********************************************************************************************************************
// Note: This function does not return if abort_on_error is true

void ParserContext::emit_error(ParserErrorCode code, const Token &token, std::string_view message)
{
   ParserDiagnostic diagnostic;
   diagnostic.severity = ParserDiagnosticSeverity::Error;
   diagnostic.code     = code;
   diagnostic.message.assign(message.begin(), message.end());
   diagnostic.token    = token;
   this->diag.report(diagnostic);

   if (this->current_config.abort_on_error) {
      // Log immediately since we're about to throw
      this->log_trace(ParserChannel::Error, token, message);

      // Save the diagnostics for client analysis
      this->lua().parser_diagnostics = new ParserDiagnostics(this->diagnostics());

      lj_lex_error(this->lex_state, this->lex_state->tok, ErrMsg::XTOKEN, this->lex_state->token2str(token.raw()));
   }
   // In DIAGNOSE mode (abort_on_error=false), skip logging - errors will be reported later
}

//********************************************************************************************************************
// Emit a warning diagnostic (non-fatal)

void ParserContext::emit_warning(ParserErrorCode code, const Token &token, std::string_view message)
{
   ParserDiagnostic diagnostic;
   diagnostic.severity = ParserDiagnosticSeverity::Warning;
   diagnostic.code     = code;
   diagnostic.message.assign(message.begin(), message.end());
   diagnostic.token    = token;
   this->diag.report(diagnostic);

   this->log_trace(ParserChannel::Warning, token, message);
}

//********************************************************************************************************************

void ParserContext::attach_to_lex()
{
   if (not this->lex_state) return;
   this->previous_context = this->lex_state->active_context;
   this->lex_state->active_context = this;
}

//********************************************************************************************************************

void ParserContext::detach_from_lex()
{
   if (not this->lex_state) return;
   if (this->lex_state->active_context IS this) {
      this->lex_state->active_context = this->previous_context;
   }
}

//********************************************************************************************************************

std::string ParserContext::format_lex_error(LexToken token) const
{
   auto text = this->lex_state->token2str(token);
   if (not text) return std::string("unexpected token");
   return std::string("unexpected ") + text;
}

//********************************************************************************************************************

std::string ParserContext::describe_token(const Token &token) const
{
   auto name = token_kind_name(token.kind(), this->lex());
   std::string result;

   result.assign(name ? name : "token");

   if ((LexToken)token.kind() <= TK_OFS) lua_pop(this->lua_state, 1);

   if (token.is_identifier()) {
      if (GCstr *identifier = token.identifier()) {
         result.push_back(' ');
         result.push_back('\'');
         result.append(strdata(identifier), identifier->len);
         result.push_back('\'');
      }
   }
   return result;
}

//********************************************************************************************************************

void ParserContext::log_trace(ParserChannel Channel, const Token &token, std::string_view note) const
{
   pf::Log log("Parser");

   std::string name = this->describe_token(token);
   BCLine line = token.span().line;
   BCLine column = token.span().column;

   VLF level = VLF::API;
   CSTRING channel;
   switch(Channel) {
      case ParserChannel::Error:   channel = "Error"; level = VLF::WARNING; break;
      case ParserChannel::Warning: channel = "Warning"; level = VLF::WARNING; break;
      case ParserChannel::Expect:  channel = "Expect"; break;
      case ParserChannel::Advance: channel = "Advance"; break;
      default:                     channel = "Unknown"; break;
   }

   if (note.empty()) {
      log.msg(level, "[%d:%d] %s: %s", (int)line, (int)column, channel, name.c_str());
      return;
   }

   log.msg(level, "[%d:%d] %s: %s - %.*s", (int)line, (int)column, channel, name.c_str(), (int)note.size(), note.data());
}

//********************************************************************************************************************
// Check if advice at the given priority level would be emitted.
// This allows callers to skip expensive checks when advice would be filtered out anyway.

#ifdef INCLUDE_ADVICE
bool ParserContext::should_emit_advice(uint8_t Priority) const
{
   auto *emitter = this->advice();
   if (not emitter) return false;
   return emitter->should_emit(Priority);
}

//********************************************************************************************************************
// Emit an advice message if the advice system is enabled.

void ParserContext::emit_advice(uint8_t Priority, AdviceCategory Category, std::string Message, const Token &Location)
{
   auto *emitter = this->advice();
   if (not emitter) return;

   std::string_view filename;
   if (this->lex_state->chunkname) {
      filename = std::string_view(strdata(this->lex_state->chunkname), this->lex_state->chunkname->len);
   }

   emitter->emit(Priority, Category, std::move(Message), Location, filename);
}
#endif
