// XPath Tokenizer and Parser Implementation

//********************************************************************************************************************
// XPathTokenizer Implementation

bool XPathTokenizer::is_alpha(char c) const {
   return std::isalpha(static_cast<unsigned char>(c));
}

bool XPathTokenizer::is_digit(char c) const {
   return std::isdigit(static_cast<unsigned char>(c));
}

bool XPathTokenizer::is_alnum(char c) const {
   return std::isalnum(static_cast<unsigned char>(c));
}

bool XPathTokenizer::is_whitespace(char c) const {
   return std::isspace(static_cast<unsigned char>(c));
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

bool XPathTokenizer::match(const char* str) {
   size_t len = std::strlen(str);
   if (position + len > length) return false;
   return input.substr(position, len) == str;
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

std::vector<XPathToken> XPathTokenizer::tokenize(std::string_view xpath) {
   input = xpath;
   position = 0;
   length = input.size();
   std::vector<XPathToken> tokens;
   int bracket_depth = 0; // '[' and ']'
   int paren_depth = 0;   // '(' and ')'

   while (position < length) {
      skip_whitespace();
      if (position >= length) break;

      // Context-aware handling for '*': wildcard vs multiply
      if (input[position] IS '*') {
         size_t start = position;
         position++;
         bool in_predicate = bracket_depth > 0;
         XPathTokenType type = XPathTokenType::WILDCARD;
         if (in_predicate) {
            // If previous token can terminate an operand, treat '*' as MULTIPLY; otherwise as WILDCARD (e.g., @*)
            if (!tokens.empty()) {
               auto prev = tokens.back().type;
               bool prev_is_operand = (prev IS XPathTokenType::NUMBER) ||
                                      (prev IS XPathTokenType::STRING) ||
                                      (prev IS XPathTokenType::IDENTIFIER) ||
                                      (prev IS XPathTokenType::RPAREN) ||
                                      (prev IS XPathTokenType::RBRACKET);
               bool prev_forces_wild = (prev IS XPathTokenType::AT) ||
                                       (prev IS XPathTokenType::AXIS_SEPARATOR) ||
                                       (prev IS XPathTokenType::SLASH) ||
                                       (prev IS XPathTokenType::DOUBLE_SLASH) ||
                                       (prev IS XPathTokenType::COLON);
               if (prev_is_operand and !prev_forces_wild) type = XPathTokenType::MULTIPLY;
            }
         }
         tokens.emplace_back(type, "*", start, 1);
      }
      else {
         XPathToken token = scan_operator();
         if (token.type == XPathTokenType::UNKNOWN) {
            if (current() IS '\'' or current() IS '"') {
               token = scan_string(current());
            } else if (is_digit(current()) or (current() IS '.' and is_digit(peek(1)))) {
               token = scan_number();
            } else if (is_name_start_char(current())) {
               token = scan_identifier();
            } else {
               // Unknown character
               size_t start = position;
               advance();
               token = XPathToken(XPathTokenType::UNKNOWN, std::string(1, input[start]), start, 1);
            }
         }

         // Track bracket/paren depth for context
         if (token.type IS XPathTokenType::LBRACKET) bracket_depth++;
         else if (token.type IS XPathTokenType::RBRACKET && bracket_depth > 0) bracket_depth--;
         else if (token.type IS XPathTokenType::LPAREN) paren_depth++;
         else if (token.type IS XPathTokenType::RPAREN && paren_depth > 0) paren_depth--;

         tokens.push_back(std::move(token));
      }
   }

   tokens.emplace_back(XPathTokenType::END_OF_INPUT, "", position, 0);
   return tokens;
}

XPathToken XPathTokenizer::scan_identifier() {
   size_t start = position;
   std::string value;

   while (position < length and is_name_char(input[position])) {
      value += input[position];
      position++;
   }

   // Check for keywords
   XPathTokenType type = XPathTokenType::IDENTIFIER;
   if (value IS "and") type = XPathTokenType::AND;
   else if (value IS "or") type = XPathTokenType::OR;
   else if (value IS "not") type = XPathTokenType::NOT;
   else if (value IS "div") type = XPathTokenType::DIVIDE;
   else if (value IS "mod") type = XPathTokenType::MODULO;

   return XPathToken(type, value, start, position - start);
}

XPathToken XPathTokenizer::scan_number() {
   size_t start = position;
   std::string value;

   bool seen_dot = false;
   while (position < length) {
      char current = input[position];
      if (is_digit(current)) {
         value += current;
         position++;
         continue;
      }
      if (!seen_dot and current IS '.') {
         seen_dot = true;
         value += current;
         position++;
         continue;
      }
      break;
   }

   return XPathToken(XPathTokenType::NUMBER, value, start, position - start);
}

XPathToken XPathTokenizer::scan_string(char quote_char) {
   size_t start = position;
   position++; // Skip opening quote

   std::string value;
   while (position < length and input[position] != quote_char) {
      if (input[position] == '\\' and position + 1 < length) {
         // Handle escape sequences
         position++;
         char escaped = input[position];
         if (escaped == quote_char or escaped == '\\' or escaped == '*') {
            value += escaped;
         } else {
            value += '\\';
            value += escaped;
         }
      } else {
         value += input[position];
      }
      position++;
   }

   if (position < length) {
      position++; // Skip closing quote
   }

   return XPathToken(XPathTokenType::STRING, value, start, position - start);
}

XPathToken XPathTokenizer::scan_operator() {
   size_t start = position;
   char ch = input[position];

   // Two-character operators
   if (position + 1 < length) {
      std::string two_char = std::string(input.substr(position, 2));
      if (two_char IS "//") {
         position += 2;
         return XPathToken(XPathTokenType::DOUBLE_SLASH, "//", start, 2);
      }
      else if (two_char IS "..") {
         position += 2;
         return XPathToken(XPathTokenType::DOUBLE_DOT, "..", start, 2);
      }
      else if (two_char IS "::") {
         position += 2;
         return XPathToken(XPathTokenType::AXIS_SEPARATOR, "::", start, 2);
      }
      else if (two_char IS "!=") {
         position += 2;
         return XPathToken(XPathTokenType::NOT_EQUALS, "!=", start, 2);
      }
      else if (two_char IS "<=") {
         position += 2;
         return XPathToken(XPathTokenType::LESS_EQUAL, "<=", start, 2);
      }
      else if (two_char IS ">=") {
         position += 2;
         return XPathToken(XPathTokenType::GREATER_EQUAL, ">=", start, 2);
      }
   }

   // Single character operators
   switch (ch) {
      case '/': position++; return XPathToken(XPathTokenType::SLASH, "/", start, 1);
      case '.': position++; return XPathToken(XPathTokenType::DOT, ".", start, 1);
      case '*': position++; return XPathToken(XPathTokenType::WILDCARD, "*", start, 1);
      case '[': position++; return XPathToken(XPathTokenType::LBRACKET, "[", start, 1);
      case ']': position++; return XPathToken(XPathTokenType::RBRACKET, "]", start, 1);
      case '(': position++; return XPathToken(XPathTokenType::LPAREN, "(", start, 1);
      case ')': position++; return XPathToken(XPathTokenType::RPAREN, ")", start, 1);
      case '@': position++; return XPathToken(XPathTokenType::AT, "@", start, 1);
      case ',': position++; return XPathToken(XPathTokenType::COMMA, ",", start, 1);
      case '|': position++; return XPathToken(XPathTokenType::PIPE, "|", start, 1);
      case '=': position++; return XPathToken(XPathTokenType::EQUALS, "=", start, 1);
      case '<': position++; return XPathToken(XPathTokenType::LESS_THAN, "<", start, 1);
      case '>': position++; return XPathToken(XPathTokenType::GREATER_THAN, ">", start, 1);
      case '+': position++; return XPathToken(XPathTokenType::PLUS, "+", start, 1);
      case '-': position++; return XPathToken(XPathTokenType::MINUS, "-", start, 1);
      case ':': position++; return XPathToken(XPathTokenType::COLON, ":", start, 1);
      case '$': position++; return XPathToken(XPathTokenType::DOLLAR, "$", start, 1);
   }

   // Unknown token
   return XPathToken(XPathTokenType::UNKNOWN, "", start, 0);
}

//********************************************************************************************************************
// XPathParser Implementation

std::unique_ptr<XPathNode> XPathParser::parse(const std::vector<XPathToken>& token_list) {
   tokens = token_list;
   current_token = 0;
   errors.clear();
   return parse_location_path();
}

bool XPathParser::check(XPathTokenType type) const {
   return peek().type IS type;
}

bool XPathParser::match(XPathTokenType type) {
   if (check(type)) {
      advance();
      return true;
   }
   return false;
}

XPathToken XPathParser::consume(XPathTokenType type, const std::string& error_message) {
   if (check(type)) return previous();

   report_error(error_message);
   return peek();
}

XPathToken XPathParser::peek() const {
   return current_token < tokens.size() ? tokens[current_token] : tokens.back(); // END_OF_INPUT
}

XPathToken XPathParser::previous() const {
   return tokens[current_token - 1];
}

bool XPathParser::is_at_end() const {
   return peek().type IS XPathTokenType::END_OF_INPUT;
}

void XPathParser::advance() {
   if (!is_at_end()) current_token++;
}

void XPathParser::report_error(const std::string& message) {
   errors.push_back(message);
}

bool XPathParser::has_errors() const {
   return !errors.empty();
}

std::vector<std::string> XPathParser::get_errors() const {
   return errors;
}

std::unique_ptr<XPathNode> XPathParser::create_binary_op(std::unique_ptr<XPathNode> left, const XPathToken& op, std::unique_ptr<XPathNode> right) {
   auto binary_op = std::make_unique<XPathNode>(XPathNodeType::BinaryOp, op.value);
   binary_op->add_child(std::move(left));
   binary_op->add_child(std::move(right));
   return binary_op;
}

std::unique_ptr<XPathNode> XPathParser::create_unary_op(const XPathToken& op, std::unique_ptr<XPathNode> operand) {
   auto unary_op = std::make_unique<XPathNode>(XPathNodeType::UnaryOp, op.value);
   unary_op->add_child(std::move(operand));
   return unary_op;
}

// Grammar implementation follows - this is a simplified parser focusing on the core features
// needed for the AST_PLAN.md phases

std::unique_ptr<XPathNode> XPathParser::parse_location_path() {
   auto path = std::make_unique<XPathNode>(XPathNodeType::LocationPath);

   bool is_absolute = false;
   if (match(XPathTokenType::SLASH)) {
      is_absolute = true;
      path->add_child(std::make_unique<XPathNode>(XPathNodeType::Root, "/"));
   }
   else if (match(XPathTokenType::DOUBLE_SLASH)) {
      is_absolute = true;
      path->add_child(std::make_unique<XPathNode>(XPathNodeType::Root, "//"));
   }

   // Parse steps
   while (!is_at_end() and !check(XPathTokenType::RBRACKET) and !check(XPathTokenType::RPAREN)) {
      auto step = parse_step();
      if (step) {
         path->add_child(std::move(step));
      }

      // Check for path separator
      if (match(XPathTokenType::SLASH)) {
         // Continue with next step
      }
      else if (match(XPathTokenType::DOUBLE_SLASH)) {
         // Add descendant-or-self step
         auto descendant_step = std::make_unique<XPathNode>(XPathNodeType::Step);
         descendant_step->add_child(std::make_unique<XPathNode>(XPathNodeType::AxisSpecifier, "descendant-or-self"));
         descendant_step->add_child(std::make_unique<XPathNode>(XPathNodeType::NodeTypeTest, "node"));
         path->add_child(std::move(descendant_step));
      }
      else {
         break;
      }
   }

   return path;
}

std::unique_ptr<XPathNode> XPathParser::parse_step() {
   auto step = std::make_unique<XPathNode>(XPathNodeType::Step);

   // Handle abbreviated steps
   if (check(XPathTokenType::DOT)) {
      advance();
      step->add_child(std::make_unique<XPathNode>(XPathNodeType::AxisSpecifier, "self"));
      step->add_child(std::make_unique<XPathNode>(XPathNodeType::NodeTypeTest, "node"));
      return step;
   }
   if (check(XPathTokenType::DOUBLE_DOT)) {
      advance();
      step->add_child(std::make_unique<XPathNode>(XPathNodeType::AxisSpecifier, "parent"));
      step->add_child(std::make_unique<XPathNode>(XPathNodeType::NodeTypeTest, "node"));
      return step;
   }

   // Handle explicit axis specifiers (axis::node-test)
   if (check(XPathTokenType::IDENTIFIER)) {
      // Look ahead for axis separator
      if (current_token + 1 < tokens.size() and tokens[current_token + 1].type IS XPathTokenType::AXIS_SEPARATOR) {
         std::string axis_name = peek().value;
         advance(); // consume axis name
         advance(); // consume "::"
         auto axis = std::make_unique<XPathNode>(XPathNodeType::AxisSpecifier, axis_name);
         step->add_child(std::move(axis));
      }
   }
   // Handle attribute axis (@)
   else if (match(XPathTokenType::AT)) {
      auto axis = std::make_unique<XPathNode>(XPathNodeType::AxisSpecifier, "attribute");
      step->add_child(std::move(axis));
   }

   // Parse node test
   auto node_test = parse_node_test();
   if (node_test) {
      step->add_child(std::move(node_test));
   }

   // Parse predicates
   while (check(XPathTokenType::LBRACKET)) {
      auto predicate = parse_predicate();
      if (predicate) {
         step->add_child(std::move(predicate));
      }
   }

   return step;
}

std::unique_ptr<XPathNode> XPathParser::parse_node_test() {
   if (check(XPathTokenType::WILDCARD)) {
      advance();
      return std::make_unique<XPathNode>(XPathNodeType::Wildcard, "*");
   }
   else if (check(XPathTokenType::IDENTIFIER)) {
      std::string name = peek().value;
      advance();

      if (check(XPathTokenType::COLON)) {
         if (current_token + 1 < tokens.size() and tokens[current_token + 1].type IS XPathTokenType::IDENTIFIER) {
            advance(); // consume ':'
            name += ':';
            name += peek().value;
            advance();
         }
      }

      return std::make_unique<XPathNode>(XPathNodeType::NameTest, name);
   }

   return nullptr;
}

std::unique_ptr<XPathNode> XPathParser::parse_predicate() {
   if (!match(XPathTokenType::LBRACKET)) return nullptr;

   auto predicate = std::make_unique<XPathNode>(XPathNodeType::Predicate);

   if (check(XPathTokenType::NUMBER)) {
      // Index predicate [1], [2], etc.
      std::string index = peek().value;
      advance();
      predicate->add_child(std::make_unique<XPathNode>(XPathNodeType::Number, index));
   }
   else if (check(XPathTokenType::EQUALS)) {
      // Content predicate [=value]
      advance(); // consume =
      if (check(XPathTokenType::STRING) or check(XPathTokenType::IDENTIFIER) or check(XPathTokenType::NUMBER)) {
         std::string content = peek().value;
         advance();
         auto content_test = std::make_unique<XPathNode>(XPathNodeType::BinaryOp, "content-equals");
         content_test->add_child(std::make_unique<XPathNode>(XPathNodeType::Literal, content));
         predicate->add_child(std::move(content_test));
      }
   }
   else if (check(XPathTokenType::AT)) {
      // Attribute predicate [@attr] or [@attr="value"]
      advance(); // consume @
      if (check(XPathTokenType::IDENTIFIER) || check(XPathTokenType::WILDCARD)) {
         std::string attr_name = peek().value;
         advance();

         // If '=' follows, parse value comparison, else treat as existence test
         if (match(XPathTokenType::EQUALS) and
             (check(XPathTokenType::STRING) or check(XPathTokenType::IDENTIFIER) or check(XPathTokenType::NUMBER))) {
            std::string attr_value = peek().value;
            advance();
            auto attr_test = std::make_unique<XPathNode>(XPathNodeType::BinaryOp, "attribute-equals");
            attr_test->add_child(std::make_unique<XPathNode>(XPathNodeType::Literal, attr_name));
            attr_test->add_child(std::make_unique<XPathNode>(XPathNodeType::Literal, attr_value));
            predicate->add_child(std::move(attr_test));
         } else {
            auto attr_exists = std::make_unique<XPathNode>(XPathNodeType::BinaryOp, "attribute-exists");
            attr_exists->add_child(std::make_unique<XPathNode>(XPathNodeType::Literal, attr_name));
            predicate->add_child(std::move(attr_exists));
         }
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

// Simplified expression parsing for core functionality
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
   auto left = parse_primary_expr();

   while (check(XPathTokenType::EQUALS) or check(XPathTokenType::NOT_EQUALS)) {
      XPathToken op = peek();
      advance();
      auto right = parse_primary_expr();
      left = create_binary_op(std::move(left), op, std::move(right));
   }

   return left;
}

std::unique_ptr<XPathNode> XPathParser::parse_primary_expr() {
   if (check(XPathTokenType::STRING)) {
      std::string value = peek().value;
      advance();
      return std::make_unique<XPathNode>(XPathNodeType::Literal, value);
   }
   else if (check(XPathTokenType::NUMBER)) {
      std::string value = peek().value;
      advance();
      return std::make_unique<XPathNode>(XPathNodeType::Number, value);
   }
   else if (check(XPathTokenType::IDENTIFIER)) {
      // Check if this is a function call
      if (current_token + 1 < tokens.size() and tokens[current_token + 1].type IS XPathTokenType::LPAREN) {
         return parse_function_call();
      }

      std::string value = peek().value;
      advance();
      return std::make_unique<XPathNode>(XPathNodeType::Literal, value);
   }

   return nullptr;
}

std::unique_ptr<XPathNode> XPathParser::parse_function_call() {
   if (!check(XPathTokenType::IDENTIFIER)) return nullptr;

   std::string function_name = peek().value;
   advance();

   if (!match(XPathTokenType::LPAREN)) return nullptr;

   auto function_node = std::make_unique<XPathNode>(XPathNodeType::FunctionCall, function_name);

   // Parse arguments
   while (!check(XPathTokenType::RPAREN) and !is_at_end()) {
      auto arg = parse_expr();
      if (arg) {
         function_node->add_child(std::move(arg));
      }

      if (check(XPathTokenType::COMMA)) {
         advance(); // consume comma
      } else {
         break;
      }
   }

   match(XPathTokenType::RPAREN); // consume closing parenthesis
   return function_node;
}

// Stubs for remaining grammar rules - implement as needed for AST_PLAN.md phases

std::unique_ptr<XPathNode> XPathParser::parse_relational_expr() {
   return parse_primary_expr();
}

std::unique_ptr<XPathNode> XPathParser::parse_additive_expr() {
   return parse_primary_expr();
}

std::unique_ptr<XPathNode> XPathParser::parse_multiplicative_expr() {
   return parse_primary_expr();
}

std::unique_ptr<XPathNode> XPathParser::parse_unary_expr() {
   return parse_primary_expr();
}

std::unique_ptr<XPathNode> XPathParser::parse_union_expr() {
   return parse_primary_expr();
}

std::unique_ptr<XPathNode> XPathParser::parse_filter_expr() {
   return parse_primary_expr();
}

std::unique_ptr<XPathNode> XPathParser::parse_path_expr() {
   return parse_location_path();
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

std::unique_ptr<XPathNode> XPathParser::parse_number() {
   if (check(XPathTokenType::NUMBER)) {
      std::string value = peek().value;
      advance();
      return std::make_unique<XPathNode>(XPathNodeType::Number, value);
   }
   return nullptr;
}

std::unique_ptr<XPathNode> XPathParser::parse_literal() {
   if (check(XPathTokenType::STRING)) {
      std::string value = peek().value;
      advance();
      return std::make_unique<XPathNode>(XPathNodeType::String, value);
   }
   return nullptr;
}

std::unique_ptr<XPathNode> XPathParser::parse_variable_reference() {
   if (check(XPathTokenType::DOLLAR)) {
      advance();
      if (check(XPathTokenType::IDENTIFIER)) {
         std::string name = peek().value;
         advance();
         return std::make_unique<XPathNode>(XPathNodeType::VariableReference, name);
      }
   }
   return nullptr;
}