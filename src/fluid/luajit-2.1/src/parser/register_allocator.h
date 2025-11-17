// Register allocator scaffolding for the modern LuaJIT parser.

#pragma once

#include <cstdint>

#include "lj_parse.h"

class RegisterAllocator;

class AllocatedRegister {
public:
   AllocatedRegister() = default;
   AllocatedRegister(RegisterAllocator* Owner, BCReg Index);
   AllocatedRegister(const AllocatedRegister&) = delete;
   AllocatedRegister& operator=(const AllocatedRegister&) = delete;
   AllocatedRegister(AllocatedRegister&& Other) noexcept;
   AllocatedRegister& operator=(AllocatedRegister&& Other) noexcept;
   ~AllocatedRegister();

   [[nodiscard]] bool is_valid() const;
   [[nodiscard]] BCReg index() const;
   BCReg release();

private:
   void reset();

   RegisterAllocator* owner = nullptr;
   BCReg register_index = NO_REG;
};

class RegisterSpan {
public:
   RegisterSpan() = default;
   RegisterSpan(RegisterAllocator* Owner, BCReg Start, BCReg Count);
   RegisterSpan(const RegisterSpan&) = delete;
   RegisterSpan& operator=(const RegisterSpan&) = delete;
   RegisterSpan(RegisterSpan&& Other) noexcept;
   RegisterSpan& operator=(RegisterSpan&& Other) noexcept;
   ~RegisterSpan();

   [[nodiscard]] bool is_valid() const;
   [[nodiscard]] BCReg start() const;
   [[nodiscard]] BCReg count() const;
   void release();

private:
   void reset();

   RegisterAllocator* owner = nullptr;
   BCReg start_register = NO_REG;
   BCReg span_size = 0;
};

class RegisterAllocator {
public:
   explicit RegisterAllocator(FuncState& FuncState);

   RegisterAllocator(const RegisterAllocator&) = delete;
   RegisterAllocator& operator=(const RegisterAllocator&) = delete;

   [[nodiscard]] AllocatedRegister acquire();
   [[nodiscard]] RegisterSpan acquire_span(BCReg Count);
   BCReg reserve_raw(BCReg Count);
   void release(BCReg Register);
   void release_span(BCReg Start, BCReg Count);
   void collapse_to(BCReg Depth);
   [[nodiscard]] BCReg free_register() const;
   [[nodiscard]] BCReg high_water_mark() const;
   [[nodiscard]] FuncState& func_state();

private:
   void bump_frame(BCReg Count);

   FuncState* func_state_ptr = nullptr;
   BCReg high_water = 0;
};

