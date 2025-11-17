#include "parser/expression_value.h"

#include <utility>

#include "parser/parse_internal.h"

ExpressionValue::ExpressionValue(ParserContext& Context, const ExpDesc& Descriptor)
   : context(&Context), value(Descriptor)
{
}

ExpressionValue::ExpressionValue(const ExpressionValue& Other)
   : context(Other.context), value(Other.value)
{
   this->allocator = nullptr;
   this->pinned_register = NO_REG;
}

ExpressionValue& ExpressionValue::operator=(const ExpressionValue& Other)
{
   if (this != &Other) {
      this->release_allocation();
      this->context = Other.context;
      this->allocator = nullptr;
      this->pinned_register = NO_REG;
      this->value = Other.value;
   }
   return *this;
}

ExpressionValue::ExpressionValue(ExpressionValue&& Other) noexcept
{
   *this = std::move(Other);
}

ExpressionValue& ExpressionValue::operator=(ExpressionValue&& Other) noexcept
{
   if (this != &Other) {
      this->release_allocation();
      this->context = Other.context;
      this->allocator = Other.allocator;
      this->pinned_register = Other.pinned_register;
      this->value = Other.value;
      Other.context = nullptr;
      Other.allocator = nullptr;
      Other.pinned_register = NO_REG;
   }
   return *this;
}

ExpressionValue::~ExpressionValue()
{
   this->release_allocation();
}

ExpDesc& ExpressionValue::descriptor()
{
   return this->value;
}

const ExpDesc& ExpressionValue::descriptor() const
{
   return this->value;
}

ParserResult<BCReg> ExpressionValue::ensure_register(RegisterAllocator& Allocator)
{
   if (this->value.k IS ExpKind::NonReloc) {
      this->allocator = &Allocator;
      this->pinned_register = this->value.u.s.info;
      return ParserResult<BCReg>::success(this->pinned_register);
   }
   BCReg target = Allocator.reserve_raw(1);
   expr_toreg(&Allocator.func_state(), &this->value, target);
   this->allocator = &Allocator;
   this->pinned_register = target;
   return ParserResult<BCReg>::success(target);
}

ExpDesc ExpressionValue::release()
{
   ExpDesc result = this->value;
   this->allocator = nullptr;
   this->pinned_register = NO_REG;
   return result;
}

void ExpressionValue::release_allocation()
{
   if (this->allocator != nullptr and this->pinned_register != NO_REG) {
      this->allocator->release(this->pinned_register);
   }
   this->allocator = nullptr;
   this->pinned_register = NO_REG;
}

