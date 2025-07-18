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
#include <thread_pool/thread_pool.h>

constexpr int MAX_DIM = 9; // Maximum matrix dimension for convolution filter effects.

//********************************************************************************************************************

class extConvolveFX : public extFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::CONVOLVEFX;
   static constexpr CSTRING CLASS_NAME = "ConvolveFX";
   using create = pf::Create<extConvolveFX>;

   double UnitX, UnitY;
   double Divisor;
   double Bias;
   int TargetX, TargetY;
   int MatrixColumns, MatrixRows;
   EM   EdgeMode;
   int MatrixSize;
   bool PreserveAlpha;
   double Matrix[MAX_DIM * MAX_DIM];

   extConvolveFX() : UnitX(1), UnitY(1), Divisor(0), Bias(0), TargetX(-1), TargetY(-1),
      MatrixColumns(3), MatrixRows(3), EdgeMode(EM::DUPLICATE), MatrixSize(9), PreserveAlpha(false) { }

   inline uint8_t * getPixel(objBitmap *Bitmap, int X, int Y) const {
      if ((X >= Bitmap->Clip.Left) and (X < Bitmap->Clip.Right) and
          (Y >= Bitmap->Clip.Top) and (Y < Bitmap->Clip.Bottom)) {
         return Bitmap->Data + (Y * Bitmap->LineWidth) + (X * Bitmap->BytesPerPixel);
      }

      switch (EdgeMode) {
         case EM::DUPLICATE:
            if (X < Bitmap->Clip.Left) X = Bitmap->Clip.Left;
            else if (X >= Bitmap->Clip.Right) X = Bitmap->Clip.Right - 1;
            if (Y < Bitmap->Clip.Top) Y = Bitmap->Clip.Top;
            else if (Y >= Bitmap->Clip.Bottom) Y = Bitmap->Clip.Bottom - 1;
            return Bitmap->Data + (Y * Bitmap->LineWidth) + (X * Bitmap->BytesPerPixel);

         case EM::WRAP: {
            auto width = Bitmap->Clip.Right - Bitmap->Clip.Left;
            auto height = Bitmap->Clip.Bottom - Bitmap->Clip.Top;

            X -= Bitmap->Clip.Left;
            Y -= Bitmap->Clip.Top;

            if (X < 0) X = std::abs(width + X) % width;
            else if (X >= width) X %= width;

            if (Y < 0) Y = std::abs(height + Y) % height;
            else if (Y >= height) Y %= height;

            return Bitmap->Data + ((Y + Bitmap->Clip.Top)  * Bitmap->LineWidth) + ((X + Bitmap->Clip.Left) * Bitmap->BytesPerPixel);
         }

         default: return nullptr;
      }
   }

   // Standard algorithm that uses edge detection at the borders (see getPixel()).

   void processClipped(objBitmap *InputBitmap, uint8_t *output, int pLeft, int pTop, int pRight, int pBottom) {
      if ((pRight <= pLeft) or (pBottom <= pTop)) return;

      const uint8_t A = InputBitmap->ColourFormat->AlphaPos>>3;
      const uint8_t R = InputBitmap->ColourFormat->RedPos>>3;
      const uint8_t G = InputBitmap->ColourFormat->GreenPos>>3;
      const uint8_t B = InputBitmap->ColourFormat->BluePos>>3;

      const double factor = 1.0 / Divisor;
      const double bias = Bias * 255.0;

      uint8_t *alpha_input = InputBitmap->Data + (pTop * InputBitmap->LineWidth);
      uint8_t *outline = output;
      for (int y=pTop; y < pBottom; y++) {
         uint8_t *out = outline;
         for (int x=pLeft; x < pRight; x++) {
            double r = 0.0, g = 0.0, b = 0.0, a = 0.0;

            // Multiply every value of the filter with corresponding image pixel

            uint8_t kv = 0;
            for (int fy=y-TargetY; fy < y+MatrixRows-TargetY; fy++) {
               for (int fx=x-TargetX; fx < x+MatrixColumns-TargetX; fx++) {
                  if (auto pixel = getPixel(InputBitmap, fx, fy)) {
                     r += pixel[R] * Matrix[kv];
                     g += pixel[G] * Matrix[kv];
                     b += pixel[B] * Matrix[kv];
                     a += pixel[A] * Matrix[kv];
                  }
                  kv++;
               }
            }

            int lr = F2I((factor * r) + bias);
            int lg = F2I((factor * g) + bias);
            int lb = F2I((factor * b) + bias);
            out[R] = glLinearRGB.invert(std::clamp(lr, 0, 255));
            out[G] = glLinearRGB.invert(std::clamp(lg, 0, 255));
            out[B] = glLinearRGB.invert(std::clamp(lb, 0, 255));
            if (!PreserveAlpha) out[A] = std::clamp(F2I(factor * a + bias), 0, 255);
            else out[A] = (alpha_input + (x * InputBitmap->BytesPerPixel))[A];
            out += 4;
         }
         alpha_input += InputBitmap->LineWidth;
         outline += (Target->Clip.Right - Target->Clip.Left)<<2;
      }
   }

   // This algorithm is unclipped and performs no edge detection, so is unsafe to use near the edge of the bitmap.

   void processFast(objBitmap *InputBitmap, uint8_t *output, int Left, int Top, int Right, int Bottom) {
      if ((Right <= Left) or (Bottom <= Top)) return;

      const uint8_t A = InputBitmap->ColourFormat->AlphaPos>>3;
      const uint8_t R = InputBitmap->ColourFormat->RedPos>>3;
      const uint8_t G = InputBitmap->ColourFormat->GreenPos>>3;
      const uint8_t B = InputBitmap->ColourFormat->BluePos>>3;

      const double factor = 1.0 / Divisor;
      const double bias = Bias * 255.0;

      uint8_t *input = InputBitmap->Data + (Top * InputBitmap->LineWidth);
      for (int y=Top; y < Bottom; y++) {
         uint8_t *out = output;
         uint8_t *filterEdge = InputBitmap->Data + (y-TargetY) * InputBitmap->LineWidth;
         for (int x=Left; x < Right; x++) {
            double r = 0.0, g = 0.0, b = 0.0, a = 0.0;
            uint8_t kv = 0;
            const int fxs = x - TargetX;
            const int fxe = x + MatrixColumns - TargetX;
            uint8_t *currentLine = filterEdge;
            for (int fy=y-TargetY; fy < y+MatrixRows-TargetY; fy++) {
               uint8_t *pixel = currentLine + (fxs<<2);
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

            int lr = F2I((factor * r) + bias);
            int lg = F2I((factor * g) + bias);
            int lb = F2I((factor * b) + bias);
            out[R] = glLinearRGB.invert(std::clamp(lr, 0, 255));
            out[G] = glLinearRGB.invert(std::clamp(lg, 0, 255));
            out[B] = glLinearRGB.invert(std::clamp(lb, 0, 255));
            if (!PreserveAlpha) out[A] = std::clamp(F2I(factor * a + bias), 0, 255);
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
   if (Self->Target->BytesPerPixel != 4) return ERR::InvalidValue;

   const int canvas_width  = Self->Target->Clip.Right - Self->Target->Clip.Left;
   const int canvas_height = Self->Target->Clip.Bottom - Self->Target->Clip.Top;

   if (canvas_width * canvas_height > 4096 * 4096) return ERR::Failed; // Bail on really large bitmaps.

   int bpp = Self->Target->BytesPerPixel;
   uint8_t *output = new (std::nothrow) uint8_t[canvas_width * canvas_height * bpp];
   if (!output) return ERR::Memory;

   objBitmap *inBmp;
   if (get_source_bitmap(Self->Filter, &inBmp, Self->SourceType, Self->Input, false) != ERR::Okay) return ERR::Failed;

   // Note: The inBmp->Data pointer is pre-adjusted to match the Clip Left and Top values (i.e. add 
   // (Clip.Left * BPP) + (Clip.Top * LineWidth) to Data in order to get its true value)

   if (Self->Filter->ColourSpace IS VCS::LINEAR_RGB) inBmp->convertToLinear();
   inBmp->premultiply();

   if ((canvas_width > Self->MatrixColumns*3) and (canvas_height > Self->MatrixRows*3)) {
      const int h_margin = Self->MatrixColumns>>1;
      const int v_margin = Self->MatrixRows>>1;
      
      int thread_count = std::thread::hardware_concurrency();
      if (thread_count > canvas_height / 4) thread_count = canvas_height / 4;

      dp::thread_pool pool(thread_count + 4);

      pool.enqueue_detach([Self, inBmp](uint8_t *Output, int L, int T, int R, int B) { // Left
         Self->processClipped(inBmp, Output, L, T, R, B);
      }, output, Self->Target->Clip.Left, Self->Target->Clip.Top, Self->Target->Clip.Left+h_margin, Self->Target->Clip.Bottom);

      pool.enqueue_detach([Self, inBmp](uint8_t *Output, int L, int T, int R, int B) { // Right
         Self->processClipped(inBmp, Output, L, T, R, B);
      }, output+((canvas_width-h_margin)*bpp), Self->Target->Clip.Right-h_margin, Self->Target->Clip.Top, Self->Target->Clip.Right, Self->Target->Clip.Bottom);

      pool.enqueue_detach([Self, inBmp](uint8_t *Output, int L, int T, int R, int B) { // Top
         Self->processClipped(inBmp, Output, L, T, R, B);
      }, output+(h_margin*bpp), Self->Target->Clip.Left+h_margin, Self->Target->Clip.Top, Self->Target->Clip.Right-h_margin, Self->Target->Clip.Top+v_margin);

      pool.enqueue_detach([Self, inBmp](uint8_t *Output, int L, int T, int R, int B) { // Bottom
         Self->processClipped(inBmp, Output, L, T, R, B);
      }, output+((canvas_height-v_margin)*bpp*canvas_width), Self->Target->Clip.Left+h_margin,  Self->Target->Clip.Bottom-v_margin, Self->Target->Clip.Right-h_margin, Self->Target->Clip.Bottom);

      // Center
      
      auto out = output + (h_margin * bpp) + (canvas_width * v_margin * bpp);
      auto left   = Self->Target->Clip.Left + h_margin;
      auto top    = Self->Target->Clip.Top + v_margin;
      auto right  = Self->Target->Clip.Right - h_margin;
      
      int lines_per_thread = canvas_height / thread_count;
      for (int i=0; i < thread_count; i++) {
         int bottom;
         if (i IS thread_count - 1) bottom = Self->Target->Clip.Bottom - v_margin;
         else bottom = top + lines_per_thread;

         pool.enqueue_detach([Self](objBitmap *Bitmap, uint8_t *Output, int L, int T, int R, int B) { 
            Self->processFast(Bitmap, Output, L, T, R, B);
         }, inBmp, out, left, top, right, bottom);

         top += lines_per_thread;
         out += lines_per_thread * canvas_width * bpp;
      }

      pool.wait_for_tasks();
   }
   else Self->processClipped(inBmp, output, Self->Target->Clip.Left, Self->Target->Clip.Top, Self->Target->Clip.Right, Self->Target->Clip.Bottom);

   // Copy the resulting output back to the bitmap.

   auto pixel = (uint32_t *)(Self->Target->Data + (Self->Target->Clip.Left * bpp) + (Self->Target->Clip.Top * Self->Target->LineWidth));
   auto src   = (uint32_t *)output;
   for (int y=0; y < canvas_height; y++) {
      copymem(src, pixel, size_t(bpp) * size_t(canvas_width));
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

   const int filter_size = Self->MatrixColumns * Self->MatrixRows;

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
      double divisor = 0;
      for (int i=0; i < filter_size; i++) divisor += Self->Matrix[i];
      if (!divisor) divisor = 1;
      Self->Divisor = divisor;
   }

   log.trace("Convolve Size: (%d,%d), Divisor: %g, Bias: %g", Self->MatrixColumns, Self->MatrixRows, Self->Divisor, Self->Bias);

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR CONVOLVEFX_Free(extConvolveFX *Self)
{
   Self->~extConvolveFX();
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR CONVOLVEFX_NewPlacement(extConvolveFX *Self)
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

static ERR CONVOLVEFX_GET_Bias(extConvolveFX *Self, double *Value)
{
   *Value = Self->Bias;
   return ERR::Okay;
}

static ERR CONVOLVEFX_SET_Bias(extConvolveFX *Self, double Value)
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

static ERR CONVOLVEFX_GET_Divisor(extConvolveFX *Self, double *Value)
{
   *Value = Self->Divisor;
   return ERR::Okay;
}

static ERR CONVOLVEFX_SET_Divisor(extConvolveFX *Self, double Value)
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

static ERR CONVOLVEFX_GET_Matrix(extConvolveFX *Self, double **Value, int *Elements)
{
   *Elements = Self->MatrixSize;
   *Value    = Self->Matrix;
   return ERR::Okay;
}

static ERR CONVOLVEFX_SET_Matrix(extConvolveFX *Self, double *Value, int Elements)
{
   pf::Log log;

   if ((Elements > 0) and (Elements <= std::ssize(Self->Matrix))) {
      Self->MatrixSize = Elements;
      copymem(Value, Self->Matrix, sizeof(double) * Elements);
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

static ERR CONVOLVEFX_GET_MatrixRows(extConvolveFX *Self, int *Value)
{
   *Value = Self->MatrixRows;
   return ERR::Okay;
}

static ERR CONVOLVEFX_SET_MatrixRows(extConvolveFX *Self, int Value)
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

static ERR CONVOLVEFX_GET_MatrixColumns(extConvolveFX *Self, int *Value)
{
   *Value = Self->MatrixColumns;
   return ERR::Okay;
}

static ERR CONVOLVEFX_SET_MatrixColumns(extConvolveFX *Self, int Value)
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

static ERR CONVOLVEFX_GET_PreserveAlpha(extConvolveFX *Self, int *Value)
{
   *Value = Self->PreserveAlpha;
   return ERR::Okay;
}

static ERR CONVOLVEFX_SET_PreserveAlpha(extConvolveFX *Self, int Value)
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

static ERR CONVOLVEFX_GET_TargetX(extConvolveFX *Self, int *Value)
{
   *Value = Self->TargetX;
   return ERR::Okay;
}

static ERR CONVOLVEFX_SET_TargetX(extConvolveFX *Self, int Value)
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

static ERR CONVOLVEFX_GET_TargetY(extConvolveFX *Self, int *Value)
{
   *Value = Self->TargetY;
   return ERR::Okay;
}

static ERR CONVOLVEFX_SET_TargetY(extConvolveFX *Self, int Value)
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

static ERR CONVOLVEFX_GET_UnitX(extConvolveFX *Self, double *Value)
{
   *Value = Self->UnitX;
   return ERR::Okay;
}

static ERR CONVOLVEFX_SET_UnitX(extConvolveFX *Self, double Value)
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

static ERR CONVOLVEFX_GET_UnitY(extConvolveFX *Self, double *Value)
{
   *Value = Self->UnitY;
   return ERR::Okay;
}

static ERR CONVOLVEFX_SET_UnitY(extConvolveFX *Self, double Value)
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
   { "EdgeMode",      FDF_VIRTUAL|FDF_INT|FDF_LOOKUP|FDF_RI,   CONVOLVEFX_GET_EdgeMode, CONVOLVEFX_SET_EdgeMode, &clEdgeMode },
   { "MatrixRows",    FDF_VIRTUAL|FDF_INT|FDF_RI,              CONVOLVEFX_GET_MatrixRows, CONVOLVEFX_SET_MatrixRows },
   { "MatrixColumns", FDF_VIRTUAL|FDF_INT|FDF_RI,              CONVOLVEFX_GET_MatrixColumns, CONVOLVEFX_SET_MatrixColumns },
   { "Matrix",        FDF_VIRTUAL|FDF_DOUBLE|FDF_ARRAY|FDF_RI, CONVOLVEFX_GET_Matrix, CONVOLVEFX_SET_Matrix },
   { "PreserveAlpha", FDF_VIRTUAL|FDF_INT|FDF_RW,              CONVOLVEFX_GET_PreserveAlpha, CONVOLVEFX_SET_PreserveAlpha },
   { "TargetX",       FDF_VIRTUAL|FDF_INT|FDF_RI,              CONVOLVEFX_GET_TargetX, CONVOLVEFX_SET_TargetX },
   { "TargetY",       FDF_VIRTUAL|FDF_INT|FDF_RI,              CONVOLVEFX_GET_TargetY, CONVOLVEFX_SET_TargetY },
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
