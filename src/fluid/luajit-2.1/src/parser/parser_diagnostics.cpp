// Copyright (C) 2025 Paul Manias

#include "parser/parser_diagnostics.h"
#include <parasol/main.h>
#include <format>

//********************************************************************************************************************

[[maybe_unused]] [[noreturn]] void err_limit(FuncState *fs, uint32_t limit, CSTRING what)
{
   if (fs->ls->active_context) fs->ls->active_context->report_limit_error(*fs, limit, what);

   if (fs->linedefined IS 0) lj_lex_error(fs->ls, 0, ErrMsg::XLIMM, limit, what);
   else lj_lex_error(fs->ls, 0, ErrMsg::XLIMF, fs->linedefined, limit, what);
}

//********************************************************************************************************************

static CSTRING severity_name(ParserDiagnosticSeverity Severity)
{
   switch (Severity) {
      case ParserDiagnosticSeverity::Info:    return "Info";
      case ParserDiagnosticSeverity::Warning: return "Warning";
      case ParserDiagnosticSeverity::Error:   return "Error";
      default: return "unknown";
   }
}

//********************************************************************************************************************

static CSTRING error_code_name(ParserErrorCode Code)
{
   switch (Code) {
      case ParserErrorCode::None:                return "None";
      case ParserErrorCode::UnexpectedToken:     return "Unexpected Token";
      case ParserErrorCode::ExpectedToken:       return "Expected Token";
      case ParserErrorCode::ExpectedIdentifier:  return "Expected Identifier";
      case ParserErrorCode::UnexpectedEndOfFile: return "Unexpected EOF";
      case ParserErrorCode::InternalInvariant:   return "Internal invariant";
      case ParserErrorCode::ExpectedTypeName:    return "Expected type name";
      case ParserErrorCode::UnknownTypeName:     return "Unknown type name";
      case ParserErrorCode::TypeMismatchArgument: return "Type mismatch (argument)";
      case ParserErrorCode::TypeMismatchAssignment:return "Type mismatch (assignment)";
      case ParserErrorCode::TypeMismatchReturn:  return "Type mismatch (return)";
      default: return "Unknown";
   }
}

//********************************************************************************************************************

std::string ParserDiagnostic::to_string(int LineOffset) const
{
   SourceSpan span = this->token.span();
   return std::format("[{}:{}] {}: {}: {}", span.line + LineOffset, span.column,
      severity_name(this->severity), error_code_name(this->code),
      this->message.empty() ? "No message" : this->message);
}

ParserDiagnostics::ParserDiagnostics() : limit(8)
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
   if (counts_against_limit) this->counted_entries += 1;
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
