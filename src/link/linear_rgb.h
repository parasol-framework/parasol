// Fast conversion table routines for rgb -> linear and vice versa.  Implemented as a singleton, so define
// glLinearRGB to appear once in your binary and use it directly.

#pragma once

#include <math.h>

class rgb_to_linear {
private:
   inline static UBYTE conv_r2l(DOUBLE X) {
      if (X <= 0.04045) X /= 12.92;
      else X = std::pow((X + 0.055) / 1.055, 2.4);
      return pf::F2T((X * 255.0) + 0.5);
   }

   inline static UBYTE conv_l2r(DOUBLE X) {
      LONG ix;

      if (X < 0.0031308) ix = pf::F2T(((X * 12.92) * 255.0) + 0.5);
      else ix = pf::F2T(((std::pow(X, 1.0 / 2.4) * 1.055 - 0.055) * 255.0) + 0.5);

      return (ix < 0) ? 0 : (ix > 255) ? 255 : ix;
   }

public:
   inline rgb_to_linear() {
      // Initialise conversion tables
      for (LONG i=0; i < 256; i++) {
         r2l[i] = conv_r2l((DOUBLE)i * (1.0 / 255.0));
         l2r[i] = conv_l2r((DOUBLE)i * (1.0 / 255.0));
      }
   }

   inline constexpr UBYTE convert(const UBYTE Colour) { // RGB to linear
      return r2l[Colour];
   }

   inline constexpr UBYTE invert(const UBYTE Colour) { // Linear to RGB
      return l2r[Colour];
   }

   // Notice that the alpha channel is not impacted by the RGB conversion.

   inline void convert(RGB8 &Colour) {
      Colour.Red   = r2l[Colour.Red];
      Colour.Green = r2l[Colour.Green];
      Colour.Blue  = r2l[Colour.Blue];
   }

   inline void invert(RGB8 &Colour) {
      Colour.Red   = l2r[Colour.Red];
      Colour.Green = l2r[Colour.Green];
      Colour.Blue  = l2r[Colour.Blue];
   }

   inline static FLOAT f_invert(FLOAT Value) {
      if (Value < 0.0031308) Value = Value * 12.92;
      else Value = std::pow(Value, 1.0 / 2.4) * 1.055 - 0.055;
      return (Value < 0) ? 0 : (Value > 255) ? 255 : Value;
   }

   inline static FLOAT f_convert(FLOAT Value) {
      if (Value <= 0.04045) return Value /= 12.92;
      else return std::pow((Value + 0.055) / 1.055, 2.4);
   }

   inline static void convert(FRGB &Colour) {
      if (Colour.Red <= 0.04045) Colour.Red /= 12.92;
      else Colour.Red = std::pow((Colour.Red + 0.055) / 1.055, 2.4);

      if (Colour.Green <= 0.04045) Colour.Green /= 12.92;
      else Colour.Green = std::pow((Colour.Green + 0.055) / 1.055, 2.4);

      if (Colour.Blue <= 0.04045) Colour.Blue /= 12.92;
      else Colour.Blue = std::pow((Colour.Blue + 0.055) / 1.055, 2.4);
   }

   inline static void invert(FRGB &Colour) {
      Colour.Red   = f_invert(Colour.Red);
      Colour.Green = f_invert(Colour.Green);
      Colour.Blue  = f_invert(Colour.Blue);
   }

private:
   UBYTE r2l[256];
   UBYTE l2r[256];
};

extern rgb_to_linear glLinearRGB;
