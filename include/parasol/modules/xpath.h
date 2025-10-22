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
   CAST_EXPRESSION = 9,
   CONDITIONAL = 10,
   FOR_EXPRESSION = 11,
   FOR_BINDING = 12,
   LET_EXPRESSION = 13,
   LET_BINDING = 14,
   FLWOR_EXPRESSION = 15,
   WHERE_CLAUSE = 16,
   GROUP_CLAUSE = 17,
   GROUP_KEY = 18,
   ORDER_CLAUSE = 19,
   ORDER_SPEC = 20,
   COUNT_CLAUSE = 21,
   QUANTIFIED_EXPRESSION = 22,
   QUANTIFIED_BINDING = 23,
   FUNCTION_CALL = 24,
   LITERAL = 25,
   VARIABLE_REFERENCE = 26,
   NAME_TEST = 27,
   NODE_TYPE_TEST = 28,
   PROCESSING_INSTRUCTION_TEST = 29,
   WILDCARD = 30,
   AXIS_SPECIFIER = 31,
   UNION = 32,
   NUMBER = 33,
   STRING = 34,
   PATH = 35,
   DIRECT_ELEMENT_CONSTRUCTOR = 36,
   DIRECT_ATTRIBUTE_CONSTRUCTOR = 37,
   DIRECT_TEXT_CONSTRUCTOR = 38,
   COMPUTED_ELEMENT_CONSTRUCTOR = 39,
   COMPUTED_ATTRIBUTE_CONSTRUCTOR = 40,
   TEXT_CONSTRUCTOR = 41,
   COMMENT_CONSTRUCTOR = 42,
   PI_CONSTRUCTOR = 43,
   DOCUMENT_CONSTRUCTOR = 44,
   CONSTRUCTOR_CONTENT = 45,
   ATTRIBUTE_VALUE_TEMPLATE = 46,
   EMPTY_SEQUENCE = 47,
   TREAT_AS_EXPRESSION = 48,
   CASTABLE_EXPRESSION = 49,
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
   ERR (*_UnitTest)(APTR Meta);
#endif // PARASOL_STATIC
};

#if !defined(PARASOL_STATIC) and !defined(PRV_XPATH_MODULE)
extern struct XPathBase *XPathBase;
namespace xp {
inline ERR Compile(objXML *XML, CSTRING Query, APTR *Result) { return XPathBase->_Compile(XML,Query,Result); }
inline ERR Evaluate(objXML *XML, APTR Query, struct XPathValue **Result) { return XPathBase->_Evaluate(XML,Query,Result); }
inline ERR Query(objXML *XML, APTR Query, FUNCTION *Callback) { return XPathBase->_Query(XML,Query,Callback); }
inline ERR UnitTest(APTR Meta) { return XPathBase->_UnitTest(Meta); }
} // namespace
#else
namespace xp {
extern ERR Compile(objXML *XML, CSTRING Query, APTR *Result);
extern ERR Evaluate(objXML *XML, APTR Query, struct XPathValue **Result);
extern ERR Query(objXML *XML, APTR Query, FUNCTION *Callback);
extern ERR UnitTest(APTR Meta);
} // namespace
#endif // PARASOL_STATIC

