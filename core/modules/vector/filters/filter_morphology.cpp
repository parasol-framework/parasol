/*****************************************************************************

Morph and dilate share the same code apart from the inner loop.

*****************************************************************************/

enum {
   OP_ERODE=0,
   OP_DILATE
};

//****************************************************************************

static void erode(struct effect *Effect)
{
   objBitmap *bmp = Effect->Bitmap;
   if (bmp->BytesPerPixel != 4) return;

   const LONG canvasWidth = bmp->Clip.Right - bmp->Clip.Left;
   const LONG canvasHeight = bmp->Clip.Bottom - bmp->Clip.Top;

   if (canvasWidth * canvasHeight > 4096 * 4096) return; // Bail on really large bitmaps.

   const UBYTE A = bmp->ColourFormat->AlphaPos>>3;
   const UBYTE R = bmp->ColourFormat->RedPos>>3;
   const UBYTE G = bmp->ColourFormat->GreenPos>>3;
   const UBYTE B = bmp->ColourFormat->BluePos>>3;

   LONG radius;
   UBYTE *output = new (std::nothrow) UBYTE[canvasWidth * canvasHeight * bmp->BytesPerPixel];
   UBYTE *input = bmp->Data + (bmp->Clip.Top * bmp->LineWidth) + (bmp->Clip.Left * bmp->BytesPerPixel);

   if (!output) return;

   if ((radius = Effect->Morph.RX) > 0) { // Top-to-bottom dilate
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
            lp += bmp->LineWidth;
            end += bmp->LineWidth;
         }
         if (x >= radius) inputline += 4;
         if (x + radius < canvasWidth - 1) endinput += 4;
         outputline += 4;
      }

      // Copy the resulting output back to the bitmap.

      ULONG *pixel = (ULONG *)(bmp->Data + (bmp->Clip.Left<<2) + (bmp->Clip.Top * bmp->LineWidth));
      ULONG *src   = (ULONG *)output;
      for (LONG y=0; y < canvasHeight; y++) {
         for (LONG x=0; x < canvasWidth; x++) pixel[x] = src[x];
         pixel += bmp->LineWidth>>2;
         src += canvasWidth;
      }
   }

   if ((radius = Effect->Morph.RY) > 0) { // Left-to-right dilate
      if (canvasHeight - 1 < radius) radius = canvasHeight - 1;

      const UBYTE *endinput = input + (radius * bmp->LineWidth); // Inner-loop will stop when reaching endinput
      UBYTE *inputline = input;
      UBYTE *outputline = output;

      for (int y=0; y < canvasHeight; y++) {
         const UBYTE *lp = inputline;
         const UBYTE *end = endinput;
         UBYTE *dptr = outputline;
         for (int x=0; x < canvasWidth; x++) {
            UBYTE minB = 255, minG = 255, minR = 255, minA = 255;
            for (const UBYTE *p=lp; p <= end; p += bmp->LineWidth) {
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
         if (y >= radius) inputline += bmp->LineWidth;
         if (y + radius < canvasHeight - 1) endinput += bmp->LineWidth;
         outputline += canvasWidth<<2;
      }

      // Copy the resulting output back to the bitmap.

      ULONG *pixel = (ULONG *)(bmp->Data + (bmp->Clip.Left<<2) + (bmp->Clip.Top * bmp->LineWidth));
      ULONG *src   = (ULONG *)output;
      for (LONG y=0; y < canvasHeight; y++) {
         for (LONG x=0; x < canvasWidth; x++) pixel[x] = src[x];
         pixel += bmp->LineWidth>>2;
         src += canvasWidth;
      }
   }

   delete [] output;
}

//****************************************************************************

static void dilate(struct effect *Effect)
{
   objBitmap *bmp = Effect->Bitmap;
   if (bmp->BytesPerPixel != 4) return;

   const LONG canvasWidth = bmp->Clip.Right - bmp->Clip.Left;
   const LONG canvasHeight = bmp->Clip.Bottom - bmp->Clip.Top;

   if (canvasWidth * canvasHeight > 4096 * 4096) return; // Bail on really large bitmaps.

   const UBYTE A = bmp->ColourFormat->AlphaPos>>3;
   const UBYTE R = bmp->ColourFormat->RedPos>>3;
   const UBYTE G = bmp->ColourFormat->GreenPos>>3;
   const UBYTE B = bmp->ColourFormat->BluePos>>3;

   LONG radius;
   UBYTE *output = new (std::nothrow) UBYTE[canvasWidth * canvasHeight * bmp->BytesPerPixel];
   UBYTE *input = bmp->Data + (bmp->Clip.Top * bmp->LineWidth) + (bmp->Clip.Left * bmp->BytesPerPixel);

   if (!output) return;

   if ((radius = Effect->Morph.RX) > 0) { // Top-to-bottom dilate
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
            lp += bmp->LineWidth;
            up += bmp->LineWidth;
         }
         if (x >= radius) inputline += 4;
         if (x + radius < canvasWidth - 1) upperinput += 4;
         outputline += 4;
      }

      // Copy the resulting output back to the bitmap.

      ULONG *pixel = (ULONG *)(bmp->Data + (bmp->Clip.Left<<2) + (bmp->Clip.Top * bmp->LineWidth));
      ULONG *src   = (ULONG *)output;
      for (LONG y=0; y < canvasHeight; y++) {
         for (LONG x=0; x < canvasWidth; x++) pixel[x] = src[x];
         pixel += bmp->LineWidth>>2;
         src += canvasWidth;
      }
   }

   if ((radius = Effect->Morph.RY) > 0) { // Left-to-right dilate
      if (canvasHeight - 1 < radius) radius = canvasHeight - 1;

      const UBYTE *endinput = input + (radius * bmp->LineWidth); // Inner-loop will stop when reaching endinput
      UBYTE *inputline = input;
      UBYTE *outputline = output;

      for (int y=0; y < canvasHeight; y++) {
         const UBYTE *lp = inputline;
         const UBYTE *end = endinput;
         UBYTE *dptr = outputline;
         for (int x=0; x < canvasWidth; x++) {
            UBYTE maxB = 0, maxG = 0, maxR = 0, maxA = 0;
            for (const UBYTE *p=lp; p <= end; p += bmp->LineWidth) {
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
         if (y >= radius) inputline += bmp->LineWidth;
         if (y + radius < canvasHeight - 1) endinput += bmp->LineWidth;
         outputline += canvasWidth<<2;
      }

      // Copy the resulting output back to the bitmap.

      ULONG *pixel = (ULONG *)(bmp->Data + (bmp->Clip.Left<<2) + (bmp->Clip.Top * bmp->LineWidth));
      ULONG *src   = (ULONG *)output;
      for (LONG y=0; y < canvasHeight; y++) {
         for (LONG x=0; x < canvasWidth; x++) pixel[x] = src[x];
         pixel += bmp->LineWidth>>2;
         src += canvasWidth;
      }
   }

   delete [] output;
}

/*****************************************************************************
** Internal: apply_morph()
*/

static void apply_morph(objVectorFilter *Self, struct effect *Effect)
{
   if (Effect->Morph.Type IS OP_ERODE) erode(Effect);
   else dilate(Effect);
}

//****************************************************************************
// Create a new morph matrix filter.

static ERROR create_morph(objVectorFilter *Self, struct XMLTag *Tag)
{
   struct effect *effect;
   if (!(effect = add_effect(Self, FE_MORPHOLOGY))) return ERR_AllocMemory;

   effect->Morph.RX = 0; // SVG default is 0
   effect->Morph.RY = 0;

   for (LONG a=1; a < Tag->TotalAttrib; a++) {
      CSTRING val = Tag->Attrib[a].Value;
      if (!val) continue;

      ULONG hash = StrHash(Tag->Attrib[a].Name, FALSE);
      switch(hash) {
         case SVF_RADIUS: {
            DOUBLE x = -1, y = -1;
            read_numseq(val, &x, &y, TAGEND);
            if (x >= 0) effect->Morph.RX = x;
            else effect->Morph.RX = 0;

            if (y >= 0) effect->Morph.RY = y;
            else effect->Morph.RY = x;
            break;
         }

         case SVF_OPERATOR:
            if (!StrMatch("erode", val)) effect->Morph.Type = OP_ERODE;
            else if (!StrMatch("dilate", val)) effect->Morph.Type = OP_DILATE;
            else LogErrorMsg("Unrecognised morphology operator '%s'", val);
            break;

         default: fe_default(Self, effect, hash, val); break;
      }
   }
   return ERR_Okay;
}
