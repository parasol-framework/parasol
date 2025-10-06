//********************************************************************************************************************
// XPath Evaluator Detail - Shared Utilities
//
// Provides internal utility functions shared across XPath evaluator translation units.  These functions handle
// type resolution, value comparison, and schema-aware operations used throughout the evaluation pipeline.
//
// Key functionality:
//   - Schema type descriptor lookup and caching
//   - Type comparison strategy resolution (numeric, string, boolean)
//   - Numeric equality and relational comparison with floating-point tolerance
//   - Node-set value extraction (string and numeric conversions)
//   - Value promotion for comparison operations
//
// These utilities ensure consistent type coercion and comparison semantics across predicates, expressions,
// and function calls in accordance with XPath 2.0 specifications.

#pragma once

#include "../xml/xpath_value.h"

#include <memory>
#include <optional>
#include <string>

enum class RelationalOperator {
   LESS,
   LESS_OR_EQUAL,
   GREATER,
   GREATER_OR_EQUAL
};

// Schema and type system helpers
std::shared_ptr<xml::schema::SchemaTypeDescriptor> schema_descriptor_for_value(const XPathVal &Value);
bool should_compare_as_boolean(const XPathVal &Left, const XPathVal &Right);
bool should_compare_as_numeric(const XPathVal &Left, const XPathVal &Right);

// Numeric comparison utilities
bool numeric_equal(double Left, double Right);
bool numeric_compare(double Left, double Right, RelationalOperator Operation);

// Predicate value extraction and comparison (implemented in xpath_evaluator_predicates.cpp)
std::string node_set_string_value(const XPathVal &Value, size_t Index);
double node_set_number_value(const XPathVal &Value, size_t Index);
std::optional<XPathVal> promote_value_comparison_operand(const XPathVal &Value);
bool compare_xpath_values(const XPathVal &LeftValue, const XPathVal &RightValue);
bool compare_xpath_relational(const XPathVal &LeftValue, const XPathVal &RightValue, RelationalOperator Operation);
