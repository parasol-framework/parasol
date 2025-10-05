#pragma once

// Name:      xpath.h
// Copyright: Paul Manias © 2025
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_XPATH (1)

#ifdef __cplusplus
#include <functional>
#include <sstream>
#ifndef STRINGS_HPP
#include <parasol/strings.hpp>
#endif

#endif

enum class XPVT : int {
   NIL = 0,
   NodeSet = 0,
   Boolean = 1,
   Number = 2,
   String = 3,
   Date = 4,
   Time = 5,
   DateTime = 6,
};

typedef struct XPathExpression {
   std::string original_expression;
} XPATHEXPRESSION;

#ifdef PARASOL_STATIC
#define JUMPTABLE_XPATH static struct XPathBase *XPathBase;
#else
#define JUMPTABLE_XPATH struct XPathBase *XPathBase;
#endif

struct XPathBase {
#ifndef PARASOL_STATIC
   ERR (*_Compile)(CSTRING Query, struct XPathExpression **Result);
   ERR (*_Evaluate)(objXML *XML, struct XPathExpression *Query, struct XPathValue **Result);
   ERR (*_Query)(objXML *XML, struct XPathExpression *Query, FUNCTION *Callback);
#endif // PARASOL_STATIC
};

#ifndef PRV_XPATH_MODULE
#ifndef PARASOL_STATIC
extern struct XPathBase *XPathBase;
namespace xp {
inline ERR Compile(CSTRING Query, struct XPathExpression **Result) { return XPathBase->_Compile(Query,Result); }
inline ERR Evaluate(objXML *XML, struct XPathExpression *Query, struct XPathValue **Result) { return XPathBase->_Evaluate(XML,Query,Result); }
inline ERR Query(objXML *XML, struct XPathExpression *Query, FUNCTION *Callback) { return XPathBase->_Query(XML,Query,Callback); }
} // namespace
#else
namespace xp {
extern ERR Compile(CSTRING Query, struct XPathExpression **Result);
extern ERR Evaluate(objXML *XML, struct XPathExpression *Query, struct XPathValue **Result);
extern ERR Query(objXML *XML, struct XPathExpression *Query, FUNCTION *Callback);
} // namespace
#endif // PARASOL_STATIC
#endif

