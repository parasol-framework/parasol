//********************************************************************************************************************
// XPath Evaluation Engine
//********************************************************************************************************************
//
// The evaluator coordinates the complete XPath execution pipeline for Parasol's XML subsystem.  It
// receives token sequences from the tokenizer, constructs an AST via the parser, and then walks that
// AST to resolve node-sets, scalar values, and boolean predicates against the in-memory document
// model.  Beyond expression evaluation, the class maintains the implicit evaluation context defined by
// the XPath specification (context node, size, position, and active attribute), marshals axis
// traversal through AxisEvaluator, and carefully mirrors document order semantics so that results
// match the behaviour expected by downstream engines.
//
// This translation unit focuses on execution concerns: stack management for nested contexts, helper
// routines for managing evaluation state, AST caching, dispatching axes, and interpretation of AST nodes.  A
// large portion of the logic is defensive—preserving cursor state for integration with the legacy
// cursor-based API, falling back gracefully when unsupported expressions are encountered, and
// honouring namespace prefix resolution rules.  By keeping the evaluator self-contained, the parser
// and tokenizer remain ignorant of runtime data structures, and testing of the evaluator can be done
// independently of XML parsing.

#include "xpath_evaluator.h"
#include "xpath_axis.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace {

// Lightweight view-based trim used for cache key normalisation.
[[maybe_unused]] std::string_view trim_view(std::string_view Value)
{
   auto start = Value.find_first_not_of(" \t\r\n");
   if (start IS std::string_view::npos) return std::string_view();

   auto end = Value.find_last_not_of(" \t\r\n");
   return Value.substr(start, end - start + 1);
}

} // namespace

enum class RelationalOperator {
   LESS,
   LESS_OR_EQUAL,
   GREATER,
   GREATER_OR_EQUAL
};

// Compare two floating-point numbers for equality using epsilon tolerance.
// This prevents precision issues when comparing computed values like averages.

static bool numeric_equal(double Left, double Right)
{
   if (std::isnan(Left) or std::isnan(Right)) return false;
   if (std::isinf(Left) or std::isinf(Right)) return Left IS Right;

   // Use relative epsilon for larger numbers, absolute epsilon for numbers near zero
   const double abs_left = std::fabs(Left);
   const double abs_right = std::fabs(Right);
   const double larger = (abs_left > abs_right) ? abs_left : abs_right;

   if (larger <= 1.0) {
      // Use absolute epsilon for small numbers
      return std::fabs(Left - Right) <= std::numeric_limits<double>::epsilon() * 16;
   } else {
      // Use relative epsilon for larger numbers
      return std::fabs(Left - Right) <= larger * std::numeric_limits<double>::epsilon() * 16;
   }
}

static bool numeric_compare(double Left, double Right, RelationalOperator Operation)
{
   if (std::isnan(Left) or std::isnan(Right)) return false;

   switch (Operation) {
      case RelationalOperator::LESS: return Left < Right;
      case RelationalOperator::LESS_OR_EQUAL: return Left <= Right;
      case RelationalOperator::GREATER: return Left > Right;
      case RelationalOperator::GREATER_OR_EQUAL: return Left >= Right;
   }

   return false;
}

//********************************************************************************************************************
// Context Management

// Preserve the current evaluation context and establish a new one for nested expressions.
void XPathEvaluator::push_context(XMLTag *Node, size_t Position, size_t Size, const XMLAttrib *Attribute)
{
   auto document = context.document ? context.document : xml;
   context_stack.push_back(context);
   context.context_node = Node;
   context.attribute_node = Attribute;
   context.position = Position;
   context.size = Size;
   context.document = document;
}

// Restore the previous context when unwinding recursive evaluation.
void XPathEvaluator::pop_context()
{
   if (context_stack.empty()) {
      context.context_node = nullptr;
      context.attribute_node = nullptr;
      context.position = 1;
      context.size = 1;
      context.document = xml;
      return;
   }

   context = context_stack.back();
   context_stack.pop_back();
}

// Snapshot cursor state so legacy cursor-based APIs can be restored after XPath evaluation.
void XPathEvaluator::push_cursor_state()
{
   CursorState state{};
   state.tags = xml->CursorTags;

   if ((xml->CursorTags) and (xml->CursorTags->begin() != xml->CursorTags->end())) {
      state.index = size_t(xml->Cursor - xml->CursorTags->begin());
   }
   else state.index = 0;

   cursor_stack.push_back(state);
}

// Reinstate any saved cursor state.
void XPathEvaluator::pop_cursor_state()
{
   if (cursor_stack.empty()) return;

   auto state = cursor_stack.back();
   cursor_stack.pop_back();

   xml->CursorTags = state.tags;

   if (!xml->CursorTags) return;

   auto begin = xml->CursorTags->begin();
   if (state.index >= size_t(xml->CursorTags->size())) {
      xml->Cursor = xml->CursorTags->end();
   }
   else xml->Cursor = begin + state.index;
}

// Convert an axis selection into a list of node or attribute matches relative to the active context.
std::vector<XPathEvaluator::AxisMatch> XPathEvaluator::dispatch_axis(AxisType Axis,
   XMLTag *ContextNode, const XMLAttrib *ContextAttribute)
{
   std::vector<AxisMatch> matches;

   // Pre-size result container based on axis type and context
   size_t estimated_capacity = axis_evaluator.estimate_result_size(Axis, ContextNode);
   matches.reserve(estimated_capacity);

   auto append_nodes = [this, &matches](std::vector<XMLTag *> &nodes) {
      matches.reserve(matches.size() + nodes.size());
      for (auto *node : nodes) {
         matches.push_back({ node, nullptr });
      }
      arena.release_node_vector(nodes);
   };

   bool attribute_context = ContextAttribute != nullptr;

   switch (Axis) {
      case AxisType::CHILD: {
         if (attribute_context) break;

         if (!ContextNode) {
            for (auto &tag : xml->Tags) {
               if (!tag.isTag()) continue;
               matches.push_back({ &tag, nullptr });
            }
         }
         else {
            auto &child_buffer = arena.acquire_node_vector();
            axis_evaluator.evaluate_axis(AxisType::CHILD, ContextNode, child_buffer);
            append_nodes(child_buffer);
         }
         break;
      }

      case AxisType::DESCENDANT: {
         if (attribute_context) break;

         if (!ContextNode) {
            for (auto &tag : xml->Tags) {
               if (!tag.isTag()) continue;
               matches.push_back({ &tag, nullptr });
               auto &desc_buffer = arena.acquire_node_vector();
               axis_evaluator.evaluate_axis(AxisType::DESCENDANT, &tag, desc_buffer);
               append_nodes(desc_buffer);
            }
         }
         else {
            auto &desc_buffer = arena.acquire_node_vector();
            axis_evaluator.evaluate_axis(AxisType::DESCENDANT, ContextNode, desc_buffer);
            append_nodes(desc_buffer);
         }
         break;
      }

      case AxisType::DESCENDANT_OR_SELF: {
         if (attribute_context) {
            matches.push_back({ ContextNode, ContextAttribute });
            break;
         }

         if (!ContextNode) {
            matches.push_back({ nullptr, nullptr });
            for (auto &tag : xml->Tags) {
               if (!tag.isTag()) continue;
               matches.push_back({ &tag, nullptr });
               auto &desc_buffer = arena.acquire_node_vector();
               axis_evaluator.evaluate_axis(AxisType::DESCENDANT, &tag, desc_buffer);
               append_nodes(desc_buffer);
            }
         }
         else {
            matches.push_back({ ContextNode, nullptr });
            auto &desc_buffer = arena.acquire_node_vector();
            axis_evaluator.evaluate_axis(AxisType::DESCENDANT, ContextNode, desc_buffer);
            append_nodes(desc_buffer);
         }
         break;
      }

      case AxisType::SELF: {
         if (attribute_context) {
            matches.push_back({ ContextNode, ContextAttribute });
         }
         else matches.push_back({ ContextNode, nullptr });
         break;
      }

      case AxisType::PARENT: {
         if (attribute_context) {
            if (ContextNode) matches.push_back({ ContextNode, nullptr });
         }
         else if (ContextNode) {
            auto &parent_buffer = arena.acquire_node_vector();
            axis_evaluator.evaluate_axis(AxisType::PARENT, ContextNode, parent_buffer);
            append_nodes(parent_buffer);
         }
         break;
      }

      case AxisType::ANCESTOR: {
         if (attribute_context) {
            if (ContextNode) {
               matches.push_back({ ContextNode, nullptr });
               auto &ancestor_buffer = arena.acquire_node_vector();
               axis_evaluator.evaluate_axis(AxisType::ANCESTOR, ContextNode, ancestor_buffer);
               append_nodes(ancestor_buffer);
            }
         }
         else if (ContextNode) {
            auto &ancestor_buffer = arena.acquire_node_vector();
            axis_evaluator.evaluate_axis(AxisType::ANCESTOR, ContextNode, ancestor_buffer);
            append_nodes(ancestor_buffer);
         }
        break;
     }

      case AxisType::ANCESTOR_OR_SELF: {
         if (attribute_context) {
            matches.push_back({ ContextNode, ContextAttribute });
            if (ContextNode) {
               matches.push_back({ ContextNode, nullptr });
               auto &ancestor_buffer = arena.acquire_node_vector();
               axis_evaluator.evaluate_axis(AxisType::ANCESTOR, ContextNode, ancestor_buffer);
               append_nodes(ancestor_buffer);
            }
         }
         else if (ContextNode) {
            matches.push_back({ ContextNode, nullptr });
            auto &ancestor_buffer = arena.acquire_node_vector();
            axis_evaluator.evaluate_axis(AxisType::ANCESTOR, ContextNode, ancestor_buffer);
            append_nodes(ancestor_buffer);
         }
         else matches.push_back({ nullptr, nullptr });
         break;
      }

      case AxisType::FOLLOWING_SIBLING: {
         if (attribute_context) break;
         if (ContextNode) {
            auto &sibling_buffer = arena.acquire_node_vector();
            axis_evaluator.evaluate_axis(AxisType::FOLLOWING_SIBLING, ContextNode, sibling_buffer);
            append_nodes(sibling_buffer);
         }
        break;
     }

      case AxisType::PRECEDING_SIBLING: {
         if (attribute_context) break;
         if (ContextNode) {
            auto &sibling_buffer = arena.acquire_node_vector();
            axis_evaluator.evaluate_axis(AxisType::PRECEDING_SIBLING, ContextNode, sibling_buffer);
            append_nodes(sibling_buffer);
         }
        break;
     }

      case AxisType::FOLLOWING: {
         if (attribute_context) break;
         if (ContextNode) {
            auto &following_buffer = arena.acquire_node_vector();
            axis_evaluator.evaluate_axis(AxisType::FOLLOWING, ContextNode, following_buffer);
            append_nodes(following_buffer);
         }
        break;
     }

      case AxisType::PRECEDING: {
         if (attribute_context) break;
         if (ContextNode) {
            auto &preceding_buffer = arena.acquire_node_vector();
            axis_evaluator.evaluate_axis(AxisType::PRECEDING, ContextNode, preceding_buffer);
            append_nodes(preceding_buffer);
         }
        break;
     }

      case AxisType::ATTRIBUTE: {
         if (attribute_context) break;
         if (ContextNode and ContextNode->isTag()) {
            for (size_t index = 1; index < ContextNode->Attribs.size(); ++index) {
               matches.push_back({ ContextNode, &ContextNode->Attribs[index] });
            }
         }
         break;
      }

      case AxisType::NAMESPACE: {
         if (attribute_context) break;
         if (ContextNode) {
            auto &namespace_buffer = arena.acquire_node_vector();
            axis_evaluator.evaluate_axis(AxisType::NAMESPACE, ContextNode, namespace_buffer);
            append_nodes(namespace_buffer);
         }
         break;
      }
   }

   return matches;
}

//********************************************************************************************************************
// AST Evaluation Methods

// Dispatch AST nodes to the appropriate evaluation routine.
ERR XPathEvaluator::evaluate_ast(const XPathNode *Node, uint32_t CurrentPrefix) {
   if (!Node) return ERR::Failed;

   switch (Node->type) {
      case XPathNodeType::LOCATION_PATH:
         return evaluate_location_path(Node, CurrentPrefix);

      case XPathNodeType::STEP:
         return evaluate_step_ast(Node, CurrentPrefix);

      case XPathNodeType::UNION:
         return evaluate_union(Node, CurrentPrefix);

      case XPathNodeType::PATH:
         if ((Node->child_count() > 0) and Node->get_child(0) and
             (Node->get_child(0)->type IS XPathNodeType::LOCATION_PATH)) {
            return evaluate_location_path(Node->get_child(0), CurrentPrefix);
         }
         return evaluate_top_level_expression(Node, CurrentPrefix);

      case XPathNodeType::EXPRESSION:
      case XPathNodeType::FILTER:
      case XPathNodeType::BINARY_OP:
      case XPathNodeType::UNARY_OP:
      case XPathNodeType::FUNCTION_CALL:
      case XPathNodeType::LITERAL:
      case XPathNodeType::VARIABLE_REFERENCE:
      case XPathNodeType::NUMBER:
      case XPathNodeType::STRING:
      case XPathNodeType::CONDITIONAL:
      case XPathNodeType::FOR_EXPRESSION:
         return evaluate_top_level_expression(Node, CurrentPrefix);

      default:
         return ERR::Failed;
   }
}

// Execute a full location path expression, managing implicit root handling and cursor updates.
ERR XPathEvaluator::evaluate_location_path(const XPathNode *PathNode, uint32_t CurrentPrefix) {
   if ((!PathNode) or (PathNode->type != XPathNodeType::LOCATION_PATH)) return ERR::Failed;

   pf::Log log(__FUNCTION__);

   std::vector<const XPathNode *> steps;
   std::vector<std::unique_ptr<XPathNode>> synthetic_steps;

   bool has_root = false;
   bool root_descendant = false;

   for (size_t i = 0; i < PathNode->child_count(); ++i) {
      auto child = PathNode->get_child(i);
      if (!child) continue;

      if ((i IS 0) and (child->type IS XPathNodeType::ROOT)) {
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

   if (steps.empty()) return ERR::Search;

   std::vector<XMLTag *> initial_context;

   if (has_root) {
      initial_context.push_back(nullptr);
   }
   else {
      if (context.context_node) initial_context.push_back(context.context_node);
      else if ((xml->CursorTags) and (xml->Cursor != xml->CursorTags->end())) initial_context.push_back(&(*xml->Cursor));
      else initial_context.push_back(nullptr);
   }

   bool matched = false;
   auto result = evaluate_step_sequence(initial_context, steps, 0, CurrentPrefix, matched);

   if ((result != ERR::Okay) and (result != ERR::Search)) return result;

   if (xml->Callback.defined()) return ERR::Okay;
   if (matched) return ERR::Okay;
   return ERR::Search;
}

ERR XPathEvaluator::evaluate_union(const XPathNode *Node, uint32_t CurrentPrefix)
{
   if ((!Node) or (Node->type != XPathNodeType::UNION)) return ERR::Failed;

   auto saved_context = context;
   auto saved_context_stack = context_stack;
   auto saved_cursor_stack = cursor_stack;
   auto saved_cursor_tags = xml->CursorTags;
   auto saved_cursor = xml->Cursor;
   auto saved_attrib = xml->Attrib;
   bool saved_expression_unsupported = expression_unsupported;

   ERR last_error = ERR::Search;

   std::unordered_set<std::string> evaluated_branches;
   evaluated_branches.reserve(Node->child_count());

   for (size_t index = 0; index < Node->child_count(); ++index) {
      auto branch = Node->get_child(index);
      if (!branch) continue;

      auto branch_signature = build_ast_signature(branch);
      if (!branch_signature.empty()) {
         auto insert_result = evaluated_branches.insert(branch_signature);
         if (!insert_result.second) continue;
      }

      context = saved_context;
      context_stack = saved_context_stack;
      cursor_stack = saved_cursor_stack;
      xml->CursorTags = saved_cursor_tags;
      xml->Cursor = saved_cursor;
      xml->Attrib = saved_attrib;
      expression_unsupported = saved_expression_unsupported;

      auto result = evaluate_ast(branch, CurrentPrefix);
      if ((result IS ERR::Okay) or (result IS ERR::Terminate)) return result;

      if (result != ERR::Search) {
         last_error = result;
         break;
      }
   }

   context = saved_context;
   context_stack = saved_context_stack;
   cursor_stack = saved_cursor_stack;
   xml->CursorTags = saved_cursor_tags;
   xml->Cursor = saved_cursor;
   xml->Attrib = saved_attrib;
   expression_unsupported = saved_expression_unsupported;

   return last_error;
}

// Evaluate a single step expression against the current context.
ERR XPathEvaluator::evaluate_step_ast(const XPathNode *StepNode, uint32_t CurrentPrefix) {
   if (!StepNode) return ERR::Failed;

   std::vector<const XPathNode *> steps;
   steps.push_back(StepNode);

   std::vector<XMLTag *> context_nodes;
   if (context.context_node) context_nodes.push_back(context.context_node);
   else if ((xml->CursorTags) and (xml->Cursor != xml->CursorTags->end())) context_nodes.push_back(&(*xml->Cursor));
   else context_nodes.push_back(nullptr);

   bool matched = false;
   auto result = evaluate_step_sequence(context_nodes, steps, 0, CurrentPrefix, matched);

   if ((result != ERR::Okay) and (result != ERR::Search)) return result;

   if (xml->Callback.defined()) return ERR::Okay;
   if (matched) return ERR::Okay;
   return ERR::Search;
}

// Recursive driver that iterates through each step in a location path.
ERR XPathEvaluator::evaluate_step_sequence(const std::vector<XMLTag *> &ContextNodes, const std::vector<const XPathNode *> &Steps, size_t StepIndex, uint32_t CurrentPrefix, bool &Matched) {
   if (StepIndex >= Steps.size()) return Matched ? ERR::Okay : ERR::Search;

   std::vector<AxisMatch> current_context;
   current_context.reserve(ContextNodes.size());

   for (auto *candidate : ContextNodes) {
      const XMLAttrib *attribute = nullptr;
      if ((candidate) and context.attribute_node and (candidate IS context.context_node)) attribute = context.attribute_node;
      current_context.push_back({ candidate, attribute });
   }

   std::vector<AxisMatch> next_context;
   next_context.reserve(current_context.size());

   std::vector<AxisMatch> filtered;
   filtered.reserve(current_context.size());
   std::vector<AxisMatch> predicate_buffer;
   predicate_buffer.reserve(current_context.size());

   for (size_t step_index = StepIndex; step_index < Steps.size(); ++step_index) {
      if (current_context.empty()) break;

      auto step_node = Steps[step_index];
      if ((!step_node) or (step_node->type != XPathNodeType::STEP)) return ERR::Failed;

      const XPathNode *axis_node = nullptr;
      const XPathNode *node_test = nullptr;
      std::vector<const XPathNode *> predicate_nodes;
      predicate_nodes.reserve(step_node->child_count());

      for (size_t i = 0; i < step_node->child_count(); ++i) {
         auto child = step_node->get_child(i);
         if (!child) continue;

         if (child->type IS XPathNodeType::AXIS_SPECIFIER) axis_node = child;
         else if (child->type IS XPathNodeType::PREDICATE) predicate_nodes.push_back(child);
         else if ((!node_test) and ((child->type IS XPathNodeType::NAME_TEST) or (child->type IS XPathNodeType::WILDCARD) or (child->type IS XPathNodeType::NODE_TYPE_TEST))) {
            node_test = child;
         }
      }

      AxisType axis = AxisType::CHILD;
      if (axis_node) axis = AxisEvaluator::parse_axis_name(axis_node->value);

      bool is_last_step = (step_index + 1 >= Steps.size());

      for (auto &context_entry : current_context) {
         auto *context_node = context_entry.node;
         const XMLAttrib *context_attribute = context_entry.attribute;

         if ((!context_attribute) and context_node and context.attribute_node and (context_node IS context.context_node)) {
            context_attribute = context.attribute_node;
         }

         auto axis_matches = dispatch_axis(axis, context_node, context_attribute);

         filtered.clear();
         filtered.reserve(axis_matches.size());

         for (auto &match : axis_matches) {
            if (!match_node_test(node_test, axis, match.node, match.attribute, CurrentPrefix)) continue;
            filtered.push_back(match);
         }

         if (filtered.empty()) continue;

         for (auto *predicate_node : predicate_nodes) {
            predicate_buffer.clear();
            predicate_buffer.reserve(filtered.size());

            for (size_t index = 0; index < filtered.size(); ++index) {
               auto &match = filtered[index];
               push_context(match.node, index + 1, filtered.size(), match.attribute);

               auto predicate_result = evaluate_predicate(predicate_node, CurrentPrefix);
               pop_context();

               if (predicate_result IS PredicateResult::UNSUPPORTED) {
                  return ERR::Failed;
               }

               if (predicate_result IS PredicateResult::MATCH) predicate_buffer.push_back(match);
            }

            filtered.swap(predicate_buffer);
            if (filtered.empty()) break;
         }

         if (filtered.empty()) continue;

         for (size_t index = 0; index < filtered.size(); ++index) {
            auto &match = filtered[index];
            auto *candidate = match.node;

            push_context(candidate, index + 1, filtered.size(), match.attribute);

            if (axis IS AxisType::ATTRIBUTE) {
               AxisMatch next_match{};
               next_match.node = candidate;
               next_match.attribute = match.attribute;

               if (!next_match.node or !next_match.attribute) {
                  pop_context();
                  continue;
               }

               if (is_last_step) {
                  auto tags = xml->getInsert(next_match.node, xml->Cursor);
                  if (!tags) {
                     pop_context();
                     continue;
                  }

                  xml->CursorTags = tags;
                  xml->Attrib = next_match.attribute->Name;

                  if (!xml->Callback.defined()) {
                     Matched = true;
                     pop_context();
                     return ERR::Okay;
                  }

                  push_cursor_state();

                  ERR callback_error = ERR::Okay;
                  if (xml->Callback.isC()) {
                     auto routine = (ERR (*)(extXML *, int, CSTRING, APTR))xml->Callback.Routine;
                     callback_error = routine(xml, next_match.node->ID, xml->Attrib.empty() ? nullptr : xml->Attrib.c_str(), xml->Callback.Meta);
                  }
                  else if (xml->Callback.isScript()) {
                     if (sc::Call(xml->Callback, std::to_array<ScriptArg>({
                        { "XML",  xml, FD_OBJECTPTR },
                        { "Tag",  next_match.node->ID },
                        { "Attrib", xml->Attrib.empty() ? CSTRING(nullptr) : xml->Attrib.c_str() }
                     }), callback_error) != ERR::Okay) callback_error = ERR::Terminate;
                  }
                  else callback_error = ERR::InvalidValue;

                  pop_cursor_state();
                  pop_context();

                  Matched = true;

                  if (callback_error IS ERR::Terminate) return ERR::Terminate;
                  if (callback_error != ERR::Okay) return callback_error;

                  continue;
               }

               next_context.push_back(next_match);
               pop_context();
               continue;
            }

            if (is_last_step) {
               if (!candidate) {
                  pop_context();
                  continue;
               }

               auto tags = xml->getInsert(candidate, xml->Cursor);
               if (!tags) {
                  pop_context();
                  continue;
               }

               xml->CursorTags = tags;
               xml->Attrib.clear();

               if (!xml->Callback.defined()) {
                  Matched = true;
                  pop_context();
                  return ERR::Okay;
               }

               push_cursor_state();
               ERR callback_error = ERR::Okay;
               if (xml->Callback.isC()) {
                  auto routine = (ERR (*)(extXML *, int, CSTRING, APTR))xml->Callback.Routine;
                  callback_error = routine(xml, candidate->ID, xml->Attrib.empty() ? nullptr : xml->Attrib.c_str(), xml->Callback.Meta);
               }
               else if (xml->Callback.isScript()) {
                  if (sc::Call(xml->Callback, std::to_array<ScriptArg>({
                     { "XML",  xml, FD_OBJECTPTR },
                     { "Tag",  candidate->ID },
                     { "Attrib", xml->Attrib.empty() ? CSTRING(nullptr) : xml->Attrib.c_str() }
                  }), callback_error) != ERR::Okay) callback_error = ERR::Terminate;
               }
               else callback_error = ERR::InvalidValue;

               pop_cursor_state();
               pop_context();

               Matched = true;

               if (callback_error IS ERR::Terminate) return ERR::Terminate;
               if (callback_error != ERR::Okay) return callback_error;

               continue;
            }

            if (!candidate) {
               pop_context();
               continue;
            }

            next_context.push_back({ candidate, nullptr });
            pop_context();
         }
      }

      current_context.swap(next_context);
      next_context.clear();
   }

   return Matched ? ERR::Okay : ERR::Search;
}

bool XPathEvaluator::match_node_test(const XPathNode *NodeTest, AxisType Axis, XMLTag *Candidate, const XMLAttrib *Attribute, uint32_t CurrentPrefix) {
   bool attribute_axis = (Axis IS AxisType::ATTRIBUTE) or ((Axis IS AxisType::SELF) and (Attribute != nullptr));

   auto resolve_namespace = [&](std::string_view Prefix, XMLTag *Scope) -> std::optional<uint32_t> {
      if (!xml) return std::nullopt;

      std::string prefix_string(Prefix);
      uint32_t namespace_hash = 0;
      XMLTag *lookup_scope = Scope ? Scope : context.context_node;
      int tag_id = lookup_scope ? lookup_scope->ID : 0;

      if (xml->resolvePrefix(prefix_string, tag_id, namespace_hash) IS ERR::Okay) {
         return namespace_hash;
      }

      if (lookup_scope and context.context_node and (lookup_scope != context.context_node)) {
         if (xml->resolvePrefix(prefix_string, context.context_node->ID, namespace_hash) IS ERR::Okay) {
            return namespace_hash;
         }
      }

      if (!prefix_string.empty()) {
         auto it = xml->Prefixes.find(prefix_string);
         if (it != xml->Prefixes.end()) return it->second;
      }

      return std::nullopt;
   };

   if (!NodeTest) {
      if (attribute_axis) return Attribute != nullptr;
      return Candidate != nullptr;
   }

   if (attribute_axis) {
      if (!Attribute) return false;

      if (NodeTest->type IS XPathNodeType::NODE_TYPE_TEST) {
         return NodeTest->value IS "node";
      }

      if (NodeTest->type IS XPathNodeType::WILDCARD) return true;

      if (NodeTest->type IS XPathNodeType::NAME_TEST) {
         std::string_view test_name = NodeTest->value;
         if (test_name.empty()) return false;

         std::string_view attribute_name = Attribute->Name;

         std::string_view expected_prefix;
         std::string_view expected_local = test_name;

         if (auto colon = test_name.find(':'); colon != std::string::npos) {
            expected_prefix = test_name.substr(0, colon);
            expected_local = test_name.substr(colon + 1);
         }

         std::string_view candidate_prefix;
         std::string_view candidate_local = attribute_name;

         if (auto colon = attribute_name.find(':'); colon != std::string::npos) {
            candidate_prefix = attribute_name.substr(0, colon);
            candidate_local = attribute_name.substr(colon + 1);
         }

         bool wildcard_local = expected_local.find('*') != std::string::npos;
         bool local_matches = wildcard_local ? pf::wildcmp(expected_local, candidate_local) : pf::iequals(expected_local, candidate_local);
         if (!local_matches) return false;

         if ((xml->Flags & XMF::NAMESPACE_AWARE) != XMF::NIL) {
            bool wildcard_prefix = (!expected_prefix.empty()) and (expected_prefix IS "*");
            if (wildcard_prefix) return true;

            if (!expected_prefix.empty()) {
               auto expected_hash = resolve_namespace(expected_prefix, Candidate);
               if (!expected_hash) return false;
               if (candidate_prefix.empty()) return false;
               auto candidate_hash = resolve_namespace(candidate_prefix, Candidate);
               if (!candidate_hash) return false;
               return *candidate_hash IS *expected_hash;
            }

            return candidate_prefix.empty();
         }

         return pf::iequals(test_name, attribute_name);
      }

      return false;
   }

   if (NodeTest->type IS XPathNodeType::NODE_TYPE_TEST) {
      if (NodeTest->value IS "node") return true;
      if (!Candidate) return false;

      if (NodeTest->value IS "text") {
         if (!Candidate->isContent()) return false;
         return ((Candidate->Flags & (XTF::COMMENT | XTF::INSTRUCTION | XTF::NOTATION)) IS XTF::NIL);
      }

      if (NodeTest->value IS "comment") {
         bool has_comment_flag = Candidate ? (((Candidate->Flags & XTF::COMMENT) IS XTF::NIL) ? false : true) : false;
         return Candidate and has_comment_flag;
      }

      return false;
   }

   if (NodeTest->type IS XPathNodeType::PROCESSING_INSTRUCTION_TEST) {
      if (!Candidate) return false;
      if ((Candidate->Flags & XTF::INSTRUCTION) IS XTF::NIL) return false;

      if (NodeTest->value.empty()) return true;

      std::string_view candidate_name;
      if (!Candidate->Attribs.empty()) candidate_name = Candidate->Attribs[0].Name;

      if (!candidate_name.empty() and (candidate_name.front() IS '?')) candidate_name.remove_prefix(1);
      if (candidate_name.empty()) return false;

      std::string candidate_target(candidate_name);
      return pf::iequals(candidate_target, NodeTest->value);
   }

   if (!Candidate) return false;

   if (NodeTest->type IS XPathNodeType::WILDCARD) return Candidate->isTag();

   if (NodeTest->type IS XPathNodeType::NAME_TEST) {
      std::string_view test_name = NodeTest->value;
      if (test_name.empty()) return false;

      std::string_view candidate_name = Candidate->name();

      if ((xml->Flags & XMF::NAMESPACE_AWARE) != XMF::NIL) {
         std::string_view expected_prefix;
         std::string_view expected_local = test_name;

         if (auto colon = test_name.find(':'); colon != std::string::npos) {
            expected_prefix = test_name.substr(0, colon);
            expected_local = test_name.substr(colon + 1);
         }

         std::string_view candidate_prefix;
         std::string_view candidate_local = candidate_name;

         if (auto colon = candidate_name.find(':'); colon != std::string::npos) {
            candidate_prefix = candidate_name.substr(0, colon);
            candidate_local = candidate_name.substr(colon + 1);
         }

         bool wildcard_local = expected_local.find('*') != std::string::npos;
         bool name_matches = wildcard_local ? pf::wildcmp(expected_local, candidate_local) : pf::iequals(expected_local, candidate_local);
         if (!name_matches) return false;

         if (!expected_prefix.empty()) {
            bool wildcard_prefix = expected_prefix IS "*";
            if (wildcard_prefix) return Candidate->isTag();

            auto expected_hash = resolve_namespace(expected_prefix, Candidate);
            if (!expected_hash) return false;
            return Candidate->NamespaceID IS *expected_hash;
         }

         auto default_hash = resolve_namespace(std::string_view(), Candidate);
         uint32_t expected_namespace = default_hash ? *default_hash : 0u;
         return Candidate->NamespaceID IS expected_namespace;
      }

      if (test_name.find('*') != std::string::npos) return pf::wildcmp(test_name, candidate_name);

      return pf::iequals(test_name, candidate_name);
   }

   return false;
}

XPathEvaluator::PredicateResult XPathEvaluator::evaluate_predicate(const XPathNode *PredicateNode, uint32_t CurrentPrefix) {
   if ((!PredicateNode) or (PredicateNode->type != XPathNodeType::PREDICATE)) {
      return PredicateResult::UNSUPPORTED;
   }

   if (PredicateNode->child_count() IS 0) return PredicateResult::UNSUPPORTED;

   const XPathNode *expression = PredicateNode->get_child(0);
   if (!expression) return PredicateResult::UNSUPPORTED;

   if (expression->type IS XPathNodeType::BINARY_OP) {
      auto *candidate = context.context_node;
      if (!candidate) return PredicateResult::NO_MATCH;

      const std::string &operation = expression->value;

      if (operation IS "attribute-exists") {
         if (expression->child_count() IS 0) return PredicateResult::UNSUPPORTED;

         const XPathNode *name_node = expression->get_child(0);
         if (!name_node) return PredicateResult::UNSUPPORTED;

         const std::string &attribute_name = name_node->value;

         if (attribute_name IS "*") {
            return (candidate->Attribs.size() > 1) ? PredicateResult::MATCH : PredicateResult::NO_MATCH;
         }

         for (int index = 1; index < std::ssize(candidate->Attribs); ++index) {
            auto &attrib = candidate->Attribs[index];
            if (pf::iequals(attrib.Name, attribute_name)) return PredicateResult::MATCH;
         }

         return PredicateResult::NO_MATCH;
      }

      if (operation IS "attribute-equals") {
         if (expression->child_count() < 2) return PredicateResult::UNSUPPORTED;

         const XPathNode *name_node = expression->get_child(0);
         const XPathNode *value_node = expression->get_child(1);
         if ((!name_node) or (!value_node)) return PredicateResult::UNSUPPORTED;

         const std::string &attribute_name = name_node->value;

         std::string attribute_value;
         bool wildcard_value = false;

         if (value_node->type IS XPathNodeType::LITERAL) {
            attribute_value = value_node->value;
            wildcard_value = attribute_value.find('*') != std::string::npos;
         }
         else {
            bool saved_expression_unsupported = expression_unsupported;
            auto evaluated_value = evaluate_expression(value_node, CurrentPrefix);
            bool evaluation_failed = expression_unsupported;
            expression_unsupported = saved_expression_unsupported;
            if (evaluation_failed) return PredicateResult::NO_MATCH;

            attribute_value = evaluated_value.to_string();
            wildcard_value = attribute_value.find('*') != std::string::npos;
         }

         bool wildcard_name = attribute_name.find('*') != std::string::npos;

         for (int index = 1; index < std::ssize(candidate->Attribs); ++index) {
            auto &attrib = candidate->Attribs[index];

            bool name_matches;
            if (attribute_name IS "*") name_matches = true;
            else if (wildcard_name) name_matches = pf::wildcmp(attribute_name, attrib.Name);
            else name_matches = pf::iequals(attrib.Name, attribute_name);

            if (!name_matches) continue;

            bool value_matches;
            if (wildcard_value) value_matches = pf::wildcmp(attribute_value, attrib.Value);
            else value_matches = pf::iequals(attrib.Value, attribute_value);

            if (value_matches) return PredicateResult::MATCH;
         }

         return PredicateResult::NO_MATCH;
      }

      if (operation IS "content-equals") {
         if (expression->child_count() IS 0) return PredicateResult::UNSUPPORTED;

         const XPathNode *value_node = expression->get_child(0);
         if (!value_node) return PredicateResult::UNSUPPORTED;

         std::string expected;
         bool wildcard_value = false;

         if (value_node->type IS XPathNodeType::LITERAL) {
            expected = value_node->value;
            wildcard_value = expected.find('*') != std::string::npos;
         }
         else {
            bool saved_expression_unsupported = expression_unsupported;
            auto evaluated_value = evaluate_expression(value_node, CurrentPrefix);
            bool evaluation_failed = expression_unsupported;
            expression_unsupported = saved_expression_unsupported;
            if (evaluation_failed) return PredicateResult::NO_MATCH;

            expected = evaluated_value.to_string();
            wildcard_value = expected.find('*') != std::string::npos;
         }

         if (!candidate->Children.empty()) {
            auto &first_child = candidate->Children[0];
            if ((!first_child.Attribs.empty()) and (first_child.Attribs[0].isContent())) {
               const std::string &content = first_child.Attribs[0].Value;
               if (wildcard_value) {
                  auto match = pf::wildcmp(expected, content) ? PredicateResult::MATCH : PredicateResult::NO_MATCH;
                  return match;
               }
               else return pf::iequals(content, expected) ? PredicateResult::MATCH : PredicateResult::NO_MATCH;
            }
         }

         return PredicateResult::NO_MATCH;
      }
   }

   auto result_value = evaluate_expression(expression, CurrentPrefix);

   if (expression_unsupported) {
      expression_unsupported = false;
      return PredicateResult::UNSUPPORTED;
   }

   if (result_value.type IS XPathValueType::NodeSet) {
      return result_value.node_set.empty() ? PredicateResult::NO_MATCH : PredicateResult::MATCH;
   }

   if (result_value.type IS XPathValueType::Boolean) {
      return result_value.to_boolean() ? PredicateResult::MATCH : PredicateResult::NO_MATCH;
   }

   if (result_value.type IS XPathValueType::String) {
      return result_value.to_string().empty() ? PredicateResult::NO_MATCH : PredicateResult::MATCH;
   }

   if (result_value.type IS XPathValueType::Number) {
      double expected = result_value.to_number();
      if (std::isnan(expected)) return PredicateResult::NO_MATCH;

      double integral_part = 0.0;
      double fractional = std::modf(expected, &integral_part);
      if (fractional != 0.0) return PredicateResult::NO_MATCH;
      if (integral_part < 1.0) return PredicateResult::NO_MATCH;

      return (context.position IS size_t(integral_part)) ? PredicateResult::MATCH : PredicateResult::NO_MATCH;
   }

   return PredicateResult::UNSUPPORTED;
}

//********************************************************************************************************************
// Function and Expression Evaluation (Phase 3 of AST_PLAN.md)

namespace {

std::string node_set_string_value(const XPathValue &Value, size_t Index)
{
   if (Value.node_set_string_override.has_value() and (Index IS 0)) {
      return *Value.node_set_string_override;
   }

   if (Index < Value.node_set_string_values.size()) {
      return Value.node_set_string_values[Index];
   }

   if (Index >= Value.node_set.size()) return std::string();

   return XPathValue::node_string_value(Value.node_set[Index]);
}

double node_set_number_value(const XPathValue &Value, size_t Index)
{
   std::string str = node_set_string_value(Value, Index);
   if (str.empty()) return std::numeric_limits<double>::quiet_NaN();

   return XPathValue::string_to_number(str);
}

std::optional<XPathValue> promote_value_comparison_operand(const XPathValue &Value)
{
   if (Value.type IS XPathValueType::NodeSet) {
      if (Value.node_set.empty()) return std::nullopt;
      return XPathValue(Value.to_string());
   }

   return Value;
}

bool compare_xpath_values(const XPathValue &left_value, const XPathValue &right_value)
{
   auto left_type = left_value.type;
   auto right_type = right_value.type;

   if ((left_type IS XPathValueType::Boolean) or (right_type IS XPathValueType::Boolean)) {
      bool left_boolean = left_value.to_boolean();
      bool right_boolean = right_value.to_boolean();
      return left_boolean IS right_boolean;
   }

   if ((left_type IS XPathValueType::Number) or (right_type IS XPathValueType::Number)) {
      if ((left_type IS XPathValueType::NodeSet) or (right_type IS XPathValueType::NodeSet)) {
         const XPathValue &node_value = (left_type IS XPathValueType::NodeSet) ? left_value : right_value;
         const XPathValue &number_value = (left_type IS XPathValueType::NodeSet) ? right_value : left_value;

         double comparison_number = number_value.to_number();
         if (std::isnan(comparison_number)) return false;

         for (size_t index = 0; index < node_value.node_set.size(); ++index) {
            double node_number = node_set_number_value(node_value, index);
            if (std::isnan(node_number)) continue;
            if (numeric_equal(node_number, comparison_number)) return true;
         }

         return false;
      }

      double left_number = left_value.to_number();
      double right_number = right_value.to_number();
      return numeric_equal(left_number, right_number);
   }

   if ((left_type IS XPathValueType::NodeSet) or (right_type IS XPathValueType::NodeSet)) {
      if ((left_type IS XPathValueType::NodeSet) and (right_type IS XPathValueType::NodeSet)) {
         for (size_t left_index = 0; left_index < left_value.node_set.size(); ++left_index) {
            std::string left_string = node_set_string_value(left_value, left_index);

            for (size_t right_index = 0; right_index < right_value.node_set.size(); ++right_index) {
               std::string right_string = node_set_string_value(right_value, right_index);
               if (left_string.compare(right_string) IS 0) return true;
            }
         }

         return false;
      }

      const XPathValue &node_value = (left_type IS XPathValueType::NodeSet) ? left_value : right_value;
      const XPathValue &string_value = (left_type IS XPathValueType::NodeSet) ? right_value : left_value;

      std::string comparison_string = string_value.to_string();

      for (size_t index = 0; index < node_value.node_set.size(); ++index) {
         std::string node_string = node_set_string_value(node_value, index);
         if (node_string.compare(comparison_string) IS 0) return true;
      }

      return false;
   }

   std::string left_string = left_value.to_string();
   std::string right_string = right_value.to_string();
   return left_string.compare(right_string) IS 0;
}

bool compare_xpath_relational(const XPathValue &left_value,
                              const XPathValue &right_value,
                              RelationalOperator Operation)
{
   auto left_type = left_value.type;
   auto right_type = right_value.type;

   if ((left_type IS XPathValueType::NodeSet) or (right_type IS XPathValueType::NodeSet)) {
      if ((left_type IS XPathValueType::NodeSet) and (right_type IS XPathValueType::NodeSet)) {
         for (size_t left_index = 0; left_index < left_value.node_set.size(); ++left_index) {
            double left_number = node_set_number_value(left_value, left_index);
            if (std::isnan(left_number)) continue;

            for (size_t right_index = 0; right_index < right_value.node_set.size(); ++right_index) {
               double right_number = node_set_number_value(right_value, right_index);
               if (std::isnan(right_number)) continue;
               if (numeric_compare(left_number, right_number, Operation)) return true;
            }
         }

         return false;
      }

      const XPathValue &node_value = (left_type IS XPathValueType::NodeSet) ? left_value : right_value;
      const XPathValue &other_value = (left_type IS XPathValueType::NodeSet) ? right_value : left_value;

      if (other_value.type IS XPathValueType::Boolean) {
         bool node_boolean = node_value.to_boolean();
         bool other_boolean = other_value.to_boolean();
         double node_number = node_boolean ? 1.0 : 0.0;
         double other_number = other_boolean ? 1.0 : 0.0;
         return numeric_compare(node_number, other_number, Operation);
      }

      double other_number = other_value.to_number();
      if (std::isnan(other_number)) return false;

      for (size_t index = 0; index < node_value.node_set.size(); ++index) {
         double node_number = node_set_number_value(node_value, index);
         if (std::isnan(node_number)) continue;
         if (numeric_compare(node_number, other_number, Operation)) return true;
      }

      return false;
   }

   double left_number = left_value.to_number();
   double right_number = right_value.to_number();
   return numeric_compare(left_number, right_number, Operation);
}

} // namespace

std::vector<XMLTag *> XPathEvaluator::collect_step_results(const std::vector<AxisMatch> &ContextNodes,
                                                                 const std::vector<const XPathNode *> &Steps,
                                                                 size_t StepIndex,
                                                                 uint32_t CurrentPrefix,
                                                                 bool &Unsupported)
{
   std::vector<XMLTag *> results;

   if (Unsupported) return results;

   if (StepIndex >= Steps.size()) {
      for (auto &entry : ContextNodes) results.push_back(entry.node);
      return results;
   }

   auto step_node = Steps[StepIndex];
   if ((!step_node) or (step_node->type != XPathNodeType::STEP)) {
      Unsupported = true;
      return results;
   }

   const XPathNode *axis_node = nullptr;
   const XPathNode *node_test = nullptr;
   std::vector<const XPathNode *> predicate_nodes;

   for (size_t index = 0; index < step_node->child_count(); ++index) {
      auto *child = step_node->get_child(index);
      if (!child) continue;

      if (child->type IS XPathNodeType::AXIS_SPECIFIER) axis_node = child;
      else if (child->type IS XPathNodeType::PREDICATE) predicate_nodes.push_back(child);
      else if ((!node_test) and ((child->type IS XPathNodeType::NAME_TEST) or
                                 (child->type IS XPathNodeType::WILDCARD) or
                                 (child->type IS XPathNodeType::NODE_TYPE_TEST))) node_test = child;
   }

   AxisType axis = AxisType::CHILD;
   if (axis_node) axis = AxisEvaluator::parse_axis_name(axis_node->value);

   bool is_last_step = (StepIndex + 1 >= Steps.size());

   for (auto &context_entry : ContextNodes) {
      auto axis_matches = dispatch_axis(axis, context_entry.node, context_entry.attribute);

      std::vector<AxisMatch> filtered;
      filtered.reserve(axis_matches.size());

      for (auto &match : axis_matches) {
         if (!match_node_test(node_test, axis, match.node, match.attribute, CurrentPrefix)) continue;
         filtered.push_back(match);
      }

      if (filtered.empty()) continue;

      for (auto *predicate_node : predicate_nodes) {
         std::vector<AxisMatch> passed;
         passed.reserve(filtered.size());

         for (size_t index = 0; index < filtered.size(); ++index) {
            auto &match = filtered[index];
            push_context(match.node, index + 1, filtered.size(), match.attribute);
            auto predicate_result = evaluate_predicate(predicate_node, CurrentPrefix);
            pop_context();

            if (predicate_result IS PredicateResult::UNSUPPORTED) {
               Unsupported = true;
               return {};
            }

            if (predicate_result IS PredicateResult::MATCH) passed.push_back(match);
         }

         filtered.swap(passed);

         if (filtered.empty()) break;
      }

      if (filtered.empty()) continue;

      if (is_last_step) {
         for (auto &match : filtered) results.push_back(match.node);
         continue;
      }

      std::vector<AxisMatch> next_context;
      next_context.reserve(filtered.size());
      for (auto &match : filtered) next_context.push_back(match);

      auto child_results = collect_step_results(next_context, Steps, StepIndex + 1, CurrentPrefix, Unsupported);
      if (Unsupported) return {};
      results.insert(results.end(), child_results.begin(), child_results.end());
   }

   return results;
}

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

      for (auto *candidate : node_results) {
         if (!candidate) continue;

         auto matches = dispatch_axis(AxisType::ATTRIBUTE, candidate);
         for (auto &match : matches) {
            if (!match.attribute) continue;
            if (!match_node_test(attribute_test, AxisType::ATTRIBUTE, match.node, match.attribute, CurrentPrefix)) continue;
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

XPathValue XPathEvaluator::evaluate_expression(const XPathNode *ExprNode, uint32_t CurrentPrefix) {
   if (!ExprNode) {
      expression_unsupported = true;
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

   if (ExprNode->type IS XPathNodeType::FOR_EXPRESSION) {
      if (ExprNode->child_count() < 2) {
         expression_unsupported = true;
         return XPathValue();
      }

      const XPathNode *sequence_node = ExprNode->get_child(0);
      const XPathNode *return_node = ExprNode->get_child(1);
      const std::string variable_name = ExprNode->value;

      if ((!sequence_node) or (!return_node) or variable_name.empty()) {
         expression_unsupported = true;
         return XPathValue();
      }

      auto sequence_value = evaluate_expression(sequence_node, CurrentPrefix);
      if (expression_unsupported) return XPathValue();

      if (sequence_value.type != XPathValueType::NodeSet) {
         expression_unsupported = true;
         return XPathValue();
      }

      bool had_previous = false;
      XPathValue previous_value;
      auto existing = context.variables.find(variable_name);
      if (existing != context.variables.end()) {
         had_previous = true;
         previous_value = existing->second;
      }

      auto restore_variable = [this, variable_name, had_previous, &previous_value]() {
         if (had_previous) context.variables[variable_name] = previous_value;
         else context.variables.erase(variable_name);
      };

      std::vector<XMLTag *> combined_nodes;
      std::vector<std::string> combined_strings;
      std::vector<const XMLAttrib *> combined_attributes;
      std::optional<std::string> combined_override;

      size_t sequence_size = sequence_value.node_set.size();

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
         if (index < sequence_value.node_set_string_values.size()) {
            item_string = sequence_value.node_set_string_values[index];
         }
         else if (item_node) item_string = XPathValue::node_string_value(item_node);

         bound_value.node_set_string_values.push_back(item_string);
         bound_value.node_set_string_override = item_string;

         context.variables[variable_name] = bound_value;

         push_context(item_node, index + 1, sequence_size, item_attribute);
         
         auto iteration_value = evaluate_expression(return_node, CurrentPrefix);
         pop_context();
         if (expression_unsupported) {
            restore_variable();
            return XPathValue();
         }

         if (iteration_value.type != XPathValueType::NodeSet) {
            restore_variable();
            expression_unsupported = true;
            return XPathValue();
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
            if (node_index < iteration_value.node_set_string_values.size()) {
               node_string = iteration_value.node_set_string_values[node_index];
            }
            else if (node) node_string = XPathValue::node_string_value(node);

            combined_strings.push_back(node_string);

            if (!combined_override.has_value()) {
               if (iteration_value.node_set_string_override.has_value()) combined_override = iteration_value.node_set_string_override;
               else combined_override = node_string;
            }
         }
      }

      restore_variable();

      XPathValue result;
      result.type = XPathValueType::NodeSet;
      result.node_set = std::move(combined_nodes);
      result.node_set_string_values = std::move(combined_strings);
      result.node_set_attributes = std::move(combined_attributes);
      if (combined_override.has_value()) result.node_set_string_override = combined_override;
      else result.node_set_string_override.reset();
      return result;
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

      auto tags = xml->getInsert(candidate, xml->Cursor);
      if (!tags) {
         pop_context();
         continue;
      }

      xml->CursorTags = tags;
      if (entry.attribute) xml->Attrib = entry.attribute->Name;
      else xml->Attrib.clear();

      if (!xml->Callback.defined()) {
         pop_context();
         return ERR::Okay;
      }

      push_cursor_state();

      ERR callback_error = ERR::Okay;
      if (xml->Callback.isC()) {
         auto routine = (ERR (*)(extXML *, int, CSTRING, APTR))xml->Callback.Routine;
         callback_error = routine(xml, candidate->ID, xml->Attrib.empty() ? nullptr : xml->Attrib.c_str(), xml->Callback.Meta);
      }
      else if (xml->Callback.isScript()) {
         if (sc::Call(xml->Callback, std::to_array<ScriptArg>({
            { "XML",  xml, FD_OBJECTPTR },
            { "Tag",  candidate->ID },
            { "Attrib", xml->Attrib.empty() ? CSTRING(nullptr) : xml->Attrib.c_str() }
         }), callback_error) != ERR::Okay) callback_error = ERR::Terminate;
      }
      else callback_error = ERR::InvalidValue;

      pop_cursor_state();
      pop_context();

      matched = true;

      if (callback_error IS ERR::Terminate) return ERR::Terminate;
      if (callback_error != ERR::Okay) return callback_error;
   }

   xml->Attrib.clear();
   if (matched) return ERR::Okay;
   return ERR::Search;
}

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
   if (expression_unsupported) return ERR::Failed;

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

   return function_library.call_function(function_name, args, context);
}

std::string XPathEvaluator::build_ast_signature(const XPathNode *Node) const
{
   if (!Node) return std::string("#");

   std::string signature;
   signature.reserve(16);

   signature += '(';
   signature += std::to_string(int(Node->type));
   signature += '|';
   signature += Node->value;
   signature += ':';

   for (size_t index = 0; index < Node->child_count(); ++index) {
      auto child = Node->get_child(index);
      signature += build_ast_signature(child);
      signature += ',';
   }

   signature += ')';

   return signature;
}

//********************************************************************************************************************
// Public method for AST Evaluation

ERR XPathEvaluator::find_tag(const CompiledXPath &CompiledPath, uint32_t CurrentPrefix)
{
   if (!CompiledPath.isValid()) return ERR::Syntax;

   // Reset the evaluator state
   axis_evaluator.reset_namespace_nodes();
   arena.reset();
   
   // Ensure the tag ID and ParentID values are defined

   (void)xml->getMap();

   return evaluate_ast(CompiledPath.getAST(), CurrentPrefix);
}

//********************************************************************************************************************
// Public method to evaluate complete XPath expressions and return computed values

ERR XPathEvaluator::evaluate_xpath_expression(const CompiledXPath &CompiledPath, XPathValue &Result, uint32_t CurrentPrefix)
{
   if (!CompiledPath.isValid()) return ERR::Syntax;
   
   // Ensure the tag ID and ParentID values are defined

   (void)xml->getMap();

   // Set context to document root if not already set

   if (!context.context_node) push_context(&xml->Tags[0], 1, 1);

   // Evaluate the compiled AST and return the XPathValue directly

   expression_unsupported = false;

   const XPathNode *expression_node = CompiledPath.getAST();
   if (expression_node and (expression_node->type IS XPathNodeType::EXPRESSION)) {
      if (expression_node->child_count() > 0) expression_node = expression_node->get_child(0);
      else expression_node = nullptr;
   }

   Result = evaluate_expression(expression_node, CurrentPrefix);

   if (expression_unsupported) return ERR::Syntax;
   else return ERR::Okay;
}

