#pragma once

#include <ankerl/unordered_dense.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

struct XPathNode;
class extXML;
class XPathErrorReporter;

struct DecimalFormat
{
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

struct XQueryFunction
{
   std::string qname;
   std::vector<std::string> parameter_names;
   std::vector<std::string> parameter_types;
   std::optional<std::string> return_type;
   std::unique_ptr<XPathNode> body;
   bool is_external = false;

   [[nodiscard]] std::string signature() const;
};

struct XQueryVariable
{
   std::string qname;
   std::unique_ptr<XPathNode> initializer;
   bool is_external = false;
};

struct XQueryModuleImport
{
   std::string target_namespace;
   std::vector<std::string> location_hints;
};

struct XQueryModuleCache
{
   std::shared_ptr<extXML> owner;
   mutable ankerl::unordered_dense::map<std::string, std::shared_ptr<extXML>> modules;

   [[nodiscard]] std::shared_ptr<extXML> fetch_or_load(std::string_view uri,
      const struct XQueryProlog &prolog, XPathErrorReporter &reporter) const;
};

struct XQueryProlog
{
   XQueryProlog();

   ankerl::unordered_dense::map<std::string, uint32_t> declared_namespaces;
   ankerl::unordered_dense::map<std::string, std::string> declared_namespace_uris;
   std::optional<uint32_t> default_element_namespace;
   std::optional<uint32_t> default_function_namespace;
   std::optional<std::string> default_element_namespace_uri;
   std::optional<std::string> default_function_namespace_uri;

   ankerl::unordered_dense::map<std::string, XQueryVariable> variables;
   ankerl::unordered_dense::map<std::string, XQueryFunction> functions;
   std::vector<XQueryModuleImport> module_imports;

   std::string static_base_uri;
   std::string default_collation;

   enum class BoundarySpace { Preserve, Strip } boundary_space = BoundarySpace::Strip;
   enum class ConstructionMode { Preserve, Strip } construction_mode = ConstructionMode::Strip;
   enum class OrderingMode { Ordered, Unordered } ordering_mode = OrderingMode::Ordered;
   enum class EmptyOrder { Greatest, Least } empty_order = EmptyOrder::Greatest;

   struct CopyNamespaces
   {
      bool preserve = true;
      bool inherit = true;
   } copy_namespaces;

   ankerl::unordered_dense::map<std::string, DecimalFormat> decimal_formats;
   ankerl::unordered_dense::map<std::string, std::string> options;

   void declare_namespace(std::string_view prefix, std::string_view uri, extXML *document);
   void declare_variable(std::string_view qname, XQueryVariable variable);
   void declare_function(XQueryFunction function);

   [[nodiscard]] const XQueryFunction * find_function(std::string_view qname, size_t arity) const;
   [[nodiscard]] const XQueryVariable * find_variable(std::string_view qname) const;
   [[nodiscard]] uint32_t resolve_prefix(std::string_view prefix, const extXML *document) const;
   [[nodiscard]] std::string normalise_function_qname(std::string_view qname, const extXML *document) const;

   void bind_module_cache(std::shared_ptr<XQueryModuleCache> cache);
   [[nodiscard]] std::shared_ptr<XQueryModuleCache> get_module_cache() const;

   private:
   std::weak_ptr<XQueryModuleCache> module_cache;
};
