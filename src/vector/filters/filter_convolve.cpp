/*****************************************************************************

*****************************************************************************/

#include <array>

enum EdgeModeType { EM_DUPLICATE = 1, EM_WRAP, EM_NONE };

#define MAX_DIM 9

class ConvolveMatrix {
public:

   LONG TargetX, TargetY;
   DOUBLE KernelUnitX, KernelUnitY;
   UBYTE FilterWidth, FilterHeight;
   UBYTE FilterSize;
   DOUBLE Divisor;
   DOUBLE Bias;
   LONG EdgeMode;
   bool PreserveAlpha;
   std::array<DOUBLE, MAX_DIM * MAX_DIM> KernelMatrix;

   ConvolveMatrix() :
      KernelUnitX(1),
      KernelUnitY(1),
      FilterWidth(3),
      FilterHeight(3),
      FilterSize(0),
      Divisor(0),
      Bias(0),
      EdgeMode(EM_DUPLICATE),
      PreserveAlpha(false) { }

   inline UBYTE * getPixel(objBitmap &Bitmap, LONG X, LONG Y) const {
      if ((X >= Bitmap.Clip.Left) and (X < Bitmap.Clip.Right) and
          (Y >= Bitmap.Clip.Top) and (Y < Bitmap.Clip.Bottom)) {
         return Bitmap.Data + (Y * Bitmap.LineWidth) + (X<<2);
      }

      switch (EdgeMode) {
         default: return NULL;

         case EM_DUPLICATE:
            if (X < Bitmap.Clip.Left) X = Bitmap.Clip.Left;
            else if (X >= Bitmap.Clip.Right) X = Bitmap.Clip.Right - 1;
            if (Y < Bitmap.Clip.Top) Y = Bitmap.Clip.Top;
            else if (Y >= Bitmap.Clip.Bottom) Y = Bitmap.Clip.Bottom - 1;
            return Bitmap.Data + (Y * Bitmap.LineWidth) + (X<<2);

         case EM_WRAP:
            while (X < Bitmap.Clip.Left) X += Bitmap.Clip.Right - Bitmap.Clip.Left;
            X %= Bitmap.Clip.Right - Bitmap.Clip.Left;
            while (Y < Bitmap.Clip.Top) Y += Bitmap.Clip.Bottom - Bitmap.Clip.Top;
            Y %= Bitmap.Clip.Bottom - Bitmap.Clip.Top;
            return Bitmap.Data + (Y * Bitmap.LineWidth) + (X<<2);
      }
   }

   void apply(objVectorFilter *Self, VectorEffect *Filter)
   {
      objBitmap *bmp = Filter->Bitmap;
      if (bmp->BytesPerPixel != 4) return;

      const LONG canvasWidth = bmp->Clip.Right - bmp->Clip.Left;
      const LONG canvasHeight = bmp->Clip.Bottom - bmp->Clip.Top;

      if (canvasWidth * canvasHeight > 4096 * 4096) return; // Bail on really large bitmaps.

      UBYTE *output = new (std::nothrow) UBYTE[canvasWidth * canvasHeight * bmp->BytesPerPixel];
      if (!output) return;

      if ((canvasWidth > FilterWidth*3) and (canvasHeight > FilterHeight*3)) {
         const LONG ew = FilterWidth>>1;
         const LONG eh = FilterHeight>>1;
         processClipped(Filter, output,                       bmp->Clip.Left,     bmp->Clip.Top, bmp->Clip.Left+ew,  bmp->Clip.Bottom); // Left
         processClipped(Filter, output+((canvasWidth-ew)*4),  bmp->Clip.Right-ew, bmp->Clip.Top, bmp->Clip.Right,    bmp->Clip.Bottom); // Right
         processClipped(Filter, output+(ew*4),                bmp->Clip.Left+ew,  bmp->Clip.Top, bmp->Clip.Right-ew, bmp->Clip.Top+eh); // Top
         processClipped(Filter, output+((canvasHeight-eh)*4*canvasWidth), bmp->Clip.Left+ew,  bmp->Clip.Bottom-eh, bmp->Clip.Right-ew, bmp->Clip.Bottom); // Bottom
         // Center
         processFast(Filter, output+(ew*4)+(canvasWidth*eh*4), bmp->Clip.Left+ew, bmp->Clip.Top+eh, bmp->Clip.Right-ew, bmp->Clip.Bottom-eh);
      }
      else processClipped(Filter, output, bmp->Clip.Left, bmp->Clip.Top, bmp->Clip.Right, bmp->Clip.Bottom);

      // Copy the resulting output back to the bitmap.

      ULONG *pixel = (ULONG *)(bmp->Data + (bmp->Clip.Left<<2) + (bmp->Clip.Top * bmp->LineWidth));
      ULONG *src   = (ULONG *)output;
      for (LONG y=0; y < canvasHeight; y++) {
         for (LONG x=0; x < canvasWidth; x++) pixel[x] = src[x];
         pixel += bmp->LineWidth>>2;
         src += canvasWidth;
      }

      delete [] output;
   }

   void processClipped(VectorEffect *Filter, UBYTE *output, LONG Left, LONG Top, LONG Right, LONG Bottom)
   {
      objBitmap *bmp = Filter->Bitmap;

      const UBYTE A = bmp->ColourFormat->AlphaPos>>3;
      const UBYTE R = bmp->ColourFormat->RedPos>>3;
      const UBYTE G = bmp->ColourFormat->GreenPos>>3;
      const UBYTE B = bmp->ColourFormat->BluePos>>3;

      const DOUBLE factor = 1.0 / Divisor;

      UBYTE *input = bmp->Data + (Top * bmp->LineWidth);
      UBYTE *outline = output;
      for (LONG y=Top; y < Bottom; y++) {
         UBYTE *out = outline;
         for (LONG x=Left; x < Right; x++) {
            DOUBLE r = 0.0, g = 0.0, b = 0.0, a = 0.0;

            // Multiply every value of the filter with corresponding image pixel

            UBYTE kv = 0;
            for (int fy=y-TargetY; fy < y+FilterHeight-TargetY; fy++) {
               for (int fx=x-TargetX; fx < x+FilterWidth-TargetX; fx++) {
                  UBYTE *pixel = getPixel(*bmp, fx, fy);
                  if (pixel) {
                     r += pixel[R] * KernelMatrix[kv];
                     g += pixel[G] * KernelMatrix[kv];
                     b += pixel[B] * KernelMatrix[kv];
                     a += pixel[A] * KernelMatrix[kv];
                  }
                  kv++;
               }
            }

            LONG lr = F2I((factor * r) + Bias);
            LONG lg = F2I((factor * g) + Bias);
            LONG lb = F2I((factor * b) + Bias);
            out[R] = MIN(MAX(lr, 0), 255);
            out[G] = MIN(MAX(lg, 0), 255);
            out[B] = MIN(MAX(lb, 0), 255);
            if (!PreserveAlpha) out[A] = MIN(MAX(F2I(factor * a + Bias), 0), 255);
            else out[A] = (input + (x<<2))[A];
            out += 4;
         }
         input += bmp->LineWidth;
         outline += (bmp->Clip.Right - bmp->Clip.Left)<<2;
      }
   }

   // This algorithm is unclipped and performs no edge detection, so is unsafe to use near the edge of the bitmap.

   void processFast(VectorEffect *Filter, UBYTE *output, LONG Left, LONG Top, LONG Right, LONG Bottom)
   {
      objBitmap *bmp = Filter->Bitmap;

      const UBYTE A = bmp->ColourFormat->AlphaPos>>3;
      const UBYTE R = bmp->ColourFormat->RedPos>>3;
      const UBYTE G = bmp->ColourFormat->GreenPos>>3;
      const UBYTE B = bmp->ColourFormat->BluePos>>3;

      const DOUBLE factor = 1.0 / Divisor;

      UBYTE *input = bmp->Data + (Top * bmp->LineWidth);
      for (LONG y=Top; y < Bottom; y++) {
         UBYTE *out = output;
         UBYTE *filterEdge = bmp->Data + (y-TargetY) * bmp->LineWidth;
         for (LONG x=Left; x < Right; x++) {
            DOUBLE r = 0.0, g = 0.0, b = 0.0, a = 0.0;
            UBYTE kv = 0;
            const LONG fxs = x-TargetX;
            const LONG fxe = x+FilterWidth-TargetX;
            UBYTE *currentLine = filterEdge;
            for (int fy=y-TargetY; fy < y+FilterHeight-TargetY; fy++) {
               UBYTE *pixel = currentLine + (fxs<<2);
               for (int fx=fxs; fx < fxe; fx++) {
                  r += pixel[R] * KernelMatrix[kv];
                  g += pixel[G] * KernelMatrix[kv];
                  b += pixel[B] * KernelMatrix[kv];
                  a += pixel[A] * KernelMatrix[kv];
                  pixel += 4;
                  kv++;
               }
               currentLine += bmp->LineWidth;
            }

            LONG lr = F2I((factor * r) + Bias);
            LONG lg = F2I((factor * g) + Bias);
            LONG lb = F2I((factor * b) + Bias);
            out[R] = MIN(MAX(lr, 0), 255);
            out[G] = MIN(MAX(lg, 0), 255);
            out[B] = MIN(MAX(lb, 0), 255);
            if (!PreserveAlpha) out[A] = MIN(MAX(F2I(factor * a + Bias), 0), 255);
            else out[A] = (input + (x<<2))[A];
            out += 4;
         }
         input += bmp->LineWidth;
         output += (bmp->Clip.Right - bmp->Clip.Left)<<2;
      }
   }
};

//****************************************************************************

static void apply_convolve(objVectorFilter *Self, VectorEffect *Filter)
{
   if (!Filter->Convolve.Matrix) return;
   Filter->Convolve.Matrix->apply(Self, Filter);
}

//****************************************************************************
// Create a new convolve matrix filter.

static ERROR create_convolve(objVectorFilter *Self, XMLTag *Tag)
{
   parasol::Log log(__FUNCTION__);
   VectorEffect *effect;
   if (!(effect = add_effect(Self, FE_CONVOLVEMATRIX))) return ERR_AllocMemory;

   effect->Convolve.Matrix = new (std::nothrow) ConvolveMatrix;
   if (!effect->Convolve.Matrix) {
      remove_effect(Self, effect);
      return ERR_AllocMemory;
   }

   ConvolveMatrix &matrix = *effect->Convolve.Matrix;

   ULONG m = 0;

   LONG tx = -1;
   LONG ty = -1;
   for (LONG a=1; a < Tag->TotalAttrib; a++) {
      CSTRING val = Tag->Attrib[a].Value;
      if (!val) continue;

      ULONG hash = StrHash(Tag->Attrib[a].Name, FALSE);
      switch(hash) {
         case SVF_ORDER: {
            DOUBLE ox = 0, oy = 0;
            read_numseq(val, &ox, &oy, TAGEND);
            if (ox < 1) ox = 3;
            if (oy < 1) oy = ox;
            matrix.FilterWidth  = ox;
            matrix.FilterHeight = oy;
            break;
         }

         case SVF_KERNELMATRIX: {
            while ((*val) AND (m < matrix.KernelMatrix.size())) {
               DOUBLE dbl;
               val = read_numseq(val, &dbl, TAGEND);
               matrix.KernelMatrix[m++] = dbl;
            }
            break;
         }

         case SVF_DIVISOR: read_numseq(val, &matrix.Divisor, TAGEND); break;
         case SVF_BIAS:    read_numseq(val, &matrix.Bias, TAGEND); break;
         case SVF_TARGETX: tx = StrToInt(val); break;
         case SVF_TARGETY: ty = StrToInt(val); break;

         case SVF_EDGEMODE:
            if (!StrMatch("duplicate", val)) matrix.EdgeMode = EM_DUPLICATE;
            else if (!StrMatch("wrap", val)) matrix.EdgeMode = EM_WRAP;
            else if (!StrMatch("none", val)) matrix.EdgeMode = EM_NONE;
            break;

         case SVF_KERNELUNITLENGTH:
            read_numseq(val, &matrix.KernelUnitX, &matrix.KernelUnitY, TAGEND);
            if (matrix.KernelUnitX < 1) matrix.KernelUnitX = 1;
            if (matrix.KernelUnitY < 1) matrix.KernelUnitY = matrix.KernelUnitX;
            break;

         // The modifications will apply to R,G,B only when preserveAlpha is true.
         case SVF_PRESERVEALPHA: if ((!StrMatch("true", val)) OR (!StrMatch("1", val))) matrix.PreserveAlpha = true;  break;

         default: fe_default(Self, effect, hash, val); break;
      }
   }

   if (matrix.FilterWidth * matrix.FilterHeight > MAX_DIM * MAX_DIM) {
      log.warning("Size of matrix exceeds internally imposed limits.");
      remove_effect(Self, effect);
      return ERR_BufferOverflow;
   }

   if (m != matrix.FilterWidth * matrix.FilterHeight) {
      log.warning("Matrix value count of %d does not match the matrix size (%d,%d)", m, matrix.FilterWidth, matrix.FilterHeight);
      remove_effect(Self, effect);
      return ERR_Failed;
   }

   matrix.FilterSize = matrix.FilterWidth * matrix.FilterHeight;

   // Use client-provided tx/ty values, otherwise default according to the SVG standard.

   if ((ty >= 0) AND (ty < matrix.FilterHeight)) matrix.TargetY = ty;
   else matrix.TargetY = floor(matrix.FilterHeight/2);

   if ((tx >= 0) AND (tx < matrix.FilterWidth)) matrix.TargetX = tx;
   else matrix.TargetX = floor(matrix.FilterWidth/2);

   // Calculate the divisor value
   if (!matrix.Divisor) {
      DOUBLE divisor = 0;
      for (UBYTE i=0; i < matrix.FilterSize; i++) divisor += matrix.KernelMatrix[i];
      if (!divisor) divisor = 1;
      matrix.Divisor = divisor;
   }

   log.msg("Convolve Size: (%d,%d), Divisor: %.2f, Bias: %.2f", matrix.FilterWidth, matrix.FilterHeight, matrix.Divisor, matrix.Bias);
   return ERR_Okay;
}
