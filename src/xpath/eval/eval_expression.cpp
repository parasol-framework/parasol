
#include "eval.h"
#include "eval_detail.h"
#include "../api/xpath_functions.h"
#include "../../xml/schema/schema_types.h"
#include "../../xml/xml.h"
#include <parasol/strings.hpp>
#include <utility>

bool XPathEvaluator::resolve_variable_value(std::string_view QName, uint32_t CurrentPrefix,
   XPathVal &OutValue, const XPathNode *ReferenceNode)
{
   std::string name(QName);

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
   if (!prolog) return false;

   auto cached_value = prolog_variable_cache.find(name);
   if (cached_value != prolog_variable_cache.end()) {
      OutValue = cached_value->second;
      return true;
   }

   const XQueryVariable *variable = prolog->find_variable(QName);
   if (!variable) {
      auto separator = name.find(':');
      if (separator != std::string::npos) {
         std::string prefix = name.substr(0, separator);
         uint32_t namespace_hash = prolog->resolve_prefix(prefix, context.document);
         if (namespace_hash != 0) {
            for (const auto &import : prolog->module_imports) {
               if (pf::strhash(import.target_namespace) IS namespace_hash) {
                  if (!context.module_cache) {
                     std::string message = "Module variable '" + name + "' requires a module cache.";
                     record_error(message, ReferenceNode, true);
                     return false;
                  }

                  std::string message = "Module variable resolution is not implemented for namespace '";
                  message.append(import.target_namespace);
                  message.append("'.");
                  record_error(message, ReferenceNode, true);
                  return false;
               }
            }
         }
      }

      return false;
   }

   if (variable->is_external) {
      std::string message = "External variable '" + name + "' is not supported.";
      record_error(message, ReferenceNode, true);
      return false;
   }

   if (!variable->initializer) {
      std::string message = "Variable '" + name + "' is missing an initialiser.";
      record_error(message, ReferenceNode, true);
      return false;
   }

   if (variables_in_evaluation.find(name) != variables_in_evaluation.end()) {
      std::string message = "Variable '" + name + "' has a circular dependency.";
      record_error(message, ReferenceNode, true);
      return false;
   }

   variables_in_evaluation.insert(name);
   XPathVal computed_value = evaluate_expression(variable->initializer.get(), CurrentPrefix);
   variables_in_evaluation.erase(name);

   if (expression_unsupported) {
      std::string message = "Failed to evaluate initialiser for variable '" + name + "'.";
      record_error(message, ReferenceNode);
      return false;
   }

   auto inserted = prolog_variable_cache.insert_or_assign(name, std::move(computed_value));
   OutValue = inserted.first->second;
   return true;
}

//********************************************************************************************************************

XPathVal XPathEvaluator::evaluate_expression(const XPathNode *ExprNode, uint32_t CurrentPrefix)
{
   pf::Log log("XPath");

   if (!ExprNode) {
      record_error("Unsupported XPath expression: empty node", (const XPathNode *)nullptr, true);
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
         record_error("LET expression requires at least one binding and a return clause.", ExprNode, true);
         return XPathVal();
      }

      const XPathNode *return_node = ExprNode->get_child(ExprNode->child_count() - 1);
      if (!return_node) {
         record_error("LET expression is missing its return clause.", ExprNode, true);
         return XPathVal();
      }

      std::vector<VariableBindingGuard> binding_guards;
      binding_guards.reserve(ExprNode->child_count() - 1);

      for (size_t index = 0; index + 1 < ExprNode->child_count(); ++index) {
         const XPathNode *binding_node = ExprNode->get_child(index);
         if ((!binding_node) or !(binding_node->type IS XPathNodeType::LET_BINDING)) {
            record_error("LET expression contains an invalid binding clause.", binding_node ? binding_node : ExprNode, true);
            return XPathVal();
         }

         if ((binding_node->value.empty()) or (binding_node->child_count() IS 0)) {
            record_error("Let binding requires a variable name and expression.", binding_node, true);
            return XPathVal();
         }

         const XPathNode *binding_expr = binding_node->get_child(0);
         if (!binding_expr) {
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

      auto append_iteration_value = [&](const XPathVal &iteration_value) -> bool {
         if (iteration_value.Type IS XPVT::NodeSet) {
            size_t length = iteration_value.node_set.size();
            if (length < iteration_value.node_set_attributes.size()) length = iteration_value.node_set_attributes.size();
            if (length < iteration_value.node_set_string_values.size()) length = iteration_value.node_set_string_values.size();
            if ((length IS 0) and iteration_value.node_set_string_override.has_value()) length = 1;

            for (size_t node_index = 0; node_index < length; ++node_index) {
               XMLTag *node = node_index < iteration_value.node_set.size() ? iteration_value.node_set[node_index] : nullptr;
               combined_nodes.push_back(node);

               const XMLAttrib *attribute = nullptr;
               if (node_index < iteration_value.node_set_attributes.size()) attribute = iteration_value.node_set_attributes[node_index];
               combined_attributes.push_back(attribute);

               std::string node_string;
               bool use_override = iteration_value.node_set_string_override.has_value() and iteration_value.node_set_string_values.empty() and (node_index IS 0);
               if (node_index < iteration_value.node_set_string_values.size()) node_string = iteration_value.node_set_string_values[node_index];
               else if (use_override) node_string = *iteration_value.node_set_string_override;
               else if (attribute) node_string = attribute->Value;
               else if (node) node_string = XPathVal::node_string_value(node);

               combined_strings.push_back(node_string);
               if (!combined_override.has_value()) combined_override = node_string;
            }

            if (iteration_value.node_set_string_override.has_value() and iteration_value.node_set_string_values.empty()) {
               if (!combined_override.has_value()) combined_override = iteration_value.node_set_string_override;
            }

            return true;
         }

         if (iteration_value.is_empty()) return true;

         std::string atomic_string = iteration_value.to_string();
         combined_nodes.push_back(nullptr);
         combined_attributes.push_back(nullptr);
         combined_strings.push_back(atomic_string);
         if (!combined_override.has_value()) combined_override = atomic_string;
         return true;
      };

      std::function<bool(size_t)> evaluate_bindings = [&](size_t binding_index) -> bool {
         if (binding_index >= bindings.size()) {
            auto iteration_value = evaluate_expression(return_node, CurrentPrefix);
            if (expression_unsupported) return false;

            if (!append_iteration_value(iteration_value)) return false;
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
               if (is_some) return true;
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

         auto append_value = [&](const XPathVal &value) {
            if (value.Type IS XPVT::NodeSet) {
               bool use_override = value.node_set_string_override.has_value() and value.node_set_string_values.empty();
               for (size_t index = 0; index < value.node_set.size(); ++index) {
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

         return XPathVal(combined_nodes, std::nullopt, std::move(combined_strings), std::move(combined_attributes));
      }

      auto left_value = evaluate_expression(left_node, CurrentPrefix);
      if (expression_unsupported) return XPathVal();
      auto right_value = evaluate_expression(right_node, CurrentPrefix);
      if (expression_unsupported) return XPathVal();

      // TODO: Hash operation variable and use switch-case for better performance.

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
            binding_list.reserve(context.variables->size() * 16);
            bool first_binding = true;
            for (const auto &entry : *context.variables) {
               if (!first_binding) binding_list.append(", ");
               binding_list.append(entry.first);
               first_binding = false;
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
