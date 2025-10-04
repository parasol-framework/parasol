//********************************************************************************************************************
// XPath Expression and Value Evaluation
//********************************************************************************************************************
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
#include "../schema/schema_types.h"
#include "../xml.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

//********************************************************************************************************************

XPathValue XPathEvaluator::evaluate_path_expression_value(const XPathNode *PathNode, uint32_t CurrentPrefix) {
   if (!PathNode) {
      expression_unsupported = true;
      return XPathValue();
   }

   const XPathNode *location = PathNode;
   if (PathNode->type IS XPathNodeType::PATH) {
      if (PathNode->child_count() IS 0) return XPathValue();
      location = PathNode->get_child(0);
   }

   if ((!location) or (location->type != XPathNodeType::LOCATION_PATH)) {
      expression_unsupported = true;
      return XPathValue();
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

   std::vector<XMLTag *> initial_context;

   if (has_root) initial_context.push_back(nullptr);
   else {
      if (context.context_node) initial_context.push_back(context.context_node);
      else if ((xml->CursorTags) and (xml->Cursor != xml->CursorTags->end())) initial_context.push_back(&(*xml->Cursor));
      else initial_context.push_back(nullptr);
   }

   if (steps.empty()) return XPathValue(initial_context);

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
   std::vector<XMLTag *> node_results;

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
      return XPathValue();
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
            return XPathValue(node_results, context.attribute_node->Value, {}, std::move(attribute_refs));
         }
      }
   }

   if (attribute_step) {
      std::vector<std::string> attribute_values;
      std::vector<XMLTag *> attribute_nodes;
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
                     return XPathValue();
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

      if (attribute_nodes.empty()) return XPathValue(attribute_nodes);

      std::optional<std::string> first_value;
      if (!attribute_values.empty()) first_value = attribute_values[0];
      return XPathValue(attribute_nodes, first_value, std::move(attribute_values), std::move(attribute_refs));
   }

   return XPathValue(node_results);
}

//********************************************************************************************************************

XPathValue XPathEvaluator::evaluate_path_from_nodes(const std::vector<XMLTag *> &InitialContext,
                                                          const std::vector<const XMLAttrib *> &InitialAttributes,
                                                          const std::vector<const XPathNode *> &Steps,
                                                          const XPathNode *AttributeStep,
                                                          const XPathNode *AttributeTest,
                                                          uint32_t CurrentPrefix)
{
   std::vector<const XPathNode *> work_steps = Steps;

   if (AttributeStep and !work_steps.empty()) work_steps.pop_back();

   std::vector<XMLTag *> node_results;

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
         return XPathValue();
      }
   }

   axis_evaluator.normalise_node_set(node_results);

   if (AttributeStep) {
      std::vector<std::string> attribute_values;
      std::vector<XMLTag *> attribute_nodes;
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

      if (attribute_nodes.empty()) return XPathValue(attribute_nodes);

      std::optional<std::string> first_value;
      if (!attribute_values.empty()) first_value = attribute_values[0];
      return XPathValue(attribute_nodes, first_value, std::move(attribute_values), std::move(attribute_refs));
   }

   return XPathValue(node_results);
}

//********************************************************************************************************************

XPathValue XPathEvaluator::evaluate_union_value(const std::vector<const XPathNode *> &Branches, uint32_t CurrentPrefix)
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
      bool operator()(const NodeIdentity &Left, const NodeIdentity &Right) const
      {
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
         return XPathValue();
      }

      if (branch_value.type != XPathValueType::NodeSet) {
         context = saved_context;
         context_stack = saved_context_stack;
         cursor_stack = saved_cursor_stack;
         xml->CursorTags = saved_cursor_tags;
         xml->Cursor = saved_cursor;
         xml->Attrib = saved_attrib;
         expression_unsupported = true;
         return XPathValue();
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
         else entry.string_value = XPathValue::node_string_value(node);

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

   std::vector<XMLTag *> combined_nodes;
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

   if (combined_nodes.empty()) return XPathValue(std::vector<XMLTag *>());

   return XPathValue(combined_nodes, combined_override, std::move(combined_strings), std::move(combined_attributes));
}

//********************************************************************************************************************

XPathValue XPathEvaluator::evaluate_intersect_value(const XPathNode *Left, const XPathNode *Right, uint32_t CurrentPrefix)
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

   auto saved_context = context;
   auto saved_context_stack = context_stack;
   auto saved_cursor_stack = cursor_stack;
   auto saved_cursor_tags = xml->CursorTags;
   auto saved_cursor = xml->Cursor;
   auto saved_attrib = xml->Attrib;
   bool saved_expression_unsupported = expression_unsupported;

   auto evaluate_operand = [&](const XPathNode *Operand) -> std::optional<XPathValue> {
      context = saved_context;
      context_stack = saved_context_stack;
      cursor_stack = saved_cursor_stack;
      xml->CursorTags = saved_cursor_tags;
      xml->Cursor = saved_cursor;
      xml->Attrib = saved_attrib;
      expression_unsupported = saved_expression_unsupported;

      auto value = evaluate_expression(Operand, CurrentPrefix);
      if (expression_unsupported) return std::nullopt;

      if (value.type != XPathValueType::NodeSet) {
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
      return XPathValue();
   }

   auto right_value_opt = evaluate_operand(Right);
   if (!right_value_opt.has_value()) {
      context = saved_context;
      context_stack = saved_context_stack;
      cursor_stack = saved_cursor_stack;
      xml->CursorTags = saved_cursor_tags;
      xml->Cursor = saved_cursor;
      xml->Attrib = saved_attrib;
      return XPathValue();
   }

   const XPathValue &left_value = *left_value_opt;
   const XPathValue &right_value = *right_value_opt;

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
      else entry.string_value = XPathValue::node_string_value(node);

      if (!combined_override.has_value()) combined_override = entry.string_value;

      entries.push_back(std::move(entry));
   }

   std::stable_sort(entries.begin(), entries.end(), [this](const SetEntry &LeftEntry, const SetEntry &RightEntry) {
      if (LeftEntry.node IS RightEntry.node) return false;
      return axis_evaluator.is_before_in_document_order(LeftEntry.node, RightEntry.node);
   });

   std::vector<XMLTag *> combined_nodes;
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

   if (combined_nodes.empty()) return XPathValue(std::vector<XMLTag *>());

   return XPathValue(combined_nodes, combined_override, std::move(combined_strings), std::move(combined_attributes));
}

//********************************************************************************************************************

XPathValue XPathEvaluator::evaluate_except_value(const XPathNode *Left, const XPathNode *Right, uint32_t CurrentPrefix)
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

   auto saved_context = context;
   auto saved_context_stack = context_stack;
   auto saved_cursor_stack = cursor_stack;
   auto saved_cursor_tags = xml->CursorTags;
   auto saved_cursor = xml->Cursor;
   auto saved_attrib = xml->Attrib;
   bool saved_expression_unsupported = expression_unsupported;

   auto evaluate_operand = [&](const XPathNode *Operand) -> std::optional<XPathValue> {
      context = saved_context;
      context_stack = saved_context_stack;
      cursor_stack = saved_cursor_stack;
      xml->CursorTags = saved_cursor_tags;
      xml->Cursor = saved_cursor;
      xml->Attrib = saved_attrib;
      expression_unsupported = saved_expression_unsupported;

      auto value = evaluate_expression(Operand, CurrentPrefix);
      if (expression_unsupported) return std::nullopt;

      if (value.type != XPathValueType::NodeSet) {
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
      return XPathValue();
   }

   auto right_value_opt = evaluate_operand(Right);
   if (!right_value_opt.has_value()) {
      context = saved_context;
      context_stack = saved_context_stack;
      cursor_stack = saved_cursor_stack;
      xml->CursorTags = saved_cursor_tags;
      xml->Cursor = saved_cursor;
      xml->Attrib = saved_attrib;
      return XPathValue();
   }

   const XPathValue &left_value = *left_value_opt;
   const XPathValue &right_value = *right_value_opt;

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
      else entry.string_value = XPathValue::node_string_value(node);

      if (!combined_override.has_value()) combined_override = entry.string_value;

      entries.push_back(std::move(entry));
   }

   std::stable_sort(entries.begin(), entries.end(), [this](const SetEntry &LeftEntry, const SetEntry &RightEntry) {
      if (LeftEntry.node IS RightEntry.node) return false;
      return axis_evaluator.is_before_in_document_order(LeftEntry.node, RightEntry.node);
   });

   std::vector<XMLTag *> combined_nodes;
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

   if (combined_nodes.empty()) return XPathValue(std::vector<XMLTag *>());

   return XPathValue(combined_nodes, combined_override, std::move(combined_strings), std::move(combined_attributes));
}

//********************************************************************************************************************

XPathValue XPathEvaluator::evaluate_expression(const XPathNode *ExprNode, uint32_t CurrentPrefix) {
   if (!ExprNode) {
      record_error("Unsupported XPath expression: empty node", true);
      return XPathValue();
   }

   if (ExprNode->type IS XPathNodeType::NUMBER) {
      char *end_ptr = nullptr;
      double value = std::strtod(ExprNode->value.c_str(), &end_ptr);
      if ((end_ptr) and (*end_ptr IS '\0')) return XPathValue(value);
      return XPathValue(std::numeric_limits<double>::quiet_NaN());
   }

   if ((ExprNode->type IS XPathNodeType::LITERAL) or (ExprNode->type IS XPathNodeType::STRING)) {
      return XPathValue(ExprNode->value);
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
         return XPathValue();
      }

      auto *condition_node = ExprNode->get_child(0);
      auto *then_node = ExprNode->get_child(1);
      auto *else_node = ExprNode->get_child(2);

      if ((!condition_node) or (!then_node) or (!else_node)) {
         expression_unsupported = true;
         return XPathValue();
      }

      auto condition_value = evaluate_expression(condition_node, CurrentPrefix);
      if (expression_unsupported) return XPathValue();

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
         return XPathValue();
      }

      const XPathNode *return_node = ExprNode->get_child(ExprNode->child_count() - 1);
      if (!return_node) {
         record_error("LET expression is missing its return clause.", true);
         return XPathValue();
      }

      std::vector<VariableBindingGuard> binding_guards;
      binding_guards.reserve(ExprNode->child_count() - 1);

      for (size_t index = 0; index + 1 < ExprNode->child_count(); ++index) {
         const XPathNode *binding_node = ExprNode->get_child(index);
         if ((!binding_node) or !(binding_node->type IS XPathNodeType::LET_BINDING)) {
            record_error("LET expression contains an invalid binding clause.", true);
            return XPathValue();
         }

         if ((binding_node->value.empty()) or (binding_node->child_count() IS 0)) {
            record_error("Let binding requires a variable name and expression.", true);
            return XPathValue();
         }

         const XPathNode *binding_expr = binding_node->get_child(0);
         if (!binding_expr) {
            record_error("Let binding requires an expression node.", true);
            return XPathValue();
         }

         XPathValue bound_value = evaluate_expression(binding_expr, CurrentPrefix);
         if (expression_unsupported) {
            record_error("Let binding expression could not be evaluated.");
            return XPathValue();
         }

         binding_guards.emplace_back(context, binding_node->value, std::move(bound_value));
      }

      auto result_value = evaluate_expression(return_node, CurrentPrefix);
      if (expression_unsupported) {
         record_error("Let return expression could not be evaluated.");
         return XPathValue();
      }
      return result_value;
   }

   // FLWOR evaluation mirrors that approach, capturing structural and runtime issues so test_xpath_flwor.fluid can assert
   // on human-readable error text while we continue to guard performance-sensitive paths.
   if (ExprNode->type IS XPathNodeType::FLWOR_EXPRESSION) {
      if (ExprNode->child_count() < 2) {
         record_error("FLWOR expression requires at least one clause and a return expression.", true);
         return XPathValue();
      }

      const XPathNode *return_node = ExprNode->get_child(ExprNode->child_count() - 1);
      if (!return_node) {
         record_error("FLWOR expression is missing its return clause.", true);
         return XPathValue();
      }

      std::vector<const XPathNode *> clauses;
      clauses.reserve(ExprNode->child_count() - 1);

      for (size_t index = 0; index + 1 < ExprNode->child_count(); ++index) {
         const XPathNode *clause_node = ExprNode->get_child(index);
         if ((!clause_node) or
             !((clause_node->type IS XPathNodeType::FOR_BINDING) or
               (clause_node->type IS XPathNodeType::LET_BINDING))) {
            record_error("FLWOR expression contains an invalid clause.", true);
            return XPathValue();
         }
         clauses.push_back(clause_node);
      }

      if (clauses.empty()) {
         record_error("FLWOR expression is missing binding clauses.", true);
         return XPathValue();
      }

      std::vector<XMLTag *> combined_nodes;
      std::vector<std::string> combined_strings;
      std::vector<const XMLAttrib *> combined_attributes;
      std::optional<std::string> combined_override;

      std::function<bool(size_t)> append_return_value;

      append_return_value = [&](size_t clause_index) -> bool {
         if (clause_index >= clauses.size()) {
            auto iteration_value = evaluate_expression(return_node, CurrentPrefix);
            if (expression_unsupported) {
               record_error("FLWOR return expression could not be evaluated.");
               return false;
            }

            if (iteration_value.type != XPathValueType::NodeSet) {
               record_error("FLWOR return expressions must yield node-sets.", true);
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
               else if (node) node_string = XPathValue::node_string_value(node);

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

         const XPathNode *clause_node = clauses[clause_index];
         if (!clause_node) {
            record_error("FLWOR expression contains an invalid clause.", true);
            return false;
         }

         if (clause_node->type IS XPathNodeType::LET_BINDING) {
            if ((clause_node->value.empty()) or (clause_node->child_count() IS 0)) {
               record_error("Let binding requires a variable name and expression.", true);
               return false;
            }

            const XPathNode *binding_expr = clause_node->get_child(0);
            if (!binding_expr) {
               record_error("Let binding requires an expression node.", true);
               return false;
            }

            XPathValue bound_value = evaluate_expression(binding_expr, CurrentPrefix);
            if (expression_unsupported) {
               record_error("Let binding expression could not be evaluated.");
               return false;
            }

            VariableBindingGuard guard(context, clause_node->value, std::move(bound_value));
            return append_return_value(clause_index + 1);
         }

         if (clause_node->type IS XPathNodeType::FOR_BINDING) {
            if ((clause_node->value.empty()) or (clause_node->child_count() IS 0)) {
               record_error("For binding requires a variable name and sequence.", true);
               return false;
            }

            const XPathNode *sequence_expr = clause_node->get_child(0);
            if (!sequence_expr) {
               record_error("For binding requires a sequence expression.", true);
               return false;
            }

            auto sequence_value = evaluate_expression(sequence_expr, CurrentPrefix);
            if (expression_unsupported) {
               record_error("For binding sequence could not be evaluated.");
               return false;
            }

            if (sequence_value.type != XPathValueType::NodeSet) {
               record_error("For binding sequences must evaluate to node-sets.", true);
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

               XPathValue bound_value;
               bound_value.type = XPathValueType::NodeSet;
               bound_value.node_set.push_back(item_node);
               bound_value.node_set_attributes.push_back(item_attribute);

               std::string item_string;
               bool use_override = sequence_value.node_set_string_override.has_value() and
                  (index IS 0) and sequence_value.node_set_string_values.empty();
               if (index < sequence_value.node_set_string_values.size()) {
                  item_string = sequence_value.node_set_string_values[index];
               }
               else if (use_override) item_string = *sequence_value.node_set_string_override;
               else if (item_node) item_string = XPathValue::node_string_value(item_node);

               bound_value.node_set_string_values.push_back(item_string);
               bound_value.node_set_string_override = item_string;

               VariableBindingGuard iteration_guard(context, clause_node->value, std::move(bound_value));

               push_context(item_node, index + 1, sequence_size, item_attribute);
               bool evaluation_ok = append_return_value(clause_index + 1);
               pop_context();

               if (!evaluation_ok) return false;
               if (expression_unsupported) return false;
            }

            return true;
         }

         record_error("FLWOR expression contains an unsupported clause type.", true);
         return false;
      };

      bool evaluation_ok = append_return_value(0);
      if (!evaluation_ok) return XPathValue();
      if (expression_unsupported) return XPathValue();

      XPathValue result;
      result.type = XPathValueType::NodeSet;
      result.node_set = std::move(combined_nodes);
      result.node_set_string_values = std::move(combined_strings);
      result.node_set_attributes = std::move(combined_attributes);
      if (combined_override.has_value()) result.node_set_string_override = combined_override;
      else result.node_set_string_override.reset();
      return result;
   }

   if (ExprNode->type IS XPathNodeType::FOR_EXPRESSION) {
      if (ExprNode->child_count() < 2) {
         expression_unsupported = true;
         return XPathValue();
      }

      const XPathNode *return_node = ExprNode->get_child(ExprNode->child_count() - 1);
      if (!return_node) {
         expression_unsupported = true;
         return XPathValue();
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
               return XPathValue();
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
            return XPathValue();
         }

         const XPathNode *sequence_node = ExprNode->get_child(0);
         if ((!sequence_node) or (!return_node) or ExprNode->value.empty()) {
            expression_unsupported = true;
            return XPathValue();
         }

         bindings.clear();
         bindings.push_back({ ExprNode->value, sequence_node });
      }

      if (bindings.empty()) {
         expression_unsupported = true;
         return XPathValue();
      }

      std::vector<XMLTag *> combined_nodes;
      std::vector<std::string> combined_strings;
      std::vector<const XMLAttrib *> combined_attributes;
      std::optional<std::string> combined_override;

      std::function<bool(size_t)> evaluate_bindings = [&](size_t binding_index) -> bool {
         if (binding_index >= bindings.size()) {
            auto iteration_value = evaluate_expression(return_node, CurrentPrefix);
            if (expression_unsupported) return false;

            if (iteration_value.type != XPathValueType::NodeSet) {
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
               else if (node) node_string = XPathValue::node_string_value(node);

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

         if (sequence_value.type != XPathValueType::NodeSet) {
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

            XPathValue bound_value;
            bound_value.type = XPathValueType::NodeSet;
            bound_value.node_set.push_back(item_node);
            bound_value.node_set_attributes.push_back(item_attribute);

            std::string item_string;
            bool use_override = sequence_value.node_set_string_override.has_value() and
               (index IS 0) and sequence_value.node_set_string_values.empty();
            if (index < sequence_value.node_set_string_values.size()) {
               item_string = sequence_value.node_set_string_values[index];
            }
            else if (use_override) item_string = *sequence_value.node_set_string_override;
            else if (item_node) item_string = XPathValue::node_string_value(item_node);

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
      if (!evaluation_ok) return XPathValue();
      if (expression_unsupported) return XPathValue();

      XPathValue result;
      result.type = XPathValueType::NodeSet;
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
         return XPathValue();
      }

      bool is_some = ExprNode->value IS "some";
      bool is_every = ExprNode->value IS "every";

      if ((!is_some) and (!is_every)) {
         expression_unsupported = true;
         return XPathValue();
      }

      const XPathNode *condition_node = ExprNode->get_child(ExprNode->child_count() - 1);
      if (!condition_node) {
         expression_unsupported = true;
         return XPathValue();
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
            return XPathValue();
         }

         if ((binding_node->value.empty()) or (binding_node->child_count() IS 0)) {
            expression_unsupported = true;
            return XPathValue();
         }

         bindings.push_back({ binding_node->value, binding_node->get_child(0) });
      }

      if (bindings.empty()) {
         expression_unsupported = true;
         return XPathValue();
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

         if (sequence_value.type != XPathValueType::NodeSet) {
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

            XPathValue bound_value;
            bound_value.type = XPathValueType::NodeSet;
            bound_value.node_set.push_back(item_node);
            bound_value.node_set_attributes.push_back(item_attribute);

            std::string item_string;
            bool use_override = sequence_value.node_set_string_override.has_value() and
               (index IS 0) and sequence_value.node_set_string_values.empty();
            if (index < sequence_value.node_set_string_values.size()) {
               item_string = sequence_value.node_set_string_values[index];
            }
            else if (use_override) item_string = *sequence_value.node_set_string_override;
            else if (item_node) item_string = XPathValue::node_string_value(item_node);

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
      if (expression_unsupported) return XPathValue();

      return XPathValue(quant_result);
   }

   if (ExprNode->type IS XPathNodeType::FILTER) {
      if (ExprNode->child_count() IS 0) {
         expression_unsupported = true;
         return XPathValue();
      }

      auto base_value = evaluate_expression(ExprNode->get_child(0), CurrentPrefix);
      if (expression_unsupported) return XPathValue();

      if (base_value.type != XPathValueType::NodeSet) {
         expression_unsupported = true;
         return XPathValue();
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
               return XPathValue();
            }

            if (predicate_result IS PredicateResult::MATCH) passed.push_back(base_index);
         }

         working_indices.swap(passed);
         if (working_indices.empty()) break;
      }

      std::vector<XMLTag *> filtered_nodes;
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

      return XPathValue(filtered_nodes, first_value, std::move(filtered_strings), std::move(filtered_attributes));
   }

   if (ExprNode->type IS XPathNodeType::PATH) {
      if (ExprNode->child_count() IS 0) {
         expression_unsupported = true;
         return XPathValue();
      }

      auto *first_child = ExprNode->get_child(0);
      if (first_child and (first_child->type IS XPathNodeType::LOCATION_PATH)) {
         return evaluate_path_expression_value(ExprNode, CurrentPrefix);
      }

      auto base_value = evaluate_expression(first_child, CurrentPrefix);
      if (expression_unsupported) return XPathValue();

      if (base_value.type != XPathValueType::NodeSet) {
         return XPathValue(base_value.to_node_set());
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
      if (expression_unsupported) return XPathValue();
      return value;
   }

   if (ExprNode->type IS XPathNodeType::UNARY_OP) {
      if (ExprNode->child_count() IS 0) {
         expression_unsupported = true;
         return XPathValue();
      }

      auto operand = evaluate_expression(ExprNode->get_child(0), CurrentPrefix);
      if (expression_unsupported) return XPathValue();

      if (ExprNode->value IS "-") {
         return XPathValue(-operand.to_number());
      }

      if (ExprNode->value IS "not") {
         return XPathValue(!operand.to_boolean());
      }

      expression_unsupported = true;
      return XPathValue();
   }

   if (ExprNode->type IS XPathNodeType::BINARY_OP) {
      if (ExprNode->child_count() < 2) {
         expression_unsupported = true;
         return XPathValue();
      }

      auto *left_node = ExprNode->get_child(0);
      auto *right_node = ExprNode->get_child(1);

      const std::string &operation = ExprNode->value;

      if (operation IS "and") {
         auto left_value = evaluate_expression(left_node, CurrentPrefix);
         if (expression_unsupported) return XPathValue();

         bool left_boolean = left_value.to_boolean();
         if (!left_boolean) return XPathValue(false);

         auto right_value = evaluate_expression(right_node, CurrentPrefix);
         if (expression_unsupported) return XPathValue();

         bool right_boolean = right_value.to_boolean();
         return XPathValue(right_boolean);
      }

      if (operation IS "or") {
         auto left_value = evaluate_expression(left_node, CurrentPrefix);
         if (expression_unsupported) return XPathValue();

         bool left_boolean = left_value.to_boolean();
         if (left_boolean) return XPathValue(true);

         auto right_value = evaluate_expression(right_node, CurrentPrefix);
         if (expression_unsupported) return XPathValue();

         bool right_boolean = right_value.to_boolean();
         return XPathValue(right_boolean);
      }

      if (operation IS "|") {
         std::vector<const XPathNode *> branches;
         branches.reserve(2);
         if (left_node) branches.push_back(left_node);
         if (right_node) branches.push_back(right_node);
         return evaluate_union_value(branches, CurrentPrefix);
      }

      if (operation IS "intersect") {
         return evaluate_intersect_value(left_node, right_node, CurrentPrefix);
      }

      if (operation IS "except") {
         return evaluate_except_value(left_node, right_node, CurrentPrefix);
      }

      auto left_value = evaluate_expression(left_node, CurrentPrefix);
      if (expression_unsupported) return XPathValue();
      auto right_value = evaluate_expression(right_node, CurrentPrefix);
      if (expression_unsupported) return XPathValue();

      if (operation IS "=") {
         bool equals = compare_xpath_values(left_value, right_value);
         return XPathValue(equals);
      }

      if (operation IS "!=") {
         bool equals = compare_xpath_values(left_value, right_value);
         return XPathValue(!equals);
      }

      if (operation IS "eq") {
         auto left_scalar = promote_value_comparison_operand(left_value);
         auto right_scalar = promote_value_comparison_operand(right_value);
         if (!left_scalar.has_value() or !right_scalar.has_value()) return XPathValue(false);
         bool equals = compare_xpath_values(*left_scalar, *right_scalar);
         return XPathValue(equals);
      }

      if (operation IS "ne") {
         auto left_scalar = promote_value_comparison_operand(left_value);
         auto right_scalar = promote_value_comparison_operand(right_value);
         if (!left_scalar.has_value() or !right_scalar.has_value()) return XPathValue(false);
         bool equals = compare_xpath_values(*left_scalar, *right_scalar);
         return XPathValue(!equals);
      }

      if (operation IS "<") {
         bool result = compare_xpath_relational(left_value, right_value, RelationalOperator::LESS);
         return XPathValue(result);
      }

      if (operation IS "<=") {
         bool result = compare_xpath_relational(left_value, right_value, RelationalOperator::LESS_OR_EQUAL);
         return XPathValue(result);
      }

      if (operation IS ">") {
         bool result = compare_xpath_relational(left_value, right_value, RelationalOperator::GREATER);
         return XPathValue(result);
      }

      if (operation IS ">=") {
         bool result = compare_xpath_relational(left_value, right_value, RelationalOperator::GREATER_OR_EQUAL);
         return XPathValue(result);
      }

      if (operation IS "lt") {
         auto left_scalar = promote_value_comparison_operand(left_value);
         auto right_scalar = promote_value_comparison_operand(right_value);
         if (!left_scalar.has_value() or !right_scalar.has_value()) return XPathValue(false);
         bool result = compare_xpath_relational(*left_scalar, *right_scalar, RelationalOperator::LESS);
         return XPathValue(result);
      }

      if (operation IS "le") {
         auto left_scalar = promote_value_comparison_operand(left_value);
         auto right_scalar = promote_value_comparison_operand(right_value);
         if (!left_scalar.has_value() or !right_scalar.has_value()) return XPathValue(false);
         bool result = compare_xpath_relational(*left_scalar, *right_scalar, RelationalOperator::LESS_OR_EQUAL);
         return XPathValue(result);
      }

      if (operation IS "gt") {
         auto left_scalar = promote_value_comparison_operand(left_value);
         auto right_scalar = promote_value_comparison_operand(right_value);
         if (!left_scalar.has_value() or !right_scalar.has_value()) return XPathValue(false);
         bool result = compare_xpath_relational(*left_scalar, *right_scalar, RelationalOperator::GREATER);
         return XPathValue(result);
      }

      if (operation IS "ge") {
         auto left_scalar = promote_value_comparison_operand(left_value);
         auto right_scalar = promote_value_comparison_operand(right_value);
         if (!left_scalar.has_value() or !right_scalar.has_value()) return XPathValue(false);
         bool result = compare_xpath_relational(*left_scalar, *right_scalar, RelationalOperator::GREATER_OR_EQUAL);
         return XPathValue(result);
      }

      if (operation IS "+") {
         double result = left_value.to_number() + right_value.to_number();
         return XPathValue(result);
      }

      if (operation IS "-") {
         double result = left_value.to_number() - right_value.to_number();
         return XPathValue(result);
      }

      if (operation IS "*") {
         double result = left_value.to_number() * right_value.to_number();
         return XPathValue(result);
      }

      if (operation IS "div") {
         double result = left_value.to_number() / right_value.to_number();
         return XPathValue(result);
      }

      if (operation IS "mod") {
         double left_number = left_value.to_number();
         double right_number = right_value.to_number();
         double result = std::fmod(left_number, right_number);
         return XPathValue(result);
      }

      expression_unsupported = true;
      return XPathValue();
   }

   if (ExprNode->type IS XPathNodeType::VARIABLE_REFERENCE) {
      auto local_variable = context.variables.find(ExprNode->value);
      if (local_variable != context.variables.end()) {
         return local_variable->second;
      }

      // Look up variable in the XML object's variable storage
      auto it = xml->Variables.find(ExprNode->value);
      if (it != xml->Variables.end()) {
         return XPathValue(it->second);
      }
      else {
         // Variable not found - XPath 1.0 spec requires this to be an error
         expression_unsupported = true;
         return XPathValue();
      }
   }

   expression_unsupported = true;
   return XPathValue();
}

//********************************************************************************************************************

ERR XPathEvaluator::process_expression_node_set(const XPathValue &Value)
{
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

   if (entries.empty()) {
      xml->Attrib.clear();
      return ERR::Search;
   }

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

   switch (value.type) {
      case XPathValueType::NodeSet:
         return process_expression_node_set(value);

      case XPathValueType::Boolean:
      case XPathValueType::Number:
      case XPathValueType::String:
      case XPathValueType::Date:
      case XPathValueType::Time:
      case XPathValueType::DateTime:
         xml->Attrib = value.to_string();
         return ERR::Okay;
   }

   return ERR::Failed;
}

//********************************************************************************************************************

XPathValue XPathEvaluator::evaluate_function_call(const XPathNode *FuncNode, uint32_t CurrentPrefix) {
   if (!FuncNode or FuncNode->type != XPathNodeType::FUNCTION_CALL) {
      return XPathValue();
   }

   std::string function_name = FuncNode->value;

   std::vector<XPathValue> args;
   args.reserve(FuncNode->child_count());

   for (size_t index = 0; index < FuncNode->child_count(); ++index) {
      auto *argument_node = FuncNode->get_child(index);
      args.push_back(evaluate_expression(argument_node, CurrentPrefix));
      if (expression_unsupported) return XPathValue();
   }

   if (function_name IS "text") {
      std::vector<XMLTag *> text_nodes;
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

      return XPathValue(text_nodes, first_value);
   }

   return XPathFunctionLibrary::instance().call_function(function_name, args, context);
}
