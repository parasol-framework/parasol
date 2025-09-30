// XPath Function Library and Value System
//
// This file contains:
// - XPath value types and conversions (Phase 3 of AST_PLAN.md)
// - Function library for XPath 1.0 functions
// - Context management for function evaluation

#pragma once

#include <parasol/main.h>
#include <ankerl/unordered_dense.h>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct TransparentStringHash {
   using is_transparent = void;

   size_t operator()(std::string_view Value) const noexcept { return std::hash<std::string_view>{}(Value); }
   size_t operator()(const std::string &Value) const noexcept { return operator()(std::string_view(Value)); }
   size_t operator()(const char *Value) const noexcept { return operator()(std::string_view(Value)); }
};

struct TransparentStringEqual {
   using is_transparent = void;

   bool operator()(std::string_view Lhs, std::string_view Rhs) const noexcept { return Lhs IS Rhs; }
   bool operator()(const std::string &Lhs, const std::string &Rhs) const noexcept { return std::string_view(Lhs) IS std::string_view(Rhs); }
   bool operator()(const char *Lhs, const char *Rhs) const noexcept { return std::string_view(Lhs) IS std::string_view(Rhs); }
   bool operator()(const std::string &Lhs, std::string_view Rhs) const noexcept { return std::string_view(Lhs) IS Rhs; }
   bool operator()(std::string_view Lhs, const std::string &Rhs) const noexcept { return Lhs IS std::string_view(Rhs); }
   bool operator()(const char *Lhs, std::string_view Rhs) const noexcept { return std::string_view(Lhs) IS Rhs; }
   bool operator()(std::string_view Lhs, const char *Rhs) const noexcept { return Lhs IS std::string_view(Rhs); }
};

struct XMLTag;
struct XMLAttrib;
class extXML;

//********************************************************************************************************************
// XPath Value System

namespace xml::schema
{
   enum class SchemaType;
   class SchemaTypeDescriptor;
}

enum class XPathValueType {
   NodeSet,
   Boolean,
   Number,
   String,
   Date,
   Time,
   DateTime
};

class XPathValue 
{
   public:
   XPathValueType type;
   std::vector<XMLTag *> node_set;
   std::optional<std::string> node_set_string_override;
   std::vector<std::string> node_set_string_values;
   std::vector<const XMLAttrib *> node_set_attributes;
   bool boolean_value = false;
   double number_value = 0.0;
   std::string string_value;
   mutable std::shared_ptr<xml::schema::SchemaTypeDescriptor> schema_type_info;
   mutable bool schema_validated = false;

   // Constructors
   XPathValue() : type(XPathValueType::Boolean) {}
   explicit XPathValue(bool value) : type(XPathValueType::Boolean), boolean_value(value) {}
   explicit XPathValue(double value) : type(XPathValueType::Number), number_value(value) {}
   explicit XPathValue(std::string value) : type(XPathValueType::String), string_value(std::move(value)) {}
   explicit XPathValue(XPathValueType ValueType, std::string value)
      : type(ValueType), string_value(std::move(value)) {}
   explicit XPathValue(const std::vector<XMLTag *> &Nodes,
                       std::optional<std::string> NodeSetString = std::nullopt,
                       std::vector<std::string> NodeSetStrings = {},
                       std::vector<const XMLAttrib *> NodeSetAttributes = {})
      : type(XPathValueType::NodeSet),
        node_set(Nodes),
        node_set_string_override(std::move(NodeSetString)),
        node_set_string_values(std::move(NodeSetStrings)),
        node_set_attributes(std::move(NodeSetAttributes)) {}

   // Type conversions
   bool to_boolean() const;
   double to_number() const;
   std::string to_string() const;
   std::vector<XMLTag *> to_node_set() const;

   // Utility methods
   bool is_empty() const;
   size_t size() const;

   // Schema metadata helpers
   bool has_schema_info() const;
   void set_schema_type(std::shared_ptr<xml::schema::SchemaTypeDescriptor> TypeInfo);
   bool validate_against_schema() const;
   xml::schema::SchemaType get_schema_type() const;

   // Helpers exposed for evaluator utilities
   static std::string node_string_value(XMLTag *Node);
   static double string_to_number(const std::string &Value);
};

//********************************************************************************************************************
// XPath Evaluation Context

namespace xml::schema
{
   class SchemaTypeRegistry;
   SchemaTypeRegistry & registry();
}

struct XPathContext
{
   XMLTag * context_node = nullptr;
   const XMLAttrib * attribute_node = nullptr;
   size_t position = 1;
   size_t size = 1;
   ankerl::unordered_dense::map<std::string, XPathValue> variables;
   extXML * document = nullptr;
   bool * expression_unsupported = nullptr;
   xml::schema::SchemaTypeRegistry * schema_registry = nullptr;

   XPathContext() = default;
   XPathContext(XMLTag *Node, size_t cursor = 1, size_t Sz = 1, const XMLAttrib *Attribute = nullptr,
                extXML *Document = nullptr, bool *UnsupportedFlag = nullptr,
                xml::schema::SchemaTypeRegistry *Registry = nullptr)
      : context_node(Node), attribute_node(Attribute), position(cursor), size(Sz), document(Document),
        expression_unsupported(UnsupportedFlag), schema_registry(Registry) {}
};

class VariableBindingGuard
{
   private:
   XPathContext & context;
   std::string variable_name;
   std::optional<XPathValue> previous_value;
   bool had_previous_value = false;

   public:
   VariableBindingGuard(XPathContext &Context, std::string Name, XPathValue Value)
      : context(Context), variable_name(std::move(Name))
   {
      auto existing = context.variables.find(variable_name);
      had_previous_value = (existing != context.variables.end());
      if (had_previous_value) previous_value = existing->second;

      context.variables[variable_name] = std::move(Value);
   }

   ~VariableBindingGuard()
   {
      if (had_previous_value) context.variables[variable_name] = *previous_value;
      else context.variables.erase(variable_name);
   }

   VariableBindingGuard(const VariableBindingGuard &) = delete;
   VariableBindingGuard & operator=(const VariableBindingGuard &) = delete;

   VariableBindingGuard(VariableBindingGuard &&) = default;
   VariableBindingGuard & operator=(VariableBindingGuard &&) = default;
};

//********************************************************************************************************************
// XPath Function Library

using XPathFunction = std::function<XPathValue(const std::vector<XPathValue> &, const XPathContext &)>;

class XPathFunctionLibrary {
   private:
   ankerl::unordered_dense::map<std::string, XPathFunction, TransparentStringHash, TransparentStringEqual> functions;
   void register_core_functions();
   const XPathFunction * find_function(std::string_view Name) const;

   // Size estimation helpers for string operations
   static size_t estimate_concat_size(const std::vector<XPathValue> &Args);
   static size_t estimate_normalize_space_size(const std::string &Input);
   static size_t estimate_translate_size(const std::string &Source, const std::string &From);

   public:
   XPathFunctionLibrary();
   ~XPathFunctionLibrary() = default;

   bool has_function(std::string_view Name) const;
   XPathValue call_function(std::string_view Name, const std::vector<XPathValue> &Args, const XPathContext &Context) const;
   void register_function(std::string_view Name, XPathFunction Func);

   // Core XPath 1.0 functions
   static XPathValue function_last(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_position(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_count(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_id(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_local_name(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_namespace_uri(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_name(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_QName(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_resolve_QName(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_prefix_from_QName(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_local_name_from_QName(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_namespace_uri_from_QName(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_namespace_uri_for_prefix(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_in_scope_prefixes(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_string(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_concat(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_codepoints_to_string(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_string_to_codepoints(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_compare(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_codepoint_equal(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_starts_with(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_ends_with(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_contains(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_substring_before(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_substring_after(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_substring(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_string_length(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_normalize_space(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_normalize_unicode(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_string_join(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_iri_to_uri(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_analyze_string(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_resolve_uri(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_format_date(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_format_time(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_format_date_time(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_format_integer(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_translate(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_upper_case(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_lower_case(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_encode_for_uri(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_escape_html_uri(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_error(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_trace(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_boolean(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_not(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_true(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_false(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_lang(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_exists(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_index_of(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_empty(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_distinct_values(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_insert_before(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_remove(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_reverse(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_subsequence(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_unordered(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_deep_equal(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_zero_or_one(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_one_or_more(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_exactly_one(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_number(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_sum(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_floor(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_ceiling(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_round(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_round_half_to_even(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_abs(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_min(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_max(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_avg(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_current_date(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_current_time(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_current_date_time(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_date_time(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_year_from_date_time(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_month_from_date_time(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_day_from_date_time(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_hours_from_date_time(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_minutes_from_date_time(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_seconds_from_date_time(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_timezone_from_date_time(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_year_from_date(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_month_from_date(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_day_from_date(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_timezone_from_date(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_hours_from_time(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_minutes_from_time(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_seconds_from_time(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_timezone_from_time(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_adjust_date_time_to_timezone(const std::vector<XPathValue> &Args,
      const XPathContext &Context);
   static XPathValue function_adjust_date_to_timezone(const std::vector<XPathValue> &Args,
      const XPathContext &Context);
   static XPathValue function_adjust_time_to_timezone(const std::vector<XPathValue> &Args,
      const XPathContext &Context);
   static XPathValue function_implicit_timezone(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_years_from_duration(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_months_from_duration(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_days_from_duration(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_hours_from_duration(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_minutes_from_duration(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_seconds_from_duration(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_years_from_year_month_duration(const std::vector<XPathValue> &Args,
      const XPathContext &Context);
   static XPathValue function_months_from_year_month_duration(const std::vector<XPathValue> &Args,
      const XPathContext &Context);
   static XPathValue function_days_from_day_time_duration(const std::vector<XPathValue> &Args,
      const XPathContext &Context);
   static XPathValue function_hours_from_day_time_duration(const std::vector<XPathValue> &Args,
      const XPathContext &Context);
   static XPathValue function_minutes_from_day_time_duration(const std::vector<XPathValue> &Args,
      const XPathContext &Context);
   static XPathValue function_seconds_from_day_time_duration(const std::vector<XPathValue> &Args,
      const XPathContext &Context);
   static XPathValue function_matches(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_replace(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_tokenize(const std::vector<XPathValue> &Args, const XPathContext &Context);
};
