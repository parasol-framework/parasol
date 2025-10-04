//********************************************************************************************************************
// XPath Evaluator - Internal Detail Declarations

#pragma once

#include "xpath_value.h"

#include <cstddef>
#include <memory>

namespace xml::schema
{
   class SchemaTypeDescriptor;
}

class XPathEvaluator;
struct XMLTag;
struct XMLAttrib;

enum class RelationalOperator
{
   LESS,
   LESS_OR_EQUAL,
   GREATER,
   GREATER_OR_EQUAL
};

class ContextGuard
{
   public:
   ContextGuard(XPathEvaluator &Evaluator, XMLTag *Node, size_t Position, size_t Size);
   ContextGuard(XPathEvaluator &Evaluator, XMLTag *Node, size_t Position, size_t Size, const XMLAttrib *Attribute);
   ContextGuard(const ContextGuard &) = delete;
   ContextGuard & operator=(const ContextGuard &) = delete;
   ContextGuard(ContextGuard &&Other) noexcept;
   ContextGuard & operator=(ContextGuard &&Other) noexcept;
   ~ContextGuard();

   private:
   XPathEvaluator * evaluator = nullptr;
   bool active = false;
};

class CursorGuard
{
   public:
   explicit CursorGuard(XPathEvaluator &Evaluator);
   CursorGuard(const CursorGuard &) = delete;
   CursorGuard & operator=(const CursorGuard &) = delete;
   CursorGuard(CursorGuard &&Other) noexcept;
   CursorGuard & operator=(CursorGuard &&Other) noexcept;
   ~CursorGuard();

   private:
   XPathEvaluator * evaluator = nullptr;
   bool active = false;
};

std::shared_ptr<xml::schema::SchemaTypeDescriptor> schema_descriptor_for_value(const XPathValue &Value);
bool should_compare_as_boolean(const XPathValue &Left, const XPathValue &Right);
bool should_compare_as_numeric(const XPathValue &Left, const XPathValue &Right);
bool numeric_equal(double Left, double Right);
bool numeric_compare(double Left, double Right, RelationalOperator Operation);
