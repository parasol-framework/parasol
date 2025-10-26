/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
XQuery: Provides an interface for XQuery evaluation and execution.

The XQuery class provides comprehensive support for executing XPath 2.0 and XQuery expressions, enabling navigation 
of XML documents.  It operates in conjunction with the @XML class to provide a standards-compliant query
engine with extensive functionality.

<header>XPath 2.0 Path Expressions</>

The class supports the full XPath 2.0 specification for navigating XML documents, including all 13 standard axes
(`child`, `descendant`, `descendant-or-self`, `following`, `following-sibling`, `parent`, `ancestor`,
`ancestor-or-self`, `preceding`, `preceding-sibling`, `self`, `attribute`, and `namespace`), node tests for element
names, wildcards (`*`), and attribute selectors (`@attr`), numeric position filters (`[1]`, `[2]`), comparison
operators, and complex boolean expressions in predicates.  Both absolute paths (`/root/element`), relative paths
(`element/subelement`), and recursive descent (`//element`) are supported.

<header>XQuery Language Support</>

The class implements core XQuery 1.0 functionality including FLWOR expressions (`for`, `let`, `where`, `order by`,
and `return` clauses) for advanced querying, sequence operations for constructing, filtering, and manipulating
sequences of nodes and values, and a comprehensive type system supporting strings, numbers, booleans, node sets,
dates, durations, and QNames.

Informal support for XQuery 2.0 functionality is also included but the feature-set is not yet complete.

<header>Function Library</>

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

<header>Expression Compilation</>

XPath and XQuery expressions are compiled into an optimised internal representation for efficient reuse.  Expressions
can be run in their own thread, with the result available in #Result and #ResultString on completion, but the targeted XML
object will be locked for the duration of the query.

<header>Evaluation Modes</>

There are two distinct methods for query evaluation.  Value evaluation returns typed results (&XPathValue)
that can represent node sets, strings, numbers, booleans, dates, or sequences.  Node iteration invokes a callback
function for each matching node, enabling streaming processing of large result sets.

<header>Usage Patterns</>

Compiling and evaluating queries:

<pre>
objXQuery::create query { statement="/bookstore/book[@price < 10]/title" };
if (query.ok()) {
   XPathValue *result;
   if (query->evaluate(xml) IS ERR::Okay) {
      log.msg("Got: %s", query->get<CSTRING>(FID_ResultString));
   }
}
</pre>

Node iteration with callbacks:

<pre>
objXQuery::create query { statement="//chapter[@status='draft']" };
if (query.ok()) {
   auto callback = C_FUNCTION(process_node);
   query->search(xml, &callback);
}
</pre>

<header>Extensions</>

The module includes several Parasol-specific extensions beyond the standard specification.  Content matching with the
`[=...]` syntax allows matching on encapsulated content, e.g., `/menu[=contentmatch]`.  Backslash (`\`) can be used as
an escape character in attribute strings.

-END-

TODO:

* Use GetKey() and SetKey() for defining variables in the query context.
* Add support for custom functions via a new method, e.g., RegisterFunction().
* Add DeclareNamespace(Prefix, URI) method to define namespaces for use in queries.
* Provide ListVariables() and ListFunctions() methods to enumerate available variables and functions in the compiled query.
* Allow modules to be preloaded - this would mean loading the module as a separate XQuery and adding it via a new method.
  Alternatively we could provide a callback that is invoked when an import is encountered to allow the host application to supply
  the XQuery module.
* Could define an ExpectedResult field that defines an XPathValueType and throws an error if the result does not match.

*********************************************************************************************************************/

static ERR build_query(extXQuery *Self)
{
   pf::Log log;

   Self->StaleBuild = false;

   if (Self->Statement.empty()) {
      Self->ErrorMsg = "Statement field undefined";
      return log.warning(ERR::FieldNotSet);
   }

   Self->ErrorMsg.clear();

   XPathTokeniser tokeniser;
   XPathParser parser;

   auto tokens = tokeniser.tokenize(Self->Statement);
   Self->ParseResult = parser.parse(tokens);

   if ((Self->ParseResult.prolog) and (Self->ParseResult.prolog->is_library_module)) {
      // XQuery module detected - empty result is normal
      // Synthesise an empty-sequence expression node so downstream code has a valid AST.
      log.msg("XQuery module compiled");
      if (not Self->ParseResult.expression) Self->ParseResult.expression = std::make_unique<XPathNode>(XPathNodeType::EMPTY_SEQUENCE);
   }
   else if (not Self->ParseResult.expression) {
      auto parser_errors = parser.get_errors();
      if (parser_errors.empty()) Self->ErrorMsg = "Failed to parse XQuery expression";
      else {
         for (const auto &err : parser_errors) {
            if (not Self->ErrorMsg.empty()) Self->ErrorMsg += "; ";
            Self->ErrorMsg += err;
         }
      }

      log.warning("%s", Self->ErrorMsg.c_str());
      return ERR::Syntax;
   }

   // If the expression featured an XQuery prolog then attach it to the parse result only.
   // Evaluator reads from the parse context; do not mutate the AST.

   std::shared_ptr<XQueryModuleCache> module_cache = Self->ParseResult.module_cache;
   if (not module_cache) {
      module_cache = std::make_shared<XQueryModuleCache>();
      module_cache->query = Self;
      Self->ParseResult.module_cache = module_cache;
   }

   if (Self->ParseResult.prolog) Self->ParseResult.prolog->bind_module_cache(module_cache);

   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
Clear: Completely clears all XQuery data and resets the object to its initial state.

Use Clear() to remove the resources consumed by the XQuery while still retaining it for future use.

*********************************************************************************************************************/

static ERR XQUERY_Clear(extXQuery *Self)
{
   Self->ErrorMsg.clear();
   Self->ParseResult.prolog.reset();
   Self->ParseResult.expression.reset();
   Self->ResultString.clear();
   Self->Result = XPathVal();
   Self->StaleBuild = true;
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
Evaluate: Run an XQuery expression against an XML document.

Use Evaluate to run a compiled XQuery expression against an XML document.  The result of the evaluation is returned
in the #Result field as !XPathValue, which can represent various types of data including node sets, strings, numbers,
or booleans.

-INPUT-
obj(XML) XML: Targeted XML document to query.  Can be NULL for XQuery expressions that do not require a context.

-ERRORS-
Okay
NullArgs
AllocMemory

*********************************************************************************************************************/

static ERR XQUERY_Evaluate(extXQuery *Self, struct xq::Evaluate *Args)
{
   pf::Log log;

   if (not Args) return log.warning(ERR::NullArgs);

   int len = 0, max_len = std::min<int>(std::ssize(Self->Statement), 60);
   while ((Self->Statement[len] != '\n') and (len < max_len)) len++;
   log.branch("Expression: %.*s", len, Self->Statement.c_str());

   if (Self->StaleBuild) {
      if (auto err = build_query(Self); err != ERR::Okay) return err;
   }

   auto xml = (extXML *)Args->XML;
   Self->XML = xml;
   
   Self->ErrorMsg.clear();

   if (xml) {
      pf::ScopedObjectLock lock(xml);

      xml->Attrib.clear();
      XPathEvaluator eval(xml, Self->ParseResult.expression.get(), &Self->ParseResult);
      auto err = eval.evaluate_xpath_expression(*(Self->ParseResult.expression.get()), &Self->Result);
      if (err != ERR::Okay) log.warning("%s", Self->ErrorMsg.c_str());
      return err;
   }
   else {
      XPathEvaluator eval(nullptr, Self->ParseResult.expression.get(), &Self->ParseResult);
      auto err = eval.evaluate_xpath_expression(*(Self->ParseResult.expression.get()), &Self->Result);
      if (err != ERR::Okay) log.warning("%s", Self->ErrorMsg.c_str());
      return err;
   }
}

//********************************************************************************************************************

static ERR XQUERY_Free(extXQuery *Self)
{
   Self->~extXQuery();
   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
GetKey: Read XQuery variable values.
-END-
*********************************************************************************************************************/

static ERR XQUERY_GetKey(extXQuery *Self, struct acGetKey *Args)
{
   if (not Args) return ERR::NullArgs;

   return ERR::NoSupport;
}

/*********************************************************************************************************************
-ACTION-
Init: Compiles the XQuery statement.

Initialisation converts a valid XQuery expression string into a compiled form that can be
executed against an XML document.  The resulting compiled expression can be reused multiple times for efficiency
and must be freed using FreeResource when no longer needed.  They are re-usable between different XML documents and
are treated as read-only for thread-safety.

If parsing fails, the object will not be initialised and an error message will be defined in the #ErrorMsg field.

Note: This function can hang temporarily if the expression references network URIs.  Consider calling it from a
separate thread to avoid blocking in such cases.

*********************************************************************************************************************/

static ERR XQUERY_Init(extXQuery *Self)
{
   return build_query(Self);
}

//********************************************************************************************************************

static ERR XQUERY_NewPlacement(extXQuery *Self)
{
   new (Self) extXQuery;
   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
Reset: Clears the information held in an XQuery object.
*********************************************************************************************************************/

static ERR XQUERY_Reset(extXQuery *Self)
{
   return acClear(Self);
}

/*********************************************************************************************************************

-METHOD-
Search: For node-based queries, calls a function for each matching node.

Use the Search method to scan an XML document for tags or attributes that match a compiled XQuery expression.
For every matching node, a user-defined callback function is invoked, allowing custom processing of each result.

If no callback is provided, the search stops after the first match and the @XML object's cursor markers will reflect
the position of the node.

Note that valid function execution can return `ERR:Search` if zero matches are found.

-INPUT-
obj(XML) XML: Target XML document to search.
ptr(func) Callback: Optional callback function to invoke for each matching node.

-ERRORS-
Okay: At least one matching node was found and processed.
NullArgs: At least one required parameter was not provided.
Syntax: The provided query expression has syntax errors.
Search: No matching node was found.
Terminate: The callback function requested termination of the search.

*********************************************************************************************************************/

static ERR XQUERY_Search(extXQuery *Self, struct xq::Search *Args)
{
   pf::Log log;

   if (not Args) return log.warning(ERR::NullArgs);

   int len = 0, max_len = std::min<int>(std::ssize(Self->Statement), 60);
   while ((Self->Statement[len] != '\n') and (len < max_len)) len++;
   log.branch("Expression: %.*s; Callback: %c", len, Self->Statement.c_str(), Args->Callback ? (Args->Callback->Type != CALL::NIL ? 'Y' : 'N') : 'N');

   if (Self->StaleBuild) {
      if (auto err = build_query(Self); err != ERR::Okay) return err;
   }

   auto xml = (extXML *)Args->XML;
   Self->XML = xml;
   Self->ErrorMsg.clear();
   if (xml) xml->ErrorMsg.clear();

   if (xml) {
      pf::ScopedObjectLock lock(xml);

      if ((Args->Callback) and (Args->Callback->defined())) Self->Callback = *Args->Callback;
      else Self->Callback.Type = CALL::NIL;

      xml->Callback = Self->Callback; // TODO: TEMPORARY

      // TODO: Can these fields be moved to extXQuery?
      xml->Attrib.clear();

      (void)xml->getMap(); // Ensure the tag ID and ParentID values are defined
      XPathEvaluator eval(xml, Self->ParseResult.expression.get(), &Self->ParseResult);
      auto error = eval.find_tag(*Self->ParseResult.expression.get(), 0); // Returns ERR:Search if no match
      if (not xml->ErrorMsg.empty()) Self->ErrorMsg = xml->ErrorMsg;
      return error;
   }
   else {
      XPathEvaluator eval(nullptr, Self->ParseResult.expression.get(), &Self->ParseResult);
      return eval.find_tag(*Self->ParseResult.expression.get(), 0); // Returns ERR:Search if no match
   }
}

/*********************************************************************************************************************

-ACTION-
SetKey: Set XQuery variable values.

Use SetKey to store key-value pairs that can be referenced in XQuery expressions using the variable syntax
`$variableName`.

-INPUT-
cstr Key: The name of the variable (case sensitive).
cstr Value: The string value to store or NULL to remove an existing key.

-ERRORS-
Okay:
NullArgs: The `Key` parameter was not specified.
-END-

*********************************************************************************************************************/

static ERR XQUERY_SetKey(extXQuery *Self, struct acSetKey *Args)
{
   pf::Log log;

   if ((not Args) or (not Args->Key)) return log.warning(ERR::NullArgs);

   log.trace("Setting variable '%s' = '%s'", Args->Key, Args->Value ? Args->Value : "");

   //if (Args->Value) Self->Variables[Args->Key] = Args->Value;
   //else Self->Variables.erase(Args->Key);

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
ErrorMsg: A readable description of the last parse or execution error.

This field may provide a textual description of the last parse or execution error that occurred.

*********************************************************************************************************************/

static ERR GET_ErrorMsg(extXQuery *Self, CSTRING *Value)
{
   if (not Self->ErrorMsg.empty()) { *Value = Self->ErrorMsg.c_str(); return ERR::Okay; }
   else return ERR::NoData;
}

/*********************************************************************************************************************

-FIELD-
Path: Base path for resolving relative references.

Set the Path field to define the base-uri for an XQuery expression.  If left unset, the path will be computed through
automated means on-the-fly, which relies  on the working directory or XML document path.

*********************************************************************************************************************/

static ERR GET_Path(extXQuery *Self, STRING *Value)
{
   if (not Self->initialised()) {
      if (not Self->Path.empty()) {
         *Value = pf::strclone(Self->Path.c_str());
         return ERR::Okay;
      }
      else return ERR::FieldNotSet;
   }

   if ((*Value = pf::strclone(Self->Path.c_str()))) {
      return ERR::Okay;
   }
   else return ERR::AllocMemory;
}

static ERR SET_Path(extXQuery *Self, CSTRING Value)
{
   Self->Path.clear();

   if ((Value) and (*Value)) {
      Self->Path = Value;
      return ERR::Okay;
   }
   else {
      Self->Path.clear();
      return ERR::Okay;
   }
}

/*********************************************************************************************************************

-FIELD-
Result: Returns the results of the most recently executed query.

Following the successful execution of an XQuery expression, the results can be retrieved as an XPathValue object
through this field.

*********************************************************************************************************************/

static ERR GET_Result(extXQuery *Self, XPathValue **Value)
{
   if (not Self->Result.is_empty()) {
      *Value = &Self->Result;
      return ERR::Okay;
   }
   else return ERR::NoData;
}

/*********************************************************************************************************************

-FIELD-
ResultString: Returns the results of the most recently executed query as a string.

Following the successful execution of an XQuery expression, the results can be retrieved as a string through this
field.  The string representation is generated from the #Result field, which holds the raw evaluation output.

Note that if the result is empty, the returned string will also be empty (i.e. is not considered an error).  The
string is managed internally and does not require manual deallocation.

The string result becomes invalid if the XQuery object is modified, re-executed or destroyed.

*********************************************************************************************************************/

static ERR GET_ResultString(extXQuery *Self, CSTRING *Value)
{
   // Return the cached string if it exists.
   if (not Self->ResultString.empty()) {
      *Value = Self->ResultString.c_str();
      return ERR::Okay;
   }

   if (Self->Result.is_empty()) { // An empty result isn't considered an error.
      Self->ResultString.clear();
      *Value = "";
      return ERR::Okay;
   }
   else {
      Self->ResultString = Self->Result.to_string();  // Cache the result
      *Value = Self->ResultString.c_str();
      return ERR::Okay;
   }
}

/*********************************************************************************************************************

-FIELD-
Statement: XQuery data is processed through this field.

Set the Statement field with an XPath or XQuery expression for compilation.

If this field is set after initialisation then @Clear() will be applied to the object first.  The expression will
be compiled on the next execution attempt.

If the statement is an XQuery expression with base-uri references, the #Path field should be set to establish
the base path for relative references.

-END-

*********************************************************************************************************************/

static ERR GET_Statement(extXQuery *Self, STRING *Value)
{
   if (not Self->initialised()) {
      if (not Self->Statement.empty()) {
         *Value = pf::strclone(Self->Statement.c_str());
         return ERR::Okay;
      }
      else return ERR::FieldNotSet;
   }
   else if ((*Value = pf::strclone(Self->Statement.c_str()))) return ERR::Okay;
   else return ERR::AllocMemory;
}

static ERR SET_Statement(extXQuery *Self, CSTRING Value)
{
   XQUERY_Clear(Self);

   if ((Value) and (*Value)) {
      Self->Statement = Value;
      return ERR::Okay;
   }
   else {
      Self->Statement.clear();
      return ERR::Okay;
   }
}

//********************************************************************************************************************

#include "xquery_class_def.cpp"

static const FieldArray clFields[] = {
   // Virtual fields
   //{ "Callback",     FDF_FUNCTIONPTR|FDF_RW,   GET_Callback, SET_Callback },
   { "ErrorMsg",     FDF_STRING|FDF_R,         GET_ErrorMsg },
   { "Path",         FDF_STRING|FDF_RW,        GET_Path, SET_Path },
   { "Result",       FDF_PTR|FDF_STRUCT|FDF_R, GET_Result, nullptr, "XPathValue" },
   { "ResultString", FDF_STRING|FDF_R,         GET_ResultString },
   { "Statement",    FDF_STRING|FDF_RW,        GET_Statement, SET_Statement },
   END_FIELD
};

static ERR add_xquery_class(void)
{
   clXQuery = objMetaClass::create::global(
      fl::BaseClassID(CLASSID::XQUERY),
      fl::ClassVersion(VER_XQUERY),
      fl::Name("XQuery"),
      fl::FileExtension("*.xqm|*.xq"),
      fl::FileDescription("XQuery Module"),
      fl::Icon("filetypes/xml"),
      fl::Category(CCF::DATA),
      fl::Actions(clXQueryActions),
      fl::Methods(clXQueryMethods),
      fl::Fields(clFields),
      fl::Size(sizeof(extXQuery)),
      fl::Path(MOD_PATH));

   return clXQuery ? ERR::Okay : ERR::AddClass;
}
