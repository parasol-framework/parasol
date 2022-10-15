#ifndef MODULES_SVG
#define MODULES_SVG 1

// Name:      svg.h
// Copyright: Paul Manias © 2010-2022
// Generator: idl-c

#ifndef MAIN_H
#include <parasol/main.h>
#endif

#define MODVERSION_SVG (1)

// SVG flags.

#define SVF_AUTOSCALE 0x00000001
#define SVF_ALPHA 0x00000002

// SVG class definition

#define VER_SVG (1.000000)

typedef struct rkSVG {
   OBJECT_HEADER
   OBJECTPTR Target;    // Refers to the target of the generated SVG scene.
   STRING    Path;      // The location of the source SVG data.
   STRING    Title;     // Automatically defined if the title element is used in the SVG source document.
   LONG      Frame;     // Draw the SVG only when this frame number is a match to the target surface frame number.
   LONG      Flags;     // Optional flags.
   LONG      FrameRate; // Maximum frame rate to use for animation.

#ifdef PRV_SVG
   objVectorScene *Scene;
   STRING Folder;
   OBJECTPTR Viewport; // First viewport (the <svg> tag) to be created on parsing the SVG document.
   FUNCTION FrameCallback;
   struct svgAnimation *Animations;
   std::unordered_map<std::string, svgID> IDs;
   svgInherit *Inherit;
   DOUBLE SVGVersion;
   TIMER AnimationTimer;
   UBYTE Animated:1;
   UBYTE PreserveWS:1; // Preserve white-space
  
#endif
} objSVG;

// SVG methods

#define MT_SvgRender -1

struct svgRender { struct rkBitmap * Bitmap; LONG X; LONG Y; LONG Width; LONG Height;  };

INLINE ERROR svgRender(APTR Ob, struct rkBitmap * Bitmap, LONG X, LONG Y, LONG Width, LONG Height) {
   struct svgRender args = { Bitmap, X, Y, Width, Height };
   return(Action(MT_SvgRender, (OBJECTPTR)Ob, &args));
}


#endif
