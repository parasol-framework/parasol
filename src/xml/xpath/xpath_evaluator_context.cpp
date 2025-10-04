//********************************************************************************************************************
// XPath Evaluator - Context Management

#include "xpath_evaluator.h"
#include "xpath_evaluator_detail.h"
#include "xpath_axis.h"
#include "xpath_functions.h"

#include <parasol/modules/xml.h>
#include "../xml.h"

ContextGuard::ContextGuard(XPathEvaluator &Evaluator, XMLTag *Node, size_t Position, size_t Size)
{
   evaluator = &Evaluator;
   active = true;
   evaluator->push_context(Node, Position, Size);
}

ContextGuard::ContextGuard(XPathEvaluator &Evaluator, XMLTag *Node, size_t Position, size_t Size, const XMLAttrib *Attribute)
{
   evaluator = &Evaluator;
   active = true;
   evaluator->push_context(Node, Position, Size, Attribute);
}

ContextGuard::ContextGuard(ContextGuard &&Other) noexcept
{
   evaluator = Other.evaluator;
   active = Other.active;
   Other.evaluator = nullptr;
   Other.active = false;
}

ContextGuard & ContextGuard::operator=(ContextGuard &&Other) noexcept
{
   if (this IS &Other) return *this;

   if (active and evaluator) evaluator->pop_context();

   evaluator = Other.evaluator;
   active = Other.active;
   Other.evaluator = nullptr;
   Other.active = false;
   return *this;
}

ContextGuard::~ContextGuard()
{
   if (active and evaluator) evaluator->pop_context();
}

CursorGuard::CursorGuard(XPathEvaluator &Evaluator)
{
   evaluator = &Evaluator;
   active = true;
   evaluator->push_cursor_state();
}

CursorGuard::CursorGuard(CursorGuard &&Other) noexcept
{
   evaluator = Other.evaluator;
   active = Other.active;
   Other.evaluator = nullptr;
   Other.active = false;
}

CursorGuard & CursorGuard::operator=(CursorGuard &&Other) noexcept
{
   if (this IS &Other) return *this;

   if (active and evaluator) evaluator->pop_cursor_state();

   evaluator = Other.evaluator;
   active = Other.active;
   Other.evaluator = nullptr;
   Other.active = false;
   return *this;
}

CursorGuard::~CursorGuard()
{
   if (active and evaluator) evaluator->pop_cursor_state();
}

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
   const std::vector<const XPathNode *> &Steps, size_t StepIndex, uint32_t CurrentPrefix, bool &Unsupported)
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
            ContextGuard guard(*this, match.node, index + 1, filtered.size(), match.attribute);
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
