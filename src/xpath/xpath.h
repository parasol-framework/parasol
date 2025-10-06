// A basic class for holding the compiled AST

#pragma once

#include <utility>
#include <parasol/modules/xpath.h>
#include "xpath_ast.h"
#include "xpath_tokenizer.h"
#include "xpath_parser.h"

// Lightweight view-based trim used for cache key normalisation.

inline std::string_view trim_view(std::string_view Value)
{
   auto start = Value.find_first_not_of(" \t\r\n");
   if (start IS std::string_view::npos) return std::string_view();

   auto end = Value.find_last_not_of(" \t\r\n");
   return Value.substr(start, end - start + 1);
}
