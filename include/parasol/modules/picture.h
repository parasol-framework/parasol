#ifndef MODULES_PICTURE
#define MODULES_PICTURE 1

// Name:      picture.h
// Copyright: Paul Manias © 2001-2022
// Generator: idl-c

#ifndef MAIN_H
#include <parasol/main.h>
#endif

#define MODVERSION_PICTURE (1)

#ifndef MODULES_DISPLAY_H
#include <parasol/modules/display.h>
#endif

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

typedef struct rkPicture {
   OBJECT_HEADER
   struct rkBitmap * Bitmap;    // Bitmap details
   struct rkBitmap * Mask;      // Monochrome bit mask or alpha channel
   LONG Flags;                  // Optional flags
   LONG DisplayHeight;          // Preferred display height
   LONG DisplayWidth;           // Preferred display width
   LONG Quality;                // Quality rating when saving image (0% low, 100% high)
   LONG FrameRate;              // Refresh & redraw the picture X times per second.  Used by pictures that have an animation refresh rate

#ifdef PRV_PICTURE
 PRV_PICTURE_FIELDS 
#endif
} objPicture;

#endif
