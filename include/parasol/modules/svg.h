#pragma once

// Name:      svg.h
// Copyright: Paul Manias © 2010-2023
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_SVG (1)

class objSVG;

// SVG flags.

#define SVF_AUTOSCALE 0x00000001
#define SVF_ALPHA 0x00000002

// SVG class definition

#define VER_SVG (1.000000)

// SVG methods

#define MT_SvgRender -1

struct svgRender { objBitmap * Bitmap; LONG X; LONG Y; LONG Width; LONG Height;  };

INLINE ERROR svgRender(APTR Ob, objBitmap * Bitmap, LONG X, LONG Y, LONG Width, LONG Height) {
   struct svgRender args = { Bitmap, X, Y, Width, Height };
   return(Action(MT_SvgRender, (OBJECTPTR)Ob, &args));
}


class objSVG : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_SVG;
   static constexpr CSTRING CLASS_NAME = "SVG";

   using create = parasol::Create<objSVG>;

   OBJECTPTR Target;    // The root Viewport that is generated during SVG initialisation can be created as a child of this target object.
   STRING    Path;      // The location of the source SVG data.
   STRING    Title;     // The title of the SVG document.
   LONG      Frame;     // Forces the graphics to be drawn to a specific frame.
   LONG      Flags;     // Optional flags.
   LONG      FrameRate; // The maximum frame rate to use when animating a vector scene.

   // Action stubs

   inline ERROR activate() { return Action(AC_Activate, this, NULL); }
   inline ERROR dataFeed(OBJECTID ObjectID, LONG Datatype, const void *Buffer, LONG Size) {
      struct acDataFeed args = { { ObjectID }, { Datatype }, Buffer, Size };
      return Action(AC_DataFeed, this, &args);
   }
   inline ERROR deactivate() { return Action(AC_Deactivate, this, NULL); }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
   inline ERROR saveImage(OBJECTID DestID, CLASSID ClassID) {
      struct acSaveImage args = { { DestID }, { ClassID } };
      return Action(AC_SaveImage, this, &args);
   }
   inline ERROR saveToObject(OBJECTID DestID, CLASSID ClassID) {
      struct acSaveToObject args = { { DestID }, { ClassID } };
      return Action(AC_SaveToObject, this, &args);
   }
};

