#pragma once

// Name:      xrandr.h
// Copyright: Paul Manias © 2014-2017
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_XRANDR (1)

extern struct XRandRBase *XRandRBase;
struct XRandRBase {
   ERROR (*_SetDisplayMode)(LONG * Width, LONG * Height);
   LONG (*_Notify)(APTR XEvent);
   void (*_SelectInput)(LONG Window);
   LONG (*_GetDisplayTotal)(void);
   APTR (*_GetDisplayMode)(LONG Index);
};

#ifndef PRV_XRANDR_MODULE
inline ERROR xrSetDisplayMode(LONG * Width, LONG * Height) { return XRandRBase->_SetDisplayMode(Width,Height); }
inline LONG xrNotify(APTR XEvent) { return XRandRBase->_Notify(XEvent); }
inline void xrSelectInput(LONG Window) { return XRandRBase->_SelectInput(Window); }
inline LONG xrGetDisplayTotal(void) { return XRandRBase->_GetDisplayTotal(); }
inline APTR xrGetDisplayMode(LONG Index) { return XRandRBase->_GetDisplayMode(Index); }
#endif

struct xrMode {
   LONG Width;    // Horizontal
   LONG Height;   // Vertical
   LONG Depth;    // bit depth
};

