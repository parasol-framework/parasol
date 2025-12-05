#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "parser/parser_diagnostics.h"
#include "parser/token_stream.h"
#include "parser/parser_profiler.h"

enum class ParserChannel : uint8_t {
   Error,
   Expect,
   Advance
};

//********************************************************************************************************************

struct ParserAllocator {
   void* userdata = nullptr;

   static ParserAllocator from(lua_State* state) {
      ParserAllocator allocator;
      allocator.userdata = (void*)state;
      return allocator;
   }
};

//********************************************************************************************************************

struct ParserConfig {
   uint32_t max_diagnostics = 8; // Defines the 'limit' value in ParserDiagnostics
   bool abort_on_error = true;
   bool enable_type_analysis = true;       // Enable static type checking
   bool type_errors_are_fatal = false;     // Treat type mismatches as errors vs warnings
   bool infer_local_types = true;          // Track types of local variables
   ParserProfilingResult profiling_result;
};

//********************************************************************************************************************

struct ParserError {
   ParserErrorCode code = ParserErrorCode::None;
   std::string message;
   Token token;
};

//********************************************************************************************************************

template<typename T>
class ParserResult {
public:
   ParserResult() : data(ParserError{}) {}

   static ParserResult<T> success(T value) {
      ParserResult<T> result;
      result.data = std::move(value);
      return result;
   }

   static ParserResult<T> failure(const ParserError& error) {
      ParserResult<T> result;
      result.data = error;
      return result;
   }

   [[nodiscard]] bool ok() const { return std::holds_alternative<T>(this->data); }
   [[nodiscard]] const T& value_ref() const { return std::get<T>(this->data); }
   [[nodiscard]] T& value_ref() { return std::get<T>(this->data); }
   [[nodiscard]] const ParserError& error_ref() const { return std::get<ParserError>(this->data); }

private:
   std::variant<T, ParserError> data;
};

//********************************************************************************************************************

class ParserContext {
public:
   static ParserContext from(LexState &lex_state, FuncState &func_state, ParserAllocator allocator,
      ParserConfig config = ParserConfig{}) {
      return ParserContext(lex_state, func_state, *lex_state.L, allocator, config);
   }

   ParserContext(LexState &lex_state, FuncState &func_state, lua_State &lua_state,
      ParserAllocator allocator, ParserConfig config)
      : lex_state(&lex_state), func_state(&func_state), lua_state(&lua_state), allocator(allocator)
      , current_config(config), token_stream(lex_state)
   {
      this->diag.set_limit(config.max_diagnostics);
      this->attach_to_lex();
   }

   ParserContext(const ParserContext&) = delete;
   ParserContext& operator=(const ParserContext&) = delete;
   ParserContext& operator=(ParserContext&& other) noexcept;

   ~ParserContext() { this->detach_from_lex(); }

   ParserContext(ParserContext &&other) noexcept
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

   inline LexState & lex() const { return *this->lex_state; }
   inline FuncState & func() const { return *this->func_state; }
   inline lua_State & lua() const { return *this->lua_state; }
   inline ParserDiagnostics & diagnostics() { return this->diag; }
   inline const ParserDiagnostics & diagnostics() const { return this->diag; }
   inline TokenStreamAdapter & tokens() { return this->token_stream; }
   inline const TokenStreamAdapter & tokens() const { return this->token_stream; }
   inline const ParserConfig & config() const { return this->current_config; }
   inline ParserProfilingResult & profiling_result() { return this->current_config.profiling_result; }
   inline const ParserProfilingResult & profiling_result() const { return this->current_config.profiling_result; }
   inline void override_config(const ParserConfig& config) { this->current_config = config; this->diag.set_limit(config.max_diagnostics); }
   inline void restore_config(const ParserConfig& config) { this->current_config = config; this->diag.set_limit(config.max_diagnostics); }

   void trace_token_advance(const Token& previous, const Token& current) const;

   ParserResult<Token> match(TokenKind kind);
   ParserResult<Token> consume(TokenKind kind, ParserErrorCode code);
   ParserResult<Token> expect_identifier(ParserErrorCode code);

   [[nodiscard]] inline bool check(TokenKind kind) const { return this->tokens().current().is(kind); }

   inline void lex_check(LexToken token) { if (not this->lex_opt(token)) this->err_token(token); }

   int lex_opt(LexToken token);
   void lex_match(LexToken what, LexToken who, BCLine line);
   GCstr* lex_str();

   void err_syntax(ErrMsg message);
   void err_token(LexToken token);
   void report_limit_error(FuncState& func_state, uint32_t limit, const char* what);

   void emit_error(ParserErrorCode code, const Token& token, std::string_view message);

private:
   void attach_to_lex();
   void detach_from_lex();
   std::string format_lex_error(LexToken token) const;
   std::string format_expected_message(TokenKind kind) const;
   ParserError make_error(ParserErrorCode code, const Token& token, std::string_view message);
   std::string describe_token(const Token& token) const;
   void log_trace(ParserChannel, const Token& token, std::string_view note) const;

   LexState* lex_state;
   FuncState* func_state;
   lua_State* lua_state;
   ParserAllocator allocator;
   ParserConfig current_config;
   ParserDiagnostics diag;
   TokenStreamAdapter token_stream;
   ParserContext* previous_context = nullptr;
};

//********************************************************************************************************************

class ParserSession {
public:
   inline ParserSession(ParserContext& context, ParserConfig config)
   : ctx(&context), previous(context.config()) {
      this->ctx->override_config(config);
   }

   inline ~ParserSession() { this->ctx->restore_config(this->previous); }

private:
   ParserContext* ctx;
   ParserConfig previous;
};
