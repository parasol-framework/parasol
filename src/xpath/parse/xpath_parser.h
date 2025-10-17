// XPath Tokeniser and Parser
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
#include "xpath_tokeniser.h"

struct XQueryProlog;
struct XQueryModuleCache;

struct XPathParseResult
{
   std::unique_ptr<XPathNode> expression;
   std::shared_ptr<XQueryProlog> prolog;
   std::shared_ptr<XQueryModuleCache> module_cache;
};

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

   // Prolog parsing helpers
   bool parse_prolog(XQueryProlog &prolog);
   bool parse_declare_statement(XQueryProlog &prolog);
   bool parse_namespace_decl(XQueryProlog &prolog);
   bool parse_default_namespace_decl(XQueryProlog &prolog, bool IsFunctionNamespace);
   bool parse_default_collation_decl(XQueryProlog &prolog);
   bool parse_variable_decl(XQueryProlog &prolog);
   bool parse_function_decl(XQueryProlog &prolog);
   bool parse_boundary_space_decl(XQueryProlog &prolog);
   bool parse_base_uri_decl(XQueryProlog &prolog);
   bool parse_construction_decl(XQueryProlog &prolog);
   bool parse_ordering_decl(XQueryProlog &prolog);
   bool parse_empty_order_decl(XQueryProlog &prolog);
   bool parse_copy_namespaces_decl(XQueryProlog &prolog);
   bool parse_decimal_format_decl(XQueryProlog &prolog);
   bool parse_option_decl(XQueryProlog &prolog);
   bool parse_import_statement(XQueryProlog &prolog);
   bool parse_import_module_decl(XQueryProlog &prolog);
   bool parse_import_schema_decl();
   void consume_declaration_separator();
   std::optional<std::string> parse_qname_string();
   std::optional<std::string> parse_ncname();
   std::optional<std::string> parse_string_literal_value();
   std::optional<std::string> parse_uri_literal();
   std::optional<std::string> collect_sequence_type();

   // Utility methods
   [[nodiscard]] inline bool check(XPathTokenType type) const {
      return peek().type IS type;
   }

   [[nodiscard]] bool is_function_call_ahead(size_t Index) const;

   [[nodiscard]] inline bool match(XPathTokenType type) {
      if (check(type)) {
         advance();
         return true;
      }
      return false;
   }

   bool check_identifier_keyword(std::string_view Keyword) const;
   bool match_identifier_keyword(std::string_view Keyword, XPathTokenType KeywordType, XPathToken &OutToken);
   bool match_literal_keyword(std::string_view Keyword);
   bool check_literal_keyword(std::string_view Keyword) const;

   // Returns true if the given keyword token type can function as an identifier
   // in name contexts (element names, attribute names, function names, etc.).
   // All XPath/XQuery keywords are valid XML names and should be permitted.
   [[nodiscard]] bool is_keyword_acceptable_as_identifier(XPathTokenType Type) const;

   // Helper that treats keyword tokens as identifiers in name contexts.
   // Use this for steps, function names, predicates, and variable bindings, where keywords are valid identifiers.
   // All XPath/XQuery keywords can function as element/attribute names since they are valid XML NCNames.
   bool is_identifier_token(const XPathToken &Token) const {
      if (Token.type IS XPathTokenType::IDENTIFIER) return true;
      return is_keyword_acceptable_as_identifier(Token.type);
   }

   [[nodiscard]] bool is_constructor_keyword(const XPathToken &Token) const;

   // Lightweight representation of a QName recognised within constructor syntax.

   struct ConstructorName {
      std::string Prefix;
      std::string LocalName;
   };

   std::optional<ConstructorName> parse_constructor_qname();
   bool consume_token(XPathTokenType, std::string_view);

   [[nodiscard]] inline const XPathToken & peek() const {
      return current_token < tokens.size() ? tokens[current_token] : tokens.back(); // END_OF_INPUT
   }

   [[nodiscard]] inline const XPathToken & previous() const {
      return tokens[current_token - 1];
   }

   [[nodiscard]] inline bool is_at_end() const {
      return peek().type IS XPathTokenType::END_OF_INPUT;
   }

   inline void advance() {
      if (!is_at_end()) current_token++;
   }

   [[nodiscard]] inline bool is_step_start_token(XPathTokenType type) const {
      // Structural tokens that start steps
      if (type IS XPathTokenType::DOT or
          type IS XPathTokenType::DOUBLE_DOT or
          type IS XPathTokenType::AT or
          type IS XPathTokenType::WILDCARD or
          type IS XPathTokenType::IDENTIFIER) {
         return true;
      }

      // All keyword tokens can start steps (as element names)
      return is_keyword_acceptable_as_identifier(type);
   }

   // Node creation helpers
   std::unique_ptr<XPathNode> create_binary_op(std::unique_ptr<XPathNode> Left, const XPathToken &Op, std::unique_ptr<XPathNode> Right);
   std::unique_ptr<XPathNode> create_unary_op(const XPathToken &Op, std::unique_ptr<XPathNode> Operand);

   public:
   XPathParser() : current_token(0) {}

   XPathParseResult parse(const std::vector<XPathToken> &TokenList);

   // Error handling

   inline void report_error(std::string_view Message) { errors.emplace_back(Message); }
   [[nodiscard]] inline bool has_errors() const { return !errors.empty(); }
   inline const std::vector<std::string> & get_errors() const { return errors; }

   private:
   std::vector<std::string> errors;
};
