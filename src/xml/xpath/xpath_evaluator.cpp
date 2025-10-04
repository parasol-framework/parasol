//********************************************************************************************************************
// XPath Evaluation Engine - Entry Points
//********************************************************************************************************************

#include "xpath_evaluator.h"
#include "xpath_evaluator_detail.h"
#include "xpath_axis.h"
#include "xpath_functions.h"

#include <parasol/modules/xml.h>
#include "../xml.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

ERR XPathEvaluator::evaluate_location_path(const XPathNode *PathNode, uint32_t CurrentPrefix)
{
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

void XPathEvaluator::record_error(std::string_view Message, bool Force)
{
   expression_unsupported = true;
   if (!xml) return;
   if (Force or xml->ErrorMsg.empty()) xml->ErrorMsg.assign(Message);
}

ERR XPathEvaluator::find_tag(const CompiledXPath &CompiledPath, uint32_t CurrentPrefix)
{
   if (!CompiledPath.isValid()) return ERR::Syntax;

   axis_evaluator.reset_namespace_nodes();
   arena.reset();

   (void)xml->getMap();

   return evaluate_ast(CompiledPath.getAST(), CurrentPrefix);
}

ERR XPathEvaluator::evaluate_xpath_expression(const CompiledXPath &CompiledPath, XPathValue &Result,
   uint32_t CurrentPrefix)
{
   if (!CompiledPath.isValid()) return ERR::Syntax;

   axis_evaluator.reset_namespace_nodes();
   arena.reset();

   auto saved_context = context;
   auto saved_context_stack = context_stack;
   auto saved_cursor_stack = cursor_stack;

   struct ContextRestorer
   {
      XPathEvaluator *evaluator;
      XPathContext previous_context;
      std::vector<XPathContext> previous_context_stack;
      std::vector<CursorState> previous_cursor_stack;

      ~ContextRestorer()
      {
         if (!evaluator) return;
         evaluator->context = previous_context;
         evaluator->context_stack = std::move(previous_context_stack);
         evaluator->cursor_stack = std::move(previous_cursor_stack);
      }
   } restorer{ this, saved_context, saved_context_stack, saved_cursor_stack };

   expression_unsupported = false;

   bool has_external_context = (saved_context.context_node != nullptr) or (saved_context.attribute_node != nullptr);

   if (has_external_context) {
      context = saved_context;
      if (!context.document) context.document = xml;
      context_stack = saved_context_stack;
      cursor_stack = saved_cursor_stack;
   }
   else {
      context = saved_context;
      context.context_node = nullptr;
      context.attribute_node = nullptr;
      context.position = 1;
      context.size = 1;
      context.document = xml;
      context_stack.clear();
      cursor_stack.clear();

      if (!xml->Tags.empty()) push_context(&xml->Tags[0], 1, 1);
      else push_context(nullptr, 1, 1);
   }

   auto ast = CompiledPath.getAST();
   auto value = evaluate_expression(ast, CurrentPrefix);

   if (expression_unsupported) {
      Result = XPathValue();
      return ERR::Failed;
   }

   Result = value;
   return ERR::Okay;
}
