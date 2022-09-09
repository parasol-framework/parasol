// Morph and dilate functionality shares the same code apart from the inner loop.

enum {
   OP_ERODE=0,
   OP_DILATE
};

class MorphEffect : public VectorEffect {
   LONG RX, RY;
   UBYTE Type;

   void xml(std::stringstream &Stream) {
      Stream << "feMorphology operator=\"";

      if (Type IS OP_ERODE) Stream << "erode\"";
      else Stream << "dilate\"";

      Stream << "radius=\"" << RX << " " << RY << "\"";
   }

public:
   MorphEffect(struct rkVectorFilter *Filter, XMLTag *Tag) : VectorEffect() {
      parasol::Log log(__FUNCTION__);

      EffectName = "feMorphology";

      RX = 0; // SVG default is 0
      RY = 0;
      Type = OP_ERODE;

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
      if (OutBitmap->BytesPerPixel != 4) return;

      const LONG canvasWidth = OutBitmap->Clip.Right - OutBitmap->Clip.Left;
      const LONG canvasHeight = OutBitmap->Clip.Bottom - OutBitmap->Clip.Top;

      if (canvasWidth * canvasHeight > 4096 * 4096) return; // Bail on really large bitmaps.

      const UBYTE A = OutBitmap->ColourFormat->AlphaPos>>3;
      const UBYTE R = OutBitmap->ColourFormat->RedPos>>3;
      const UBYTE G = OutBitmap->ColourFormat->GreenPos>>3;
      const UBYTE B = OutBitmap->ColourFormat->BluePos>>3;

      UBYTE *out_line;
      UBYTE *buffer = NULL;
      LONG out_linewidth;
      bool buffer_as_input;

      // A temporary buffer is required if we are applying the effect on both axis.  Otherwise we can
      // directly write to the target bitmap.

      if ((RX > 0) and (RY > 0)) {
         buffer = new (std::nothrow) UBYTE[canvasWidth * canvasHeight * 4];
         if (!buffer) return;
         out_line = buffer;
         out_linewidth = canvasWidth * 4;
         buffer_as_input = true;
      }
      else {
         out_line = (UBYTE *)(OutBitmap->Data + (OutBitmap->Clip.Left<<2) + (OutBitmap->Clip.Top * OutBitmap->LineWidth));
         out_linewidth = OutBitmap->LineWidth;
         buffer_as_input = false;
      }

      objBitmap *inBmp;
      if (get_source_bitmap(Filter, &inBmp, SourceType, InputID, false)) return;
      UBYTE *input = inBmp->Data + (inBmp->Clip.Top * inBmp->LineWidth) + (inBmp->Clip.Left * inBmp->BytesPerPixel);

      LONG radius;
      if ((radius = RX) > 0) { // Top-to-bottom dilate
         if (canvasWidth - 1 < radius) radius = canvasWidth - 1;

         const UBYTE *endinput  = input + (radius * 4);
         const UBYTE *inputline = input;

         for (int x=0; x < canvasWidth; ++x) {
            const UBYTE *in  = inputline;
            const UBYTE *end = endinput;
            auto out         = out_line;

            if (Type IS OP_DILATE) {
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

      if ((radius = RY) > 0) { // Left-to-right dilate
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

         out_line = (UBYTE *)(OutBitmap->Data + (OutBitmap->Clip.Left<<2) + (OutBitmap->Clip.Top * OutBitmap->LineWidth));
         out_linewidth = OutBitmap->LineWidth;

         for (int y=0; y < canvasHeight; y++) {
            const UBYTE *in = inputline;
            const UBYTE *end = endinput;
            auto out = out_line;

            if (Type IS OP_DILATE) {
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
            out_line += OutBitmap->LineWidth;
         }
      }

      if (buffer) delete [] buffer;
   }

   virtual ~MorphEffect() { }
};
