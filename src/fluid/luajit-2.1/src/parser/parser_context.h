#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "parser/parser_diagnostics.h"
#include "parser/token_stream.h"

struct ParserAllocator {
   void* userdata = nullptr;

   static ParserAllocator from(lua_State* state);
};

struct ParserConfig {
   uint32_t max_diagnostics = 8;
   bool abort_on_error = true;
   bool trace_tokens = false;
   bool trace_expectations = false;
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

   static ParserResult<T> success(const T& value)
   {
      ParserResult<T> result;
      result.has_value = true;
      result.value = value;
      return result;
   }

   static ParserResult<T> failure(const ParserError& error)
   {
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

   LexState& lex() const;
   FuncState& func() const;
   lua_State& lua() const;

   ParserDiagnostics& diagnostics();
   const ParserDiagnostics& diagnostics() const;

   TokenStreamAdapter& tokens();
   const TokenStreamAdapter& tokens() const;

   ParserConfig config() const;
   void override_config(const ParserConfig& config);
   void restore_config(const ParserConfig& config);

   ParserResult<Token> match(TokenKind kind);
   ParserResult<Token> consume(TokenKind kind, ParserErrorCode code);
   bool check(TokenKind kind) const;
   ParserResult<Token> expect_identifier(ParserErrorCode code);

   void emit_error(ParserErrorCode code, const Token& token, std::string_view message);

private:
   ParserError make_error(ParserErrorCode code, const Token& token, std::string_view message);

   LexState* lex_state;
   FuncState* func_state;
   lua_State* lua_state;
   ParserAllocator allocator;
   ParserConfig current_config;
   ParserDiagnostics diag;
   TokenStreamAdapter token_stream;
};

class ParserSession {
public:
   ParserSession(ParserContext& context, ParserConfig config);
   ~ParserSession();

private:
   ParserContext* ctx;
   ParserConfig previous;
};

