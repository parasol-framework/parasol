#pragma once

// Name:      fluid.h
// Copyright: Paul Manias © 2006-2025
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_FLUID (1)

class objFluid;
// JIT behaviour options

enum class JOF : uint32_t {
   NIL = 0,
   DIAGNOSE = 0x00000001,
   DUMP_BYTECODE = 0x00000002,
   PROFILE = 0x00000004,
   TOP_TIPS = 0x00000008,
   TIPS = 0x00000010,
   ALL_TIPS = 0x00000020,
   TRACE_CFG = 0x00000040,
   TRACE_TYPES = 0x00000080,
   TRACE_TOKENS = 0x00000100,
   TRACE_EXPECT = 0x00000200,
   TRACE_BOUNDARY = 0x00000400,
   TRACE_OPERATORS = 0x00000800,
   TRACE_REGISTERS = 0x00001000,
   TRACE_ASSIGNMENTS = 0x00002000,
   TRACE_VALUE_CATEGORY = 0x00004000,
   TRACE = 0x00007fc0,
};

DEFINE_ENUM_FLAG_OPERATORS(JOF)

#ifdef PARASOL_STATIC
#define JUMPTABLE_FLUID [[maybe_unused]] static struct FluidBase *FluidBase = nullptr;
#else
#define JUMPTABLE_FLUID struct FluidBase *FluidBase = nullptr;
#endif

struct FluidBase {
#ifndef PARASOL_STATIC
   ERR (*_SetVariable)(objScript *Script, CSTRING Name, int Type, ...);
#endif // PARASOL_STATIC
};

#if !defined(PARASOL_STATIC) and !defined(PRV_FLUID_MODULE)
extern struct FluidBase *FluidBase;
namespace fl {
template<class... Args> ERR SetVariable(objScript *Script, CSTRING Name, int Type, Args... Tags) { return FluidBase->_SetVariable(Script,Name,Type,Tags...); }
} // namespace
#else
namespace fl {
extern ERR SetVariable(objScript *Script, CSTRING Name, int Type, ...);
} // namespace
#endif // PARASOL_STATIC

// Fluid class definition

#define VER_FLUID (1.000000)

// Fluid methods

namespace sc {
struct Step { static const AC id = AC(-20); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct ClearBreakpoint { CSTRING File; int Line; static const AC id = AC(-21); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct SetBreakpoint { CSTRING File; int Line; static const AC id = AC(-22); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objFluid : public objScript {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::FLUID;
   static constexpr CSTRING CLASS_NAME = "Fluid";

   using create = pf::Create<objFluid>;

   // Action stubs

   inline ERR activate() noexcept { return Action(AC::Activate, this, nullptr); }
   inline ERR dataFeed(OBJECTPTR Object, DATA Datatype, const void *Buffer, int Size) noexcept {
      struct acDataFeed args = { Object, Datatype, Buffer, Size };
      return Action(AC::DataFeed, this, &args);
   }
   inline ERR init() noexcept { return InitObject(this); }
   inline ERR saveToObject(OBJECTPTR Dest, CLASSID ClassID = CLASSID::NIL) noexcept {
      struct acSaveToObject args = { Dest, { ClassID } };
      return Action(AC::SaveToObject, this, &args);
   }
   inline ERR step() noexcept {
      return(Action(AC(-20), this, nullptr));
   }
   inline ERR clearBreakpoint(CSTRING File, int Line) noexcept {
      struct sc::ClearBreakpoint args = { File, Line };
      return(Action(AC(-21), this, &args));
   }
   inline ERR setBreakpoint(CSTRING File, int Line) noexcept {
      struct sc::SetBreakpoint args = { File, Line };
      return(Action(AC(-22), this, &args));
   }

   // Customised field setting

};

