//********************************************************************************************************************
// XPath Tokenizer and Parser Implementation
//
// The parser transforms raw XPath source into the structured representation consumed by the evaluator.
// It begins with a character-level tokenizer that understands XPath's contextual grammar—treating the
// asterisk differently depending on predicate state, tracking bracket and parenthesis depth, and
// recognising identifiers, literals, and operators.  The resulting token stream feeds a recursive
// descent parser that builds an explicit AST mirroring the production rules handled by the evaluator.
//
// The code is split between lightweight lexical helpers (contained in the tokenizer) and the parsing
// routines that emit XPathNode instances.  Keeping both stages together simplifies debugging: the
// parser can make nuanced decisions based on how tokens were generated, and the evaluator benefits
// from a consistent interpretation of ambiguous constructs.

#include <cctype>
#include <mutex>

// Static string interning storage
ankerl::unordered_dense::map<std::string_view, std::string> XPathTokenizer::interned_strings;

void XPathTokenizer::initialize_interned_strings() {
   // Common XPath keywords and operators
   interned_strings["and"] = "and";
   interned_strings["or"] = "or";
   interned_strings["not"] = "not";
   interned_strings["div"] = "div";
   interned_strings["mod"] = "mod";

   // Common node type tests
   interned_strings["node"] = "node";
   interned_strings["text"] = "text";
   interned_strings["comment"] = "comment";
   interned_strings["processing-instruction"] = "processing-instruction";

   // Common axis names
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

   // Common function names
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
}

bool XPathTokenizer::is_alpha(char c) const {
   return std::isalpha((unsigned char)(c));
}

bool XPathTokenizer::is_digit(char c) const {
   return std::isdigit((unsigned char)(c));
}

bool XPathTokenizer::is_alnum(char c) const {
   return std::isalnum((unsigned char)(c));
}

bool XPathTokenizer::is_whitespace(char c) const {
   return std::isspace((unsigned char)(c));
}

bool XPathTokenizer::is_name_start_char(char c) const {
   return is_alpha(c) or c IS '_';
}

bool XPathTokenizer::is_name_char(char c) const {
   return is_alnum(c) or c IS '_' or c IS '-' or c IS '.';
}

char XPathTokenizer::peek(size_t offset) const {
   size_t pos = position + offset;
   return pos < length ? input[pos] : '\0';
}

void XPathTokenizer::skip_whitespace() {
   while (position < length and is_whitespace(input[position])) {
      position++;
   }
}

bool XPathTokenizer::match(std::string_view Str) {
   if (position + Str.size() > length) return false;
   return input.compare(position, Str.size(), Str) IS 0;
}

char XPathTokenizer::current() const {
   return position < length ? input[position] : '\0';
}

void XPathTokenizer::advance() {
   if (position < length) position++;
}

bool XPathTokenizer::has_more() const {
   return position < length;
}

//********************************************************************************************************************
// Produce a token stream while preserving context so operators like '*' are classified correctly.

std::vector<XPathToken> XPathTokenizer::tokenize(std::string_view XPath) {
   static std::once_flag is_once;
   std::call_once(is_once, &XPathTokenizer::initialize_interned_strings);

   input = XPath;
   position = 0;
   length = input.size();
   std::vector<XPathToken> tokens;
   tokens.reserve(XPath.size() + 1);
   int bracket_depth = 0; // '[' and ']'
   int paren_depth = 0;   // '(' and ')'

   while (position < length) {
      skip_whitespace();
      if (position >= length) break;

      // Context-aware handling for '*': wildcard vs multiply.  Wildcards are permitted for use as string comparators,
      // e.g. /menu/*[@id='5'], /root/section[@*="alpha"], /*ab/, /A*B/ 
      // In other contexts (e.g. /menu/thing[price*2>10]) it is consumed as a binary operator unless enclosed in quotes.

      if (input[position] IS '*') {
         size_t start = position;
         position++;

         XPathTokenType type = XPathTokenType::WILDCARD;

         bool prev_is_operand = false;
         bool prev_forces_wild = false;

         if (!tokens.empty()) {
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
         if (!tokens.empty()) {
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

         if (prev_is_operand and prev_allows_binary and !prev_forces_wild and
             operand_index != std::string_view::npos) {
            type = XPathTokenType::MULTIPLY;
         }

         auto wildcard_char = input.substr(start, 1);
         tokens.emplace_back(type, wildcard_char, start, 1);
      }
      else {
         XPathToken token = scan_operator();
         if (token.type IS XPathTokenType::UNKNOWN) {
            if (current() IS '\'' or current() IS '"') {
               token = scan_string(current());
            } else if (is_digit(current()) or (current() IS '.' and is_digit(peek(1)))) {
               token = scan_number();
            } else if (is_name_start_char(current())) {
               token = scan_identifier();
            } else {
               // Unknown character
               size_t start = position;
               auto unknown_char = input.substr(start, 1);
               advance();
               token = XPathToken(XPathTokenType::UNKNOWN, unknown_char, start, 1);
            }
         }

         // Track bracket/paren depth for context
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

//********************************************************************************************************************

XPathToken XPathTokenizer::scan_identifier() {
   size_t start = position;

   while (position < length and is_name_char(input[position])) {
      position++;
   }

   auto identifier = input.substr(start, position - start);

   // Check for keywords
   XPathTokenType type = XPathTokenType::IDENTIFIER;
   if (identifier IS "and") type = XPathTokenType::AND;
   else if (identifier IS "or") type = XPathTokenType::OR;
   else if (identifier IS "not") type = XPathTokenType::NOT;
   else if (identifier IS "div") type = XPathTokenType::DIVIDE;
   else if (identifier IS "mod") type = XPathTokenType::MODULO;

   // Use string_view directly - no copying
   return XPathToken(type, identifier, start, position - start);
}

//********************************************************************************************************************

XPathToken XPathTokenizer::scan_number() {
   size_t start = position;
   bool seen_dot = false;
   while (position < length) {
      char current = input[position];
      if (is_digit(current)) {
         position++;
         continue;
      }
      if (!seen_dot and current IS '.') {
         seen_dot = true;
         position++;
         continue;
      }
      break;
   }

   auto number_view = input.substr(start, position - start);
   // Use string_view directly - no copying for numbers
   return XPathToken(XPathTokenType::NUMBER, number_view, start, position - start);
}

//********************************************************************************************************************

XPathToken XPathTokenizer::scan_string(char QuoteChar) {
   size_t start = position;
   position++; // Skip opening quote
   size_t content_start = position;

   // First pass: check if string contains escape sequences
   bool has_escapes = false;
   size_t scan_pos = position;
   while ((scan_pos < length) and (input[scan_pos] != QuoteChar)) {
      if (input[scan_pos] IS '\\') {
         has_escapes = true;
         break;
      }
      scan_pos++;
   }

   if (!has_escapes) {
      // Fast path: no escape sequences, use string_view directly
      size_t content_end = scan_pos;
      position = scan_pos;
      if (position < length) position++; // Skip closing quote

      auto string_content = input.substr(content_start, content_end - content_start);
      return XPathToken(XPathTokenType::STRING, string_content, start, position - start);
   }

   // Slow path: process escape sequences
   std::string value;
   value.reserve(scan_pos - content_start + 10); // Reserve with some buffer for escapes

   while ((position < length) and (input[position] != QuoteChar)) {
      if (input[position] IS '\\' and position + 1 < length) {
         // Handle escape sequences
         position++;
         char escaped = input[position];
         if (escaped IS QuoteChar or escaped IS '\\' or escaped IS '*') {
            value += escaped;
         } 
         else {
            value += '\\';
            value += escaped;
         }
      } 
      else value += input[position];

      position++;
   }

   if (position < length) position++; // Skip closing quote

   return XPathToken(XPathTokenType::STRING, std::move(value), start, position - start);
}

//********************************************************************************************************************

XPathToken XPathTokenizer::scan_operator() 
{
   size_t start = position;
   char ch = input[position];

   // Two-character operators
   if (position + 1 < length) {
      std::string_view two_char = input.substr(position, 2);
      if (two_char IS "//") {
         position += 2;
         return XPathToken(XPathTokenType::DOUBLE_SLASH, two_char, start, 2);
      }
      else if (two_char IS "..") {
         position += 2;
         return XPathToken(XPathTokenType::DOUBLE_DOT, two_char, start, 2);
      }
      else if (two_char IS "::") {
         position += 2;
         return XPathToken(XPathTokenType::AXIS_SEPARATOR, two_char, start, 2);
      }
      else if (two_char IS "!=") {
         position += 2;
         return XPathToken(XPathTokenType::NOT_EQUALS, two_char, start, 2);
      }
      else if (two_char IS "<=") {
         position += 2;
         return XPathToken(XPathTokenType::LESS_EQUAL, two_char, start, 2);
      }
      else if (two_char IS ">=") {
         position += 2;
         return XPathToken(XPathTokenType::GREATER_EQUAL, two_char, start, 2);
      }
   }

   // Single character operators - use string_view to avoid copying
   auto single_char = input.substr(position, 1);
   switch (ch) {
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

   // Unknown token
   return XPathToken(XPathTokenType::UNKNOWN, std::string_view(""), start, 0);
}

//********************************************************************************************************************
// XPathParser Implementation

std::unique_ptr<XPathNode> XPathParser::parse(const std::vector<XPathToken> &TokenList) 
{
   tokens = TokenList;
   current_token = 0;
   errors.clear();
   auto expression = parse_expr();

   if (!is_at_end()) {
      XPathToken token = peek();
      std::string token_text(token.value);
      if (token_text.empty()) token_text = "<unexpected>";
      report_error("Unexpected token '" + token_text + "' in XPath expression");
   }

   if (has_errors() or (!expression)) return nullptr;

   if (expression->type IS XPathNodeType::LOCATION_PATH) {
      return expression;
   }

   if (expression->type IS XPathNodeType::PATH) {
      if (expression->child_count() IS 1) {
         auto child = std::move(expression->children[0]);
         if (child and (child->type IS XPathNodeType::LOCATION_PATH)) return child;
      }
   }

   auto root = std::make_unique<XPathNode>(XPathNodeType::EXPRESSION);
   root->add_child(std::move(expression));
   return root;
}

//********************************************************************************************************************

bool XPathParser::check(XPathTokenType Type) const {
   return peek().type IS Type;
}

bool XPathParser::match(XPathTokenType Type) {
   if (check(Type)) {
      advance();
      return true;
   }
   return false;
}

const XPathToken & XPathParser::peek() const 
{
   return current_token < tokens.size() ? tokens[current_token] : tokens.back(); // END_OF_INPUT
}

const XPathToken & XPathParser::previous() const 
{
   return tokens[current_token - 1];
}

bool XPathParser::is_at_end() const 
{
   return peek().type IS XPathTokenType::END_OF_INPUT;
}

void XPathParser::advance() 
{
   if (!is_at_end()) current_token++;
}

bool XPathParser::is_step_start_token(XPathTokenType type) const 
{
   switch (type) {
      case XPathTokenType::DOT:
      case XPathTokenType::DOUBLE_DOT:
      case XPathTokenType::AT:
      case XPathTokenType::IDENTIFIER:
      case XPathTokenType::WILDCARD:
         return true;
      default:
         return false;
   }
}

void XPathParser::report_error(std::string_view Message) 
{
   errors.emplace_back(Message);
}

bool XPathParser::has_errors() const 
{
   return !errors.empty();
}

std::vector<std::string> XPathParser::get_errors() const 
{
   return errors;
}

std::unique_ptr<XPathNode> XPathParser::create_binary_op(std::unique_ptr<XPathNode> Left, const XPathToken &Op, std::unique_ptr<XPathNode> Right) 
{
   auto binary_op = std::make_unique<XPathNode>(XPathNodeType::BINARY_OP, std::string(Op.value));
   binary_op->add_child(std::move(Left));
   binary_op->add_child(std::move(Right));
   return binary_op;
}

std::unique_ptr<XPathNode> XPathParser::create_unary_op(const XPathToken &Op, std::unique_ptr<XPathNode> Operand) {
   auto unary_op = std::make_unique<XPathNode>(XPathNodeType::UNARY_OP, std::string(Op.value));
   unary_op->add_child(std::move(Operand));
   return unary_op;
}

//********************************************************************************************************************
// Grammar implementation

std::unique_ptr<XPathNode> XPathParser::parse_location_path() {
   auto path = std::make_unique<XPathNode>(XPathNodeType::LOCATION_PATH);

   bool is_absolute = false;
   if (match(XPathTokenType::SLASH)) {
      is_absolute = true;
      path->add_child(std::make_unique<XPathNode>(XPathNodeType::ROOT, "/"));
   }
   else if (match(XPathTokenType::DOUBLE_SLASH)) {
      is_absolute = true;
      path->add_child(std::make_unique<XPathNode>(XPathNodeType::ROOT, "//"));
   }

   // Parse steps
   while (!is_at_end()) {
      if (check(XPathTokenType::RBRACKET) or
          check(XPathTokenType::RPAREN) or
          check(XPathTokenType::COMMA) or
          check(XPathTokenType::PIPE)) {
         break;
      }

      if (!is_step_start_token(peek().type)) break;

      auto step = parse_step();
      if (step) path->add_child(std::move(step));
      else break;

      if (match(XPathTokenType::SLASH)) continue;

      if (match(XPathTokenType::DOUBLE_SLASH)) {
         auto descendant_step = std::make_unique<XPathNode>(XPathNodeType::STEP);
         descendant_step->add_child(std::make_unique<XPathNode>(XPathNodeType::AXIS_SPECIFIER, "descendant-or-self"));
         descendant_step->add_child(std::make_unique<XPathNode>(XPathNodeType::NODE_TYPE_TEST, "node"));
         path->add_child(std::move(descendant_step));
         continue;
      }

      break;
   }

   return path;
}

//********************************************************************************************************************

std::unique_ptr<XPathNode> XPathParser::parse_step() {
   auto step = std::make_unique<XPathNode>(XPathNodeType::STEP);

   // Handle abbreviated steps
   if (check(XPathTokenType::DOT)) {
      advance();
      step->add_child(std::make_unique<XPathNode>(XPathNodeType::AXIS_SPECIFIER, "self"));
      step->add_child(std::make_unique<XPathNode>(XPathNodeType::NODE_TYPE_TEST, "node"));
      return step;
   }

   if (check(XPathTokenType::DOUBLE_DOT)) {
      advance();
      step->add_child(std::make_unique<XPathNode>(XPathNodeType::AXIS_SPECIFIER, "parent"));
      step->add_child(std::make_unique<XPathNode>(XPathNodeType::NODE_TYPE_TEST, "node"));
      return step;
   }

   // Handle explicit axis specifiers (axis::node-test)
   if (check(XPathTokenType::IDENTIFIER)) {
      // Look ahead for axis separator
      if (current_token + 1 < tokens.size() and tokens[current_token + 1].type IS XPathTokenType::AXIS_SEPARATOR) {
         std::string axis_name(peek().value);
         advance(); // consume axis name
         advance(); // consume "::"
         auto axis = std::make_unique<XPathNode>(XPathNodeType::AXIS_SPECIFIER, axis_name);
         step->add_child(std::move(axis));
      }
   }
   else if (match(XPathTokenType::AT)) { // Handle attribute axis (@)
      auto axis = std::make_unique<XPathNode>(XPathNodeType::AXIS_SPECIFIER, "attribute");
      step->add_child(std::move(axis));
   }

   // Parse node test
   auto node_test = parse_node_test();
   if (node_test) step->add_child(std::move(node_test));

   // Parse predicates (square brackets and round brackets)
   while (true) {
      if (check(XPathTokenType::LBRACKET)) {
         auto predicate = parse_predicate();
         if (predicate) step->add_child(std::move(predicate));
         else break;
      }
      else break;
   }

   return step;
}

//********************************************************************************************************************

std::unique_ptr<XPathNode> XPathParser::parse_node_test() {
   if (check(XPathTokenType::WILDCARD)) {
      advance();
      return std::make_unique<XPathNode>(XPathNodeType::WILDCARD, "*");
   }
   else if (check(XPathTokenType::IDENTIFIER)) {
      std::string name(peek().value);

      bool is_node_type = false;
      if ((name IS "node") or (name IS "text") or (name IS "comment") or (name IS "processing-instruction")) {
         if ((current_token + 1 < tokens.size()) and (tokens[current_token + 1].type IS XPathTokenType::LPAREN)) {
            is_node_type = true;
         }
      }

      if (is_node_type) {
         advance(); // consume identifier

         if (!match(XPathTokenType::LPAREN)) {
            report_error("Expected '(' after node type test");
            return nullptr;
         }

         if (name IS "processing-instruction") {
            std::string target;

            if (!check(XPathTokenType::RPAREN)) {
               if (check(XPathTokenType::STRING) or check(XPathTokenType::IDENTIFIER)) {
                  target = peek().value;
                  advance();
               }
               else report_error("Expected literal target in processing-instruction()");
            }

            if (!match(XPathTokenType::RPAREN)) {
               report_error("Expected ')' after processing-instruction() test");
            }

            return std::make_unique<XPathNode>(XPathNodeType::PROCESSING_INSTRUCTION_TEST, target);
         }

         if (!match(XPathTokenType::RPAREN)) {
            report_error("Expected ')' after node type test");
         }

         return std::make_unique<XPathNode>(XPathNodeType::NODE_TYPE_TEST, name);
      }

      advance();

      if (check(XPathTokenType::COLON)) {
         if (current_token + 1 < tokens.size() and tokens[current_token + 1].type IS XPathTokenType::IDENTIFIER) {
            advance(); // consume ':'
            name += ':';
            name += peek().value;
            advance();
         }
      }

      return std::make_unique<XPathNode>(XPathNodeType::NAME_TEST, name);
   }

   return nullptr;
}

//********************************************************************************************************************

std::unique_ptr<XPathNode> XPathParser::parse_predicate() {
   if (!match(XPathTokenType::LBRACKET)) return nullptr;

   auto predicate = std::make_unique<XPathNode>(XPathNodeType::PREDICATE);

   if (check(XPathTokenType::NUMBER)) {
      // Index predicate [1], [2], etc.
      std::string index(peek().value);
      advance();
      predicate->add_child(std::make_unique<XPathNode>(XPathNodeType::NUMBER, index));
   }
   else if (check(XPathTokenType::EQUALS)) {
      // Content predicate [=value]
      advance(); // consume =
      auto content_value = parse_predicate_value();

      if (content_value) {
         auto content_test = std::make_unique<XPathNode>(XPathNodeType::BINARY_OP, "content-equals");
         content_test->add_child(std::move(content_value));
         predicate->add_child(std::move(content_test));
      }
      else report_error("Expected literal after '=' in content predicate");
   }
   else if (check(XPathTokenType::AT)) {
      size_t attribute_token_index = current_token;
      advance(); // consume '@'

      bool handled_attribute = false;
      std::unique_ptr<XPathNode> attribute_expression;

      if (check(XPathTokenType::IDENTIFIER) or check(XPathTokenType::WILDCARD)) {
         std::string attr_name(peek().value);
         advance();

         if (match(XPathTokenType::COLON)) {
            attr_name += ':';
            if (check(XPathTokenType::IDENTIFIER) or check(XPathTokenType::WILDCARD)) {
               attr_name += peek().value;
               advance();
            } else {
               report_error("Expected identifier or wildcard after ':' in attribute name");
            }
         }

         if (check(XPathTokenType::EQUALS) or check(XPathTokenType::RBRACKET)) {
            if (match(XPathTokenType::EQUALS)) {
               auto attr_value = parse_predicate_value();

               if (attr_value) {
                  auto attr_test = std::make_unique<XPathNode>(XPathNodeType::BINARY_OP, "attribute-equals");
                  attr_test->add_child(std::make_unique<XPathNode>(XPathNodeType::LITERAL, attr_name));
                  attr_test->add_child(std::move(attr_value));
                  attribute_expression = std::move(attr_test);
               }
               else report_error("Expected literal after '=' in attribute predicate");
            }
            else {
               auto attr_exists = std::make_unique<XPathNode>(XPathNodeType::BINARY_OP, "attribute-exists");
               attr_exists->add_child(std::make_unique<XPathNode>(XPathNodeType::LITERAL, attr_name));
               attribute_expression = std::move(attr_exists);
            }

            if (attribute_expression and check(XPathTokenType::RBRACKET)) {
               predicate->add_child(std::move(attribute_expression));
               handled_attribute = true;
            }
         }
      }

      if (!handled_attribute) {
         current_token = attribute_token_index;
         auto expression = parse_expr();
         if (expression) predicate->add_child(std::move(expression));
      }
   }
   else {
      // Complex expression
      auto expression = parse_expr();
      if (expression) {
         predicate->add_child(std::move(expression));
      }
   }

   match(XPathTokenType::RBRACKET); // consume closing bracket
   return predicate;
}

std::unique_ptr<XPathNode> XPathParser::parse_predicate_value() {
   if (check(XPathTokenType::STRING)) {
      std::string value(peek().value);
      advance();
      return std::make_unique<XPathNode>(XPathNodeType::LITERAL, value);
   }

   if (check(XPathTokenType::IDENTIFIER) or check(XPathTokenType::NUMBER)) {
      std::string value(peek().value);
      advance();

      while (check(XPathTokenType::MULTIPLY) or check(XPathTokenType::WILDCARD)) {
         value += '*';
         advance();
      }

      return std::make_unique<XPathNode>(XPathNodeType::LITERAL, value);
   }

   if (check(XPathTokenType::DOLLAR)) {
      return parse_variable_reference();
   }

   return nullptr;
}

// Expression parsing for XPath 1.0 precedence rules

std::unique_ptr<XPathNode> XPathParser::parse_expr() {
   return parse_or_expr();
}

std::unique_ptr<XPathNode> XPathParser::parse_or_expr() {
   auto left = parse_and_expr();

   while (check(XPathTokenType::OR)) {
      XPathToken op = peek();
      advance();
      auto right = parse_and_expr();
      left = create_binary_op(std::move(left), op, std::move(right));
   }

   return left;
}

std::unique_ptr<XPathNode> XPathParser::parse_and_expr() {
   auto left = parse_equality_expr();

   while (check(XPathTokenType::AND)) {
      XPathToken op = peek();
      advance();
      auto right = parse_equality_expr();
      left = create_binary_op(std::move(left), op, std::move(right));
   }

   return left;
}

std::unique_ptr<XPathNode> XPathParser::parse_equality_expr() {
   auto left = parse_relational_expr();

   while (check(XPathTokenType::EQUALS) or check(XPathTokenType::NOT_EQUALS)) {
      XPathToken op = peek();
      advance();
      auto right = parse_relational_expr();
      left = create_binary_op(std::move(left), op, std::move(right));
   }

   return left;
}

std::unique_ptr<XPathNode> XPathParser::parse_relational_expr() {
   auto left = parse_additive_expr();

   while (check(XPathTokenType::LESS_THAN) or check(XPathTokenType::LESS_EQUAL) or
          check(XPathTokenType::GREATER_THAN) or check(XPathTokenType::GREATER_EQUAL)) {
      XPathToken op = peek();
      advance();
      auto right = parse_additive_expr();
      left = create_binary_op(std::move(left), op, std::move(right));
   }

   return left;
}

std::unique_ptr<XPathNode> XPathParser::parse_additive_expr() {
   auto left = parse_multiplicative_expr();

   while (check(XPathTokenType::PLUS) or check(XPathTokenType::MINUS)) {
      XPathToken op = peek();
      advance();
      auto right = parse_multiplicative_expr();
      left = create_binary_op(std::move(left), op, std::move(right));
   }

   return left;
}

std::unique_ptr<XPathNode> XPathParser::parse_multiplicative_expr() {
   auto left = parse_unary_expr();

   while (check(XPathTokenType::MULTIPLY) or check(XPathTokenType::DIVIDE) or
          check(XPathTokenType::MODULO)) {
      XPathToken op = peek();
      advance();
      auto right = parse_unary_expr();
      left = create_binary_op(std::move(left), op, std::move(right));
   }

   return left;
}

std::unique_ptr<XPathNode> XPathParser::parse_unary_expr() {
   if (match(XPathTokenType::MINUS)) {
      XPathToken op = previous();
      auto operand = parse_unary_expr();
      return create_unary_op(op, std::move(operand));
   }

   if (check(XPathTokenType::NOT)) {
      XPathToken op = peek();
      advance();

      std::unique_ptr<XPathNode> operand;
      if (match(XPathTokenType::LPAREN)) {
         operand = parse_expr();
         match(XPathTokenType::RPAREN);
      } 
      else operand = parse_unary_expr();

      return create_unary_op(op, std::move(operand));
   }

   return parse_union_expr();
}

std::unique_ptr<XPathNode> XPathParser::parse_union_expr() {
   auto left = parse_path_expr();
   if (!left) return nullptr;

   if (!check(XPathTokenType::PIPE)) return left;

   auto union_node = std::make_unique<XPathNode>(XPathNodeType::UNION);
   union_node->add_child(std::move(left));

   while (match(XPathTokenType::PIPE)) {
      auto branch = parse_path_expr();
      if (!branch) {
         report_error("Expected path expression after '|' in union expression");
         break;
      }
      union_node->add_child(std::move(branch));
   }

   return union_node;
}

std::unique_ptr<XPathNode> XPathParser::parse_path_expr() {
   bool looks_like_path = false;

   if (check(XPathTokenType::SLASH) or check(XPathTokenType::DOUBLE_SLASH)) looks_like_path = true;
   else if (is_step_start_token(peek().type)) {
      looks_like_path = true;

      if (peek().type IS XPathTokenType::IDENTIFIER) {
         if (current_token + 1 < tokens.size() and tokens[current_token + 1].type IS XPathTokenType::LPAREN) {
            looks_like_path = false;
         }
      }
   }

   if (looks_like_path) {
      auto location = parse_location_path();
      if (!location) return nullptr;

      auto path_node = std::make_unique<XPathNode>(XPathNodeType::PATH);
      path_node->add_child(std::move(location));
      return path_node;
   }

   return parse_filter_expr();
}

std::unique_ptr<XPathNode> XPathParser::parse_filter_expr() {
   auto primary = parse_primary_expr();
   if (!primary) return nullptr;

   std::unique_ptr<XPathNode> current = std::move(primary);

   bool has_predicate = false;
   while (check(XPathTokenType::LBRACKET)) {
      auto predicate = parse_predicate();
      if (!predicate) return nullptr;

      if (!has_predicate) {
         auto filter = std::make_unique<XPathNode>(XPathNodeType::FILTER);
         filter->add_child(std::move(current));
         current = std::move(filter);
         has_predicate = true;
      }

      current->add_child(std::move(predicate));
   }

   while (true) {
      XPathTokenType slash_type = XPathTokenType::UNKNOWN;

      if (match(XPathTokenType::SLASH)) slash_type = XPathTokenType::SLASH;
      else if (match(XPathTokenType::DOUBLE_SLASH)) slash_type = XPathTokenType::DOUBLE_SLASH;

      if (slash_type IS XPathTokenType::UNKNOWN) break;

      auto relative = parse_location_path();
      if (!relative) return nullptr;

      auto path_node = std::make_unique<XPathNode>(XPathNodeType::PATH);
      path_node->add_child(std::move(current));

      if (slash_type IS XPathTokenType::DOUBLE_SLASH) {
         auto descendant_step = std::make_unique<XPathNode>(XPathNodeType::STEP);
         descendant_step->add_child(std::make_unique<XPathNode>(XPathNodeType::AXIS_SPECIFIER, "descendant-or-self"));
         descendant_step->add_child(std::make_unique<XPathNode>(XPathNodeType::NODE_TYPE_TEST, "node"));
         path_node->add_child(std::move(descendant_step));
      }

      for (auto &child : relative->children) {
         if (child and (child->type IS XPathNodeType::STEP)) {
            path_node->add_child(std::move(child));
         }
      }

      current = std::move(path_node);
   }

   return current;
}

std::unique_ptr<XPathNode> XPathParser::parse_primary_expr() {
   if (match(XPathTokenType::LPAREN)) {
      auto expr = parse_expr();
      match(XPathTokenType::RPAREN);
      return expr;
   }

   if (check(XPathTokenType::STRING)) {
      std::string value(peek().value);
      advance();
      return std::make_unique<XPathNode>(XPathNodeType::LITERAL, value);
   }

   if (check(XPathTokenType::NUMBER)) {
      std::string value(peek().value);
      advance();
      return std::make_unique<XPathNode>(XPathNodeType::NUMBER, value);
   }

   if (check(XPathTokenType::DOLLAR)) {
      return parse_variable_reference();
   }

   if (check(XPathTokenType::IDENTIFIER)) {
      if (current_token + 1 < tokens.size() and tokens[current_token + 1].type IS XPathTokenType::LPAREN) {
         return parse_function_call();
      }

      std::string value(peek().value);
      advance();
      return std::make_unique<XPathNode>(XPathNodeType::LITERAL, value);
   }

   return nullptr;
}

std::unique_ptr<XPathNode> XPathParser::parse_function_call() {
   if (!check(XPathTokenType::IDENTIFIER)) return nullptr;

   std::string function_name(peek().value);
   advance();

   if (!match(XPathTokenType::LPAREN)) return nullptr;

   auto function_node = std::make_unique<XPathNode>(XPathNodeType::FUNCTION_CALL, function_name);

   while (!check(XPathTokenType::RPAREN) and !is_at_end()) {
      auto arg = parse_expr();
      if (arg) function_node->add_child(std::move(arg));

      if (check(XPathTokenType::COMMA)) advance();
      else break;
   }

   match(XPathTokenType::RPAREN);
   return function_node;
}

std::unique_ptr<XPathNode> XPathParser::parse_absolute_location_path() {
   return parse_location_path();
}

std::unique_ptr<XPathNode> XPathParser::parse_relative_location_path() {
   return parse_location_path();
}

std::unique_ptr<XPathNode> XPathParser::parse_axis_specifier() {
   return nullptr; // Handled in parse_step
}

std::unique_ptr<XPathNode> XPathParser::parse_abbreviated_step() {
   return nullptr; // Handled in parse_step
}

std::unique_ptr<XPathNode> XPathParser::parse_argument() {
   return parse_expr();
}

std::unique_ptr<XPathNode> XPathParser::parse_number() 
{
   if (check(XPathTokenType::NUMBER)) {
      std::string value(peek().value);
      advance();
      return std::make_unique<XPathNode>(XPathNodeType::NUMBER, value);
   }
   return nullptr;
}

std::unique_ptr<XPathNode> XPathParser::parse_literal() 
{
   if (check(XPathTokenType::STRING)) {
      std::string value(peek().value);
      advance();
      return std::make_unique<XPathNode>(XPathNodeType::STRING, value);
   }
   return nullptr;
}

std::unique_ptr<XPathNode> XPathParser::parse_variable_reference() 
{
   if (check(XPathTokenType::DOLLAR)) {
      advance();
      if (check(XPathTokenType::IDENTIFIER)) {
         std::string name(peek().value);
         advance();
         return std::make_unique<XPathNode>(XPathNodeType::VARIABLE_REFERENCE, name);
      }
   }
   return nullptr;
}

//********************************************************************************************************************
// CompiledXPath Implementation

CompiledXPath CompiledXPath::compile(std::string_view XPath)
{
   CompiledXPath result;
   result.original_expression = std::string(XPath);

   XPathTokenizer tokenizer;
   auto tokens = tokenizer.tokenize(XPath);

   XPathParser parser;
   auto parsed_ast = parser.parse(tokens);

   if (!parsed_ast) {
      result.errors = parser.get_errors();
      if (result.errors.empty()) {
         result.errors.push_back("Failed to parse XPath expression");
      }
      return result;
   }

   result.ast = std::move(parsed_ast);
   result.is_valid = true;

   return result;
}
