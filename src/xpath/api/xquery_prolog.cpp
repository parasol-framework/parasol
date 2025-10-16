
#include "xquery_prolog.h"
#include <parasol/strings.hpp>
#include <utility>
#include "../xpath.h"
#include "../../xml/xml.h"

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
   auto hash = pf::strhash(uri);
   declared_namespaces[std::string(prefix)] = hash;

   if (document) {
      document->NSRegistry[hash] = std::string(uri);
      document->Prefixes[std::string(prefix)] = hash;
   }
}

void XQueryProlog::declare_variable(std::string_view qname, XQueryVariable variable)
{
   variables[std::string(qname)] = std::move(variable);
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
