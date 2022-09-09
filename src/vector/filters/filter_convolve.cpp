
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

   void xml(std::stringstream &Stream) { // TODO: Support exporting attributes
      Stream << "feConvolve";
   }

   inline UBYTE * getPixel(LONG X, LONG Y) const {
      if ((X >= OutBitmap->Clip.Left) and (X < OutBitmap->Clip.Right) and
          (Y >= OutBitmap->Clip.Top) and (Y < OutBitmap->Clip.Bottom)) {
         return OutBitmap->Data + (Y * OutBitmap->LineWidth) + (X<<2);
      }

      switch (EdgeMode) {
         default: return NULL;

         case EM_DUPLICATE:
            if (X < OutBitmap->Clip.Left) X = OutBitmap->Clip.Left;
            else if (X >= OutBitmap->Clip.Right) X = OutBitmap->Clip.Right - 1;
            if (Y < OutBitmap->Clip.Top) Y = OutBitmap->Clip.Top;
            else if (Y >= OutBitmap->Clip.Bottom) Y = OutBitmap->Clip.Bottom - 1;
            return OutBitmap->Data + (Y * OutBitmap->LineWidth) + (X<<2);

         case EM_WRAP:
            while (X < OutBitmap->Clip.Left) X += OutBitmap->Clip.Right - OutBitmap->Clip.Left;
            X %= OutBitmap->Clip.Right - OutBitmap->Clip.Left;
            while (Y < OutBitmap->Clip.Top) Y += OutBitmap->Clip.Bottom - OutBitmap->Clip.Top;
            Y %= OutBitmap->Clip.Bottom - OutBitmap->Clip.Top;
            return OutBitmap->Data + (Y * OutBitmap->LineWidth) + (X<<2);
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
         input   += InputBitmap->LineWidth;
         outline += (OutBitmap->Clip.Right - OutBitmap->Clip.Left)<<2;
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
               currentLine += InputBitmap->LineWidth;
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
         input  += InputBitmap->LineWidth;
         output += (InputBitmap->Clip.Right - InputBitmap->Clip.Left)<<2;
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
      EffectName    = "feConvolve";

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
            case SVF_PRESERVEALPHA:
               PreserveAlpha = (!StrMatch("true", val)) or (!StrMatch("1", val));
               break;

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
      if (OutBitmap->BytesPerPixel != 4) return;

      const LONG canvasWidth = OutBitmap->Clip.Right - OutBitmap->Clip.Left;
      const LONG canvasHeight = OutBitmap->Clip.Bottom - OutBitmap->Clip.Top;

      if (canvasWidth * canvasHeight > 4096 * 4096) return; // Bail on really large bitmaps.

      UBYTE *output = new (std::nothrow) UBYTE[canvasWidth * canvasHeight * OutBitmap->BytesPerPixel];
      if (!output) return;

      objBitmap *inBmp;
      if (get_source_bitmap(Filter, &inBmp, SourceType, InputID, true)) return;

      if ((canvasWidth > FilterWidth*3) and (canvasHeight > FilterHeight*3)) {
         const LONG ew = FilterWidth>>1;
         const LONG eh = FilterHeight>>1;
         processClipped(inBmp, output,                       OutBitmap->Clip.Left,     OutBitmap->Clip.Top, OutBitmap->Clip.Left+ew,  OutBitmap->Clip.Bottom); // Left
         processClipped(inBmp, output+((canvasWidth-ew)*4),  OutBitmap->Clip.Right-ew, OutBitmap->Clip.Top, OutBitmap->Clip.Right,    OutBitmap->Clip.Bottom); // Right
         processClipped(inBmp, output+(ew*4),                OutBitmap->Clip.Left+ew,  OutBitmap->Clip.Top, OutBitmap->Clip.Right-ew, OutBitmap->Clip.Top+eh); // Top
         processClipped(inBmp, output+((canvasHeight-eh)*4*canvasWidth), OutBitmap->Clip.Left+ew,  OutBitmap->Clip.Bottom-eh, OutBitmap->Clip.Right-ew, OutBitmap->Clip.Bottom); // Bottom
         // Center
         processFast(inBmp, output+(ew*4)+(canvasWidth*eh*4), OutBitmap->Clip.Left+ew, OutBitmap->Clip.Top+eh, OutBitmap->Clip.Right-ew, OutBitmap->Clip.Bottom-eh);
      }
      else processClipped(inBmp, output, OutBitmap->Clip.Left, OutBitmap->Clip.Top, OutBitmap->Clip.Right, OutBitmap->Clip.Bottom);

      // Copy the resulting output back to the bitmap.

      ULONG *pixel = (ULONG *)(OutBitmap->Data + (OutBitmap->Clip.Left<<2) + (OutBitmap->Clip.Top * OutBitmap->LineWidth));
      ULONG *src   = (ULONG *)output;
      for (LONG y=0; y < canvasHeight; y++) {
         CopyMemory(src, pixel, 4 * canvasWidth);
         pixel += OutBitmap->LineWidth>>2;
         src += canvasWidth;
      }

      delete [] output;

      demultiply_bitmap(inBmp);
   }

   virtual ~ConvolveEffect() { }
};

