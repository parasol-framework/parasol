// Parser context scaffolding for LuaJIT bytecode parser.

#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "parser/token_stream.h"
#include "parser/parser_trace.h"

struct ParserConfig {
   std::size_t max_diagnostics = 8;
   std::size_t max_trace_events = 32;
   bool enable_tracing = false;
   ParserPipelineMode pipeline_mode = ParserPipelineMode::AstPreferred;
};

enum class ParserSeverity : uint8_t {
   Info,
   Warning,
   Error
};

enum class ParserErrorCode : uint16_t {
   None = 0,
   UnexpectedToken,
   IdentifierExpected,
   LegacySyntaxError,
   InternalError
};

struct ParserDiagnostic {
   ParserSeverity severity = ParserSeverity::Error;
   ParserErrorCode code = ParserErrorCode::None;
   std::string message;
   Token token;
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

   static ParserResult success(T Value)
   {
      ParserResult Result;
      Result.value = std::move(Value);
      return Result;
   }

   static ParserResult failure(ParserError Error)
   {
      ParserResult Result;
      Result.error = std::move(Error);
      return Result;
   }

   [[nodiscard]] bool has_value() const { return this->value.has_value(); }
   [[nodiscard]] explicit operator bool() const { return this->has_value(); }
   T& get() { return this->value.value(); }
   const T& get() const { return this->value.value(); }
   const ParserError& get_error() const { return this->error; }

private:
   std::optional<T> value;
   ParserError error;
};

class ParserDiagnostics {
public:
   ParserDiagnostics();
   explicit ParserDiagnostics(std::size_t Limit);

   void configure(std::size_t Limit);
   void report(const ParserDiagnostic& Diagnostic);
   [[nodiscard]] bool has_errors() const;
   [[nodiscard]] std::span<const ParserDiagnostic> entries() const;

private:
   std::size_t limit = 8;
   std::vector<ParserDiagnostic> diagnostics;
};

struct ParserAllocatorView {
   lua_State* lua_state = nullptr;
};

class ParserContext {
public:
   static ParserContext from(LexState& Lex, FuncState& Func, lua_State& State, ParserConfig Config = {});

   ParserContext(LexState& Lex, FuncState& Func, lua_State& State, ParserConfig Config);

   ParserContext(const ParserContext&) = delete;
   ParserContext& operator=(const ParserContext&) = delete;

   LexState& lex();
   FuncState& func();
   lua_State& state();
   const ParserConfig& config() const;
   void set_config(const ParserConfig& Config);
   TokenStreamAdapter& tokens();
   ParserDiagnostics& diagnostics();
   const ParserDiagnostics& diagnostics() const;
   ParserTraceSink& trace();
   const ParserTraceSink& trace() const;
   void trace_event(ParserTraceEventKind Kind, std::string_view Message, const Token& TokenInfo);
   bool tracing_enabled() const;

   void emit_error(ParserErrorCode Code, std::string_view Message, const Token& TokenInfo);
   void emit_legacy_error(ErrMsg MessageId, const Token& TokenInfo);

   ParserResult<Token> consume(TokenKind Kind, ParserErrorCode Code);
   bool match(TokenKind Kind);
   bool check(TokenKind Kind) const;
   ParserResult<Token> expect_identifier(ParserErrorCode Code);

private:
   ParserConfig config_data;
   ParserDiagnostics diagnostics_data;
   LexState* lex_state = nullptr;
   FuncState* func_state = nullptr;
   lua_State* lua_state = nullptr;
   ParserAllocatorView allocator;
   TokenStreamAdapter token_stream;
   ParserTraceSink trace_data;

   void refresh_configuration();
};

class ParserSession {
public:
   ParserSession(ParserContext& Context, ParserConfig Override);
   ~ParserSession();

   ParserSession(const ParserSession&) = delete;
   ParserSession& operator=(const ParserSession&) = delete;

private:
   ParserContext* context = nullptr;
   ParserConfig previous;
};

std::string format_unexpected_token_message(TokenKind Expected, TokenKind Actual);

