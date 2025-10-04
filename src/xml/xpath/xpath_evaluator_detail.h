//********************************************************************************************************************
// XPath Evaluator Internal Helpers

#pragma once

#include "xpath_value.h"
#include "../schema/schema_types.h"

#include <memory>

namespace xpath::evaluator::detail {

enum class RelationalOperator {
   LESS,
   LESS_OR_EQUAL,
   GREATER,
   GREATER_OR_EQUAL
};

std::shared_ptr<xml::schema::SchemaTypeDescriptor> schema_descriptor_for_value(const XPathValue &Value);
bool should_compare_as_boolean(const XPathValue &Left, const XPathValue &Right);
bool should_compare_as_numeric(const XPathValue &Left, const XPathValue &Right);
bool numeric_equal(double Left, double Right);
bool numeric_compare(double Left, double Right, RelationalOperator Operation);

} // namespace xpath::evaluator::detail
