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
   QUANTIFIED_EXPRESSION = 15,
   QUANTIFIED_BINDING = 16,
   FUNCTION_CALL = 17,
   LITERAL = 18,
   VARIABLE_REFERENCE = 19,
   NAME_TEST = 20,
   NODE_TYPE_TEST = 21,
   PROCESSING_INSTRUCTION_TEST = 22,
   WILDCARD = 23,
   AXIS_SPECIFIER = 24,
   UNION = 25,
   NUMBER = 26,
   STRING = 27,
   PATH = 28,
   DIRECT_ELEMENT_CONSTRUCTOR = 29,
   DIRECT_ATTRIBUTE_CONSTRUCTOR = 30,
   DIRECT_TEXT_CONSTRUCTOR = 31,
   COMPUTED_ELEMENT_CONSTRUCTOR = 32,
   COMPUTED_ATTRIBUTE_CONSTRUCTOR = 33,
   TEXT_CONSTRUCTOR = 34,
   COMMENT_CONSTRUCTOR = 35,
   PI_CONSTRUCTOR = 36,
   DOCUMENT_CONSTRUCTOR = 37,
   CONSTRUCTOR_CONTENT = 38,
   ATTRIBUTE_VALUE_TEMPLATE = 39,
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

   XPathNodeType type;
   std::string value;
   std::vector<std::unique_ptr<XPathNode>> children;
   std::optional<XPathConstructorInfo> constructor_info;
   std::vector<XPathAttributeValuePart> attribute_value_parts;
   bool attribute_value_has_expressions = false;
   std::unique_ptr<XPathNode> name_expression;

   XPathNode(XPathNodeType t, std::string v = "") : type(t), value(std::move(v)) {}

   void add_child(std::unique_ptr<XPathNode> child) { children.push_back(std::move(child)); }
   [[nodiscard]] XPathNode * get_child(size_t index) const { return index < children.size() ? children[index].get() : nullptr; }
   [[nodiscard]] size_t child_count() const { return children.size(); }

   void set_constructor_info(XPathConstructorInfo info)
   {
      constructor_info = std::move(info);
   }

   [[nodiscard]] bool has_constructor_info() const
   {
      return constructor_info.has_value();
   }

   void set_attribute_value_parts(std::vector<XPathAttributeValuePart> parts)
   {
      attribute_value_has_expressions = false;
      for (const auto &part : parts)
      {
         if (part.is_expression)
         {
            attribute_value_has_expressions = true;
            break;
         }
      }
      attribute_value_parts = std::move(parts);
   }

   void set_name_expression(std::unique_ptr<XPathNode> expr)
   {
      name_expression = std::move(expr);
   }

   [[nodiscard]] XPathNode * get_name_expression() const
   {
      return name_expression.get();
   }

   [[nodiscard]] bool has_name_expression() const
   {
      return name_expression != nullptr;
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

