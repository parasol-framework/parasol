#pragma once

// Name:      xpath.h
// Copyright: Paul Manias © 2025
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_XPATH (1)

#include <parasol/modules/xml.h>

#ifdef __cplusplus
#include <functional>
#include <optional>
#include <sstream>
#ifndef STRINGS_HPP
#include <parasol/strings.hpp>
#endif

#endif

enum class XPathNodeType : int {
   NIL = 0,
   LOCATION_PATH = 0,
   STEP = 1,
   NODE_TEST = 2,
   PREDICATE = 3,
   ROOT = 4,
   EXPRESSION = 5,
   FILTER = 6,
   BINARY_OP = 7,
   UNARY_OP = 8,
   CONDITIONAL = 9,
   FOR_EXPRESSION = 10,
   FOR_BINDING = 11,
   LET_EXPRESSION = 12,
   LET_BINDING = 13,
   FLWOR_EXPRESSION = 14,
   WHERE_CLAUSE = 15,
   GROUP_CLAUSE = 16,
   GROUP_KEY = 17,
   ORDER_CLAUSE = 18,
   ORDER_SPEC = 19,
   COUNT_CLAUSE = 20,
   QUANTIFIED_EXPRESSION = 21,
   QUANTIFIED_BINDING = 22,
   FUNCTION_CALL = 23,
   LITERAL = 24,
   VARIABLE_REFERENCE = 25,
   NAME_TEST = 26,
   NODE_TYPE_TEST = 27,
   PROCESSING_INSTRUCTION_TEST = 28,
   WILDCARD = 29,
   AXIS_SPECIFIER = 30,
   UNION = 31,
   NUMBER = 32,
   STRING = 33,
   PATH = 34,
   DIRECT_ELEMENT_CONSTRUCTOR = 35,
   DIRECT_ATTRIBUTE_CONSTRUCTOR = 36,
   DIRECT_TEXT_CONSTRUCTOR = 37,
   COMPUTED_ELEMENT_CONSTRUCTOR = 38,
   COMPUTED_ATTRIBUTE_CONSTRUCTOR = 39,
   TEXT_CONSTRUCTOR = 40,
   COMMENT_CONSTRUCTOR = 41,
   PI_CONSTRUCTOR = 42,
   DOCUMENT_CONSTRUCTOR = 43,
   CONSTRUCTOR_CONTENT = 44,
   ATTRIBUTE_VALUE_TEMPLATE = 45,
};

typedef struct XPathNode {
   struct XPathAttributeValuePart
   {
      bool is_expression = false;
      std::string text;
   };

   struct XPathConstructorAttribute
   {
      std::string prefix;
      std::string name;
      std::string namespace_uri;
      bool is_namespace_declaration = false;
      std::vector<XPathAttributeValuePart> value_parts;
      std::vector<std::unique_ptr<XPathNode>> expression_parts;

      void set_expression_for_part(size_t index, std::unique_ptr<XPathNode> expr)
      {
         if (expression_parts.size() <= index) expression_parts.resize(index + 1);
         expression_parts[index] = std::move(expr);
      }

      [[nodiscard]] XPathNode * get_expression_for_part(size_t index) const
      {
         return index < expression_parts.size() ? expression_parts[index].get() : nullptr;
      }
   };

   struct XPathConstructorInfo
   {
      std::string prefix;
      std::string name;
      std::string namespace_uri;
      bool is_empty_element = false;
      bool is_direct = false;
      std::vector<XPathConstructorAttribute> attributes;
   };

   struct XPathOrderSpecOptions
   {
      bool is_descending = false;
      bool has_empty_mode = false;
      bool empty_is_greatest = false;
      std::string collation_uri;

      [[nodiscard]] bool has_collation() const
      {
         return !collation_uri.empty();
      }
   };

   struct XPathGroupKeyInfo
   {
      std::string variable_name;

      [[nodiscard]] bool has_variable() const
      {
         return !variable_name.empty();
      }
   };

   XPathNodeType type;
   std::string value;
   std::vector<std::unique_ptr<XPathNode>> children;
   std::optional<XPathConstructorInfo> constructor_info;
   std::vector<XPathAttributeValuePart> attribute_value_parts;
   bool attribute_value_has_expressions = false;
   std::unique_ptr<XPathNode> name_expression;
   bool order_clause_is_stable = false;
   std::optional<XPathOrderSpecOptions> order_spec_options;
   std::optional<XPathGroupKeyInfo> group_key_info;

   XPathNode(XPathNodeType t, std::string v = "") : type(t), value(std::move(v)) {}

   inline void add_child(std::unique_ptr<XPathNode> child) { children.push_back(std::move(child)); }
   [[nodiscard]] XPathNode * get_child(size_t index) const { return index < children.size() ? children[index].get() : nullptr; }
   [[nodiscard]] size_t child_count() const { return children.size(); }
   inline void set_constructor_info(XPathConstructorInfo info) { constructor_info = std::move(info); }
   [[nodiscard]] bool has_constructor_info() const { return constructor_info.has_value(); }
   inline void set_name_expression(std::unique_ptr<XPathNode> expr) { name_expression = std::move(expr); }
   [[nodiscard]] XPathNode * get_name_expression() const { return name_expression.get(); }
   [[nodiscard]] bool has_name_expression() const { return name_expression != nullptr; }
   inline void set_group_key_info(XPathGroupKeyInfo Info) { group_key_info = std::move(Info); }
   [[nodiscard]] bool has_group_key_info() const { return group_key_info.has_value(); }
   [[nodiscard]] const XPathGroupKeyInfo * get_group_key_info() const { return group_key_info ? &(*group_key_info) : nullptr; }

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
} XPATHNODE;

#ifdef PARASOL_STATIC
#define JUMPTABLE_XPATH [[maybe_unused]] static struct XPathBase *XPathBase = nullptr;
#else
#define JUMPTABLE_XPATH struct XPathBase *XPathBase = nullptr;
#endif

struct XPathBase {
#ifndef PARASOL_STATIC
   ERR (*_Compile)(objXML *XML, CSTRING Query, struct XPathNode **Result);
   ERR (*_Evaluate)(objXML *XML, struct XPathNode *Query, struct XPathValue **Result);
   ERR (*_Query)(objXML *XML, struct XPathNode *Query, FUNCTION *Callback);
#endif // PARASOL_STATIC
};

#ifndef PRV_XPATH_MODULE
#ifndef PARASOL_STATIC
extern struct XPathBase *XPathBase;
namespace xp {
inline ERR Compile(objXML *XML, CSTRING Query, struct XPathNode **Result) { return XPathBase->_Compile(XML,Query,Result); }
inline ERR Evaluate(objXML *XML, struct XPathNode *Query, struct XPathValue **Result) { return XPathBase->_Evaluate(XML,Query,Result); }
inline ERR Query(objXML *XML, struct XPathNode *Query, FUNCTION *Callback) { return XPathBase->_Query(XML,Query,Callback); }
} // namespace
#else
namespace xp {
extern ERR Compile(objXML *XML, CSTRING Query, struct XPathNode **Result);
extern ERR Evaluate(objXML *XML, struct XPathNode *Query, struct XPathValue **Result);
extern ERR Query(objXML *XML, struct XPathNode *Query, FUNCTION *Callback);
} // namespace
#endif // PARASOL_STATIC
#endif

