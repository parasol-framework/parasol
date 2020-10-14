#ifndef MODULES_XML
#define MODULES_XML 1

// Name:      xml.h
// Copyright: Paul Manias © 2001-2020
// Generator: idl-c

#ifndef MAIN_H
#include <parasol/main.h>
#endif

#define MODVERSION_XML (1)

// For SetAttrib()

#define XMS_UPDATE_ONLY -2
#define XMS_NEW -1
#define XMS_UPDATE -3

// Options for the Sort method.

#define XSF_DESC 0x00000001
#define XSF_REPORT_SORTING 0x00000002
#define XSF_CHECK_SORT 0x00000004

// Standard flags for the XML class.

#define XMF_WELL_FORMED 0x00000001
#define XMF_INCLUDE_COMMENTS 0x00000002
#define XMF_STRIP_CONTENT 0x00000004
#define XMF_LOWER_CASE 0x00000008
#define XMF_UPPER_CASE 0x00000010
#define XMF_READABLE 0x00000020
#define XMF_INDENT 0x00000020
#define XMF_LOCK_REMOVE 0x00000040
#define XMF_STRIP_HEADERS 0x00000080
#define XMF_NEW 0x00000100
#define XMF_NO_ESCAPE 0x00000200
#define XMF_ALL_CONTENT 0x00000400
#define XMF_PARSE_HTML 0x00000800
#define XMF_STRIP_CDATA 0x00001000
#define XMF_DEBUG 0x00002000
#define XMF_PARSE_ENTITY 0x00004000
#define XMF_INCLUDE_SIBLINGS 0x80000000

// Tag insertion options.

#define XMI_PREV 0
#define XMI_PREVIOUS 0
#define XMI_CHILD 1
#define XMI_NEXT 2
#define XMI_CHILD_END 3
#define XMI_END 4

typedef struct XMLAttrib {
   STRING Name;    // The name of the attribute.
   STRING Value;   // The value assigned to the attribute.
} XMLATT;

typedef struct XMLTag {
   LONG  Index;                  // Position within the XML array
   LONG  ID;                     // Unique ID assigned to the tag on creation
   struct XMLTag * Child;        // Reference to further child tags
   struct XMLTag * Prev;         // Reference to the previous tag at this level in the chain
   struct XMLTag * Next;         // Reference to the next tag at this level in the chain
   APTR  Private;                // Developer's private memory reference
   struct XMLAttrib * Attrib;    // Attributes of the tag, starting with the name
   WORD  TotalAttrib;            // Total number of listed attributes for this tag
   UWORD Branch;                 // The branch level for this XML node
   LONG  LineNo;                 // Line number on which this tag was encountered
  #ifdef PRV_XML
     LONG  AttribSize;       // The length of all attribute strings, compressed together
     UWORD CData:1;          // CDATA content section
     UWORD Instruction:1;    // Processing instruction, e.g. <?xml ?> or <?php ?>
     UWORD Notation:1;       // Unparsable notations such as <!DOCTYPE ... >
     WORD  pad01;
  #endif
    
} XMLTAG;

// XML class definition

#define VER_XML (1.000000)

typedef struct rkXML {
   OBJECT_HEADER
   STRING    Path;            // Location of the XML data file
   struct XMLTag * * Tags;    // Array of tag pointers, in linear order.  Useful for looking up indexes.  NULL-terminated.
   OBJECTPTR Source;          // Alternative data source to specifying a Path
   LONG      TagCount;        // Total number of tags loaded into the array
   LONG      Flags;           // Optional user flags
   LONG      CurrentTag;      // Current Tag - used for certain operations
   LONG      PrivateDataSize; // Extra data can be allocated per tag, according to the number of bytes specified here
   LONG      RootIndex;       // Root tag index
   LONG      Modified;        // Modification timestamp

#ifdef PRV_XML
   struct xml_cache *Cache; // Array of tag pointers, in linear order
   STRING Statement;
   ERROR  ParseError;
   LONG   Balance;          // Indicates that the tag structure is correctly balanced if zero
   UBYTE  ReadOnly:1;
   LONG   LineNo;
  
#endif
} objXML;

// XML methods

#define MT_XMLSetAttrib -1
#define MT_XMLGetString -2
#define MT_XMLInsertXML -3
#define MT_XMLGetContent -4
#define MT_XMLSort -5
#define MT_XMLRemoveTag -6
#define MT_XMLMoveTags -7
#define MT_XMLGetAttrib -8
#define MT_XMLInsertXPath -9
#define MT_XMLFindTag -10
#define MT_XMLFilter -11
#define MT_XMLSetRoot -12
#define MT_XMLCount -13
#define MT_XMLInsertContent -14
#define MT_XMLRemoveXPath -15
#define MT_XMLGetXPath -16
#define MT_XMLFindTagFromIndex -17
#define MT_XMLGetTag -18

struct xmlSetAttrib { LONG Index; LONG Attrib; CSTRING Name; CSTRING Value;  };
struct xmlGetString { LONG Index; LONG Flags; STRING Result;  };
struct xmlInsertXML { LONG Index; LONG Where; CSTRING XML; LONG Result;  };
struct xmlGetContent { LONG Index; STRING Buffer; LONG Length;  };
struct xmlSort { CSTRING XPath; CSTRING Sort; LONG Flags;  };
struct xmlRemoveTag { LONG Index; LONG Total;  };
struct xmlMoveTags { LONG Index; LONG Total; LONG DestIndex; LONG Where;  };
struct xmlGetAttrib { LONG Index; CSTRING Attrib; CSTRING Value;  };
struct xmlInsertXPath { CSTRING XPath; LONG Where; CSTRING XML; LONG Result;  };
struct xmlFindTag { CSTRING XPath; FUNCTION * Callback; LONG Result;  };
struct xmlFilter { CSTRING XPath;  };
struct xmlSetRoot { CSTRING XPath;  };
struct xmlCount { CSTRING XPath; LONG Result;  };
struct xmlInsertContent { LONG Index; LONG Where; CSTRING Content; LONG Result;  };
struct xmlRemoveXPath { CSTRING XPath; LONG Total;  };
struct xmlGetXPath { LONG Index; STRING Result;  };
struct xmlFindTagFromIndex { CSTRING XPath; LONG Start; FUNCTION * Callback; LONG Result;  };
struct xmlGetTag { LONG Index; struct XMLTag * Result;  };

INLINE ERROR xmlSetAttrib(APTR Ob, LONG Index, LONG Attrib, CSTRING Name, CSTRING Value) {
   struct xmlSetAttrib args = { Index, Attrib, Name, Value };
   return(Action(MT_XMLSetAttrib, Ob, &args));
}

INLINE ERROR xmlGetString(APTR Ob, LONG Index, LONG Flags, STRING * Result) {
   struct xmlGetString args = { Index, Flags, 0 };
   ERROR error = Action(MT_XMLGetString, Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

INLINE ERROR xmlInsertXML(APTR Ob, LONG Index, LONG Where, CSTRING XML, LONG * Result) {
   struct xmlInsertXML args = { Index, Where, XML, 0 };
   ERROR error = Action(MT_XMLInsertXML, Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

INLINE ERROR xmlGetContent(APTR Ob, LONG Index, STRING Buffer, LONG Length) {
   struct xmlGetContent args = { Index, Buffer, Length };
   return(Action(MT_XMLGetContent, Ob, &args));
}

INLINE ERROR xmlSort(APTR Ob, CSTRING XPath, CSTRING Sort, LONG Flags) {
   struct xmlSort args = { XPath, Sort, Flags };
   return(Action(MT_XMLSort, Ob, &args));
}

INLINE ERROR xmlRemoveTag(APTR Ob, LONG Index, LONG Total) {
   struct xmlRemoveTag args = { Index, Total };
   return(Action(MT_XMLRemoveTag, Ob, &args));
}

INLINE ERROR xmlMoveTags(APTR Ob, LONG Index, LONG Total, LONG DestIndex, LONG Where) {
   struct xmlMoveTags args = { Index, Total, DestIndex, Where };
   return(Action(MT_XMLMoveTags, Ob, &args));
}

INLINE ERROR xmlGetAttrib(APTR Ob, LONG Index, CSTRING Attrib, CSTRING * Value) {
   struct xmlGetAttrib args = { Index, Attrib, 0 };
   ERROR error = Action(MT_XMLGetAttrib, Ob, &args);
   if (Value) *Value = args.Value;
   return(error);
}

INLINE ERROR xmlInsertXPath(APTR Ob, CSTRING XPath, LONG Where, CSTRING XML, LONG * Result) {
   struct xmlInsertXPath args = { XPath, Where, XML, 0 };
   ERROR error = Action(MT_XMLInsertXPath, Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

INLINE ERROR xmlFindTag(APTR Ob, CSTRING XPath, FUNCTION * Callback, LONG * Result) {
   struct xmlFindTag args = { XPath, Callback, 0 };
   ERROR error = Action(MT_XMLFindTag, Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

INLINE ERROR xmlFilter(APTR Ob, CSTRING XPath) {
   struct xmlFilter args = { XPath };
   return(Action(MT_XMLFilter, Ob, &args));
}

INLINE ERROR xmlSetRoot(APTR Ob, CSTRING XPath) {
   struct xmlSetRoot args = { XPath };
   return(Action(MT_XMLSetRoot, Ob, &args));
}

INLINE ERROR xmlCount(APTR Ob, CSTRING XPath, LONG * Result) {
   struct xmlCount args = { XPath, 0 };
   ERROR error = Action(MT_XMLCount, Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

INLINE ERROR xmlInsertContent(APTR Ob, LONG Index, LONG Where, CSTRING Content, LONG * Result) {
   struct xmlInsertContent args = { Index, Where, Content, 0 };
   ERROR error = Action(MT_XMLInsertContent, Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

INLINE ERROR xmlRemoveXPath(APTR Ob, CSTRING XPath, LONG Total) {
   struct xmlRemoveXPath args = { XPath, Total };
   return(Action(MT_XMLRemoveXPath, Ob, &args));
}

INLINE ERROR xmlGetXPath(APTR Ob, LONG Index, STRING * Result) {
   struct xmlGetXPath args = { Index, 0 };
   ERROR error = Action(MT_XMLGetXPath, Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

INLINE ERROR xmlFindTagFromIndex(APTR Ob, CSTRING XPath, LONG Start, FUNCTION * Callback, LONG * Result) {
   struct xmlFindTagFromIndex args = { XPath, Start, Callback, 0 };
   ERROR error = Action(MT_XMLFindTagFromIndex, Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

INLINE ERROR xmlGetTag(APTR Ob, LONG Index, struct XMLTag ** Result) {
   struct xmlGetTag args = { Index, 0 };
   ERROR error = Action(MT_XMLGetTag, Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}


INLINE STRING XMLATTRIB(struct XMLTag *Tag, CSTRING Attrib) {
   LONG i;
   for (i=0; i < Tag->TotalAttrib; i++) {
      if (!StrMatch((CSSTRING)Attrib, (CSSTRING)Tag->Attrib[i].Name)) {
         if (!Tag->Attrib[i].Value) return (STRING)"1";
         else return Tag->Attrib[i].Value;
      }
   }
   return NULL;
}

INLINE BYTE XMLATTRIBCHECK(struct XMLTag *Tag, CSTRING Attrib) {
   LONG i;
   for (i=0; i < Tag->TotalAttrib; i++) {
      if (!StrMatch((CSSTRING)Attrib, (CSSTRING)Tag->Attrib[i].Name)) {
         return TRUE;
      }
   }
   return FALSE;
}

INLINE struct XMLTag * XMLFIND(struct XMLTag **List, CSTRING Name) {
   while (*List) {
      if (!StrMatch((CSSTRING)Name, (CSSTRING)List[0]->Attrib->Name)) return List[0];
      List++;
   }
   return 0;
}

INLINE ERROR xmlSetAttribDouble(objXML *XML, LONG Tag, LONG Flags, CSTRING Attrib, DOUBLE Value)
{
   char buffer[48];
   StrFormat(buffer, sizeof(buffer), "%g", Value);
   return xmlSetAttrib(XML, Tag, Flags, Attrib, buffer);
}

INLINE ERROR xmlSetAttribLong(objXML *XML, LONG Tag, LONG Flags, CSTRING Attrib, LONG Value)
{
   char buffer[20];
   StrFormat(buffer, sizeof(buffer), "%d", Value);
   return xmlSetAttrib(XML, Tag, Flags, Attrib, buffer);
}

  
#endif
