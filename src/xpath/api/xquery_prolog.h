#pragma once

#include <parasol/main.h>
#include <ankerl/unordered_dense.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <unordered_set>

struct XPathNode;
class extXML;
struct XQueryProlog;

class XPathErrorReporter {
   public:
   virtual ~XPathErrorReporter() = default;
   virtual void record_error(std::string_view Message, bool Force = false) = 0;
   virtual void record_error(std::string_view Message, const XPathNode *Node, bool Force = false) = 0;
};

// Represents a user-defined decimal format declared in the prolog.

struct DecimalFormat {
   std::string name;
   std::string decimal_separator = ".";
   std::string grouping_separator = ",";
   std::string infinity = "INF";
   std::string minus_sign = "-";
   std::string nan = "NaN";
   std::string percent = "%";
   std::string per_mille = "‰";
   std::string zero_digit = "0";
   std::string digit = "#";
   std::string pattern_separator = ";";
};

// Represents a user-defined XQuery function declared in the prolog.

struct XQueryFunction {
   std::string qname;
   std::vector<std::string> parameter_names;
   std::vector<std::string> parameter_types;
   std::optional<std::string> return_type;
   std::unique_ptr<XPathNode> body;
   bool is_external = false;

   [[nodiscard]] std::string signature() const;
};

// Represents a user-defined XQuery variable declared in the prolog.

struct XQueryVariable {
   std::string qname;
   std::unique_ptr<XPathNode> initializer;
   bool is_external = false;
};

// Represents an XQuery module import declaration.

struct XQueryModuleImport {
   std::string target_namespace;
   std::vector<std::string> location_hints;
};

//********************************************************************************************************************
// Utilised by XPathNode to cache imported modules.

using XMODULE = XPathNode;

struct XQueryModuleCache {
   // Referenced as a UID from xp::Compile() because it's a weak reference.
   // Used by fetch_or_load() primarily to determine the origin path of the XML data.
   OBJECTID owner = 0;
   mutable ankerl::unordered_dense::map<std::string, XMODULE *> modules;
   mutable std::unordered_set<std::string> loading_in_progress;

   [[nodiscard]] XMODULE * fetch_or_load(std::string_view, const struct XQueryProlog &, XPathErrorReporter &) const;
   [[nodiscard]] const XMODULE * find_module(std::string_view uri) const;
};

//********************************************************************************************************************
// If an XQuery expression contains a prolog, it will be parsed into this structure and maintained in the XPathNode
// prolog field.

struct XQueryProlog {
   XQueryProlog();

   struct CopyNamespaces {
      bool preserve = true;
      bool inherit = true;
   } copy_namespaces;

   struct ExportValidationResult {
      bool valid = true;
      std::string error_message;
      std::string problematic_qname;
      bool is_function = false;  // true if problematic item is a function, false if variable
   };

   enum class BoundarySpace { Preserve, Strip } boundary_space = BoundarySpace::Strip;
   enum class ConstructionMode { Preserve, Strip } construction_mode = ConstructionMode::Strip;
   enum class OrderingMode { Ordered, Unordered } ordering_mode = OrderingMode::Ordered;
   enum class EmptyOrder { Greatest, Least } empty_order = EmptyOrder::Greatest;

   ankerl::unordered_dense::map<std::string, uint32_t> declared_namespaces;
   ankerl::unordered_dense::map<std::string, std::string> declared_namespace_uris;
   ankerl::unordered_dense::map<std::string, XQueryVariable> variables;
   ankerl::unordered_dense::map<std::string, XQueryFunction> functions;
   ankerl::unordered_dense::map<std::string, DecimalFormat> decimal_formats;
   ankerl::unordered_dense::map<std::string, std::string> options;

   std::vector<XQueryModuleImport> module_imports;

   std::optional<uint32_t> default_element_namespace;
   std::optional<uint32_t> default_function_namespace;
   std::optional<std::string> default_element_namespace_uri;
   std::optional<std::string> default_function_namespace_uri;
   std::optional<std::string> module_namespace_uri;
   std::optional<std::string> module_namespace_prefix;

   std::string static_base_uri;
   std::string default_collation;

   bool is_library_module = false;
   bool static_base_uri_declared = false;
   bool default_collation_declared = false;
   bool boundary_space_declared = false;
   bool construction_declared = false;
   bool ordering_declared = false;
   bool empty_order_declared = false;
   bool copy_namespaces_declared = false;
   bool default_decimal_format_declared = false;

   bool declare_namespace(std::string_view prefix, std::string_view uri, extXML *document);
   bool declare_variable(std::string_view qname, XQueryVariable variable);
   bool declare_function(XQueryFunction function);
   bool declare_module_import(XQueryModuleImport import_decl, std::string *error_message = nullptr);
   [[nodiscard]] bool validate_library_exports() const;
   [[nodiscard]] ExportValidationResult validate_library_exports_detailed() const;
   [[nodiscard]] const XQueryFunction * find_function(std::string_view qname, size_t arity) const;
   [[nodiscard]] const XQueryVariable * find_variable(std::string_view qname) const;
   [[nodiscard]] uint32_t resolve_prefix(std::string_view prefix, const extXML *document) const;
   [[nodiscard]] std::string normalise_function_qname(std::string_view qname, const XPathNode *document = nullptr) const;
   void bind_module_cache(std::shared_ptr<XQueryModuleCache> cache);
   [[nodiscard]] std::shared_ptr<XQueryModuleCache> get_module_cache() const;

   private:
   std::weak_ptr<XQueryModuleCache> module_cache;
};
