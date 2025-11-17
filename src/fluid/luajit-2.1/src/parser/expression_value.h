// ExpressionValue encapsulates ExpDesc lifecycle management.

#pragma once

#include "parser/parser_context.h"
#include "parser/register_allocator.h"

class ExpressionValue {
public:
   ExpressionValue(ParserContext& Context, const ExpDesc& Descriptor);
   ExpressionValue(const ExpressionValue& Other);
   ExpressionValue& operator=(const ExpressionValue& Other);
   ExpressionValue(ExpressionValue&& Other) noexcept;
   ExpressionValue& operator=(ExpressionValue&& Other) noexcept;
   ~ExpressionValue();

   [[nodiscard]] ExpDesc& descriptor();
   [[nodiscard]] const ExpDesc& descriptor() const;

   ParserResult<BCReg> ensure_register(RegisterAllocator& Allocator);
   ExpDesc release();

private:
   void release_allocation();

   ParserContext* context = nullptr;
   RegisterAllocator* allocator = nullptr;
   BCReg pinned_register = NO_REG;
   ExpDesc value{};
};

