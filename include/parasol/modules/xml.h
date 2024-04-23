#pragma once

// Name:      xml.h
// Copyright: Paul Manias © 2001-2024
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_XML (1)

#ifdef __cplusplus
#include <functional>
#include <sstream>
#endif

class objXML;

// For SetAttrib()

#define XMS_NEW -1
#define XMS_UPDATE_ONLY -2
#define XMS_UPDATE -3

// Options for the Sort method.

enum class XSF : ULONG {
   NIL = 0,
   DESC = 0x00000001,
   CHECK_SORT = 0x00000002,
};

DEFINE_ENUM_FLAG_OPERATORS(XSF)

// Standard flags for the XML class.

enum class XMF : ULONG {
   NIL = 0,
   WELL_FORMED = 0x00000001,
   INCLUDE_COMMENTS = 0x00000002,
   STRIP_CONTENT = 0x00000004,
   INDENT = 0x00000008,
   READABLE = 0x00000008,
   LOCK_REMOVE = 0x00000010,
   STRIP_HEADERS = 0x00000020,
   NEW = 0x00000040,
   NO_ESCAPE = 0x00000080,
   ALL_CONTENT = 0x00000100,
   PARSE_HTML = 0x00000200,
   STRIP_CDATA = 0x00000400,
   LOG_ALL = 0x00000800,
   PARSE_ENTITY = 0x00001000,
   INCLUDE_SIBLINGS = 0x80000000,
};

DEFINE_ENUM_FLAG_OPERATORS(XMF)

// Tag insertion options.

enum class XMI : LONG {
   NIL = 0,
   PREV = 0,
   PREVIOUS = 0,
   CHILD = 1,
   NEXT = 2,
   CHILD_END = 3,
   END = 4,
};

// Standard flags for XMLTag.

enum class XTF : ULONG {
   NIL = 0,
   CDATA = 0x00000001,
   INSTRUCTION = 0x00000002,
   NOTATION = 0x00000004,
};

DEFINE_ENUM_FLAG_OPERATORS(XTF)

typedef struct XMLAttrib {
   std::string Name;    // Name of the attribute
   std::string Value;   // Value of the attribute
   inline bool isContent() const { return Name.empty(); }
   inline bool isTag() const { return !Name.empty(); }
   XMLAttrib(std::string pName, std::string pValue = "") : Name(pName), Value(pValue) { };
   XMLAttrib() = default;
} XMLATTRIB;

typedef struct XMLTag {
   LONG ID;                          // Unique ID assigned to the tag on creation
   LONG ParentID;                    // Unique ID of the parent tag
   LONG LineNo;                      // Line number on which this tag was encountered
   XTF  Flags;                       // Optional flags
   pf::vector<XMLAttrib> Attribs;    // Array of attributes for this tag
   pf::vector<XMLTag> Children;      // Array of child tags
   XMLTag(LONG pID, LONG pLine = 0) :
      ID(pID), ParentID(0), LineNo(pLine), Flags(XTF::NIL)
      { }

   XMLTag(LONG pID, LONG pLine, pf::vector<XMLAttrib> pAttribs) :
      ID(pID), ParentID(0), LineNo(pLine), Flags(XTF::NIL), Attribs(pAttribs)
      { }

   XMLTag() { XMLTag(0); }

   inline CSTRING name() const { return Attribs[0].Name.c_str(); }
   inline bool hasContent() const { return (!Children.empty()) and (Children[0].Attribs[0].Name.empty()); }
   inline bool isContent() const { return Attribs[0].Name.empty(); }
   inline bool isTag() const { return !Attribs[0].Name.empty(); }

   inline bool hasChildTags() const { 
      for (auto &scan : Children) {
         if (!scan.Attribs[0].Name.empty()) return true;
      }
      return false;
   }

   inline const std::string * attrib(const std::string &Name) const {
      for (unsigned a=1; a < Attribs.size(); a++) {
         if (StrMatch(Attribs[a].Name, Name) IS ERR::Okay) return &Attribs[a].Value;
      }
      return NULL;
   }

   inline std::string getContent() const {
      if (Children.empty()) return std::string("");

      std::ostringstream str;
      for (auto &scan : Children) {
         if (scan.Attribs.empty()) continue; // Sanity check
         if (scan.Attribs[0].isContent()) str << scan.Attribs[0].Value;
      }
      return str.str();
   }
} XMLTAG;

// XML class definition

#define VER_XML (1.000000)

// XML methods

#define MT_XMLSetAttrib -1
#define MT_XMLSerialise -2
#define MT_XMLInsertXML -3
#define MT_XMLGetContent -4
#define MT_XMLSort -5
#define MT_XMLRemoveTag -6
#define MT_XMLMoveTags -7
#define MT_XMLGetAttrib -8
#define MT_XMLInsertXPath -9
#define MT_XMLFindTag -10
#define MT_XMLFilter -11
#define MT_XMLCount -13
#define MT_XMLInsertContent -14
#define MT_XMLRemoveXPath -15
#define MT_XMLGetTag -18

struct xmlSetAttrib { LONG Index; LONG Attrib; CSTRING Name; CSTRING Value;  };
struct xmlSerialise { LONG Index; XMF Flags; STRING Result;  };
struct xmlInsertXML { LONG Index; XMI Where; CSTRING XML; LONG Result;  };
struct xmlGetContent { LONG Index; STRING Buffer; LONG Length;  };
struct xmlSort { CSTRING XPath; CSTRING Sort; XSF Flags;  };
struct xmlRemoveTag { LONG Index; LONG Total;  };
struct xmlMoveTags { LONG Index; LONG Total; LONG DestIndex; XMI Where;  };
struct xmlGetAttrib { LONG Index; CSTRING Attrib; CSTRING Value;  };
struct xmlInsertXPath { CSTRING XPath; XMI Where; CSTRING XML; LONG Result;  };
struct xmlFindTag { CSTRING XPath; FUNCTION * Callback; LONG Result;  };
struct xmlFilter { CSTRING XPath;  };
struct xmlCount { CSTRING XPath; LONG Result;  };
struct xmlInsertContent { LONG Index; XMI Where; CSTRING Content; LONG Result;  };
struct xmlRemoveXPath { CSTRING XPath; LONG Limit;  };
struct xmlGetTag { LONG Index; struct XMLTag * Result;  };

inline ERR xmlSetAttrib(APTR Ob, LONG Index, LONG Attrib, CSTRING Name, CSTRING Value) noexcept {
   struct xmlSetAttrib args = { Index, Attrib, Name, Value };
   return(Action(MT_XMLSetAttrib, (OBJECTPTR)Ob, &args));
}

inline ERR xmlSerialise(APTR Ob, LONG Index, XMF Flags, STRING * Result) noexcept {
   struct xmlSerialise args = { Index, Flags, (STRING)0 };
   ERR error = Action(MT_XMLSerialise, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

inline ERR xmlInsertXML(APTR Ob, LONG Index, XMI Where, CSTRING XML, LONG * Result) noexcept {
   struct xmlInsertXML args = { Index, Where, XML, (LONG)0 };
   ERR error = Action(MT_XMLInsertXML, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

inline ERR xmlGetContent(APTR Ob, LONG Index, STRING Buffer, LONG Length) noexcept {
   struct xmlGetContent args = { Index, Buffer, Length };
   return(Action(MT_XMLGetContent, (OBJECTPTR)Ob, &args));
}

inline ERR xmlSort(APTR Ob, CSTRING XPath, CSTRING Sort, XSF Flags) noexcept {
   struct xmlSort args = { XPath, Sort, Flags };
   return(Action(MT_XMLSort, (OBJECTPTR)Ob, &args));
}

inline ERR xmlRemoveTag(APTR Ob, LONG Index, LONG Total) noexcept {
   struct xmlRemoveTag args = { Index, Total };
   return(Action(MT_XMLRemoveTag, (OBJECTPTR)Ob, &args));
}

inline ERR xmlMoveTags(APTR Ob, LONG Index, LONG Total, LONG DestIndex, XMI Where) noexcept {
   struct xmlMoveTags args = { Index, Total, DestIndex, Where };
   return(Action(MT_XMLMoveTags, (OBJECTPTR)Ob, &args));
}

inline ERR xmlGetAttrib(APTR Ob, LONG Index, CSTRING Attrib, CSTRING * Value) noexcept {
   struct xmlGetAttrib args = { Index, Attrib, (CSTRING)0 };
   ERR error = Action(MT_XMLGetAttrib, (OBJECTPTR)Ob, &args);
   if (Value) *Value = args.Value;
   return(error);
}

inline ERR xmlInsertXPath(APTR Ob, CSTRING XPath, XMI Where, CSTRING XML, LONG * Result) noexcept {
   struct xmlInsertXPath args = { XPath, Where, XML, (LONG)0 };
   ERR error = Action(MT_XMLInsertXPath, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

inline ERR xmlFindTag(APTR Ob, CSTRING XPath, FUNCTION * Callback, LONG * Result) noexcept {
   struct xmlFindTag args = { XPath, Callback, (LONG)0 };
   ERR error = Action(MT_XMLFindTag, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

inline ERR xmlFilter(APTR Ob, CSTRING XPath) noexcept {
   struct xmlFilter args = { XPath };
   return(Action(MT_XMLFilter, (OBJECTPTR)Ob, &args));
}

inline ERR xmlCount(APTR Ob, CSTRING XPath, LONG * Result) noexcept {
   struct xmlCount args = { XPath, (LONG)0 };
   ERR error = Action(MT_XMLCount, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

inline ERR xmlInsertContent(APTR Ob, LONG Index, XMI Where, CSTRING Content, LONG * Result) noexcept {
   struct xmlInsertContent args = { Index, Where, Content, (LONG)0 };
   ERR error = Action(MT_XMLInsertContent, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

inline ERR xmlRemoveXPath(APTR Ob, CSTRING XPath, LONG Limit) noexcept {
   struct xmlRemoveXPath args = { XPath, Limit };
   return(Action(MT_XMLRemoveXPath, (OBJECTPTR)Ob, &args));
}

inline ERR xmlGetTag(APTR Ob, LONG Index, struct XMLTag ** Result) noexcept {
   struct xmlGetTag args = { Index, (struct XMLTag *)0 };
   ERR error = Action(MT_XMLGetTag, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}


class objXML : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_XML;
   static constexpr CSTRING CLASS_NAME = "XML";

   using create = pf::Create<objXML>;

   STRING    Path;      // Set this field if the XML document originates from a file source.
   OBJECTPTR Source;    // Set this field if the XML data is to be sourced from another object.
   XMF       Flags;     // Optional flags.
   LONG      Start;     // Set a starting cursor to affect the starting point for some XML operations.
   LONG      Modified;  // A timestamp of when the XML data was last modified.
   ERR       ParseError; // Private
   LONG      LineNo;    // Private
   public:
   typedef pf::vector<XMLTag> TAGS;
   typedef pf::vector<XMLTag>::iterator CURSOR;
   TAGS Tags;

   // Action stubs

   inline ERR clear() noexcept { return Action(AC_Clear, this, NULL); }
   inline ERR dataFeed(OBJECTPTR Object, DATA Datatype, const void *Buffer, LONG Size) noexcept {
      struct acDataFeed args = { Object, Datatype, Buffer, Size };
      return Action(AC_DataFeed, this, &args);
   }
   inline ERR getVar(CSTRING FieldName, STRING Buffer, LONG Size) noexcept {
      struct acGetVar args = { FieldName, Buffer, Size };
      auto error = Action(AC_GetVar, this, &args);
      if ((error != ERR::Okay) and (Buffer)) Buffer[0] = 0;
      return error;
   }
   inline ERR init() noexcept { return InitObject(this); }
   inline ERR reset() noexcept { return Action(AC_Reset, this, NULL); }
   inline ERR saveToObject(OBJECTPTR Dest, CLASSID ClassID = 0) noexcept {
      struct acSaveToObject args = { Dest, { ClassID } };
      return Action(AC_SaveToObject, this, &args);
   }
   inline ERR acSetVar(CSTRING FieldName, CSTRING Value) noexcept {
      struct acSetVar args = { FieldName, Value };
      return Action(AC_SetVar, this, &args);
   }

   // Customised field setting

   template <class T> inline ERR setPath(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERR setSource(OBJECTPTR Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Source = Value;
      return ERR::Okay;
   }

   inline ERR setFlags(const XMF Value) noexcept {
      this->Flags = Value;
      return ERR::Okay;
   }

   inline ERR setStart(const LONG Value) noexcept {
      this->Start = Value;
      return ERR::Okay;
   }

   inline ERR setReadOnly(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   template <class T> inline ERR setStatement(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, 0x08800320, to_cstring(Value), 1);
   }

};

//********************************************************************************************************************

template <class T> inline ERR xmlSetAttribValue(objXML *XML, LONG Tag, LONG Flags, T &&Attrib, LONG Value) {
   auto attrib = to_cstring(Attrib);
   auto buffer = std::to_string(Value);
   return xmlSetAttrib(XML, Tag, Flags, attrib, buffer.c_str());
}

template <class T> inline ERR xmlSetAttribValue(objXML *XML, LONG Tag, LONG Flags, T &&Attrib, DOUBLE Value) {
   auto attrib = to_cstring(Attrib);
   auto buffer = std::to_string(Value);
   return xmlSetAttrib(XML, Tag, Flags, attrib, buffer.c_str());
}

inline void xmlUpdateAttrib(XMLTag &Tag, const std::string Name, const std::string Value, bool CanCreate = false)
{
   for (auto a = Tag.Attribs.begin(); a != Tag.Attribs.end(); a++) {
      if (StrMatch(Name, a->Name) IS ERR::Okay) {
         a->Name  = Name;
         a->Value = Value;
         return;
      }
   }

   if (CanCreate) Tag.Attribs.emplace_back(Name, Value);
}

inline void xmlNewAttrib(XMLTag &Tag, const std::string Name, const std::string Value) {
   Tag.Attribs.emplace_back(Name, Value);
}

inline void xmlNewAttrib(XMLTag *Tag, const std::string Name, const std::string Value) {
   Tag->Attribs.emplace_back(Name, Value);
}

inline std::string xmlGetContent(const XMLTag &Tag) {
   std::string value;
   for (auto &scan : Tag.Children) {
      if (scan.Attribs.empty()) continue;
      if (scan.Attribs[0].isContent()) value.append(scan.Attribs[0].Value);
   }
   return value;
}

template <class T> inline ERR xmlInsertStatement(APTR Ob, LONG Index, XMI Where, T Statement, XMLTag **Result) {
   struct xmlInsertXML insert = { Index, Where, to_cstring(Statement) };

   if (auto error = Action(MT_XMLInsertXML, (OBJECTPTR)Ob, &insert); error IS ERR::Okay) {
      struct xmlGetTag get = { insert.Result };
      error = Action(MT_XMLGetTag, (OBJECTPTR)Ob, &get);
      *Result = get.Result;
      return error;
   }
   else return error;
}

//********************************************************************************************************************
// Call a Function for every attribute in the XML tree.  Allows you to modify attributes quite easily, e.g. to convert
// all attribute names to uppercase:
//
// std::transform(attrib.Name.begin(), attrib.Name.end(), attrib.Name.begin(),
//   [](UBYTE c){ return std::toupper(c); });

inline void xmlForEachAttrib(objXML::TAGS &Tags, std::function<void(XMLAttrib &)> &Function)
{
   for (auto &tag : Tags) {
      for (auto &attrib : tag.Attribs) {
         Function(attrib);
      }
      if (!tag.Children.empty()) xmlForEachAttrib(tag.Children, Function);
   }
}
