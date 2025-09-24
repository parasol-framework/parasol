
#include <memory>

struct ParseState {
   CSTRING Pos;
   int   Balance;  // Indicates that the tag structure is correctly balanced if zero

   // Namespace context for this parsing scope
   std::map<std::string, uint32_t> PrefixMap;  // Prefix -> namespace URI hash
   uint32_t DefaultNamespace;                  // Default namespace URI hash

   ParseState() : Pos(nullptr), Balance(0), DefaultNamespace(0) { }

   // Copy constructor for inheriting namespace context from parent scope
   ParseState(const ParseState& parent) : Pos(parent.Pos), Balance(parent.Balance),
                                          PrefixMap(parent.PrefixMap), DefaultNamespace(parent.DefaultNamespace) { }
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

   // Variable storage for XPath variable references
   ankerl::unordered_dense::map<std::string, std::string> Variables;

   // Namespace registry using pf::strhash() values, this allows us to store URIs in compact form in XMLTag structures.
   ankerl::unordered_dense::map<uint32_t, std::string> NSRegistry; // hash(URI) -> URI

   // Link prefixes to namespace URIs
   // WARNING: If the XML document overwrites namespace URIs on the same prefix name (legal!)
   // then this lookup table returns the most recently assigned URI.
   std::map<std::string, uint32_t> Prefixes; // hash(Prefix) -> hash(URI)

   extXML() : ReadOnly(false), StaleMap(true) { }

   [[nodiscard]] ankerl::unordered_dense::map<int, XMLTag *> & getMap() {
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
      return find_tag(XPath, 0);
   }

   // Namespace utility methods

   inline uint32_t registerNamespace(const std::string &URI) {
      if (URI.empty()) return 0;
      auto hash = pf::strhash(URI);
      NSRegistry[hash] = URI;
      return hash;
   }

   inline std::string * getNamespaceURI(uint32_t Hash) {
      auto it = NSRegistry.find(Hash);
      return (it != NSRegistry.end()) ? &it->second : nullptr;
   }

   // XPath 1.0 Implementation for Parasol
   // 
   // Extra Features Supported:
   // [=...] Match on encapsulated content (Not an XPath standard but we support it)
   // The use of \ as an escape character in attribute strings is supported, but keep in mind that this is not an official
   // feature of the XPath standard.
   //
   // Examples:
   //   /menu/submenu
   //   /menu[2]/window
   //   /menu/window/@title
   //   /menu/window[@title='foo']/...
   //   /menu[=contentmatch]
   //   /menu//window
   //   /menu/window/* (First child of the window tag)
   //   /menu/*[@id='5']

   ERR find_tag(std::string_view XPath, uint32_t CurrentPrefix);

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

#include "xpath/xpath_ast.h"
#include "xpath/xpath_functions.h"
#include "xpath/xpath_axis.h"
#include "xpath/xpath_parser.h"
#include "xpath/xpath_evaluator.h"

inline ERR extXML::find_tag(std::string_view XPath, uint32_t CurrentPrefix)
{
   if ((!CursorTags) or (CursorTags->empty())) {
      return pf::Log(__FUNCTION__).warning(ERR::SanityCheckFailed);
   }

   SimpleXPathEvaluator evaluator(this);
   return evaluator.find_tag_enhanced(XPath, CurrentPrefix);
}
