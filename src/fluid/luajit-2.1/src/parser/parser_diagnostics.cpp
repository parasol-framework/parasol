// Copyright (C) 2025 Paul Manias

#include "parser/parser_diagnostics.h"

ParserDiagnostics::ParserDiagnostics()
   : limit(8)
{
}

void ParserDiagnostics::set_limit(uint32_t new_limit)
{
   this->limit = new_limit;
}

void ParserDiagnostics::report(const ParserDiagnostic& diagnostic)
{
   if (this->storage.size() >= this->limit) return;
   this->storage.push_back(diagnostic);
}

bool ParserDiagnostics::has_errors() const
{
   for (const auto& entry : this->storage) {
      if (entry.severity IS ParserDiagnosticSeverity::Error) return true;
   }
   return false;
}

std::span<const ParserDiagnostic> ParserDiagnostics::entries() const
{
   return std::span<const ParserDiagnostic>(this->storage.data(), this->storage.size());
}

void ParserDiagnostics::clear()
{
   this->storage.clear();
}

