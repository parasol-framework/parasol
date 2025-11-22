// Register allocation interface and RAII helpers for LuaJIT parser.
// Copyright (C) 2025 Paul Manias.

#pragma once

struct ExpDesc;
struct FuncState;

class RegisterAllocator;

class RegisterSpan {
public:
   RegisterSpan() : allocator_(nullptr), start_(0), count_(0), expected_top_(0) {}
   RegisterSpan(RegisterSpan&& Other) noexcept
      : allocator_(Other.allocator_), start_(Other.start_), count_(Other.count_), expected_top_(Other.expected_top_)
   {
      Other.allocator_ = nullptr;
   }
   RegisterSpan& operator=(RegisterSpan&& Other) noexcept
   {
      if (this IS &Other) return *this;

      this->release();
      allocator_ = Other.allocator_;
      start_ = Other.start_;
      count_ = Other.count_;
      expected_top_ = Other.expected_top_;
      Other.allocator_ = nullptr;
      return *this;
   }
   ~RegisterSpan() { this->release(); }

   void release();

   [[nodiscard]] bool empty() const { return count_ IS 0; }
   [[nodiscard]] BCReg start() const { return start_; }
   [[nodiscard]] BCReg count() const { return count_; }
   [[nodiscard]] BCReg expected_top() const { return expected_top_; }

private:
   friend class RegisterAllocator;

   RegisterSpan(RegisterAllocator* Allocator, BCReg Start, BCReg Count, BCReg ExpectedTop)
      : allocator_(Allocator), start_(Start), count_(Count), expected_top_(ExpectedTop) {}

   RegisterAllocator* allocator_;
   BCReg start_;
   BCReg count_;
   BCReg expected_top_;
};

class AllocatedRegister {
public:
   AllocatedRegister() : allocator_(nullptr), index_(NO_REG), expected_top_(0) {}
   AllocatedRegister(AllocatedRegister&& Other) noexcept
      : allocator_(Other.allocator_), index_(Other.index_), expected_top_(Other.expected_top_)
   {
      Other.allocator_ = nullptr;
   }
   AllocatedRegister& operator=(AllocatedRegister&& Other) noexcept
   {
      if (this IS &Other) return *this;

      this->release();
      allocator_ = Other.allocator_;
      index_ = Other.index_;
      expected_top_ = Other.expected_top_;
      Other.allocator_ = nullptr;
      return *this;
   }
   ~AllocatedRegister() { this->release(); }

   void release();

   [[nodiscard]] bool valid() const { return allocator_ != nullptr; }
   [[nodiscard]] BCReg index() const { return index_; }
   [[nodiscard]] BCReg expected_top() const { return expected_top_; }

private:
   friend class RegisterAllocator;

   AllocatedRegister(RegisterAllocator* Allocator, BCReg Index, BCReg ExpectedTop)
      : allocator_(Allocator), index_(Index), expected_top_(ExpectedTop) {}

   RegisterAllocator* allocator_;
   BCReg index_;
   BCReg expected_top_;
};

struct TableOperandCopies {
   ExpDesc duplicated;
   RegisterSpan reserved;
};

class RegisterAllocator {
public:
   explicit RegisterAllocator(FuncState* State);

   void bump(BCReg Count);
   void reserve(BCReg Count);

   [[nodiscard]] AllocatedRegister acquire();
   [[nodiscard]] RegisterSpan reserve_span(BCReg Count);

   void release(AllocatedRegister& Handle);
   void release(RegisterSpan& Span);
   void release_register(BCReg Register);
   void release_expression(ExpDesc* Expression);

   [[nodiscard]] TableOperandCopies duplicate_table_operands(const ExpDesc& Expression);

   [[nodiscard]] FuncState* state() const { return func_state; }

#if LJ_DEBUG
   // Debug verification methods (Phase 3 Stage 5)
   void verify_balance(const char* Context) const;
   void verify_no_leaks(const char* Context) const;
   void trace_allocation(BCReg Start, BCReg Count, const char* Context) const;
   void trace_release(BCReg Start, BCReg Count, const char* Context) const;
#endif

private:
   [[nodiscard]] BCReg reserve_slots(BCReg Count);
   void release_span_internal(BCReg Start, BCReg Count, BCReg ExpectedTop);

   FuncState* func_state;

#if LJ_DEBUG
   BCReg initial_freereg;
#endif
};

