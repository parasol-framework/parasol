// XPath Function Library and Value System
//
// This file contains:
// - XPath value types and conversions (Phase 3 of AST_PLAN.md)
// - Function library for XPath 1.0 functions
// - Context management for function evaluation

#pragma once

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
   bool boolean_value = false;
   double number_value = 0.0;
   std::string string_value;

   // Constructors
   XPathValue() : type(XPathValueType::Boolean) {}
   explicit XPathValue(bool value) : type(XPathValueType::Boolean), boolean_value(value) {}
   explicit XPathValue(double value) : type(XPathValueType::Number), number_value(value) {}
   explicit XPathValue(std::string value) : type(XPathValueType::String), string_value(std::move(value)) {}
   explicit XPathValue(const std::vector<XMLTag *> &Nodes) : type(XPathValueType::NodeSet), node_set(Nodes) {}

   // Type conversions
   bool to_boolean() const;
   double to_number() const;
   std::string to_string() const;
   std::vector<XMLTag *> to_node_set() const;

   // Utility methods
   bool is_empty() const;
   size_t size() const;
};

//********************************************************************************************************************
// XPath Evaluation Context

struct XPathContext {
   XMLTag * context_node = nullptr;
   size_t position = 1;
   size_t size = 1;
   std::map<std::string, XPathValue> variables;

   XPathContext() = default;
   XPathContext(XMLTag *Node, size_t Pos = 1, size_t Sz = 1)
      : context_node(Node), position(Pos), size(Sz) {}
};

//********************************************************************************************************************
// XPath Function Library

using XPathFunction = std::function<XPathValue(const std::vector<XPathValue> &, const XPathContext &)>;

class XPathFunctionLibrary {
   private:
   std::map<std::string, XPathFunction> functions;
   void register_core_functions();

   public:
   XPathFunctionLibrary();
   ~XPathFunctionLibrary() = default;

   bool has_function(const std::string &Name) const;
   XPathValue call_function(const std::string &Name, const std::vector<XPathValue> &Args, const XPathContext &Context) const;
   void register_function(const std::string &Name, XPathFunction Func);

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