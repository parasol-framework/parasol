//*****************************************************************************************************************
// XPath Accessor Support Utilities
//
// Shared helpers that offer document and schema discovery for accessor-style XPath functions.  The routines
// consolidate logic that was previously embedded inside the document helper functions so that fn:base-uri(),
// fn:document-uri(), fn:data(), fn:nilled() and related calls can reuse consistent behaviour regardless of the
// source document for a node.
//*****************************************************************************************************************

#include "accessor_support.h"

#include "../api/xpath_functions.h"
#include "../../xml/xml.h"
#include "../../xml/schema/schema_parser.h"
#include "../../xml/uri_utils.h"

#include <parasol/strings.hpp>

#include <algorithm>
#include <format>
#include <string_view>

namespace xpath::accessor {

//********************************************************************************************************************

[[nodiscard]] static bool attribute_is_xml_base(const XMLAttrib &Attribute)
{
   if (Attribute.Name.empty()) return false;
   return pf::iequals(Attribute.Name, "xml:base");
}

//********************************************************************************************************************

[[nodiscard]] static std::optional<std::string> document_path(extXML *Document)
{
   if (!Document) return std::nullopt;

   std::string path = Document->Path;
   if (!path.empty()) return path;

   return std::nullopt;
}

//********************************************************************************************************************

[[nodiscard]] static XMLTag * parent_for_node(extXML *Document, XMLTag *Node)
{
   if ((!Document) or (!Node)) return nullptr;
   if (Node->ParentID IS 0) return nullptr;
   return Document->getTag(Node->ParentID);
}

//********************************************************************************************************************

[[nodiscard]] static XMLTag * find_attribute_owner(extXML *Document, const XMLAttrib *Attribute)
{
   if ((!Document) or (!Attribute)) return nullptr;

   auto &map = Document->getMap();
   for (auto &entry : map) {
      XMLTag *candidate = entry.second;
      if (!candidate) continue;

      for (auto &attrib : candidate->Attribs) {
         const XMLAttrib *attrib_ptr = &attrib;
         if (attrib_ptr IS Attribute) return candidate;
      }
   }

   return nullptr;
}

//********************************************************************************************************************
// Returns the XMLTag that owns an attribute.  If a NodeHint is provided, it is checked first to see if it owns the attribute.

[[nodiscard]] static XMLTag * resolve_attribute_scope(const XPathContext &Context, XMLTag *NodeHint, const XMLAttrib *Attribute, extXML *&Document)
{
   if (!Attribute) return NodeHint;

   if (NodeHint) {
      for (auto &attrib : NodeHint->Attribs) {
         const XMLAttrib *attrib_ptr = &attrib;
         if (attrib_ptr IS Attribute) return NodeHint;
      }
   }

   auto locate_in_document = [&](extXML *Candidate) -> XMLTag * {
      if (!Candidate) return nullptr;
      if (XMLTag *owner = find_attribute_owner(Candidate, Attribute); owner) {
         Document = Candidate;
         return owner;
      }
      return nullptr;
   };

   if (Document) {
      if (auto owner = locate_in_document(Document)) return owner;
   }

   if (Context.document) {
      if (auto owner = locate_in_document(Context.document)) return owner;

      for (auto &entry : Context.document->XMLCache) {
         if (auto owner = locate_in_document(entry.second)) return owner;
      }
   }

   return NodeHint;
}

//********************************************************************************************************************

[[nodiscard]] static std::shared_ptr<xml::schema::ElementDescriptor> find_element_descriptor(extXML *Document,
   std::string_view Name)
{
   if ((!Document) or (!Document->SchemaContext)) return nullptr;

   auto &context = *Document->SchemaContext;

   auto lookup = context.elements.find(std::string(Name));
   if (lookup != context.elements.end()) return lookup->second;

   auto local = std::string(xml::schema::extract_local_name(Name));
   lookup = context.elements.find(local);
   if (lookup != context.elements.end()) return lookup->second;

   if (!context.target_namespace_prefix.empty()) {
      std::string qualified = std::format("{}:{}", context.target_namespace_prefix, local);
      lookup = context.elements.find(qualified);
      if (lookup != context.elements.end()) return lookup->second;
   }

   return nullptr;
}

//********************************************************************************************************************

[[nodiscard]] static std::shared_ptr<xml::schema::SchemaTypeDescriptor> resolve_type_descriptor(
   const XPathContext &Context, extXML *Document, const std::string &TypeName)
{
   if (TypeName.empty()) return nullptr;

   if ((Document) and Document->SchemaContext) {
      auto &types = Document->SchemaContext->types;
      auto iter = types.find(TypeName);
      if (iter != types.end()) return iter->second;

      auto local = std::string(xml::schema::extract_local_name(TypeName));
      iter = types.find(local);
      if (iter != types.end()) return iter->second;
   }

   if (!Context.schema_registry) return nullptr;

   if (auto descriptor = Context.schema_registry->find_descriptor(TypeName)) return descriptor;

   auto local = std::string(xml::schema::extract_local_name(TypeName));
   return Context.schema_registry->find_descriptor(local);
}

//********************************************************************************************************************

[[nodiscard]] static bool attribute_matches_nil(const XMLAttrib &Attribute, XMLTag *Scope, extXML *Document)
{
   if (Attribute.Name.empty()) return false;

   std::string_view name(Attribute.Name);
   auto colon = name.find(':');
   if (colon IS std::string::npos) return false;

   std::string prefix(name.substr(0, colon));
   std::string local(name.substr(colon + 1));

   if (!pf::iequals(local, "nil")) return false;

   if (pf::iequals(prefix, "xml")) return false;
   if (pf::iequals(prefix, "xmlns")) return false;

   std::string uri;
   if (Document) uri = find_in_scope_namespace(Scope, Document, prefix);
   if (uri.empty()) return false;

   return pf::iequals(uri, "http://www.w3.org/2001/XMLSchema-instance");
}

//********************************************************************************************************************
// Locates the document that contains a particular node.

[[nodiscard]] extXML * locate_node_document(const XPathContext &Context, XMLTag *Node)
{
   if (!Node) return nullptr;

   if (Context.document) {
      auto &map = Context.document->getMap();
      auto it = map.find(Node->ID);
      if ((it != map.end()) and (it->second IS Node)) {
         return Context.document;
      }

      for (auto &it : Context.document->XMLCache) {
         extXML *cached_xml = it.second;
         auto &cached_map = cached_xml->getMap();
         auto cit = cached_map.find(Node->ID);
         if ((cit != cached_map.end()) and (cit->second IS Node)) return cached_xml;
      }
   }
   return nullptr;
}

//********************************************************************************************************************
// Builds the base URI chain for a node or attribute-only node-set.  Note that setting AttributeNode will result in
// Node being recomputed.

std::optional<std::string> build_base_uri_chain(const XPathContext &Context, XMLTag *Node, const XMLAttrib *AttributeNode)
{
   extXML *document;
   if (extXML *origin = locate_node_document(Context, Node)) document = origin;
   else document = Context.document;

   if (AttributeNode) {
      Node = resolve_attribute_scope(Context, Node, AttributeNode, document);

      if (Node) {
         extXML *owner_origin = locate_node_document(Context, Node);
         if (owner_origin) document = owner_origin;
      }
   }

   if (!Node) {
      auto base = document_path(document ? document : Context.document);
      if (base.has_value()) return xml::uri::normalise_uri_separators(*base);
      return std::nullopt;
   }

   if (!document) {
      extXML *owner_origin = locate_node_document(Context, Node);
      if (owner_origin) document = owner_origin;
      else document = Context.document;
   }

   if ((Node->ParentID IS 0) and (!AttributeNode)) { // Root-level node with no attribute
      auto base = document_path(document ? document : Context.document);
      if (base.has_value()) return xml::uri::normalise_uri_separators(*base);
   }

   const std::string * cached_base = nullptr;
   if (document) cached_base = document->findBaseURI(Node->ID);
   else if (Context.document) cached_base = Context.document->findBaseURI(Node->ID);

   if (cached_base) return xml::uri::normalise_uri_separators(*cached_base);

   std::vector<std::string> chain;
   for (XMLTag *current = Node; current; ) {
      bool skip_current_xml_base = (not current->ParentID) and (current IS Node) and (not AttributeNode);

      for (size_t index = 1; index < current->Attribs.size(); ++index) {
         const XMLAttrib &attrib = current->Attribs[index];
         if (!attribute_is_xml_base(attrib)) continue;
         if (skip_current_xml_base) continue;
         chain.push_back(attrib.Value);
      }

      XMLTag *parent = parent_for_node(document, current);
      if (!parent) break;
      current = parent;
   }

   std::optional<std::string> base = document_path(document);

   for (auto iterator = chain.rbegin(); iterator != chain.rend(); ++iterator) {
      if (base.has_value()) base = xml::uri::resolve_relative_uri(*iterator, *base);
      else base = *iterator;
   }

   if (base.has_value()) return xml::uri::normalise_uri_separators(*base);
   return std::nullopt;
}

//********************************************************************************************************************
// Resolves the document URI for a node.

std::optional<std::string> resolve_document_uri(const XPathContext &Context, XMLTag *Node)
{
   if (!Node) return std::nullopt;

   extXML *document = locate_node_document(Context, Node);
   if (!document) return std::nullopt;
   if (document->Path and document->Path[0]) {
      return xml::uri::normalise_uri_separators(std::string(document->Path));
   }

   // Perform a reverse lookup in the XML cache to find the document URI.

   if (Context.document) {
      for (auto &entry : Context.document->XMLCache) {
         if (entry.second IS document) {
            return xml::uri::normalise_uri_separators(entry.first); // TODO: Is normalisation needed here?
         }
      }
   }

   return std::nullopt;
}

//********************************************************************************************************************
// Infers the schema type for an element node.

std::shared_ptr<xml::schema::SchemaTypeDescriptor> infer_schema_type(const XPathContext &Context, XMLTag *Node,
   const XMLAttrib *AttributeNode)
{
   if (!Context.schema_registry) return nullptr;
   if (!Node) return nullptr;
   if (AttributeNode) return nullptr;
   if (Node->Attribs.empty()) return nullptr;
   if (Node->Attribs[0].Name.empty()) return nullptr;

   extXML *origin = locate_node_document(Context, Node);
   extXML *document = origin ? origin : Context.document;
   if ((!document) or (!document->SchemaContext)) return nullptr;

   auto descriptor = find_element_descriptor(document, Node->Attribs[0].Name);
   if (!descriptor) return nullptr;

   if (descriptor->type) return descriptor->type;

   if (!descriptor->type_name.empty()) {
      if (auto resolved = resolve_type_descriptor(Context, document, descriptor->type_name)) return resolved;
   }

   return nullptr;
}

//********************************************************************************************************************
// Determines whether an element is explicitly marked as nilled via the xsi:nil attribute.

bool is_element_explicitly_nilled(const XPathContext &Context, XMLTag *Node)
{
   if ((!Node) or Node->Attribs.empty()) return false;
   if (Node->Attribs[0].Name.empty()) return false;

   extXML *origin = locate_node_document(Context, Node);
   extXML *document = origin ? origin : Context.document;

   for (size_t index = 1; index < Node->Attribs.size(); ++index) {
      const XMLAttrib &attrib = Node->Attribs[index];
      if (!attribute_matches_nil(attrib, Node, document)) continue;

      auto parsed = parse_schema_boolean(attrib.Value);
      if (parsed.has_value()) return parsed.value();
   }

   return false;
}

} // namespace xpath::accessor
