#pragma once

// Name:      svg.h
// Copyright: Paul Manias © 2010-2025
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_SVG (1)

class objSVG;

// SVG flags.

enum class SVF : uint32_t {
   NIL = 0,
   AUTOSCALE = 0x00000001,
   ALPHA = 0x00000002,
   ENFORCE_TRACKING = 0x00000004,
};

DEFINE_ENUM_FLAG_OPERATORS(SVF)

// SVG class definition

#define VER_SVG (1.000000)

// SVG methods

namespace svg {
struct Render { objBitmap * Bitmap; int X; int Y; int Width; int Height; static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct ParseSymbol { CSTRING ID; objVectorViewport * Viewport; static const AC id = AC(-2); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objSVG : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::SVG;
   static constexpr CSTRING CLASS_NAME = "SVG";

   using create = pf::Create<objSVG>;

   OBJECTPTR Target;    // The container object for new SVG content can be declared here.
   STRING    Path;      // A path referring to an SVG file.
   STRING    Title;     // The title of the SVG document.
   STRING    Statement; // A string containing SVG data.
   int       Frame;     // Forces the graphics to be drawn to a specific frame.
   SVF       Flags;     // Optional flags.
   int       FrameRate; // The maximum frame rate to use when animating a vector scene.

   // Action stubs

   inline ERR activate() noexcept { return Action(AC::Activate, this, NULL); }
   inline ERR dataFeed(OBJECTPTR Object, DATA Datatype, const void *Buffer, int Size) noexcept {
      struct acDataFeed args = { Object, Datatype, Buffer, Size };
      return Action(AC::DataFeed, this, &args);
   }
   inline ERR deactivate() noexcept { return Action(AC::Deactivate, this, NULL); }
   inline ERR init() noexcept { return InitObject(this); }
   inline ERR saveImage(OBJECTPTR Dest, CLASSID ClassID = CLASSID::NIL) noexcept {
      struct acSaveImage args = { Dest, { ClassID } };
      return Action(AC::SaveImage, this, &args);
   }
   inline ERR saveToObject(OBJECTPTR Dest, CLASSID ClassID = CLASSID::NIL) noexcept {
      struct acSaveToObject args = { Dest, { ClassID } };
      return Action(AC::SaveToObject, this, &args);
   }
   inline ERR render(objBitmap * Bitmap, int X, int Y, int Width, int Height) noexcept {
      struct svg::Render args = { Bitmap, X, Y, Width, Height };
      return(Action(AC(-1), this, &args));
   }
   inline ERR parseSymbol(CSTRING ID, objVectorViewport * Viewport) noexcept {
      struct svg::ParseSymbol args = { ID, Viewport };
      return(Action(AC(-2), this, &args));
   }

   // Customised field setting

   inline ERR setTarget(OBJECTPTR Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[7];
      return field->WriteValue(target, field, 0x08000501, Value, 1);
   }

   template <class T> inline ERR setPath(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERR setTitle(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[6];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERR setStatement(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[10];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERR setFrame(const int Value) noexcept {
      this->Frame = Value;
      return ERR::Okay;
   }

   inline ERR setFlags(const SVF Value) noexcept {
      this->Flags = Value;
      return ERR::Okay;
   }

   inline ERR setFrameRate(const int Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   template <class T> inline ERR setColour(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[15];
      return field->WriteValue(target, field, 0x08800308, to_cstring(Value), 1);
   }

   inline ERR setFrameCallback(const FUNCTION Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

};

