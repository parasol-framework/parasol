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
   bool counts_against_limit = diagnostic.severity IS ParserDiagnosticSeverity::Error
      or diagnostic.severity IS ParserDiagnosticSeverity::Warning;

   if (counts_against_limit and this->counted_entries >= this->limit) return;

   this->storage.push_back(diagnostic);
   if (counts_against_limit) {
      this->counted_entries += 1;
   }
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
   this->counted_entries = 0;
}

