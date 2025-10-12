#pragma once

// Name:      regex.h
// Copyright: Paul Manias © 2025
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_REGEX (1)

// Optional flags for the Regex functions.

enum class REGEX : uint32_t {
   NIL = 0,
   ICASE = 0x00000001,
   MULTILINE = 0x00000002,
   DOT_ALL = 0x00000004,
   EXTENDED = 0x00000008,
   AWK = 0x00000010,
   GREP = 0x00000020,
};

DEFINE_ENUM_FLAG_OPERATORS(REGEX)

enum class RMATCH : uint32_t {
   NIL = 0,
   NOT_BEGIN_OF_LINE = 0x00000001,
   NOT_END_OF_LINE = 0x00000002,
   NOT_BEGIN_OF_WORD = 0x00000004,
   NOT_END_OF_WORD = 0x00000008,
   NOT_NULL = 0x00000010,
   CONTINUOUS = 0x00000020,
   PREV_AVAILABLE = 0x00000040,
   REPLACE_NO_COPY = 0x00000080,
   REPLACE_FIRST_ONLY = 0x00000100,
};

DEFINE_ENUM_FLAG_OPERATORS(RMATCH)

struct Regex {
   std::string Pattern;    // Original pattern string
   REGEX Flags;            // Compilation flags
};

#ifdef PARASOL_STATIC
#define JUMPTABLE_REGEX [[maybe_unused]] static struct RegexBase *RegexBase = nullptr;
#else
#define JUMPTABLE_REGEX struct RegexBase *RegexBase = nullptr;
#endif

struct RegexBase {
#ifndef PARASOL_STATIC
   ERR (*_Compile)(const std::string_view & Pattern, REGEX Flags, std::string *ErrorMsg, struct Regex **Result);
   ERR (*_Match)(struct Regex *Regex, const std::string_view & Text, RMATCH Flags, FUNCTION *Callback);
   ERR (*_Search)(struct Regex *Regex, const std::string_view & Text, RMATCH Flags, FUNCTION *Callback);
   ERR (*_Replace)(struct Regex *Regex, const std::string_view & Text, const std::string_view & Replacement, std::string *Output, RMATCH Flags);
   ERR (*_Split)(struct Regex *Regex, const std::string_view & Text, pf::vector<std::string> *Output, RMATCH Flags);
   ERR (*_GetCaptureIndex)(struct Regex *Regex, const std::string_view & Name, pf::vector<int> *Indices);
#endif // PARASOL_STATIC
};

#ifndef PRV_REGEX_MODULE
#ifndef PARASOL_STATIC
extern struct RegexBase *RegexBase;
namespace rx {
inline ERR Compile(const std::string_view & Pattern, REGEX Flags, std::string *ErrorMsg, struct Regex **Result) { return RegexBase->_Compile(Pattern,Flags,ErrorMsg,Result); }
inline ERR Match(struct Regex *Regex, const std::string_view & Text, RMATCH Flags, FUNCTION *Callback) { return RegexBase->_Match(Regex,Text,Flags,Callback); }
inline ERR Search(struct Regex *Regex, const std::string_view & Text, RMATCH Flags, FUNCTION *Callback) { return RegexBase->_Search(Regex,Text,Flags,Callback); }
inline ERR Replace(struct Regex *Regex, const std::string_view & Text, const std::string_view & Replacement, std::string *Output, RMATCH Flags) { return RegexBase->_Replace(Regex,Text,Replacement,Output,Flags); }
inline ERR Split(struct Regex *Regex, const std::string_view & Text, pf::vector<std::string> *Output, RMATCH Flags) { return RegexBase->_Split(Regex,Text,Output,Flags); }
inline ERR GetCaptureIndex(struct Regex *Regex, const std::string_view & Name, pf::vector<int> *Indices) { return RegexBase->_GetCaptureIndex(Regex,Name,Indices); }
} // namespace
#else
namespace rx {
extern ERR Compile(const std::string_view & Pattern, REGEX Flags, std::string *ErrorMsg, struct Regex **Result);
extern ERR Match(struct Regex *Regex, const std::string_view & Text, RMATCH Flags, FUNCTION *Callback);
extern ERR Search(struct Regex *Regex, const std::string_view & Text, RMATCH Flags, FUNCTION *Callback);
extern ERR Replace(struct Regex *Regex, const std::string_view & Text, const std::string_view & Replacement, std::string *Output, RMATCH Flags);
extern ERR Split(struct Regex *Regex, const std::string_view & Text, pf::vector<std::string> *Output, RMATCH Flags);
extern ERR GetCaptureIndex(struct Regex *Regex, const std::string_view & Name, pf::vector<int> *Indices);
} // namespace
#endif // PARASOL_STATIC
#endif

