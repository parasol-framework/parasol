
#include "xquery_prolog.h"
#include "xpath_errors.h"
#include <parasol/strings.hpp>
#include <utility>
#include "../xpath.h"
#include "../../xml/xml.h"
#include "../../xml/uri_utils.h"

//********************************************************************************************************************
// Builds a canonical identifier combining the QName and arity so functions can be stored in a flat map.

[[nodiscard]] inline std::string build_function_signature(std::string_view QName, size_t Arity) {
   std::string signature;
   signature.reserve(QName.size() + 16U);
   signature.append(QName);
   signature.push_back('/');
   signature.append(std::to_string(Arity));
   return signature;
}

//********************************************************************************************************************
// Initialises the prolog defaults so that standard collations and decimal format entries are always present.

XQueryProlog::XQueryProlog()
{
   default_collation = "http://www.w3.org/2005/xpath-functions/collation/codepoint";
   decimal_formats[std::string()] = DecimalFormat();
}

//********************************************************************************************************************
// Returns the canonical signature text used to register the function in the prolog lookup table.

std::string XQueryFunction::signature() const
{
   return build_function_signature(qname, parameter_names.size());
}

//********************************************************************************************************************
// Attempts to locate a compiled module for the supplied URI, optionally consulting the owning document cache.
//
// When module loading is fully implemented (Phases A-C), this function should:
// 1. Check if module is already loaded (current functionality)
// 2. Check for circular dependencies using loading_in_progress set
//    - Use xquery::errors::circular_module_dependency() for XQDY0054
// 3. Resolve module location from import hints
//    - Use xquery::errors::module_not_found() for XQST0059 if no valid location
//    - Use xquery::errors::module_location_not_found() if specific location fails
// 4. Load and compile the module file
// 5. Validate it's a library module
//    - Use xquery::errors::not_library_module() for XQST0059
// 6. Validate namespace matches
//    - Use xquery::errors::namespace_mismatch() for XQST0059
// 7. Validate exports using validate_library_exports_detailed()
//    - Use result.error_message for XQST0048
// 8. Cache the successfully loaded module

extXML * XQueryModuleCache::fetch_or_load(std::string_view uri, const XQueryProlog &prolog, XPathErrorReporter &reporter) const
{
   if (uri.empty()) return nullptr;

   std::string uri_key = xml::uri::normalise_uri_separators(std::string(uri));

   // Step 1: Check if already loaded
   auto existing = modules.find(uri_key);
   if (existing not_eq modules.end()) return existing->second.document;

   // Check document cache for pre-loaded modules
   if (owner) {
      pf::ScopedObjectLock<extXML> xml(owner);
      if (xml.granted()) {
         auto capture_entry = [&](const std::string &key) -> extXML * {
            auto cache_entry = xml->DocumentCache.find(key);
            if (cache_entry not_eq xml->DocumentCache.end()) {
               ModuleInfo info;
               info.document = cache_entry->second;
               modules.insert_or_assign(key, std::move(info));
               return cache_entry->second;
            }
            return nullptr;
         };

         if (auto cached = capture_entry(uri_key)) return cached;

         std::string original(uri);
         if (original not_eq uri_key) {
            if (auto cached = capture_entry(original)) return cached;
         }
      }
   }

   // TODO: Implement full module loading when Phases A-C are complete:
   // - Circular dependency detection (XQDY0054)
   // - Location resolution and file loading (XQST0059)
   // - Library module validation (XQST0059, XQST0048)
   // See error code helpers in xpath_errors.h

   return nullptr;
}

const XQueryModuleCache::ModuleInfo * XQueryModuleCache::find_module(std::string_view uri) const
{
   std::string original(uri);
   std::string uri_key = xml::uri::normalise_uri_separators(original);
   auto existing = modules.find(uri_key);
   if (existing not_eq modules.end()) return &existing->second;
   if (uri_key not_eq original) {
      auto fallback = modules.find(original);
      if (fallback not_eq modules.end()) return &fallback->second;
   }
   return nullptr;
}

//********************************************************************************************************************
// Performs a lookup for a user-defined function using the generated signature key.

const XQueryFunction * XQueryProlog::find_function(std::string_view qname, size_t arity) const
{
   auto signature = build_function_signature(qname, arity);
   auto entry = functions.find(signature);
   if (entry != functions.end()) return &entry->second;
   return nullptr;
}

//********************************************************************************************************************
// Retrieves a declared variable definition by its canonical QName string.

const XQueryVariable * XQueryProlog::find_variable(std::string_view qname) const
{
   auto entry = variables.find(std::string(qname));
   if (entry != variables.end()) return &entry->second;
   return nullptr;
}

//********************************************************************************************************************
// Resolves a namespace prefix against the prolog declarations, falling back to the document bindings when required.

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

//********************************************************************************************************************
// Records a namespace binding inside the prolog and optionally mirrors it into the backing document.  Rejects duplicates.

bool XQueryProlog::declare_namespace(std::string_view prefix, std::string_view uri, extXML *document)
{
   std::string cleaned = xml::uri::normalise_uri_separators(std::string(uri));
   auto hash = pf::strhash(cleaned);
   std::string prefix_key(prefix);
   auto inserted = declared_namespaces.emplace(prefix_key, hash);
   if (not inserted.second) return false;
   declared_namespace_uris[prefix_key] = cleaned;

   if (document) {
      document->NSRegistry[hash] = cleaned;
      document->Prefixes[prefix_key] = hash;
   }
   return true;
}

//********************************************************************************************************************
// Stores a variable declaration, ensuring the original QName is preserved as the map key.  Rejects duplicates.

bool XQueryProlog::declare_variable(std::string_view qname, XQueryVariable variable)
{
   std::string key(qname);
   auto inserted = variables.emplace(key, std::move(variable));
   if (not inserted.second) return false;
   inserted.first->second.qname = key;
   return true;
}

//********************************************************************************************************************
// Inserts a function declaration using the computed signature as the lookup handle.  Rejects duplicates.

bool XQueryProlog::declare_function(XQueryFunction function)
{
   auto signature = function.signature();
   auto inserted = functions.emplace(std::move(signature), std::move(function));
   if (not inserted.second) return false;
   return true;
}

//********************************************************************************************************************
// Records a module import declaration, ensuring no duplicate imports for the same namespace (XQST0047).

bool XQueryProlog::declare_module_import(XQueryModuleImport import_decl, std::string *error_message)
{
   std::string namespace_key = xml::uri::normalise_uri_separators(import_decl.target_namespace);

   // Check for duplicate module imports (XQST0047)
   for (const auto &existing : module_imports) {
      std::string existing_key = xml::uri::normalise_uri_separators(existing.target_namespace);
      if (existing_key IS namespace_key) {
         if (error_message) {
            *error_message = xquery::errors::duplicate_module_import(namespace_key);
         }
         return false;
      }
   }

   module_imports.push_back(std::move(import_decl));
   return true;
}

bool XQueryProlog::validate_library_exports() const
{
   return validate_library_exports_detailed().valid;
}

XQueryProlog::ExportValidationResult XQueryProlog::validate_library_exports_detailed() const
{
   ExportValidationResult result;

   if (not is_library_module) {
      result.valid = true;
      return result;
   }

   if (not module_namespace_uri) {
      result.valid = false;
      result.error_message = "Library module is missing namespace URI declaration";
      return result;
   }

   uint32_t module_hash = pf::strhash(*module_namespace_uri);

   auto matches_namespace = [&](std::string_view qname) -> bool {
      if (qname.empty()) return false;

      // Q{uri}local format
      if ((qname.size() > 2U) and (qname[0] IS 'Q') and (qname[1] IS '{')) {
         size_t closing = qname.find('}');
         if (closing IS std::string::npos) return false;
         std::string_view uri_view = qname.substr(2U, closing - 2U);
         return uri_view IS std::string_view(*module_namespace_uri);
      }

      // prefix:local format
      size_t colon = qname.find(':');
      if (colon IS std::string::npos) return false;
      std::string_view prefix = qname.substr(0U, colon);
      uint32_t prefix_hash = resolve_prefix(prefix, nullptr);
      if (prefix_hash IS 0U) return false;
      return prefix_hash IS module_hash;
   };

   // Validate all functions are in the module namespace (XQST0048)
   for (const auto &entry : functions) {
      if (not matches_namespace(entry.second.qname)) {
         result.valid = false;
         result.problematic_qname = entry.second.qname;
         result.is_function = true;
         result.error_message = xquery::errors::export_not_in_namespace(
            "Function", entry.second.qname, *module_namespace_uri);
         return result;
      }
   }

   // Validate all variables are in the module namespace (XQST0048)
   for (const auto &entry : variables) {
      if (not matches_namespace(entry.second.qname)) {
         result.valid = false;
         result.problematic_qname = entry.second.qname;
         result.is_function = false;
         result.error_message = xquery::errors::export_not_in_namespace(
            "Variable", entry.second.qname, *module_namespace_uri);
         return result;
      }
   }

   result.valid = true;
   return result;
}

//********************************************************************************************************************
// Associates a module cache with the prolog so evaluators can reuse loaded modules.

void XQueryProlog::bind_module_cache(std::shared_ptr<XQueryModuleCache> cache)
{
   module_cache = cache;
}

//********************************************************************************************************************
// Returns the active module cache if one has been attached to the prolog.

std::shared_ptr<XQueryModuleCache> XQueryProlog::get_module_cache() const
{
   return module_cache.lock();
}

//********************************************************************************************************************
// Normalises a function QName using the prolog and document namespace tables to produce the canonical expanded form.

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
   if (colon != std::string_view::npos) {
      std::string prefix(qname.substr(0U, colon));
      std::string_view local_view = qname.substr(colon + 1U);

      auto uri_entry = declared_namespace_uris.find(prefix);
      if (uri_entry not_eq declared_namespace_uris.end()) {
         return build_expanded(uri_entry->second, local_view);
      }

      if (document) {
         auto prefix_it = document->Prefixes.find(prefix);
         if (prefix_it not_eq document->Prefixes.end()) {
            auto ns_it = document->NSRegistry.find(prefix_it->second);
            if (ns_it not_eq document->NSRegistry.end()) {
               return build_expanded(ns_it->second, local_view);
            }
         }
      }

      return std::string(qname);
   }

   if (default_function_namespace_uri.has_value()) {
      return build_expanded(*default_function_namespace_uri, qname);
   }

   if (default_function_namespace.has_value()) {
      std::string expanded("Q{");
      expanded.append(std::to_string(*default_function_namespace));
      expanded.push_back('}');
      expanded.append(qname);
      return expanded;
   }

   return std::string(qname);
}
