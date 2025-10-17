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

#ifdef PARASOL_STATIC
#define JUMPTABLE_XPATH [[maybe_unused]] static struct XPathBase *XPathBase = nullptr;
#else
#define JUMPTABLE_XPATH struct XPathBase *XPathBase = nullptr;
#endif

struct XPathBase {
#ifndef PARASOL_STATIC
   ERR (*_Compile)(objXML *XML, CSTRING Query, APTR *Result);
   ERR (*_Evaluate)(objXML *XML, APTR Query, struct XPathValue **Result);
   ERR (*_Query)(objXML *XML, APTR Query, FUNCTION *Callback);
   void (*_UnitTest)(APTR Meta);
#endif // PARASOL_STATIC
};

#ifndef PRV_XPATH_MODULE
#ifndef PARASOL_STATIC
extern struct XPathBase *XPathBase;
namespace xp {
inline ERR Compile(objXML *XML, CSTRING Query, APTR *Result) { return XPathBase->_Compile(XML,Query,Result); }
inline ERR Evaluate(objXML *XML, APTR Query, struct XPathValue **Result) { return XPathBase->_Evaluate(XML,Query,Result); }
inline ERR Query(objXML *XML, APTR Query, FUNCTION *Callback) { return XPathBase->_Query(XML,Query,Callback); }
inline void UnitTest(APTR Meta) { return XPathBase->_UnitTest(Meta); }
} // namespace
#else
namespace xp {
extern ERR Compile(objXML *XML, CSTRING Query, APTR *Result);
extern ERR Evaluate(objXML *XML, APTR Query, struct XPathValue **Result);
extern ERR Query(objXML *XML, APTR Query, FUNCTION *Callback);
extern void UnitTest(APTR Meta);
} // namespace
#endif // PARASOL_STATIC
#endif

