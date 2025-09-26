#pragma once

#include <parasol/main.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace pf {

enum SyntaxOption : unsigned int {
   SyntaxECMAScript = 1u << 0,
   SyntaxIgnoreCase = 1u << 1,
   SyntaxNoSubexpressions = 1u << 2,
   SyntaxOptimise = 1u << 3,
   SyntaxMultiline = 1u << 4,
   SyntaxSticky = 1u << 5,
   SyntaxDotAll = 1u << 6,
   SyntaxUnicodeSets = 1u << 7,
   SyntaxVerboseMode = 1u << 8,
   SyntaxQuiet = 1u << 9
};

using SyntaxOptions = unsigned int;

enum MatchFlag : unsigned int {
   MatchDefault = 0u,
   MatchNotBeginOfLine = 1u << 0,
   MatchNotEndOfLine = 1u << 1,
   MatchNotBeginOfWord = 1u << 2,
   MatchNotEndOfWord = 1u << 3,
   MatchNotNull = 1u << 4,
   MatchContinuous = 1u << 5,
   MatchPrevAvailable = 1u << 6,
   FormatNoCopy = 1u << 7,
   FormatFirstOnly = 1u << 8
};

using MatchFlags = unsigned int;

struct CaptureSpan {
   size_t offset = 0u;
   size_t length = 0u;
};

struct MatchResult {
   bool matched = false;
   CaptureSpan span;
   std::vector<std::string> captures;
   std::vector<CaptureSpan> capture_spans;
   std::string prefix;
   std::string suffix;
};

class Regex {
public:
   Regex();
   Regex(Regex &&Other) noexcept;
   Regex &operator=(Regex &&Other) noexcept;
   Regex(const Regex &) = delete;
   Regex &operator=(const Regex &) = delete;
   ~Regex();

   bool compile(std::string_view Pattern, SyntaxOptions Options = SyntaxECMAScript);
   bool is_ready() const;
   SyntaxOptions options() const;
   unsigned int last_error_code() const;
   const std::string &last_error_message() const;

   bool match(std::string_view Text, MatchResult &Result, MatchFlags Flags = MatchDefault) const;
   bool search(std::string_view Text, MatchResult &Result, MatchFlags Flags = MatchDefault) const;
   bool replace(std::string_view Text, std::string_view Replacement, std::string &Output, MatchFlags Flags = MatchDefault) const;
   bool tokenize(std::string_view Text, int Submatch, std::vector<std::string> &Output, MatchFlags Flags = MatchDefault) const;

private:
   struct Implementation;
   Implementation *impl;
};

std::string describe_error(unsigned int ErrorCode);

} // namespace pf

