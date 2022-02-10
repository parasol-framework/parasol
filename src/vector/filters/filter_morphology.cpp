// Morph and dilate functionality shares the same code apart from the inner loop.

enum {
   OP_ERODE=0,
   OP_DILATE
};

class MorphEffect : public VectorEffect {
   LONG RX, RY;
   UBYTE Type;

   void erode() {
      if (Bitmap->BytesPerPixel != 4) return;

      const LONG canvasWidth = Bitmap->Clip.Right - Bitmap->Clip.Left;
      const LONG canvasHeight = Bitmap->Clip.Bottom - Bitmap->Clip.Top;

      if (canvasWidth * canvasHeight > 4096 * 4096) return; // Bail on really large bitmaps.

      const UBYTE A = Bitmap->ColourFormat->AlphaPos>>3;
      const UBYTE R = Bitmap->ColourFormat->RedPos>>3;
      const UBYTE G = Bitmap->ColourFormat->GreenPos>>3;
      const UBYTE B = Bitmap->ColourFormat->BluePos>>3;

      LONG radius;
      UBYTE *output = new (std::nothrow) UBYTE[canvasWidth * canvasHeight * Bitmap->BytesPerPixel];
      UBYTE *input = Bitmap->Data + (Bitmap->Clip.Top * Bitmap->LineWidth) + (Bitmap->Clip.Left * Bitmap->BytesPerPixel);

      if (!output) return;

      if ((radius = RX) > 0) { // Top-to-bottom dilate
         if (canvasWidth - 1 < radius) radius = canvasWidth - 1;

         const UBYTE *endinput = input + (radius * 4);
         UBYTE *inputline = input;
         UBYTE *outputline = output;

         for (int x=0; x < canvasWidth; ++x) {
            const UBYTE *lp = inputline;
            const UBYTE *end = endinput;
            UBYTE *dptr = outputline;
            for (int y = 0; y < canvasHeight; ++y) {
               UBYTE minB = 255, minG = 255, minR = 255, minA = 255;
               for (const UBYTE *p=lp; p <= end; p += 4) {
                  if (p[B] < minB) minB = p[B];
                  if (p[G] < minG) minG = p[G];
                  if (p[R] < minR) minR = p[R];
                  if (p[A] < minA) minA = p[A];
               }
               dptr[R] = minR; dptr[G] = minG; dptr[B] = minB; dptr[A] = minA;
               dptr += canvasWidth<<2;
               lp += Bitmap->LineWidth;
               end += Bitmap->LineWidth;
            }
            if (x >= radius) inputline += 4;
            if (x + radius < canvasWidth - 1) endinput += 4;
            outputline += 4;
         }

         // Copy the resulting output back to the bitmap.

         ULONG *pixel = (ULONG *)(Bitmap->Data + (Bitmap->Clip.Left<<2) + (Bitmap->Clip.Top * Bitmap->LineWidth));
         ULONG *src   = (ULONG *)output;
         for (LONG y=0; y < canvasHeight; y++) {
            for (LONG x=0; x < canvasWidth; x++) pixel[x] = src[x];
            pixel += Bitmap->LineWidth>>2;
            src += canvasWidth;
         }
      }

      if ((radius = RY) > 0) { // Left-to-right dilate
         if (canvasHeight - 1 < radius) radius = canvasHeight - 1;

         const UBYTE *endinput = input + (radius * Bitmap->LineWidth); // Inner-loop will stop when reaching endinput
         UBYTE *inputline = input;
         UBYTE *outputline = output;

         for (int y=0; y < canvasHeight; y++) {
            const UBYTE *lp = inputline;
            const UBYTE *end = endinput;
            UBYTE *dptr = outputline;
            for (int x=0; x < canvasWidth; x++) {
               UBYTE minB = 255, minG = 255, minR = 255, minA = 255;
               for (const UBYTE *p=lp; p <= end; p += Bitmap->LineWidth) {
                  if (p[B] < minB) minB = p[B];
                  if (p[G] < minG) minG = p[G];
                  if (p[R] < minR) minR = p[R];
                  if (p[A] < minA) minA = p[A];
               }
               dptr[R] = minR; dptr[G] = minG; dptr[B] = minB; dptr[A] = minA;
               dptr += 4;
               lp   += 4;
               end  += 4;
            }
            if (y >= radius) inputline += Bitmap->LineWidth;
            if (y + radius < canvasHeight - 1) endinput += Bitmap->LineWidth;
            outputline += canvasWidth<<2;
         }

         // Copy the resulting output back to the bitmap.

         ULONG *pixel = (ULONG *)(Bitmap->Data + (Bitmap->Clip.Left<<2) + (Bitmap->Clip.Top * Bitmap->LineWidth));
         ULONG *src   = (ULONG *)output;
         for (LONG y=0; y < canvasHeight; y++) {
            for (LONG x=0; x < canvasWidth; x++) pixel[x] = src[x];
            pixel += Bitmap->LineWidth>>2;
            src += canvasWidth;
         }
      }

      delete [] output;
   }

   void dilate() {
      if (Bitmap->BytesPerPixel != 4) return;

      const LONG canvasWidth = Bitmap->Clip.Right - Bitmap->Clip.Left;
      const LONG canvasHeight = Bitmap->Clip.Bottom - Bitmap->Clip.Top;

      if (canvasWidth * canvasHeight > 4096 * 4096) return; // Bail on really large bitmaps.

      const UBYTE A = Bitmap->ColourFormat->AlphaPos>>3;
      const UBYTE R = Bitmap->ColourFormat->RedPos>>3;
      const UBYTE G = Bitmap->ColourFormat->GreenPos>>3;
      const UBYTE B = Bitmap->ColourFormat->BluePos>>3;

      LONG radius;
      UBYTE *output = new (std::nothrow) UBYTE[canvasWidth * canvasHeight * Bitmap->BytesPerPixel];
      UBYTE *input = Bitmap->Data + (Bitmap->Clip.Top * Bitmap->LineWidth) + (Bitmap->Clip.Left * Bitmap->BytesPerPixel);

      if (!output) return;

      if ((radius = RX) > 0) { // Top-to-bottom dilate
         if (canvasWidth - 1 < radius) radius = canvasWidth - 1;

         const UBYTE *upperinput = input + (radius * 4);
         UBYTE *inputline = input;
         UBYTE *outputline = output;

         for (int x=0; x < canvasWidth; ++x) {
            const UBYTE *lp = inputline;
            const UBYTE *up = upperinput;
            UBYTE *dptr = outputline;
            for (int y = 0; y < canvasHeight; ++y) {
               UBYTE maxB = 0, maxG = 0, maxR = 0, maxA = 0;
               for (const UBYTE *p=lp; p <= up; p += 4) {
                  if (p[B] > maxB) maxB = p[B];
                  if (p[G] > maxG) maxG = p[G];
                  if (p[R] > maxR) maxR = p[R];
                  if (p[A] > maxA) maxA = p[A];
               }
               dptr[R] = maxR; dptr[G] = maxG; dptr[B] = maxB; dptr[A] = maxA;
               dptr += canvasWidth<<2;
               lp += Bitmap->LineWidth;
               up += Bitmap->LineWidth;
            }
            if (x >= radius) inputline += 4;
            if (x + radius < canvasWidth - 1) upperinput += 4;
            outputline += 4;
         }

         // Copy the resulting output back to the bitmap.

         ULONG *pixel = (ULONG *)(Bitmap->Data + (Bitmap->Clip.Left<<2) + (Bitmap->Clip.Top * Bitmap->LineWidth));
         ULONG *src   = (ULONG *)output;
         for (LONG y=0; y < canvasHeight; y++) {
            for (LONG x=0; x < canvasWidth; x++) pixel[x] = src[x];
            pixel += Bitmap->LineWidth>>2;
            src += canvasWidth;
         }
      }

      if ((radius = RY) > 0) { // Left-to-right dilate
         if (canvasHeight - 1 < radius) radius = canvasHeight - 1;

         const UBYTE *endinput = input + (radius * Bitmap->LineWidth); // Inner-loop will stop when reaching endinput
         UBYTE *inputline = input;
         UBYTE *outputline = output;

         for (int y=0; y < canvasHeight; y++) {
            const UBYTE *lp = inputline;
            const UBYTE *end = endinput;
            UBYTE *dptr = outputline;
            for (int x=0; x < canvasWidth; x++) {
               UBYTE maxB = 0, maxG = 0, maxR = 0, maxA = 0;
               for (const UBYTE *p=lp; p <= end; p += Bitmap->LineWidth) {
                  if (p[B] > maxB) maxB = p[B];
                  if (p[G] > maxG) maxG = p[G];
                  if (p[R] > maxR) maxR = p[R];
                  if (p[A] > maxA) maxA = p[A];
               }
               dptr[R] = maxR; dptr[G] = maxG; dptr[B] = maxB; dptr[A] = maxA;
               dptr += 4;
               lp   += 4;
               end  += 4;
            }
            if (y >= radius) inputline += Bitmap->LineWidth;
            if (y + radius < canvasHeight - 1) endinput += Bitmap->LineWidth;
            outputline += canvasWidth<<2;
         }

         // Copy the resulting output back to the bitmap.

         ULONG *pixel = (ULONG *)(Bitmap->Data + (Bitmap->Clip.Left<<2) + (Bitmap->Clip.Top * Bitmap->LineWidth));
         ULONG *src   = (ULONG *)output;
         for (LONG y=0; y < canvasHeight; y++) {
            for (LONG x=0; x < canvasWidth; x++) pixel[x] = src[x];
            pixel += Bitmap->LineWidth>>2;
            src += canvasWidth;
         }
      }

      delete [] output;
   }

public:
   MorphEffect(struct rkVectorFilter *Filter, XMLTag *Tag) : VectorEffect() {
      parasol::Log log(__FUNCTION__);

      RX = 0; // SVG default is 0
      RY = 0;

      for (LONG a=1; a < Tag->TotalAttrib; a++) {
         CSTRING val = Tag->Attrib[a].Value;
         if (!val) continue;

         ULONG hash = StrHash(Tag->Attrib[a].Name, FALSE);
         switch(hash) {
            case SVF_RADIUS: {
               DOUBLE x = -1, y = -1;
               read_numseq(val, &x, &y, TAGEND);
               RX = (x > 0) ? x : 0;
               RY = (y > 0) ? y : 0;
               break;
            }

            case SVF_OPERATOR:
               if (!StrMatch("erode", val)) Type = OP_ERODE;
               else if (!StrMatch("dilate", val)) Type = OP_DILATE;
               else log.warning("Unrecognised morphology operator '%s'", val);
               break;

            default: fe_default(Filter, this, hash, val); break;
         }
      }
   }

   void apply(objVectorFilter *Filter) {
      if (Type IS OP_ERODE) erode();
      else dilate();
   }

   virtual ~MorphEffect() { }
};
