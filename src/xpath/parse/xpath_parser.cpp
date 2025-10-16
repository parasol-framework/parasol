#include "xpath_parser.h"

//********************************************************************************************************************
// Parses a list of XPath tokens into an abstract syntax tree (AST), returning the root node of the parse tree or
// nullptr if parsing fails.

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
// Checks if the current token is an identifier matching the specified keyword, or a dedicated token type for certain
// keywords like 'union', 'intersect', and 'except'.

bool XPathParser::check_identifier_keyword(std::string_view Keyword) const
{
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
   else if (Keyword IS "where") {
      if (token.type IS XPathTokenType::WHERE) return true;
   }
   else if (Keyword IS "group") {
      if (token.type IS XPathTokenType::GROUP) return true;
   }
   else if (Keyword IS "stable") {
      if (token.type IS XPathTokenType::STABLE) return true;
   }
   else if (Keyword IS "order") {
      if (token.type IS XPathTokenType::ORDER) return true;
   }
   else if (Keyword IS "count") {
      if (token.type IS XPathTokenType::COUNT) return true;
   }

   return (token.type IS XPathTokenType::IDENTIFIER) and (token.value IS Keyword);
}

//********************************************************************************************************************
// Attempts to match and consume a keyword token, accepting either the dedicated token type or an identifier with
// matching text, returning the matched token via OutToken.

bool XPathParser::match_identifier_keyword(std::string_view Keyword, XPathTokenType KeywordType, XPathToken &OutToken)
{
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




//********************************************************************************************************************
// Constructs a binary operation AST node from left operand, operator token, and right operand.

std::unique_ptr<XPathNode> XPathParser::create_binary_op(std::unique_ptr<XPathNode> Left, const XPathToken &Op, std::unique_ptr<XPathNode> Right)
{
   auto binary_op = std::make_unique<XPathNode>(XPathNodeType::BINARY_OP, std::string(Op.value));
   binary_op->add_child(std::move(Left));
   binary_op->add_child(std::move(Right));
   return binary_op;
}

//********************************************************************************************************************
// Constructs a unary operation AST node from operator token and operand.

std::unique_ptr<XPathNode> XPathParser::create_unary_op(const XPathToken &Op, std::unique_ptr<XPathNode> Operand)
{
   auto unary_op = std::make_unique<XPathNode>(XPathNodeType::UNARY_OP, std::string(Op.value));
   unary_op->add_child(std::move(Operand));
   return unary_op;
}

//********************************************************************************************************************
// Parses location path expressions handling both absolute (starting with / or //) and relative paths, collecting
// individual steps separated by path separators.

std::unique_ptr<XPathNode> XPathParser::parse_location_path()
{
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
// Parses a single location path step, including abbreviated steps (. and ..), axis specifiers, node tests,
// and predicates attached to the step.

std::unique_ptr<XPathNode> XPathParser::parse_step()
{
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
   if (is_identifier_token(peek())) {
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
// Parses node tests including wildcards, name tests (element names), qualified names with namespaces,
// and node type tests like node(), text(), comment(), and processing-instruction().

std::unique_ptr<XPathNode> XPathParser::parse_node_test()
{
   if (check(XPathTokenType::WILDCARD)) {
      advance();
      return std::make_unique<XPathNode>(XPathNodeType::WILDCARD, "*");
   }
   else if (is_identifier_token(peek())) {
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
               if (check(XPathTokenType::STRING) or is_identifier_token(peek())) {
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
         if (current_token + 1 < tokens.size() and is_identifier_token(tokens[current_token + 1])) {
            advance(); // consume ':'
            name = std::format("{}:{}", name, peek().value);
            advance();
         }
      }

      return std::make_unique<XPathNode>(XPathNodeType::NAME_TEST, name);
   }

   return nullptr;
}

//********************************************************************************************************************
// Parses predicate expressions enclosed in square brackets, handling index predicates, content equality tests,
// attribute tests, and general expressions for filtering node sets.

std::unique_ptr<XPathNode> XPathParser::parse_predicate()
{
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

      if (is_identifier_token(peek()) or check(XPathTokenType::WILDCARD)) {
         std::string attr_name(peek().value);
         advance();

         if (match(XPathTokenType::COLON)) {
            if (is_identifier_token(peek()) or check(XPathTokenType::WILDCARD)) {
               attr_name = std::format("{}:{}", attr_name, peek().value);
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

   (void)match(XPathTokenType::RBRACKET); // consume closing bracket
   return predicate;
}

//********************************************************************************************************************
// Parses values within predicates, handling strings, identifiers, numbers, wildcards, and variable references.

std::unique_ptr<XPathNode> XPathParser::parse_predicate_value()
{
   if (check(XPathTokenType::STRING)) {
      std::string value(peek().value);
      advance();
      return std::make_unique<XPathNode>(XPathNodeType::LITERAL, value);
   }

   if (is_identifier_token(peek()) or check(XPathTokenType::NUMBER)) {
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

// Expression parsing for XPath precedence rules

//********************************************************************************************************************
// Parses a single XPath expression, dispatching to control flow (if, for, let, some, every) or operator parsing.

std::unique_ptr<XPathNode> XPathParser::parse_expr_single()
{
   if (check(XPathTokenType::IF)) {
      return parse_if_expr();
   }

   if (check(XPathTokenType::FOR) or check(XPathTokenType::LET) or check_identifier_keyword("let")) {
      return parse_flwor_expr();
   }

   if (check(XPathTokenType::SOME) or check(XPathTokenType::EVERY)) {
      return parse_quantified_expr();
   }

   return parse_or_expr();
}

//********************************************************************************************************************
// Parses comma-separated expressions building a sequence from multiple expressions.

std::unique_ptr<XPathNode> XPathParser::parse_expr()
{
   auto expression = parse_expr_single();
   if (!expression) return nullptr;

   while (match(XPathTokenType::COMMA)) {
      XPathToken comma = previous();
      auto right = parse_expr_single();
      if (!right) return nullptr;
      expression = create_binary_op(std::move(expression), comma, std::move(right));
   }

   return expression;
}

//********************************************************************************************************************
// Parses FLWOR (For, Let, Where, Order by, Return) expressions with ordered optional clauses.

std::unique_ptr<XPathNode> XPathParser::parse_flwor_expr()
{
   std::vector<std::unique_ptr<XPathNode>> binding_nodes;
   std::vector<std::unique_ptr<XPathNode>> clause_nodes;
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
            if (is_identifier_token(peek())) {
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

            auto sequence_expr = parse_expr_single();
            if (!sequence_expr) return nullptr;

            auto binding_node = std::make_unique<XPathNode>(XPathNodeType::FOR_BINDING, variable_name);
            binding_node->add_child(std::move(sequence_expr));
            binding_nodes.push_back(std::move(binding_node));

            expect_binding = match(XPathTokenType::COMMA);
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
            if (is_identifier_token(peek())) {
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

            auto binding_expr = parse_expr_single();
            if (!binding_expr) {
               report_error("Expected expression after ':=' in let binding");
               return nullptr;
            }

            auto binding_node = std::make_unique<XPathNode>(XPathNodeType::LET_BINDING, variable_name);
            binding_node->add_child(std::move(binding_expr));
            binding_nodes.push_back(std::move(binding_node));

            parsing_bindings = match(XPathTokenType::COMMA);
         }

         continue;
      }

      break;
   }

   if (binding_nodes.empty()) {
      report_error("Expected 'for' or 'let' expression");
      return nullptr;
   }

   bool saw_where = false;
   bool saw_group = false;
   bool saw_order = false;
   bool saw_count_clause = false;
   bool has_non_binding_clause = false;

   while (true) {
      if (check_identifier_keyword("where")) {
         if (saw_where) {
            report_error("Multiple where clauses are not permitted in FLWOR expression");
            return nullptr;
         }

         if (saw_group) {
            report_error("where clause must precede group by clause");
            return nullptr;
         }

         if (saw_order) {
            report_error("where clause must precede order by clause");
            return nullptr;
         }

         if (saw_count_clause) {
            report_error("where clause must precede count clause");
            return nullptr;
         }

         auto where_clause = parse_where_clause();
         if (!where_clause) return nullptr;
         clause_nodes.push_back(std::move(where_clause));
         saw_where = true;
         has_non_binding_clause = true;
         continue;
      }

      if (check_identifier_keyword("group")) {
         if (saw_group) {
            report_error("Multiple group by clauses are not permitted in FLWOR expression");
            return nullptr;
         }

         if (saw_order) {
            report_error("group by clause must precede order by clause");
            return nullptr;
         }

         if (saw_count_clause) {
            report_error("group by clause must precede count clause");
            return nullptr;
         }

         auto group_clause = parse_group_clause();
         if (!group_clause) return nullptr;
         clause_nodes.push_back(std::move(group_clause));
         saw_group = true;
         has_non_binding_clause = true;
         continue;
      }

      if (check_identifier_keyword("stable")) {
         if (saw_order) {
            report_error("Multiple order by clauses are not permitted in FLWOR expression");
            return nullptr;
         }

         if (saw_count_clause) {
            report_error("order by clause must precede count clause");
            return nullptr;
         }

         auto order_clause = parse_order_clause(true);
         if (!order_clause) return nullptr;
         clause_nodes.push_back(std::move(order_clause));
         saw_order = true;
         has_non_binding_clause = true;
         continue;
      }

      if (check_identifier_keyword("order")) {
         if (saw_order) {
            report_error("Multiple order by clauses are not permitted in FLWOR expression");
            return nullptr;
         }

         if (saw_count_clause) {
            report_error("order by clause must precede count clause");
            return nullptr;
         }

         auto order_clause = parse_order_clause(false);
         if (!order_clause) return nullptr;
         clause_nodes.push_back(std::move(order_clause));
         saw_order = true;
         has_non_binding_clause = true;
         continue;
      }

      if (check_identifier_keyword("count")) {
         if (saw_count_clause) {
            report_error("Multiple count clauses are not permitted in FLWOR expression");
            return nullptr;
         }

         auto count_clause = parse_count_clause();
         if (!count_clause) return nullptr;
         clause_nodes.push_back(std::move(count_clause));
         saw_count_clause = true;
         has_non_binding_clause = true;
         continue;
      }

      break;
   }

   XPathToken return_token(XPathTokenType::UNKNOWN, std::string_view());
   if (!match_identifier_keyword("return", XPathTokenType::RETURN, return_token)) {
      report_error("Expected 'return' in FLWOR expression");
      return nullptr;
   }

   auto return_expr = parse_expr_single();
   if (!return_expr) {
      report_error("Expected expression after 'return'");
      return nullptr;
   }

   if (!has_non_binding_clause and saw_for and !saw_let) {
      auto for_node = std::make_unique<XPathNode>(XPathNodeType::FOR_EXPRESSION);
      for (auto &binding : binding_nodes) {
         if ((!binding) or !(binding->type IS XPathNodeType::FOR_BINDING)) {
            report_error("Invalid for binding in FLWOR expression");
            return nullptr;
         }
         for_node->add_child(std::move(binding));
      }
      for_node->add_child(std::move(return_expr));
      return for_node;
   }

   if (!has_non_binding_clause and saw_let and !saw_for) {
      auto let_node = std::make_unique<XPathNode>(XPathNodeType::LET_EXPRESSION);
      for (auto &binding : binding_nodes) {
         if ((!binding) or !(binding->type IS XPathNodeType::LET_BINDING)) {
            report_error("Invalid let binding in FLWOR expression");
            return nullptr;
         }
         let_node->add_child(std::move(binding));
      }
      let_node->add_child(std::move(return_expr));
      return let_node;
   }

   auto flwor_node = std::make_unique<XPathNode>(XPathNodeType::FLWOR_EXPRESSION);
   for (auto &binding : binding_nodes) {
      if (!binding) {
         report_error("Invalid clause in FLWOR expression");
         return nullptr;
      }
      flwor_node->add_child(std::move(binding));
   }
   for (auto &clause : clause_nodes) {
      if (!clause) {
         report_error("Invalid clause in FLWOR expression");
         return nullptr;
      }
      flwor_node->add_child(std::move(clause));
   }
   flwor_node->add_child(std::move(return_expr));
   return flwor_node;
}

//********************************************************************************************************************
// Parses a where clause in a FLWOR expression, consuming the 'where' keyword and filtering condition expression.

std::unique_ptr<XPathNode> XPathParser::parse_where_clause()
{
   XPathToken where_token(XPathTokenType::UNKNOWN, std::string_view());
   if (!match_identifier_keyword("where", XPathTokenType::WHERE, where_token)) {
      report_error("Expected 'where' clause");
      return nullptr;
   }

   auto predicate = parse_expr_single();
   if (!predicate) {
      report_error("Expected expression after 'where'");
      return nullptr;
   }

   auto clause = std::make_unique<XPathNode>(XPathNodeType::WHERE_CLAUSE);
   clause->add_child(std::move(predicate));
   return clause;
}

//********************************************************************************************************************
// Parses a group by clause with comma-separated variable bindings and key expressions for grouping.

std::unique_ptr<XPathNode> XPathParser::parse_group_clause()
{
   XPathToken group_token(XPathTokenType::UNKNOWN, std::string_view());
   if (!match_identifier_keyword("group", XPathTokenType::GROUP, group_token)) {
      report_error("Expected 'group' clause");
      return nullptr;
   }

   XPathToken by_token(XPathTokenType::UNKNOWN, std::string_view());
   if (!match_identifier_keyword("by", XPathTokenType::BY, by_token)) {
      report_error("Expected 'by' after 'group'");
      return nullptr;
   }

   auto clause = std::make_unique<XPathNode>(XPathNodeType::GROUP_CLAUSE);

   bool expect_key = true;
   while (expect_key) {
      if (!match(XPathTokenType::DOLLAR)) {
         report_error("Expected '$' to begin group by key binding");
         return nullptr;
      }

      std::string variable_name;
      if (is_identifier_token(peek())) {
         variable_name = peek().value;
         advance();
      }
      else {
         report_error("Expected variable name in group by key binding");
         return nullptr;
      }

      if (!match(XPathTokenType::ASSIGN)) {
         report_error("Expected ':=' after group by variable name");
         return nullptr;
      }

      auto key_expr = parse_expr_single();
      if (!key_expr) {
         report_error("Expected expression after ':=' in group by clause");
         return nullptr;
      }

      auto key_node = std::make_unique<XPathNode>(XPathNodeType::GROUP_KEY);
      XPathNode::XPathGroupKeyInfo info;
      info.variable_name = std::move(variable_name);
      key_node->set_group_key_info(std::move(info));
      key_node->add_child(std::move(key_expr));
      clause->add_child(std::move(key_node));

      expect_key = match(XPathTokenType::COMMA);
   }

   return clause;
}

//********************************************************************************************************************
// Parses an order by clause with optional stability modifier and comma-separated ordering specifications.

std::unique_ptr<XPathNode> XPathParser::parse_order_clause(bool StartsWithStable)
{
   bool clause_is_stable = false;

   if (StartsWithStable) {
      XPathToken stable_token(XPathTokenType::UNKNOWN, std::string_view());
      if (!match_identifier_keyword("stable", XPathTokenType::STABLE, stable_token)) {
         report_error("Expected 'stable' keyword to start stable order by clause");
         return nullptr;
      }
      clause_is_stable = true;
   }

   XPathToken order_token(XPathTokenType::UNKNOWN, std::string_view());
   if (!match_identifier_keyword("order", XPathTokenType::ORDER, order_token)) {
      report_error("Expected 'order' in order by clause");
      return nullptr;
   }

   XPathToken by_token(XPathTokenType::UNKNOWN, std::string_view());
   if (!match_identifier_keyword("by", XPathTokenType::BY, by_token)) {
      report_error("Expected 'by' after 'order'");
      return nullptr;
   }

   auto clause = std::make_unique<XPathNode>(XPathNodeType::ORDER_CLAUSE);
   clause->order_clause_is_stable = clause_is_stable;

   auto first_spec = parse_order_spec();
   if (!first_spec) return nullptr;
   clause->add_child(std::move(first_spec));

   while (match(XPathTokenType::COMMA)) {
      auto next_spec = parse_order_spec();
      if (!next_spec) return nullptr;
      clause->add_child(std::move(next_spec));
   }

   return clause;
}

//********************************************************************************************************************
// Parses an individual order specification with direction and empty value handling options.

std::unique_ptr<XPathNode> XPathParser::parse_order_spec()
{
   auto order_expr = parse_expr_single();
   if (!order_expr) {
      report_error("Expected expression in order by clause");
      return nullptr;
   }

   auto spec_node = std::make_unique<XPathNode>(XPathNodeType::ORDER_SPEC);
   spec_node->add_child(std::move(order_expr));

   XPathNode::XPathOrderSpecOptions options;
   bool has_options = false;

   XPathToken keyword_token(XPathTokenType::UNKNOWN, std::string_view());
   if (match_identifier_keyword("ascending", XPathTokenType::ASCENDING, keyword_token)) {
      has_options = true;
      options.is_descending = false;
   }
   else if (match_identifier_keyword("descending", XPathTokenType::DESCENDING, keyword_token)) {
      has_options = true;
      options.is_descending = true;
   }

   if (match_identifier_keyword("empty", XPathTokenType::EMPTY, keyword_token)) {
      has_options = true;
      options.has_empty_mode = true;
      if (match_identifier_keyword("greatest", XPathTokenType::GREATEST, keyword_token)) {
         options.empty_is_greatest = true;
      }
      else if (match_identifier_keyword("least", XPathTokenType::LEAST, keyword_token)) {
         options.empty_is_greatest = false;
      }
      else {
         report_error("Expected 'greatest' or 'least' after 'empty' in order by clause");
         return nullptr;
      }
   }

   if (match_identifier_keyword("collation", XPathTokenType::COLLATION, keyword_token)) {
      has_options = true;
      if (!check(XPathTokenType::STRING)) {
         report_error("Expected string literal after 'collation' in order by clause");
         return nullptr;
      }

      std::string collation_value(peek().value);
      advance();
      options.collation_uri = std::move(collation_value);
   }

   if (has_options) spec_node->set_order_spec_options(std::move(options));
   return spec_node;
}

//********************************************************************************************************************
// Parses a count clause binding a variable to a position counter in a FLWOR expression.

std::unique_ptr<XPathNode> XPathParser::parse_count_clause()
{
   XPathToken count_token(XPathTokenType::UNKNOWN, std::string_view());
   if (!match_identifier_keyword("count", XPathTokenType::COUNT, count_token)) {
      report_error("Expected 'count' clause");
      return nullptr;
   }

   if (!match(XPathTokenType::DOLLAR)) {
      report_error("Expected '$' after 'count'");
      return nullptr;
   }

   std::string variable_name;
   if (is_identifier_token(peek())) {
      variable_name = peek().value;
      advance();
   }
   else {
      report_error("Expected variable name in count clause");
      return nullptr;
   }

   return std::make_unique<XPathNode>(XPathNodeType::COUNT_CLAUSE, std::move(variable_name));
}

//********************************************************************************************************************
// Parses logical OR expressions, building left-associative binary operation trees from 'or' operators.

std::unique_ptr<XPathNode> XPathParser::parse_or_expr()
{
   auto left = parse_and_expr();

   while (check(XPathTokenType::OR)) {
      XPathToken op = peek();
      advance();
      auto right = parse_and_expr();
      left = create_binary_op(std::move(left), op, std::move(right));
   }

   return left;
}

//********************************************************************************************************************
// Parses logical AND expressions, building left-associative binary operation trees from 'and' operators.

std::unique_ptr<XPathNode> XPathParser::parse_and_expr()
{
   auto left = parse_equality_expr();

   while (check(XPathTokenType::AND)) {
      XPathToken op = peek();
      advance();
      auto right = parse_equality_expr();
      left = create_binary_op(std::move(left), op, std::move(right));
   }

   return left;
}

//********************************************************************************************************************
// Parses equality expressions, handling =, !=, eq, and ne operators with left-associative binding.

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

//********************************************************************************************************************
// Parses relational comparison expressions, handling <, <=, >, >=, lt, le, gt, and ge operators.

std::unique_ptr<XPathNode> XPathParser::parse_relational_expr()
{
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

//********************************************************************************************************************
// Parses additive expressions for + and - operators with left-associative binding.

std::unique_ptr<XPathNode> XPathParser::parse_additive_expr()
{
   auto left = parse_multiplicative_expr();

   while (check(XPathTokenType::PLUS) or check(XPathTokenType::MINUS)) {
      XPathToken op = peek();
      advance();
      auto right = parse_multiplicative_expr();
      left = create_binary_op(std::move(left), op, std::move(right));
   }

   return left;
}

//********************************************************************************************************************
// Parses multiplicative expressions for *, div, and mod operators with left-associative binding.

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

//********************************************************************************************************************
// Parses unary expressions, handling unary minus and logical NOT operators, allowing recursive unary operator
// application.

std::unique_ptr<XPathNode> XPathParser::parse_unary_expr()
{
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
         (void)match(XPathTokenType::RPAREN);
      }
      else operand = parse_unary_expr();

      return create_unary_op(op, std::move(operand));
   }

   return parse_union_expr();
}

//********************************************************************************************************************
// Parses set intersection and exception expressions using 'intersect' and 'except' operators.

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

//********************************************************************************************************************
// Parses union expressions combining multiple node sets with the '|' or 'union' operator into a single UNION node.

std::unique_ptr<XPathNode> XPathParser::parse_union_expr()
{
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

//********************************************************************************************************************
// Parses path expressions, distinguishing between location paths and filter expressions by examining token patterns.

std::unique_ptr<XPathNode> XPathParser::parse_path_expr()
{
   bool looks_like_path = false;

   if (check(XPathTokenType::SLASH) or check(XPathTokenType::DOUBLE_SLASH)) looks_like_path = true;
   else if (is_step_start_token(peek().type)) {
      looks_like_path = true;

      if (is_identifier_token(peek())) {
         if (current_token + 1 < tokens.size() and tokens[current_token + 1].type IS XPathTokenType::LPAREN) {
            looks_like_path = false;
         }
         else if (is_constructor_keyword(peek())) {
            size_t lookahead = current_token + 1;
            while ((lookahead < tokens.size()) and is_identifier_token(tokens[lookahead])) {
               lookahead++;
            }

            if (lookahead < tokens.size()) {
               auto next_type = tokens[lookahead].type;
               if ((next_type IS XPathTokenType::LBRACE) or (next_type IS XPathTokenType::STRING)) {
                  looks_like_path = false;
               }
            }
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

//********************************************************************************************************************
// Parses filter expressions consisting of a primary expression optionally followed by predicates and path
// continuations using / or // operators.

std::unique_ptr<XPathNode> XPathParser::parse_filter_expr()
{
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

//********************************************************************************************************************
// Parses conditional if-then-else expressions with mandatory condition, then branch, and else branch.

std::unique_ptr<XPathNode> XPathParser::parse_if_expr()
{
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

   auto then_branch = parse_expr_single();

   if (!match(XPathTokenType::ELSE)) {
      report_error("Expected 'else' in if expression");
      return nullptr;
   }

   auto else_branch = parse_expr_single();

   auto conditional = std::make_unique<XPathNode>(XPathNodeType::CONDITIONAL);
   conditional->add_child(std::move(condition));
   conditional->add_child(std::move(then_branch));
   conditional->add_child(std::move(else_branch));
   return conditional;
}

//********************************************************************************************************************
// Parses quantified expressions using 'some' or 'every' keywords with variable bindings and a 'satisfies' condition.

std::unique_ptr<XPathNode> XPathParser::parse_quantified_expr()
{
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
      if (is_identifier_token(peek())) {
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

      auto sequence_expr = parse_expr_single();
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

   auto condition_expr = parse_expr_single();
   if (!condition_expr) return nullptr;

   quant_node->add_child(std::move(condition_expr));
   return quant_node;
}

//********************************************************************************************************************
// Parses primary expressions including parenthesised expressions, direct and computed constructors, literals,
// numbers, variable references, function calls, and bare identifiers.

std::unique_ptr<XPathNode> XPathParser::parse_primary_expr()
{
   if (match(XPathTokenType::LPAREN)) {
      auto expr = parse_expr();
      (void)match(XPathTokenType::RPAREN);
      return expr;
   }

   if (check(XPathTokenType::TAG_OPEN)) {
      return parse_direct_constructor();
   }

   if (is_constructor_keyword(peek())) {
      size_t next_index = current_token + 1;
      bool is_function_call = (next_index < tokens.size()) and (tokens[next_index].type IS XPathTokenType::LPAREN);
      if (!is_function_call) return parse_computed_constructor();
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

   if (is_identifier_token(peek())) {
      if (current_token + 1 < tokens.size() and tokens[current_token + 1].type IS XPathTokenType::LPAREN) {
         return parse_function_call();
      }

      std::string value(peek().value);
      advance();
      return std::make_unique<XPathNode>(XPathNodeType::LITERAL, value);
   }

   return nullptr;
}

//********************************************************************************************************************
// Parses function call expressions with optional comma-separated arguments enclosed in parentheses.

std::unique_ptr<XPathNode> XPathParser::parse_function_call()
{
   if (!is_identifier_token(peek())) return nullptr;

   std::string function_name(peek().value);
   advance();

   if (!match(XPathTokenType::LPAREN)) return nullptr;

   auto function_node = std::make_unique<XPathNode>(XPathNodeType::FUNCTION_CALL, function_name);

   while (!check(XPathTokenType::RPAREN) and !is_at_end()) {
      auto arg = parse_expr_single();
      if (!arg) break;
      function_node->add_child(std::move(arg));

      if (!match(XPathTokenType::COMMA)) break;
   }

   (void)match(XPathTokenType::RPAREN);
   return function_node;
}

//********************************************************************************************************************
// Wrapper for parse_location_path() to maintain API compatibility for absolute location path parsing.

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
   return parse_expr_single();
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
      if (is_identifier_token(peek())) {
         std::string name(peek().value);
         advance();
         return std::make_unique<XPathNode>(XPathNodeType::VARIABLE_REFERENCE, name);
      }
   }
   return nullptr;
}

//********************************************************************************************************************
// Determines whether the supplied token introduces a computed constructor keyword,
// enabling the parser to divert from normal name lookup rules.

bool XPathParser::is_constructor_keyword(const XPathToken &Token) const
{
   if (Token.type IS XPathTokenType::IDENTIFIER) {
      std::string_view keyword = Token.value;
      if (keyword IS "element") return true;
      if (keyword IS "attribute") return true;
      if (keyword IS "text") return true;
      if (keyword IS "comment") return true;
      if (keyword IS "processing-instruction") return true;
      if (keyword IS "document") return true;
   }
   return false;
}

bool XPathParser::consume_token(XPathTokenType type, std::string_view ErrorMessage)
{
   if (match(type)) return true;
   report_error(ErrorMessage);
   return false;
}

//********************************************************************************************************************
// Parses the QName that follows constructor tokens, handling prefixed names and returning both the prefix and local
// part for later namespace resolution.

std::optional<XPathParser::ConstructorName> XPathParser::parse_constructor_qname()
{
   ConstructorName name;

   if (!is_identifier_token(peek())) {
      report_error("Expected name in constructor");
      return std::nullopt;
   }

   name.LocalName = std::string(peek().value);
   advance();

   if (match(XPathTokenType::COLON)) {
      name.Prefix = name.LocalName;
      if (!is_identifier_token(peek())) {
         report_error("Expected local name after ':' in constructor");
         return std::nullopt;
      }
      name.LocalName = std::string(peek().value);
      advance();
   }

   return name;
}

//********************************************************************************************************************
// Parses direct element constructors beginning with '<', capturing namespace
// declarations, attribute value templates, and nested content until the matching
// closing tag is encountered.

std::unique_ptr<XPathNode> XPathParser::parse_direct_constructor()
{
   if (!consume_token(XPathTokenType::TAG_OPEN, "Expected '<' to start direct constructor")) {
      return nullptr;
   }

   auto element_node = std::make_unique<XPathNode>(XPathNodeType::DIRECT_ELEMENT_CONSTRUCTOR);
   XPathConstructorInfo info;
   info.is_direct = true;
   info.is_empty_element = false;

   auto element_name = parse_constructor_qname();
   if (!element_name) return nullptr;

   info.prefix = element_name->Prefix;
   info.name = element_name->LocalName;

   std::vector<XPathConstructorAttribute> attributes;

   while (!check(XPathTokenType::TAG_CLOSE) and !check(XPathTokenType::EMPTY_TAG_CLOSE)) {
      if (is_at_end()) {
         report_error("Unexpected end of input in direct constructor start tag");
         return nullptr;
      }

      auto attribute_name = parse_constructor_qname();
      if (!attribute_name) return nullptr;

      XPathConstructorAttribute attribute;
      attribute.prefix = attribute_name->Prefix;
      attribute.name = attribute_name->LocalName;
      attribute.is_namespace_declaration = (attribute.prefix.empty() and attribute.name IS "xmlns") or (attribute.prefix IS "xmlns");

      if (!consume_token(XPathTokenType::EQUALS, "Expected '=' after attribute name")) {
         return nullptr;
      }

      if (!check(XPathTokenType::STRING)) {
         report_error("Expected quoted attribute value in direct constructor");
         return nullptr;
      }

      XPathToken attribute_token = peek();
      advance();

      std::vector<XPathAttributeValuePart> parts;
      if (!attribute_token.attribute_value_parts.empty()) {
         parts.reserve(attribute_token.attribute_value_parts.size());
         size_t part_index = 0;
         for (const auto &token_part : attribute_token.attribute_value_parts) {
            XPathAttributeValuePart part;
            part.is_expression = token_part.is_expression;
            part.text = token_part.text;
            if (part.is_expression) {
               auto expr = parse_embedded_expr(part.text);
               if (!expr) return nullptr;
               attribute.set_expression_for_part(part_index, std::move(expr));
            }
            parts.push_back(std::move(part));
            part_index++;
         }
      }
      else {
         XPathAttributeValuePart literal_part;
         literal_part.is_expression = false;
         literal_part.text = std::string(attribute_token.value);
         parts.push_back(std::move(literal_part));
      }

      if (attribute.is_namespace_declaration and !parts.empty() and !parts.front().is_expression) {
         attribute.namespace_uri = parts.front().text;
      }

      attribute.value_parts = std::move(parts);
      attributes.push_back(std::move(attribute));
   }

   if (check(XPathTokenType::EMPTY_TAG_CLOSE)) {
      (void)match(XPathTokenType::EMPTY_TAG_CLOSE);
      info.is_empty_element = true;
      info.attributes = std::move(attributes);
      element_node->set_constructor_info(std::move(info));
      return element_node;
   }

   if (!consume_token(XPathTokenType::TAG_CLOSE, "Expected '>' to close start tag")) {
      return nullptr;
   }

   info.attributes = std::move(attributes);

   std::string text_buffer;

   auto flush_text = [&]() {
      if (text_buffer.empty()) return;
      auto text_node = std::make_unique<XPathNode>(XPathNodeType::CONSTRUCTOR_CONTENT, text_buffer);
      element_node->add_child(std::move(text_node));
      text_buffer.clear();
   };

   while (!check(XPathTokenType::CLOSE_TAG_OPEN)) {
      if (is_at_end()) {
         report_error("Unexpected end of input in direct constructor content");
         return nullptr;
      }

      if (check(XPathTokenType::TAG_OPEN)) {
         flush_text();
         auto child = parse_direct_constructor();
         if (!child) return nullptr;
         element_node->add_child(std::move(child));
         continue;
      }

      if (check(XPathTokenType::LBRACE)) {
         flush_text();
         auto expr = parse_enclosed_expr();
         if (!expr) return nullptr;
         auto content_node = std::make_unique<XPathNode>(XPathNodeType::CONSTRUCTOR_CONTENT);
         content_node->add_child(std::move(expr));
         element_node->add_child(std::move(content_node));
         continue;
      }

      const XPathToken &token = peek();
      if (token.type IS XPathTokenType::END_OF_INPUT) {
         report_error("Unexpected end of input in direct constructor content");
         return nullptr;
      }

      text_buffer += std::string(token.value);
      advance();
   }

   flush_text();

   if (!consume_token(XPathTokenType::CLOSE_TAG_OPEN, "Expected closing tag")) {
      return nullptr;
   }

   auto closing_name = parse_constructor_qname();
   if (!closing_name) return nullptr;

   if (!consume_token(XPathTokenType::TAG_CLOSE, "Expected '>' to close end tag")) {
      return nullptr;
   }

   if (not (closing_name->Prefix IS info.prefix and closing_name->LocalName IS info.name)) {
      report_error("Mismatched closing tag in direct constructor");
      return nullptr;
   }

   element_node->set_constructor_info(std::move(info));
   return element_node;
}

std::unique_ptr<XPathNode> XPathParser::parse_enclosed_expr()
{
   if (!consume_token(XPathTokenType::LBRACE, "Expected '{' to begin expression")) {
      return nullptr;
   }

   auto expr = parse_expr();
   if (!expr) return nullptr;

   if (!consume_token(XPathTokenType::RBRACE, "Expected '}' to close expression")) {
      return nullptr;
   }

   return expr;
}

std::unique_ptr<XPathNode> XPathParser::parse_embedded_expr(std::string_view Source)
{
   XPathTokeniser embedded_tokeniser;
   auto token_list = embedded_tokeniser.tokenize(Source);

   XPathParser embedded_parser;
   auto expr = embedded_parser.parse(token_list);

   if (!expr or embedded_parser.has_errors()) {
      if (embedded_parser.has_errors()) {
         for (const auto &message : embedded_parser.get_errors()) {
            report_error(message);
         }
      }

      if (!expr) report_error("Failed to parse embedded expression");

      return nullptr;
   }

   return expr;
}

//********************************************************************************************************************
// Dispatches to the appropriate computed constructor parser based on the leading keyword so each form can apply its
// specialised grammar.

std::unique_ptr<XPathNode> XPathParser::parse_computed_constructor()
{
   std::string keyword(peek().value);
   advance();

   if (keyword IS "element") return parse_computed_element_constructor();
   if (keyword IS "attribute") return parse_computed_attribute_constructor();
   if (keyword IS "text") return parse_computed_text_constructor();
   if (keyword IS "comment") return parse_computed_comment_constructor();
   if (keyword IS "processing-instruction") return parse_computed_pi_constructor();
   if (keyword IS "document") return parse_computed_document_constructor();

   report_error("Unsupported computed constructor keyword");
   return nullptr;
}

//********************************************************************************************************************
// Parses computed element constructors, optionally accepting enclosed expressions for the element name and always
// parsing the content expression sequence.

std::unique_ptr<XPathNode> XPathParser::parse_computed_element_constructor()
{
   auto node = std::make_unique<XPathNode>(XPathNodeType::COMPUTED_ELEMENT_CONSTRUCTOR);
   XPathConstructorInfo info;
   info.is_direct = false;
   info.is_empty_element = false;

   if (check(XPathTokenType::LBRACE)) {
      auto name_expr = parse_enclosed_expr();
      if (!name_expr) return nullptr;
      node->set_name_expression(std::move(name_expr));
   }
   else {
      auto name = parse_constructor_qname();
      if (!name) return nullptr;
      info.prefix = name->Prefix;
      info.name = name->LocalName;
   }

   auto content_expr = parse_enclosed_expr();
   if (!content_expr) return nullptr;

   auto content_node = std::make_unique<XPathNode>(XPathNodeType::CONSTRUCTOR_CONTENT);
   content_node->add_child(std::move(content_expr));
   node->add_child(std::move(content_node));

   node->set_constructor_info(std::move(info));
   return node;
}

//********************************************************************************************************************
// Parses computed attribute constructors which may provide the attribute name either as a literal QName or as an
// enclosed expression, followed by the attribute value expression.

std::unique_ptr<XPathNode> XPathParser::parse_computed_attribute_constructor()
{
   auto node = std::make_unique<XPathNode>(XPathNodeType::COMPUTED_ATTRIBUTE_CONSTRUCTOR);
   XPathConstructorInfo info;
   info.is_direct = false;
   info.is_empty_element = false;

   if (check(XPathTokenType::LBRACE)) {
      auto name_expr = parse_enclosed_expr();
      if (!name_expr) return nullptr;
      node->set_name_expression(std::move(name_expr));
   }
   else {
      auto name = parse_constructor_qname();
      if (!name) return nullptr;
      info.prefix = name->Prefix;
      info.name = name->LocalName;
   }

   auto value_expr = parse_enclosed_expr();
   if (!value_expr) return nullptr;

   auto content_node = std::make_unique<XPathNode>(XPathNodeType::CONSTRUCTOR_CONTENT);
   content_node->add_child(std::move(value_expr));
   node->add_child(std::move(content_node));

   node->set_constructor_info(std::move(info));
   return node;
}

//********************************************************************************************************************
// Parses computed text constructors by wrapping the enclosed expression inside a CONSTRUCTOR_CONTENT node so the
// evaluator can produce the resulting text node.

std::unique_ptr<XPathNode> XPathParser::parse_computed_text_constructor()
{
   auto node = std::make_unique<XPathNode>(XPathNodeType::TEXT_CONSTRUCTOR);
   auto content_expr = parse_enclosed_expr();
   if (!content_expr) return nullptr;

   auto content_node = std::make_unique<XPathNode>(XPathNodeType::CONSTRUCTOR_CONTENT);
   content_node->add_child(std::move(content_expr));
   node->add_child(std::move(content_node));
   return node;
}

//********************************************************************************************************************
// Parses computed comment constructors mirroring the text constructor structure but targeting comment nodes in the AST.

std::unique_ptr<XPathNode> XPathParser::parse_computed_comment_constructor()
{
   auto node = std::make_unique<XPathNode>(XPathNodeType::COMMENT_CONSTRUCTOR);
   auto content_expr = parse_enclosed_expr();
   if (!content_expr) return nullptr;

   auto content_node = std::make_unique<XPathNode>(XPathNodeType::CONSTRUCTOR_CONTENT);
   content_node->add_child(std::move(content_expr));
   node->add_child(std::move(content_node));
   return node;
}

//********************************************************************************************************************
// Parses computed processing-instruction constructors, supporting literal or computed targets along with the
// required content expression.

std::unique_ptr<XPathNode> XPathParser::parse_computed_pi_constructor()
{
   auto node = std::make_unique<XPathNode>(XPathNodeType::PI_CONSTRUCTOR);
   XPathConstructorInfo info;
   info.is_direct = false;
   info.is_empty_element = false;

   if (check(XPathTokenType::LBRACE)) {
      auto target_expr = parse_enclosed_expr();
      if (!target_expr) return nullptr;
      node->set_name_expression(std::move(target_expr));
   }
   else if (check(XPathTokenType::STRING)) {
      info.name = std::string(peek().value);
      advance();
   }
   else if (is_identifier_token(peek())) {
      info.name = std::string(peek().value);
      advance();
      if (check(XPathTokenType::COLON)) {
         report_error("Processing-instruction target must be an NCName");
         return nullptr;
      }
   }
   else {
      report_error("Expected processing-instruction target");
      return nullptr;
   }

   auto content_expr = parse_enclosed_expr();
   if (!content_expr) return nullptr;

   auto content_node = std::make_unique<XPathNode>(XPathNodeType::CONSTRUCTOR_CONTENT);
   content_node->add_child(std::move(content_expr));
   node->add_child(std::move(content_node));

   node->set_constructor_info(std::move(info));
   return node;
}

//********************************************************************************************************************
// Parses computed document constructors that evaluate their enclosed expression to populate a synthetic document
// node.

std::unique_ptr<XPathNode> XPathParser::parse_computed_document_constructor()
{
   auto node = std::make_unique<XPathNode>(XPathNodeType::DOCUMENT_CONSTRUCTOR);
   auto content_expr = parse_enclosed_expr();
   if (!content_expr) return nullptr;

   auto content_node = std::make_unique<XPathNode>(XPathNodeType::CONSTRUCTOR_CONTENT);
   content_node->add_child(std::move(content_expr));
   node->add_child(std::move(content_node));
   return node;
}
