// Fast conversion table routines for rgb -> linear and vice versa.  Implemented as a singleton, so define
// glLinearRGB to appear once in your binary and use it directly.

#ifndef LINEAR_RGB_H
#define LINEAR_RGB_H 1

#include <math.h>

class rgb_to_linear {
private:
   inline static UBYTE conv_r2l(DOUBLE X) {
      if (X <= 0.04045) X /= 12.92;
      else X = std::pow((X + 0.055) / 1.055, 2.4);
      return F2T((X * 255.0) + 0.5);
   }

   inline static UBYTE conv_l2r(DOUBLE X) {
      LONG ix;

      if (X < 0.0031308) ix = F2T(((X * 12.92) * 255.0) + 0.5);
      else ix = F2T(((std::pow(X, 1.0 / 2.4) * 1.055 - 0.055) * 255.0) + 0.5);

      if (ix < 0) return 0;
      else if (ix > 255) return 255;
      else return ix;
   }

public:
   rgb_to_linear() {
      // Initialise conversion tables
      for (LONG i=0; i < 256; i++) {
         r2l[i] = conv_r2l((DOUBLE)i * (1.0 / 255.0));
         l2r[i] = conv_l2r((DOUBLE)i * (1.0 / 255.0));
      }
   }

   inline UBYTE convert(const UBYTE Colour) {
      return r2l[Colour];
   }

   inline UBYTE invert(const UBYTE Colour) {
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

private:
   UBYTE r2l[256];
   UBYTE l2r[256];
};

extern rgb_to_linear glLinearRGB;

#endif // LINEAR_RGB_H
