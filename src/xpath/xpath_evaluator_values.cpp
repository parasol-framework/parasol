//********************************************************************************************************************
// XPath Expression and Value Evaluation
//
// This translation unit contains the core expression evaluation engine for XPath. It handles:
//   - Location path evaluation (evaluate_path_expression_value, evaluate_path_from_nodes)
//   - Set operations (union, intersect, except)
//   - Expression evaluation for all XPath types (evaluate_expression - the main dispatcher)
//   - Function call evaluation
//   - Top-level expression processing and result handling
//
// All value evaluators consume comparison utilities from xpath_evaluator_detail.h and navigation
// functions from xpath_evaluator_navigation.cpp to maintain clean separation of concerns.

#include "xpath_evaluator.h"
#include "xpath_evaluator_detail.h"
#include "xpath_functions.h"
#include "../xml/schema/schema_types.h"
#include "../xml/xml.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace {

//********************************************************************************************************************

bool is_ncname_start(char Ch)
{
   if ((Ch >= 'A') and (Ch <= 'Z')) return true;
   if ((Ch >= 'a') and (Ch <= 'z')) return true;
   return Ch IS '_';
}

bool is_ncname_char(char Ch)
{
   if (is_ncname_start(Ch)) return true;
   if ((Ch >= '0') and (Ch <= '9')) return true;
   if (Ch IS '-') return true;
   return Ch IS '.';
}

// Determines if the supplied string adheres to the NCName production so constructor
// names can be validated without deferring to the XML runtime.

bool is_valid_ncname(std::string_view Value)
{
   if (Value.empty()) return false;
   if (!is_ncname_start(Value.front())) return false;

   for (size_t index = 1; index < Value.length(); ++index)
   {
      if (!is_ncname_char(Value[index])) return false;
   }

   return true;
}


//********************************************************************************************************************
// Removes leading and trailing XML whitespace characters from constructor data so that lexical comparisons can be 
// performed using the normalised string.

std::string trim_constructor_whitespace(std::string_view Value)
{
   size_t start = 0;
   size_t end = Value.length();

   while ((start < end) and (uint8_t(Value[start]) <= 0x20u)) ++start;
   while ((end > start) and (uint8_t(Value[end - 1]) <= 0x20u)) --end;

   return std::string(Value.substr(start, end - start));
}

size_t combine_group_hash(size_t Seed, size_t Value)
{
   // 0x9e3779b97f4a7c15ull is the 64-bit golden ratio constant commonly used to
   // decorrelate values when mixing hashes.  Incorporating it here improves the
   // distribution of combined group hashes.
   Seed ^= Value + 0x9e3779b97f4a7c15ull + (Seed << 6) + (Seed >> 2);
   return Seed;
}

size_t hash_xpath_group_value(const XPathVal &Value)
{
   size_t seed = size_t(Value.Type);

   switch (Value.Type)
   {
      case XPVT::Boolean:
      case XPVT::Number:
      {
         double number = Value.to_number();
         if (std::isnan(number)) return combine_group_hash(seed, 0x7ff8000000000000ull);
         size_t hashed = std::hash<double>{}(number);
         return combine_group_hash(seed, hashed);
      }

      case XPVT::String:
      case XPVT::Date:
      case XPVT::Time:
      case XPVT::DateTime:
      {
         std::string string_value = Value.to_string();
         size_t hashed = std::hash<std::string>{}(string_value);
         return combine_group_hash(seed, hashed);
      }

      case XPVT::NodeSet:
      {
         size_t combined = seed;
         for (XMLTag *node : Value.node_set)
         {
            combined = combine_group_hash(combined, std::hash<XMLTag *>{}(node));
         }

         for (const XMLAttrib *attribute : Value.node_set_attributes)
         {
            combined = combine_group_hash(combined, std::hash<const XMLAttrib *>{}(attribute));
         }

         for (const auto &entry : Value.node_set_string_values)
         {
            combined = combine_group_hash(combined, std::hash<std::string>{}(entry));
         }

         if (Value.node_set_string_override.has_value())
         {
            combined = combine_group_hash(combined, std::hash<std::string>{}(*Value.node_set_string_override));
         }

         return combined;
      }
   }

   return seed;
}


//********************************************************************************************************************
// Represents a QName or expanded QName parsed from constructor syntax, capturing the prefix, local part, and resolved 
// namespace URI when known.

struct ConstructorQName {
   bool valid = false;
   std::string prefix;
   std::string local;
   std::string namespace_uri;
};

//********************************************************************************************************************
// Parses a QName or expanded QName literal used by computed constructors.  The function recognises the "Q{uri}local" 
// form as well as prefixed names and produces a structured representation that downstream evaluators can inspect.

ConstructorQName parse_constructor_qname_string(std::string_view Value)
{
   ConstructorQName result;
   if (Value.empty()) return result;

   std::string trimmed = trim_constructor_whitespace(Value);
   if (trimmed.empty()) return result;

   std::string_view working(trimmed);

   if ((working.length() >= 2) and (working[0] IS 'Q') and (working[1] IS '{')) {
      size_t closing = working.find('}');
      if (closing IS std::string_view::npos) return result;

      result.namespace_uri = std::string(working.substr(2, closing - 2));
      std::string_view remainder = working.substr(closing + 1);
      if (remainder.empty()) return result;
      if (!is_valid_ncname(remainder)) return result;

      result.local = std::string(remainder);
      result.valid = true;
      return result;
   }

   size_t colon = working.find(':');
   if (colon IS std::string_view::npos) {
      if (!is_valid_ncname(working)) return result;
      result.local = std::string(working);
      result.valid = true;
      return result;
   }

   std::string_view prefix_view = working.substr(0, colon);
   std::string_view local_view = working.substr(colon + 1);
   if (prefix_view.empty() or local_view.empty()) return result;
   if (!is_valid_ncname(prefix_view) or !is_valid_ncname(local_view)) return result;

   result.prefix = std::string(prefix_view);
   result.local = std::string(local_view);
   result.valid = true;
   return result;
}

} // Anonymous namespace

//********************************************************************************************************************

XPathVal XPathEvaluator::evaluate_path_expression_value(const XPathNode *PathNode, uint32_t CurrentPrefix) 
{
   if (!PathNode) {
      expression_unsupported = true;
      return XPathVal();
   }

   const XPathNode *location = PathNode;
   if (PathNode->type IS XPathNodeType::PATH) {
      if (PathNode->child_count() IS 0) return XPathVal();
      location = PathNode->get_child(0);
   }

   if ((!location) or (location->type != XPathNodeType::LOCATION_PATH)) {
      expression_unsupported = true;
      return XPathVal();
   }

   std::vector<const XPathNode *> steps;
   std::vector<std::unique_ptr<XPathNode>> synthetic_steps;

   bool has_root = false;
   bool root_descendant = false;

   for (size_t index = 0; index < location->child_count(); ++index) {
      auto *child = location->get_child(index);
      if (!child) continue;

      if ((index IS 0) and (child->type IS XPathNodeType::ROOT)) {
         has_root = true;
         root_descendant = child->value IS "//";
         continue;
      }

      if (child->type IS XPathNodeType::STEP) steps.push_back(child);
   }

   if (root_descendant) {
      auto descendant_step = std::make_unique<XPathNode>(XPathNodeType::STEP);
      descendant_step->add_child(std::make_unique<XPathNode>(XPathNodeType::AXIS_SPECIFIER, "descendant-or-self"));
      descendant_step->add_child(std::make_unique<XPathNode>(XPathNodeType::NODE_TYPE_TEST, "node"));
      steps.insert(steps.begin(), descendant_step.get());
      synthetic_steps.push_back(std::move(descendant_step));
   }

   NODES initial_context;

   if (has_root) initial_context.push_back(nullptr);
   else {
      if (context.context_node) initial_context.push_back(context.context_node);
      else if ((xml->CursorTags) and (xml->Cursor != xml->CursorTags->end())) initial_context.push_back(&(*xml->Cursor));
      else initial_context.push_back(nullptr);
   }

   if (steps.empty()) return XPathVal(initial_context);

   const XPathNode *attribute_step = nullptr;
   const XPathNode *attribute_test = nullptr;

   auto last_step = steps.back();
   if (last_step) {
      const XPathNode *axis_node = nullptr;
      const XPathNode *node_test = nullptr;

      for (size_t index = 0; index < last_step->child_count(); ++index) {
         auto *child = last_step->get_child(index);
         if (!child) continue;

         if (child->type IS XPathNodeType::AXIS_SPECIFIER) axis_node = child;
         else if ((!node_test) and ((child->type IS XPathNodeType::NAME_TEST) or
                                    (child->type IS XPathNodeType::WILDCARD) or
                                    (child->type IS XPathNodeType::NODE_TYPE_TEST))) node_test = child;
      }

      AxisType axis = axis_node ? AxisEvaluator::parse_axis_name(axis_node->value) : AxisType::CHILD;
      if (axis IS AxisType::ATTRIBUTE) {
         attribute_step = last_step;
         attribute_test = node_test;
      }
   }

   std::vector<const XPathNode *> work_steps = steps;
   if (attribute_step) work_steps.pop_back();

   bool unsupported = false;
   NODES node_results;

   if (work_steps.empty()) {
      for (auto *candidate : initial_context) {
         if (candidate) node_results.push_back(candidate);
      }
   }
   else {
      std::vector<AxisMatch> initial_matches;
      initial_matches.reserve(initial_context.size());

      for (auto *candidate : initial_context) {
         const XMLAttrib *attribute = nullptr;
         if ((candidate) and context.attribute_node and (candidate IS context.context_node)) attribute = context.attribute_node;
         initial_matches.push_back({ candidate, attribute });
      }

      node_results = collect_step_results(initial_matches, work_steps, 0, CurrentPrefix, unsupported);
   }

   if (unsupported) {
      expression_unsupported = true;
      return XPathVal();
   }

   axis_evaluator.normalise_node_set(node_results);

   if (context.attribute_node and (steps.size() IS 1)) {
      const XPathNode *step = steps[0];
      const XPathNode *axis_node = nullptr;
      const XPathNode *node_test = nullptr;

      for (size_t index = 0; index < step->child_count(); ++index) {
         auto *child = step->get_child(index);
         if (!child) continue;

         if (child->type IS XPathNodeType::AXIS_SPECIFIER) axis_node = child;
         else if ((!node_test) and ((child->type IS XPathNodeType::NAME_TEST) or
                                    (child->type IS XPathNodeType::WILDCARD) or
                                    (child->type IS XPathNodeType::NODE_TYPE_TEST))) node_test = child;
      }

      AxisType axis = axis_node ? AxisEvaluator::parse_axis_name(axis_node->value) : AxisType::CHILD;

      if ((axis IS AxisType::SELF) and !node_results.empty()) {
         bool accepts_attribute = false;

         if (!node_test) accepts_attribute = true;
         else if (node_test->type IS XPathNodeType::WILDCARD) accepts_attribute = true;
         else if (node_test->type IS XPathNodeType::NODE_TYPE_TEST) accepts_attribute = node_test->value IS "node";

         if (accepts_attribute) {
            std::vector<const XMLAttrib *> attribute_refs(node_results.size(), context.attribute_node);
            return XPathVal(node_results, context.attribute_node->Value, {}, std::move(attribute_refs));
         }
      }
   }

   if (attribute_step) {
      std::vector<std::string> attribute_values;
      NODES attribute_nodes;
      std::vector<const XMLAttrib *> attribute_refs;
      std::vector<const XPathNode *> attribute_predicates;

      for (size_t index = 0; index < attribute_step->child_count(); ++index) {
         auto *child = attribute_step->get_child(index);
         if (child and (child->type IS XPathNodeType::PREDICATE)) attribute_predicates.push_back(child);
      }

      for (auto *candidate : node_results) {
         if (!candidate) continue;

         auto matches = dispatch_axis(AxisType::ATTRIBUTE, candidate);
         if (matches.empty()) continue;

         std::vector<AxisMatch> filtered;
         filtered.reserve(matches.size());

         for (auto &match : matches) {
            if (!match.attribute) continue;
            if (!match_node_test(attribute_test, AxisType::ATTRIBUTE, match.node, match.attribute, CurrentPrefix)) continue;
            filtered.push_back(match);
         }

         if (filtered.empty()) continue;

         if (!attribute_predicates.empty()) {
            std::vector<AxisMatch> predicate_buffer;
            predicate_buffer.reserve(filtered.size());

            for (auto *predicate_node : attribute_predicates) {
               predicate_buffer.clear();
               predicate_buffer.reserve(filtered.size());

               for (size_t index = 0; index < filtered.size(); ++index) {
                  auto &match = filtered[index];

                  push_context(match.node, index + 1, filtered.size(), match.attribute);
                  auto predicate_result = evaluate_predicate(predicate_node, CurrentPrefix);
                  pop_context();

                  if (predicate_result IS PredicateResult::UNSUPPORTED) {
                     expression_unsupported = true;
                     return XPathVal();
                  }

                  if (predicate_result IS PredicateResult::MATCH) predicate_buffer.push_back(match);
               }

               filtered.swap(predicate_buffer);
               if (filtered.empty()) break;
            }

            if (filtered.empty()) continue;
         }

         for (auto &match : filtered) {
            attribute_values.push_back(match.attribute->Value);
            attribute_nodes.push_back(match.node);
            attribute_refs.push_back(match.attribute);
         }
      }

      if (attribute_nodes.empty()) return XPathVal(attribute_nodes);

      std::optional<std::string> first_value;
      if (!attribute_values.empty()) first_value = attribute_values[0];
      return XPathVal(attribute_nodes, first_value, std::move(attribute_values), std::move(attribute_refs));
   }

   return XPathVal(node_results);
}

//********************************************************************************************************************

XPathVal XPathEvaluator::evaluate_path_from_nodes(const NODES &InitialContext,
   const std::vector<const XMLAttrib *> &InitialAttributes, const std::vector<const XPathNode *> &Steps,
   const XPathNode *AttributeStep, const XPathNode *AttributeTest, uint32_t CurrentPrefix)
{
   std::vector<const XPathNode *> work_steps = Steps;

   if (AttributeStep and !work_steps.empty()) work_steps.pop_back();

   NODES node_results;

   if (work_steps.empty()) {
      node_results = InitialContext;
   }
   else {
      std::vector<AxisMatch> initial_matches;
      initial_matches.reserve(InitialContext.size());

      for (size_t index = 0; index < InitialContext.size(); ++index) {
         auto *candidate = InitialContext[index];
         const XMLAttrib *attribute = nullptr;
         if (index < InitialAttributes.size()) attribute = InitialAttributes[index];
         initial_matches.push_back({ candidate, attribute });
      }

      bool unsupported = false;
      node_results = collect_step_results(initial_matches, work_steps, 0, CurrentPrefix, unsupported);

      if (unsupported) {
         expression_unsupported = true;
         return XPathVal();
      }
   }

   axis_evaluator.normalise_node_set(node_results);

   if (AttributeStep) {
      std::vector<std::string> attribute_values;
      NODES attribute_nodes;
      std::vector<const XMLAttrib *> attribute_refs;

      for (auto *candidate : node_results) {
         if (!candidate) continue;

         auto matches = dispatch_axis(AxisType::ATTRIBUTE, candidate);
         for (auto &match : matches) {
            if (!match.attribute) continue;
            if (!match_node_test(AttributeTest, AxisType::ATTRIBUTE, match.node, match.attribute, CurrentPrefix)) continue;
            attribute_values.push_back(match.attribute->Value);
            attribute_nodes.push_back(match.node);
            attribute_refs.push_back(match.attribute);

         }
      }

      if (attribute_nodes.empty()) return XPathVal(attribute_nodes);

      std::optional<std::string> first_value;
      if (!attribute_values.empty()) first_value = attribute_values[0];
      return XPathVal(attribute_nodes, first_value, std::move(attribute_values), std::move(attribute_refs));
   }

   return XPathVal(node_results);
}

//********************************************************************************************************************

XPathVal XPathEvaluator::evaluate_union_value(const std::vector<const XPathNode *> &Branches, uint32_t CurrentPrefix)
{
   struct NodeIdentity {
      XMLTag * node;
      const XMLAttrib * attribute;
   };

   struct NodeIdentityHash {
      size_t operator()(const NodeIdentity &Value) const {
         size_t node_hash = std::hash<XMLTag *>()(Value.node);
         size_t attrib_hash = std::hash<const XMLAttrib *>()(Value.attribute);
         return node_hash ^ (attrib_hash << 1);
      }
   };

   struct NodeIdentityEqual {
      bool operator()(const NodeIdentity &Left, const NodeIdentity &Right) const {
         return (Left.node IS Right.node) and (Left.attribute IS Right.attribute);
      }
   };

   auto saved_context = context;
   auto saved_context_stack = context_stack;
   auto saved_cursor_stack = cursor_stack;
   auto saved_cursor_tags = xml->CursorTags;
   auto saved_cursor = xml->Cursor;
   auto saved_attrib = xml->Attrib;
   bool saved_expression_unsupported = expression_unsupported;

   std::unordered_set<NodeIdentity, NodeIdentityHash, NodeIdentityEqual> seen_entries;
   seen_entries.reserve(Branches.size() * 4);

   struct UnionEntry {
      XMLTag * node = nullptr;
      const XMLAttrib * attribute = nullptr;
      std::string string_value;
   };

   std::vector<UnionEntry> entries;
   entries.reserve(Branches.size() * 4);

   std::optional<std::string> combined_override;

   for (auto *branch : Branches) {
      if (!branch) continue;

      context = saved_context;
      context_stack = saved_context_stack;
      cursor_stack = saved_cursor_stack;
      xml->CursorTags = saved_cursor_tags;
      xml->Cursor = saved_cursor;
      xml->Attrib = saved_attrib;
      expression_unsupported = saved_expression_unsupported;

      auto branch_value = evaluate_expression(branch, CurrentPrefix);
      if (expression_unsupported) {
         context = saved_context;
         context_stack = saved_context_stack;
         cursor_stack = saved_cursor_stack;
         xml->CursorTags = saved_cursor_tags;
         xml->Cursor = saved_cursor;
         xml->Attrib = saved_attrib;
         expression_unsupported = true;
         return XPathVal();
      }

      if (branch_value.Type != XPVT::NodeSet) {
         context = saved_context;
         context_stack = saved_context_stack;
         cursor_stack = saved_cursor_stack;
         xml->CursorTags = saved_cursor_tags;
         xml->Cursor = saved_cursor;
         xml->Attrib = saved_attrib;
         expression_unsupported = true;
         return XPathVal();
      }

      for (size_t index = 0; index < branch_value.node_set.size(); ++index) {
         XMLTag *node = branch_value.node_set[index];
         const XMLAttrib *attribute = nullptr;
         if (index < branch_value.node_set_attributes.size()) attribute = branch_value.node_set_attributes[index];

         NodeIdentity identity { node, attribute };
         if (!seen_entries.insert(identity).second) continue;

         UnionEntry entry;
         entry.node = node;
         entry.attribute = attribute;

         if (index < branch_value.node_set_string_values.size()) entry.string_value = branch_value.node_set_string_values[index];
         else entry.string_value = XPathVal::node_string_value(node);

         if (!combined_override.has_value()) {
            if (branch_value.node_set_string_override.has_value()) combined_override = branch_value.node_set_string_override;
            else combined_override = entry.string_value;
         }

         entries.push_back(std::move(entry));
      }
   }

   std::stable_sort(entries.begin(), entries.end(), [this](const UnionEntry &Left, const UnionEntry &Right) {
      if (Left.node IS Right.node) return false;
      return axis_evaluator.is_before_in_document_order(Left.node, Right.node);
   });

   NODES combined_nodes;
   std::vector<const XMLAttrib *> combined_attributes;
   std::vector<std::string> combined_strings;
   combined_nodes.reserve(entries.size());
   combined_attributes.reserve(entries.size());
   combined_strings.reserve(entries.size());

   for (const auto &entry : entries) {
      combined_nodes.push_back(entry.node);
      combined_attributes.push_back(entry.attribute);
      combined_strings.push_back(entry.string_value);
   }

   context         = saved_context;
   context_stack   = saved_context_stack;
   cursor_stack    = saved_cursor_stack;
   xml->CursorTags = saved_cursor_tags;
   xml->Cursor     = saved_cursor;
   xml->Attrib     = saved_attrib;
   expression_unsupported = saved_expression_unsupported;

   if (combined_nodes.empty()) return XPathVal(NODES());

   return XPathVal(combined_nodes, combined_override, std::move(combined_strings), std::move(combined_attributes));
}

//********************************************************************************************************************

XPathVal XPathEvaluator::evaluate_intersect_value(const XPathNode *Left, const XPathNode *Right, uint32_t CurrentPrefix)
{
   struct NodeIdentity {
      XMLTag * node;
      const XMLAttrib * attribute;
   };

   struct NodeIdentityHash {
      size_t operator()(const NodeIdentity &Value) const {
         size_t node_hash = std::hash<XMLTag *>()(Value.node);
         size_t attrib_hash = std::hash<const XMLAttrib *>()(Value.attribute);
         return node_hash ^ (attrib_hash << 1);
      }
   };

   struct NodeIdentityEqual {
      bool operator()(const NodeIdentity &LeftIdentity, const NodeIdentity &RightIdentity) const {
         return (LeftIdentity.node IS RightIdentity.node) and (LeftIdentity.attribute IS RightIdentity.attribute);
      }
   };

   struct SetEntry {
      XMLTag * node = nullptr;
      const XMLAttrib * attribute = nullptr;
      std::string string_value;
   };

   auto saved_context = context;
   auto saved_context_stack = context_stack;
   auto saved_cursor_stack = cursor_stack;
   auto saved_cursor_tags = xml->CursorTags;
   auto saved_cursor = xml->Cursor;
   auto saved_attrib = xml->Attrib;
   bool saved_expression_unsupported = expression_unsupported;

   auto evaluate_operand = [&](const XPathNode *Operand) -> std::optional<XPathVal> {
      context = saved_context;
      context_stack = saved_context_stack;
      cursor_stack = saved_cursor_stack;
      xml->CursorTags = saved_cursor_tags;
      xml->Cursor = saved_cursor;
      xml->Attrib = saved_attrib;
      expression_unsupported = saved_expression_unsupported;

      auto value = evaluate_expression(Operand, CurrentPrefix);
      if (expression_unsupported) return std::nullopt;

      if (value.Type != XPVT::NodeSet) {
         expression_unsupported = true;
         return std::nullopt;
      }

      return value;
   };

   auto left_value_opt = evaluate_operand(Left);
   if (!left_value_opt.has_value()) {
      context         = saved_context;
      context_stack   = saved_context_stack;
      cursor_stack    = saved_cursor_stack;
      xml->CursorTags = saved_cursor_tags;
      xml->Cursor     = saved_cursor;
      xml->Attrib     = saved_attrib;
      return XPathVal();
   }

   auto right_value_opt = evaluate_operand(Right);
   if (!right_value_opt.has_value()) {
      context         = saved_context;
      context_stack   = saved_context_stack;
      cursor_stack    = saved_cursor_stack;
      xml->CursorTags = saved_cursor_tags;
      xml->Cursor     = saved_cursor;
      xml->Attrib     = saved_attrib;
      return XPathVal();
   }

   const XPathVal &left_value = *left_value_opt;
   const XPathVal &right_value = *right_value_opt;

   std::unordered_set<NodeIdentity, NodeIdentityHash, NodeIdentityEqual> right_entries;
   right_entries.reserve(right_value.node_set.size() * 2);

   for (size_t index = 0; index < right_value.node_set.size(); ++index) {
      XMLTag *node = right_value.node_set[index];
      const XMLAttrib *attribute = nullptr;
      if (index < right_value.node_set_attributes.size()) attribute = right_value.node_set_attributes[index];

      NodeIdentity identity { node, attribute };
      right_entries.insert(identity);
   }

   std::unordered_set<NodeIdentity, NodeIdentityHash, NodeIdentityEqual> inserted;
   inserted.reserve(left_value.node_set.size());

   std::vector<SetEntry> entries;
   entries.reserve(left_value.node_set.size());

   std::optional<std::string> combined_override = left_value.node_set_string_override;

   for (size_t index = 0; index < left_value.node_set.size(); ++index) {
      XMLTag *node = left_value.node_set[index];
      const XMLAttrib *attribute = nullptr;
      if (index < left_value.node_set_attributes.size()) attribute = left_value.node_set_attributes[index];

      NodeIdentity identity { node, attribute };
      if (!right_entries.contains(identity)) continue;
      if (!inserted.insert(identity).second) continue;

      SetEntry entry;
      entry.node = node;
      entry.attribute = attribute;

      if (index < left_value.node_set_string_values.size()) entry.string_value = left_value.node_set_string_values[index];
      else entry.string_value = XPathVal::node_string_value(node);

      if (!combined_override.has_value()) combined_override = entry.string_value;

      entries.push_back(std::move(entry));
   }

   std::stable_sort(entries.begin(), entries.end(), [this](const SetEntry &LeftEntry, const SetEntry &RightEntry) {
      if (LeftEntry.node IS RightEntry.node) return false;
      return axis_evaluator.is_before_in_document_order(LeftEntry.node, RightEntry.node);
   });

   NODES combined_nodes;
   std::vector<const XMLAttrib *> combined_attributes;
   std::vector<std::string> combined_strings;
   combined_nodes.reserve(entries.size());
   combined_attributes.reserve(entries.size());
   combined_strings.reserve(entries.size());

   for (const auto &entry : entries) {
      combined_nodes.push_back(entry.node);
      combined_attributes.push_back(entry.attribute);
      combined_strings.push_back(entry.string_value);
   }

   context = saved_context;
   context_stack   = saved_context_stack;
   cursor_stack    = saved_cursor_stack;
   xml->CursorTags = saved_cursor_tags;
   xml->Cursor     = saved_cursor;
   xml->Attrib     = saved_attrib;
   expression_unsupported = saved_expression_unsupported;

   if (combined_nodes.empty()) return XPathVal(NODES());

   return XPathVal(combined_nodes, combined_override, std::move(combined_strings), std::move(combined_attributes));
}

//********************************************************************************************************************

XPathVal XPathEvaluator::evaluate_except_value(const XPathNode *Left, const XPathNode *Right, uint32_t CurrentPrefix)
{
   struct NodeIdentity {
      XMLTag * node;
      const XMLAttrib * attribute;
   };

   struct NodeIdentityHash {
      size_t operator()(const NodeIdentity &Value) const
      {
         size_t node_hash = std::hash<XMLTag *>()(Value.node);
         size_t attrib_hash = std::hash<const XMLAttrib *>()(Value.attribute);
         return node_hash ^ (attrib_hash << 1);
      }
   };

   struct NodeIdentityEqual {
      bool operator()(const NodeIdentity &LeftIdentity, const NodeIdentity &RightIdentity) const
      {
         return (LeftIdentity.node IS RightIdentity.node) and (LeftIdentity.attribute IS RightIdentity.attribute);
      }
   };

   struct SetEntry {
      XMLTag * node = nullptr;
      const XMLAttrib * attribute = nullptr;
      std::string string_value;
   };

   auto saved_context       = context;
   auto saved_context_stack = context_stack;
   auto saved_cursor_stack  = cursor_stack;
   auto saved_cursor_tags   = xml->CursorTags;
   auto saved_cursor        = xml->Cursor;
   auto saved_attrib        = xml->Attrib;
   bool saved_expression_unsupported = expression_unsupported;

   auto evaluate_operand = [&](const XPathNode *Operand) -> std::optional<XPathVal> {
      context = saved_context;
      context_stack = saved_context_stack;
      cursor_stack = saved_cursor_stack;
      xml->CursorTags = saved_cursor_tags;
      xml->Cursor = saved_cursor;
      xml->Attrib = saved_attrib;
      expression_unsupported = saved_expression_unsupported;

      auto value = evaluate_expression(Operand, CurrentPrefix);
      if (expression_unsupported) return std::nullopt;

      if (value.Type != XPVT::NodeSet) {
         expression_unsupported = true;
         return std::nullopt;
      }

      return value;
   };

   auto left_value_opt = evaluate_operand(Left);
   if (!left_value_opt.has_value()) {
      context = saved_context;
      context_stack = saved_context_stack;
      cursor_stack = saved_cursor_stack;
      xml->CursorTags = saved_cursor_tags;
      xml->Cursor = saved_cursor;
      xml->Attrib = saved_attrib;
      return XPathVal();
   }

   auto right_value_opt = evaluate_operand(Right);
   if (!right_value_opt.has_value()) {
      context = saved_context;
      context_stack = saved_context_stack;
      cursor_stack = saved_cursor_stack;
      xml->CursorTags = saved_cursor_tags;
      xml->Cursor = saved_cursor;
      xml->Attrib = saved_attrib;
      return XPathVal();
   }

   const XPathVal &left_value = *left_value_opt;
   const XPathVal &right_value = *right_value_opt;

   std::unordered_set<NodeIdentity, NodeIdentityHash, NodeIdentityEqual> right_entries;
   right_entries.reserve(right_value.node_set.size() * 2);

   for (size_t index = 0; index < right_value.node_set.size(); ++index) {
      XMLTag *node = right_value.node_set[index];
      const XMLAttrib *attribute = nullptr;
      if (index < right_value.node_set_attributes.size()) attribute = right_value.node_set_attributes[index];

      NodeIdentity identity { node, attribute };
      right_entries.insert(identity);
   }

   std::unordered_set<NodeIdentity, NodeIdentityHash, NodeIdentityEqual> inserted;
   inserted.reserve(left_value.node_set.size());

   std::vector<SetEntry> entries;
   entries.reserve(left_value.node_set.size());

   std::optional<std::string> combined_override = left_value.node_set_string_override;

   for (size_t index = 0; index < left_value.node_set.size(); ++index) {
      XMLTag *node = left_value.node_set[index];
      const XMLAttrib *attribute = nullptr;
      if (index < left_value.node_set_attributes.size()) attribute = left_value.node_set_attributes[index];

      NodeIdentity identity { node, attribute };
      if (right_entries.contains(identity)) continue;
      if (!inserted.insert(identity).second) continue;

      SetEntry entry;
      entry.node = node;
      entry.attribute = attribute;

      if (index < left_value.node_set_string_values.size()) entry.string_value = left_value.node_set_string_values[index];
      else entry.string_value = XPathVal::node_string_value(node);

      if (!combined_override.has_value()) combined_override = entry.string_value;

      entries.push_back(std::move(entry));
   }

   std::stable_sort(entries.begin(), entries.end(), [this](const SetEntry &LeftEntry, const SetEntry &RightEntry) {
      if (LeftEntry.node IS RightEntry.node) return false;
      return axis_evaluator.is_before_in_document_order(LeftEntry.node, RightEntry.node);
   });

   NODES combined_nodes;
   std::vector<const XMLAttrib *> combined_attributes;
   std::vector<std::string> combined_strings;
   combined_nodes.reserve(entries.size());
   combined_attributes.reserve(entries.size());
   combined_strings.reserve(entries.size());

   for (const auto &entry : entries) {
      combined_nodes.push_back(entry.node);
      combined_attributes.push_back(entry.attribute);
      combined_strings.push_back(entry.string_value);
   }

   context = saved_context;
   context_stack = saved_context_stack;
   cursor_stack = saved_cursor_stack;
   xml->CursorTags = saved_cursor_tags;
   xml->Cursor = saved_cursor;
   xml->Attrib = saved_attrib;
   expression_unsupported = saved_expression_unsupported;

   if (combined_nodes.empty()) return XPathVal(NODES());

   return XPathVal(combined_nodes, combined_override, std::move(combined_strings), std::move(combined_attributes));
}

//********************************************************************************************************************
// Registers the supplied namespace URI with the owning XML document so constructed nodes reference consistent 
// namespace identifiers.

uint32_t XPathEvaluator::register_constructor_namespace(const std::string &URI) const
{
   if (!xml) return 0;
   return xml->registerNamespace(URI);
}

//********************************************************************************************************************
// Resolves a prefix within the chained constructor namespace scopes, honouring the nearest declaration and falling 
// back to the default namespace when the prefix is empty.

std::optional<uint32_t> XPathEvaluator::resolve_constructor_prefix(const ConstructorNamespaceScope &Scope,
   std::string_view Prefix) const
{
   std::string prefix_key(Prefix);
   const ConstructorNamespaceScope *cursor = &Scope;

   if (prefix_key.empty())
   {
      while (cursor)
      {
         if (cursor->default_namespace.has_value()) return cursor->default_namespace;
         cursor = cursor->parent;
      }
      return uint32_t{0};
   }

   while (cursor)
   {
      auto iter = cursor->prefix_bindings.find(prefix_key);
      if (iter != cursor->prefix_bindings.end()) return iter->second;
      cursor = cursor->parent;
   }

   return std::nullopt;
}

//********************************************************************************************************************
// Recursively clones an XML node subtree so constructor operations can duplicate existing content without mutating 
// the original document tree.

XMLTag XPathEvaluator::clone_node_subtree(const XMLTag &Source, int ParentID)
{
   XMLTag clone(next_constructed_node_id--, Source.LineNo);
   clone.ParentID = ParentID;
   clone.Flags = Source.Flags;
   clone.NamespaceID = Source.NamespaceID;
   clone.Attribs = Source.Attribs;

   clone.Children.reserve(Source.Children.size());
   for (const auto &child : Source.Children) {
      XMLTag child_clone = clone_node_subtree(child, clone.ID);
      clone.Children.push_back(std::move(child_clone));
   }

   return clone;
}

//********************************************************************************************************************
// Appends a sequence value produced by constructor content into the target element, handling node cloning, attribute
// creation, and text concatenation according to the XPath constructor rules.

bool XPathEvaluator::append_constructor_sequence(XMLTag &Parent, const XPathVal &Value, uint32_t CurrentPrefix, 
   const ConstructorNamespaceScope &Scope)
{
   (void)CurrentPrefix;
   (void)Scope;

   if (Value.Type IS XPVT::NodeSet) {
      Parent.Children.reserve(Parent.Children.size() + Value.node_set.size());

      for (size_t index = 0; index < Value.node_set.size(); ++index) {
         XMLTag *node = Value.node_set[index];
         if (!node) continue;

         const XMLAttrib *attribute = nullptr;
         if (index < Value.node_set_attributes.size()) attribute = Value.node_set_attributes[index];
         if (attribute)
         {
            std::string_view attribute_name = attribute->Name;
            if (attribute_name.empty()) continue;

            bool duplicate = false;
            for (size_t attrib_index = 1; attrib_index < Parent.Attribs.size(); ++attrib_index)
            {
               if (Parent.Attribs[attrib_index].Name IS attribute_name)
               {
                  duplicate = true;
                  break;
               }
            }

            if (duplicate)
            {
               record_error("Duplicate attribute name in constructor content.", true);
               return false;
            }

            Parent.Attribs.emplace_back(std::string(attribute_name), attribute->Value);
            continue;
         }

         XMLTag clone = clone_node_subtree(*node, Parent.ID);
         Parent.Children.push_back(std::move(clone));
      }

      return true;
   }

   std::string text = Value.to_string();
   if (text.empty()) return true;

   pf::vector<XMLAttrib> text_attribs;
   text_attribs.emplace_back("", text);

   XMLTag text_node(next_constructed_node_id--, 0, text_attribs);
   text_node.ParentID = Parent.ID;
   Parent.Children.push_back(std::move(text_node));
   return true;
}

//********************************************************************************************************************
// Evaluates an attribute value template (AVT) collected during parsing.  The template
// parts alternate between literal text and embedded expressions, and the resolved
// string is returned for assignment to the constructed attribute.

std::optional<std::string> XPathEvaluator::evaluate_attribute_value_template(const XPathConstructorAttribute &Attribute,
   uint32_t CurrentPrefix)
{
   std::string result;

   for (size_t index = 0; index < Attribute.value_parts.size(); ++index) {
      const auto &part = Attribute.value_parts[index];
      if (!part.is_expression) {
         result += part.text;
         continue;
      }

      auto *expr = Attribute.get_expression_for_part(index);
      if (!expr) {
         record_error("Attribute value template part is missing its expression.", true);
         return std::nullopt;
      }

      std::string previous_xml_error;
      if (xml) previous_xml_error = xml->ErrorMsg;

      size_t previous_constructed = constructed_nodes.size();
      auto saved_id = next_constructed_node_id;
      bool previous_flag = expression_unsupported;
      expression_unsupported = false;
      XPathVal value = evaluate_expression(expr, CurrentPrefix);

      bool evaluation_failed = expression_unsupported;
      std::string evaluation_error;
      if (xml) evaluation_error = xml->ErrorMsg;

      if (evaluation_failed) {
         std::string signature = build_ast_signature(expr);
         std::string variable_list;
         if (context.variables.empty()) variable_list.assign("[]");
         else {
            variable_list.reserve(context.variables.size() * 16);
            variable_list.push_back('[');
            bool first_binding = true;
            for (const auto &binding : context.variables) {
               if (!first_binding) variable_list.append(", ");
               variable_list.append(binding.first);
               first_binding = false;
            }
            variable_list.push_back(']');
         }

         if (is_trace_enabled(TraceCategory::XPath)) {
            pf::Log log("XPath");
            log.msg(trace_detail_level, "AVT context variable count: %zu", context.variables.size());
            log.msg(trace_detail_level, "AVT expression failed: %s | context-vars=%s | prev-flag=%s",
               signature.c_str(), variable_list.c_str(), previous_flag ? "true" : "false");
         }

         std::string message = "Attribute value template expression could not be evaluated for part ";
         message += std::to_string(index);
         message.append(". AST signature: ");
         message += signature;
         message.push_back('.');
         if (!evaluation_error.empty()) {
            message.append(" XPath error: ");
            message += evaluation_error;
            if (evaluation_error.back() != '.') message.push_back('.');
         }
         if (context.variables.empty()) message.append(" No context variables were in scope.");
         else {
            message.append(" In-scope variables: ");
            message += variable_list;
            message.push_back('.');
         }

         record_error(message, true);
         constructed_nodes.resize(previous_constructed);
         next_constructed_node_id = saved_id;
         return std::nullopt;
      }

      if (xml and (xml->ErrorMsg.compare(previous_xml_error) != 0)) xml->ErrorMsg = previous_xml_error;
      result += value.to_string();
      constructed_nodes.resize(previous_constructed);
      next_constructed_node_id = saved_id;
   }

   return result;
}

//********************************************************************************************************************
// Reduces the child expressions beneath a constructor content node to a single string value.  Each child expression 
// is evaluated and the textual representation is concatenated to form the returned content.

std::optional<std::string> XPathEvaluator::evaluate_constructor_content_string(const XPathNode *Node, uint32_t CurrentPrefix)
{
   if (!Node) return std::string();
   if (!Node->value.empty()) return Node->value;

   if (Node->child_count() IS 0) return std::string();

   const XPathNode *expr = Node->get_child(0);
   if (!expr) return std::string();

   size_t previous_constructed = constructed_nodes.size();
   auto saved_id = next_constructed_node_id;
   XPathVal value = evaluate_expression(expr, CurrentPrefix);
   if (expression_unsupported)
   {
      if (is_trace_enabled(TraceCategory::XPath)) {
         std::string signature = build_ast_signature(expr);
         pf::Log log("XPath");
         log.msg(trace_detail_level, "Constructor content expression failed: %s", signature.c_str());
      }
      record_error("Constructor content expression could not be evaluated.");
      if (xml and xml->ErrorMsg.empty()) xml->ErrorMsg.assign("Constructor content expression could not be evaluated.");
      constructed_nodes.resize(previous_constructed);
      next_constructed_node_id = saved_id;
      return std::nullopt;
   }

   std::string result;

   if (value.Type IS XPVT::NodeSet)
   {
      if (value.node_set_string_override.has_value()) result += *value.node_set_string_override;
      else
      {
         for (size_t index = 0; index < value.node_set.size(); ++index)
         {
            const XMLAttrib *attribute = nullptr;
            if (index < value.node_set_attributes.size()) attribute = value.node_set_attributes[index];
            if (attribute)
            {
               result += attribute->Value;
               continue;
            }

            if (!value.node_set_string_values.empty() and (index < value.node_set_string_values.size()))
            {
               result += value.node_set_string_values[index];
               continue;
            }

            XMLTag *node = value.node_set[index];
            if (!node) continue;
            result += XPathVal::node_string_value(node);
         }
      }
   }
   else result = value.to_string();

   constructed_nodes.resize(previous_constructed);
   next_constructed_node_id = saved_id;
   return result;
}

//********************************************************************************************************************
// Resolves the lexical name of a constructor by evaluating the optional expression or using the literal metadata 
// captured by the parser.  The resulting string retains the raw QName form so later stages can validate namespace 
// bindings.

std::optional<std::string> XPathEvaluator::evaluate_constructor_name_string(const XPathNode *Node, uint32_t CurrentPrefix)
{
   if (!Node) return std::string();

   size_t previous_constructed = constructed_nodes.size();
   auto saved_id = next_constructed_node_id;
   XPathVal value = evaluate_expression(Node, CurrentPrefix);
   if (expression_unsupported)
   {
      if (is_trace_enabled(TraceCategory::XPath)) {
         std::string signature = build_ast_signature(Node);
         pf::Log log("XPath");
         log.msg(trace_detail_level, "Constructor name expression failed: %s", signature.c_str());
      }
      record_error("Constructor name expression could not be evaluated.");
      if (xml and xml->ErrorMsg.empty()) xml->ErrorMsg.assign("Constructor name expression could not be evaluated.");
      constructed_nodes.resize(previous_constructed);
      next_constructed_node_id = saved_id;
      return std::nullopt;
   }

   std::string raw = value.to_string();
   constructed_nodes.resize(previous_constructed);
   next_constructed_node_id = saved_id;
   return trim_constructor_whitespace(raw);
}

//********************************************************************************************************************
// Builds an XMLTag representing a direct element constructor.  The function walks the parsed constructor metadata, 
// creates namespace scopes, instantiates attributes, and recursively processes nested constructors and enclosed 
// expressions.

std::optional<XMLTag> XPathEvaluator::build_direct_element_node(const XPathNode *Node, uint32_t CurrentPrefix,
   ConstructorNamespaceScope *ParentScope, int ParentID)
{
   if ((!Node) or (Node->type != XPathNodeType::DIRECT_ELEMENT_CONSTRUCTOR)) {
      record_error("Invalid direct constructor node encountered.", true);
      return std::nullopt;
   }

   if (!Node->has_constructor_info()) {
      record_error("Direct constructor is missing structural metadata.", true);
      return std::nullopt;
   }

   const auto &info = *Node->constructor_info;

   ConstructorNamespaceScope element_scope;
   element_scope.parent = ParentScope;
   if (ParentScope and ParentScope->default_namespace.has_value()) {
      element_scope.default_namespace = ParentScope->default_namespace;
   }

   struct EvaluatedAttribute {
      const XPathConstructorAttribute *definition = nullptr;
      std::string value;
   };

   std::vector<EvaluatedAttribute> evaluated_attributes;
   evaluated_attributes.reserve(info.attributes.size());

   for (const auto &attribute : info.attributes) {
      auto value = evaluate_attribute_value_template(attribute, CurrentPrefix);
      if (!value) return std::nullopt;

      EvaluatedAttribute evaluated;
      evaluated.definition = &attribute;
      evaluated.value = std::move(*value);
      evaluated_attributes.push_back(std::move(evaluated));
   }

   pf::vector<XMLAttrib> element_attributes;

   std::string element_name;
   if (info.prefix.empty()) element_name = info.name;
   else {
      element_name = info.prefix;
      element_name += ':';
      element_name += info.name;
   }

   element_attributes.emplace_back(element_name, "");

   for (const auto &entry : evaluated_attributes)
   {
      const auto *attribute = entry.definition;
      const std::string &value = entry.value;

      if (!attribute->is_namespace_declaration) continue;

      if (attribute->prefix.empty() and (attribute->name IS "xmlns")) {
         if (value.empty()) element_scope.default_namespace = uint32_t{0};
         else element_scope.default_namespace = register_constructor_namespace(value);
      }
      else if (attribute->prefix IS "xmlns") {
         if (attribute->name IS "xml") {
            record_error("Cannot redeclare the xml prefix in constructor scope.", true);
            return std::nullopt;
         }

         if (value.empty()) {
            record_error("Namespace prefix declarations require a non-empty URI.", true);
            return std::nullopt;
         }

         element_scope.prefix_bindings[attribute->name] = register_constructor_namespace(value);
      }

      std::string attribute_name;
      if (attribute->prefix.empty()) attribute_name = attribute->name;
      else {
         attribute_name = attribute->prefix;
         attribute_name += ':';
         attribute_name += attribute->name;
      }

      element_attributes.emplace_back(attribute_name, value);
   }

   for (const auto &entry : evaluated_attributes) {
      const auto *attribute = entry.definition;
      const std::string &value = entry.value;

      if (attribute->is_namespace_declaration) continue;

      if (!attribute->prefix.empty()) {
         auto resolved = resolve_constructor_prefix(element_scope, attribute->prefix);
         if (!resolved.has_value())
         {
            record_error("Attribute prefix is not bound in constructor scope.", true);
            return std::nullopt;
         }
      }

      std::string attribute_name;
      if (attribute->prefix.empty()) attribute_name = attribute->name;
      else {
         attribute_name = attribute->prefix;
         attribute_name += ':';
         attribute_name += attribute->name;
      }

      element_attributes.emplace_back(attribute_name, value);
   }

   uint32_t namespace_id = 0;
   if (!info.namespace_uri.empty()) namespace_id = register_constructor_namespace(info.namespace_uri);
   else if (!info.prefix.empty()) {
      auto resolved = resolve_constructor_prefix(element_scope, info.prefix);
      if (!resolved.has_value()) {
         record_error("Element prefix is not declared within constructor scope.", true);
         return std::nullopt;
      }
      namespace_id = *resolved;
   }
   else if (element_scope.default_namespace.has_value()) namespace_id = *element_scope.default_namespace;

   XMLTag element(next_constructed_node_id--, 0);
   element.ParentID = ParentID;
   element.Flags = XTF::NIL;
   element.NamespaceID = namespace_id;
   element.Attribs = element_attributes;

   element.Children.reserve(Node->child_count());

   for (size_t index = 0; index < Node->child_count(); ++index)
   {
      const XPathNode *child = Node->get_child(index);
      if (!child) continue;

      if (child->type IS XPathNodeType::DIRECT_ELEMENT_CONSTRUCTOR)
      {
         auto nested = build_direct_element_node(child, CurrentPrefix, &element_scope, element.ID);
         if (!nested) return std::nullopt;
         element.Children.push_back(std::move(*nested));
         continue;
      }

      if (child->type IS XPathNodeType::CONSTRUCTOR_CONTENT) {
         if (!child->value.empty()) {
            pf::vector<XMLAttrib> text_attribs;
            text_attribs.emplace_back("", child->value);
            XMLTag text_node(next_constructed_node_id--, 0, text_attribs);
            text_node.ParentID = element.ID;
            element.Children.push_back(std::move(text_node));
            continue;
         }

         if (child->child_count() IS 0) continue;

         const XPathNode *expr = child->get_child(0);
         if (!expr) continue;

         size_t previous_constructed = constructed_nodes.size();
         auto saved_id = next_constructed_node_id;
         XPathVal value = evaluate_expression(expr, CurrentPrefix);
         if (expression_unsupported) return std::nullopt;
         if (!append_constructor_sequence(element, value, CurrentPrefix, element_scope)) return std::nullopt;
         constructed_nodes.resize(previous_constructed);
         next_constructed_node_id = saved_id;
         continue;
      }

      record_error("Unsupported node encountered within direct constructor content.", true);
      return std::nullopt;
   }

   return element;
}

//********************************************************************************************************************
// Entry point used by the evaluator to execute direct element constructors in the expression tree.  The resulting 
// element is appended to the constructed node list and wrapped in an XPathVal for downstream consumers.

XPathVal XPathEvaluator::evaluate_direct_element_constructor(const XPathNode *Node, uint32_t CurrentPrefix)
{
   auto element = build_direct_element_node(Node, CurrentPrefix, nullptr, 0);
   if (!element) {
      if (xml and xml->ErrorMsg.empty()) record_error("Direct element constructor could not be evaluated.", true);
      return XPathVal();
   }

   auto stored = std::make_unique<XMLTag>(*element);
   XMLTag *root = stored.get();
   constructed_nodes.push_back(std::move(stored));

   NODES nodes;
   nodes.push_back(root);

   std::vector<std::string> string_values;
   std::string node_string = XPathVal::node_string_value(root);
   string_values.push_back(node_string);

   return XPathVal(nodes, node_string, std::move(string_values));
}

//********************************************************************************************************************
// Handles computed element constructors where the element name or namespace is driven by runtime expressions.  The 
// method prepares the namespace scope and evaluates the content sequence before emitting the constructed element.

XPathVal XPathEvaluator::evaluate_computed_element_constructor(const XPathNode *Node, uint32_t CurrentPrefix)
{
   if ((!Node) or (Node->type != XPathNodeType::COMPUTED_ELEMENT_CONSTRUCTOR)) {
      record_error("Invalid computed element constructor node encountered.", true);
      return XPathVal();
   }

   if (!Node->has_constructor_info()) {
      record_error("Computed element constructor is missing metadata.", true);
      return XPathVal();
   }

   ConstructorQName name_info;

   if (Node->has_name_expression()) {
      auto name_string = evaluate_constructor_name_string(Node->get_name_expression(), CurrentPrefix);
      if (!name_string) return XPathVal();

      auto parsed = parse_constructor_qname_string(*name_string);
      if (!parsed.valid) {
         record_error("Computed element name must resolve to a QName.", true);
         return XPathVal();
      }

      name_info = std::move(parsed);
   }
   else {
      const auto &info = *Node->constructor_info;
      name_info.valid = true;
      name_info.prefix = info.prefix;
      name_info.local = info.name;
      name_info.namespace_uri = info.namespace_uri;
   }

   if (name_info.local.empty()) {
      record_error("Computed element constructor requires a local name.", true);
      return XPathVal();
   }

   auto resolve_prefix_in_context = [&](std::string_view Prefix) -> std::optional<uint32_t> {
      if (Prefix.empty()) return uint32_t{0};
      if (!xml) return std::nullopt;
      if (Prefix.compare("xml") IS 0) return register_constructor_namespace("http://www.w3.org/XML/1998/namespace");
      if (!context.context_node) return std::nullopt;

      uint32_t resolved_hash = 0;
      if (xml->resolvePrefix(Prefix, context.context_node->ID, resolved_hash) IS ERR::Okay) return resolved_hash;
      return std::nullopt;
   };

   uint32_t namespace_id = 0;
   if (!name_info.namespace_uri.empty()) namespace_id = register_constructor_namespace(name_info.namespace_uri);
   else if (!name_info.prefix.empty())
   {
      auto resolved = resolve_prefix_in_context(name_info.prefix);
      if (!resolved.has_value())
      {
         record_error("Element prefix is not bound in scope.", true);
         return XPathVal();
      }
      namespace_id = *resolved;
   }

   std::string element_name;
   if (name_info.prefix.empty()) element_name = name_info.local;
   else
   {
      element_name = name_info.prefix;
      element_name += ':';
      element_name += name_info.local;
   }

   pf::vector<XMLAttrib> element_attributes;
   element_attributes.emplace_back(element_name, "");

   XMLTag element(next_constructed_node_id--, 0, element_attributes);
   element.ParentID = 0;
   element.Flags = XTF::NIL;
   element.NamespaceID = namespace_id;

   ConstructorNamespaceScope scope;
   scope.parent = nullptr;

   if (Node->child_count() > 0) {
      const XPathNode *content_node = Node->get_child(0);
      if (content_node) {
         if (!content_node->value.empty()) {
            pf::vector<XMLAttrib> text_attribs;
            text_attribs.emplace_back("", content_node->value);
            XMLTag text_node(next_constructed_node_id--, 0, text_attribs);
            text_node.ParentID = element.ID;
            element.Children.push_back(std::move(text_node));
         }
         else if (content_node->child_count() > 0) {
            const XPathNode *expr = content_node->get_child(0);
            if (expr) {
               size_t previous_constructed = constructed_nodes.size();
               auto saved_id = next_constructed_node_id;
               XPathVal value = evaluate_expression(expr, CurrentPrefix);
               if (expression_unsupported) return XPathVal();
               if (!append_constructor_sequence(element, value, CurrentPrefix, scope)) return XPathVal();
               constructed_nodes.resize(previous_constructed);
               next_constructed_node_id = saved_id;
            }
         }
      }
   }

   auto stored = std::make_unique<XMLTag>(std::move(element));
   XMLTag *root = stored.get();
   constructed_nodes.push_back(std::move(stored));

   NODES nodes;
   nodes.push_back(root);

   std::vector<std::string> string_values;
   std::string node_string = XPathVal::node_string_value(root);
   string_values.push_back(node_string);

   return XPathVal(nodes, node_string, std::move(string_values));
}

//********************************************************************************************************************
// Implements computed attribute constructors, resolving the attribute name at runtime and constructing a single 
// attribute node according to the XPath specification.

XPathVal XPathEvaluator::evaluate_computed_attribute_constructor(const XPathNode *Node, uint32_t CurrentPrefix)
{
   if ((!Node) or (Node->type != XPathNodeType::COMPUTED_ATTRIBUTE_CONSTRUCTOR)) {
      record_error("Invalid computed attribute constructor node encountered.", true);
      return XPathVal();
   }

   if (!Node->has_constructor_info()) {
      record_error("Computed attribute constructor is missing metadata.", true);
      return XPathVal();
   }

   ConstructorQName name_info;

   if (Node->has_name_expression()) {
      auto name_string = evaluate_constructor_name_string(Node->get_name_expression(), CurrentPrefix);
      if (!name_string) return XPathVal();

      auto parsed = parse_constructor_qname_string(*name_string);
      if (!parsed.valid) {
         record_error("Computed attribute name must resolve to a QName.", true);
         return XPathVal();
      }

      if (!parsed.prefix.empty()) name_info.prefix = parsed.prefix;
      name_info.local = parsed.local;
      name_info.namespace_uri = parsed.namespace_uri;
      name_info.valid = true;
   }
   else {
      const auto &info = *Node->constructor_info;
      name_info.valid = true;
      name_info.prefix = info.prefix;
      name_info.local = info.name;
      name_info.namespace_uri = info.namespace_uri;
   }

   if (name_info.local.empty()) {
      record_error("Computed attribute constructor requires a local name.", true);
      return XPathVal();
   }

   auto resolve_prefix_in_context = [&](std::string_view Prefix) -> std::optional<uint32_t>
   {
      if (Prefix.empty()) return uint32_t{0};
      if (!xml) return std::nullopt;
      if (Prefix.compare("xml") IS 0) return register_constructor_namespace("http://www.w3.org/XML/1998/namespace");
      if (!context.context_node) return std::nullopt;

      uint32_t resolved_hash = 0;
      if (xml->resolvePrefix(Prefix, context.context_node->ID, resolved_hash) IS ERR::Okay) return resolved_hash;
      return std::nullopt;
   };

   uint32_t namespace_id = 0;
   if (!name_info.namespace_uri.empty()) namespace_id = register_constructor_namespace(name_info.namespace_uri);
   else if (!name_info.prefix.empty()) {
      auto resolved = resolve_prefix_in_context(name_info.prefix);
      if (!resolved.has_value()) {
         record_error("Attribute prefix is not bound in scope.", true);
         return XPathVal();
      }
      namespace_id = *resolved;
   }

   std::string attribute_name;
   if (name_info.prefix.empty()) attribute_name = name_info.local;
   else {
      attribute_name = name_info.prefix;
      attribute_name += ':';
      attribute_name += name_info.local;
   }

   const XPathNode *content_node = Node->child_count() > 0 ? Node->get_child(0) : nullptr;
   auto value_string = evaluate_constructor_content_string(content_node, CurrentPrefix);
   if (!value_string) return XPathVal();

   pf::vector<XMLAttrib> attribute_attribs;
   attribute_attribs.emplace_back("$attribute", "");
   attribute_attribs.emplace_back(attribute_name, *value_string);

   XMLTag attribute_tag(next_constructed_node_id--, 0, attribute_attribs);
   attribute_tag.ParentID = 0;
   attribute_tag.Flags = XTF::NIL;
   attribute_tag.NamespaceID = namespace_id;

   auto stored = std::make_unique<XMLTag>(std::move(attribute_tag));
   XMLTag *owner = stored.get();
   const XMLAttrib *attribute_ptr = owner->Attribs.size() > 1 ? &owner->Attribs[1] : nullptr;
   constructed_nodes.push_back(std::move(stored));

   NODES nodes;
   nodes.push_back(owner);

   std::vector<const XMLAttrib *> attributes;
   attributes.push_back(attribute_ptr);

   return XPathVal(nodes, std::nullopt, {}, std::move(attributes));
}

//********************************************************************************************************************
// Evaluates text constructors by flattening the enclosed expression into a string and returning it as a text node 
// for inclusion in the result sequence.

XPathVal XPathEvaluator::evaluate_text_constructor(const XPathNode *Node, uint32_t CurrentPrefix)
{
   if ((!Node) or (Node->type != XPathNodeType::TEXT_CONSTRUCTOR)) {
      record_error("Invalid text constructor node encountered.", true);
      return XPathVal();
   }

   const XPathNode *content_node = Node->child_count() > 0 ? Node->get_child(0) : nullptr;
   auto content = evaluate_constructor_content_string(content_node, CurrentPrefix);
   if (!content) return XPathVal();

   pf::vector<XMLAttrib> text_attribs;
   text_attribs.emplace_back("", *content);

   XMLTag text_node(next_constructed_node_id--, 0, text_attribs);
   text_node.ParentID = 0;
   text_node.Flags = XTF::NIL;
   text_node.NamespaceID = 0;

   auto stored = std::make_unique<XMLTag>(std::move(text_node));
   XMLTag *root = stored.get();
   constructed_nodes.push_back(std::move(stored));

   NODES nodes;
   nodes.push_back(root);

   std::vector<std::string> string_values;
   string_values.push_back(*content);

   return XPathVal(nodes, *content, std::move(string_values));
}

//********************************************************************************************************************
// Evaluates comment constructors by producing the textual content and wrapping it in a
// comment node for downstream processing.

XPathVal XPathEvaluator::evaluate_comment_constructor(const XPathNode *Node, uint32_t CurrentPrefix)
{
   if ((!Node) or (Node->type != XPathNodeType::COMMENT_CONSTRUCTOR)) {
      record_error("Invalid comment constructor node encountered.", true);
      return XPathVal();
   }

   const XPathNode *content_node = Node->child_count() > 0 ? Node->get_child(0) : nullptr;
   auto content = evaluate_constructor_content_string(content_node, CurrentPrefix);
   if (!content) return XPathVal();

   auto double_dash = content->find("--");
   if (!(double_dash IS std::string::npos))
   {
      record_error("Comments cannot contain consecutive hyphen characters.", true);
      return XPathVal();
   }

   if (!content->empty() and (content->back() IS '-'))
   {
      record_error("Comments cannot end with a hyphen.", true);
      return XPathVal();
   }

   pf::vector<XMLAttrib> comment_attribs;
   comment_attribs.emplace_back("", *content);

   XMLTag comment_node(next_constructed_node_id--, 0, comment_attribs);
   comment_node.ParentID = 0;
   comment_node.Flags = XTF::COMMENT;
   comment_node.NamespaceID = 0;

   auto stored = std::make_unique<XMLTag>(std::move(comment_node));
   XMLTag *root = stored.get();
   constructed_nodes.push_back(std::move(stored));

   NODES nodes;
   nodes.push_back(root);

   std::vector<std::string> string_values;
   string_values.push_back(*content);

   return XPathVal(nodes, *content, std::move(string_values));
}

//********************************************************************************************************************
// Executes processing-instruction constructors, resolving the target name and content while enforcing NCName rules 
// defined by XPath.

XPathVal XPathEvaluator::evaluate_pi_constructor(const XPathNode *Node, uint32_t CurrentPrefix)
{
   if ((!Node) or (Node->type != XPathNodeType::PI_CONSTRUCTOR)) {
      record_error("Invalid processing-instruction constructor encountered.", true);
      return XPathVal();
   }

   std::string target;

   if (Node->has_name_expression())
   {
      auto target_string = evaluate_constructor_name_string(Node->get_name_expression(), CurrentPrefix);
      if (!target_string) return XPathVal();
      target = *target_string;
   }
   else if (Node->has_constructor_info()) target = Node->constructor_info->name;

   target = trim_constructor_whitespace(target);

   if (target.empty()) {
      record_error("Processing-instruction constructor requires a target name.", true);
      return XPathVal();
   }

   if (!is_valid_ncname(target)) {
      record_error("Processing-instruction target must be an NCName.", true);
      return XPathVal();
   }

   const XPathNode *content_node = Node->child_count() > 0 ? Node->get_child(0) : nullptr;
   auto content = evaluate_constructor_content_string(content_node, CurrentPrefix);
   if (!content) return XPathVal();

   auto terminator = content->find("?>");
   if (!(terminator IS std::string::npos))
   {
      record_error("Processing-instruction content cannot contain '?>'.", true);
      return XPathVal();
   }

   std::string attribute_name = "?" + target;

   pf::vector<XMLAttrib> instruction_attribs;
   instruction_attribs.emplace_back(attribute_name, *content);

   XMLTag instruction(next_constructed_node_id--, 0, instruction_attribs);
   instruction.ParentID = 0;
   instruction.Flags = XTF::INSTRUCTION;
   instruction.NamespaceID = 0;

   auto stored = std::make_unique<XMLTag>(std::move(instruction));
   XMLTag *root = stored.get();
   constructed_nodes.push_back(std::move(stored));

   NODES nodes;
   nodes.push_back(root);

   std::vector<std::string> string_values;
   string_values.push_back(*content);

   return XPathVal(nodes, *content, std::move(string_values));
}

//********************************************************************************************************************
// Produces document nodes by evaluating the enclosed content, constructing a temporary
// root scope, and appending the resulting children to a synthetic document element.

XPathVal XPathEvaluator::evaluate_document_constructor(const XPathNode *Node, uint32_t CurrentPrefix)
{
   if ((!Node) or (Node->type != XPathNodeType::DOCUMENT_CONSTRUCTOR))
   {
      record_error("Invalid document constructor node encountered.", true);
      return XPathVal();
   }

   pf::vector<XMLAttrib> document_attribs;
   document_attribs.emplace_back("#document", "");

   XMLTag document_node(next_constructed_node_id--, 0, document_attribs);
   document_node.ParentID = 0;
   document_node.Flags = XTF::NIL;
   document_node.NamespaceID = 0;

   ConstructorNamespaceScope scope;
   scope.parent = nullptr;

   if (Node->child_count() > 0) {
      const XPathNode *content_node = Node->get_child(0);
      if (content_node) {
         if (!content_node->value.empty()) {
            pf::vector<XMLAttrib> text_attribs;
            text_attribs.emplace_back("", content_node->value);
            XMLTag text_node(next_constructed_node_id--, 0, text_attribs);
            text_node.ParentID = document_node.ID;
            document_node.Children.push_back(std::move(text_node));
         }
         else if (content_node->child_count() > 0) {
            const XPathNode *expr = content_node->get_child(0);
            if (expr) {
               size_t previous_constructed = constructed_nodes.size();
               auto saved_id = next_constructed_node_id;
               XPathVal value = evaluate_expression(expr, CurrentPrefix);
               if (expression_unsupported) return XPathVal();
               if (!append_constructor_sequence(document_node, value, CurrentPrefix, scope)) return XPathVal();
               constructed_nodes.resize(previous_constructed);
               next_constructed_node_id = saved_id;
            }
         }
      }
   }

   auto stored = std::make_unique<XMLTag>(std::move(document_node));
   XMLTag *root = stored.get();
   constructed_nodes.push_back(std::move(stored));

   NODES nodes;
   nodes.push_back(root);

   std::vector<std::string> string_values;
   std::string node_string = XPathVal::node_string_value(root);
   string_values.push_back(node_string);

   return XPathVal(nodes, node_string, std::move(string_values));
}

//********************************************************************************************************************

XPathVal XPathEvaluator::evaluate_flwor_pipeline(const XPathNode *Node, uint32_t CurrentPrefix)
{
   if (!Node) {
      record_error("FLWOR expression is missing its AST node.", true);
      return XPathVal();
   }

   if (Node->child_count() < 2) {
      record_error("FLWOR expression requires at least one clause and a return expression.", true);
      return XPathVal();
   }

   const XPathNode *return_node = Node->get_child(Node->child_count() - 1);
   if (!return_node) {
      record_error("FLWOR expression is missing its return clause.", true);
      return XPathVal();
   }

   bool tracing_flwor = is_trace_enabled(TraceCategory::XPath);

   struct FlworTuple
   {
      std::unordered_map<std::string, XPathVal> bindings;
      XMLTag *context_node = nullptr;
      const XMLAttrib *context_attribute = nullptr;
      size_t context_position = 1;
      size_t context_size = 1;
      std::vector<XPathVal> order_keys;
      std::vector<bool> order_key_empty;
      size_t original_index = 0;
   };

   struct TupleScope
   {
      XPathEvaluator & evaluator;
      XPathContext & context_ref;
      std::vector<VariableBindingGuard> guards;

      TupleScope(XPathEvaluator &Evaluator, XPathContext &ContextRef, const FlworTuple &Tuple)
         : evaluator(Evaluator), context_ref(ContextRef)
      {
         evaluator.push_context(Tuple.context_node, Tuple.context_position, Tuple.context_size, Tuple.context_attribute);
         guards.reserve(Tuple.bindings.size());
         for (const auto &entry : Tuple.bindings) guards.emplace_back(context_ref, entry.first, entry.second);
      }

      ~TupleScope()
      {
         evaluator.pop_context();
      }
   };

   struct GroupKey
   {
      pf::vector<XPathVal> values;
   };

   struct GroupKeyHasher
   {
      size_t operator()(const GroupKey &Key) const noexcept
      {
         size_t seed = Key.values.size();
         for (const auto &value : Key.values) seed = combine_group_hash(seed, hash_xpath_group_value(value));
         return seed;
      }
   };

   struct GroupKeyEqual
   {
      bool operator()(const GroupKey &Left, const GroupKey &Right) const noexcept
      {
         if (Left.values.size() != Right.values.size()) return false;
         for (size_t index = 0; index < Left.values.size(); ++index)
         {
            if (!compare_xpath_values(Left.values[index], Right.values[index])) return false;
         }
         return true;
      }
   };

   auto nodeset_length = [](const XPathVal &value) -> size_t
   {
      size_t length = value.node_set.size();
      if (length < value.node_set_attributes.size()) length = value.node_set_attributes.size();
      if (length < value.node_set_string_values.size()) length = value.node_set_string_values.size();
      if ((length IS 0) and value.node_set_string_override.has_value()) length = 1;
      return length;
   };

   auto nodeset_string_at = [](const XPathVal &value, size_t index) -> std::string
   {
      if (index < value.node_set_string_values.size()) return value.node_set_string_values[index];

      bool use_override = value.node_set_string_override.has_value() and value.node_set_string_values.empty() and (index IS 0);
      if (use_override) return *value.node_set_string_override;

      if (index < value.node_set_attributes.size()) {
         const XMLAttrib *attribute = value.node_set_attributes[index];
         if (attribute) return attribute->Value;
      }

      if (index < value.node_set.size()) {
         XMLTag *node = value.node_set[index];
         if (node) return XPathVal::node_string_value(node);
      }

      return std::string();
   };

   auto trace_detail = [&](const char *Format, auto ...Args)
   {
      if (!tracing_flwor) return;
      pf::Log log("XPath");
      log.msg(trace_detail_level, Format, Args...);
   };

   auto trace_verbose = [&](const char *Format, auto ...Args)
   {
      if (!tracing_flwor) return;
      pf::Log log("XPath");
      log.msg(trace_verbose_level, Format, Args...);
   };

   auto describe_value_for_trace = [&](const XPathVal &value) -> std::string
   {
      switch (value.Type)
      {
         case XPVT::Boolean:
            return value.to_boolean() ? std::string("true") : std::string("false");

         case XPVT::Number:
            return value.to_string();

         case XPVT::String:
         case XPVT::Date:
         case XPVT::Time:
         case XPVT::DateTime:
            return value.to_string();

         case XPVT::NodeSet:
         {
            size_t length = nodeset_length(value);
            std::string summary("node-set[");
            summary.append(std::to_string(length));
            summary.push_back(']');

            if (length > 0)
            {
               std::string preview = nodeset_string_at(value, 0);
               if (!preview.empty())
               {
                  summary.append(": ");
                  summary.append(preview);
               }
            }

            return summary;
         }

         default:
            break;
      }

      return value.to_string();
   };

   auto describe_tuple_bindings = [&](const FlworTuple &tuple) -> std::string
   {
      if (tuple.bindings.empty()) return std::string();

      std::vector<std::string> entries;
      entries.reserve(tuple.bindings.size());

      for (const auto &entry : tuple.bindings)
      {
         std::string binding = entry.first;
         binding.append("=");
         binding.append(describe_value_for_trace(entry.second));
         entries.push_back(std::move(binding));
      }

      std::sort(entries.begin(), entries.end());

      std::string summary;
      for (size_t index = 0; index < entries.size(); ++index)
      {
         if (index > 0) summary.append(", ");
         summary.append(entries[index]);
      }

      return summary;
   };

   auto describe_value_sequence = [&](const auto &values) -> std::string
   {
      if (values.empty()) return std::string();

      std::string summary;
      for (size_t index = 0; index < values.size(); ++index)
      {
         if (index > 0) summary.append(" | ");
         summary.append(describe_value_for_trace(values[index]));
      }

      return summary;
   };

   auto ensure_nodeset_binding = [&](XPathVal &value)
   {
      if (value.Type IS XPVT::NodeSet) return;

      bool has_existing = !value.is_empty();
      std::string preserved_string;
      if (has_existing) preserved_string = value.to_string();

      value.Type = XPVT::NodeSet;
      value.NumberValue = 0.0;
      value.StringValue.clear();
      value.node_set.clear();
      value.node_set_attributes.clear();
      value.node_set_string_values.clear();
      value.node_set_string_override.reset();
      value.preserve_node_order = false;
      value.schema_type_info.reset();
      value.schema_validated = false;

      if (has_existing) {
         value.node_set.push_back(nullptr);
         value.node_set_attributes.push_back(nullptr);
         value.node_set_string_values.push_back(std::move(preserved_string));
         value.node_set_string_override = value.node_set_string_values.back();
      }
   };

   auto append_binding_value = [&](XPathVal &target_nodeset, const XPathVal &source_value)
   {
      target_nodeset.preserve_node_order = false;
      if (source_value.Type IS XPVT::NodeSet) {
         size_t length = nodeset_length(source_value);
         for (size_t value_index = 0; value_index < length; ++value_index) {
            XMLTag *node = value_index < source_value.node_set.size() ? source_value.node_set[value_index] : nullptr;
            target_nodeset.node_set.push_back(node);

            const XMLAttrib *attribute = nullptr;
            if (value_index < source_value.node_set_attributes.size()) attribute = source_value.node_set_attributes[value_index];
            target_nodeset.node_set_attributes.push_back(attribute);

            std::string node_string = nodeset_string_at(source_value, value_index);
            target_nodeset.node_set_string_values.push_back(std::move(node_string));
         }

         if (!target_nodeset.node_set_string_values.empty()) target_nodeset.node_set_string_override.reset();
         return;
      }

      if (source_value.is_empty()) return;

      std::string atomic_string = source_value.to_string();
      target_nodeset.node_set.push_back(nullptr);
      target_nodeset.node_set_attributes.push_back(nullptr);
      target_nodeset.node_set_string_values.push_back(std::move(atomic_string));

      if (!target_nodeset.node_set_string_values.empty()) target_nodeset.node_set_string_override.reset();
   };

   auto merge_binding_values = [&](XPathVal &target, const XPathVal &source)
   {
      ensure_nodeset_binding(target);
      append_binding_value(target, source);
   };

   auto merge_binding_maps = [&](FlworTuple &target_tuple, const FlworTuple &source_tuple)
   {
      for (const auto &entry : source_tuple.bindings) {
         const std::string &variable_name = entry.first;
         const XPathVal &source_value = entry.second;

         auto existing = target_tuple.bindings.find(variable_name);
         if (existing == target_tuple.bindings.end()) {
            target_tuple.bindings[variable_name] = source_value;
            continue;
         }

         merge_binding_values(existing->second, source_value);
      }

      if (target_tuple.original_index > source_tuple.original_index) {
         target_tuple.original_index = source_tuple.original_index;
      }
   };

   std::vector<const XPathNode *> binding_nodes;
   binding_nodes.reserve(Node->child_count());

   const XPathNode *where_clause = nullptr;
   const XPathNode *group_clause = nullptr;
   const XPathNode *order_clause = nullptr;
   const XPathNode *count_clause = nullptr;

   for (size_t index = 0; index + 1 < Node->child_count(); ++index) {
      const XPathNode *child = Node->get_child(index);
      if (!child) {
         record_error("FLWOR expression contains an invalid clause.", true);
         return XPathVal();
      }

      if ((child->type IS XPathNodeType::FOR_BINDING) or (child->type IS XPathNodeType::LET_BINDING)) {
         binding_nodes.push_back(child);
         continue;
      }

      if (child->type IS XPathNodeType::WHERE_CLAUSE) {
         where_clause = child;
         continue;
      }

      if (child->type IS XPathNodeType::GROUP_CLAUSE) {
         group_clause = child;
         continue;
      }

      if (child->type IS XPathNodeType::ORDER_CLAUSE) {
         order_clause = child;
         continue;
      }

      if (child->type IS XPathNodeType::COUNT_CLAUSE) {
         count_clause = child;
         continue;
      }

      record_error("FLWOR expression contains an unsupported clause type.", true);
      return XPathVal();
   }

   if (binding_nodes.empty()) {
      record_error("FLWOR expression is missing binding clauses.", true);
      return XPathVal();
   }

   std::vector<FlworTuple> tuples;
   tuples.reserve(8);

   FlworTuple initial_tuple;
   initial_tuple.context_node = context.context_node;
   initial_tuple.context_attribute = context.attribute_node;
   initial_tuple.context_position = context.position;
   initial_tuple.context_size = context.size;
   initial_tuple.original_index = 0;
   tuples.push_back(std::move(initial_tuple));

   for (const XPathNode *binding_node : binding_nodes) {
      if (!binding_node) continue;

      if (binding_node->type IS XPathNodeType::LET_BINDING) {
         if ((binding_node->value.empty()) or (binding_node->child_count() IS 0)) {
            record_error("Let binding requires a variable name and expression.", true);
            return XPathVal();
         }

         const XPathNode *binding_expr = binding_node->get_child(0);
         if (!binding_expr) {
            record_error("Let binding requires an expression node.", true);
            return XPathVal();
         }

         std::vector<FlworTuple> next_tuples;
         next_tuples.reserve(tuples.size());

         for (const auto &tuple : tuples) {
            TupleScope scope(*this, context, tuple);
            XPathVal bound_value = evaluate_expression(binding_expr, CurrentPrefix);
            if (expression_unsupported) {
               record_error("Let binding expression could not be evaluated.");
               return XPathVal();
            }

            FlworTuple updated_tuple = tuple;
            updated_tuple.bindings[binding_node->value] = std::move(bound_value);
            next_tuples.push_back(std::move(updated_tuple));
         }

         tuples = std::move(next_tuples);
         for (size_t tuple_index = 0; tuple_index < tuples.size(); ++tuple_index) {
            tuples[tuple_index].original_index = tuple_index;
         }
         continue;
      }

      if (binding_node->type IS XPathNodeType::FOR_BINDING) {
         if ((binding_node->value.empty()) or (binding_node->child_count() IS 0)) {
            record_error("For binding requires a variable name and sequence.", true);
            return XPathVal();
         }

         const XPathNode *sequence_expr = binding_node->get_child(0);
         if (!sequence_expr) {
            record_error("For binding requires a sequence expression.", true);
            return XPathVal();
         }

         std::vector<FlworTuple> next_tuples;
         next_tuples.reserve(tuples.size());

         for (const auto &tuple : tuples) {
            TupleScope scope(*this, context, tuple);
            XPathVal sequence_value = evaluate_expression(sequence_expr, CurrentPrefix);
            if (expression_unsupported) {
               record_error("For binding sequence could not be evaluated.");
               return XPathVal();
            }

            if (sequence_value.Type != XPVT::NodeSet) {
               record_error("For binding sequences must evaluate to node-sets.", true);
               return XPathVal();
            }

            size_t sequence_size = sequence_value.node_set.size();
            if (sequence_size IS 0) continue;

            bool use_override = sequence_value.node_set_string_override.has_value() and
               sequence_value.node_set_string_values.empty();

            for (size_t item_index = 0; item_index < sequence_size; ++item_index) {
               FlworTuple next_tuple = tuple;

               XMLTag *item_node = sequence_value.node_set[item_index];
               const XMLAttrib *item_attribute = nullptr;
               if (item_index < sequence_value.node_set_attributes.size()) {
                  item_attribute = sequence_value.node_set_attributes[item_index];
               }

               std::string item_string;
               if (item_index < sequence_value.node_set_string_values.size()) {
                  item_string = sequence_value.node_set_string_values[item_index];
               }
               else if (use_override) item_string = *sequence_value.node_set_string_override;
               else if (item_attribute) item_string = item_attribute->Value;
               else if (item_node) item_string = XPathVal::node_string_value(item_node);

               XPathVal bound_value = xpath_nodeset_singleton(item_node, item_attribute, item_string);

               next_tuple.bindings[binding_node->value] = std::move(bound_value);
               next_tuple.context_node = item_node;
               next_tuple.context_attribute = item_attribute;
               next_tuple.context_position = item_index + 1;
               next_tuple.context_size = sequence_size;

               next_tuples.push_back(std::move(next_tuple));
            }
         }

         tuples = std::move(next_tuples);
         for (size_t tuple_index = 0; tuple_index < tuples.size(); ++tuple_index) {
            tuples[tuple_index].original_index = tuple_index;
         }
         continue;
      }

      record_error("FLWOR expression contains an unsupported binding clause.", true);
      return XPathVal();
   }

   if (tuples.empty()) {
      NODES empty_nodes;
      return XPathVal(empty_nodes);
   }

   if (where_clause) {
      if (where_clause->child_count() IS 0) {
         record_error("Where clause requires a predicate expression.", true);
         return XPathVal();
      }

      const XPathNode *predicate_node = where_clause->get_child(0);
      if (!predicate_node) {
         record_error("Where clause requires a predicate expression.", true);
         return XPathVal();
      }

      std::vector<FlworTuple> filtered;
      filtered.reserve(tuples.size());

      for (const auto &tuple : tuples) {
         TupleScope scope(*this, context, tuple);
         XPathVal predicate_value = evaluate_expression(predicate_node, CurrentPrefix);
         if (expression_unsupported) {
            record_error("Where clause expression could not be evaluated.");
            return XPathVal();
         }

         if (predicate_value.to_boolean()) filtered.push_back(tuple);
      }

      tuples = std::move(filtered);
      for (size_t tuple_index = 0; tuple_index < tuples.size(); ++tuple_index) {
         tuples[tuple_index].original_index = tuple_index;
      }

      if (tuples.empty()) {
         NODES empty_nodes;
         return XPathVal(empty_nodes);
      }
   }

   if (group_clause) {
      if (group_clause->child_count() IS 0) {
         record_error("Group clause requires at least one key definition.", true);
         return XPathVal();
      }

      std::unordered_map<GroupKey, size_t, GroupKeyHasher, GroupKeyEqual> group_lookup;
      group_lookup.reserve(tuples.size());
      std::vector<FlworTuple> grouped;
      grouped.reserve(tuples.size());

      if (tracing_flwor) {
         trace_detail("FLWOR group-by: tuple-count=%zu, key-count=%zu", tuples.size(), group_clause->child_count());
      }

      for (size_t tuple_index = 0; tuple_index < tuples.size(); ++tuple_index) {
         const FlworTuple &tuple = tuples[tuple_index];
         GroupKey key;
         key.values.reserve(group_clause->child_count());

         std::string tuple_binding_summary;
         if (tracing_flwor) tuple_binding_summary = describe_tuple_bindings(tuple);

         {
            TupleScope scope(*this, context, tuple);
            for (size_t key_index = 0; key_index < group_clause->child_count(); ++key_index) {
               const XPathNode *key_node = group_clause->get_child(key_index);
               if (!key_node) {
                  record_error("Group clause contains an invalid key.", true);
                  return XPathVal();
               }

               const XPathNode *key_expr = key_node->get_child(0);
               if (!key_expr) {
                  record_error("Group key requires an expression.", true);
                  return XPathVal();
               }

               XPathVal key_value = evaluate_expression(key_expr, CurrentPrefix);
               if (expression_unsupported) {
                  record_error("Group key expression could not be evaluated.");
                  return XPathVal();
               }

               key.values.push_back(std::move(key_value));

               if (tracing_flwor) {
                  const XPathVal &evaluated_value = key.values.back();
                  std::string value_summary = describe_value_for_trace(evaluated_value);
                  trace_verbose("FLWOR group key[%zu,%zu]: %s", tuple_index, key_index, value_summary.c_str());
               }
            }
         }

         std::string key_summary;
         if (tracing_flwor) key_summary = describe_value_sequence(key.values);

         auto lookup = group_lookup.find(key);
         if (lookup == group_lookup.end()) {
            size_t group_index = grouped.size();

            FlworTuple grouped_tuple = tuple;
            grouped_tuple.original_index = tuple.original_index;

            for (size_t key_index = 0; key_index < group_clause->child_count(); ++key_index) {
               const XPathNode *key_node = group_clause->get_child(key_index);
               if (!key_node) continue;
               const auto *info = key_node->get_group_key_info();
               if ((info) and info->has_variable()) {
                  grouped_tuple.bindings[info->variable_name] = key.values[key_index];
               }
            }

            grouped.push_back(std::move(grouped_tuple));

            if (tracing_flwor) {
               if (tuple_binding_summary.empty()) {
                  trace_detail("FLWOR group create tuple[%zu] -> group %zu, keys: %s", tuple_index, group_index,
                     key_summary.c_str());
               }
               else {
                  trace_detail("FLWOR group create tuple[%zu] -> group %zu, keys: %s, bindings: %s", tuple_index,
                     group_index, key_summary.c_str(), tuple_binding_summary.c_str());
               }
            }

            group_lookup.emplace(std::move(key), group_index);
            continue;
         }

         FlworTuple &existing_group = grouped[lookup->second];
         merge_binding_maps(existing_group, tuple);

         for (size_t key_index = 0; key_index < group_clause->child_count(); ++key_index) {
            const XPathNode *key_node = group_clause->get_child(key_index);
            if (!key_node) continue;
            const auto *info = key_node->get_group_key_info();
            if ((info) and info->has_variable()) {
               existing_group.bindings[info->variable_name] = key.values[key_index];
            }
         }

         if (tracing_flwor) {
            std::string merged_summary = describe_tuple_bindings(existing_group);
            trace_detail("FLWOR group merge tuple[%zu] into group %zu, keys: %s", tuple_index, lookup->second,
               key_summary.c_str());
            if (!merged_summary.empty()) {
               trace_verbose("FLWOR group[%zu] bindings: %s", lookup->second, merged_summary.c_str());
            }
         }
      }

      tuples = std::move(grouped);
      if (tuples.empty()) {
         NODES empty_nodes;
         return XPathVal(empty_nodes);
      }
   }

   if (order_clause) {
      if (order_clause->child_count() IS 0) {
         record_error("Order by clause requires at least one sort specification.", true);
         return XPathVal();
      }

      struct OrderSpecMetadata
      {
         const XPathNode *node = nullptr;
         XPathNode::XPathOrderSpecOptions options{};
         bool has_options = false;
         XPathOrderComparatorOptions comparator_options{};
      };

      std::vector<OrderSpecMetadata> order_specs;
      order_specs.reserve(order_clause->child_count());

      for (size_t spec_index = 0; spec_index < order_clause->child_count(); ++spec_index) {
         const XPathNode *spec_node = order_clause->get_child(spec_index);
         if (!spec_node) {
            record_error("Order by clause contains an invalid specification.", true);
            return XPathVal();
         }

         OrderSpecMetadata metadata;
         metadata.node = spec_node;
         if (spec_node->has_order_spec_options()) {
            metadata.options = *spec_node->get_order_spec_options();
            metadata.has_options = true;
         }

         if (metadata.has_options and metadata.options.has_collation()) {
            const std::string &uri = metadata.options.collation_uri;
            if (!xpath_collation_supported(uri)) {
               record_error("FLWOR order by clause collation '" + uri + "' is not supported.", true);
               return XPathVal();
            }
            metadata.comparator_options.has_collation = true;
            metadata.comparator_options.collation_uri = uri;
         }

         if (metadata.has_options) {
            metadata.comparator_options.descending = metadata.options.is_descending;
            metadata.comparator_options.has_empty_mode = metadata.options.has_empty_mode;
            metadata.comparator_options.empty_is_greatest = metadata.options.empty_is_greatest;
         }

         order_specs.push_back(metadata);
      }

      if (tracing_flwor) {
         trace_detail("FLWOR order-by: tuple-count=%zu, spec-count=%zu", tuples.size(), order_specs.size());
         for (size_t spec_index = 0; spec_index < order_specs.size(); ++spec_index) {
            const auto &spec = order_specs[spec_index];
            const XPathNode *spec_expr = spec.node ? spec.node->get_child(0) : nullptr;
            std::string expression_signature;
            if (spec_expr) expression_signature = build_ast_signature(spec_expr);
            else expression_signature = std::string("<missing>");

            std::string collation = spec.comparator_options.has_collation ? spec.comparator_options.collation_uri :
               std::string("(default)");
            const char *direction = spec.comparator_options.descending ? "descending" : "ascending";
            const char *empty_mode = "no-empty-order";
            if (spec.comparator_options.has_empty_mode) {
               empty_mode = spec.comparator_options.empty_is_greatest ? "empty-greatest" : "empty-least";
            }

            trace_detail("FLWOR order spec[%zu]: expr=%s, collation=%s, direction=%s, empty=%s", spec_index,
               expression_signature.c_str(), collation.c_str(), direction, empty_mode);
         }
      }

      for (size_t tuple_index = 0; tuple_index < tuples.size(); ++tuple_index) {
         FlworTuple &tuple = tuples[tuple_index];
         tuple.order_keys.clear();
         tuple.order_key_empty.clear();
         tuple.order_keys.reserve(order_specs.size());
         tuple.order_key_empty.reserve(order_specs.size());

         if (tracing_flwor) {
            std::string binding_summary = describe_tuple_bindings(tuple);
            if (binding_summary.empty()) {
               trace_detail("FLWOR order tuple[%zu] original=%zu has no bindings", tuple_index, tuple.original_index);
            }
            else {
               trace_detail("FLWOR order tuple[%zu] original=%zu bindings: %s", tuple_index, tuple.original_index,
                  binding_summary.c_str());
            }
         }

         TupleScope scope(*this, context, tuple);
         for (size_t spec_index = 0; spec_index < order_specs.size(); ++spec_index) {
            const auto &spec = order_specs[spec_index];
            const XPathNode *spec_expr = spec.node ? spec.node->get_child(0) : nullptr;
            if (!spec_expr) {
               record_error("Order by clause requires an expression.", true);
               return XPathVal();
            }

            XPathVal key_value = evaluate_expression(spec_expr, CurrentPrefix);
            if (expression_unsupported) {
               record_error("Order by expression could not be evaluated.");
               return XPathVal();
            }

            bool is_empty_key = xpath_order_key_is_empty(key_value);

            tuple.order_keys.push_back(std::move(key_value));
            tuple.order_key_empty.push_back(is_empty_key);

            if (tracing_flwor) {
               const XPathVal &stored_value = tuple.order_keys.back();
               std::string value_summary = describe_value_for_trace(stored_value);
               trace_verbose("FLWOR order key[%zu,%zu]: %s%s", tuple_index, spec_index,
                  value_summary.c_str(), is_empty_key ? " (empty)" : "");
            }
         }

         if (tracing_flwor) {
            std::string key_summary = describe_value_sequence(tuple.order_keys);
            trace_detail("FLWOR order tuple[%zu] generated %zu key(s): %s", tuple_index, tuple.order_keys.size(),
               key_summary.c_str());
         }
      }

      auto comparator = [&](const FlworTuple &lhs, const FlworTuple &rhs) -> bool
      {
         for (size_t spec_index = 0; spec_index < order_specs.size(); ++spec_index) {
            const auto &spec = order_specs[spec_index];
            const XPathVal *left_ptr = spec_index < lhs.order_keys.size() ? &lhs.order_keys[spec_index] : nullptr;
            const XPathVal *right_ptr = spec_index < rhs.order_keys.size() ? &rhs.order_keys[spec_index] : nullptr;

            XPathVal left_placeholder;
            XPathVal right_placeholder;

            const XPathVal &left_value = left_ptr ? *left_ptr : left_placeholder;
            const XPathVal &right_value = right_ptr ? *right_ptr : right_placeholder;

            bool left_empty = left_ptr ? (spec_index < lhs.order_key_empty.size() ? lhs.order_key_empty[spec_index] :
               xpath_order_key_is_empty(left_value)) : true;
            bool right_empty = right_ptr ? (spec_index < rhs.order_key_empty.size() ? rhs.order_key_empty[spec_index] :
               xpath_order_key_is_empty(right_value)) : true;

            int comparison = xpath_compare_order_keys(left_value, left_empty, right_value, right_empty,
               spec.comparator_options);
            if (comparison != 0) return comparison < 0;
         }

         return lhs.original_index < rhs.original_index;
      };

      if (order_clause->order_clause_is_stable) std::stable_sort(tuples.begin(), tuples.end(), comparator);
      else std::sort(tuples.begin(), tuples.end(), comparator);

      if (tracing_flwor) {
         std::string index_summary;
         index_summary.reserve(tuples.size() * 4);
         for (size_t tuple_index = 0; tuple_index < tuples.size(); ++tuple_index) {
            if (tuple_index > 0) index_summary.append(", ");
            index_summary.append(std::to_string(tuples[tuple_index].original_index));
         }

         const char *sort_mode = order_clause->order_clause_is_stable ? "stable" : "unstable";
         trace_detail("FLWOR order-by sorted (%s), original indices: %s", sort_mode, index_summary.c_str());
      }
   }

   if (count_clause) {
      if (count_clause->value.empty()) {
         record_error("Count clause requires a variable name.", true);
         return XPathVal();
      }

      if (tracing_flwor) {
         trace_detail("FLWOR count clause applying to %zu sorted tuple(s)", tuples.size());
      }

      for (size_t tuple_index = 0; tuple_index < tuples.size(); ++tuple_index) {
         XPathVal counter(double(tuple_index + 1));
         tuples[tuple_index].bindings[count_clause->value] = std::move(counter);

         if (tracing_flwor) {
            trace_verbose("FLWOR count tuple[%zu] original=%zu -> %zu", tuple_index,
               tuples[tuple_index].original_index, tuple_index + 1);
         }
      }
   }

   size_t tuple_count = tuples.size();
   for (size_t tuple_index = 0; tuple_index < tuple_count; ++tuple_index) {
      tuples[tuple_index].context_position = tuple_index + 1;
      tuples[tuple_index].context_size = tuple_count;
   }

   NODES combined_nodes;
   std::vector<const XMLAttrib *> combined_attributes;
   std::vector<std::string> combined_strings;
   std::optional<std::string> combined_override;

   for (size_t tuple_index = 0; tuple_index < tuples.size(); ++tuple_index) {
      const auto &tuple = tuples[tuple_index];
      if (tracing_flwor) {
         std::string binding_summary = describe_tuple_bindings(tuple);
         if (binding_summary.empty()) {
            trace_detail("FLWOR return tuple[%zu] original=%zu context=%zu/%zu evaluating", tuple_index,
               tuple.original_index, tuple.context_position, tuple.context_size);
         }
         else {
            trace_detail("FLWOR return tuple[%zu] original=%zu context=%zu/%zu bindings: %s", tuple_index,
               tuple.original_index, tuple.context_position, tuple.context_size, binding_summary.c_str());
         }
      }

      TupleScope scope(*this, context, tuple);
      XPathVal iteration_value = evaluate_expression(return_node, CurrentPrefix);
      if (expression_unsupported) {
         if (tracing_flwor) {
            const char *error_msg = xml ? xml->ErrorMsg.c_str() : "<no-xml>";
            trace_detail("FLWOR return tuple[%zu] evaluation failed: %s", tuple_index, error_msg);
         }
         record_error("FLWOR return expression could not be evaluated.");
         if (xml and xml->ErrorMsg.empty()) xml->ErrorMsg.assign("FLWOR return expression could not be evaluated.");
         return XPathVal();
      }

      if (iteration_value.Type IS XPVT::NodeSet) {
         size_t length = nodeset_length(iteration_value);
         if (tracing_flwor) {
            trace_detail("FLWOR return tuple[%zu] produced node-set length=%zu", tuple_index, length);
            if (length > 0) {
               for (size_t value_index = 0; value_index < length; ++value_index) {
                  XMLTag *node = value_index < iteration_value.node_set.size() ? iteration_value.node_set[value_index] : nullptr;
                  int node_id = node ? node->ID : -1;
                  const XMLAttrib *attribute = value_index < iteration_value.node_set_attributes.size() ?
                     iteration_value.node_set_attributes[value_index] : nullptr;
                  const char *attribute_name = (attribute and !attribute->Name.empty()) ?
                     attribute->Name.c_str() : "<node>";
                  trace_verbose("FLWOR return tuple[%zu] value[%zu]: node-id=%d attribute=%s", tuple_index,
                     value_index, node_id, attribute_name);
               }
            }
            else {
               trace_verbose("FLWOR return tuple[%zu] produced empty node-set", tuple_index);
            }
         }
         if (length IS 0) continue;

         for (size_t value_index = 0; value_index < length; ++value_index) {
            XMLTag *node = value_index < iteration_value.node_set.size() ? iteration_value.node_set[value_index] : nullptr;
            combined_nodes.push_back(node);

            const XMLAttrib *attribute = nullptr;
            if (value_index < iteration_value.node_set_attributes.size()) {
               attribute = iteration_value.node_set_attributes[value_index];
            }
            combined_attributes.push_back(attribute);

            std::string node_string = nodeset_string_at(iteration_value, value_index);
            combined_strings.push_back(node_string);
            if (!combined_override.has_value()) combined_override = node_string;
         }

         if (iteration_value.node_set_string_override.has_value() and iteration_value.node_set_string_values.empty()) {
            if (!combined_override.has_value()) combined_override = iteration_value.node_set_string_override;
         }
         continue;
      }

      if (iteration_value.is_empty()) continue;

      if (tracing_flwor) trace_detail("FLWOR return tuple[%zu] produced non-node-set type %d", tuple_index,
         int(iteration_value.Type));
      record_error("FLWOR return expressions must yield node-sets.", true);
      return XPathVal();
   }

   XPathVal result;
   result.Type = XPVT::NodeSet;
   result.node_set = std::move(combined_nodes);
   result.node_set_attributes = std::move(combined_attributes);
   result.node_set_string_values = std::move(combined_strings);
   if (combined_override.has_value()) result.node_set_string_override = combined_override;
   else result.node_set_string_override.reset();
   result.preserve_node_order = (order_clause != nullptr);
   return result;
}

//********************************************************************************************************************

XPathVal XPathEvaluator::evaluate_expression(const XPathNode *ExprNode, uint32_t CurrentPrefix) {
   if (!ExprNode) {
      record_error("Unsupported XPath expression: empty node", true);
      return XPathVal();
   }

   if (ExprNode->type IS XPathNodeType::NUMBER) {
      char *end_ptr = nullptr;
      double value = std::strtod(ExprNode->value.c_str(), &end_ptr);
      if ((end_ptr) and (*end_ptr IS '\0')) return XPathVal(value);
      return XPathVal(std::numeric_limits<double>::quiet_NaN());
   }

   if ((ExprNode->type IS XPathNodeType::LITERAL) or (ExprNode->type IS XPathNodeType::STRING)) {
      return XPathVal(ExprNode->value);
   }

   if (ExprNode->type IS XPathNodeType::DIRECT_ELEMENT_CONSTRUCTOR) {
      return evaluate_direct_element_constructor(ExprNode, CurrentPrefix);
   }

   if (ExprNode->type IS XPathNodeType::COMPUTED_ELEMENT_CONSTRUCTOR) {
      return evaluate_computed_element_constructor(ExprNode, CurrentPrefix);
   }

   if (ExprNode->type IS XPathNodeType::COMPUTED_ATTRIBUTE_CONSTRUCTOR) {
      return evaluate_computed_attribute_constructor(ExprNode, CurrentPrefix);
   }

   if (ExprNode->type IS XPathNodeType::TEXT_CONSTRUCTOR) {
      return evaluate_text_constructor(ExprNode, CurrentPrefix);
   }

   if (ExprNode->type IS XPathNodeType::COMMENT_CONSTRUCTOR) {
      return evaluate_comment_constructor(ExprNode, CurrentPrefix);
   }

   if (ExprNode->type IS XPathNodeType::PI_CONSTRUCTOR) {
      return evaluate_pi_constructor(ExprNode, CurrentPrefix);
   }

   if (ExprNode->type IS XPathNodeType::DOCUMENT_CONSTRUCTOR) {
      return evaluate_document_constructor(ExprNode, CurrentPrefix);
   }

   if (ExprNode->type IS XPathNodeType::LOCATION_PATH) {
      return evaluate_path_expression_value(ExprNode, CurrentPrefix);
   }

   if (ExprNode->type IS XPathNodeType::UNION) {
      std::vector<const XPathNode *> branches;
      branches.reserve(ExprNode->child_count());

      for (size_t index = 0; index < ExprNode->child_count(); ++index) {
         auto *branch = ExprNode->get_child(index);
         if (branch) branches.push_back(branch);
      }

      return evaluate_union_value(branches, CurrentPrefix);
   }

   if (ExprNode->type IS XPathNodeType::CONDITIONAL) {
      if (ExprNode->child_count() < 3) {
         expression_unsupported = true;
         return XPathVal();
      }

      auto *condition_node = ExprNode->get_child(0);
      auto *then_node = ExprNode->get_child(1);
      auto *else_node = ExprNode->get_child(2);

      if ((!condition_node) or (!then_node) or (!else_node)) {
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

   // LET expressions share the same diagnostic surface as the parser.  Whenever a binding fails we populate
   // extXML::ErrorMsg so Fluid callers receive precise feedback rather than generic failure codes.
   if (ExprNode->type IS XPathNodeType::LET_EXPRESSION) {
      if (ExprNode->child_count() < 2) {
         record_error("LET expression requires at least one binding and a return clause.", true);
         return XPathVal();
      }

      const XPathNode *return_node = ExprNode->get_child(ExprNode->child_count() - 1);
      if (!return_node) {
         record_error("LET expression is missing its return clause.", true);
         return XPathVal();
      }

      std::vector<VariableBindingGuard> binding_guards;
      binding_guards.reserve(ExprNode->child_count() - 1);

      for (size_t index = 0; index + 1 < ExprNode->child_count(); ++index) {
         const XPathNode *binding_node = ExprNode->get_child(index);
         if ((!binding_node) or !(binding_node->type IS XPathNodeType::LET_BINDING)) {
            record_error("LET expression contains an invalid binding clause.", true);
            return XPathVal();
         }

         if ((binding_node->value.empty()) or (binding_node->child_count() IS 0)) {
            record_error("Let binding requires a variable name and expression.", true);
            return XPathVal();
         }

         const XPathNode *binding_expr = binding_node->get_child(0);
         if (!binding_expr) {
            record_error("Let binding requires an expression node.", true);
            return XPathVal();
         }

         XPathVal bound_value = evaluate_expression(binding_expr, CurrentPrefix);
         if (expression_unsupported) {
            record_error("Let binding expression could not be evaluated.");
            return XPathVal();
         }

         binding_guards.emplace_back(context, binding_node->value, std::move(bound_value));
      }

      auto result_value = evaluate_expression(return_node, CurrentPrefix);
      if (expression_unsupported) {
         record_error("Let return expression could not be evaluated.");
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
      if (!return_node) {
         expression_unsupported = true;
         return XPathVal();
      }

      struct ForBindingDefinition {
         std::string name;
         const XPathNode * sequence;
      };

      std::vector<ForBindingDefinition> bindings;
      bindings.reserve(ExprNode->child_count());

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

         legacy_layout = true;
         break;
      }

      if (legacy_layout) {
         if (ExprNode->child_count() < 2) {
            expression_unsupported = true;
            return XPathVal();
         }

         const XPathNode *sequence_node = ExprNode->get_child(0);
         if ((!sequence_node) or (!return_node) or ExprNode->value.empty()) {
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

      std::function<bool(size_t)> evaluate_bindings = [&](size_t binding_index) -> bool {
         if (binding_index >= bindings.size()) {
            auto iteration_value = evaluate_expression(return_node, CurrentPrefix);
            if (expression_unsupported) return false;

            if (iteration_value.Type != XPVT::NodeSet) {
               expression_unsupported = true;
               return false;
            }

            for (size_t node_index = 0; node_index < iteration_value.node_set.size(); ++node_index) {
               XMLTag *node = iteration_value.node_set[node_index];
               combined_nodes.push_back(node);

               const XMLAttrib *attribute = nullptr;
               if (node_index < iteration_value.node_set_attributes.size()) {
                  attribute = iteration_value.node_set_attributes[node_index];
               }
               combined_attributes.push_back(attribute);

               std::string node_string;
               bool use_override = iteration_value.node_set_string_override.has_value() and
                  (node_index IS 0) and iteration_value.node_set_string_values.empty();
               if (node_index < iteration_value.node_set_string_values.size()) {
                  node_string = iteration_value.node_set_string_values[node_index];
               }
               else if (use_override) node_string = *iteration_value.node_set_string_override;
               else if (node) node_string = XPathVal::node_string_value(node);

               combined_strings.push_back(node_string);

               if (!combined_override.has_value()) {
                  if (iteration_value.node_set_string_override.has_value()) {
                     combined_override = iteration_value.node_set_string_override;
                  }
                  else combined_override = node_string;
               }
            }

            return true;
         }

         const auto &binding = bindings[binding_index];
         if (!binding.sequence) {
            expression_unsupported = true;
            return false;
         }

         const std::string variable_name = binding.name;

         auto sequence_value = evaluate_expression(binding.sequence, CurrentPrefix);
         if (expression_unsupported) return false;

         if (sequence_value.Type != XPVT::NodeSet) {
            expression_unsupported = true;
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
         bound_value.node_set_attributes.push_back(item_attribute);

            std::string item_string;
            bool use_override = sequence_value.node_set_string_override.has_value() and
               (index IS 0) and sequence_value.node_set_string_values.empty();
            if (index < sequence_value.node_set_string_values.size()) {
               item_string = sequence_value.node_set_string_values[index];
            }
            else if (use_override) item_string = *sequence_value.node_set_string_override;
            else if (item_node) item_string = XPathVal::node_string_value(item_node);

            bound_value.node_set_string_values.push_back(item_string);
            bound_value.node_set_string_override = item_string;

            VariableBindingGuard iteration_guard(context, variable_name, std::move(bound_value));

            push_context(item_node, index + 1, sequence_size, item_attribute);
            bool iteration_ok = evaluate_bindings(binding_index + 1);
            pop_context();

            if (!iteration_ok) return false;

            if (expression_unsupported) return false;
         }

         return true;
      };

      bool evaluation_ok = evaluate_bindings(0);
      if (!evaluation_ok) return XPathVal();
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

      if ((!is_some) and (!is_every)) {
         expression_unsupported = true;
         return XPathVal();
      }

      const XPathNode *condition_node = ExprNode->get_child(ExprNode->child_count() - 1);
      if (!condition_node) {
         expression_unsupported = true;
         return XPathVal();
      }

      struct QuantifiedBindingDefinition {
         std::string name;
         const XPathNode * sequence;
      };

      std::vector<QuantifiedBindingDefinition> bindings;
      bindings.reserve(ExprNode->child_count());

      for (size_t index = 0; index + 1 < ExprNode->child_count(); ++index) {
         const XPathNode *binding_node = ExprNode->get_child(index);
         if ((!binding_node) or (binding_node->type != XPathNodeType::QUANTIFIED_BINDING)) {
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

      std::function<bool(size_t)> evaluate_binding = [&](size_t binding_index) -> bool {
         if (binding_index >= bindings.size()) {
            auto condition_value = evaluate_expression(condition_node, CurrentPrefix);
            if (expression_unsupported) return false;
            return condition_value.to_boolean();
         }

         const auto &binding = bindings[binding_index];
         if (!binding.sequence) {
            expression_unsupported = true;
            return false;
         }

         const std::string variable_name = binding.name;

         auto sequence_value = evaluate_expression(binding.sequence, CurrentPrefix);
         if (expression_unsupported) return false;

         if (sequence_value.Type != XPVT::NodeSet) {
            expression_unsupported = true;
            return false;
         }

         size_t sequence_size = sequence_value.node_set.size();

         if (sequence_size IS 0) return is_every;

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
         bound_value.node_set_attributes.push_back(item_attribute);

            std::string item_string;
            bool use_override = sequence_value.node_set_string_override.has_value() and
               (index IS 0) and sequence_value.node_set_string_values.empty();
            if (index < sequence_value.node_set_string_values.size()) {
               item_string = sequence_value.node_set_string_values[index];
            }
            else if (use_override) item_string = *sequence_value.node_set_string_override;
            else if (item_node) item_string = XPathVal::node_string_value(item_node);

            bound_value.node_set_string_values.push_back(item_string);
            bound_value.node_set_string_override = item_string;

            VariableBindingGuard iteration_guard(context, variable_name, std::move(bound_value));

            push_context(item_node, index + 1, sequence_size, item_attribute);
            bool branch_result = evaluate_binding(binding_index + 1);
            pop_context();

            if (expression_unsupported) return false;

            if (branch_result) {
               if (is_some) {
                  return true;
               }
            }
            else {
               if (is_every) return false;
            }
         }

         return is_every;
      };

      bool quant_result = evaluate_binding(0);
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
         if (!predicate_node) continue;

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
      if (!working_indices.empty()) {
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

      if (!steps.empty()) {
         auto *last_step = steps.back();
         const XPathNode *axis_node = nullptr;
         const XPathNode *node_test = nullptr;

         for (size_t index = 0; index < last_step->child_count(); ++index) {
            auto *child = last_step->get_child(index);
            if (!child) continue;

            if (child->type IS XPathNodeType::AXIS_SPECIFIER) axis_node = child;
            else if ((!node_test) and ((child->type IS XPathNodeType::NAME_TEST) or
                                       (child->type IS XPathNodeType::WILDCARD) or
                                       (child->type IS XPathNodeType::NODE_TYPE_TEST))) node_test = child;
         }

         AxisType axis = axis_node ? AxisEvaluator::parse_axis_name(axis_node->value) : AxisType::CHILD;
         if (axis IS AxisType::ATTRIBUTE) {
            attribute_step = last_step;
            attribute_test = node_test;
         }
      }

      return evaluate_path_from_nodes(base_value.node_set,
                                      base_value.node_set_attributes,
                                      steps,
                                      attribute_step,
                                      attribute_test,
                                      CurrentPrefix);
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
      if (ExprNode->value IS "not") return XPathVal(!operand.to_boolean());

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

      // TODO: Hash the operation with pf::strhash() and use switch-case for better performance.

      if (operation IS "and") {
         auto left_value = evaluate_expression(left_node, CurrentPrefix);
         if (expression_unsupported) return XPathVal();

         bool left_boolean = left_value.to_boolean();
         if (!left_boolean) return XPathVal(false);

         auto right_value = evaluate_expression(right_node, CurrentPrefix);
         if (expression_unsupported) return XPathVal();

         bool right_boolean = right_value.to_boolean();
         return XPathVal(right_boolean);
      }

      if (operation IS "or") {
         auto left_value = evaluate_expression(left_node, CurrentPrefix);
         if (expression_unsupported) return XPathVal();

         bool left_boolean = left_value.to_boolean();
         if (left_boolean) return XPathVal(true);

         auto right_value = evaluate_expression(right_node, CurrentPrefix);
         if (expression_unsupported) return XPathVal();

         bool right_boolean = right_value.to_boolean();
         return XPathVal(right_boolean);
      }

      if (operation IS "|") {
         std::vector<const XPathNode *> branches;
         branches.reserve(2);
         if (left_node) branches.push_back(left_node);
         if (right_node) branches.push_back(right_node);
         return evaluate_union_value(branches, CurrentPrefix);
      }

      if (operation IS "intersect") return evaluate_intersect_value(left_node, right_node, CurrentPrefix);
      if (operation IS "except") return evaluate_except_value(left_node, right_node, CurrentPrefix);

      if (operation IS ",")
      {
         auto left_value = evaluate_expression(left_node, CurrentPrefix);
         if (expression_unsupported) return XPathVal();

         auto right_value = evaluate_expression(right_node, CurrentPrefix);
         if (expression_unsupported) return XPathVal();

         struct SequenceEntry
         {
            XMLTag *node = nullptr;
            const XMLAttrib *attribute = nullptr;
            std::string string_value;
         };

         std::vector<SequenceEntry> entries;
         entries.reserve(left_value.node_set.size() + right_value.node_set.size());

         auto append_value = [&](const XPathVal &value)
         {
            if (value.Type IS XPVT::NodeSet)
            {
               bool use_override = value.node_set_string_override.has_value() and value.node_set_string_values.empty();
               for (size_t index = 0; index < value.node_set.size(); ++index)
               {
                  XMLTag *node = value.node_set[index];
                  if (!node) continue;

                  const XMLAttrib *attribute = nullptr;
                  if (index < value.node_set_attributes.size()) attribute = value.node_set_attributes[index];

                  std::string item_string;
                  if (index < value.node_set_string_values.size()) item_string = value.node_set_string_values[index];
                  else if (use_override) item_string = *value.node_set_string_override;
                  else if (attribute) item_string = attribute->Value;
                  else item_string = XPathVal::node_string_value(node);

                  entries.push_back({ node, attribute, std::move(item_string) });
               }
               return;
            }

            std::string text = value.to_string();
            pf::vector<XMLAttrib> text_attribs;
            text_attribs.emplace_back("", text);

            XMLTag text_node(next_constructed_node_id--, 0, text_attribs);
            text_node.ParentID = 0;

            auto stored = std::make_unique<XMLTag>(std::move(text_node));
            XMLTag *root = stored.get();
            constructed_nodes.push_back(std::move(stored));

            entries.push_back({ root, nullptr, std::move(text) });
         };

         append_value(left_value);
         append_value(right_value);

         if (entries.empty())
         {
            NODES empty_nodes;
            return XPathVal(empty_nodes);
         }

         NODES combined_nodes;
         combined_nodes.reserve(entries.size());
         std::vector<const XMLAttrib *> combined_attributes;
         combined_attributes.reserve(entries.size());
         std::vector<std::string> combined_strings;
         combined_strings.reserve(entries.size());

         for (auto &entry : entries)
         {
            combined_nodes.push_back(entry.node);
            combined_attributes.push_back(entry.attribute);
            combined_strings.push_back(std::move(entry.string_value));
         }

         return XPathVal(combined_nodes, std::nullopt, std::move(combined_strings), std::move(combined_attributes));
      }

      auto left_value = evaluate_expression(left_node, CurrentPrefix);
      if (expression_unsupported) return XPathVal();
      auto right_value = evaluate_expression(right_node, CurrentPrefix);
      if (expression_unsupported) return XPathVal();

      if (operation IS "=") {
         bool equals = compare_xpath_values(left_value, right_value);
         return XPathVal(equals);
      }

      if (operation IS "!=") {
         bool equals = compare_xpath_values(left_value, right_value);
         return XPathVal(!equals);
      }

      if (operation IS "eq") {
         auto left_scalar = promote_value_comparison_operand(left_value);
         auto right_scalar = promote_value_comparison_operand(right_value);
         if (!left_scalar.has_value() or !right_scalar.has_value()) return XPathVal(false);
         bool equals = compare_xpath_values(*left_scalar, *right_scalar);
         return XPathVal(equals);
      }

      if (operation IS "ne") {
         auto left_scalar = promote_value_comparison_operand(left_value);
         auto right_scalar = promote_value_comparison_operand(right_value);
         if (!left_scalar.has_value() or !right_scalar.has_value()) return XPathVal(false);
         bool equals = compare_xpath_values(*left_scalar, *right_scalar);
         return XPathVal(!equals);
      }

      if (operation IS "<") {
         bool result = compare_xpath_relational(left_value, right_value, RelationalOperator::LESS);
         return XPathVal(result);
      }

      if (operation IS "<=") {
         bool result = compare_xpath_relational(left_value, right_value, RelationalOperator::LESS_OR_EQUAL);
         return XPathVal(result);
      }

      if (operation IS ">") {
         bool result = compare_xpath_relational(left_value, right_value, RelationalOperator::GREATER);
         return XPathVal(result);
      }

      if (operation IS ">=") {
         bool result = compare_xpath_relational(left_value, right_value, RelationalOperator::GREATER_OR_EQUAL);
         return XPathVal(result);
      }

      if (operation IS "lt") {
         auto left_scalar = promote_value_comparison_operand(left_value);
         auto right_scalar = promote_value_comparison_operand(right_value);
         if (!left_scalar.has_value() or !right_scalar.has_value()) return XPathVal(false);
         bool result = compare_xpath_relational(*left_scalar, *right_scalar, RelationalOperator::LESS);
         return XPathVal(result);
      }

      if (operation IS "le") {
         auto left_scalar = promote_value_comparison_operand(left_value);
         auto right_scalar = promote_value_comparison_operand(right_value);
         if (!left_scalar.has_value() or !right_scalar.has_value()) return XPathVal(false);
         bool result = compare_xpath_relational(*left_scalar, *right_scalar, RelationalOperator::LESS_OR_EQUAL);
         return XPathVal(result);
      }

      if (operation IS "gt") {
         auto left_scalar = promote_value_comparison_operand(left_value);
         auto right_scalar = promote_value_comparison_operand(right_value);
         if (!left_scalar.has_value() or !right_scalar.has_value()) return XPathVal(false);
         bool result = compare_xpath_relational(*left_scalar, *right_scalar, RelationalOperator::GREATER);
         return XPathVal(result);
      }

      if (operation IS "ge") {
         auto left_scalar = promote_value_comparison_operand(left_value);
         auto right_scalar = promote_value_comparison_operand(right_value);
         if (!left_scalar.has_value() or !right_scalar.has_value()) return XPathVal(false);
         bool result = compare_xpath_relational(*left_scalar, *right_scalar, RelationalOperator::GREATER_OR_EQUAL);
         return XPathVal(result);
      }

      if (operation IS "+") {
         double result = left_value.to_number() + right_value.to_number();
         return XPathVal(result);
      }

      if (operation IS "-") {
         double result = left_value.to_number() - right_value.to_number();
         return XPathVal(result);
      }

      if (operation IS "*") {
         double result = left_value.to_number() * right_value.to_number();
         return XPathVal(result);
      }

      if (operation IS "div") {
         double result = left_value.to_number() / right_value.to_number();
         return XPathVal(result);
      }

      if (operation IS "mod") {
         double left_number = left_value.to_number();
         double right_number = right_value.to_number();
         double result = std::fmod(left_number, right_number);
         return XPathVal(result);
      }

      expression_unsupported = true;
      return XPathVal();
   }

   if (ExprNode->type IS XPathNodeType::VARIABLE_REFERENCE) {
      auto local_variable = context.variables.find(ExprNode->value);
      if (local_variable != context.variables.end()) {
         return local_variable->second;
      }

      if (is_trace_enabled(TraceCategory::XPath)) {
         pf::Log log("XPath");
         log.msg(trace_detail_level, "Variable lookup failed for '%s'", ExprNode->value.c_str());
         if (!context.variables.empty()) {
            std::string binding_list;
            binding_list.reserve(context.variables.size() * 16);
            bool first_binding = true;
            for (const auto &entry : context.variables) {
               if (!first_binding) binding_list.append(", ");
               binding_list.append(entry.first);
               first_binding = false;
            }
            log.msg(trace_detail_level, "Context bindings available: [%s]", binding_list.c_str());
         }
      }

      // Look up variable in the XML object's variable storage
      auto it = xml->Variables.find(ExprNode->value);
      if (it != xml->Variables.end()) {
         return XPathVal(it->second);
      }
      else {
         // Variable not found - XPath 1.0 spec requires this to be an error
         expression_unsupported = true;
         return XPathVal();
      }
   }

   expression_unsupported = true;
   return XPathVal();
}

//********************************************************************************************************************

ERR XPathEvaluator::process_expression_node_set(const XPathVal &Value)
{
   bool tracing_xpath = is_trace_enabled(TraceCategory::XPath);
   auto trace_nodes_detail = [&](const char *Format, auto ...Args)
   {
      if (!tracing_xpath) return;
      pf::Log log("XPath");
      log.msg(trace_detail_level, Format, Args...);
   };

   auto trace_nodes_verbose = [&](const char *Format, auto ...Args)
   {
      if (!tracing_xpath) return;
      pf::Log log("XPath");
      log.msg(trace_verbose_level, Format, Args...);
   };

   struct NodeEntry {
      XMLTag * node = nullptr;
      const XMLAttrib * attribute = nullptr;
      size_t original_index = 0;
   };

   std::vector<NodeEntry> entries;
   entries.reserve(Value.node_set.size());

   for (size_t index = 0; index < Value.node_set.size(); ++index) {
      XMLTag *candidate = Value.node_set[index];
      if (!candidate) continue;

      const XMLAttrib *attribute = nullptr;
      if (index < Value.node_set_attributes.size()) attribute = Value.node_set_attributes[index];

      entries.push_back({ candidate, attribute, index });
   }

   if (tracing_xpath) {
      std::string original_summary;
      original_summary.reserve(entries.size() * 4);
      for (size_t entry_index = 0; entry_index < entries.size(); ++entry_index) {
         if (entry_index > 0) original_summary.append(", ");
         original_summary.append(std::to_string(entries[entry_index].original_index));
      }

      trace_nodes_detail("FLWOR emit initial tuple materialisation: nodes=%zu, attributes=%zu, order=[%s]",
         entries.size(), Value.node_set_attributes.size(), original_summary.c_str());

      for (size_t entry_index = 0; entry_index < entries.size(); ++entry_index) {
         const auto &entry = entries[entry_index];
         int node_id = entry.node ? entry.node->ID : -1;
         const char *attribute_name = (entry.attribute and !entry.attribute->Name.empty()) ?
            entry.attribute->Name.c_str() : "<node>";
         trace_nodes_verbose("FLWOR emit initial entry[%zu]: node-id=%d, attribute=%s, original=%zu",
            entry_index, node_id, attribute_name, entry.original_index);
      }
   }

   if (entries.empty()) {
      xml->Attrib.clear();
      return ERR::Search;
   }

   if (Value.preserve_node_order) {
      std::vector<NodeEntry> unique_entries;
      unique_entries.reserve(entries.size());

      for (const auto &entry : entries) {
         bool duplicate = false;
         for (const auto &existing : unique_entries) {
            if ((existing.node IS entry.node) and (existing.attribute IS entry.attribute)) {
               duplicate = true;
               break;
            }
         }
         if (!duplicate) unique_entries.push_back(entry);
      }

      if (tracing_xpath) {
         std::string preserved_summary;
         preserved_summary.reserve(unique_entries.size() * 4);
         for (size_t entry_index = 0; entry_index < unique_entries.size(); ++entry_index) {
            if (entry_index > 0) preserved_summary.append(", ");
            preserved_summary.append(std::to_string(unique_entries[entry_index].original_index));
         }

         trace_nodes_detail("FLWOR emit preserved-order pass: unique=%zu, order=[%s]", unique_entries.size(),
            preserved_summary.c_str());
      }

      entries.swap(unique_entries);
   }
   else {
      std::stable_sort(entries.begin(), entries.end(), [this](const NodeEntry &Left, const NodeEntry &Right) {
         if (Left.node IS Right.node) return Left.original_index < Right.original_index;
         if (!Left.node) return false;
         if (!Right.node) return true;
         return axis_evaluator.is_before_in_document_order(Left.node, Right.node);
      });

      auto unique_end = std::unique(entries.begin(), entries.end(), [](const NodeEntry &Left, const NodeEntry &Right) {
         return (Left.node IS Right.node) and (Left.attribute IS Right.attribute);
      });
      entries.erase(unique_end, entries.end());

      if (tracing_xpath) {
         std::string sorted_summary;
         sorted_summary.reserve(entries.size() * 4);
         for (size_t entry_index = 0; entry_index < entries.size(); ++entry_index) {
            if (entry_index > 0) sorted_summary.append(", ");
            sorted_summary.append(std::to_string(entries[entry_index].original_index));
         }

         trace_nodes_detail("FLWOR emit document-order pass: unique=%zu, order=[%s]", entries.size(),
            sorted_summary.c_str());
      }
   }

   bool matched = false;

   for (size_t index = 0; index < entries.size(); ++index) {
      auto &entry = entries[index];
      XMLTag *candidate = entry.node;
      push_context(candidate, index + 1, entries.size(), entry.attribute);

      if (!candidate) {
         pop_context();
         continue;
      }

      bool should_terminate = false;
      if (tracing_xpath) {
         int node_id = candidate ? candidate->ID : -1;
         const char *attribute_name = (entry.attribute and !entry.attribute->Name.empty()) ?
            entry.attribute->Name.c_str() : "<node>";
         trace_nodes_detail("FLWOR emit invoking callback index=%zu node-id=%d attribute=%s original=%zu",
            index, node_id, attribute_name, entry.original_index);
      }
      auto callback_error = invoke_callback(candidate, entry.attribute, matched, should_terminate);
      pop_context();

      if (callback_error IS ERR::Terminate) return ERR::Terminate;
      if (callback_error != ERR::Okay) return callback_error;
      if (should_terminate) return ERR::Okay;
   }

   xml->Attrib.clear();
   if (matched) return ERR::Okay;
   return ERR::Search;
}

//********************************************************************************************************************

ERR XPathEvaluator::evaluate_top_level_expression(const XPathNode *Node, uint32_t CurrentPrefix)
{
   if (!Node) return ERR::Failed;

   const XPathNode *expression = Node;

   if (Node->type IS XPathNodeType::EXPRESSION) {
      if (Node->child_count() IS 0) {
         xml->Attrib.clear();
         return ERR::Search;
      }

      expression = Node->get_child(0);
   }

   expression_unsupported = false;
   auto value = evaluate_expression(expression, CurrentPrefix);
   if (expression_unsupported) {
      if (xml and xml->ErrorMsg.empty()) xml->ErrorMsg = "Unsupported XPath expression.";
      return ERR::Failed;
   }

   switch (value.Type) {
      case XPVT::NodeSet:
         return process_expression_node_set(value);

      case XPVT::Boolean:
      case XPVT::Number:
      case XPVT::String:
      case XPVT::Date:
      case XPVT::Time:
      case XPVT::DateTime:
         xml->Attrib = value.to_string();
         return ERR::Okay;
   }

   return ERR::Failed;
}

//********************************************************************************************************************

XPathVal XPathEvaluator::evaluate_function_call(const XPathNode *FuncNode, uint32_t CurrentPrefix) {
   if (!FuncNode or FuncNode->type != XPathNodeType::FUNCTION_CALL) {
      return XPathVal();
   }

   std::string function_name = FuncNode->value;

   std::vector<XPathVal> args;
   args.reserve(FuncNode->child_count());

   for (size_t index = 0; index < FuncNode->child_count(); ++index) {
      auto *argument_node = FuncNode->get_child(index);
      args.push_back(evaluate_expression(argument_node, CurrentPrefix));
      if (expression_unsupported) return XPathVal();
   }

   if (function_name IS "text") {
      NODES text_nodes;
      std::optional<std::string> first_value;

      if (context.context_node) {
         for (auto &child : context.context_node->Children) {
            if (!child.isContent()) continue;
            text_nodes.push_back(&child);

            if ((!first_value.has_value()) and (!child.Attribs.empty())) {
               first_value = child.Attribs[0].Value;
            }
         }
      }

      return XPathVal(text_nodes, first_value);
   }

   return XPathFunctionLibrary::instance().call_function(function_name, args, context);
}
