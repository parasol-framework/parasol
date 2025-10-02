
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <string_view>
#include <unordered_map>
#include <deque>
#include <unordered_set>
#include <functional>
#include <map>
#include <optional>
#include <ankerl/unordered_dense.h>

#include "schema/schema_parser.h"

#include <concepts>
#include <ranges>
#include <algorithm>

// C++20 Concepts for type safety

template<typename F>
concept XMLCallback = requires(F f, extXML* xml, XMLTag& tag, const char* attrib) {
   { f(xml, tag, attrib) } -> std::same_as<ERR>;
};

template<typename F>
concept XMLTagPredicate = requires(F f, const XMLTag& tag) {
   { f(tag) } -> std::convertible_to<bool>;
};

template<typename Key>
concept MapKey = std::equality_comparable<Key> and std::default_initializable<Key>;

constexpr std::array<uint64_t, 4> name_char_table = []() {
   std::array<uint64_t, 4> table{0, 0, 0, 0};
   auto set_bit = [&](unsigned int c) { table[c >> 6] |= (uint64_t{1} << (c & 63)); };
   for (uint8_t c = 'A'; c <= 'Z'; ++c) set_bit(c);
   for (uint8_t c = 'a'; c <= 'z'; ++c) set_bit(c);
   for (uint8_t c = '0'; c <= '9'; ++c) set_bit(c);
   set_bit('.');
   set_bit('-');
   set_bit('_');
   set_bit(':');
   return table;
}();

constexpr std::array<uint64_t, 4> name_start_table = []() {
   std::array<uint64_t, 4> table{0, 0, 0, 0};
   auto set_bit = [&](unsigned int c) { table[c >> 6] |= (uint64_t{1} << (c & 63)); };
   for (uint8_t c = 'A'; c <= 'Z'; ++c) set_bit(c);
   for (uint8_t c = 'a'; c <= 'z'; ++c) set_bit(c);
   set_bit('_');
   set_bit(':');
   return table;
}();

constexpr std::array<char, 256> to_lower_table = []() {
   std::array<char, 256> table{};
   for (int i = 0; i < 256; ++i) {
      char ch = char(i);
      table[i] = ((ch >= 'A') and (ch <= 'Z')) ? ch + 0x20 : ch;
   }
   return table;
}();

constexpr bool is_name_char(char ch) noexcept
{
   return (name_char_table[uint8_t(ch) >> 6] & (uint64_t{1} << (ch & 63))) != 0;
}

constexpr bool is_name_start(char ch) noexcept
{
   return (name_start_table[uint8_t(ch) >> 6] & (uint64_t{1} << (ch & 63))) != 0;
}

constexpr char to_lower(char ch) noexcept
{
   return to_lower_table[uint8_t(ch)];
}

// XML entity escape sequences
constexpr std::string_view xml_entities[] = {
   "&amp;", "&lt;", "&gt;", "&quot;"
};

constexpr char xml_chars[] = { '&', '<', '>', '"' };

// Optimized lookup table for XML character escaping
constexpr std::array<const char*, 256> xml_escape_table = []() {
   std::array<const char*, 256> table{};
   for (int i = 0; i < 256; ++i) {
      table[i] = nullptr; // Most characters don't need escaping
   }
   table['&'] = "&amp;";
   table['<'] = "&lt;";
   table['>'] = "&gt;";
   table['"'] = "&quot;";
   return table;
}();

struct ParseState {
   std::string_view cursor;
   int   Balance;  // Indicates that the tag structure is correctly balanced if zero

   // Namespace context for this parsing scope
   ankerl::unordered_dense::map<std::string, uint32_t> PrefixMap;  // Prefix -> namespace URI hash
   uint32_t DefaultNamespace;                  // Default namespace URI hash
   inline char current() const { return cursor.empty() ? '\0' : cursor.front(); }
   inline bool done() const { return cursor.empty(); }
   inline void next(size_t n = 1) { cursor.remove_prefix(n); }

   inline bool startsWith(std::string_view str) const {
      return cursor.starts_with(str);
   }

   inline void skipTo(char ch, int &line) {
      while ((not done()) and (current() != ch)) {
         if (current() == '\n') line++;
         next();
      }
   }
   
   inline void skipTo(std::string_view Seq, int &line) {
      while ((not done()) and (not cursor.starts_with(Seq))) {
         if (current() == '\n') line++;
         next();
      }
   }

   inline char skipWhitespace(int &line) {
      while ((not done()) and (uint8_t(current()) <= 0x20)) {
         if (current() == '\n') line++;
         next();
      }
      return current();
   }

   ParseState() : cursor(), Balance(0), DefaultNamespace(0) { }
   ParseState(std::string_view Text) : cursor(Text), Balance(0), DefaultNamespace(0) { }

   // Copy constructor for inheriting namespace context from parent scope
   ParseState(const ParseState& parent) : cursor(parent.cursor), Balance(parent.Balance),
                                          PrefixMap(parent.PrefixMap), DefaultNamespace(parent.DefaultNamespace) { }
   
   inline bool operator!=(ParseState const &rhs) const {return !(*this == rhs);}

   inline bool operator==(ParseState const &rhs) const {
      return (cursor.data() == rhs.cursor.data());
   }
   
   inline size_t operator-(ParseState const &rhs) const {
      return (cursor.data() - rhs.cursor.data());
   }
};

typedef objXML::TAGS TAGS;
typedef objXML::CURSOR CURSOR;

// Forward declarations
class CompiledXPath;

// Generic lookup templates with concepts

template<MapKey Key, typename Value>
[[nodiscard]] inline auto find_in_map(
   const ankerl::unordered_dense::map<Key, Value>& Map,
   const Key& SearchKey) noexcept -> const Value*
{
   auto it = Map.find(SearchKey);
   return (it != Map.end()) ? &it->second : nullptr;
}

template<MapKey Key, typename Value>
[[nodiscard]] inline auto find_in_map(
   ankerl::unordered_dense::map<Key, Value>& Map,
   const Key& SearchKey) noexcept -> Value*
{
   auto it = Map.find(SearchKey);
   return (it != Map.end()) ? &it->second : nullptr;
}

template<typename Range, typename Pred>
   requires std::ranges::input_range<Range> and
            std::predicate<Pred, std::ranges::range_reference_t<Range>>
[[nodiscard]] inline auto find_if_range(Range&& range, Pred pred)
{
   return std::ranges::find_if(std::forward<Range>(range), pred);
}

//********************************************************************************************************************

class extXML : public objXML {
   public:
   ankerl::unordered_dense::map<int, XMLTag *> Map; // Lookup for any indexed tag.
   std::string ErrorMsg;    // The most recent error message for an activity, e.g. XPath parsing error
   std::string Statement;
   std::string Attrib;
   bool   ReadOnly;
   bool   StaleMap;         // True if map requires a rebuild

   TAGS *CursorParent;  // Parent tag, if any
   TAGS *CursorTags;    // Updated by findTag().  This is the tag array to which the Cursor reference belongs
   CURSOR Cursor;       // Resulting cursor position (tag) after a successful search.
   FUNCTION Callback;
   
   std::shared_ptr<xml::schema::SchemaContext> SchemaContext;
   ankerl::unordered_dense::map<std::string, std::string> Variables; // XPath variable references
   ankerl::unordered_dense::map<std::string, std::string> Entities; // For general entities
   ankerl::unordered_dense::map<std::string, std::string> ParameterEntities; // For parameter entities
   ankerl::unordered_dense::map<std::string, std::string> Notations; // For notation declarations

   // Namespace registry using pf::strhash() values, this allows us to store URIs in compact form in XMLTag structures.
   ankerl::unordered_dense::map<uint32_t, std::string> NSRegistry; // hash(URI) -> URI

   // Link prefixes to namespace URIs
   // WARNING: If the XML document overwrites namespace URIs on the same prefix name (legal!)
   // then this lookup table returns the most recently assigned URI.
   ankerl::unordered_dense::map<std::string, uint32_t> Prefixes; // hash(Prefix) -> hash(URI)

   ankerl::unordered_dense::map<std::string, std::shared_ptr<extXML>> DocumentCache;
   ankerl::unordered_dense::map<std::string, std::shared_ptr<std::string>> UnparsedTextCache;
   ankerl::unordered_dense::map<const XMLTag *, std::weak_ptr<extXML>> DocumentNodeOwners;

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

   ERR resolveEntity(const std::string &Name, std::string &Value, bool Parameter = false);
   
   // XPath 1.0 Implementation for Parasol
   // 
   // Extra Features Supported:
   // [=...] Match on encapsulated content (Not an XPath standard but we support it)
   // The use of \ as an escape character in attribute strings is supported, but keep in mind that this is not an official
   // feature of the XPath standard.
   // Wildcards are legal only in the context of string comparisons, e.g. a node or attribute lookup
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
   //   /root/section[@*="alpha"] (Match any attribute with value "alpha")

   ERR findTag(CSTRING XPath, FUNCTION *pCallback = nullptr);
   ERR findTag(const CompiledXPath &CompiledPath, FUNCTION *pCallback = nullptr);
   ERR evaluate(CSTRING, std::string &);
   ERR evaluate(const CompiledXPath &, std::string &);

   // Namespace utility methods

   inline uint32_t registerNamespace(const std::string &URI) {
      if (URI.empty()) return 0;
      auto hash = pf::strhash(URI);
      NSRegistry[hash] = URI;
      return hash;
   }

   [[nodiscard]] inline std::string * getNamespaceURI(uint32_t Hash) {
      return find_in_map(NSRegistry, Hash);
   }

   // Fast implementation of ResolvePrefix() for internal use,

   [[nodiscard]] inline ERR resolvePrefix(const std::string_view Prefix, int TagID, uint32_t &Result) {
      for (auto tag = getTag(TagID); tag; tag = getTag(tag->ParentID)) {
         // Check this tag's attributes for namespace declarations using ranges
         auto attribs_view = tag->Attribs | std::views::drop(1);

         auto it = std::ranges::find_if(attribs_view, [&](const auto& attrib) {
            // Check for xmlns:prefix="uri" declarations
            if (attrib.Name.starts_with("xmlns:") and attrib.Name.size() > 6) {
               return attrib.Name.substr(6) IS Prefix;
            }
            // Check for default namespace if looking for empty prefix
            return (attrib.Name IS "xmlns") and Prefix.empty();
         });

         if (it != attribs_view.end()) {
            Result = pf::strhash(it->Value);
            return ERR::Okay;
         }

         if (!tag->ParentID) break; // Reached root
      }

      return ERR::Search;
   }

   inline void updateIDs(TAGS &List, int ParentID) {
      for (auto &tag : List) {
         Map[tag.ID] = &tag;
         tag.ParentID = ParentID;
         if (!tag.Children.empty()) updateIDs(tag.Children, tag.ID);
      }
   }
};

#include "xpath/xpath_ast.h"
#include "xpath/xpath_functions.h"
#include "xpath/xpath_axis.h"
#include "xpath/xpath_parser.h"
#include "xpath/xpath_evaluator.h"

inline ERR extXML::findTag(CSTRING XPath, FUNCTION *pCallback)
{
   auto compiled_path = CompiledXPath::compile(XPath);
   if (not compiled_path.isValid()) {
      ErrorMsg = "XPath compilation error: ";
      for (const auto &err : compiled_path.getErrors()) {
         if (!ErrorMsg.empty()) ErrorMsg += "; ";
         ErrorMsg += err;
      }
      return pf::Log(__FUNCTION__).warning(ERR::Syntax);
   }
   return findTag(compiled_path, pCallback);
}

inline ERR extXML::findTag(const CompiledXPath &CompiledPath, FUNCTION *pCallback)
{
   this->Attrib.clear();

   if (pCallback) this->Callback = *pCallback;
   else this->Callback.Type = CALL::NIL;

   this->CursorTags = &this->Tags;

   Cursor = this->Tags.begin();

   ErrorMsg.clear();

   if ((!CursorTags) or (CursorTags->empty())) {
      return pf::Log(__FUNCTION__).warning(ERR::SanityCheckFailed);
   }

   XPathEvaluator eval(this);
   return eval.find_tag(CompiledPath, 0);
}

inline ERR extXML::evaluate(CSTRING XPath, std::string &Result)
{
   auto compiled_path = CompiledXPath::compile(XPath);
   if (not compiled_path.isValid()) {
      ErrorMsg = "XPath compilation error: ";
      for (const auto &err : compiled_path.getErrors()) {
         if (!ErrorMsg.empty()) ErrorMsg += "; ";
         ErrorMsg += err;
      }
      return pf::Log(__FUNCTION__).warning(ERR::Syntax);
   }
   return evaluate(compiled_path, Result);
}

inline ERR extXML::evaluate(const CompiledXPath &CompiledPath, std::string &Result)
{
   this->Attrib.clear();
   this->CursorTags = &this->Tags;
   Cursor = this->Tags.begin();

   ErrorMsg.clear();

   if ((!CursorTags) or (CursorTags->empty())) {
      return pf::Log(__FUNCTION__).warning(ERR::SanityCheckFailed);
   }

   XPathValue eval_result;
   XPathEvaluator eval(this);
   auto err = eval.evaluate_xpath_expression(CompiledPath, eval_result);
   if (err IS ERR::Okay) Result = eval_result.to_string();
   return err;
}
