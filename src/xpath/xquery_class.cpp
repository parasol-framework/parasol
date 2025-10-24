/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
XQuery: Provides an interface for XQuery evaluation and execution.

...

-END-

*********************************************************************************************************************/

static ERR build_query(extXQuery *Self)
{
   pf::Log log;

   if (Self->Statement.empty()) return log.warning(ERR::FieldNotSet);
   
   Self->ErrorMsg.clear();

   int len = 0;
   while ((Self->Statement[len] != '\n') and (len < std::ssize(Self->Statement)) and (len < 60)) len++;
   log.branch("Expression: %.*s", len, Self->Statement.c_str());

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

   // Move the module cache across if one was created during parsing.

   std::shared_ptr<XQueryModuleCache> module_cache = Self->ParseResult.module_cache;
   if (not module_cache) {
      module_cache = std::make_shared<XQueryModuleCache>();
      module_cache->owner = Self->UID;
   }

   if (module_cache) { // Retain on the result only; evaluator uses parse-context.
      Self->ParseResult.module_cache = module_cache;
      if (Self->ParseResult.prolog) Self->ParseResult.prolog->bind_module_cache(module_cache);
   }

   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
Clear: Completely clears all XQuery data and resets the object to its initial state.

Use Clear() to remove the resources consumed by the XQuery while still retaining it for future use.

-END-
*********************************************************************************************************************/

static ERR XQUERY_Clear(extXQuery *Self)
{
   Self->ErrorMsg.clear();
   Self->ParseResult.prolog.reset();
   Self->ParseResult.expression.reset();
   Self->Result = XPathVal();
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
Evaluate: Run an XQuery expression against an XQuery document.

Use Evaluate to run a compiled XQuery expression against an XML document.  The result of the
evaluation is returned in the #Result field as !XPathValue, which can represent various types of data including
node sets, strings, numbers, or booleans.

-INPUT-
obj(XML) XML: Targeted XML document to query.

-ERRORS-
Okay
NullArgs
AllocMemory
-END-

*********************************************************************************************************************/

static ERR XQUERY_Evaluate(extXQuery *Self, struct xq::Evaluate *Args)
{
   pf::Log log;

   if ((not Args) or (not Args->XML)) return log.warning(ERR::NullArgs);

   log.branch("");

   auto xml = (extXML *)Args->XML;
   Self->XML = xml;

   if (xml->Tags.empty()) return log.warning(ERR::NoData);

   xml->Attrib.clear();
   xml->CursorTags = &xml->Tags;
   xml->Cursor = xml->Tags.begin();

   Self->ErrorMsg.clear();

   XPathEvaluator eval(xml, Self->ParseResult.expression.get(), &Self->ParseResult);
   auto err = eval.evaluate_xpath_expression(*(Self->ParseResult.expression.get()), &Self->Result);
   if (err != ERR::Okay) {
      log.warning("%s", Self->ErrorMsg.c_str());
   }

   return err;
}

//********************************************************************************************************************

static ERR XQUERY_Free(extXQuery *Self)
{
   Self->~extXQuery();
   return ERR::Okay;
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

-END-
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

-METHOD-
Query: For node-based queries, evaluates a compiled expression and calls a function for each matching node.

Use the Query function to scan an XML document for tags or attributes that match a compiled XQuery expression.
For every matching node, a user-defined callback function is invoked, allowing custom processing of each result.

If no callback is provided, the search stops after the first match and the @XML object's cursor markers will reflect
the position of the node.

Note that valid function execution can return `ERR:Search` if zero matches are found.

-INPUT-
obj(XML) XML: Targeted XML document to query.
ptr(func) Callback: Optional callback function to invoke for each matching node.

-ERRORS-
Okay: At least one matching node was found and processed.
NullArgs: At least one required parameter was not provided.
NoData: The XML document contains no data to search.
Syntax: The provided query expression has syntax errors.
Search: No matching node was found.
-END-

*********************************************************************************************************************/

static ERR XQUERY_Query(extXQuery *Self, struct xq::Query *Args)
{
   pf::Log log(__FUNCTION__);

   if (not Args->XML) return ERR::NullArgs;

   auto xml = (extXML *)Args->XML;
   Self->XML = xml;

   if (xml->Tags.empty()) return log.warning(ERR::NoData); // Empty document

   if (Args->Callback) Self->Callback = *Args->Callback;
   else Self->Callback.Type = CALL::NIL;

   // TODO: Can these fields be moved to extXQuery?
   xml->Attrib.clear();
   xml->CursorTags = &xml->Tags;
   xml->Cursor = xml->Tags.begin();
   
   (void)xml->getMap(); // Ensure the tag ID and ParentID values are defined
   
   XPathEvaluator eval(xml, Self->ParseResult.expression.get(), &Self->ParseResult);
   return eval.find_tag(*Self->ParseResult.expression.get(), 0); // Returns ERR:Search if no match
}

/*********************************************************************************************************************
-ACTION-
Reset: Clears the information held in an XQuery object.
-END-
*********************************************************************************************************************/

static ERR XQUERY_Reset(extXQuery *Self)
{
   return acClear(Self);
}

/*********************************************************************************************************************

-FIELD-
ErrorMsg: A textual description of the last parse error.

This field may provide a textual description of the last parse error that occurred, in conjunction with the most
recently received error code.  Issues parsing malformed XPath expressions may also be reported here.

*********************************************************************************************************************/

static ERR GET_ErrorMsg(extXQuery *Self, CSTRING *Value)
{
   if (not Self->ErrorMsg.empty()) { *Value = Self->ErrorMsg.c_str(); return ERR::Okay; }
   else return ERR::NoData;
}

/*********************************************************************************************************************

-FIELD-
Path: Base path for resolving relative references.

Set the Path field to parse an XQuery formatted data string through the object.  If this field is set after
initialisation then the XQuery object will clear any existing data first.

Be aware that setting this field with an invalid statement will result in an empty XQuery object.

Reading the Statement field will return a serialised string of XQuery data.  By default all tags will be included in the
statement unless a predefined starting position is set by the #Start field.  The string result is an allocation that
must be freed.

If the statement is an XQuery expression with base-uri references, the #Path field should be set to establish
the base path for relative references.

-END-

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

-END-

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

-END-

*********************************************************************************************************************/

static ERR GET_ResultString(extXQuery *Self, CSTRING *Value)
{
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
      Self->ResultString = Self->Result.to_string();
      *Value = Self->ResultString.c_str();
      return ERR::Okay;
   }
}

/*********************************************************************************************************************

-FIELD-
Statement: XQuery data is processed through this field.

Set the Statement field to parse an XQuery formatted data string through the object.  If this field is set after
initialisation then the XQuery object will clear any existing data first.

Be aware that setting this field with an invalid statement will result in an empty XQuery object.

Reading the Statement field will return a serialised string of XQuery data.  By default all tags will be included in the
statement unless a predefined starting position is set by the #Start field.  The string result is an allocation that
must be freed.

If the statement is an XQuery expression with base-uri references, the #Path field should be set to establish
the base path for relative references.

-END-

*********************************************************************************************************************/

static ERR GET_Statement(extXQuery *Self, STRING *Value)
{
   pf::Log log;

   if (not Self->initialised()) {
      if (not Self->Statement.empty()) {
         *Value = pf::strclone(Self->Statement.c_str());
         return ERR::Okay;
      }
      else return ERR::FieldNotSet;
   }

   if ((*Value = pf::strclone(Self->Statement.c_str()))) {
      return ERR::Okay;
   }
   else return ERR::AllocMemory;
}

static ERR SET_Statement(extXQuery *Self, CSTRING Value)
{
   Self->Statement.clear();

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
