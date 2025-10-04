//********************************************************************************************************************
// XPath Evaluator Context Utilities

#include "xpath_evaluator.h"
#include "../xml.h"

#include <memory>
#include <vector>

namespace {

class ContextGuard {
   public:
   ContextGuard(XPathEvaluator &Evaluator, XMLTag *Node, size_t Position, size_t Size,
      const XMLAttrib *Attribute)
      : evaluator(Evaluator), active(true)
   {
      evaluator.push_context(Node, Position, Size, Attribute);
   }

   ~ContextGuard()
   {
      if (active) evaluator.pop_context();
   }

   ContextGuard(const ContextGuard &) = delete;
   ContextGuard &operator=(const ContextGuard &) = delete;

   void release()
   {
      if (!active) return;
      evaluator.pop_context();
      active = false;
   }

   private:
   XPathEvaluator &evaluator;
   bool active = false;
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

   xml->CursorTags = (objXML::TAGS *)state.tags;

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

   if (StepIndex >= Steps.size()) return results;

   auto *current_step = Steps[StepIndex];
   if ((!current_step) or (current_step->type != XPathNodeType::STEP)) {
      Unsupported = true;
      return {};
   }

   const XPathNode *axis_node = nullptr;
   const XPathNode *node_test = nullptr;
   std::vector<const XPathNode *> predicate_nodes;
   predicate_nodes.reserve(current_step->child_count());

   for (size_t index = 0; index < current_step->child_count(); ++index) {
      auto *child = current_step->get_child(index);
      if (!child) continue;

      if (child->type IS XPathNodeType::AXIS_SPECIFIER) axis_node = child;
      else if (child->type IS XPathNodeType::PREDICATE) predicate_nodes.push_back(child);
      else if ((!node_test) and ((child->type IS XPathNodeType::NAME_TEST) or
         (child->type IS XPathNodeType::WILDCARD) or (child->type IS XPathNodeType::NODE_TYPE_TEST))) node_test = child;
   }

   AxisType axis = axis_node ? AxisEvaluator::parse_axis_name(axis_node->value) : AxisType::CHILD;
   bool is_last_step = (StepIndex + 1 >= Steps.size());

   std::vector<AxisMatch> filtered;
   filtered.reserve(ContextNodes.size());

   for (size_t index = 0; index < ContextNodes.size(); ++index) {
      auto &entry = ContextNodes[index];
      std::vector<AxisMatch> axis_matches = dispatch_axis(axis, entry.node, entry.attribute);
      if (axis_matches.empty()) continue;

      filtered.clear();
      filtered.reserve(axis_matches.size());

      for (auto &match : axis_matches) {
         if (!match_node_test(node_test, axis, match.node, match.attribute, CurrentPrefix)) continue;
         filtered.push_back(match);
      }

      if (filtered.empty()) continue;

      for (auto *predicate_node : predicate_nodes) {
         std::vector<AxisMatch> passed;
         passed.reserve(filtered.size());

         for (size_t filtered_index = 0; filtered_index < filtered.size(); ++filtered_index) {
            auto &match = filtered[filtered_index];
            ContextGuard guard(*this, match.node, filtered_index + 1, filtered.size(), match.attribute);
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
