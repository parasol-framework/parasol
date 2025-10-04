//********************************************************************************************************************
// XPath Evaluator Predicate Handling

#include "xpath_evaluator.h"
#include "xpath_evaluator_detail.h"

#include <cmath>
#include <optional>
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

   for (size_t index = 1; index < candidate->Attribs.size(); ++index) {
      auto &attribute = candidate->Attribs[index];
      if (pf::iequals(attribute.Name, attribute_name)) return PredicateResult::MATCH;
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

   std::string attribute_name = name_node->value;
   if (attribute_name.empty()) return PredicateResult::NO_MATCH;

   std::string expected_value;
   bool wildcard_value = false;

   if (value_node->type IS XPathNodeType::LITERAL) {
      expected_value = value_node->value;
      wildcard_value = expected_value.find('*') != std::string::npos;
   }
   else {
      bool saved_expression_unsupported = expression_unsupported;
      auto evaluated_value = evaluate_expression(value_node, CurrentPrefix);
      bool evaluation_failed = expression_unsupported;
      expression_unsupported = saved_expression_unsupported;

      if (evaluation_failed) return PredicateResult::NO_MATCH;
      expected_value = evaluated_value.to_string();
      wildcard_value = expected_value.find('*') != std::string::npos;
   }

   bool wildcard_name = attribute_name.find('*') != std::string::npos;

   for (size_t index = 1; index < candidate->Attribs.size(); ++index) {
      auto &attribute = candidate->Attribs[index];

      bool name_matches;
      if (attribute_name IS "*") name_matches = true;
      else if (wildcard_name) name_matches = pf::wildcmp(attribute_name, attribute.Name);
      else name_matches = pf::iequals(attribute.Name, attribute_name);

      if (!name_matches) continue;

      if (wildcard_value) {
         return pf::wildcmp(expected_value, attribute.Value) ? PredicateResult::MATCH : PredicateResult::NO_MATCH;
      }

      if (pf::iequals(expected_value, attribute.Value)) return PredicateResult::MATCH;
      return PredicateResult::NO_MATCH;
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

XPathEvaluator::PredicateResult XPathEvaluator::evaluate_predicate(const XPathNode *PredicateNode, uint32_t CurrentPrefix)
{
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
