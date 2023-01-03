#pragma once

// Name:      picture.h
// Copyright: Paul Manias © 2001-2022
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_PICTURE (1)

#include <parasol/modules/display.h>

class objPicture;

// Flags for the Picture class.

#define PCF_RESIZE_X 0x00000001
#define PCF_NO_PALETTE 0x00000002
#define PCF_SCALABLE 0x00000004
#define PCF_RESIZE_Y 0x00000008
#define PCF_RESIZE 0x00000009
#define PCF_NEW 0x00000010
#define PCF_MASK 0x00000020
#define PCF_ALPHA 0x00000040
#define PCF_LAZY 0x00000080
#define PCF_FORCE_ALPHA_32 0x00000100

// Picture class definition

#define VER_PICTURE (1.000000)

class objPicture : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_PICTURE;
   static constexpr CSTRING CLASS_NAME = "Picture";

   using create = parasol::Create<objPicture>;

   objBitmap * Bitmap;    // Represents a picture's image data.
   objBitmap * Mask;      // Refers to a Bitmap that imposes a mask on the image.
   LONG Flags;            // Optional initialisation flags.
   LONG DisplayHeight;    // The preferred height to use when displaying the image.
   LONG DisplayWidth;     // The preferred width to use when displaying the image.
   LONG Quality;          // Defines the quality level to use when saving the image.
   LONG FrameRate;        // Refresh & redraw the picture X times per second.  Used by pictures that have an animation refresh rate
};

