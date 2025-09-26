#include "regex.h"
#include "srell/srell.hpp"

#include <iterator>
#include <utility>

namespace pf {

namespace {

srell::regex_constants::syntax_option_type convert_syntax(SyntaxOptions Options)
{
   unsigned int native = 0u;

   if ((Options & SyntaxECMAScript) != 0u) native |= (unsigned int)srell::regex_constants::ECMAScript;
   if ((Options & SyntaxIgnoreCase) != 0u) native |= (unsigned int)srell::regex_constants::icase;
   if ((Options & SyntaxNoSubexpressions) != 0u) native |= (unsigned int)srell::regex_constants::nosubs;
   if ((Options & SyntaxOptimise) != 0u) native |= (unsigned int)srell::regex_constants::optimize;
   if ((Options & SyntaxMultiline) != 0u) native |= (unsigned int)srell::regex_constants::multiline;
   if ((Options & SyntaxSticky) != 0u) native |= (unsigned int)srell::regex_constants::sticky;
   if ((Options & SyntaxDotAll) != 0u) native |= (unsigned int)srell::regex_constants::dotall;
   if ((Options & SyntaxUnicodeSets) != 0u) native |= (unsigned int)srell::regex_constants::unicodesets;
   if ((Options & SyntaxVerboseMode) != 0u) native |= (unsigned int)srell::regex_constants::vmode;
   if ((Options & SyntaxQuiet) != 0u) native |= (unsigned int)srell::regex_constants::quiet;

   return (srell::regex_constants::syntax_option_type)native;
}

srell::regex_constants::match_flag_type convert_match_flags(MatchFlags Flags)
{
   unsigned int native = 0u;

   if ((Flags & MatchNotBeginOfLine) != 0u) native |= (unsigned int)srell::regex_constants::match_not_bol;
   if ((Flags & MatchNotEndOfLine) != 0u) native |= (unsigned int)srell::regex_constants::match_not_eol;
   if ((Flags & MatchNotBeginOfWord) != 0u) native |= (unsigned int)srell::regex_constants::match_not_bow;
   if ((Flags & MatchNotEndOfWord) != 0u) native |= (unsigned int)srell::regex_constants::match_not_eow;
   if ((Flags & MatchNotNull) != 0u) native |= (unsigned int)srell::regex_constants::match_not_null;
   if ((Flags & MatchContinuous) != 0u) native |= (unsigned int)srell::regex_constants::match_continuous;
   if ((Flags & MatchPrevAvailable) != 0u) native |= (unsigned int)srell::regex_constants::match_prev_avail;
   if ((Flags & FormatNoCopy) != 0u) native |= (unsigned int)srell::regex_constants::format_no_copy;
   if ((Flags & FormatFirstOnly) != 0u) native |= (unsigned int)srell::regex_constants::format_first_only;

   return (srell::regex_constants::match_flag_type)native;
}

void populate_result(std::string_view Text, const srell::cmatch &Native, bool Success, MatchResult &Result)
{
   Result.matched = Success;
   Result.captures.clear();
   Result.capture_spans.clear();
   Result.prefix.clear();
   Result.suffix.clear();
   Result.span.offset = 0u;
   Result.span.length = 0u;

   if (!Success) return;

   size_t capture_count = (size_t)Native.size();
   Result.captures.reserve(capture_count);
   Result.capture_spans.reserve(capture_count);

   for (size_t index = 0u; index < capture_count; ++index) {
      const srell::sub_match<const char *> &segment = Native[index];
      size_t offset = (size_t)std::string::npos;
      size_t length = 0u;

      if (segment.matched) {
         offset = (size_t)(segment.first - Text.data());
         length = (size_t)segment.length();
      }

      Result.captures.emplace_back(segment.str());
      Result.capture_spans.push_back({ offset, length });

      if (index IS 0u) {
         Result.span.offset = offset;
         Result.span.length = length;
      }
   }

   const srell::sub_match<const char *> &prefix = Native.prefix();
   const srell::sub_match<const char *> &suffix = Native.suffix();

   if (prefix.matched) Result.prefix.assign(prefix.first, prefix.second);
   if (suffix.matched) Result.suffix.assign(suffix.first, suffix.second);
}

std::string map_error_code(unsigned int ErrorCode)
{
   if (ErrorCode IS 0u) return std::string("ok");

   switch (ErrorCode) {
      case srell::regex_constants::error_collate: return std::string("error_collate: invalid collating element");
      case srell::regex_constants::error_ctype: return std::string("error_ctype: invalid character class");
      case srell::regex_constants::error_escape: return std::string("error_escape: invalid escape sequence");
      case srell::regex_constants::error_backref: return std::string("error_backref: invalid back reference");
      case srell::regex_constants::error_brack: return std::string("error_brack: mismatched brackets");
      case srell::regex_constants::error_paren: return std::string("error_paren: mismatched parentheses");
      case srell::regex_constants::error_brace: return std::string("error_brace: mismatched braces");
      case srell::regex_constants::error_badbrace: return std::string("error_badbrace: invalid range quantifier");
      case srell::regex_constants::error_range: return std::string("error_range: invalid character range");
      case srell::regex_constants::error_space: return std::string("error_space: insufficient memory");
      case srell::regex_constants::error_badrepeat: return std::string("error_badrepeat: nothing to repeat");
      case srell::regex_constants::error_complexity: return std::string("error_complexity: pattern is too complex");
      case srell::regex_constants::error_stack: return std::string("error_stack: stack exhausted");
      case srell::regex_constants::error_utf8: return std::string("error_utf8: invalid UTF-8 sequence");
      case srell::regex_constants::error_property: return std::string("error_property: unknown Unicode property");
      case srell::regex_constants::error_noescape: return std::string("error_noescape: escape is required in Unicode set mode");
      case srell::regex_constants::error_operator: return std::string("error_operator: invalid set operator in Unicode set mode");
      case srell::regex_constants::error_complement: return std::string("error_complement: invalid complement in Unicode set mode");
      case srell::regex_constants::error_modifier: return std::string("error_modifier: duplicated or misplaced inline modifier");
      default: break;
   }

   if (ErrorCode IS srell::regex_constants::error_internal) return std::string("error_internal: internal engine failure");

#if defined(SRELL_FIXEDWIDTHLOOKBEHIND)
   if (ErrorCode IS srell::regex_constants::error_lookbehind) return std::string("error_lookbehind: variable width look-behind");
#endif

   return std::string("error_unknown: ") + std::to_string(ErrorCode);
}

} // namespace

struct Regex::Implementation {
   srell::regex pattern;
   SyntaxOptions chosen_options = SyntaxECMAScript;
   unsigned int error_code = 0u;
   std::string error_message;
   bool ready = false;
};

Regex::Regex()
   : impl(new Implementation())
{
}

Regex::Regex(Regex &&Other) noexcept
   : impl(Other.impl)
{
   Other.impl = nullptr;
}

Regex &Regex::operator=(Regex &&Other) noexcept
{
   if (!(this IS &Other)) {
      delete impl;
      impl = Other.impl;
      Other.impl = nullptr;
   }
   return *this;
}

Regex::~Regex()
{
   delete impl;
}

bool Regex::compile(std::string_view Pattern, SyntaxOptions Options)
{
   if (!impl) impl = new Implementation();

   impl->chosen_options = Options;
   srell::regex_constants::syntax_option_type native = convert_syntax(Options);
   impl->pattern.assign(Pattern.data(), (size_t)Pattern.size(), native);
   impl->error_code = (unsigned int)impl->pattern.ecode();
   impl->error_message = map_error_code(impl->error_code);
   impl->ready = impl->error_code IS 0u;

   return impl->ready;
}

bool Regex::is_ready() const
{
   return impl and impl->ready;
}

SyntaxOptions Regex::options() const
{
   return impl ? impl->chosen_options : SyntaxECMAScript;
}

unsigned int Regex::last_error_code() const
{
   return impl ? impl->error_code : srell::regex_constants::error_internal;
}

const std::string &Regex::last_error_message() const
{
   if (impl) return impl->error_message;

   static std::string fallback = map_error_code(srell::regex_constants::error_internal);
   return fallback;
}

bool Regex::match(std::string_view Text, MatchResult &Result, MatchFlags Flags) const
{
   if (!impl or !impl->ready) {
      Result.matched = false;
      Result.captures.clear();
      Result.capture_spans.clear();
      Result.prefix.clear();
      Result.suffix.clear();
      Result.span.offset = 0u;
      Result.span.length = 0u;
      return false;
   }

   srell::cmatch native;
   bool success = impl->pattern.match(Text.data(), Text.data() + Text.size(), native, convert_match_flags(Flags));
   populate_result(Text, native, success, Result);
   return success;
}

bool Regex::search(std::string_view Text, MatchResult &Result, MatchFlags Flags) const
{
   if (!impl or !impl->ready) {
      Result.matched = false;
      Result.captures.clear();
      Result.capture_spans.clear();
      Result.prefix.clear();
      Result.suffix.clear();
      Result.span.offset = 0u;
      Result.span.length = 0u;
      return false;
   }

   srell::cmatch native;
   bool success = impl->pattern.search(Text.data(), Text.data() + Text.size(), native, convert_match_flags(Flags));
   populate_result(Text, native, success, Result);
   return success;
}

bool Regex::replace(std::string_view Text, std::string_view Replacement, std::string &Output, MatchFlags Flags) const
{
   Output.clear();

   if (!impl or !impl->ready) return false;

   srell::regex_constants::match_flag_type native = convert_match_flags(Flags);
   Output = srell::regex_replace(std::string(Text), impl->pattern, std::string(Replacement), native);
   return true;
}

bool Regex::tokenize(std::string_view Text, int Submatch, std::vector<std::string> &Output, MatchFlags Flags) const
{
   Output.clear();

   if (!impl or !impl->ready) return false;

   const char *begin = Text.data();
   const char *end = begin + Text.size();
   srell::regex_constants::match_flag_type native = convert_match_flags(Flags);
   srell::cregex_token_iterator iter(begin, end, impl->pattern, Submatch, native);
   srell::cregex_token_iterator sentinel;

   for (; !(iter IS sentinel); ++iter) {
      Output.emplace_back(iter->str());
   }

   return true;
}

std::string describe_error(unsigned int ErrorCode)
{
   return map_error_code(ErrorCode);
}

} // namespace parasol::regex

