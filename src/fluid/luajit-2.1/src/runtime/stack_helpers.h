// Type-safe stack management helpers for LuaJIT.
// Copyright (C) 2025 Paul Manias.
//
// These helpers replace error-prone manual stack arithmetic with type-safe
// abstractions that encapsulate LJ_FR2 frame layout details.

#pragma once

#include "lj_obj.h"
#include "lj_state.h"
#include "lj_vm.h"

//********************************************************************************************************************
// StackRef: Safe stack reference that survives reallocation
//
// Problem: When calling lj_vm_call(), the stack may be reallocated, invalidating
// all TValue* pointers. Raw code often forgets to use savestack/restorestack.
//
// Solution: StackRef stores a byte offset from the stack base, automatically
// converting back to a valid pointer via get().
//
// Usage:
//    StackRef slot(L, L->top - 1);   // Save reference to a stack slot
//    lj_vm_call(L, base, nres);      // Stack may be reallocated
//    TValue* p = slot.get();         // Get valid pointer after call
//
// Note: The offset is from L->stack, not L->base, so it remains valid even
// if the base pointer changes during the call.

class StackRef {
   lua_State* L_;
   ptrdiff_t offset_;

public:
   // Construct from a TValue* on the stack
   StackRef(lua_State* L, TValue* ptr) noexcept
      : L_(L), offset_(savestack(L, ptr)) {}

   // Construct from a const TValue* on the stack
   StackRef(lua_State* L, cTValue* ptr) noexcept
      : L_(L), offset_(savestack(L, ptr)) {}

   // Get the current valid pointer (may differ from original if stack was reallocated)
   [[nodiscard]] TValue* get() const noexcept {
      return restorestack(L_, offset_);
   }

   // Allow implicit conversion to TValue* for convenience
   [[nodiscard]] operator TValue*() const noexcept {
      return get();
   }

   // Get the stored offset (for debugging or advanced use)
   [[nodiscard]] ptrdiff_t offset() const noexcept {
      return offset_;
   }
};

//********************************************************************************************************************
// Frame: Named constants and helpers for frame structure layout
//
// LuaJIT uses a 2-slot frame structure (LJ_FR2 mode) on 64-bit:
//
//    base-2  base-1      |  base  base+1 ...
//   [func   PC/delta/ft] | [slots ...]
//   ^-- frame            | ^-- base   ^-- top
//
// When calling lj_vm_call():
// - Push function at some position (call it 'base')
// - Push nil in the next slot (frame link slot)
// - Set L->top past the arguments
// - Call lj_vm_call(L, base, nres1)
// - After return, adjust L->top -= N + LJ_FR2 where N is pushed args
// - Result is at L->top + 1 + LJ_FR2
//
// These constants and functions encapsulate this layout to prevent errors.

namespace Frame {
   // Frame overhead: number of slots beyond arguments that VM uses
   // In LJ_FR2 mode, this is 2 (function slot + frame link slot)
   constexpr int OVERHEAD = 2;

   // Adjustment to L->top after lj_vm_call returns
   // For a call with N pushed values, adjust: L->top -= N + adjustment()
   constexpr int adjustment() noexcept {
      return LJ_FR2;
   }

   // Total slots consumed by a call frame with N arguments
   // Includes the function slot, frame link slot, and arguments
   constexpr int callFrameSize(int nargs) noexcept {
      return OVERHEAD + nargs;
   }

   // Get pointer to result after adjusting L->top post-call
   // After: L->top -= pushed_count + LJ_FR2
   // Result is at: L->top + 1 + LJ_FR2
   [[nodiscard]] inline TValue* result(lua_State* L) noexcept {
      return L->top + 1 + LJ_FR2;
   }

   // Adjust L->top after a VM call and return the result location
   // Use for simple 1-result calls: pass the number of values you pushed
   // Example:
   //    copyTV(L, base, funcval);   // Push function
   //    setnilV(base + 1);          // Frame link
   //    L->top = base + 2;          // 0 arguments
   //    lj_vm_call(L, base, 1 + 1);
   //    TValue* result = Frame::adjustAndGetResult(L, 2);  // Pushed 2 slots
   [[nodiscard]] inline TValue* adjustAndGetResult(lua_State* L, int pushedCount) noexcept {
      L->top -= pushedCount + LJ_FR2;
      return result(L);
   }
}

//********************************************************************************************************************
// VMCall: Builder pattern for safe VM call setup
//
// Encapsulates the setup, invocation, and result retrieval of a VM call.
// Handles the frame layout automatically and uses StackRef for safety.
//
// Usage (zero-argument call with 1 result):
//    VMCall call(L);
//    call.func(funcVal);
//    TValue* result = call.invoke(1);
//    copyTV(L, destination, result);
//
// Usage (call with arguments):
//    VMCall call(L);
//    call.func(funcVal);
//    call.arg(arg1);
//    call.arg(arg2);
//    TValue* result = call.invoke(1);
//
// Performance: Minimal overhead - inline methods compile to essentially the
// same code as manual setup, but with compile-time safety.

class VMCall {
   lua_State* L_;
   TValue* base_;        // Base of call frame (function slot)
   int argCount_;        // Number of arguments pushed

public:
   // Start a new VM call at the current top of stack
   explicit VMCall(lua_State* L) noexcept
      : L_(L), base_(L->top), argCount_(0) {}

   // Set the function to call (must be called first)
   VMCall& func(TValue* fn) noexcept {
      copyTV(L_, base_, fn);        // Copy function to base
      setnilV(base_ + 1);           // Frame link slot (required for LJ_FR2)
      L_->top = base_ + 2;          // Set top past frame link
      return *this;
   }

   // Set function from a const TValue*
   VMCall& func(cTValue* fn) noexcept {
      return func(const_cast<TValue*>(fn));
   }

   // Push an argument (call after func(), before invoke())
   VMCall& arg(TValue* v) noexcept {
      copyTV(L_, L_->top, v);
      L_->top++;
      argCount_++;
      return *this;
   }

   // Push argument from const TValue*
   VMCall& arg(cTValue* v) noexcept {
      return arg(const_cast<TValue*>(v));
   }

   // Invoke the call and return pointer to first result
   // nresults: number of expected results (default 1)
   // Returns: pointer to first result (valid until next stack operation)
   [[nodiscard]] TValue* invoke(int nresults = 1) noexcept {
      // nres1 parameter to lj_vm_call is nresults + 1 (for sentinel)
      lj_vm_call(L_, base_, nresults + 1);

      // Adjust top: we pushed 2 (func + frame link) + argCount_ values
      // VM consumed them plus LJ_FR2 overhead
      L_->top -= Frame::callFrameSize(argCount_) + Frame::adjustment();

      return Frame::result(L_);
   }

   // Get the base pointer (for advanced use, e.g., with lj_vm_pcall)
   [[nodiscard]] TValue* base() const noexcept {
      return base_;
   }

   // Get the current argument count
   [[nodiscard]] int argCount() const noexcept {
      return argCount_;
   }
};

//********************************************************************************************************************
// SimpleDeferredCall: Specialised helper for evaluating deferred expressions
//
// Deferred expressions are zero-argument functions that return one value.
// This helper encapsulates the common pattern used in deferred_resolve().
//
// Usage:
//    if (isdeferred(funcV(o))) {
//       StackRef slot(L, o);
//       SimpleDeferredCall call(L);
//       TValue* result = call.evaluate(o);
//       copyTV(L, slot.get(), result);
//    }

class SimpleDeferredCall {
   lua_State* L_;

public:
   explicit SimpleDeferredCall(lua_State* L) noexcept : L_(L) {}

   // Evaluate a deferred expression (zero-argument function returning 1 value)
   // Returns pointer to the result
   [[nodiscard]] TValue* evaluate(TValue* deferredFunc) noexcept {
      VMCall call(L_);
      call.func(deferredFunc);
      return call.invoke(1);
   }

   [[nodiscard]] TValue* evaluate(cTValue* deferredFunc) noexcept {
      return evaluate(const_cast<TValue*>(deferredFunc));
   }
};

//********************************************************************************************************************
// MetaCall: Helper for invoking metamethods where lj_meta_* has set up the frame
//
// The lj_meta_* functions (lj_meta_equal, lj_meta_comp, lj_meta_tget, lj_meta_tset)
// set up the call frame and return a base pointer. This helper handles the
// invocation and result retrieval.
//
// Common pattern in lj_api.cpp:
//    TValue* base = lj_meta_equal(L, o1, o2, 0);
//    if ((uintptr_t)base <= 1) return (int)(uintptr_t)base;
//    L->top = base + 2;
//    lj_vm_call(L, base, 1 + 1);
//    L->top -= 2 + LJ_FR2;
//    return tvistruecond(L->top + 1 + LJ_FR2);
//
// With MetaCall:
//    TValue* base = lj_meta_equal(L, o1, o2, 0);
//    if ((uintptr_t)base <= 1) return (int)(uintptr_t)base;
//    return tvistruecond(MetaCall::invoke(L, base, 2, 1));

namespace MetaCall {
   // Invoke a metamethod call where base was returned by lj_meta_*
   // slotsUsed: number of slots the metamethod prepared (typically 2 for binary ops)
   // nresults: number of expected results (typically 1)
   // Returns: pointer to first result
   [[nodiscard]] inline TValue* invoke(lua_State* L, TValue* base, int slotsUsed, int nresults = 1) noexcept {
      L->top = base + slotsUsed;
      lj_vm_call(L, base, nresults + 1);
      L->top -= slotsUsed + LJ_FR2;
      return Frame::result(L);
   }

   // Invoke for table get metamethod (__index)
   // Returns pointer to result value
   [[nodiscard]] inline TValue* invokeGet(lua_State* L) noexcept {
      L->top += 2;
      lj_vm_call(L, L->top - 2, 1 + 1);
      L->top -= 2 + LJ_FR2;
      return Frame::result(L);
   }

   // Invoke for table set metamethod (__newindex) from lua_settable
   // lj_meta_tset sets up [func, nil, table, key] at L->top
   // We need to copy the value (which is at a fixed offset before base) to base+2
   // Value offset: 3 + 2*LJ_FR2 slots before base
   inline void invokeSetTable(lua_State* L, TValue* base) noexcept {
      copyTV(L, base + 2, base - 3 - 2 * LJ_FR2);  // Copy value to arg position
      L->top = base + 3;                           // func, nil, table, key, value -> 3 args past base
      lj_vm_call(L, base, 0 + 1);                  // No results expected
      L->top -= 3 + LJ_FR2;                        // Adjust stack
   }

   // Invoke for table set metamethod (__newindex) from lua_setfield
   // Similar to invokeSetTable but different final adjustment
   inline void invokeSetField(lua_State* L, TValue* base) noexcept {
      copyTV(L, base + 2, base - 3 - 2 * LJ_FR2);  // Copy value to arg position
      L->top = base + 3;
      lj_vm_call(L, base, 0 + 1);
      L->top -= 2 + LJ_FR2;                        // Different adjustment for setfield
   }

   // Invoke concat metamethod (__concat)
   // top: base pointer returned by lj_meta_cat
   // Returns: offset to adjust n by (how many slots were consumed)
   // After call, copies result to L->top - 1
   inline int invokeConcat(lua_State* L, TValue* top) noexcept {
      int consumed = int(L->top - (top - 2 * LJ_FR2));
      L->top = top + 2;
      lj_vm_call(L, top, 1 + 1);
      L->top -= 1 + LJ_FR2;
      copyTV(L, L->top - 1, L->top + LJ_FR2);
      return consumed;
   }
}
