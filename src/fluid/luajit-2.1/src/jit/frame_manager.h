// Frame management abstractions for JIT trace recorder.
// Copyright (C) 2025 Paul Manias

#pragma once

#include "../debug/lj_jit.h"
#include "../runtime/lj_frame.h"
#include "../lj_iropt.h"

// Named constants for frame layout (2-slot frame mode, LJ_FR2=1)

namespace FRC { // 'FRame Const'
   // Frame header size: function slot + return continuation slot.  In 2-slot mode (LJ_FR2=1): 1 + LJ_FR2 = 2
   inline constexpr BCREG HEADER_SIZE = 2;

   // Continuation frame size: 2 << LJ_FR2 = 4 slots.  Used for metamethod continuation frames
   inline constexpr BCREG CONT_FRAME_SIZE = 4;

   // Offset from base to function slot: -1 - LJ_FR2 = -2.  The function is stored at base[-2] in 2-slot mode
   inline constexpr int32_t FUNC_SLOT_OFFSET = -2;

   // Minimum baseslot value: 1 + LJ_FR2 = 2.  The invoking function is at base[-1-LJ_FR2] = base[-2]
   inline constexpr BCREG MIN_BASESLOT = 2;

   // Check if trace is at root level (no nested frames or pending returns)
   // This is a common check: framedepth + retdepth IS 0
   [[nodiscard]] inline bool at_trace_root(jit_State *J) {
      return J->framedepth + J->retdepth IS 0;
   }

   // Check if currently at root frame depth (framedepth IS 0, ignoring retdepth)
   [[nodiscard]] inline bool at_root_depth(jit_State *J) {
      return J->framedepth IS 0;
   }

   // Increment frame depth (for call recording)
   inline void inc_depth(jit_State *J) {
      J->framedepth++;
   }

   // Decrement frame depth and return result for error checking
   [[nodiscard]] inline int32_t dec_depth(jit_State *J) {
      return --J->framedepth;
   }

   // Decrement frame depth by amount and return result
   [[nodiscard]] inline int32_t dec_depth_by(jit_State *J, int32_t amount) {
      J->framedepth -= amount;
      return J->framedepth;
   }
}

//********************************************************************************************************************
// FrameManager - Encapsulates frame push/pop arithmetic for the JIT recorder.
//
// The JIT recorder maintains J->base and J->baseslot which must stay in sync.  This class provides methods that
// correctly adjust both together, avoiding off-by-one errors in the frame header size calculations.
//
// Frame layout in 2-slot mode:
//   base[-2]  = function slot (func)
//   base[-1]  = frame marker (TREF_FRAME or PC/delta)
//   base[0]   = first argument/local slot
//   ...
//
// When pushing a call frame at slot 'func': new_base = old_base + func + HEADER_SIZE
// When popping a Lua frame with cbase:      new_base = old_base - cbase - HEADER_SIZE

class FrameManager {
   jit_State *J;

public:
   explicit FrameManager(jit_State *j) : J(j) {}

   // Push a new call frame (adjusts base by func_slot + header)
   // Used after setting up call with rec_call_setup
   void push_call_frame(BCREG func_slot) {
      BCREG offset = func_slot + FRC::HEADER_SIZE;
      J->base += offset;
      J->baseslot += offset;
   }

   // Pop vararg/pcall/continuation frame (delta-based, no header adjustment)
   // These frames use frame_delta() which already accounts for slot layout
   void pop_delta_frame(BCREG cbase) {
      J->baseslot -= cbase;
      J->base -= cbase;
   }

   // Pop Lua return frame (includes 2-slot header)
   // Used when returning from a Lua function call
   void pop_lua_frame(BCREG cbase) {
      BCREG offset = cbase + FRC::HEADER_SIZE;
      J->baseslot -= offset;
      J->base -= offset;
   }

   // Access frame function slot (base[-2] in 2-slot mode)
   [[nodiscard]] TRef& func_slot() { return J->base[FRC::FUNC_SLOT_OFFSET]; }
   [[nodiscard]] TRef func_slot() const { return J->base[FRC::FUNC_SLOT_OFFSET]; }

   // Check stack overflow before pushing
   [[nodiscard]] bool would_overflow(BCREG additional_slots) const {
      return (J->baseslot + additional_slots >= LJ_MAX_JSLOTS);
   }

   // Move slots for tail call (compact stack)
   // Moves func + args from source position to frame function slot position
   void compact_tailcall(BCREG func_slot, BCREG slot_count) {
      memmove(&J->base[FRC::FUNC_SLOT_OFFSET], &J->base[func_slot], sizeof(TRef) * (slot_count + FRC::HEADER_SIZE));
   }

   // Copy results during return
   void copy_results(int32_t dest_offset, int32_t src_offset, ptrdiff_t count) {
      memmove(&J->base[dest_offset], &J->base[src_offset], sizeof(TRef) * count);
   }

   // Clear frame slots (set to zero)
   void clear_frame(int32_t start_offset, BCREG count) {
      memset(&J->base[start_offset], 0, sizeof(TRef) * count);
   }

   // Get current baseslot value
   [[nodiscard]] BCREG baseslot() const { return J->baseslot; }

   // Check if at minimum baseslot (root frame)
   [[nodiscard]] bool at_root_baseslot() const {
      return J->baseslot IS FRC::MIN_BASESLOT;
   }
};

//********************************************************************************************************************
// FrameDepthGuard - RAII guard for tracking frame depth during call recording.
//
// Frame depth tracks how many frames deep we are in the trace being recorded.  This guard ensures that framedepth is
// decremented on scope exit, unless explicitly released.
//
// Usage patterns:
//   1. Simple increment/auto-decrement:
//        FrameDepthGuard fdg(J);  // framedepth++
//        // ... on scope exit, framedepth-- automatically
//
//   2. Manual decrement with error check (for returns):
//        FrameDepthGuard fdg(J, false);  // Don't auto-increment
//        if (fdg.decrement_and_check() < 0) error();  // Manual decrement + check
//
//   3. Release to keep incremented depth:
//        FrameDepthGuard fdg(J);  // framedepth++
//        fdg.release();  // Don't decrement on exit

class FrameDepthGuard {
   jit_State *J;
   bool released = false;

public:
   // Construct and increment framedepth
   explicit FrameDepthGuard(jit_State *j, bool auto_increment = true) : J(j) {
      if (auto_increment) J->framedepth++;
   }

   ~FrameDepthGuard() {
      if (not released) J->framedepth--;
   }

   // Non-copyable
   FrameDepthGuard(const FrameDepthGuard&) = delete;
   FrameDepthGuard& operator=(const FrameDepthGuard&) = delete;

   // Decrement and return new depth (for error checking)
   // Does NOT release - destructor will still decrement unless release() called
   [[nodiscard]] int32_t decrement_and_check() {
      return --J->framedepth;
   }

   // Decrement by specific amount (for continuation frames that subtract 2)
   [[nodiscard]] int32_t decrement_by(int32_t amount) {
      J->framedepth -= amount;
      return J->framedepth;
   }

   // Release guard - framedepth won't be decremented on destruction
   void release() { released = true; }

   // Check current depth
   [[nodiscard]] int32_t depth() const { return J->framedepth; }

   // Check if at root (framedepth IS 0)
   [[nodiscard]] bool at_root() const { return J->framedepth IS 0; }

   // Check combined depth (framedepth + retdepth)
   [[nodiscard]] int32_t combined_depth() const { return J->framedepth + J->retdepth; }
};

//********************************************************************************************************************
// IRRollbackPoint - Captures state for potential IR rollback.
//
// Some IR emission paths speculatively emit guards that may need to be rolled back if a later optimization
// discovers the guard was unnecessary.  This struct captures the rollback point (nins) and guardemit state.
//
// Usage:
//   IRRollbackPoint rbp;
//   rbp.mark(J);              // Capture current state
//   // ... emit speculative IR ...
//   if (need_rollback) rbp.rollback(J);

struct IRRollbackPoint {
   IRRef ref = 0;            // IR instruction reference for rollback point
   IRType1 guardemit{};      // Saved guardemit state

   // Mark current position as a potential rollback point
   void mark(jit_State *J) {
      ref = J->cur.nins;
      guardemit = J->guardemit;
   }

   // Check if a rollback point was marked
   [[nodiscard]] bool is_marked() const { return ref != 0; }

   // Perform rollback to the marked point
   void rollback(jit_State *J) {
      lj_ir_rollback(J, ref);
      J->guardemit = guardemit;
   }

   // Check if rollback is needed (when forwarding occurred)
   [[nodiscard]] bool needs_rollback(TRef result) const {
      return ref != 0 and tref_ref(result) < ref;
   }

   // Conditionally rollback if needed
   void rollback_if_forwarded(jit_State *J, TRef result) {
      if (needs_rollback(result)) rollback(J);
   }
};
