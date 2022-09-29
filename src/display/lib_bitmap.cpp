
#include "defs.h"

static size_t glDitherSize = 0;

//****************************************************************************
// NOTE: Please ensure that the Width and Height are already clipped to meet the restrictions of BOTH the source and
// destination bitmaps.

#define DITHER_ERROR(c)                  /* Dither one colour component */ \
   dif = (buf1[x].c>>3) - (brgb.c<<3);   /* An eighth of the error */ \
   if (dif) {                            \
      val3 = buf2[x+1].c + (dif<<1);     /* 1/4 down & right */ \
      dif = dif + dif + dif;             \
      val1 = buf1[x+1].c + dif;          /* 3/8 to the right */ \
      val2 = buf2[x].c + dif;            /* 3/8 to the next row */ \
      if (dif > 0) {                     /* Check for overflow */ \
         buf1[x+1].c = MIN(16383, val1); \
         buf2[x].c   = MIN(16383, val2); \
         buf2[x+1].c = MIN(16383, val3); \
      }                                  \
      else if (dif < 0) {                \
         buf1[x+1].c = MAX(0, val1);     \
         buf2[x].c   = MAX(0, val2);     \
         buf2[x+1].c = MAX(0, val3);     \
      }                                  \
   }

static ERROR dither(objBitmap *Bitmap, objBitmap *Dest, ColourFormat *Format, LONG Width, LONG Height,
   LONG SrcX, LONG SrcY, LONG DestX, LONG DestY)
{
   parasol::Log log(__FUNCTION__);
   RGB16 *buf1, *buf2, *buffer;
   RGB8 brgb;
   UBYTE *srcdata, *destdata, *data;
   LONG dif, val1, val2, val3;
   LONG x, y;
   ULONG colour;
   WORD index;
   UBYTE rmask, gmask, bmask;

   if ((Width < 1) or (Height < 1)) return ERR_Okay;

   // Punch the developer for making stupid mistakes

   if ((Dest->BitsPerPixel >= 24) and (!Format)) {
      log.warning("Dithering attempted to a %dbpp bitmap.", Dest->BitsPerPixel);
      return ERR_Failed;
   }

   // Do a straight copy if the bitmap is too small for dithering

   if ((Height < 2) or (Width < 2)) {
      for (y=SrcY; y < SrcY+Height; y++) {
         for (x=SrcX; x < SrcX+Width; x++) {
            Bitmap->ReadUCRPixel(Bitmap, x, y, &brgb);
            Dest->DrawUCRPixel(Dest, x, y, &brgb);
         }
      }
      return ERR_Okay;
   }

   // Allocate buffer for dithering

   if (Width * sizeof(RGB16) * 2 > glDitherSize) {
      if (glDither) { FreeResource(glDither); glDither = NULL; }

      if (AllocMemory(Width * sizeof(RGB16) * 2, MEM_NO_CLEAR|MEM_UNTRACKED, &glDither, NULL) != ERR_Okay) {
         return ERR_AllocMemory;
      }
      glDitherSize = Width * sizeof(RGB16) * 2;
   }

   buf1 = (RGB16 *)glDither;
   buf2 = buf1 + Width;

   // Prime buf2, which will be copied to buf1 at the start of the loop.  We work with six binary "decimal places" to reduce roundoff errors.

   for (x=0,index=0; x < Width; x++,index+=Bitmap->BytesPerPixel) {
      Bitmap->ReadUCRIndex(Bitmap, Bitmap->Data + index, &brgb);
      buf2[x].Red   = brgb.Red<<6;
      buf2[x].Green = brgb.Green<<6;
      buf2[x].Blue  = brgb.Blue<<6;
      buf2[x].Alpha = brgb.Alpha;
   }

   if (!Format) Format = &Dest->prvColourFormat;

   srcdata = Bitmap->Data + ((SrcY+1) * Bitmap->LineWidth);
   destdata = Dest->Data + (DestY * Dest->LineWidth);
   rmask = Format->RedMask   << Format->RedShift;
   gmask = Format->GreenMask << Format->GreenShift;
   bmask = Format->BlueMask  << Format->BlueShift;

   for (y=0; y < Height - 1; y++) {
      // Move line 2 to line 1, line 2 then is empty for reading the next row

      buffer = buf2;
      buf2 = buf1;
      buf1 = buffer;

      // Read the next source line

      if (Bitmap->BytesPerPixel IS 4) {
         buffer = buf2;
         data = srcdata+(SrcX<<2);
         for (x=0; x < Width; x++, data+=4, buffer++) {
            colour = ((ULONG *)data)[0];
            buffer->Red   = ((UBYTE)(colour >> Bitmap->prvColourFormat.RedPos))<<6;
            buffer->Green = ((UBYTE)(colour >> Bitmap->prvColourFormat.GreenPos))<<6;
            buffer->Blue  = ((UBYTE)(colour >> Bitmap->prvColourFormat.BluePos))<<6;
            buffer->Alpha = ((UBYTE)(colour >> Bitmap->prvColourFormat.AlphaPos));
         }
      }
      else if (Bitmap->BytesPerPixel IS 2) {
         buffer = buf2;
         data = srcdata+(SrcX<<1);
         for (x=0; x < Width; x++, data+=2, buffer++) {
            colour = ((UWORD *)data)[0];
            buffer->Red   = UnpackRed(Bitmap, colour)<<6;
            buffer->Green = UnpackGreen(Bitmap, colour)<<6;
            buffer->Blue  = UnpackBlue(Bitmap, colour)<<6;
         }
      }
      else {
         buffer = buf2;
         data = srcdata + (SrcX * Bitmap->BytesPerPixel);
         for (x=0; x < Width; x++, data+=Bitmap->BytesPerPixel, buffer++) {
            Bitmap->ReadUCRIndex(Bitmap, data, &brgb);
            buffer->Red   = brgb.Red<<6;
            buffer->Green = brgb.Green<<6;
            buffer->Blue  = brgb.Blue<<6;
         }
      }

      // Dither

      buffer = buf1;
      data = destdata + (DestX * Dest->BytesPerPixel);
      if (Dest->BytesPerPixel IS 2) {
         for (x=0; x < Width - 1; x++,data+=2,buffer++) {
            brgb.Red   = (buffer->Red>>6) & rmask;
            brgb.Green = (buffer->Green>>6) & gmask;
            brgb.Blue  = (buffer->Blue>>6) & bmask;
            ((UWORD *)data)[0] = ((brgb.Red>>Dest->prvColourFormat.RedShift) << Dest->prvColourFormat.RedPos) |
                                 ((brgb.Green>>Dest->prvColourFormat.GreenShift) << Dest->prvColourFormat.GreenPos) |
                                 ((brgb.Blue>>Dest->prvColourFormat.BlueShift) << Dest->prvColourFormat.BluePos);
            DITHER_ERROR(Red);
            DITHER_ERROR(Green);
            DITHER_ERROR(Blue);
         }
      }
      else if (Dest->BytesPerPixel IS 4) {
         for (x=0; x < Width-1; x++,data+=4,buffer++) {
            brgb.Red   = (buffer->Red>>6) & rmask;
            brgb.Green = (buffer->Green>>6) & gmask;
            brgb.Blue  = (buffer->Blue>>6) & bmask;
            ((ULONG *)data)[0] = PackPixelWBA(Dest, brgb.Red, brgb.Green, brgb.Blue, buffer->Alpha);
            DITHER_ERROR(Red);
            DITHER_ERROR(Green);
            DITHER_ERROR(Blue);
         }
      }
      else {
         for (x=0; x < Width - 1; x++,data+=Dest->BytesPerPixel,buffer++) {
            brgb.Red   = (buffer->Red>>6) & rmask;
            brgb.Green = (buffer->Green>>6) & gmask;
            brgb.Blue  = (buffer->Blue>>6) & bmask;
            Dest->DrawUCRIndex(Dest, data, &brgb);
            DITHER_ERROR(Red);
            DITHER_ERROR(Green);
            DITHER_ERROR(Blue);
         }
      }

      // Draw the last pixel in the row - no downward propagation

      brgb.Red   = buf1[Width-1].Red>>6;
      brgb.Green = buf1[Width-1].Green>>6;
      brgb.Blue  = buf1[Width-1].Blue>>6;
      brgb.Alpha = buf1[Width-1].Alpha;
      Dest->DrawUCRIndex(Dest, destdata + ((Width - 1) * Dest->BytesPerPixel), &brgb);

      srcdata += Bitmap->LineWidth;
      destdata += Dest->LineWidth;
   }

   // Draw the last row of pixels - no leftward propagation

   if (Bitmap != Dest) {
      for (x=0,index=0; x < Width; x++,index+=Dest->BytesPerPixel) {
         brgb.Red   = buf2[x].Red>>6;
         brgb.Green = buf2[x].Green>>6;
         brgb.Blue  = buf2[x].Blue>>6;
         brgb.Alpha = buf2[x].Alpha;
         Dest->DrawUCRIndex(Dest, destdata+index, &brgb);
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
Compress: Compresses bitmap data to save memory.

A bitmap can be compressed with the Compress() function to save memory when the bitmap is not in use.  This is useful
if a large bitmap needs to be stored in memory and it is anticipated that the bitmap will be used infrequently.

Once a bitmap is compressed, its image data is invalid.  Any attempt to access the bitmap image data will likely
result in a memory access fault.  The image data will remain invalid until the ~Decompress() function is
called to restore the bitmap to its original state.

The `BMF_COMPRESSED` bit will be set in the Flags field after a successful call to this function to indicate that the
bitmap is compressed.

-INPUT-
obj(Bitmap) Bitmap: Pointer to the @Bitmap that will be compressed.
int Level: Level of compression.  Zero uses a default setting (recommended), the maximum is 10.

-ERRORS-
Okay
Args
AllocMemory
ReallocMemory
CreateObject: A Compression object could not be created.

*****************************************************************************/

ERROR gfxCompress(objBitmap *Bitmap, LONG Level)
{
   return ActionTags(MT_BmpCompress, Bitmap, Level);
}

/*****************************************************************************

-FUNCTION-
Decompress: Decompresses a compressed bitmap.

The Decompress() function is used to restore a compressed bitmap to its original state.  If the bitmap is not
compressed, this function does nothing.

By default the original compression data will be terminated, however it can be retained by setting the RetainData
argument to TRUE.  Retaining the data will allow it to be decompressed on consecutive occasions.  Because both the raw
and compressed image data will be held in memory, it is recommended that CompressBitmap is called as soon as possible
with the Altered argument set to FALSE.  This will remove the raw image data from memory while retaining the original
compressed data without starting a recompression process.

-INPUT-
obj(Bitmap) Bitmap: Pointer to the @Bitmap that will be decompressed.
int RetainData:     Retains the compression data if TRUE.

-ERRORS-
Okay
AllocMemory

*****************************************************************************/

ERROR gfxDecompress(objBitmap *Bitmap, LONG RetainData)
{
   return ActionTags(MT_BmpDecompress, Bitmap, RetainData);
}

/*****************************************************************************

-FUNCTION-
CopyArea: Copies a rectangular area from one bitmap to another.

This function copies rectangular areas from one bitmap to another.  It performs a straight region-copy only, using the
fastest method available.  Bitmaps may be of a different type (e.g. bit depth), however this will result in performance
penalties.  The copy process will respect the clipping region defined in both the source and destination bitmap
objects.

If the `TRANSPARENT` flag is set in the source object, all colours that match the ColourIndex field will be ignored in
the copy operation.

To enable dithering, pass `BAF_DITHER` in the Flags argument.  The drawing algorithm will use dithering if the source
needs to be down-sampled to the target bitmap's bit depth.  To enable alpha blending, set `BAF_BLEND` (the source bitmap
will also need to have the `BMF_ALPHA_CHANNEL` flag set to indicate that an alpha channel is available).

-INPUT-
obj(Bitmap) Bitmap: The source bitmap.
obj(Bitmap) Dest: Pointer to the destination bitmap.
int(BAF) Flags: Special flags.
int X:      The horizontal position of the area to be copied.
int Y:      The vertical position of the area to be copied.
int Width:  The width of the area.
int Height: The height of the area.
int XDest:  The horizontal position to copy the area to.
int YDest:  The vertical position to copy the area to.

-ERRORS-
Okay:
NullArgs: The DestBitmap argument was not specified.
Mismatch: The destination bitmap is not a close enough match to the source bitmap in order to perform the blit.
-END-

*****************************************************************************/

UBYTE validate_clip(CSTRING Header, CSTRING Name, objBitmap *Bitmap)
{
   parasol::Log log(Header);

#ifdef DEBUG // Force break if clipping is wrong (use gdb)
   if (((Bitmap->XOffset + Bitmap->Clip.Right) > Bitmap->Width) or
       ((Bitmap->YOffset + Bitmap->Clip.Bottom) > Bitmap->Height) or
       ((Bitmap->XOffset + Bitmap->Clip.Left) < 0) or
       (Bitmap->Clip.Left >= Bitmap->Clip.Right) or
       (Bitmap->Clip.Top >= Bitmap->Clip.Bottom)) {
      DEBUG_BREAK
   }
#else
   if ((Bitmap->XOffset + Bitmap->Clip.Right) > Bitmap->Width) {
      log.warning("#%d %s: Invalid right-clip of %d (offset %d), limited to width of %d.", Bitmap->Head.UID, Name, Bitmap->Clip.Right, Bitmap->XOffset, Bitmap->Width);
      Bitmap->Clip.Right = Bitmap->Width - Bitmap->XOffset;
   }

   if ((Bitmap->YOffset + Bitmap->Clip.Bottom) > Bitmap->Height) {
      log.warning("#%d %s: Invalid bottom-clip of %d (offset %d), limited to height of %d.", Bitmap->Head.UID, Name, Bitmap->Clip.Bottom, Bitmap->YOffset, Bitmap->Height);
      Bitmap->Clip.Bottom = Bitmap->Height - Bitmap->YOffset;
   }

   if ((Bitmap->XOffset + Bitmap->Clip.Left) < 0) {
      log.warning("#%d %s: Invalid left-clip of %d (offset %d).", Bitmap->Head.UID, Name, Bitmap->Clip.Left, Bitmap->XOffset);
      Bitmap->XOffset = 0;
      Bitmap->Clip.Left = 0;
   }

   if ((Bitmap->YOffset + Bitmap->Clip.Top) < 0) {
      log.warning("#%d %s: Invalid top-clip of %d (offset %d).", Bitmap->Head.UID, Name, Bitmap->Clip.Top, Bitmap->YOffset);
      Bitmap->YOffset = 0;
      Bitmap->Clip.Top = 0;
   }

   if (Bitmap->Clip.Left >= Bitmap->Clip.Right) {
      log.warning("#%d %s: Left clip >= Right clip (%d >= %d)", Bitmap->Head.UID, Name, Bitmap->Clip.Left, Bitmap->Clip.Right);
      return 1;
   }

   if (Bitmap->Clip.Top >= Bitmap->Clip.Bottom) {
      log.warning("#%d %s: Top clip >= Bottom clip (%d >= %d)", Bitmap->Head.UID, Name, Bitmap->Clip.Top, Bitmap->Clip.Bottom);
      return 1;
   }
#endif

   return 0;
}

ERROR gfxCopyArea(objBitmap *Bitmap, objBitmap *dest, LONG Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG DestX, LONG DestY)
{
   parasol::Log log(__FUNCTION__);
   RGB8 pixel, src;
   UBYTE *srctable, *desttable;
   LONG i;
   ULONG colour;
   UBYTE *data, *srcdata;

   if (!dest) return ERR_NullArgs;
   if (dest->Head.ClassID != ID_BITMAP) {
      log.warning("Destination #%d is not a Bitmap.", dest->Head.UID);
      return ERR_InvalidObject;
   }

   if (!(Bitmap->Head.Flags & NF_INITIALISED)) return log.warning(ERR_NotInitialised);

   //log.trace("%dx%d,%dx%d to %dx%d, Offset: %dx%d to %dx%d", X, Y, Width, Height, DestX, DestY, Bitmap->XOffset, Bitmap->YOffset, dest->XOffset, dest->YOffset);

   if (validate_clip(__FUNCTION__, "Source", Bitmap)) return ERR_Okay;

   if (Bitmap != dest) { // Validate the clipping region of the destination
      if (validate_clip(__FUNCTION__, "Dest", dest)) return ERR_Okay;
   }

   if (Bitmap IS dest) { // Use this clipping routine only if we are copying within the same bitmap
      if (X < Bitmap->Clip.Left) {
         Width = Width - (Bitmap->Clip.Left - X);
         DestX = DestX + (Bitmap->Clip.Left - X);
         X = Bitmap->Clip.Left;
      }
      else if (X >= Bitmap->Clip.Right) {
         log.trace("Clipped: X >= Bitmap->ClipRight (%d >= %d)", X, Bitmap->Clip.Right);
         return ERR_Okay;
      }

      if (Y < Bitmap->Clip.Top) {
         Height = Height - (Bitmap->Clip.Top - Y);
         DestY  = DestY + (Bitmap->Clip.Top - Y);
         Y = Bitmap->Clip.Top;
      }
      else if (Y >= Bitmap->Clip.Bottom) {
         log.trace("Clipped: Y >= Bitmap->ClipBottom (%d >= %d)", Y, Bitmap->Clip.Bottom);
         return ERR_Okay;
      }

      // Clip the destination coordinates

      if ((DestX < dest->Clip.Left)) {
         Width = Width - (dest->Clip.Left - DestX);
         if (Width < 1) return ERR_Okay;
         X = X + (dest->Clip.Left - DestX);
         DestX = dest->Clip.Left;
      }
      else if (DestX >= dest->Clip.Right) {
         log.trace("Clipped: DestX >= RightClip (%d >= %d)", DestX, dest->Clip.Right);
         return ERR_Okay;
      }

      if ((DestY < dest->Clip.Top)) {
         Height = Height - (dest->Clip.Top - DestY);
         if (Height < 1) return ERR_Okay;
         Y = Y + (dest->Clip.Top - DestY);
         DestY = dest->Clip.Top;
      }
      else if (DestY >= dest->Clip.Bottom) {
         log.trace("Clipped: DestY >= BottomClip (%d >= %d)", DestY, dest->Clip.Bottom);
         return ERR_Okay;
      }

      // Clip the Width and Height

      if ((DestX + Width)   >= Bitmap->Clip.Right)  Width  = Bitmap->Clip.Right - DestX;
      if ((DestY + Height)  >= Bitmap->Clip.Bottom) Height = Bitmap->Clip.Bottom - DestY;

      if ((X + Width)  >= Bitmap->Clip.Right)  Width  = Bitmap->Clip.Right - X;
      if ((Y + Height) >= Bitmap->Clip.Bottom) Height = Bitmap->Clip.Bottom - Y;
   }
   else {
      // Check if the destination that we are copying to is within the drawable area.

      if (DestX < dest->Clip.Left) {
         Width = Width - (dest->Clip.Left - DestX);
         if (Width < 1) return ERR_Okay;
         X = X + (dest->Clip.Left - DestX);
         DestX = dest->Clip.Left;
      }
      else if (DestX >= dest->Clip.Right) return ERR_Okay;

      if (DestY < dest->Clip.Top) {
         Height = Height - (dest->Clip.Top - DestY);
         if (Height < 1) return ERR_Okay;
         Y = Y + (dest->Clip.Top - DestY);
         DestY = dest->Clip.Top;
      }
      else if (DestY >= dest->Clip.Bottom) return ERR_Okay;

      // Check if the source that we are copying from is within its own drawable area.

      if (X < Bitmap->Clip.Left) {
         DestX += (Bitmap->Clip.Left - X);
         Width = Width - (Bitmap->Clip.Left - X);
         if (Width < 1) return ERR_Okay;
         X = Bitmap->Clip.Left;
      }
      else if (X >= Bitmap->Clip.Right) return ERR_Okay;

      if (Y < Bitmap->Clip.Top) {
         DestY += (Bitmap->Clip.Top - Y);
         Height = Height - (Bitmap->Clip.Top - Y);
         if (Height < 1) return ERR_Okay;
         Y = Bitmap->Clip.Top;
      }
      else if (Y >= Bitmap->Clip.Bottom) return ERR_Okay;

      // Clip the Width and Height of the source area, based on the imposed clip region.

      if ((DestX + Width)  >= dest->Clip.Right) Width   = dest->Clip.Right - DestX;
      if ((DestY + Height) >= dest->Clip.Bottom) Height = dest->Clip.Bottom - DestY;
      if ((X + Width)  >= Bitmap->Clip.Right)  Width  = Bitmap->Clip.Right - X;
      if ((Y + Height) >= Bitmap->Clip.Bottom) Height = Bitmap->Clip.Bottom - Y;
   }

   if (Width < 1) return ERR_Okay;
   if (Height < 1) return ERR_Okay;

   // Adjust coordinates by offset values

   X += Bitmap->XOffset;
   Y += Bitmap->YOffset;
   DestX  += dest->XOffset;
   DestY  += dest->YOffset;

#ifdef _WIN32
   if (dest->win.Drawable) { // Destination is a window

      if (Bitmap->win.Drawable) { // Both the source and destination are window areas
         LONG error;
         if ((error = winBlit(dest->win.Drawable, DestX, DestY, Width, Height, Bitmap->win.Drawable, X, Y))) {
            char buffer[80];
            buffer[0] = 0;
            winGetError(error, buffer, sizeof(buffer));
            log.warning("BitBlt(): %s", buffer);
         }
      }
      else { // The source is a software image
         if ((Flags & BAF_BLEND) and (Bitmap->BitsPerPixel IS 32) and (Bitmap->Flags & BMF_ALPHA_CHANNEL)) {
            ULONG *srcdata;
            UBYTE destred, destgreen, destblue, red, green, blue, alpha;

            // 32-bit alpha blending is enabled

            srcdata = (ULONG *)(Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<2));

            while (Height > 0) {
               for (i=0; i < Width; i++) {
                  alpha = 255 - CFUnpackAlpha(&Bitmap->prvColourFormat, srcdata[i]);

                  if (alpha >= BLEND_MAX_THRESHOLD) {
                     red   = srcdata[i] >> Bitmap->prvColourFormat.RedPos;
                     green = srcdata[i] >> Bitmap->prvColourFormat.GreenPos;
                     blue  = srcdata[i] >> Bitmap->prvColourFormat.BluePos;
                     SetPixelV(dest->win.Drawable, DestX+i, DestY, (blue<<16) | (green<<8) | red);
                  }
                  else if (alpha >= BLEND_MIN_THRESHOLD) {
                     colour = GetPixel(dest->win.Drawable, DestX+i, DestY);
                     destred   = colour & 0xff;
                     destgreen = (colour>>8) & 0xff;
                     destblue  = (colour>>16) & 0xff;
                     red   = srcdata[i] >> Bitmap->prvColourFormat.RedPos;
                     green = srcdata[i] >> Bitmap->prvColourFormat.GreenPos;
                     blue  = srcdata[i] >> Bitmap->prvColourFormat.BluePos;
                     red   = destred   + (((red   - destred)   * alpha)>>8);
                     green = destgreen + (((green - destgreen) * alpha)>>8);
                     blue  = destblue  + (((blue  - destblue)  * alpha)>>8);
                     SetPixelV(dest->win.Drawable, DestX+i, DestY, (blue<<16) | (green<<8) | red);
                  }
               }
               srcdata = (ULONG *)(((UBYTE *)srcdata) + Bitmap->LineWidth);
               DestY++;
               Height--;
            }
         }
         else if (Bitmap->Flags & BMF_TRANSPARENT) {
            ULONG wincolour;
            while (Height > 0) {
               for (i=0; i < Width; i++) {
                  colour = Bitmap->ReadUCPixel(Bitmap, X + i, Y);
                  if (colour != (ULONG)Bitmap->TransIndex) {
                     wincolour = UnpackRed(Bitmap, colour);
                     wincolour |= UnpackGreen(Bitmap, colour)<<8;
                     wincolour |= UnpackBlue(Bitmap, colour)<<16;
                     SetPixelV(dest->win.Drawable, DestX + i, DestY, wincolour);
                  }
               }
               Y++; DestY++;
               Height--;
            }
         }
         else  {
            winSetDIBitsToDevice(dest->win.Drawable, DestX, DestY, Width, Height, X, Y,
               Bitmap->Width, Bitmap->Height, Bitmap->BitsPerPixel, Bitmap->Data,
               Bitmap->ColourFormat->RedMask   << Bitmap->ColourFormat->RedPos,
               Bitmap->ColourFormat->GreenMask << Bitmap->ColourFormat->GreenPos,
               Bitmap->ColourFormat->BlueMask  << Bitmap->ColourFormat->BluePos);
         }
      }

      return ERR_Okay;
   }

#elif __xwindows__

   // Use this routine if the destination is a pixmap (write only memory).  X11 windows are always represented as pixmaps.

   if ((dest->Flags & BMF_X11_DGA) and (glDGAAvailable) and (dest != Bitmap)) {
      // We have direct access to the graphics address, so drop through to the software routine
      dest->Data = (UBYTE *)glDGAVideo;
   }
   else if (dest->x11.drawable) {
      if (!Bitmap->x11.drawable) {
         if ((Flags & BAF_BLEND) and (Bitmap->BitsPerPixel IS 32) and (Bitmap->Flags & BMF_ALPHA_CHANNEL)) {
            ULONG *srcdata;
            UBYTE alpha;
            WORD cl, cr, ct, cb;

            cl = dest->Clip.Left;
            cr = dest->Clip.Right;
            ct = dest->Clip.Top;
            cb = dest->Clip.Bottom;
            dest->Clip.Left   = DestX - dest->XOffset;
            dest->Clip.Right  = DestX + Width - dest->XOffset;
            dest->Clip.Top    = DestY - dest->YOffset;
            dest->Clip.Bottom = DestY + Height - dest->YOffset;
            if (!lock_surface(dest, SURFACE_READ|SURFACE_WRITE)) {
               srcdata = (ULONG *)(Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<2));

               while (Height > 0) {
                  for (i=0; i < Width; i++) {
                     alpha = 255 - UnpackAlpha(Bitmap, srcdata[i]);

                     if (alpha >= BLEND_MAX_THRESHOLD) {
                        pixel.Red   = (UBYTE)(srcdata[i] >> Bitmap->prvColourFormat.RedPos);
                        pixel.Green = (UBYTE)(srcdata[i] >> Bitmap->prvColourFormat.GreenPos);
                        pixel.Blue  = (UBYTE)(srcdata[i] >> Bitmap->prvColourFormat.BluePos);
                        dest->DrawUCRPixel(dest, DestX+i, DestY, &pixel);
                     }
                     else if (alpha >= BLEND_MIN_THRESHOLD) {
                        dest->ReadUCRPixel(dest, DestX+i, DestY, &pixel);
                        pixel.Red   += ((((UBYTE)(srcdata[i] >> Bitmap->prvColourFormat.RedPos)   - pixel.Red)   * alpha)>>8);
                        pixel.Green += ((((UBYTE)(srcdata[i] >> Bitmap->prvColourFormat.GreenPos) - pixel.Green) * alpha)>>8);
                        pixel.Blue  += ((((UBYTE)(srcdata[i] >> Bitmap->prvColourFormat.BluePos)  - pixel.Blue)  * alpha)>>8);
                        dest->DrawUCRPixel(dest, DestX+i, DestY, &pixel);
                     }
                  }
                  srcdata = (ULONG *)(((UBYTE *)srcdata) + Bitmap->LineWidth);
                  DestY++;
                  Height--;
               }
               unlock_surface(dest);
            }
            dest->Clip.Left   = cl;
            dest->Clip.Right  = cr;
            dest->Clip.Top    = ct;
            dest->Clip.Bottom = cb;
         }
         else if (Bitmap->Flags & BMF_TRANSPARENT) {
            while (Height > 0) {
               for (i = 0; i < Width; i++) {
                  colour = Bitmap->ReadUCPixel(Bitmap, X + i, Y);
                  if (colour != (ULONG)Bitmap->TransIndex) dest->DrawUCPixel(dest, DestX + i, DestY, colour);
               }
               Y++; DestY++;
               Height--;
            }
         }
         else {
            // Source is an ximage, destination is a pixmap

            if (Bitmap->x11.XShmImage IS TRUE)  {
               if (XShmPutImage(XDisplay, dest->x11.drawable, glXGC, &Bitmap->x11.ximage, X, Y, DestX, DestY, Width, Height, False)) {

               }
               else log.warning("XShmPutImage() failed.");
            }
            else {
               XPutImage(XDisplay, dest->x11.drawable, glXGC,
                  &Bitmap->x11.ximage, X, Y, DestX, DestY, Width, Height);
            }
         }
      }
      else {
         // Both the source and the destination are pixmaps

         XCopyArea(XDisplay, Bitmap->x11.drawable, dest->x11.drawable,
            glXGC, X, Y, Width, Height, DestX, DestY);
      }

      return ERR_Okay;
   }

#elif _GLES_

   if (dest->DataFlags & MEM_VIDEO) { // Destination is the video display.
      if (Bitmap->DataFlags & MEM_VIDEO) { // Source is the video display.
         // No simple way to support this in OpenGL - we have to copy the display into a texture buffer, then copy the texture back to the display.

         ERROR error;
         if (!lock_graphics_active(__func__)) {
            GLuint texture;
            if (alloc_texture(Bitmap->Width, Bitmap->Height, &texture) IS GL_NO_ERROR) {
               //glViewport(0, 0, Bitmap->Width, Bitmap->Height);  // Set viewport so it matches texture size of ^2
               glCopyTexImage2D(GL_TEXTURE_2D, 0, Bitmap->prvGLPixel, 0, 0, Bitmap->Width, Bitmap->Height, 0); // Copy screen to texture
               //glViewport(0, 0, Bitmap->Width, Bitmap->Height);  // Restore viewport to display size
               glDrawTexiOES(DestX, -DestY, 1, Bitmap->Width, Bitmap->Height);
               glBindTexture(GL_TEXTURE_2D, 0);
               eglSwapBuffers(glEGLDisplay, glEGLSurface);
               glDeleteTextures(1, &texture);
               error = ERR_Okay;
            }
            else error = log.warning(ERR_OpenGL);

            unlock_graphics();
         }
         else error = ERR_LockFailed;

         return error;
      }
      else if (Bitmap->DataFlags & MEM_TEXTURE) {
         // Texture-to-video blitting (


      }
      else {
         // RAM-to-video blitting.  We have to allocate a temporary texture, copy the data to it and then blit that to the display.

         ERROR error;
         if (!lock_graphics_active(__func__)) {
            GLuint texture;
            if (alloc_texture(Bitmap->Width, Bitmap->Height, &texture) IS GL_NO_ERROR) {
               glTexImage2D(GL_TEXTURE_2D, 0, Bitmap->prvGLPixel, Bitmap->Width, Bitmap->Height, 0, Bitmap->prvGLPixel, Bitmap->prvGLFormat, Bitmap->Data); // Copy the bitmap content to the texture.
               if (glGetError() IS GL_NO_ERROR) {
                  glDrawTexiOES(0, 0, 1, Bitmap->Width, Bitmap->Height);
                  glBindTexture(GL_TEXTURE_2D, 0);
                  eglSwapBuffers(glEGLDisplay, glEGLSurface);
               }
               else error = ERR_OpenGL;

               glDeleteTextures(1, &texture);
               error = ERR_Okay;
            }
            else error = log.warning(ERR_OpenGL);

            unlock_graphics();
         }
         else error = ERR_LockFailed;

         return error;
      }
   }

#endif

   // GENERIC SOFTWARE BLITTING ROUTINES

   if ((Flags & BAF_BLEND) and (Bitmap->BitsPerPixel IS 32) and (Bitmap->Flags & BMF_ALPHA_CHANNEL)) {
      // 32-bit alpha blending support

      if (!lock_surface(Bitmap, SURFACE_READ)) {
         if (!lock_surface(dest, SURFACE_WRITE)) {
            UBYTE red, green, blue, *dest_lookup;
            UWORD alpha;

            dest_lookup = glAlphaLookup + (255<<8);

            if (dest->BitsPerPixel IS 32) { // Both bitmaps are 32 bit
               const UBYTE sA = Bitmap->ColourFormat->AlphaPos>>3;
               const UBYTE sR = Bitmap->ColourFormat->RedPos>>3;
               const UBYTE sG = Bitmap->ColourFormat->GreenPos>>3;
               const UBYTE sB = Bitmap->ColourFormat->BluePos>>3;
               const UBYTE dA = dest->ColourFormat->AlphaPos>>3;
               const UBYTE dR = dest->ColourFormat->RedPos>>3;
               const UBYTE dG = dest->ColourFormat->GreenPos>>3;
               const UBYTE dB = dest->ColourFormat->BluePos>>3;

               UBYTE *sdata = Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<2);
               UBYTE *ddata = dest->Data + (DestY * dest->LineWidth) + (DestX<<2);

               if (Flags & BAF_COPY) { // Avoids blending in cases where the destination pixel is zero alpha.
                  for (LONG y=0; y < Height; y++) {
                     UBYTE *sp = sdata, *dp = ddata;
                     for (LONG x=0; x < Width; x++) {
                        if (dp[dA]) {
                           if (sp[sA] IS 0xff) ((ULONG *)dp)[0] = ((ULONG *)sp)[0];
                           else if (sp[sA]) {
                              dp[dR] = dp[dR] + (((sp[sR] - dp[dR]) * sp[sA])>>8);
                              dp[dG] = dp[dG] + (((sp[sG] - dp[dG]) * sp[sA])>>8);
                              dp[dB] = dp[dB] + (((sp[sB] - dp[dB]) * sp[sA])>>8);
                              dp[dA] = dp[dA] + ((sp[sA] * (0xff-dp[dA]))>>8);
                           }
                        }
                        else ((ULONG *)dp)[0] = ((ULONG *)sp)[0];

                        sp += 4;
                        dp += 4;
                     }
                     sdata += Bitmap->LineWidth;
                     ddata += dest->LineWidth;
                  }
               }
               else {
                  while (Height > 0) {
                     UBYTE *sp = sdata, *dp = ddata;
                     if (Bitmap->Opacity IS 0xff) {
                        for (i=0; i < Width; i++) {
                           if (sp[sA] IS 0xff) ((ULONG *)dp)[0] = ((ULONG *)sp)[0];
                           else if (sp[sA]) {
                              const UBYTE alpha = sp[sA];
                              dp[dR] = dp[dR] + (((sp[sR] - dp[dR]) * alpha)>>8);
                              dp[dG] = dp[dG] + (((sp[sG] - dp[dG]) * alpha)>>8);
                              dp[dB] = dp[dB] + (((sp[sB] - dp[dB]) * alpha)>>8);
                              dp[dA] = dp[dA] + ((sp[sA] * (0xff-dp[dA]))>>8);
                           }

                           sp += 4;
                           dp += 4;
                        }
                     }
                     else {
                        for (i=0; i < Width; i++) {
                           if (sp[sA]) {
                              const UBYTE alpha = (sp[sA] * Bitmap->Opacity)>>8;
                              dp[dR] = dp[dR] + (((sp[sR] - dp[dR]) * alpha)>>8);
                              dp[dG] = dp[dG] + (((sp[sG] - dp[dG]) * alpha)>>8);
                              dp[dB] = dp[dB] + (((sp[sB] - dp[dB]) * alpha)>>8);
                              dp[dA] = dp[dA] + ((sp[sA] * (0xff-dp[dA]))>>8);
                           }

                           sp += 4;
                           dp += 4;
                        }
                     }
                     sdata += Bitmap->LineWidth;
                     ddata += dest->LineWidth;
                     Height--;
                  }
               }
            }
            else if (dest->BytesPerPixel IS 2) {
               UWORD *ddata;
               ULONG *sdata = (ULONG *)(Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<2));
               ddata = (UWORD *)(dest->Data + (DestY * dest->LineWidth) + (DestX<<1));
               while (Height > 0) {
                  for (i=0; i < Width; i++) {
                     colour = sdata[i];
                     alpha = ((UBYTE)(colour >> Bitmap->prvColourFormat.AlphaPos));
                     alpha = (glAlphaLookup + (alpha<<8))[Bitmap->Opacity]<<8; // Multiply the source pixel by overall translucency level

                     if (alpha >= BLEND_MAX_THRESHOLD<<8) {
                        ddata[i] = PackPixel(dest, (UBYTE)(colour >> Bitmap->prvColourFormat.RedPos),
                                                   (UBYTE)(colour >> Bitmap->prvColourFormat.GreenPos),
                                                   (UBYTE)(colour >> Bitmap->prvColourFormat.BluePos));
                     }
                     else if (alpha >= BLEND_MIN_THRESHOLD<<8) {
                        red   = colour >> Bitmap->prvColourFormat.RedPos;
                        green = colour >> Bitmap->prvColourFormat.GreenPos;
                        blue  = colour >> Bitmap->prvColourFormat.BluePos;
                        srctable  = glAlphaLookup + (alpha);
                        desttable = dest_lookup - (alpha);
                        ddata[i] = PackPixel(dest, (UBYTE)(srctable[red]   + desttable[UnpackRed(dest, ddata[i])]),
                                                   (UBYTE)(srctable[green] + desttable[UnpackGreen(dest, ddata[i])]),
                                                   (UBYTE)(srctable[blue]  + desttable[UnpackBlue(dest, ddata[i])]));
                     }
                  }
                  sdata = (ULONG *)(((UBYTE *)sdata) + Bitmap->LineWidth);
                  ddata = (UWORD *)(((UBYTE *)ddata) + dest->LineWidth);
                  Height--;
               }
            }
            else {
               ULONG *sdata = (ULONG *)(Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<2));
               while (Height > 0) {
                  for (i=0; i < Width; i++) {
                     colour = sdata[i];
                     alpha = ((UBYTE)(colour >> Bitmap->prvColourFormat.AlphaPos));
                     alpha = (glAlphaLookup + (alpha<<8))[Bitmap->Opacity]; // Multiply the source pixel by overall translucency level

                     if (alpha >= BLEND_MAX_THRESHOLD) {
                        pixel.Red   = colour >> Bitmap->prvColourFormat.RedPos;
                        pixel.Green = colour >> Bitmap->prvColourFormat.GreenPos;
                        pixel.Blue  = colour >> Bitmap->prvColourFormat.BluePos;
                        dest->DrawUCRPixel(dest, DestX + i, DestY, &pixel);
                     }
                     else if (alpha >= BLEND_MIN_THRESHOLD) {
                        red   = colour >> Bitmap->prvColourFormat.RedPos;
                        green = colour >> Bitmap->prvColourFormat.GreenPos;
                        blue  = colour >> Bitmap->prvColourFormat.BluePos;

                        srctable  = glAlphaLookup + (alpha<<8);
                        desttable = glAlphaLookup + ((255-alpha)<<8);

                        dest->ReadUCRPixel(dest, DestX + i, DestY, &pixel);
                        pixel.Red   = srctable[red]   + desttable[pixel.Red];
                        pixel.Green = srctable[green] + desttable[pixel.Green];
                        pixel.Blue  = srctable[blue]  + desttable[pixel.Blue];
                        dest->DrawUCRPixel(dest, DestX + i, DestY, &pixel);
                     }
                  }
                  sdata = (ULONG *)(((UBYTE *)sdata) + Bitmap->LineWidth);
                  DestY++;
                  Height--;
               }
            }

            unlock_surface(dest);
         }
         unlock_surface(Bitmap);
      }

      return ERR_Okay;
   }
   else if (Bitmap->Flags & BMF_TRANSPARENT) {
      // Transparent colour copying.  In this mode, the alpha component of individual source pixels is ignored

      if (!lock_surface(Bitmap, SURFACE_READ)) {
         if (!lock_surface(dest, SURFACE_WRITE)) {
            if (Bitmap->Opacity < 255) { // Transparent mask with translucent pixels (consistent blend level)
               srctable  = glAlphaLookup + (Bitmap->Opacity<<8);
               desttable = glAlphaLookup + ((255-Bitmap->Opacity)<<8);
               while (Height > 0) {
                  for (i=0; i < Width; i++) {
                     colour = Bitmap->ReadUCPixel(Bitmap, X + i, Y);
                     if (colour != (ULONG)Bitmap->TransIndex) {
                        dest->ReadUCRPixel(dest, DestX + i, DestY, &pixel);

                        pixel.Red   = srctable[UnpackRed(Bitmap, colour)]   + desttable[pixel.Red];
                        pixel.Green = srctable[UnpackGreen(Bitmap, colour)] + desttable[pixel.Green];
                        pixel.Blue  = srctable[UnpackBlue(Bitmap, colour)]  + desttable[pixel.Blue];

                        dest->DrawUCRPixel(dest, DestX + i, DestY, &pixel);
                     }
                  }
                  Y++; DestY++;
                  Height--;
               }
            }
            else if (Bitmap->BitsPerPixel IS dest->BitsPerPixel) {
               if (Bitmap->BytesPerPixel IS 4) {
                  ULONG *ddata, *sdata;

                  sdata = (ULONG *)(Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<2));
                  ddata = (ULONG *)(dest->Data + (DestY * dest->LineWidth) + (DestX<<2));
                  colour = Bitmap->TransIndex;
                  while (Height > 0) {
                     for (i=0; i < Width; i++) if (sdata[i] != colour) ddata[i] = sdata[i];
                     ddata = (ULONG *)(((BYTE *)ddata) + dest->LineWidth);
                     sdata = (ULONG *)(((BYTE *)sdata) + Bitmap->LineWidth);
                     Height--;
                  }
               }
               else if (Bitmap->BytesPerPixel IS 2) {
                  UWORD *ddata, *sdata;

                  sdata = (UWORD *)(Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<1));
                  ddata = (UWORD *)(dest->Data + (DestY * dest->LineWidth) + (DestX<<1));
                  colour = Bitmap->TransIndex;
                  while (Height > 0) {
                     for (i=0; i < Width; i++) if (sdata[i] != colour) ddata[i] = sdata[i];
                     ddata = (UWORD *)(((BYTE *)ddata) + dest->LineWidth);
                     sdata = (UWORD *)(((BYTE *)sdata) + Bitmap->LineWidth);
                     Height--;
                  }
               }
               else {
                  while (Height > 0) {
                     for (LONG i=0; i < Width; i++) {
                        colour = Bitmap->ReadUCPixel(Bitmap, X + i, Y);
                        if (colour != (ULONG)Bitmap->TransIndex) dest->DrawUCPixel(dest, DestX + i, DestY, colour);
                     }
                     Y++; DestY++;
                     Height--;
                  }
               }
            }
            else if (Bitmap->BitsPerPixel IS 8) {
               while (Height > 0) {
                  for (LONG i=0; i < Width; i++) {
                     colour = Bitmap->ReadUCPixel(Bitmap, X + i, Y);
                     if (colour != (ULONG)Bitmap->TransIndex) {
                        dest->DrawUCRPixel(dest, DestX + i, DestY, &Bitmap->Palette->Col[colour]);
                     }
                  }
                  Y++; DestY++;
                  Height--;
               }
            }
            else while (Height > 0) {
               for (LONG i=0; i < Width; i++) {
                  Bitmap->ReadUCRPixel(Bitmap, X + i, Y, &pixel);
                  if ((pixel.Red != Bitmap->TransRGB.Red) or (pixel.Green != Bitmap->TransRGB.Green) or (pixel.Blue != Bitmap->TransRGB.Blue)) {
                     dest->DrawUCRPixel(dest, DestX + i, DestY, &pixel);
                  }
               }
               Y++; DestY++;
               Height--;
            }

            unlock_surface(dest);
         }
         unlock_surface(Bitmap);
      }

      return ERR_Okay;
   }
   else { // Straight copy operation
      if (!lock_surface(Bitmap, SURFACE_READ)) {
         if (!lock_surface(dest, SURFACE_WRITE)) {
            if (Bitmap->Opacity < 255) { // Translucent draw
               srctable  = glAlphaLookup + (Bitmap->Opacity<<8);
               desttable = glAlphaLookup + ((255-Bitmap->Opacity)<<8);

               if ((Bitmap->BytesPerPixel IS 4) and (dest->BytesPerPixel IS 4)) {
                  ULONG *ddata, *sdata;
                  ULONG cmp_alpha;

                  sdata = (ULONG *)(Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<2));
                  ddata = (ULONG *)(dest->Data + (DestY * dest->LineWidth) + (DestX<<2));
                  cmp_alpha = 255 << Bitmap->prvColourFormat.AlphaPos;
                  while (Height > 0) {
                     for (i=0; i < Width; i++) {
                        ddata[i] = ((srctable[(UBYTE)(sdata[i]>>Bitmap->prvColourFormat.RedPos)]   + desttable[(UBYTE)(ddata[i]>>dest->prvColourFormat.RedPos)]) << dest->prvColourFormat.RedPos) |
                                   ((srctable[(UBYTE)(sdata[i]>>Bitmap->prvColourFormat.GreenPos)] + desttable[(UBYTE)(ddata[i]>>dest->prvColourFormat.GreenPos)]) << dest->prvColourFormat.GreenPos) |
                                   ((srctable[(UBYTE)(sdata[i]>>Bitmap->prvColourFormat.BluePos)]  + desttable[(UBYTE)(ddata[i]>>dest->prvColourFormat.BluePos)]) << dest->prvColourFormat.BluePos) |
                                   cmp_alpha;
                     }
                     ddata = (ULONG *)(((BYTE *)ddata) + dest->LineWidth);
                     sdata = (ULONG *)(((BYTE *)sdata) + Bitmap->LineWidth);
                     Height--;
                  }
               }
               else if ((Bitmap->BytesPerPixel IS 2) and (dest->BytesPerPixel IS 2)) {
                  UWORD *ddata, *sdata;

                  sdata = (UWORD *)(Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<1));
                  ddata = (UWORD *)(dest->Data + (DestY * dest->LineWidth) + (DestX<<1));
                  while (Height > 0) {
                     for (i=0; i < Width; i++) {
                        ddata[i] = PackPixel(dest, srctable[UnpackRed(Bitmap, sdata[i])]   + desttable[UnpackRed(dest, ddata[i])],
                                                   srctable[UnpackGreen(Bitmap, sdata[i])] + desttable[UnpackGreen(dest, ddata[i])],
                                                   srctable[UnpackBlue(Bitmap, sdata[i])]  + desttable[UnpackBlue(dest, ddata[i])]);
                     }
                     ddata = (UWORD *)(((BYTE *)ddata) + dest->LineWidth);
                     sdata = (UWORD *)(((BYTE *)sdata) + Bitmap->LineWidth);
                     Height--;
                  }
               }
               else while (Height > 0) {
                  for (i=0; i < Width; i++) {
                     Bitmap->ReadUCRPixel(Bitmap, X + i, Y, &src);
                     dest->ReadUCRPixel(dest, DestX + i, DestY, &pixel);

                     pixel.Red   = srctable[src.Red]   + desttable[pixel.Red];
                     pixel.Green = srctable[src.Green] + desttable[pixel.Green];
                     pixel.Blue  = srctable[src.Blue]  + desttable[pixel.Blue];

                     dest->DrawUCRPixel(dest, DestX + i, DestY, &pixel);
                  }
                  Y++; DestY++;
                  Height--;
               }
            }
            else if (Bitmap->BitsPerPixel IS dest->BitsPerPixel) {
               // Use this fast routine for identical bitmaps

               srcdata = Bitmap->Data + (X * Bitmap->BytesPerPixel) + (Y * Bitmap->LineWidth);
               data    = dest->Data + (DestX  * dest->BytesPerPixel) + (DestY * dest->LineWidth);
               Width   = Width * Bitmap->BytesPerPixel;

               if ((Bitmap IS dest) and (DestY >= Y) and (DestY < Y+Height)) {
                  // Copy backwards when we are copying within the same bitmap and there is an overlap.

                  srcdata += Bitmap->LineWidth * (Height-1);
                  data    += dest->LineWidth * (Height-1);

                  while (Height > 0) {
                     for (i=Width-1; i >= 0; i--) data[i] = srcdata[i];
                     srcdata -= Bitmap->LineWidth;
                     data    -= dest->LineWidth;
                     Height--;
                  }
               }
               else {
                  while (Height > 0) {
                     for (i=0; (size_t)i > sizeof(LONG); i += sizeof(LONG)) {
                        ((LONG *)(data+i))[0] = ((LONG *)(srcdata+i))[0];
                     }
                     while (i < Width) { data[i] = srcdata[i]; i++; }
                     srcdata += Bitmap->LineWidth;
                     data    += dest->LineWidth;
                     Height--;
                  }
               }
            }
            else {
               // If the bitmaps do not match then we need to use this slower RGB translation subroutine.

               bool dithered = FALSE;
               if (Flags & BAF_DITHER) {
                  if ((dest->BitsPerPixel < 24) and
                      ((Bitmap->BitsPerPixel > dest->BitsPerPixel) or
                       ((Bitmap->BitsPerPixel <= 8) and (dest->BitsPerPixel > 8)))) {
                     if (Bitmap->Flags & BMF_TRANSPARENT);
                     else {
                        dither(Bitmap, dest, NULL, Width, Height, X, Y, DestX, DestY);
                        dithered = TRUE;
                     }
                  }
               }

               if (dithered IS FALSE) {
                  if ((Bitmap IS dest) and (DestY >= Y) and (DestY < Y+Height)) {
                     while (Height > 0) {
                        Y += Height - 1;
                        DestY  += Height - 1;
                        for (i=0; i < Width; i++) {
                           Bitmap->ReadUCRPixel(Bitmap, X + i, Y, &pixel);
                           dest->DrawUCRPixel(dest, DestX + i, DestY, &pixel);
                        }
                        Y--; DestY--;
                        Height--;
                     }
                  }
                  else {
                     while (Height > 0) {
                        for (i=0; i < Width; i++) {
                           Bitmap->ReadUCRPixel(Bitmap, X + i, Y, &pixel);
                           dest->DrawUCRPixel(dest, DestX + i, DestY, &pixel);
                        }
                        Y++; DestY++;
                        Height--;
                     }
                  }
               }
            }

            unlock_surface(dest);
         }
         unlock_surface(Bitmap);
      }

      return ERR_Okay;
   }
}

/*****************************************************************************

-FUNCTION-
CopyRawBitmap: Copies graphics data from an arbitrary surface to a bitmap.

This function will copy data from a described surface to a destination bitmap object.  You are required to provide the
function with a full description of the source in a `BitmapSurface` structure.

The X, Y, Width and Height parameters define the area from the source that you wish to copy.  The XDest and
YDest parameters define the top left corner that you will blit the graphics to in the destination.

-INPUT-
struct(*BitmapSurface) Surface: Description of the surface source.
obj(Bitmap) Bitmap: Destination bitmap.
int(CSRF) Flags:  Optional flags.
int X:      Horizontal source coordinate.
int Y:      Vertical source coordinate.
int Width:  Source width.
int Height: Source height.
int XDest:  Horizontal destination coordinate.
int YDest:  Vertical destination coordinate.

-ERRORS-
Okay:
Args:
NullArgs:

*****************************************************************************/

#define UnpackSRed(a,b)   ((((b) >> (a)->Format.RedPos)   & (a)->Format.RedMask) << (a)->Format.RedShift)
#define UnpackSGreen(a,b) ((((b) >> (a)->Format.GreenPos) & (a)->Format.GreenMask) << (a)->Format.GreenShift)
#define UnpackSBlue(a,b)  ((((b) >> (a)->Format.BluePos)  & (a)->Format.BlueMask) << (a)->Format.BlueShift)
#define UnpackSAlpha(a,b) ((((b) >> (a)->Format.AlphaPos) & (a)->Format.AlphaMask))

static ULONG read_surface8(BITMAPSURFACE *Surface, WORD X, WORD Y)
{
   return ((UBYTE *)Surface->Data)[(Surface->LineWidth * Y) + X];
}

static ULONG read_surface16(BITMAPSURFACE *Surface, WORD X, WORD Y)
{
   return ((UWORD *)((BYTE *)Surface->Data + (Y * Surface->LineWidth) + X + X))[0];
}

static ULONG read_surface_lsb24(BITMAPSURFACE *Surface, WORD X, WORD Y)
{
   UBYTE *data;
   data = (UBYTE *)Surface->Data + (Surface->LineWidth * Y) + (X + X + X);
   return (data[2]<<16) | (data[1]<<8) | data[0];
}

static ULONG read_surface_msb24(BITMAPSURFACE *Surface, WORD X, WORD Y)
{
   UBYTE *data;
   data = (UBYTE *)Surface->Data + (Surface->LineWidth * Y) + (X + X + X);
   return (data[0]<<16) | (data[1]<<8) | data[2];
}

static ULONG read_surface32(BITMAPSURFACE *Surface, WORD X, WORD Y)
{
   return ((ULONG *)((UBYTE *)Surface->Data + (Surface->LineWidth * Y) + (X<<2)))[0];
}

ERROR gfxCopyRawBitmap(BITMAPSURFACE *Surface, objBitmap *Bitmap,
          LONG Flags, LONG X, LONG Y, LONG Width, LONG Height,
          LONG XDest, LONG YDest)
{
   parasol::Log log(__FUNCTION__);
   RGB8 pixel, src;
   UBYTE *srctable, *desttable;
   LONG i;
   WORD srcwidth;
   ULONG colour;
   UBYTE *data, *srcdata;
   ULONG (*read_surface)(BITMAPSURFACE *, WORD, WORD);

   if ((!Surface) or (!Bitmap)) return log.warning(ERR_NullArgs);

   if ((!Surface->Data) or (Surface->LineWidth < 1) or (!Surface->BitsPerPixel)) {
      return log.warning(ERR_Args);
   }

   srcwidth = Surface->LineWidth / Surface->BytesPerPixel;

   // Check if the destination that we are copying to is within the drawable area.

   if ((XDest < Bitmap->Clip.Left)) {
      Width = Width - (Bitmap->Clip.Left - X);
      if (Width < 1) return ERR_Okay;
      X = X + (Bitmap->Clip.Left - X);
      XDest = Bitmap->Clip.Left;
   }
   else if (XDest >= Bitmap->Clip.Right) return ERR_Okay;

   if ((YDest < Bitmap->Clip.Top)) {
      Height = Height - (Bitmap->Clip.Top - YDest);
      if (Height < 1) return ERR_Okay;
      Y = Y + (Bitmap->Clip.Top - YDest);
      YDest = Bitmap->Clip.Top;
   }
   else if (YDest >= Bitmap->Clip.Bottom) return ERR_Okay;

   // Check if the source that we are blitting from is within its own drawable area.

   if (Flags & CSRF_CLIP) {
      if (X < 0) {
         if ((Width += X) < 1) return ERR_Okay;
         X = 0;
      }
      else if (X >= srcwidth) return ERR_Okay;

      if (Y < 0) {
         if ((Height += Y) < 1) return ERR_Okay;
         Y = 0;
      }
      else if (Y >= Surface->Height) return ERR_Okay;
   }

   // Clip the width and height

   if ((XDest + Width)  >= Bitmap->Clip.Right)  Width  = Bitmap->Clip.Right - XDest;
   if ((YDest + Height) >= Bitmap->Clip.Bottom) Height = Bitmap->Clip.Bottom - YDest;

   if (Flags & CSRF_CLIP) {
      if ((X + Width)  >= Surface->Clip.Right)  Width  = Surface->Clip.Right - X;
      if ((Y + Height) >= Surface->Clip.Bottom) Height = Surface->Clip.Bottom - Y;
   }

   if (Width < 1) return ERR_Okay;
   if (Height < 1) return ERR_Okay;

   // Adjust coordinates by offset values

   if (Flags & CSRF_OFFSET) {
      X += Surface->XOffset;
      Y += Surface->YOffset;
   }

   XDest += Bitmap->XOffset;
   YDest += Bitmap->YOffset;

   if (Flags & CSRF_DEFAULT_FORMAT) gfxGetColourFormat(&Surface->Format, Surface->BitsPerPixel, 0, 0, 0, 0);;

   switch(Surface->BytesPerPixel) {
      case 1: read_surface = read_surface8; break;
      case 2: read_surface = read_surface16; break;
      case 3: if (Surface->Format.RedPos IS 16) read_surface = read_surface_lsb24;
              else read_surface = read_surface_msb24;
              break;
      case 4: read_surface = read_surface32; break;
      default: return log.warning(ERR_Args);
   }

#ifdef __xwindows__

   // Use this routine if the destination is a pixmap (write only memory).  X11 windows are always represented as pixmaps.

   if (Bitmap->x11.drawable) {
      // Source is an ximage, destination is a pixmap.  NB: If DGA is enabled, we will avoid using these routines because mem-copying from software
      // straight to video RAM is a lot faster.

      XImage ximage;
      WORD alignment;

      if (Bitmap->LineWidth & 0x0001) alignment = 8;
      else if (Bitmap->LineWidth & 0x0002) alignment = 16;
      else alignment = 32;

      ximage.width            = Surface->LineWidth / Surface->BytesPerPixel;  // Image width
      ximage.height           = Surface->Height; // Image height
      ximage.xoffset          = 0;               // Number of pixels offset in X direction
      ximage.format           = ZPixmap;         // XYBitmap, XYPixmap, ZPixmap
      ximage.data             = (char *)Surface->Data;   // Pointer to image data
      ximage.byte_order       = 0;               // LSBFirst / MSBFirst
      ximage.bitmap_unit      = alignment;       // Quant. of scanline - 8, 16, 32
      ximage.bitmap_bit_order = 0;               // LSBFirst / MSBFirst
      ximage.bitmap_pad       = alignment;       // 8, 16, 32, either XY or Zpixmap
      if (Surface->BitsPerPixel IS 32) ximage.depth = 24;
      else ximage.depth = Surface->BitsPerPixel;            // Actual bits per pixel
      ximage.bytes_per_line   = Surface->LineWidth;         // Accelerator to next line
      ximage.bits_per_pixel   = Surface->BytesPerPixel * 8; // Bits per pixel-group
      ximage.red_mask         = 0;
      ximage.green_mask       = 0;
      ximage.blue_mask        = 0;
      XInitImage(&ximage);

      XPutImage(XDisplay, Bitmap->x11.drawable, glXGC,
         &ximage, X, Y, XDest, YDest, Width, Height);

      return ERR_Okay;
   }

#endif // __xwindows__

   if (lock_surface(Bitmap, SURFACE_WRITE) IS ERR_Okay) {
      if ((Flags & CSRF_ALPHA) and (Surface->BitsPerPixel IS 32)) { // 32-bit alpha blending support
         ULONG *sdata = (ULONG *)((BYTE *)Surface->Data + (Y * Surface->LineWidth) + (X<<2));

         if (Bitmap->BitsPerPixel IS 32) {
            ULONG *ddata = (ULONG *)(Bitmap->Data + (YDest * Bitmap->LineWidth) + (XDest<<2));
            while (Height > 0) {
               for (LONG i=0; i < Width; i++) {
                  colour = sdata[i];

                  UBYTE alpha = ((UBYTE)(colour >> Surface->Format.AlphaPos));
                  alpha = (glAlphaLookup + (alpha<<8))[Surface->Opacity]; // Multiply the source pixel by overall translucency level

                  if (alpha >= BLEND_MAX_THRESHOLD) ddata[i] = colour;
                  else if (alpha >= BLEND_MIN_THRESHOLD) {

                     UBYTE red   = colour >> Surface->Format.RedPos;
                     UBYTE green = colour >> Surface->Format.GreenPos;
                     UBYTE blue  = colour >> Surface->Format.BluePos;

                     colour = ddata[i];
                     UBYTE destred   = colour >> Bitmap->prvColourFormat.RedPos;
                     UBYTE destgreen = colour >> Bitmap->prvColourFormat.GreenPos;
                     UBYTE destblue  = colour >> Bitmap->prvColourFormat.BluePos;

                     srctable  = glAlphaLookup + (alpha<<8);
                     desttable = glAlphaLookup + ((255-alpha)<<8);
                     ddata[i] = PackPixelWBA(Bitmap, srctable[red] + desttable[destred],
                                                  srctable[green] + desttable[destgreen],
                                                  srctable[blue] + desttable[destblue],
                                                  255);
                  }
               }
               sdata = (ULONG *)(((UBYTE *)sdata) + Surface->LineWidth);
               ddata = (ULONG *)(((UBYTE *)ddata) + Bitmap->LineWidth);
               Height--;
            }
         }
         else while (Height > 0) {
            for (LONG i=0; i < Width; i++) {
               colour = sdata[i];
               UBYTE alpha = ((UBYTE)(colour >> Surface->Format.AlphaPos));
               alpha = (glAlphaLookup + (alpha<<8))[Surface->Opacity]; // Multiply the source pixel by overall translucency level

               if (alpha >= BLEND_MAX_THRESHOLD) {
                  pixel.Red   = colour >> Surface->Format.RedPos;
                  pixel.Green = colour >> Surface->Format.GreenPos;
                  pixel.Blue  = colour >> Surface->Format.BluePos;
                  Bitmap->DrawUCRPixel(Bitmap, XDest + i, YDest, &pixel);
               }
               else if (alpha >= BLEND_MIN_THRESHOLD) {
                  UBYTE red   = colour >> Surface->Format.RedPos;
                  UBYTE green = colour >> Surface->Format.GreenPos;
                  UBYTE blue  = colour >> Surface->Format.BluePos;

                  srctable  = glAlphaLookup + (alpha<<8);
                  desttable = glAlphaLookup + ((255-alpha)<<8);

                  Bitmap->ReadUCRPixel(Bitmap, XDest + i, YDest, &pixel);
                  pixel.Red   = srctable[red]   + desttable[pixel.Red];
                  pixel.Green = srctable[green] + desttable[pixel.Green];
                  pixel.Blue  = srctable[blue]  + desttable[pixel.Blue];
                  Bitmap->DrawUCRPixel(Bitmap, XDest + i, YDest, &pixel);
               }
            }
            sdata = (ULONG *)(((UBYTE *)sdata) + Surface->LineWidth);
            YDest++;
            Height--;
         }
      }
      else if (Flags & CSRF_TRANSPARENT) {
         // Transparent colour blitting

         if ((Flags & CSRF_TRANSLUCENT) and (Surface->Opacity < 255)) {
            // Transparent mask with translucent pixels

            srctable  = glAlphaLookup + (Surface->Opacity<<8);
            desttable = glAlphaLookup + ((255-Surface->Opacity)<<8);

            while (Height > 0) {
               for (LONG i=0; i < Width; i++) {
                  colour = read_surface(Surface, X + i, Y);
                  if (colour != (ULONG)Surface->Colour) {
                     Bitmap->ReadUCRPixel(Bitmap, XDest + i, YDest, &pixel);

                     pixel.Red   = srctable[UnpackSRed(Surface, colour)]   + desttable[pixel.Red];
                     pixel.Green = srctable[UnpackSGreen(Surface, colour)] + desttable[pixel.Green];
                     pixel.Blue  = srctable[UnpackSBlue(Surface, colour)]  + desttable[pixel.Blue];

                     Bitmap->DrawUCRPixel(Bitmap, XDest + i, YDest, &pixel);
                  }
               }
               Y++; YDest++;
               Height--;
            }
         }
         else if (Surface->BitsPerPixel IS Bitmap->BitsPerPixel) {
            if (Surface->BytesPerPixel IS 4) {
               ULONG *sdata = (ULONG *)((BYTE *)Surface->Data + (Y * Surface->LineWidth) + (X<<2));
               ULONG *ddata = (ULONG *)(Bitmap->Data + (YDest * Bitmap->LineWidth) + (XDest<<2));
               colour = Surface->Colour;
               while (Height > 0) {
                  for (i=0; i < Width; i++) if (sdata[i] != colour) ddata[i] = sdata[i];
                  ddata = (ULONG *)(((BYTE *)ddata) + Bitmap->LineWidth);
                  sdata = (ULONG *)(((BYTE *)sdata) + Surface->LineWidth);
                  Height--;
               }
            }
            else if (Surface->BytesPerPixel IS 2) {
               UWORD *ddata, *sdata;

               sdata = (UWORD *)((BYTE *)Surface->Data + (Y * Surface->LineWidth) + (X<<1));
               ddata = (UWORD *)(Bitmap->Data + (YDest * Bitmap->LineWidth) + (XDest<<1));
               colour = Surface->Colour;
               while (Height > 0) {
                  for (i=0; i < Width; i++) if (sdata[i] != colour) ddata[i] = sdata[i];
                  ddata = (UWORD *)(((BYTE *)ddata) + Bitmap->LineWidth);
                  sdata = (UWORD *)(((BYTE *)sdata) + Surface->LineWidth);
                  Height--;
               }
            }
            else {
               while (Height > 0) {
                  for (i=0; i < Width; i++) {
                     colour = read_surface(Surface, X + i, Y);
                     if (colour != (ULONG)Surface->Colour) Bitmap->DrawUCPixel(Bitmap, XDest + i, YDest, colour);
                  }
                  Y++; YDest++;
                  Height--;
               }
            }
         }
         else {
            while (Height > 0) {
               for (i=0; i < Width; i++) {
                  colour = read_surface(Surface, X + i, Y);
                  if (colour != (ULONG)Surface->Colour) {
                     pixel.Red   = UnpackSRed(Surface, colour);
                     pixel.Green = UnpackSGreen(Surface, colour);
                     pixel.Blue  = UnpackSBlue(Surface, colour);
                     Bitmap->DrawUCRPixel(Bitmap, XDest + i, YDest, &pixel);
                  }
               }
               Y++; YDest++;
               Height--;
            }
         }
      }
      else { // Straight copy operation
         if ((Flags & CSRF_TRANSLUCENT) and (Surface->Opacity < 255)) { // Straight translucent blit
            srctable  = glAlphaLookup + (Surface->Opacity<<8);
            desttable = glAlphaLookup + ((255-Surface->Opacity)<<8);

            if ((Surface->BytesPerPixel IS 4) and (Bitmap->BytesPerPixel IS 4)) {
               ULONG *ddata, *sdata;

               sdata = (ULONG *)((BYTE *)Surface->Data + (Y * Surface->LineWidth) + (X<<2));
               ddata = (ULONG *)(Bitmap->Data + (YDest * Bitmap->LineWidth) + (XDest<<2));
               while (Height > 0) {
                  for (LONG i=0; i < Width; i++) {
                     ddata[i] = ((srctable[(UBYTE)(sdata[i]>>Surface->Format.RedPos)]   + desttable[(UBYTE)(ddata[i]>>Bitmap->prvColourFormat.RedPos)]) << Bitmap->prvColourFormat.RedPos) |
                                ((srctable[(UBYTE)(sdata[i]>>Surface->Format.GreenPos)] + desttable[(UBYTE)(ddata[i]>>Bitmap->prvColourFormat.GreenPos)]) << Bitmap->prvColourFormat.GreenPos) |
                                ((srctable[(UBYTE)(sdata[i]>>Surface->Format.BluePos)]  + desttable[(UBYTE)(ddata[i]>>Bitmap->prvColourFormat.BluePos)]) << Bitmap->prvColourFormat.BluePos);
                  }
                  ddata = (ULONG *)(((BYTE *)ddata) + Bitmap->LineWidth);
                  sdata = (ULONG *)(((BYTE *)sdata) + Surface->LineWidth);
                  Height--;
               }
            }
            else if ((Surface->BytesPerPixel IS 2) and (Bitmap->BytesPerPixel IS 2)) {
               UWORD *ddata, *sdata;

               sdata = (UWORD *)((BYTE *)Surface->Data + (Y * Surface->LineWidth) + (X<<1));
               ddata = (UWORD *)(Bitmap->Data + (YDest * Bitmap->LineWidth) + (XDest<<1));
               while (Height > 0) {
                  for (i=0; i < Width; i++) {
                     ddata[i] = PackPixel(Bitmap, srctable[UnpackSRed(Surface, sdata[i])] + desttable[UnpackRed(Bitmap, ddata[i])],
                                                  srctable[UnpackSGreen(Surface, sdata[i])] + desttable[UnpackGreen(Bitmap, ddata[i])],
                                                  srctable[UnpackSBlue(Surface, sdata[i])] + desttable[UnpackBlue(Bitmap, ddata[i])]);
                  }
                  ddata = (UWORD *)(((BYTE *)ddata) + Bitmap->LineWidth);
                  sdata = (UWORD *)(((BYTE *)sdata) + Surface->LineWidth);
                  Height--;
               }
            }
            else while (Height > 0) {
               for (LONG i=0; i < Width; i++) {
                  colour = read_surface(Surface, X + i, Y);
                  src.Red   = UnpackSRed(Surface, colour);
                  src.Green = UnpackSGreen(Surface, colour);
                  src.Blue  = UnpackSBlue(Surface, colour);

                  Bitmap->ReadUCRPixel(Bitmap, XDest + i, YDest, &pixel);

                  pixel.Red   = srctable[src.Red]   + desttable[pixel.Red];
                  pixel.Green = srctable[src.Green] + desttable[pixel.Green];
                  pixel.Blue  = srctable[src.Blue]  + desttable[pixel.Blue];

                  Bitmap->DrawUCRPixel(Bitmap, XDest + i, YDest, &pixel);
               }
               Y++; YDest++;
               Height--;
            }
         }
         else if (Surface->BitsPerPixel IS Bitmap->BitsPerPixel) {
            // Use this fast routine for identical bitmaps

            srcdata = (UBYTE *)Surface->Data + (X * Surface->BytesPerPixel) + (Y * Surface->LineWidth);
            data    = Bitmap->Data + (XDest  * Bitmap->BytesPerPixel) + (YDest * Bitmap->LineWidth);
            Width   = Width * Surface->BytesPerPixel;

            while (Height > 0) {
               for (i=0; (size_t)i > sizeof(LONG); i += sizeof(LONG)) {
                  ((LONG *)(data+i))[0] = ((LONG *)(srcdata+i))[0];
               }
               while (i < Width) { data[i] = srcdata[i]; i++; }
               srcdata += Surface->LineWidth;
               data    += Bitmap->LineWidth;
               Height--;
            }
         }
         else {
            // If the bitmaps do not match then we need to use this slower RGB translation subroutine.

            while (Height > 0) {
               for (LONG i=0; i < Width; i++) {
                  colour = read_surface(Surface, X + i, Y);
                  src.Red   = UnpackSRed(Surface, colour);
                  src.Green = UnpackSGreen(Surface, colour);
                  src.Blue  = UnpackSBlue(Surface, colour);
                  Bitmap->DrawUCRPixel(Bitmap, XDest + i, YDest, &src);
               }
               Y++; YDest++;
               Height--;
            }
         }
      }

      unlock_surface(Bitmap);
   }

   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
DrawLine: Draws a line to a bitmap.

This function will draw a line using a bitmap colour value.  The line will start from the position determined by
(X, Y) and end at (EndX, EndY) inclusive.  Hardware acceleration will be used to draw the line if available.

The opacity of the line is determined by the value in the Opacity field of the target bitmap.

-INPUT-
obj(Bitmap) Bitmap: The target bitmap.
int X: X-axis starting position.
int Y: Y-axis starting position.
int XEnd: X-axis end position.
int YEnd: Y-axis end position.
uint Colour: The pixel colour for drawing the line.

*****************************************************************************/

void gfxDrawLine(objBitmap *Bitmap, LONG X, LONG Y, LONG EndX, LONG EndY, ULONG Colour)
{
   RGB8 pixel, rgb;
   LONG i, dx, dy, l, m, x_inc, y_inc;
   LONG err_1, dx2, dy2;
   LONG drawx, drawy, clipleft, clipright, clipbottom, cliptop;
   ULONG colour;

   if (Bitmap->Opacity < 1) return;

   #ifdef __xwindows__
      if ((Bitmap->DataFlags & (MEM_VIDEO|MEM_TEXTURE)) and (Bitmap->Opacity >= 255)) {
         XRectangle rectangles;
         rectangles.x      = Bitmap->Clip.Left + Bitmap->XOffset;
         rectangles.y      = Bitmap->Clip.Top + Bitmap->YOffset;
         rectangles.width  = Bitmap->Clip.Right + Bitmap->XOffset - rectangles.x;
         rectangles.height = Bitmap->Clip.Bottom + Bitmap->YOffset - rectangles.y;
         XSetClipRectangles(XDisplay, glClipXGC, 0, 0, &rectangles, 1, YXSorted);

         XSetForeground(XDisplay, glClipXGC, Colour);
         XDrawLine(XDisplay, Bitmap->x11.drawable, glClipXGC, X + Bitmap->XOffset, Y + Bitmap->YOffset, EndX + Bitmap->XOffset, EndY + Bitmap->YOffset);
         return;
      }
   #endif

   rgb.Red   = UnpackRed(Bitmap, Colour);
   rgb.Green = UnpackGreen(Bitmap, Colour);
   rgb.Blue  = UnpackBlue(Bitmap, Colour);

   #ifdef _WIN32

      if ((Bitmap->prvAFlags & BF_WINVIDEO) and (Bitmap->Opacity >= 255)) {
         winSetClipping(Bitmap->win.Drawable, Bitmap->Clip.Left + Bitmap->XOffset, Bitmap->Clip.Top + Bitmap->YOffset,
            Bitmap->Clip.Right + Bitmap->XOffset, Bitmap->Clip.Bottom + Bitmap->YOffset);
         winDrawLine(Bitmap->win.Drawable, X + Bitmap->XOffset, Y + Bitmap->YOffset,
            EndX + Bitmap->XOffset, EndY + Bitmap->YOffset, &rgb.Red);
         winSetClipping(Bitmap->win.Drawable, 0, 0, 0, 0);

         return;
      }

   #endif

   if (lock_surface(Bitmap, SURFACE_READWRITE) != ERR_Okay) return;

   drawx = X + Bitmap->XOffset;
   drawy = Y + Bitmap->YOffset;
   dx    = ((EndX + Bitmap->XOffset) - (X + Bitmap->XOffset));
   dy    = ((EndY + Bitmap->YOffset) - (Y + Bitmap->YOffset));
   x_inc = (dx < 0) ? -1 : 1;
   if (dx < 0) l = -dx;
   else l = dx;
   y_inc = (dy < 0) ? -1 : 1;
   if (dy < 0) m = -dy;
   else m = dy;
   dx2   = l << 1;
   dy2   = m << 1;

   cliptop    = Bitmap->Clip.Top    + Bitmap->YOffset;
   clipbottom = Bitmap->Clip.Bottom + Bitmap->YOffset;
   clipleft   = Bitmap->Clip.Left   + Bitmap->XOffset;
   clipright  = Bitmap->Clip.Right  + Bitmap->XOffset;

   if (Bitmap->Opacity < 255) {
      // Translucent routine

      if ((l >= m)) {
         err_1 = dy2 - l;
         for (i = 0; i < l; i++) {
            if ((drawx >= clipleft) and (drawx < clipright) and (drawy >= cliptop) and (drawy < clipbottom))  {
               Bitmap->ReadUCRPixel(Bitmap, drawx, drawy, &pixel);
               pixel.Red   = rgb.Red   + (((pixel.Red   - rgb.Red)   * (255 - Bitmap->Opacity))>>8);
               pixel.Green = rgb.Green + (((pixel.Green - rgb.Green) * (255 - Bitmap->Opacity))>>8);
               pixel.Blue  = rgb.Blue  + (((pixel.Blue  - rgb.Blue)  * (255 - Bitmap->Opacity))>>8);
               pixel.Alpha = 255;
               Bitmap->DrawUCRPixel(Bitmap, drawx, drawy, &pixel);
            }
            if (err_1 > 0) { drawy += y_inc; err_1 -= dx2; }
            err_1 += dy2;
            drawx += x_inc;
         }
      }
      else {
         err_1 = dx2 - m;
         for (i = 0; i < m; i++) {
            if ((drawx >= clipleft) and (drawx < clipright) and (drawy >= cliptop) and (drawy < clipbottom))  {
               Bitmap->ReadUCRPixel(Bitmap, drawx, drawy, &pixel);
               pixel.Red   = rgb.Red   + (((pixel.Red   - rgb.Red)   * (255 - Bitmap->Opacity))>>8);
               pixel.Green = rgb.Green + (((pixel.Green - rgb.Green) * (255 - Bitmap->Opacity))>>8);
               pixel.Blue  = rgb.Blue  + (((pixel.Blue  - rgb.Blue)  * (255 - Bitmap->Opacity))>>8);
               pixel.Alpha = 255;
               Bitmap->DrawUCRPixel(Bitmap, drawx, drawy, &pixel);
            }
            if (err_1 > 0) { drawx += x_inc; err_1 -= dy2; }
            err_1 += dx2;
            drawy += y_inc;
         }
      }

      if ((drawx >= clipleft) and (drawx < clipright) and (drawy >= cliptop) and (drawy < clipbottom))  {
         Bitmap->ReadUCRPixel(Bitmap, drawx, drawy, &pixel);
         pixel.Red   = rgb.Red   + (((pixel.Red   - rgb.Red) * (255 - Bitmap->Opacity))>>8);
         pixel.Green = rgb.Green + (((pixel.Green - rgb.Green) * (255 - Bitmap->Opacity))>>8);
         pixel.Blue  = rgb.Blue  + (((pixel.Blue  - rgb.Blue) * (255 - Bitmap->Opacity))>>8);
         pixel.Alpha = 255;
         Bitmap->DrawUCRPixel(Bitmap, drawx, drawy, &pixel);
      }
   }
   else {
      colour = Colour;

      if (l >= m) {
         err_1 = dy2 - l;
         for (i = 0; i < l; i++) {
            if ((drawx >= clipleft) and (drawx < clipright) and (drawy >= cliptop) and (drawy < clipbottom))  {
               Bitmap->DrawUCPixel(Bitmap, drawx, drawy, colour);
            }
            if (err_1 > 0) { drawy += y_inc; err_1 -= dx2; }
            err_1 += dy2;
            drawx += x_inc;
         }
      }
      else {
         err_1 = dx2 - m;
         for (i = 0; i < m; i++) {
            if ((drawx >= clipleft) and (drawx < clipright) and (drawy >= cliptop) and (drawy < clipbottom))  {
               Bitmap->DrawUCPixel(Bitmap, drawx, drawy, colour);
            }
            if (err_1 > 0) { drawx += x_inc; err_1 -= dy2; }
            err_1 += dx2;
            drawy += y_inc;
         }
      }

      if ((drawx >= clipleft) and (drawx < clipright) and (drawy >= cliptop) and (drawy < clipbottom))  {
         Bitmap->DrawUCPixel(Bitmap, drawx, drawy, colour);
      }
   }

   unlock_surface(Bitmap);
}

/*****************************************************************************

-FUNCTION-
DrawRectangle: Draws rectangles, both filled and unfilled.

This function draws both filled and unfilled rectangles.  The rectangle is drawn to the target bitmap at position
(X, Y) with dimensions determined by the specified Width and Height.  If the Flags parameter defines `BAF_FILL` then
the rectangle will be filled, otherwise only the outline will be drawn.  The colour of the rectangle is determined by
the pixel value in the Colour argument.  Blending is not enabled unless the `BAF_BLEND` flag is defined and an alpha
value is present in the Colour.

-INPUT-
obj(Bitmap) Bitmap: Pointer to the target @Bitmap.
int X:       The left-most coordinate of the rectangle.
int Y:       The top-most coordinate of the rectangle.
int Width:   The width of the rectangle.
int Height:  The height of the rectangle.
uint Colour: The colour value to use for the rectangle.
int(BAF) Flags: Use BAF_FILL to fill the rectangle.  Use of BAF_BLEND will enable blending.

*****************************************************************************/

void gfxDrawRectangle(objBitmap *Bitmap, LONG X, LONG Y, LONG Width, LONG Height, ULONG Colour, LONG Flags)
{
   parasol::Log log(__FUNCTION__);
   RGB8 pixel;
   UBYTE *data;
   UWORD *word;
   ULONG *longdata;
   LONG xend, x, EX, EY, i;

   if (!Bitmap) return;

   // If we are not going to fill the rectangle, use this routine to draw an outline.

   if ((!(Flags & BAF_FILL)) and (Width > 1) and (Height > 1)) {
      EX = X + Width - 1;
      EY = Y + Height - 1;
      if (X >= Bitmap->Clip.Left) gfxDrawRectangle(Bitmap, X, Y, 1, Height, Colour, Flags|BAF_FILL); // Left
      if (Y >= Bitmap->Clip.Top)  gfxDrawRectangle(Bitmap, X, Y, Width, 1, Colour, Flags|BAF_FILL); // Top
      if (Y + Height <= Bitmap->Clip.Bottom) gfxDrawRectangle(Bitmap, X, EY, Width, 1, Colour, Flags|BAF_FILL); // Bottom
      if (X + Width <= Bitmap->Clip.Right)   gfxDrawRectangle(Bitmap, X+Width-1, Y, 1, Height, Colour, Flags|BAF_FILL);
      return;
   }

   if (!(Bitmap->Head.Flags & NF_INITIALISED)) { log.warning(ERR_NotInitialised); return; }

   X += Bitmap->XOffset;
   Y += Bitmap->YOffset;

   if (X >= Bitmap->Clip.Right + Bitmap->XOffset) return;
   if (Y >= Bitmap->Clip.Bottom + Bitmap->YOffset) return;
   if (X + Width <= Bitmap->Clip.Left + Bitmap->XOffset) return;
   if (Y + Height <= Bitmap->Clip.Top + Bitmap->YOffset) return;

   if (X < Bitmap->Clip.Left + Bitmap->XOffset) {
      Width -= Bitmap->Clip.Left + Bitmap->XOffset - X;
      X = Bitmap->Clip.Left + Bitmap->XOffset;
   }

   if (Y < Bitmap->Clip.Top + Bitmap->YOffset) {
      Height -= Bitmap->Clip.Top + Bitmap->YOffset - Y;
      Y = Bitmap->Clip.Top + Bitmap->YOffset;
   }

   if ((X + Width) >= Bitmap->Clip.Right + Bitmap->XOffset)   Width = Bitmap->Clip.Right + Bitmap->XOffset - X;
   if ((Y + Height) >= Bitmap->Clip.Bottom + Bitmap->YOffset) Height = Bitmap->Clip.Bottom + Bitmap->YOffset - Y;

   UWORD red   = UnpackRed(Bitmap, Colour);
   UWORD green = UnpackGreen(Bitmap, Colour);
   UWORD blue  = UnpackBlue(Bitmap, Colour);

   // Translucent rectangle support

   UBYTE opacity = 255;
   if (Flags & BAF_BLEND) {
      opacity = UnpackAlpha(Bitmap, Colour);
   }
   else opacity = Bitmap->Opacity; // Pulling the opacity from the bitmap is deprecated, used BAF_BLEND instead.

   if (opacity < 255) {
      if (!lock_surface(Bitmap, SURFACE_READWRITE)) {
         UWORD wordpixel;

         if (Bitmap->BitsPerPixel IS 32) {
            ULONG cmb_alpha;
            longdata = (ULONG *)(Bitmap->Data + (Bitmap->LineWidth * Y));
            xend = X + Width;
            cmb_alpha = 255 << Bitmap->prvColourFormat.AlphaPos;
            while (Height > 0) {
               i = X;
               while (i < xend) {
                  UBYTE sr = longdata[i]>>Bitmap->prvColourFormat.RedPos;
                  UBYTE sg = longdata[i]>>Bitmap->prvColourFormat.GreenPos;
                  UBYTE sb = longdata[i]>>Bitmap->prvColourFormat.BluePos;

                  longdata[i] = (((((red   - sr)*opacity)>>8)+sr) << Bitmap->prvColourFormat.RedPos) |
                                (((((green - sg)*opacity)>>8)+sg) << Bitmap->prvColourFormat.GreenPos) |
                                (((((blue  - sb)*opacity)>>8)+sb) << Bitmap->prvColourFormat.BluePos) |
                                cmb_alpha;
                  i++;
               }
               longdata = (ULONG *)(((BYTE *)longdata) + Bitmap->LineWidth);
               Height--;
            }
         }
         else if (Bitmap->BitsPerPixel IS 24) {
            data = Bitmap->Data + (Bitmap->LineWidth * Y);
            X    = X * Bitmap->BytesPerPixel;
            xend = X + (Width * Bitmap->BytesPerPixel);
            while (Height > 0) {
               i = X;
               while (i < xend) {
                  data[i] = (((blue - data[i])*opacity)>>8)+data[i]; i++;
                  data[i] = (((green - data[i])*opacity)>>8)+data[i]; i++;
                  data[i] = (((red - data[i])*opacity)>>8)+data[i]; i++;
               }
               data += Bitmap->LineWidth;
               Height--;
            }
         }
         else if (Bitmap->BitsPerPixel IS 16) {
            word = (UWORD *)(Bitmap->Data + (Bitmap->LineWidth * Y));
            xend = X + Width;
            while (Height > 0) {
               i = X;
               while (i < xend) {
                  UBYTE sr = (word[i] & 0x001f)<<3;
                  UBYTE sg = (word[i] & 0x07e0)>>3;
                  UBYTE sb = (word[i] & 0xf800)>>8;
                  sr = (((red   - sr)*opacity)>>8) + sr;
                  sg = (((green - sg)*opacity)>>8) + sg;
                  sb = (((blue  - sb)*opacity)>>8) + sb;
                  wordpixel =  (sb>>3) & 0x001f;
                  wordpixel |= (sg<<3) & 0x07e0;
                  wordpixel |= (sr<<8) & 0xf800;
                  word[i] = wordpixel;
                  i++;
               }
               word = (UWORD *)(((UBYTE *)word) + Bitmap->LineWidth);
               Height--;
            }
         }
         else if (Bitmap->BitsPerPixel IS 15) {
            word = (UWORD *)(Bitmap->Data + (Bitmap->LineWidth * Y));
            xend = X + Width;
            while (Height > 0) {
               i = X;
               while (i < xend) {
                  UBYTE sr = (word[i] & 0x001f)<<3;
                  UBYTE sg = (word[i] & 0x03e0)>>2;
                  UBYTE sb = (word[i] & 0x7c00)>>7;
                  sr = (((red   - sr)*opacity)>>8) + sr;
                  sg = (((green - sg)*opacity)>>8) + sg;
                  sb = (((blue  - sb)*opacity)>>8) + sb;
                  wordpixel =  (sb>>3) & 0x001f;
                  wordpixel |= (sg<<2) & 0x03e0;
                  wordpixel |= (sr<<7) & 0x7c00;
                  word[i] = wordpixel;
                  i++;
               }
               word = (UWORD *)(((UBYTE *)word) + Bitmap->LineWidth);
               Height--;
            }
         }
         else {
            while (Height > 0) {
               for (i=X; i < X + Width; i++) {
                  Bitmap->ReadUCRPixel(Bitmap, i, Y, &pixel);
                  pixel.Red   = (((red - pixel.Red)*opacity)>>8) + pixel.Red;
                  pixel.Green = (((green - pixel.Green)*opacity)>>8) + pixel.Green;
                  pixel.Blue  = (((blue - pixel.Blue)*opacity)>>8) + pixel.Blue;
                  pixel.Alpha = 255;
                  Bitmap->DrawUCRPixel(Bitmap, i, Y, &pixel);
               }
               Y++;
               Height--;
            }
         }

         unlock_surface(Bitmap);
      }

      return;
   }

   // Standard rectangle (no translucency) video support

   #ifdef _GLES_
      if (Bitmap->DataFlags & MEM_VIDEO) {
      log.warning("TODO: Draw rectangles to opengl");
         glClearColor(0.5, 0.5, 0.5, 1.0);
         glClear(GL_COLOR_BUFFER_BIT);
         return;
      }
   #endif

   #ifdef _WIN32
      if (Bitmap->win.Drawable) {
         winDrawRectangle(Bitmap->win.Drawable, X, Y, Width, Height, red, green, blue);
         return;
      }
   #endif

   #ifdef __xwindows__
      if (Bitmap->DataFlags & (MEM_VIDEO|MEM_TEXTURE)) {
         XSetForeground(XDisplay, glXGC, Colour);
         XFillRectangle(XDisplay, Bitmap->x11.drawable, glXGC, X, Y, Width, Height);
         return;
      }
   #endif

   // Standard rectangle data support

   if (!lock_surface(Bitmap, SURFACE_WRITE)) {
      if (!Bitmap->Data) {
         unlock_surface(Bitmap);
         return;
      }

      if (Bitmap->Type IS BMP_CHUNKY) {
         if (Bitmap->BitsPerPixel IS 32) {
            longdata = (ULONG *)(Bitmap->Data + (Bitmap->LineWidth * Y));
            while (Height > 0) {
               for (x=X; x < (X+Width); x++) longdata[x] = Colour;
               longdata = (ULONG *)(((UBYTE *)longdata) + Bitmap->LineWidth);
               Height--;
            }
         }
         else if (Bitmap->BitsPerPixel IS 24) {
            data = Bitmap->Data + (Bitmap->LineWidth * Y);
            X = X + X + X;
            xend = X + Width + Width + Width;
            while (Height > 0) {
               for (x=X; x < xend;) {
                  data[x++] = blue; data[x++] = green; data[x++] = red;
               }
               data += Bitmap->LineWidth;
               Height--;
            }
         }
         else if ((Bitmap->BitsPerPixel IS 16) or (Bitmap->BitsPerPixel IS 15)) {
            word = (UWORD *)(Bitmap->Data + (Bitmap->LineWidth * Y));
            xend = X + Width;
            while (Height > 0) {
               for (x=X; x < xend; x++) word[x] = (UWORD)Colour;
               word = (UWORD *)(((BYTE *)word) + Bitmap->LineWidth);
               Height--;
            }
         }
         else if (Bitmap->BitsPerPixel IS 8) {
            data = Bitmap->Data + (Bitmap->LineWidth * Y);
            xend = X + Width;
            while (Height > 0) {
               for (x=X; x < xend;) data[x++] = Colour;
               data += Bitmap->LineWidth;
               Height--;
            }
         }
         else while (Height > 0) {
            for (i=X; i < X + Width; i++) Bitmap->DrawUCPixel(Bitmap, i, Y, Colour);
            Y++;
            Height--;
         }
      }
      else while (Height > 0) {
         for (i=X; i < X + Width; i++) Bitmap->DrawUCPixel(Bitmap, i, Y, Colour);
         Y++;
         Height--;
      }

      unlock_surface(Bitmap);
   }

   return;
}

/*****************************************************************************

-FUNCTION-
DrawRGBPixel: Draws a 24 bit pixel to a bitmap.

This function draws an RGB colour to the (X, Y) position of a target bitmap.  The function will check the given
coordinates to ensure that the pixel is inside the bitmap's clipping area.

-INPUT-
obj(Bitmap) Bitmap: The target bitmap object.
int X: Horizontal coordinate of the pixel.
int Y: Vertical coordinate of the pixel.
struct(*RGB8) RGB: The colour to be drawn, in RGB format.

*****************************************************************************/

void gfxDrawRGBPixel(objBitmap *Bitmap, LONG X, LONG Y, RGB8 *Pixel)
{
   if ((X >= Bitmap->Clip.Right) or (X < Bitmap->Clip.Left)) return;
   if ((Y >= Bitmap->Clip.Bottom) or (Y < Bitmap->Clip.Top)) return;
   Bitmap->DrawUCRPixel(Bitmap, X + Bitmap->XOffset, Y + Bitmap->YOffset, Pixel);
}

/*****************************************************************************

-FUNCTION-
DrawPixel: Draws a single pixel to a bitmap.

This function draws a pixel to the coordinates X, Y on a bitmap with a colour determined by the Colour index.
This function will check the given coordinates to make sure that the pixel is inside the bitmap's clipping area.

-INPUT-
obj(Bitmap) Bitmap: The target bitmap object.
int X: The horizontal coordinate of the pixel.
int Y: The vertical coordinate of the pixel.
uint Colour: The colour value to use for the pixel.

*****************************************************************************/

void gfxDrawPixel(objBitmap *Bitmap, LONG X, LONG Y, ULONG Colour)
{
   if ((X >= Bitmap->Clip.Right) or (X < Bitmap->Clip.Left)) return;
   if ((Y >= Bitmap->Clip.Bottom) or (Y < Bitmap->Clip.Top)) return;
   Bitmap->DrawUCPixel(Bitmap, X + Bitmap->XOffset, Y + Bitmap->YOffset, Colour);
}

/*****************************************************************************

-FUNCTION-
FlipBitmap: Flips a bitmap around its horizontal or vertical axis.

The FlipBitmap() function is used to flip bitmap images on their horizontal or vertical axis.  The amount of time
required to flip a bitmap is dependent on the area of the bitmap you are trying to flip over and its total number of
colours.

-INPUT-
obj(Bitmap) Bitmap: Pointer to a bitmap object.
int(FLIP) Orientation: Set to either FLIP_HORIZONTAL or FLIP_VERTICAL.  If set to neither, the function does nothing.

*****************************************************************************/

void gfxFlipBitmap(objBitmap *Bitmap, LONG Orientation)
{
   ActionTags(MT_BmpFlip, Bitmap, Orientation);
}

/*****************************************************************************

-FUNCTION-
GetColourFormat: Generates the values for a ColourFormat structure for a given bit depth.

This function will generate the values for a `ColourFormat` structure, for either a given bit depth or
customised colour bit values.  The `ColourFormat` structure is used by internal bitmap routines to pack and unpack bit
values to and from bitmap memory.

<pre>
struct ColourFormat {
   UBYTE  RedShift;    // Right shift value (applies only to 15/16 bit formats for eliminating redundant bits)
   UBYTE  BlueShift;
   UBYTE  GreenShift;
   UBYTE  AlphaShift;
   UBYTE  RedMask;     // The unshifted mask value (ranges from 0x00 to 0xff)
   UBYTE  GreenMask;
   UBYTE  BlueMask;
   UBYTE  AlphaMask;
   UBYTE  RedPos;      // Left shift/positional value
   UBYTE  GreenPos;
   UBYTE  BluePos;
   UBYTE  AlphaPos;
};
</pre>

The `ColourFormat` structure is supported by the following macros for packing and unpacking colour bit values:

<pre>
Colour = CFPackPixel(Format,Red,Green,Blue)
Colour = CFPackPixelA(Format,Red,Green,Blue,Alpha)
Colour = CFPackAlpha(Format,Alpha)
Red    = CFUnpackRed(Format,Colour)
Green  = CFUnpackGreen(Format,Colour)
Blue   = CFUnpackBlue(Format,Colour)
Alpha  = CFUnpackAlpha(Format,Colour)
</pre>

-INPUT-
struct(*ColourFormat) Format: Pointer to an empty ColourFormat structure.
int BitsPerPixel: The depth that you would like to generate colour values for.  Ignored if mask values are set.
int RedMask:      Red component bit mask value.  Set this value to zero if the BitsPerPixel argument is used.
int GreenMask:    Green component bit mask value.
int BlueMask:     Blue component bit mask value.
int AlphaMask:    Alpha component bit mask value.

*****************************************************************************/

void gfxGetColourFormat(ColourFormat *Format, LONG BPP, LONG RedMask, LONG GreenMask, LONG BlueMask, LONG AlphaMask)
{
   LONG mask;

   //log.function("R: $%.8x G: $%.8x, B: $%.8x, A: $%.8x", RedMask, GreenMask, BlueMask, AlphaMask);

   if (!RedMask) {
      if (BPP IS 15) {
         RedMask   = 0x7c00;
         GreenMask = 0x03e0;
         BlueMask  = 0x001f;
         AlphaMask = 0x0000;
      }
      else if (BPP IS 16) {
         RedMask   = 0xf800;
         GreenMask = 0x07e0;
         BlueMask  = 0x001f;
         AlphaMask = 0x0000;
      }
      else {
         BPP = 32;
         AlphaMask = 0xff000000;
         RedMask   = 0x00ff0000;
         GreenMask = 0x0000ff00;
         BlueMask  = 0x000000ff;
      }
   }

   // Calculate the lower byte mask and the position (left shift) of the colour

   mask = RedMask;
   Format->RedPos = 0;
   Format->RedShift = 0;
   while ((mask) and (!(mask & 1))) { mask = mask>>1; Format->RedPos++; }
   Format->RedMask = mask;
   for (mask=0x80; (mask) and (!(mask & Format->RedMask)); mask=mask>>1) Format->RedShift++;

   mask = BlueMask;
   Format->BluePos = 0;
   Format->BlueShift = 0;
   while ((mask) and (!(mask & 1))) { mask = mask>>1; Format->BluePos++; }
   Format->BlueMask = mask;
   for (mask=0x80; (mask) and (!(mask & Format->BlueMask)); mask=mask>>1) Format->BlueShift++;

   mask = GreenMask;
   Format->GreenPos = 0;
   Format->GreenShift = 0;
   while ((mask) and (!(mask & 1))) { mask = mask>>1; Format->GreenPos++; }
   Format->GreenMask = mask;
   for (mask=0x80; (mask) and (!(mask & Format->GreenMask)); mask=mask>>1) Format->GreenShift++;

   mask = AlphaMask;
   Format->AlphaPos = 0;
   Format->AlphaShift = 0;
   while ((mask) and (!(mask & 1))) { mask = mask>>1; Format->AlphaPos++; }
   Format->AlphaMask = mask;
   for (mask=0x80; (mask) and (!(mask & Format->AlphaMask)); mask=mask>>1) Format->AlphaShift++;

   Format->BitsPerPixel = BPP;
}

/*****************************************************************************

-FUNCTION-
ReadRGBPixel: Reads a pixel's colour from the target bitmap.

This function reads a pixel from a bitmap surface and returns the value in an RGB structure that remains good up until
the next call to this function.  Zero is returned in the alpha component if the pixel is out of bounds.

This function is thread-safe if the target Bitmap is locked.

-INPUT-
obj(Bitmap) Bitmap: Pointer to a bitmap object.
int X: The horizontal coordinate of the pixel.
int Y: The vertical coordinate of the pixel.
&struct(RGB8) RGB: The colour values will be stored in this RGB structure.

*****************************************************************************/

void gfxReadRGBPixel(objBitmap *Bitmap, LONG X, LONG Y, RGB8 **Pixel)
{
   static THREADVAR RGB8 pixel;
   if ((X >= Bitmap->Clip.Right) or (X < Bitmap->Clip.Left) or
       (Y >= Bitmap->Clip.Bottom) or (Y < Bitmap->Clip.Top)) {
      pixel.Red = 0; pixel.Green = 0; pixel.Blue = 0; pixel.Alpha = 0;
   }
   else {
      pixel.Alpha = 255;
      Bitmap->ReadUCRPixel(Bitmap, X + Bitmap->XOffset, Y + Bitmap->YOffset, &pixel);
   }
   *Pixel = &pixel;
}

/*****************************************************************************

-FUNCTION-
ReadPixel: Reads a pixel's colour from the target bitmap.

This function reads a pixel from a bitmap area and returns its colour index (if the Bitmap is indexed with a palette)
or its packed pixel value.  Zero is returned if the pixel is out of bounds.

-INPUT-
obj(Bitmap) Bitmap: Pointer to a bitmap object.
int X: The horizontal coordinate of the pixel.
int Y: The vertical coordinate of the pixel.

-RESULT-
uint: The colour value of the pixel will be returned.  Zero is returned if the pixel is out of bounds.

*****************************************************************************/

ULONG gfxReadPixel(objBitmap *Bitmap, LONG X, LONG Y)
{
   if ((X >= Bitmap->Clip.Right) or (X < Bitmap->Clip.Left) or
       (Y >= Bitmap->Clip.Bottom) or (Y < Bitmap->Clip.Top)) return 0;
   else return Bitmap->ReadUCPixel(Bitmap, X, Y);
}

/*****************************************************************************

-FUNCTION-
Resample: Resamples a bitmap by dithering it to a new set of colour masks.

The Resample() function provides a means for resampling a bitmap to a new colour format without changing the actual
bit depth of the image. It uses dithering so as to retain the quality of the image when down-sampling.  This function
is generally used to 'pre-dither' true colour bitmaps in preparation for copying to bitmaps with lower colour quality.

You are required to supply a ColourFormat structure that describes the colour format that you would like to apply to
the bitmap's image data.

-INPUT-
obj(Bitmap) Bitmap: The bitmap object to be resampled.
struct(*ColourFormat) ColourFormat: The new colour format to be applied to the bitmap.

-ERRORS-
Okay
NullArgs

*****************************************************************************/

ERROR gfxResample(objBitmap *Bitmap, ColourFormat *Format)
{
   if ((!Bitmap) or (!Format)) return ERR_NullArgs;

   dither(Bitmap, Bitmap, Format, Bitmap->Width, Bitmap->Height, 0, 0, 0, 0);
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
SetClipRegion: Sets a clipping region for a bitmap object.

The SetClipRegion() method is used to manage the clipping regions assigned to a bitmap object.  Each new bitmap that is
created has at least one clip region assigned to it, but by using SetClipRegion() you can also define multiple clipping
areas, which is useful for complex graphics management.

Each clipping region that you set is assigned a Number, starting from zero which is the default.  Each time that you
set a new clip region you must specify the number of the region that you wish to set.  If you attempt to 'skip'
regions - for instance, if you set regions 0, 1, 2 and 3, then skip 4 and set 5, the routine will set region 4 instead.
If you have specified multiple clip regions and want to lower the count or reset the list, set the number of the last
region that you want in your list and set the Terminate argument to TRUE to kill the regions specified beyond it.

The `ClipLeft`, `ClipTop`, `ClipRight` and `ClipBottom` fields in the target Bitmap will be updated to reflect the overall
area that is covered by the clipping regions that have been set.

-INPUT-
obj(Bitmap) Bitmap: The target bitmap.
int Number:    The number of the clip region to set.
int Left:      The horizontal start of the clip region.
int Top:       The vertical start of the clip region.
int Right:     The right-most edge of the clip region.
int Bottom:    The bottom-most edge of the clip region.
int Terminate: Set to TRUE if this is the last clip region in the list, otherwise FALSE.

*****************************************************************************/

void gfxSetClipRegion(objBitmap *Bitmap, LONG Number, LONG Left, LONG Top, LONG Right, LONG Bottom,
   LONG Terminate)
{
   Bitmap->Clip.Left   = Left;
   Bitmap->Clip.Top    = Top;
   Bitmap->Clip.Right  = Right;
   Bitmap->Clip.Bottom = Bottom;

   if (Bitmap->Clip.Left < 0) Bitmap->Clip.Left = 0;
   if (Bitmap->Clip.Top  < 0) Bitmap->Clip.Top = 0;
   if (Bitmap->Clip.Right  > Bitmap->Width)  Bitmap->Clip.Right = Bitmap->Width;
   if (Bitmap->Clip.Bottom > Bitmap->Height) Bitmap->Clip.Bottom = Bitmap->Height;
}

/*****************************************************************************

-FUNCTION-
Sync: Waits for the completion of all active bitmap operations.

The Sync() function will wait for all current video operations to complete before it returns.  This ensures that it is
safe to write to video memory with the CPU, preventing any possibility of clashes with the onboard graphics chip.

-INPUT-
obj(Bitmap) Bitmap: Pointer to the bitmap that you want to synchronise or NULL to sleep on the graphics accelerator.
-END-

*****************************************************************************/

void gfxSync(objBitmap *Bitmap)
{

}
