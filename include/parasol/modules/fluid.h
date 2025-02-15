#pragma once

// Name:      fluid.h
// Copyright: Paul Manias © 2006-2025
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_FLUID (1)

#ifdef PARASOL_STATIC
#define JUMPTABLE_FLUID static struct FluidBase *FluidBase;
#else
#define JUMPTABLE_FLUID struct FluidBase *FluidBase;
#endif

struct FluidBase {
#ifndef PARASOL_STATIC
   ERR (*_SetVariable)(OBJECTPTR Script, CSTRING Name, LONG Type, ...);
#endif // PARASOL_STATIC
};

#ifndef PRV_FLUID_MODULE
#ifndef PARASOL_STATIC
extern struct FluidBase *FluidBase;
namespace fl {
template<class... Args> ERR SetVariable(OBJECTPTR Script, CSTRING Name, LONG Type, Args... Tags) { return FluidBase->_SetVariable(Script,Name,Type,Tags...); }
} // namespace
#else
namespace fl {
extern ERR SetVariable(OBJECTPTR Script, CSTRING Name, LONG Type, ...);
} // namespace
#endif // PARASOL_STATIC
#endif

