
#include "xquery_prolog.h"
#include <parasol/strings.hpp>
#include <utility>
#include "../xpath.h"
#include "../../xml/xml.h"
#include "../../xml/uri_utils.h"

namespace {
   [[nodiscard]] inline std::string build_function_signature(std::string_view QName, size_t Arity) {
      std::string signature;
      signature.reserve(QName.size() + 16U);
      signature.append(QName);
      signature.push_back('/');
      signature.append(std::to_string(Arity));
      return signature;
   }
}

XQueryProlog::XQueryProlog()
{
   default_collation = "http://www.w3.org/2005/xpath-functions/collation/codepoint";
   decimal_formats[std::string()] = DecimalFormat();
}

std::string XQueryFunction::signature() const
{
   return build_function_signature(qname, parameter_names.size());
}

std::shared_ptr<extXML> XQueryModuleCache::fetch_or_load(std::string_view uri,
   const XQueryProlog &prolog, XPathErrorReporter &reporter) const
{
   (void)prolog;
   (void)reporter;

   if (uri.empty()) return nullptr;

   std::string uri_key(uri);

   auto found = modules.find(uri_key);
   if (found != modules.end()) return found->second;

   if (owner) {
      auto document_cache = owner->DocumentCache.find(uri_key);
      if (document_cache != owner->DocumentCache.end()) {
         modules[document_cache->first] = document_cache->second;
         return document_cache->second;
      }
   }

   return nullptr;
}

const XQueryFunction * XQueryProlog::find_function(std::string_view qname, size_t arity) const
{
   auto signature = build_function_signature(qname, arity);
   auto entry = functions.find(signature);
   if (entry != functions.end()) return &entry->second;
   return nullptr;
}

const XQueryVariable * XQueryProlog::find_variable(std::string_view qname) const
{
   auto entry = variables.find(std::string(qname));
   if (entry != variables.end()) return &entry->second;
   return nullptr;
}

uint32_t XQueryProlog::resolve_prefix(std::string_view prefix, const extXML *document) const
{
   auto mapping = declared_namespaces.find(std::string(prefix));
   if (mapping != declared_namespaces.end()) return mapping->second;

   if (prefix.empty()) {
      if (default_element_namespace.has_value()) return *default_element_namespace;
      return 0U;
   }

   if (document) {
      auto doc_entry = document->Prefixes.find(std::string(prefix));
      if (doc_entry != document->Prefixes.end()) return doc_entry->second;
   }

   return 0U;
}

void XQueryProlog::declare_namespace(std::string_view prefix, std::string_view uri, extXML *document)
{
   std::string cleaned = xml::uri::normalise_uri_separators(std::string(uri));
   auto hash = pf::strhash(cleaned);
   std::string prefix_key(prefix);
   declared_namespaces[prefix_key] = hash;
   declared_namespace_uris[prefix_key] = cleaned;

   if (document) {
      document->NSRegistry[hash] = cleaned;
      document->Prefixes[prefix_key] = hash;
   }
}

void XQueryProlog::declare_variable(std::string_view qname, XQueryVariable variable)
{
   std::string key(qname);
   auto inserted = variables.insert_or_assign(key, std::move(variable));
   inserted.first->second.qname = key;
}

void XQueryProlog::declare_function(XQueryFunction function)
{
   auto signature = function.signature();
   functions[std::move(signature)] = std::move(function);
}

void XQueryProlog::bind_module_cache(std::shared_ptr<XQueryModuleCache> cache)
{
   module_cache = cache;
}

std::shared_ptr<XQueryModuleCache> XQueryProlog::get_module_cache() const
{
   return module_cache.lock();
}

std::string XQueryProlog::normalise_function_qname(std::string_view qname, const extXML *document) const
{
   auto build_expanded = [](const std::string &NamespaceURI, std::string_view Local) {
      std::string expanded("Q{");
      expanded.append(NamespaceURI);
      expanded.push_back('}');
      expanded.append(Local);
      return expanded;
   };

   size_t colon = qname.find(':');
   if (colon != std::string_view::npos)
   {
      std::string prefix(qname.substr(0U, colon));
      std::string_view local_view = qname.substr(colon + 1U);

      auto uri_entry = declared_namespace_uris.find(prefix);
      if (uri_entry not_eq declared_namespace_uris.end())
      {
         return build_expanded(uri_entry->second, local_view);
      }

      if (document)
      {
         auto prefix_it = document->Prefixes.find(prefix);
         if (prefix_it not_eq document->Prefixes.end())
         {
            auto ns_it = document->NSRegistry.find(prefix_it->second);
            if (ns_it not_eq document->NSRegistry.end())
            {
               return build_expanded(ns_it->second, local_view);
            }
         }
      }

      return std::string(qname);
   }

   if (default_function_namespace_uri.has_value())
   {
      return build_expanded(*default_function_namespace_uri, qname);
   }

   if (default_function_namespace.has_value())
   {
      std::string expanded("Q{");
      expanded.append(std::to_string(*default_function_namespace));
      expanded.push_back('}');
      expanded.append(qname);
      return expanded;
   }

   return std::string(qname);
}
