#pragma once

// Name:      fluid.h
// Copyright: Paul Manias © 2006-2025
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_FLUID (1)

class objFluid;

#ifdef PARASOL_STATIC
#define JUMPTABLE_FLUID static struct FluidBase *FluidBase;
#else
#define JUMPTABLE_FLUID struct FluidBase *FluidBase;
#endif

struct FluidBase {
#ifndef PARASOL_STATIC
   ERR (*_SetVariable)(OBJECTPTR Script, CSTRING Name, int Type, ...);
#endif // PARASOL_STATIC
};

#ifndef PRV_FLUID_MODULE
#ifndef PARASOL_STATIC
extern struct FluidBase *FluidBase;
namespace fl {
template<class... Args> ERR SetVariable(OBJECTPTR Script, CSTRING Name, int Type, Args... Tags) { return FluidBase->_SetVariable(Script,Name,Type,Tags...); }
} // namespace
#else
namespace fl {
extern ERR SetVariable(OBJECTPTR Script, CSTRING Name, int Type, ...);
} // namespace
#endif // PARASOL_STATIC
#endif

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

