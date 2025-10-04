//********************************************************************************************************************
// XPath Predicate Evaluation
//********************************************************************************************************************
//
// This translation unit contains predicate and comparison logic for XPath expressions. It handles:
//   - Value comparisons (=, !=, eq, ne)
//   - Relational comparisons (<, >, <=, >=, lt, gt, le, ge)
//   - Node-set to scalar conversions for predicate contexts
//   - Schema-aware type coercion during comparisons
//
// The comparison routines consume shared utilities from xpath_evaluator_detail.h (numeric_equal, numeric_compare,
// schema helpers) to ensure consistent behaviour across the XPath evaluation pipeline.

#include "xpath_evaluator.h"
#include "xpath_evaluator_detail.h"
#include "xpath_functions.h"
#include "../schema/schema_types.h"
#include "../xml.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

//********************************************************************************************************************
// Predicate Value Extraction
//********************************************************************************************************************

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

//********************************************************************************************************************
// Equality Comparison Logic
//********************************************************************************************************************

bool compare_xpath_values(const XPathValue &left_value, const XPathValue &right_value)
{
   auto left_type = left_value.type;
   auto right_type = right_value.type;

   bool schema_boolean = should_compare_as_boolean(left_value, right_value);
   if (schema_boolean) {
      auto left_descriptor = schema_descriptor_for_value(left_value);
      auto right_descriptor = schema_descriptor_for_value(right_value);

      bool left_boolean = left_value.to_boolean();
      bool right_boolean = right_value.to_boolean();

      if (left_descriptor) {
         auto coerced = left_descriptor->coerce_value(left_value, xml::schema::SchemaType::XPathBoolean);
         left_boolean = coerced.to_boolean();
      }

      if (right_descriptor) {
         auto coerced = right_descriptor->coerce_value(right_value, xml::schema::SchemaType::XPathBoolean);
         right_boolean = coerced.to_boolean();
      }

      return left_boolean IS right_boolean;
   }

   bool schema_numeric = should_compare_as_numeric(left_value, right_value);
   if ((left_type IS XPathValueType::Boolean) or (right_type IS XPathValueType::Boolean)) {
      bool left_boolean = left_value.to_boolean();
      bool right_boolean = right_value.to_boolean();
      return left_boolean IS right_boolean;
   }

   if ((left_type IS XPathValueType::Number) or (right_type IS XPathValueType::Number) or schema_numeric) {
      if ((left_type IS XPathValueType::NodeSet) or (right_type IS XPathValueType::NodeSet)) {
         const XPathValue &node_value = (left_type IS XPathValueType::NodeSet) ? left_value : right_value;
         const XPathValue &number_value = (left_type IS XPathValueType::NodeSet) ? right_value : left_value;

         double comparison_number = number_value.to_number();
         if (schema_numeric) {
            auto descriptor = schema_descriptor_for_value(number_value);
            if (descriptor) {
               auto coerced = descriptor->coerce_value(number_value, xml::schema::SchemaType::XPathNumber);
               comparison_number = coerced.to_number();
            }
         }

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

      if (schema_numeric) {
         if (auto descriptor = schema_descriptor_for_value(left_value)) {
            auto coerced = descriptor->coerce_value(left_value, xml::schema::SchemaType::XPathNumber);
            left_number = coerced.to_number();
         }

         if (auto descriptor = schema_descriptor_for_value(right_value)) {
            auto coerced = descriptor->coerce_value(right_value, xml::schema::SchemaType::XPathNumber);
            right_number = coerced.to_number();
         }
      }

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

//********************************************************************************************************************
// Relational Comparison Logic
//********************************************************************************************************************

bool compare_xpath_relational(const XPathValue &left_value,
                              const XPathValue &right_value,
                              RelationalOperator Operation)
{
   auto left_type = left_value.type;
   auto right_type = right_value.type;
   bool schema_numeric = should_compare_as_numeric(left_value, right_value);

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
      if (schema_numeric) {
         if (auto descriptor = schema_descriptor_for_value(other_value)) {
            auto coerced = descriptor->coerce_value(other_value, xml::schema::SchemaType::XPathNumber);
            other_number = coerced.to_number();
         }
      }
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
   if (schema_numeric) {
      if (auto descriptor = schema_descriptor_for_value(left_value)) {
         auto coerced = descriptor->coerce_value(left_value, xml::schema::SchemaType::XPathNumber);
         left_number = coerced.to_number();
      }

      if (auto descriptor = schema_descriptor_for_value(right_value)) {
         auto coerced = descriptor->coerce_value(right_value, xml::schema::SchemaType::XPathNumber);
         right_number = coerced.to_number();
      }
   }
   return numeric_compare(left_number, right_number, Operation);
}
