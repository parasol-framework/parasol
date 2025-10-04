#include "xpath_evaluator_detail.h"

#include "../schema/schema_types.h"

#include <cmath>
#include <limits>

std::shared_ptr<xml::schema::SchemaTypeDescriptor> schema_descriptor_for_value(const XPathValue &Value)
{
   if (Value.schema_type_info) return Value.schema_type_info;

   auto &registry = xml::schema::registry();
   auto type = Value.get_schema_type();
   return registry.find_descriptor(type);
}

bool should_compare_as_boolean(const XPathValue &Left, const XPathValue &Right)
{
   if ((Left.type IS XPathValueType::NodeSet) or (Right.type IS XPathValueType::NodeSet)) return false;
   if ((Left.type IS XPathValueType::Boolean) or (Right.type IS XPathValueType::Boolean)) return true;

   auto left_descriptor = schema_descriptor_for_value(Left);
   auto right_descriptor = schema_descriptor_for_value(Right);
   if (!left_descriptor or !right_descriptor) return false;

   return left_descriptor->can_coerce_to(xml::schema::SchemaType::XPathBoolean) and
          right_descriptor->can_coerce_to(xml::schema::SchemaType::XPathBoolean);
}

bool should_compare_as_numeric(const XPathValue &Left, const XPathValue &Right)
{
   auto left_descriptor = schema_descriptor_for_value(Left);
   auto right_descriptor = schema_descriptor_for_value(Right);
   if (!left_descriptor or !right_descriptor) return false;

   return left_descriptor->can_coerce_to(xml::schema::SchemaType::XPathNumber) and
          right_descriptor->can_coerce_to(xml::schema::SchemaType::XPathNumber);
}

bool numeric_equal(double Left, double Right)
{
   if (std::isnan(Left) or std::isnan(Right)) return false;
   if (std::isinf(Left) or std::isinf(Right)) return Left IS Right;

   const double abs_left = std::fabs(Left);
   const double abs_right = std::fabs(Right);
   const double larger = (abs_left > abs_right) ? abs_left : abs_right;

   if (larger <= 1.0) {
      return std::fabs(Left - Right) <= std::numeric_limits<double>::epsilon() * 16;
   } else {
      return std::fabs(Left - Right) <= larger * std::numeric_limits<double>::epsilon() * 16;
   }
}

bool numeric_compare(double Left, double Right, RelationalOperator Operation)
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
