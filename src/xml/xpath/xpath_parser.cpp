#include "xpath_parser.h"

#include <parasol/main.h>

#include <algorithm>
#include <string>
#include <utility>

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
      return nullptr;
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

bool XPathParser::check_identifier_keyword(std::string_view Keyword) const {
   const XPathToken &token = peek();

   if (Keyword IS "union") {
      if (token.type IS XPathTokenType::UNION) return true;
   }
   else if (Keyword IS "intersect") {
      if (token.type IS XPathTokenType::INTERSECT) return true;
   }
   else if (Keyword IS "except") {
      if (token.type IS XPathTokenType::EXCEPT) return true;
   }

   return (token.type IS XPathTokenType::IDENTIFIER) and (token.value IS Keyword);
}

bool XPathParser::match_identifier_keyword(std::string_view Keyword,
                                           XPathTokenType KeywordType,
                                           XPathToken &OutToken) {
   if (match(KeywordType)) {
      OutToken = previous();
      return true;
   }

   if (check(XPathTokenType::IDENTIFIER) and (peek().value IS Keyword)) {
      OutToken = peek();
      OutToken.type = KeywordType;
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
          check(XPathTokenType::PIPE) or
          check(XPathTokenType::UNION) or
          check(XPathTokenType::INTERSECT) or
          check(XPathTokenType::EXCEPT)) {
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

            if (has_errors()) return nullptr;
            else return std::make_unique<XPathNode>(XPathNodeType::PROCESSING_INSTRUCTION_TEST, target);
         }

         if (!match(XPathTokenType::RPAREN)) {
            report_error("Expected ')' after node type test");
            return nullptr;
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
            }
            else report_error("Expected identifier or wildcard after ':' in attribute name");
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

   if (has_errors()) return nullptr;

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
   // Handle conditional expressions at the top level
   if (check(XPathTokenType::IF)) {
      return parse_if_expr();
   }

   // Handle FLWOR expressions (for/let) at the top level
   if (check(XPathTokenType::FOR) or check(XPathTokenType::LET) or check_identifier_keyword("let")) {
      return parse_flwor_expr();
   }

   // Handle quantified expressions at the top level
   if (check(XPathTokenType::SOME) or check(XPathTokenType::EVERY)) {
      return parse_quantified_expr();
   }

   return parse_or_expr();
}

std::unique_ptr<XPathNode> XPathParser::parse_flwor_expr() {
   std::vector<std::unique_ptr<XPathNode>> clauses;
   bool saw_for = false;
   bool saw_let = false;

   while (true) {
      if (match(XPathTokenType::FOR)) {
         saw_for = true;

         bool expect_binding = true;
         while (expect_binding) {
            if (!match(XPathTokenType::DOLLAR)) {
               report_error("Expected '$' after 'for'");
               return nullptr;
            }

            std::string variable_name;
            if (check(XPathTokenType::IDENTIFIER)) {
               variable_name = peek().value;
               advance();
            }
            else {
               report_error("Expected variable name after '$' in for expression");
               return nullptr;
            }

            if (!match(XPathTokenType::IN)) {
               report_error("Expected 'in' in for expression");
               return nullptr;
            }

            auto sequence_expr = parse_expr();
            if (!sequence_expr) return nullptr;

            auto binding_node = std::make_unique<XPathNode>(XPathNodeType::FOR_BINDING, variable_name);
            binding_node->add_child(std::move(sequence_expr));
            clauses.push_back(std::move(binding_node));

            if (match(XPathTokenType::COMMA)) expect_binding = true;
            else expect_binding = false;
         }

         continue;
      }

      if (check(XPathTokenType::LET) or check_identifier_keyword("let")) {
         XPathToken let_token(XPathTokenType::UNKNOWN, std::string_view());
         if (!match_identifier_keyword("let", XPathTokenType::LET, let_token)) {
            report_error("Expected 'let' expression");
            return nullptr;
         }

         saw_let = true;

         bool parsing_bindings = true;
         while (parsing_bindings) {
            if (!match(XPathTokenType::DOLLAR)) {
               report_error("Expected '$' after 'let'");
               return nullptr;
            }

            std::string variable_name;
            if (check(XPathTokenType::IDENTIFIER)) {
               variable_name = peek().value;
               advance();
            }
            else {
               report_error("Expected variable name after '$' in let binding");
               return nullptr;
            }

            if (!match(XPathTokenType::ASSIGN)) {
               report_error("Expected ':=' in let binding");
               return nullptr;
            }

            auto binding_expr = parse_expr();
            if (!binding_expr) {
               report_error("Expected expression after ':=' in let binding");
               return nullptr;
            }

            auto binding_node = std::make_unique<XPathNode>(XPathNodeType::LET_BINDING, variable_name);
            binding_node->add_child(std::move(binding_expr));
            clauses.push_back(std::move(binding_node));

            if (match(XPathTokenType::COMMA)) parsing_bindings = true;
            else parsing_bindings = false;
         }

         continue;
      }

      break;
   }

   if (clauses.empty()) {
      report_error("Expected 'for' or 'let' expression");
      return nullptr;
   }

   XPathToken return_token(XPathTokenType::UNKNOWN, std::string_view());
   if (!match_identifier_keyword("return", XPathTokenType::RETURN, return_token)) {
      report_error("Expected 'return' in FLWOR expression");
      return nullptr;
   }

   auto return_expr = parse_expr();
   if (!return_expr) {
      report_error("Expected expression after 'return'");
      return nullptr;
   }

   if (saw_for and !saw_let) {
      auto for_node = std::make_unique<XPathNode>(XPathNodeType::FOR_EXPRESSION);
      for (auto &clause : clauses) {
         if ((!clause) or !(clause->type IS XPathNodeType::FOR_BINDING)) {
            report_error("Invalid for binding in FLWOR expression");
            return nullptr;
         }
         for_node->add_child(std::move(clause));
      }
      for_node->add_child(std::move(return_expr));
      return for_node;
   }

   if (saw_let and !saw_for) {
      auto let_node = std::make_unique<XPathNode>(XPathNodeType::LET_EXPRESSION);
      for (auto &clause : clauses) {
         if ((!clause) or !(clause->type IS XPathNodeType::LET_BINDING)) {
            report_error("Invalid let binding in FLWOR expression");
            return nullptr;
         }
         let_node->add_child(std::move(clause));
      }
      let_node->add_child(std::move(return_expr));
      return let_node;
   }

   auto flwor_node = std::make_unique<XPathNode>(XPathNodeType::FLWOR_EXPRESSION);
   for (auto &clause : clauses) {
      if (!clause) {
         report_error("Invalid clause in FLWOR expression");
         return nullptr;
      }
      flwor_node->add_child(std::move(clause));
   }
   flwor_node->add_child(std::move(return_expr));
   return flwor_node;
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

   while (check(XPathTokenType::EQUALS) or check(XPathTokenType::NOT_EQUALS) or
          check(XPathTokenType::EQ) or check(XPathTokenType::NE)) {
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
          check(XPathTokenType::GREATER_THAN) or check(XPathTokenType::GREATER_EQUAL) or
          check(XPathTokenType::LT) or check(XPathTokenType::LE) or
          check(XPathTokenType::GT) or check(XPathTokenType::GE)) {
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

std::unique_ptr<XPathNode> XPathParser::parse_intersect_expr() {
   auto left = parse_path_expr();
   if (!left) return nullptr;

   while (true) {
      XPathToken op(XPathTokenType::UNKNOWN, std::string_view(), 0, 0);
      if (!match_identifier_keyword("intersect", XPathTokenType::INTERSECT, op)) {
         if (!match_identifier_keyword("except", XPathTokenType::EXCEPT, op)) break;
      }

      auto right = parse_path_expr();
      if (!right) {
         report_error("Expected expression after set operator");
         return nullptr;
      }

      left = create_binary_op(std::move(left), op, std::move(right));
   }

   return left;
}

std::unique_ptr<XPathNode> XPathParser::parse_union_expr() {
   auto left = parse_intersect_expr();
   if (!left) return nullptr;

   if (!check(XPathTokenType::PIPE) and !check_identifier_keyword("union")) return left;

   auto union_node = std::make_unique<XPathNode>(XPathNodeType::UNION);
   union_node->add_child(std::move(left));

   while (true) {
      if (!match(XPathTokenType::PIPE)) {
         XPathToken union_token(XPathTokenType::UNKNOWN, std::string_view(), 0, 0);
         if (!match_identifier_keyword("union", XPathTokenType::UNION, union_token)) break;
      }

      auto branch = parse_intersect_expr();
      if (!branch) {
         report_error("Expected expression after union operator");
         return nullptr;
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

std::unique_ptr<XPathNode> XPathParser::parse_if_expr() {
   if (!match(XPathTokenType::IF)) return nullptr;

   if (!match(XPathTokenType::LPAREN)) {
      report_error("Expected '(' after 'if'");
      return nullptr;
   }

   auto condition = parse_expr();

   if (!match(XPathTokenType::RPAREN)) {
      report_error("Expected ')' after condition in if expression");
      return nullptr;
   }

   if (!match(XPathTokenType::THEN)) {
      report_error("Expected 'then' in if expression");
      return nullptr;
   }

   auto then_branch = parse_expr();

   if (!match(XPathTokenType::ELSE)) {
      report_error("Expected 'else' in if expression");
      return nullptr;
   }

   auto else_branch = parse_expr();

   auto conditional = std::make_unique<XPathNode>(XPathNodeType::CONDITIONAL);
   conditional->add_child(std::move(condition));
   conditional->add_child(std::move(then_branch));
   conditional->add_child(std::move(else_branch));
   return conditional;
}

std::unique_ptr<XPathNode> XPathParser::parse_quantified_expr() {
   bool is_some = match(XPathTokenType::SOME);
   bool is_every = false;

   if (!is_some) {
      if (!match(XPathTokenType::EVERY)) return nullptr;
      is_every = true;
   }

   auto quant_node = std::make_unique<XPathNode>(XPathNodeType::QUANTIFIED_EXPRESSION, is_some ? "some" : "every");

   bool expect_binding = true;
   while (expect_binding) {
      if (!match(XPathTokenType::DOLLAR)) {
         report_error("Expected '$' after quantified expression keyword");
         return nullptr;
      }

      std::string variable_name;
      if (check(XPathTokenType::IDENTIFIER)) {
         variable_name = peek().value;
         advance();
      }
      else {
         report_error("Expected variable name in quantified expression");
         return nullptr;
      }

      if (!match(XPathTokenType::IN)) {
         report_error("Expected 'in' in quantified expression");
         return nullptr;
      }

      auto sequence_expr = parse_expr();
      if (!sequence_expr) return nullptr;

      auto binding_node = std::make_unique<XPathNode>(XPathNodeType::QUANTIFIED_BINDING, variable_name);
      binding_node->add_child(std::move(sequence_expr));
      quant_node->add_child(std::move(binding_node));

      if (match(XPathTokenType::COMMA)) expect_binding = true;
      else expect_binding = false;
   }

   if (!match(XPathTokenType::SATISFIES)) {
      report_error("Expected 'satisfies' in quantified expression");
      return nullptr;
   }

   auto condition_expr = parse_expr();
   if (!condition_expr) return nullptr;

   quant_node->add_child(std::move(condition_expr));
   return quant_node;
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
