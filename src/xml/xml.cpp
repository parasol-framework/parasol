/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
XML: Provides XML data management services.

The XML class provides functionality to create and maintain XML data files.  It is capable of parsing and validating
XML files with or without correct structure, and can perform optional parsing behaviours such as stripping comments
during processing.

Data can be loaded into an XML object either by specifying a file #Path or by giving it an XML #Statement.  If
multiple XML statements need to be processed then reset the Path or Statement field after initialisation and the XML
object will rebuild itself.  This saves on allocating multiple XML objects for batch processing.

Successfully processed data can be read back by scanning the array referenced in the #Tags field.  The array contains
an XMLTag structure for each tag parsed from the original XML statement.  For more information on how to scan this
information, refer to the #Tags field.  C++ developers are recommended to interact with #Tags directly, which
is represented as `pf::vector&lt;XMLTag&gt;`.  Note that adding new Tags is a volatile action that can destabilise the
object (taking a complete copy of the tags may be warranted instead).

-END-

ENTITY       <!ENTITY % textgizmo "fontgizmo">
INSTRUCTION  <?XML version="1.0" standalone="yes" ?>
NOTATION     <!NOTATION gif SYSTEM "viewer.exe">

*********************************************************************************************************************/

#define PRV_XML
#include <parasol/modules/xml.h>
#include <functional>
#include <sstream>

JUMPTABLE_CORE

static OBJECTPTR clXML = NULL;
static ULONG glTagID = 1;

struct ParseState {
   CSTRING Pos;
   LONG   Balance;  // Indicates that the tag structure is correctly balanced if zero

   ParseState() : Pos(NULL), Balance(0) { }
};

typedef objXML::TAGS TAGS;
typedef objXML::CURSOR CURSOR;

//********************************************************************************************************************

class extXML : public objXML {
   public:
   std::unordered_map<LONG, XMLTag *> Map; // Lookup for any indexed tag.
   STRING Statement;
   bool   ReadOnly;
   bool   StaleMap;         // True if map requires a rebuild

   TAGS *CursorParent;  // Parent tag, if any
   TAGS *CursorTags;    // Tag array to which the Cursor belongs
   CURSOR Cursor;       // Resulting cursor position (tag) after a successful search.
   std::string Attrib;
   FUNCTION Callback;

   extXML() : ReadOnly(false), StaleMap(true) { }

   std::unordered_map<LONG, XMLTag *> & getMap() {
      if (StaleMap) {
         Map.clear();
         updateIDs(Tags, 0);
         StaleMap = false;
      }

      return Map;
   }

   // Return the tag for a particular ID.

   XMLTag * getTag(LONG ID) {
      auto &map = getMap();
      auto it = map.find(ID);
      if (it IS map.end()) return NULL;
      else return it->second;
   }

   TAGS * getInsert(LONG ID, CURSOR &Iterator) {
      if (auto tag = getTag(ID)) {
         return getInsert(tag, Iterator);
      }
      else return NULL;
   }

   // For a given tag, return its vector array and cursor position.

   TAGS * getInsert(XMLTag *Tag, CURSOR &Iterator) {
      TAGS *tags;

      if (Tag->ParentID) {
         auto parent = getTag(Tag->ParentID);
         if (parent) tags = &parent->Children;
         else return NULL;
      }
      else tags = &Tags;

      auto it = tags->begin();
      for (; it != tags->end(); it++) {
         if (it->ID IS Tag->ID) {
            Iterator = it;
            return tags;
         }
      }

      return NULL;
   }

   void modified() {
      StaleMap = true;
      Modified++;
   }

   ERR findTag(CSTRING XPath, FUNCTION *pCallback = NULL) {
      this->Attrib.clear();

      if (pCallback) this->Callback = *pCallback;
      else this->Callback.Type = CALL::NIL;

      this->CursorTags = &this->Tags;

      Cursor = this->Tags.begin();
      return find_tag(XPath);
   }

   private:
   ERR find_tag(CSTRING XPath);

   void updateIDs(TAGS &List, LONG ParentID) {
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

static ERR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   return add_xml_class();
}

static ERR CMDExpunge(void)
{
   if (clXML) { FreeResource(clXML); clXML = NULL; }
   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
Clear: Clears all of the data held in an XML object.
-END-
*********************************************************************************************************************/

static ERR XML_Clear(extXML *Self, APTR Void)
{
   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }

   Self->Tags.clear();
   Self->LineNo = 1;
   Self->Start  = 0;
   Self->ParseError = ERR::Okay;
   Self->modified();
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
Count: Count all tags that match a given XPath.

This method will count all tags that match a given XPath and return the value in the Result parameter.

-INPUT-
cstr XPath: The XPath on which to perform the count.
&int Result: The total number of matching tags is returned here.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

static THREADVAR LONG tlXMLCounter;

static ERR xml_count(extXML *Self, XMLTag &Tag, CSTRING Attrib)
{
   tlXMLCounter++;
   return ERR::Okay;
}

static ERR XML_Count(extXML *Self, struct xmlCount *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->XPath)) return log.warning(ERR::NullArgs);

   tlXMLCounter = 0;

   auto call = FUNCTION(xml_count);
   Self->findTag(Args->XPath, &call);

   Args->Result = tlXMLCounter;
   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
DataFeed: XML data can be added to an XML object through this action.
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

   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
Filter: Filters the XML data down to a single tag and its children.

The Filter method is used to reduce the amount of data in an XML tree, filtering out all data exclusive to the targeted
tag and its children.  This is useful for speeding up XPath queries where interest is limited to only one area of the
XML tree, or for reducing the memory footprint of large trees.

Data that has been filtered out by this method is permanently removed.

-INPUT-
cstr XPath: Refers to a valid XPath string.

-ERRORS-
Okay
NullArgs
Search: A matching tag could not be found.

*********************************************************************************************************************/

static ERR XML_Filter(extXML *Self, struct xmlFilter *Args)
{
   if ((!Args) or (!Args->XPath)) return ERR::NullArgs;

   if (Self->findTag(Args->XPath) IS ERR::Okay) {
      auto new_tags = TAGS(Self->Cursor, Self->Cursor);
      Self->Tags = std::move(new_tags);
      Self->modified();
      return ERR::Okay;
   }
   else return ERR::Search;
}

/*********************************************************************************************************************

-METHOD-
FindTag: Searches for a tag via XPath.

This method will return the first tag that matches the search string specified in XPath.  Optionally, if the XPath uses
wildcards or would match multiple tags, a Callback function may be passed that will be called for each matching tag
that is discovered.  The prototype for the callback function is `ERR Function(*XML, XMLTag &Tag, CSTRING Attrib)`.

The Callback routine can terminate the search early by returning `ERR::Terminate`.  All other error codes are ignored.

-INPUT-
cstr XPath: An XPath string.
ptr(func) Callback: Optional reference to a function that should be called for each matching tag.
&int Result: The index of the first matching tag is returned in this parameter (not valid if a Callback is defined).

-ERRORS-
Okay: A matching tag was found.
NullArgs:
NoData:
Search: A matching tag could not be found.

*********************************************************************************************************************/

static ERR XML_FindTag(extXML *Self, struct xmlFindTag *Args)
{
   pf::Log log;

   if (!Args) return ERR::NullArgs;
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

static ERR XML_Free(extXML *Self, APTR Void)
{
   if (Self->Path)      { FreeResource(Self->Path); Self->Path = NULL; }
   if (Self->Statement) { FreeResource(Self->Statement); Self->Statement = NULL; }
   Self->~extXML();
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
GetAttrib: Retrieves the value of an XML attribute.

The GetAttrib method scans a tag for a specific attribute and returns it. A tag index and name of the required
attribute must be specified.  If found, the attribute value is returned in the Value parameter.

It is recommended that C/C++ programs bypass this method and access the XMLAttrib structure directly.

-INPUT-
int Index: The index of the XML tag to search.
cstr Attrib: The name of the attribute to search for (case insensitive).  If NULL or an empty string, the tag name is returned as the result.
&cstr Value: The value of the attribute is returned here, or NULL if the named attribute does not exist.

-ERRORS-
Okay: The attribute was found.
Args: The required arguments were not specified.
NotFound: The attribute name was not found.

*********************************************************************************************************************/

static ERR XML_GetAttrib(extXML *Self, struct xmlGetAttrib *Args)
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
      if (StrMatch(Args->Attrib, attrib.Name) IS ERR::Okay) {
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
GetVar: Retrieves data from an xml object.

The XML class uses variable fields for the execution of XPath queries.  Documentation of the XPath standard is out
of the scope for this document, however the following examples illustrate the majority of uses for this query language
and a number of special instructions that we support:

<types type="Path">
<type name="/menu/submenu">Return the content of the submenu tag whose parent is the first window.</>
<type name="xpath:/menu[2]/window">Return the content of the submenu tag whose parent is the 3rd window.</>
<type name="count:/menu">Return a count of all menu tags at the root level.</>
<type name="xml:/menu/window/@title">Return the value of the title attribute from the window tag.</>
<type name="content:/menu/window(@title='foo')">Return the content of the window tag which has title 'foo'.</>
<type name="extract:/menu/window(@title='bar')">Extract all XML from the window tag which has title 'bar'.</>
<type name="extract:/menu//window(=apple)">Extract all XML from the first window tag found anywhere inside &lt;menu&gt; that contains content 'apple'.</>
<type name="exists:/menu/@title">Return '1' if a menu with a title attribute can be matched, otherwise '0'.</>
<type name="contentexists:/menu">Return '1' if if the immediate child tags of the XPath contain text (white space is ignored).</>
<type name="//window">Return content of the first window discovered at any branch of the XML tree (double-slash enables flat scanning of the XML tree).</>
</types>

The `xpath`, `xml` and `/` prefixes are all identical in identifying the start of an xpath.  The `content` prefix is
used to specifically extract the content of the tag that matches the xpath.  Square brackets and round brackets may be
used interchangeably for lookups and filtering clauses.
-END-

*********************************************************************************************************************/

static ERR XML_GetVar(extXML *Self, struct acGetVar *Args)
{
   pf::Log log;
   LONG count;

   if (!Args) return log.warning(ERR::NullArgs);
   if ((!Args->Field) or (!Args->Buffer) or (Args->Size < 1)) return log.warning(ERR::NullArgs);
   if (!Self->initialised()) return log.warning(ERR::NotInitialised);

   CSTRING field = Args->Field;
   Args->Buffer[0] = 0;

   if (StrCompare("count:", field, 6) IS ERR::Okay) {
      if (xmlCount(Self, field+6, &count) IS ERR::Okay) {
         Args->Buffer[IntToStr(count, Args->Buffer, Args->Size)] = 0;
         return ERR::Okay;
      }
      else return ERR::Failed;
   }
   else if (StrCompare("exists:", field, 7) IS ERR::Okay) {
      Args->Buffer[0] = '0';
      Args->Buffer[1] = 0;

      if (Self->findTag(field+7) != ERR::Okay) return ERR::Okay;

      if (!Self->Attrib.empty()) {
         for (auto &scan : Self->Cursor->Attribs) {
            if (StrMatch(scan.Name, Self->Attrib) IS ERR::Okay) {
               Args->Buffer[0] = '1';
               break;
            }
         }
      }
      else Args->Buffer[0] = '1';

      return ERR::Okay;
   }
   else if (StrCompare("contentexists:", field, 14) IS ERR::Okay) {
      Args->Buffer[0] = '0';
      Args->Buffer[1] = 0;

      if (Self->findTag(field+14) != ERR::Okay) return ERR::Okay;

      for (auto &scan : Self->Cursor->Children) {
         if (scan.Attribs[0].isContent()) {
            auto str = scan.Attribs[0].Value.c_str();
            while (*str) {
               if (*str > 0x20) {
                  Args->Buffer[0] = '1';
                  return ERR::Okay;
               }
               str++;
            }
         }
      }

      return ERR::Okay;
   }
   else if ((StrCompare("xpath:", field, 6) IS ERR::Okay) or
            (StrCompare("xml:", field, 4) IS ERR::Okay) or
            (StrCompare("content:", field, 8) IS ERR::Okay) or
            (StrCompare("extract:", field, 8) IS ERR::Okay) or
            (field[0] IS '/')) {
      LONG j;
      for (j=0; field[j] and (field[j] != '/'); j++) j++;

      if (Self->findTag(field+j) != ERR::Okay) {
         log.msg("Failed to lookup tag '%s'", field+j);
         return ERR::Search;
      }

      if (!Self->Attrib.empty()) { // Extract attribute value
         for (auto &scan : Self->Cursor->Attribs) {
            if (StrMatch(scan.Name, Self->Attrib) IS ERR::Okay) {
               StrCopy(scan.Value, Args->Buffer, Args->Size);
               return ERR::Okay;
            }
         }
         return ERR::Failed;
      }
      else {
         // Extract tag content

         UBYTE extract;

         if (StrCompare("content:", field, 8) IS ERR::Okay) extract = 1; // 'In-Depth' content extract.
         else if (StrCompare("extract:", field, 8) IS ERR::Okay) extract = 2;
         else extract = 0;

         Args->Buffer[0] = 0;
         if (extract IS 1) {
            return get_content(Self, *Self->Cursor, Args->Buffer, Args->Size);
         }
         else if (extract IS 2) {
            STRING str;
            ERR error = xmlSerialise(Self, Self->Cursor->Children[0].ID, XMF::INCLUDE_SIBLINGS, &str);
            if (error IS ERR::Okay) {
               StrCopy(str, Args->Buffer, Args->Size);
               FreeResource(str);
            }

            return error;
         }
         else { // 'Immediate' content extract (not deep)
            if (!Self->Cursor->Children.empty()) {
               LONG j = 0;
               for (auto &scan : Self->Cursor->Children) {
                  if (!scan.Attribs[0].isContent()) j += StrCopy(scan.Attribs[0].Value, Args->Buffer+j, Args->Size-j);
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
GetContent: Extracts the content of an XML tag.

The GetContent method is used to extract the string content from an XML tag.  It will extract content that is
immediately embedded within the XML tag and will not perform deep analysis of the tag structure (refer to
#Serialise() for deep extraction).  Consider the following structure:

<pre>
&lt;body&gt;
  Hello
  &lt;bold&gt;my&lt;/bold&gt;
  friend!
&lt;/body&gt;
</pre>

This will produce the result "Hello friend!" and omit everything encapsulated within the bold tag.

-INPUT-
int Index: Index of a tag that contains content.
buf(str) Buffer: Pointer to a buffer that will receive the string data.
bufsize Length: The length of the Buffer in bytes.

-ERRORS-
Okay: The content string was successfully extracted.
Args
NotFound: The tag identified by Index was not found.
BufferOverflow: The buffer was not large enough to hold the content (the resulting string will be valid but truncated).

*********************************************************************************************************************/

static ERR XML_GetContent(extXML *Self, struct xmlGetContent *Args)
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
Serialise: Serialise part of the XML tree to an XML string.

The Serialise method will serialise all or part of the XML data tree to a string.

The string will be allocated as a memory block and stored in the Result parameter.  It must be freed once the data
is no longer required.

-INPUT-
int Index: Index to a source tag for which serialisation will start.  Set to zero to serialise the entire tree.
int(XMF) Flags: Use INCLUDE_SIBLINGS to include siblings of the tag found at Index.
!str Result: The resulting string is returned in this parameter.

-ERRORS-
Okay: The XML string was retrieved.
Args:
NoData: No information has been loaded into the XML object.
AllocMemory: Failed to allocate an XML string for the result.

*********************************************************************************************************************/

static ERR XML_Serialise(extXML *Self, struct xmlSerialise *Args)
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

   pf::SwitchContext ctx(GetParentContext());
   if ((Args->Result = StrClone(buffer.str().c_str()))) return ERR::Okay;
   else return log.warning(ERR::AllocMemory);
}

/*********************************************************************************************************************

-METHOD-
GetTag: Returns a pointer to the XMLTag structure for a given tag index.

This method will return the XMLTag structure for a given tag Index.  The Index is checked to ensure it is valid prior
to retrieval, and an `ERR::OutOfRange` error will be returned if it is invalid.

-INPUT-
int Index:  The index of the tag that is being retrieved.
&struct(*XMLTag) Result: The XMLTag is returned in this parameter.

-ERRORS-
Okay
NullArgs
NotFound: The Index is not recognised.

*********************************************************************************************************************/

static ERR XML_GetTag(extXML *Self, struct xmlGetTag *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);

   if ((Args->Result = Self->getTag(Args->Index))) return ERR::Okay;
   else return ERR::NotFound;
}

//********************************************************************************************************************

static ERR XML_Init(extXML *Self, APTR Void)
{
   pf::Log log;

   if (Self->isSubClass()) return ERR::Okay; // Break here for sub-classes to perform initialisation

   if (Self->Statement) {
      Self->LineNo = 1;
      if ((Self->ParseError = txt_to_xml(Self, Self->Tags, Self->Statement)) != ERR::Okay) {
         if ((Self->ParseError IS ERR::InvalidData) or (Self->ParseError IS ERR::NoData)) return ERR::NoSupport;

         log.warning("XML parsing error #%d: %s", LONG(Self->ParseError), GetErrorMsg(Self->ParseError));
      }

      FreeResource(Self->Statement);
      Self->Statement = NULL;

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
InsertContent: Inserts XML content into the XML tree.

The InsertContent method will insert content strings into any position within the XML tree.  A content string
must be provided in the Content parameter and the target insertion point is specified in the Index parameter.
An insertion point relative to the target index must be specified in the Where parameter.  The new tags can be
inserted as a child of the target by using a Where value of `XMI::CHILD`.  To insert behind or after the target, use
`XMI::PREV` or `XMI::NEXT`.

To modify existing content, call #SetAttrib() instead.

-INPUT-
int Index: Identifies the target XML tag.
int(XMI) Where: Use PREV or NEXT to insert behind or ahead of the target tag.  Use CHILD for a child insert.
cstr Content: The content to insert.
&int Result: The index of the new tag is returned here.

-ERRORS-
Okay
NullArgs
ReadOnly

*********************************************************************************************************************/

static ERR XML_InsertContent(extXML *Self, struct xmlInsertContent *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);
   if (Self->ReadOnly) return log.warning(ERR::ReadOnly);
   if ((Self->Flags & XMF::LOG_ALL) != XMF::NIL) log.branch("Index: %d, Insert: %d", Args->Index, LONG(Args->Where));

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

The InsertXML method is used to translate and insert a new set of XML tags into any position within the XML tree.  A
standard XML statement must be provided in the XML parameter and the target insertion point is specified in the Index
parameter.  An insertion point relative to the target index must be specified in the Insert parameter.  The new tags
can be inserted as a child of the target by using a Insert value of `XMI::CHILD`.  Use `XMI::CHILD_END` to insert at the end
of the child list.  To insert behind or after the target, use `XMI::PREV` or `XMI::NEXT`.

-INPUT-
int Index: The new data will target the tag specified here.
int(XMI) Where: Use PREV or NEXT to insert behind or ahead of the target tag.  Use CHILD or CHILD_END for a child insert.
cstr XML: An XML statement to parse.
&int Result: The resulting tag index.

-ERRORS-
Okay: The statement was added successfully.
Args
NullArgs
OutOfRange
ReadOnly: Changes to the XML data are not permitted.

*********************************************************************************************************************/

static ERR XML_InsertXML(extXML *Self, struct xmlInsertXML *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);
   if (Self->ReadOnly) return log.warning(ERR::ReadOnly);
   if ((Self->Flags & XMF::LOG_ALL) != XMF::NIL) log.branch("Index: %d, Where: %d, XML: %.40s", Args->Index, LONG(Args->Where), Args->XML);

   auto src = Self->getTag(Args->Index);
   if (!src) return log.warning(ERR::NotFound);

   ERR error;
   TAGS insert;
   if ((error = txt_to_xml(Self, insert, Args->XML)) != ERR::Okay) return log.warning(error);
   if (insert.empty()) return ERR::NoData;
   auto result = insert[0].ID;

   if (Args->Where IS XMI::NEXT) {
      CURSOR it;
      if (auto tags = Self->getInsert(src, it)) tags->insert(it, insert.begin(), insert.end());
      else return log.warning(ERR::NotFound);
   }
   else if (Args->Where IS XMI::PREV) {
      CURSOR it;
      if (auto tags = Self->getInsert(src, it)) {
        if (it IS tags->begin()) tags->insert(it, insert.begin(), insert.end());
        else tags->insert(it - 1, insert.begin(), insert.end());
      }
      else return log.warning(ERR::NotFound);
   }
   else if (Args->Where IS XMI::CHILD) {
      src->Children.insert(src->Children.begin(), insert.begin(), insert.end());
   }
   else if (Args->Where IS XMI::CHILD_END) {
      src->Children.insert(src->Children.end(), insert.begin(), insert.end());
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
XPath location string.  An insertion point relative to the XPath target must be specified in the Insert parameter.  The
new tags can be inserted as a child of the target by using an Insert value of `XMI::CHILD` or `XMI::CHILD_END`.  To insert
behind or after the target, use `XMI::PREV` or `XMI::NEXT`.

-INPUT-
cstr XPath: An XPath string that refers to the target insertion point.
int(XMI) Where: Use PREV or NEXT to insert behind or ahead of the target tag.  Use CHILD for a child insert.
cstr XML: The statement to process.
&int Result: The index of the new tag is returned here.

-ERRORS-
Okay
NullArgs
Search: The XPath could not be resolved.

*********************************************************************************************************************/

ERR XML_InsertXPath(extXML *Self, struct xmlInsertXPath *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->XPath) or (!Args->XML)) return log.warning(ERR::NullArgs);
   if (Self->ReadOnly) return log.warning(ERR::ReadOnly);

   log.branch("Insert: %d, XPath: %s", LONG(Args->Where), Args->XPath);

   if (Self->findTag(Args->XPath) IS ERR::Okay) {
      struct xmlInsertXML insert = { .Index = Self->Cursor->ID, .Where = Args->Where, .XML = Args->XML };
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

An insertion point relative to the target index must be specified in the Where parameter.  The source tag can be
inserted as a child of the destination by using a Where of `XMI::CHILD`.  To insert behind or after the target, use
`XMI::PREV` or `XMI::NEXT`.

-INPUT-
int Index: Index of the source tag to be moved.
int Total: The total number of sibling tags to be moved from the source index.  Minimum value of 1.
int DestIndex: The destination tag index.  If the index exceeds the total number of tags, the value will be automatically limited to the last tag index.
int(XMI) Where: Use PREV or NEXT to insert behind or ahead of the target tag.  Use CHILD for a child insert.

-ERRORS-
Okay: Tags were moved successfully.
NullArgs
NotFound:
ReadOnly
-END-

*********************************************************************************************************************/

static ERR XML_MoveTags(extXML *Self, struct xmlMoveTags *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);
   if (Self->ReadOnly) return log.warning(ERR::ReadOnly);
   if (Args->Total < 1) return log.warning(ERR::Args);
   if (Args->Index IS Args->DestIndex) return ERR::Okay;

   // Extricate the source tag

   CURSOR it;
   auto src_tags = Self->getInsert(Args->Index, it);
   if (!src_tags) return log.warning(ERR::NotFound);

   TAGS copy;
   unsigned s;
   for (s=0; s < src_tags->size(); s++) {
      if (src_tags[0][s].ID IS it->ID) {
         copy = TAGS(src_tags->begin() + s, src_tags->begin() + s + Args->Total);
      }
   }

   if (copy.empty()) return log.warning(ERR::NotFound);

   // Verify that the destination tag exists (if not, the destination is probably within the source
   // and therefore the move is impossible).

   auto dest = Self->getTag(Args->DestIndex);
   if (!dest) return log.warning(ERR::NotFound);

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


   Self->Tags.erase(Self->Tags.begin() + s, Self->Tags.begin() + s + Args->Total);

   Self->modified();
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR XML_NewObject(extXML *Self, APTR Void)
{
   new (Self) extXML;
   Self->LineNo = 1;
   Self->ParseError = ERR::Okay;
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
RemoveTag: Removes tag(s) from the XML structure.

The RemoveTag method is used to remove one or more tags from an XML structure.  Child tags will automatically be
discarded as a consequence of using this method, in order to maintain a valid XML structure.

This method is capable of deleting multiple tags if the Total parameter is set to a value greater than 1.  Each
consecutive tag and its children following the targeted tag will be removed from the XML structure until the count is
exhausted. This is useful for mass delete operations.

This method is volatile and will destabilise any cached address pointers that have been acquired from the XML object.

-INPUT-
int Index: Reference to the tag that will be removed.
int Total: The total number of sibling (neighbouring) tags that should also be deleted.  A value of one or less will remove only the indicated tag and its children.  The total may exceed the number of tags actually available, in which case all tags up to the end of the branch will be affected.

-ERRORS-
Okay
NullArgs
OutOfRange
ReadOnly
-END-

*********************************************************************************************************************/

static ERR XML_RemoveTag(extXML *Self, struct xmlRemoveTag *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);
   if (Self->ReadOnly) return log.warning(ERR::ReadOnly);
   if ((Self->Flags & XMF::LOCK_REMOVE) != XMF::NIL) return log.warning(ERR::ReadOnly);

   LONG count = Args->Total;
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

Individual tag attributes can also be removed if an attribute is referenced at the end of the XPath.

The removal routine will be repeated so that each tag that matches the XPath will be deleted, or the Total is reached.

This method is volatile and will destabilise any cached address pointers that have been acquired from the XML object.

-INPUT-
cstr XPath: An XML path string.
int Limit: The maximum number of matching tags that should be deleted.  A value of one or less will remove only the indicated tag and its children.  The total may exceed the number of tags actually available, in which case all matching tags up to the end of the tree will be affected.

-ERRORS-
Okay
NullArgs
ReadOnly
-END-

*********************************************************************************************************************/

static ERR XML_RemoveXPath(extXML *Self, struct xmlRemoveXPath *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->XPath)) return ERR::NullArgs;

   if (Self->Tags.empty()) return ERR::NoData;
   if (Self->ReadOnly) return log.warning(ERR::ReadOnly);
   if ((Self->Flags & XMF::LOCK_REMOVE) != XMF::NIL) return log.warning(ERR::ReadOnly);

   auto limit = Args->Limit;
   if (limit < 0) limit = 0x7fffffff;
   while (limit > 0) {
      if (Self->findTag(Args->XPath) IS ERR::Okay) return ERR::Okay; // Assume tag already removed if no match

      if (!Self->Attrib.empty()) { // Remove an attribute
         for (LONG a=0; a < std::ssize(Self->Cursor->Attribs); a++) {
            if (StrMatch(Self->Attrib, Self->Cursor->Attribs[a].Name) IS ERR::Okay) {
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

static ERR XML_Reset(extXML *Self, APTR Void)
{
   return acClear(Self);
}

/*********************************************************************************************************************
-ACTION-
SaveToObject: Saves XML data to a storage object (e.g. file).
-END-
*********************************************************************************************************************/

static ERR XML_SaveToObject(extXML *Self, struct acSaveToObject *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Dest)) return log.warning(ERR::NullArgs);
   if (Self->Tags.size() <= 0) return ERR::Okay;

   log.traceBranch("To: %d", Args->Dest->UID);

   ERR error;
   STRING str;
   if ((error = xmlSerialise(Self, 0, XMF::READABLE|XMF::INCLUDE_SIBLINGS, &str)) IS ERR::Okay) {
      struct acWrite write = { str, StrLength(str) };
      if (Action(AC_Write, Args->Dest, &write) != ERR::Okay) error = ERR::Write;
      FreeResource(str);
      return error;
   }
   else return error;
}

/*********************************************************************************************************************

-METHOD-
SetAttrib: Adds, updates and removes XML attributes.

This method is used to update and add attributes to existing XML tags, as well as adding or modifying content.

The data for the attribute is defined in the Name and Value parameters.  Use an empty string if no data is to be
associated with the attribute.  Set the Value pointer to NULL to remove the attribute. If both Name and Value are NULL,
an error will be returned.

NOTE: The attribute at position 0 declares the name of the tag and should not normally be accompanied with a
value declaration.  However, if the tag represents content within its parent, then the Name must be set to NULL and the
Value string will determine the content.

-INPUT-
int Index: Identifies the tag that is to be updated.
int(XMS) Attrib: Either the index number of the attribute that is to be updated, or set to XMS_NEW, XMS_UPDATE or XMS_UPDATE_ONLY.
cstr Name: String containing the new name for the attribute.  If NULL, the name will not be changed.  If Attrib is XMS_UPDATE or XMS_UPDATE_ONLY, the Name is used to find the attribute.
cstr Value: String containing the new value for the attribute.  If NULL, the attribute is removed.

-ERRORS-
Okay
NullArgs
Args
OutOfRange: The Index or Attrib value is out of range.
Search: The attribute, identified by Name, could not be found.
ReadOnly: The XML object is read-only.
-END-

*********************************************************************************************************************/

static ERR XML_SetAttrib(extXML *Self, struct xmlSetAttrib *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);
   if (Self->ReadOnly) return log.warning(ERR::ReadOnly);

   log.trace("Tag: %d, Attrib: $%.8x, %s = '%s'", Args->Index, Args->Attrib, Args->Name, Args->Value);

   auto tag = Self->getTag(Args->Index);
   if (!tag) return log.warning(ERR::Search);

   LONG cmd = Args->Attrib;
   if ((cmd IS XMS_UPDATE) or (cmd IS XMS_UPDATE_ONLY)) {
      for (auto a = tag->Attribs.begin(); a != tag->Attribs.end(); a++) {
         if (StrMatch(Args->Name, a->Name) IS ERR::Okay) {
            if (Args->Value) {
               a->Name  = Args->Name;
               a->Value = Args->Value;
            }
            else tag->Attribs.erase(a);
            Self->Modified++;
            return ERR::Okay;
         }
      }

      if (cmd IS XMS_UPDATE) {
         if ((!Args->Value) or (!Args->Value[0])) return ERR::Okay; // User wants to remove a non-existing attribute, so return ERR::Okay
         else { // Create new attribute if name wasn't found
            tag->Attribs.push_back({ Args->Name, Args->Value });
            Self->Modified++;
            return ERR::Okay;
         }
      }
      else return ERR::Search;
   }
   else if (cmd IS XMS_NEW) {
      tag->Attribs.push_back({ Args->Name, Args->Value });
      Self->Modified++;
      return ERR::Okay;
   }

   // Attribute indexing

   if ((Args->Attrib < 0) or (Args->Attrib >= LONG(tag->Attribs.size()))) return log.warning(ERR::OutOfRange);

   if (Args->Value) {
      if (Args->Name) tag->Attribs[Args->Attrib].Name = Args->Name;
      tag->Attribs[Args->Attrib].Value = Args->Value;
   }
   else {
      if (Args->Attrib IS 0) tag->Attribs[Args->Attrib].Value.clear(); // Content is erased when Attrib == 0
      else tag->Attribs.erase(tag->Attribs.begin() + Args->Attrib);
   }

   Self->Modified++;
   return ERR::Okay;
}

/*********************************************************************************************************************

-ACTION-
SetVar: Sets attributes and content in the XML tree using XPaths,

Use SetVar to add tag attributes and content using XPaths.  The XPath is specified in the Field parameter and the data
is specified in the Value parameter.  Setting the Value to NULL will remove the attribute or existing content, while an
empty string will keep an attribute but eliminate any associated data.

It is not possible to add new tags using this action - it is only possible to update existing tags.

Please note that making changes to the XML tree will render all previously obtained tag pointers and indexes invalid.

-ERRORS-
Okay
ReadOnly: Changes to the XML structure are not permitted.
Search: Failed to find the tag referenced by the XPath.

*********************************************************************************************************************/

static ERR XML_SetVar(extXML *Self, struct acSetVar *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Field)) return log.warning(ERR::NullArgs);
   if (Self->ReadOnly) return log.warning(ERR::ReadOnly);

   if (Self->findTag(Args->Field) IS ERR::Okay) {
      if (!Self->Attrib.empty()) { // Updating or adding an attribute
         unsigned i;
         for (i=0; i < Self->Cursor->Attribs.size(); i++) {
            if (StrMatch(Self->Attrib, Self->Cursor->Attribs[i].Name) IS ERR::Okay) break;
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
      log.msg("Failed to find '%s'", Args->Field);
      return ERR::Search;
   }
}

/*********************************************************************************************************************

-METHOD-
Sort: Sorts XML tags to your specifications.

The Sort method is used to sort a single branch of XML tags in ascending or descending order.  An XPath is required
that refers to the tag containing each item that will be sorted.  To sort the root level, use an XPath of NULL.

The Sort parameter is used to specify a list of sorting instructions.  The format for the Sort string is
`Tag:Attrib,Tag:Attrib,...`.  The Tag indicates the tag name that should be identified for sorting each node, and
child tags are supported for this purpose.  Wildcard filtering is allowed and a Tag value of `*` will match every
tag at the requested XPath level.  The optional Attrib value names the attribute containing the sort string.  To
sort on content, do not define an Attrib value (use the format `Tag,Tag,...`).

-INPUT-
cstr XPath: Sort everything under the specified tag, or NULL to sort the entire top level.
cstr Sort: Pointer to a sorting instruction string.
int(XSF) Flags: Optional flags.

-ERRORS-
Okay: The XML object was successfully sorted.
NullArgs
Search: The provided XPath failed to locate a tag.
ReadOnly
AllocMemory:
-END-

*********************************************************************************************************************/

static ERR XML_SortXML(extXML *Self, struct xmlSort *Args)
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
         XMLTag *tag = NULL;
         // Check for matching tag name, either at the current tag or in one of the child tags underneath it.
         if (StrCompare(filter.first, scan.Attribs[0].Name, 0, STR::WILDCARD) IS ERR::Okay) {
            tag = &scan;
         }
         else {
            for (auto &child : scan.Children) {
               if (StrCompare(filter.first, child.Attribs[0].Name, 0, STR::WILDCARD) IS ERR::Okay) {
                  tag = &child;
                  break;
               }
            }
         }

         if (!tag) break;

         if ((Args->Flags & XSF::CHECK_SORT) != XSF::NIL) { // Give precedence for a 'sort' attribute in the XML tag
            auto attrib = tag->Attribs.begin()+1;
            for (; attrib != tag->Attribs.end(); attrib++) {
               if (StrMatch("sort", attrib->Name) IS ERR::Okay) {
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
               if (StrCompare(filter.second, attrib->Name, 0, STR::WILDCARD) IS ERR::Okay) {
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
Flags: Optional flags.

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
   if (Self->Source) SET_Source(Self, NULL);
   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }
   if (Self->Statement) { FreeResource(Self->Statement); Self->Statement = NULL; }

   if (StrCompare("string:", Value, 7) IS ERR::Okay) {
      // If the string: path type is used then we can optimise things by setting the following path string as the
      // statement.

      return SET_Statement(Self, Value+7);
   }
   else if ((Value) and (*Value)) {
      if ((Self->Path = StrClone(Value))) {
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

This field can be set to TRUE prior to initialisation of an XML object that will use an existing data source.  It
prevents modifications to the XML object.  If the data originates from a file path, the data may be cached to optimise
parsing where the same data is used across multiple XML objects.

*********************************************************************************************************************/

static ERR GET_ReadOnly(extXML *Self, LONG *Value)
{
   *Value = Self->ReadOnly;
   return ERR::Okay;
}

static ERR SET_ReadOnly(extXML *Self, LONG Value)
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
   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }
   if (Self->Statement) { FreeResource(Self->Statement); Self->Statement = NULL; }

   if (Value) {
      Self->Source = Value;
      if (Self->initialised()) {
         parse_source(Self);
         return Self->ParseError;
      }
   }
   else Self->Source = NULL;

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
         *Value = StrClone(Self->Statement);
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

   if ((*Value = StrClone(buffer.str().c_str()))) {
      return ERR::Okay;
   }
   else return ERR::AllocMemory;
}

static ERR SET_Statement(extXML *Self, CSTRING Value)
{
   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }
   if (Self->Statement) { FreeResource(Self->Statement); Self->Statement = NULL; }

   if ((Value) and (*Value)) {
      if (Self->initialised()) {
         Self->Tags.clear();
         Self->LineNo = 1;
         Self->ParseError = txt_to_xml(Self, Self->Tags, Value);
         return Self->ParseError;
      }
      else if ((Self->Statement = StrClone(Value))) return ERR::Okay;
      else return ERR::AllocMemory;
   }
   else {
      if (Self->initialised()) {
         auto temp = Self->ReadOnly;
         Self->ReadOnly = FALSE;
         acClear(Self);
         Self->ReadOnly = temp;
      }
      return ERR::Okay;
   }
}

/*********************************************************************************************************************

-FIELD-
Tags: Points to an array of tags loaded into an XML object.

The successful parsing of XML data will make the information available via the Tags array.  The array is presented as
a series of XMLTag structures.

Each XMLTag will also have at least one attribute set in the Attribs array.  The first attribute will either reflect
the tag name or a content string if the Name is undefined.  The Children array provides access to all child elements.

Developers may treat the entire tag hierarchy as readable, but writes should be accomplished with the
available XML methods.

*********************************************************************************************************************/

static ERR GET_Tags(extXML *Self, XMLTag **Values, LONG *Elements)
{
   *Values = Self->Tags.data();
   *Elements = Self->Tags.size();
   return ERR::Okay;
}

//********************************************************************************************************************

#include "xml_def.c"

static const FieldArray clFields[] = {
   { "Path",       FDF_STRING|FDF_RW, NULL, SET_Path },
   { "Source",     FDF_OBJECT|FDF_RI },
   { "Flags",      FDF_LONGFLAGS|FDF_RW, NULL, NULL, &clXMLFlags },
   { "Start",      FDF_LONG|FDF_RW },
   { "Modified",   FDF_LONG|FDF_R },
   { "ParseError", FDF_LONG|FD_PRIVATE|FDF_R },
   { "LineNo",     FDF_LONG|FD_PRIVATE|FDF_R },
   // Virtual fields
   { "ReadOnly",   FDF_LONG|FDF_RI, GET_ReadOnly, SET_ReadOnly },
   { "Src",        FDF_STRING|FDF_SYNONYM|FDF_RW, GET_Path, SET_Path },
   { "Statement",  FDF_STRING|FDF_ALLOC|FDF_RW, GET_Statement, SET_Statement },
   { "Tags",       FDF_ARRAY|FDF_STRUCT|FDF_R, GET_Tags, NULL, "XMLTag" },
   END_FIELD
};

static ERR add_xml_class(void)
{
   clXML = objMetaClass::create::global(
      fl::BaseClassID(ID_XML),
      fl::ClassVersion(VER_XML),
      fl::Name("XML"),
      fl::FileExtension("*.xml"),
      fl::FileDescription("XML File"),
      fl::Category(CCF::DATA),
      fl::Flags(CLF::PROMOTE_INTEGRAL),
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

PARASOL_MOD(CMDInit, NULL, NULL, CMDExpunge, MOD_IDL, &glStructures)
extern "C" struct ModHeader * register_xml_module() { return &ModHeader; }
