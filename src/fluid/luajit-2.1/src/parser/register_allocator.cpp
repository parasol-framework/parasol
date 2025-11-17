#include "parser/register_allocator.h"

#include <utility>

#include "lj_err.h"

AllocatedRegister::AllocatedRegister(RegisterAllocator* Owner, BCReg Index)
   : owner(Owner), register_index(Index)
{
}

AllocatedRegister::AllocatedRegister(AllocatedRegister&& Other) noexcept
{
   *this = std::move(Other);
}

AllocatedRegister& AllocatedRegister::operator=(AllocatedRegister&& Other) noexcept
{
   if (this != &Other) {
      this->release();
      this->owner = Other.owner;
      this->register_index = Other.register_index;
      Other.owner = nullptr;
      Other.register_index = NO_REG;
   }
   return *this;
}

AllocatedRegister::~AllocatedRegister()
{
   this->release();
}

bool AllocatedRegister::is_valid() const
{
   return this->owner != nullptr and this->register_index != NO_REG;
}

BCReg AllocatedRegister::index() const
{
   return this->register_index;
}

BCReg AllocatedRegister::release()
{
   BCReg value = this->register_index;
   if (this->owner != nullptr and this->register_index != NO_REG) {
      this->owner->release(this->register_index);
      this->reset();
   }
   return value;
}

void AllocatedRegister::reset()
{
   this->owner = nullptr;
   this->register_index = NO_REG;
}

RegisterSpan::RegisterSpan(RegisterAllocator* Owner, BCReg Start, BCReg Count)
   : owner(Owner), start_register(Start), span_size(Count)
{
}

RegisterSpan::RegisterSpan(RegisterSpan&& Other) noexcept
{
   *this = std::move(Other);
}

RegisterSpan& RegisterSpan::operator=(RegisterSpan&& Other) noexcept
{
   if (this != &Other) {
      this->release();
      this->owner = Other.owner;
      this->start_register = Other.start_register;
      this->span_size = Other.span_size;
      Other.owner = nullptr;
      Other.start_register = NO_REG;
      Other.span_size = 0;
   }
   return *this;
}

RegisterSpan::~RegisterSpan()
{
   this->release();
}

bool RegisterSpan::is_valid() const
{
   return this->owner != nullptr and this->span_size != 0;
}

BCReg RegisterSpan::start() const
{
   return this->start_register;
}

BCReg RegisterSpan::count() const
{
   return this->span_size;
}

void RegisterSpan::release()
{
   if (this->owner != nullptr and this->span_size != 0) {
      this->owner->release_span(this->start_register, this->span_size);
      this->reset();
   }
}

void RegisterSpan::reset()
{
   this->owner = nullptr;
   this->start_register = NO_REG;
   this->span_size = 0;
}

RegisterAllocator::RegisterAllocator(FuncState& FuncState)
   : func_state_ptr(&FuncState), high_water(FuncState.freereg)
{
}

AllocatedRegister RegisterAllocator::acquire()
{
   BCReg index = this->reserve_raw(1);
   return AllocatedRegister(this, index);
}

RegisterSpan RegisterAllocator::acquire_span(BCReg Count)
{
   if (Count IS 0)
      return RegisterSpan();
   BCReg start = this->reserve_raw(Count);
   return RegisterSpan(this, start, Count);
}

BCReg RegisterAllocator::reserve_raw(BCReg Count)
{
   this->bump_frame(Count);
   BCReg start = this->func_state_ptr->freereg;
   this->func_state_ptr->freereg += Count;
   if (this->func_state_ptr->freereg > this->high_water)
      this->high_water = this->func_state_ptr->freereg;
   return start;
}

void RegisterAllocator::release(BCReg Register)
{
   FuncState* fs = this->func_state_ptr;
   if (Register >= fs->nactvar) {
      fs->freereg--;
      lj_assertFS(Register IS fs->freereg, "bad register release order");
   }
}

void RegisterAllocator::release_span(BCReg Start, BCReg Count)
{
   if (Count IS 0)
      return;
   FuncState* fs = this->func_state_ptr;
   BCReg expected = Start + Count;
   lj_assertFS(expected IS fs->freereg, "span release must match stack tail");
   while (Count--) {
      this->release(Start + Count);
   }
}

void RegisterAllocator::collapse_to(BCReg Depth)
{
   FuncState* fs = this->func_state_ptr;
   while (fs->freereg > Depth)
      this->release(fs->freereg - 1);
}

BCReg RegisterAllocator::free_register() const
{
   return this->func_state_ptr->freereg;
}

BCReg RegisterAllocator::high_water_mark() const
{
   return this->high_water;
}

FuncState& RegisterAllocator::func_state()
{
   lj_assertX(this->func_state_ptr != nullptr, "allocator requires a function state");
   return *this->func_state_ptr;
}

void RegisterAllocator::bump_frame(BCReg Count)
{
   FuncState* fs = this->func_state_ptr;
   BCReg size = fs->freereg + Count;
   if (size > fs->framesize) {
      if (size >= LJ_MAX_SLOTS)
         fs->ls->err_syntax(LJ_ERR_XSLOTS);
      fs->framesize = uint8_t(size);
   }
}

