// XPath Tokenizer and Parser
//
// This file contains:
// - XPath tokenization (converting string to tokens)
// - XPath parsing (converting tokens to AST)
// - Grammar implementation for XPath 1.0 syntax

#pragma once

//********************************************************************************************************************
// XPath Tokenizer

class XPathTokenizer {
   private:
   std::string_view input;
   size_t position;
   size_t length;

   // Character classification
   bool is_alpha(char c) const;
   bool is_digit(char c) const;
   bool is_alnum(char c) const;
   bool is_whitespace(char c) const;
   bool is_name_start_char(char c) const;
   bool is_name_char(char c) const;

   // Token extraction methods
   XPathToken scan_identifier();
   XPathToken scan_number();
   XPathToken scan_string(char quote_char);
   XPathToken scan_operator();

   // Lookahead and utility
   char peek(size_t offset = 0) const;
   void skip_whitespace();
   bool match(const char* str);

   public:
   XPathTokenizer() : position(0), length(0) {}

   std::vector<XPathToken> tokenize(std::string_view xpath);
   bool has_more() const;
   char current() const;
   void advance();
};

//********************************************************************************************************************
// XPath Parser

class XPathParser {
   private:
   std::vector<XPathToken> tokens;
   size_t current_token;

   // Grammar rule methods (XPath 1.0 grammar)
   std::unique_ptr<XPathNode> parse_expr();
   std::unique_ptr<XPathNode> parse_or_expr();
   std::unique_ptr<XPathNode> parse_and_expr();
   std::unique_ptr<XPathNode> parse_equality_expr();
   std::unique_ptr<XPathNode> parse_relational_expr();
   std::unique_ptr<XPathNode> parse_additive_expr();
   std::unique_ptr<XPathNode> parse_multiplicative_expr();
   std::unique_ptr<XPathNode> parse_unary_expr();
   std::unique_ptr<XPathNode> parse_union_expr();
   std::unique_ptr<XPathNode> parse_path_expr();
   std::unique_ptr<XPathNode> parse_filter_expr();
   std::unique_ptr<XPathNode> parse_location_path();
   std::unique_ptr<XPathNode> parse_absolute_location_path();
   std::unique_ptr<XPathNode> parse_relative_location_path();
   std::unique_ptr<XPathNode> parse_step();
   std::unique_ptr<XPathNode> parse_axis_specifier();
   std::unique_ptr<XPathNode> parse_node_test();
   std::unique_ptr<XPathNode> parse_predicate();
   std::unique_ptr<XPathNode> parse_abbreviated_step();
   std::unique_ptr<XPathNode> parse_primary_expr();
   std::unique_ptr<XPathNode> parse_function_call();
   std::unique_ptr<XPathNode> parse_argument();
   std::unique_ptr<XPathNode> parse_number();
   std::unique_ptr<XPathNode> parse_literal();
   std::unique_ptr<XPathNode> parse_variable_reference();

   // Utility methods
   bool check(XPathTokenType type) const;
   bool match(XPathTokenType type);
   XPathToken consume(XPathTokenType type, const std::string& error_message);
   XPathToken peek() const;
   XPathToken previous() const;
   bool is_at_end() const;
   void advance();

   // Node creation helpers
   std::unique_ptr<XPathNode> create_binary_op(std::unique_ptr<XPathNode> left, const XPathToken& op, std::unique_ptr<XPathNode> right);
   std::unique_ptr<XPathNode> create_unary_op(const XPathToken& op, std::unique_ptr<XPathNode> operand);

   public:
   XPathParser() : current_token(0) {}

   std::unique_ptr<XPathNode> parse(const std::vector<XPathToken>& token_list);

   // Error handling
   void report_error(const std::string& message);
   bool has_errors() const;
   std::vector<std::string> get_errors() const;

   private:
   std::vector<std::string> errors;
};