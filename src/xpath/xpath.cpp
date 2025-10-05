/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-MODULE-
XPath: Provides XPath 2.0 and XQuery support for the XML module.

The XPath module provides support for XPath 2.0 and XQuery languages, in conjunction with the XML class.

-END-

Extra Features Supported:

[=...] Match on encapsulated content (Not an XPath standard but we support it)
The use of \ as an escape character in attribute strings is supported, but keep in mind that this is not an official
feature of the XPath standard.
Wildcards are legal only in the context of string comparisons, e.g. a node or attribute lookup

Examples:

  /menu/submenu
  /menu[2]/window
  /menu/window/@title
  /menu/window[@title='foo']/...
  /menu[=contentmatch]
  /menu//window
  /menu/window/ * (First child of the window tag)
  /menu/ *[@id='5']
  /root/section[@*="alpha"] (Match any attribute with value "alpha")

*********************************************************************************************************************/

#include <parasol/modules/xml.h>
#include <parasol/modules/xpath.h>
#include <parasol/strings.hpp>
#include <array>
#include <format>
#include <functional>
#include <sstream>
#include "../link/unicode.h"
#include "../xml/xml.h"
#include "xpath.h"

JUMPTABLE_CORE

#include "../xml/unescape.cpp"
#include "xpath_axis.cpp"
#include "xpath_parser.cpp"
#include "xpath_evaluator.h"
#include "xpath_evaluator.cpp"

#include "xpath_def.c"

// TODO: Replace with SetResourceMgr() when available

static void set_memory_manager(APTR Address, ResourceManager *Manager)
{
   ResourceManager **address_mgr = (ResourceManager **)((char *)Address - sizeof(int) - sizeof(int) - sizeof(ResourceManager *));
   address_mgr[0] = Manager;
}

//********************************************************************************************************************
// C++ destructor for cleaning up compiled XPath objects

static ERR xpe_free(APTR Address)
{
   ((CompiledXPath *)Address)->~CompiledXPath();
   return ERR::Okay;
}

static ResourceManager glCompiledQuery = {
   "XPathExpression",
   &xpe_free
};

//********************************************************************************************************************

static ERR xpv_free(APTR Address)
{
   ((XPathValue *)Address)->~XPathValue();
   return ERR::Okay;
}

static ResourceManager glXPVManager = {
   "XPathValue",
   &xpv_free
};

//********************************************************************************************************************

static ERR MODInit(OBJECTPTR pModule, struct CoreBase *pCore)
{
   CoreBase = pCore;
   return ERR::Okay;
}

static ERR MODExpunge(void)
{
   return ERR::Okay;
}

namespace xp {

/*********************************************************************************************************************

-FUNCTION-
Compile: Compiles an XPath or XQuery expression into an executable form.

Call the Compile function to convert a valid XPath or XQuery expression string into a compiled form that can be
executed against an XML document.  The resulting compiled expression can be reused multiple times for efficiency
and must be freed using FreeResource when no longer needed.

-INPUT-
cstr Query: A valid XPath or XQuery expression string.
!&struct(XPathExpression) Result: Receives a pointer to an CompiledXPath object on success

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

ERR Compile(CSTRING Query, XPathExpression **Result)
{
   if ((not Query) or (not Result)) return ERR::NullArgs;

   std::vector<std::string> errors;
   if (auto cmp = CompiledXPath::compile(std::string_view(Query), errors)) {
      if (AllocMemory(sizeof(CompiledXPath), MEM::DATA|MEM::MANAGED, (APTR *)&Result, nullptr) IS ERR::Okay) {
         set_memory_manager(*Result, &glCompiledQuery);
         *Result = cmp;
         return ERR::Okay;
      }
      else return ERR::AllocMemory;
   }
   else {
      // TODO Need to feed errors back to the caller.
      return ERR::Failed;
   }
}

/*********************************************************************************************************************

-FUNCTION-
Evaluate: Evaluates a compiled XPath or XQuery expression against an XML document.

Use Evaluate to run a previously compiled XPath or XQuery expression against an XML document.
The result of the evaluation is returned in the Result parameter as !XPathValue, which can represent various types 
of data including node sets, strings, numbers, or booleans.

-INPUT-
obj(XML) XML: The XML document to evaluate the query against.
struct(XPathExpression) Query: The compiled XPath or XQuery expression.
!&struct(XPathValue) Result: Receives the result of the evaluation.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

ERR Evaluate(objXML *XML, XPathExpression *Query, XPathValue **Result)
{
   pf::Log log(__FUNCTION__);

   if ((not XML) or (not Query) or (not Result)) return log.warning(ERR::NullArgs);

   auto xml = (extXML *)XML;
   
   if (xml->Tags.empty()) return log.warning(ERR::NoData);

   auto compiled_path = (CompiledXPath *)Query;

   xml->Attrib.clear();
   xml->ErrorMsg.clear();
   xml->CursorTags = &xml->Tags;
   xml->Cursor = xml->Tags.begin();

   if (AllocMemory(sizeof(XPathValue), MEM::DATA|MEM::MANAGED, (APTR *)&Result, nullptr) != ERR::Okay) {
      return ERR::AllocMemory;
   }
   
   set_memory_manager(*Result, &glXPVManager);
   
   XPathEvaluator eval(xml);
   auto err = eval.evaluate_xpath_expression(*compiled_path, *Result);
   if (err != ERR::Okay) {
      FreeResource(*Result);
      *Result = nullptr;
   }

   return err;
}

/*********************************************************************************************************************

-FUNCTION-
Query: For node-based queries, evaluates a compiled expression and calls a function for each matching node.

Use the Query function to scan an XML document for tags or attributes that match a compiled XPath or XQuery expression.
For every matching node, a user-defined callback function is invoked, allowing custom processing of each result.

If no callback is provided, the search stops after the first match and the XML object's cursor markers will reflect
the position of the node.

Note that valid function execution can return `ERR:Search` if zero matches are found.

-INPUT-
obj(XML) XML: The XML document to evaluate the query against.
ptr(struct(XPathExpression)) Query: The compiled XPath or XQuery expression.
ptr(func) Callback: Pointer to a callback function that will be called for each matching node.  Can be NULL if searching for the first matching node.

-ERRORS-
Okay: At least one matching node was found and processed.
NullArgs: At least one required parameter was not provided.
NoData: The XML document contains no data to search.
Syntax: The provided query expression has syntax errors.
Search: No matching node was found.
-END-

*********************************************************************************************************************/

ERR Query(objXML *XML, XPathExpression *Query, FUNCTION *Callback)
{
   pf::Log log(__FUNCTION__);

   if ((not XML) or (not Query)) return ERR::NullArgs;
   
   auto xml = (extXML *)XML;
   
   if (xml->Tags.empty()) return log.warning(ERR::NoData);

   if (Callback) xml->Callback = *Callback;
   else xml->Callback.Type = CALL::NIL;
   
   xml->Attrib.clear();
   xml->CursorTags = &xml->Tags;
   xml->Cursor = xml->Tags.begin();
   xml->ErrorMsg.clear();

   XPathEvaluator eval(xml);
   return eval.find_tag(*(const CompiledXPath *)Query, 0); // Returns ERR:Search if no match
}

} // namespace xp

//********************************************************************************************************************

static STRUCTS glStructures = {
   { "XPathExpression", sizeof(CompiledXPath) },
   { "XPathValue", sizeof(XPathValue) }
};

//********************************************************************************************************************

PARASOL_MOD(MODInit, nullptr, nullptr, MODExpunge, MOD_IDL, &glStructures)
extern "C" struct ModHeader * register_xpath_module() { return &ModHeader; }
