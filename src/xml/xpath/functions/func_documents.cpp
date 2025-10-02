//********************************************************************************************************************
// XPath Document and Text Retrieval Functions

#include <parasol/modules/xml.h>
#include <parasol/modules/core.h>
#include <parasol/strings.hpp>

#include "../xpath_functions.h"
#include "../../xml.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <optional>
#include <string_view>
#include <unordered_set>

namespace {

namespace fs = std::filesystem;

static bool is_string_uri(std::string_view Value)
{
   return Value.rfind("string:", 0u) IS 0u;
}

static std::string normalise_newlines(const std::string &Input)
{
   std::string output;
   output.reserve(Input.length());

   size_t index = 0u;
   while (index < Input.length()) {
      char ch = Input[index];
      if (ch IS '\r') {
         output.push_back('\n');
         if ((index + 1u < Input.length()) and (Input[index + 1u] IS '\n')) index++;
      }
      else output.push_back(ch);
      index++;
   }

   return output;
}

static std::optional<std::string> get_context_directory(const XPathContext &Context)
{
   if ((Context.document) and (Context.document->Path) and (*Context.document->Path)) {
      std::string resolved;
      if (ResolvePath(Context.document->Path, RSF::NO_FILE_CHECK, &resolved) IS ERR::Okay) {
         fs::path base_path(resolved);
         base_path = base_path.parent_path();
         return base_path.string();
      }

      std::string raw = Context.document->Path;
      size_t slash = raw.find_last_of("/\\");
      if (slash != std::string::npos) return raw.substr(0u, slash + 1u);
   }

   return std::nullopt;
}

static bool resolve_resource_location(const XPathContext &Context, const std::string &Uri, std::string &Resolved)
{
   if (Uri.empty()) return false;
   if (is_string_uri(Uri)) { Resolved = Uri; return true; }

   std::string candidate;
   if (ResolvePath(Uri, RSF::NO_FILE_CHECK, &candidate) IS ERR::Okay) {
      Resolved = candidate;
      return true;
   }

   if (Context.document) {
      auto base = get_context_directory(Context);
      if (base.has_value()) {
         fs::path base_path(*base);
         fs::path combined = base_path / Uri;
         std::string combined_str = combined.lexically_normal().string();

         if (ResolvePath(combined_str, RSF::NO_FILE_CHECK, &candidate) IS ERR::Okay) {
            Resolved = candidate;
            return true;
         }

         Resolved = combined_str;
         return true;
      }
   }

   Resolved = Uri;
   return true;
}

static void register_document_node(extXML *Owner, const std::shared_ptr<extXML> &Document, XMLTag &Tag)
{
   if ((!Owner) or (!Document)) return;

   Owner->DocumentNodeOwners[&Tag] = Document;
   for (auto &child : Tag.Children) register_document_node(Owner, Document, child);
}

static void register_document_nodes(extXML *Owner, const std::shared_ptr<extXML> &Document)
{
   if ((!Owner) or (!Document)) return;
   for (auto &tag : Document->Tags) register_document_node(Owner, Document, tag);
}

static std::shared_ptr<extXML> load_document(extXML *Owner, const std::string &Uri)
{
   if (!Owner) return nullptr;

   auto existing = Owner->DocumentCache.find(Uri);
   if (existing != Owner->DocumentCache.end()) return existing->second;

   std::shared_ptr<extXML> document;

   if (is_string_uri(Uri)) {
      std::string payload = Uri.substr(7u);
      if (auto raw = pf::Create<extXML>::global({ fl::Statement(payload.c_str()), fl::Flags(XMF::WELL_FORMED | XMF::NAMESPACE_AWARE) })) {
         document = pf::make_shared_object(raw);
      }
   }
   else {
      if (auto raw = pf::Create<extXML>::global({ fl::Path(Uri.c_str()), fl::Flags(XMF::WELL_FORMED | XMF::NAMESPACE_AWARE) })) {
         document = pf::make_shared_object(raw);
      }
   }

   if (!document) return nullptr;
   if (document->Tags.empty()) return nullptr;

   document->getMap();
   register_document_nodes(Owner, document);
   Owner->DocumentCache[Uri] = document;
   return document;
}

static bool read_text_resource(extXML *Owner, const std::string &Uri, const std::optional<std::string> &Encoding,
   std::shared_ptr<std::string> &Result)
{
   if (!Owner) return false;

   if (Encoding.has_value()) {
      std::string lowered = lowercase_copy(*Encoding);
      if ((lowered != "utf-8") and (lowered != "utf8")) return false;
   }

   auto existing = Owner->UnparsedTextCache.find(Uri);
   if (existing != Owner->UnparsedTextCache.end()) {
      Result = existing->second;
      return true;
   }

   if (is_string_uri(Uri)) {
      auto text = std::make_shared<std::string>(normalise_newlines(Uri.substr(7u)));
      Owner->UnparsedTextCache[Uri] = text;
      Result = text;
      return true;
   }

   CacheFile *filecache = nullptr;
   if (LoadFile(Uri.c_str(), LDF::NIL, &filecache) != ERR::Okay) return false;

   std::shared_ptr<std::string> text = std::make_shared<std::string>((const char *)filecache->Data,
      (size_t)filecache->Size);
   UnloadFile(filecache);

   *text = normalise_newlines(*text);
   Owner->UnparsedTextCache[Uri] = text;
   Result = text;
   return true;
}

static extXML * locate_document_for_node(const XPathContext &Context, XMLTag *Node, std::shared_ptr<extXML> *Holder)
{
   if (!Node) return nullptr;

   if (Context.document) {
      auto &map = Context.document->getMap();
      auto it = map.find(Node->ID);
      if ((it != map.end()) and (it->second IS Node)) {
         if (Holder) Holder->reset();
         return Context.document;
      }
   }

   if (Context.document) {
      auto entry = Context.document->DocumentNodeOwners.find(Node);
      if (entry != Context.document->DocumentNodeOwners.end()) {
         auto doc = entry->second.lock();
         if (doc) {
            if (Holder) *Holder = doc;
            return doc.get();
         }
      }
   }

   return nullptr;
}

static XMLTag * locate_root_node(extXML *Document, XMLTag *Node)
{
   if ((!Document) or (!Node)) return nullptr;

   XMLTag *current = Node;
   while (current->ParentID != 0) {
      XMLTag *parent = Document->getTag(current->ParentID);
      if (!parent) break;
      current = parent;
   }

   return current;
}

static std::vector<std::string> split_whitespace_tokens(std::string_view Value)
{
   std::vector<std::string> tokens;
   size_t offset = 0u;

   while (offset < Value.length()) {
      while ((offset < Value.length()) and std::isspace((unsigned char)Value[offset])) offset++;
      size_t start = offset;
      while ((offset < Value.length()) and (not std::isspace((unsigned char)Value[offset]))) offset++;
      if (offset > start) tokens.emplace_back(Value.substr(start, offset - start));
   }

   return tokens;
}

static void collect_idref_matches(extXML *Document, const std::unordered_set<std::string> &Targets,
   std::unordered_set<const XMLTag *> &Seen, std::vector<XMLTag *> &Matches)
{
   if ((!Document) or Targets.empty()) return;

   std::vector<XMLTag *> stack;
   stack.reserve(Document->Tags.size());
   for (auto &root : Document->Tags) stack.push_back(&root);

   while (!stack.empty()) {
      XMLTag *current = stack.back();
      stack.pop_back();

      if ((!current->Attribs.empty()) and current->isTag()) {
         for (size_t index = 1u; index < current->Attribs.size(); ++index) {
            const auto &attrib = current->Attribs[index];
            if (attrib.Name.empty()) continue;

            if (pf::iequals(attrib.Name, "idref") or pf::iequals(attrib.Name, "xml:idref") or
                pf::iequals(attrib.Name, "idrefs") or pf::iequals(attrib.Name, "xml:idrefs")) {
               auto tokens = split_whitespace_tokens(attrib.Value);
               bool matched = false;
               for (const auto &token : tokens) {
                  if (Targets.find(token) != Targets.end()) { matched = true; break; }
               }

               if (matched) {
                  if (Seen.insert(current).second) Matches.push_back(current);
                  break;
               }
            }
         }
      }

      for (auto &child : current->Children) stack.push_back(&child);
   }
}

static std::vector<std::string> enumerate_collection(const std::string &Directory)
{
   std::vector<std::string> entries;
   std::error_code ec;
   fs::directory_iterator iter(fs::path(Directory), fs::directory_options::skip_permission_denied, ec);
   if (ec) return entries;

   fs::directory_iterator end;
   for (; iter != end; iter.increment(ec)) {
      if (ec) break;
      const fs::directory_entry &entry = *iter;

      std::error_code status_ec;
      if (not entry.is_regular_file(status_ec)) continue;

      std::string extension = entry.path().extension().string();
      if (pf::iequals(extension, ".xml")) entries.push_back(entry.path().string());
   }

   std::sort(entries.begin(), entries.end());
   return entries;
}

} // namespace

XPathValue XPathFunctionLibrary::function_root(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   XMLTag *node = nullptr;

   if (!Args.empty()) {
      const XPathValue &value = Args[0];
      if ((value.type IS XPathValueType::NodeSet) and (not value.node_set.empty())) node = value.node_set[0];
      else return XPathValue(std::vector<XMLTag *>());
   }
   else node = Context.context_node;

   if (!node) return XPathValue(std::vector<XMLTag *>());

   std::shared_ptr<extXML> holder;
   extXML *document = locate_document_for_node(Context, node, &holder);
   if (!document) return XPathValue(std::vector<XMLTag *>());

   XMLTag *root = locate_root_node(document, node);
   if (!root) return XPathValue(std::vector<XMLTag *>());

   std::vector<XMLTag *> result = { root };
   return XPathValue(result);
}

XPathValue XPathFunctionLibrary::function_doc(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::vector<XMLTag *>());
   if (!Context.document) return XPathValue(std::vector<XMLTag *>());

   std::string uri = Args[0].to_string();
   if (uri.empty()) return XPathValue(std::vector<XMLTag *>());

   std::string resolved;
   if (!resolve_resource_location(Context, uri, resolved)) return XPathValue(std::vector<XMLTag *>());

   auto document = load_document(Context.document, resolved);
   if (!document) return XPathValue(std::vector<XMLTag *>());

   std::vector<XMLTag *> nodes;
   for (auto &tag : document->Tags) {
      if ((tag.Flags & XTF::INSTRUCTION) != XTF::NIL) continue;
      nodes.push_back(&tag);
   }

   return XPathValue(nodes);
}

XPathValue XPathFunctionLibrary::function_doc_available(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(false);
   if (!Context.document) return XPathValue(false);

   std::string uri = Args[0].to_string();
   if (uri.empty()) return XPathValue(false);

   std::string resolved;
   if (!resolve_resource_location(Context, uri, resolved)) return XPathValue(false);

   if (is_string_uri(resolved)) return XPathValue(true);

   if (Context.document->DocumentCache.find(resolved) != Context.document->DocumentCache.end()) return XPathValue(true);

   CacheFile *filecache = nullptr;
   if (LoadFile(resolved.c_str(), LDF::NIL, &filecache) IS ERR::Okay) {
      UnloadFile(filecache);
      return XPathValue(true);
   }

   return XPathValue(false);
}

XPathValue XPathFunctionLibrary::function_collection(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (!Context.document) return XPathValue(std::vector<XMLTag *>());

   std::string resolved;
   if (Args.empty()) {
      auto base = get_context_directory(Context);
      if (not base.has_value()) return XPathValue(std::vector<XMLTag *>());
      resolved = *base;
   }
   else {
      std::string uri = Args[0].to_string();
      if (uri.empty()) return XPathValue(std::vector<XMLTag *>());
      if (!resolve_resource_location(Context, uri, resolved)) return XPathValue(std::vector<XMLTag *>());
   }

   if (is_string_uri(resolved)) return XPathValue(std::vector<XMLTag *>());

   auto entries = enumerate_collection(resolved);
   std::vector<XMLTag *> nodes;

   for (const auto &entry : entries) {
      auto document = load_document(Context.document, entry);
      if (!document) continue;

      for (auto &tag : document->Tags) {
         if ((tag.Flags & XTF::INSTRUCTION) != XTF::NIL) continue;
         nodes.push_back(&tag);
      }
   }

   return XPathValue(nodes);
}

XPathValue XPathFunctionLibrary::function_uri_collection(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (!Context.document) return XPathValue(std::vector<XMLTag *>());

   std::string resolved;
   if (Args.empty()) {
      auto base = get_context_directory(Context);
      if (not base.has_value()) return XPathValue(std::vector<XMLTag *>());
      resolved = *base;
   }
   else {
      std::string uri = Args[0].to_string();
      if (uri.empty()) return XPathValue(std::vector<XMLTag *>());
      if (!resolve_resource_location(Context, uri, resolved)) return XPathValue(std::vector<XMLTag *>());
   }

   if (is_string_uri(resolved)) return XPathValue(std::vector<XMLTag *>());

   auto entries = enumerate_collection(resolved);
   std::vector<XMLTag *> nodes;
   std::vector<std::string> values;

   for (const auto &entry : entries) {
      nodes.push_back(nullptr);
      values.push_back(entry);
   }

   return XPathValue(nodes, std::nullopt, values);
}

XPathValue XPathFunctionLibrary::function_unparsed_text(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::string());
   if (!Context.document) return XPathValue(std::string());

   std::string uri = Args[0].to_string();
   if (uri.empty()) return XPathValue(std::string());

   std::optional<std::string> encoding;
   if (Args.size() > 1u) {
      std::string value = Args[1].to_string();
      if (not value.empty()) encoding = value;
   }

   std::string resolved;
   if (!resolve_resource_location(Context, uri, resolved)) return XPathValue(std::string());

   std::shared_ptr<std::string> text;
   if (!read_text_resource(Context.document, resolved, encoding, text)) return XPathValue(std::string());

   return XPathValue(*text);
}

XPathValue XPathFunctionLibrary::function_unparsed_text_available(const std::vector<XPathValue> &Args,
   const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(false);
   if (!Context.document) return XPathValue(false);

   std::string uri = Args[0].to_string();
   if (uri.empty()) return XPathValue(false);

   std::optional<std::string> encoding;
   if (Args.size() > 1u) {
      std::string value = Args[1].to_string();
      if (not value.empty()) encoding = value;
   }

   std::string resolved;
   if (!resolve_resource_location(Context, uri, resolved)) return XPathValue(false);

   std::shared_ptr<std::string> text;
   if (read_text_resource(Context.document, resolved, encoding, text)) return XPathValue(true);
   return XPathValue(false);
}

XPathValue XPathFunctionLibrary::function_unparsed_text_lines(const std::vector<XPathValue> &Args,
   const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::vector<XMLTag *>());
   if (!Context.document) return XPathValue(std::vector<XMLTag *>());

   std::string uri = Args[0].to_string();
   if (uri.empty()) return XPathValue(std::vector<XMLTag *>());

   std::optional<std::string> encoding;
   if (Args.size() > 1u) {
      std::string value = Args[1].to_string();
      if (not value.empty()) encoding = value;
   }

   std::string resolved;
   if (!resolve_resource_location(Context, uri, resolved)) return XPathValue(std::vector<XMLTag *>());

   std::shared_ptr<std::string> text;
   if (!read_text_resource(Context.document, resolved, encoding, text)) return XPathValue(std::vector<XMLTag *>());

   std::vector<XMLTag *> nodes;
   std::vector<std::string> lines;

   size_t start = 0u;
   while (start <= text->length()) {
      size_t end = text->find('\n', start);
      if (end IS std::string::npos) {
         lines.emplace_back(text->substr(start));
         break;
      }

      lines.emplace_back(text->substr(start, end - start));
      start = end + 1u;
      if (start > text->length()) lines.emplace_back(std::string());
   }

   return XPathValue(nodes, std::nullopt, lines);
}

XPathValue XPathFunctionLibrary::function_idref(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   std::vector<XMLTag *> results;
   if (Args.empty()) return XPathValue(results);
   if (!Context.document) return XPathValue(results);

   std::unordered_set<std::string> requested_ids;

   auto add_tokens = [&](const std::string &Value) {
      auto tokens = split_whitespace_tokens(Value);
      for (const auto &token : tokens) requested_ids.insert(token);
   };

   for (const auto &arg : Args) {
      switch (arg.type) {
         case XPathValueType::NodeSet: {
            if (not arg.node_set_string_values.empty()) {
               for (const auto &entry : arg.node_set_string_values) add_tokens(entry);
            }
            else if (arg.node_set_string_override.has_value()) add_tokens(*arg.node_set_string_override);
            else {
               for (auto *node : arg.node_set) {
                  if (not node) continue;
                  add_tokens(node->getContent());
               }
            }
            break;
         }

         case XPathValueType::String:
         case XPathValueType::Date:
         case XPathValueType::Time:
         case XPathValueType::DateTime:
            add_tokens(arg.string_value);
            break;

         case XPathValueType::Boolean:
            add_tokens(arg.to_string());
            break;

         case XPathValueType::Number:
            if (not std::isnan(arg.number_value)) add_tokens(arg.to_string());
            break;
      }
   }

   if (requested_ids.empty()) return XPathValue(results);

   std::unordered_set<const XMLTag *> seen;
   collect_idref_matches(Context.document, requested_ids, seen, results);

   for (const auto &entry : Context.document->DocumentCache) {
      collect_idref_matches(entry.second.get(), requested_ids, seen, results);
   }

   return XPathValue(results);
}

