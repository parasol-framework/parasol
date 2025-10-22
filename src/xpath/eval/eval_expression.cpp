
#include "eval.h"
#include "eval_detail.h"
#include "../api/xpath_functions.h"
#include "../../xml/schema/schema_types.h"
#include "../../xml/schema/type_checker.h"
#include "../../xml/xml.h"
#include <parasol/strings.hpp>
#include <format>
#include <cmath>
#include <utility>
#include <string_view>
#include <charconv>
#include <cctype>
#include <unordered_map>
#include <system_error>
#include <optional>
#include <limits>

struct SequenceEntry {
   XMLTag * node = nullptr;
   const XMLAttrib * attribute = nullptr;
   std::string string_value;
};

struct ForBindingDefinition {
   std::string name;
   const XPathNode * sequence = nullptr;
};

struct QuantifiedBindingDefinition {
   std::string name;
   const XPathNode * sequence = nullptr;
};

struct CastTargetInfo {
   std::string type_name;
   bool allows_empty = false;
};

enum class SequenceCardinality {
   ExactlyOne,
   ZeroOrOne,
   OneOrMore,
   ZeroOrMore
};

enum class SequenceItemKind {
   Atomic,
   Element,
   Attribute,
   Text,
   Node,
   Item,
   EmptySequence
};

struct SequenceTypeInfo {
   SequenceCardinality occurrence = SequenceCardinality::ExactlyOne;
   SequenceItemKind kind = SequenceItemKind::Atomic;
   std::string type_name;

   [[nodiscard]] bool allows_empty() const
   {
      return (occurrence IS SequenceCardinality::ZeroOrOne) or (occurrence IS SequenceCardinality::ZeroOrMore);
   }

   [[nodiscard]] bool allows_multiple() const
   {
      return (occurrence IS SequenceCardinality::OneOrMore) or (occurrence IS SequenceCardinality::ZeroOrMore);
   }
};

static bool is_space_character(char ch) noexcept {
   return (ch IS ' ') or (ch IS '\t') or (ch IS '\n') or (ch IS '\r');
}

static std::string_view trim_view(std::string_view Text)
{
   size_t start = 0;
   while ((start < Text.size()) and is_space_character(Text[start])) start++;
   size_t end = Text.size();
   while ((end > start) and is_space_character(Text[end - 1])) end--;
   return Text.substr(start, end - start);
}

static CastTargetInfo parse_cast_target_literal(std::string_view Literal)
{
   CastTargetInfo info;

   size_t start = 0;
   while ((start < Literal.size()) and is_space_character(Literal[start])) start++;
   if (start >= Literal.size()) return info;

   size_t end = Literal.size();
   while (end > start and is_space_character(Literal[end - 1])) end--;

   std::string_view trimmed = Literal.substr(start, end - start);

   if ((!trimmed.empty()) and (trimmed.back() IS '?')) {
      info.allows_empty = true;
      trimmed.remove_suffix(1);
      while ((!trimmed.empty()) and is_space_character(trimmed.back())) trimmed.remove_suffix(1);
   }

   info.type_name.assign(trimmed);
   return info;
}

static std::optional<SequenceTypeInfo> parse_sequence_type_literal(std::string_view Literal)
{
   SequenceTypeInfo info;
   auto trimmed = trim_view(Literal);
   if (trimmed.empty()) return std::nullopt;

   size_t end = trimmed.size();
   char occurrence_marker = '\0';

   if (end > 0) {
      char marker = trimmed[end - 1];
      if ((marker IS '?') or (marker IS '+') or (marker IS '*')) {
         occurrence_marker = marker;
         end--;
         while ((end > 0) and is_space_character(trimmed[end - 1])) end--;
      }
   }

   switch (occurrence_marker) {
      case '?': info.occurrence = SequenceCardinality::ZeroOrOne; break;
      case '+': info.occurrence = SequenceCardinality::OneOrMore; break;
      case '*': info.occurrence = SequenceCardinality::ZeroOrMore; break;
      default: break;
   }

   std::string_view core = trimmed.substr(0, end);
   core = trim_view(core);
   if (core.empty()) return std::nullopt;

   // Normalise away internal whitespace for node-test tokens like "element()" which may appear as "element ( )"
   std::string core_compact;
   core_compact.reserve(core.size());
   for (char ch : core) { if ((ch != ' ') and (ch != '\t') and (ch != '\r') and (ch != '\n')) core_compact.push_back(ch); }

   if (core_compact IS "item()") info.kind = SequenceItemKind::Item;
   else if (core_compact IS "node()") info.kind = SequenceItemKind::Node;
   else if (core_compact IS "element()") info.kind = SequenceItemKind::Element;
   else if (core_compact IS "attribute()") info.kind = SequenceItemKind::Attribute;
   else if (core_compact IS "text()") info.kind = SequenceItemKind::Text;
   else if (core_compact IS "empty-sequence()") info.kind = SequenceItemKind::EmptySequence;
   else {
      info.kind = SequenceItemKind::Atomic;
      info.type_name.assign(core);
   }

   return info;
}

static size_t sequence_item_count(const XPathVal &Value)
{
   if (Value.Type IS XPVT::NodeSet) return Value.node_set.size();
   if (Value.Type IS XPVT::NIL) return 0;

   switch (Value.Type) {
      case XPVT::Boolean:
      case XPVT::Number:
      case XPVT::String:
      case XPVT::Date:
      case XPVT::Time:
      case XPVT::DateTime:
         return 1;
      default:
         break;
   }

   return Value.is_empty() ? 0 : 1;
}

static std::string nodeset_item_string(const XPathVal &Value, size_t Index)
{
   if (Index < Value.node_set_string_values.size()) return Value.node_set_string_values[Index];

   if ((Index < Value.node_set_attributes.size()) and Value.node_set_attributes[Index]) {
      return Value.node_set_attributes[Index]->Value;
   }

   bool use_override = Value.node_set_string_override.has_value() and Value.node_set_string_values.empty();
   if (use_override and (Index IS 0)) return *Value.node_set_string_override;

   if (Index < Value.node_set.size() and Value.node_set[Index]) {
      return XPathVal::node_string_value(Value.node_set[Index]);
   }

   return std::string();
}

static std::string describe_nodeset_item_kind(const XMLTag *Node, const XMLAttrib *Attribute)
{
   if (Attribute) return std::string("attribute()");
   if (!Node) return std::string("item()");
   if (Node->Attribs.empty()) return std::string("node()");
   if (Node->Attribs[0].Name.empty()) return std::string("text()");
   if ((Node->Flags & XTF::COMMENT) != XTF::NIL) return std::string("comment()");
   if ((Node->Flags & XTF::INSTRUCTION) != XTF::NIL) return std::string("processing-instruction()");
   return std::string("element()");
}

static bool is_text_node(const XMLTag *Node)
{
   if (!Node) return false;
   if (Node->Attribs.empty()) return false;
   return Node->Attribs[0].Name.empty();
}

static bool is_constructed_scalar_text(const XMLTag *Node)
{
   if (!is_text_node(Node)) return false;
   return Node->ParentID IS 0;
}

static bool is_valid_timezone(std::string_view Value)
{
   if (Value.empty()) return true;
   if ((Value.size() IS 1) and (Value[0] IS 'Z')) return true;

   if ((Value.size() IS 6) and ((Value[0] IS '+') or (Value[0] IS '-')))
   {
      if ((Value[3] != ':') or (Value[4] < '0') or (Value[4] > '9') or (Value[5] < '0') or (Value[5] > '9')) return false;
      if ((Value[1] < '0') or (Value[1] > '9') or (Value[2] < '0') or (Value[2] > '9')) return false;

      int hour = (Value[1] - '0') * 10 + (Value[2] - '0');
      int minute = (Value[4] - '0') * 10 + (Value[5] - '0');

      if (hour > 14) return false;
      if (minute >= 60) return false;
      if ((hour IS 14) and (minute != 0)) return false;

      return true;
   }

   return false;
}

static bool parse_xs_date_components(std::string_view Value, long long &Year, int &Month, int &Day, size_t &NextIndex)
{
   if (Value.empty()) return false;

   size_t index = 0;
   bool negative = false;

   if ((Value[index] IS '+') or (Value[index] IS '-'))
   {
      negative = Value[index] IS '-';
      index++;
      if (index >= Value.size()) return false;
   }

   size_t year_start = index;
   while ((index < Value.size()) and (Value[index] >= '0') and (Value[index] <= '9')) index++;
   size_t year_digits = index - year_start;
   if (year_digits < 4) return false;

   long long year_value = 0;
   auto result = std::from_chars(Value.data() + year_start, Value.data() + index, year_value);
   if (result.ec != std::errc()) return false;
   if (negative) year_value = -year_value;

   if ((index >= Value.size()) or (Value[index] != '-')) return false;
   index++;
   if (index + 2 > Value.size()) return false;

   int month_value = (Value[index] - '0') * 10 + (Value[index + 1] - '0');
   if ((month_value < 1) or (month_value > 12)) return false;
   index += 2;

   if ((index >= Value.size()) or (Value[index] != '-')) return false;
   index++;
   if (index + 2 > Value.size()) return false;

   int day_value = (Value[index] - '0') * 10 + (Value[index + 1] - '0');
   if ((day_value < 1) or (day_value > 31)) return false;
   index += 2;

   int max_day = 31;
   if ((month_value IS 4) or (month_value IS 6) or (month_value IS 9) or (month_value IS 11)) max_day = 30;
   else if (month_value IS 2)
   {
      bool leap = false;
      if ((year_value % 4) IS 0)
      {
         leap = ((year_value % 100) != 0) or ((year_value % 400) IS 0);
      }
      max_day = leap ? 29 : 28;
   }

   if (day_value > max_day) return false;

   Year = year_value;
   Month = month_value;
   Day = day_value;
   NextIndex = index;
   return true;
}

static bool is_valid_xs_date(std::string_view Value)
{
   long long year = 0;
   int month = 0;
   int day = 0;
   size_t next_index = 0;

   if (not parse_xs_date_components(Value, year, month, day, next_index)) return false;
   std::string_view timezone = Value.substr(next_index);
   return is_valid_timezone(timezone);
}

static bool is_valid_xs_date_no_timezone(std::string_view Value)
{
   long long year = 0;
   int month = 0;
   int day = 0;
   size_t next_index = 0;

   if (not parse_xs_date_components(Value, year, month, day, next_index)) return false;
   return next_index IS Value.size();
}

static bool is_valid_xs_time(std::string_view Value)
{
   if (Value.size() < 8) return false;

   if ((Value[0] < '0') or (Value[0] > '9') or (Value[1] < '0') or (Value[1] > '9')) return false;
   int hour = (Value[0] - '0') * 10 + (Value[1] - '0');
   if (hour > 23) return false;

   if (Value[2] != ':') return false;
   if ((Value[3] < '0') or (Value[3] > '9') or (Value[4] < '0') or (Value[4] > '9')) return false;
   int minute = (Value[3] - '0') * 10 + (Value[4] - '0');
   if (minute >= 60) return false;

   if (Value[5] != ':') return false;
   if ((Value[6] < '0') or (Value[6] > '9') or (Value[7] < '0') or (Value[7] > '9')) return false;
   int second = (Value[6] - '0') * 10 + (Value[7] - '0');
   if (second >= 60) return false;

   size_t index = 8;
   if ((index < Value.size()) and (Value[index] IS '.'))
   {
      index++;
      size_t fraction_start = index;
      while ((index < Value.size()) and (Value[index] >= '0') and (Value[index] <= '9')) index++;
      if (index IS fraction_start) return false;
   }

   std::string_view timezone = Value.substr(index);
   return is_valid_timezone(timezone);
}

static bool is_valid_xs_datetime(std::string_view Value)
{
   size_t position = Value.find('T');
   if (position IS std::string_view::npos) return false;

   std::string_view date_part = Value.substr(0, position);
   std::string_view time_part = Value.substr(position + 1);
   if (time_part.empty()) return false;

   if (not is_valid_xs_date_no_timezone(date_part)) return false;
   return is_valid_xs_time(time_part);
}

static thread_local std::unordered_map<std::string, std::weak_ptr<xml::schema::SchemaTypeDescriptor>> cast_target_cache;

static bool is_value_castable_to_type(const XPathVal &Value,
   const std::shared_ptr<xml::schema::SchemaTypeDescriptor> &SourceDescriptor,
   const std::shared_ptr<xml::schema::SchemaTypeDescriptor> &TargetDescriptor,
   std::string_view Lexical)
{
   if (!TargetDescriptor) return false;

   auto target_type = TargetDescriptor->schema_type;

   if ((target_type IS xml::schema::SchemaType::XPathString) or (target_type IS xml::schema::SchemaType::XSString)) {
      return true;
   }

   XPathVal coerced = Value;
   if (SourceDescriptor) coerced = SourceDescriptor->coerce_value(Value, target_type);

   if (xml::schema::is_numeric(target_type)) {
      double numeric_value = coerced.to_number();
      return !std::isnan(numeric_value);
   }

   if ((target_type IS xml::schema::SchemaType::XPathBoolean) or (target_type IS xml::schema::SchemaType::XSBoolean)) {
      if (Value.Type IS XPVT::String) {
         auto parsed_boolean = parse_schema_boolean(Lexical);
         return parsed_boolean.has_value();
      }
      return true;
   }

   if (target_type IS xml::schema::SchemaType::XSDate) {
      if ((Value.Type IS XPVT::Date) or (Value.Type IS XPVT::DateTime)) return true;
      return is_valid_xs_date(Lexical);
   }

   if (target_type IS xml::schema::SchemaType::XSDateTime) {
      if (Value.Type IS XPVT::DateTime) return true;
      return is_valid_xs_datetime(Lexical);
   }

   if (target_type IS xml::schema::SchemaType::XSTime) {
      if (Value.Type IS XPVT::Time) return true;
      return is_valid_xs_time(Lexical);
   }

   if (SourceDescriptor) return SourceDescriptor->can_coerce_to(target_type);
   return false;
}

//********************************************************************************************************************
// File-local helpers extracted from large embedded lambdas to reduce function length and improve readability.

static std::string canonicalise_variable_qname(std::string_view Candidate,
   const XQueryProlog &SourceProlog, const extXML *Document)
{
   if ((Candidate.size() > 2) and (Candidate[0] IS 'Q') and (Candidate[1] IS '{')) {
      return std::string(Candidate);
   }

   size_t colon_position = Candidate.find(':');
   if (colon_position != std::string_view::npos) {
      std::string prefix(Candidate.substr(0, colon_position));
      std::string_view local_name_view = Candidate.substr(colon_position + 1);

      auto uri_entry = SourceProlog.declared_namespace_uris.find(prefix);
      if (uri_entry != SourceProlog.declared_namespace_uris.end()) {
         return std::format("Q{{{}}}{}", uri_entry->second, local_name_view);
      }

      if (Document) {
         auto prefix_it = Document->Prefixes.find(prefix);
         if (prefix_it != Document->Prefixes.end()) {
            auto ns_it = Document->NSRegistry.find(prefix_it->second);
            if (ns_it != Document->NSRegistry.end()) {
               return std::format("Q{{{}}}{}", ns_it->second, local_name_view);
            }
         }
      }
   }

   return std::string(Candidate);
}

//********************************************************************************************************************

static void append_value_to_sequence(const XPathVal &Value, std::vector<SequenceEntry> &Entries,
   int &NextConstructedNodeId, std::vector<std::unique_ptr<XMLTag>> &ConstructedNodes)
{
   if (Value.Type IS XPVT::NodeSet) {
      bool use_override = Value.node_set_string_override.has_value() and Value.node_set_string_values.empty();
      for (size_t index = 0; index < Value.node_set.size(); ++index) {
         XMLTag *node = Value.node_set[index];
         if (not node) continue;

         const XMLAttrib *attribute = nullptr;
         if (index < Value.node_set_attributes.size()) attribute = Value.node_set_attributes[index];

         std::string item_string;
         if (index < Value.node_set_string_values.size()) item_string = Value.node_set_string_values[index];
         else if (use_override) item_string = *Value.node_set_string_override;
         else if (attribute) item_string = attribute->Value;
         else item_string = XPathVal::node_string_value(node);

         Entries.push_back({ node, attribute, std::move(item_string) });
      }
      return;
   }

   std::string text = Value.to_string();
   pf::vector<XMLAttrib> text_attribs;
   text_attribs.emplace_back("", text);

   XMLTag text_node(NextConstructedNodeId--, 0, text_attribs);
   text_node.ParentID = 0;

   auto stored = std::make_unique<XMLTag>(std::move(text_node));
   XMLTag *root = stored.get();
   ConstructedNodes.push_back(std::move(stored));

   Entries.push_back({ root, nullptr, std::move(text) });
}

//********************************************************************************************************************

enum class BinaryOperationKind {
   AND,
   OR,
   UNION,
   INTERSECT,
   EXCEPT,
   COMMA,
   EQ,
   NE,
   EQ_WORD,
   NE_WORD,
   LT,
   LE,
   GT,
   GE,
   ADD,
   SUB,
   MUL,
   DIV,
   MOD,
   RANGE,
   UNKNOWN
};

static BinaryOperationKind map_binary_operation(std::string_view Op)
{
   if (Op IS "and") return BinaryOperationKind::AND;
   if (Op IS "or") return BinaryOperationKind::OR;
   if (Op IS "|") return BinaryOperationKind::UNION;
   if (Op IS "intersect") return BinaryOperationKind::INTERSECT;
   if (Op IS "except") return BinaryOperationKind::EXCEPT;
   if (Op IS ",") return BinaryOperationKind::COMMA;
   if (Op IS "=") return BinaryOperationKind::EQ;
   if (Op IS "!=") return BinaryOperationKind::NE;
   if (Op IS "eq") return BinaryOperationKind::EQ_WORD;
   if (Op IS "ne") return BinaryOperationKind::NE_WORD;
   if ((Op IS "<") or (Op IS "lt")) return BinaryOperationKind::LT;
   if ((Op IS "<=") or (Op IS "le")) return BinaryOperationKind::LE;
   if ((Op IS ">") or (Op IS "gt")) return BinaryOperationKind::GT;
   if ((Op IS ">=") or (Op IS "ge")) return BinaryOperationKind::GE;
   if (Op IS "+") return BinaryOperationKind::ADD;
   if (Op IS "-") return BinaryOperationKind::SUB;
   if (Op IS "*") return BinaryOperationKind::MUL;
   if (Op IS "div") return BinaryOperationKind::DIV;
   if (Op IS "mod") return BinaryOperationKind::MOD;
   if (Op IS "to") return BinaryOperationKind::RANGE;
   return BinaryOperationKind::UNKNOWN;
}

static constexpr int64_t RANGE_ITEM_LIMIT = 100000;

//********************************************************************************************************************

static bool append_iteration_value_helper(const XPathVal &IterationValue, NODES &CombinedNodes,
   std::vector<const XMLAttrib *> &CombinedAttributes, std::vector<std::string> &CombinedStrings,
   std::optional<std::string> &CombinedOverride)
{
   if (IterationValue.Type IS XPVT::NodeSet) {
      size_t length = IterationValue.node_set.size();
      if (length < IterationValue.node_set_attributes.size()) length = IterationValue.node_set_attributes.size();
      if (length < IterationValue.node_set_string_values.size()) length = IterationValue.node_set_string_values.size();
      if ((length IS 0) and IterationValue.node_set_string_override.has_value()) length = 1;

      for (size_t node_index = 0; node_index < length; ++node_index) {
         XMLTag *node = node_index < IterationValue.node_set.size() ? IterationValue.node_set[node_index] : nullptr;
         CombinedNodes.push_back(node);

         const XMLAttrib *attribute = nullptr;
         if (node_index < IterationValue.node_set_attributes.size()) attribute = IterationValue.node_set_attributes[node_index];
         CombinedAttributes.push_back(attribute);

         std::string node_string;
         bool use_override = IterationValue.node_set_string_override.has_value() and IterationValue.node_set_string_values.empty() and (node_index IS 0);
         if (node_index < IterationValue.node_set_string_values.size()) node_string = IterationValue.node_set_string_values[node_index];
         else if (use_override) node_string = *IterationValue.node_set_string_override;
         else if (attribute) node_string = attribute->Value;
         else if (node) node_string = XPathVal::node_string_value(node);

         CombinedStrings.push_back(node_string);
         if (not CombinedOverride.has_value()) CombinedOverride = node_string;
      }

      if (IterationValue.node_set_string_override.has_value() and IterationValue.node_set_string_values.empty()) {
         if (not CombinedOverride.has_value()) CombinedOverride = IterationValue.node_set_string_override;
      }

      return true;
   }

   if (IterationValue.is_empty()) return true;

   std::string atomic_string = IterationValue.to_string();
   CombinedNodes.push_back(nullptr);
   CombinedAttributes.push_back(nullptr);
   CombinedStrings.push_back(atomic_string);
   if (not CombinedOverride.has_value()) CombinedOverride = atomic_string;
   return true;
}

//********************************************************************************************************************

static bool evaluate_for_bindings_recursive(XPathEvaluator &Self, XPathContext &Context,
   const std::vector<ForBindingDefinition> &Bindings, size_t BindingIndex,
   const XPathNode *ReturnNode, uint32_t CurrentPrefix, NODES &CombinedNodes,
   std::vector<const XMLAttrib *> &CombinedAttributes, std::vector<std::string> &CombinedStrings,
   std::optional<std::string> &CombinedOverride, bool &ExpressionUnsupported)
{
   if (BindingIndex >= Bindings.size()) {
      auto iteration_value = Self.evaluate_expression(ReturnNode, CurrentPrefix);
      if (ExpressionUnsupported) return false;
      if (not append_iteration_value_helper(iteration_value, CombinedNodes, CombinedAttributes, CombinedStrings, CombinedOverride)) return false;
      return true;
   }

   const auto &binding = Bindings[BindingIndex];
   if (not binding.sequence) {
      ExpressionUnsupported = true;
      return false;
   }

   const std::string variable_name = binding.name;

   auto sequence_value = Self.evaluate_expression(binding.sequence, CurrentPrefix);
   if (ExpressionUnsupported) return false;

   if (sequence_value.Type != XPVT::NodeSet) {
      ExpressionUnsupported = true;
      return false;
   }

   size_t sequence_size = sequence_value.node_set.size();

   if (sequence_size IS 0) return true;

   for (size_t index = 0; index < sequence_size; ++index) {
      XMLTag *item_node = sequence_value.node_set[index];
      const XMLAttrib *item_attribute = nullptr;
      if (index < sequence_value.node_set_attributes.size()) {
         item_attribute = sequence_value.node_set_attributes[index];
      }

      XPathVal bound_value;
      bound_value.Type = XPVT::NodeSet;
      bound_value.preserve_node_order = false;
      bound_value.node_set.push_back(item_node);
      if (item_attribute) bound_value.node_set_attributes.push_back(item_attribute);

      std::string item_string;
      bool use_override = sequence_value.node_set_string_override.has_value() and (index IS 0) and sequence_value.node_set_string_values.empty();
      if (index < sequence_value.node_set_string_values.size()) {
         item_string = sequence_value.node_set_string_values[index];
      }
      else if (use_override) item_string = *sequence_value.node_set_string_override;
      else if (item_node) item_string = XPathVal::node_string_value(item_node);

      bound_value.node_set_string_values.push_back(item_string);
      bound_value.node_set_string_override = item_string;

      VariableBindingGuard iteration_guard(Context, variable_name, std::move(bound_value));

      Self.push_context(item_node, index + 1, sequence_size, item_attribute);
      bool iteration_ok = evaluate_for_bindings_recursive(Self, Context, Bindings, BindingIndex + 1,
         ReturnNode, CurrentPrefix, CombinedNodes, CombinedAttributes, CombinedStrings, CombinedOverride, ExpressionUnsupported);
      Self.pop_context();

      if (not iteration_ok) return false;
      if (ExpressionUnsupported) return false;
   }

   return true;
}

//********************************************************************************************************************

static bool evaluate_quantified_binding_recursive(XPathEvaluator &Self, XPathContext &Context,
   const std::vector<QuantifiedBindingDefinition> &Bindings, size_t BindingIndex,
   bool IsSome, bool IsEvery, const XPathNode *ConditionNode, uint32_t CurrentPrefix,
   bool &ExpressionUnsupported)
{
   if (BindingIndex >= Bindings.size()) {
      auto condition_value = Self.evaluate_expression(ConditionNode, CurrentPrefix);
      if (ExpressionUnsupported) return false;
      return condition_value.to_boolean();
   }

   const auto &binding = Bindings[BindingIndex];
   if (not binding.sequence) {
      ExpressionUnsupported = true;
      return false;
   }

   const std::string variable_name = binding.name;

   auto sequence_value = Self.evaluate_expression(binding.sequence, CurrentPrefix);
   if (ExpressionUnsupported) return false;

   if (sequence_value.Type != XPVT::NodeSet) {
      ExpressionUnsupported = true;
      return false;
   }

   size_t sequence_size = sequence_value.node_set.size();

   if (sequence_size IS 0) return IsEvery;

   for (size_t index = 0; index < sequence_size; ++index) {
      XMLTag *item_node = sequence_value.node_set[index];
      const XMLAttrib *item_attribute = nullptr;
      if (index < sequence_value.node_set_attributes.size()) {
         item_attribute = sequence_value.node_set_attributes[index];
      }

      XPathVal bound_value;
      bound_value.Type = XPVT::NodeSet;
      bound_value.preserve_node_order = false;
      bound_value.node_set.push_back(item_node);
      if (item_attribute) bound_value.node_set_attributes.push_back(item_attribute);

      std::string item_string;
      bool use_override = sequence_value.node_set_string_override.has_value() and (index IS 0) and sequence_value.node_set_string_values.empty();
      if (index < sequence_value.node_set_string_values.size()) {
         item_string = sequence_value.node_set_string_values[index];
      }
      else if (use_override) item_string = *sequence_value.node_set_string_override;
      else if (item_node) item_string = XPathVal::node_string_value(item_node);

      bound_value.node_set_string_values.push_back(item_string);
      bound_value.node_set_string_override = item_string;

      VariableBindingGuard iteration_guard(Context, variable_name, std::move(bound_value));

      Self.push_context(item_node, index + 1, sequence_size, item_attribute);
      bool branch_result = evaluate_quantified_binding_recursive(Self, Context, Bindings, BindingIndex + 1,
         IsSome, IsEvery, ConditionNode, CurrentPrefix, ExpressionUnsupported);
      Self.pop_context();

      if (ExpressionUnsupported) return false;

      if (branch_result) {
         if (IsSome) return true;
      }
      else if (IsEvery) return false;
   }

   return IsEvery;
}

//********************************************************************************************************************
// Resolves a variable reference by consulting the dynamic context, document bindings, and finally the prolog.

bool XPathEvaluator::resolve_variable_value(std::string_view QName, uint32_t CurrentPrefix,
   XPathVal &OutValue, const XPathNode *ReferenceNode)
{
   std::string name(QName);
   std::string normalised_name(name);

   if (context.variables) {
      auto local_variable = context.variables->find(name);
      if (local_variable != context.variables->end()) {
         OutValue = local_variable->second;
         return true;
      }
   }

   if (xml) {
      auto xml_variable = xml->Variables.find(name);
      if (xml_variable != xml->Variables.end()) {
         OutValue = XPathVal(xml_variable->second);
         return true;
      }
   }

   auto prolog = context.prolog;
   if (not prolog) return false;

   const XQueryVariable *variable = prolog->find_variable(QName);
   std::shared_ptr<XQueryProlog> owner_prolog = prolog;
   auto active_module_cache = context.module_cache;
   std::string module_uri;
   std::string imported_local_name;
   std::string canonical_lookup;

   if (not variable) {
      uint32_t namespace_hash = 0;

      if ((name.size() > 2) and (name[0] IS 'Q') and (name[1] IS '{')) {
         size_t closing = name.find('}');
         if (closing != std::string::npos) {
            module_uri = name.substr(2, closing - 2);
            imported_local_name = name.substr(closing + 1);
            if (not module_uri.empty()) namespace_hash = pf::strhash(module_uri);
         }
      }

      if (not namespace_hash) {
         auto separator = name.find(':');
         if (separator != std::string::npos) {
            std::string prefix = name.substr(0, separator);
            imported_local_name = name.substr(separator + 1);
            namespace_hash = prolog->resolve_prefix(prefix, context.document);
            if (namespace_hash != 0) {
               auto uri_entry = prolog->declared_namespace_uris.find(prefix);
               if (uri_entry != prolog->declared_namespace_uris.end()) module_uri = uri_entry->second;
               else if (context.document) {
                  auto prefix_it = context.document->Prefixes.find(prefix);
                  if (prefix_it != context.document->Prefixes.end()) {
                     auto ns_it = context.document->NSRegistry.find(prefix_it->second);
                     if (ns_it != context.document->NSRegistry.end()) module_uri = ns_it->second;
                  }
               }
            }
         }
      }

      const XQueryModuleImport *matched_import = nullptr;
      if (namespace_hash != 0) {
         for (const auto &import : prolog->module_imports) {
            if (pf::strhash(import.target_namespace) IS namespace_hash) {
               matched_import = &import;
               if (module_uri.empty()) module_uri = import.target_namespace;
               break;
            }
         }
      }

      if (matched_import) {
         if (module_uri.empty()) {
            std::string message = "Module variable '" + name + "' has an unresolved namespace.";
            record_error(message, ReferenceNode, true);
            return false;
         }

         auto module_cache = context.module_cache;
         if (not module_cache) {
            record_error("Module variable '" + name + "' requires a module cache.", ReferenceNode, true);
            return false;
         }

         (void)module_cache->fetch_or_load(module_uri, *prolog, *this);

         auto module_info = module_cache->find_module(module_uri);
         if (not module_info) {
            // Preserve earlier loader diagnostics when present
            record_error("Module '" + module_uri + "' could not be loaded for variable '" + name + "'.", ReferenceNode, false);
            return false;
         }
         else if (not module_info->prolog) {
            record_error("Module '" + module_uri + "' does not expose a prolog.", ReferenceNode, false);
            return false;
         }

         const XQueryVariable *module_variable = module_info->prolog->find_variable(name);

         if (not module_uri.empty() and not imported_local_name.empty()) {
            canonical_lookup = std::format("Q{{{}}}{}", module_uri, imported_local_name);
         }

         if (not module_variable and not canonical_lookup.empty()) {
            module_variable = module_info->prolog->find_variable(canonical_lookup);
         }

         if (not module_variable) {
            for (const auto &entry : module_info->prolog->variables) {
               const auto &candidate = entry.second;
               if (candidate.qname IS name) {
                  module_variable = &candidate;
                  break;
               }

               if (not canonical_lookup.empty() and (candidate.qname IS canonical_lookup)) {
                  module_variable = &candidate;
                  break;
               }

               size_t colon_pos = candidate.qname.find(':');
               if ((colon_pos != std::string::npos) and !imported_local_name.empty()) {
                  std::string candidate_prefix = candidate.qname.substr(0, colon_pos);
                  std::string candidate_local = candidate.qname.substr(colon_pos + 1);
                  if (candidate_local IS imported_local_name) {
                     uint32_t candidate_hash = module_info->prolog->resolve_prefix(candidate_prefix, nullptr);
                     if (candidate_hash IS namespace_hash) {
                        module_variable = &candidate;
                        break;
                     }
                  }
               }
            }
         }

         if (not module_variable) {
            std::string message = "Module variable '" + name + "' is not declared by namespace '" + module_uri + "'.";
            record_error(message, ReferenceNode, true);
            return false;
         }

         variable = module_variable;
         owner_prolog = module_info->prolog;
         active_module_cache = module_cache;
      }

      if (not variable) return false;
   }

   if (owner_prolog) {
      if (not canonical_lookup.empty()) normalised_name = canonical_lookup;
      else {
         normalised_name = canonicalise_variable_qname(name, *owner_prolog, context.document);
         if (normalised_name IS name) {
            normalised_name = canonicalise_variable_qname(variable->qname, *owner_prolog, context.document);
         }
      }
   }

   auto cached_value = prolog_variable_cache.find(normalised_name);
   if (cached_value != prolog_variable_cache.end()) {
      OutValue = cached_value->second;
      return true;
   }

   if (normalised_name != name) {
      auto alias_value = prolog_variable_cache.find(name);
      if (alias_value != prolog_variable_cache.end()) {
         prolog_variable_cache.insert_or_assign(normalised_name, alias_value->second);
         OutValue = alias_value->second;
         return true;
      }
   }

   if (variable->qname != normalised_name) {
      auto declared_value = prolog_variable_cache.find(variable->qname);
      if (declared_value != prolog_variable_cache.end()) {
         prolog_variable_cache.insert_or_assign(normalised_name, declared_value->second);
         if (normalised_name != name) {
            prolog_variable_cache.insert_or_assign(name, declared_value->second);
         }
         OutValue = declared_value->second;
         return true;
      }
   }

   if (variable->is_external) {
      std::string message = "External variable '" + name + "' is not supported.";
      record_error(message, ReferenceNode, true);
      return false;
   }

   if (not variable->initializer) {
      std::string message = "Variable '" + name + "' is missing an initialiser.";
      record_error(message, ReferenceNode, true);
      return false;
   }

   if (variables_in_evaluation.find(normalised_name) != variables_in_evaluation.end()) {
      std::string message = "Variable '" + name + "' has a circular dependency.";
      record_error(message, ReferenceNode, true);
      return false;
   }

   auto previous_prolog = context.prolog;
   auto previous_cache = context.module_cache;

   bool switched_context = false;
   if (owner_prolog and (owner_prolog.get() != previous_prolog.get())) {
      context.prolog = owner_prolog;
      if (active_module_cache) context.module_cache = active_module_cache;
      switched_context = true;
   }

   variables_in_evaluation.insert(normalised_name);
   XPathVal computed_value = evaluate_expression(variable->initializer.get(), CurrentPrefix);
   variables_in_evaluation.erase(normalised_name);

   if (switched_context) {
      context.prolog = previous_prolog;
      context.module_cache = previous_cache;
   }

   if (expression_unsupported) {
      std::string message = "Failed to evaluate initialiser for variable '" + name + "'.";
      record_error(message, ReferenceNode);
      return false;
   }

   auto inserted = prolog_variable_cache.insert_or_assign(normalised_name, computed_value);
   OutValue = inserted.first->second;

   if (normalised_name != name) {
      prolog_variable_cache.insert_or_assign(name, inserted.first->second);
   }

   if (variable->qname != normalised_name and variable->qname != name) {
      prolog_variable_cache.insert_or_assign(variable->qname, inserted.first->second);
   }

   return true;
}

//********************************************************************************************************************
// Evaluates an XPath/XQuery expression node and returns its computed value.  Responsibilities:
//
// - Dispatches on node kind (numbers, literals, constructors, paths, predicates, and control flow).
// - Preserves XPath semantics such as document order, short-circuiting (and/or), and context-sensitive
//   evaluation for filters, paths, and quantified/for expressions.
// - Integrates XQuery Prolog settings (ordering, construction, namespaces) and consults the module cache
//   when user-defined functions or variables require module resolution.
// - Uses push_context/pop_context to manage the evaluation context for node-set operations and predicates.
// - Signals unsupported constructs via 'expression_unsupported' and reports diagnostics with record_error().
// - Produces results as XPathVal, including node-set values with associated attribute/string metadata.
//
// Notes:
// - Function is side-effect free for input XML; constructed text nodes are owned by 'constructed_nodes'.
// - Returns empty values on failure paths; callers must check 'expression_unsupported' when necessary.

XPathVal XPathEvaluator::evaluate_expression(const XPathNode *ExprNode, uint32_t CurrentPrefix)
{
   pf::Log log("XPath");

   if (not ExprNode) {
      record_error("Unsupported XPath expression: empty node", (const XPathNode *)nullptr, true);
      return XPathVal();
   }

   auto matches_sequence_type = [&](const XPathVal &Value, const SequenceTypeInfo &SequenceInfo, const XPathNode *ContextNode)
      -> std::optional<bool>
   {
      size_t item_count = sequence_item_count(Value);

      if (SequenceInfo.kind IS SequenceItemKind::EmptySequence) return item_count IS 0;

      if ((item_count IS 0) and (not SequenceInfo.allows_empty())) return false;
      if ((item_count > 1) and (not SequenceInfo.allows_multiple())) return false;
      if ((SequenceInfo.occurrence IS SequenceCardinality::ExactlyOne) and (item_count != 1)) return false;

      if (item_count IS 0) return true;

      if (SequenceInfo.kind IS SequenceItemKind::Item) return true;

      if (SequenceInfo.kind IS SequenceItemKind::Node) {
         if (Value.Type IS XPVT::NodeSet) {
            for (size_t index = 0; index < item_count; ++index) {
               const XMLAttrib *attribute = (index < Value.node_set_attributes.size()) ?
                  Value.node_set_attributes[index] : nullptr;
               XMLTag *node = (index < Value.node_set.size()) ? Value.node_set[index] : nullptr;

               if (attribute) continue;
               if (!node) return false;
               if (is_constructed_scalar_text(node)) return false;
            }
            return true;
         }
         return false;
      }

      if (SequenceInfo.kind IS SequenceItemKind::Element) {
         if (Value.Type IS XPVT::NodeSet) {
            for (size_t index = 0; index < item_count; ++index) {
               const XMLAttrib *attribute = (index < Value.node_set_attributes.size()) ?
                  Value.node_set_attributes[index] : nullptr;
               XMLTag *node = (index < Value.node_set.size()) ? Value.node_set[index] : nullptr;
               if (attribute or (not node) or (not node->isTag())) return false;
            }
            return true;
         }
         return false;
      }

      if (SequenceInfo.kind IS SequenceItemKind::Attribute) {
         if (Value.Type IS XPVT::NodeSet) {
            for (size_t index = 0; index < item_count; ++index) {
               const XMLAttrib *attribute = (index < Value.node_set_attributes.size()) ?
                  Value.node_set_attributes[index] : nullptr;
               if (!attribute) return false;
            }
            return true;
         }
         return false;
      }

      if (SequenceInfo.kind IS SequenceItemKind::Text) {
         if (Value.Type IS XPVT::NodeSet) {
            for (size_t index = 0; index < item_count; ++index) {
               const XMLAttrib *attribute = (index < Value.node_set_attributes.size()) ?
                  Value.node_set_attributes[index] : nullptr;
               XMLTag *node = (index < Value.node_set.size()) ? Value.node_set[index] : nullptr;
               if (attribute or (not is_text_node(node))) return false;
            }
            return true;
         }
         return false;
      }

      auto &registry = xml::schema::registry();
      auto target_descriptor = registry.find_descriptor(SequenceInfo.type_name);
      if (!target_descriptor) {
         auto message = std::format("XPST0052: Sequence type '{}' is not defined.", SequenceInfo.type_name);
         record_error(message, ContextNode, true);
         return std::nullopt;
      }

      xml::schema::TypeChecker checker(registry);

      if (Value.Type IS XPVT::NodeSet) {
         for (size_t index = 0; index < item_count; ++index) {
            const XMLAttrib *attribute = (index < Value.node_set_attributes.size()) ?
               Value.node_set_attributes[index] : nullptr;
            XMLTag *node = (index < Value.node_set.size()) ? Value.node_set[index] : nullptr;

            if (attribute) return false;
            if (!node) return false;
            if (!is_constructed_scalar_text(node)) return false;

            std::string lexical = nodeset_item_string(Value, index);
            XPathVal item_value(lexical);
            if (not checker.validate_value(item_value, *target_descriptor)) return false;
         }
         return true;
      }

      auto target_schema = target_descriptor->schema_type;
      auto value_schema = Value.get_schema_type();
      auto value_descriptor = registry.find_descriptor(value_schema);

      auto is_boolean_schema = [](xml::schema::SchemaType Type) noexcept
      {
         return (Type IS xml::schema::SchemaType::XPathBoolean) or (Type IS xml::schema::SchemaType::XSBoolean);
      };

      if (xml::schema::is_numeric(target_schema)) {
         if (not xml::schema::is_numeric(value_schema)) return false;
      }
      else if (is_boolean_schema(target_schema)) {
         if (not is_boolean_schema(value_schema)) return false;
      }
      else if (xml::schema::is_string_like(target_schema)) {
         if (not xml::schema::is_string_like(value_schema)) return false;
      }
      else if (value_descriptor) {
         if (not value_descriptor->is_derived_from(target_schema) and not target_descriptor->is_derived_from(value_schema)) {
            return false;
         }
      }

      if (not checker.validate_value(Value, *target_descriptor)) return false;
      return true;
   };

   // Use a switch for common node kinds for clarity and consistent early-returns
   switch (ExprNode->type) {
      case XPathNodeType::EMPTY_SEQUENCE: {
         // Return an empty node-set to represent the empty sequence
         return XPathVal(pf::vector<XMLTag *>{});
      }
      case XPathNodeType::NUMBER: {
         char *end_ptr = nullptr;
         double value = std::strtod(ExprNode->value.c_str(), &end_ptr);
         if ((end_ptr) and (*end_ptr IS '\0')) return XPathVal(value);
         return XPathVal(std::numeric_limits<double>::quiet_NaN());
      }
      case XPathNodeType::LITERAL:
      case XPathNodeType::STRING: {
         return XPathVal(ExprNode->value);
      }
      case XPathNodeType::DIRECT_ELEMENT_CONSTRUCTOR: {
         return evaluate_direct_element_constructor(ExprNode, CurrentPrefix);
      }
      case XPathNodeType::COMPUTED_ELEMENT_CONSTRUCTOR: {
         return evaluate_computed_element_constructor(ExprNode, CurrentPrefix);
      }
      case XPathNodeType::COMPUTED_ATTRIBUTE_CONSTRUCTOR: {
         return evaluate_computed_attribute_constructor(ExprNode, CurrentPrefix);
      }
      case XPathNodeType::TEXT_CONSTRUCTOR: {
         return evaluate_text_constructor(ExprNode, CurrentPrefix);
      }
      case XPathNodeType::COMMENT_CONSTRUCTOR: {
         return evaluate_comment_constructor(ExprNode, CurrentPrefix);
      }
      case XPathNodeType::PI_CONSTRUCTOR: {
         return evaluate_pi_constructor(ExprNode, CurrentPrefix);
      }
      case XPathNodeType::DOCUMENT_CONSTRUCTOR: {
         return evaluate_document_constructor(ExprNode, CurrentPrefix);
      }
      case XPathNodeType::LOCATION_PATH: {
         return evaluate_path_expression_value(ExprNode, CurrentPrefix);
      }
      case XPathNodeType::CAST_EXPRESSION: {
         if (ExprNode->child_count() IS 0) {
            record_error("Cast expression requires an operand.", ExprNode, true);
            return XPathVal();
         }

         auto target_info = parse_cast_target_literal(ExprNode->value);
         if (target_info.type_name.empty()) {
            record_error("XPST0003: Cast expression is missing its target type.", ExprNode, true);
            return XPathVal();
         }

         auto &registry = xml::schema::registry();
         auto target_descriptor = registry.find_descriptor(target_info.type_name);
         if (!target_descriptor) {
            auto message = std::format("XPST0052: Cast target type '{}' is not defined.", target_info.type_name);
            record_error(message, ExprNode, true);
            return XPathVal();
         }

         const XPathNode *operand_node = ExprNode->get_child(0);
         if (!operand_node) {
            record_error("Cast expression requires an operand.", ExprNode, true);
            return XPathVal();
         }

         XPathVal operand_value = evaluate_expression(operand_node, CurrentPrefix);
         if (expression_unsupported) return XPathVal();

         if (operand_value.Type IS XPVT::NodeSet) {
            size_t item_count = operand_value.node_set.size();
            if (item_count IS 0) {
               if (target_info.allows_empty) return XPathVal(pf::vector<XMLTag *>{});
               auto message = std::format("XPTY0004: Cast to '{}' requires a single item, but the operand was empty.",
                  target_descriptor->type_name);
               record_error(message, ExprNode, true);
               return XPathVal();
            }

            if (item_count > 1) {
               auto message = std::format("XPTY0004: Cast to '{}' requires a single item, but the operand had {} items.",
                  target_descriptor->type_name, item_count);
               record_error(message, ExprNode, true);
               return XPathVal();
            }

            std::string atomised_string = operand_value.to_string();
            operand_value = XPathVal(atomised_string);
            auto string_descriptor = registry.find_descriptor(xml::schema::SchemaType::XPathString);
            if (string_descriptor) operand_value.set_schema_type(string_descriptor);
         }

         auto source_descriptor = schema_descriptor_for_value(operand_value);
         if (!source_descriptor) source_descriptor = registry.find_descriptor(xml::schema::schema_type_for_xpath(operand_value.Type));
         if (!source_descriptor) {
            record_error("XPTY0006: Cast operand type could not be determined.", ExprNode, true);
            return XPathVal();
         }

         

         std::string operand_lexical = operand_value.to_string();
         XPathVal coerced = source_descriptor->coerce_value(operand_value, target_descriptor->schema_type);

         if (xml::schema::is_numeric(target_descriptor->schema_type)) {
            double numeric_value = coerced.to_number();
            if (std::isnan(numeric_value)) {
               auto message = std::format("XPTY0006: Value '{}' cannot be cast to numeric type '{}'.",
                  operand_lexical, target_descriptor->type_name);
               record_error(message, ExprNode, true);
               return XPathVal();
            }
            coerced = XPathVal(numeric_value);
         }
         else if ((target_descriptor->schema_type IS xml::schema::SchemaType::XPathBoolean) or
                  (target_descriptor->schema_type IS xml::schema::SchemaType::XSBoolean)) {
            bool lexical_valid = true;
            bool boolean_result = coerced.to_boolean();

            if (operand_value.Type IS XPVT::String) {
               auto parsed_boolean = parse_schema_boolean(operand_lexical);
               if (not parsed_boolean.has_value()) lexical_valid = false;
               else boolean_result = *parsed_boolean;
            }

            if (not lexical_valid) {
               auto message = std::format("XPTY0006: Value '{}' cannot be cast to boolean type '{}'.",
                  operand_lexical, target_descriptor->type_name);
               record_error(message, ExprNode, true);
               return XPathVal();
            }

            coerced = XPathVal(boolean_result);
         }
         else if ((target_descriptor->schema_type IS xml::schema::SchemaType::XPathString) or
                  (target_descriptor->schema_type IS xml::schema::SchemaType::XSString)) {
            coerced = XPathVal(operand_lexical);
         }

         coerced.set_schema_type(target_descriptor);
         return coerced;
      }
      case XPathNodeType::TREAT_AS_EXPRESSION: {
         if (ExprNode->child_count() IS 0) {
            record_error("Treat as expression requires an operand.", ExprNode, true);
            return XPathVal();
         }

         auto sequence_info = parse_sequence_type_literal(ExprNode->value);
         if (not sequence_info.has_value()) {
            record_error("XPST0003: Treat as expression is missing its sequence type.", ExprNode, true);
            return XPathVal();
         }

         const XPathNode *operand_node = ExprNode->get_child(0);
         if (!operand_node) {
            record_error("Treat as expression requires an operand.", ExprNode, true);
            return XPathVal();
         }

         XPathVal operand_value = evaluate_expression(operand_node, CurrentPrefix);
         if (expression_unsupported) return XPathVal();

         size_t item_count = sequence_item_count(operand_value);

         if (sequence_info->kind IS SequenceItemKind::EmptySequence) {
            if (item_count IS 0) return operand_value;

            auto message = std::format("XPTY0004: Treat as expression for 'empty-sequence()' requires an empty operand, "
               "but it contained {} item(s).", item_count);
            record_error(message, ExprNode, true);
            return XPathVal();
         }

         if ((item_count IS 0) and (not sequence_info->allows_empty())) {
            auto message = std::format("XPTY0004: Treat as expression for '{}' requires at least one item, "
               "but the operand was empty.", ExprNode->value);
            record_error(message, ExprNode, true);
            return XPathVal();
         }

         if ((item_count > 1) and (not sequence_info->allows_multiple())) {
            auto message = std::format("XPTY0004: Treat as expression for '{}' allows at most one item, "
               "but the operand had {} item(s).", ExprNode->value, item_count);
            record_error(message, ExprNode, true);
            return XPathVal();
         }

         if ((sequence_info->occurrence IS SequenceCardinality::ExactlyOne) and (item_count != 1)) {
            auto message = std::format("XPTY0004: Treat as expression for '{}' requires exactly one item, "
               "but the operand had {} item(s).", ExprNode->value, item_count);
            record_error(message, ExprNode, true);
            return XPathVal();
         }

         if (item_count IS 0) return operand_value;

         if (sequence_info->kind IS SequenceItemKind::Item) return operand_value;

         if (sequence_info->kind IS SequenceItemKind::Node) {
            if (operand_value.Type IS XPVT::NodeSet) return operand_value;

            auto message = std::format("XPTY0004: Treat as expression for 'node()' requires node values, but received '{}'.",
               operand_value.to_string());
            record_error(message, ExprNode, true);
            return XPathVal();
         }

         if (sequence_info->kind IS SequenceItemKind::Element) {
            if (operand_value.Type IS XPVT::NodeSet) {
               for (size_t index = 0; index < operand_value.node_set.size(); ++index) {
                  const XMLAttrib *attribute = (index < operand_value.node_set_attributes.size()) ?
                     operand_value.node_set_attributes[index] : nullptr;
                  XMLTag *node = operand_value.node_set[index];

                  if (attribute or (not node) or (not node->isTag())) {
                     auto encountered = describe_nodeset_item_kind(node, attribute);
                     auto message = std::format("XPTY0004: Treat as expression for 'element()' encountered {}.", encountered);
                     record_error(message, ExprNode, true);
                     return XPathVal();
                  }
               }

               return operand_value;
            }

            auto message = std::format("XPTY0004: Treat as expression for 'element()' requires node values, "
               "but received '{}'.", operand_value.to_string());
            record_error(message, ExprNode, true);
            return XPathVal();
         }

         if (sequence_info->kind IS SequenceItemKind::Attribute) {
            if (operand_value.Type IS XPVT::NodeSet) {
               for (size_t index = 0; index < operand_value.node_set.size(); ++index) {
                  const XMLAttrib *attribute = (index < operand_value.node_set_attributes.size()) ?
                     operand_value.node_set_attributes[index] : nullptr;
                  if (not attribute) {
                     auto node = operand_value.node_set[index];
                     auto encountered = describe_nodeset_item_kind(node, nullptr);
                     auto message = std::format("XPTY0004: Treat as expression for 'attribute()' encountered {}.", encountered);
                     record_error(message, ExprNode, true);
                     return XPathVal();
                  }
               }

               return operand_value;
            }

            auto message = std::format("XPTY0004: Treat as expression for 'attribute()' requires attribute nodes, "
               "but received '{}'.", operand_value.to_string());
            record_error(message, ExprNode, true);
            return XPathVal();
         }

         if (sequence_info->kind IS SequenceItemKind::Text) {
            if (operand_value.Type IS XPVT::NodeSet) {
               for (size_t index = 0; index < operand_value.node_set.size(); ++index) {
                  const XMLAttrib *attribute = (index < operand_value.node_set_attributes.size()) ?
                     operand_value.node_set_attributes[index] : nullptr;
                  XMLTag *node = operand_value.node_set[index];

                  bool is_text_node = node and (not node->Attribs.empty()) and node->Attribs[0].Name.empty();
                  if (attribute or (not is_text_node)) {
                     auto encountered = describe_nodeset_item_kind(node, attribute);
                     auto message = std::format("XPTY0004: Treat as expression for 'text()' encountered {}.", encountered);
                     record_error(message, ExprNode, true);
                     return XPathVal();
                  }
               }

               return operand_value;
            }

            auto message = std::format("XPTY0004: Treat as expression for 'text()' requires text nodes, but received '{}'.",
               operand_value.to_string());
            record_error(message, ExprNode, true);
            return XPathVal();
         }

         // Atomic sequence handling
         auto &registry = xml::schema::registry();
         auto target_descriptor = registry.find_descriptor(sequence_info->type_name);
         if (!target_descriptor) {
            auto message = std::format("XPST0052: Treat target type '{}' is not defined.", sequence_info->type_name);
            record_error(message, ExprNode, true);
            return XPathVal();
         }

         xml::schema::TypeChecker checker(registry);

         if (operand_value.Type IS XPVT::NodeSet) {
            size_t length = sequence_item_count(operand_value);
            for (size_t index = 0; index < length; ++index) {
               const XMLAttrib *attribute = (index < operand_value.node_set_attributes.size()) ?
                  operand_value.node_set_attributes[index] : nullptr;
               XMLTag *node = (index < operand_value.node_set.size()) ? operand_value.node_set[index] : nullptr;

               // Attributes are never atomic for treat as
               if (attribute) {
                  auto encountered = describe_nodeset_item_kind(node, attribute);
                  auto message = std::format(
                     "XPTY0004: Treat as expression for '{}' encountered {} which is not an atomic value.",
                     ExprNode->value, encountered);
                  record_error(message, ExprNode, true);
                  return XPathVal();
               }

               // Permit constructed scalar placeholders produced by comma sequences: text nodes with no parent
               if (node) {
                  bool is_text_node = (not node->Attribs.empty()) and node->Attribs[0].Name.empty();
                  bool constructed_scalar = is_text_node and (node->ParentID IS 0);
                  if (not constructed_scalar) {
                     auto encountered = describe_nodeset_item_kind(node, attribute);
                     auto message = std::format(
                        "XPTY0004: Treat as expression for '{}' encountered {} which is not an atomic value.",
                        ExprNode->value, encountered);
                     record_error(message, ExprNode, true);
                     return XPathVal();
                  }
               }

               // Validate the lexical form of each atomic item against the target type
               std::string lexical = nodeset_item_string(operand_value, index);
               XPathVal item_value(lexical);
               if (not checker.validate_value(item_value, *target_descriptor)) {
                  std::string detail = checker.last_error();
                  if (detail.empty()) detail = std::format("Value '{}' is not valid for type {}.", lexical, target_descriptor->type_name);
                  auto message = std::format("XPTY0004: {}", detail);
                  record_error(message, ExprNode, true);
                  return XPathVal();
               }
            }

            // Operand already represents an atomic sequence in this value model; return unchanged
            return operand_value;
         }

         if (not checker.validate_value(operand_value, *target_descriptor)) {
            std::string detail = checker.last_error();
            if (detail.empty()) detail = std::format("Value '{}' is not valid for type {}.", operand_value.to_string(),
               target_descriptor->type_name);
            auto message = std::format("XPTY0004: {}", detail);
            record_error(message, ExprNode, true);
            return XPathVal();
         }

         operand_value.set_schema_type(target_descriptor);
         return operand_value;
      }
      case XPathNodeType::INSTANCE_OF_EXPRESSION: {
         if (ExprNode->child_count() IS 0) {
            record_error("Instance of expression requires an operand.", ExprNode, true);
            return XPathVal();
         }

         auto sequence_info = parse_sequence_type_literal(ExprNode->value);
         if (not sequence_info.has_value()) {
            record_error("XPST0003: Instance of expression is missing its sequence type.", ExprNode, true);
            return XPathVal();
         }

         const XPathNode *operand_node = ExprNode->get_child(0);
         if (!operand_node) {
            record_error("Instance of expression requires an operand.", ExprNode, true);
            return XPathVal();
         }

         XPathVal operand_value = evaluate_expression(operand_node, CurrentPrefix);
         if (expression_unsupported) return XPathVal();

         auto match_result = matches_sequence_type(operand_value, *sequence_info, ExprNode);
         if (not match_result.has_value()) return XPathVal();
         return XPathVal(*match_result);
      }
      case XPathNodeType::CASTABLE_EXPRESSION: {
         if (ExprNode->child_count() IS 0) {
            record_error("Castable expression requires an operand.", ExprNode, true);
            return XPathVal();
         }

         auto target_info = parse_cast_target_literal(ExprNode->value);
         if (target_info.type_name.empty()) {
            record_error("XPST0003: Castable expression is missing its target type.", ExprNode, true);
            return XPathVal();
         }

         auto &registry = xml::schema::registry();
         std::shared_ptr<xml::schema::SchemaTypeDescriptor> target_descriptor;

         auto cache_entry = cast_target_cache.find(target_info.type_name);
         if (cache_entry != cast_target_cache.end()) target_descriptor = cache_entry->second.lock();

         if (!target_descriptor) {
            target_descriptor = registry.find_descriptor(target_info.type_name);
            if (!target_descriptor) {
               auto message = std::format("XPST0052: Cast target type '{}' is not defined.", target_info.type_name);
               record_error(message, ExprNode, true);
               return XPathVal();
            }
            cast_target_cache.insert_or_assign(target_info.type_name, target_descriptor);
         }

         const XPathNode *operand_node = ExprNode->get_child(0);
         if (!operand_node) {
            record_error("Castable expression requires an operand.", ExprNode, true);
            return XPathVal();
         }

         XPathVal operand_value = evaluate_expression(operand_node, CurrentPrefix);
         if (expression_unsupported) return XPathVal();

         if (operand_value.Type IS XPVT::NodeSet) {
            size_t item_count = operand_value.node_set.size();
            if (item_count IS 0) return XPathVal(target_info.allows_empty);
            if (item_count > 1) return XPathVal(false);

            std::string atomised_string = operand_value.to_string();
            operand_value = XPathVal(atomised_string);
            auto string_descriptor = registry.find_descriptor(xml::schema::SchemaType::XPathString);
            if (string_descriptor) operand_value.set_schema_type(string_descriptor);
         }

         auto source_descriptor = schema_descriptor_for_value(operand_value);
         if (!source_descriptor) source_descriptor = registry.find_descriptor(xml::schema::schema_type_for_xpath(operand_value.Type));
         if (!source_descriptor) return XPathVal(false);

         std::string operand_lexical = operand_value.to_string();
         bool castable_success = is_value_castable_to_type(operand_value, source_descriptor, target_descriptor, operand_lexical);
         return XPathVal(castable_success);
      }
      case XPathNodeType::TYPESWITCH_EXPRESSION: {
         if (ExprNode->child_count() < 2) {
            record_error("Typeswitch expression requires at least one clause.", ExprNode, true);
            return XPathVal();
         }

         const XPathNode *operand_node = ExprNode->get_child(0);
         if (!operand_node) {
            record_error("Typeswitch expression is missing its operand.", ExprNode, true);
            return XPathVal();
         }

         XPathVal operand_value = evaluate_expression(operand_node, CurrentPrefix);
         if (expression_unsupported) return XPathVal();

         const XPathNode *default_clause = nullptr;

         for (size_t index = 1; index < ExprNode->child_count(); ++index) {
            const XPathNode *clause_node = ExprNode->get_child(index);
            if (!clause_node) continue;

            if (clause_node->type IS XPathNodeType::TYPESWITCH_CASE) {
               const auto *info = clause_node->get_typeswitch_case_info();
               if ((not info) or (not info->has_sequence_type())) {
                  record_error("Typeswitch case clause is missing its sequence type.", clause_node, true);
                  return XPathVal();
               }

               auto sequence_info = parse_sequence_type_literal(info->sequence_type);
               if (not sequence_info.has_value()) {
                  record_error("XPST0003: Typeswitch case sequence type could not be parsed.", clause_node, true);
                  return XPathVal();
               }

               auto match_result = matches_sequence_type(operand_value, *sequence_info, clause_node);
               if (not match_result.has_value()) return XPathVal();
               if (*match_result) {
                  if (clause_node->child_count() IS 0) {
                     record_error("Typeswitch case clause requires a return expression.", clause_node, true);
                     return XPathVal();
                  }

                  const XPathNode *branch_expr = clause_node->get_child(0);
                  if (!branch_expr) {
                     record_error("Typeswitch case clause requires a return expression.", clause_node, true);
                     return XPathVal();
                  }

                  std::optional<VariableBindingGuard> binding_guard;
                  if (info->has_variable()) binding_guard.emplace(context, info->variable_name, operand_value);

                  auto branch_value = evaluate_expression(branch_expr, CurrentPrefix);
                  if (expression_unsupported) return XPathVal();
                  return branch_value;
               }

               continue;
            }

            if (clause_node->type IS XPathNodeType::TYPESWITCH_DEFAULT_CASE) {
               default_clause = clause_node;
               continue;
            }

            record_error("Typeswitch expression encountered an unknown clause.", clause_node, true);
            return XPathVal();
         }

         if (!default_clause) {
            record_error("Typeswitch expression requires a default clause.", ExprNode, true);
            return XPathVal();
         }

         if (default_clause->child_count() IS 0) {
            record_error("Typeswitch default clause requires a return expression.", default_clause, true);
            return XPathVal();
         }

         const XPathNode *default_expr = default_clause->get_child(0);
         if (!default_expr) {
            record_error("Typeswitch default clause requires a return expression.", default_clause, true);
            return XPathVal();
         }

         std::optional<VariableBindingGuard> default_guard;
         const auto *default_info = default_clause->get_typeswitch_case_info();
         if (default_info and default_info->has_variable()) {
            default_guard.emplace(context, default_info->variable_name, operand_value);
         }

         auto default_value = evaluate_expression(default_expr, CurrentPrefix);
         if (expression_unsupported) return XPathVal();
         return default_value;
      }
      case XPathNodeType::UNION: {
         std::vector<const XPathNode *> branches;
         branches.reserve(ExprNode->child_count());
         for (size_t index = 0; index < ExprNode->child_count(); ++index) {
            auto *branch = ExprNode->get_child(index);
            if (branch) branches.push_back(branch);
         }
         return evaluate_union_value(branches, CurrentPrefix);
      }
      case XPathNodeType::CONDITIONAL: {
         if (ExprNode->child_count() < 3) {
            expression_unsupported = true;
            return XPathVal();
         }
         auto *condition_node = ExprNode->get_child(0);
         auto *then_node = ExprNode->get_child(1);
         auto *else_node = ExprNode->get_child(2);
         if ((not condition_node) or (not then_node) or (not else_node)) {
            expression_unsupported = true;
            return XPathVal();
         }
         auto condition_value = evaluate_expression(condition_node, CurrentPrefix);
         if (expression_unsupported) return XPathVal();
         bool condition_boolean = condition_value.to_boolean();
         auto *selected_node = condition_boolean ? then_node : else_node;
         auto branch_value = evaluate_expression(selected_node, CurrentPrefix);
         return branch_value;
      }
      default: break; // handled below
   }

   // LET expressions share the same diagnostic surface as the parser.  Whenever a binding fails we populate
   // extXML::ErrorMsg so Fluid callers receive precise feedback rather than generic failure codes.

   if (ExprNode->type IS XPathNodeType::LET_EXPRESSION) {
      if (ExprNode->child_count() < 2) {
         record_error("LET expression requires at least one binding and a return clause.", ExprNode, true);
         return XPathVal();
      }

      const XPathNode *return_node = ExprNode->get_child(ExprNode->child_count() - 1);
      if (not return_node) {
         record_error("LET expression is missing its return clause.", ExprNode, true);
         return XPathVal();
      }

      std::vector<VariableBindingGuard> binding_guards;
      binding_guards.reserve(ExprNode->child_count() - 1);

      for (size_t index = 0; index + 1 < ExprNode->child_count(); ++index) {
         const XPathNode *binding_node = ExprNode->get_child(index);
         if ((not binding_node) or !(binding_node->type IS XPathNodeType::LET_BINDING)) {
            record_error("LET expression contains an invalid binding clause.", binding_node ? binding_node : ExprNode, true);
            return XPathVal();
         }

         if ((binding_node->value.empty()) or (binding_node->child_count() IS 0)) {
            record_error("Let binding requires a variable name and expression.", binding_node, true);
            return XPathVal();
         }

         const XPathNode *binding_expr = binding_node->get_child(0);
         if (not binding_expr) {
            record_error("Let binding requires an expression node.", binding_node, true);
            return XPathVal();
         }

         XPathVal bound_value = evaluate_expression(binding_expr, CurrentPrefix);
         if (expression_unsupported) {
            record_error("Let binding expression could not be evaluated.", binding_expr);
            return XPathVal();
         }

         binding_guards.emplace_back(context, binding_node->value, std::move(bound_value));
      }

      auto result_value = evaluate_expression(return_node, CurrentPrefix);
      if (expression_unsupported) {
         record_error("Let return expression could not be evaluated.", return_node);
         return XPathVal();
      }
      return result_value;
   }

   // FLWOR evaluation mirrors that approach, capturing structural and runtime issues so test_xpath_flwor.fluid can assert
   // on human-readable error text while we continue to guard performance-sensitive paths.

   if (ExprNode->type IS XPathNodeType::FLWOR_EXPRESSION) {
      return evaluate_flwor_pipeline(ExprNode, CurrentPrefix);
   }

   if (ExprNode->type IS XPathNodeType::FOR_EXPRESSION) {
      if (ExprNode->child_count() < 2) {
         expression_unsupported = true;
         return XPathVal();
      }

      const XPathNode *return_node = ExprNode->get_child(ExprNode->child_count() - 1);
      if (not return_node) {
         expression_unsupported = true;
         return XPathVal();
      }

      std::vector<ForBindingDefinition> bindings;
      bindings.reserve(ExprNode->child_count());

      // Support both the current and historical AST layouts for simple for-expressions.
      // Older parsers encoded a single binding by placing the variable name in
      // ExprNode->value and the iteration sequence at child(0), with the return
      // expression as the last child. Newer trees emit one or more explicit
      // FOR_BINDING children followed by the return expression. The flag below
      // allows the evaluator to accept either form for backwards compatibility.

      bool legacy_layout = false;

      for (size_t index = 0; index + 1 < ExprNode->child_count(); ++index) {
         const XPathNode *binding_node = ExprNode->get_child(index);
         if (binding_node and (binding_node->type IS XPathNodeType::FOR_BINDING)) {
            if ((binding_node->value.empty()) or (binding_node->child_count() IS 0)) {
               expression_unsupported = true;
               return XPathVal();
            }

            bindings.push_back({ binding_node->value, binding_node->get_child(0) });
            continue;
         }

         // Encountered a child that is not a FOR_BINDING: treat the node
         // as using the legacy single-binding layout described above.

         legacy_layout = true;
         break;
      }

      if (legacy_layout) {
         if (ExprNode->child_count() < 2) {
            expression_unsupported = true;
            return XPathVal();
         }

         const XPathNode *sequence_node = ExprNode->get_child(0);
         if ((not sequence_node) or (not return_node) or ExprNode->value.empty()) {
            expression_unsupported = true;
            return XPathVal();
         }

         bindings.clear();
         bindings.push_back({ ExprNode->value, sequence_node });
      }

      if (bindings.empty()) {
         expression_unsupported = true;
         return XPathVal();
      }

      NODES combined_nodes;
      std::vector<std::string> combined_strings;
      std::vector<const XMLAttrib *> combined_attributes;
      std::optional<std::string> combined_override;

      bool evaluation_ok = evaluate_for_bindings_recursive(*this, context, bindings, 0, return_node, CurrentPrefix,
         combined_nodes, combined_attributes, combined_strings, combined_override, expression_unsupported);
      if (not evaluation_ok) return XPathVal();
      if (expression_unsupported) return XPathVal();

      XPathVal result;
      result.Type = XPVT::NodeSet;
      result.preserve_node_order = false;
      result.node_set = std::move(combined_nodes);
      result.node_set_string_values = std::move(combined_strings);
      result.node_set_attributes = std::move(combined_attributes);
      if (combined_override.has_value()) result.node_set_string_override = combined_override;
      else result.node_set_string_override.reset();
      return result;
   }

   if (ExprNode->type IS XPathNodeType::QUANTIFIED_EXPRESSION) {
      if (ExprNode->child_count() < 2) {
         expression_unsupported = true;
         return XPathVal();
      }

      bool is_some = ExprNode->value IS "some";
      bool is_every = ExprNode->value IS "every";

      if ((not is_some) and (not is_every)) {
         expression_unsupported = true;
         return XPathVal();
      }

      const XPathNode *condition_node = ExprNode->get_child(ExprNode->child_count() - 1);
      if (not condition_node) {
         expression_unsupported = true;
         return XPathVal();
      }

      std::vector<QuantifiedBindingDefinition> bindings;
      bindings.reserve(ExprNode->child_count());

      for (size_t index = 0; index + 1 < ExprNode->child_count(); ++index) {
         const XPathNode *binding_node = ExprNode->get_child(index);
         if ((not binding_node) or (binding_node->type != XPathNodeType::QUANTIFIED_BINDING)) {
            expression_unsupported = true;
            return XPathVal();
         }

         if ((binding_node->value.empty()) or (binding_node->child_count() IS 0)) {
            expression_unsupported = true;
            return XPathVal();
         }

         bindings.push_back({ binding_node->value, binding_node->get_child(0) });
      }

      if (bindings.empty()) {
         expression_unsupported = true;
         return XPathVal();
      }

      bool quant_result = evaluate_quantified_binding_recursive(*this, context, bindings, 0,
         is_some, is_every, condition_node, CurrentPrefix, expression_unsupported);
      if (expression_unsupported) return XPathVal();

      return XPathVal(quant_result);
   }

   if (ExprNode->type IS XPathNodeType::FILTER) {
      if (ExprNode->child_count() IS 0) {
         expression_unsupported = true;
         return XPathVal();
      }

      auto base_value = evaluate_expression(ExprNode->get_child(0), CurrentPrefix);
      if (expression_unsupported) return XPathVal();

      if (base_value.Type != XPVT::NodeSet) {
         expression_unsupported = true;
         return XPathVal();
      }

      std::vector<size_t> working_indices(base_value.node_set.size());
      for (size_t index = 0; index < working_indices.size(); ++index) {
         working_indices[index] = index;
      }

      for (size_t predicate_index = 1; predicate_index < ExprNode->child_count(); ++predicate_index) {
         auto *predicate_node = ExprNode->get_child(predicate_index);
         if (not predicate_node) continue;

         std::vector<size_t> passed;
         passed.reserve(working_indices.size());

         for (size_t position = 0; position < working_indices.size(); ++position) {
            size_t base_index = working_indices[position];
            XMLTag *candidate = base_value.node_set[base_index];
            const XMLAttrib *attribute = nullptr;
            if (base_index < base_value.node_set_attributes.size()) {
               attribute = base_value.node_set_attributes[base_index];
            }

            push_context(candidate, position + 1, working_indices.size(), attribute);
            auto predicate_result = evaluate_predicate(predicate_node, CurrentPrefix);
            pop_context();

            if (predicate_result IS PredicateResult::UNSUPPORTED) {
               expression_unsupported = true;
               return XPathVal();
            }

            if (predicate_result IS PredicateResult::MATCH) passed.push_back(base_index);
         }

         working_indices.swap(passed);
         if (working_indices.empty()) break;
      }

      NODES filtered_nodes;
      filtered_nodes.reserve(working_indices.size());

      std::vector<std::string> filtered_strings;
      filtered_strings.reserve(working_indices.size());

      std::vector<const XMLAttrib *> filtered_attributes;
      filtered_attributes.reserve(working_indices.size());

      for (size_t index : working_indices) {
         filtered_nodes.push_back(base_value.node_set[index]);
         if (index < base_value.node_set_string_values.size()) {
            filtered_strings.push_back(base_value.node_set_string_values[index]);
         }
         const XMLAttrib *attribute = nullptr;
         if (index < base_value.node_set_attributes.size()) {
            attribute = base_value.node_set_attributes[index];
         }
         filtered_attributes.push_back(attribute);
      }

      std::optional<std::string> first_value;
      if (not working_indices.empty()) {
         size_t first_index = working_indices[0];
         if (base_value.node_set_string_override.has_value() and (first_index IS 0)) {
            first_value = base_value.node_set_string_override;
         }
         else if (first_index < base_value.node_set_string_values.size()) {
            first_value = base_value.node_set_string_values[first_index];
         }
      }

      return XPathVal(filtered_nodes, first_value, std::move(filtered_strings), std::move(filtered_attributes));
   }

   if (ExprNode->type IS XPathNodeType::PATH) {
      if (ExprNode->child_count() IS 0) {
         expression_unsupported = true;
         return XPathVal();
      }

      auto *first_child = ExprNode->get_child(0);
      if (first_child and (first_child->type IS XPathNodeType::LOCATION_PATH)) {
         return evaluate_path_expression_value(ExprNode, CurrentPrefix);
      }

      auto base_value = evaluate_expression(first_child, CurrentPrefix);
      if (expression_unsupported) return XPathVal();

      if (base_value.Type != XPVT::NodeSet) {
         return XPathVal(base_value.to_node_set());
      }

      std::vector<const XPathNode *> steps;
      for (size_t index = 1; index < ExprNode->child_count(); ++index) {
         auto *child = ExprNode->get_child(index);
         if (child and (child->type IS XPathNodeType::STEP)) steps.push_back(child);
      }

      if (steps.empty()) return base_value;

      const XPathNode *attribute_step = nullptr;
      const XPathNode *attribute_test = nullptr;

      if (not steps.empty()) {
         auto *last_step = steps.back();
         const XPathNode *axis_node = nullptr;
         const XPathNode *node_test = nullptr;

         for (size_t index = 0; index < last_step->child_count(); ++index) {
            auto *child = last_step->get_child(index);
            if (not child) continue;

            if (child->type IS XPathNodeType::AXIS_SPECIFIER) axis_node = child;
            else if ((not node_test) and ((child->type IS XPathNodeType::NAME_TEST) or
                                       (child->type IS XPathNodeType::WILDCARD) or
                                       (child->type IS XPathNodeType::NODE_TYPE_TEST))) node_test = child;
         }

         AxisType axis = axis_node ? AxisEvaluator::parse_axis_name(axis_node->value) : AxisType::CHILD;
         if (axis IS AxisType::ATTRIBUTE) {
            attribute_step = last_step;
            attribute_test = node_test;
         }
      }

      return evaluate_path_from_nodes(base_value.node_set, base_value.node_set_attributes, steps, attribute_step,
         attribute_test, CurrentPrefix);
   }

   if (ExprNode->type IS XPathNodeType::FUNCTION_CALL) {
      auto value = evaluate_function_call(ExprNode, CurrentPrefix);
      if (expression_unsupported) return XPathVal();
      return value;
   }

   if (ExprNode->type IS XPathNodeType::UNARY_OP) {
      if (ExprNode->child_count() IS 0) {
         expression_unsupported = true;
         return XPathVal();
      }

      auto operand = evaluate_expression(ExprNode->get_child(0), CurrentPrefix);
      if (expression_unsupported) return XPathVal();

      if (ExprNode->value IS "-") return XPathVal(-operand.to_number());
      if (ExprNode->value IS "not") return XPathVal(not operand.to_boolean());

      expression_unsupported = true;
      return XPathVal();
   }

   if (ExprNode->type IS XPathNodeType::BINARY_OP) {
      if (ExprNode->child_count() < 2) {
         expression_unsupported = true;
         return XPathVal();
      }

      auto *left_node = ExprNode->get_child(0);
      auto *right_node = ExprNode->get_child(1);

      const std::string &operation = ExprNode->value;

      // Switch-based dispatch for common logical/set operations
      switch (map_binary_operation(operation)) {
         case BinaryOperationKind::AND: {
            auto left_value = evaluate_expression(left_node, CurrentPrefix);
            if (expression_unsupported) return XPathVal();
            bool left_boolean = left_value.to_boolean();
            if (not left_boolean) return XPathVal(false);
            auto right_value = evaluate_expression(right_node, CurrentPrefix);
            if (expression_unsupported) return XPathVal();
            bool right_boolean = right_value.to_boolean();
            return XPathVal(right_boolean);
         }
         case BinaryOperationKind::OR: {
            auto left_value = evaluate_expression(left_node, CurrentPrefix);
            if (expression_unsupported) return XPathVal();
            bool left_boolean = left_value.to_boolean();
            if (left_boolean) return XPathVal(true);
            auto right_value = evaluate_expression(right_node, CurrentPrefix);
            if (expression_unsupported) return XPathVal();
            bool right_boolean = right_value.to_boolean();
            return XPathVal(right_boolean);
         }
         case BinaryOperationKind::UNION: {
            std::vector<const XPathNode *> branches;
            branches.reserve(2);
            if (left_node) branches.push_back(left_node);
            if (right_node) branches.push_back(right_node);
            return evaluate_union_value(branches, CurrentPrefix);
         }
         case BinaryOperationKind::INTERSECT: {
            return evaluate_intersect_value(left_node, right_node, CurrentPrefix);
         }
         case BinaryOperationKind::EXCEPT: {
            return evaluate_except_value(left_node, right_node, CurrentPrefix);
         }
         default: {
            // Other operations handled below
            break;
         }
      }

      // and/or/union/intersect/except handled above via switch

      BinaryOperationKind op_kind = map_binary_operation(operation);

      if (op_kind IS BinaryOperationKind::COMMA) {
         auto left_value = evaluate_expression(left_node, CurrentPrefix);
         if (expression_unsupported) return XPathVal();
         auto right_value = evaluate_expression(right_node, CurrentPrefix);
         if (expression_unsupported) return XPathVal();

         std::vector<SequenceEntry> entries;
         entries.reserve(left_value.node_set.size() + right_value.node_set.size());

         append_value_to_sequence(left_value, entries, next_constructed_node_id, constructed_nodes);
         append_value_to_sequence(right_value, entries, next_constructed_node_id, constructed_nodes);

         if (entries.empty()) {
            NODES empty_nodes;
            return XPathVal(empty_nodes);
         }

         NODES combined_nodes;
         combined_nodes.reserve(entries.size());
         std::vector<const XMLAttrib *> combined_attributes;
         combined_attributes.reserve(entries.size());
         std::vector<std::string> combined_strings;
         combined_strings.reserve(entries.size());

         for (auto &entry : entries) {
            combined_nodes.push_back(entry.node);
            combined_attributes.push_back(entry.attribute);
            combined_strings.push_back(std::move(entry.string_value));
         }

         XPathVal result(combined_nodes, std::nullopt, std::move(combined_strings), std::move(combined_attributes));
         if (not prolog_ordering_is_ordered()) result.preserve_node_order = true;
         return result;
      }

      auto left_value = evaluate_expression(left_node, CurrentPrefix);
      if (expression_unsupported) return XPathVal();
      auto right_value = evaluate_expression(right_node, CurrentPrefix);
      if (expression_unsupported) return XPathVal();

      switch (op_kind) {
         case BinaryOperationKind::EQ: {
            bool equals = compare_xpath_values(left_value, right_value);
            return XPathVal(equals);
         }
         case BinaryOperationKind::NE: {
            bool equals = compare_xpath_values(left_value, right_value);
            return XPathVal(not equals);
         }
         case BinaryOperationKind::EQ_WORD: {
            auto left_scalar = promote_value_comparison_operand(left_value);
            auto right_scalar = promote_value_comparison_operand(right_value);
            if (not left_scalar.has_value() or !right_scalar.has_value()) return XPathVal(false);
            bool equals = compare_xpath_values(*left_scalar, *right_scalar);
            return XPathVal(equals);
         }
         case BinaryOperationKind::NE_WORD: {
            auto left_scalar = promote_value_comparison_operand(left_value);
            auto right_scalar = promote_value_comparison_operand(right_value);
            if (not left_scalar.has_value() or (not right_scalar.has_value())) return XPathVal(false);
            bool equals = compare_xpath_values(*left_scalar, *right_scalar);
            return XPathVal(not equals);
         }
         case BinaryOperationKind::LT: {
            // Handles both symbol '<' and textual 'lt'
            if (operation IS "lt") {
               auto left_scalar = promote_value_comparison_operand(left_value);
               auto right_scalar = promote_value_comparison_operand(right_value);
               if (not left_scalar.has_value() or (not right_scalar.has_value())) return XPathVal(false);
               bool result = compare_xpath_relational(*left_scalar, *right_scalar, RelationalOperator::LESS);
               return XPathVal(result);
            }
            bool result = compare_xpath_relational(left_value, right_value, RelationalOperator::LESS);
            return XPathVal(result);
         }
         case BinaryOperationKind::LE: {
            if (operation IS "le") {
               auto left_scalar = promote_value_comparison_operand(left_value);
               auto right_scalar = promote_value_comparison_operand(right_value);
               if (not left_scalar.has_value() or not right_scalar.has_value()) return XPathVal(false);
               bool result = compare_xpath_relational(*left_scalar, *right_scalar, RelationalOperator::LESS_OR_EQUAL);
               return XPathVal(result);
            }
            bool result = compare_xpath_relational(left_value, right_value, RelationalOperator::LESS_OR_EQUAL);
            return XPathVal(result);
         }
         case BinaryOperationKind::GT: {
            if (operation IS "gt") {
               auto left_scalar = promote_value_comparison_operand(left_value);
               auto right_scalar = promote_value_comparison_operand(right_value);
               if (not left_scalar.has_value() or not right_scalar.has_value()) return XPathVal(false);
               bool result = compare_xpath_relational(*left_scalar, *right_scalar, RelationalOperator::GREATER);
               return XPathVal(result);
            }
            bool result = compare_xpath_relational(left_value, right_value, RelationalOperator::GREATER);
            return XPathVal(result);
         }
         case BinaryOperationKind::GE: {
            if (operation IS "ge") {
               auto left_scalar = promote_value_comparison_operand(left_value);
               auto right_scalar = promote_value_comparison_operand(right_value);
               if (not left_scalar.has_value() or not right_scalar.has_value()) return XPathVal(false);
               bool result = compare_xpath_relational(*left_scalar, *right_scalar, RelationalOperator::GREATER_OR_EQUAL);
               return XPathVal(result);
            }
            bool result = compare_xpath_relational(left_value, right_value, RelationalOperator::GREATER_OR_EQUAL);
            return XPathVal(result);
         }
         case BinaryOperationKind::ADD: {
            double result = left_value.to_number() + right_value.to_number();
            return XPathVal(result);
         }
         case BinaryOperationKind::SUB: {
            double result = left_value.to_number() - right_value.to_number();
            return XPathVal(result);
         }
         case BinaryOperationKind::MUL: {
            double result = left_value.to_number() * right_value.to_number();
            return XPathVal(result);
         }
         case BinaryOperationKind::DIV: {
            double result = left_value.to_number() / right_value.to_number();
            return XPathVal(result);
         }
         case BinaryOperationKind::MOD: {
            double left_number = left_value.to_number();
            double right_number = right_value.to_number();
            double result = std::fmod(left_number, right_number);
            return XPathVal(result);
         }
         case BinaryOperationKind::RANGE: {
            size_t start_count = sequence_item_count(left_value);
            if (start_count IS 0) {
               record_error("XPTY0004: Range start requires a single numeric value, but the operand was empty.", ExprNode, true);
               return XPathVal();
            }
            if (start_count > 1) {
               auto message = std::format(
                  "XPTY0004: Range start requires a single numeric value, but the operand had {} items.", start_count);
               record_error(message, ExprNode, true);
               return XPathVal();
            }

            size_t end_count = sequence_item_count(right_value);
            if (end_count IS 0) {
               record_error("XPTY0004: Range end requires a single numeric value, but the operand was empty.", ExprNode, true);
               return XPathVal();
            }
            if (end_count > 1) {
               auto message = std::format(
                  "XPTY0004: Range end requires a single numeric value, but the operand had {} items.", end_count);
               record_error(message, ExprNode, true);
               return XPathVal();
            }

            double start_numeric = left_value.to_number();
            double end_numeric = right_value.to_number();

            if (not std::isfinite(start_numeric) or not std::isfinite(end_numeric)) {
               record_error("XPTY0004: Range boundaries must be finite numeric values.", ExprNode, true);
               return XPathVal();
            }

            double start_integral = 0.0;
            double end_integral = 0.0;
            double start_fraction = std::modf(start_numeric, &start_integral);
            double end_fraction = std::modf(end_numeric, &end_integral);

            auto fraction_is_zero = [](double Fraction) noexcept
            {
               return std::fabs(Fraction) <= std::numeric_limits<double>::epsilon();
            };

            if (not fraction_is_zero(start_fraction)) {
               auto lexical = left_value.to_string();
               auto message = std::format(
                  "XPTY0004: Range start value '{}' is not an integer.", lexical);
               record_error(message, ExprNode, true);
               return XPathVal();
            }

            if (not fraction_is_zero(end_fraction)) {
               auto lexical = right_value.to_string();
               auto message = std::format(
                  "XPTY0004: Range end value '{}' is not an integer.", lexical);
               record_error(message, ExprNode, true);
               return XPathVal();
            }

            if ((start_integral < (double)std::numeric_limits<int64_t>::min()) or
                (start_integral > (double)std::numeric_limits<int64_t>::max()) or
                (end_integral < (double)std::numeric_limits<int64_t>::min()) or
                (end_integral > (double)std::numeric_limits<int64_t>::max())) {
               record_error("FOAR0002: Range boundaries fall outside supported integer limits.", ExprNode, true);
               return XPathVal();
            }

            int64_t start_int = (int64_t)start_integral;
            int64_t end_int = (int64_t)end_integral;

            if (start_int > end_int) {
               NODES empty_nodes;
               XPathVal empty_result(empty_nodes);
               empty_result.preserve_node_order = true;
               return empty_result;
            }

            long double length_ld = static_cast<long double>(end_int) - static_cast<long double>(start_int) + 1.0L;
            if (length_ld > static_cast<long double>(RANGE_ITEM_LIMIT)) {
               auto start_lexical = format_xpath_number(start_numeric);
               auto end_lexical = format_xpath_number(end_numeric);
               auto message = std::format(
                  "FOAR0002: Range from {} to {} produces {:.0Lf} items which exceeds the supported limit of {}.",
                  start_lexical, end_lexical, length_ld, RANGE_ITEM_LIMIT);
               record_error(message, ExprNode, true);
               return XPathVal();
            }

            int64_t length = (int64_t)length_ld;

            NODES range_nodes;
            range_nodes.reserve((size_t)length);
            std::vector<std::string> range_strings;
            range_strings.reserve((size_t)length);

            for (int64_t value = start_int; value <= end_int; ++value) {
               range_nodes.push_back(nullptr);
               range_strings.push_back(format_xpath_number((double)value));
            }

            XPathVal range_result;
            range_result.Type = XPVT::NodeSet;
            range_result.preserve_node_order = true;
            range_result.node_set = std::move(range_nodes);
            range_result.node_set_string_values = std::move(range_strings);
            range_result.node_set_attributes.clear();
            range_result.node_set_string_override.reset();
            return range_result;
         }
         default: {
            expression_unsupported = true;
            return XPathVal();
         }
      }
   }

   // EXPRESSION nodes are wrappers - unwrap to the child node
   if (ExprNode->type IS XPathNodeType::EXPRESSION) {
      if (ExprNode->child_count() > 0) {
         return evaluate_expression(ExprNode->get_child(0), CurrentPrefix);
      }
      expression_unsupported = true;
      return XPathVal();
   }

   if (ExprNode->type IS XPathNodeType::VARIABLE_REFERENCE) {
      XPathVal resolved_value;
      if (resolve_variable_value(ExprNode->value, CurrentPrefix, resolved_value, ExprNode)) {
         return resolved_value;
      }

      if (is_trace_enabled()) {
         log.msg(VLF::TRACE, "Variable lookup failed for '%s'", ExprNode->value.c_str());
         if (context.variables and context.variables->empty() IS false) {
            std::string binding_list;
            auto it = context.variables->begin();
            if (it != context.variables->end()) {
               binding_list = it->first;
               ++it;
               for (; it != context.variables->end(); ++it) {
                  binding_list.append(", ");
                  binding_list.append(it->first);
               }
            }
            log.msg(VLF::TRACE, "Context bindings available: [%s]", binding_list.c_str());
         }
      }

      expression_unsupported = true;
      return XPathVal();
   }

   expression_unsupported = true;
   return XPathVal();
}
