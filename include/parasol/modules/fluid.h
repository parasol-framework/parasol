#pragma once

// Name:      fluid.h
// Copyright: Paul Manias © 2006-2023
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_FLUID (1)

extern struct FluidBase *FluidBase;
struct FluidBase {
   ERROR (*_SetVariable)(OBJECTPTR Script, CSTRING Name, LONG Type, ...);
};

#ifndef PRV_FLUID_MODULE
template<class... Args> ERROR flSetVariable(OBJECTPTR Script, CSTRING Name, LONG Type, Args... Tags) { return FluidBase->_SetVariable(Script,Name,Type,Tags...); }
#endif

