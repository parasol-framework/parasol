#pragma once

// Name:      xpath.h
// Copyright: Paul Manias © 2025
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_XPATH (1)

#include <parasol/modules/xml.h>

#ifdef __cplusplus
#include <functional>
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
};

typedef struct XPathNode {
   XPathNodeType type;
   std::string value;
   std::vector<std::unique_ptr<XPathNode>> children;

   XPathNode(XPathNodeType t, std::string v = "") : type(t), value(std::move(v)) {}

   void add_child(std::unique_ptr<XPathNode> child) { children.push_back(std::move(child)); }
   [[nodiscard]] XPathNode * get_child(size_t index) const { return index < children.size() ? children[index].get() : nullptr; }
   [[nodiscard]] size_t child_count() const { return children.size(); }
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

