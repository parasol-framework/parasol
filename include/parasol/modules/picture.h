#pragma once

// Name:      picture.h
// Copyright: Paul Manias © 2001-2023
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_PICTURE (1)

#include <parasol/modules/display.h>

class objPicture;

// Flags for the Picture class.

enum class PCF : ULONG {
   NIL = 0,
   RESIZE_X = 0x00000001,
   NO_PALETTE = 0x00000002,
   SCALABLE = 0x00000004,
   RESIZE_Y = 0x00000008,
   RESIZE = 0x00000009,
   NEW = 0x00000010,
   MASK = 0x00000020,
   ALPHA = 0x00000040,
   LAZY = 0x00000080,
   FORCE_ALPHA_32 = 0x00000100,
};

DEFINE_ENUM_FLAG_OPERATORS(PCF)

// Picture class definition

#define VER_PICTURE (1.000000)

class objPicture : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_PICTURE;
   static constexpr CSTRING CLASS_NAME = "Picture";

   using create = pf::Create<objPicture>;

   objBitmap * Bitmap;    // Represents a picture's image data.
   objBitmap * Mask;      // Refers to a Bitmap that imposes a mask on the image.
   PCF  Flags;            // Optional initialisation flags.
   LONG DisplayHeight;    // The preferred height to use when displaying the image.
   LONG DisplayWidth;     // The preferred width to use when displaying the image.
   LONG Quality;          // Defines the quality level to use when saving the image.
   LONG FrameRate;        // Refresh & redraw the picture X times per second.  Used by pictures that have an animation refresh rate

   // Action stubs

   inline ERROR activate() { return Action(AC_Activate, this, NULL); }
   inline ERROR init() { return InitObject(this); }
   inline ERROR query() { return Action(AC_Query, this, NULL); }
   template <class T, class U> ERROR read(APTR Buffer, T Size, U *Result) {
      static_assert(std::is_integral<U>::value, "Result value must be an integer type");
      static_assert(std::is_integral<T>::value, "Size value must be an integer type");
      ERROR error;
      const LONG bytes = (Size > 0x7fffffff) ? 0x7fffffff : Size;
      struct acRead read = { (BYTE *)Buffer, bytes };
      if (!(error = Action(AC_Read, this, &read))) *Result = static_cast<U>(read.Result);
      else *Result = 0;
      return error;
   }
   template <class T> ERROR read(APTR Buffer, T Size) {
      static_assert(std::is_integral<T>::value, "Size value must be an integer type");
      const LONG bytes = (Size > 0x7fffffff) ? 0x7fffffff : Size;
      struct acRead read = { (BYTE *)Buffer, bytes };
      return Action(AC_Read, this, &read);
   }
   inline ERROR refresh() { return Action(AC_Refresh, this, NULL); }
   inline ERROR saveImage(OBJECTPTR Dest, CLASSID ClassID = 0) {
      struct acSaveImage args = { Dest, { ClassID } };
      return Action(AC_SaveImage, this, &args);
   }
   inline ERROR saveToObject(OBJECTPTR Dest, CLASSID ClassID = 0) {
      struct acSaveToObject args = { Dest, { ClassID } };
      return Action(AC_SaveToObject, this, &args);
   }
   inline ERROR seek(DOUBLE Offset, SEEK Position = SEEK::CURRENT) {
      struct acSeek args = { Offset, Position };
      return Action(AC_Seek, this, &args);
   }
   inline ERROR seekStart(DOUBLE Offset)   { return seek(Offset, SEEK::START); }
   inline ERROR seekEnd(DOUBLE Offset)     { return seek(Offset, SEEK::END); }
   inline ERROR seekCurrent(DOUBLE Offset) { return seek(Offset, SEEK::CURRENT); }
   inline ERROR write(CPTR Buffer, LONG Size, LONG *Result = NULL) {
      ERROR error;
      struct acWrite write = { (BYTE *)Buffer, Size };
      if (!(error = Action(AC_Write, this, &write))) {
         if (Result) *Result = write.Result;
      }
      else if (Result) *Result = 0;
      return error;
   }
   inline ERROR write(std::string Buffer, LONG *Result = NULL) {
      ERROR error;
      struct acWrite write = { (BYTE *)Buffer.c_str(), LONG(Buffer.size()) };
      if (!(error = Action(AC_Write, this, &write))) {
         if (Result) *Result = write.Result;
      }
      else if (Result) *Result = 0;
      return error;
   }
   inline LONG writeResult(CPTR Buffer, LONG Size) {
      struct acWrite write = { (BYTE *)Buffer, Size };
      if (!Action(AC_Write, this, &write)) return write.Result;
      else return 0;
   }

   // Customised field setting

   inline ERROR setFlags(const PCF Value) {
      this->Flags = Value;
      return ERR_Okay;
   }

   inline ERROR setDisplayHeight(const LONG Value) {
      this->DisplayHeight = Value;
      return ERR_Okay;
   }

   inline ERROR setDisplayWidth(const LONG Value) {
      this->DisplayWidth = Value;
      return ERR_Okay;
   }

   inline ERROR setQuality(const LONG Value) {
      this->Quality = Value;
      return ERR_Okay;
   }

   template <class T> inline ERROR setAuthor(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[18];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setCopyright(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[8];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setDescription(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setDisclaimer(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[10];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERROR setHeader(APTR Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, 0x08000500, Value, 1);
   }

   template <class T> inline ERROR setPath(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[13];
      return field->WriteValue(target, field, 0x08800500, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setSoftware(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[20];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setTitle(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

};

