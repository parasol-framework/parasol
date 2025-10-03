//********************************************************************************************************************
// XPath Accessor Support Utilities
//
// These helpers centralise the document and schema lookups required by accessor-style XPath functions.  The
// existing document functions in func_documents.cpp expose similar logic but rely on internal static helpers.  During
// the phase 9 implementation we will migrate those routines into this shared layer so base-uri(), document-uri(), and
// related accessors can operate consistently whether a node originates from the primary document tree or a cached
// external resource.
//
// The helpers are intentionally declared here without concrete implementations yet.  Each function documents the
// precise responsibilities it will fulfil once phase 9 reaches the implementation stage.  Keeping the contracts explicit
// at this stage ensures subsequent patches can focus on behaviour rather than reverse-engineering requirements.
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
      extXML * document = nullptr;
      std::shared_ptr<extXML> holder;
   };

   NodeOrigin locate_node_document(const XPathContext &Context, XMLTag *Node);

   std::optional<std::string> build_base_uri_chain(const XPathContext &Context, XMLTag *Node, const XMLAttrib *AttributeNode);

   std::optional<std::string> resolve_document_uri(const XPathContext &Context, XMLTag *Node);

   std::shared_ptr<xml::schema::SchemaTypeDescriptor> infer_schema_type(const XPathContext &Context, XMLTag *Node,
      const XMLAttrib *AttributeNode);

   bool is_element_explicitly_nilled(const XPathContext &Context, XMLTag *Node);
}

