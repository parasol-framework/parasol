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

#include "xpath_ast.h"
#include "xpath_tokenizer.h"

//********************************************************************************************************************
// XPath Parser

class XPathParser {
   private:
   std::vector<XPathToken> tokens;
   size_t current_token;

   // Grammar rule methods (XPath 1.0 grammar)
   std::unique_ptr<XPathNode> parse_expr();
   std::unique_ptr<XPathNode> parse_flwor_expr();
   std::unique_ptr<XPathNode> parse_or_expr();
   std::unique_ptr<XPathNode> parse_and_expr();
   std::unique_ptr<XPathNode> parse_equality_expr();
   std::unique_ptr<XPathNode> parse_relational_expr();
   std::unique_ptr<XPathNode> parse_additive_expr();
   std::unique_ptr<XPathNode> parse_multiplicative_expr();
   std::unique_ptr<XPathNode> parse_unary_expr();
   std::unique_ptr<XPathNode> parse_intersect_expr();
   std::unique_ptr<XPathNode> parse_union_expr();
   std::unique_ptr<XPathNode> parse_path_expr();
   std::unique_ptr<XPathNode> parse_filter_expr();
   std::unique_ptr<XPathNode> parse_if_expr();
   std::unique_ptr<XPathNode> parse_quantified_expr();
   std::unique_ptr<XPathNode> parse_location_path();
   std::unique_ptr<XPathNode> parse_absolute_location_path();
   std::unique_ptr<XPathNode> parse_relative_location_path();
   std::unique_ptr<XPathNode> parse_step();
   std::unique_ptr<XPathNode> parse_axis_specifier();
   std::unique_ptr<XPathNode> parse_node_test();
   std::unique_ptr<XPathNode> parse_predicate();
   std::unique_ptr<XPathNode> parse_predicate_value();
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
   bool check_identifier_keyword(std::string_view Keyword) const;
   bool match_identifier_keyword(std::string_view Keyword, XPathTokenType KeywordType, XPathToken &OutToken);
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
   const std::vector<std::string> & get_errors() const;

   private:
   std::vector<std::string> errors;
};

//********************************************************************************************************************
// Compiled XPath - A reusable compiled XPath expression

class CompiledXPath {
   private:
   std::shared_ptr<XPathNode> ast;
   std::string original_expression;
   std::vector<std::string> errors;
   bool is_valid;

   public:
   CompiledXPath() : is_valid(false) {}

   // Compile an XPath expression
   static CompiledXPath compile(std::string_view XPath);

   // Check if compilation was successful
   [[nodiscard]] bool isValid() const { return is_valid; }

   // Get the compiled AST (for internal use by evaluator)
   [[nodiscard]] const XPathNode * getAST() const { return ast.get(); }
   [[nodiscard]] std::shared_ptr<XPathNode> getASTShared() const { return ast; }

   // Get the original expression
   [[nodiscard]] const std::string & getExpression() const { return original_expression; }

   // Get compilation errors
   [[nodiscard]] const std::vector<std::string> & getErrors() const { return errors; }

   // Move constructor and assignment for efficient storage
   CompiledXPath(CompiledXPath &&other) noexcept
      : ast(std::move(other.ast)),
        original_expression(std::move(other.original_expression)),
        errors(std::move(other.errors)),
        is_valid(other.is_valid) {
      other.is_valid = false;
   }

   CompiledXPath & operator=(CompiledXPath &&other) noexcept {
      if (this != &other) {
         ast = std::move(other.ast);
         original_expression = std::move(other.original_expression);
         errors = std::move(other.errors);
         is_valid = other.is_valid;
         other.is_valid = false;
      }
      return *this;
   }

   // Disable copy constructor and assignment to avoid shared AST issues
   CompiledXPath(const CompiledXPath &) = delete;
   CompiledXPath & operator=(const CompiledXPath &) = delete;
};
