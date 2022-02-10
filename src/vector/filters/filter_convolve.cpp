
#include <array>

enum EdgeModeType { EM_DUPLICATE = 1, EM_WRAP, EM_NONE };

#define MAX_DIM 9

class ConvolveEffect : public VectorEffect {
   DOUBLE KernelUnitX, KernelUnitY;
   DOUBLE Divisor;
   DOUBLE Bias;
   LONG TargetX, TargetY;
   LONG FilterWidth, FilterHeight;
   LONG FilterSize;
   LONG EdgeMode;
   bool PreserveAlpha;
   std::array<DOUBLE, MAX_DIM * MAX_DIM> KernelMatrix;

   inline UBYTE * getPixel(LONG X, LONG Y) const {
      if ((X >= Bitmap->Clip.Left) and (X < Bitmap->Clip.Right) and
          (Y >= Bitmap->Clip.Top) and (Y < Bitmap->Clip.Bottom)) {
         return Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<2);
      }

      switch (EdgeMode) {
         default: return NULL;

         case EM_DUPLICATE:
            if (X < Bitmap->Clip.Left) X = Bitmap->Clip.Left;
            else if (X >= Bitmap->Clip.Right) X = Bitmap->Clip.Right - 1;
            if (Y < Bitmap->Clip.Top) Y = Bitmap->Clip.Top;
            else if (Y >= Bitmap->Clip.Bottom) Y = Bitmap->Clip.Bottom - 1;
            return Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<2);

         case EM_WRAP:
            while (X < Bitmap->Clip.Left) X += Bitmap->Clip.Right - Bitmap->Clip.Left;
            X %= Bitmap->Clip.Right - Bitmap->Clip.Left;
            while (Y < Bitmap->Clip.Top) Y += Bitmap->Clip.Bottom - Bitmap->Clip.Top;
            Y %= Bitmap->Clip.Bottom - Bitmap->Clip.Top;
            return Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<2);
      }
   }

   void processClipped(UBYTE *output, LONG pLeft, LONG pTop, LONG pRight, LONG pBottom) {
      const UBYTE A = Bitmap->ColourFormat->AlphaPos>>3;
      const UBYTE R = Bitmap->ColourFormat->RedPos>>3;
      const UBYTE G = Bitmap->ColourFormat->GreenPos>>3;
      const UBYTE B = Bitmap->ColourFormat->BluePos>>3;

      const DOUBLE factor = 1.0 / Divisor;

      UBYTE *input = Bitmap->Data + (pTop * Bitmap->LineWidth);
      UBYTE *outline = output;
      for (LONG y=pTop; y < pBottom; y++) {
         UBYTE *out = outline;
         for (LONG x=pLeft; x < pRight; x++) {
            DOUBLE r = 0.0, g = 0.0, b = 0.0, a = 0.0;

            // Multiply every value of the filter with corresponding image pixel

            UBYTE kv = 0;
            for (int fy=y-TargetY; fy < y+FilterHeight-TargetY; fy++) {
               for (int fx=x-TargetX; fx < x+FilterWidth-TargetX; fx++) {
                  UBYTE *pixel = getPixel(fx, fy);
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
         input   += Bitmap->LineWidth;
         outline += (Bitmap->Clip.Right - Bitmap->Clip.Left)<<2;
      }
   }

   // This algorithm is unclipped and performs no edge detection, so is unsafe to use near the edge of the bitmap.

   void processFast(UBYTE *output, LONG Left, LONG Top, LONG Right, LONG Bottom) {
      const UBYTE A = Bitmap->ColourFormat->AlphaPos>>3;
      const UBYTE R = Bitmap->ColourFormat->RedPos>>3;
      const UBYTE G = Bitmap->ColourFormat->GreenPos>>3;
      const UBYTE B = Bitmap->ColourFormat->BluePos>>3;

      const DOUBLE factor = 1.0 / Divisor;

      UBYTE *input = Bitmap->Data + (Top * Bitmap->LineWidth);
      for (LONG y=Top; y < Bottom; y++) {
         UBYTE *out = output;
         UBYTE *filterEdge = Bitmap->Data + (y-TargetY) * Bitmap->LineWidth;
         for (LONG x=Left; x < Right; x++) {
            DOUBLE r = 0.0, g = 0.0, b = 0.0, a = 0.0;
            UBYTE kv = 0;
            const LONG fxs = x - TargetX;
            const LONG fxe = x + FilterWidth - TargetX;
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
               currentLine += Bitmap->LineWidth;
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
         input += Bitmap->LineWidth;
         output += (Bitmap->Clip.Right - Bitmap->Clip.Left)<<2;
      }
   }

public:
   ConvolveEffect(struct rkVectorFilter *Filter, XMLTag *Tag) : VectorEffect() {
      parasol::Log log(__FUNCTION__);

      KernelUnitX   = 1;
      KernelUnitY   = 1;
      FilterWidth   = 3;
      FilterHeight  = 3;
      FilterSize    = 0;
      Divisor       = 0;
      Bias          = 0;
      EdgeMode      = EM_DUPLICATE;
      PreserveAlpha = false;

      LONG m = 0;
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
               FilterWidth  = ox;
               FilterHeight = oy;
               break;
            }

            case SVF_KERNELMATRIX: {
               while ((*val) and (m < (LONG)KernelMatrix.size())) {
                  DOUBLE dbl;
                  val = read_numseq(val, &dbl, TAGEND);
                  KernelMatrix[m++] = dbl;
               }
               break;
            }

            case SVF_DIVISOR: read_numseq(val, &Divisor, TAGEND); break;
            case SVF_BIAS:    read_numseq(val, &Bias, TAGEND); break;
            case SVF_TARGETX: tx = StrToInt(val); break;
            case SVF_TARGETY: ty = StrToInt(val); break;

            case SVF_EDGEMODE:
               if (!StrMatch("duplicate", val)) EdgeMode = EM_DUPLICATE;
               else if (!StrMatch("wrap", val)) EdgeMode = EM_WRAP;
               else if (!StrMatch("none", val)) EdgeMode = EM_NONE;
               break;

            case SVF_KERNELUNITLENGTH:
               read_numseq(val, &KernelUnitX, &KernelUnitY, TAGEND);
               if (KernelUnitX < 1) KernelUnitX = 1;
               if (KernelUnitY < 1) KernelUnitY = KernelUnitX;
               break;

            // The modifications will apply to R,G,B only when preserveAlpha is true.
            case SVF_PRESERVEALPHA: if ((!StrMatch("true", val)) or (!StrMatch("1", val))) PreserveAlpha = true;  break;

            default: fe_default(Filter, this, hash, val); break;
         }
      }

      if (FilterWidth * FilterHeight > MAX_DIM * MAX_DIM) {
         log.warning("Size of matrix exceeds internally imposed limits.");
         Error = ERR_BufferOverflow;
         return;
      }

      if (m != FilterWidth * FilterHeight) {
         log.warning("Matrix value count of %d does not match the matrix size (%d,%d)", m, FilterWidth, FilterHeight);
         Error = ERR_Failed;
         return;
      }

      FilterSize = FilterWidth * FilterHeight;

      // Use client-provided tx/ty values, otherwise default according to the SVG standard.

      if ((ty >= 0) and (ty < FilterHeight)) TargetY = ty;
      else TargetY = floor(FilterHeight/2);

      if ((tx >= 0) and (tx < FilterWidth)) TargetX = tx;
      else TargetX = floor(FilterWidth/2);

      if (!Divisor) { // Calculate the divisor value
         DOUBLE divisor = 0;
         for (UBYTE i=0; i < FilterSize; i++) divisor += KernelMatrix[i];
         if (!divisor) divisor = 1;
         Divisor = divisor;
      }

      log.trace("Convolve Size: (%d,%d), Divisor: %.2f, Bias: %.2f", FilterWidth, FilterHeight, Divisor, Bias);
   }

   void apply(objVectorFilter *Filter) {
      if (Bitmap->BytesPerPixel != 4) return;

      const LONG canvasWidth = Bitmap->Clip.Right - Bitmap->Clip.Left;
      const LONG canvasHeight = Bitmap->Clip.Bottom - Bitmap->Clip.Top;

      if (canvasWidth * canvasHeight > 4096 * 4096) return; // Bail on really large bitmaps.

      UBYTE *output = new (std::nothrow) UBYTE[canvasWidth * canvasHeight * Bitmap->BytesPerPixel];
      if (!output) return;

      if ((canvasWidth > FilterWidth*3) and (canvasHeight > FilterHeight*3)) {
         const LONG ew = FilterWidth>>1;
         const LONG eh = FilterHeight>>1;
         processClipped(output,                       Bitmap->Clip.Left,     Bitmap->Clip.Top, Bitmap->Clip.Left+ew,  Bitmap->Clip.Bottom); // Left
         processClipped(output+((canvasWidth-ew)*4),  Bitmap->Clip.Right-ew, Bitmap->Clip.Top, Bitmap->Clip.Right,    Bitmap->Clip.Bottom); // Right
         processClipped(output+(ew*4),                Bitmap->Clip.Left+ew,  Bitmap->Clip.Top, Bitmap->Clip.Right-ew, Bitmap->Clip.Top+eh); // Top
         processClipped(output+((canvasHeight-eh)*4*canvasWidth), Bitmap->Clip.Left+ew,  Bitmap->Clip.Bottom-eh, Bitmap->Clip.Right-ew, Bitmap->Clip.Bottom); // Bottom
         // Center
         processFast(output+(ew*4)+(canvasWidth*eh*4), Bitmap->Clip.Left+ew, Bitmap->Clip.Top+eh, Bitmap->Clip.Right-ew, Bitmap->Clip.Bottom-eh);
      }
      else processClipped(output, Bitmap->Clip.Left, Bitmap->Clip.Top, Bitmap->Clip.Right, Bitmap->Clip.Bottom);

      // Copy the resulting output back to the bitmap.

      ULONG *pixel = (ULONG *)(Bitmap->Data + (Bitmap->Clip.Left<<2) + (Bitmap->Clip.Top * Bitmap->LineWidth));
      ULONG *src   = (ULONG *)output;
      for (LONG y=0; y < canvasHeight; y++) {
         for (LONG x=0; x < canvasWidth; x++) pixel[x] = src[x];
         pixel += Bitmap->LineWidth>>2;
         src += canvasWidth;
      }

      delete [] output;
   }

   virtual ~ConvolveEffect() { }
};

