/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-MODULE-
XPath: Provides XPath 2.0 and XQuery support for the XML module.

The XPath module provides comprehensive support for XPath 2.0 and XQuery languages, enabling powerful querying and
navigation of XML documents.  It operates in conjunction with the @XML class to provide a standards-compliant query
engine with extensive functionality.

<header>XPath 2.0 Path Expressions</header>

The module supports the full XPath 2.0 specification for navigating XML documents, including all 13 standard axes
(`child`, `descendant`, `descendant-or-self`, `following`, `following-sibling`, `parent`, `ancestor`,
`ancestor-or-self`, `preceding`, `preceding-sibling`, `self`, `attribute`, and `namespace`), node tests for element
names, wildcards (`*`), and attribute selectors (`@attr`), numeric position filters (`[1]`, `[2]`), comparison
operators, and complex boolean expressions in predicates.  Both absolute paths (`/root/element`), relative paths
(`element/subelement`), and recursive descent (`//element`) are supported.

<header>XQuery Language Support</header>

The module implements core XQuery functionality including FLWOR expressions (`for`, `let`, `where`, `order by`,
and `return` clauses) for advanced querying, sequence operations for constructing, filtering, and manipulating
sequences of nodes and values, and a comprehensive type system supporting strings, numbers, booleans, node sets,
dates, durations, and QNames.

<header>Function Library</header>

A rich set of standard functions is provided across multiple categories:

<list type="bullet">
<li>Node Functions: `position()`, `last()`, `count()`, `id()`, `name()`, `local-name()`, `namespace-uri()`, `root()`, `node-name()`, `base-uri()`</li>
<li>String Functions: `concat()`, `substring()`, `contains()`, `starts-with()`, `ends-with()`, `string-length()`, `normalize-space()`, `upper-case()`, `lower-case()`, `translate()`, `string-join()`, `encode-for-uri()`, `escape-html-uri()`</li>
<li>Numeric Functions: `number()`, `sum()`, `floor()`, `ceiling()`, `round()`, `round-half-to-even()`, `abs()`, `min()`, `max()`, `avg()`</li>
<li>Boolean Functions: `boolean()`, `not()`, `true()`, `false()`, `exists()`, `empty()`, `lang()`</li>
<li>Sequence Functions: `distinct-values()`, `index-of()`, `insert-before()`, `remove()`, `reverse()`, `subsequence()`, `unordered()`, `deep-equal()`, `zero-or-one()`, `one-or-more()`, `exactly-one()`</li>
<li>Regular Expressions: `matches()`, `replace()`, `tokenize()`, `analyze-string()`</li>
<li>Date and Time Functions: `current-date()`, `current-time()`, `current-dateTime()`, date and time component extractors, timezone adjustments, duration calculations</li>
<li>Document Functions: `doc()`, `doc-available()`, `collection()`, `unparsed-text()`, `unparsed-text-lines()`, `document-uri()`</li>
<li>QName Functions: `QName()`, `resolve-QName()`, `prefix-from-QName()`, `local-name-from-QName()`, `namespace-uri-from-QName()`, `namespace-uri-for-prefix()`, `in-scope-prefixes()`</li>
<li>URI Functions: `resolve-uri()`, `iri-to-uri()`</li>
<li>Formatting Functions: `format-date()`, `format-time()`, `format-dateTime()`, `format-integer()`</li>
<li>Utility Functions: `error()`, `trace()`</li>
</list>

<header>Expression Compilation</header>

XPath and XQuery expressions are compiled into an optimised internal representation for efficient reuse.  Compiled
expressions are thread-safe and can be shared across multiple XML documents.  The compilation step validates syntax
and reports detailed error messages.  Compiled expressions are managed as resources and can be freed when no longer
needed.

<header>Evaluation Modes</header>

The module provides two distinct modes for query evaluation.  Value evaluation returns typed results (&XPathValue)
that can represent node sets, strings, numbers, booleans, dates, or sequences.  Node iteration invokes a callback
function for each matching node, enabling streaming processing of large result sets.

<header>Usage Patterns</header>

Compiling and evaluating queries:

<pre>
APTR query;
if (xp::Compile(xml, "/bookstore/book[@price < 10]/title", &query) IS ERR::Okay) {
   XPathValue *result;
   if (xp::Evaluate(xml, query, &result) IS ERR::Okay) {
      // Process result...
      FreeResource(result);
   }
   FreeResource(query);
}
</pre>

Node iteration with callbacks:

<pre>
APTR query;
if (xp::Compile(xml, "//chapter[@status='draft']", &query) IS ERR::Okay) {
   auto callback = C_FUNCTION(process_node);
   xp::Query(xml, query, &callback);
   FreeResource(query);
}
</pre>

<header>Extensions</header>

The module includes several Parasol-specific extensions beyond the standard specification.  Content matching with the
`[=...]` syntax allows matching on encapsulated content, e.g., `/menu[=contentmatch]`.  Backslash (`\`) can be used as
an escape character in attribute strings.  The `@*` syntax matches any attribute, e.g., `/root/section[@*="alpha"]`.

-END-

Examples:

  /menu/window[@title='foo']/...
  /menu[=contentmatch]
  /menu//window
  /menu/window/ * (First child of the window tag)
  /menu/ *[@id='5']
  /root/section[@*="alpha"] (Match any attribute with value "alpha")

*********************************************************************************************************************/

#include <parasol/modules/xml.h>
#include <parasol/modules/xpath.h>
#include <parasol/modules/regex.h>
#include <parasol/strings.hpp>
#include <array>
#include <format>
#include <functional>
#include <sstream>
#include <memory>
#include <utility>
#include <string>
#include "../link/unicode.h"
#include "../xml/uri_utils.h"
#include "../xml/xml.h"
#include "functions/accessor_support.h"
#include "api/xquery_prolog.h"
#include "parse/xpath_tokeniser.h"
#include "parse/xpath_parser.h"
#include "eval/eval.h"
#include "xpath.h"

JUMPTABLE_CORE
JUMPTABLE_REGEX

#include "xpath_def.c"

static OBJECTPTR glContext = nullptr;
static OBJECTPTR modRegex = nullptr;

//*********************************************************************************************************************
// Dynamic loader for the Regex functionality.  We only load it as needed due to the size of the module.

extern "C" ERR load_regex(void)
{
#ifndef PARASOL_STATIC
   if (not modRegex) {
      pf::SwitchContext ctx(glContext);
      if (objModule::load("regex", &modRegex, &RegexBase) != ERR::Okay) return ERR::InitModule;
   }
#endif
   return ERR::Okay;
}

//********************************************************************************************************************
// C++ destructor for cleaning up compiled XPath objects

static ERR xpnode_free(APTR Address)
{
   ((XPathParseResult *)Address)->~XPathParseResult();
   return ERR::Okay;
}

static ResourceManager glNodeManager = {
   "XPathParseResult",
   &xpnode_free
};

//********************************************************************************************************************

static ERR xpv_free(APTR Address)
{
   ((XPathVal *)Address)->~XPathVal();
   return ERR::Okay;
}

static ResourceManager glXPVManager = {
   "XPathValue",
   &xpv_free
};

//********************************************************************************************************************

#ifdef ENABLE_UNIT_TESTS
#include "unit_tests.cpp"
#endif

//********************************************************************************************************************

static ERR MODInit(OBJECTPTR pModule, struct CoreBase *pCore)
{
   CoreBase = pCore;
   glContext = CurrentContext();
   return ERR::Okay;
}

static ERR MODOpen(OBJECTPTR Module)
{
   Module->set(FID_FunctionList, glFunctions);
   return ERR::Okay;
}

static ERR MODExpunge(void)
{
   if (modRegex) { FreeResource(modRegex); modRegex = nullptr; }
   return ERR::Okay;
}

namespace xp {

/*********************************************************************************************************************

-FUNCTION-
Compile: Compiles an XQuery expressions into an executable form.

Call the Compile function to convert a valid XQuery expression string into a compiled form that can be
executed against an XML document.  The resulting compiled expression can be reused multiple times for efficiency
and must be freed using FreeResource when no longer needed.  They are re-usable between different XML documents and
are treated as read-only for thread-safety.

The XML parameter is not required for compilation, but can potentially enhance syntax checking when parsing the
expression.  An additional benefit is that error messages will be defined in the #ErrorMsg field of the XML object if
parsing fails.

Note: This function can hang temporarily if the expression references network URIs.  Consider calling it from a
separate thread to avoid blocking in such cases.

-INPUT-
obj(XML) XML: The XML document context for the query (can be NULL if not needed).
cstr Query: A valid XQuery / XPath expression string.
!&ptr Result: Receives a pointer to an XPathNode object on success.

-ERRORS-
Okay
NullArgs
Syntax
AllocMemory
-END-

*********************************************************************************************************************/

ERR Compile(objXML *XML, CSTRING Query, APTR *Result)
{
   pf::Log log(__FUNCTION__);

   if ((not Query) or (not Result)) return ERR::NullArgs;

   *Result = nullptr;

   int len = 0;
   while ((Query[len]) and (Query[len] != '\n')) len++;
   log.branch("XML: %d, Query: %.*s", XML ? XML->UID : 0, len, Query);

   XPathParseResult *node;
   if (AllocMemory(sizeof(XPathParseResult), MEM::MANAGED, (APTR *)&node, nullptr) IS ERR::Okay) {
      SetResourceMgr(node, &glNodeManager);

      // Construct the result object in-place before assigning into it
      new (node) XPathParseResult();

      XPathTokeniser tokeniser;
      XPathParser parser;

      auto tokens = tokeniser.tokenize(Query);
      auto parsed = parser.parse(tokens);
      *node = std::move(parsed);

      auto prolog = node->prolog;

      if ((prolog) and (prolog->is_library_module)) {
         // XQuery module detected - empty result is normal
         // Synthesise an empty-sequence expression node so downstream code has a valid AST.
         log.msg("XQuery module compiled");
         if (not node->expression) node->expression = std::make_unique<XPathNode>(XPathNodeType::EMPTY_SEQUENCE);
      }
      else if (not node->expression) {
         auto parser_errors = parser.get_errors();
         std::string message;

         if (parser_errors.empty()) message = "Failed to parse XQuery expression";
         else {
            for (const auto &err : parser_errors) {
               if (not message.empty()) message += "; ";
               message += err;
            }
         }

         if (XML) ((extXML *)XML)->ErrorMsg = message;
         if (not message.empty()) log.warning("%s", message.c_str());
         FreeResource(node);
         return ERR::Syntax;
      }

      // If the expression featured an XQuery prolog then attach it to the parse result only.
      // Evaluator reads from the parse context; do not mutate the AST.

      // Move the module cache across if one was created during parsing.

      std::shared_ptr<XQueryModuleCache> module_cache = node->module_cache;
      if (not module_cache) {
         module_cache = std::make_shared<XQueryModuleCache>();
         // An XML object is required for module importation to work correctly.
         if (XML) module_cache->owner = XML->UID;
      }

      if (module_cache) {
         // Retain on the result only; evaluator uses parse-context.
         node->module_cache = module_cache;
         if (prolog) prolog->bind_module_cache(module_cache);
      }

      *Result = node;
      return ERR::Okay;
   }
   else return ERR::AllocMemory;
}

/*********************************************************************************************************************

-FUNCTION-
Evaluate: Evaluates a compiled XQuery expression against an XML document.

Use Evaluate to run a previously compiled XQuery expression against an XML document.  The result of the
evaluation is returned in the Result parameter as !XPathValue, which can represent various types of data including
node sets, strings, numbers, or booleans.

-INPUT-
obj(XML) XML: The XML document to evaluate the query against.
ptr Query: The compiled XQuery / XPath expression.
!&struct(XPathValue) Result: Receives the result of the evaluation.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

ERR Evaluate(objXML *XML, APTR Query, XPathValue **Result)
{
   pf::Log log(__FUNCTION__);

   if ((not XML) or (not Query) or (not Result)) return log.warning(ERR::NullArgs);

   auto xml = (extXML *)XML;
   *Result = nullptr;

   if (xml->Tags.empty()) return log.warning(ERR::NoData);

   auto query = (XPathParseResult *)Query;

   xml->Attrib.clear();
   xml->ErrorMsg.clear();
   xml->CursorTags = &xml->Tags;
   xml->Cursor = xml->Tags.begin();

   XPathVal *xpv;
   if (AllocMemory(sizeof(XPathVal), MEM::DATA|MEM::MANAGED, (APTR *)&xpv, nullptr) IS ERR::Okay) {
      SetResourceMgr(xpv, &glXPVManager);
      new (xpv) XPathVal(); // Placement new to construct a dummy XPathVal object
      // No longer mutate the AST; evaluator is given parse context directly.

      XPathEvaluator eval(xml, query->expression.get(), query);
      auto err = eval.evaluate_xpath_expression(*(query->expression.get()), (XPathVal *)xpv);
      if (err != ERR::Okay) {
         FreeResource(xpv);
         log.warning("%s", xml->ErrorMsg.c_str());
      }
      else *Result = xpv;
      return err;
   }
   else return ERR::AllocMemory;
}

/*********************************************************************************************************************

-FUNCTION-
Query: For node-based queries, evaluates a compiled expression and calls a function for each matching node.

Use the Query function to scan an XML document for tags or attributes that match a compiled XQuery expression.
For every matching node, a user-defined callback function is invoked, allowing custom processing of each result.

If no callback is provided, the search stops after the first match and the @XML object's cursor markers will reflect
the position of the node.

Note that valid function execution can return `ERR:Search` if zero matches are found.

-INPUT-
obj(XML) XML: The XML document to evaluate the query against.
ptr Query: The compiled XQuery / XPath expression.
ptr(func) Callback: Pointer to a callback function that will be called for each matching node.  Can be NULL if searching for the first matching node.

-ERRORS-
Okay: At least one matching node was found and processed.
NullArgs: At least one required parameter was not provided.
NoData: The XML document contains no data to search.
Syntax: The provided query expression has syntax errors.
Search: No matching node was found.
-END-

*********************************************************************************************************************/

ERR Query(objXML *XML, APTR Query, FUNCTION *Callback)
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
   (void)xml->getMap(); // Ensure the tag ID and ParentID values are defined
   
   auto query = (XPathParseResult *)Query;
   // No longer mutate the AST; evaluator is given parse context directly.

   XPathEvaluator eval(xml, ((XPathParseResult *)Query)->expression.get(), (XPathParseResult *)Query);
   return eval.find_tag(*((XPathParseResult *)Query)->expression.get(), 0); // Returns ERR:Search if no match
}

/*********************************************************************************************************************

-FUNCTION-
UnitTest: Private function for internal unit testing of the XPath module.

Private function for internal unit testing of the XPath module.

-INPUT-
ptr Meta: Optional pointer meaningful to the test functions.

-ERRORS-
Okay: All tests passed.
Failed: One or more tests failed.

-END-

*********************************************************************************************************************/

ERR UnitTest(APTR Meta)
{
#ifdef ENABLE_UNIT_TESTS
   return run_unit_tests(Meta);
#else
   return ERR::Okay;
#endif
}

} // namespace xp

//********************************************************************************************************************

static STRUCTS glStructures = {
};

//********************************************************************************************************************

PARASOL_MOD(MODInit, nullptr, MODOpen, MODExpunge, MOD_IDL, &glStructures)
extern "C" struct ModHeader * register_xpath_module() { return &ModHeader; }
