#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "xpath_ast.h"

class XPathTokenizer
{
   private:
   std::string_view input;
   size_t position;
   size_t length;

   [[nodiscard]] bool is_alpha(char c) const;
   [[nodiscard]] bool is_digit(char c) const;
   [[nodiscard]] bool is_alnum(char c) const;
   [[nodiscard]] bool is_whitespace(char c) const;
   [[nodiscard]] bool is_name_start_char(char c) const;
   [[nodiscard]] bool is_name_char(char c) const;

   XPathToken scan_identifier();
   XPathToken scan_number();
   XPathToken scan_string(char QuoteChar);
   XPathToken scan_operator();

   [[nodiscard]] char peek(size_t offset = 0) const;
   void skip_whitespace();
   [[nodiscard]] bool match(std::string_view Str);

   public:
   XPathTokenizer() : position(0), length(0) {}

   std::vector<XPathToken> tokenize(std::string_view XPath);
   bool has_more() const;
   [[nodiscard]] char current() const;
   void advance();
};
