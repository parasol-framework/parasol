// XPath Function Library and Value System
//
// This file contains:
// - XPath value types and conversions (Phase 3 of AST_PLAN.md)
// - Function library for XPath 1.0 functions
// - Context management for function evaluation

#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
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

enum class XPathValueType {
   NodeSet,
   Boolean,
   Number,
   String
};

class XPathValue {
   public:
   XPathValueType type;
   std::vector<XMLTag *> node_set;
   std::optional<std::string> node_set_string_override;
   std::vector<std::string> node_set_string_values;
   std::vector<const XMLAttrib *> node_set_attributes;
   bool boolean_value = false;
   double number_value = 0.0;
   std::string string_value;

   // Constructors
   XPathValue() : type(XPathValueType::Boolean) {}
   explicit XPathValue(bool value) : type(XPathValueType::Boolean), boolean_value(value) {}
   explicit XPathValue(double value) : type(XPathValueType::Number), number_value(value) {}
   explicit XPathValue(std::string value) : type(XPathValueType::String), string_value(std::move(value)) {}
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

   // Helpers exposed for evaluator utilities
   static std::string node_string_value(XMLTag *Node);
   static double string_to_number(const std::string &Value);
};

//********************************************************************************************************************
// XPath Evaluation Context

struct XPathContext {
   XMLTag * context_node = nullptr;
   const XMLAttrib * attribute_node = nullptr;
   size_t position = 1;
   size_t size = 1;
   std::map<std::string, XPathValue> variables;
   extXML * document = nullptr;

   XPathContext() = default;
   XPathContext(XMLTag *Node,
                size_t Pos = 1,
                size_t Sz = 1,
                const XMLAttrib *Attribute = nullptr,
                extXML *Document = nullptr)
      : context_node(Node),
        attribute_node(Attribute),
        position(Pos),
        size(Sz),
        document(Document) {}
};

//********************************************************************************************************************
// XPath Function Library

using XPathFunction = std::function<XPathValue(const std::vector<XPathValue> &, const XPathContext &)>;

class XPathFunctionLibrary {
   private:
   std::unordered_map<std::string, XPathFunction, TransparentStringHash, TransparentStringEqual> functions;
   mutable std::unordered_map<std::string, const XPathFunction *, TransparentStringHash, TransparentStringEqual> function_cache;
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
   static XPathValue function_string(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_concat(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_starts_with(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_contains(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_substring_before(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_substring_after(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_substring(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_string_length(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_normalize_space(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_translate(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_boolean(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_not(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_true(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_false(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_lang(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_number(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_sum(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_floor(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_ceiling(const std::vector<XPathValue> &Args, const XPathContext &Context);
   static XPathValue function_round(const std::vector<XPathValue> &Args, const XPathContext &Context);
};
