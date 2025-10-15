// XPath Tokenizer and Parser
//
// This file contains:
// - XPath tokenization (converting string to tokens)
// - XPath parsing (converting tokens to AST)
// - Grammar implementation for XPath syntax

#pragma once

#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <parasol/modules/xpath.h>

#include "xpath_ast.h"
#include "xpath_tokenizer.h"

//********************************************************************************************************************
// XPath Parser

class XPathParser {
   private:
   std::vector<XPathToken> tokens;
   size_t current_token;

   // Grammar rule methods
   std::unique_ptr<XPathNode> parse_expr();
   std::unique_ptr<XPathNode> parse_expr_single();
   std::unique_ptr<XPathNode> parse_flwor_expr();
   std::unique_ptr<XPathNode> parse_where_clause();
   std::unique_ptr<XPathNode> parse_group_clause();
   std::unique_ptr<XPathNode> parse_order_clause(bool StartsWithStable);
   std::unique_ptr<XPathNode> parse_order_spec();
   std::unique_ptr<XPathNode> parse_count_clause();
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
   std::unique_ptr<XPathNode> parse_direct_constructor();
   std::unique_ptr<XPathNode> parse_computed_constructor();
   std::unique_ptr<XPathNode> parse_computed_element_constructor();
   std::unique_ptr<XPathNode> parse_computed_attribute_constructor();
   std::unique_ptr<XPathNode> parse_computed_text_constructor();
   std::unique_ptr<XPathNode> parse_computed_comment_constructor();
   std::unique_ptr<XPathNode> parse_computed_pi_constructor();
   std::unique_ptr<XPathNode> parse_computed_document_constructor();
   std::unique_ptr<XPathNode> parse_enclosed_expr();
   std::unique_ptr<XPathNode> parse_embedded_expr(std::string_view Source);

   // Utility methods
   bool check(XPathTokenType type) const;
   bool match(XPathTokenType type);
   bool check_identifier_keyword(std::string_view Keyword) const;
   bool match_identifier_keyword(std::string_view Keyword, XPathTokenType KeywordType, XPathToken &OutToken);
   // Helper that treats certain keyword tokens (e.g., COUNT, EMPTY) as identifiers.
   // Use this for steps, function names, predicates, and variable bindings, where such keywords are valid identifiers.
   // This differs from a simple IDENTIFIER type check, which would exclude these keyword tokens.
   bool is_identifier_token(const XPathToken &Token) const;
   bool is_constructor_keyword(const XPathToken &Token) const;

   // Lightweight representation of a QName recognised within constructor syntax.

   struct ConstructorName {
      std::string Prefix;
      std::string LocalName;
   };
   std::optional<ConstructorName> parse_constructor_qname();
   bool consume_token(XPathTokenType type, std::string_view ErrorMessage);
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
