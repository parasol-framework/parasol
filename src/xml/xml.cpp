/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
XML: Provides XML data management services for parsing, manipulation and serialisation.

The XML class is designed to provide robust functionality for creating, parsing and maintaining XML data structures.
It supports both well-formed and loosely structured XML documents, offering flexible parsing behaviours to
accommodate various XML formats.  The class includes comprehensive support for XPath queries, content manipulation
and document validation.

<header>Data Loading and Parsing</header>

XML documents can be loaded into an XML object through multiple mechanisms:

The #Path field allows loading from file system sources, with automatic parsing upon initialisation.  The class
supports ~Core.LoadFile() caching for frequently accessed files, improving performance for repeated operations.

The #Statement field enables direct parsing of XML strings, supporting dynamic content processing and in-memory
document construction.

The #Source field provides object-based input, allowing XML data to be sourced from any object supporting the Read
action.

For batch processing scenarios, the Path or Statement fields can be reset post-initialisation, causing the XML
object to automatically rebuild itself.  This approach optimises memory usage by reusing existing object instances
rather than creating new ones.

<header>Document Structure and Access</header>

Successfully parsed XML data is accessible through the #Tags field, which contains a hierarchical array of !XMLTag
structures.  Each XMLTag represents a complete XML element including its attributes, content and child elements.
The structure maintains the original document hierarchy, enabling both tree traversal and direct element access.

C++ developers benefit from direct access to the Tags field, represented as `pf::vector&lt;XMLTag&gt;`.  This provides
efficient iteration and element access with standard STL semantics.  However, direct modification of the Tags array
is discouraged as it can destabilise internal object state - developers should use the provided methods for safe
manipulation.

-END-

ENTITY       <!ENTITY % textgizmo "fontgizmo">
INSTRUCTION  <?XML version="1.0" standalone="yes" ?>
NOTATION     <!NOTATION gif SYSTEM "viewer.exe">

*********************************************************************************************************************/

#define PRV_XML
#include <parasol/modules/xml.h>
#include <parasol/strings.hpp>
#include <functional>
#include <sstream>
#include "../link/unicode.h"

JUMPTABLE_CORE

static OBJECTPTR clXML = nullptr;
static uint32_t glTagID = 1;

struct ParseState {
   CSTRING Pos;
   int   Balance;  // Indicates that the tag structure is correctly balanced if zero

   ParseState() : Pos(nullptr), Balance(0) { }
};

typedef objXML::TAGS TAGS;
typedef objXML::CURSOR CURSOR;

//********************************************************************************************************************

class extXML : public objXML {
   public:
   ankerl::unordered_dense::map<int, XMLTag *> Map; // Lookup for any indexed tag.
   STRING Statement;
   bool   ReadOnly;
   bool   StaleMap;         // True if map requires a rebuild

   TAGS *CursorParent;  // Parent tag, if any
   TAGS *CursorTags;    // Updated by findTag().  This is the tag array to which the Cursor reference belongs
   CURSOR Cursor;       // Resulting cursor position (tag) after a successful search.
   std::string Attrib;
   FUNCTION Callback;

   // Namespace registry using pf::strhash() values, this allows us to store URIs in compact form in XMLTag structures.
   ankerl::unordered_dense::map<uint32_t, std::string> NSRegistry; // hash(URI) -> URI

   // Link prefixes to namespace URIs
   std::map<std::string, uint32_t> Prefixes; // hash(Prefix) -> hash(URI)

   std::map<std::string, uint32_t> CurrentPrefixMap; // hash(Prefix) -> hash(URI) [current scope only]
   uint32_t DefaultNamespace = 0; // [current scope only]

   extXML() : ReadOnly(false), StaleMap(true) { }

   ankerl::unordered_dense::map<int, XMLTag *> & getMap() {
      if (StaleMap) {
         Map.clear();
         updateIDs(Tags, 0);
         StaleMap = false;
      }

      return Map;
   }

   // Return the tag for a particular ID.

   [[nodiscard]] inline XMLTag * getTag(int ID) noexcept {
      auto &map = getMap();
      auto it = map.find(ID);
      if (it IS map.end()) return nullptr;
      else return it->second;
   }

   [[nodiscard]] inline TAGS * getInsert(int ID, CURSOR &Iterator) {
      if (auto tag = getTag(ID)) {
         return getInsert(tag, Iterator);
      }
      else return nullptr;
   }

   // For a given tag, return its vector array

   [[nodiscard]] inline TAGS * getTags(XMLTag *Tag) {
      if (!Tag->ParentID) return &Tags;
      else if (auto parent = getTag(Tag->ParentID)) return &parent->Children;
      else return nullptr;
   }

   // For a given tag, return its vector array and cursor position.

   [[nodiscard]] TAGS * getInsert(XMLTag *Tag, CURSOR &Iterator) {
      TAGS *tags;

      if (Tag->ParentID) {
         auto parent = getTag(Tag->ParentID);
         if (parent) tags = &parent->Children;
         else return nullptr;
      }
      else tags = &Tags;

      for (auto it = tags->begin(); it != tags->end(); it++) {
         if (it->ID IS Tag->ID) {
            Iterator = it;
            return tags;
         }
      }

      return nullptr;
   }

   inline void modified() {
      StaleMap = true;
      Modified++;
   }

   inline ERR findTag(CSTRING XPath, FUNCTION *pCallback = nullptr) {
      this->Attrib.clear();

      if (pCallback) this->Callback = *pCallback;
      else this->Callback.Type = CALL::NIL;

      this->CursorTags = &this->Tags;

      Cursor = this->Tags.begin();
      return find_tag(XPath);
   }

   // Namespace utility methods

   inline uint32_t registerNamespace(const std::string &uri) {
      if (uri.empty()) return 0;
      auto hash = pf::strhash(uri);
      NSRegistry[hash] = uri;
      return hash;
   }

   inline std::string * getNamespaceURI(uint32_t hash) {
      auto it = NSRegistry.find(hash);
      return (it != NSRegistry.end()) ? &it->second : nullptr;
   }

   ERR find_tag(std::string_view XPath);

   inline void updateIDs(TAGS &List, int ParentID) {
      for (auto &tag : List) {
         Map[tag.ID] = &tag;
         tag.ParentID = ParentID;
         if (!tag.Children.empty()) updateIDs(tag.Children, tag.ID);
      }
   }
};

static ERR add_xml_class(void);
static ERR SET_Statement(extXML *, CSTRING);
static ERR SET_Source(extXML *, OBJECTPTR);

#include "unescape.cpp"
#include "xml_functions.cpp"
#include "xml_search.cpp"

//********************************************************************************************************************

static ERR MODInit(OBJECTPTR pModule, struct CoreBase *pCore)
{
   CoreBase = pCore;

   return add_xml_class();
}

static ERR MODExpunge(void)
{
   if (clXML) { FreeResource(clXML); clXML = nullptr; }
   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
Clear: Completely clears all XML data and resets the object to its initial state.

The Clear action removes all parsed XML content from the object, including the complete tag hierarchy, cached data structures and internal state information.  This action effectively returns the XML object to its freshly-initialised condition, ready to accept new XML data.
-END-
*********************************************************************************************************************/

static ERR XML_Clear(extXML *Self)
{
   if (Self->Path) { FreeResource(Self->Path); Self->Path = nullptr; }

   Self->Tags.clear();
   Self->LineNo = 1;
   Self->Start  = 0;
   Self->ParseError = ERR::Okay;
   Self->modified();
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
Count: Count all tags that match a given XPath expression.

This method will count all tags that match a given `XPath` and return the value in the `Result` parameter.  It is
optimised for performance and does not modify the XML structure in any way.  It is safe to call concurrently from
multiple threads.

-INPUT-
cstr XPath: A valid XPath expression string defining the elements to count.  The expression must conform to XPath 1.0 syntax with Parasol extensions.
&int Result: Pointer to an integer variable that will receive the total count of matching tags.

-ERRORS-
Okay: The count operation completed successfully.
NullArgs: Either the XPath parameter or Result parameter was NULL.

-END-

*********************************************************************************************************************/

static thread_local int tlXMLCounter;

static ERR xml_count(extXML *Self, XMLTag &Tag, CSTRING Attrib)
{
   tlXMLCounter++;
   return ERR::Okay;
}

static ERR XML_Count(extXML *Self, struct xml::Count *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->XPath)) return log.warning(ERR::NullArgs);

   tlXMLCounter = 0;

   auto call = C_FUNCTION(xml_count);
   Self->findTag(Args->XPath, &call);

   Args->Result = tlXMLCounter;
   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
DataFeed: Processes and integrates external XML data into the object's document structure.

The DataFeed action provides a mechanism for supplying XML content to the object from external sources or streaming
data.  This action supports both complete document replacement and incremental content addition, depending on the
current state of the XML object.

The action accepts data in XML or plain text format and automatically performs parsing and integration.  When the
object contains no existing content, the provided data becomes the complete document structure.  If the object already
contains parsed XML, the new data is parsed separately and appended to the existing tag hierarchy.

If the provided data contains malformed XML or cannot be parsed according to the current validation settings, the
action will return appropriate error codes without modifying the existing document structure.  This ensures that
partial parsing failures do not corrupt previously loaded content.

Attempts to feed data into a read-only XML object will be rejected to maintain document integrity.

Example:

<code>
local xml = obj.new('xml')
local err = xml.acDataFeed(nil, DATA_XML, '<first>First element</first>')
</code>

-END-
*********************************************************************************************************************/

static ERR XML_DataFeed(extXML *Self, struct acDataFeed *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);

   if ((Args->Datatype IS DATA::XML) or (Args->Datatype IS DATA::TEXT)) {
      if (Self->ReadOnly) return log.warning(ERR::ReadOnly);

      if (Self->Tags.empty()) {
         if (auto error = txt_to_xml(Self, Self->Tags, (CSTRING)Args->Buffer); error != ERR::Okay) {
            return log.warning(error);
         }
      }
      else {
         TAGS tags;
         if (auto error = txt_to_xml(Self, tags, (CSTRING)Args->Buffer); error != ERR::Okay) {
            return log.warning(error);
         }

         Self->Tags.insert(Self->Tags.end(), tags.begin(), tags.end());
      }

      Self->modified();
   }
   else return log.warning(ERR::InvalidData);

   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
Filter: Filters the XML data structure to retain only a specific tag and its descendants.

The Filter method provides a mechanism for reducing large XML documents to a specific subtree, permanently removing
all content that exists outside the targeted element and its children.  This operation is particularly valuable for
performance optimisation when working with large documents where only a specific section is relevant.

The filtering process begins by locating the target element using the provided XPath expression.  Once found, a
new XML structure is created containing only the matched tag and its complete descendant hierarchy.  All sibling
tags, parent elements (excluding the direct lineage) and unrelated branches are permanently discarded.

-INPUT-
cstr XPath: A valid XPath expression string that identifies the target tag to retain.  The expression must resolve to exactly one element for successful filtering.

-ERRORS-
Okay: The filtering operation completed successfully and the XML structure now contains only the specified subtree.
NullArgs: The XPath parameter was NULL or empty.
Search: No matching tag could be found for the specified XPath expression.

*********************************************************************************************************************/

static ERR XML_Filter(extXML *Self, struct xml::Filter *Args)
{
   if ((!Args) or (!Args->XPath)) return ERR::NullArgs;

   if (Self->findTag(Args->XPath) IS ERR::Okay) {
      auto new_tags = TAGS(Self->Cursor, Self->Cursor + 1);
      Self->Tags = std::move(new_tags);
      Self->modified();
      return ERR::Okay;
   }
   else return ERR::Search;
}

/*********************************************************************************************************************

-METHOD-
FindTag: Searches for XML elements using XPath expressions with optional callback processing.

The FindTag method provides the primary mechanism for locating XML elements within the document structure using
XPath 1.0 compatible expressions.  The method supports both single-result queries and comprehensive tree traversal
with callback-based processing for complex operations.

The method supports comprehensive XPath syntax including absolute paths, attribute matching, content matching (a Parasol
extension), wildcarding, deep scanning with double-slash notation, indexed access and complex expressions with multiple
criteria.

When no callback function is provided, FindTag returns the first matching element and terminates the search
immediately.  This is optimal for simple queries where only the first occurrence is required.

When a callback function is specified, FindTag continues searching through the entire document structure, calling the
provided function for each matching element.  This enables comprehensive processing of all matching elements in a
single traversal.

The C++ prototype for the callback function is `ERR Function(*XML, XMLTag &Tag, CSTRING Attrib)`.

The callback should return `ERR::Okay` to continue processing, or `ERR::Terminate` to halt the search immediately.  All
other error codes are ignored to maintain search robustness.

-INPUT-
cstr XPath: A valid XPath expression string conforming to XPath 1.0 syntax with Parasol extensions.  Must not be NULL or empty.
ptr(func) Callback: Optional pointer to a callback function for processing multiple matches.
&int Result: Pointer to an integer that will receive the unique ID of the first matching tag.  Only valid when no callback is provided.

-ERRORS-
Okay: A matching tag was found (or callback processing completed successfully).
NullArgs: The XPath parameter was NULL or the Result parameter was NULL when no callback was provided.
NoData: The XML document contains no data to search.
Search: No matching tag could be found for the specified XPath expression.

*********************************************************************************************************************/

static ERR XML_FindTag(extXML *Self, struct xml::FindTag *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->XPath)) return ERR::NullArgs;
   if ((Self->Flags & XMF::LOG_ALL) != XMF::NIL) log.msg("XPath: %s", Args->XPath);
   if (Self->Tags.empty()) return ERR::NoData;

   if (Self->findTag(Args->XPath, Args->Callback) IS ERR::Okay) {
      if ((Self->Flags & XMF::LOG_ALL) != XMF::NIL) log.msg("Found tag %d, Attrib: %s", Self->Cursor->ID, Self->Attrib.c_str());
      Args->Result = Self->Cursor->ID;
      return ERR::Okay;
   }
   else if (Args->Callback) return ERR::Okay;
   else {
      if ((Self->Flags & XMF::LOG_ALL) != XMF::NIL) log.msg("Failed to find tag through XPath.");
      return ERR::Search;
   }
}

//********************************************************************************************************************

static ERR XML_Free(extXML *Self)
{
   if (Self->Path)      { FreeResource(Self->Path); Self->Path = nullptr; }
   if (Self->Statement) { FreeResource(Self->Statement); Self->Statement = nullptr; }
   Self->~extXML();
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
GetAttrib: Retrieves the value of a specific XML attribute from a tagged element.

The GetAttrib method provides efficient access to individual attribute values within XML elements.  Given a tag
identifier and attribute name, the method performs a case-insensitive search through the element's attribute
collection and returns the corresponding value.


When a specific attribute name is provided, the method searches through all attributes of the target tag.  The search
is case-insensitive to accommodate XML documents with varying capitalisation conventions.

When the attribute name is NULL or empty, the method returns the tag name itself, providing convenient access to
element names without requiring separate API calls.

<header>Performance Considerations</header>

For applications requiring frequent attribute access or high-performance scenarios, C++ developers should consider
direct access to the !XMLAttrib structure array.  This bypasses the method call overhead and provides immediate
access to all attributes simultaneously.

The method performs a linear search through the attribute collection, so performance scales with the number of
attributes per element.  For elements with many attributes, caching frequently accessed values may improve performance.

<header>Data Integrity</header>

The returned string pointer references internal XML object memory and remains valid until the XML structure is
modified.  Callers should not attempt to modify or free the returned string.  For persistent storage, the string
content should be copied to application-managed memory.

-INPUT-
int Index: The unique identifier of the XML tag to search.  This must correspond to a valid tag ID as returned by methods such as #FindTag().
cstr Attrib: The name of the attribute to retrieve (case insensitive).  If NULL or empty, the element's tag name is returned instead.
&cstr Value: Pointer to a string pointer that will receive the attribute value.  Set to NULL if the specified attribute does not exist.

-ERRORS-
Okay: The attribute was successfully found and its value returned.
NullArgs: Required arguments were not specified correctly.
NotFound: Either the specified tag Index does not exist, or the named attribute was not found within the tag.

*********************************************************************************************************************/

static ERR XML_GetAttrib(extXML *Self, struct xml::GetAttrib *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);

   auto tag = Self->getTag(Args->Index);
   if (!tag) return log.warning(ERR::NotFound);

   if ((!Args->Attrib) or (!Args->Attrib[0])) {
      Args->Value = tag->Attribs[0].Name.c_str();
      return ERR::Okay;
   }

   for (auto &attrib : tag->Attribs) {
      if (pf::iequals(Args->Attrib, attrib.Name)) {
         Args->Value = attrib.Value.c_str();
         log.trace("Attrib %s = %s", Args->Attrib, Args->Value);
         return ERR::Okay;
      }
   }

   if ((Self->Flags & XMF::LOG_ALL) != XMF::NIL) log.msg("Attrib %s not found in tag %d", Args->Attrib, Args->Index);
   return ERR::NotFound;
}

/*********************************************************************************************************************

-ACTION-
GetKey: Retrieves data from an xml object.

The XML class uses key-values for the execution of XPath queries.  Documentation of the XPath standard is out
of the scope for this document, however the following examples illustrate the majority of uses for this query language
and a number of special instructions that we support:

<types type="Path">
<type name="/menu/submenu">Return the content of the submenu tag whose parent is the first menu.</>
<type name="xpath:/menu[2]/submenu">Return the content of the submenu tag whose parent is the third menu.</>
<type name="count:/menu">Return a count of all menu tags at the root level.</>
<type name="xml:/menu/window/@title">Return the value of the title attribute from the window tag.</>
<type name="content:/menu/window(@title='foo')">Return the content of the window tag which has title `foo`.</>
<type name="extract:/menu/window(@title='bar')">Extract all XML starting from the window tag that has title `bar`.</>
<type name="extract:/menu//window(=apple)">Extract all XML from the first window tag found anywhere inside `&lt;menu&gt;` that contains content `apple`.</>
<type name="extract-under:/menu/window(@title='bar')">Extract all XML underneath the window tag that has title `bar`.</>
<type name="exists:/menu/@title">Return `1` if a menu with a title attribute can be matched, otherwise `0`.</>
<type name="contentexists:/menu">Return `1` if if the immediate child tags of the XPath contain text (white space is ignored).</>
<type name="//window">Return content of the first window discovered at any branch of the XML tree (double-slash enables flat scanning of the XML tree).</>
</types>

The `xpath`, `xml` and `/` prefixes are all identical in identifying the start of an xpath.  The `content` prefix is
used to specifically extract the content of the tag that matches the xpath.  Square brackets and round brackets may be
used interchangeably for lookups and filtering clauses.
-END-

*********************************************************************************************************************/

static ERR XML_GetKey(extXML *Self, struct acGetKey *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);
   if ((!Args->Key) or (!Args->Value) or (Args->Size < 1)) return log.warning(ERR::NullArgs);
   if (!Self->initialised()) return log.warning(ERR::NotInitialised);

   CSTRING field = Args->Key;
   Args->Value[0] = 0;

   if (pf::startswith("count:", field)) {
      tlXMLCounter = 0;
      auto call = C_FUNCTION(xml_count);
      auto error = Self->findTag(field+6, &call);
      if (error IS ERR::Okay) {
         Args->Value[pf::strcopy(std::to_string(tlXMLCounter), Args->Value, Args->Size)] = 0;
         return ERR::Okay;
      }
      else return error;
   }
   else if (pf::startswith("exists:", field)) {
      Args->Value[0] = '0';
      Args->Value[1] = 0;

      if (Self->findTag(field+7) != ERR::Okay) return ERR::Okay;

      if (!Self->Attrib.empty()) {
         for (auto &scan : Self->Cursor->Attribs) {
            if (pf::iequals(scan.Name, Self->Attrib)) {
               Args->Value[0] = '1';
               break;
            }
         }
      }
      else Args->Value[0] = '1';

      return ERR::Okay;
   }
   else if (pf::startswith("contentexists:", field)) {
      Args->Value[0] = '0';
      Args->Value[1] = 0;

      if (Self->findTag(field+14) != ERR::Okay) return ERR::Okay;

      for (auto &scan : Self->Cursor->Children) {
         if (scan.Attribs[0].isContent()) {
            auto str = scan.Attribs[0].Value.c_str();
            while (*str) {
               if (*str > 0x20) {
                  Args->Value[0] = '1';
                  return ERR::Okay;
               }
               str++;
            }
         }
      }

      return ERR::Okay;
   }
   else if ((pf::startswith("xpath:", field)) or
            (pf::startswith("xml:", field)) or
            (pf::startswith("content:", field)) or
            (pf::startswith("extract:", field)) or
            (pf::startswith("extract-under:", field)) or
            (field[0] IS '/')) {
      int j;
      for (j=0; field[j] and (field[j] != '/'); j++) j++;

      if (Self->findTag(field+j) != ERR::Okay) {
         log.msg("Failed to lookup tag '%s'", field+j);
         return ERR::Search;
      }

      if (!Self->Attrib.empty()) { // Extract attribute value
         for (auto &scan : Self->Cursor->Attribs) {
            if (pf::iequals(scan.Name, Self->Attrib)) {
               pf::strcopy(scan.Value, Args->Value, Args->Size);
               return ERR::Okay;
            }
         }
         return ERR::Failed;
      }
      else {
         // Extract tag content

         uint8_t extract;

         if (pf::startswith("content:", field)) extract = 1; // 'In-Depth' content extract.
         else if (pf::startswith("extract:", field)) extract = 2;
         else if (pf::startswith("extract-under:", field)) extract = 3;
         else extract = 0;

         Args->Value[0] = 0;
         if (extract IS 1) {
            int output;
            return get_all_content(Self, *Self->Cursor, Args->Value, Args->Size, output);
         }
         else if (extract IS 2) {
            STRING str;
            ERR error = Self->serialise(Self->Cursor->ID, XMF::NIL, &str);
            if (error IS ERR::Okay) {
               pf::strcopy(str, Args->Value, Args->Size);
               FreeResource(str);
            }

            return error;
         }
         else if (extract IS 3) {
            STRING str;
            ERR error = Self->serialise(Self->Cursor->Children[0].ID, XMF::INCLUDE_SIBLINGS, &str);
            if (error IS ERR::Okay) {
               pf::strcopy(str, Args->Value, Args->Size);
               FreeResource(str);
            }

            return error;
         }
         else { // 'Immediate' content extract (not deep)
            if (!Self->Cursor->Children.empty()) {
               int j = 0;
               for (auto &scan : Self->Cursor->Children) {
                  if (scan.Attribs[0].isContent()) j += pf::strcopy(scan.Attribs[0].Value, Args->Value+j, Args->Size-j);
               }
               if (j >= Args->Size-1) log.warning(ERR::BufferOverflow);
            }
         }
      }

      return ERR::Okay;
   }
   else {
      log.msg("Unsupported field \"%s\".", field);
      return ERR::UnsupportedField;
   }
}

/*********************************************************************************************************************

-METHOD-
GetContent: Extracts the immediate text content of an XML element, excluding nested tags.

The GetContent method provides efficient extraction of text content from XML elements using a shallow parsing approach.
It retrieves only the immediate text content of the specified element, deliberately excluding any text contained within
nested child elements.  This behaviour is valuable for scenarios requiring precise content extraction without
recursive tag processing.

Consider the following XML structure:

<pre>
&lt;body&gt;
  Hello
  &lt;bold&gt;emphasis&lt;/bold&gt;
  world!
&lt;/body&gt;
</pre>

The GetContent method would extract `Hello world!` and deliberately exclude `emphasis` since it is contained within the
nested `&lt;bold&gt;` element.

<header>Comparison with Deep Extraction</header>

For scenarios requiring complete text extraction including all nested content, use the #Serialise() method with
appropriate flags to perform deep content analysis.  The GetContent method is optimised for cases where nested tag
content should be excluded from the result.

If the resulting content exceeds the buffer capacity, the result will be truncated but remain null-terminated.

It is recommended that C++ programs bypass this method and access the !XMLAttrib structure directly.

-INPUT-
int Index: The unique identifier of the XML element from which to extract content.  This must correspond to a valid tag ID as returned by search methods.
buf(str) Buffer: Pointer to a pre-allocated character buffer that will receive the extracted content string.  Must not be NULL.
bufsize Length: The size of the provided buffer in bytes, including space for null termination.  Must be at least 1.

-ERRORS-
Okay: The content string was successfully extracted and copied to the buffer.
NullArgs: Either the Buffer parameter was NULL or other required arguments were missing.
Args: The Length parameter was less than 1, indicating insufficient buffer space.
NotFound: The tag identified by Index does not exist in the XML structure.
BufferOverflow: The buffer was not large enough to hold the complete content.  The result is truncated but valid.

*********************************************************************************************************************/

static ERR XML_GetContent(extXML *Self, struct xml::GetContent *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Buffer)) return log.warning(ERR::NullArgs);
   if (Args->Length < 1) return log.warning(ERR::Args);

   if (auto tag = Self->getTag(Args->Index)) {
      return get_content(Self, *tag, Args->Buffer, Args->Length);
   }
   else return log.warning(ERR::NotFound);
}

/*********************************************************************************************************************

-METHOD-
GetNamespaceURI: Retrieve the namespace URI for a given namespace UID.

This method retrieves the original namespace URI string for a given namespace UID.

-INPUT-
uint NamespaceID: The UID of the namespace.
&cstr Result: Pointer to a string pointer that will receive the namespace URI.

-ERRORS-
Okay: The namespace URI was successfully retrieved.
NullArgs: Required arguments were not specified correctly.
Search: No namespace found for the specified UID.

-END-

*********************************************************************************************************************/

static ERR XML_GetNamespaceURI(extXML *Self, struct xml::GetNamespaceURI *Args)
{
   pf::Log log;

   if (not Args) return log.warning(ERR::NullArgs);

   auto uri = Self->getNamespaceURI(Args->NamespaceID);
   if (!uri) return log.warning(ERR::Search);

   Args->Result = uri->c_str();
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
GetTag: Returns a pointer to the !XMLTag structure for a given tag index.

This method will return the !XMLTag structure for a given tag `Index`.  The `Index` is checked to ensure it is valid
prior to retrieval, and an `ERR::OutOfRange` error will be returned if it is invalid.

-INPUT-
int Index:  The index of the tag that is being retrieved.
&struct(*XMLTag) Result: The !XMLTag is returned in this parameter.

-ERRORS-
Okay
NullArgs
NotFound: The Index is not recognised.

*********************************************************************************************************************/

static ERR XML_GetTag(extXML *Self, struct xml::GetTag *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);

   if ((Args->Result = Self->getTag(Args->Index))) return ERR::Okay;
   else return ERR::NotFound;
}

//********************************************************************************************************************

static ERR XML_Init(extXML *Self)
{
   pf::Log log;

   if (Self->isSubClass()) return ERR::Okay; // Break here for sub-classes to perform initialisation

   if (Self->Statement) {
      Self->LineNo = 1;
      if ((Self->ParseError = txt_to_xml(Self, Self->Tags, Self->Statement)) != ERR::Okay) {
         // Return NoSupport to defer parsing to other data handlers
         if (Self->ParseError IS ERR::InvalidData) return ERR::NoSupport;

         log.warning("XML parsing error #%d: %s", int(Self->ParseError), GetErrorMsg(Self->ParseError));
      }

      FreeResource(Self->Statement);
      Self->Statement = nullptr;

      return Self->ParseError;
   }
   else if ((Self->Path) or (Self->Source)) {
      if ((Self->Flags & XMF::NEW) != XMF::NIL) {
         return ERR::Okay;
      }
      else if (parse_source(Self) != ERR::Okay) {
         log.warning("XML parsing error: %s [File: %s]", GetErrorMsg(Self->ParseError), Self->Path ? Self->Path : "Object");
         return Self->ParseError;
      }
      else return ERR::Okay;
   }
   else {
      // NOTE: We do not fail if no data has been loaded into the XML object, the developer may be creating an XML data
      // structure from scratch, or could intend to send us information later.

      if ((Self->Flags & XMF::NEW) IS XMF::NIL) log.msg("Warning: No content given.");
      return ERR::Okay;
   }
}

/*********************************************************************************************************************

-METHOD-
InsertContent: Inserts text content into the XML document structure at specified positions.

The InsertContent method will insert content strings into any position within the XML tree.  A content string
must be provided in the `Content` parameter and the target insertion point is specified in the `Index` parameter.
An insertion point relative to the target index must be specified in the `Where` parameter.  The new tags can be
inserted as a child of the target by using a `Where` value of `XMI::CHILD`.  To insert behind or after the target, use
`XMI::PREV` or `XMI::NEXT`.

To modify existing content, call #SetAttrib() instead.

-INPUT-
int Index: The unique identifier of the target XML element that will serve as the reference point for insertion.
int(XMI) Where: Specifies the insertion position relative to the target element.  Use PREV or NEXT for sibling insertion, or CHILD for child content insertion.
cstr Content: The text content to insert.  Special XML characters will be automatically escaped to ensure document validity.
&int Result: Pointer to an integer that will receive the unique identifier of the newly created content node.

-ERRORS-
Okay: The content was successfully inserted and a new content node was created.
NullArgs: Required parameters were NULL or not properly specified.
NotFound: The target Index does not correspond to a valid XML element.
ReadOnly: The XML object is in read-only mode and cannot be modified.
Args: The Where parameter specifies an invalid insertion position.

*********************************************************************************************************************/

static ERR XML_InsertContent(extXML *Self, struct xml::InsertContent *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);
   if (Self->ReadOnly) return log.warning(ERR::ReadOnly);
   if ((Self->Flags & XMF::LOG_ALL) != XMF::NIL) log.branch("Index: %d, Insert: %d", Args->Index, int(Args->Where));

   auto src = Self->getTag(Args->Index);
   if (!src) return log.warning(ERR::NotFound);

   std::ostringstream buffer;
   output_attribvalue(std::string(Args->Content), buffer);
   XMLTag content(glTagID++, 0, { { "", buffer.str() } });

   if (Args->Where IS XMI::NEXT) {
      CURSOR it;
      if (auto tags = Self->getInsert(src, it)) tags->insert(it, content);
      else return log.warning(ERR::NotFound);
   }
   else if (Args->Where IS XMI::CHILD) {
      src->Children.insert(src->Children.begin(), content);
   }
   else if (Args->Where IS XMI::PREV) {
      CURSOR it;
      if (auto tags = Self->getInsert(src, it)) {
        if (it IS tags->begin()) tags->insert(it, content);
        else tags->insert(it - 1, content);
      }
      else return log.warning(ERR::NotFound);
   }
   else return log.warning(ERR::Args);

   Args->Result = content.ID;
   Self->modified();
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
InsertXML: Parse an XML string and insert it in the XML tree.

The InsertXML() method is used to translate and insert a new set of XML tags into any position within the XML tree.  A
standard XML statement must be provided in the XML parameter and the target insertion point is specified in the Index
parameter.  An insertion point relative to the target index must be specified in the `Where` parameter.  The new tags
can be inserted as a child of the target by using a `Where` value of `XMI::CHILD`.  Use `XMI::CHILD_END` to insert at the end
of the child list.  To insert behind or after the target, use `XMI::PREV` or `XMI::NEXT`.

-INPUT-
int Index: The new data will target the tag specified here.
int(XMI) Where: Use `PREV` or `NEXT` to insert behind or ahead of the target tag.  Use `CHILD` or `CHILD_END` for a child insert.
cstr XML: An XML statement to parse.
&int Result: The resulting tag index.

-ERRORS-
Okay: The statement was added successfully.
Args: The Where parameter specifies an invalid insertion position.
NullArgs: Required parameters were NULL or not properly specified.
NotFound: The target Index does not correspond to a valid XML element.
ReadOnly: Changes to the XML data are not permitted.
NoData: The provided XML statement parsed to an empty result.

*********************************************************************************************************************/

static ERR XML_InsertXML(extXML *Self, struct xml::InsertXML *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);
   if (Self->ReadOnly) return log.warning(ERR::ReadOnly);
   if ((Self->Flags & XMF::LOG_ALL) != XMF::NIL) log.branch("Index: %d, Where: %d, XML: %.40s", Args->Index, int(Args->Where), Args->XML);

   auto src = Self->getTag(Args->Index);
   if (!src) return log.warning(ERR::NotFound);

   ERR error;
   TAGS insert;
   if ((error = txt_to_xml(Self, insert, Args->XML)) != ERR::Okay) return log.warning(error);
   if (insert.empty()) return ERR::NoData;
   auto result = insert[0].ID;

   if (Args->Where IS XMI::NEXT) {
      CURSOR it;
      if (auto tags = Self->getInsert(src, it)) {
         tags->insert(it + 1, insert.begin(), insert.end());
      }
      else return log.warning(ERR::NotFound);
   }
   else if (Args->Where IS XMI::PREV) {
      CURSOR it;
      if (auto tags = Self->getInsert(src, it)) {
        if (it IS tags->begin()) tags->insert(it, insert.begin(), insert.end());
        else tags->insert(it, insert.begin(), insert.end());
      }
      else return log.warning(ERR::NotFound);
   }
   else if (Args->Where IS XMI::CHILD) {
      src->Children.insert(src->Children.begin(), insert.begin(), insert.end());
   }
   else if (Args->Where IS XMI::CHILD_END) {
      src->Children.insert(src->Children.end(), insert.begin(), insert.end());
   }
   else if (Args->Where IS XMI::END) {
      if (auto tags = Self->getTags(src)) {
         tags->insert(tags->end() - 1, insert.begin(), insert.end());
      }
      else return log.warning(ERR::NotFound);
   }
   else return log.warning(ERR::Args);

   Args->Result = result;
   Self->modified();
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
InsertXPath: Inserts an XML statement in an XML tree.

The InsertXPath method is used to translate and insert a new set of XML tags into any position within the XML tree.  A
standard XML statement must be provided in the XML parameter and the target insertion point is referenced as a valid
`XPath` location string.  An insertion point relative to the `XPath` target must be specified in the `Where` parameter.  The
new tags can be inserted as a child of the target by using an Insert value of `XMI::CHILD` or `XMI::CHILD_END`.  To insert
behind or after the target, use `XMI::PREV` or `XMI::NEXT`.

-INPUT-
cstr XPath: An XPath string that refers to the target insertion point.
int(XMI) Where: Use `PREV` or `NEXT` to insert behind or ahead of the target tag.  Use `CHILD` for a child insert.
cstr XML: The statement to process.
&int Result: The index of the new tag is returned here.

-ERRORS-
Okay: The XML statement was successfully inserted at the specified XPath location.
NullArgs: Required parameters were NULL or not properly specified.
Search: The XPath could not be resolved to a valid location.
ReadOnly: The XML object is in read-only mode and cannot be modified.

*********************************************************************************************************************/

ERR XML_InsertXPath(extXML *Self, struct xml::InsertXPath *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->XPath) or (!Args->XML)) return log.warning(ERR::NullArgs);
   if (Self->ReadOnly) return log.warning(ERR::ReadOnly);

   log.branch("Insert: %d, XPath: %s", int(Args->Where), Args->XPath);

   if (Self->findTag(Args->XPath) IS ERR::Okay) {
      struct xml::InsertXML insert = { .Index = Self->Cursor->ID, .Where = Args->Where, .XML = Args->XML };
      if (auto error = XML_InsertXML(Self, &insert); error IS ERR::Okay) {
         Args->Result = insert.Result;
         return ERR::Okay;
      }
      else return error;
   }
   else return ERR::Search;
}

/*********************************************************************************************************************

-METHOD-
MoveTags: Move an XML tag group to a new position in the XML tree.

This method is used to move XML tags within the XML tree structure.  It supports the movement of single and groups of
tags from one index to another.  The client must supply the index of the tag that will be moved and the index of the
target tag.  All child tags of the source will be included in the move.

An insertion point relative to the target index must be specified in the `Where` parameter.  The source tag can be
inserted as a child of the destination by using a `Where` of `XMI::CHILD`.  To insert behind or after the target, use
`XMI::PREV` or `XMI::NEXT`.

-INPUT-
int Index: Index of the source tag to be moved.
int Total: The total number of sibling tags (including the targeted tag) to be moved from the source index.  Minimum value of 1.
int DestIndex: The destination tag index.  If the index exceeds the total number of tags, the value will be automatically limited to the last tag index.
int(XMI) Where: Use `PREV` or `NEXT` to insert behind or ahead of the target tag.  Use `CHILD` for a child insert.

-ERRORS-
Okay: Tags were moved successfully.
Args: Invalid parameter values were provided.
NullArgs: Required parameters were NULL or not properly specified.
NotFound: Either the source or destination tag index does not exist.
ReadOnly: The XML object is in read-only mode and cannot be modified.
SanityCheckFailed: An internal consistency check failed during the move operation.
-END-

*********************************************************************************************************************/

static ERR XML_MoveTags(extXML *Self, struct xml::MoveTags *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);
   if (Self->ReadOnly) return log.warning(ERR::ReadOnly);
   if (Args->Total < 1) return log.warning(ERR::Args);
   if (Args->Index IS Args->DestIndex) return ERR::Okay;

   if ((Args->DestIndex > Args->Index) and (Args->DestIndex < Args->Index + Args->Total)) {
      return log.warning(ERR::Args);
   }

   auto dest = Self->getTag(Args->DestIndex);
   if (!dest) return log.warning(ERR::NotFound);

   // Take a copy of the source tags

   CURSOR it;
   auto src_tags = Self->getInsert(Args->Index, it);
   if (!src_tags) return log.warning(ERR::NotFound);

   int total = Args->Total;
   TAGS copy;
   unsigned si;
   for (si=0; si < src_tags->size(); si++) {
      if (src_tags[0][si].ID IS it->ID) {
         if (si + total > src_tags->size()) total = int(src_tags->size() - si);
         copy = TAGS(src_tags->begin() + si, src_tags->begin() + si + total);
         break;
      }
   }

   if (copy.empty()) return log.warning(ERR::SanityCheckFailed);
   if (si >= src_tags->size()) return log.warning(ERR::SanityCheckFailed);

   switch (Args->Where) {
      case XMI::PREV: {
         CURSOR it;
         if (auto target = Self->getInsert(dest, it)) {
            target->insert(it, copy.begin(), copy.end());
         }
         else return log.warning(ERR::NotFound);
         break;
      }

      case XMI::NEXT: {
         CURSOR it;
         if (auto target = Self->getInsert(dest, it)) {
            target->insert(it + 1, copy.begin(), copy.end());
         }
         else return log.warning(ERR::NotFound);
         break;
      }

      case XMI::CHILD:
         dest->Children.insert(dest->Children.begin(), copy.begin(), copy.end());
         break;

      case XMI::CHILD_END:
         dest->Children.insert(dest->Children.end(), copy.begin(), copy.end());
         break;

      default:
         return log.warning(ERR::Args);
   }

   src_tags->erase(src_tags->begin() + si, src_tags->begin() + si + Args->Total);

   Self->modified();
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR XML_NewPlacement(extXML *Self)
{
   new (Self) extXML;
   Self->LineNo = 1;
   Self->ParseError = ERR::Okay;
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
RegisterNamespace: Register a namespace URI and return its UID.

This method registers a namespace URI and returns a UID that can be used to identify the namespace
efficiently throughout the XML document.

-INPUT-
cstr URI: The namespace URI to register. Must not be NULL or empty.
&uint Result: Pointer to an integer that will receive the UID for the namespace URI.

-ERRORS-
Okay: The namespace was successfully registered.
NullArgs: Required arguments were not specified correctly.
Failed: The URI was empty or invalid.

-END-

*********************************************************************************************************************/

static ERR XML_RegisterNamespace(extXML *Self, struct xml::RegisterNamespace *Args)
{
   pf::Log log;

   if (!Args or !Args->URI) return log.warning(ERR::NullArgs);

   // Register the namespace URI and get its hash
   auto hash = Self->registerNamespace(Args->URI);
   if (hash IS 0) return log.warning(ERR::Failed);

   Args->Result = hash;
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
RemoveTag: Removes tag(s) from the XML structure.

The RemoveTag method is used to remove one or more tags from an XML structure.  Child tags will automatically be
discarded as a consequence of using this method, in order to maintain a valid XML structure.

This method is capable of deleting multiple tags if the `Total` parameter is set to a value greater than 1.  Each
consecutive tag and its children following the targeted tag will be removed from the XML structure until the count is
exhausted. This is useful for mass delete operations.

This method is volatile and will destabilise any cached address pointers that have been acquired from the XML object.

-INPUT-
int Index: Reference to the tag that will be removed.
int Total: The total number of sibling (neighbouring) tags that should also be deleted.  A value of one or less will remove only the indicated tag and its children.  The total may exceed the number of tags actually available, in which case all tags up to the end of the branch will be affected.

-ERRORS-
Okay: The tag(s) were successfully removed.
NullArgs: Required parameters were NULL or not properly specified.
NotFound: The specified tag Index does not exist in the XML structure.
ReadOnly: The XML object is in read-only mode and cannot be modified.
-END-

*********************************************************************************************************************/

static ERR XML_RemoveTag(extXML *Self, struct xml::RemoveTag *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);
   if (Self->ReadOnly) return log.warning(ERR::ReadOnly);
   if ((Self->Flags & XMF::LOCK_REMOVE) != XMF::NIL) return log.warning(ERR::ReadOnly);

   int count = Args->Total;
   if (count < 1) count = 1;

   if (auto tag = Self->getTag(Args->Index)) {
      if (tag->ParentID) {
         if (auto parent = Self->getTag(tag->ParentID)) {
            for (auto it=parent->Children.begin(); it != parent->Children.end(); it++) {
               if (it->ID IS Args->Index) {
                  parent->Children.erase(it, it + count);
                  Self->modified();
                  return ERR::Okay;
               }
            }
         }
      }
      else { // No parent, access the root level
         for (auto it=Self->Tags.begin(); it != Self->Tags.end(); it++) {
            if (it->ID IS Args->Index) {
               Self->Tags.erase(it, it + count);
               Self->modified();
               return ERR::Okay;
            }
         }
      }
   }

   return log.warning(ERR::NotFound);
}

/*********************************************************************************************************************

-METHOD-
RemoveXPath: Removes tag(s) from the XML structure, using an xpath lookup.

The RemoveXPath method is used to remove one or more tags from an XML structure.  Child tags will automatically be
discarded as a consequence of using this method, in order to maintain a valid XML structure.

Individual tag attributes can also be removed if an attribute is referenced at the end of the `XPath`.

The removal routine will be repeated so that each tag that matches the XPath will be deleted, or the `Limit` is reached.

This method is volatile and will destabilise any cached address pointers that have been acquired from the XML object.

-INPUT-
cstr XPath: An XML path string.
int Limit: The maximum number of matching tags to delete.  A value of one or zero will remove only the indicated tag and its children.  A value of -1 removes all matching tags.

-ERRORS-
Okay: The matching tag(s) or attribute(s) were successfully removed.
NullArgs: Required parameters were NULL or not properly specified.
ReadOnly: The XML object is in read-only mode and cannot be modified.
NoData: The XML document contains no data to process.
-END-

*********************************************************************************************************************/

static ERR XML_RemoveXPath(extXML *Self, struct xml::RemoveXPath *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->XPath)) return ERR::NullArgs;

   if (Self->Tags.empty()) return ERR::NoData;
   if (Self->ReadOnly) return log.warning(ERR::ReadOnly);
   if ((Self->Flags & XMF::LOCK_REMOVE) != XMF::NIL) return log.warning(ERR::ReadOnly);

   auto limit = Args->Limit;
   if (limit IS -1) limit = 0x7fffffff;
   else if (!limit) limit = 1;

   while (limit > 0) {
      if (Self->findTag(Args->XPath) != ERR::Okay) return ERR::Okay; // Assume tag already removed if no match

      if (!Self->Attrib.empty()) { // Remove an attribute
         for (int a=0; a < std::ssize(Self->Cursor->Attribs); a++) {
            if (pf::iequals(Self->Attrib, Self->Cursor->Attribs[a].Name)) {
               Self->Cursor->Attribs.erase(Self->Cursor->Attribs.begin() + a);
               break;
            }
         }
      }
      else if (Self->Cursor->ParentID) {
         if (auto parent = Self->getTag(Self->Cursor->ParentID)) {
            for (auto it=parent->Children.begin(); it != parent->Children.end(); it++) {
               if (Self->Cursor->ID IS it->ID) {
                  parent->Children.erase(it);
                  break;
               }
            }
         }
      }
      else {
         for (auto it=Self->Tags.begin(); it != Self->Tags.end(); it++) {
            if (Self->Cursor->ID IS it->ID) {
               Self->Tags.erase(it);
               break;
            }
         }
      }

      limit--;
   }

   Self->modified();
   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
Reset: Clears the information held in an XML object.
-END-
*********************************************************************************************************************/

static ERR XML_Reset(extXML *Self)
{
   return acClear(Self);
}

/*********************************************************************************************************************

-METHOD-
ResolvePrefix: Resolve a namespace prefix to the UID of its namespace URI within a tag's scope.

This method resolves a namespace prefix to its corresponding URI by examining namespace declarations within the 
specified tag's hierarchical scope. The resolution process:

<list type="ordered">
<li>First checks the global prefix map for efficiency.</li>
<li>If not found, walks up the tag hierarchy from the specified tag to the root.</li>
<li>Examines xmlns:prefix and xmlns attributes in each tag to find the declaration.</li>
<li>Returns the hash of the first matching namespace URI found.</li>
</list>

This approach correctly handles nested namespace scopes and prefix redefinitions.

-INPUT-
cstr Prefix: The namespace prefix to resolve. Use empty string for default namespace.
int TagID: The tag ID defining the starting scope for namespace resolution.
&uint Result: Pointer to an integer that will receive the resolved namespace hash.

-ERRORS-
Okay: The prefix was successfully resolved.
NullArgs: Required arguments were not specified correctly.
NotFound: The specified tag was not found.
Search: The prefix could not be resolved in any accessible scope.

-END-

*********************************************************************************************************************/

static ERR XML_ResolvePrefix(extXML *Self, struct xml::ResolvePrefix *Args)
{
   pf::Log log;

   if ((not Args) or (not Args->Prefix)) return log.warning(ERR::NullArgs);

   // First check the current global prefix map (for efficiency in common cases)
   auto it = Self->Prefixes.find(Args->Prefix);
   if (it != Self->Prefixes.end()) {
      Args->Result = it->second;
      return ERR::Okay;
   }

   // If not found globally, walk up the tag hierarchy to find namespace declarations
   for (auto tag = Self->getTag(Args->TagID); tag; tag = Self->getTag(tag->ParentID)) {
      // Check this tag's attributes for namespace declarations
      for (size_t i = 1; i < tag->Attribs.size(); i++) {
         const auto &attrib = tag->Attribs[i];

         // Check for xmlns:prefix="uri" declarations
         if (attrib.Name.starts_with("xmlns:") and attrib.Name.size() > 6) {
            if (attrib.Name.substr(6) IS Args->Prefix) {
               // Found the prefix declaration, return its namespace hash
               Args->Result = pf::strhash(attrib.Value);
               return ERR::Okay;
            }
         }
         // Check for default namespace if looking for empty prefix
         else if ((attrib.Name IS "xmlns") and ((not Args->Prefix) or (not Args->Prefix[0]))) {
            Args->Result = pf::strhash(attrib.Value);
            return ERR::Okay;
         }
      }

      if (!tag->ParentID) break; // Reached root
   }

   return log.warning(ERR::Search);
}

/*********************************************************************************************************************
-ACTION-
SaveToObject: Saves XML data to a storage object (e.g. @File).
-END-
*********************************************************************************************************************/

static ERR XML_SaveToObject(extXML *Self, struct acSaveToObject *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Dest)) return log.warning(ERR::NullArgs);
   if (Self->Tags.size() <= 0) return ERR::Okay;

   log.traceBranch("To: %d", Args->Dest->UID);

   STRING str;
   if (auto error = Self->serialise(0, XMF::READABLE|XMF::INCLUDE_SIBLINGS, &str); error IS ERR::Okay) {
      if (acWrite(Args->Dest, str, strlen(str), nullptr) != ERR::Okay) error = ERR::Write;
      FreeResource(str);
      return error;
   }
   else return error;
}

/*********************************************************************************************************************

-METHOD-
Serialise: Serialise part of the XML tree to an XML string.

The Serialise() method will serialise all or part of the XML data tree to a string.

The string will be allocated as a memory block and stored in the Result parameter.  It must be freed once the data
is no longer required.

-INPUT-
int Index: Index to a source tag for which serialisation will start.  Set to zero to serialise the entire tree.
int(XMF) Flags: Use `INCLUDE_SIBLINGS` to include siblings of the tag found at Index.
!str Result: The resulting string is returned in this parameter.

-ERRORS-
Okay: The XML string was successfully serialised.
NullArgs: Required parameters were NULL or not properly specified.
NoData: No information has been loaded into the XML object.
NotFound: The specified tag Index does not exist in the XML structure.
AllocMemory: Failed to allocate memory for the XML string result.

*********************************************************************************************************************/

static ERR XML_Serialise(extXML *Self, struct xml::Serialise *Args)
{
   pf::Log log;

   if (Self->Tags.empty()) return log.warning(ERR::NoData);
   if (!Args) return log.warning(ERR::NullArgs);

   log.traceBranch("Tag: %d", Args->Index);

   std::ostringstream buffer;

   auto tag = Args->Index ? Self->getTag(Args->Index) : &Self->Tags[0];
   if (!tag) return log.warning(ERR::NotFound);

   if ((Args->Flags & XMF::INCLUDE_SIBLINGS) != XMF::NIL) {
      if (auto parent = Self->getTag(tag->ParentID)) {
         auto it = parent->Children.begin();
         for (; it != parent->Children.end(); it++) {
            if (it->ID IS Args->Index) break;
         }

         for (; it != parent->Children.end(); it++) {
            serialise_xml(*it, buffer, Args->Flags);
         }
      }
      else {
         auto it = Self->Tags.begin();
         for (; it != Self->Tags.end(); it++) {
            if (it IS tag) break;
         }

         for (; it != Self->Tags.end(); it++) {
            serialise_xml(*it, buffer, Args->Flags);
         }
      }
   }
   else serialise_xml(*tag, buffer, Args->Flags);

   pf::SwitchContext ctx(ParentContext());
   if ((Args->Result = pf::strclone(buffer.str()))) return ERR::Okay;
   else return log.warning(ERR::AllocMemory);
}

/*********************************************************************************************************************

-METHOD-
SetAttrib: Adds, updates and removes XML attributes.

This method is used to update and add attributes to existing XML tags, as well as adding or modifying content.

The data for the attribute is defined in the `Name` and `Value` parameters.  Use an empty string if no data is to be
associated with the attribute.  Set the `Value` pointer to `NULL` to remove the attribute. If both `Name` and `Value` are `NULL`,
an error will be returned.

NOTE: The attribute at position 0 declares the name of the tag and should not normally be accompanied with a value
declaration.  However, if the tag represents content within its parent, then the Name must be set to `NULL` and the
`Value` string will determine the content.

-INPUT-
int Index: Identifies the tag that is to be updated.
int(XMS) Attrib: Either the index number of the attribute that is to be updated, or set to `NEW`, `UPDATE` or `UPDATE_ONLY`.
cstr Name: String containing the new name for the attribute.  If `NULL`, the name will not be changed.  If Attrib is `UPDATE` or `UPDATE_ONLY`, the `Name` is used to find the attribute.
cstr Value: String containing the new value for the attribute.  If `NULL`, the attribute is removed.

-ERRORS-
Okay
NullArgs
Args
OutOfRange: The `Index` or `Attrib` value is out of range.
Search: The attribute, identified by `Name`, could not be found.
ReadOnly: The XML object is read-only.
-END-

*********************************************************************************************************************/

static ERR XML_SetAttrib(extXML *Self, struct xml::SetAttrib *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);
   if (Self->ReadOnly) return log.warning(ERR::ReadOnly);

   log.trace("Tag: %d, Attrib: $%.8x, %s = '%s'", Args->Index, Args->Attrib, Args->Name, Args->Value);

   auto tag = Self->getTag(Args->Index);
   if (!tag) return log.warning(ERR::Search);

   auto cmd = Args->Attrib;
   if ((cmd IS XMS::UPDATE) or (cmd IS XMS::UPDATE_ONLY)) {
      for (auto a = tag->Attribs.begin(); a != tag->Attribs.end(); a++) {
         if (pf::iequals(Args->Name, a->Name)) {
            if (Args->Value) {
               a->Name  = Args->Name;
               a->Value = Args->Value;
            }
            else tag->Attribs.erase(a);
            Self->Modified++;
            return ERR::Okay;
         }
      }

      if (cmd IS XMS::UPDATE) {
         if ((!Args->Value) or (!Args->Value[0])) return ERR::Okay; // User wants to remove a non-existing attribute, so return ERR::Okay
         else { // Create new attribute if name wasn't found
            tag->Attribs.push_back({ Args->Name, Args->Value });
            Self->Modified++;
            return ERR::Okay;
         }
      }
      else return ERR::Search;
   }
   else if (cmd IS XMS::NEW) {
      tag->Attribs.push_back({ Args->Name, Args->Value });
      Self->Modified++;
      return ERR::Okay;
   }

   // Attribute indexing

   if ((int(Args->Attrib) < 0) or (int(Args->Attrib) >= int(tag->Attribs.size()))) return log.warning(ERR::OutOfRange);

   if (Args->Value) {
      if (Args->Name) tag->Attribs[int(Args->Attrib)].Name = Args->Name;
      tag->Attribs[int(Args->Attrib)].Value = Args->Value;
   }
   else {
      if (Args->Attrib IS XMS::NIL) tag->Attribs[int(Args->Attrib)].Value.clear(); // Content is erased when Attrib == 0
      else tag->Attribs.erase(tag->Attribs.begin() + int(Args->Attrib));
   }

   Self->Modified++;
   return ERR::Okay;
}

/*********************************************************************************************************************

-ACTION-
SetKey: Sets attributes and content in the XML tree using XPaths.

Use SetKey to add tag attributes and content using XPaths.  The XPath is specified in the `Key` parameter and the data
is specified in the `Value` parameter.  Setting the Value to `NULL` will remove the attribute or existing content,
while an empty string will keep an attribute but eliminate any associated data.

It is not possible to add new tags using this action - it is only possible to update existing tags.

Please note that making changes to the XML tree will render all previously obtained tag pointers and indexes invalid.

-ERRORS-
Okay
ReadOnly: Changes to the XML structure are not permitted.
Search: Failed to find the tag referenced by the XPath.

*********************************************************************************************************************/

static ERR XML_SetKey(extXML *Self, struct acSetKey *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Key)) return log.warning(ERR::NullArgs);
   if (Self->ReadOnly) return log.warning(ERR::ReadOnly);

   if (Self->findTag(Args->Key) IS ERR::Okay) {
      if (!Self->Attrib.empty()) { // Updating or adding an attribute
         unsigned i;
         for (i=0; i < Self->Cursor->Attribs.size(); i++) {
            if (pf::iequals(Self->Attrib, Self->Cursor->Attribs[i].Name)) break;
         }

         if (i < Self->Cursor->Attribs.size()) Self->Cursor->Attribs[i].Value = Args->Value; // Modify existing
         else Self->Cursor->Attribs.emplace_back(std::string(Self->Attrib), std::string(Args->Value)); // Add new
         Self->Modified++;
      }
      else if (!Self->Cursor->Children.empty()) { // Update existing content
         Self->Cursor->Children[0].Attribs[0].Value = Args->Value;
         Self->Modified++;
      }
      else {
         Self->Cursor->Children.emplace_back(XMLTag(glTagID++, 0, {
            { "", std::string(Args->Value) }
         }));
         Self->modified();
      }

      return ERR::Okay;
   }
   else {
      log.msg("Failed to find '%s'", Args->Key);
      return ERR::Search;
   }
}

/*********************************************************************************************************************

-METHOD-
SetTagNamespace: Set the namespace for a specific XML tag.

This method assigns a namespace to an XML tag using the namespace's UID.

-INPUT-
int TagID: The unique identifier of the XML tag.
int NamespaceID: The UID of the namespace to assign.

-ERRORS-
Okay: The namespace was successfully assigned to the tag.
NullArgs: Required arguments were not specified correctly.
NotFound: The specified tag was not found.

-END-

*********************************************************************************************************************/

static ERR XML_SetTagNamespace(extXML *Self, struct xml::SetTagNamespace *Args)
{
   pf::Log log;

   if (not Args) return log.warning(ERR::NullArgs);

   // Find the tag and assign the namespace
   auto tag = Self->getTag(Args->TagID);
   if (!tag) return log.warning(ERR::NotFound);

   tag->NamespaceID = Args->NamespaceID;
   Self->modified();
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
Sort: Sorts XML tags to your specifications.

The Sort method is used to sort a single branch of XML tags in ascending or descending order.  An `XPath` is required
that refers to the tag containing each item that will be sorted.  To sort the root level, use an `XPath` of `NULL`.

The `Sort` parameter is used to specify a list of sorting instructions.  The format for the `Sort` string is
`Tag:Attrib,Tag:Attrib,...`.  The `Tag` indicates the tag name that should be identified for sorting each node, and
child tags are supported for this purpose.  Wildcard filtering is allowed and a `Tag` value of `*` will match every
tag at the requested `XPath` level.  The optional `Attrib` value names the attribute containing the sort string.  To
sort on content, do not define an `Attrib` value (use the format `Tag,Tag,...`).

-INPUT-
cstr XPath: Sort everything under the specified tag, or `NULL` to sort the entire top level.
cstr Sort: Pointer to a sorting instruction string.
int(XSF) Flags: Optional flags.

-ERRORS-
Okay: The XML object was successfully sorted.
NullArgs: Required parameters were NULL or not properly specified.
Search: The provided XPath failed to locate a tag.
ReadOnly: The XML object is in read-only mode and cannot be modified.
-END-

*********************************************************************************************************************/

static ERR XML_Sort(extXML *Self, struct xml::Sort *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Sort)) return log.warning(ERR::NullArgs);
   if (Self->ReadOnly) return log.warning(ERR::ReadOnly);

   CURSOR tag;
   TAGS *branch;
   if ((!Args->XPath) or (!Args->XPath[0])) {
      branch = &Self->Tags;
      tag = &Self->Tags[0];
      if (!tag) return ERR::Okay;
   }
   else {
      if (Self->findTag(Args->XPath) != ERR::Okay) return log.warning(ERR::Search);
      branch = &Self->Cursor->Children;
   }

   if (branch->size() < 2) return ERR::Okay;

   log.traceBranch("Path: %s, Tag: %s", Args->XPath, Args->Sort);

   std::vector<std::pair<std::string, std::string>> filters;
   std::string cmd(Args->Sort);
   for (unsigned i=0; i < cmd.size(); ) {
      std::string tag_match, attrib_match;

      // Format is a CSV list of "Tag:Attrib,..."

      auto end = cmd.find_first_of(":,", i);
      if (end IS std::string::npos) end = cmd.size();
      tag_match.assign(cmd, i, end-i);
      i = end;

      if (cmd[i] IS ':') {
         end = cmd.find(',', ++i);
         if (end IS std::string::npos) end = cmd.size();
         attrib_match.assign(cmd, i, end-i);
         i = end;
      }

      filters.push_back(make_pair(tag_match, attrib_match));

      i = cmd.find(",", i); // Next filter
      if (i < cmd.size()) i++; // Skip comma
   }

   // Build a sorting array (extract a sort value for each tag and keep a tag reference).

   struct ListSort {
      XMLTag *Tag;
      std::string Value;
      ListSort(XMLTag *pTag, const std::string pValue) : Tag(pTag), Value(pValue) { }
   };

   std::vector<ListSort> list;
   for (auto &scan : branch[0]) {
      std::string sortval;
      for (auto &filter : filters) {
         XMLTag *tag = nullptr;
         // Check for matching tag name, either at the current tag or in one of the child tags underneath it.
         if (pf::wildcmp(filter.first, scan.Attribs[0].Name)) {
            tag = &scan;
         }
         else {
            for (auto &child : scan.Children) {
               if (pf::wildcmp(filter.first, child.Attribs[0].Name)) {
                  tag = &child;
                  break;
               }
            }
         }

         if (!tag) break;

         if ((Args->Flags & XSF::CHECK_SORT) != XSF::NIL) { // Give precedence for a 'sort' attribute in the XML tag
            auto attrib = tag->Attribs.begin()+1;
            for (; attrib != tag->Attribs.end(); attrib++) {
               if (pf::iequals("sort", attrib->Name)) {
                  sortval.append(attrib->Value);
                  sortval.append("\x01");
                  break;
               }
            }
            if (attrib IS tag->Attribs.end()) continue;
         }

         if (filter.second.empty()) { // Use content as the sort data
            for (auto &child : tag->Children) {
               if (child.isContent()) sortval += child.Attribs[0].Value;
            }
         }
         else { // Extract the sort data from the specified tag attribute
            for (auto attrib=tag->Attribs.begin()+1; attrib != tag->Attribs.end(); attrib++) {
               if (pf::wildcmp(filter.second, attrib->Name)) {
                  sortval += attrib->Value;
                  break;
               }
            }
         }

         // Each string in the sort list is separated with a byte value of 0x01

         sortval.append("\x01");
      }

      list.emplace_back(&scan, sortval);
   }

   if ((Args->Flags & XSF::DESC) != XSF::NIL) {
      std::sort(list.begin(), list.end(), [](const ListSort &A, const ListSort &B) -> bool {
         return A.Value > B.Value;
      });
   }
   else {
      std::sort(list.begin(), list.end(), [](const ListSort &A, const ListSort &B) -> bool {
         return A.Value < B.Value;
      });
   }

   // Build new tag list for this branch, then apply it.

   TAGS new_branch(branch->size());
   for (auto &rec : list) new_branch.emplace_back(rec.Tag[0]);
   branch[0] = std::move(new_branch);

   Self->modified();
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Flags: Controls XML parsing behaviour and processing options.

-FIELD-
Path: Set this field if the XML document originates from a file source.

XML documents can be loaded from the file system by specifying a file path in this field.  If set post-initialisation,
all currently loaded data will be cleared and the file will be parsed automatically.

The XML class supports ~Core.LoadFile(), so an XML file can be pre-cached by the program if it is frequently used
during a program's life cycle.

*********************************************************************************************************************/

static ERR GET_Path(extXML *Self, STRING *Value)
{
   if (Self->Path) { *Value = Self->Path; return ERR::Okay; }
   else return ERR::NoData;
}

static ERR SET_Path(extXML *Self, CSTRING Value)
{
   if (Self->Source) SET_Source(Self, nullptr);
   if (Self->Path) { FreeResource(Self->Path); Self->Path = nullptr; }
   if (Self->Statement) { FreeResource(Self->Statement); Self->Statement = nullptr; }

   if (pf::startswith("string:", Value)) {
      // If the string: path type is used then we can optimise things by setting the following path string as the
      // statement.

      return SET_Statement(Self, Value+7);
   }
   else if ((Value) and (*Value)) {
      if ((Self->Path = pf::strclone(Value))) {
         if (Self->initialised()) {
            parse_source(Self);
            return Self->ParseError;
         }
      }
      else return ERR::AllocMemory;
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Modified: A timestamp of when the XML data was last modified.

The Modified field provides an artificial timestamp value of when the XML data was last modified (e.g. by a tag insert
or update).  Storing the current Modified value and making comparisons later makes it easy to determine that a change
has been made.  A rough idea of the total number of change requests can also be calculated by subtracting out the
difference.

-FIELD-
ReadOnly: Prevents modifications and enables caching for a loaded XML data source.

This field can be set to `true` prior to initialisation of an XML object that will use an existing data source.  It
prevents modifications to the XML object.  If the data originates from a file path, the data may be cached to optimise
parsing where the same data is used across multiple XML objects.

*********************************************************************************************************************/

static ERR GET_ReadOnly(extXML *Self, int *Value)
{
   *Value = Self->ReadOnly;
   return ERR::Okay;
}

static ERR SET_ReadOnly(extXML *Self, int Value)
{
   Self->ReadOnly = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Source: Set this field if the XML data is to be sourced from another object.

An XML document can be loaded from another object by referencing it here, on the condition that the object's class
supports the Read action.

If set post-initialisation, all currently loaded data will be cleared and the source object will be parsed
automatically.

*********************************************************************************************************************/

static ERR SET_Source(extXML *Self, OBJECTPTR Value)
{
   if (Self->Path) { FreeResource(Self->Path); Self->Path = nullptr; }
   if (Self->Statement) { FreeResource(Self->Statement); Self->Statement = nullptr; }

   if (Value) {
      Self->Source = Value;
      if (Self->initialised()) {
         parse_source(Self);
         return Self->ParseError;
      }
   }
   else Self->Source = nullptr;

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Start: Set a starting cursor to affect the starting point for some XML operations.

When using any XML function that creates an XML string (e.g. SaveToObject), the XML object will include the entire XML
tree by default.  Defining the Start value will restrict processing to a specific tag and its children.

The Start field currently affects the #SaveToObject() action and the #Statement field.

-FIELD-
Statement: XML data is processed through this field.

Set the Statement field to parse an XML formatted data string through the object.  If this field is set after
initialisation then the XML object will clear any existing data first.

Be aware that setting this field with an invalid statement will result in an empty XML object.

Reading the Statement field will return a serialised string of XML data.  By default all tags will be included in the
statement unless a predefined starting position is set by the #Start field.  The string result is an allocation that
must be freed.
-END-

*********************************************************************************************************************/

static ERR GET_Statement(extXML *Self, STRING *Value)
{
   pf::Log log;

   if (!Self->initialised()) {
      if (Self->Statement) {
         *Value = pf::strclone(Self->Statement);
         return ERR::Okay;
      }
      else return ERR::FieldNotSet;
   }

   if (Self->Tags.empty()) return ERR::FieldNotSet;

   std::ostringstream buffer;

   if (auto tag = Self->getTag(Self->Start)) {
      CURSOR it;
      if (auto tags = Self->getInsert(tag, it)) {
         while (it != tags->end()) {
            serialise_xml(*it, buffer, Self->Flags);
            it++;
         }
      }
      else return log.warning(ERR::NotFound);
   }
   else return log.warning(ERR::NotFound);

   if ((*Value = pf::strclone(buffer.str()))) {
      return ERR::Okay;
   }
   else return ERR::AllocMemory;
}

static ERR SET_Statement(extXML *Self, CSTRING Value)
{
   if (Self->Path) { FreeResource(Self->Path); Self->Path = nullptr; }
   if (Self->Statement) { FreeResource(Self->Statement); Self->Statement = nullptr; }

   if ((Value) and (*Value)) {
      if (Self->initialised()) {
         Self->Tags.clear();
         Self->LineNo = 1;
         Self->ParseError = txt_to_xml(Self, Self->Tags, Value);
         return Self->ParseError;
      }
      else if ((Self->Statement = pf::strclone(Value))) return ERR::Okay;
      else return ERR::AllocMemory;
   }
   else {
      if (Self->initialised()) {
         auto temp = Self->ReadOnly;
         Self->ReadOnly = false;
         acClear(Self);
         Self->ReadOnly = temp;
      }
      return ERR::Okay;
   }
}

/*********************************************************************************************************************

-FIELD-
Tags: Provides direct access to the XML document structure.

The Tags field exposes the complete XML document structure as a hierarchical array of !XMLTag structures.  This field
becomes available after successful XML parsing and provides the primary interface for reading XML content programmatically.

Each !XMLTag will have at least one attribute set in the `Attribs` array.  The first attribute will either reflect
the tag name or a content string if the `Name` is undefined.  The `Children` array provides access to all child elements.

Direct read access to the Tags hierarchy is safe and efficient for traversing the document structure.  However,
modifications should be performed using the XML object's methods (#InsertXML(), #SetAttrib(), #RemoveTag(), etc.) to
maintain internal consistency and trigger appropriate cache invalidation.

NOTE: Fluid will copy this field on read, caching the value is therefore recommended.

*********************************************************************************************************************/

static ERR GET_Tags(extXML *Self, XMLTag **Values, int *Elements)
{
   *Values = Self->Tags.data();
   *Elements = Self->Tags.size();
   return ERR::Okay;
}

//********************************************************************************************************************

#include "xml_def.c"

static const FieldArray clFields[] = {
   { "Path",       FDF_STRING|FDF_RW, nullptr, SET_Path },
   { "Source",     FDF_OBJECT|FDF_RI },
   { "Flags",      FDF_INTFLAGS|FDF_RW, nullptr, nullptr, &clXMLFlags },
   { "Start",      FDF_INT|FDF_RW },
   { "Modified",   FDF_INT|FDF_R },
   { "ParseError", FDF_INT|FD_PRIVATE|FDF_R },
   { "LineNo",     FDF_INT|FD_PRIVATE|FDF_R },
   // Virtual fields
   { "ReadOnly",   FDF_INT|FDF_RI, GET_ReadOnly, SET_ReadOnly },
   { "Src",        FDF_STRING|FDF_SYNONYM|FDF_RW, GET_Path, SET_Path },
   { "Statement",  FDF_STRING|FDF_ALLOC|FDF_RW, GET_Statement, SET_Statement },
   { "Tags",       FDF_ARRAY|FDF_STRUCT|FDF_R, GET_Tags, nullptr, "XMLTag" },
   END_FIELD
};

static ERR add_xml_class(void)
{
   clXML = objMetaClass::create::global(
      fl::BaseClassID(CLASSID::XML),
      fl::ClassVersion(VER_XML),
      fl::Name("XML"),
      fl::FileExtension("*.xml"),
      fl::FileDescription("Extendable Markup Language (XML)"),
      fl::Icon("filetypes/xml"),
      fl::Category(CCF::DATA),
      fl::Actions(clXMLActions),
      fl::Methods(clXMLMethods),
      fl::Fields(clFields),
      fl::Size(sizeof(extXML)),
      fl::Path(MOD_PATH));

   return clXML ? ERR::Okay : ERR::AddClass;
}

//********************************************************************************************************************

static STRUCTS glStructures = {
   { "XMLTag", sizeof(XMLTag) }
};

//********************************************************************************************************************

PARASOL_MOD(MODInit, nullptr, nullptr, MODExpunge, MOD_IDL, &glStructures)
extern "C" struct ModHeader * register_xml_module() { return &ModHeader; }
