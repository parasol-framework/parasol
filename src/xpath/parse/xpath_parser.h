// XPath Tokeniser and Parser
//
// This file contains:
// - XPath tokenization (converting string to tokens)
// - XPath parsing (converting tokens to AST)
// - Grammar implementation for XPath syntax

#pragma once

#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <parasol/modules/xpath.h>
