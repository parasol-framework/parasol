// Parser tracing helpers for instrumentation and debugging.

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "parser/token_types.h"

enum class ParserPipelineMode : uint8_t {
   LegacyOnly = 0,
   AstPreferred
};

enum class ParserTraceEventKind : uint8_t {
   AstPrimaryAttempt,
   AstPrimarySuccess,
   AstPrimaryFailure,
   AstPrimaryFallback,
   LocalStatementAttempt,
   LocalStatementSuccess,
   LocalStatementFailure,
   LocalStatementFallback
};

struct ParserTraceEvent {
   ParserTraceEventKind kind = ParserTraceEventKind::AstPrimaryAttempt;
   std::string message;
   Token token;
};

class ParserTraceSink {
public:
   ParserTraceSink();

   void configure(std::size_t Limit);
   void set_enabled(bool Enabled);
   [[nodiscard]] bool enabled() const;
   void record(ParserTraceEventKind Kind, std::string_view Message, const Token& TokenInfo);
   void clear();
   [[nodiscard]] std::span<const ParserTraceEvent> entries() const;

private:
   bool tracing_enabled = false;
   std::size_t limit = 0;
   std::vector<ParserTraceEvent> events;
};

