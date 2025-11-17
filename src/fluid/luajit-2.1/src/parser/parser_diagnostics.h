// Copyright (C) 2025 Paul Manias

#pragma once

#include <span>
#include <string>
#include <vector>

#include "parser/token_types.h"

enum class ParserDiagnosticSeverity : uint8_t {
   Info,
   Warning,
   Error
};

enum class ParserErrorCode : uint16_t {
   None = 0,
   UnexpectedToken,
   ExpectedToken,
   ExpectedIdentifier,
   UnexpectedEndOfFile,
   InternalInvariant
};

struct ParserDiagnostic {
   ParserDiagnosticSeverity severity = ParserDiagnosticSeverity::Error;
   ParserErrorCode code = ParserErrorCode::UnexpectedToken;
   std::string message;
   Token token;
};

class ParserDiagnostics {
public:
   ParserDiagnostics();

   void set_limit(uint32_t limit);
   void report(const ParserDiagnostic& diagnostic);
   [[nodiscard]] bool has_errors() const;
   [[nodiscard]] std::span<const ParserDiagnostic> entries() const;
   void clear();

private:
   uint32_t limit;
   std::vector<ParserDiagnostic> storage;
};

