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
#include "parse/xpath_tokeniser.h"
#include "eval/eval.h"
#include "xpath.h"

JUMPTABLE_CORE
JUMPTABLE_REGEX

#include "xpath_def.c"

static OBJECTPTR glContext = nullptr;
static OBJECTPTR modRegex = nullptr;
static OBJECTPTR clXQuery = nullptr;

static ERR add_xquery_class(void);

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

#ifdef ENABLE_UNIT_TESTS
#include "unit_tests.cpp"
#endif

//********************************************************************************************************************

static ERR MODInit(OBJECTPTR pModule, struct CoreBase *pCore)
{
   CoreBase = pCore;
   glContext = CurrentContext();
   return add_xquery_class();
}

static ERR MODOpen(OBJECTPTR Module)
{
   Module->set(FID_FunctionList, glFunctions);
   return ERR::Okay;
}

static ERR MODExpunge(void)
{
   if (clXQuery) { FreeResource(clXQuery); clXQuery = nullptr; }
   if (modRegex) { FreeResource(modRegex); modRegex = nullptr; }
   return ERR::Okay;
}

namespace xp {

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

#include "xquery_class.cpp"

//********************************************************************************************************************

static STRUCTS glStructures = {
};

//********************************************************************************************************************

PARASOL_MOD(MODInit, nullptr, MODOpen, MODExpunge, MOD_IDL, &glStructures)
extern "C" struct ModHeader * register_xpath_module() { return &ModHeader; }
