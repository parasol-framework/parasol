// Copyright (C) 2025 Paul Manias

#include <parasol/main.h>
#include <parasol/modules/fluid.h>
#include "../../../defs.h"
#include "parser/parser_context.h"

#include <cstdio>

#include "parser/parse_types.h"
#include "lj_parse.h"

ParserAllocator ParserAllocator::from(lua_State* state)
{
   ParserAllocator allocator;
   allocator.userdata = (void*)state;
   return allocator;
}

ParserContext ParserContext::from(LexState &lex_state, FuncState &func_state, ParserAllocator allocator,
   ParserConfig config)
{
   return ParserContext(lex_state, func_state, *lex_state.L, allocator, config);
}

ParserContext::ParserContext(LexState &lex_state, FuncState &func_state, lua_State &lua_state,
   ParserAllocator allocator, ParserConfig config)
   : lex_state(&lex_state), func_state(&func_state), lua_state(&lua_state), allocator(allocator)
   , current_config(config), token_stream(lex_state)
{
   this->diag.set_limit(config.max_diagnostics);
   this->attach_to_lex();
}

ParserContext::ParserContext(ParserContext &&other) noexcept
   : lex_state(other.lex_state), func_state(other.func_state)
   , lua_state(other.lua_state), allocator(other.allocator)
   , current_config(other.current_config), diag(std::move(other.diag))
   , token_stream(other.token_stream), previous_context(other.previous_context)
{
   if (this->lex_state) this->lex_state->active_context = this;
   other.lex_state = nullptr;
   other.func_state = nullptr;
   other.lua_state = nullptr;
   other.previous_context = nullptr;
}

ParserContext& ParserContext::operator=(ParserContext&& other) noexcept
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

ParserContext::~ParserContext()
{
   this->detach_from_lex();
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

const ParserConfig& ParserContext::config() const
{
   return this->current_config;
}

ParserProfilingResult& ParserContext::profiling_result()
{
   return this->current_config.profiling_result;
}

const ParserProfilingResult& ParserContext::profiling_result() const
{
   return this->current_config.profiling_result;
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

   auto prv = (prvFluid *)this->lua_state->Script->ChildPrivate;
   if ((prv->JitOptions & JOF::TRACE_EXPECT) != JOF::NIL) {
      std::string expectation = this->format_expected_message(kind);
      ParserDiagnostic diagnostic;
      diagnostic.severity = ParserDiagnosticSeverity::Info;
      diagnostic.code = ParserErrorCode::ExpectedToken;
      diagnostic.message = expectation;
      diagnostic.token = current;
      this->diag.report(diagnostic);
      this->log_trace("expect", current, expectation);
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

int ParserContext::lex_opt(LexToken token)
{
   if (this->tokens().current().raw() IS token) {
      this->token_stream.advance();
      return 1;
   }
   return 0;
}

void ParserContext::lex_check(LexToken token)
{
   if (not this->lex_opt(token)) this->err_token(token);
}

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

void ParserContext::err_syntax(ErrMsg message)
{
   Token current = this->tokens().current();
   ParserDiagnostic diagnostic;
   diagnostic.severity = ParserDiagnosticSeverity::Error;
   diagnostic.code = ParserErrorCode::UnexpectedToken;
   GCstr *text = lj_err_str(this->lua_state, message);
   if (text) diagnostic.message.assign(strdata(text), text->len);
   diagnostic.token = current;
   this->diag.report(diagnostic);
   lj_lex_error(this->lex_state, this->lex_state->tok, message);
}

void ParserContext::err_token(LexToken token)
{
   Token current = this->tokens().current();
   ParserDiagnostic diagnostic;
   diagnostic.severity = ParserDiagnosticSeverity::Error;
   diagnostic.code = ParserErrorCode::UnexpectedToken;
   diagnostic.message = this->format_lex_error(token);
   diagnostic.token = current;
   this->diag.report(diagnostic);
   lj_lex_error(this->lex_state, this->lex_state->tok, ErrMsg::XTOKEN, this->lex_state->token2str(token));
}

void ParserContext::report_limit_error(FuncState& func_state, uint32_t limit, const char* what)
{
   ParserDiagnostic diagnostic;
   diagnostic.severity = ParserDiagnosticSeverity::Error;
   diagnostic.code = ParserErrorCode::UnexpectedToken;
   diagnostic.message = std::string("function limit exceeded for ") + what;
   diagnostic.token = this->tokens().current();
   this->diag.report(diagnostic);
   if (func_state.linedefined IS 0) {
      lj_lex_error(func_state.ls, 0, ErrMsg::XLIMM, limit, what);
      return;
   }
   lj_lex_error(func_state.ls, 0, ErrMsg::XLIMF, func_state.linedefined, limit, what);
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

ParserError ParserContext::make_error(ParserErrorCode code, const Token &token, std::string_view message)
{
   ParserError error;
   error.code = code;
   error.token = token;
   error.message.assign(message.begin(), message.end());
   return error;
}

void ParserContext::trace_token_advance(const Token &previous, const Token &current) const
{
   auto prv = (prvFluid *)this->lua_state->Script->ChildPrivate;

   if ((prv->JitOptions & JOF::TRACE_TOKENS) != JOF::NIL) {
      std::string detail = std::string("previous: ") + this->describe_token(previous);
      this->log_trace("advance", current, detail);
   }
}

void ParserContext::emit_error(ParserErrorCode code, const Token &token, std::string_view message)
{
   ParserDiagnostic diagnostic;
   diagnostic.severity = ParserDiagnosticSeverity::Error;
   diagnostic.code = code;
   diagnostic.message.assign(message.begin(), message.end());
   diagnostic.token = token;
   this->diag.report(diagnostic);

   auto prv = (prvFluid *)this->lua_state->Script->ChildPrivate;

   if ((prv->JitOptions & JOF::TRACE_EXPECT) != JOF::NIL) {
      this->log_trace("error", token, message);
   }

   if (this->current_config.abort_on_error) {
      lj_lex_error(this->lex_state, this->lex_state->tok, ErrMsg::XTOKEN, this->lex_state->token2str(token.raw()));
   }
}

void ParserContext::attach_to_lex()
{
   if (not this->lex_state) return;
   this->previous_context = this->lex_state->active_context;
   this->lex_state->active_context = this;
}

void ParserContext::detach_from_lex()
{
   if (not this->lex_state) return;
   if (this->lex_state->active_context IS this) {
      this->lex_state->active_context = this->previous_context;
   }
}

std::string ParserContext::format_lex_error(LexToken token) const
{
   auto text = this->lex_state->token2str(token);
   if (not text) return std::string("unexpected token");
   return std::string("unexpected ") + text;
}

std::string ParserContext::describe_token(const Token &token) const
{
   auto name = token_kind_name(token.kind(), this->lex());
   std::string result;

   result.assign(name ? name : "token");

   if ((LexToken)token.kind() <= TK_OFS) lua_pop(this->lua_state, 1);

   if (token.is_identifier()) {
      if (GCstr *identifier = token.identifier()) {
         result.push_back(' ');
         result.push_back((char)39);
         result.append(strdata(identifier), identifier->len);
         result.push_back((char)39);
      }
   }
   return result;
}

//********************************************************************************************************************

void ParserContext::log_trace(const char* channel, const Token &token, std::string_view note) const
{
   pf::Log log("Parser");

   std::string name = this->describe_token(token);
   BCLine line = token.span().line;
   BCLine column = token.span().column;

   if (note.empty()) {
      log.detail("%s: %s (line %d, column %d)", channel, name.c_str(), (int)line, (int)column);
      return;
   }

   log.detail("%s: %s (line %d, column %d) - %.*s", channel, name.c_str(), (int)line, (int)column,
      (int)note.size(), note.data());
}

ParserSession::ParserSession(ParserContext& context, ParserConfig config)
   : ctx(&context), previous(context.config())
{
   this->ctx->override_config(config);
}

ParserSession::~ParserSession()
{
   this->ctx->restore_config(this->previous);
}

