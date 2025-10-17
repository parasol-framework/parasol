//********************************************************************************************************************
// XPath Tokeniser Interface
//
// Defines the XPathTokeniser class responsible for lexical analysis of XPath query strings.  The tokeniser
// breaks input strings into a sequence of tokens (operators, identifiers, literals, keywords) that can be
// consumed by the parser to build an abstract syntax tree.
//
// The tokeniser handles:
//   - Operator recognition (/, //, ::, etc.)
//   - String and numeric literals
//   - Identifiers and keywords
//   - Whitespace handling
//   - Character classification for XML names
//
// This is a single-pass tokeniser with lookahead capabilities for resolving ambiguous syntax.

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "xpath_ast.h"

class XPathTokeniser
{
   private:
   std::string_view input;
   size_t position;
   size_t length;
   XPathTokenType previous_token_type;

   [[nodiscard]] bool is_alpha(char c) const;
   [[nodiscard]] bool is_digit(char c) const;
   [[nodiscard]] bool is_alnum(char c) const;
   [[nodiscard]] bool is_whitespace(char c) const;
   [[nodiscard]] bool is_name_start_char(char c) const;
   [[nodiscard]] bool is_name_char(char c) const;

   XPathToken scan_identifier();
   XPathToken scan_number();
   XPathToken scan_string(char QuoteChar);
   XPathToken scan_attribute_value(char QuoteChar, bool ProcessTemplates);
   XPathToken scan_operator();

   [[nodiscard]] char peek(size_t offset = 0) const;
   void skip_whitespace();
   [[nodiscard]] bool match(std::string_view Str);

   public:
   XPathTokeniser() : position(0), length(0), previous_token_type(XPathTokenType::UNKNOWN) {}

   std::vector<XPathToken> tokenize(std::string_view XPath);
   bool has_more() const;
   [[nodiscard]] char current() const;
   void advance();
};
