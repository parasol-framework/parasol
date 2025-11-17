#include "parser/parser_trace.h"

#include <array>
#include <sstream>

ParserTraceSink::ParserTraceSink() = default;

void ParserTraceSink::configure(std::size_t Limit)
{
   this->limit = Limit;
   if (this->events.size() > this->limit)
      this->events.resize(this->limit);
}

void ParserTraceSink::set_enabled(bool Enabled)
{
   this->tracing_enabled = Enabled;
   if (!this->tracing_enabled)
      this->events.clear();
}

bool ParserTraceSink::enabled() const
{
   return this->tracing_enabled;
}

void ParserTraceSink::record(ParserTraceEventKind Kind, std::string_view Message, const Token& TokenInfo)
{
   if (!this->tracing_enabled)
      return;
   if (this->limit != 0 and this->events.size() >= this->limit)
      return;
   ParserTraceEvent Event;
   Event.kind = Kind;
   Event.message.assign(Message.begin(), Message.end());
   Event.token = TokenInfo;
   this->events.push_back(Event);
}

void ParserTraceSink::clear()
{
   this->events.clear();
}

std::span<const ParserTraceEvent> ParserTraceSink::entries() const
{
   return std::span(this->events.data(), this->events.size());
}

void ParserTraceSummary::record(ParserTraceEventKind Kind)
{
   switch (Kind) {
   case ParserTraceEventKind::AstPrimaryAttempt:
      this->ast_primary_attempts += 1;
      break;
   case ParserTraceEventKind::AstPrimarySuccess:
      this->ast_primary_successes += 1;
      break;
   case ParserTraceEventKind::AstPrimaryFailure:
      this->ast_primary_failures += 1;
      break;
   case ParserTraceEventKind::AstPrimaryFallback:
      this->ast_primary_fallbacks += 1;
      break;
   case ParserTraceEventKind::LocalStatementAttempt:
      this->local_statement_attempts += 1;
      break;
   case ParserTraceEventKind::LocalStatementSuccess:
      this->local_statement_successes += 1;
      break;
   case ParserTraceEventKind::LocalStatementFailure:
      this->local_statement_failures += 1;
      break;
   case ParserTraceEventKind::LocalStatementFallback:
      this->local_statement_fallbacks += 1;
      break;
   }
}

void ParserTraceSummary::merge(const ParserTraceSummary& Other)
{
   this->ast_primary_attempts += Other.ast_primary_attempts;
   this->ast_primary_successes += Other.ast_primary_successes;
   this->ast_primary_failures += Other.ast_primary_failures;
   this->ast_primary_fallbacks += Other.ast_primary_fallbacks;
   this->local_statement_attempts += Other.local_statement_attempts;
   this->local_statement_successes += Other.local_statement_successes;
   this->local_statement_failures += Other.local_statement_failures;
   this->local_statement_fallbacks += Other.local_statement_fallbacks;
}

bool ParserTraceSummary::matches(const ParserTraceSummary& Other) const
{
   return this->ast_primary_attempts IS Other.ast_primary_attempts
      and this->ast_primary_successes IS Other.ast_primary_successes
      and this->ast_primary_failures IS Other.ast_primary_failures
      and this->ast_primary_fallbacks IS Other.ast_primary_fallbacks
      and this->local_statement_attempts IS Other.local_statement_attempts
      and this->local_statement_successes IS Other.local_statement_successes
      and this->local_statement_failures IS Other.local_statement_failures
      and this->local_statement_fallbacks IS Other.local_statement_fallbacks;
}

ParserTraceSummary summarize_trace(std::span<const ParserTraceEvent> Events)
{
   ParserTraceSummary Summary;
   for (const auto& Event : Events)
      Summary.record(Event.kind);
   return Summary;
}

static void append_summary_value(std::ostringstream& Stream, const char* Label, std::size_t Value)
{
   Stream << Label << '=' << Value;
}

std::string format_trace_summary(const ParserTraceSummary& Summary)
{
   std::ostringstream Builder;
   append_summary_value(Builder, "primary_attempts", Summary.ast_primary_attempts);
   Builder << ", ";
   append_summary_value(Builder, "primary_successes", Summary.ast_primary_successes);
   Builder << ", ";
   append_summary_value(Builder, "primary_failures", Summary.ast_primary_failures);
   Builder << ", ";
   append_summary_value(Builder, "primary_fallbacks", Summary.ast_primary_fallbacks);
   Builder << ", ";
   append_summary_value(Builder, "local_attempts", Summary.local_statement_attempts);
   Builder << ", ";
   append_summary_value(Builder, "local_successes", Summary.local_statement_successes);
   Builder << ", ";
   append_summary_value(Builder, "local_failures", Summary.local_statement_failures);
   Builder << ", ";
   append_summary_value(Builder, "local_fallbacks", Summary.local_statement_fallbacks);
   return Builder.str();
}

