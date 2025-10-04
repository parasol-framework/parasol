//********************************************************************************************************************
// XPath Evaluator - Predicate Handling

#include "xpath_evaluator.h"
#include "xpath_evaluator_detail.h"

#include <parasol/modules/xml.h>
#include "../xml.h"

#include <iterator>
#include <string>
#include <unordered_map>

const std::unordered_map<std::string_view, XPathEvaluator::PredicateHandler> &XPathEvaluator::predicate_handler_map() const
{
   static const std::unordered_map<std::string_view, PredicateHandler> handlers = {
      { "attribute-exists", &XPathEvaluator::handle_attribute_exists_predicate },
      { "attribute-equals", &XPathEvaluator::handle_attribute_equals_predicate },
      { "content-equals", &XPathEvaluator::handle_content_equals_predicate }
   };
   return handlers;
}

XPathEvaluator::PredicateResult XPathEvaluator::dispatch_predicate_operation(std::string_view OperationName,
   const XPathNode *Expression, uint32_t CurrentPrefix)
{
   auto &handlers = predicate_handler_map();
   auto it = handlers.find(OperationName);
   if (it IS handlers.end()) return PredicateResult::UNSUPPORTED;

   auto handler = it->second;
   return (this->*handler)(Expression, CurrentPrefix);
}

XPathEvaluator::PredicateResult XPathEvaluator::handle_attribute_exists_predicate(const XPathNode *Expression,
   uint32_t CurrentPrefix)
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

XPathEvaluator::PredicateResult XPathEvaluator::handle_attribute_equals_predicate(const XPathNode *Expression,
   uint32_t CurrentPrefix)
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

XPathEvaluator::PredicateResult XPathEvaluator::handle_content_equals_predicate(const XPathNode *Expression,
   uint32_t CurrentPrefix)
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

   std::string candidate_content;
   candidate_content.reserve(candidate->Attribs.empty() ? 0 : candidate->Attribs[0].Value.size());

   if (candidate->isContent()) {
      candidate_content = candidate->getContent();
   }
   else {
      for (auto &child : candidate->Children) {
         if (!child.isContent()) continue;
         candidate_content += child.getContent();
      }
   }

   if (wildcard_value) {
      return pf::wildcmp(expected, candidate_content) ? PredicateResult::MATCH : PredicateResult::NO_MATCH;
   }

   return pf::iequals(candidate_content, expected) ? PredicateResult::MATCH : PredicateResult::NO_MATCH;
}

XPathEvaluator::PredicateResult XPathEvaluator::evaluate_predicate(const XPathNode *PredicateNode,
   uint32_t CurrentPrefix)
{
   if ((!PredicateNode) or (PredicateNode->child_count() IS 0)) return PredicateResult::UNSUPPORTED;

   const XPathNode *operation_node = PredicateNode->get_child(0);
   if (!operation_node) return PredicateResult::UNSUPPORTED;

   if (operation_node->type IS XPathNodeType::FUNCTION_CALL) {
      auto operation_name = operation_node->value;
      return dispatch_predicate_operation(operation_name, operation_node, CurrentPrefix);
   }

   if ((operation_node->type IS XPathNodeType::EXPRESSION) or (operation_node->type IS XPathNodeType::FILTER) or
       (operation_node->type IS XPathNodeType::BINARY_OP) or (operation_node->type IS XPathNodeType::UNARY_OP) or
       (operation_node->type IS XPathNodeType::NUMBER) or (operation_node->type IS XPathNodeType::STRING)) {
      bool saved_expression_unsupported = expression_unsupported;
      auto value = evaluate_expression(operation_node, CurrentPrefix);
      bool evaluation_failed = expression_unsupported;
      expression_unsupported = saved_expression_unsupported;
      if (evaluation_failed) return PredicateResult::UNSUPPORTED;

      if (value.type IS XPathValueType::Boolean) {
         return value.to_boolean() ? PredicateResult::MATCH : PredicateResult::NO_MATCH;
      }

      if (value.type IS XPathValueType::Number) {
         return value.to_number() != 0.0 ? PredicateResult::MATCH : PredicateResult::NO_MATCH;
      }

      return value.to_string().empty() ? PredicateResult::NO_MATCH : PredicateResult::MATCH;
   }

   return PredicateResult::UNSUPPORTED;
}
