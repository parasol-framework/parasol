#pragma once

// Name:      fluid.h
// Copyright: Paul Manias © 2006-2023
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
   ERROR (*_SetVariable)(OBJECTPTR Script, CSTRING Name, LONG Type, ...);
#endif // PARASOL_STATIC
};

#ifndef PRV_FLUID_MODULE
#ifndef PARASOL_STATIC
extern struct FluidBase *FluidBase;
template<class... Args> ERROR flSetVariable(OBJECTPTR Script, CSTRING Name, LONG Type, Args... Tags) { return FluidBase->_SetVariable(Script,Name,Type,Tags...); }
#else
extern "C" {
extern ERROR flSetVariable(OBJECTPTR Script, CSTRING Name, LONG Type, ...);
}
#endif // PARASOL_STATIC
#endif

