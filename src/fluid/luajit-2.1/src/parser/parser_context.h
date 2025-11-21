#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "parser/parser_diagnostics.h"
#include "parser/token_stream.h"
#include "parser/parser_profiler.h"

struct ParserAllocator {
   void* userdata = nullptr;

   static ParserAllocator from(lua_State* state);
};

struct ParserConfig {
   uint32_t max_diagnostics = 8; // Defines the 'limit' value in ParserDiagnostics
   bool abort_on_error = true;
   ParserProfilingResult profiling_result;
};

struct ParserError {
   ParserErrorCode code = ParserErrorCode::None;
   std::string message;
   Token token;
};

template<typename T>
class ParserResult {
public:
   ParserResult() = default;

   static ParserResult<T> success(T value) {
      ParserResult<T> result;
      result.has_value = true;
      result.value = std::move(value);
      return result;
   }

   static ParserResult<T> failure(const ParserError& error) {
      ParserResult<T> result;
      result.has_value = false;
      result.error = error;
      return result;
   }

   [[nodiscard]] bool ok() const { return this->has_value; }
   [[nodiscard]] const T& value_ref() const { return this->value; }
   [[nodiscard]] T& value_ref() { return this->value; }
   [[nodiscard]] const ParserError& error_ref() const { return this->error; }

private:
   bool has_value = false;
   T value;
   ParserError error;
};

class ParserContext {
public:
   static ParserContext from(LexState& lex_state, FuncState& func_state, ParserAllocator allocator,
      ParserConfig config = ParserConfig{});

   ParserContext(LexState& lex_state, FuncState& func_state, lua_State& lua_state,
      ParserAllocator allocator, ParserConfig config);
   ParserContext(const ParserContext&) = delete;
   ParserContext& operator=(const ParserContext&) = delete;
   ParserContext(ParserContext&& other) noexcept;
   ParserContext& operator=(ParserContext&& other) noexcept;
   ~ParserContext();

   LexState& lex() const;
   FuncState& func() const;
   lua_State& lua() const;

   ParserDiagnostics& diagnostics();
   const ParserDiagnostics& diagnostics() const;

   TokenStreamAdapter& tokens();
   const TokenStreamAdapter& tokens() const;

   const ParserConfig& config() const;
   ParserProfilingResult& profiling_result();
   const ParserProfilingResult& profiling_result() const;
   void override_config(const ParserConfig& config);
   void restore_config(const ParserConfig& config);

   void trace_token_advance(const Token& previous, const Token& current) const;

   ParserResult<Token> match(TokenKind kind);
   ParserResult<Token> consume(TokenKind kind, ParserErrorCode code);
   bool check(TokenKind kind) const;
   ParserResult<Token> expect_identifier(ParserErrorCode code);

   int lex_opt(LexToken token);
   void lex_check(LexToken token);
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
   void log_trace(const char* channel, const Token& token, std::string_view note) const;

   LexState* lex_state;
   FuncState* func_state;
   lua_State* lua_state;
   ParserAllocator allocator;
   ParserConfig current_config;
   ParserDiagnostics diag;
   TokenStreamAdapter token_stream;
   ParserContext* previous_context = nullptr;
};

class ParserSession {
public:
   ParserSession(ParserContext& context, ParserConfig config);
   ~ParserSession();

private:
   ParserContext* ctx;
   ParserConfig previous;
};

