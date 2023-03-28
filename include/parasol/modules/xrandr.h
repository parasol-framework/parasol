#ifndef MODULES_XRANDR
#define MODULES_XRANDR 1

// Name:      xrandr.h
// Copyright: Paul Manias © 2014-2017
// Generator: idl-c

#ifndef MAIN_H
#include <parasol/main.h>
#endif

#define MODVERSION_XRANDR (1)

struct XRandRBase {
   ERROR (*_SetDisplayMode)(LONG *, LONG *);
   LONG (*_Notify)(APTR);
   void (*_SelectInput)(LONG);
   LONG (*_GetDisplayTotal)(void);
   APTR (*_GetDisplayMode)(LONG);
};

#ifndef PRV_XRANDR_MODULE
#define xrSetDisplayMode(...) (XRandRBase->_SetDisplayMode)(__VA_ARGS__)
#define xrNotify(...) (XRandRBase->_Notify)(__VA_ARGS__)
#define xrSelectInput(...) (XRandRBase->_SelectInput)(__VA_ARGS__)
#define xrGetDisplayTotal(...) (XRandRBase->_GetDisplayTotal)(__VA_ARGS__)
#define xrGetDisplayMode(...) (XRandRBase->_GetDisplayMode)(__VA_ARGS__)
#endif

struct xrMode {
   LONG Width;    // Horizontal
   LONG Height;   // Vertical
   LONG Depth;    // bit depth
};

#endif
