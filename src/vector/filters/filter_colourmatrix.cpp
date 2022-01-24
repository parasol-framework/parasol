/*****************************************************************************

This code was originally ported from Javascript.  The source code license follows.

******************************************************************************

ColourMatrix Class v2.1
released under MIT License (X11)
http://www.opensource.org/licenses/mit-license.php

Author: Mario Klingemann
http://www.quasimondo.com

Copyright (c) 2008 Mario Klingemann

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*****************************************************************************/

#include <array>

//****************************************************************************

#define CM_SIZE 20

enum { // Colour modes
   CM_MATRIX=1,
   CM_SATURATE,
   CM_HUE_ROTATE,
   CM_LUMINANCE_ALPHA,
   CM_CONTRAST,
   CM_BRIGHTNESS,
   CM_HUE,
   CM_DESATURATE,
   CM_COLOURISE
};

static const DOUBLE LUMA_R = 0.212671;
static const DOUBLE LUMA_G = 0.71516;
static const DOUBLE LUMA_B = 0.072169;
static const DOUBLE ONETHIRD = 1.0 / 3.0;

typedef std::array<DOUBLE, CM_SIZE> MATRIX;

static const MATRIX IDENTITY = { 1,0,0,0,0,   0,1,0,0,0,   0,0,1,0,0,   0,0,0,1,0 };

class ColourMatrix {
public:
   MATRIX matrix;
   ColourMatrix *preHue;
   ColourMatrix *postHue;

   ColourMatrix(const ColourMatrix &Copy) : preHue(0), postHue(0) {
      matrix = Copy.matrix;
   }

   ColourMatrix(const MATRIX Matrix) : preHue(0), postHue(0) {
      matrix = Matrix;
   }

   ColourMatrix() : preHue(0), postHue(0) {
      reset();
   }

   ~ColourMatrix() {
      delete preHue;
      delete postHue;
   }

   void reset() {
      matrix = IDENTITY;
   }

   DOUBLE & operator[] (int x) {
      return matrix[x];
   }

   void apply(MATRIX mat) {
      MATRIX temp;
      LONG i = 0;
      for (LONG y = 0; y < 4; y++) {
         for (LONG x = 0; x < 5; x++) {
            temp[i+x] = mat[i] * matrix[x] + mat[(i+1)] * matrix[(x+5)] + mat[(i+2)] * matrix[(x+10)] +
                        mat[(i+3)] * matrix[(x + 15)] + (x == 4 ? mat[(i+4)] : 0);
         }
         i += 5;
      }
      matrix = temp;
   }

   void invert() {
      apply(MATRIX {
         -1 ,  0,  0, 0, 255,
         0 , -1,  0, 0, 255,
         0 ,  0, -1, 0, 255,
         0,   0,  0, 1,   0
      });
   }

   /* s: Typical values come in the range 0.0 ... 2.0
            0.0 means 0% Saturation
            0.5 means 50% Saturation
            1.0 is 100% Saturation (aka no change)
            2.0 is 200% Saturation

         Other values outside of this range are possible: -1.0 will invert the hue but keep the luminance
   */

   void adjustSaturation(DOUBLE s) {
      apply(MATRIX {
         LUMA_R+(1.0-LUMA_R)*s, LUMA_G-(LUMA_G*s),     LUMA_B-(LUMA_B*s), 0, 0,
         LUMA_R-(LUMA_R*s),     LUMA_G+(1.0-LUMA_G)*s, LUMA_B-(LUMA_B*s), 0, 0,
         LUMA_R-(LUMA_R*s),     LUMA_G-(LUMA_G*s),     LUMA_B+(1.0-LUMA_B)*s, 0, 0,
         0, 0, 0, 1, 0
      });
   }

   /* Changes the contrast
      s: Typical values come in the range -1.0 ... 1.0
            -1.0 means no contrast (grey)
            0 means no change
            1.0 is high contrast
   */

   void adjustContrast(DOUBLE r, DOUBLE g = NAN, DOUBLE b = NAN)
   {
      if (isnan(g)) g = r;
      if (isnan(b)) b = r;
      r += 1;
      g += 1;
      b += 1;
      apply(MATRIX {
         r, 0, 0, 0, (128 * (1 - r)),
         0, g, 0, 0, (128 * (1 - g)),
         0, 0, b, 0, (128 * (1 - b)),
         0, 0, 0, 1, 0
      });
   }

   void adjustBrightness(DOUBLE r, DOUBLE g = NAN, DOUBLE b = NAN)
   {
      if (isnan(g)) g = r;
      if (isnan(b)) b = r;
      apply(MATRIX {
         1, 0, 0, 0, r,
         0, 1, 0, 0, g,
         0, 0, 1, 0, b,
         0, 0, 0, 1, 0
      });
   }

   void adjustHue(DOUBLE degrees)
   {
       degrees *= DEG2RAD;
       DOUBLE ccos = cos(degrees);
       DOUBLE csin = sin(degrees);
       apply(MATRIX {
          ((LUMA_R + (ccos * (1 - LUMA_R))) + (csin * -(LUMA_R))), ((LUMA_G + (ccos * -(LUMA_G))) + (csin * -(LUMA_G))), ((LUMA_B + (ccos * -(LUMA_B))) + (csin * (1 - LUMA_B))), 0, 0,
          ((LUMA_R + (ccos * -(LUMA_R))) + (csin * 0.143)), ((LUMA_G + (ccos * (1 - LUMA_G))) + (csin * 0.14)), ((LUMA_B + (ccos * -(LUMA_B))) + (csin * -0.283)), 0, 0,
          ((LUMA_R + (ccos * -(LUMA_R))) + (csin * -((1 - LUMA_R)))), ((LUMA_G + (ccos * -(LUMA_G))) + (csin * LUMA_G)), ((LUMA_B + (ccos * (1 - LUMA_B))) + (csin * LUMA_B)), 0, 0,
          0, 0, 0, 1, 0
       });
   }

   void rotateHue(DOUBLE degrees)
   {
      if (!initHue()) {
         apply(preHue->matrix);
         rotateBlue(degrees);
         apply(postHue->matrix);
      }
   }

   void luminance2Alpha() {
      apply(MATRIX {
         0, 0, 0, 0, 1,
         0, 0, 0, 0, 1,
         0, 0, 0, 0, 1,
         LUMA_R, LUMA_G, LUMA_B, 0, 0
      });
   }

   void adjustAlphaContrast(DOUBLE amount)
   {
       amount += 1;
       apply(MATRIX {
          1, 0, 0, 0, 0,
          0, 1, 0, 0, 0,
          0, 0, 1, 0, 0,
          0, 0, 0, amount, (128 * (1 - amount))
       });
   }

   // Values should tend to be between 0 and 1 for colourise, e.g. "1 0 0" will redden the entire image and eliminate
   // the G and B channels.  Values greater than 1 will tend to over-expose the image.  Lowering the amount parameter < 1
   // will allow you to tint the image.

   void colourise(DOUBLE r, DOUBLE g, DOUBLE b, DOUBLE amount=1)
   {
       DOUBLE inv_amount = (1.0 - amount);

       apply(MATRIX {
          (inv_amount + ((amount * r) * LUMA_R)), ((amount * r) * LUMA_G), ((amount * r) * LUMA_B), 0, 0,
          ((amount * g) * LUMA_R), (inv_amount + ((amount * g) * LUMA_G)), ((amount * g) * LUMA_B), 0, 0,
          ((amount * b) * LUMA_R), ((amount * b) * LUMA_G), (inv_amount + ((amount * b) * LUMA_B)), 0, 0,
          0, 0, 0, 1, 0
       });
   }

   void average(DOUBLE r=ONETHIRD, DOUBLE g=ONETHIRD, DOUBLE b=ONETHIRD)
   {
       apply(MATRIX {
          r, g, b, 0, 0,
          r, g, b, 0, 0,
          r, g, b, 0, 0,
          0, 0, 0, 1, 0
       });
   }

   void invertAlpha()
   {
      apply(MATRIX {
         1, 0, 0, 0, 0,
         0, 1, 0, 0, 0,
         0, 0, 1, 0, 0,
         0, 0, 0, -1, 255 });
   }

   void rotateRed(DOUBLE degrees) {
      rotateColour(degrees, 2, 1);
   }

   void rotateGreen(DOUBLE degrees) {
      rotateColour(degrees, 0, 2);
   }

   void rotateBlue(DOUBLE degrees) {
      rotateColour(degrees, 1, 0);
   }

   void rotateColour(DOUBLE degrees, LONG x, LONG y)
   {
      degrees *= DEG2RAD;
      auto mat = IDENTITY;
      mat[x + x * 5] = mat[y + y * 5] = cos(degrees);
      mat[y + x * 5] = sin(degrees);
      mat[x + y * 5] = -sin(degrees);
      apply(mat);
   }

   void shearRed(DOUBLE green, DOUBLE blue)
   {
      shearColour( 0, 1, green, 2, blue );
   }

   void shearGreen(DOUBLE red, DOUBLE blue)
   {
      shearColour( 1, 0, red, 2, blue );
   }

   void shearBlue(DOUBLE red, DOUBLE green)
   {
      shearColour( 2, 0, red, 1, green );
   }

   void shearColour(LONG x, LONG y1, DOUBLE d1, LONG y2, DOUBLE d2) {
      MATRIX mat = IDENTITY;
      mat[y1 + x * 5] = d1;
      mat[y2 + x * 5] = d2;
      apply( mat );
   }

   void transformVector(std::array<DOUBLE, 4> &values) {
      DOUBLE r = values[0] * matrix[0] + values[1] * matrix[1] + values[2] * matrix[2] + values[3] * matrix[3] + matrix[4];
      DOUBLE g = values[0] * matrix[5] + values[1] * matrix[6] + values[2] * matrix[7] + values[3] * matrix[8] + matrix[9];
      DOUBLE b = values[0] * matrix[10] + values[1] * matrix[11] + values[2] * matrix[12] + values[3] * matrix[13] + matrix[14];
      DOUBLE a = values[0] * matrix[15] + values[1] * matrix[16] + values[2] * matrix[17] + values[3] * matrix[18] + matrix[19];

      values[0] = r;
      values[1] = g;
      values[2] = b;
      values[3] = a;
   }

   ERROR initHue()
   {
      const DOUBLE greenRotation = 39.182655;
      UBYTE init = false;

      if (!init) {
         preHue = new (std::nothrow) ColourMatrix();
         postHue = new (std::nothrow) ColourMatrix();
         if ((!preHue) or (!postHue)) return ERR_AllocMemory;

         preHue->rotateRed(45);
         preHue->rotateGreen(-greenRotation);

         std::array<DOUBLE, 4> lum = { LUMA_R, LUMA_G, LUMA_B, 1.0 };
         preHue->transformVector(lum);

         DOUBLE red = lum[0] / lum[2];
         DOUBLE green = lum[1] / lum[2];

         preHue->shearBlue(red, green);

         postHue->shearBlue(-red, -green);
         postHue->rotateGreen(greenRotation);
         postHue->rotateRed(-45.0);

         init = true;
      }
      return ERR_Okay;
   }
};

//****************************************************************************

static void apply_cmatrix(objVectorFilter *Self, VectorEffect *Effect)
{
   auto bmp = Effect->Bitmap;
   if (bmp->BytesPerPixel != 4) return;
   if (!Effect->Colour.Matrix) return;

   const UBYTE A = bmp->ColourFormat->AlphaPos>>3;
   const UBYTE R = bmp->ColourFormat->RedPos>>3;
   const UBYTE G = bmp->ColourFormat->GreenPos>>3;
   const UBYTE B = bmp->ColourFormat->BluePos>>3;

   ColourMatrix &matrix = *Effect->Colour.Matrix;

   UBYTE *data = bmp->Data + (bmp->Clip.Left<<2) + (bmp->Clip.Top * bmp->LineWidth);

   for (LONG y=0; y < bmp->Clip.Bottom - bmp->Clip.Top; y++) {
      UBYTE *pixel = data + (bmp->LineWidth * y);
      for (LONG x=0; x < bmp->Clip.Right - bmp->Clip.Left; x++) {

         DOUBLE a = pixel[A];
         DOUBLE r = pixel[R];
         DOUBLE g = pixel[G];
         DOUBLE b = pixel[B];

         LONG r2 = 0.5 + (r * matrix[0]) + (g * matrix[1]) + (b * matrix[2]) + (a * matrix[3]) + matrix[4];
         LONG g2 = 0.5 + (r * matrix[5]) + (g * matrix[6]) + (b * matrix[7]) + (a * matrix[8]) + matrix[9];
         LONG b2 = 0.5 + (r * matrix[10]) + (g * matrix[11]) + (b * matrix[12]) + (a * matrix[13]) + matrix[14];
         LONG a2 = 0.5 + (r * matrix[15]) + (g * matrix[16]) + (b * matrix[17]) + (a * matrix[18]) + matrix[19];

         if (a2 < 0) pixel[A] = 0;
         else if (a2 > 255) pixel[A] = 255;
         else pixel[A] = a2;

         if (r2 < 0)   pixel[R] = 0;
         else if (r2 > 255) pixel[R] = 255;
         else pixel[R] = r2;

         if (g2 < 0) pixel[G] = 0;
         else if (g2 > 255) pixel[G] = 255;
         else pixel[G] = g2;

         if (b2 < 0) pixel[B] = 0;
         else if (b2 > 255) pixel[B] = 255;
         else pixel[B] = b2;

         pixel += 4;
      }
   }
}

//****************************************************************************
// Create a new colour matrix effect.

static ERROR create_cmatrix(objVectorFilter *Self, XMLTag *Tag)
{
   VectorEffect *effect;
   if (!(effect = add_effect(Self, FE_COLOURMATRIX))) return ERR_AllocMemory;

   MATRIX m = IDENTITY;
   for (LONG a=1; a < Tag->TotalAttrib; a++) {
      CSTRING val = Tag->Attrib[a].Value;
      if (!val) continue;

      ULONG hash = StrHash(Tag->Attrib[a].Name, FALSE);
      switch(hash) {
         case SVF_TYPE: {
            switch(StrHash(val, FALSE)) {
               case SVF_MATRIX:        effect->Colour.Mode = CM_MATRIX; break;
               case SVF_SATURATE:      effect->Colour.Mode = CM_SATURATE; break;
               case SVF_HUEROTATE:     effect->Colour.Mode = CM_HUE_ROTATE; break;
               case SVF_LUMINANCETOALPHA: effect->Colour.Mode = CM_LUMINANCE_ALPHA; break;
               // These are special modes that are not included by SVG
               case SVF_CONTRAST:      effect->Colour.Mode = CM_CONTRAST; break;
               case SVF_BRIGHTNESS:    effect->Colour.Mode = CM_BRIGHTNESS; break;
               case SVF_HUE:           effect->Colour.Mode = CM_HUE; break;
               case SVF_COLOURISE:     effect->Colour.Mode = CM_COLOURISE; break;
               case SVF_DESATURATE:    effect->Colour.Mode = CM_DESATURATE; break;
               // These are special modes that are not included by SVG
               case SVF_PROTANOPIA:    effect->Colour.Mode = CM_MATRIX; m = MATRIX { 0.567,0.433,0,0,0, 0.558,0.442,0,0,0, 0,0.242,0.758,0,0, 0,0,0,1,0 }; break;
               case SVF_PROTANOMALY:   effect->Colour.Mode = CM_MATRIX; m = MATRIX { 0.817,0.183,0,0,0, 0.333,0.667,0,0,0, 0,0.125,0.875,0,0, 0,0,0,1,0 }; break;
               case SVF_DEUTERANOPIA:  effect->Colour.Mode = CM_MATRIX; m = MATRIX { 0.625,0.375,0,0,0, 0.7,0.3,0,0,0, 0,0.3,0.7,0,0, 0,0,0,1,0 }; break;
               case SVF_DEUTERANOMALY: effect->Colour.Mode = CM_MATRIX; m = MATRIX { 0.8,0.2,0,0,0, 0.258,0.742,0,0,0, 0,0.142,0.858,0,0, 0,0,0,1,0 }; break;
               case SVF_TRITANOPIA:    effect->Colour.Mode = CM_MATRIX; m = MATRIX { 0.95,0.05,0,0,0, 0,0.433,0.567,0,0, 0,0.475,0.525,0,0, 0,0,0,1,0 }; break;
               case SVF_TRITANOMALY:   effect->Colour.Mode = CM_MATRIX; m = MATRIX { 0.967,0.033,0,0,0, 0,0.733,0.267,0,0, 0,0.183,0.817,0,0, 0,0,0,1,0 }; break;
               case SVF_ACHROMATOPSIA: effect->Colour.Mode = CM_MATRIX; m = MATRIX { 0.299,0.587,0.114,0,0, 0.299,0.587,0.114,0,0, 0.299,0.587,0.114,0,0, 0,0,0,1,0 }; break;
               case SVF_ACHROMATOMALY: effect->Colour.Mode = CM_MATRIX; m = MATRIX { 0.618,0.320,0.062,0,0, 0.163,0.775,0.062,0,0, 0.163,0.320,0.516,0,0, 0,0,0,1,0 }; break;

               default:
                  LogErrorMsg("Unrecognised colour matrix type '%s'", val);
                  return ERR_Failed;
            }
            break;
         }

         case SVF_VALUES: {
            for (LONG i=0; (*val) AND (i < CM_SIZE); i++) {
               DOUBLE dbl;
               val = read_numseq(val, &dbl, TAGEND);
               m[i] = dbl;
            }
            break;
         }

         default: fe_default(Self, effect, hash, val); break;
      }
   }

   // If a special colour mode was selected, convert the provided value(s) to the matrix format.

   ColourMatrix *matrix;

   switch (effect->Colour.Mode) {
      case CM_SATURATE:        matrix = new (std::nothrow) ColourMatrix(); if (matrix) matrix->adjustSaturation(m[0]); break;
      case CM_HUE_ROTATE:      matrix = new (std::nothrow) ColourMatrix(); if (matrix) matrix->rotateHue(m[0]); break;
      case CM_LUMINANCE_ALPHA: matrix = new (std::nothrow) ColourMatrix(); if (matrix) matrix->luminance2Alpha(); break;
      case CM_CONTRAST:        matrix = new (std::nothrow) ColourMatrix(); if (matrix) matrix->adjustContrast(m[0]); break;
      case CM_BRIGHTNESS:      matrix = new (std::nothrow) ColourMatrix(); if (matrix) matrix->adjustBrightness(m[0]); break;
      case CM_HUE:             matrix = new (std::nothrow) ColourMatrix(); if (matrix) matrix->adjustHue(m[0]); break;
      case CM_COLOURISE:       matrix = new (std::nothrow) ColourMatrix(); if (matrix) matrix->colourise(m[0], m[1], m[2], m[3] < 0.001 ? 1.0 : m[3]); break;
      case CM_DESATURATE:      matrix = new (std::nothrow) ColourMatrix(); if (matrix) matrix->adjustSaturation(0); break;
      default:                 matrix = new (std::nothrow) ColourMatrix(m); break;
   }

   if (!matrix) return ERR_AllocMemory;
   effect->Colour.Matrix = matrix; // Will be deleted in free_effect_resources()

   return ERR_Okay;
}
