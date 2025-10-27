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

There are two distinct methods for query evaluation.  Value evaluation returns typed results (XPathValue)
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
* An InspectFunction() method would allow the function signature and list of parameter names to be returned.
* Add support for custom functions via a new method, e.g., RegisterFunction().
* Allow modules to be preloaded.  There are many ways this could be achieved, e.g.
  - Load the module as a separate XQuery and link it via a new method.
  - Provide a callback that is invoked when an import is encountered, this allows the the host application to supply
    the XQuery module.
  - Create a global cache of loaded modules that is shared by all XQuery instances.  A single LoadModule(URI) function
    would manage it.  Probably the best option.

*********************************************************************************************************************/

static ERR build_query(extXQuery *Self)
{
   pf::Log log;

   Self->StaleBuild = false;
   Self->ListVariables.clear();
   Self->ListFunctions.clear();

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
      if (not Self->ParseResult.expression) Self->ParseResult.expression = std::make_unique<XPathNode>(XQueryNodeType::EMPTY_SEQUENCE);
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

//********************************************************************************************************************
// Convert an expanded QName (e.g., Q{uri}local) to lexical form (e.g., prefix:local or local).

static std::string to_lexical_name(const XQueryProlog &prolog, const std::string &qname) 
{
   if ((qname.size() > 2) and (qname[0] IS 'Q') and (qname[1] IS '{')) {
      size_t closing = qname.find('}');
      if (closing IS std::string::npos) return qname;
      std::string uri = qname.substr(2, closing - 2);
      std::string local = qname.substr(closing + 1);

      // Prefer explicit module prefix if it matches
      if (prolog.module_namespace_uri.has_value()) {
         if (std::string_view(*prolog.module_namespace_uri) IS std::string_view(uri)) {
            if (prolog.module_namespace_prefix.has_value()) {
               return std::format("{}:{}", *prolog.module_namespace_prefix, local);
            }
         }
      }

      // Fall back to any declared prefix bound to the URI
      for (const auto &ns : prolog.declared_namespace_uris) {
         if (std::string_view(ns.second) IS std::string_view(uri)) {
            return std::format("{}:{}", ns.first, local);
         }
      }

      // If default function namespace matches, return local name only
      if (prolog.default_function_namespace_uri.has_value()) {
         if (std::string_view(*prolog.default_function_namespace_uri) IS std::string_view(uri)) {
            return local;
         }
      }

      return qname; // Leave expanded form if no mapping available
   }
   return qname; // Already in lexical form (e.g., prefix:local or local)
};

/*********************************************************************************************************************

-ACTION-
Activate: Run an XQuery expression.

Use Activate to run a compiled XQuery expression without an XML document reference.  The result of the evaluation is
returned in the #Result field as XPathValue, which can represent various types of data including node sets, strings,
numbers, or booleans.  On error, the #ErrorMsg field will contain a descriptive message.

Use #Evaluate() or #Search() for expressions expecting an XML document context.

-ERRORS-
Okay
Syntax

*********************************************************************************************************************/

static ERR XQUERY_Activate(extXQuery *Self)
{
   pf::Log log;

   int len = 0, max_len = std::min<int>(std::ssize(Self->Statement), 40);
   while ((Self->Statement[len] != '\n') and (len < max_len)) len++;
   log.branch("Expression: %.*s, BasePath: %s", len, Self->Statement.c_str(), Self->Path.c_str());

   if (Self->StaleBuild) {
      if (auto err = build_query(Self); err != ERR::Okay) return err;
   }

   Self->XML = nullptr;
   XPathEvaluator eval(Self, nullptr, Self->ParseResult.expression.get(), &Self->ParseResult);
   auto err = eval.evaluate_xpath_expression(*(Self->ParseResult.expression.get()), &Self->Result);
   Self->ErrorMsg = Self->ParseResult.error_msg;
   return err;
}

/*********************************************************************************************************************

-ACTION-
Clear: Clears all XQuery results and returns the object to its pre-compiled state.

Use Clear() to remove the resources consumed by the XQuery and reset its state.  The #Statement and #Path field 
values are retained, allowing the object to be seamlessly re-activated at any time.

*********************************************************************************************************************/

static ERR XQUERY_Clear(extXQuery *Self)
{
   Self->ErrorMsg.clear();
   Self->ListVariables.clear();
   Self->ListFunctions.clear();
   Self->ParseResult = CompiledXQuery();
   Self->ResultString.clear();
   Self->Result = XPathVal();
   Self->StaleBuild = true;
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
Evaluate: Run an XQuery expression against an XML document.

Use Evaluate to run a compiled XQuery expression against an XML document.  The result of the evaluation is returned
in the #Result field as XPathValue, which can represent various types of data including node sets, strings, numbers,
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

   int len = 0, max_len = std::min<int>(std::ssize(Self->Statement), 40);
   while ((Self->Statement[len] != '\n') and (len < max_len)) len++;
   log.branch("Expression: %.*s, BasePath: %s", len, Self->Statement.c_str(), Self->Path.c_str());

   if (Self->StaleBuild) {
      if (auto err = build_query(Self); err != ERR::Okay) return err;
   }

   auto xml = (extXML *)Args->XML;
   Self->XML = xml;

   if (xml) {
      pf::ScopedObjectLock lock(xml);

      if (Self->Path.empty() and (xml->Path)) Self->Path = xml->Path;

      xml->Attrib.clear();
      XPathEvaluator eval(Self, xml, Self->ParseResult.expression.get(), &Self->ParseResult);
      auto err = eval.evaluate_xpath_expression(*(Self->ParseResult.expression.get()), &Self->Result);
      Self->ErrorMsg = Self->ParseResult.error_msg;
      return err;
   }
   else {
      XPathEvaluator eval(Self, nullptr, Self->ParseResult.expression.get(), &Self->ParseResult);
      auto err = eval.evaluate_xpath_expression(*(Self->ParseResult.expression.get()), &Self->Result);
      Self->ErrorMsg = Self->ParseResult.error_msg;
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
   if ((not Args) or (not Args->Value) or (not Args->Key)) return ERR::NullArgs;
   if (Args->Size < 2) return ERR::Args;

   if (auto it = Self->Variables.find(Args->Key); it != Self->Variables.end()) {
      pf::strcopy(it->second.c_str(), Args->Value, Args->Size);
      return ERR::Okay;
   }
   else {
      Args->Value[0] = 0;
      return ERR::UnsupportedField;
   }
}

/*********************************************************************************************************************

-ACTION-
Init: Compiles the XQuery statement.

Initialisation will compile the XQuery #Statement string into a compiled form that can be executed.

If parsing fails, the object will not be initialised and an error message will be defined in the #ErrorMsg field.

Note: This function can hang temporarily if the expression references network URIs.  Consider calling it from a
separate thread to avoid blocking in such cases.

*********************************************************************************************************************/

static ERR XQUERY_Init(extXQuery *Self)
{
   // Not providing a statement is permitted as the object may be preallocated for later use.
   if (not Self->Statement.empty()) return build_query(Self);
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR XQUERY_NewPlacement(extXQuery *Self)
{
   new (Self) extXQuery;
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
RegisterFunction: Register a custom XQuery function.

Use RegisterFunction to define a custom function that can be invoked within XQuery expressions.  The function
will be associated with the specified name and can be called like any standard XQuery function.

-INPUT-
cpp(str) FunctionName: The name of the function to register (e.g., "my:custom-function").
ptr(func) Callback: The callback function to register for FunctionName.

*********************************************************************************************************************/

static ERR XQUERY_RegisterFunction(extXQuery *Self, struct xq::RegisterFunction *Args)
{
   pf::Log log;

   if (not Args) return log.warning(ERR::NullArgs);

   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
Reset: Synonym for #Clear().
-END-
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

   int len = 0, max_len = std::min<int>(std::ssize(Self->Statement), 40);
   while ((Self->Statement[len] != '\n') and (len < max_len)) len++;
   log.branch("Expression: %.*s; Callback: %c, BasePath: %s", len, Self->Statement.c_str(), Args->Callback ? (Args->Callback->Type != CALL::NIL ? 'Y' : 'N') : 'N', Self->Path.c_str());

   if (Self->StaleBuild) {
      if (auto err = build_query(Self); err != ERR::Okay) return err;
   }

   auto xml = (extXML *)Args->XML;
   Self->XML = xml;

   if (xml) {
      pf::ScopedObjectLock lock(xml);

      if (Self->Path.empty() and (xml->Path)) Self->Path = xml->Path;

      if ((Args->Callback) and (Args->Callback->defined())) Self->Callback = *Args->Callback;
      else Self->Callback.Type = CALL::NIL;

      xml->Callback = Self->Callback; // TODO: TEMPORARY

      // TODO: Can these fields be moved to extXQuery?
      xml->Attrib.clear();

      (void)xml->getMap(); // Ensure the tag ID and ParentID values are defined
      XPathEvaluator eval(Self, xml, Self->ParseResult.expression.get(), &Self->ParseResult);
      auto error = eval.find_tag(*Self->ParseResult.expression.get(), 0); // Returns ERR:Search if no match
      Self->ErrorMsg = Self->ParseResult.error_msg;
      return error;
   }
   else {
      XPathEvaluator eval(Self, nullptr, Self->ParseResult.expression.get(), &Self->ParseResult);
      auto error = eval.find_tag(*Self->ParseResult.expression.get(), 0); // Returns ERR:Search if no match
      Self->ErrorMsg = Self->ParseResult.error_msg;
      return error;
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

   if (Args->Value) {
      Self->Variables[Args->Key] = Args->Value;
   }
   else {
      // Remove variable if Value is null
      Self->Variables.erase(Args->Key);
   }

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
FeatureFlags: Flags indicating the features of a compiled XQuery expression.

*********************************************************************************************************************/

static ERR GET_FeatureFlags(extXQuery *Self, XQF &Value)
{
   if (not Self->initialised()) return ERR::NotInitialised;

   Value = Self->ParseResult.feature_flags();
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Functions: Returns an allocated list of all declared XQuery functions.

Provides a list of all XQuery functions that have been defined by the user or during evaluation of the XQuery 
expression (via the `declare` keyword).

Example: For `declare function math:cube($x) { }` the name `math:cube` would appear in the list.

Duplicate function names are not removed.

*********************************************************************************************************************/

static ERR GET_Functions(extXQuery *Self, pf::vector<std::string> **Value)
{
   if (not Self->initialised()) return ERR::NotInitialised;

   if ((Self->ListFunctions.empty()) and (Self->ParseResult.prolog)) {
      // Include functions declared in the main query prolog
      for (const auto &entry : Self->ParseResult.prolog->functions) {
         const auto &fn = entry.second;
         Self->ListFunctions.push_back(to_lexical_name(*Self->ParseResult.prolog, fn.qname));
      }

      // Include functions declared in imported modules
      std::shared_ptr<XQueryModuleCache> mod_cache = Self->ParseResult.prolog->get_module_cache();
      if (mod_cache) {
         for (auto it = mod_cache->modules.begin(); it != mod_cache->modules.end(); ++it) {
            if ((it->second) and (it->second->prolog)) {
               for (auto fn = it->second->prolog->functions.begin(); fn != it->second->prolog->functions.end(); ++fn) {
                  Self->ListFunctions.push_back(to_lexical_name(*it->second->prolog, fn->second.qname));
               }
            }
         }
      }
   }

   *Value = &Self->ListFunctions;
   return ERR::Okay;
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
ResultType: Returns the value type of the most recently executed query.
Prefix: XPVT

If an XQuery expression returns a #Result, the type can be retrieved from this field.

*********************************************************************************************************************/

static ERR GET_ResultType(extXQuery *Self, XPVT &Value)
{
   if (Self->Result.is_empty()) { // An empty result isn't considered an error.
      Value = XPVT::NIL;
      return ERR::Okay;
   }
   else {
      Value = Self->Result.Type;
      return ERR::Okay;
   }
}

/*********************************************************************************************************************

-FIELD-
Statement: XQuery data is processed through this field.

Set the Statement field with an XPath or XQuery expression for compilation.

If this field is set after initialisation then #Clear() will be applied to the object first.  The expression will
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

/*********************************************************************************************************************

-FIELD-
Variables: Returns an allocated list of all defined XQuery variables.

Provides a list of all XQuery variables that have been defined using the #SetKey() action, or during evaluation of
the XQuery expression (via the `declare` keyword).

Example: For `declare variable $math:pi := 3.14159;` the variable name `math:pi` would appear in the list.

Duplicate variable names are not removed.

*********************************************************************************************************************/

static ERR GET_Variables(extXQuery *Self, pf::vector<std::string> **Value)
{
   if (not Self->initialised()) return ERR::NotInitialised;

   if (Self->ListVariables.empty()) {
      for (const auto &var : Self->Variables) {
         Self->ListVariables.push_back(var.first);
      }
   }

   // Scan imported modules for additional variables

   if (Self->ParseResult.prolog) {
      // Include variables declared in the main query prolog
      for (auto var = Self->ParseResult.prolog->variables.begin(); var != Self->ParseResult.prolog->variables.end(); ++var) {
         Self->ListVariables.push_back(var->first);
      }

      // Include variables declared in imported modules
      std::shared_ptr<XQueryModuleCache> mod_cache = Self->ParseResult.prolog->get_module_cache();
      if (mod_cache) {
         for (auto it = mod_cache->modules.begin(); it != mod_cache->modules.end(); ++it) {
            if ((it->second) and (it->second->prolog)) {
               for (auto var = it->second->prolog->variables.begin(); var != it->second->prolog->variables.end(); ++var) {
                  Self->ListVariables.push_back(var->first);
               }
            }
         }
      }
   }

   *Value = &Self->ListVariables;
   return ERR::Okay;
}

//********************************************************************************************************************

#include "xquery_class_def.cpp"

static const FieldArray clFields[] = {
   // Virtual fields
   { "ErrorMsg",     FDF_STRING|FDF_R,         GET_ErrorMsg },
   { "FeatureFlags", FDF_INTFLAGS|FDF_R,       GET_FeatureFlags, nullptr, &clXQueryXQF },
   { "Path",         FDF_STRING|FDF_RW,        GET_Path, SET_Path },
   { "Result",       FDF_PTR|FDF_STRUCT|FDF_R, GET_Result, nullptr, "XPathValue" },
   { "ResultString", FDF_STRING|FDF_R,         GET_ResultString },
   { "ResultType",   FDF_INT|FDF_R,            GET_ResultType },
   { "Statement",    FDF_STRING|FDF_RW,        GET_Statement, SET_Statement },
   { "Functions",    FDF_ARRAY|FDF_CPP|FDF_STRING|FDF_R, GET_Functions },
   { "Variables",    FDF_ARRAY|FDF_CPP|FDF_STRING|FDF_R, GET_Variables },
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
