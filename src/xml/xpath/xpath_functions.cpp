//********************************************************************************************************************
// XPath Function Library and Value System Implementation
//********************************************************************************************************************
//
// XPath expressions depend on a rich set of standard functions and a loosely typed value model.  This
// file provides both: XPathValue encapsulates conversions between node-sets, numbers, booleans, and
// strings, while the function registry offers implementations of the core function library required by
// the evaluator.  The code emphasises fidelity to the XPath 1.0 specification—string coercions mirror
// the spec's edge cases, numeric conversions preserve NaN semantics, and node-set operations respect
// document order guarantees enforced elsewhere in the module.
//
// The implementation is intentionally self-contained.  The evaluator interacts with XPathValue to
// manipulate intermediate results and delegates built-in function invocations to the routines defined
// below.  Keeping the behaviour consolidated here simplifies future extensions (for example, adding
// namespace-aware functions or performance-focused helpers) without polluting the evaluator with
// coercion details.

#include "xpath_functions.h"
#include "../xml.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string_view>
#include <unordered_set>

#include "../../link/regex.h"

//********************************************************************************************************************
// XPathValue Implementation

namespace {

// Format a double according to XPath 1.0 number-to-string rules.
std::string format_xpath_number(double Value)
{
   if (std::isnan(Value)) return std::string("NaN");
   if (std::isinf(Value)) return (Value > 0) ? std::string("Infinity") : std::string("-Infinity");
   if (Value IS 0.0) return std::string("0");

   std::ostringstream stream;
   stream.setf(std::ios::fmtflags(0), std::ios::floatfield);
   stream.precision(15);
   stream << Value;

   std::string result = stream.str();

   if (!result.empty() and (result[0] IS '+')) result.erase(result.begin());

   auto decimal = result.find('.');
   if (decimal != std::string::npos) {
      while ((!result.empty()) and (result.back() IS '0')) result.pop_back();
      if ((!result.empty()) and (result.back() IS '.')) result.pop_back();
   }

   return result;
}

bool is_unreserved_uri_character(unsigned char Code)
{
   if ((Code >= 'A' and Code <= 'Z') or (Code >= 'a' and Code <= 'z') or (Code >= '0' and Code <= '9')) return true;

   switch (Code) {
      case '-':
      case '_':
      case '.':
      case '!':
      case '~':
      case '*':
      case '\'':
      case '(':
      case ')':
         return true;
   }

   return false;
}

std::string encode_for_uri_impl(const std::string &Value)
{
   std::string result;
   result.reserve(Value.length() * 3);

   for (char ch : Value) {
      unsigned char code = (unsigned char)ch;
      if (is_unreserved_uri_character(code)) {
         result.push_back((char)code);
      }
      else {
         static const char hex_digits[] = "0123456789ABCDEF";
         result.push_back('%');
         result.push_back(hex_digits[(code >> 4) & 0x0F]);
         result.push_back(hex_digits[code & 0x0F]);
      }
   }

   return result;
}

void replace_all(std::string &Text, std::string_view From, std::string_view To)
{
   if (From.empty()) return;

   size_t position = 0;
   while (true) {
      position = Text.find(From, position);
      if (position IS std::string::npos) break;
      Text.replace(position, From.length(), To);
      position += To.length();
   }
}

std::string escape_html_uri_impl(const std::string &Value)
{
   std::string encoded = encode_for_uri_impl(Value);

   replace_all(encoded, "%26", "&amp;");
   replace_all(encoded, "%3C", "&lt;");
   replace_all(encoded, "%3E", "&gt;");
   replace_all(encoded, "%22", "&quot;");
   replace_all(encoded, "%27", "&apos;");

   return encoded;
}

std::string apply_string_case(const std::string &Value, bool Upper)
{
   std::string result = Value;
   std::transform(result.begin(), result.end(), result.begin(), [Upper](char Ch) {
      unsigned char code = (unsigned char)Ch;
      return Upper ? (char)std::toupper(code) : (char)std::tolower(code);
   });
   return result;
}

pf::SyntaxOptions build_regex_options(const std::string &Flags, bool *UnsupportedFlag)
{
   pf::SyntaxOptions options = pf::SyntaxECMAScript;

   for (char flag : Flags) {
      unsigned char code = (unsigned char)flag;
      char normalised = (char)std::tolower(code);

      if (normalised IS 'i') options |= pf::SyntaxIgnoreCase;
      else if (normalised IS 'm') options |= pf::SyntaxMultiline;
      else if (normalised IS 's') options |= pf::SyntaxDotAll;
      else if (normalised IS 'u') options |= pf::SyntaxUnicodeSets;
      else if (normalised IS 'y') options |= pf::SyntaxSticky;
      else if (normalised IS 'q') options |= pf::SyntaxQuiet;
      else if (normalised IS 'v') options |= pf::SyntaxVerboseMode;
      else if (UnsupportedFlag) *UnsupportedFlag = true;
   }

   return options;
}

void append_numbers_from_nodeset(const XPathValue &Value, std::vector<double> &Numbers)
{
   if (Value.node_set_string_override.has_value()) {
      double number = XPathValue::string_to_number(*Value.node_set_string_override);
      if (!std::isnan(number)) Numbers.push_back(number);
      return;
   }

   if (!Value.node_set_attributes.empty()) {
      for (const XMLAttrib *attribute : Value.node_set_attributes) {
         if (!attribute) continue;
         double number = XPathValue::string_to_number(attribute->Value);
         if (!std::isnan(number)) Numbers.push_back(number);
      }
      return;
   }

   if (!Value.node_set_string_values.empty()) {
      for (const std::string &entry : Value.node_set_string_values) {
         double number = XPathValue::string_to_number(entry);
         if (!std::isnan(number)) Numbers.push_back(number);
      }
      return;
   }

   for (XMLTag *node : Value.node_set) {
      if (!node) continue;
      std::string content = XPathValue::node_string_value(node);
      double number = XPathValue::string_to_number(content);
      if (!std::isnan(number)) Numbers.push_back(number);
   }
}

void append_numbers_from_value(const XPathValue &Value, std::vector<double> &Numbers)
{
   switch (Value.type) {
      case XPathValueType::Number:
         if (!std::isnan(Value.number_value)) Numbers.push_back(Value.number_value);
         break;
      case XPathValueType::String: {
         double number = XPathValue::string_to_number(Value.string_value);
         if (!std::isnan(number)) Numbers.push_back(number);
         break;
      }
      case XPathValueType::Boolean:
         Numbers.push_back(Value.boolean_value ? 1.0 : 0.0);
         break;
      case XPathValueType::NodeSet:
         append_numbers_from_nodeset(Value, Numbers);
         break;
   }
}

} // namespace

bool XPathValue::to_boolean() const {
   switch (type) {
      case XPathValueType::Boolean: return boolean_value;
      case XPathValueType::Number: return number_value != 0.0 and !std::isnan(number_value);
      case XPathValueType::String: return !string_value.empty();
      case XPathValueType::NodeSet: return !node_set.empty();
   }
   return false;
}

// Convert a string to a number using XPath's relaxed numeric parsing rules.
double XPathValue::string_to_number(const std::string &Value)
{
   if (Value.empty()) return std::numeric_limits<double>::quiet_NaN();

   char *end_ptr = nullptr;
   double result = std::strtod(Value.c_str(), &end_ptr);
   if ((end_ptr IS Value.c_str()) or (*end_ptr != '\0')) {
      return std::numeric_limits<double>::quiet_NaN();
   }

   return result;
}

// Obtain the string-value of a node, following XPath's definition for text and element nodes.
std::string XPathValue::node_string_value(XMLTag *Node)
{
   if (!Node) return std::string();

   if (Node->isContent()) {
      if (!Node->Children.empty()) return Node->getContent();
      if (!Node->Attribs.empty()) return Node->Attribs[0].Value;
      return std::string();
   }

   return Node->getContent();
}

double XPathValue::to_number() const {
   switch (type) {
      case XPathValueType::Boolean: return boolean_value ? 1.0 : 0.0;
      case XPathValueType::Number: return number_value;
      case XPathValueType::String: {
         return string_to_number(string_value);
      }
      case XPathValueType::NodeSet: {
         if (node_set.empty()) return std::numeric_limits<double>::quiet_NaN();
         if (node_set_string_override.has_value()) return string_to_number(*node_set_string_override);
         if (!node_set_string_values.empty()) return string_to_number(node_set_string_values[0]);
         return string_to_number(node_string_value(node_set[0]));
      }
   }
   return 0.0;
}

std::string XPathValue::to_string() const {
   switch (type) {
      case XPathValueType::Boolean: return boolean_value ? "true" : "false";
      case XPathValueType::Number: {
         return format_xpath_number(number_value);
      }
      case XPathValueType::String: return string_value;
      case XPathValueType::NodeSet: {
         if (node_set_string_override.has_value()) return *node_set_string_override;
         if (!node_set_string_values.empty()) return node_set_string_values[0];
         if (node_set.empty()) return "";

         return node_string_value(node_set[0]);
      }
   }
   return "";
}

std::vector<XMLTag *> XPathValue::to_node_set() const {
   if (type IS XPathValueType::NodeSet) {
      return node_set;
   }
   return {};
}

bool XPathValue::is_empty() const {
   switch (type) {
      case XPathValueType::Boolean: return false;
      case XPathValueType::Number: return false;
      case XPathValueType::String: return string_value.empty();
      case XPathValueType::NodeSet: return node_set.empty();
   }
   return true;
}

size_t XPathValue::size() const {
   switch (type) {
      case XPathValueType::NodeSet: return node_set.size();
      default: return is_empty() ? 0 : 1;
   }
}

namespace {

// Walk up the tree to locate a namespace declaration corresponding to the requested prefix.
std::string find_in_scope_namespace(XMLTag *Node, extXML *Document, std::string_view Prefix)
{
   XMLTag *current = Node;

   while (current) {
      for (size_t index = 1; index < current->Attribs.size(); ++index) {
         const auto &attrib = current->Attribs[index];

         if (Prefix.empty()) {
            if ((attrib.Name.length() IS 5) and (attrib.Name.compare(0, 5, "xmlns") IS 0)) return attrib.Value;
         }
         else {
            if (attrib.Name.compare(0, 6, "xmlns:") IS 0) {
               std::string declared = attrib.Name.substr(6);
               if ((declared.length() IS Prefix.size()) and (declared.compare(Prefix) IS 0)) return attrib.Value;
            }
         }
      }

      if (!Document) break;
      if (current->ParentID IS 0) break;
      current = Document->getTag(current->ParentID);
   }

   return std::string();
}

std::string find_language_for_node(XMLTag *Node, extXML *Document)
{
   XMLTag *current = Node;

   while (current) {
      for (size_t index = 1; index < current->Attribs.size(); ++index) {
         const auto &attrib = current->Attribs[index];
         if (pf::iequals(attrib.Name, "xml:lang")) return attrib.Value;
      }

      if (!Document) break;
      if (current->ParentID IS 0) break;
      current = Document->getTag(current->ParentID);
   }

   return std::string();
}

std::string lowercase_copy(const std::string &Value)
{
   std::string result = Value;
   std::transform(result.begin(), result.end(), result.begin(), [](unsigned char Ch) { return char(std::tolower(Ch)); });
   return result;
}

bool language_matches(const std::string &Candidate, const std::string &Requested)
{
   if (Requested.empty()) return false;

   std::string candidate_lower = lowercase_copy(Candidate);
   std::string requested_lower = lowercase_copy(Requested);

   if (candidate_lower.compare(0, requested_lower.length(), requested_lower) IS 0) {
      if (candidate_lower.length() IS requested_lower.length()) return true;
      if ((candidate_lower.length() > requested_lower.length()) and (candidate_lower[requested_lower.length()] IS '-')) return true;
   }

   return false;
}

} // namespace

//********************************************************************************************************************
// XPathFunctionLibrary Implementation

XPathFunctionLibrary::XPathFunctionLibrary() {
   register_core_functions();
}

void XPathFunctionLibrary::register_core_functions() {
   // Node Set Functions
   register_function("last", function_last);
   register_function("position", function_position);
   register_function("count", function_count);
   register_function("id", function_id);
   register_function("local-name", function_local_name);
   register_function("namespace-uri", function_namespace_uri);
   register_function("name", function_name);

   // String Functions
   register_function("string", function_string);
   register_function("concat", function_concat);
   register_function("starts-with", function_starts_with);
   register_function("contains", function_contains);
   register_function("substring-before", function_substring_before);
   register_function("substring-after", function_substring_after);
   register_function("substring", function_substring);
   register_function("string-length", function_string_length);
   register_function("normalize-space", function_normalize_space);
   register_function("translate", function_translate);
   register_function("upper-case", function_upper_case);
   register_function("lower-case", function_lower_case);
   register_function("encode-for-uri", function_encode_for_uri);
   register_function("escape-html-uri", function_escape_html_uri);

   register_function("matches", function_matches);
   register_function("replace", function_replace);
   register_function("tokenize", function_tokenize);

   // Boolean Functions
   register_function("boolean", function_boolean);
   register_function("not", function_not);
   register_function("true", function_true);
   register_function("false", function_false);
   register_function("lang", function_lang);

   // Number Functions
   register_function("number", function_number);
   register_function("sum", function_sum);
   register_function("floor", function_floor);
   register_function("ceiling", function_ceiling);
   register_function("round", function_round);
   register_function("abs", function_abs);
   register_function("min", function_min);
   register_function("max", function_max);
   register_function("avg", function_avg);
}

bool XPathFunctionLibrary::has_function(std::string_view Name) const {
   return find_function(Name) != nullptr;
}

XPathValue XPathFunctionLibrary::call_function(std::string_view Name, const std::vector<XPathValue> &Args,
   const XPathContext &Context) const {
   if (const auto *function_ptr = find_function(Name)) {
      return (*function_ptr)(Args, Context);
   }

   if (Context.expression_unsupported) *Context.expression_unsupported = true;

   if (Context.document) {
      if (!Context.document->ErrorMsg.empty()) Context.document->ErrorMsg.append("\n");
      Context.document->ErrorMsg.append("Unsupported XPath function: ").append(Name);
   }

   return XPathValue();
}

void XPathFunctionLibrary::register_function(std::string_view Name, XPathFunction Func) {
   functions.insert_or_assign(std::string(Name), std::move(Func));
}

const XPathFunction * XPathFunctionLibrary::find_function(std::string_view Name) const {
   auto iter = functions.find(Name);
   if (iter != functions.end()) return &iter->second;

   return nullptr;
}

//********************************************************************************************************************
// Size Estimation Helpers for String Operations

size_t XPathFunctionLibrary::estimate_concat_size(const std::vector<XPathValue> &Args) {
   size_t total = 0;
   for (const auto &arg : Args) {
      // Conservative estimate based on type
      switch (arg.type) {
         case XPathValueType::String:
            total += arg.string_value.length();
            break;
         case XPathValueType::Number:
            total += 32; // Conservative estimate for number formatting
            break;
         case XPathValueType::Boolean:
            total += 5; // "false" is longest
            break;
         case XPathValueType::NodeSet:
            if (arg.node_set_string_override.has_value()) {
               total += arg.node_set_string_override->length();
            } else if (!arg.node_set_string_values.empty()) {
               total += arg.node_set_string_values[0].length();
            } else {
               total += 64; // Conservative estimate for node content
            }
            break;
      }
   }
   return total;
}

size_t XPathFunctionLibrary::estimate_normalize_space_size(const std::string &Input) {
   // Worst case: no whitespace collapsing needed
   return Input.length();
}

size_t XPathFunctionLibrary::estimate_translate_size(const std::string &Source, const std::string &From) {
   // Best case: no characters removed, worst case: same size as source
   return Source.length();
}

//********************************************************************************************************************
// Core XPath 1.0 Function Implementations

XPathValue XPathFunctionLibrary::function_last(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   return XPathValue((double)Context.size);
}

XPathValue XPathFunctionLibrary::function_position(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   return XPathValue((double)Context.position);
}

XPathValue XPathFunctionLibrary::function_count(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   if (Args.size() != 1) return XPathValue(0.0);
   if (Args[0].type != XPathValueType::NodeSet) return XPathValue(0.0);
   return XPathValue((double)Args[0].node_set.size());
}

XPathValue XPathFunctionLibrary::function_id(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   std::vector<XMLTag *> results;

   if (Args.empty()) return XPathValue(results);

   std::unordered_set<std::string> requested_ids;

   auto add_tokens = [&](const std::string &Value) {
      size_t start = Value.find_first_not_of(" \t\r\n");
      while (start != std::string::npos) {
         size_t end = Value.find_first_of(" \t\r\n", start);
         std::string token = Value.substr(start, (end IS std::string::npos) ? std::string::npos : end - start);
         if (!token.empty()) requested_ids.insert(token);
         if (end IS std::string::npos) break;
         start = Value.find_first_not_of(" \t\r\n", end);
      }
   };

   auto collect_from_value = [&](const XPathValue &Value) {
      switch (Value.type) {
         case XPathValueType::NodeSet: {
            if (!Value.node_set_string_values.empty()) {
               for (const auto &entry : Value.node_set_string_values) add_tokens(entry);
            }
            else if (Value.node_set_string_override.has_value()) add_tokens(*Value.node_set_string_override);
            else {
               for (auto *node : Value.node_set) {
                  if (!node) continue;
                  add_tokens(node->getContent());
               }
            }
            break;
         }

         case XPathValueType::String:
            add_tokens(Value.string_value);
            break;

         case XPathValueType::Boolean:
            add_tokens(Value.to_string());
            break;

         case XPathValueType::Number:
            if (!std::isnan(Value.number_value)) add_tokens(Value.to_string());
            break;
      }
   };

   for (const auto &arg : Args) collect_from_value(arg);

   if (requested_ids.empty()) return XPathValue(results);
   if (!Context.document) return XPathValue(results);

   std::unordered_set<int> seen_tags;

   std::function<void(XMLTag &)> visit = [&](XMLTag &Tag) {
      if (Tag.isTag()) {
         for (size_t index = 1; index < Tag.Attribs.size(); ++index) {
            const auto &attrib = Tag.Attribs[index];
            if (!(pf::iequals(attrib.Name, "id") or pf::iequals(attrib.Name, "xml:id"))) continue;

            size_t start = attrib.Value.find_first_not_of(" \t\r\n");
            while (start != std::string::npos) {
               size_t end = attrib.Value.find_first_of(" \t\r\n", start);
               std::string token = attrib.Value.substr(start, (end IS std::string::npos) ? std::string::npos : end - start);
               if (!token.empty() and (requested_ids.find(token) != requested_ids.end())) {
                  if (seen_tags.insert(Tag.ID).second) results.push_back(&Tag);
                  break;
               }
               if (end IS std::string::npos) break;
               start = attrib.Value.find_first_not_of(" \t\r\n", end);
            }
         }
      }

      for (auto &child : Tag.Children) visit(child);
   };

   for (auto &root : Context.document->Tags) visit(root);

   return XPathValue(results);
}

XPathValue XPathFunctionLibrary::function_local_name(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   XMLTag *target_node = nullptr;
   const XMLAttrib *target_attribute = nullptr;

   if (Args.empty()) {
      target_node = Context.context_node;
      target_attribute = Context.attribute_node;
   }
   else if (Args[0].type IS XPathValueType::NodeSet) target_node = Args[0].node_set.empty() ? nullptr : Args[0].node_set[0];
   else return XPathValue("");

   if (target_attribute) {
      std::string_view name = target_attribute->Name;
      auto colon = name.find(':');
      if (colon IS std::string::npos) return XPathValue(std::string(name));
      return XPathValue(std::string(name.substr(colon + 1)));
   }

   if (!target_node) return XPathValue("");
   if (target_node->Attribs.empty()) return XPathValue("");

   std::string_view node_name = target_node->Attribs[0].Name;
   if (node_name.empty()) return XPathValue("");

   auto colon = node_name.find(':');
   if (colon IS std::string::npos) return XPathValue(std::string(node_name));
   return XPathValue(std::string(node_name.substr(colon + 1)));
}

XPathValue XPathFunctionLibrary::function_namespace_uri(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   XMLTag *target_node = nullptr;
   const XMLAttrib *target_attribute = nullptr;

   if (Args.empty()) {
      target_node = Context.context_node;
      target_attribute = Context.attribute_node;
   }
   else if (Args[0].type IS XPathValueType::NodeSet) target_node = Args[0].node_set.empty() ? nullptr : Args[0].node_set[0];
   else return XPathValue("");

   if (target_attribute) {
      std::string_view name = target_attribute->Name;
      auto colon = name.find(':');
      if (colon IS std::string::npos) return XPathValue("");

      std::string prefix(name.substr(0, colon));
      if (pf::iequals(prefix, "xml")) return XPathValue("http://www.w3.org/XML/1998/namespace");
      if (pf::iequals(prefix, "xmlns")) return XPathValue("http://www.w3.org/2000/xmlns/");

      XMLTag *scope_node = target_node ? target_node : Context.context_node;
      if (!scope_node) return XPathValue("");

      if (Context.document) {
         std::string uri = find_in_scope_namespace(scope_node, Context.document, prefix);
         return XPathValue(uri);
      }

      return XPathValue("");
   }

   if (!target_node) return XPathValue("");

   std::string prefix;
   if (!target_node->Attribs.empty()) {
      std::string_view node_name = target_node->Attribs[0].Name;
      auto colon = node_name.find(':');
      if (colon != std::string::npos) prefix = std::string(node_name.substr(0, colon));
   }

   if (!prefix.empty()) {
      if (pf::iequals(prefix, "xml")) return XPathValue("http://www.w3.org/XML/1998/namespace");
      if (pf::iequals(prefix, "xmlns")) return XPathValue("http://www.w3.org/2000/xmlns/");
   }

   if ((target_node->NamespaceID != 0) and Context.document) {
      if (auto uri = Context.document->getNamespaceURI(target_node->NamespaceID)) return XPathValue(*uri);
   }

   if (Context.document) {
      std::string uri;
      if (!prefix.empty()) uri = find_in_scope_namespace(target_node, Context.document, prefix);
      else uri = find_in_scope_namespace(target_node, Context.document, std::string());
      return XPathValue(uri);
   }

   return XPathValue("");
}

XPathValue XPathFunctionLibrary::function_name(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   XMLTag *target_node = nullptr;
   const XMLAttrib *target_attribute = nullptr;

   if (Args.empty()) {
      target_node = Context.context_node;
      target_attribute = Context.attribute_node;
   }
   else if (Args[0].type IS XPathValueType::NodeSet) target_node = Args[0].node_set.empty() ? nullptr : Args[0].node_set[0];
   else return XPathValue("");

   if (target_attribute) return XPathValue(target_attribute->Name);

   if (!target_node) return XPathValue("");
   if (target_node->Attribs.empty()) return XPathValue("");

   return XPathValue(target_node->Attribs[0].Name);
}

XPathValue XPathFunctionLibrary::function_string(const std::vector<XPathValue> &Args, const XPathContext &Context) 
{
   if (Args.empty()) {
      if (Context.attribute_node) {
         return XPathValue(Context.attribute_node->Value);
      }

      if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_set_value(nodes);
         return XPathValue(node_set_value.to_string());
      }
      return XPathValue("");
   }
   return XPathValue(Args[0].to_string());
}

XPathValue XPathFunctionLibrary::function_concat(const std::vector<XPathValue> &Args, const XPathContext &Context) 
{
   // Pre-calculate total length to avoid quadratic behavior
   size_t total_length = 0;
   std::vector<std::string> arg_strings;
   arg_strings.reserve(Args.size());

   for (const auto &arg : Args) {
      arg_strings.emplace_back(arg.to_string());
      total_length += arg_strings.back().length();
   }

   std::string result;
   result.reserve(total_length);

   for (const auto &str : arg_strings) {
      result += str;
   }

   return XPathValue(result);
}

XPathValue XPathFunctionLibrary::function_starts_with(const std::vector<XPathValue> &Args, const XPathContext &Context) 
{
   if (Args.size() != 2) return XPathValue(false);
   std::string str = Args[0].to_string();
   std::string prefix = Args[1].to_string();
   return XPathValue(str.substr(0, prefix.length()) IS prefix);
}

XPathValue XPathFunctionLibrary::function_contains(const std::vector<XPathValue> &Args, const XPathContext &Context) 
{
   if (Args.size() != 2) return XPathValue(false);
   std::string str = Args[0].to_string();
   std::string substr = Args[1].to_string();
   return XPathValue(str.find(substr) != std::string::npos);
}

XPathValue XPathFunctionLibrary::function_substring_before(const std::vector<XPathValue> &Args, const XPathContext &Context) 
{
   if (Args.size() != 2) return XPathValue("");

   std::string source = Args[0].to_string();
   std::string pattern = Args[1].to_string();

   if (pattern.empty()) return XPathValue("");

   size_t position = source.find(pattern);
   if (position IS std::string::npos) return XPathValue("");

   return XPathValue(source.substr(0, position));
}

XPathValue XPathFunctionLibrary::function_substring_after(const std::vector<XPathValue> &Args, const XPathContext &Context) 
{
   if (Args.size() != 2) return XPathValue("");

   std::string source = Args[0].to_string();
   std::string pattern = Args[1].to_string();

   if (pattern.empty()) return XPathValue(source);

   size_t position = source.find(pattern);
   if (position IS std::string::npos) return XPathValue("");

   return XPathValue(source.substr(position + pattern.length()));
}

XPathValue XPathFunctionLibrary::function_substring(const std::vector<XPathValue> &Args, const XPathContext &Context) 
{
   if (Args.size() < 2 or Args.size() > 3) return XPathValue("");

   std::string str = Args[0].to_string();
   if (str.empty()) return XPathValue("");

   double start_pos = Args[1].to_number();
   if (std::isnan(start_pos) or std::isinf(start_pos)) return XPathValue("");

   // XPath uses 1-based indexing
   int start_index = (int)std::round(start_pos) - 1;
   if (start_index < 0) start_index = 0;
   if (start_index >= (int)str.length()) return XPathValue("");

   if (Args.size() IS 3) {
      double length = Args[2].to_number();
      if (std::isnan(length) or std::isinf(length) or length <= 0) return XPathValue("");

      int len = (int)std::round(length);
      int remaining = (int)str.length() - start_index;
      if (len > remaining) len = remaining;

      // For small substrings, avoid extra allocation overhead
      if (len <= 0) return XPathValue("");

      return XPathValue(str.substr(start_index, len));
   }

   // Return substring from start_index to end
   return XPathValue(str.substr(start_index));
}

XPathValue XPathFunctionLibrary::function_string_length(const std::vector<XPathValue> &Args, const XPathContext &Context) 
{
   std::string str;
   if (Args.empty()) {
      if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_set_value(nodes);
         str = node_set_value.to_string();
      }
   } 
   else str = Args[0].to_string();
   return XPathValue((double)str.length());
}

XPathValue XPathFunctionLibrary::function_normalize_space(const std::vector<XPathValue> &Args, const XPathContext &Context) 
{
   std::string str;
   if (Args.empty()) {
      if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_set_value(nodes);
         str = node_set_value.to_string();
      }
   } 
   else str = Args[0].to_string();

   // Remove leading and trailing whitespace, collapse internal whitespace
   size_t start = str.find_first_not_of(" \t\n\r");
   if (start IS std::string::npos) return XPathValue("");

   size_t end = str.find_last_not_of(" \t\n\r");
   str = str.substr(start, end - start + 1);

   // Collapse internal whitespace
   std::string result;
   result.reserve(estimate_normalize_space_size(str));
   bool in_whitespace = false;
   for (char c : str) {
      if (c IS ' ' or c IS '\t' or c IS '\n' or c IS '\r') {
         if (!in_whitespace) {
            result += ' ';
            in_whitespace = true;
         }
      } 
      else {
         result += c;
         in_whitespace = false;
      }
   }

   return XPathValue(result);
}

XPathValue XPathFunctionLibrary::function_translate(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   if (Args.size() != 3) return XPathValue("");

   std::string source = Args[0].to_string();
   std::string from = Args[1].to_string();
   std::string to = Args[2].to_string();

   if (source.empty()) return XPathValue("");

   // Pre-size result string based on input length (worst case: no characters removed)
   std::string result;
   result.reserve(source.size());

   // Build character mapping for faster lookups if worth it
   if (from.size() > 10) {
      // Use array mapping for better performance with larger translation sets
      std::array<int, 256> char_map;
      char_map.fill(-1);

      for (size_t i = 0; i < from.size() and i < 256; ++i) {
         unsigned char ch = (unsigned char)from[i];
         if (char_map[ch] IS -1) { // First occurrence takes precedence
            char_map[ch] = (int)i;
         }
      }

      for (char ch : source) {
         unsigned char uch = (unsigned char)ch;
         int index = char_map[uch];
         if (index IS -1) result.push_back(ch);
         else if (index < (int)to.length()) result.push_back(to[index]);
      }
   } 
   else {
      // Use simple find for small translation sets
      for (char ch : source) {
         size_t index = from.find(ch);
         if (index IS std::string::npos) result.push_back(ch);
         else if (index < to.length()) result.push_back(to[index]);
      }
   }

   return XPathValue(result);
}

XPathValue XPathFunctionLibrary::function_upper_case(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   std::string input;

   if (Args.empty()) {
      if (Context.attribute_node) input = Context.attribute_node->Value;
      else if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_value(nodes);
         input = node_value.to_string();
      }
      else input = std::string();
   }
   else input = Args[0].to_string();

   return XPathValue(apply_string_case(input, true));
}

XPathValue XPathFunctionLibrary::function_lower_case(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   std::string input;

   if (Args.empty()) {
      if (Context.attribute_node) input = Context.attribute_node->Value;
      else if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_value(nodes);
         input = node_value.to_string();
      }
      else input = std::string();
   }
   else input = Args[0].to_string();

   return XPathValue(apply_string_case(input, false));
}

XPathValue XPathFunctionLibrary::function_encode_for_uri(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   std::string input;

   if (Args.empty()) {
      if (Context.attribute_node) input = Context.attribute_node->Value;
      else if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_value(nodes);
         input = node_value.to_string();
      }
      else input = std::string();
   }
   else input = Args[0].to_string();

   return XPathValue(encode_for_uri_impl(input));
}

XPathValue XPathFunctionLibrary::function_escape_html_uri(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   std::string input;

   if (Args.empty()) {
      if (Context.attribute_node) input = Context.attribute_node->Value;
      else if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_value(nodes);
         input = node_value.to_string();
      }
      else input = std::string();
   }
   else input = Args[0].to_string();

   return XPathValue(escape_html_uri_impl(input));
}

XPathValue XPathFunctionLibrary::function_matches(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2 or Args.size() > 3) return XPathValue(false);

   std::string input = Args[0].to_string();
   std::string pattern = Args[1].to_string();
   std::string flags = (Args.size() IS 3) ? Args[2].to_string() : std::string();

   pf::Regex compiled;
   if (not compiled.compile(pattern, build_regex_options(flags, Context.expression_unsupported))) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
      return XPathValue(false);
   }

   pf::MatchResult result;
   bool matched = compiled.search(input, result);
   return XPathValue(matched);
}

XPathValue XPathFunctionLibrary::function_replace(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 3 or Args.size() > 4) return XPathValue("");

   std::string input = Args[0].to_string();
   std::string pattern = Args[1].to_string();
   std::string replacement = Args[2].to_string();
   std::string flags = (Args.size() IS 4) ? Args[3].to_string() : std::string();

   pf::Regex compiled;
   if (!compiled.compile(pattern, build_regex_options(flags, Context.expression_unsupported))) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
      return XPathValue(input);
   }

   std::string replaced;
   if (!compiled.replace(input, replacement, replaced)) replaced = input;

   return XPathValue(replaced);
}

XPathValue XPathFunctionLibrary::function_tokenize(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2 or Args.size() > 3) return XPathValue(std::vector<XMLTag *>());

   std::string input = Args[0].to_string();
   std::string pattern = Args[1].to_string();
   std::string flags = (Args.size() IS 3) ? Args[2].to_string() : std::string();

   std::vector<std::string> tokens;

   if (pattern.empty()) {
      for (size_t index = 0; index < input.length(); ++index) {
         tokens.emplace_back(input.substr(index, 1));
      }
   }
   else {
      pf::SyntaxOptions options = build_regex_options(flags, Context.expression_unsupported);

      pf::Regex compiled;
      if (!compiled.compile(pattern, options)) {
         return XPathValue(std::vector<XMLTag *>());
      }

      compiled.tokenize(input, -1, tokens);

      if (!tokens.empty() and tokens.back().empty()) tokens.pop_back();
   }

   std::vector<XMLTag *> placeholders(tokens.size(), nullptr);
   return XPathValue(placeholders, std::nullopt, tokens);
}

XPathValue XPathFunctionLibrary::function_boolean(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 1) return XPathValue(false);
   return XPathValue(Args[0].to_boolean());
}

XPathValue XPathFunctionLibrary::function_not(const std::vector<XPathValue> &Args, const XPathContext &Context) 
{
   if (Args.size() != 1) return XPathValue(true);
   return XPathValue(!Args[0].to_boolean());
}

XPathValue XPathFunctionLibrary::function_true(const std::vector<XPathValue> &Args, const XPathContext &Context) 
{
   return XPathValue(true);
}

XPathValue XPathFunctionLibrary::function_false(const std::vector<XPathValue> &Args, const XPathContext &Context) 
{
   return XPathValue(false);
}

XPathValue XPathFunctionLibrary::function_lang(const std::vector<XPathValue> &Args, const XPathContext &Context) 
{
   if (Args.size() != 1) return XPathValue(false);

   std::string requested = Args[0].to_string();
   if (requested.empty()) return XPathValue(false);

   XMLTag *node = Context.context_node;
   if (!node) return XPathValue(false);

   std::string language = find_language_for_node(node, Context.document);
   if (language.empty()) return XPathValue(false);

   return XPathValue(language_matches(language, requested));
}

XPathValue XPathFunctionLibrary::function_number(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   if (Args.empty()) {
      if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_set_value(nodes);
         return XPathValue(node_set_value.to_number());
      }
      return XPathValue(std::numeric_limits<double>::quiet_NaN());
   }
   return XPathValue(Args[0].to_number());
}

XPathValue XPathFunctionLibrary::function_sum(const std::vector<XPathValue> &Args, const XPathContext &Context) 
{
   if (Args.size() != 1) return XPathValue(0.0);
   if (Args[0].type != XPathValueType::NodeSet) return XPathValue(0.0);

   const auto &nodeset = Args[0];
   double sum = 0.0;

   // Handle attribute nodes if present
   if (!nodeset.node_set_attributes.empty()) {
      for (const XMLAttrib *attr : nodeset.node_set_attributes) {
         if (attr) {
            double value = XPathValue::string_to_number(attr->Value);
            if (!std::isnan(value)) {
               sum += value;
            }
         }
      }
   }
   // Handle string values if present
   else if (!nodeset.node_set_string_values.empty()) {
      for (const std::string &str : nodeset.node_set_string_values) {
         double value = XPathValue::string_to_number(str);
         if (!std::isnan(value)) {
            sum += value;
         }
      }
   }
   // Handle regular element nodes
   else {
      for (XMLTag *node : nodeset.node_set) {
         if (node) {
            std::string node_content = XPathValue::node_string_value(node);
            double value = XPathValue::string_to_number(node_content);
            if (!std::isnan(value)) {
               sum += value;
            }
         }
      }
   }
   return XPathValue(sum);
}

XPathValue XPathFunctionLibrary::function_floor(const std::vector<XPathValue> &Args, const XPathContext &Context) 
{
   if (Args.size() != 1) return XPathValue(std::numeric_limits<double>::quiet_NaN());
   double value = Args[0].to_number();
   if (std::isnan(value) or std::isinf(value)) return XPathValue(value);
   return XPathValue(std::floor(value));
}

XPathValue XPathFunctionLibrary::function_ceiling(const std::vector<XPathValue> &Args, const XPathContext &Context) 
{
   if (Args.size() != 1) return XPathValue(std::numeric_limits<double>::quiet_NaN());
   double value = Args[0].to_number();
   if (std::isnan(value) or std::isinf(value)) return XPathValue(value);
   return XPathValue(std::ceil(value));
}

XPathValue XPathFunctionLibrary::function_round(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 1) return XPathValue(std::numeric_limits<double>::quiet_NaN());
   double value = Args[0].to_number();
   if (std::isnan(value) or std::isinf(value)) return XPathValue(value);
   return XPathValue(std::round(value));
}

XPathValue XPathFunctionLibrary::function_abs(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 1) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   double value = Args[0].to_number();
   if (std::isnan(value) or std::isinf(value)) return XPathValue(value);

   return XPathValue(std::fabs(value));
}

XPathValue XPathFunctionLibrary::function_min(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   std::vector<double> numbers;
   numbers.reserve(Args.size());

   for (const auto &arg : Args) append_numbers_from_value(arg, numbers);

   if (numbers.empty()) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   double minimum = numbers[0];
   for (size_t index = 1; index < numbers.size(); ++index) {
      if (numbers[index] < minimum) minimum = numbers[index];
   }

   return XPathValue(minimum);
}

XPathValue XPathFunctionLibrary::function_max(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   std::vector<double> numbers;
   numbers.reserve(Args.size());

   for (const auto &arg : Args) append_numbers_from_value(arg, numbers);

   if (numbers.empty()) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   double maximum = numbers[0];
   for (size_t index = 1; index < numbers.size(); ++index) {
      if (numbers[index] > maximum) maximum = numbers[index];
   }

   return XPathValue(maximum);
}

XPathValue XPathFunctionLibrary::function_avg(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   std::vector<double> numbers;
   numbers.reserve(Args.size());

   for (const auto &arg : Args) append_numbers_from_value(arg, numbers);

   if (numbers.empty()) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   double total = 0.0;
   for (double value : numbers) total += value;

   return XPathValue(total / (double)numbers.size());
}
