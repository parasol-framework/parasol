//********************************************************************************************************************
// XPath Tokenizer Implementation
//
// The tokenizer converts XPath query strings into a sequence of tokens that can be parsed into an abstract
// syntax tree.  This lexical analysis stage handles all XPath token types including operators, literals,
// keywords, identifiers, and special syntax like axis specifiers and node tests.
//
// The tokenizer uses a single-pass character-by-character scan with lookahead to resolve ambiguous tokens
// (such as differentiating between the multiply operator and wildcard, or recognising multi-character
// operators like '::' and '//').  It maintains keyword mappings for language keywords ('and', 'or', 'if', etc.)
// and properly handles string literals, numeric constants, and qualified names.
//
// This implementation focuses on producing clean token streams that simplify the parser's job, allowing
// the parser to focus on grammatical structure rather than low-level character processing.

#include "xpath_tokenizer.h"

#include <parasol/main.h>

#include <array>
#include <cctype>
#include <ranges>
#include <utility>

namespace {

struct KeywordMapping {
   std::string_view text;
   XPathTokenType type;
};

constexpr std::array keyword_mappings{
   KeywordMapping{ "and", XPathTokenType::AND },
   KeywordMapping{ "or", XPathTokenType::OR },
   KeywordMapping{ "not", XPathTokenType::NOT },
   KeywordMapping{ "div", XPathTokenType::DIVIDE },
   KeywordMapping{ "mod", XPathTokenType::MODULO },
   KeywordMapping{ "eq", XPathTokenType::EQ },
   KeywordMapping{ "ne", XPathTokenType::NE },
   KeywordMapping{ "lt", XPathTokenType::LT },
   KeywordMapping{ "le", XPathTokenType::LE },
   KeywordMapping{ "gt", XPathTokenType::GT },
   KeywordMapping{ "ge", XPathTokenType::GE },
   KeywordMapping{ "if", XPathTokenType::IF },
   KeywordMapping{ "then", XPathTokenType::THEN },
   KeywordMapping{ "else", XPathTokenType::ELSE },
   KeywordMapping{ "for", XPathTokenType::FOR },
   KeywordMapping{ "let", XPathTokenType::LET },
   KeywordMapping{ "in", XPathTokenType::IN },
   KeywordMapping{ "return", XPathTokenType::RETURN },
   KeywordMapping{ "some", XPathTokenType::SOME },
   KeywordMapping{ "every", XPathTokenType::EVERY },
   KeywordMapping{ "satisfies", XPathTokenType::SATISFIES }
};

constexpr std::array multi_char_operators{
   std::pair{ std::string_view("//"), XPathTokenType::DOUBLE_SLASH },
   std::pair{ std::string_view(".."), XPathTokenType::DOUBLE_DOT },
   std::pair{ std::string_view("::"), XPathTokenType::AXIS_SEPARATOR },
   std::pair{ std::string_view("!="), XPathTokenType::NOT_EQUALS },
   std::pair{ std::string_view("<="), XPathTokenType::LESS_EQUAL },
   std::pair{ std::string_view(">="), XPathTokenType::GREATER_EQUAL },
   std::pair{ std::string_view(":="), XPathTokenType::ASSIGN }
};

}

bool XPathTokenizer::is_alpha(char c) const
{
   return std::isalpha((unsigned char)(c));
}

bool XPathTokenizer::is_digit(char c) const
{
   return std::isdigit((unsigned char)(c));
}

bool XPathTokenizer::is_alnum(char c) const
{
   return std::isalnum((unsigned char)(c));
}

bool XPathTokenizer::is_whitespace(char c) const
{
   return std::isspace((unsigned char)(c));
}

bool XPathTokenizer::is_name_start_char(char c) const
{
   return is_alpha(c) or c IS '_';
}

bool XPathTokenizer::is_name_char(char c) const
{
   return is_alnum(c) or c IS '_' or c IS '-' or c IS '.';
}

char XPathTokenizer::peek(size_t offset) const
{
   size_t pos = position + offset;
   return pos < length ? input[pos] : '\0';
}

// Advances the position pointer past all consecutive whitespace characters in the input string.

void XPathTokenizer::skip_whitespace()
{
   while (position < length and is_whitespace(input[position]))
   {
      position++;
   }
}

bool XPathTokenizer::match(std::string_view Str)
{
   if (position + Str.size() > length) return false;
   return input.compare(position, Str.size(), Str) IS 0;
}

char XPathTokenizer::current() const
{
   return position < length ? input[position] : '\0';
}

void XPathTokenizer::advance()
{
   if (position < length) position++;
}

bool XPathTokenizer::has_more() const
{
   return position < length;
}

// Main tokenization function that converts an XPath query string into a vector of tokens. Handles operators,
// literals, identifiers, keywords, and special XPath syntax. Resolves ambiguities such as differentiating
// between the multiply operator and wildcard based on context. Tracks bracket and parenthesis depth to
// inform operator disambiguation logic.

std::vector<XPathToken> XPathTokenizer::tokenize(std::string_view XPath)
{
   input = XPath;
   position = 0;
   length = input.size();
   std::vector<XPathToken> tokens;
   tokens.reserve(XPath.size() + 1);
   int bracket_depth = 0;
   int paren_depth = 0;

   while (position < length) {
      skip_whitespace();
      if (position >= length) break;

      if (input[position] IS '*') {
         size_t start = position;
         position++;

         XPathTokenType type = XPathTokenType::WILDCARD;

         bool prev_is_operand = false;
         bool prev_forces_wild = false;

         if (not tokens.empty()) {
            auto prev = tokens.back().type;
            prev_is_operand = (prev IS XPathTokenType::NUMBER) or
                              (prev IS XPathTokenType::STRING) or
                              (prev IS XPathTokenType::IDENTIFIER) or
                              (prev IS XPathTokenType::RPAREN) or
                              (prev IS XPathTokenType::RBRACKET);

            prev_forces_wild = (prev IS XPathTokenType::AT) or
                               (prev IS XPathTokenType::AXIS_SEPARATOR) or
                               (prev IS XPathTokenType::SLASH) or
                               (prev IS XPathTokenType::DOUBLE_SLASH) or
                               (prev IS XPathTokenType::COLON);
         }

         auto is_operand_start = [&](size_t index) {
            if (index >= length) return false;
            char ch = input[index];
            if (is_digit(ch)) return true;
            if (ch IS '.') {
               if (index + 1 < length) {
                  char next = input[index + 1];
                  if (is_digit(next)) return true;
                  if (next IS '.' or next IS '/') return true;
               }
               return true;
            }
            if (ch IS '/') return true;
            if (is_name_start_char(ch)) return true;
            if (ch IS '@' or ch IS '$' or ch IS '(') return true;
            if (ch IS '\'' or ch IS '"') return true;
            return false;
         };

         auto unary_context_before = [&](size_t index) {
            size_t prev = index;
            while (prev > 0 and is_whitespace(input[prev - 1])) prev--;
            if (prev IS 0) return true;

            char before = input[prev - 1];
            if (before IS '(' or before IS '[') return true;
            if (before IS '@' or before IS '$' or before IS ',' or before IS ':') return true;
            if (before IS '+' or before IS '-' or before IS '*' or before IS '/' or before IS '|' or before IS '!' or before IS '<' or before IS '>' or before IS '=') return true;
            return false;
         };

         auto next_operand_index = [&]() -> size_t {
            size_t lookahead = position;
            while (lookahead < length and is_whitespace(input[lookahead])) lookahead++;
            if (lookahead >= length) return std::string_view::npos;

            char next_char = input[lookahead];
            if (next_char IS '-' or next_char IS '+') {
               if (not unary_context_before(lookahead)) return std::string_view::npos;
               size_t after_sign = lookahead + 1;
               while (after_sign < length and is_whitespace(input[after_sign])) after_sign++;
               if (after_sign >= length) return std::string_view::npos;
               return is_operand_start(after_sign) ? after_sign : std::string_view::npos;
            }

            return is_operand_start(lookahead) ? lookahead : std::string_view::npos;
         };

         size_t operand_index = next_operand_index();
         bool inside_structural_context = (bracket_depth > 0) or (paren_depth > 0);

         bool prev_allows_binary = false;
         if (not tokens.empty()) {
            auto prev_type = tokens.back().type;

            if ((prev_type IS XPathTokenType::IDENTIFIER) or
                (prev_type IS XPathTokenType::RPAREN) or
                (prev_type IS XPathTokenType::RBRACKET) or
                (prev_type IS XPathTokenType::WILDCARD)) {
               prev_allows_binary = true;
            }
            else if ((prev_type IS XPathTokenType::NUMBER) or (prev_type IS XPathTokenType::STRING)) {
               if (inside_structural_context) prev_allows_binary = true;
            }
         }

         if (prev_is_operand and prev_allows_binary and !prev_forces_wild and operand_index != std::string_view::npos) {
            type = XPathTokenType::MULTIPLY;
         }

         tokens.emplace_back(type, input.substr(start, 1), start, 1);
      }
      else {
         XPathToken token = scan_operator();
         if (token.type IS XPathTokenType::UNKNOWN) {
            if (current() IS '\'' or current() IS '"') {
               token = scan_string(current());
            }
            else if (is_digit(current()) or (current() IS '.' and is_digit(peek(1)))) {
               token = scan_number();
            }
            else if (is_name_start_char(current())) {
               token = scan_identifier();
            }
            else {
               size_t start = position;
               auto unknown_char = input.substr(start, 1);
               advance();
               token = XPathToken(XPathTokenType::UNKNOWN, unknown_char, start, 1);
            }
         }

         if (token.type IS XPathTokenType::LBRACKET) bracket_depth++;
         else if (token.type IS XPathTokenType::RBRACKET and bracket_depth > 0) bracket_depth--;
         else if (token.type IS XPathTokenType::LPAREN) paren_depth++;
         else if (token.type IS XPathTokenType::RPAREN and paren_depth > 0) paren_depth--;

         tokens.push_back(std::move(token));
      }
   }

   tokens.emplace_back(XPathTokenType::END_OF_INPUT, std::string_view(""), position, 0);
   return tokens;
}

// Scans and tokenizes an identifier or keyword from the current position. Checks the scanned name against
// the keyword mappings to determine if it's a reserved word (like 'and', 'or', 'if') or a regular identifier.

XPathToken XPathTokenizer::scan_identifier()
{
   size_t start = position;

   while (position < length and is_name_char(input[position])) {
      position++;
   }

   auto identifier = input.substr(start, position - start);

   auto match = std::ranges::find_if(keyword_mappings, [identifier](const KeywordMapping &entry) {
      return identifier IS entry.text;
   });

   XPathTokenType type = (match != keyword_mappings.end()) ? match->type : XPathTokenType::IDENTIFIER;
   return XPathToken(type, identifier, start, position - start);
}

// Scans numeric literals from the input, handling both integers and decimal numbers. Parses consecutive
// digits and at most one decimal point to form a valid numeric token.

XPathToken XPathTokenizer::scan_number()
{
   size_t start = position;
   bool seen_dot = false;
   while (position < length) {
      char current = input[position];
      if (is_digit(current)) {
         position++;
         continue;
      }

      if (not seen_dot and current IS '.') {
         seen_dot = true;
         position++;
         continue;
      }
      break;
   }

   auto number_view = input.substr(start, position - start);
   return XPathToken(XPathTokenType::NUMBER, number_view, start, position - start);
}

// Scans string literals enclosed in quotes (single or double). Handles escape sequences for quote characters,
// backslashes, and wildcards. Uses optimised fast-path for strings without escapes, otherwise builds the
// unescaped string content with proper escape processing.

XPathToken XPathTokenizer::scan_string(char QuoteChar)
{
   size_t start = position;
   position++;
   size_t content_start = position;

   bool has_escapes = false;
   size_t scan_pos = position;
   while ((scan_pos < length) and (input[scan_pos] != QuoteChar)) {
      if (input[scan_pos] IS '\\') {
         has_escapes = true;
         break;
      }
      scan_pos++;
   }

   if (not has_escapes) {
      size_t content_end = scan_pos;
      position = scan_pos;
      if (position < length) position++;

      auto string_content = input.substr(content_start, content_end - content_start);
      return XPathToken(XPathTokenType::STRING, string_content, start, position - start);
   }

   std::string value;
   value.reserve(scan_pos - content_start + 10);

   while ((position < length) and (input[position] != QuoteChar))
   {
      if (input[position] IS '\\' and position + 1 < length)
      {
         position++;
         char escaped = input[position];
         if (escaped IS QuoteChar or escaped IS '\\' or escaped IS '*')
         {
            value += escaped;
         }
         else
         {
            value += '\\';
            value += escaped;
         }
      }
      else value += input[position];

      position++;
   }

   if (position < length) position++;

   return XPathToken(XPathTokenType::STRING, std::move(value), start, position - start);
}

// Scans operator tokens including multi-character operators (like '//', '::', '!=') and single-character
// operators. Returns the appropriate token type for recognised operators, or UNKNOWN for unrecognised characters.

XPathToken XPathTokenizer::scan_operator()
{
   size_t start = position;
   char ch = input[position];

   for (const auto &[text, token_type] : multi_char_operators)
   {
      size_t length_required = text.size();
      if (position + length_required > length) continue;
      if (input.compare(position, length_required, text) IS 0)
      {
         position += length_required;
         return XPathToken(token_type, input.substr(start, length_required), start, length_required);
      }
   }

   auto single_char = input.substr(position, 1);
   switch (ch)
   {
      case '/': position++; return XPathToken(XPathTokenType::SLASH, single_char, start, 1);
      case '.': position++; return XPathToken(XPathTokenType::DOT, single_char, start, 1);
      case '*': position++; return XPathToken(XPathTokenType::WILDCARD, single_char, start, 1);
      case '[': position++; return XPathToken(XPathTokenType::LBRACKET, single_char, start, 1);
      case ']': position++; return XPathToken(XPathTokenType::RBRACKET, single_char, start, 1);
      case '(': position++; return XPathToken(XPathTokenType::LPAREN, single_char, start, 1);
      case ')': position++; return XPathToken(XPathTokenType::RPAREN, single_char, start, 1);
      case '@': position++; return XPathToken(XPathTokenType::AT, single_char, start, 1);
      case ',': position++; return XPathToken(XPathTokenType::COMMA, single_char, start, 1);
      case '|': position++; return XPathToken(XPathTokenType::PIPE, single_char, start, 1);
      case '=': position++; return XPathToken(XPathTokenType::EQUALS, single_char, start, 1);
      case '<': position++; return XPathToken(XPathTokenType::LESS_THAN, single_char, start, 1);
      case '>': position++; return XPathToken(XPathTokenType::GREATER_THAN, single_char, start, 1);
      case '+': position++; return XPathToken(XPathTokenType::PLUS, single_char, start, 1);
      case '-': position++; return XPathToken(XPathTokenType::MINUS, single_char, start, 1);
      case ':': position++; return XPathToken(XPathTokenType::COLON, single_char, start, 1);
      case '$': position++; return XPathToken(XPathTokenType::DOLLAR, single_char, start, 1);
   }

   return XPathToken(XPathTokenType::UNKNOWN, std::string_view(""), start, 0);
}
