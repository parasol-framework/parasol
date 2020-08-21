#ifndef MODULES_FLUID
#define MODULES_FLUID 1

// Name:      fluid.h
// Copyright: Paul Manias © 2006-2020
// Generator: idl-c

#ifndef MAIN_H
#include <parasol/main.h>
#endif

#define MODVERSION_FLUID (1)

struct FluidBase {
   ERROR (*_SetVariable)(APTR, CSTRING, LONG, ...);
};

#ifndef PRV_FLUID_MODULE
#define flSetVariable(...) (FluidBase->_SetVariable)(__VA_ARGS__)
#endif

#endif
