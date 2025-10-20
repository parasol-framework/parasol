#pragma once

// Name:      fluid.h
// Copyright: Paul Manias © 2006-2025
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_FLUID (1)

#ifdef PARASOL_STATIC
#define JUMPTABLE_FLUID [[maybe_unused]] static struct FluidBase *FluidBase = nullptr;
#else
#define JUMPTABLE_FLUID struct FluidBase *FluidBase = nullptr;
#endif

struct FluidBase {
#ifndef PARASOL_STATIC
   ERR (*_SetVariable)(OBJECTPTR Script, CSTRING Name, int Type, ...);
#endif // PARASOL_STATIC
};

#if !defined(PARASOL_STATIC) and !defined(PRV_FLUID_MODULE)
extern struct FluidBase *FluidBase;
namespace fl {
template<class... Args> ERR SetVariable(OBJECTPTR Script, CSTRING Name, int Type, Args... Tags) { return FluidBase->_SetVariable(Script,Name,Type,Tags...); }
} // namespace
#else
namespace fl {
extern ERR SetVariable(OBJECTPTR Script, CSTRING Name, int Type, ...);
} // namespace
#endif // PARASOL_STATIC

