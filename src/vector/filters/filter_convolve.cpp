/*********************************************************************************************************************

-CLASS-
ConvolveFX: Applies a matrix convolution filter effect.

Convolve applies a matrix convolution filter effect to an input source.  A convolution combines pixels in the input
image with neighbouring pixels to produce a resulting image.  A wide variety of imaging operations can be achieved
through convolutions, including blurring, edge detection, sharpening, embossing and beveling.

A matrix convolution is based on an `n-by-m` matrix (the convolution kernel) which describes how a given pixel value in
the input image is combined with its neighbouring pixel values to produce a resulting pixel value. Each result pixel
is determined by applying the kernel matrix to the corresponding source pixel and its neighbouring pixels.  The basic
convolution formula which is applied to each colour value for a given pixel is:

<pre>
COLOURX,Y = (
     SUM I=0 to [MatrixRows-1] {
       SUM J=0 to [MatrixColumns-1] {
         SOURCE X - TargetX + J, Y - TargetY + I * Matrix * MatrixColumns - J - 1,  MatrixRows - I - 1
       }
     }
   ) / Divisor + Bias * ALPHAX,Y
</pre>

Note in the above formula that the values in the kernel matrix are applied such that the kernel matrix is rotated
180 degrees relative to the source and destination images in order to match convolution theory as described in many
computer graphics textbooks.

Because they operate on pixels, matrix convolutions are inherently resolution-dependent.  To make
resolution-independent results, an explicit value should be provided for either the `ResX` and `ResY` attributes
on the parent @VectorFilter and/or #UnitX and #UnitY.

-END-

TODO: As per the SVG spec...

Because they operate on pixels, matrix convolutions are inherently resolution-dependent.  To make convolve produce
resolution-independent results, an explicit value should be provided for either the ResX/Y on the filter element
and/or UnitX/Y.

UnitX/Y, in combination with the other attributes, defines an implicit pixel grid in the filter
effects coordinate system (i.e., the coordinate system established by the primitiveUnits attribute).
If the pixel grid established by UnitX/Y is not scaled to match the pixel grid established by
attribute filterRes (implicitly or explicitly), then the input image will be temporarily rescaled to
match its pixels with UnitX/Y. The convolution happens on the resampled image. After applying
the convolution, the image is resampled back to the original resolution.

When the image must be resampled to match the coordinate system defined by UnitX/Y prior to
convolution, or resampled to match the device coordinate system after convolution, it is recommended that
high quality viewers make use of appropriate interpolation techniques, for example bilinear or bicubic.
Depending on the speed of the available interpolents, this choice may be affected by the image-rendering
property setting. Note that implementations might choose approaches that minimize or eliminate resampling
when not necessary to produce proper results, such as when the document is zoomed out such that
UnitX/Y is considerably smaller than a device pixel.

*********************************************************************************************************************/

#include <array>

#define MAX_DIM 9

//********************************************************************************************************************

class extConvolveFX : public extFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::CONVOLVEFX;
   static constexpr CSTRING CLASS_NAME = "ConvolveFX";
   using create = pf::Create<extConvolveFX>;

   DOUBLE UnitX, UnitY;
   DOUBLE Divisor;
   DOUBLE Bias;
   LONG TargetX, TargetY;
   LONG MatrixColumns, MatrixRows;
   EM   EdgeMode;
   LONG MatrixSize;
   bool PreserveAlpha;
   DOUBLE Matrix[MAX_DIM * MAX_DIM];

   extConvolveFX() : UnitX(1), UnitY(1), Divisor(0), Bias(0), TargetX(-1), TargetY(-1),
      MatrixColumns(3), MatrixRows(3), EdgeMode(EM::DUPLICATE), MatrixSize(9), PreserveAlpha(false) { }

   inline UBYTE * getPixel(objBitmap *Bitmap, LONG X, LONG Y) const {
      if ((X >= Bitmap->Clip.Left) and (X < Bitmap->Clip.Right) and
          (Y >= Bitmap->Clip.Top) and (Y < Bitmap->Clip.Bottom)) {
         return Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<2);
      }

      switch (EdgeMode) {
         default: return NULL;

         case EM::DUPLICATE:
            if (X < Bitmap->Clip.Left) X = Bitmap->Clip.Left;
            else if (X >= Bitmap->Clip.Right) X = Bitmap->Clip.Right - 1;
            if (Y < Bitmap->Clip.Top) Y = Bitmap->Clip.Top;
            else if (Y >= Bitmap->Clip.Bottom) Y = Bitmap->Clip.Bottom - 1;
            return Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<2);

         case EM::WRAP:
            while (X < Bitmap->Clip.Left) X += Bitmap->Clip.Right - Bitmap->Clip.Left;
            X %= Bitmap->Clip.Right - Bitmap->Clip.Left;
            while (Y < Bitmap->Clip.Top) Y += Bitmap->Clip.Bottom - Bitmap->Clip.Top;
            Y %= Bitmap->Clip.Bottom - Bitmap->Clip.Top;
            return Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<2);
      }
   }

   void processClipped(objBitmap *InputBitmap, UBYTE *output, LONG pLeft, LONG pTop, LONG pRight, LONG pBottom) {
      const UBYTE A = InputBitmap->ColourFormat->AlphaPos>>3;
      const UBYTE R = InputBitmap->ColourFormat->RedPos>>3;
      const UBYTE G = InputBitmap->ColourFormat->GreenPos>>3;
      const UBYTE B = InputBitmap->ColourFormat->BluePos>>3;

      const DOUBLE factor = 1.0 / Divisor;

      UBYTE *input = InputBitmap->Data + (pTop * InputBitmap->LineWidth);
      UBYTE *outline = output;
      for (LONG y=pTop; y < pBottom; y++) {
         UBYTE *out = outline;
         for (LONG x=pLeft; x < pRight; x++) {
            DOUBLE r = 0.0, g = 0.0, b = 0.0, a = 0.0;

            // Multiply every value of the filter with corresponding image pixel

            UBYTE kv = 0;
            for (int fy=y-TargetY; fy < y+MatrixRows-TargetY; fy++) {
               for (int fx=x-TargetX; fx < x+MatrixColumns-TargetX; fx++) {
                  UBYTE *pixel = getPixel(InputBitmap, fx, fy);
                  if (pixel) {
                     r += pixel[R] * Matrix[kv];
                     g += pixel[G] * Matrix[kv];
                     b += pixel[B] * Matrix[kv];
                     a += pixel[A] * Matrix[kv];
                  }
                  kv++;
               }
            }

            LONG lr = F2I((factor * r) + Bias);
            LONG lg = F2I((factor * g) + Bias);
            LONG lb = F2I((factor * b) + Bias);
            out[R] = glLinearRGB.invert(std::min(std::max(lr, 0), 255));
            out[G] = glLinearRGB.invert(std::min(std::max(lg, 0), 255));
            out[B] = glLinearRGB.invert(std::min(std::max(lb, 0), 255));
            if (!PreserveAlpha) out[A] = std::min(std::max(F2I(factor * a + Bias), 0), 255);
            else out[A] = (input + (x<<2))[A];
            out += 4;
         }
         input   += InputBitmap->LineWidth;
         outline += (Target->Clip.Right - Target->Clip.Left)<<2;
      }
   }

   // This algorithm is unclipped and performs no edge detection, so is unsafe to use near the edge of the bitmap.

   void processFast(objBitmap *InputBitmap, UBYTE *output, LONG Left, LONG Top, LONG Right, LONG Bottom) {
      const UBYTE A = InputBitmap->ColourFormat->AlphaPos>>3;
      const UBYTE R = InputBitmap->ColourFormat->RedPos>>3;
      const UBYTE G = InputBitmap->ColourFormat->GreenPos>>3;
      const UBYTE B = InputBitmap->ColourFormat->BluePos>>3;

      const DOUBLE factor = 1.0 / Divisor;

      UBYTE *input = InputBitmap->Data + (Top * InputBitmap->LineWidth);
      for (LONG y=Top; y < Bottom; y++) {
         UBYTE *out = output;
         UBYTE *filterEdge = InputBitmap->Data + (y-TargetY) * InputBitmap->LineWidth;
         for (LONG x=Left; x < Right; x++) {
            DOUBLE r = 0.0, g = 0.0, b = 0.0, a = 0.0;
            UBYTE kv = 0;
            const LONG fxs = x - TargetX;
            const LONG fxe = x + MatrixColumns - TargetX;
            UBYTE *currentLine = filterEdge;
            for (int fy=y-TargetY; fy < y+MatrixRows-TargetY; fy++) {
               UBYTE *pixel = currentLine + (fxs<<2);
               for (int fx=fxs; fx < fxe; fx++) {
                  r += pixel[R] * Matrix[kv];
                  g += pixel[G] * Matrix[kv];
                  b += pixel[B] * Matrix[kv];
                  a += pixel[A] * Matrix[kv];
                  pixel += 4;
                  kv++;
               }
               currentLine += InputBitmap->LineWidth;
            }

            LONG lr = F2I((factor * r) + Bias);
            LONG lg = F2I((factor * g) + Bias);
            LONG lb = F2I((factor * b) + Bias);
            out[R] = glLinearRGB.invert(std::min(std::max(lr, 0), 255));
            out[G] = glLinearRGB.invert(std::min(std::max(lg, 0), 255));
            out[B] = glLinearRGB.invert(std::min(std::max(lb, 0), 255));
            if (!PreserveAlpha) out[A] = std::min(std::max(F2I(factor * a + Bias), 0), 255);
            else out[A] = (input + (x<<2))[A];
            out += 4;
         }
         input  += InputBitmap->LineWidth;
         output += (InputBitmap->Clip.Right - InputBitmap->Clip.Left)<<2;
      }
   }
};

//********************************************************************************************************************

static ERR CONVOLVEFX_Draw(extConvolveFX *Self, struct acDraw *Args)
{
   if (Self->Target->BytesPerPixel != 4) return ERR::Failed;

   const LONG canvas_width  = Self->Target->Clip.Right - Self->Target->Clip.Left;
   const LONG canvas_height = Self->Target->Clip.Bottom - Self->Target->Clip.Top;

   if (canvas_width * canvas_height > 4096 * 4096) return ERR::Failed; // Bail on really large bitmaps.

   UBYTE *output = new (std::nothrow) UBYTE[canvas_width * canvas_height * Self->Target->BytesPerPixel];
   if (!output) return ERR::Memory;

   objBitmap *inBmp;
   if (get_source_bitmap(Self->Filter, &inBmp, Self->SourceType, Self->Input, false) != ERR::Okay) return ERR::Failed;

   if (Self->Filter->ColourSpace IS VCS::LINEAR_RGB) inBmp->convertToLinear();
   inBmp->premultiply();

   if ((canvas_width > Self->MatrixColumns*3) and (canvas_height > Self->MatrixRows*3)) {
      const LONG ew = Self->MatrixColumns>>1;
      const LONG eh = Self->MatrixRows>>1;
      Self->processClipped(inBmp, output,                       Self->Target->Clip.Left,     Self->Target->Clip.Top, Self->Target->Clip.Left+ew,  Self->Target->Clip.Bottom); // Left
      Self->processClipped(inBmp, output+((canvas_width-ew)*4), Self->Target->Clip.Right-ew, Self->Target->Clip.Top, Self->Target->Clip.Right,    Self->Target->Clip.Bottom); // Right
      Self->processClipped(inBmp, output+(ew*4),                Self->Target->Clip.Left+ew,  Self->Target->Clip.Top, Self->Target->Clip.Right-ew, Self->Target->Clip.Top+eh); // Top
      Self->processClipped(inBmp, output+((canvas_height-eh)*4*canvas_width), Self->Target->Clip.Left+ew,  Self->Target->Clip.Bottom-eh, Self->Target->Clip.Right-ew, Self->Target->Clip.Bottom); // Bottom
      // Center
      Self->processFast(inBmp, output+(ew*4)+(canvas_width*eh*4), Self->Target->Clip.Left+ew, Self->Target->Clip.Top+eh, Self->Target->Clip.Right-ew, Self->Target->Clip.Bottom-eh);
   }
   else Self->processClipped(inBmp, output, Self->Target->Clip.Left, Self->Target->Clip.Top, Self->Target->Clip.Right, Self->Target->Clip.Bottom);

   // Copy the resulting output back to the bitmap.

   ULONG *pixel = (ULONG *)(Self->Target->Data + (Self->Target->Clip.Left<<2) + (Self->Target->Clip.Top * Self->Target->LineWidth));
   ULONG *src   = (ULONG *)output;
   for (LONG y=0; y < canvas_height; y++) {
      CopyMemory(src, pixel, 4 * canvas_width);
      pixel += Self->Target->LineWidth>>2;
      src += canvas_width;
   }

   delete [] output;

   if (Self->Filter->ColourSpace IS VCS::LINEAR_RGB) inBmp->convertToRGB();

   inBmp->demultiply();
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR CONVOLVEFX_Init(extConvolveFX *Self)
{
   pf::Log log;

   if (!Self->UnitY) Self->UnitY = Self->UnitX;

   if (Self->MatrixColumns * Self->MatrixRows > MAX_DIM * MAX_DIM) {
      log.warning("Size of matrix exceeds internally imposed limits.");
      return ERR::BufferOverflow;
   }

   const LONG filter_size = Self->MatrixColumns * Self->MatrixRows;

   if (Self->MatrixSize != filter_size) {
      log.warning("Matrix size of %d does not match the filter size of %dx%d", Self->MatrixSize, Self->MatrixColumns, Self->MatrixRows);
      return ERR::Failed;
   }

   // Use client-provided tx/ty values, otherwise default according to the SVG standard.

   if ((Self->TargetX < 0) or (Self->TargetX >= Self->MatrixColumns)) {
      Self->TargetX = floor(Self->MatrixColumns / 2);
   }

   if ((Self->TargetY < 0) or (Self->TargetY >= Self->MatrixRows)) {
      Self->TargetY = floor(Self->MatrixRows / 2);
   }

   if (!Self->Divisor) {
      DOUBLE divisor = 0;
      for (LONG i=0; i < filter_size; i++) divisor += Self->Matrix[i];
      if (!divisor) divisor = 1;
      Self->Divisor = divisor;
   }

   log.trace("Convolve Size: (%d,%d), Divisor: %.2f, Bias: %.2f", Self->MatrixColumns, Self->MatrixRows, Self->Divisor, Self->Bias);

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR CONVOLVEFX_Free(extConvolveFX *Self)
{
   Self->~extConvolveFX();
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR CONVOLVEFX_NewObject(extConvolveFX *Self)
{
   new (Self) extConvolveFX;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Bias: Used to adjust the final result of each computed RGB value.

After applying the #Matrix to the input image to yield a number and applying the #Divisor, the Bias value is added to
each component.  One application of Bias is when it is desirable to have .5 gray value be the zero response of the
filter.  The Bias value shifts the range of the filter.  This allows representation of values that would otherwise be
clamped to 0 or 1.  The default is 0.

*********************************************************************************************************************/

static ERR CONVOLVEFX_GET_Bias(extConvolveFX *Self, DOUBLE *Value)
{
   *Value = Self->Bias;
   return ERR::Okay;
}

static ERR CONVOLVEFX_SET_Bias(extConvolveFX *Self, DOUBLE Value)
{
   Self->Bias = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Divisor: Defines the divisor value in the convolution algorithm.

After applying the #Matrix to the input image to yield a number, that number is divided by #Divisor to yield the
final destination color value.  A divisor that is the sum of all the matrix values tends to have an evening effect
on the overall color intensity of the result.  The default value is the sum of all values in #Matrix, with the
exception that if the sum is zero, then the divisor is set to `1`.

*********************************************************************************************************************/

static ERR CONVOLVEFX_GET_Divisor(extConvolveFX *Self, DOUBLE *Value)
{
   *Value = Self->Divisor;
   return ERR::Okay;
}

static ERR CONVOLVEFX_SET_Divisor(extConvolveFX *Self, DOUBLE Value)
{
   pf::Log log;
   if (Value <= 0) return log.warning(ERR::InvalidValue);
   Self->Divisor = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
EdgeMode: Defines the behaviour of the convolve algorithm around the edges of the input image.

The EdgeMode determines how to extend the input image with color values so that the matrix operations can be applied
when the #Matrix is positioned at or near the edge of the input image.

*********************************************************************************************************************/

static ERR CONVOLVEFX_GET_EdgeMode(extConvolveFX *Self, EM *Value)
{
   *Value = Self->EdgeMode;
   return ERR::Okay;
}

static ERR CONVOLVEFX_SET_EdgeMode(extConvolveFX *Self, EM Value)
{
   Self->EdgeMode = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Matrix: A list of numbers that make up the kernel matrix for the convolution.

A list of numbers that make up the kernel matrix for the convolution.  The number of entries in the list must equal
`MatrixColumns * MatrixRows`.

*********************************************************************************************************************/

static ERR CONVOLVEFX_GET_Matrix(extConvolveFX *Self, DOUBLE **Value, LONG *Elements)
{
   *Elements = Self->MatrixSize;
   *Value    = Self->Matrix;
   return ERR::Okay;
}

static ERR CONVOLVEFX_SET_Matrix(extConvolveFX *Self, DOUBLE *Value, LONG Elements)
{
   pf::Log log;

   if ((Elements > 0) and (Elements <= ARRAYSIZE(Self->Matrix))) {
      Self->MatrixSize = Elements;
      CopyMemory(Value, Self->Matrix, sizeof(DOUBLE) * Elements);
      return ERR::Okay;
   }
   else return log.warning(ERR::InvalidValue);
}

/*********************************************************************************************************************

-FIELD-
MatrixRows: The number of rows in the Matrix.

Indicates the number of rows represented in #Matrix.  A typical value is `3`.  It is recommended that only small
values are used; higher values may result in very high CPU overhead and usually do not produce results that justify
the impact on performance.  The default value is 3.

*********************************************************************************************************************/

static ERR CONVOLVEFX_GET_MatrixRows(extConvolveFX *Self, LONG *Value)
{
   *Value = Self->MatrixRows;
   return ERR::Okay;
}

static ERR CONVOLVEFX_SET_MatrixRows(extConvolveFX *Self, LONG Value)
{
   pf::Log log;
   if (Value <= 0) return log.warning(ERR::InvalidValue);

   Self->MatrixRows = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
MatrixColumns: The number of columns in the Matrix.

Indicates the number of columns represented in #Matrix.  A typical value is `3`.  It is recommended that only small
values are used; higher values may result in very high CPU overhead and usually do not produce results that justify
the impact on performance.  The default value is `3`.

*********************************************************************************************************************/

static ERR CONVOLVEFX_GET_MatrixColumns(extConvolveFX *Self, LONG *Value)
{
   *Value = Self->MatrixColumns;
   return ERR::Okay;
}

static ERR CONVOLVEFX_SET_MatrixColumns(extConvolveFX *Self, LONG Value)
{
   pf::Log log;
   if (Value <= 0) return log.warning(ERR::InvalidValue);

   Self->MatrixColumns = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
PreserveAlpha: If TRUE, the alpha channel is protected from the effects of the convolve algorithm.

*********************************************************************************************************************/

static ERR CONVOLVEFX_GET_PreserveAlpha(extConvolveFX *Self, LONG *Value)
{
   *Value = Self->PreserveAlpha;
   return ERR::Okay;
}

static ERR CONVOLVEFX_SET_PreserveAlpha(extConvolveFX *Self, LONG Value)
{
   Self->PreserveAlpha = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
TargetX: The X position of the matrix in relation to the input image.

Determines the positioning in X of the convolution matrix relative to a given target pixel in the input image.  The
left-most column of the matrix is column number zero.  The value must be such that `0 &lt;= TargetX &lt; MatrixColumns`.  By
default, the convolution matrix is centered in X over each pixel of the input image, i.e.
`TargetX = floor(MatrixColumns / 2)`.

*********************************************************************************************************************/

static ERR CONVOLVEFX_GET_TargetX(extConvolveFX *Self, LONG *Value)
{
   *Value = Self->TargetX;
   return ERR::Okay;
}

static ERR CONVOLVEFX_SET_TargetX(extConvolveFX *Self, LONG Value)
{
   if (Self->initialised()) {
      pf::Log log;
      if ((Value < 0) or (Value >= Self->MatrixColumns)) return log.warning(ERR::OutOfRange);
   }

   Self->TargetX = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
TargetY: The Y position of the matrix in relation to the input image.

Determines the positioning in Y of the convolution matrix relative to a given target pixel in the input image.  The
left-most column of the matrix is column number zero.  The value must be such that `0 &lt;= TargetY &lt; MatrixRows`.  By
default, the convolution matrix is centered in Y over each pixel of the input image, i.e.
`TargetY = floor(MatrixRows / 2)`.

*********************************************************************************************************************/

static ERR CONVOLVEFX_GET_TargetY(extConvolveFX *Self, LONG *Value)
{
   *Value = Self->TargetY;
   return ERR::Okay;
}

static ERR CONVOLVEFX_SET_TargetY(extConvolveFX *Self, LONG Value)
{
   if (Self->initialised()) {
      pf::Log log;
      if ((Value < 0) or (Value >= Self->MatrixRows)) return log.warning(ERR::OutOfRange);
   }

   Self->TargetY = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
UnitX: The distance in filter units between rows in the Matrix.

Indicates the intended distance in current filter units (i.e. as determined by the value of PrimitiveUnits)
between successive columns and rows, respectively, in the #Matrix.  By specifying value(s) for #UnitX, the kernel
becomes defined in a scalable, abstract coordinate system.  If #UnitX is not specified, the default value is one pixel
in the offscreen bitmap, which is a pixel-based coordinate system, and thus potentially not scalable.  For some level
of consistency across display media and user agents, it is necessary that a value be provided for at least one of
ResX and #UnitX.

The most consistent results and the fastest performance will be achieved if the pixel grid of the offscreen bitmap
aligns with the pixel grid of the kernel.

*********************************************************************************************************************/

static ERR CONVOLVEFX_GET_UnitX(extConvolveFX *Self, DOUBLE *Value)
{
   *Value = Self->UnitX;
   return ERR::Okay;
}

static ERR CONVOLVEFX_SET_UnitX(extConvolveFX *Self, DOUBLE Value)
{
   if (Value < 0) return ERR::InvalidValue;

   Self->UnitX = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
UnitY: The distance in filter units between columns in the Matrix.

Indicates the intended distance in current filter units (i.e. as determined by the value of PrimitiveUnits)
between successive columns and rows, respectively, in the #Matrix.  By specifying value(s) for #UnitY, the kernel
becomes defined in a scalable, abstract coordinate system.  If #UnitY is not specified, the default value is one pixel
in the offscreen bitmap, which is a pixel-based coordinate system, and thus potentially not scalable.  For some level
of consistency across display media and user agents, it is necessary that a value be provided for at least one of
ResY and #UnitY.

The most consistent results and the fastest performance will be achieved if the pixel grid of the offscreen bitmap
aligns with the pixel grid of the kernel.

*********************************************************************************************************************/

static ERR CONVOLVEFX_GET_UnitY(extConvolveFX *Self, DOUBLE *Value)
{
   *Value = Self->UnitY;
   return ERR::Okay;
}

static ERR CONVOLVEFX_SET_UnitY(extConvolveFX *Self, DOUBLE Value)
{
   if (Value < 0) return ERR::InvalidValue;

   Self->UnitY = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
XMLDef: Returns an SVG compliant XML string that describes the effect.
-END-

*********************************************************************************************************************/

static ERR CONVOLVEFX_GET_XMLDef(extConvolveFX *Self, STRING *Value)
{
   std::stringstream stream;

   stream << "feConvolveMatrix";

   *Value = strclone(stream.str());
   return ERR::Okay;
}

//********************************************************************************************************************

#include "filter_convolve_def.c"

static const FieldDef clEdgeMode[] = {
   { "Duplicate", EM::DUPLICATE },
   { "Wrap",      EM::WRAP },
   { "None",      EM::NONE },
   { NULL, 0 }
};

static const FieldArray clConvolveFXFields[] = {
   { "Bias",          FDF_VIRTUAL|FDF_DOUBLE|FDF_RI,           CONVOLVEFX_GET_Bias, CONVOLVEFX_SET_Bias },
   { "Divisor",       FDF_VIRTUAL|FDF_DOUBLE|FDF_RI,           CONVOLVEFX_GET_Divisor, CONVOLVEFX_SET_Divisor },
   { "EdgeMode",      FDF_VIRTUAL|FDF_LONG|FDF_LOOKUP|FDF_RI,  CONVOLVEFX_GET_EdgeMode, CONVOLVEFX_SET_EdgeMode, &clEdgeMode },
   { "MatrixRows",    FDF_VIRTUAL|FDF_LONG|FDF_RI,             CONVOLVEFX_GET_MatrixRows, CONVOLVEFX_SET_MatrixRows },
   { "MatrixColumns", FDF_VIRTUAL|FDF_LONG|FDF_RI,             CONVOLVEFX_GET_MatrixColumns, CONVOLVEFX_SET_MatrixColumns },
   { "Matrix",        FDF_VIRTUAL|FDF_DOUBLE|FDF_ARRAY|FDF_RI, CONVOLVEFX_GET_Matrix, CONVOLVEFX_SET_Matrix },
   { "PreserveAlpha", FDF_VIRTUAL|FDF_LONG|FDF_RW,             CONVOLVEFX_GET_PreserveAlpha, CONVOLVEFX_SET_PreserveAlpha },
   { "TargetX",       FDF_VIRTUAL|FDF_LONG|FDF_RI,             CONVOLVEFX_GET_TargetX, CONVOLVEFX_SET_TargetX },
   { "TargetY",       FDF_VIRTUAL|FDF_LONG|FDF_RI,             CONVOLVEFX_GET_TargetY, CONVOLVEFX_SET_TargetY },
   { "UnitX",         FDF_VIRTUAL|FDF_DOUBLE|FDF_RI,           CONVOLVEFX_GET_UnitX, CONVOLVEFX_SET_UnitX },
   { "UnitY",         FDF_VIRTUAL|FDF_DOUBLE|FDF_RI,           CONVOLVEFX_GET_UnitY, CONVOLVEFX_SET_UnitY },
   { "XMLDef",        FDF_VIRTUAL|FDF_STRING|FDF_ALLOC|FDF_R,  CONVOLVEFX_GET_XMLDef },
   END_FIELD
};

//********************************************************************************************************************

ERR init_convolvefx(void)
{
   clConvolveFX = objMetaClass::create::global(
      fl::BaseClassID(CLASSID::FILTEREFFECT),
      fl::ClassID(CLASSID::CONVOLVEFX),
      fl::Name("ConvolveFX"),
      fl::Category(CCF::GRAPHICS),
      fl::Actions(clConvolveFXActions),
      fl::Fields(clConvolveFXFields),
      fl::Size(sizeof(extConvolveFX)),
      fl::Path(MOD_PATH));

   return clConvolveFX ? ERR::Okay : ERR::AddClass;
}
