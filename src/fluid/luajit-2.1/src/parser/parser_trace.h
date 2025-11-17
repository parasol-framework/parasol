// Parser tracing helpers for instrumentation and debugging.

#pragma once

#include <cstddef>
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

struct ParserTraceSummary {
   std::size_t ast_primary_attempts = 0;
   std::size_t ast_primary_successes = 0;
   std::size_t ast_primary_failures = 0;
   std::size_t ast_primary_fallbacks = 0;
   std::size_t local_statement_attempts = 0;
   std::size_t local_statement_successes = 0;
   std::size_t local_statement_failures = 0;
   std::size_t local_statement_fallbacks = 0;

   void record(ParserTraceEventKind Kind);
   void merge(const ParserTraceSummary& Other);
   [[nodiscard]] bool matches(const ParserTraceSummary& Other) const;
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

ParserTraceSummary summarize_trace(std::span<const ParserTraceEvent> Events);
std::string format_trace_summary(const ParserTraceSummary& Summary);


