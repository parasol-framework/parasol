/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
MorphologyFX: Applies the morphology filter effect.

The MorphologyFX class performs "fattening" or "thinning" of artwork.  It is particularly useful for fattening or
thinning an alpha channel.

The dilation (or erosion) kernel is a rectangle with a width of `2 * RadiusX` and a height of `2 * RadiusY`. In
dilation, the output pixel is the individual component-wise maximum of the corresponding R,G,B,A values in the input
image's kernel rectangle.  In erosion, the output pixel is the individual component-wise minimum of the
corresponding R,G,B,A values in the input image's kernel rectangle.

Frequently this operation will take place on alpha-only images, such as that produced by the built-in input,
SourceAlpha.  In that case, the implementation might want to optimize the single channel case.

Because the algorithm operates on premultipied color values, it will always result in color values less than or
equal to the alpha channel.

-END-

*********************************************************************************************************************/

class extMorphologyFX : public extFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = ID_MORPHOLOGYFX;
   static constexpr CSTRING CLASS_NAME = "MorphologyFX";
   using create = pf::Create<extMorphologyFX>;

   LONG RadiusX, RadiusY;
   MOP Operator;
};

/*********************************************************************************************************************
-ACTION-
Draw: Render the effect to the target bitmap.
-END-
*********************************************************************************************************************/

static ERROR MORPHOLOGYFX_Draw(extMorphologyFX *Self, struct acDraw *Args)
{
   const LONG canvasWidth = Self->Target->Clip.Right - Self->Target->Clip.Left;
   const LONG canvasHeight = Self->Target->Clip.Bottom - Self->Target->Clip.Top;

   if (canvasWidth * canvasHeight > 4096 * 4096) return ERR_Failed; // Bail on really large bitmaps.

   const UBYTE A = Self->Target->ColourFormat->AlphaPos>>3;
   const UBYTE R = Self->Target->ColourFormat->RedPos>>3;
   const UBYTE G = Self->Target->ColourFormat->GreenPos>>3;
   const UBYTE B = Self->Target->ColourFormat->BluePos>>3;

   UBYTE *out_line;
   UBYTE *buffer = NULL;
   LONG out_linewidth;
   bool buffer_as_input;

   // A temporary buffer is required if we are applying the effect on both axis.  Otherwise we can
   // directly write to the target bitmap.

   if ((Self->RadiusX > 0) and (Self->RadiusY > 0)) {
      buffer = new (std::nothrow) UBYTE[canvasWidth * canvasHeight * 4];
      if (!buffer) return ERR_Memory;
      out_line = buffer;
      out_linewidth = canvasWidth * 4;
      buffer_as_input = true;
   }
   else {
      out_line = (UBYTE *)(Self->Target->Data + (Self->Target->Clip.Left<<2) + (Self->Target->Clip.Top * Self->Target->LineWidth));
      out_linewidth = Self->Target->LineWidth;
      buffer_as_input = false;
   }

   objBitmap *inBmp;
   if (get_source_bitmap(Self->Filter, &inBmp, Self->SourceType, Self->Input, false)) return ERR_Failed;
   UBYTE *input = inBmp->Data + (inBmp->Clip.Top * inBmp->LineWidth) + (inBmp->Clip.Left * inBmp->BytesPerPixel);

   if (Self->RadiusX > 0) { // Top-to-bottom dilate
      auto radius = Self->RadiusX;
      if (canvasWidth - 1 < radius) radius = canvasWidth - 1;

      const UBYTE *endinput  = input + (radius * 4);
      const UBYTE *inputline = input;

      for (int x=0; x < canvasWidth; ++x) {
         const UBYTE *in  = inputline;
         const UBYTE *end = endinput;
         auto out         = out_line;

         if (Self->Operator IS MOP::DILATE) {
            for (int y = 0; y < canvasHeight; ++y) {
               UBYTE maxB = 0, maxG = 0, maxR = 0, maxA = 0;
               for (const UBYTE *pix=in; pix <= end; pix += 4) {
                  if (pix[B] > maxB) maxB = pix[B];
                  if (pix[G] > maxG) maxG = pix[G];
                  if (pix[R] > maxR) maxR = pix[R];
                  if (pix[A] > maxA) maxA = pix[A];
               }
               out[R] = maxR; out[G] = maxG; out[B] = maxB; out[A] = maxA;

               out += out_linewidth;
               in  += inBmp->LineWidth;
               end += inBmp->LineWidth;
            }
         }
         else { // ERODE
            for (int y = 0; y < canvasHeight; ++y) {
               UBYTE minB = 255, minG = 255, minR = 255, minA = 255;
               for (const UBYTE *p=in; p <= end; p += 4) {
                  if (p[B] < minB) minB = p[B];
                  if (p[G] < minG) minG = p[G];
                  if (p[R] < minR) minR = p[R];
                  if (p[A] < minA) minA = p[A];
               }
               out[R] = minR; out[G] = minG; out[B] = minB; out[A] = minA;

               out += out_linewidth;
               in  += inBmp->LineWidth;
               end += inBmp->LineWidth;
            }
         }

         if (x >= radius) inputline += 4;
         if (x + radius < canvasWidth - 1) endinput += 4;
         out_line += 4;
      }
   }

   if (Self->RadiusY > 0) { // Left-to-right dilate
      auto radius = Self->RadiusY;
      if (canvasHeight - 1 < radius) radius = canvasHeight - 1;

      const UBYTE *endinput;
      const UBYTE *inputline;
      LONG inwidth;

      if (buffer_as_input) {
         endinput  = buffer + (radius * (canvasWidth * 4));
         inputline = buffer;
         inwidth   = canvasWidth * 4;
      }
      else {
         endinput  = input + (radius * inBmp->LineWidth); // Inner-loop will stop when reaching endinput
         inputline = input;
         inwidth   = inBmp->LineWidth;
      }

      out_line = (UBYTE *)(Self->Target->Data + (Self->Target->Clip.Left<<2) + (Self->Target->Clip.Top * Self->Target->LineWidth));
      out_linewidth = Self->Target->LineWidth;

      for (int y=0; y < canvasHeight; y++) {
         const UBYTE *in = inputline;
         const UBYTE *end = endinput;
         auto out = out_line;

         if (Self->Operator IS MOP::DILATE) {
            for (int x=0; x < canvasWidth; x++) {
               UBYTE maxB = 0, maxG = 0, maxR = 0, maxA = 0;
               for (const UBYTE *pix=in; pix <= end; pix += inwidth) {
                  if (pix[B] > maxB) maxB = pix[B];
                  if (pix[G] > maxG) maxG = pix[G];
                  if (pix[R] > maxR) maxR = pix[R];
                  if (pix[A] > maxA) maxA = pix[A];
               }
               out[R] = maxR; out[G] = maxG; out[B] = maxB; out[A] = maxA;
               out += 4;
               in  += 4;
               end += 4;
            }
         }
         else { // ERODE
            for (int x=0; x < canvasWidth; x++) {
               UBYTE minB = 255, minG = 255, minR = 255, minA = 255;
               for (const UBYTE *pix=in; pix <= end; pix += inwidth) {
                  if (pix[B] < minB) minB = pix[B];
                  if (pix[G] < minG) minG = pix[G];
                  if (pix[R] < minR) minR = pix[R];
                  if (pix[A] < minA) minA = pix[A];
               }
               out[R] = minR; out[G] = minG; out[B] = minB; out[A] = minA;
               out += 4;
               in  += 4;
               end += 4;
            }
         }
         if (y >= radius) inputline += inwidth;
         if (y + radius < canvasHeight - 1) endinput += inwidth;
         out_line += Self->Target->LineWidth;
      }
   }

   if (buffer) delete [] buffer;

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR MORPHOLOGYFX_NewObject(extMorphologyFX *Self, APTR Void)
{
   Self->Operator = MOP::ERODE;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Operator: Set to either 'erode' or 'dilate'.
Lookup: MOP

*********************************************************************************************************************/

static ERROR MORPHOLOGYFX_GET_Operator(extMorphologyFX *Self, MOP *Value)
{
   *Value = Self->Operator;
   return ERR_Okay;
}

static ERROR MORPHOLOGYFX_SET_Operator(extMorphologyFX *Self, MOP Value)
{
   Self->Operator = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
RadiusX: X radius value.

*********************************************************************************************************************/

static ERROR MORPHOLOGYFX_GET_RadiusX(extMorphologyFX *Self, LONG *Value)
{
   *Value = Self->RadiusX;
   return ERR_Okay;
}

static ERROR MORPHOLOGYFX_SET_RadiusX(extMorphologyFX *Self, LONG Value)
{
   if (Value >= 0) {
      Self->RadiusX = Value;
      return ERR_Okay;
   }
   else return ERR_OutOfRange;
}

/*********************************************************************************************************************

-FIELD-
RadiusY: Y radius value.

*********************************************************************************************************************/

static ERROR MORPHOLOGYFX_GET_RadiusY(extMorphologyFX *Self, LONG *Value)
{
   *Value = Self->RadiusY;
   return ERR_Okay;
}

static ERROR MORPHOLOGYFX_SET_RadiusY(extMorphologyFX *Self, LONG Value)
{
   if (Value >= 0) {
      Self->RadiusY = Value;
      return ERR_Okay;
   }
   else return ERR_OutOfRange;
}

/*********************************************************************************************************************

-FIELD-
XMLDef: Returns an SVG compliant XML string that describes the effect.
-END-

*********************************************************************************************************************/

static ERROR MORPHOLOGYFX_GET_XMLDef(extMorphologyFX *Self, STRING *Value)
{
   std::stringstream stream;

   stream << "feMorphology operator=\"";

   if (Self->Operator IS MOP::ERODE) stream << "erode\"";
   else stream << "dilate\"";

   stream << "radius=\"" << Self->RadiusX << " " << Self->RadiusY << "\"";

   *Value = StrClone(stream.str().c_str());
   return ERR_Okay;
}

//********************************************************************************************************************

#include "filter_morphology_def.c"

static const FieldDef clMorphologyFXOperator[] = {
   { "Erode",  MOP::ERODE },
   { "Dilate", MOP::DILATE },
   { NULL, 0 }
};

static const FieldArray clMorphologyFXFields[] = {
   { "Operator", FDF_VIRTUAL|FDF_LONG|FDF_LOOKUP|FDF_RW, MORPHOLOGYFX_GET_Operator, MORPHOLOGYFX_SET_Operator, &clMorphologyFXOperator },
   { "RadiusX",  FDF_VIRTUAL|FDF_LONG|FDF_RW, MORPHOLOGYFX_GET_RadiusX, MORPHOLOGYFX_SET_RadiusX },
   { "RadiusY",  FDF_VIRTUAL|FDF_LONG|FDF_RW, MORPHOLOGYFX_GET_RadiusY, MORPHOLOGYFX_SET_RadiusY },
   { "XMLDef",   FDF_VIRTUAL|FDF_STRING|FDF_ALLOC|FDF_R, MORPHOLOGYFX_GET_XMLDef },
   END_FIELD
};

//********************************************************************************************************************

ERROR init_morphfx(void)
{
   clMorphologyFX = objMetaClass::create::global(
      fl::BaseClassID(ID_FILTEREFFECT),
      fl::ClassID(ID_MORPHOLOGYFX),
      fl::Name("MorphologyFX"),
      fl::Category(CCF::GRAPHICS),
      fl::Actions(clMorphologyFXActions),
      fl::Fields(clMorphologyFXFields),
      fl::Size(sizeof(extMorphologyFX)),
      fl::Path(MOD_PATH));

   return clMorphologyFX ? ERR_Okay : ERR_AddClass;
}
