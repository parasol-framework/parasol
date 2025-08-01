#pragma once

// Name:      xml.h
// Copyright: Paul Manias © 2001-2025
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_XML (1)

#ifdef __cplusplus
#include <functional>
#include <sstream>
#ifndef STRINGS_HPP
#include <parasol/strings.hpp>
#endif

#endif

class objXML;

// For SetAttrib()

enum class XMS : int {
   NIL = 0,
   NEW = -1,
   UPDATE_ONLY = -2,
   UPDATE = -3,
};

// Options for the Sort method.

enum class XSF : uint32_t {
   NIL = 0,
   DESC = 0x00000001,
   CHECK_SORT = 0x00000002,
};

DEFINE_ENUM_FLAG_OPERATORS(XSF)

// Standard flags for the XML class.

enum class XMF : uint32_t {
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
   INCLUDE_WHITESPACE = 0x00000100,
   PARSE_HTML = 0x00000200,
   STRIP_CDATA = 0x00000400,
   LOG_ALL = 0x00000800,
   PARSE_ENTITY = 0x00001000,
   OMIT_TAGS = 0x00002000,
   INCLUDE_SIBLINGS = 0x80000000,
};

DEFINE_ENUM_FLAG_OPERATORS(XMF)

// Tag insertion options.

enum class XMI : int {
   NIL = 0,
   PREV = 0,
   PREVIOUS = 0,
   CHILD = 1,
   NEXT = 2,
   CHILD_END = 3,
   END = 4,
};

// Standard flags for XMLTag.

enum class XTF : uint32_t {
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
   int ID;                           // Unique ID assigned to the tag on creation
   int ParentID;                     // Unique ID of the parent tag
   int LineNo;                       // Line number on which this tag was encountered
   XTF Flags;                        // Optional flags
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
         if (pf::iequals(Attribs[a].Name, Name)) return &Attribs[a].Value;
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

namespace xml {
struct SetAttrib { int Index; XMS Attrib; CSTRING Name; CSTRING Value; static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Serialise { int Index; XMF Flags; STRING Result; static const AC id = AC(-2); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct InsertXML { int Index; XMI Where; CSTRING XML; int Result; static const AC id = AC(-3); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct GetContent { int Index; STRING Buffer; int Length; static const AC id = AC(-4); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Sort { CSTRING XPath; CSTRING Sort; XSF Flags; static const AC id = AC(-5); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct RemoveTag { int Index; int Total; static const AC id = AC(-6); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct MoveTags { int Index; int Total; int DestIndex; XMI Where; static const AC id = AC(-7); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct GetAttrib { int Index; CSTRING Attrib; CSTRING Value; static const AC id = AC(-8); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct InsertXPath { CSTRING XPath; XMI Where; CSTRING XML; int Result; static const AC id = AC(-9); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct FindTag { CSTRING XPath; FUNCTION * Callback; int Result; static const AC id = AC(-10); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Filter { CSTRING XPath; static const AC id = AC(-11); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Count { CSTRING XPath; int Result; static const AC id = AC(-13); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct InsertContent { int Index; XMI Where; CSTRING Content; int Result; static const AC id = AC(-14); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct RemoveXPath { CSTRING XPath; int Limit; static const AC id = AC(-15); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct GetTag { int Index; struct XMLTag * Result; static const AC id = AC(-18); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objXML : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::XML;
   static constexpr CSTRING CLASS_NAME = "XML";

   using create = pf::Create<objXML>;

   STRING    Path;      // Set this field if the XML document originates from a file source.
   OBJECTPTR Source;    // Set this field if the XML data is to be sourced from another object.
   XMF       Flags;     // Controls XML parsing behaviour and processing options.
   int       Start;     // Set a starting cursor to affect the starting point for some XML operations.
   int       Modified;  // A timestamp of when the XML data was last modified.
   ERR       ParseError; // Private
   int       LineNo;    // Private
   public:
   typedef pf::vector<XMLTag> TAGS;
   typedef pf::vector<XMLTag>::iterator CURSOR;
   TAGS Tags;

   template <class T> inline ERR insertStatement(LONG Index, XMI Where, T Statement, XMLTag **Result) {
      LONG index_result;
      XMLTag *tag_result;
      if (auto error = insertXML(Index, Where, to_cstring(Statement), &index_result); error IS ERR::Okay) {
         error = getTag(index_result, &tag_result);
         *Result = tag_result;
         return error;
      }
      else return error;
   }

   template <class T> inline ERR setAttribValue(LONG Tag, LONG Flags, T &&Attrib, LONG Value) {
      auto attrib = to_cstring(Attrib);
      auto buffer = std::to_string(Value);
      return setAttrib(Tag, Flags, attrib, buffer.c_str());
   }

   template <class T> inline ERR setAttribValue(LONG Tag, LONG Flags, T &&Attrib, DOUBLE Value) {
      auto attrib = to_cstring(Attrib);
      auto buffer = std::to_string(Value);
      return setAttrib(Tag, Flags, attrib, buffer.c_str());
   }

   // Action stubs

   inline ERR clear() noexcept { return Action(AC::Clear, this, nullptr); }
   inline ERR dataFeed(OBJECTPTR Object, DATA Datatype, const void *Buffer, int Size) noexcept {
      struct acDataFeed args = { Object, Datatype, Buffer, Size };
      return Action(AC::DataFeed, this, &args);
   }
   inline ERR getKey(CSTRING Key, STRING Value, int Size) noexcept {
      struct acGetKey args = { Key, Value, Size };
      auto error = Action(AC::GetKey, this, &args);
      if ((error != ERR::Okay) and (Value)) Value[0] = 0;
      return error;
   }
   inline ERR init() noexcept { return InitObject(this); }
   inline ERR reset() noexcept { return Action(AC::Reset, this, nullptr); }
   inline ERR saveToObject(OBJECTPTR Dest, CLASSID ClassID = CLASSID::NIL) noexcept {
      struct acSaveToObject args = { Dest, { ClassID } };
      return Action(AC::SaveToObject, this, &args);
   }
   inline ERR acSetKey(CSTRING FieldName, CSTRING Value) noexcept {
      struct acSetKey args = { FieldName, Value };
      return Action(AC::SetKey, this, &args);
   }
   inline ERR setAttrib(int Index, XMS Attrib, CSTRING Name, CSTRING Value) noexcept {
      struct xml::SetAttrib args = { Index, Attrib, Name, Value };
      return(Action(AC(-1), this, &args));
   }
   inline ERR serialise(int Index, XMF Flags, STRING * Result) noexcept {
      struct xml::Serialise args = { Index, Flags, (STRING)0 };
      ERR error = Action(AC(-2), this, &args);
      if (Result) *Result = args.Result;
      return(error);
   }
   inline ERR insertXML(int Index, XMI Where, CSTRING XML, int * Result) noexcept {
      struct xml::InsertXML args = { Index, Where, XML, (int)0 };
      ERR error = Action(AC(-3), this, &args);
      if (Result) *Result = args.Result;
      return(error);
   }
   inline ERR getContent(int Index, STRING Buffer, int Length) noexcept {
      struct xml::GetContent args = { Index, Buffer, Length };
      return(Action(AC(-4), this, &args));
   }
   inline ERR sort(CSTRING XPath, CSTRING Sort, XSF Flags) noexcept {
      struct xml::Sort args = { XPath, Sort, Flags };
      return(Action(AC(-5), this, &args));
   }
   inline ERR removeTag(int Index, int Total) noexcept {
      struct xml::RemoveTag args = { Index, Total };
      return(Action(AC(-6), this, &args));
   }
   inline ERR moveTags(int Index, int Total, int DestIndex, XMI Where) noexcept {
      struct xml::MoveTags args = { Index, Total, DestIndex, Where };
      return(Action(AC(-7), this, &args));
   }
   inline ERR getAttrib(int Index, CSTRING Attrib, CSTRING * Value) noexcept {
      struct xml::GetAttrib args = { Index, Attrib, (CSTRING)0 };
      ERR error = Action(AC(-8), this, &args);
      if (Value) *Value = args.Value;
      return(error);
   }
   inline ERR insertXPath(CSTRING XPath, XMI Where, CSTRING XML, int * Result) noexcept {
      struct xml::InsertXPath args = { XPath, Where, XML, (int)0 };
      ERR error = Action(AC(-9), this, &args);
      if (Result) *Result = args.Result;
      return(error);
   }
   inline ERR findTag(CSTRING XPath, FUNCTION Callback, int * Result) noexcept {
      struct xml::FindTag args = { XPath, &Callback, (int)0 };
      ERR error = Action(AC(-10), this, &args);
      if (Result) *Result = args.Result;
      return(error);
   }
   inline ERR filter(CSTRING XPath) noexcept {
      struct xml::Filter args = { XPath };
      return(Action(AC(-11), this, &args));
   }
   inline ERR count(CSTRING XPath, int * Result) noexcept {
      struct xml::Count args = { XPath, (int)0 };
      ERR error = Action(AC(-13), this, &args);
      if (Result) *Result = args.Result;
      return(error);
   }
   inline ERR insertContent(int Index, XMI Where, CSTRING Content, int * Result) noexcept {
      struct xml::InsertContent args = { Index, Where, Content, (int)0 };
      ERR error = Action(AC(-14), this, &args);
      if (Result) *Result = args.Result;
      return(error);
   }
   inline ERR removeXPath(CSTRING XPath, int Limit) noexcept {
      struct xml::RemoveXPath args = { XPath, Limit };
      return(Action(AC(-15), this, &args));
   }
   inline ERR getTag(int Index, struct XMLTag ** Result) noexcept {
      struct xml::GetTag args = { Index, (struct XMLTag *)0 };
      ERR error = Action(AC(-18), this, &args);
      if (Result) *Result = args.Result;
      return(error);
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

   inline ERR setStart(const int Value) noexcept {
      this->Start = Value;
      return ERR::Okay;
   }

   inline ERR setReadOnly(const int Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, FD_INT, &Value, 1);
   }

   template <class T> inline ERR setStatement(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, 0x08800320, to_cstring(Value), 1);
   }

};

//********************************************************************************************************************

namespace xml {

inline void UpdateAttrib(XMLTag &Tag, const std::string Name, const std::string Value, bool CanCreate = false)
{
   for (auto a = Tag.Attribs.begin(); a != Tag.Attribs.end(); a++) {
      if (pf::iequals(Name, a->Name)) {
         a->Name  = Name;
         a->Value = Value;
         return;
      }
   }

   if (CanCreate) Tag.Attribs.emplace_back(Name, Value);
}

inline void NewAttrib(XMLTag &Tag, const std::string Name, const std::string Value) {
   Tag.Attribs.emplace_back(Name, Value);
}

inline void NewAttrib(XMLTag *Tag, const std::string Name, const std::string Value) {
   Tag->Attribs.emplace_back(Name, Value);
}

inline std::string GetContent(const XMLTag &Tag) {
   std::string value;
   for (auto &scan : Tag.Children) {
      if (scan.Attribs.empty()) continue;
      if (scan.Attribs[0].isContent()) value.append(scan.Attribs[0].Value);
   }
   return value;
}

//********************************************************************************************************************
// Call a Function for every attribute in the XML tree.  Allows you to modify attributes quite easily, e.g. to convert
// all attribute names to uppercase:
//
// std::transform(attrib.Name.begin(), attrib.Name.end(), attrib.Name.begin(),
//   [](UBYTE c){ return std::toupper(c); });

inline void ForEachAttrib(objXML::TAGS &Tags, std::function<void(XMLAttrib &)> &Function)
{
   for (auto &tag : Tags) {
      for (auto &attrib : tag.Attribs) {
         Function(attrib);
      }
      if (!tag.Children.empty()) ForEachAttrib(tag.Children, Function);
   }
}

} // namespace
