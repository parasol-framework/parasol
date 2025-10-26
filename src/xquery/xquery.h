#pragma once

#include <parasol/system/errors.h>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <utility>
#include <parasol/modules/xquery.h>

//*********************************************************************************************************************

enum class BinaryOperationKind {
   AND,
   OR,
   UNION,
   INTERSECT,
   EXCEPT,
   COMMA,
   EQ,
   NE,
   EQ_WORD,
   NE_WORD,
   LT,
   LE,
   GT,
   GE,
   ADD,
   SUB,
   MUL,
   DIV,
   MOD,
   RANGE,
   UNKNOWN
};

enum class XPathTokenType {
   // Path operators
   SLASH,             // /
   DOUBLE_SLASH,      // //
   DOT,               // .
   DOUBLE_DOT,        // ..

   // Identifiers and literals
   IDENTIFIER,        // element names, function names
   STRING,            // quoted strings
   NUMBER,            // numeric literals
   WILDCARD,          // *

   // Brackets and delimiters
   LBRACKET,          // [
   RBRACKET,          // ]
   LPAREN,            // (
   RPAREN,            // )
   AT,                // @
   COMMA,             // ,
   SEMICOLON,         // ;
   PIPE,              // |
   UNION,             // union keyword
   INTERSECT,         // intersect keyword
   EXCEPT,            // except keyword

   // Operators
   EQUALS,            // =
   NOT_EQUALS,        // !=
   LESS_THAN,         // <
   LESS_EQUAL,        // <=
   GREATER_THAN,      // >
   GREATER_EQUAL,     // >=
   EQ,                // eq
   NE,                // ne
   LT,                // lt
   LE,                // le
   GT,                // gt
   GE,                // ge

   // Boolean operators
   AND,               // and
   OR,                // or
   NOT,               // not

   // Flow keywords
   IF,                // if
   THEN,              // then
   ELSE,              // else
   FOR,               // for
   LET,               // let
   IN,                // in
   RETURN,            // return
   WHERE,             // where
   GROUP,             // group
   BY,                // by
   ORDER,             // order
   STABLE,            // stable
   ASCENDING,         // ascending
   DESCENDING,        // descending
   EMPTY,             // empty
   DEFAULT,           // default
   TYPESWITCH,        // typeswitch keyword
   CASE,              // case keyword
   DECLARE,           // declare keyword
   FUNCTION,          // function keyword
   VARIABLE,          // variable keyword
   NAMESPACE,         // namespace keyword
   EXTERNAL,          // external keyword
   BOUNDARY_SPACE,    // boundary-space keyword
   BASE_URI,          // base-uri keyword
   GREATEST,          // greatest
   LEAST,             // least
   COLLATION,         // collation
   CONSTRUCTION,      // construction
   ORDERING,          // ordering keyword used in prolog
   COPY_NAMESPACES,   // copy-namespaces keyword
   DECIMAL_FORMAT,    // decimal-format keyword
   OPTION,            // option keyword
   IMPORT,            // import keyword
   MODULE,            // module keyword
   SCHEMA,            // schema keyword
   COUNT,             // count
   SOME,              // some
   EVERY,             // every
   SATISFIES,         // satisfies
   CAST,              // cast keyword
   CASTABLE,          // castable keyword
   TREAT,             // treat keyword
   AS,                // as keyword
   INSTANCE,          // instance keyword
   OF,                // of keyword
   TO,                // to keyword

   // Arithmetic operators
   PLUS,              // +
   MINUS,             // -
   MULTIPLY,          // * (when not wildcard)
   DIVIDE,            // div
   MODULO,            // mod

   // Axis specifiers
   AXIS_SEPARATOR,    // ::
   COLON,             // :

   // Variables and functions
   DOLLAR,            // $
   ASSIGN,            // :=

   // Constructor delimiters
   LBRACE,            // {
   RBRACE,            // }
   TAG_OPEN,          // < (direct constructors)
   CLOSE_TAG_OPEN,    // </
   TAG_CLOSE,         // >
   EMPTY_TAG_CLOSE,   // />
   PI_START,          // <?
   PI_END,            // ?>

   // Special tokens
   TEXT_CONTENT,      // literal content inside direct constructors
   QUESTION_MARK,     // ? occurrence indicator
   END_OF_INPUT,
   UNKNOWN
};

// XPath Axis Types

enum class AxisType {
   CHILD,
   DESCENDANT,
   PARENT,
   ANCESTOR,
   FOLLOWING_SIBLING,
   PRECEDING_SIBLING,
   FOLLOWING,
   PRECEDING,
   ATTRIBUTE,
   NAMESPACE,
   SELF,
   DESCENDANT_OR_SELF,
   ANCESTOR_OR_SELF
};

enum class SequenceCardinality {
   ExactlyOne,
   ZeroOrOne,
   OneOrMore,
   ZeroOrMore
};

enum class SequenceItemKind {
   Atomic,
   Element,
   Attribute,
   Text,
   Node,
   Item,
   EmptySequence
};

//********************************************************************************************************************

struct XPathNode;

class XPathErrorReporter {
   public:
   virtual ~XPathErrorReporter() = default;
   virtual void record_error(std::string_view Message, bool Force = false) = 0;
   virtual void record_error(std::string_view Message, const XPathNode *Node, bool Force = false) = 0;
};

// Represents a user-defined decimal format declared in the prolog.

struct DecimalFormat {
   std::string name;
   std::string decimal_separator = ".";
   std::string grouping_separator = ",";
   std::string infinity = "INF";
   std::string minus_sign = "-";
   std::string nan = "NaN";
   std::string percent = "%";
   std::string per_mille = "‰";
   std::string zero_digit = "0";
   std::string digit = "#";
   std::string pattern_separator = ";";
};

// Represents a user-defined XQuery function declared in the prolog.

struct XQueryFunction {
   std::string qname;
   std::vector<std::string> parameter_names;
   std::vector<std::string> parameter_types;
   std::optional<std::string> return_type;
   std::unique_ptr<XPathNode> body;
   bool is_external = false;

   [[nodiscard]] std::string signature() const;
};

// Represents a user-defined XQuery variable declared in the prolog.

struct XQueryVariable {
   std::string qname;
   std::unique_ptr<XPathNode> initializer;
   bool is_external = false;
};

// Represents an XQuery module import declaration.

struct XQueryModuleImport {
   std::string target_namespace;
   std::vector<std::string> location_hints;
};

struct XPathNode;

struct XPathAttributeValuePart {
   bool is_expression = false;
   std::string text;
};

struct XPathConstructorAttribute {
   std::string prefix;
   std::string name;
   std::string namespace_uri;
   bool is_namespace_declaration = false;
   std::vector<XPathAttributeValuePart> value_parts;
   std::vector<std::unique_ptr<XPathNode>> expression_parts;

   void set_expression_for_part(size_t index, std::unique_ptr<XPathNode> expr) {
      if (expression_parts.size() <= index) expression_parts.resize(index + 1);
      expression_parts[index] = std::move(expr);
   }

   [[nodiscard]] XPathNode * get_expression_for_part(size_t index) const {
      return index < expression_parts.size() ? expression_parts[index].get() : nullptr;
   }
};

struct XPathConstructorInfo {
   std::string prefix, name, namespace_uri;
   bool is_empty_element = false;
   bool is_direct = false;
   std::vector<XPathConstructorAttribute> attributes;
};

struct XPathOrderSpecOptions {
   bool is_descending = false;
   bool has_empty_mode = false;
   bool empty_is_greatest = false;
   std::string collation_uri;
   [[nodiscard]] bool has_collation() const { return !collation_uri.empty(); }
};

struct XPathGroupKeyInfo {
   std::string variable_name;
   [[nodiscard]] bool has_variable() const { return !variable_name.empty(); }
};

struct XPathTypeswitchCaseInfo {
   std::string variable_name;
   std::string sequence_type;
   bool is_default = false;

   [[nodiscard]] bool has_variable() const { return !variable_name.empty(); }
   [[nodiscard]] bool has_sequence_type() const { return !sequence_type.empty(); }
   [[nodiscard]] bool is_default_case() const { return is_default; }
};

struct XPathNode {
   XQueryNodeType type;
   std::string value;
   std::vector<std::unique_ptr<XPathNode>> children;
   std::optional<XPathConstructorInfo> constructor_info;
   std::vector<XPathAttributeValuePart> attribute_value_parts;
   bool attribute_value_has_expressions = false;
   std::unique_ptr<XPathNode> name_expression;
   bool order_clause_is_stable = false;
   std::optional<XPathOrderSpecOptions> order_spec_options;
   std::optional<XPathGroupKeyInfo> group_key_info;
   std::optional<XPathTypeswitchCaseInfo> typeswitch_case_info;

   XPathNode(XQueryNodeType t, std::string v = "") : type(t), value(std::move(v)) {}

   inline void add_child(std::unique_ptr<XPathNode> child) { children.push_back(std::move(child)); }
   [[nodiscard]] inline XPathNode * get_child(size_t index) const { return index < children.size() ? children[index].get() : nullptr; }
   [[nodiscard]] inline size_t child_count() const { return children.size(); }
   inline void set_constructor_info(XPathConstructorInfo info) { constructor_info = std::move(info); }
   [[nodiscard]] inline bool has_constructor_info() const { return constructor_info.has_value(); }
   inline void set_name_expression(std::unique_ptr<XPathNode> expr) { name_expression = std::move(expr); }
   [[nodiscard]] inline XPathNode * get_name_expression() const { return name_expression.get(); }
   [[nodiscard]] inline bool has_name_expression() const { return name_expression != nullptr; }
   inline void set_group_key_info(XPathGroupKeyInfo Info) { group_key_info = std::move(Info); }
   [[nodiscard]] inline bool has_group_key_info() const { return group_key_info.has_value(); }
   [[nodiscard]] inline const XPathGroupKeyInfo * get_group_key_info() const { return group_key_info ? &(*group_key_info) : nullptr; }

   inline void set_typeswitch_case_info(XPathTypeswitchCaseInfo Info) { typeswitch_case_info = std::move(Info); }

   [[nodiscard]] inline bool has_typeswitch_case_info() const { return typeswitch_case_info.has_value(); }

   [[nodiscard]] inline const XPathTypeswitchCaseInfo * get_typeswitch_case_info() const { return typeswitch_case_info ? &(*typeswitch_case_info) : nullptr; }

   void set_attribute_value_parts(std::vector<XPathAttributeValuePart> parts) {
      attribute_value_has_expressions = false;
      for (const auto &part : parts) {
         if (part.is_expression) {
            attribute_value_has_expressions = true;
            break;
         }
      }
      attribute_value_parts = std::move(parts);
   }


   inline void set_order_spec_options(XPathOrderSpecOptions Options) {
      order_spec_options = std::move(Options);
   }

   [[nodiscard]] inline bool has_order_spec_options() const {
      return order_spec_options.has_value();
   }

   [[nodiscard]] inline const XPathOrderSpecOptions * get_order_spec_options() const {
      return order_spec_options ? &(*order_spec_options) : nullptr;
   }
};

//********************************************************************************************************************
// XPath Tokenization Infrastructure

struct XPathToken {
   XPathTokenType type;
   std::string_view value;
   size_t position;
   size_t length;

   // For tokens that need string storage (e.g., processed strings with escapes)
   std::string stored_value;
   bool is_attribute_value = false;
   std::vector<XPathAttributeValuePart> attribute_value_parts;

   // Constructor for string_view tokens (no copying)
   XPathToken(XPathTokenType t, std::string_view v, size_t pos = 0, size_t len = 0)
      : type(t), value(v), position(pos), length(len) {}

   // Constructor for tokens requiring string storage
   XPathToken(XPathTokenType t, std::string v, size_t pos = 0, size_t len = 0)
      : type(t), position(pos), length(len), stored_value(std::move(v)) {
      value = stored_value;
   }

   [[nodiscard]] bool has_attribute_template() const {
      return is_attribute_value and !attribute_value_parts.empty();
   }
};

extern std::string_view keyword_from_token_type(XPathTokenType Type);

//********************************************************************************************************************

class XPathTokeniser {
   private:
   std::string_view input;
   size_t position;
   size_t length;
   XPathTokenType previous_token_type;
   XPathTokenType prior_token_type;

   [[nodiscard]] bool is_alpha(char c) const;
   [[nodiscard]] bool is_digit(char c) const;
   [[nodiscard]] bool is_alnum(char c) const;
   [[nodiscard]] bool is_whitespace(char c) const;
   [[nodiscard]] bool is_name_start_char(char c) const;
   [[nodiscard]] bool is_name_char(char c) const;

   XPathToken scan_identifier();
   XPathToken scan_number();
   XPathToken scan_string(char QuoteChar);
   XPathToken scan_attribute_value(char QuoteChar, bool ProcessTemplates);
   XPathToken scan_operator();

   [[nodiscard]] char peek(size_t offset = 0) const;
   void skip_whitespace();
   [[nodiscard]] bool match(std::string_view Str);

   public:
   XPathTokeniser() : position(0), length(0), previous_token_type(XPathTokenType::UNKNOWN), prior_token_type(XPathTokenType::UNKNOWN) {}

   std::vector<XPathToken> tokenize(std::string_view XPath);
   bool has_more() const;
   [[nodiscard]] char current() const;
   void advance();
};

//********************************************************************************************************************

struct XQueryProlog;
struct XQueryModuleCache;

struct CompiledXQuery {
   std::unique_ptr<XPathNode> expression;
   std::shared_ptr<XQueryProlog> prolog;
   std::shared_ptr<XQueryModuleCache> module_cache;

   // Cache for loaded XML documents, e.g. via the doc() function in XQuery.
   ankerl::unordered_dense::map<URI_STR, extXML *> XMLCache;

   CompiledXQuery() = default;
   CompiledXQuery(CompiledXQuery &&) = default;
   CompiledXQuery &operator=(CompiledXQuery &&) = default;
   CompiledXQuery(const CompiledXQuery &) = delete;
   CompiledXQuery &operator=(const CompiledXQuery &) = delete;

   ~CompiledXQuery() {
      for (auto &entry : XMLCache) {
         if (entry.second) FreeResource(entry.second);
      }
   }
};

//********************************************************************************************************************
// Utilised to cache imported XQuery modules (compiled query result).

class extXQuery;
class XPathEvaluator;

struct XQueryModuleCache {
   // Referenced as a UID from xp::Compile() because it's a weak reference.
   // Used by fetch_or_load() primarily to determine the origin path of the XML data.
   extXQuery *query = nullptr;
   mutable ankerl::unordered_dense::map<std::string, std::shared_ptr<CompiledXQuery>> modules;
   mutable std::unordered_set<std::string> loading_in_progress;
   std::string base_path;

   CompiledXQuery * fetch_or_load(std::string_view, const struct XQueryProlog &, XPathEvaluator &) const;
   const CompiledXQuery * find_module(std::string_view uri) const;
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
   std::unique_ptr<XPathNode> parse_instance_of_expr();
   std::unique_ptr<XPathNode> parse_range_expr();
   std::unique_ptr<XPathNode> parse_additive_expr();
   std::unique_ptr<XPathNode> parse_multiplicative_expr();
   std::unique_ptr<XPathNode> parse_cast_expr();
   std::unique_ptr<XPathNode> parse_unary_expr();
   std::unique_ptr<XPathNode> parse_intersect_expr();
   std::unique_ptr<XPathNode> parse_union_expr();
   std::unique_ptr<XPathNode> parse_path_expr();
   std::unique_ptr<XPathNode> parse_filter_expr();
   std::unique_ptr<XPathNode> parse_if_expr();
   std::unique_ptr<XPathNode> parse_typeswitch_expr();
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
   bool parse_module_decl(XQueryProlog &prolog);
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
   std::optional<std::string> parse_qname_string();
   std::optional<std::string> parse_ncname();
   std::optional<std::string> parse_string_literal_value();

   inline std::optional<std::string> parse_uri_literal() {
      return parse_string_literal_value();
   }

   // Consumes any declaration separators (semicolons) and returns true if any were found.

   inline bool consume_declaration_separator() {
      bool consumed = false;
      while (match(XPathTokenType::SEMICOLON)) consumed = true;
      return consumed;
   }

   std::optional<std::string> collect_sequence_type(bool StopAtReturnKeyword = false);

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

   [[nodiscard]] inline bool is_keyword_acceptable_as_identifier(XPathTokenType Type) const {
      return not keyword_from_token_type(Type).empty();
   }

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

   CompiledXQuery parse(const std::vector<XPathToken> &TokenList);

   // Error handling

   inline void report_error(std::string_view Message) { errors.emplace_back(Message); }
   [[nodiscard]] inline bool has_errors() const { return !errors.empty(); }
   inline const std::vector<std::string> & get_errors() const { return errors; }

   private:
   std::vector<std::string> errors;
   XQueryProlog *active_prolog = nullptr;
};

//*********************************************************************************************************************

class extXQuery : public objXQuery {
public:
   FUNCTION Callback;
   std::string Statement;
   std::string ErrorMsg;
   CompiledXQuery ParseResult; // Result of parsing the query.
   XPathVal Result; // Result of the last execution.
   std::string ResultString; // Cached string representation of the result.
   std::string Path; // Base path for resolving relative URIs.
   extXML *XML; // During query execution, the context XML document.
   bool StaleBuild = true; // If true, the compiled query needs to be rebuilt.

   ~extXQuery() {
   }
};

//********************************************************************************************************************
// If an XQuery expression contains a prolog, it will be parsed into this structure and maintained in the XPathNode
// prolog field.

struct XQueryProlog {
   XQueryProlog();

   struct CopyNamespaces {
      bool preserve = true;
      bool inherit = true;
   } copy_namespaces;

   struct ExportValidationResult {
      bool valid = true;
      std::string error_message;
      std::string problematic_qname;
      bool is_function = false;  // true if problematic item is a function, false if variable
   };

   enum class BoundarySpace { Preserve, Strip } boundary_space = BoundarySpace::Strip;
   enum class ConstructionMode { Preserve, Strip } construction_mode = ConstructionMode::Strip;
   enum class OrderingMode { Ordered, Unordered } ordering_mode = OrderingMode::Ordered;
   enum class EmptyOrder { Greatest, Least } empty_order = EmptyOrder::Greatest;

   ankerl::unordered_dense::map<std::string, uint32_t> declared_namespaces;
   ankerl::unordered_dense::map<std::string, std::string> declared_namespace_uris;
   ankerl::unordered_dense::map<std::string, XQueryVariable> variables;
   ankerl::unordered_dense::map<std::string, XQueryFunction> functions;
   ankerl::unordered_dense::map<std::string, DecimalFormat> decimal_formats;
   ankerl::unordered_dense::map<std::string, std::string> options;

   std::vector<XQueryModuleImport> module_imports;

   std::optional<uint32_t> default_element_namespace;
   std::optional<uint32_t> default_function_namespace;
   std::optional<std::string> default_element_namespace_uri;
   std::optional<std::string> default_function_namespace_uri;
   std::optional<std::string> module_namespace_uri;
   std::optional<std::string> module_namespace_prefix;

   std::string static_base_uri;
   std::string default_collation;

   bool is_library_module = false;
   bool static_base_uri_declared = false;
   bool default_collation_declared = false;
   bool boundary_space_declared = false;
   bool construction_declared = false;
   bool ordering_declared = false;
   bool empty_order_declared = false;
   bool copy_namespaces_declared = false;
   bool default_decimal_format_declared = false;

   bool declare_namespace(std::string_view prefix, std::string_view uri, extXML *document);
   bool declare_variable(std::string_view qname, XQueryVariable variable);
   bool declare_function(XQueryFunction function);
   bool declare_module_import(XQueryModuleImport import_decl, std::string *error_message = nullptr);
   [[nodiscard]] bool validate_library_exports() const;
   [[nodiscard]] ExportValidationResult validate_library_exports_detailed() const;
   [[nodiscard]] const XQueryFunction * find_function(std::string_view qname, size_t arity) const;
   [[nodiscard]] const XQueryVariable * find_variable(std::string_view qname) const;
   [[nodiscard]] uint32_t resolve_prefix(std::string_view prefix, const extXML *document) const;
   [[nodiscard]] std::string normalise_function_qname(std::string_view qname, const XPathNode *document = nullptr) const;
   void bind_module_cache(std::shared_ptr<XQueryModuleCache> cache);
   [[nodiscard]] std::shared_ptr<XQueryModuleCache> get_module_cache() const;

   private:
   std::weak_ptr<XQueryModuleCache> module_cache;
};

struct TransparentStringHash {
   using is_transparent = void;

   size_t operator()(std::string_view Value) const noexcept { return std::hash<std::string_view>{}(Value); }
   size_t operator()(const std::string &Value) const noexcept { return operator()(std::string_view(Value)); }
   size_t operator()(const char *Value) const noexcept { return operator()(std::string_view(Value)); }
};

struct TransparentStringEqual {
   using is_transparent = void;

   bool operator()(std::string_view Lhs, std::string_view Rhs) const noexcept { return Lhs IS Rhs; }
   bool operator()(const std::string &Lhs, const std::string &Rhs) const noexcept { return std::string_view(Lhs) IS std::string_view(Rhs); }
   bool operator()(const char *Lhs, const char *Rhs) const noexcept { return std::string_view(Lhs) IS std::string_view(Rhs); }
   bool operator()(const std::string &Lhs, std::string_view Rhs) const noexcept { return std::string_view(Lhs) IS Rhs; }
   bool operator()(std::string_view Lhs, const std::string &Rhs) const noexcept { return Lhs IS std::string_view(Rhs); }
   bool operator()(const char *Lhs, std::string_view Rhs) const noexcept { return std::string_view(Lhs) IS Rhs; }
   bool operator()(std::string_view Lhs, const char *Rhs) const noexcept { return Lhs IS std::string_view(Rhs); }
};

namespace xml::schema {
   class SchemaTypeRegistry;
   SchemaTypeRegistry & registry();
}

class XPathArena {
   private:
   template<typename T>
   struct VectorPool {
      // Supplies cleared std::vector instances for XPath evaluation and reuses
      // them to avoid repeated heap allocations during query processing.
      std::vector<std::unique_ptr<std::vector<T>>> storage;
      std::vector<std::vector<T> *> free_list;

      // Fetches an available vector from the free list, or allocates a new
      // one, ensuring the container is reset before handing it to the caller.
      std::vector<T> & acquire() {
         if (!free_list.empty()) {
            auto *vector = free_list.back();
            free_list.pop_back();
            vector->clear();
            return *vector;
         }

         storage.push_back(std::make_unique<std::vector<T>>());
         auto &vector = *storage.back();
         vector.clear();
         return vector;
      }

      void release(std::vector<T> &vector) {
         vector.clear();
         free_list.push_back(&vector);
      }

      // Clears the pool state while retaining allocated storage, preparing the
      // vectors for reuse without incurring new allocations.
      void reset() {
         free_list.clear();
         for (auto &entry : storage) {
            entry->clear();
            free_list.push_back(entry.get());
         }
      }
   };

   // Specialized pool for XMLTag * that uses pf::vector
   struct NodeVectorPool {
      std::vector<std::unique_ptr<NODES>> storage;
      std::vector<NODES *> free_list;

      NODES & acquire() {
         if (!free_list.empty()) {
            auto *vector = free_list.back();
            free_list.pop_back();
            vector->clear();
            return *vector;
         }

         storage.push_back(std::make_unique<NODES>());
         auto &vector = *storage.back();
         vector.clear();
         return vector;
      }

      void release(NODES &vector) {
         vector.clear();
         free_list.push_back(&vector);
      }

      void reset() {
         free_list.clear();
         for (auto &entry : storage) {
            entry->clear();
            free_list.push_back(entry.get());
         }
      }
   };

   NodeVectorPool node_vectors;
   VectorPool<const XMLAttrib *> attribute_vectors;
   VectorPool<std::string> string_vectors;

   public:
   XPathArena() = default;
   XPathArena(const XPathArena &) = delete;
   XPathArena & operator=(const XPathArena &) = delete;

   NODES & acquire_node_vector() { return node_vectors.acquire(); }
   void release_node_vector(NODES &Vector) { node_vectors.release(Vector); }

   std::vector<const XMLAttrib *> & acquire_attribute_vector() { return attribute_vectors.acquire(); }
   void release_attribute_vector(std::vector<const XMLAttrib *> &Vector) { attribute_vectors.release(Vector); }

   std::vector<std::string> & acquire_string_vector() { return string_vectors.acquire(); }
   void release_string_vector(std::vector<std::string> &Vector) { string_vectors.release(Vector); }

   void reset() {
      node_vectors.reset();
      attribute_vectors.reset();
      string_vectors.reset();
   }
};

//********************************************************************************************************************
// Axis Evaluation Engine

class AxisEvaluator {
   private:
   CompiledXQuery *state;
   extXML *xml;
   XPathArena & arena;
   std::vector<std::unique_ptr<XMLTag>> namespace_node_storage;
   bool id_cache_built = false;

   struct AncestorPathView {
      std::span<XMLTag *const> Path;
      NODES * Storage = nullptr;
      bool Cached = false;
   };

   std::unordered_map<XMLTag *, NODES *> ancestor_path_cache;
   std::vector<std::unique_ptr<NODES>> ancestor_path_storage;
   std::unordered_map<uint64_t, bool> document_order_cache;

   // Optimized namespace handling data structures
   struct NamespaceDeclaration {
      std::string prefix;
      std::string uri;

      bool operator<(const NamespaceDeclaration &other) const {
         return prefix < other.prefix;
      }
   };

   // Reusable storage for namespace processing to avoid allocations
   std::vector<NamespaceDeclaration> namespace_declarations;
   std::vector<int> visited_node_ids;

   // Namespace node pool for reuse to reduce allocations
   std::vector<std::unique_ptr<XMLTag>> namespace_node_pool;

   // Helper methods for specific axes
   void evaluate_child_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_descendant_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_parent_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_ancestor_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_following_sibling_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_preceding_sibling_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_following_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_preceding_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_namespace_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_self_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_descendant_or_self_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_ancestor_or_self_axis(XMLTag *ContextNode, NODES &Output);

   void collect_subtree_reverse(XMLTag *Node, NODES &Output);

   // Optimized namespace collection
   void collect_namespace_declarations(XMLTag *Node, std::vector<NamespaceDeclaration> &declarations);

   // Namespace node pooling helpers
   XMLTag * acquire_namespace_node();
   void recycle_namespace_nodes();

   // Document order utilities
   void sort_document_order(NODES &Nodes);
   AncestorPathView build_ancestor_path(XMLTag *Node);
   void release_ancestor_path(AncestorPathView &View);
   uint64_t make_document_order_key(XMLTag *Left, XMLTag *Right);

   // Helper methods for tag lookup
   void build_id_cache();
   XMLTag * find_parent(XMLTag *ReferenceNode);

   public:
   explicit AxisEvaluator(CompiledXQuery *State, extXML *XML, XPathArena &Arena) 
      : state(State), xml(XML), arena(Arena) {}

   // Main evaluation method
   void evaluate_axis(AxisType Axis, XMLTag *ContextNode, NODES &Output);

   // Evaluation lifecycle helpers
   void reset_namespace_nodes();

   // Node-set utilities
   void normalise_node_set(NODES &Nodes);
   bool is_before_in_document_order(XMLTag *Node1, XMLTag *Node2);

   // Utility methods
   static AxisType parse_axis_name(std::string_view AxisName);
   static std::string_view axis_name_to_string(AxisType Axis);
   static bool is_reverse_axis(AxisType Axis);

   // Capacity estimation helper
   size_t estimate_result_size(AxisType Axis, XMLTag *ContextNode);
};

struct XMLAttrib;
class CompiledXQuery;
class XPathContext;
class XPathEvaluator;

//********************************************************************************************************************
// XPath Evaluation Context.  Stored in XPathEvaluator and initialised in its constructor.
// The context is pushed and popped as a stack frame during the evaluation process.

struct XPathContext {
   mutable XPathEvaluator *eval = nullptr;
   XMLTag *context_node = nullptr;
   const XMLAttrib * attribute_node = nullptr;
   size_t position = 1;
   size_t size = 1;
   ankerl::unordered_dense::map<std::string, XPathVal> * variables = nullptr;
   extXML *xml = nullptr;
   bool *expression_unsupported = nullptr;
   xml::schema::SchemaTypeRegistry * schema_registry = nullptr;
   std::shared_ptr<XQueryProlog> prolog;
   std::shared_ptr<XQueryModuleCache> module_cache;
      
   [[nodiscard]] inline std::shared_ptr<XQueryModuleCache> modules();

   XPathContext() = default;
};

class XPathEvaluator : public XPathErrorReporter {
   public:

   enum class PredicateResult {
      MATCH,
      NO_MATCH,
      UNSUPPORTED
   };

   extXML * xml;
   const XPathNode * query_root = nullptr;
   CompiledXQuery * parse_context = nullptr;
   XPathContext context;
   XPathArena arena;
   AxisEvaluator axis_evaluator;
   bool expression_unsupported = false;
   bool trace_xpath_enabled = false;
   bool construction_preserve_mode = false;

   // Variable storage owned by the evaluator
   ankerl::unordered_dense::map<std::string, XPathVal> variable_storage;
   ankerl::unordered_dense::map<std::string, XPathVal> prolog_variable_cache;
   std::unordered_set<std::string> variables_in_evaluation;

   // Tracks in-scope namespace declarations while building constructed nodes so nested
   // constructors inherit and override prefixes correctly.
   struct ConstructorNamespaceScope {
      const ConstructorNamespaceScope * parent = nullptr;
      std::unordered_map<std::string, uint32_t> prefix_bindings;
      std::optional<uint32_t> default_namespace;
   };

   std::vector<std::unique_ptr<XMLTag>> constructed_nodes;
   int next_constructed_node_id = -1;

   struct AxisMatch {
      XMLTag * node = nullptr;
      const XMLAttrib * attribute = nullptr;
   };

   using PredicateHandler = PredicateResult (XPathEvaluator::*)(const XPathNode *, uint32_t);

   struct CursorState {
      pf::vector<XMLTag> * tags;
      size_t index;
   };

   std::vector<CursorState> cursor_stack;
   std::vector<XPathContext> context_stack;
   
   // Cache for any form of unparsed text resource, e.g. loaded via the unparsed-text() function in XQuery.

   ankerl::unordered_dense::map<std::string, std::string> text_cache;

   std::vector<AxisMatch> dispatch_axis(AxisType Axis, XMLTag *ContextNode, const XMLAttrib *ContextAttribute = nullptr);
   extXML * resolve_document_for_node(XMLTag *Node) const;
   bool is_foreign_document_node(XMLTag *Node) const;
   NODES collect_step_results(const std::vector<AxisMatch> &,
      const std::vector<const XPathNode *> &, size_t, uint32_t, bool &);
   XPathVal evaluate_path_expression_value(const XPathNode *PathNode, uint32_t CurrentPrefix);
   XPathVal evaluate_path_from_nodes(const NODES &,
      const std::vector<const XMLAttrib *> &, const std::vector<const XPathNode *> &, const XPathNode *,
      const XPathNode *, uint32_t);
   ERR evaluate_top_level_expression(const XPathNode *Node, uint32_t CurrentPrefix);
   ERR process_expression_node_set(const XPathVal &Value);
   XPathVal evaluate_union_value(const std::vector<const XPathNode *> &Branches, uint32_t CurrentPrefix);
   XPathVal evaluate_intersect_value(const XPathNode *Left, const XPathNode *Right, uint32_t CurrentPrefix);
   XPathVal evaluate_except_value(const XPathNode *Left, const XPathNode *Right, uint32_t CurrentPrefix);
   ERR evaluate_union(const XPathNode *Node, uint32_t CurrentPrefix);
   XPathVal evaluate_flwor_pipeline(const XPathNode *Node, uint32_t CurrentPrefix);
   void initialise_query_context(const XPathNode *Root);

   std::optional<uint32_t> resolve_constructor_prefix(const ConstructorNamespaceScope &Scope,
      std::string_view Prefix) const;
   uint32_t register_constructor_namespace(const std::string &URI) const;
   XMLTag clone_node_subtree(const XMLTag &Source, int ParentID);
   bool append_constructor_sequence(XMLTag &Parent, const XPathVal &Value,
      uint32_t CurrentPrefix, const ConstructorNamespaceScope &Scope, bool PreserveConstruction);
   std::optional<std::string> evaluate_attribute_value_template(const XPathConstructorAttribute &Attribute,
      uint32_t CurrentPrefix);
   std::optional<std::string> evaluate_constructor_content_string(const XPathNode *Node, uint32_t CurrentPrefix,
      bool ApplyWhitespaceRules, bool PreserveConstruction);
   std::optional<std::string> evaluate_constructor_name_string(const XPathNode *Node, uint32_t CurrentPrefix);
   std::optional<std::string> prepare_constructor_text(std::string_view Text, bool IsLiteral) const;
   bool prolog_has_boundary_space_preserve() const;
   bool prolog_construction_preserve() const;
   bool prolog_ordering_is_ordered() const;
   bool prolog_empty_is_greatest() const;
   std::optional<XMLTag> build_direct_element_node(const XPathNode *Node, uint32_t CurrentPrefix,
      ConstructorNamespaceScope *ParentScope, int ParentID);
   XPathVal evaluate_direct_element_constructor(const XPathNode *Node, uint32_t CurrentPrefix);
   XPathVal evaluate_computed_element_constructor(const XPathNode *Node, uint32_t CurrentPrefix);
   XPathVal evaluate_computed_attribute_constructor(const XPathNode *Node, uint32_t CurrentPrefix);
   XPathVal evaluate_text_constructor(const XPathNode *Node, uint32_t CurrentPrefix);
   XPathVal evaluate_comment_constructor(const XPathNode *Node, uint32_t CurrentPrefix);
   XPathVal evaluate_pi_constructor(const XPathNode *Node, uint32_t CurrentPrefix);
   XPathVal evaluate_document_constructor(const XPathNode *Node, uint32_t CurrentPrefix);

   void expand_axis_candidates(const AxisMatch &ContextEntry, AxisType Axis,
      const XPathNode *NodeTest, uint32_t CurrentPrefix, std::vector<AxisMatch> &FilteredMatches);
   ERR apply_predicates_to_candidates(const std::vector<const XPathNode *> &PredicateNodes,
      uint32_t CurrentPrefix, std::vector<AxisMatch> &Candidates, std::vector<AxisMatch> &ScratchBuffer);
   ERR process_step_matches(const std::vector<AxisMatch> &Matches, AxisType Axis, bool IsLastStep,
      bool &Matched, std::vector<AxisMatch> &NextContext, bool &ShouldTerminate);
   ERR invoke_callback(XMLTag *Node, const XMLAttrib *Attribute, bool &Matched, bool &ShouldTerminate);

   PredicateResult dispatch_predicate_operation(std::string_view OperationName, const XPathNode *Expression,
      uint32_t CurrentPrefix);
   const std::unordered_map<std::string_view, PredicateHandler> &predicate_handler_map() const;
   PredicateResult handle_attribute_exists_predicate(const XPathNode *Expression, uint32_t CurrentPrefix);
   PredicateResult handle_attribute_equals_predicate(const XPathNode *Expression, uint32_t CurrentPrefix);
   PredicateResult handle_content_equals_predicate(const XPathNode *Expression, uint32_t CurrentPrefix);

   std::string build_ast_signature(const XPathNode *Node) const;

   void record_error(std::string_view Message, bool Force = false) override;
   void record_error(std::string_view Message, const XPathNode *Node, bool Force = false) override;
   std::optional<XPathVal> resolve_user_defined_function(std::string_view FunctionName,
      const std::vector<XPathVal> &Args, uint32_t CurrentPrefix, const XPathNode *FuncNode);
   XPathVal evaluate_user_defined_function(const XQueryFunction &Function,
      const std::vector<XPathVal> &Args, uint32_t CurrentPrefix, const XPathNode *FuncNode);
   bool resolve_variable_value(std::string_view QName, uint32_t CurrentPrefix,
      XPathVal &OutValue, const XPathNode *ReferenceNode);

   public:
   XPathEvaluator(extXML *, const XPathNode *, CompiledXQuery *);

   ERR evaluate_ast(const XPathNode *Node, uint32_t CurrentPrefix);
   ERR evaluate_location_path(const XPathNode *PathNode, uint32_t CurrentPrefix);
   ERR evaluate_step_ast(const XPathNode *StepNode, uint32_t CurrentPrefix);
   ERR evaluate_step_sequence(const NODES &ContextNodes, const std::vector<const XPathNode *> &Steps, size_t StepIndex, uint32_t CurrentPrefix, bool &Matched);
   bool match_node_test(const XPathNode *NodeTest, AxisType Axis, XMLTag *Candidate, const XMLAttrib *Attribute, uint32_t CurrentPrefix);
   PredicateResult evaluate_predicate(const XPathNode *PredicateNode, uint32_t CurrentPrefix);

   XPathVal evaluate_expression(const XPathNode *ExprNode, uint32_t CurrentPrefix);
   XPathVal evaluate_function_call(const XPathNode *FuncNode, uint32_t CurrentPrefix);

   // Entry point for compiled XPath evaluation
   ERR find_tag(const XPathNode &, uint32_t);

   inline bool is_trace_enabled() const { return trace_xpath_enabled; }

   // Full XPath expression evaluation returning computed values.  Will update the provided XPathValue
   ERR evaluate_xpath_expression(const XPathNode &, XPathVal *, uint32_t CurrentPrefix = 0);

   // Context management for AST evaluation
   void push_context(XMLTag *Node, size_t Position = 1, size_t Size = 1, const XMLAttrib *Attribute = nullptr);
   void pop_context();

   [[nodiscard]] inline XMLTag * get_context_node() const;

   // Stack management for deep traversal
   void push_cursor_state();
   void pop_cursor_state();

   bool has_cursor_state() const { return !cursor_stack.empty(); }
};

//********************************************************************************************************************

struct SequenceEntry {
   XMLTag * node = nullptr;
   const XMLAttrib * attribute = nullptr;
   std::string string_value;
};

struct ForBindingDefinition {
   std::string name;
   const XPathNode * sequence = nullptr;
};

struct QuantifiedBindingDefinition {
   std::string name;
   const XPathNode * sequence = nullptr;
};

struct CastTargetInfo {
   std::string type_name;
   bool allows_empty = false;
};

struct SequenceTypeInfo {
   SequenceCardinality occurrence = SequenceCardinality::ExactlyOne;
   SequenceItemKind kind = SequenceItemKind::Atomic;
   std::string type_name;

   [[nodiscard]] inline bool allows_empty() const {
      return (occurrence IS SequenceCardinality::ZeroOrOne) or (occurrence IS SequenceCardinality::ZeroOrMore);
   }

   [[nodiscard]] inline bool allows_multiple() const {
      return (occurrence IS SequenceCardinality::OneOrMore) or (occurrence IS SequenceCardinality::ZeroOrMore);
   }
};

//********************************************************************************************************************

class VariableBindingGuard
{
   private:
   XPathContext & context;
   std::string variable_name;
   std::optional<XPathVal> previous_value;
   bool had_previous_value = false;

   public:
   VariableBindingGuard(XPathContext &Context, std::string Name, XPathVal Value)
      : context(Context), variable_name(std::move(Name))
   {
      auto existing = context.variables->find(variable_name);
      had_previous_value = (existing != context.variables->end());
      if (had_previous_value) previous_value = existing->second;

      (*context.variables)[variable_name] = std::move(Value);
   }

   ~VariableBindingGuard() {
      if (had_previous_value) (*context.variables)[variable_name] = *previous_value;
      else context.variables->erase(variable_name);
   }

   VariableBindingGuard(const VariableBindingGuard &) = delete;
   VariableBindingGuard & operator=(const VariableBindingGuard &) = delete;

   VariableBindingGuard(VariableBindingGuard &&) = default;
   VariableBindingGuard & operator=(VariableBindingGuard &&) = default;
};

[[nodiscard]] inline XMLTag * XPathEvaluator::get_context_node() const { return context.context_node; };
[[nodiscard]] inline std::shared_ptr<XQueryModuleCache> XPathContext::modules() { return eval->parse_context->module_cache; };

extern "C" ERR load_regex(void);
