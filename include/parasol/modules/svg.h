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

   using create = pf::Create<objSVG>;

   OBJECTPTR Target;    // The root Viewport that is generated during SVG initialisation can be created as a child of this target object.
   STRING    Path;      // The location of the source SVG data.
   STRING    Title;     // The title of the SVG document.
   LONG      Frame;     // Forces the graphics to be drawn to a specific frame.
   LONG      Flags;     // Optional flags.
   LONG      FrameRate; // The maximum frame rate to use when animating a vector scene.

   // Action stubs

   inline ERROR activate() { return Action(AC_Activate, this, NULL); }
   inline ERROR dataFeed(OBJECTPTR Object, LONG Datatype, const void *Buffer, LONG Size) {
      struct acDataFeed args = { Object, Datatype, Buffer, Size };
      return Action(AC_DataFeed, this, &args);
   }
   inline ERROR deactivate() { return Action(AC_Deactivate, this, NULL); }
   inline ERROR init() { return InitObject(this); }
   inline ERROR saveImage(OBJECTPTR Dest, CLASSID ClassID = 0) {
      struct acSaveImage args = { Dest, { ClassID } };
      return Action(AC_SaveImage, this, &args);
   }
   inline ERROR saveToObject(OBJECTPTR Dest, CLASSID ClassID = 0) {
      struct acSaveToObject args = { Dest, { ClassID } };
      return Action(AC_SaveToObject, this, &args);
   }

   // Customised field setting

   inline ERROR setTarget(OBJECTPTR Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[7];
      return field->WriteValue(target, field, 0x08000501, Value, 1);
   }

   inline ERROR setPath(STRING Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, 0x08800300, Value, 1);
   }

   inline ERROR setTitle(STRING Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[6];
      return field->WriteValue(target, field, 0x08800300, Value, 1);
   }

   inline ERROR setFrame(const LONG Value) {
      this->Frame = Value;
      return ERR_Okay;
   }

   inline ERROR setFlags(const LONG Value) {
      this->Flags = Value;
      return ERR_Okay;
   }

   inline ERROR setFrameRate(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setFrameCallback(const FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[10];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

};

