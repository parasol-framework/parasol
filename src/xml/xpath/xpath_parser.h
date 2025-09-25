// XPath Tokenizer and Parser
//
// This file contains:
// - XPath tokenization (converting string to tokens)
// - XPath parsing (converting tokens to AST)
// - Grammar implementation for XPath 1.0 syntax

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

//********************************************************************************************************************
// XPath Tokenizer

class XPathTokenizer {
   private:
   std::string_view input;
   size_t position;
   size_t length;

   // String interning for frequently used identifiers
   static std::unordered_map<std::string_view, std::string> interned_strings;
   static void initialize_interned_strings();

   // Character classification
   [[nodiscard]] inline bool is_alpha(char c) const;
   [[nodiscard]] inline bool is_digit(char c) const;
   [[nodiscard]] inline bool is_alnum(char c) const;
   [[nodiscard]] inline bool is_whitespace(char c) const;
   [[nodiscard]] inline bool is_name_start_char(char c) const;
   [[nodiscard]] inline bool is_name_char(char c) const;

   // Token extraction methods
   XPathToken scan_identifier();
   XPathToken scan_number();
   XPathToken scan_string(char QuoteChar);
   XPathToken scan_operator();

   // Lookahead and utility
   [[nodiscard]] inline char peek(size_t offset = 0) const;
   inline void skip_whitespace();
   [[nodiscard]] inline bool match(std::string_view Str);

   public:
   XPathTokenizer() : position(0), length(0) {}

   std::vector<XPathToken> tokenize(std::string_view XPath);
   bool has_more() const;
   [[nodiscard]]  char current() const;
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
   std::string parse_predicate_literal();
   std::unique_ptr<XPathNode> parse_abbreviated_step();
   std::unique_ptr<XPathNode> parse_primary_expr();
   std::unique_ptr<XPathNode> parse_function_call();
   std::unique_ptr<XPathNode> parse_argument();
   std::unique_ptr<XPathNode> parse_number();
   std::unique_ptr<XPathNode> parse_literal();

   // Utility methods
   bool check(XPathTokenType type) const;
   bool match(XPathTokenType type);
   const XPathToken & peek() const;
   const XPathToken & previous() const;
   bool is_at_end() const;
   void advance();
   bool is_step_start_token(XPathTokenType type) const;

   // Node creation helpers
   std::unique_ptr<XPathNode> create_binary_op(std::unique_ptr<XPathNode> Left, const XPathToken &Op, std::unique_ptr<XPathNode> Right);
   std::unique_ptr<XPathNode> create_unary_op(const XPathToken &Op, std::unique_ptr<XPathNode> Operand);

   public:
   XPathParser() : current_token(0) {}

   std::unique_ptr<XPathNode> parse(const std::vector<XPathToken> &TokenList);

   // Error handling
   void report_error(std::string_view Message);
   bool has_errors() const;
   std::vector<std::string> get_errors() const;

   private:
   std::vector<std::string> errors;
};
