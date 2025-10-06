//********************************************************************************************************************
// XPath Accessor Support Utilities
//
// These helpers centralise the document and schema lookups required by accessor-style XPath functions.  The
// routines replicate the behaviour previously embedded inside func_documents.cpp so that the accessor
// implementations can determine owning documents, resolve xml:base inheritance, and query schema metadata in a
// consistent manner regardless of the source tree for a node.
//********************************************************************************************************************

#pragma once

#include <memory>
#include <optional>
#include <string>

struct XMLTag;
class extXML;
struct XPathContext;
struct XMLAttrib;

namespace xml::schema
{
   class SchemaTypeDescriptor;
}

namespace xpath::accessor
{
   struct NodeOrigin
   {
      // `document` is a borrowed pointer to the owning extXML instance.  Callers can use it immediately without
      // incurring shared ownership, which matches the historical behaviour of helpers that accepted raw document
      // pointers.
      extXML * document = nullptr;
      // `holder` keeps a shared_ptr alive when a node originates from a cached document that is not referenced by
      // the current evaluation context.  When populated it ensures the document stays valid for the duration of the
      // accessor operation.
      std::shared_ptr<extXML> holder;
   };

   NodeOrigin locate_node_document(const XPathContext &Context, XMLTag *Node);

   std::optional<std::string> build_base_uri_chain(const XPathContext &Context, XMLTag *Node, const XMLAttrib *AttributeNode);

   std::optional<std::string> resolve_document_uri(const XPathContext &Context, XMLTag *Node);

   std::shared_ptr<xml::schema::SchemaTypeDescriptor> infer_schema_type(const XPathContext &Context, XMLTag *Node,
      const XMLAttrib *AttributeNode);

   bool is_element_explicitly_nilled(const XPathContext &Context, XMLTag *Node);
}

