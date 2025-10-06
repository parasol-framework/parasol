//********************************************************************************************************************
// XPath Evaluator Common Utilities
//
// This translation unit provides shared utility functions used throughout the XPath evaluator for value
// comparisons, type coercion, and schema-aware operations.  These helpers maintain consistent behaviour
// across different parts of the evaluation pipeline.
//
// Key functionality includes:
//   - Schema type descriptor lookup and caching
//   - Comparison type resolution (numeric vs string vs boolean)
//   - Numeric equality testing with epsilon handling for floating-point values
//   - String normalisation and comparison utilities
//   - Type coercion rules for mixed-type comparisons
//
// By centralising these operations, the evaluator ensures that predicates, function calls, and expression
// evaluation all apply the same semantic rules for value comparison and type conversion.

#include "xpath_evaluator_detail.h"
#include "../xml/schema/schema_types.h"
#include <cmath>
#include <limits>

// Retrieves or looks up the schema type descriptor for a given XPath value. Uses cached type info if available,
// otherwise queries the schema registry for the value's schema type.

std::shared_ptr<xml::schema::SchemaTypeDescriptor> schema_descriptor_for_value(const XPathVal &Value)
{
   if (Value.schema_type_info) return Value.schema_type_info;

   auto &registry = xml::schema::registry();
   auto type = Value.get_schema_type();
   return registry.find_descriptor(type);
}

// Determines if two XPath values should be compared as booleans based on their types. Returns true if either
// value is already a boolean, or if both values' schema types support coercion to XPath boolean type.

bool should_compare_as_boolean(const XPathVal &Left, const XPathVal &Right)
{
   if ((Left.type IS XPVT::NodeSet) or (Right.type IS XPVT::NodeSet)) return false;
   if ((Left.type IS XPVT::Boolean) or (Right.type IS XPVT::Boolean)) return true;

   auto left_descriptor = schema_descriptor_for_value(Left);
   auto right_descriptor = schema_descriptor_for_value(Right);
   if (!left_descriptor or !right_descriptor) return false;

   return left_descriptor->can_coerce_to(xml::schema::SchemaType::XPathBoolean) and
          right_descriptor->can_coerce_to(xml::schema::SchemaType::XPathBoolean);
}

// Determines if two XPath values should be compared as numeric values by checking if both values' schema
// descriptors support coercion to XPath number type.

bool should_compare_as_numeric(const XPathVal &Left, const XPathVal &Right)
{
   auto left_descriptor = schema_descriptor_for_value(Left);
   auto right_descriptor = schema_descriptor_for_value(Right);
   if (!left_descriptor or !right_descriptor) return false;

   return left_descriptor->can_coerce_to(xml::schema::SchemaType::XPathNumber) and
          right_descriptor->can_coerce_to(xml::schema::SchemaType::XPathNumber);
}

// Compares two floating-point numbers for equality using epsilon-based tolerance to handle floating-point
// precision issues. Special handling for NaN (always unequal) and infinity (equal only if both infinite
// with same sign). Uses relative epsilon for values larger than 1.0 and absolute epsilon otherwise.

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

// Performs relational comparisons (less than, greater than, etc.) between two numeric values. Returns false
// if either value is NaN, otherwise applies the specified comparison operator.

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
