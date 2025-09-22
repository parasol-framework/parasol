#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>

//********************************************************************************************************************
// XPath Query
//
// [0-9]  Used for indexing
// '*'    For wild-carding of tag names
// '@'    An attribute
// '..'   Parent
// [=...] Match on encapsulated content (Not an XPath standard but we support it)
// //     Double-slash enables deep scanning of the XML tree.
//
// Round brackets may also be used as an alternative to square brackets.
//
// The use of \ as an escape character in attribute strings is supported, but keep in mind that this is not an official
// feature of the XPath standard.
//
// Examples:
//   /menu/submenu
//   /menu[2]/window
//   /menu/window/@title
//   /menu/window[@title='foo']/...
//   /menu[=contentmatch]
//   /menu//window
//   /menu/window/* (First child of the window tag)
//   /menu/*[@id='5']

//********************************************************************************************************************
// Phase 2: XPath Tokenization and AST Infrastructure

enum class XPathTokenType {
   // Path operators
   SLASH,              // /
   DOUBLE_SLASH,       // //
   DOT,               // .
   DOUBLE_DOT,        // ..

   // Identifiers and literals
   IDENTIFIER,         // element names, function names
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
   PIPE,              // |

   // Operators
   EQUALS,            // =
   NOT_EQUALS,        // !=
   LESS_THAN,         // <
   LESS_EQUAL,        // <=
   GREATER_THAN,      // >
   GREATER_EQUAL,     // >=

   // Boolean operators
   AND,               // and
   OR,                // or
   NOT,               // not

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

   // Special tokens
   END_OF_INPUT,
   UNKNOWN
};

struct XPathToken {
   XPathTokenType type;
   std::string value;
   size_t position;
   size_t length;

   XPathToken(XPathTokenType t, std::string v, size_t pos = 0, size_t len = 0)
      : type(t), value(std::move(v)), position(pos), length(len) {}
};

//********************************************************************************************************************
// XPath AST Node Types

enum class XPathNodeType {
   // Path components
   LocationPath,       // Absolute or relative path
   Step,              // Single step in path
   Predicate,         // Filter expression in brackets

   // Expressions
   BinaryOp,          // Binary operations (and, or, =, !=, etc.)
   UnaryOp,           // Unary operations (not, -)
   Function,          // Function calls
   Variable,          // Variable references

   // Literals
   Literal,           // String literals
   Number,            // Numeric literals

   // Node tests
   NameTest,          // Element name test
   NodeTypeTest,      // node(), text(), comment(), etc.

   // Axis specifiers
   AxisSpecifier,     // child::, descendant::, etc.

   // Special
   Wildcard,          // *
   Root              // Root node indicator
};

struct XPathNode {
   XPathNodeType type;
   std::string value;
   std::vector<std::unique_ptr<XPathNode>> children;

   // Constructor
   XPathNode(XPathNodeType t, std::string v = "")
      : type(t), value(std::move(v)) {}

   // Helper methods
   void add_child(std::unique_ptr<XPathNode> child) {
      children.push_back(std::move(child));
   }

   XPathNode* get_child(size_t index) const {
      return index < children.size() ? children[index].get() : nullptr;
   }

   size_t child_count() const {
      return children.size();
   }
};

//********************************************************************************************************************
// Phase 3: XPath 1.0 Value System and Function Library

enum class XPathValueType {
   NodeSet,
   Boolean,
   Number,
   String
};

class XPathValue {
   public:
   XPathValueType type;
   std::vector<XMLTag*> node_set;
   bool boolean_value = false;
   double number_value = 0.0;
   std::string string_value;

   // Constructors
   XPathValue() : type(XPathValueType::Boolean) {}
   explicit XPathValue(bool value) : type(XPathValueType::Boolean), boolean_value(value) {}
   explicit XPathValue(double value) : type(XPathValueType::Number), number_value(value) {}
   explicit XPathValue(std::string value) : type(XPathValueType::String), string_value(std::move(value)) {}
   explicit XPathValue(const std::vector<XMLTag*> &nodes) : type(XPathValueType::NodeSet), node_set(nodes) {}

   // Type conversions
   bool to_boolean() const {
      switch (type) {
         case XPathValueType::Boolean: return boolean_value;
         case XPathValueType::Number: return number_value != 0.0 and !std::isnan(number_value);
         case XPathValueType::String: return !string_value.empty();
         case XPathValueType::NodeSet: return !node_set.empty();
      }
      return false;
   }

   double to_number() const {
      switch (type) {
         case XPathValueType::Boolean: return boolean_value ? 1.0 : 0.0;
         case XPathValueType::Number: return number_value;
         case XPathValueType::String: {
            if (string_value.empty()) return std::numeric_limits<double>::quiet_NaN();
            char* end_ptr = nullptr;
            double result = std::strtod(string_value.c_str(), &end_ptr);
            if (end_ptr IS string_value.c_str() or *end_ptr != '\0') {
               return std::numeric_limits<double>::quiet_NaN();
            }
            return result;
         }
         case XPathValueType::NodeSet: {
            if (node_set.empty()) return std::numeric_limits<double>::quiet_NaN();
            std::string str = string_value_of_first_node();
            if (str.empty()) return std::numeric_limits<double>::quiet_NaN();
            char* end_ptr = nullptr;
            double result = std::strtod(str.c_str(), &end_ptr);
            if (end_ptr IS str.c_str() or *end_ptr != '\0') {
               return std::numeric_limits<double>::quiet_NaN();
            }
            return result;
         }
      }
      return 0.0;
   }

   std::string to_string() const {
      switch (type) {
         case XPathValueType::Boolean: return boolean_value ? "true" : "false";
         case XPathValueType::Number: {
            if (std::isnan(number_value)) return "NaN";
            if (std::isinf(number_value)) return number_value > 0 ? "Infinity" : "-Infinity";
            if (number_value IS std::floor(number_value)) {
               return std::to_string((long long)number_value);
            }
            return std::to_string(number_value);
         }
         case XPathValueType::String: return string_value;
         case XPathValueType::NodeSet: return string_value_of_first_node();
      }
      return "";
   }

   private:
   std::string string_value_of_first_node() const {
      if (node_set.empty()) return "";
      XMLTag *tag = node_set[0];
      if (tag) {
         return tag->getContent();
      }
      return "";
   }
};

struct XPathContext {
   XMLTag *context_node = nullptr;
   size_t position = 1;
   size_t size = 1;
   std::map<std::string, XPathValue> variables;

   void bind_variable(const std::string& name, XPathValue value) {
      variables[name] = std::move(value);
   }

   XPathValue get_variable(const std::string& name) const {
      auto it = variables.find(name);
      if (it != variables.end()) {
         return it->second;
      }
      return XPathValue(); // Return empty boolean
   }
};

class XPathFunctionLibrary {
   public:
   XPathValue evaluate_function(const std::string& name,
                              const std::vector<XPathValue>& args,
                              XPathContext& context) {
      // Node Set Functions
      if (name IS "position") return func_position(context);
      if (name IS "last") return func_last(context);
      if (name IS "count") return func_count(args);

      // String Functions
      if (name IS "string") return func_string(args, context);
      if (name IS "concat") return func_concat(args);
      if (name IS "starts-with") return func_starts_with(args);
      if (name IS "contains") return func_contains(args);
      if (name IS "substring") return func_substring(args);
      if (name IS "string-length") return func_string_length(args, context);
      if (name IS "normalize-space") return func_normalize_space(args, context);

      // Boolean Functions
      if (name IS "boolean") return func_boolean(args);
      if (name IS "not") return func_not(args);
      if (name IS "true") return func_true();
      if (name IS "false") return func_false();

      // Number Functions
      if (name IS "number") return func_number(args, context);
      if (name IS "sum") return func_sum(args);
      if (name IS "floor") return func_floor(args);
      if (name IS "ceiling") return func_ceiling(args);
      if (name IS "round") return func_round(args);

      // Unknown function
      return XPathValue(); // Return empty boolean
   }

   private:
   // Node Set Functions
   XPathValue func_position(XPathContext& context) {
      return XPathValue((double)context.position);
   }

   XPathValue func_last(XPathContext& context) {
      return XPathValue((double)context.size);
   }

   XPathValue func_count(const std::vector<XPathValue>& args) {
      if (args.size() != 1) return XPathValue(0.0);
      if (args[0].type != XPathValueType::NodeSet) return XPathValue(0.0);
      return XPathValue((double)args[0].node_set.size());
   }

   // String Functions
   XPathValue func_string(const std::vector<XPathValue>& args, XPathContext& context) {
      if (args.empty()) {
         // Convert context node to string
         if (context.context_node) {
            std::vector<XMLTag*> nodes = { context.context_node };
            XPathValue node_set_value(nodes);
            return XPathValue(node_set_value.to_string());
         }
         return XPathValue("");
      }
      return XPathValue(args[0].to_string());
   }

   XPathValue func_concat(const std::vector<XPathValue>& args) {
      std::string result;
      for (const auto& arg : args) {
         result += arg.to_string();
      }
      return XPathValue(result);
   }

   XPathValue func_starts_with(const std::vector<XPathValue>& args) {
      if (args.size() != 2) return XPathValue(false);
      std::string str = args[0].to_string();
      std::string prefix = args[1].to_string();
      return XPathValue(str.substr(0, prefix.length()) IS prefix);
   }

   XPathValue func_contains(const std::vector<XPathValue>& args) {
      if (args.size() != 2) return XPathValue(false);
      std::string str = args[0].to_string();
      std::string substr = args[1].to_string();
      return XPathValue(str.find(substr) != std::string::npos);
   }

   XPathValue func_substring(const std::vector<XPathValue>& args) {
      if (args.size() < 2 or args.size() > 3) return XPathValue("");

      std::string str = args[0].to_string();
      double start_pos = args[1].to_number();

      if (std::isnan(start_pos) or std::isinf(start_pos)) return XPathValue("");

      // XPath uses 1-based indexing
      int start_index = (int)std::round(start_pos) - 1;
      if (start_index < 0) start_index = 0;
      if (start_index >= (int)str.length()) return XPathValue("");

      if (args.size() == 3) {
         double length = args[2].to_number();
         if (std::isnan(length) or std::isinf(length) or length <= 0) return XPathValue("");
         int len = (int)std::round(length);
         return XPathValue(str.substr(start_index, len));
      }

      return XPathValue(str.substr(start_index));
   }

   XPathValue func_string_length(const std::vector<XPathValue>& args, XPathContext& context) {
      std::string str;
      if (args.empty()) {
         if (context.context_node) {
            std::vector<XMLTag*> nodes = { context.context_node };
            XPathValue node_set_value(nodes);
            str = node_set_value.to_string();
         }
      } else {
         str = args[0].to_string();
      }
      return XPathValue((double)str.length());
   }

   XPathValue func_normalize_space(const std::vector<XPathValue>& args, XPathContext& context) {
      std::string str;
      if (args.empty()) {
         if (context.context_node) {
            std::vector<XMLTag*> nodes = { context.context_node };
            XPathValue node_set_value(nodes);
            str = node_set_value.to_string();
         }
      } else {
         str = args[0].to_string();
      }

      // Remove leading and trailing whitespace, collapse internal whitespace
      size_t start = str.find_first_not_of(" \t\n\r");
      if (start == std::string::npos) return XPathValue("");

      size_t end = str.find_last_not_of(" \t\n\r");
      str = str.substr(start, end - start + 1);

      // Collapse internal whitespace
      std::string result;
      bool in_whitespace = false;
      for (char c : str) {
         if (c == ' ' or c == '\t' or c == '\n' or c == '\r') {
            if (!in_whitespace) {
               result += ' ';
               in_whitespace = true;
            }
         } else {
            result += c;
            in_whitespace = false;
         }
      }

      return XPathValue(result);
   }

   // Boolean Functions
   XPathValue func_boolean(const std::vector<XPathValue>& args) {
      if (args.size() != 1) return XPathValue(false);
      return XPathValue(args[0].to_boolean());
   }

   XPathValue func_not(const std::vector<XPathValue>& args) {
      if (args.size() != 1) return XPathValue(true);
      return XPathValue(!args[0].to_boolean());
   }

   XPathValue func_true() {
      return XPathValue(true);
   }

   XPathValue func_false() {
      return XPathValue(false);
   }

   // Number Functions
   XPathValue func_number(const std::vector<XPathValue>& args, XPathContext& context) {
      if (args.empty()) {
         if (context.context_node) {
            std::vector<XMLTag*> nodes = { context.context_node };
            XPathValue node_set_value(nodes);
            return XPathValue(node_set_value.to_number());
         }
         return XPathValue(std::numeric_limits<double>::quiet_NaN());
      }
      return XPathValue(args[0].to_number());
   }

   XPathValue func_sum(const std::vector<XPathValue>& args) {
      if (args.size() != 1) return XPathValue(0.0);
      if (args[0].type != XPathValueType::NodeSet) return XPathValue(0.0);

      double sum = 0.0;
      for (XMLTag* node : args[0].node_set) {
         if (node) {
            std::vector<XMLTag*> single_node = { node };
            XPathValue node_value(single_node);
            double value = node_value.to_number();
            if (!std::isnan(value)) {
               sum += value;
            }
         }
      }
      return XPathValue(sum);
   }

   XPathValue func_floor(const std::vector<XPathValue>& args) {
      if (args.size() != 1) return XPathValue(std::numeric_limits<double>::quiet_NaN());
      double value = args[0].to_number();
      if (std::isnan(value) or std::isinf(value)) return XPathValue(value);
      return XPathValue(std::floor(value));
   }

   XPathValue func_ceiling(const std::vector<XPathValue>& args) {
      if (args.size() != 1) return XPathValue(std::numeric_limits<double>::quiet_NaN());
      double value = args[0].to_number();
      if (std::isnan(value) or std::isinf(value)) return XPathValue(value);
      return XPathValue(std::ceil(value));
   }

   XPathValue func_round(const std::vector<XPathValue>& args) {
      if (args.size() != 1) return XPathValue(std::numeric_limits<double>::quiet_NaN());
      double value = args[0].to_number();
      if (std::isnan(value) or std::isinf(value)) return XPathValue(value);
      return XPathValue(std::round(value));
   }
};

//********************************************************************************************************************
// XPath Axis Support - Phase 5

enum class AxisType {
   Child,
   Descendant,
   DescendantOrSelf,
   Following,
   FollowingSibling,
   Parent,
   Ancestor,
   AncestorOrSelf,
   Preceding,
   PrecedingSibling,
   Self,
   Attribute
};

class AxisEvaluator {
   public:
   explicit AxisEvaluator(extXML* xml) : xml(xml) {}

   std::vector<XMLTag*> evaluate_axis(AxisType axis, XMLTag* context_node) {
      if (!context_node) return {};

      switch (axis) {
         case AxisType::Child:
            return get_children(context_node);
         case AxisType::Descendant:
            return get_descendants(context_node, false);
         case AxisType::DescendantOrSelf:
            return get_descendants(context_node, true);
         case AxisType::Following:
            return get_following(context_node);
         case AxisType::FollowingSibling:
            return get_following_siblings(context_node);
         case AxisType::Parent:
            return get_parent(context_node);
         case AxisType::Ancestor:
            return get_ancestors(context_node, false);
         case AxisType::AncestorOrSelf:
            return get_ancestors(context_node, true);
         case AxisType::Preceding:
            return get_preceding(context_node);
         case AxisType::PrecedingSibling:
            return get_preceding_siblings(context_node);
         case AxisType::Self:
            return get_self(context_node);
         case AxisType::Attribute:
            return get_attributes(context_node);
         default:
            return {};
      }
   }

   AxisType string_to_axis_type(const std::string& axis_name) {
      if (axis_name IS "child") return AxisType::Child;
      else if (axis_name IS "descendant") return AxisType::Descendant;
      else if (axis_name IS "descendant-or-self") return AxisType::DescendantOrSelf;
      else if (axis_name IS "following") return AxisType::Following;
      else if (axis_name IS "following-sibling") return AxisType::FollowingSibling;
      else if (axis_name IS "parent") return AxisType::Parent;
      else if (axis_name IS "ancestor") return AxisType::Ancestor;
      else if (axis_name IS "ancestor-or-self") return AxisType::AncestorOrSelf;
      else if (axis_name IS "preceding") return AxisType::Preceding;
      else if (axis_name IS "preceding-sibling") return AxisType::PrecedingSibling;
      else if (axis_name IS "self") return AxisType::Self;
      else if (axis_name IS "attribute") return AxisType::Attribute;
      else return AxisType::Child; // Default axis
   }

   private:
   extXML* xml;

   // Helper method to find a tag by ID in the current document
   XMLTag* find_tag_by_id(int id) {
      if (id IS 0) return nullptr;

      // Search through the entire document tree using the Tags vector
      for (auto& tag : xml->Tags) {
         auto found = find_tag_recursive(tag, id);
         if (found) return found;
      }
      return nullptr;
   }

   // Recursive helper to search through tag hierarchy
   XMLTag* find_tag_recursive(XMLTag& tag, int id) {
      if (tag.ID IS id) return &tag;

      for (auto& child : tag.Children) {
         auto found = find_tag_recursive(child, id);
         if (found) return found;
      }
      return nullptr;
   }

   std::vector<XMLTag*> get_children(XMLTag* node) {
      std::vector<XMLTag*> children;
      if (!node) return children;

      // Only include actual tag children, not content nodes
      for (auto& child : node->Children) {
         if (child.isTag()) { // Use helper method from XMLTag
            children.push_back(&child);
         }
      }
      return children;
   }

   std::vector<XMLTag*> get_descendants(XMLTag* node, bool include_self) {
      std::vector<XMLTag*> descendants;
      if (!node) return descendants;

      if (include_self and node->isTag()) {
         descendants.push_back(node);
      }

      // Only process actual tag children, not content nodes
      for (auto& child : node->Children) {
         if (child.isTag()) {
            descendants.push_back(&child);
            auto child_descendants = get_descendants(&child, false);
            descendants.insert(descendants.end(), child_descendants.begin(), child_descendants.end());
         }
      }
      return descendants;
   }

   std::vector<XMLTag*> get_ancestors(XMLTag* node, bool include_self) {
      std::vector<XMLTag*> ancestors;
      if (!node) return ancestors;

      if (include_self) {
         ancestors.push_back(node);
      }

      XMLTag* parent = find_tag_by_id(node->ParentID);
      while (parent) {
         ancestors.push_back(parent);
         parent = find_tag_by_id(parent->ParentID);
      }
      return ancestors;
   }

   std::vector<XMLTag*> get_parent(XMLTag* node) {
      std::vector<XMLTag*> parents;
      if (node and node->ParentID != 0) {
         XMLTag* parent = find_tag_by_id(node->ParentID);
         if (parent) {
            parents.push_back(parent);
         }
      }
      return parents;
   }

   std::vector<XMLTag*> get_following_siblings(XMLTag* node) {
      std::vector<XMLTag*> siblings;
      if (!node) return siblings;

      // Find parent and locate this node in parent's children
      XMLTag* parent = find_tag_by_id(node->ParentID);
      if (!parent) return siblings;

      bool found_self = false;
      for (auto& child : parent->Children) {
         if (found_self and child.isTag()) {
            siblings.push_back(&child);
         } else if (&child IS node) {
            found_self = true;
         }
      }
      return siblings;
   }

   std::vector<XMLTag*> get_preceding_siblings(XMLTag* node) {
      std::vector<XMLTag*> siblings;
      if (!node) return siblings;

      // Find parent and locate this node in parent's children
      XMLTag* parent = find_tag_by_id(node->ParentID);
      if (!parent) return siblings;

      for (auto& child : parent->Children) {
         if (&child IS node) {
            break; // Stop when we reach the current node
         }
         if (child.isTag()) {
            siblings.push_back(&child);
         }
      }
      return siblings;
   }

   std::vector<XMLTag*> get_following(XMLTag* node) {
      std::vector<XMLTag*> following;
      if (!node) return following;

      // Get following siblings and their descendants (document order)
      auto following_siblings = get_following_siblings(node);
      for (auto* sibling : following_siblings) {
         if (sibling->isTag()) {
            following.push_back(sibling);
            auto descendants = get_descendants(sibling, false);
            following.insert(following.end(), descendants.begin(), descendants.end());
         }
      }

      // Recursively check parent's following context for complete XPath semantics
      XMLTag* parent = find_tag_by_id(node->ParentID);
      if (parent) {
         auto parent_following = get_following(parent);
         following.insert(following.end(), parent_following.begin(), parent_following.end());
      }

      return following;
   }

   std::vector<XMLTag*> get_preceding(XMLTag* node) {
      std::vector<XMLTag*> preceding;
      if (!node) return preceding;

      // Get preceding siblings and their descendants (reverse document order)
      auto preceding_siblings = get_preceding_siblings(node);
      for (auto* sibling : preceding_siblings) {
         if (sibling->isTag()) {
            auto descendants = get_descendants(sibling, false);
            preceding.insert(preceding.end(), descendants.begin(), descendants.end());
            preceding.push_back(sibling); // Add sibling after its descendants
         }
      }

      // Recursively include parent's preceding context
      XMLTag* parent = find_tag_by_id(node->ParentID);
      if (parent) {
         auto parent_preceding = get_preceding(parent);
         preceding.insert(preceding.end(), parent_preceding.begin(), parent_preceding.end());
      }

      return preceding;
   }

   std::vector<XMLTag*> get_self(XMLTag* node) {
      std::vector<XMLTag*> self;
      if (node) {
         self.push_back(node);
      }
      return self;
   }

   std::vector<XMLTag*> get_attributes(XMLTag* node) {
      // In Parasol's XML implementation, attributes are not separate nodes
      // but are stored as properties of the tag. For XPath compatibility,
      // we return an empty set since attribute access is handled differently
      // via the @ syntax in predicates.
      return {};
   }
};

//********************************************************************************************************************
// XPath Tokenizer

class XPathTokenizer {
   private:
   std::string_view input;
   size_t position = 0;

   public:
   std::vector<XPathToken> tokenize(std::string_view xpath) {
      input = xpath;
      position = 0;
      std::vector<XPathToken> tokens;
      int bracket_depth = 0; // '[' and ']'
      int paren_depth = 0;   // '(' and ')'

      while (position < input.size()) {
         skip_whitespace();
         if (position >= input.size()) break;

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
            XPathToken token = next_token();
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

   private:
   void skip_whitespace() {
      while (position < input.size() and std::isspace(input[position])) {
         position++;
      }
   }

   XPathToken next_token() {
      size_t start = position;
      char ch = input[position];

      // Two-character operators
      if (position + 1 < input.size()) {
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

      // String literals
      if (ch IS '\'' or ch IS '"') {
         return parse_string_literal();
      }

      // Numbers
      if (std::isdigit(ch)) {
         return parse_number();
      }

      // Identifiers and keywords
      if (std::isalpha(ch) or ch IS '_') {
         return parse_identifier_or_keyword();
      }

      // Unknown character
      position++;
      return XPathToken(XPathTokenType::UNKNOWN, std::string(1, ch), start, 1);
   }

   XPathToken parse_string_literal() {
      size_t start = position;
      char quote = input[position];
      position++; // Skip opening quote

      std::string value;
      while (position < input.size() and input[position] != quote) {
         if (input[position] == '\\' and position + 1 < input.size()) {
            // Handle escape sequences
            position++;
            char escaped = input[position];
            if (escaped == quote or escaped == '\\' or escaped == '*') {
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

      if (position < input.size()) {
         position++; // Skip closing quote
      }

      return XPathToken(XPathTokenType::STRING, value, start, position - start);
   }

   XPathToken parse_number() {
      size_t start = position;
      std::string value;

      while (position < input.size() and (std::isdigit(input[position]) or input[position] IS '.')) {
         value += input[position];
         position++;
      }

      return XPathToken(XPathTokenType::NUMBER, value, start, position - start);
   }

   XPathToken parse_identifier_or_keyword() {
      size_t start = position;
      std::string value;

      while (position < input.size() and
             (std::isalnum(input[position]) or input[position] IS '_' or input[position] IS '-')) {
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
};

//********************************************************************************************************************
// XPath Parser (Recursive Descent)

class XPathParser {
   private:
   std::vector<XPathToken> tokens;
   size_t current = 0;

   public:
   std::unique_ptr<XPathNode> parse(const std::vector<XPathToken>& input_tokens) {
      tokens = input_tokens;
      current = 0;
      return parse_location_path();
   }

   private:
   const XPathToken& peek() const {
      return current < tokens.size() ? tokens[current] : tokens.back(); // END_OF_INPUT
   }

   const XPathToken& consume() {
      return current < tokens.size() ? tokens[current++] : tokens.back();
   }

   bool match(XPathTokenType type) {
      if (peek().type IS type) {
         consume();
         return true;
      }
      return false;
   }

   std::unique_ptr<XPathNode> parse_location_path() {
      auto path = std::make_unique<XPathNode>(XPathNodeType::LocationPath);

      // Check for absolute path (starts with /)
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
      while (peek().type != XPathTokenType::END_OF_INPUT and peek().type != XPathTokenType::RBRACKET and
             peek().type != XPathTokenType::RPAREN) {

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

   std::unique_ptr<XPathNode> parse_step() {
      auto step = std::make_unique<XPathNode>(XPathNodeType::Step);

      // Handle explicit axis specifiers (axis::node-test)
      if (peek().type IS XPathTokenType::IDENTIFIER) {
         // Look ahead for axis separator
         if (current + 1 < tokens.size() and tokens[current + 1].type IS XPathTokenType::AXIS_SEPARATOR) {
            std::string axis_name = consume().value; // consume axis name
            consume(); // consume "::"
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

      // Parse predicates - only square brackets are valid in XPath
      while (peek().type IS XPathTokenType::LBRACKET) {
         auto predicate = parse_predicate();
         if (predicate) {
            step->add_child(std::move(predicate));
         }
      }

      return step;
   }

   std::unique_ptr<XPathNode> parse_node_test() {
      if (peek().type IS XPathTokenType::WILDCARD) {
         consume();
         return std::make_unique<XPathNode>(XPathNodeType::Wildcard, "*");
      }
      else if (peek().type IS XPathTokenType::DOT) {
         // '.' means self::node()
         consume();
         auto step = std::make_unique<XPathNode>(XPathNodeType::Step);
         step->add_child(std::make_unique<XPathNode>(XPathNodeType::AxisSpecifier, "self"));
         step->add_child(std::make_unique<XPathNode>(XPathNodeType::NodeTypeTest, "node"));
         return step;
      }
      else if (peek().type IS XPathTokenType::DOUBLE_DOT) {
         consume();
         auto step = std::make_unique<XPathNode>(XPathNodeType::Step);
         step->add_child(std::make_unique<XPathNode>(XPathNodeType::AxisSpecifier, "parent"));
         step->add_child(std::make_unique<XPathNode>(XPathNodeType::NodeTypeTest, "node"));
         return step;
      }
      else if (peek().type IS XPathTokenType::IDENTIFIER) {
         std::string name = consume().value;
         return std::make_unique<XPathNode>(XPathNodeType::NameTest, name);
      }

      return nullptr;
   }

   std::unique_ptr<XPathNode> parse_predicate() {
      // Predicates must start with '[' and end with ']'
      if (peek().type != XPathTokenType::LBRACKET) return nullptr;
      consume(); // consume '['

      auto predicate = std::make_unique<XPathNode>(XPathNodeType::Predicate);

      // Simple predicate parsing - handle basic cases for now
      if (peek().type IS XPathTokenType::NUMBER) {
         // Index predicate [1], [2], etc.
         std::string index = consume().value;
         predicate->add_child(std::make_unique<XPathNode>(XPathNodeType::Number, index));
      }
      else if (peek().type IS XPathTokenType::EQUALS) {
         // Content predicate [=value]
         consume(); // consume =
         if (peek().type IS XPathTokenType::STRING) {
            std::string content = consume().value;
            auto content_test = std::make_unique<XPathNode>(XPathNodeType::BinaryOp, "content-equals");
            content_test->add_child(std::make_unique<XPathNode>(XPathNodeType::Literal, content));
            predicate->add_child(std::move(content_test));
         }
      }
      else if (peek().type IS XPathTokenType::AT) {
         // Attribute predicate [@attr] or [@attr="value"]
         consume(); // consume @
         if (peek().type IS XPathTokenType::IDENTIFIER || peek().type IS XPathTokenType::WILDCARD) {
            std::string attr_name = consume().value; // may be "*"
            // If '=' follows, parse value comparison, else treat as existence test
            if (match(XPathTokenType::EQUALS) and peek().type IS XPathTokenType::STRING) {
               std::string attr_value = consume().value;
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
      else if (peek().type IS XPathTokenType::IDENTIFIER) {
         // Handle function calls and comparisons
         std::string identifier = peek().value;

         // Check if this is a function call (followed by parentheses)
         if (current + 1 < tokens.size() and tokens[current + 1].type IS XPathTokenType::LPAREN) {
            auto function_call = parse_function_call();
            if (function_call) {
               predicate->add_child(std::move(function_call));
            }
         }
         else {
            // Handle comparison expressions like position()=2, last(), etc.
            auto expression = parse_expression();
            if (expression) {
               predicate->add_child(std::move(expression));
            }
         }
      }

      // Consume closing ']'
      match(XPathTokenType::RBRACKET);

      return predicate;
   }

   std::unique_ptr<XPathNode> parse_function_call() {
      if (peek().type != XPathTokenType::IDENTIFIER) return nullptr;

      std::string function_name = consume().value;
      auto function_node = std::make_unique<XPathNode>(XPathNodeType::Function, function_name);

      if (!match(XPathTokenType::LPAREN)) return nullptr;

      // Parse function arguments
      while (peek().type != XPathTokenType::RPAREN and peek().type != XPathTokenType::END_OF_INPUT) {
         auto arg = parse_expression();
         if (arg) {
            function_node->add_child(std::move(arg));
         }

         if (peek().type == XPathTokenType::COMMA) {
            consume(); // consume comma
         } else {
            break;
         }
      }

      match(XPathTokenType::RPAREN); // consume closing parenthesis
      return function_node;
   }

   // XPath operator precedence (higher number = higher precedence)
   int get_operator_precedence(XPathTokenType type) {
      switch (type) {
         case XPathTokenType::PIPE:           // |
            return 1;
         case XPathTokenType::OR:             // or
            return 2;
         case XPathTokenType::AND:            // and
            return 3;
         case XPathTokenType::EQUALS:         // =
         case XPathTokenType::NOT_EQUALS:     // !=
            return 4;
         case XPathTokenType::LESS_THAN:      // <
         case XPathTokenType::LESS_EQUAL:     // <=
         case XPathTokenType::GREATER_THAN:   // >
         case XPathTokenType::GREATER_EQUAL:  // >=
            return 5;
         case XPathTokenType::PLUS:           // +
         case XPathTokenType::MINUS:          // -
            return 6;
         case XPathTokenType::MULTIPLY:       // *
         case XPathTokenType::DIVIDE:         // div
         case XPathTokenType::MODULO:         // mod
            return 7;
         default:
            return 0;
      }
   }

   bool is_binary_operator(XPathTokenType type) {
      return get_operator_precedence(type) > 0;
   }

   std::unique_ptr<XPathNode> parse_expression() {
      return parse_or_expression();
   }

   std::unique_ptr<XPathNode> parse_or_expression() {
      auto left = parse_and_expression();

      while (peek().type == XPathTokenType::OR) {
         std::string op = consume().value;
         auto right = parse_and_expression();

         auto binary_op = std::make_unique<XPathNode>(XPathNodeType::BinaryOp, op);
         binary_op->add_child(std::move(left));
         binary_op->add_child(std::move(right));
         left = std::move(binary_op);
      }

      return left;
   }

   std::unique_ptr<XPathNode> parse_and_expression() {
      auto left = parse_equality_expression();

      while (peek().type == XPathTokenType::AND) {
         std::string op = consume().value;
         auto right = parse_equality_expression();

         auto binary_op = std::make_unique<XPathNode>(XPathNodeType::BinaryOp, op);
         binary_op->add_child(std::move(left));
         binary_op->add_child(std::move(right));
         left = std::move(binary_op);
      }

      return left;
   }

   std::unique_ptr<XPathNode> parse_equality_expression() {
      auto left = parse_relational_expression();

      while (peek().type == XPathTokenType::EQUALS or peek().type == XPathTokenType::NOT_EQUALS) {
         std::string op = consume().value;
         auto right = parse_relational_expression();

         auto binary_op = std::make_unique<XPathNode>(XPathNodeType::BinaryOp, op);
         binary_op->add_child(std::move(left));
         binary_op->add_child(std::move(right));
         left = std::move(binary_op);
      }

      return left;
   }

   std::unique_ptr<XPathNode> parse_relational_expression() {
      auto left = parse_additive_expression();

      while (peek().type == XPathTokenType::LESS_THAN or
             peek().type == XPathTokenType::LESS_EQUAL or
             peek().type == XPathTokenType::GREATER_THAN or
             peek().type == XPathTokenType::GREATER_EQUAL) {
         std::string op = consume().value;
         auto right = parse_additive_expression();

         auto binary_op = std::make_unique<XPathNode>(XPathNodeType::BinaryOp, op);
         binary_op->add_child(std::move(left));
         binary_op->add_child(std::move(right));
         left = std::move(binary_op);
      }

      return left;
   }

   std::unique_ptr<XPathNode> parse_additive_expression() {
      auto left = parse_multiplicative_expression();

      while (peek().type == XPathTokenType::PLUS or peek().type == XPathTokenType::MINUS) {
         std::string op = consume().value;
         auto right = parse_multiplicative_expression();

         auto binary_op = std::make_unique<XPathNode>(XPathNodeType::BinaryOp, op);
         binary_op->add_child(std::move(left));
         binary_op->add_child(std::move(right));
         left = std::move(binary_op);
      }

      return left;
   }

   std::unique_ptr<XPathNode> parse_multiplicative_expression() {
      auto left = parse_unary_expression();

      while (peek().type == XPathTokenType::MULTIPLY or
             peek().type == XPathTokenType::DIVIDE or
             peek().type == XPathTokenType::MODULO) {
         std::string op = consume().value;
         auto right = parse_unary_expression();

         auto binary_op = std::make_unique<XPathNode>(XPathNodeType::BinaryOp, op);
         binary_op->add_child(std::move(left));
         binary_op->add_child(std::move(right));
         left = std::move(binary_op);
      }

      return left;
   }

   std::unique_ptr<XPathNode> parse_unary_expression() {
      if (peek().type == XPathTokenType::NOT or peek().type == XPathTokenType::MINUS) {
         std::string op = consume().value;
         auto operand = parse_unary_expression();

         auto unary_op = std::make_unique<XPathNode>(XPathNodeType::UnaryOp, op);
         unary_op->add_child(std::move(operand));
         return unary_op;
      }

      return parse_union_expression();
   }

   std::unique_ptr<XPathNode> parse_union_expression() {
      auto left = parse_primary_expression();

      while (peek().type IS XPathTokenType::PIPE) {
         std::string op = consume().value;
         auto right = parse_primary_expression();

         auto binary_op = std::make_unique<XPathNode>(XPathNodeType::BinaryOp, op);
         binary_op->add_child(std::move(left));
         binary_op->add_child(std::move(right));
         left = std::move(binary_op);
      }

      return left;
   }

   std::unique_ptr<XPathNode> parse_primary_expression() {
      if (peek().type IS XPathTokenType::STRING) {
         std::string value = consume().value;
         return std::make_unique<XPathNode>(XPathNodeType::Literal, value);
      }
      else if (peek().type IS XPathTokenType::NUMBER) {
         std::string value = consume().value;
         return std::make_unique<XPathNode>(XPathNodeType::Number, value);
      }
      else if (peek().type IS XPathTokenType::IDENTIFIER) {
         // Check if this is a function call
         if (current + 1 < tokens.size() and tokens[current + 1].type IS XPathTokenType::LPAREN) {
            return parse_function_call();
         }

         std::string value = consume().value;
         return std::make_unique<XPathNode>(XPathNodeType::Literal, value);
      }

      return nullptr;
   }
};

//********************************************************************************************************************
// SimpleXPathEvaluator - Phase 1 refactoring to extract XPath logic from find_tag()

class SimpleXPathEvaluator {
   public:
   struct PathInfo {
      bool flat_scan = false;
      size_t pos = 0;
      std::string_view tag_name;
      uint32_t tag_prefix = 0;
      std::string attrib_value;
      std::string_view attrib_name;
      bool wild = false;
      int subscript = 0;
   };

   extXML *xml;
   XPathFunctionLibrary function_library;
   XPathContext context;
   AxisEvaluator axis_evaluator;
   explicit SimpleXPathEvaluator(extXML *XML) : xml(XML), axis_evaluator(XML) {}

   // Phase 1 methods (string-based)
   ERR parse_path(std::string_view XPath, PathInfo &info);
   bool match_tag(const PathInfo &info, uint32_t current_prefix);
   ERR evaluate_step(std::string_view XPath, PathInfo info, uint32_t current_prefix);

   // Phase 2 methods (AST-based)
   ERR evaluate_ast(const XPathNode* node, uint32_t current_prefix);
   ERR evaluate_location_path(const XPathNode* path_node, uint32_t current_prefix);
   ERR evaluate_step_ast(const XPathNode* step_node, uint32_t current_prefix);
   bool match_node_test(const XPathNode* node_test, uint32_t current_prefix);
   bool evaluate_predicate(const XPathNode* predicate_node, uint32_t current_prefix);

   // Phase 3 methods (function support)
   XPathValue evaluate_expression(const XPathNode* expr_node, uint32_t current_prefix);
   XPathValue evaluate_function_call(const XPathNode* func_node, uint32_t current_prefix);

   // Utility method to try AST-based parsing first, fall back to string-based
   ERR find_tag_enhanced(std::string_view XPath, uint32_t current_prefix);

   // Helper method to evaluate simple function expressions in string-based evaluation
   bool evaluate_function_expression(const std::string& expression);
};

//********************************************************************************************************************
// Parse XPath and extract path components

ERR SimpleXPathEvaluator::parse_path(std::string_view XPath, PathInfo &info)
{
   pf::Log log(__FUNCTION__);

   if ((XPath.empty()) or (XPath[0] != '/')) {
      log.warning("Missing '/' prefix in '%.*s'.", int(XPath.size()), XPath.data());
      return ERR::StringFormat;
   }

   // Check for flat scan (//)
   info.pos = [&info, XPath]() mutable {
      if (XPath[0] != '/') return 0;
      if ((XPath.size() > 1) and (XPath[1] != '/')) return 1;
      info.flat_scan = true;
      return 2;
   }();

   // Check if the path is something like '/@attrib' which means we want the attribute value of the current tag
   if ((info.pos < XPath.size()) and (XPath[info.pos] IS '@')) {
      xml->Attrib.assign(XPath.substr(info.pos + 1));
      return ERR::Okay;
   }

   // Parse the tag name
   auto start = info.pos;
   auto delimiter_pos = XPath.find_first_of("/[(", info.pos);
   info.pos = (delimiter_pos != std::string_view::npos) ? delimiter_pos : XPath.size();
   if (info.pos > start) info.tag_name = XPath.substr(start, info.pos - start);
   else info.tag_name = "*";

   // Parse namespace prefix from current tag
   if ((xml->Flags & XMF::NAMESPACE_AWARE) != XMF::NIL) {
      if (auto colon = info.tag_name.find(':'); colon != std::string_view::npos) {
         info.tag_prefix = pf::strhash(info.tag_name.substr(0, colon));
         info.tag_name = info.tag_name.substr(colon + 1);
      }
   }

   // Parse filter instructions
   char end_char;
   if ((info.pos < XPath.size()) and ((XPath[info.pos] IS '[') or (XPath[info.pos] IS '('))) {
      if (XPath[info.pos] IS '[') end_char = ']';
      else end_char = ')';

      info.pos++;

      auto non_space_pos = XPath.find_first_not_of(" \t\n\r", info.pos);
      info.pos = (non_space_pos != std::string_view::npos) ? non_space_pos : XPath.size();

      if ((info.pos < XPath.size()) and (XPath[info.pos] >= '0') and (XPath[info.pos] <= '9')) { // Parse index
         char *end;
         info.subscript = strtol(XPath.data() + info.pos, &end, 0);
         if (info.subscript < 1) return ERR::Syntax; // Subscripts start from 1, not 0
         info.pos = end - XPath.data();
      }
      else if ((info.pos < XPath.size()) and ((XPath[info.pos] IS '@') or (XPath[info.pos] IS '='))) {
         if (XPath[info.pos] IS '@') {  // Parse attribute filter such as "[@id='5']" or "[@*='v']" or "[@attr]"
            info.pos++;

            auto len = info.pos;
            if ((len < XPath.size()) and (XPath[len] IS '*')) {
               // Attribute wildcard
               info.attrib_name = "*";
               len++;
            } else {
               while ((len < XPath.size()) and (((XPath[len] >= 'a') and (XPath[len] <= 'z')) or
                      ((XPath[len] >= 'A') and (XPath[len] <= 'Z')) or
                      (XPath[len] IS '_'))) len++;
               info.attrib_name = XPath.substr(info.pos, len - info.pos);
            }
            if (info.attrib_name.empty()) return ERR::Syntax;

            info.pos = len;
            auto non_space_pos2 = XPath.find_first_not_of(" \t\n\r", info.pos);
            info.pos = (non_space_pos2 != std::string_view::npos) ? non_space_pos2 : XPath.size(); // Skip whitespace

            if ((info.pos < XPath.size()) and (XPath[info.pos] IS '=')) info.pos++;
         }
         else info.pos++; // Skip '=' (indicates matching on content)

         auto non_space_pos3 = XPath.find_first_not_of(" \t\n\r", info.pos);
         info.pos = (non_space_pos3 != std::string_view::npos) ? non_space_pos3 : XPath.size(); // Skip whitespace

         // Parse value (optional). If no value provided, treat as attribute-existence test [@attr]
         if ((info.pos < XPath.size()) and ((XPath[info.pos] IS '\'') or (XPath[info.pos] IS '"'))) {
            const char quote = XPath[info.pos++];
            bool esc_attrib = false;
            auto end = info.pos;
            while ((end < XPath.size()) and (XPath[end] != quote)) {
               if (XPath[end] IS '\\') { // Escape character check
                  if ((end + 1 < XPath.size())) {
                     auto ch = XPath[end+1];
                     if ((ch IS '*') or (ch IS quote) or (ch IS '\\')) {
                        end++;
                        esc_attrib = true;
                     }
                  }
               }
               else if (XPath[end] IS '*') info.wild = true;
               end++;
            }

            if ((end >= XPath.size()) or (XPath[end] != quote)) return ERR::Syntax; // Quote not terminated correctly

            info.attrib_value.assign(XPath.substr(info.pos, end - info.pos));
            info.pos = end + 1;

            if (esc_attrib) {
               for (int i=0; i < std::ssize(info.attrib_value); i++) {
                  if (info.attrib_value[i] != '\\') continue;
                  auto ch = info.attrib_value[i+1];
                  if ((ch IS '*') or (ch IS quote) or (ch IS '\\')) {
                     info.attrib_value.erase(i);
                     i--;
                  }
               }
            }
         }
         else if ((info.pos < XPath.size()) and (XPath[info.pos] != end_char)) {
            auto end_pos = XPath.find(end_char, info.pos);
            int end = (end_pos != std::string_view::npos) ? end_pos : XPath.size();

            // Check for wildcards in the range
            if (XPath.substr(info.pos, end - info.pos).find('*') != std::string_view::npos) info.wild = true;
            info.attrib_value.assign(XPath.substr(info.pos, end - info.pos));
            info.pos = end;
         }
      }
      else return ERR::Syntax;

      auto non_space_pos4 = XPath.find_first_not_of(" \t\n\r", info.pos);
      info.pos = (non_space_pos4 != std::string_view::npos) ? non_space_pos4 : XPath.size(); // Skip whitespace
      if ((info.pos >= XPath.size()) or (XPath[info.pos] != end_char)) return ERR::Syntax;
      info.pos++;
   }

   return ERR::Okay;
}

//********************************************************************************************************************
// Match a tag against the parsed path criteria

bool SimpleXPathEvaluator::match_tag(const PathInfo &info, uint32_t current_prefix)
{
   bool tag_matched = false;
   uint32_t cursor_prefix = current_prefix;

   // Special handling for function calls - check if subscript == 0 and attrib_value contains function
   if (info.subscript IS 0 and !info.attrib_value.empty()) {
      // Check if attrib_value looks like a function call (contains "position()=", "last()", etc.)
      if (info.attrib_value.find("position()") != std::string::npos or
          info.attrib_value.find("last()") != std::string::npos or
          info.attrib_value.find("count(") != std::string::npos) {

         // Try to parse and evaluate the function call
         // For now, handle simple cases like "position()=2"
         if (info.attrib_value.find("position()=") != std::string::npos) {
            // Extract the number after "position()="
            auto pos = info.attrib_value.find("position()=");
            if (pos != std::string::npos) {
               auto num_start = pos + 11; // Length of "position()="
               if (num_start < info.attrib_value.length()) {
                  int expected_position = std::stoi(info.attrib_value.substr(num_start));
                  // The position context is handled in evaluate_step, but for string-based
                  // we need to manually track position. This is a limitation we'll note.
                  // We'll handle this in evaluate_step with position tracking
                  return false;
               }
            }
         }
      }
   }

   // Match both tag name and prefix, if applicable
   if ((xml->Flags & XMF::NAMESPACE_AWARE) != XMF::NIL) {
      std::string_view cursor_local_name = xml->Cursor->name();
      if (auto colon = cursor_local_name.find(':'); colon != std::string_view::npos) {
         cursor_prefix = pf::strhash(cursor_local_name.substr(0, colon));
         cursor_local_name = cursor_local_name.substr(colon + 1);
      }

      auto tag_wild = info.tag_name.find('*') != std::string_view::npos;
      bool name_matches = tag_wild ? pf::wildcmp(info.tag_name, cursor_local_name) : pf::iequals(info.tag_name, cursor_local_name);
      bool prefix_matches = info.tag_prefix ? cursor_prefix IS info.tag_prefix : true;
      tag_matched = name_matches and prefix_matches;
   }
   else { // Traditional matching: just compare full tag names
      auto tag_wild = info.tag_name.find('*') != std::string_view::npos;
      if (tag_wild) tag_matched = pf::wildcmp(info.tag_name, xml->Cursor->name());
      else tag_matched = pf::iequals(info.tag_name, xml->Cursor->name());
   }

   if (!tag_matched) return false;

   // Check attribute/content filters
      if ((!info.attrib_name.empty()) or (!info.attrib_value.empty())) {
      if (xml->Cursor->name()) {
         if (!info.attrib_name.empty()) { // Match by named attribute value or existence; '*' matches any attribute name
            for (int a=1; a < std::ssize(xml->Cursor->Attribs); ++a) {
               bool name_matches = (info.attrib_name IS "*") ? true : pf::iequals(xml->Cursor->Attribs[a].Name, info.attrib_name);
               if (name_matches) {
                  // If no attribute value specified, treat as existence test
                  if (info.attrib_value.empty()) return true;
                  if (info.wild) {
                     if (pf::wildcmp(xml->Cursor->Attribs[a].Value, info.attrib_value)) return true;
                  }
                  else if (pf::iequals(xml->Cursor->Attribs[a].Value, info.attrib_value)) {
                     return true;
                  }
               }
            }
            return false;
         }
         else if (!info.attrib_value.empty()) {
            if ((!xml->Cursor->Children.empty()) and (xml->Cursor->Children[0].Attribs[0].isContent())) {
               if (info.wild) {
                  return pf::wildcmp(xml->Cursor->Children[0].Attribs[0].Value, info.attrib_value);
               }
               else return pf::iequals(xml->Cursor->Children[0].Attribs[0].Value, info.attrib_value);
            }
            return false;
         }
      }
      return false;
   }

   return true;
}

//********************************************************************************************************************
// Evaluate a single XPath step with tree traversal

ERR SimpleXPathEvaluator::evaluate_step(std::string_view XPath, PathInfo info, uint32_t current_prefix)
{
   pf::Log log(__FUNCTION__);

   if ((xml->Flags & XMF::LOG_ALL) != XMF::NIL) {
      log.branch("XPath: %.*s, TagName: %.*s", int(XPath.size()), XPath.data(), int(info.tag_name.size()), info.tag_name.data());
   }

   // Check if this predicate contains function calls that need position tracking
   bool has_function_call = false;
   std::string function_expression;
   if (!info.attrib_value.empty()) {
      pf::Log func_log("Function Detection");
      func_log.msg("Checking attrib_value: '%s'", info.attrib_value.c_str());
      if (info.attrib_value.find("position()") != std::string::npos or
          info.attrib_value.find("last()") != std::string::npos) {
         has_function_call = true;
         function_expression = info.attrib_value;
         func_log.msg("Function call detected: '%s'", function_expression.c_str());
      }
   }

   // For function calls, we need to collect all matching nodes first to get the total count
   std::vector<XMLTag*> matching_nodes;
   if (has_function_call) {
      // First pass: collect all nodes that match the tag name (without predicate)
      PathInfo tag_only_info = info;
      tag_only_info.attrib_value.clear(); // Remove predicate
      tag_only_info.subscript = 0; // Remove index

      for (auto cursor = xml->CursorTags->begin(); cursor != xml->CursorTags->end(); ++cursor) {
         auto saved_cursor = xml->Cursor;
         xml->Cursor = cursor;
         if (match_tag(tag_only_info, current_prefix)) {
            matching_nodes.push_back(&(*cursor));
         }
         xml->Cursor = saved_cursor;
      }

      // Now evaluate the function call for each matching node
      for (size_t pos = 0; pos < matching_nodes.size(); ++pos) {
         // Set up context for function evaluation
         context.context_node = matching_nodes[pos];
         context.position = pos + 1; // XPath positions are 1-based
         context.size = matching_nodes.size();

         // Find this node in CursorTags and set xml->Cursor to it
         for (auto cursor = xml->CursorTags->begin(); cursor != xml->CursorTags->end(); ++cursor) {
            if (&(*cursor) IS matching_nodes[pos]) {
               xml->Cursor = cursor;
               break;
            }
         }

         // Evaluate the function expression
         if (evaluate_function_expression(function_expression)) {
            // This node matches the function predicate
            if (info.pos < XPath.size() and XPath[info.pos] IS '/' and info.pos + 1 < XPath.size() and XPath[info.pos+1] IS '@') {
               xml->Attrib.assign(XPath.substr(info.pos + 2));
            } else {
               xml->Attrib.clear();
            }

            if (!xml->Callback.defined()) return ERR::Okay;

            // Call callback if defined
            auto error = ERR::Okay;
            if (xml->Callback.isC()) {
               auto routine = (ERR (*)(extXML *, int, CSTRING, APTR))xml->Callback.Routine;
               error = routine(xml, xml->Cursor->ID, xml->Attrib.empty() ? nullptr : xml->Attrib.c_str(), xml->Callback.Meta);
            }
            else if (xml->Callback.isScript()) {
               if (sc::Call(xml->Callback, std::to_array<ScriptArg>({
                  { "XML",  xml, FD_OBJECTPTR },
                  { "Tag",  xml->Cursor->ID },
                  { "Attrib", xml->Attrib.empty() ? CSTRING(nullptr) : xml->Attrib.c_str() }
               }), error) != ERR::Okay) error = ERR::Terminate;
            }

            if (error IS ERR::Terminate) return ERR::Terminate;
            if (error != ERR::Okay) return error;
         }
      }

      return xml->Callback.defined() ? ERR::Okay : ERR::Search;
   }

   // Original logic for non-function predicates
   bool stop = false;
   for (; xml->Cursor != xml->CursorTags->end() and (!stop); xml->Cursor++) {
      bool match = false;
      uint32_t cursor_prefix = current_prefix;

      if (info.flat_scan or match_tag(info, cursor_prefix)) {
         if (info.flat_scan and !match_tag(info, cursor_prefix)) {
            // For flat scan, check children if current tag doesn't match
            if (!xml->Cursor->Children.empty()) {
               auto save_cursor = xml->Cursor;
               auto save_tags   = xml->CursorTags;

               xml->CursorTags = &xml->Cursor->Children;
               xml->Cursor     = xml->Cursor->Children.begin();

               ERR error = evaluate_step(XPath, info, cursor_prefix);
               if ((error IS ERR::Okay) and (!xml->Callback.defined())) return ERR::Okay;

               xml->Cursor     = save_cursor;
               xml->CursorTags = save_tags;
            }
            continue;
         }

         match = true;
      }

      if ((not match) and (not info.flat_scan)) continue;

      if (info.subscript > 1) {
         info.subscript--;
         continue;
      }
      else if (info.subscript IS 1) {
         stop = true;
      }

      bool path_ended = info.pos >= XPath.size() or ((XPath[info.pos] IS '/') and (info.pos + 1 < XPath.size()) and (XPath[info.pos+1] IS '@'));
      if ((match) and (path_ended)) { // Matching tag found and there is nothing left in the path
         if (info.pos < XPath.size()) xml->Attrib.assign(XPath.substr(info.pos + 2));
         else xml->Attrib.clear();

         if (!xml->Callback.defined()) return ERR::Okay; // End of query, successfully found tag

         auto error = ERR::Okay;
         if (xml->Callback.isC()) {
            auto routine = (ERR (*)(extXML *, int, CSTRING, APTR))xml->Callback.Routine;
            error = routine(xml, xml->Cursor->ID, xml->Attrib.empty() ? nullptr : xml->Attrib.c_str(), xml->Callback.Meta);
         }
         else if (xml->Callback.isScript()) {
            if (sc::Call(xml->Callback, std::to_array<ScriptArg>({
               { "XML",  xml, FD_OBJECTPTR },
               { "Tag",  xml->Cursor->ID },
               { "Attrib", xml->Attrib.empty() ? CSTRING(nullptr) : xml->Attrib.c_str() }
            }), error) != ERR::Okay) error = ERR::Terminate;
         }
         else return ERR::InvalidValue;

         if (error IS ERR::Terminate) return ERR::Terminate;
         if (error != ERR::Okay) return error;

         // Searching will continue because the client specified a callback...
      }
      else if (!xml->Cursor->Children.empty()) { // Tag matched & XPath continues, OR flat-scan enabled.  Scan deeper into the tree
         auto save_cursor = xml->Cursor;
         auto save_tags   = xml->CursorTags;

         xml->CursorTags = &xml->Cursor->Children;
         xml->Cursor     = xml->Cursor->Children.begin();

         ERR error;
         if (info.flat_scan) error = evaluate_step(XPath, info, cursor_prefix); // Continue search from the beginning of the tag name
         else {
            // Parse the next step
            PathInfo next_info;
            auto parse_err = parse_path(XPath.substr(info.pos), next_info);
            if (parse_err != ERR::Okay) return parse_err;
            error = evaluate_step(XPath.substr(info.pos), next_info, cursor_prefix);
         }

         if ((error IS ERR::Okay) and (!xml->Callback.defined())) return ERR::Okay;
         if (error IS ERR::Terminate) return ERR::Terminate;

         xml->Cursor     = save_cursor;
         xml->CursorTags = save_tags;
      }
   }

   if (xml->Callback.defined()) return ERR::Okay;
   else return ERR::Search;
}

//********************************************************************************************************************
// Phase 3: Function Call Support

XPathValue SimpleXPathEvaluator::evaluate_expression(const XPathNode* expr_node, uint32_t current_prefix)
{
   if (!expr_node) return XPathValue();

   switch (expr_node->type) {
      case XPathNodeType::Function:
         return evaluate_function_call(expr_node, current_prefix);

      case XPathNodeType::Literal:
         return XPathValue(expr_node->value);

      case XPathNodeType::Number:
         return XPathValue(std::stod(expr_node->value));

      case XPathNodeType::Variable:
         return context.get_variable(expr_node->value);

      case XPathNodeType::BinaryOp:
         {
            if (expr_node->child_count() >= 2) {
               XPathValue left = evaluate_expression(expr_node->get_child(0), current_prefix);
               XPathValue right = evaluate_expression(expr_node->get_child(1), current_prefix);

               // Handle binary operators
               if (expr_node->value IS "=") {
                  return XPathValue(left.to_string() IS right.to_string());
               }
               else if (expr_node->value IS "!=") {
                  return XPathValue(left.to_string() != right.to_string());
               }
               else if (expr_node->value IS "<") {
                  return XPathValue(left.to_number() < right.to_number());
               }
               else if (expr_node->value IS "<=") {
                  return XPathValue(left.to_number() <= right.to_number());
               }
               else if (expr_node->value IS ">") {
                  return XPathValue(left.to_number() > right.to_number());
               }
               else if (expr_node->value IS ">=") {
                  return XPathValue(left.to_number() >= right.to_number());
               }
               else if (expr_node->value IS "and") {
                  return XPathValue(left.to_boolean() and right.to_boolean());
               }
               else if (expr_node->value IS "or") {
                  return XPathValue(left.to_boolean() or right.to_boolean());
               }
               else if (expr_node->value IS "+") {
                  return XPathValue(left.to_number() + right.to_number());
               }
               else if (expr_node->value IS "-") {
                  return XPathValue(left.to_number() - right.to_number());
               }
               else if (expr_node->value IS "*") {
                  return XPathValue(left.to_number() * right.to_number());
               }
               else if (expr_node->value IS "div") {
                  double right_num = right.to_number();
                  if (right_num IS 0.0) {
                     return XPathValue(std::numeric_limits<double>::infinity());
                  }
                  return XPathValue(left.to_number() / right_num);
               }
               else if (expr_node->value IS "mod") {
                  return XPathValue(std::fmod(left.to_number(), right.to_number()));
               }
               else if (expr_node->value IS "|") {
                  // Union operator - combine node sets
                  std::vector<XMLTag*> combined_nodes;

                  // Add all nodes from left operand
                  if (left.type == XPathValueType::NodeSet) {
                     combined_nodes = left.node_set;
                  }

                  // Add unique nodes from right operand
                  if (right.type == XPathValueType::NodeSet) {
                     for (auto* node : right.node_set) {
                        // Check if node is already in result set
                        bool found = false;
                        for (auto* existing : combined_nodes) {
                           if (existing IS node) {
                              found = true;
                              break;
                           }
                        }
                        if (!found) {
                           combined_nodes.push_back(node);
                        }
                     }
                  }

                  return XPathValue(combined_nodes);
               }
            }
         }
         break;

      case XPathNodeType::UnaryOp:
         {
            if (expr_node->child_count() >= 1) {
               XPathValue operand = evaluate_expression(expr_node->get_child(0), current_prefix);

               if (expr_node->value == "not") {
                  return XPathValue(!operand.to_boolean());
               }
               else if (expr_node->value == "-") {
                  return XPathValue(-operand.to_number());
               }
            }
         }
         break;

      default:
         break;
   }

   return XPathValue(); // Default: empty boolean
}

XPathValue SimpleXPathEvaluator::evaluate_function_call(const XPathNode* func_node, uint32_t current_prefix)
{
   if (!func_node or func_node->type != XPathNodeType::Function) {
      return XPathValue();
   }

   // Set up context for function evaluation
   context.context_node = xml->Cursor;
   // Position and size would need to be set based on current node set context

   // Evaluate arguments
   std::vector<XPathValue> args;
   for (size_t i = 0; i < func_node->child_count(); ++i) {
      const XPathNode* arg_node = func_node->get_child(i);
      XPathValue arg_value = evaluate_expression(arg_node, current_prefix);
      args.push_back(arg_value);
   }

   // Call the function
   return function_library.evaluate_function(func_node->value, args, context);
}

//********************************************************************************************************************
// Helper method to evaluate simple function expressions in string-based evaluation

bool SimpleXPathEvaluator::evaluate_function_expression(const std::string& expression)
{
   // Handle simple function expressions like:
   // "position()=2"
   // "last()"
   // "position()!=1"

   if (expression.find("position()=") != std::string::npos) {
      auto pos = expression.find("position()=");
      if (pos != std::string::npos) {
         auto num_start = pos + 11; // Length of "position()="
         if (num_start < expression.length()) {
            int expected_position = std::stoi(expression.substr(num_start));
            XPathValue pos_result = function_library.evaluate_function("position", {}, context);
            return pos_result.to_number() IS expected_position;
         }
      }
   }
   else if (expression IS "last()") {
      XPathValue last_result = function_library.evaluate_function("last", {}, context);
      return last_result.to_boolean(); // last() itself is truthy
   }
   else if (expression.find("last()") != std::string::npos and expression.find("=") != std::string::npos) {
      // Handle expressions like "position()=last()"
      XPathValue pos_result = function_library.evaluate_function("position", {}, context);
      XPathValue last_result = function_library.evaluate_function("last", {}, context);
      return pos_result.to_number() IS last_result.to_number();
   }

   return false; // Unsupported expression
}

//********************************************************************************************************************
// Phase 2 AST-based evaluation methods

ERR SimpleXPathEvaluator::find_tag_enhanced(std::string_view XPath, uint32_t current_prefix)
{
   pf::Log log(__FUNCTION__);
   log.msg("Enhanced XPath: %.*s", int(XPath.size()), XPath.data());

   // Try AST-based parsing first
   XPathTokenizer tokenizer;
   auto tokens = tokenizer.tokenize(XPath);

   XPathParser parser;
   auto ast = parser.parse(tokens);

   if (ast) {
      log.msg("AST parsed successfully, evaluating...");
      auto result = evaluate_ast(ast.get(), current_prefix);
      log.msg("AST evaluation result: %d", int(result));
      if (result IS ERR::Okay or result IS ERR::Search) {
         return result;
      }
      // If AST evaluation fails, fall back to string-based
      log.msg("AST evaluation failed, falling back to string-based parsing");
   } else {
      log.msg("AST parsing failed");
   }

   // Fall back to Phase 1 string-based evaluation
   PathInfo info;
   auto parse_result = parse_path(XPath, info);
   if (parse_result != ERR::Okay) return parse_result;

   if (!xml->Attrib.empty()) return ERR::Okay;
   return evaluate_step(XPath, info, current_prefix);
}

ERR SimpleXPathEvaluator::evaluate_ast(const XPathNode* node, uint32_t current_prefix)
{
   pf::Log log("evaluate_ast");
   if (!node) {
      log.msg("Null node passed");
      return ERR::NullArgs;
   }

   log.msg("Evaluating AST node type: %d", int(node->type));
   switch (node->type) {
      case XPathNodeType::LocationPath:
         log.msg("Evaluating LocationPath");
         return evaluate_location_path(node, current_prefix);

      case XPathNodeType::Step:
         log.msg("Evaluating Step");
         return evaluate_step_ast(node, current_prefix);

      default:
         log.msg("Unknown node type: %d", int(node->type));
         return ERR::Failed;
   }
}

ERR SimpleXPathEvaluator::evaluate_location_path(const XPathNode* path_node, uint32_t current_prefix)
{
   pf::Log log(__FUNCTION__);
   log.msg("Evaluating location path with %zu children", path_node->child_count());

   if (!path_node or path_node->type != XPathNodeType::LocationPath) {
      return ERR::NullArgs;
   }

   bool is_absolute = false;
   size_t step_start = 0;

   // Check if this is an absolute path (starts with Root node)
   if (path_node->child_count() > 0 and path_node->get_child(0)->type IS XPathNodeType::Root) {
      is_absolute = true;
      step_start = 1;

      // Handle descendant-or-self for // paths
      if (path_node->get_child(0)->value IS "//") {
         // Set flag for deep scanning
         // This will be handled in the step evaluation
      }
   }

   // For now, delegate back to string-based evaluation with enhanced function support
   // The AST-based multi-step path evaluation is complex and the string-based version
   // already handles path navigation correctly.
   log.msg("Delegating to enhanced string-based evaluation");
   return ERR::Failed; // This will cause fallback to string-based with function enhancement

   return xml->Callback.defined() ? ERR::Okay : ERR::Search;
}

ERR SimpleXPathEvaluator::evaluate_step_ast(const XPathNode* step_node, uint32_t current_prefix)
{
   if (!step_node or step_node->type != XPathNodeType::Step) {
      return ERR::NullArgs;
   }

   // Extract components from the step
   const XPathNode* axis_specifier = nullptr;
   const XPathNode* node_test = nullptr;
   std::vector<const XPathNode*> predicates;

   for (size_t i = 0; i < step_node->child_count(); ++i) {
      const XPathNode* child = step_node->get_child(i);
      switch (child->type) {
         case XPathNodeType::AxisSpecifier:
            axis_specifier = child;
            break;
         case XPathNodeType::NameTest:
         case XPathNodeType::Wildcard:
         case XPathNodeType::NodeTypeTest:
            node_test = child;
            break;
         case XPathNodeType::Predicate:
            predicates.push_back(child);
            break;
         default:
            break;
      }
   }

   // Default axis is child
   std::string axis_name = axis_specifier ? axis_specifier->value : "child";
   AxisType axis = axis_evaluator.string_to_axis_type(axis_name);

   // Get current context node
   XMLTag* context_node = nullptr;
   if (xml->Cursor != xml->CursorTags->end()) {
      context_node = &(*xml->Cursor); // XMLTag is stored by value, not pointer
   }
   if (!context_node) {
      return ERR::Search;
   }

   // Evaluate axis to get candidate nodes
   auto candidate_nodes = axis_evaluator.evaluate_axis(axis, context_node);

   // Set up context for function evaluation - size is the total number of candidates
   context.size = candidate_nodes.size();

   bool found_match = false;
   size_t position = 1; // XPath positions are 1-based
   for (XMLTag* candidate : candidate_nodes) {
      // Set current position in context for position() function
      context.position = position;
      // Check if this candidate matches the node test
      if (node_test) {
         // Set up temporary cursor position for candidate
         auto saved_cursor = xml->Cursor;
         auto saved_cursor_tags = xml->CursorTags;

         // Find the candidate in the document structure
         bool candidate_found = false;

         // Look for the candidate in the current cursor tags first
         for (auto iter = xml->CursorTags->begin(); iter != xml->CursorTags->end(); ++iter) {
            if (&(*iter) IS candidate) {
               xml->Cursor = iter;
               candidate_found = true;
               break;
            }
         }

         // If not found in current context, search the entire document
         if (!candidate_found) {
            for (auto iter = xml->Tags.begin(); iter != xml->Tags.end(); ++iter) {
               if (&(*iter) IS candidate) {
                  xml->CursorTags = &xml->Tags;
                  xml->Cursor = iter;
                  candidate_found = true;
                  break;
               }
            }
         }

         if (!candidate_found) {
            xml->Cursor = saved_cursor;
            xml->CursorTags = saved_cursor_tags;
            continue;
         }

         if (!match_node_test(node_test, xml->Cursor->ID)) {
            xml->Cursor = saved_cursor;
            xml->CursorTags = saved_cursor_tags;
            continue;
         }
      }

      // Test all predicates
      bool all_predicates_match = true;
      for (const XPathNode* predicate : predicates) {
         if (!evaluate_predicate(predicate, xml->Cursor->ID)) {
            all_predicates_match = false;
            break;
         }
      }

      if (all_predicates_match) {
         found_match = true;

         // If we have a callback, invoke it
         if (xml->Callback.defined()) {
            // Use existing callback mechanism from the codebase
            auto error = ERR::Okay;
            if (xml->Callback.isC()) {
               auto routine = (ERR (*)(extXML *, int, CSTRING, APTR))xml->Callback.Routine;
               error = routine(xml, xml->Cursor->ID, xml->Attrib.empty() ? nullptr : xml->Attrib.c_str(), xml->Callback.Meta);
            }
            else if (xml->Callback.isScript()) {
               if (sc::Call(xml->Callback, std::to_array<ScriptArg>({
                  { "XML",  xml, FD_OBJECTPTR },
                  { "Tag",  xml->Cursor->ID },
                  { "Attrib", xml->Attrib.empty() ? CSTRING(nullptr) : xml->Attrib.c_str() }
               }), error) != ERR::Okay) error = ERR::Terminate;
            }
            if (error == ERR::Terminate) return ERR::Terminate;
         } else {
            // No callback, we found our match
            return ERR::Okay;
         }
      }

      // Increment position for next candidate
      position++;
   }

   return found_match ? ERR::Okay : ERR::Search;
}

bool SimpleXPathEvaluator::match_node_test(const XPathNode* node_test, uint32_t current_prefix)
{
   if (!node_test) return true;

   switch (node_test->type) {
      case XPathNodeType::Wildcard:
         return true; // * matches any element

      case XPathNodeType::NameTest:
         {
            std::string_view tag_name = node_test->value;
            bool name_matches = false;

            if ((xml->Flags & XMF::NAMESPACE_AWARE) != XMF::NIL) {
               std::string_view cursor_local_name = xml->Cursor->name();
               uint32_t cursor_prefix = current_prefix;

               if (auto colon = cursor_local_name.find(':'); colon != std::string_view::npos) {
                  cursor_prefix = pf::strhash(cursor_local_name.substr(0, colon));
                  cursor_local_name = cursor_local_name.substr(colon + 1);
               }

               name_matches = pf::iequals(tag_name, cursor_local_name);
            }
            else {
               name_matches = pf::iequals(tag_name, xml->Cursor->name());
            }

            return name_matches;
         }

      case XPathNodeType::NodeTypeTest:
         // For now, only support basic node type tests
         return true;

      default:
         return false;
   }
}

bool SimpleXPathEvaluator::evaluate_predicate(const XPathNode* predicate_node, uint32_t current_prefix)
{
   if (!predicate_node or predicate_node->type != XPathNodeType::Predicate) {
      return true;
   }

   if (predicate_node->child_count() IS 0) return true;

   const XPathNode* child = predicate_node->get_child(0);

   switch (child->type) {
      case XPathNodeType::Number:
         {
            // Index predicate [1], [2], etc.
            int index = std::stoi(child->value);
            // For now, just accept the first match (simplified)
            return index IS 1;
         }

      case XPathNodeType::BinaryOp:
         {
            if (child->value IS "attribute-equals") {
               if (child->child_count() >= 2) {
                  std::string attr_name = child->get_child(0)->value;
                  std::string attr_value = child->get_child(1)->value;

                  // Check if current cursor has this attribute with this value
                  for (int a = 1; a < std::ssize(xml->Cursor->Attribs); ++a) {
                     bool name_matches = (attr_name == "*") ? true : pf::iequals(xml->Cursor->Attribs[a].Name, attr_name);
                     if (name_matches) {
                        if (attr_name == "*") {
                           // wildcard name: true if any attribute equals value (case-insensitive)
                           if (pf::iequals(xml->Cursor->Attribs[a].Value, attr_value)) return true;
                           continue;
                        }
                        return pf::iequals(xml->Cursor->Attribs[a].Value, attr_value);
                     }
                  }
               }
               return false;
            }
            else if (child->value IS "content-equals") {
               if (child->child_count() >= 1) {
                  std::string content_value = child->get_child(0)->value;

                  // Check if current cursor has this content
                  if ((!xml->Cursor->Children.empty()) and (xml->Cursor->Children[0].Attribs[0].isContent())) {
                     return pf::iequals(xml->Cursor->Children[0].Attribs[0].Value, content_value);
                  }
               }
               return false;
            }
            else if (child->value IS "attribute-exists") {
               if (child->child_count() >= 1) {
                  std::string attr_name = child->get_child(0)->value;
                  for (int a = 1; a < std::ssize(xml->Cursor->Attribs); ++a) {
                     if ((attr_name == "*") || pf::iequals(xml->Cursor->Attribs[a].Name, attr_name)) {
                        return true;
                     }
                  }
               }
               return false;
            }
            // Handle general comparison operations like position()=2
            else if (child->child_count() >= 2) {
               XPathValue left = evaluate_expression(child->get_child(0), current_prefix);
               XPathValue right = evaluate_expression(child->get_child(1), current_prefix);

               if (child->value IS "=") {
                  if (left.type == XPathValueType::Number and right.type IS XPathValueType::Number) {
                     return left.to_number() IS right.to_number();
                  }
                  return left.to_string() IS right.to_string();
               }
               else if (child->value IS "!=") {
                  return left.to_string() != right.to_string();
               }
               else if (child->value IS "<") {
                  return left.to_number() < right.to_number();
               }
               else if (child->value IS "<=") {
                  return left.to_number() <= right.to_number();
               }
               else if (child->value IS ">") {
                  return left.to_number() > right.to_number();
               }
               else if (child->value IS ">=") {
                  return left.to_number() >= right.to_number();
               }
            }
            return false;
         }

      case XPathNodeType::Function:
         {
            // Evaluate function in predicate context
            XPathValue result = evaluate_function_call(child, current_prefix);
            return result.to_boolean();
         }


      default:
         return true;
   }
}

ERR extXML::find_tag(std::string_view XPath, uint32_t CurrentPrefix)
{
   pf::Log log(__FUNCTION__);

   if ((!CursorTags) or (CursorTags->empty())) {
      log.warning("Sanity check failed; CursorTags not defined or empty.");
      return ERR::Failed;
   }

   // Create evaluator and delegate to it
   SimpleXPathEvaluator evaluator(this);

   // Try Phase 2/3 AST-based evaluation first only for function calls (not round bracket predicates)
   // Fall back to Phase 1 string-based evaluation for backward compatibility
   auto paren_pos = XPath.find('(');
   if (paren_pos != std::string_view::npos and XPath.find(')') != std::string_view::npos) {
      // Check if this looks like a function call (identifier followed by opening parenthesis)
      // vs. round bracket predicate (path component followed by opening parenthesis)
      bool is_function_call = false;
      if (paren_pos > 0) {
         // Look backwards to see if there's an identifier immediately before the '('
         auto before_paren = paren_pos - 1;
         // Skip any whitespace
         while (before_paren > 0 and (XPath[before_paren] IS ' ' or XPath[before_paren] IS '\t')) {
            before_paren--;
         }
         // Check if the character before '(' is part of an identifier (letter, digit, underscore)
         if (std::isalnum(XPath[before_paren]) or XPath[before_paren] IS '_' or XPath[before_paren] IS '-') {
            // Check if there's no path separator right before the identifier
            auto identifier_start = before_paren;
            while (identifier_start > 0 and (std::isalnum(XPath[identifier_start-1]) or XPath[identifier_start-1] IS '_' or XPath[identifier_start-1] IS '-')) {
               identifier_start--;
            }
            // Function calls can appear in multiple contexts:
            // 1. At start of path or after '/' (e.g., position() at root)
            // 2. Inside predicates after '[' (e.g., [position()=2])
            // 3. In expressions after operators (e.g., [count(item)>2])
            if (identifier_start IS 0 or XPath[identifier_start-1] != '/') {
               is_function_call = true;
            }
         }
      }

      if (is_function_call) {
         pf::Log log("XPath Function Call");
         log.msg("Detected function call in XPath: %.*s", int(XPath.size()), XPath.data());
         auto enhanced_result = evaluator.find_tag_enhanced(XPath, CurrentPrefix);
         log.msg("Enhanced result: %d", int(enhanced_result));
         if (enhanced_result IS ERR::Okay or enhanced_result IS ERR::Search) {
            return enhanced_result;
         }
      }
   }

   SimpleXPathEvaluator::PathInfo info;

   // Parse the path
   auto parse_result = evaluator.parse_path(XPath, info);
   if (parse_result != ERR::Okay) return parse_result;

   // Handle simple attribute-only paths like '/@attrib' (already handled in parse_path)
   if (!Attrib.empty()) return ERR::Okay;

   // Evaluate the XPath step
   return evaluator.evaluate_step(XPath, info, CurrentPrefix);
}
