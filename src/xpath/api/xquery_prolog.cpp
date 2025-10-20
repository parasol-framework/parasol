
#include "xquery_prolog.h"
#include "xpath_errors.h"
#include <parasol/strings.hpp>
#include <parasol/modules/xpath.h>
#include <utility>
#include <format>
#include <optional>
#include <algorithm>
#include "../xpath.h"
#include "../functions/accessor_support.h"
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

XPathNode * XQueryModuleCache::fetch_or_load(std::string_view URI, const XQueryProlog &prolog, 
   XPathErrorReporter &reporter) const
{
   pf::Log log(__FUNCTION__);

   log.branch("URI: %.*s", int(URI.size()), URI.data());

   if (URI.empty()) return nullptr;

   if (not owner) {
      reporter.record_error("XQST0059: Cannot load module without a pre-existing XML object: " + std::string(URI));
      return nullptr;
   }
   
   pf::ScopedObjectLock<extXML> xml(owner);
   if (not xml.granted()) {
      reporter.record_error(std::format("XQST0059: Cannot lock XML object #{} for module loading: {}", xml->UID, std::string(URI)));
      return nullptr;
   }

   pf::SwitchContext ctx(*xml);

   auto normalise_cache_key = [](const std::string &value) -> std::string {
      return xml::uri::normalise_uri_separators(value);
   };

   auto strip_file_scheme = [](const std::string &value) -> std::string {
      if (!value.rfind("file:", 0)) {
         std::string stripped = value.substr(5);
         if (!stripped.rfind("//", 0)) stripped = stripped.substr(2);
         return stripped;
      }
      return value;
   };

   auto base_directory = xpath::accessor::resolve_document_base_directory(*xml);

   auto resolve_hint_to_path = [&](const std::string &hint) -> std::string {
      std::string normalised = normalise_cache_key(hint);
      if (normalised.empty()) return std::string();

      if (xml::uri::is_absolute_uri(normalised)) {
         if (!normalised.rfind("file:", 0)) {
            return normalise_cache_key(strip_file_scheme(normalised));
         }
         return std::string();
      }

      if (!prolog.static_base_uri.empty()) {
         std::string resolved = xml::uri::resolve_relative_uri(normalised, prolog.static_base_uri);
         if (!resolved.rfind("file:", 0)) {
            return normalise_cache_key(strip_file_scheme(resolved));
         }
         if (!xml::uri::is_absolute_uri(resolved)) return normalise_cache_key(resolved);
      }

      if (base_directory) {
         std::string combined = *base_directory;
         combined.append(normalised);
         return normalise_cache_key(combined);
      }

      return normalised;
   };

   auto uri_key = normalise_cache_key(std::string(URI));
   std::string original_uri(URI);

   // Check if already loaded

   auto existing = modules.find(uri_key);
   if (existing != modules.end()) return existing->second.compiled_query;

   // Detect circular dependencies

   if (loading_in_progress.contains(uri_key)) {
      reporter.record_error("XQDY0054: Circular module dependency detected: " + uri_key);
      return nullptr;
   }

   // Find matching import declaration

   const XQueryModuleImport *import_decl = nullptr;
   for (const auto &imp : prolog.module_imports) {
      std::string normalised_namespace = normalise_cache_key(imp.target_namespace);
      if ((normalised_namespace IS uri_key) or (imp.target_namespace IS original_uri)) {
         import_decl = &imp;
         break;
      }
   }

   if (not import_decl) {
      reporter.record_error("XQST0059: No import declaration found for: " + uri_key);
      return nullptr;
   }
   
   std::vector<std::string> location_candidates;
   if (import_decl) {
      for (const auto &hint : import_decl->location_hints) {
         std::string candidate = resolve_hint_to_path(hint);
         if (candidate.empty()) continue;

         bool duplicate = false;
         for (const auto &existing_path : location_candidates) {
            if (existing_path IS candidate) {
               duplicate = true;
               break;
            }
         }

         if (!duplicate) location_candidates.push_back(candidate);
      }
   }

   // Check document cache for pre-loaded modules

   auto find_module = [&](const std::string &key) -> XPathNode * {
      auto cache_entry = xml->ModuleCache.find(key);
      if (cache_entry != xml->ModuleCache.end()) return cache_entry->second;
      else return nullptr;
   };

   if (auto cached = find_module(uri_key)) return cached;
   if (original_uri != uri_key) {
      if (auto cached = find_module(original_uri)) return cached;
   }
   for (const auto &candidate : location_candidates) {
      if (auto cached = find_module(candidate)) return cached;
   }

   // Mark as loading to detect recursion

   loading_in_progress.insert(uri_key);

   auto cleanup = pf::Defer([&]() {
      loading_in_progress.erase(uri_key);
   });

   // Load file content

   if (location_candidates.empty()) location_candidates.push_back(uri_key);

   std::string *content = nullptr;
   std::string loaded_location;
   const std::optional<std::string> encoding("utf-8");

   for (const auto &candidate : location_candidates) {
      if (read_text_resource(*xml, candidate, encoding, content)) {
         loaded_location = candidate;
         break;
      }
   }

   if (!content) {
      std::string attempted;
      for (size_t index = 0; index < location_candidates.size(); ++index) {
         if (index > 0) attempted.append(", ");
         attempted.append(location_candidates[index]);
      }

      reporter.record_error(std::format("XQST0059: Cannot load module for namespace {} (attempted: {})", uri_key, attempted));
      return nullptr;
   }

   // Compile the module query
   XPathNode *compiled = nullptr;
   if (xp::Compile((objXML *)*xml, content->c_str(), (APTR*)&compiled) != ERR::Okay) {
      reporter.record_error(std::format("Cannot compile module: {}", URI));
      return nullptr;
   }

   // Verify that it's a library module

   auto module_prolog = compiled->get_prolog();
   if ((not module_prolog) or (not module_prolog->is_library_module)) {
      FreeResource(compiled);
      reporter.record_error(std::format("Module is not a library module: {}", uri_key));
      return nullptr;
   }

   // Validate namespace matches

   if (module_prolog->module_namespace_uri != uri_key) {
      FreeResource(compiled);
      reporter.record_error(std::format("Module namespace mismatch: expected {}", uri_key));
      return nullptr;
   }

   // Validate exports

   if (!module_prolog->validate_library_exports()) {
      FreeResource(compiled);
      reporter.record_error(std::format("Module exports not in target namespace: {}", uri_key));
      return nullptr;
   }

   // Cache the module

   modules[uri_key] = { .compiled_query = compiled, .prolog = module_prolog };
   xml->ModuleCache[uri_key] = compiled;
   if (original_uri != uri_key) xml->ModuleCache[original_uri] = compiled;
   if (!loaded_location.empty()) xml->ModuleCache[loaded_location] = compiled;

   return compiled;
}

//********************************************************************************************************************

const XQueryModuleCache::ModuleInfo * XQueryModuleCache::find_module(std::string_view uri) const
{
   std::string original(uri);
   std::string uri_key = xml::uri::normalise_uri_separators(original);
   auto existing = modules.find(uri_key);
   if (existing != modules.end()) return &existing->second;
   if (uri_key != original) {
      auto fallback = modules.find(original);
      if (fallback != modules.end()) return &fallback->second;
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

//********************************************************************************************************************

bool XQueryProlog::validate_library_exports() const
{
   return validate_library_exports_detailed().valid;
}

//********************************************************************************************************************

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

std::string XQueryProlog::normalise_function_qname(std::string_view qname, const XPathNode *Node) const
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
      std::string prefix(qname.substr(0, colon));
      std::string_view local_view = qname.substr(colon + 1U);

      auto uri_entry = declared_namespace_uris.find(prefix);
      if (uri_entry != declared_namespace_uris.end()) {
         return build_expanded(uri_entry->second, local_view);
      }

      if (Node) {
         if (auto other_prolog = Node->get_prolog()) {
            auto uri_entry2 = other_prolog->declared_namespace_uris.find(prefix);
            if (uri_entry2 != other_prolog->declared_namespace_uris.end()) {
               return build_expanded(uri_entry2->second, local_view);
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
