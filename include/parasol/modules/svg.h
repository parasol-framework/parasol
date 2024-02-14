#pragma once

// Name:      svg.h
// Copyright: Paul Manias © 2010-2024
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_SVG (1)

class objSVG;

// SVG flags.

enum class SVF : ULONG {
   NIL = 0,
   AUTOSCALE = 0x00000001,
   ALPHA = 0x00000002,
};

DEFINE_ENUM_FLAG_OPERATORS(SVF)

// SVG class definition

#define VER_SVG (1.000000)

// SVG methods

#define MT_SvgRender -1
#define MT_SvgParseSymbol -2

struct svgRender { objBitmap * Bitmap; LONG X; LONG Y; LONG Width; LONG Height;  };
struct svgParseSymbol { CSTRING ID; objVectorViewport * Viewport;  };

INLINE ERROR svgRender(APTR Ob, objBitmap * Bitmap, LONG X, LONG Y, LONG Width, LONG Height) noexcept {
   struct svgRender args = { Bitmap, X, Y, Width, Height };
   return(Action(MT_SvgRender, (OBJECTPTR)Ob, &args));
}

INLINE ERROR svgParseSymbol(APTR Ob, CSTRING ID, objVectorViewport * Viewport) noexcept {
   struct svgParseSymbol args = { ID, Viewport };
   return(Action(MT_SvgParseSymbol, (OBJECTPTR)Ob, &args));
}


class objSVG : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_SVG;
   static constexpr CSTRING CLASS_NAME = "SVG";

   using create = pf::Create<objSVG>;

   OBJECTPTR Target;    // The root Viewport that is generated during SVG initialisation can be created as a child of this target object.
   STRING    Path;      // A path referring to an SVG file.
   STRING    Title;     // The title of the SVG document.
   STRING    Statement; // A string containing SVG data.
   LONG      Frame;     // Forces the graphics to be drawn to a specific frame.
   SVF       Flags;     // Optional flags.
   LONG      FrameRate; // The maximum frame rate to use when animating a vector scene.

   // Action stubs

   inline ERROR activate() noexcept { return Action(AC_Activate, this, NULL); }
   inline ERROR dataFeed(OBJECTPTR Object, DATA Datatype, const void *Buffer, LONG Size) noexcept {
      struct acDataFeed args = { Object, Datatype, Buffer, Size };
      return Action(AC_DataFeed, this, &args);
   }
   inline ERROR deactivate() noexcept { return Action(AC_Deactivate, this, NULL); }
   inline ERROR init() noexcept { return InitObject(this); }
   inline ERROR saveImage(OBJECTPTR Dest, CLASSID ClassID = 0) noexcept {
      struct acSaveImage args = { Dest, { ClassID } };
      return Action(AC_SaveImage, this, &args);
   }
   inline ERROR saveToObject(OBJECTPTR Dest, CLASSID ClassID = 0) noexcept {
      struct acSaveToObject args = { Dest, { ClassID } };
      return Action(AC_SaveToObject, this, &args);
   }

   // Customised field setting

   inline ERROR setTarget(OBJECTPTR Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[7];
      return field->WriteValue(target, field, 0x08000501, Value, 1);
   }

   template <class T> inline ERROR setPath(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setTitle(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[6];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setStatement(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[10];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERROR setFrame(const LONG Value) noexcept {
      this->Frame = Value;
      return ERR_Okay;
   }

   inline ERROR setFlags(const SVF Value) noexcept {
      this->Flags = Value;
      return ERR_Okay;
   }

   inline ERROR setFrameRate(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setFrameCallback(const FUNCTION Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

};

