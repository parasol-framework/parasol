#include "xpath_evaluator.h"
#include "xpath_evaluator_detail.h"
#include "xpath_functions.h"
#include "xpath_axis.h"

#include "../xml/schema/schema_types.h"
#include "../xml/xml.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace {

class ContextGuard {
   private:
   XPathEvaluator * evaluator;
   bool active;

   public:
   ContextGuard(XPathEvaluator &Evaluator, XMLTag *Node, size_t Position, size_t Size, const XMLAttrib *Attribute)
      : evaluator(&Evaluator), active(true)
   {
      evaluator->push_context(Node, Position, Size, Attribute);
   }

   ContextGuard(ContextGuard &&Other) noexcept
      : evaluator(Other.evaluator), active(Other.active)
   {
      Other.evaluator = nullptr;
      Other.active = false;
   }

   ContextGuard & operator=(ContextGuard &&Other) noexcept
   {
      if (this IS &Other) return *this;

      if (active and evaluator) evaluator->pop_context();

      evaluator = Other.evaluator;
      active = Other.active;

      Other.evaluator = nullptr;
      Other.active = false;

      return *this;
   }

   ContextGuard(const ContextGuard &) = delete;
   ContextGuard & operator=(const ContextGuard &) = delete;

   ~ContextGuard()
   {
      if (active and evaluator) evaluator->pop_context();
   }
};

class CursorGuard {
   private:
   XPathEvaluator * evaluator;
   bool active;

   public:
   explicit CursorGuard(XPathEvaluator &Evaluator)
      : evaluator(&Evaluator), active(true)
   {
      evaluator->push_cursor_state();
   }

   CursorGuard(CursorGuard &&Other) noexcept
      : evaluator(Other.evaluator), active(Other.active)
   {
      Other.evaluator = nullptr;
      Other.active = false;
   }

   CursorGuard & operator=(CursorGuard &&Other) noexcept
   {
      if (this IS &Other) return *this;

      if (active and evaluator) evaluator->pop_cursor_state();

      evaluator = Other.evaluator;
      active = Other.active;

      Other.evaluator = nullptr;
      Other.active = false;

      return *this;
   }

   CursorGuard(const CursorGuard &) = delete;
   CursorGuard & operator=(const CursorGuard &) = delete;

   ~CursorGuard()
   {
      if (active and evaluator) evaluator->pop_cursor_state();
   }
};

} // namespace

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


//********************************************************************************************************************
// AST Evaluation Methods

// Dispatch AST nodes to the appropriate evaluation routine.
ERR XPathEvaluator::evaluate_ast(const XPathNode *Node, uint32_t CurrentPrefix) 
{
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
      case XPathNodeType::LET_EXPRESSION:
      case XPathNodeType::FLWOR_EXPRESSION:
      case XPathNodeType::QUANTIFIED_EXPRESSION:
         return evaluate_top_level_expression(Node, CurrentPrefix);

      default:
         return ERR::Failed;
   }
}

// Execute a full location path expression, managing implicit root handling and cursor updates.
// Returns ERR::Search if no matches were found.

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
void XPathEvaluator::expand_axis_candidates(const AxisMatch &ContextEntry, AxisType Axis,
   const XPathNode *NodeTest, uint32_t CurrentPrefix, std::vector<AxisMatch> &FilteredMatches)
{
   FilteredMatches.clear();

   auto *context_node = ContextEntry.node;
   const XMLAttrib *context_attribute = ContextEntry.attribute;

   if ((!context_attribute) and context_node and context.attribute_node and (context_node IS context.context_node)) {
      context_attribute = context.attribute_node;
   }

   auto axis_matches = dispatch_axis(Axis, context_node, context_attribute);
   if (FilteredMatches.capacity() < axis_matches.size()) {
      FilteredMatches.reserve(axis_matches.size());
   }

   for (auto &match : axis_matches) {
      if (!match_node_test(NodeTest, Axis, match.node, match.attribute, CurrentPrefix)) continue;
      FilteredMatches.push_back(match);
   }
}

ERR XPathEvaluator::apply_predicates_to_candidates(const std::vector<const XPathNode *> &PredicateNodes,
   uint32_t CurrentPrefix, std::vector<AxisMatch> &Candidates, std::vector<AxisMatch> &ScratchBuffer)
{
   for (auto *predicate_node : PredicateNodes) {
      ScratchBuffer.clear();
      if (ScratchBuffer.capacity() < Candidates.size()) {
         ScratchBuffer.reserve(Candidates.size());
      }

      for (size_t index = 0; index < Candidates.size(); ++index) {
         auto &match = Candidates[index];
         ContextGuard context_guard(*this, match.node, index + 1, Candidates.size(), match.attribute);

         auto predicate_result = evaluate_predicate(predicate_node, CurrentPrefix);

         if (predicate_result IS PredicateResult::UNSUPPORTED) {
            return ERR::Failed;
         }

         if (predicate_result IS PredicateResult::MATCH) ScratchBuffer.push_back(match);
      }

      Candidates.swap(ScratchBuffer);
      if (Candidates.empty()) break;
   }

   return ERR::Okay;
}

ERR XPathEvaluator::invoke_callback(XMLTag *Node, const XMLAttrib *Attribute, bool &Matched, bool &ShouldTerminate)
{
   ShouldTerminate = false;
   if (!Node) return ERR::Okay;

   auto tags = xml->getInsert(Node, xml->Cursor);
   if (!tags) return ERR::Okay;

   xml->CursorTags = tags;

   if (Attribute) xml->Attrib = Attribute->Name;
   else xml->Attrib.clear();

   if (!xml->Callback.defined()) {
      Matched = true;
      ShouldTerminate = true;
      return ERR::Okay;
   }

   CursorGuard cursor_guard(*this);

   ERR callback_error = ERR::Okay;
   if (xml->Callback.isC()) {
      auto routine = (ERR (*)(extXML *, int, CSTRING, APTR))xml->Callback.Routine;
      callback_error = routine(xml, Node->ID, xml->Attrib.empty() ? nullptr : xml->Attrib.c_str(), xml->Callback.Meta);
   }
   else if (xml->Callback.isScript()) {
      if (sc::Call(xml->Callback, std::to_array<ScriptArg>({
         { "XML",  xml, FD_OBJECTPTR },
         { "Tag",  Node->ID },
         { "Attrib", xml->Attrib.empty() ? CSTRING(nullptr) : xml->Attrib.c_str() }
      }), callback_error) != ERR::Okay) callback_error = ERR::Terminate;
   }
   else callback_error = ERR::InvalidValue;

   Matched = true;

   if (callback_error IS ERR::Terminate) return ERR::Terminate;
   if (callback_error != ERR::Okay) return callback_error;

   return ERR::Okay;
}

ERR XPathEvaluator::process_step_matches(const std::vector<AxisMatch> &Matches, AxisType Axis, bool IsLastStep,
   bool &Matched, std::vector<AxisMatch> &NextContext, bool &ShouldTerminate)
{
   ShouldTerminate = false;

   for (size_t index = 0; index < Matches.size(); ++index) {
      auto &match = Matches[index];
      auto *candidate = match.node;

      ContextGuard context_guard(*this, candidate, index + 1, Matches.size(), match.attribute);

      if (Axis IS AxisType::ATTRIBUTE) {
         AxisMatch next_match{};
         next_match.node = candidate;
         next_match.attribute = match.attribute;

         if (!next_match.node or !next_match.attribute) continue;

         if (IsLastStep) {
            ShouldTerminate = false;
            auto callback_error = invoke_callback(next_match.node, next_match.attribute, Matched, ShouldTerminate);

            if (callback_error IS ERR::Terminate) return ERR::Terminate;
            if (callback_error != ERR::Okay) return callback_error;
            if (ShouldTerminate) return ERR::Okay;

            continue;
         }

         NextContext.push_back(next_match);
         continue;
      }

      if (IsLastStep) {
         if (!candidate) continue;

         ShouldTerminate = false;
         auto callback_error = invoke_callback(candidate, nullptr, Matched, ShouldTerminate);

         if (callback_error IS ERR::Terminate) return ERR::Terminate;
         if (callback_error != ERR::Okay) return callback_error;
         if (ShouldTerminate) return ERR::Okay;

         continue;
      }

      if (!candidate) continue;

      NextContext.push_back({ candidate, nullptr });
   }

   return ERR::Okay;
}

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

   std::vector<AxisMatch> axis_candidates;
   axis_candidates.reserve(current_context.size());
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
      next_context.clear();

      for (auto &context_entry : current_context) {
         expand_axis_candidates(context_entry, axis, node_test, CurrentPrefix, axis_candidates);
         if (axis_candidates.empty()) continue;

         auto predicate_error = apply_predicates_to_candidates(predicate_nodes, CurrentPrefix, axis_candidates, predicate_buffer);
         if (predicate_error != ERR::Okay) return predicate_error;
         if (axis_candidates.empty()) continue;

         bool should_terminate = false;
         auto step_error = process_step_matches(axis_candidates, axis, is_last_step, Matched, next_context, should_terminate);
         if (step_error != ERR::Okay) return step_error;
         if (should_terminate) return ERR::Okay;
      }

      current_context.swap(next_context);
   }

   return Matched ? ERR::Okay : ERR::Search;
}

const std::unordered_map<std::string_view, XPathEvaluator::PredicateHandler> &XPathEvaluator::predicate_handler_map() const
{
   static const std::unordered_map<std::string_view, PredicateHandler> handlers = {
      { "attribute-exists", &XPathEvaluator::handle_attribute_exists_predicate },
      { "attribute-equals", &XPathEvaluator::handle_attribute_equals_predicate },
      { "content-equals", &XPathEvaluator::handle_content_equals_predicate }
   };
   return handlers;
}

XPathEvaluator::PredicateResult XPathEvaluator::dispatch_predicate_operation(std::string_view OperationName, const XPathNode *Expression,
   uint32_t CurrentPrefix)
{
   auto &handlers = predicate_handler_map();
   auto it = handlers.find(OperationName);
   if (it IS handlers.end()) return PredicateResult::UNSUPPORTED;

   auto handler = it->second;
   return (this->*handler)(Expression, CurrentPrefix);
}

XPathEvaluator::PredicateResult XPathEvaluator::handle_attribute_exists_predicate(const XPathNode *Expression, uint32_t CurrentPrefix)
{
   (void)CurrentPrefix;

   auto *candidate = context.context_node;
   if (!candidate) return PredicateResult::NO_MATCH;
   if ((!Expression) or (Expression->child_count() IS 0)) return PredicateResult::UNSUPPORTED;

   const XPathNode *name_node = Expression->get_child(0);
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

XPathEvaluator::PredicateResult XPathEvaluator::handle_attribute_equals_predicate(const XPathNode *Expression, uint32_t CurrentPrefix)
{
   auto *candidate = context.context_node;
   if (!candidate) return PredicateResult::NO_MATCH;
   if ((!Expression) or (Expression->child_count() < 2)) return PredicateResult::UNSUPPORTED;

   const XPathNode *name_node = Expression->get_child(0);
   const XPathNode *value_node = Expression->get_child(1);
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

XPathEvaluator::PredicateResult XPathEvaluator::handle_content_equals_predicate(const XPathNode *Expression, uint32_t CurrentPrefix)
{
   auto *candidate = context.context_node;
   if (!candidate) return PredicateResult::NO_MATCH;
   if ((!Expression) or (Expression->child_count() IS 0)) return PredicateResult::UNSUPPORTED;

   const XPathNode *value_node = Expression->get_child(0);
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
            return pf::wildcmp(expected, content) ? PredicateResult::MATCH : PredicateResult::NO_MATCH;
         }
         return pf::iequals(content, expected) ? PredicateResult::MATCH : PredicateResult::NO_MATCH;
      }
   }

   return PredicateResult::NO_MATCH;
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

      auto dispatched = dispatch_predicate_operation(expression->value, expression, CurrentPrefix);
      if (dispatched != PredicateResult::UNSUPPORTED) return dispatched;
   }

   auto result_value = evaluate_expression(expression, CurrentPrefix);

   if (expression_unsupported) {
      expression_unsupported = false;
      return PredicateResult::UNSUPPORTED;
   }

   if (result_value.type IS XPVT::NodeSet) {
      return result_value.node_set.empty() ? PredicateResult::NO_MATCH : PredicateResult::MATCH;
   }

   if (result_value.type IS XPVT::Boolean) {
      return result_value.to_boolean() ? PredicateResult::MATCH : PredicateResult::NO_MATCH;
   }

   if (result_value.type IS XPVT::String) {
      return result_value.to_string().empty() ? PredicateResult::NO_MATCH : PredicateResult::MATCH;
   }

   if (result_value.type IS XPVT::Number) {
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

extXML * XPathEvaluator::resolve_document_for_node(XMLTag *Node) const
{
   if ((!Node) or (!xml)) return nullptr;

   auto &map = xml->getMap();
   auto base = map.find(Node->ID);
   if ((base != map.end()) and (base->second IS Node)) return xml;

   auto registered = xml->DocumentNodeOwners.find(Node);
   if (registered != xml->DocumentNodeOwners.end()) {
      if (auto document = registered->second.lock(); document) return document.get();
   }

   return nullptr;
}

bool XPathEvaluator::is_foreign_document_node(XMLTag *Node) const
{
   auto *document = resolve_document_for_node(Node);
   return (document) and (document != xml);
}

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

      if (filtered.empty()) {
         if ((axis IS AxisType::CHILD) and context_entry.node and (context_entry.node->ParentID IS 0) and
             is_foreign_document_node(context_entry.node)) {
            if (match_node_test(node_test, axis, context_entry.node, context_entry.attribute, CurrentPrefix)) {
               filtered.push_back(context_entry);
            }
         }
      }

      if (filtered.empty()) continue;

      for (auto *predicate_node : predicate_nodes) {
         std::vector<AxisMatch> passed;
         passed.reserve(filtered.size());

         for (size_t index = 0; index < filtered.size(); ++index) {
            auto &match = filtered[index];
            ContextGuard context_guard(*this, match.node, index + 1, filtered.size(), match.attribute);
            auto predicate_result = evaluate_predicate(predicate_node, CurrentPrefix);

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

