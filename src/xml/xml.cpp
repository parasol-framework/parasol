/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
XML: Provides XML data management services.

The XML class provides the necessary functionality to create and maintain XML data files.  It is capable of interpreting
and validating XML files with or without correct structure and can perform various data manipulations while doing so.

Data can be loaded into an XML object either by specifying a file #Path or by giving it an XML #Statement.  If multiple
XML statements need to be processed then reset the Path or Statement field after initialisation and the XML object
will rebuild itself.  This saves on allocating multiple XML objects for batch processing.

Successfully processed data can be read back by scanning the array referenced in the #Tags field.  The array contains an
XMLTag structure for each tag parsed from the original XML statement.  For more information on how to scan this
information, refer to the #Tags field.

Please note that all tag address pointers that are listed in the #Tags field are volatile.  Any write
operation to an XML object's tree structure will invalidate the pointers.
-END-

ENTITY       <!ENTITY % textgizmo "fontgizmo">
INSTRUCTION  <?XML version="1.0" standalone="yes" ?>
NOTATION     <!NOTATION gif SYSTEM "viewer.exe">

*****************************************************************************/

#undef DEBUG
//#define DEBUG // Enables extensive debugging of the XML tree
//#define DEBUG_TREE_REMOVE // Print out the tree structure whenever RemoveTag is used
//#define DEBUG_TREE_INSERT // Print out the tree structure whenever InsertXML is used
//#define DEBUG_TREE_MOVE   // Print out the tree structure whenever MoveTags is used

#define PRV_XML
#include <parasol/modules/xml.h>

MODULE_COREBASE;
static OBJECTPTR clXML = NULL;
static UWORD glTagID = 1;

// Any flag that affects interpretation of the XML source data must be defined in XMF_MODFLAGS.
#define XMF_MODFLAGS (XMF_INCLUDE_COMMENTS|XMF_STRIP_CONTENT|XMF_LOWER_CASE|XMF_UPPER_CASE|XMF_STRIP_HEADERS|XMF_NO_ESCAPE|XMF_ALL_CONTENT|XMF_PARSE_HTML|XMF_PARSE_ENTITY)

struct ListSort {
   XMLTag *Tag;     // Pointer to the XML tag
   char String[80]; // Sort data
};

struct exttag {
   CSTRING Start;
   CSTRING Pos;
   LONG   TagIndex;
   LONG   Branch;
};

class extXML : public objXML {
   public:
   struct xml_cache *Cache; // Array of tag pointers, in linear order
   STRING Statement;
   LONG   Balance;          // Indicates that the tag structure is correctly balanced if zero
   UBYTE  ReadOnly:1;
};

static ERROR add_xml_class(void);
static XMLTag * build_xml_string(XMLTag *, STRING Buffer, LONG, LONG *);
static void clear_tags(extXML *XML);
static ERROR txt_to_xml(extXML *, CSTRING);
static ERROR extract_tag(extXML *, exttag *);
static ERROR extract_content(extXML *, exttag *);
static XMLTag * find_tag(extXML *, XMLTag *, CSTRING, CSTRING *, FUNCTION *);
static void free_xml(extXML *);
static ERROR get_content(extXML *, XMLTag *, STRING Buffer, LONG Size);
static XMLTag * len_xml_str(XMLTag *, LONG, LONG *);
static ERROR parse_source(extXML *);
static void parse_doctype(extXML *, CSTRING Input);
static void tag_count(XMLTag *, LONG *);
static void sift_down(ListSort **, LONG, LONG);
static void sift_up(ListSort **, LONG, LONG);
static XMLTag * next_sibling(extXML *, XMLTag *, LONG, STRING, LONG);
static void xml_unescape(extXML *, STRING);
static ERROR SET_Statement(extXML *, CSTRING Value);
static ERROR SET_Source(extXML *Self, OBJECTPTR Value);

/*****************************************************************************
** Debug routines
*/

#if defined(DEBUG) || defined(DEBUG_TREE_REMOVE) || defined(DEBUG_TREE_INSERT) || defined(DEBUG_TREE_MOVE)

static void debug_tree(STRING Header, extXML *Self) __attribute__ ((unused));
static void debug_tree(STRING Header, extXML *Self)
{
   parasol::Log log(Header);
   LONG i, j;
   char buffer[1000];

   for (LONG index=0; index < Self->TagCount; index++) {
      auto tag = Self->Tags[index];

      // Indenting

      for (i=0; i < tag->Branch; i++) buffer[i] = ' ';
      buffer[i] = 0;

      if (tag->Attrib) {
         if (tag->Attrib->Name) {
            log.msg("%.3d/%.3d: %p<-%p->%p Child %p %s%s {%d}", index, tag->Index, tag->Prev, tag, tag->Next, tag->Child, buffer, tag->Attrib->Name, tag->TotalAttrib);
         }
         else {
            // Extract a limited amount of content
            for (j=0; (tag->Attrib->Value[j]) and (j < 16) and ((size_t)i < sizeof(buffer)); j++) {
               if (tag->Attrib->Value[j] IS '\n') buffer[i++] = '.';
               else buffer[i++] = tag->Attrib->Value[j];
            }
            buffer[i] = 0;
            log.msg("%.3d/%.3d: %p<-%p->%p Child %p %s", index, tag->Index, tag->Prev, tag, tag->Next, tag->Child, buffer);
            //log.msg("%.3d: %s", index, buffer);
         }
      }
      else log.msg( "%.3d/%.3d: %p<-%p->%p Child %p Special", index, tag->Index, tag->Prev, tag, tag->Next, tag->Child);
   }
}

#endif

//**********************************************************************

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   return add_xml_class();
}

ERROR CMDExpunge(void)
{
   if (clXML) { acFree(clXML); clXML = NULL; }
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Clear: Clears all of the data held in an XML object.
-END-
*****************************************************************************/

static ERROR XML_Clear(extXML *Self, APTR Void)
{
   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }

   clear_tags(Self);
   Self->Modified++;
   return ERR_Okay;
}

/*****************************************************************************

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

*****************************************************************************/

static THREADVAR LONG tlXMLCounter;

static ERROR xml_count(extXML *Self, XMLTag *Tag, CSTRING Attrib)
{
   parasol::Log log;
   tlXMLCounter++;
   log.trace("IncCount: %d, Tag: %d: %s", tlXMLCounter, Tag->Index, Tag->Attrib->Name);
   return ERR_Okay;
}

static ERROR XML_Count(extXML *Self, struct xmlCount *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->XPath)) return log.warning(ERR_NullArgs);

   tlXMLCounter = 0;
   XMLTag *tags = Self->Tags[Self->RootIndex];

   FUNCTION callback;
   SET_FUNCTION_STDC(callback, (APTR)&xml_count);
   find_tag(Self, tags, Args->XPath, NULL, &callback);

   Args->Result = tlXMLCounter;
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
DataFeed: XML data can be added to an XML object through this action.
-END-
*****************************************************************************/

static ERROR XML_DataFeed(extXML *Self, struct acDataFeed *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if ((Args->DataType IS DATA_XML) or (Args->DataType IS DATA_TEXT)) {
      extXML xml;
      XMLTag *tag;
      ERROR error;

      if (Self->ReadOnly) return log.warning(ERR_ReadOnly);

      // If there is no existing data in our object, just add the XML from scratch

      if (Self->TagCount < 1) return txt_to_xml(Self, (CSTRING)Args->Buffer);

      ClearMemory(&xml, sizeof(xml));
      AllocMemory(sizeof(APTR), MEM_DATA, &xml.Tags, NULL);
      xml.PrivateDataSize = Self->PrivateDataSize;
      xml.Flags = Self->Flags;

      if ((error = txt_to_xml(&xml, (CSTRING)Args->Buffer))) {
         free_xml(&xml);
         return log.warning(error);
      }

      // Increase the size of our XML tag array.  The tag array is a list of all the XML tags so that the developer can
      // look up using indexes.

      if (ReallocMemory(Self->Tags, sizeof(APTR) * (Self->TagCount + xml.TagCount + 1), &Self->Tags, NULL) != ERR_Okay) {
         log.warning("Failed to reallocate tag array.");
         free_xml(&xml);
         return ERR_ReallocMemory;
      }

      // Correct the end of the chain to correctly link up.  Note that we append to the last root tag, which is not
      // necessarily the last tag at the end of the array.  For that reason, we need the loop.

      if ((tag = Self->Tags[0])) {
         while (tag->Next) tag = tag->Next;
         tag->Next = xml.Tags[0];
         xml.Tags[0]->Prev = tag;
      }

      // Copy the new tags into the array and set the tag index numbers

      auto index = Self->TagCount;
      for (LONG i=0; i < xml.TagCount; i++, index++) {
         Self->Tags[index] = xml.Tags[i];
         Self->Tags[index]->Index = index;
      }

      Self->Tags[index] = NULL; // Terminate the tag array
      Self->TagCount = index;   // Set the new tag count
      Self->Modified++;

      FreeResource(xml.Tags);
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR XML_Free(extXML *Self, APTR Void)
{
   free_xml(Self);
   return ERR_Okay;
}

/*****************************************************************************

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

*****************************************************************************/

static ERROR XML_Filter(extXML *Self, struct xmlFilter *Args)
{
   if ((!Args) or (!Args->XPath)) return ERR_NullArgs;

   // Find the target tag

   XMLTag *tag;
   if ((tag = find_tag(Self, Self->Tags[0], Args->XPath, NULL, NULL))) {
      xmlMoveTags(Self, tag->Index, 1, 0, XMI_PREV);
      xmlRemoveTag(Self, tag->Next->Index, 0x7fffffff);
   }
   else return ERR_Search;

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
FindTag: Searches for a tag via XPath.

This method will return the first tag that matches the search string specified in XPath.  Optionally, if the XPath uses
wildcards or would match multiple tags, a Callback function may be passed that will be called for each matching tag
that  is discovered.  The prototype for the callback function is `ERROR Function(*XML, XMLTag *Tag, STRING Attrib)`.

The callback routine can terminate the search early by returning ERR_Terminate.  All other error codes are ignored.

The #RootIndex value has no effect on this method.  Use #FindTagFromIndex() to make restricted searches.

-INPUT-
cstr XPath: An XPath string.
ptr(func) Callback: Optional reference to a function that should be called for each matching tag.
&int Result: The index of the first matching tag is returned in this parameter (not valid if a Callback is defined).

-ERRORS-
Okay: A matching tag was found.
NullArgs:
NoData:
Search: A matching tag could not be found.

*****************************************************************************/

static ERROR XML_FindTag(extXML *Self, struct xmlFindTag *Args)
{
   parasol::Log log;

   if (!Args) return ERR_NullArgs;
   if (Self->Flags & XMF_DEBUG) log.msg("XPath: %s", Args->XPath);
   if (!Self->Tags[0]) return ERR_NoData;

   XMLTag *tag;
   CSTRING attrib;
   if ((tag = find_tag(Self, Self->Tags[0], Args->XPath, &attrib, Args->Callback))) {
      if (Self->Flags & XMF_DEBUG) log.msg("Found tag %d, Attrib: %s", tag->Index, attrib);
      Args->Result = tag->Index;
      return ERR_Okay;
   }
   else if (Args->Callback) return ERR_Okay;
   else {
      if (Self->Flags & XMF_DEBUG) log.msg("Failed to find tag through XPath.");
      return ERR_Search;
   }
}

/*****************************************************************************

-METHOD-
FindTagFromIndex: Searches for a tag via XPath, starting from a specific tag index.

This method will return the first tag that matches the search string specified in XPath.  The search begins at the tag
indicated by the Start value.  This should be set to zero if searching from the top of the XML tree.

Optionally, if the XPath uses wildcards or would match multiple tags, a Callback function may be passed that will be
called for each matching tag that is discovered.  The C prototype for the callback function is
`ERROR Function(*XML, XMLTag *Tag, CSTRING Attrib)`.

For Fluid: `function Callback(XML {ID}, TagIndex {Long}, Attrib {String})`.

The callback routine can terminate the search early by returning ERR_Terminate.  All other error codes are ignored.

-INPUT-
cstr XPath: An XPath string.
int Start: The tag index to start searching from.
ptr(func) Callback: Optional reference to a function that should be called for each matching tag.
&int Result: The index of the first matching tag is returned in this parameter (not valid if a Callback is defined).

-ERRORS-
Okay:
OutOfRange: The Index is invalid.

*****************************************************************************/

static ERROR XML_FindTagFromIndex(extXML *Self, struct xmlFindTagFromIndex *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);
   if (Self->Flags & XMF_DEBUG) log.msg("XPath: %s", Args->XPath);
   if (!Self->Tags[0]) return ERR_NoData;
   if ((Args->Start < 0) or (Args->Start >= Self->TagCount)) return log.warning(ERR_OutOfRange);

   if ((!Args->XPath) or (!Args->XPath[0])) {
      Args->Result = Args->Start;
      return ERR_Okay;
   }

   XMLTag *tag;
   CSTRING attrib;
   if ((tag = find_tag(Self, Self->Tags[Args->Start], Args->XPath, &attrib, Args->Callback))) {
      if (Self->Flags & XMF_DEBUG) log.msg("Found tag %d, Attrib: %s", tag->Index, attrib);
      Args->Result = tag->Index;
      return ERR_Okay;
   }
   else if (Args->Callback) return ERR_Okay;
   else {
      if (Self->Flags & XMF_DEBUG) log.msg("Failed to find tag through XPath.");
      return ERR_Search;
   }
}

/*****************************************************************************

-METHOD-
GetAttrib: Retrieves the value of an XML attribute.

The GetAttrib method scans a tag for a specific attribute and returns it. You need to provide the tag index and the name
of the attribute that you are looking for.  If found, the attribute value is returned in the Value parameter.

A faster alternative for C/C++ users is to use the inline function XMLATTRIB(Tag,Attrib), which returns the attribute
value or NULL if not found.

-INPUT-
int Index: The index of the XML tag to search.
cstr Attrib: The name of the attribute to search for (case insensitive).  If NULL or an empty string, the tag name is returned as the result.
&cstr Value: The value of the attribute is returned here, or NULL if the named attribute does not exist.

-ERRORS-
Okay: The attribute was found.
Args: The required arguments were not specified.
NotFound: The attribute name was not found.

****************************************************************************/

static ERROR XML_GetAttrib(extXML *Self, struct xmlGetAttrib *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   LONG tagindex = Args->Index;
   if ((tagindex < 0) or (tagindex >= Self->TagCount)) return log.warning(ERR_OutOfRange);

   auto tag = Self->Tags[tagindex];

   if ((!Args->Attrib) or (!Args->Attrib[0])) {
      Args->Value = tag->Attrib->Name;
      return ERR_Okay;
   }

   for (LONG i=0; i < tag->TotalAttrib; i++) {
      if (!StrMatch(Args->Attrib, tag->Attrib[i].Name)) {
         Args->Value = tag->Attrib[i].Value;
         log.trace("Attrib %s = %s", Args->Attrib, Args->Value);
         return ERR_Okay;
      }
   }

   if (Self->Flags & XMF_DEBUG) log.msg("Attrib %s not found in tag %p", Args->Attrib, tag);
   return ERR_NotFound;
}

/*****************************************************************************

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

Direct tag lookups are also supported through variable fields (this is not normally possible using XPath).  You can
retrieve an attribute or the content of any tag if you know its index number, using this format:
`Tag(Name, Index, Attrib)`.

The `Name` indicates the name of the tag that you will be matching to.  If omitted, all tags are included in the lookup.
The `Index` is used to match to the nth tag name that you are comparing against if a Name is specified, otherwise the
Index is a direct lookup into the #Tags array.  If an `Attrib` name is specified, then the value for that
attribute will be returned.  If the Attrib name is omitted, the content of the matching tag will be returned.
-END-

*****************************************************************************/

static ERROR XML_GetVar(extXML *Self, struct acGetVar *Args)
{
   parasol::Log log;
   XMLTag *tags;
   LONG i, j, count;

   if (!Args) return log.warning(ERR_NullArgs);
   if ((!Args->Field) or (!Args->Buffer) or (Args->Size < 1)) return log.warning(ERR_NullArgs);
   if (!Self->initialised()) return log.warning(ERR_NotInitialised);

   CSTRING field = Args->Field;
   Args->Buffer[0] = 0;

   if (!StrCompare("TagCount", field, 0, 0)) {
      return log.warning(ERR_Obsolete); // Replaced with 'count:' and the existing TagCount field.
   }
   else if (!StrCompare("count:", field, 6, 0)) {
      if (!xmlCount(Self, field+6, &count)) {
         Args->Buffer[IntToStr(count, Args->Buffer, Args->Size)] = 0;
         return ERR_Okay;
      }
      else return ERR_Failed;
   }
   else if (!StrCompare("exists:", field, 7, 0)) {
      CSTRING attrib;

      XMLTag *tag = find_tag(Self,  Self->Tags[Self->RootIndex], field+7, &attrib, 0);

      Args->Buffer[0] = '0';
      Args->Buffer[1] = 0;

      if (!tag) return ERR_Okay;

      if (attrib) {
         for (i=0; i < tag->TotalAttrib; i++) {
            if (!StrMatch(tag->Attrib[i].Name, attrib)) {
               Args->Buffer[0] = '1';
               break;
            }
         }
      }
      else Args->Buffer[0] = '1';

      return ERR_Okay;
   }
   else if (!StrCompare("contentexists:", field, 14, 0)) {
      CSTRING str, attrib;

      XMLTag *tag = find_tag(Self,  Self->Tags[Self->RootIndex], field+14, &attrib, NULL);

      Args->Buffer[0] = '0';
      Args->Buffer[1] = 0;

      if (!tag) return ERR_Okay;

      tag = tag->Child;
      while (tag) {
         if ((!tag->Attrib->Name) and (tag->Attrib->Value)) {
            str = tag->Attrib->Value;
            while (*str) {
               if (*str > 0x20) {
                  Args->Buffer[0] = '1';
                  return ERR_Okay;
               }
               str++;
            }
         }
         tag = tag->Next;
      }

      return ERR_Okay;
   }
   else if (!StrCompare("Tag(", field, 0, 0)) {
      char tagname[40], attribute[40];
      LONG index, pos;

      // The tag field allows specific XML tags/attributes to be read.
      //   Format:  Tag(tagname, index, attribute)
      //   Example: Tag(menu, 3, menuitem)
      // NB: This functionality may appear to be superseded by XPath's, however it allows the user to directly lookup
      // tags based on their true index number (including content tags).

      i = 0;
      for (pos=4; ((size_t)i < sizeof(tagname)-1) and (field[pos]) and (field[pos] != ')') and (field[pos] != ','); pos++) {
         tagname[i++] = field[pos];
      }
      tagname[i] = 0;

      if (field[pos] IS ',') pos++;
      while ((field[pos] > 0) and (field[pos] <= 0x20)) pos++;

      // Get the index number

      if ((field[pos] >= '0') and (field[pos] <= '9')) {
         index = StrToInt(field+pos);
      }
      else index = 0;

      while ((field[pos]) and (field[pos] != ')') and (field[pos] != ',')) pos++;
      if (field[pos] IS ',') pos++;
      while ((field[pos] > 0) and (field[pos] <= 0x20)) pos++;

      // Get the name of the requested attribute

      i = 0;
      while (((size_t)i < sizeof(attribute)) and (field[pos]) and (field[pos] != ')') and (field[pos] != ',')) {
         attribute[i++] = field[pos++];
      }
      attribute[i] = 0;

      // Find the tag and extract the data

      if (!tagname[0]) {
         if (index < Self->TagCount) tags = Self->Tags[index];
         else tags = NULL;
      }
      else {
         tags = Self->Tags[Self->RootIndex];
         while (tags) {
            if (!StrMatch(tagname, tags->Attrib->Name)) {
               if (!index) break;
               index--;
               if (tags->Next) {
                  tags = tags->Next; // Skip all children so that we don't include children with the same tag name
                  continue;
               }
            }
            tags = Self->Tags[tags->Index+1];
         }
      }

      if (tags) {
         if (attribute[0]) {
            // Extract tag attribute value
            for (j=0; j < tags->TotalAttrib; j++) {
               if (!StrMatch(attribute, tags->Attrib[j].Name)) {
                  if ((tags->Attrib[j].Value) and (tags->Attrib[j].Value[0])) StrCopy(tags->Attrib[j].Value, Args->Buffer, Args->Size);
                  return ERR_Okay;
               }
            }
         }
         else {
            // Extract tag content

            Args->Buffer[0] = 0;
            j = 0;
            if ((tags = tags->Child)) {
               for (i=0; i < tags->TotalAttrib; i++) {
                  if (!tags->Attrib[i].Name) {
                     while ((j < Args->Size) and (tags->Attrib[i].Value[j])) {
                        j += StrCopy(tags->Attrib[i].Value, Args->Buffer+j, Args->Size-j);
                     }
                  }
               }
            }
         }
         return ERR_Okay;
      }

      log.msg("Search failed: %s", field);
      return ERR_Search;
   }
   else if ((!StrCompare("xpath:", field, 6, 0)) or
            (!StrCompare("xml:", field, 4, 0)) or
            (!StrCompare("content:", field, 8, 0)) or
            (!StrCompare("extract:", field, 8, 0)) or
            (field[0] IS '/')) {
      LONG j;
      for (j=0; field[j] and (field[j] != '/'); j++) j++;

      CSTRING attrib;
      XMLTag *current = find_tag(Self,  Self->Tags[Self->RootIndex], field+j, &attrib, NULL);

      if (!current) {
         log.msg("Failed to lookup tag '%s'", field+j);
         return ERR_Search;
      }

      if (attrib) { // Extract attribute value
         for (i=0; i < current->TotalAttrib; i++) {
            if (!StrMatch(current->Attrib[i].Name, attrib)) {
               StrCopy(current->Attrib[i].Value, Args->Buffer, Args->Size);
               return ERR_Okay;
            }
         }
         return ERR_Failed;
      }
      else {
         // Extract tag content

         UBYTE extract;

         if (!StrCompare("content:", field, 8, 0)) extract = 1; // 'In-Depth' content extract.
         else if (!StrCompare("extract:", field, 8, 0)) extract = 2;
         else extract = 0;

         Args->Buffer[0] = 0;
         if (extract IS 1) {
            return get_content(Self, current, Args->Buffer, Args->Size);
         }
         else if (extract IS 2) {
            STRING str;
            ERROR error = xmlGetString(Self, current->Child->Index, XMF_INCLUDE_SIBLINGS, &str);
            if (!error) {
               StrCopy(str, Args->Buffer, Args->Size);
               FreeResource(str);
            }

            return error;
         }
         else { // 'Immediate' content extract (not deep)
            tags = current->Child;
            LONG j = 0;
            while (tags) {
               if (!tags->Attrib->Name) j += StrCopy(tags->Attrib->Value, Args->Buffer+j, Args->Size-j);
               tags = tags->Next;
            }
            if (j >= Args->Size-1) log.warning(ERR_BufferOverflow);
         }
      }

      return ERR_Okay;
   }
   else {
      log.msg("Unsupported field \"%s\".", field);
      return ERR_UnsupportedField;
   }
}

/*****************************************************************************

-METHOD-
GetContent: Extracts the content embedded inside an XML tag.

The GetContent method is used to extract the string content from an XML tag.  It will extract content that is
immediately embedded within the XML tag and will not perform deep analysis of the tag structure (refer to
#GetString() for deep extraction).  Consider the following structure:

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
BufferOverflow: The buffer was not large enough to hold the content (the resulting string will be valid but truncated).

*****************************************************************************/

static ERROR XML_GetContent(extXML *Self, struct xmlGetContent *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Buffer)) return log.warning(ERR_NullArgs);
   if (Args->Length < 1) return log.warning(ERR_Args);
   if ((Args->Index < 0) or (Args->Index >= Self->TagCount)) return log.warning(ERR_OutOfRange);

   return get_content(Self, Self->Tags[Args->Index], Args->Buffer, Args->Length);
}

/*****************************************************************************

-METHOD-
GetString: Retrieves data from an XML object in standard XML string format.

The GetString method builds XML strings from data that has been loaded into an XML object.  The string is created from
the entire XML object or from a specific area of the XML tree by setting the Index parameter.

The XML string that is built by this method will be stored in the Result parameter.  The memory block must be freed
once the content is no longer required.

-INPUT-
int Index: Index to a source tag for pulling data out of the XML object.
int(XMF) Flags: Special flags that affect the construction of the XML string.
!str Result: The resulting string is returned in this parameter.

-ERRORS-
Okay: The XML string was retrieved.
Args:
NoData: No information has been loaded into the XML object.
AllocMemory: Failed to allocate an XML string for the result.

*****************************************************************************/

static ERROR XML_GetString(extXML *Self, struct xmlGetString *Args)
{
   parasol::Log log;

   if (Self->TagCount <= 0) return log.warning(ERR_NoData);
   if (!Args) return log.warning(ERR_NullArgs);
   if ((Args->Index < 0) or (Args->Index >= Self->TagCount)) return log.warning(ERR_OutOfRange);

   log.traceBranch("Tag: %d", Args->Index);

   // Calculate the size of the buffer required to save the XML information

   XMLTag *tag;
   if (!(tag = Self->Tags[Args->Index])) return log.warning(ERR_InvalidData);

   LONG size;
   if (Args->Flags & XMF_INCLUDE_SIBLINGS) {
      size = 0;
      for (auto scan=tag; scan; scan=scan->Next) {
         len_xml_str(scan, Args->Flags, &size);
      }
   }
   else {
      size = 0;
      len_xml_str(tag, Args->Flags, &size);
   }

   // Allocate a buffer large enough to hold the XML information

   STRING buffer;
   if (!AllocMemory(size+1, MEM_STRING|MEM_NO_CLEAR|MEM_CALLER, &buffer, NULL)) {
      buffer[0] = 0;

      LONG offset;
      if (Args->Flags & XMF_INCLUDE_SIBLINGS) {
         offset = 0;
         for (auto scan=tag; scan; scan=scan->Next) {
            build_xml_string(scan, buffer, Args->Flags, &offset);
         }
      }
      else {
         offset = 0;
         build_xml_string(tag, buffer, Args->Flags, &offset);
      }

      if ((offset != size) and (!(Args->Flags & XMF_STRIP_CDATA))) {
         log.warning("Wrote %d bytes instead of the expected %d", offset, size);
      }
      else log.trace("Finished writing %d bytes.", size);

      Args->Result = buffer;
      return ERR_Okay;
   }
   else return log.warning(ERR_AllocMemory);
}

/*****************************************************************************

-METHOD-
GetTag: Returns a pointer to the XMLTag structure for a given tag index.

This method will return the XMLTag structure for a given tag Index.  The Index is checked to ensure it is valid prior
to retrieval, and an ERR_OutOfRange error will be returned if it is invalid.

-INPUT-
int Index:  The index of the tag that is being retrieved.
&struct(*XMLTag) Result: The XMLTag is returned in this parameter.

-ERRORS-
Okay
NullArgs
OutOfRange: The Index parameter is invalid.

*****************************************************************************/

static ERROR XML_GetTag(extXML *Self, struct xmlGetTag *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);
   if ((Args->Index < 0) or (Args->Index >= Self->TagCount)) return log.warning(ERR_OutOfRange);

   Args->Result = Self->Tags[Args->Index];
   return ERR_Okay;
}

//****************************************************************************

static ERROR XML_Init(extXML *Self, APTR Void)
{
   parasol::Log log;

   if (Self->SubID) return ERR_Okay; // Break here for sub-classes to perform initialisation

   if (Self->Statement) {
      if ((Self->ParseError = txt_to_xml(Self, Self->Statement))) {
         if ((Self->ParseError IS ERR_InvalidData) or (Self->ParseError IS ERR_NoData)) return ERR_NoSupport;

         log.warning("XML parsing error #%d: %s", Self->ParseError, GetErrorMsg(Self->ParseError));
      }

      FreeResource(Self->Statement);
      Self->Statement = NULL;

      return Self->ParseError;
   }
   else if ((Self->Path) or (Self->Source)) {
      if (Self->Flags & XMF_NEW) {
         return ERR_Okay;
      }
      else if (parse_source(Self)) {
         log.warning("XML parsing error: %s [File: %s]", GetErrorMsg(Self->ParseError), Self->Path ? Self->Path : "Object");
         return Self->ParseError;
      }
      else {
         #ifdef DEBUG
            debug_tree("Init", Self);
         #endif
         return ERR_Okay;
      }
   }
   else {
      // NOTE: We do not fail if no data has been loaded into the XML object, the developer may be creating an XML data
      // structure from scratch, or could intend to send us information later.

      if (!(Self->Flags & XMF_NEW)) log.msg("Warning: No content given.");
      return ERR_Okay;
   }
}

/*****************************************************************************

-METHOD-
InsertContent: Inserts XML content into the XML tree.

The InsertContent method is used to insert content strings into any position within the XML tree.  A content string
must be provided in the Content parameter and the target insertion point is specified in the Index parameter.
An insertion point relative to the target index must be specified in the Where parameter.  The new tags can be
inserted as a child of the target by using a Where value of XMI_CHILD.  To insert behind or after the target, use
XMI_PREV or XMI_NEXT.

To modify existing content, the #SetAttrib() method should be used.

-INPUT-
int Index: The index to which the statement should be inserted (zero is the beginning).  If the index exceeds the #TagCount, the value will be automatically limited to the last tag index.
int(XMI) Where: Use XMI_PREV or XMI_NEXT to insert behind or ahead of the target tag.  Use XMI_CHILD for a child insert.
cstr Content: The content to insert.
&int Result: The index of the new tag is returned here.

-ERRORS-
Okay
NullArgs
ReadOnly

*****************************************************************************/

static ERROR XML_InsertContent(extXML *Self, struct xmlInsertContent *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);
   if (Self->ReadOnly) return log.warning(ERR_ReadOnly);
   if (Self->Flags & XMF_DEBUG) log.branch("Index: %d, Insert: %d", Args->Index, Args->Where);

   // The insertion method is simple: Add the XML statement to the end
   // of the XML tags, then move the new XML tags to the insertion point.

   ERROR error;
   LONG srcindex = Self->TagCount; // Store the tag count before we add our new tag to the tree
   if (!(error = acDataXML(Self, "<x/>"))) { // Add a dummy tag - this will be converted to a content tag
      LONG total = 0;
      for (auto tag=Self->Tags[srcindex]; tag; tag=tag->Next) total++;

      xmlSetAttrib(Self, Self->Tags[srcindex]->Index, 0, NULL, Args->Content); // Convert the tag to content

      if (!(error = xmlMoveTags(Self, srcindex, total, Args->Index, Args->Where))) {
         XMLTag *result;

         if (!srcindex) result = Self->Tags[0];
         else if (Args->Where IS XMI_NEXT)  result = Self->Tags[Args->Index]->Next;
         else if (Args->Where IS XMI_CHILD) result = Self->Tags[Args->Index]->Child;
         else if (Args->Where IS XMI_PREV) {
            if (!Args->Index) result = Self->Tags[Args->Index];
            else result = Self->Tags[Args->Index]->Prev;
         }
         else result = NULL;

         if (result) Args->Result = result->Index;
         else Args->Result = 0;

         return ERR_Okay;
      }
      else return error;
   }
   else return error;
}

/*****************************************************************************

-METHOD-
InsertXML: Inserts an XML statement in the XML tree.

The InsertXML method is used to translate and insert a new set of XML tags into any position within the XML tree.  A
standard XML statement must be provided in the XML parameter and the target insertion point is specified in the Index
parameter.  An insertion point relative to the target index must be specified in the Insert parameter.  The new tags
can be inserted as a child of the target by using a Insert value of XMI_CHILD.  Use XMI_CHILD_END to insert at the end
of the child list.  To insert behind or after the target, use XMI_PREV or XMI_NEXT.

The #RootIndex value has no effect on this method.

-INPUT-
int Index:  The index to which the statement should be inserted (zero is the beginning).
int(XMI) Where: Use XMI_PREV or XMI_NEXT to insert behind or ahead of the target tag.  Use XMI_CHILD or XMI_CHILD_END for a child insert.
cstr XML:    The statement to process.
&int Result: The resulting tag index.

-ERRORS-
Okay: The statement was added successfully.
Args
NullArgs
OutOfRange
ReadOnly: Changes to the XML data are not permitted.

*****************************************************************************/

static ERROR XML_InsertXML(extXML *Self, struct xmlInsertXML *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);
   if ((Args->Index < 0) or (Args->Index > Self->TagCount)) return log.warning(ERR_OutOfRange);
   if ((Args->Where < 0) or (Args->Where >= XMI_END)) return log.warning(ERR_Args);

   if (Self->ReadOnly) return log.warning(ERR_ReadOnly);

   log.traceBranch("Index: %d, Where: %d, XML: %.40s", Args->Index, Args->Where, Args->XML);

   LONG srcindex = Self->TagCount;
   LONG index    = Args->Index;
   LONG insert   = Args->Where;

   // The insertion method is simple: Add the XML statement to the end of the XML tags, then move the new XML tags to
   // the insertion point.

   ERROR error;
   if (!(error = acDataXML(Self, Args->XML))) {
      LONG total = 0;
      for (auto tag=Self->Tags[srcindex]; tag; tag=tag->Next) total++;

      #ifdef DEBUG_TREE_INSERT
         debug_tree("Insert-Before", Self);
      #endif

      if (insert IS XMI_CHILD) {
         // If we're doing a child insert, check if a content tag already exists under the target.  If so, change the
         // insertion so that the XML comes after the content.

         auto tag = Self->Tags[index]->Child;
         if ((tag) and (!tag->Attrib[0].Name)) {
            log.trace("Target tag %d has content - switching from XMI_CHILD to XMI_NEXT.", index);
            insert = XMI_NEXT;
            index = tag->Index;
         }
      }
      else if (insert IS XMI_CHILD_END) {
         // We modify CHILD_END because MoveTags() doesn't support it and we need to calculate the insertion point in
         // advance anyway.

         auto tag = Self->Tags[index]->Child;
         if (!tag) insert = XMI_CHILD;
         else {
            while (tag->Next) tag = tag->Next;
            index = tag->Index;
            insert = XMI_NEXT;
         }
      }

      // Move the tags

      if (!(error = xmlMoveTags(Self, srcindex, total, index, insert))) {
         XMLTag *result;

         #ifdef DEBUG_TREE_INSERT
            debug_tree("Insert-After", Self);
         #endif

         if (!srcindex) result = Self->Tags[0];
         else if (insert IS XMI_NEXT)  result = Self->Tags[index]->Next;
         else if (insert IS XMI_CHILD) result = Self->Tags[index]->Child;
         else if (insert IS XMI_PREV) {
            if (!index) result = Self->Tags[index];
            else result = Self->Tags[index]->Prev;
         }
         else result = Self->Tags[srcindex];

         if (result) Args->Result = result->Index;
         else Args->Result = 0;

         return ERR_Okay;
      }
      else return error;
   }
   else return error;
}

/*****************************************************************************

-METHOD-
InsertXPath: Inserts an XML statement in an XML tree.

The InsertXPath method is used to translate and insert a new set of XML tags into any position within the XML tree.  A
standard XML statement must be provided in the XML parameter and the target insertion point is referenced as a valid
XPath location string.  An insertion point relative to the XPath target must be specified in the Insert parameter.  The
new tags can be inserted as a child of the target by using an Insert value of XMI_CHILD or XMI_CHILD_END.  To insert
behind or after the target, use XMI_PREV or XMI_NEXT.

-INPUT-
cstr XPath: An XPath string that refers to the target insertion point.
int(XMI) Where: Use XMI_PREV or XMI_NEXT to insert behind or ahead of the target tag.  Use XMI_CHILD for a child insert.
cstr XML: The statement to process.
&int Result: The index of the new tag is returned here.

-ERRORS-
Okay
NullArgs
Search: The XPath could not be resolved.

*****************************************************************************/

ERROR XML_InsertXPath(extXML *Self, struct xmlInsertXPath *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->XPath) or (!Args->XML)) return log.warning(ERR_NullArgs);

   if (Self->ReadOnly) return log.warning(ERR_ReadOnly);

   log.branch("Insert: %d, XPath: %s", Args->Where, Args->XPath);

   XMLTag *tag;
   CSTRING attrib;
   if ((tag = find_tag(Self, Self->Tags[0], Args->XPath, &attrib, NULL))) {
      ERROR error;
      struct xmlInsertXML insert = { .Index  = tag->Index, .Where = Args->Where, .XML = Args->XML };
      if (!(error = XML_InsertXML(Self, &insert))) {
         Args->Result = insert.Result;
         return ERR_Okay;
      }
      else return error;
   }
   else return ERR_Search;
}

/*****************************************************************************

-METHOD-
MoveTags: Move an XML tag group to a new position in the XML tree.

This method is used to move XML tags within the XML tree structure.  It supports the movement of single and groups of
tags from one index to another.  The client must supply the index of the tag that will be moved and the index of the
target tag.  All child tags of the source will be included in the move.

An insertion point relative to the target index must be specified in the Where parameter.  The source tag can be
inserted as a child of the destination by using a Where of XMI_CHILD.  To insert behind or after the target, use
XMI_PREV or XMI_NEXT.

-INPUT-
int Index: Index of the source tag to be moved.
int Total: The total number of sibling tags to be moved from the source index.  Minimum value of 1.
int DestIndex: The destination tag index.  If the index exceeds the #TagCount, the value will be automatically limited to the last tag index.
int(XMI) Where: Use XMI_PREV or XMI_NEXT to insert behind or ahead of the target tag.  Use XMI_CHILD for a child insert.

-ERRORS-
Okay: Tags were moved successfully.
NullArgs
ReadOnly
-END-

*****************************************************************************/

static void recalc_indexes(extXML *Self, XMLTag *Tag, LONG *Index, LONG *);

static ERROR XML_MoveTags(extXML *Self, struct xmlMoveTags *Args)
{
   parasol::Log log;
   LONG min_index, max_index;

   if (!Args) return log.warning(ERR_NullArgs);
   if (Self->ReadOnly) return log.warning(ERR_ReadOnly);

   LONG srcindex  = Args->Index;
   LONG destindex = Args->DestIndex;
   LONG total     = Args->Total;

   if (srcindex IS destindex) return ERR_Okay;

   // Check the index ranges

   if ((srcindex < 0) or (srcindex > Self->TagCount)) srcindex = 0;
   if (destindex < 0) destindex = 0;
   if (destindex >= Self->TagCount) destindex = Self->TagCount - 1;

   if (total < 1) total = 1;
   if (srcindex + total > Self->TagCount) total = Self->TagCount - srcindex;

   // Get the true total by counting child tags in the range to be moved

   log.trace("Validating total tags and calculating true total.");

   LONG total_tags = 0;
   LONG last_tag = srcindex;

   {
      auto tag = Self->Tags[srcindex];
      LONG i;
      for (i=0; (i < total) and (tag); i++) {
         if (tag->Child) tag_count(tag->Child, &total_tags);
         last_tag = tag->Index; // The last top-level tag in the source set
         tag = tag->Next;
         total_tags++;
      }
      total = i;
   }

   if ((destindex >= srcindex) and (destindex < srcindex + total_tags)) return log.warning(ERR_Args);

   if (srcindex < destindex) {
      min_index = srcindex;
      max_index = destindex;
   }
   else {
      min_index = destindex;
      max_index = srcindex;
      if ((Args->Where IS XMI_CHILD) or (Args->Where IS XMI_CHILD_END)) min_index++;
   }

   auto src = Self->Tags[srcindex];
   auto dest = Self->Tags[destindex];
   auto last = Self->Tags[last_tag];

   log.trace("%d (%p) to %d (%p), Total: %d, TotalTags: %d, Last: %d (%p), Mode: %d", srcindex, src, destindex, dest, total, total_tags, last_tag, last, Args->Where);

   // This set of checks prevents us from going any further if the new position is the same as the current position.

   if ((Args->Where IS XMI_NEXT) and (dest->Next IS src)) return ERR_Okay;
   else if ((Args->Where IS XMI_PREV) and (dest->Prev IS src)) return ERR_Okay;
   else if ((Args->Where IS XMI_CHILD) and (dest->Child IS src)) return ERR_Okay;

   #ifdef DEBUG_TREE_MOVE
      debug_tree("Move-Before", Self);
   #endif

   // Rearrange the XML tag array.  First, untangle the source tag-set from its siblings and parent.  Then patch the
   // source into the destination.

   if (src->Prev)  src->Prev->Next = last->Next;
   if (last->Next) last->Next->Prev = src->Prev;
   if ((srcindex > 0) and (Self->Tags[srcindex-1]->Child IS Self->Tags[srcindex])) {
      Self->Tags[srcindex-1]->Child = last->Next;
   }
   src->Prev = NULL;
   last->Next = NULL;

   if (Args->Where IS XMI_PREVIOUS) { // Insert behind the target
      if (dest->Prev) dest->Prev->Next = src;
      else if (!destindex) Self->Tags[0] = src;
      else if (Self->Tags[destindex-1]->Child IS dest) Self->Tags[destindex-1]->Child = src;
      src->Prev  = dest->Prev;
      last->Next = dest;
      dest->Prev = last;
   }
   else if (Args->Where IS XMI_CHILD) {
      // Child insert.  The source tag we are moving becomes an immediate child of the destination tag, i.e. it goes at
      // the start of the list if there are other children present.

      last->Next = dest->Child;
      if (last->Next) last->Next->Prev = src;
      dest->Child = src;
   }
   else if (Args->Where IS XMI_NEXT) { // Next insert
      if (dest->Next) dest->Next->Prev = last;
      if (destindex) src->Prev  = dest;
      last->Next = dest->Next;
      dest->Next = src;
   }
   else return log.warning(ERR_Args);

   // Rebuild the tag array

   LONG i = 0;
   LONG nest = 0;
   recalc_indexes(Self, Self->Tags[0], &i, &nest);

   #ifdef DEBUG_TREE_MOVE
      debug_tree("Move-After", Self);
   #endif

   Self->Modified++;
   return ERR_Okay;
}

static void recalc_indexes(extXML *Self, XMLTag *Tag, LONG *Index, LONG *Level)
{
   while (Tag) {
      Self->Tags[*Index] = Tag;
      Tag->Index = *Index;
      Tag->Branch = *Level;
      *Index = *Index + 1;
      if (Tag->Child) {
         *Level = *Level + 1;
         recalc_indexes(Self, Tag->Child, Index, Level);
         *Level = *Level - 1;
      }
      Tag = Tag->Next;
   }
}

//****************************************************************************

static ERROR XML_NewObject(extXML *Self, APTR Void)
{
   if (!AllocMemory(sizeof(APTR), MEM_DATA, &Self->Tags, NULL)) {
      Self->ParseError = ERR_Okay;
      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

/*****************************************************************************

-METHOD-
RemoveTag: Removes tag(s) from the XML structure.

The RemoveTag method is used to remove one or more tags from an XML structure.  Child tags will automatically be
discarded as a consequence of using this method, in order to maintain a valid XML structure.

This method is capable of deleting multiple tags if the Total parameter is set to a value greater than 1.  Each
consecutive tag and its children following the targeted tag will be removed from the XML structure until the count is
exhausted. This is useful for mass delete operations.

After using this method, you must assume that all tag addresses have been changed due to the rearrangement of the XML
structure.  Thus if you have obtained pointers to various parts of the XML structure, they are invalid and must be
recalculated.

-INPUT-
int Index: Reference to the tag that will be removed.
int Total: The total number of sibling (neighbouring) tags that should also be deleted.  A value of one or less will remove only the indicated tag and its children.  The total may exceed the number of tags actually available, in which case all tags up to the end of the branch will be affected.

-ERRORS-
Okay
NullArgs
OutOfRange
ReadOnly
-END-

*****************************************************************************/

static ERROR XML_RemoveTag(extXML *Self, struct xmlRemoveTag *Args)
{
   parasol::Log log;
   LONG i;

   if (!Args) return log.warning(ERR_NullArgs);
   if ((Args->Index < 0) or (Args->Index >= Self->TagCount)) return log.warning(ERR_OutOfRange);
   if (Self->ReadOnly) return log.warning(ERR_ReadOnly);
   if (Self->Flags & XMF_LOCK_REMOVE) return log.warning(ERR_ReadOnly);

   LONG index = Args->Index;
   LONG count = Args->Total;
   if (count < 1) count = 1;

   #if defined(DEBUG) || defined(DEBUG_TREE_REMOVE)
      log.msg("Tag: %d, Total: %d", index, count);
   #endif

   #ifdef DEBUG_TREE_REMOVE
      debug_tree("Remove-Before", Self);
   #endif

   // Determine what the last valid tag would be when the tags are removed (may be NULL if there would be no further
   // tags in this section of the XML hierarchy).

   auto last_tag = Self->Tags[index];
   for (i=0; (i < count) and (last_tag); i++) {
      last_tag = last_tag->Next;
   }

   // If we ran out of tags, update the count variable so that it is accurate.  Note that this figure counts for
   // top-level tags only and does not include child tags.

   if (i < count) count = i + 1;

   // Calculate the total number of tags that we are going to remove, including child tags.

   LONG actual_count = 0;
   auto tag = Self->Tags[index];
   for (i=0; (i < count) and (tag); i++) {
      actual_count++;
      if (tag->Child) tag_count(tag->Child, &actual_count);
      tag = tag->Next;
   }

   // Fix up the address pointers of neighbouring tags

   if (index > 0) {
      if (Self->Tags[index-1]->Child IS Self->Tags[index]) {
         // If the tag to be removed is the child of a parent tag, fix up the parent's child pointer so that it points
         // to the next child in the list.

         Self->Tags[index-1]->Child = last_tag;
      }
      else {
         if (Self->Tags[index]->Prev) {
            // Fix up the Next pointer of the previous neighbouring tag
            Self->Tags[index]->Prev->Next = last_tag;
         }
      }
   }

   if (last_tag) last_tag->Prev = Self->Tags[index]->Prev;

   // Remove the tags from the array

   for (LONG i=index; (i < index + actual_count); i++) {
      if (Self->Tags[i]) { FreeResource(Self->Tags[i]); Self->Tags[i] = NULL; }
   }

   // Clean up the hole that we have left in the tag list array

   CopyMemory(Self->Tags + index + actual_count,
              Self->Tags + index,
              sizeof(APTR) * (Self->TagCount - (index + actual_count)));

   Self->TagCount -= actual_count; // Subtract the total number of tags that were removed
   Self->Tags[Self->TagCount] = NULL; // Terminate the array

   for (LONG i=0; i < Self->TagCount; i++) {  // Repair index numbers
      Self->Tags[i]->Index = i;
   }

   #ifdef DEBUG_TREE_REMOVE
      debug_tree("Remove-After", Self);
   #endif

   Self->Modified++;
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
RemoveXPath: Removes tag(s) from the XML structure, using an xpath lookup.

The RemoveXPath method is used to remove one or more tags from an XML structure.  Child tags will automatically be
discarded as a consequence of using this method, in order to maintain a valid XML structure.

Individual tag attributes can also be removed if an attribute is referenced at the end of the XPath.

The removal routine will be repeated so that each tag that matches the XPath will be deleted, or the Total is reached.

After using this method, you must assume that all tag addresses have been changed due to the rearrangement of the XML
structure.  Thus if you have obtained pointers to various parts of the XML structure then you should proceed to consider
them invalid and re-establish them.

-INPUT-
cstr XPath: An XML path string.
int Total: The total number of matching tags that should be deleted.  A value of one or less will remove only the indicated tag and its children.  The total may exceed the number of tags actually available, in which case all matching tags up to the end of the tree will be affected.

-ERRORS-
Okay
NullArgs
ReadOnly
-END-

*****************************************************************************/

static ERROR XML_RemoveXPath(extXML *Self, struct xmlRemoveXPath *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->XPath)) return ERR_NullArgs;

   if (Self->ReadOnly) return log.warning(ERR_ReadOnly);
   if (Self->Flags & XMF_LOCK_REMOVE) return log.warning(ERR_ReadOnly);

   LONG count = Args->Total;
   if (count < 0) count = 0x7fffffff;
   auto tag = Self->Tags[Self->RootIndex];
   while ((tag) and (count > 0)) {
      CSTRING attrib;
      ERROR error;
      if ((tag = find_tag(Self, tag, Args->XPath, &attrib, NULL))) {
         LONG i = tag->Index;

         if (attrib) { // Remove an attribute
            for (LONG index=0; index < tag->TotalAttrib; index++) {
               if (!StrMatch(attrib, tag->Attrib[index].Name)) {
                  xmlSetAttrib(Self, i, index, NULL, NULL);
                  break;
               }
            }
         }
         else if ((error = xmlRemoveTag(Self, i, 1))) return error;

         count--;

         if (Self->Tags[i]) tag = Self->Tags[i]->Next;
         else break;
      }
      else break;
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Reset: Clears the information held in an XML object.
-END-
*****************************************************************************/

static ERROR XML_Reset(extXML *Self, APTR Void)
{
   return acClear(Self);
}

/*****************************************************************************
-ACTION-
SaveToObject: Saves XML data to a storage object (e.g. file).
-END-
*****************************************************************************/

static ERROR XML_SaveToObject(extXML *Self, struct acSaveToObject *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->DestID)) return log.warning(ERR_NullArgs);
   if (Self->TagCount <= 0) return ERR_Okay;

   log.traceBranch("To: %d", Args->DestID);

   ERROR error;
   STRING str;
   if (!(error = xmlGetString(Self, 0, XMF_READABLE|XMF_INCLUDE_SIBLINGS, &str))) {
      struct acWrite write = { str, StrLength(str) };
      if (ActionMsg(AC_Write, Args->DestID, &write) != ERR_Okay) error = ERR_Write;
      FreeResource(str);
      return error;
   }
   else return error;
}

/*****************************************************************************

-METHOD-
SetAttrib: Adds, updates and removes XML attributes.

This method is used to update and add attributes to existing XML tags, as well as adding or modifying content.  You
need to supply the address of a tag index and the index number of the attribute that you are going to update.

The data for the attribute is defined in the Name and Value parameters. You can set a Value of "" if no data is to be
associated with the attribute.  Set the Value pointer to NULL to remove the attribute. If both Name and Value are NULL,
an error will be returned.

NOTE: The attribute at index 0 declares the name of the tag and should not normally be accompanied with a
value declaration.  However, if the tag represents content within its parent, then the Name must be set to NULL and the
Value string will determine the content.

NOTE: The address of the modified tag will be invalidated as a result of calling this method.  Any tag
pointers that have been stored by the client program will need to be refreshed from the XML tag list after calling this
method, for example:

<pre>
LONG index = tag->Index; // Store the index of tag.
xmlSetAttrib(XML, tag->Index, XMS_NEW, "name", "value");
tag = XML->Tags[index]; // Refresh the pointer to tag.
</pre>

-INPUT-
int Index: Index to the XML tag that is to be updated.
int(XMS) Attrib: Either the index number of the attribute that is to be updated, or set to XMS_NEW, XMS_UPDATE or XMS_UPDATE_ONLY.
cstr Name: String containing the new name for the attribute.  If NULL, the name will not be changed.  If Attrib is XMS_NEW, XMS_UPDATE or XMS_UPDATE_ONLY, the Name is used to find the attribute.
cstr Value: String containing the new value for the attribute.  If NULL, the attribute is removed.

-ERRORS-
Okay
NullArgs
Args
OutOfRange: The Index or Attrib value is out of range.
AllocMemory: A call to AllocMemory() failed.
Search: The attribute, identified by Name, could not be found.
ReadOnly: The XML object is read-only.
-END-

*****************************************************************************/

static ERROR XML_SetAttrib(extXML *Self, struct xmlSetAttrib *Args)
{
   parasol::Log log;
   XMLTag *newtag;
   STRING buffer;
   CSTRING name, value;
   LONG i, j, n, namelen, valuelen, pos;

   if (!Args) return log.warning(ERR_NullArgs);
   if ((Args->Index < 0) or (Args->Index >= Self->TagCount)) return log.warning(ERR_OutOfRange);
   if (Self->ReadOnly) return log.warning(ERR_ReadOnly);

   log.trace("Tag: %d, Attrib: $%.8x, %s = '%s'", Args->Index, Args->Attrib, Args->Name, Args->Value);

   LONG tagindex = Args->Index;
   XMLTag *tag = Self->Tags[tagindex];
   LONG attribindex = Args->Attrib;

   // If Attrib is XMS_UPDATE, we need to search for the attribute by name

   if ((attribindex IS XMS_UPDATE) or (attribindex IS XMS_UPDATE_ONLY)) {
      for (attribindex=0; attribindex < tag->TotalAttrib; attribindex++) {
         if (!StrMatch(Args->Name, tag->Attrib[attribindex].Name)) {
            break;
         }
      }

      if (attribindex >= tag->TotalAttrib) {
         if (Args->Attrib IS XMS_UPDATE) {
            if ((!Args->Value) or (!Args->Value[0])) return ERR_Okay; // User wants to remove a non-existing attribute, so return ERR_Okay
            else { // Create a new attribute if the named attribute wasn't found
               attribindex = XMS_NEW;
            }
         }
         else return ERR_Search;
      }
   }

   if (attribindex IS XMS_NEW) {
      // Add a new attribute

      XMLAttrib *attrib = tag->Attrib;
      LONG attribsize = tag->AttribSize;

      if ((name = Args->Name)) attribsize += StrLength(name) + 1;
      else return log.warning(ERR_NullArgs);

      if ((value = Args->Value)) attribsize += StrLength(value) + 1;

      if (AllocMemory(sizeof(XMLTag) + Self->PrivateDataSize + (sizeof(XMLAttrib) * (tag->TotalAttrib + 1)) + attribsize,
            MEM_UNTRACKED, &newtag, NULL) != ERR_Okay) {
         return log.warning(ERR_AllocMemory);
      }

      // Copy the old tag details to the new tag

      CopyMemory(tag, newtag, sizeof(XMLTag) + Self->PrivateDataSize);

      // Fix up address pointers on either side of the tag, as well as the parent's child tag, if there is an immediate parent.

      if (newtag->Prev) newtag->Prev->Next = newtag;
      if (newtag->Next) newtag->Next->Prev = newtag;
      if ((tagindex > 0) and (Self->Tags[tagindex-1]->Child IS tag)) {
         Self->Tags[tagindex-1]->Child = newtag;
      }

      newtag->Private = (BYTE *)newtag + sizeof(XMLTag);
      newtag->Attrib  = (XMLAttrib *)((BYTE *)newtag + sizeof(XMLTag) + Self->PrivateDataSize);
      buffer          = (STRING)(newtag->Attrib + tag->TotalAttrib + 1);
      LONG pos = 0;
      for (i=0; i < tag->TotalAttrib; i++) {
         if (attrib[i].Name) {
            newtag->Attrib[i].Name = buffer+pos;
            for (j=0; attrib[i].Name[j]; j++) buffer[pos++] = attrib[i].Name[j];
            buffer[pos++] = 0;
         }
         else newtag->Attrib[i].Name = NULL;

         if (attrib[i].Value) {
            newtag->Attrib[i].Value = buffer+pos;
            for (j=0; attrib[i].Value[j]; j++) buffer[pos++] = attrib[i].Value[j];
            buffer[pos++] = 0;
         }
         else newtag->Attrib[i].Value = NULL;
      }

      // Add the new name/value strings to the end of the list

      newtag->TotalAttrib = tag->TotalAttrib + 1;

      if (name) {
         newtag->Attrib[i].Name = buffer + pos;
         for (j=0; name[j]; j++) buffer[pos++] = name[j];
         buffer[pos++] = 0;
      }
      else newtag->Attrib[i].Name = NULL;

      if (value) {
         newtag->Attrib[i].Value = buffer + pos;
         for (j=0; value[j]; j++) buffer[pos++] = value[j];
         buffer[pos++] = 0;
      }
      else newtag->Attrib[i].Value = NULL;

      newtag->AttribSize = pos;

      FreeResource(tag);

      Self->Tags[tagindex] = newtag;
      Self->Modified++;
      return ERR_Okay;
   }

   if (attribindex >= 0) {
      // This subroutine deals with situations where the developer is setting an existing attribute.

      if (attribindex >= tag->TotalAttrib) return log.warning(ERR_OutOfRange);

      // Get the lengths of the name and value strings

      name  = Args->Name;
      value = Args->Value;
      if ((!name) or (!name[0])) {
         if (attribindex > 0) name = tag->Attrib[attribindex].Name;
         else name = NULL;
      }

      if (name) for (namelen=0; name[namelen]; namelen++);
      else namelen = 0;

      if (value) for (valuelen=0; value[valuelen]; valuelen++);
      else valuelen = 0;

      // Calculate the new size of our buffer

      XMLAttrib *attrib = tag->Attrib;
      LONG attribsize = tag->AttribSize;

      // Remove the existing name and value sizes for the indexed attribute from the total.

      if (attrib[attribindex].Name) {
         for (i=0; attrib[attribindex].Name[i]; i++);
         attribsize -= i + 1;
      }

      if (attrib[attribindex].Value) {
         for (i=0; attrib[attribindex].Value[i]; i++);
         attribsize -= i + 1;
      }

      // Add the length of the new name and value strings to the overall size.

      if (value) {
         if (name)  attribsize += namelen + 1;
         if (value[0]) attribsize += valuelen + 1;
      }
      else {
         // If the value is NULL then the tag is being removed
      }

      // Print a warning if an attempt is made to change a normal tag into a content tag - this is almost certainly an
      // error on the developer's part although we will allow it.

      if ((attribindex IS 0) and (!name) and (attrib->Name)) {
         if ((attrib->Name[0] IS 'x') and (!attrib->Name[1])); // We've probably been legally called from InsertContent() if the tag is <x/>
         else log.warning("Warning - You are changing a tag @ %d with name '%s' into a content tag.", tagindex, attrib->Name);
      }

      // If the new buffer size is less or equal to the existing buffer's size, we can simply overwrite the existing
      // attribute.  Otherwise, we need to build a new buffer from scratch.

      if (attribsize <= tag->AttribSize) {
         if (value) {
            if (attrib[attribindex].Name) buffer = attrib[attribindex].Name;
            else if (attrib[attribindex].Value) buffer = attrib[attribindex].Value;
            else return log.warning(ERR_ObjectCorrupt);

            if (name) {
               for (i=0; i < namelen; i++) *buffer++ = name[i];
               *buffer++ = 0;
            }
            else attrib[attribindex].Name = NULL;

            if (value[0]) {
               attrib[attribindex].Value = buffer;
               for (i=0; i < valuelen; i++) *buffer++ = value[i];
               *buffer++ = 0;
            }
            else attrib[attribindex].Value = NULL;
         }
         else {
            if (attribindex IS 0) { // Content is being eliminated
               attrib[0].Value = NULL;
            }
            else {
               // The attribute is being removed, so just shift the array along and reduce the total number of entries.

               if (attribindex < tag->TotalAttrib-1) {
                  CopyMemory(attrib + attribindex + 1,
                     attrib + attribindex,
                     sizeof(XMLAttrib) * (tag->TotalAttrib - attribindex));
               }
               tag->TotalAttrib--;
            }
         }

         Self->Modified++;
         return ERR_Okay;
      }
      else if (!AllocMemory(sizeof(XMLTag) + Self->PrivateDataSize + (sizeof(XMLAttrib) * tag->TotalAttrib) + attribsize,
         MEM_UNTRACKED, &newtag, NULL)) {

         // Copy the old tag details to the new tag

         CopyMemory(tag, newtag, sizeof(XMLTag) + Self->PrivateDataSize);

         // Clean up the new tag and neighbouring tags

         newtag->Private = (BYTE *)newtag + sizeof(XMLTag);
         newtag->Attrib  = (XMLAttrib *)((BYTE *)newtag + sizeof(XMLTag) + Self->PrivateDataSize);
         newtag->AttribSize = attribsize;
         if (newtag->Prev) newtag->Prev->Next = newtag;
         if (newtag->Next) newtag->Next->Prev = newtag;
         if ((newtag->Index > 0) and (Self->Tags[newtag->Index-1]->Child IS tag)) {
            Self->Tags[newtag->Index-1]->Child = newtag;
         }

         // Recreate attribute names and values

         buffer = (char *)newtag->Attrib + (sizeof(XMLAttrib) * newtag->TotalAttrib);
         pos = 0;
         n = 0;
         for (i=0; i < tag->TotalAttrib; i++) {
            if (i IS attribindex) {
               if (value) { // Use the new name/value strings
                  if (name) {
                     newtag->Attrib[n].Name = buffer+pos;
                     for (j=0; j < namelen; j++) buffer[pos++] = name[j];
                     buffer[pos++] = 0;
                  }

                  if (value[0]) {
                     newtag->Attrib[n].Value = buffer+pos;
                     for (j=0; j < valuelen; j++) buffer[pos++] = value[j];
                     buffer[pos++] = 0;
                  }
                  n++;
               }
               else; // Attribute is being removed
            }
            else { // Use the old name/value strings
               if (attrib[i].Name) {
                  newtag->Attrib[n].Name = buffer+pos;
                  for (j=0; attrib[i].Name[j]; j++) buffer[pos++] = attrib[i].Name[j];
                  buffer[pos++] = 0;
               }

               if (attrib[i].Value) {
                  newtag->Attrib[n].Value = buffer+pos;
                  for (j=0; attrib[i].Value[j]; j++) buffer[pos++] = attrib[i].Value[j];
                  buffer[pos++] = 0;
               }
               n++;
            }
         }

         newtag->TotalAttrib = n;

         #ifdef DEBUG
            // Clearing the tag may pickup on re-use errors after the memory block is destroyed
            for (i=sizeof(XMLTag) + Self->PrivateDataSize + (sizeof(XMLAttrib) * tag->TotalAttrib) + tag->AttribSize-1; i >= 0; i--) {
               ((UBYTE *)tag)[i] = 0xee;
            }
         #endif

         FreeResource(tag);
         Self->Tags[tagindex] = newtag;
         Self->Modified++;
         return ERR_Okay;
      }
      else return log.warning(ERR_AllocMemory);
   }
   else return log.warning(ERR_Args);
}

/*****************************************************************************

-METHOD-
SetRoot: Defines a root-level tag for all XPath queries.

To optimise XPath processing in a particular area of the XML tree, a root tag can be defined that will limit all
lookups to that tag and its children.  The root tag will remain in effect until either this method is called with a
NULL XPath or the XML tree is modified.

The following example illustrates setting the root to the 84th action tag and then extracting the content from its name
tag.

<pre>
xml.SetRoot("/action(84)")
name = xml.get("content:/action/name")
</pre>

Changing the root index affects variable field queries only.  Siblings of the targeted root tag will still be
accessible for lookup following a call to this method whilst prior and parent tags are not.

As an alternative, the root tag can be set by writing the #RootIndex field with a valid tag index.

-INPUT-
cstr XPath: The new root-level tag, expressed as an XPath string.  Set to NULL to reset the root index.

-ERRORS-
Okay
Search: Unable to find a tag matching the provided XPath.
OutOfRange: The Index parameter is invalid.

*****************************************************************************/

static ERROR XML_SetRoot(extXML *Self, struct xmlSetRoot *Args)
{
   parasol::Log log;

   if (!Args) {
      Self->RootIndex = 0;
      return ERR_Okay;
   }

   if ((Args->XPath) and (Args->XPath[0])) {
      XMLTag *tag;
      if ((tag = find_tag(Self, Self->Tags[0], Args->XPath, NULL, NULL))) {
         Self->RootIndex = tag->Index;
      }
      else {
         log.warning("Failed to find %s", Args->XPath);
         return ERR_Search;
      }
   }
   else Self->RootIndex = 0;

   return ERR_Okay;
}

/*****************************************************************************

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

*****************************************************************************/

static ERROR XML_SetVar(extXML *Self, struct acSetVar *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Field)) return log.warning(ERR_NullArgs);
   if (Self->ReadOnly) return log.warning(ERR_ReadOnly);

   XMLTag *tag;
   CSTRING attrib;
   if ((tag = find_tag(Self, Self->Tags[Self->RootIndex], Args->Field, &attrib, NULL))) {
      if (attrib) { // Updating or adding an attribute
         LONG i;
         for (i=0; i < tag->TotalAttrib; i++) {
            if (!StrMatch(attrib, tag->Attrib[i].Name)) break;
         }

         if (i < tag->TotalAttrib) {
            xmlSetAttrib(Self, tag->Index, i, NULL, Args->Value); // Update the attribute.
         }
         else xmlSetAttrib(Self, tag->Index, -1, attrib, Args->Value); // Add the attribute.
      }
      else if ((tag->Child) and (!tag->Child->Attrib[0].Name)) { // Update content
         xmlSetAttrib(Self, tag->Child->Index, 0, NULL, Args->Value);
      }
      else { // Add content
         xmlInsertContent(Self, tag->Index, XMI_CHILD, Args->Value, NULL);
      }

      return ERR_Okay;
   }
   else {
      log.msg("Failed to find '%s'", Args->Field);
      return ERR_Search;
   }
}

/*****************************************************************************

-METHOD-
Sort: Sorts XML tags to your specifications.

The Sort method is used to sort a block of XML tags to your specifications.  You are required to pass an XPath that
refers to the tag containing each item that you want to sort.  To sort the root level, use an XPath of NULL.  A Sort
parameter is required that will specify the sorting instructions.  The format for the Sort string is `Tag:Attrib,Tag:Attrib,...`.

The Tag indicates the tag name that should be identified for sorting each node (naming child tags is acceptable).  Set
the Tag name to "Default" if sorting values should be retrieved from every tag at the requested XPath level.

The Attrib field defines what attribute contains the sort data in each XML tag.  To sort on content, do not define an
Attrib value (use the format `Tag:,`).

-INPUT-
cstr XPath: Sort everything under the specified tag, or NULL to sort the entire top level.
cstr Sort: Pointer to a sorting instruction string.
int(XSF) Flags: Optional flags.

-ERRORS-
Okay: The XML object was successfully sorted.
NullArgs
ReadOnly
AllocMemory:
NothingDone: The XML array was already sorted to your specifications.   Dependent on XSF_REPORT_SORTING.
-END-

*****************************************************************************/

static ERROR XML_SortXML(extXML *Self, struct xmlSort *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Sort)) return log.warning(ERR_NullArgs);
   if (Self->ReadOnly) return log.warning(ERR_ReadOnly);

   XMLTag *tag, *tmp;
   if ((!Args->XPath) or (!Args->XPath[0])) {
      tag = Self->Tags[0];
      if (!tag) return ERR_Okay;
   }
   else {
      CSTRING attrib;
      tag = find_tag(Self, Self->Tags[0], Args->XPath, &attrib, NULL);
      if (!tag) return log.warning(ERR_Search);
   }

   LONG insert_index = tag->Index;

   // Count the number of root-tags to be sorted (does not include child tags)

   LONG root_total, i, j;
   for (tmp=tag, root_total=0; tmp; tmp=tmp->Next, root_total++);

   if (root_total < 2) return ERR_Okay;

   LONG sort_total = 0;
   tag_count(Self->Tags[insert_index], &sort_total);

   log.trace("Index: %d, Tag: %s, Root-Total: %d, Sort-Total: %d of %d",
      insert_index, Args->Sort, root_total, sort_total, Self->TagCount);

   // Allocate an array to store the sort results

   ListSort *list;
   if (AllocMemory(sizeof(ListSort) * root_total, MEM_NO_CLEAR, &list, NULL) != ERR_Okay) {
      return ERR_AllocMemory;
   }

   // Tag address list

   ListSort **lookup;
   if (AllocMemory(sizeof(APTR) * root_total, MEM_NO_CLEAR, &lookup, NULL) != ERR_Okay) {
      FreeResource(list);
      return ERR_AllocMemory;
   }

   for (i=0; i < root_total; i++) lookup[i] = list + i;

   // Copy the matching tags into the sorting array (we just need to extract the data string for each tag and a pointer
   // to the tag represented by the data).

   for (LONG index=0; (tag) and (index < root_total); index++) {
      LONG pos = 0;
      list[index].String[0] = 0;
      list[index].Tag = 0;
      for (i=0; (Args->Sort[i]) and ((size_t)pos < sizeof(list[0].String));) {
         char tagname[80], attrib[80];

         // Format is CSV: Tag:Attrib,...

         UBYTE found = FALSE;

         for (j=0; (Args->Sort[i]) and (Args->Sort[i] != ':') and ((size_t)j < sizeof(tagname)-1); j++) tagname[j] = Args->Sort[i++];
         tagname[j] = 0;
         while ((Args->Sort[i]) and (Args->Sort[i] != ':') and (Args->Sort[i] != '/') and (Args->Sort[i] != ',')) i++;
         if (Args->Sort[i] IS ':') i++;

         for (j=0; (Args->Sort[i]) and (Args->Sort[i] != '/') and (Args->Sort[i] != ',') and ((size_t)j < sizeof(attrib)-1); j++) attrib[j] = Args->Sort[i++];
         attrib[j] = 0;
         while ((Args->Sort[i]) and (Args->Sort[i] != '/') and (Args->Sort[i] != ',')) i++;
         if (Args->Sort[i]) i++; // Skip separator

         if (!StrMatch("Default", tagname)) {
            tmp = tag;
            found = TRUE;
         }
         else {
            // Check for matching tag name, either at the current tag or in one of the child tags underneath it.

            if (tag->Next) j = tag->Next->Index;
            else j = Self->TagCount;

            for (tmp=tag; (tmp) and (tmp->Index < j); tmp=Self->Tags[tmp->Index+1]) {
               if (!StrMatch(tagname, tmp->Attrib->Name)) {
                  found = TRUE;
                  break;
               }
            }
         }

         if (found) {
            if (Args->Flags & XSF_CHECK_SORT) { // Scan for a 'sort' attribute in the XML tag
               for (LONG j=0; j < tmp->TotalAttrib; j++) {
                  if (!StrMatch("sort", tmp->Attrib[j].Name)) {
                     pos += StrCopy(tmp->Attrib[j].Value, list[index].String+pos, sizeof(list[index].String)-pos);
                     found = FALSE; // Turning off this flag will skip normal attribute extraction
                     break;
                  }
               }
            }

            if (found) {
               if (!attrib[0]) { // Use content as the sort data
                  XMLTag *child;
                  for (child=tmp->Child; (child) and ((size_t)pos < sizeof(list[index].String)); child=child->Next) {
                     if (!child->Attrib->Name) {
                        pos += StrCopy(child->Attrib->Value, list[index].String+pos, sizeof(list[index].String)-pos);
                     }
                  }
               }
               else { // Extract the sort data from the specified tag attribute
                  for (LONG j=0; j < tmp->TotalAttrib; j++) {
                     if (!StrMatch(tmp->Attrib[j].Name, attrib)) {
                        pos += StrCopy(tmp->Attrib[j].Value, list[index].String+pos, sizeof(list[index].String)-pos);
                        break;
                     }
                  }
               }
            }
         }

         // Each string in the sort list is separated with a byte value of 0x01

         if ((size_t)pos < sizeof(list[0].String)-1) list[index].String[pos++] = 0x01;
      }

      if ((size_t)pos >= sizeof(list[0].String)) pos = sizeof(list[0].String) - 1;
      list[index].String[pos] = 0;
      list[index].Tag = tag;

      tag = tag->Next;
   }

   BYTE rearranged;

#ifdef SHELL_SORT
   // Shell sort.  Similar to bubble sort but much faster because it can copy records over larger distances.

   BYTE test;

   LONG h = 1;
   while (h < root_total / 9) h = 3 * h + 1;

   rearranged = FALSE;
   if (Args->List[0].Flags & XSF_DESC) {
      for (; h > 0; h /= 3) {
         for (i=h; i < root_total; i++) {
            auto temp = lookup[i];
            for (j=i; (j >= h) and (StrSortCompare(lookup[j - h]->String, temp->String) < 0); j -= h) {
               lookup[j] = lookup[j - h];
               rearranged = TRUE;
            }
            lookup[j] = temp;
         }
      }
   }
   else {
      for (; h > 0; h /= 3) {
         for (i=h; i < root_total; i++) {
            auto temp = lookup[i];
            for (j=i; (j >= h) and (StrSortCompare(lookup[j - h]->String, temp->String) > 0); j -= h) {
               lookup[j] = lookup[j - h];
               rearranged = TRUE;
            }
            lookup[j] = temp;
         }
      }
   }

#else
   // Binary heap sort.  An extremely fast sorting algorithm, but assumes that the list has been rearranged on completion.

   if (Args->Flags & XSF_DESC) {
      for (i=root_total>>1; i >= 0; i--) sift_down(lookup, i, root_total);

      LONG heapsize = root_total;
      for (i=heapsize; i > 0; i--) {
         auto temp = lookup[0];
         lookup[0] = lookup[i-1];
         lookup[i-1] = temp;
         sift_down(lookup, 0, --heapsize);
      }
   }
   else {
      for (i=root_total>>1; i >= 0; i--) sift_up(lookup, i, root_total);

      LONG heapsize = root_total;
      for (i=heapsize; i > 0; i--) {
         auto temp = lookup[0];
         lookup[0] = lookup[i-1];
         lookup[i-1] = temp;
         sift_up(lookup, 0, --heapsize);
      }
   }

   rearranged = TRUE;

#endif

   // Return if no sorting was required

   if (rearranged IS FALSE) {
      FreeResource(list);
      FreeResource(lookup);
      if (Args->Flags & XSF_REPORT_SORTING) return ERR_NothingDone;
      else return ERR_Okay;
   }

   // Clone the original tag array which will act as the target

   XMLTag **clone_array;
   if (AllocMemory(sizeof(APTR) * (Self->TagCount + 1), MEM_UNTRACKED|MEM_NO_CLEAR, &clone_array, NULL) != ERR_Okay) {
      FreeResource(list);
      FreeResource(lookup);
      return log.warning(ERR_Memory);
   }

   CopyMemory(Self->Tags, clone_array, sizeof(APTR) * (Self->TagCount + 1));

   LONG index = insert_index;
   for (LONG i=0; i < root_total; i++) {
      // Determine the total number of tags in the block to be copied
      LONG tagcount = 1;
      if (lookup[i]->Tag->Child) tag_count(lookup[i]->Tag->Child, &tagcount);

      CopyMemory(Self->Tags + lookup[i]->Tag->Index, clone_array + index, sizeof(APTR) * tagcount);

      if (i < root_total - 1) clone_array[index]->Next = lookup[i+1]->Tag;
      else clone_array[index]->Next = NULL; // The last tag must be null-terminated

      index += tagcount;
   }

   FreeResource(Self->Tags);
   Self->Tags = clone_array;

   // Reset index numbers within the sorted range

   for (LONG i=insert_index; i < insert_index + sort_total; i++) {
      Self->Tags[i]->Index = i;
   }

   // Reset prev pointers for top-most tags in the sorted range

   Self->Tags[0]->Prev = NULL;
   for (tag=Self->Tags[insert_index]; tag; tag=tag->Next) {
      if (tag->Next) tag->Next->Prev = tag;
   }

   Self->Modified++;

   FreeResource(list);
   FreeResource(lookup);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
CurrentTag: Determines the index of the main tag to use when building XML strings.

When using any XML function that creates an XML string (e.g. SaveToObject), the XML object will include the entire XML
tree by default.  If you would like to change this behaviour so that only a certain section of the tree is included in
the build, set the CurrentTag to the index of the tag that you would like to start building from.  You can learn the
index number of an XMLTag by reading its Index field.  An index of zero will cover the entire XML hierarchy.

The CurrentTag field currently affects the #SaveToObject() action and the #Statement field.

-FIELD-
Flags: Optional flags.

-FIELD-
Path: Set this field if the XML document originates from a file source.

XML documents can be loaded from the file system by specifying a file path in this field.  If set post-initialisation,
all currently loaded data will be cleared and the file will be parsed automatically.

The XML class supports ~Core.LoadFile(), so an XML file can be pre-cached by the program if it is frequently used
during a program's life cycle.

****************************************************************************/

static ERROR GET_Path(extXML *Self, STRING *Value)
{
   if (Self->Path) { *Value = Self->Path; return ERR_Okay; }
   else return ERR_NoData;
}

static ERROR SET_Path(extXML *Self, CSTRING Value)
{
   if (Self->Source) SET_Source(Self, NULL);
   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }
   if (Self->Statement) { FreeResource(Self->Statement); Self->Statement = NULL; }

   if (!StrCompare("string:", Value, 7, 0)) {
      // If the string: path type is used then we can optimise things by setting the following path string as the
      // statement.

      return SET_Statement(Self, Value+7);
   }
   else if ((Value) and (*Value)) {
      if ((Self->Path = StrClone(Value))) {
         if (Self->initialised()) {
            parse_source(Self);
            Self->Modified++;
            return Self->ParseError;
         }
      }
      else return ERR_AllocMemory;
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Modified: A timestamp of when the XML data was last modified.

The Modified field provides an artificial timestamp value of when the XML data was last modified (e.g. by a tag insert
or update).  This allows you to read the current Modified value and then later make a comparison to the current value
to see if any changes have been made to the XML data.

-FIELD-
PrivateDataSize: Allocates a private data buffer for the owner's use against each XML tag.

Buffer space can be allocated against each XML tag by defining a value in this field.  If set, each XMLTag structure
referenced in the #Tags field will be allocated extra bytes that will be referenced in the Private field.  The
owner of the XML object is free to use this extra space as it wishes.

*****************************************************************************/

static ERROR SET_PrivateDataSize(extXML *Self, LONG Value)
{
   if (Value >= 0) {
      Self->PrivateDataSize = Value;
      return ERR_Okay;
   }
   else return ERR_OutOfRange;
}

/*****************************************************************************

-FIELD-
ReadOnly: Prevents modifications and enables caching for a loaded XML data source.

This field can be set to TRUE prior to initialisation of an XML object that will use an existing data source.  It
prevents modifications to the XML object.  If the data originates from a file path, the data may be cached to optimise
parsing where the same data is used across multiple XML objects.

*****************************************************************************/

static ERROR GET_ReadOnly(extXML *Self, LONG *Value)
{
   *Value = Self->ReadOnly;
   return ERR_Okay;
}

static ERROR SET_ReadOnly(extXML *Self, LONG Value)
{
   Self->ReadOnly = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
RootIndex: Defines the root tag for queries into the XML tree.

XML tree queries are offset by the value of the RootIndex field.  By default the root index is set to zero so that all
queries start from the top of the tree.  The index cannot exceed the value in the #TagCount field.

*****************************************************************************/

static ERROR SET_RootIndex(extXML *Self, LONG Value)
{
   if ((Value >= 0) and (Value < Self->TagCount)) {
      Self->RootIndex = Value;
      return ERR_Okay;
   }
   else {
      parasol::Log log;
      return log.warning(ERR_OutOfRange);
   }
}

/*****************************************************************************

-FIELD-
Source: Set this field if the XML document is to be sourced from another object.

An XML document can be loaded from another object by referencing it here, on the condition that the object's class
supports the Read action.

If set post-initialisation, all currently loaded data will be cleared and the source object will be parsed
automatically.

*****************************************************************************/

static ERROR SET_Source(extXML *Self, OBJECTPTR Value)
{
   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }
   if (Self->Statement) { FreeResource(Self->Statement); Self->Statement = NULL; }

   if (Value) {
      Self->Source = Value;
      if (Self->initialised()) {
         parse_source(Self);
         Self->Modified++;
         return Self->ParseError;
      }
   }
   else Self->Source = NULL;

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Statement: XML data is processed through this field.

To parse a string through an XML object, set the Statement field with a pointer to the XML formatted data.  If this
field is set after initialisation then the XML object will respond by clearing out any existing data and processing the
new information that has been passed to it.

Be warned that setting this field with an invalid statement will result in an empty XML object.

If the Statement field is read, a string-based version of the XML object's data is returned.  By default all tags will
be included in the statement unless a predefined starting position is set by the #CurrentTag field.  The string result
is an allocation that must be freed.
-END-

*****************************************************************************/

static ERROR GET_Statement(extXML *Self, STRING *Value)
{
   if (!Self->initialised()) {
      if (Self->Statement) {
         *Value = StrClone(Self->Statement);
         return ERR_Okay;
      }
      else return ERR_FieldNotSet;
   }

   if (Self->TagCount <= 0) return ERR_FieldNotSet;

   // Calculate the size of the buffer required to save the XML information

   LONG size = 0;
   for (auto tag=Self->Tags[Self->CurrentTag]; tag; tag=tag->Next) {
      len_xml_str(tag, Self->Flags, &size);
   }

   // Allocate a buffer large enough to hold the XML information

   STRING buffer;
   if (!AllocMemory(size+1, MEM_STRING|MEM_NO_CLEAR, &buffer, NULL)) {
      LONG offset = 0;
      for (auto tag=Self->Tags[Self->CurrentTag]; tag; tag=tag->Next) {
         build_xml_string(tag, buffer, Self->Flags, &offset);
      }

      *Value = buffer;
      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

static ERROR SET_Statement(extXML *Self, CSTRING Value)
{
   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }
   if (Self->Statement) { FreeResource(Self->Statement); Self->Statement = NULL; }

   if ((Value) and (*Value)) {
      if (Self->initialised()) {
         Self->ParseError = txt_to_xml(Self, Value);
         Self->Modified++;
         return Self->ParseError;
      }
      else if ((Self->Statement = StrClone(Value))) return ERR_Okay;
      else return ERR_AllocMemory;
   }
   else {
      if (Self->initialised()) {
         auto temp = Self->ReadOnly;
         Self->ReadOnly = FALSE;
         acClear(Self);
         Self->ReadOnly = temp;
      }
      return ERR_Okay;
   }
}

/*****************************************************************************

-FIELD-
TagCount: Reflects the total number of tags in the XML Tags array.

-FIELD-
Tags: Points to an array of tags loaded into an XML object.

After initialising data to an XML object, you can read the processed information through the Tags field.  The Tags
field is a null terminated array of all the XML tags that were retrieved from the original XML data. The XML tags are
represented in an XMLTag structure.

Each XMLTag will also have at least one attribute set in the Attrib array (the first attribute reflects the tag name)
and the exact number of attributes is indicated by the TotalAttrib field.

There are two methods available for reading the XML tags.  The first involves looping through the array, reading each
XMLTag reference until a NULL entry is reached (this gives a 'flat' view of the data).  The second is to read the first
tag in the array and use the Next, Prev and Child fields to recurse through the XML tags.  Tags that have a Name setting
of NULL in the first attribute are to be treated as embedded content, the data of which is defined in the Value string.

The Tags field is still defined in the event that it is empty (the array will consist of a null terminated entry).

*****************************************************************************/

static ERROR GET_Tags(extXML *Self, XMLTag ***Values, LONG *Elements)
{
   *Values = Self->Tags;
   *Elements = Self->TagCount;
   return ERR_Okay;
}

#include "xml_def.c"

static const FieldArray clFields[] = {
   { "Path",            FDF_STRING|FDF_RW,    0, NULL, (APTR)SET_Path },
   { "Tags",            FDF_ARRAY|FDF_POINTER|FDF_STRUCT|FDF_R, (MAXINT)"XMLTag", (APTR)GET_Tags, NULL },
   { "Source",          FDF_OBJECT|FDF_RI,    0, NULL, NULL },
   { "TagCount",        FDF_LONG|FDF_R,       0, NULL, NULL },
   { "Flags",           FDF_LONGFLAGS|FDF_RW, (MAXINT)&clXMLFlags, NULL, NULL },
   { "CurrentTag",      FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "PrivateDataSize", FDF_LONG|FDF_RI,      0, NULL, (APTR)SET_PrivateDataSize },
   { "RootIndex",       FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_RootIndex },
   { "Modified",        FDF_LONG|FDF_R,       0, NULL, NULL },
   { "ParseError",      FDF_LONG|FD_PRIVATE|FDF_R, 0, NULL, NULL },
   { "LineNo",          FDF_LONG|FD_PRIVATE|FDF_R, 0, NULL, NULL },
   // Virtual fields
   { "Location",        FDF_SYNONYM|FDF_STRING|FDF_RW, 0, (APTR)GET_Path, (APTR)SET_Path },
   { "ReadOnly",        FDF_LONG|FDF_RI,               0, (APTR)GET_ReadOnly, (APTR)SET_ReadOnly },
   { "Src",             FDF_STRING|FDF_SYNONYM|FDF_RW, 0, (APTR)GET_Path, (APTR)SET_Path },
   { "Statement",       FDF_STRING|FDF_ALLOC|FDF_RW,   0, (APTR)GET_Statement, (APTR)SET_Statement },
   END_FIELD
};

//****************************************************************************

#include "xml_functions.cpp"
#include "unescape.cpp"

//****************************************************************************

static ERROR add_xml_class(void)
{
   return CreateObject(ID_METACLASS, 0, &clXML,
      FID_BaseClassID|TLONG,    ID_XML,
      FID_ClassVersion|TFLOAT,  VER_XML,
      FID_Name|TSTR,            "XML",
      FID_FileExtension|TSTR,   "*.xml",
      FID_FileDescription|TSTR, "XML File",
      FID_Category|TLONG,       CCF_DATA,
      FID_Flags|TLONG,          CLF_PROMOTE_INTEGRAL,
      FID_Actions|TPTR,         clXMLActions,
      FID_Methods|TARRAY,       clXMLMethods,
      FID_Fields|TARRAY,        clFields,
      FID_Size|TLONG,           sizeof(extXML),
      FID_Path|TSTR,            MOD_PATH,
      TAGEND);
}

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, NULL, CMDExpunge, MODVERSION_XML)
