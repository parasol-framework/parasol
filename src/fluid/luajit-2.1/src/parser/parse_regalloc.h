// Register allocation interface and RAII helpers for LuaJIT parser.
// Copyright (C) 2025 Paul Manias.

#pragma once

struct ExpDesc;
struct FuncState;

class RegisterAllocator;

//********************************************************************************************************************

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

   inline void release();

   [[nodiscard]] bool empty() const { return count_ IS 0; }
   [[nodiscard]] BCREG start() const { return start_; }
   [[nodiscard]] BCREG count() const { return count_; }
   [[nodiscard]] BCREG expected_top() const { return expected_top_; }

private:
   friend class RegisterAllocator;

   RegisterSpan(RegisterAllocator* Allocator, BCREG Start, BCREG Count, BCREG ExpectedTop)
      : allocator_(Allocator), start_(Start), count_(Count), expected_top_(ExpectedTop) {}

   RegisterAllocator* allocator_;
   BCREG start_;
   BCREG count_;
   BCREG expected_top_;
};

//********************************************************************************************************************

class AllocatedRegister {
public:
   AllocatedRegister() : allocator_(nullptr), index_(NO_REG), expected_top_(0) {}
   AllocatedRegister(AllocatedRegister&& Other) noexcept
      : allocator_(Other.allocator_), index_(Other.index_), expected_top_(Other.expected_top_)
   {
      Other.allocator_ = nullptr;
   }
   AllocatedRegister& operator=(AllocatedRegister &&Other) noexcept
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

   inline void release();

   [[nodiscard]] bool valid() const { return allocator_ != nullptr; }
   [[nodiscard]] BCREG index() const { return index_; }
   [[nodiscard]] BCREG expected_top() const { return expected_top_; }

private:
   friend class RegisterAllocator;

   AllocatedRegister(RegisterAllocator* Allocator, BCREG Index, BCREG ExpectedTop)
      : allocator_(Allocator), index_(Index), expected_top_(ExpectedTop) {}

   RegisterAllocator* allocator_;
   BCREG index_;
   BCREG expected_top_;
};

//********************************************************************************************************************

struct TableOperandCopies {
   ExpDesc duplicated;
   RegisterSpan reserved;
};

//********************************************************************************************************************

class RegisterAllocator {
public:
   inline explicit RegisterAllocator(FuncState* State);

   void bump(BCREG Count);
   inline void reserve(BCREG Count);

   [[nodiscard]] inline AllocatedRegister acquire();
   // Reserve a strict RAII span: the allocator expects the span to be released
   // while freereg still equals the top of the span. Used when callers rely on
   // RegisterSpan to pop temporaries in LIFO order.
   [[nodiscard]] RegisterSpan reserve_span(BCREG Count);

   // Reserve a "soft" span: the allocator tracks the range but does not enforce
   // RAII invariants or adjust freereg when the span is released. This is used
   // in patterns where callers explicitly manage freereg (e.g. assignment
   // emitters that duplicate table operands and later collapse freereg to
   // nactvar).
   [[nodiscard]] RegisterSpan reserve_span_soft(BCREG Count);

   void release(AllocatedRegister& Handle);
   void release(RegisterSpan& Span);
   void release_register(BCREG Register);
   void release_expression(ExpDesc* Expression);

   void collapse_freereg(BCREG ResultReg);

   [[nodiscard]] TableOperandCopies duplicate_table_operands(const ExpDesc& Expression);

   [[nodiscard]] FuncState* state() const { return func_state; }

   // Debug verification methods
   void verify_no_leaks(const char* Context) const;
   void trace_allocation(BCREG Start, BCREG Count, const char* Context) const;
   void trace_release(BCREG Start, BCREG Count, const char* Context) const;

private:
   BCREG reserve_slots(BCREG Count);
   void release_span_internal(BCREG Start, BCREG Count, BCREG ExpectedTop);

   FuncState* func_state;
};

//********************************************************************************************************************

inline RegisterAllocator::RegisterAllocator(FuncState* State) : func_state(State) { }

inline void RegisterAllocator::reserve(BCREG Count) { this->reserve_slots(Count); }

inline AllocatedRegister RegisterAllocator::acquire() {
   BCREG start = this->reserve_slots(1);
   return AllocatedRegister(this, start, start + 1);
}

inline void RegisterSpan::release() { if (allocator_) allocator_->release(*this); }

inline void AllocatedRegister::release() { if (allocator_) allocator_->release(*this); }

// Get pointer to bytecode instruction for expression.

[[nodiscard]] inline BCIns * bcptr(FuncState *fs, const ExpDesc *e) { return &fs->bcbase[e->u.s.info].ins; }
