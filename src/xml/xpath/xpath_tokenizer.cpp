#include "xpath_tokenizer.h"

#include <parasol/main.h>

#include <cctype>
#include <mutex>

ankerl::unordered_dense::map<std::string_view, std::string> XPathTokenizer::interned_strings;

void XPathTokenizer::initialize_interned_strings()
{
   interned_strings["and"] = "and";
   interned_strings["or"] = "or";
   interned_strings["not"] = "not";
   interned_strings["div"] = "div";
   interned_strings["mod"] = "mod";
   interned_strings["some"] = "some";
   interned_strings["every"] = "every";
   interned_strings["satisfies"] = "satisfies";
   interned_strings["let"] = "let";

   interned_strings["node"] = "node";
   interned_strings["text"] = "text";
   interned_strings["comment"] = "comment";
   interned_strings["processing-instruction"] = "processing-instruction";

   interned_strings["child"] = "child";
   interned_strings["parent"] = "parent";
   interned_strings["ancestor"] = "ancestor";
   interned_strings["descendant"] = "descendant";
   interned_strings["following"] = "following";
   interned_strings["preceding"] = "preceding";
   interned_strings["following-sibling"] = "following-sibling";
   interned_strings["preceding-sibling"] = "preceding-sibling";
   interned_strings["attribute"] = "attribute";
   interned_strings["namespace"] = "namespace";
   interned_strings["self"] = "self";
   interned_strings["descendant-or-self"] = "descendant-or-self";
   interned_strings["ancestor-or-self"] = "ancestor-or-self";

   interned_strings["last"] = "last";
   interned_strings["position"] = "position";
   interned_strings["count"] = "count";
   interned_strings["name"] = "name";
   interned_strings["local-name"] = "local-name";
   interned_strings["namespace-uri"] = "namespace-uri";
   interned_strings["string"] = "string";
   interned_strings["concat"] = "concat";
   interned_strings["starts-with"] = "starts-with";
   interned_strings["contains"] = "contains";
   interned_strings["substring"] = "substring";
   interned_strings["substring-before"] = "substring-before";
   interned_strings["substring-after"] = "substring-after";
   interned_strings["normalize-space"] = "normalize-space";
   interned_strings["translate"] = "translate";
   interned_strings["boolean"] = "boolean";
   interned_strings["number"] = "number";
   interned_strings["sum"] = "sum";
   interned_strings["floor"] = "floor";
   interned_strings["ceiling"] = "ceiling";
   interned_strings["round"] = "round";
   interned_strings["base-uri"] = "base-uri";
   interned_strings["data"] = "data";
   interned_strings["document-uri"] = "document-uri";
   interned_strings["node-name"] = "node-name";
   interned_strings["nilled"] = "nilled";
   interned_strings["static-base-uri"] = "static-base-uri";
   interned_strings["default-collation"] = "default-collation";
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

std::vector<XPathToken> XPathTokenizer::tokenize(std::string_view XPath)
{
   static std::once_flag is_once;
   std::call_once(is_once, &XPathTokenizer::initialize_interned_strings);

   input = XPath;
   position = 0;
   length = input.size();
   std::vector<XPathToken> tokens;
   tokens.reserve(XPath.size() + 1);
   int bracket_depth = 0;
   int paren_depth = 0;

   while (position < length)
   {
      skip_whitespace();
      if (position >= length) break;

      if (input[position] IS '*')
      {
         size_t start = position;
         position++;

         XPathTokenType type = XPathTokenType::WILDCARD;

         bool prev_is_operand = false;
         bool prev_forces_wild = false;

         if (!tokens.empty())
         {
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

         auto is_operand_start = [&](size_t index)
         {
            if (index >= length) return false;
            char ch = input[index];
            if (is_digit(ch)) return true;
            if (ch IS '.')
            {
               if (index + 1 < length)
               {
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

         auto unary_context_before = [&](size_t index)
         {
            size_t prev = index;
            while (prev > 0 and is_whitespace(input[prev - 1])) prev--;
            if (prev IS 0) return true;

            char before = input[prev - 1];
            if (before IS '(' or before IS '[') return true;
            if (before IS '@' or before IS '$' or before IS ',' or before IS ':') return true;
            if (before IS '+' or before IS '-' or before IS '*' or before IS '/' or before IS '|' or before IS '!' or before IS '<' or before IS '>' or before IS '=') return true;
            return false;
         };

         auto next_operand_index = [&]() -> size_t
         {
            size_t lookahead = position;
            while (lookahead < length and is_whitespace(input[lookahead])) lookahead++;
            if (lookahead >= length) return std::string_view::npos;

            char next_char = input[lookahead];
            if (next_char IS '-' or next_char IS '+')
            {
               if (!unary_context_before(lookahead)) return std::string_view::npos;
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
         if (!tokens.empty())
         {
            auto prev_type = tokens.back().type;

            if ((prev_type IS XPathTokenType::IDENTIFIER) or
                (prev_type IS XPathTokenType::RPAREN) or
                (prev_type IS XPathTokenType::RBRACKET) or
                (prev_type IS XPathTokenType::WILDCARD))
            {
               prev_allows_binary = true;
            }
            else if ((prev_type IS XPathTokenType::NUMBER) or (prev_type IS XPathTokenType::STRING))
            {
               if (inside_structural_context) prev_allows_binary = true;
            }
         }

         if (prev_is_operand and prev_allows_binary and !prev_forces_wild and operand_index != std::string_view::npos)
         {
            type = XPathTokenType::MULTIPLY;
         }

         auto wildcard_char = input.substr(start, 1);
         tokens.emplace_back(type, wildcard_char, start, 1);
      }
      else
      {
         XPathToken token = scan_operator();
         if (token.type IS XPathTokenType::UNKNOWN)
         {
            if (current() IS '\'' or current() IS '"')
            {
               token = scan_string(current());
            }
            else if (is_digit(current()) or (current() IS '.' and is_digit(peek(1))))
            {
               token = scan_number();
            }
            else if (is_name_start_char(current()))
            {
               token = scan_identifier();
            }
            else
            {
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

XPathToken XPathTokenizer::scan_identifier()
{
   size_t start = position;

   while (position < length and is_name_char(input[position]))
   {
      position++;
   }

   auto identifier = input.substr(start, position - start);

   XPathTokenType type = XPathTokenType::IDENTIFIER;
   if (identifier IS "and") type = XPathTokenType::AND;
   else if (identifier IS "or") type = XPathTokenType::OR;
   else if (identifier IS "not") type = XPathTokenType::NOT;
   else if (identifier IS "div") type = XPathTokenType::DIVIDE;
   else if (identifier IS "mod") type = XPathTokenType::MODULO;
   else if (identifier IS "eq") type = XPathTokenType::EQ;
   else if (identifier IS "ne") type = XPathTokenType::NE;
   else if (identifier IS "lt") type = XPathTokenType::LT;
   else if (identifier IS "le") type = XPathTokenType::LE;
   else if (identifier IS "gt") type = XPathTokenType::GT;
   else if (identifier IS "ge") type = XPathTokenType::GE;
   else if (identifier IS "if") type = XPathTokenType::IF;
   else if (identifier IS "then") type = XPathTokenType::THEN;
   else if (identifier IS "else") type = XPathTokenType::ELSE;
   else if (identifier IS "for") type = XPathTokenType::FOR;
   else if (identifier IS "let") type = XPathTokenType::LET;
   else if (identifier IS "in") type = XPathTokenType::IN;
   else if (identifier IS "return") type = XPathTokenType::RETURN;
   else if (identifier IS "some") type = XPathTokenType::SOME;
   else if (identifier IS "every") type = XPathTokenType::EVERY;
   else if (identifier IS "satisfies") type = XPathTokenType::SATISFIES;

   return XPathToken(type, identifier, start, position - start);
}

XPathToken XPathTokenizer::scan_number()
{
   size_t start = position;
   bool seen_dot = false;
   while (position < length)
   {
      char current = input[position];
      if (is_digit(current))
      {
         position++;
         continue;
      }
      if (!seen_dot and current IS '.')
      {
         seen_dot = true;
         position++;
         continue;
      }
      break;
   }

   auto number_view = input.substr(start, position - start);
   return XPathToken(XPathTokenType::NUMBER, number_view, start, position - start);
}

XPathToken XPathTokenizer::scan_string(char QuoteChar)
{
   size_t start = position;
   position++;
   size_t content_start = position;

   bool has_escapes = false;
   size_t scan_pos = position;
   while ((scan_pos < length) and (input[scan_pos] != QuoteChar))
   {
      if (input[scan_pos] IS '\\')
      {
         has_escapes = true;
         break;
      }
      scan_pos++;
   }

   if (!has_escapes)
   {
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

XPathToken XPathTokenizer::scan_operator()
{
   size_t start = position;
   char ch = input[position];

   if (position + 1 < length)
   {
      std::string_view two_char = input.substr(position, 2);
      if (two_char IS "//")
      {
         position += 2;
         return XPathToken(XPathTokenType::DOUBLE_SLASH, two_char, start, 2);
      }
      else if (two_char IS "..")
      {
         position += 2;
         return XPathToken(XPathTokenType::DOUBLE_DOT, two_char, start, 2);
      }
      else if (two_char IS "::")
      {
         position += 2;
         return XPathToken(XPathTokenType::AXIS_SEPARATOR, two_char, start, 2);
      }
      else if (two_char IS "!=")
      {
         position += 2;
         return XPathToken(XPathTokenType::NOT_EQUALS, two_char, start, 2);
      }
      else if (two_char IS "<=")
      {
         position += 2;
         return XPathToken(XPathTokenType::LESS_EQUAL, two_char, start, 2);
      }
      else if (two_char IS ">=")
      {
         position += 2;
         return XPathToken(XPathTokenType::GREATER_EQUAL, two_char, start, 2);
      }
      else if (two_char IS ":=")
      {
         position += 2;
         return XPathToken(XPathTokenType::ASSIGN, two_char, start, 2);
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

