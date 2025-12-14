// Unit tests for JIT frame management abstractions.
// Copyright (C) 2025 Paul Manias
//
// These tests verify the correctness of the FrameManager class and FRC constants used in the JIT trace recorder. They
// complement the existing Fluid integration tests by providing low-level verification of frame arithmetic operations.

#include <parasol/main.h>

#ifdef ENABLE_UNIT_TESTS

#include "frame_manager.h"
#include "../debug/lj_jit.h"
#include <array>
#include <cstring>

namespace {

// For unit tests, we use the real jit_State structure but only initialize the fields
// we need. This avoids layout mismatch issues with mocks.
// Verify that jit_State has the expected layout for the fields we access:
static_assert(offsetof(jit_State, base) IS offsetof(jit_State, cur) + sizeof(GCtrace) + sizeof(void*) * 5,
   "jit_State::base offset unexpected");
static_assert(offsetof(jit_State, baseslot) IS offsetof(jit_State, base) + sizeof(TRef*) + sizeof(uint32_t) + sizeof(BCREG),
   "jit_State::baseslot offset unexpected");

// Helper to initialize a jit_State for testing frame operations only
static void init_test_jit_state(jit_State& J) {
   memset(&J, 0, sizeof(J));
   J.baseslot = FRC::MIN_BASESLOT;
   J.base = J.slot + J.baseslot;
   J.maxslot = 0;
   J.framedepth = 0;
}

// Test that FRC constants match the expected values for LJ_FR2=1
static bool test_frc_constants(pf::Log& log)
{
   // In 2-slot frame mode (LJ_FR2=1):
   // HEADER_SIZE = 1 + LJ_FR2 = 2
   static_assert(FRC::HEADER_SIZE IS 2, "HEADER_SIZE should be 2");

   // CONT_FRAME_SIZE = 2 << LJ_FR2 = 4
   static_assert(FRC::CONT_FRAME_SIZE IS 4, "CONT_FRAME_SIZE should be 4");

   // FUNC_SLOT_OFFSET = -1 - LJ_FR2 = -2
   static_assert(FRC::FUNC_SLOT_OFFSET IS -2, "FUNC_SLOT_OFFSET should be -2");

   // MIN_BASESLOT = 1 + LJ_FR2 = 2
   static_assert(FRC::MIN_BASESLOT IS 2, "MIN_BASESLOT should be 2");

   return true;
}

// Test push/pop symmetry for call frames
static bool test_frame_push_pop_symmetry(pf::Log& log)
{
   jit_State J;
   init_test_jit_state(J);
   FrameManager fm(&J);

   BCREG initial_baseslot = J.baseslot;

   // Push frame at slot 5: base moves by 5 + 2 (header) = 7
   fm.push_call_frame(5);
   if (J.baseslot != initial_baseslot + 5 + FRC::HEADER_SIZE) {
      log.error("push_call_frame: expected baseslot=%u, got %u", initial_baseslot + 5 + FRC::HEADER_SIZE, J.baseslot);
      return false;
   }

   // Pop Lua frame with cbase=5: base moves back by 5 + 2 = 7
   fm.pop_lua_frame(5);
   if (J.baseslot != initial_baseslot) {
      log.error("pop_lua_frame: expected baseslot=%u, got %u", initial_baseslot, J.baseslot);
      return false;
   }

   return true;
}

// Test delta-only pop (for vararg/pcall frames)
static bool test_delta_frame_pop(pf::Log& log)
{
   jit_State J;
   init_test_jit_state(J);

   // Start at base + some offset
   J.baseslot = 10;
   J.base = J.slot + J.baseslot;

   FrameManager fm(&J);

   // Push frame at slot 3
   fm.push_call_frame(3);  // Now at 10 + 3 + 2 = 15

   if (J.baseslot != 15) {
      log.error("push_call_frame: expected baseslot=15, got %u", J.baseslot);
      return false;
   }

   // Pop delta-only (vararg frames use just the delta, no header adjustment)
   fm.pop_delta_frame(3);  // Back by 3 only = 12

   if (J.baseslot != 12) {
      log.error("pop_delta_frame: expected baseslot=12, got %u", J.baseslot);
      return false;
   }

   return true;
}

// Test func_slot accessor
static bool test_func_slot_access(pf::Log& log)
{
   jit_State J;
   init_test_jit_state(J);
   FrameManager fm(&J);

   // Set a value at the function slot position
   J.base[FRC::FUNC_SLOT_OFFSET] = 0x12345678;

   TRef result = fm.func_slot();
   if (result != 0x12345678) {
      log.error("func_slot: expected 0x12345678, got 0x%x", result);
      return false;
   }

   // Test writing through func_slot
   fm.func_slot() = 0xDEADBEEF;
   if (J.base[FRC::FUNC_SLOT_OFFSET] != 0xDEADBEEF) {
      log.error("func_slot write: expected 0xDEADBEEF, got 0x%x",
              J.base[FRC::FUNC_SLOT_OFFSET]);
      return false;
   }

   return true;
}

// Test overflow detection
static bool test_overflow_detection(pf::Log& log)
{
   jit_State J;
   init_test_jit_state(J);
   J.baseslot = LJ_MAX_JSLOTS - 10;
   J.base = J.slot + J.baseslot;

   FrameManager fm(&J);

   // Should detect overflow
   if (not fm.would_overflow(15)) {
      log.error("would_overflow: should detect overflow for 15 slots");
      return false;
   }

   // Should not overflow
   if (fm.would_overflow(5)) {
      log.error("would_overflow: should not overflow for 5 slots");
      return false;
   }

   return true;
}

// Test at_root_baseslot
static bool test_root_baseslot_detection(pf::Log& log)
{
   jit_State J;
   init_test_jit_state(J);
   FrameManager fm(&J);

   // At initialization, should be at root
   if (not fm.at_root_baseslot()) {
      log.error("at_root_baseslot: should be true at init");
      return false;
   }

   // After pushing a frame, should not be at root
   fm.push_call_frame(0);
   if (fm.at_root_baseslot()) {
      log.error("at_root_baseslot: should be false after push");
      return false;
   }

   return true;
}

// Test compact_tailcall memory move
static bool test_compact_tailcall(pf::Log& log)
{
   jit_State J;
   init_test_jit_state(J);
   J.baseslot = 10;
   J.base = J.slot + J.baseslot;

   // Set up some test values
   J.base[5] = 0xAAAA;  // func at slot 5
   J.base[6] = 0xBBBB;  // frame marker
   J.base[7] = 0xCCCC;  // arg 1
   J.base[8] = 0xDDDD;  // arg 2

   FrameManager fm(&J);
   fm.compact_tailcall(5, 2);  // Move func + 2 args + header

   // Check that values moved to function slot position
   if (J.base[FRC::FUNC_SLOT_OFFSET] != 0xAAAA or
       J.base[FRC::FUNC_SLOT_OFFSET + 1] != 0xBBBB or
       J.base[FRC::FUNC_SLOT_OFFSET + 2] != 0xCCCC or
       J.base[FRC::FUNC_SLOT_OFFSET + 3] != 0xDDDD) {
      log.error("compact_tailcall: values not moved correctly");
      return false;
   }

   return true;
}

struct TestCase {
   const char* name;
   bool (*fn)(pf::Log&);
};

} // anonymous namespace

// Public test entry point - matches parser_unit_tests signature
extern void jit_frame_unit_tests(int &Passed, int &Total)
{
   constexpr std::array<TestCase, 7> tests = { {
      { "frc_constants", test_frc_constants },
      { "frame_push_pop_symmetry", test_frame_push_pop_symmetry },
      { "delta_frame_pop", test_delta_frame_pop },
      { "func_slot_access", test_func_slot_access },
      { "overflow_detection", test_overflow_detection },
      { "root_baseslot_detection", test_root_baseslot_detection },
      { "compact_tailcall", test_compact_tailcall }
   } };

   for (const TestCase& test : tests) {
      pf::Log log("JitFrameTests");
      log.branch("Running %s", test.name);
      ++Total;
      if (test.fn(log)) {
         ++Passed;
         log.msg("%s passed", test.name);
      }
      else {
         log.error("%s failed", test.name);
      }
   }
}

#endif // ENABLE_UNIT_TESTS
