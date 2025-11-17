#include "parser/parser_trace.h"

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

