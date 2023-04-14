/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
Bitmap: Manages bitmap graphics and provides drawing functionality.

The Bitmap class provides a way of describing an area of memory that an application can draw to, and/or display if the
data is held in video memory.  Bitmaps are used in the handling of @Display and @Picture objects, and form the backbone
of Parasol's graphics functionality.  The Bitmap class supports everything from basic graphics primitives to masking and
alpha blending features.

To create a new bitmap object, you need to specify its #Width and #Height at a minimum.  Preferably, you should also
know how many colours you want to use and whether the bitmap data should be held in standard memory (for CPU based
reading and writing) or video memory (for hardware based drawing).  After creating a bitmap you can use a number of
available drawing methods for the purpose of image management.  Please note that these methods are designed to be
called under exclusive conditions, and it is not recommended that you call methods on a bitmap using the message
system.

By default, the CPU can only be used to read and write data directly to or from a bitmap when it is held in standard
memory (this is the default type).  If the `BLIT` or `VIDEO` flags are specified in the #DataFlags field then the
CPU cannot access this memory, unless you specifically request it.  To do this, use the #Lock() and #Unlock() actions
to temporarily gain read/write access to a bitmap.

If you require complex drawing functionality that is not available in the Bitmap class, please use the functionality
provided by the Vector module.

To save the image of a bitmap, either copy its image to a @Picture object, or use the SaveImage
action to save the data in PNG format.  Raw data can also be processed through a bitmap by using the Read and Write
actions.
-END-

*********************************************************************************************************************/

#include "defs.h"

#ifdef _WIN32
#define DLLCALL // __declspec(dllimport)
#define WINAPI  __stdcall

DLLCALL LONG WINAPI SetPixelV(APTR, LONG, LONG, LONG);
DLLCALL LONG WINAPI SetPixel(APTR, LONG, LONG, LONG);
DLLCALL LONG WINAPI GetPixel(APTR, LONG, LONG);
#endif

static LONG CalculatePixelRoutines(extBitmap *);

//********************************************************************************************************************
// Pixel and pen based functions.

// Video Pixel Routines

#ifdef _WIN32

static void  VideoDrawPixel(objBitmap *, LONG, LONG, ULONG);
static void  VideoDrawRGBPixel(objBitmap *, LONG, LONG, RGB8 *);
static void  VideoDrawRGBIndex(objBitmap *, UBYTE *, RGB8 *);
static ULONG VideoReadPixel(objBitmap *, LONG, LONG);
static void  VideoReadRGBPixel(objBitmap *, LONG, LONG, RGB8 *);
static void  VideoReadRGBIndex(objBitmap *, UBYTE *, RGB8 *);

#else

static void VideoDrawPixel32(objBitmap *, LONG, LONG, ULONG);
static void VideoDrawPixel24(objBitmap *, LONG, LONG, ULONG);
static void VideoDrawPixel16(objBitmap *, LONG, LONG, ULONG);
static void VideoDrawPixel8(objBitmap *,  LONG, LONG, ULONG);

static void VideoDrawRGBPixel32(objBitmap *, LONG, LONG, RGB8 *);
static void VideoDrawRGBPixel24(objBitmap *, LONG, LONG, RGB8 *);
static void VideoDrawRGBPixel16(objBitmap *, LONG, LONG, RGB8 *);
static void VideoDrawRGBPixel8(objBitmap *,  LONG, LONG, RGB8 *);

static void VideoDrawRGBIndex32(objBitmap *, ULONG *, RGB8 *);
static void VideoDrawRGBIndex24(objBitmap *, UBYTE *, RGB8 *);
static void VideoDrawRGBIndex16(objBitmap *, UWORD *, RGB8 *);
static void VideoDrawRGBIndex8(objBitmap *,  UBYTE *, RGB8 *);

static ULONG VideoReadPixel32(objBitmap *, LONG, LONG);
static ULONG VideoReadPixel24(objBitmap *, LONG, LONG);
static ULONG VideoReadPixel16(objBitmap *, LONG, LONG);
static ULONG VideoReadPixel8(objBitmap *,  LONG, LONG);

static void VideoReadRGBPixel32(objBitmap *, LONG, LONG, RGB8 *);
static void VideoReadRGBPixel24(objBitmap *, LONG, LONG, RGB8 *);
static void VideoReadRGBPixel16(objBitmap *, LONG, LONG, RGB8 *);
static void VideoReadRGBPixel8(objBitmap *,  LONG, LONG, RGB8 *);

static void VideoReadRGBIndex32(objBitmap *, ULONG *, RGB8 *);
static void VideoReadRGBIndex24(objBitmap *, UBYTE *, RGB8 *);
static void VideoReadRGBIndex16(objBitmap *, UWORD *, RGB8 *);
static void VideoReadRGBIndex8(objBitmap *,  UBYTE *, RGB8 *);

#endif

// Memory Pixel Routines

static void MemDrawPixel32(objBitmap *, LONG, LONG, ULONG);
static void MemDrawLSBPixel24(objBitmap *, LONG, LONG, ULONG);
static void MemDrawMSBPixel24(objBitmap *, LONG, LONG, ULONG);
static void MemDrawPixel16(objBitmap *, LONG, LONG, ULONG);
static void MemDrawPixel8(objBitmap *, LONG, LONG, ULONG);

static ULONG MemReadPixel32(objBitmap *, LONG, LONG);
static ULONG MemReadLSBPixel24(objBitmap *, LONG, LONG);
static ULONG MemReadMSBPixel24(objBitmap *, LONG, LONG);
static ULONG MemReadPixel16(objBitmap *, LONG, LONG);
static ULONG MemReadPixel8(objBitmap *, LONG, LONG);

static void MemDrawRGBPixel32(objBitmap *, LONG, LONG, RGB8 *);
static void MemDrawLSBRGBPixel24(objBitmap *, LONG, LONG, RGB8 *);
static void MemDrawMSBRGBPixel24(objBitmap *, LONG, LONG, RGB8 *);
static void MemDrawRGBPixel16(objBitmap *, LONG, LONG, RGB8 *);
static void MemDrawRGBPixel8(objBitmap *, LONG, LONG, RGB8 *);

static void MemDrawRGBIndex32(objBitmap *, ULONG *, RGB8 *);
static void MemDrawLSBRGBIndex24(objBitmap *, UBYTE *, RGB8 *);
static void MemDrawMSBRGBIndex24(objBitmap *, UBYTE *, RGB8 *);
static void MemDrawRGBIndex16(objBitmap *, UWORD *, RGB8 *);
static void MemDrawRGBIndex8(objBitmap *, UBYTE *, RGB8 *);

static void MemReadRGBPixel32(objBitmap *, LONG, LONG, RGB8 *);
static void MemReadLSBRGBPixel24(objBitmap *, LONG, LONG, RGB8 *);
static void MemReadMSBRGBPixel24(objBitmap *, LONG, LONG, RGB8 *);
static void MemReadRGBPixel16(objBitmap *, LONG, LONG, RGB8 *);
static void MemReadRGBPixel8(objBitmap *, LONG, LONG, RGB8 *);

static void MemReadRGBIndex32(objBitmap *, ULONG *, RGB8 *);
static void MemReadLSBRGBIndex24(objBitmap *, UBYTE *, RGB8 *);
static void MemReadMSBRGBIndex24(objBitmap *, UBYTE *, RGB8 *);
static void MemReadRGBIndex16(objBitmap *, UWORD *, RGB8 *);
static void MemReadRGBIndex8(objBitmap *, UBYTE *, RGB8 *);

static void MemReadRGBPixelPlanar(objBitmap *, LONG, LONG, RGB8 *);
static void MemReadRGBIndexPlanar(objBitmap *, UBYTE *, RGB8 *);
static void MemDrawPixelPlanar(objBitmap *, LONG, LONG, ULONG);
static ULONG MemReadPixelPlanar(objBitmap *, LONG, LONG);

static void DrawRGBPixelPlanar(objBitmap *, LONG X, LONG Y, RGB8 *);

//********************************************************************************************************************

static ERROR GET_Handle(extBitmap *, APTR *);

static ERROR SET_Bkgd(extBitmap *, RGB8 *);
static ERROR SET_BkgdIndex(extBitmap *, LONG);
static ERROR SET_Trans(extBitmap *, RGB8 *);
static ERROR SET_TransIndex(extBitmap *, LONG);
static ERROR SET_Data(extBitmap *, UBYTE *);
static ERROR SET_Handle(extBitmap *, APTR);
static ERROR SET_Palette(extBitmap *, RGBPalette *);

static const FieldDef clDataFlags[] = {
   { "Video", MEM::VIDEO }, { "Blit", MEM::TEXTURE }, { "NoClear", MEM::NO_CLEAR }, { "Data", 0 },
   { NULL, 0 }
};

FDEF argsDrawUCPixel[]  = { { "Void", FD_VOID  }, { "Bitmap", FD_OBJECTPTR }, { "X", FD_LONG }, { "Y", FD_LONG }, { "Colour", FD_LONG }, { NULL, 0 } };
FDEF argsDrawUCRPixel[] = { { "Void", FD_VOID  }, { "Bitmap", FD_OBJECTPTR }, { "X", FD_LONG }, { "Y", FD_LONG }, { "Colour", FD_PTR|FD_RGB }, { NULL, 0 } };
FDEF argsReadUCPixel[]  = { { "Value", FD_LONG }, { "Bitmap", FD_OBJECTPTR }, { "X", FD_LONG }, { "Y", FD_LONG }, { "Colour", FD_PTR|FD_RESULT|FD_RGB }, { NULL, 0 } };
FDEF argsReadUCRPixel[] = { { "Void", FD_VOID  }, { "Bitmap", FD_OBJECTPTR }, { "X", FD_LONG }, { "Y", FD_LONG }, { "Colour", FD_PTR|FD_RESULT|FD_RGB }, { NULL, 0 } };
FDEF argsDrawUCRIndex[] = { { "Void", FD_VOID  }, { "Bitmap", FD_OBJECTPTR }, { "Data", FD_PTR }, { "Colour", FD_PTR|FD_RGB }, { NULL, 0 } };
FDEF argsReadUCRIndex[] = { { "Void", FD_VOID  }, { "Bitmap", FD_OBJECTPTR }, { "Data", FD_PTR }, { "Colour", FD_PTR|FD_RGB|FD_RESULT }, { NULL, 0 } };

//********************************************************************************************************************

#ifdef __xwindows__
static ERROR alloc_shm(LONG Size, UBYTE **Data, LONG *ID)
{
   pf::Log log(__FUNCTION__);

   auto id = shmget(IPC_PRIVATE, Size, IPC_CREAT|IPC_EXCL|S_IRWXO|S_IRWXG|S_IRWXU);
   if (id IS -1) {
      log.warning("shmget() returned: %s", strerror(errno));
      return ERR_Memory;
   }

   auto addr = shmat(id, NULL, 0);
   if ((addr != (APTR)-1) and (addr != NULL)) {
      *Data = (UBYTE *)addr;
      *ID = id;
      return ERR_Okay;
   }
   else {
      log.warning("shmat() returned: %s", strerror(errno));
      return ERR_LockFailed;
   }
}

static void free_shm(APTR Address, LONG ID)
{
   shmdt(Address);
   shmctl(ID, IPC_RMID, NULL);
}
#endif

//********************************************************************************************************************
// Score = Abs(BB1 - BB2) + Abs(GG1 - GG2) + Abs(RR1 - RR2)
// The closer the score is to zero, the better the colour match.

static ULONG RGBToValue(RGB8 *RGB, RGBPalette *Palette)
{
   LONG BestMatch  = 0x7fffffff; // Highest possible value
   ULONG best = 0;
   WORD mred   = RGB->Red;
   WORD mgreen = RGB->Green;
   WORD mblue  = RGB->Blue;

   WORD i;
   for (i=Palette->AmtColours-1; i > 0; i--) {
      LONG Match = mred - Palette->Col[i].Red; // R1 - R2
      if (Match < 0) Match = -Match; // Abs(R1 - R2)

      WORD g = mgreen - Palette->Col[i].Green;
      if (g < 0) Match -= g; else Match += g;

      WORD b = mblue - Palette->Col[i].Blue;
      if (b < 0) Match -= b; else Match += b;

      if (Match < BestMatch) {
         if (!Match) return i;
         BestMatch  = Match;
         best = i;
      }
   }

   return best;
}

//********************************************************************************************************************

inline static UBYTE conv_l2r(DOUBLE X) {
   LONG ix;

   if (X < 0.0031308) ix = F2T(((X * 12.92) * 255.0) + 0.5);
   else ix = F2T(((std::pow(X, 1.0 / 2.4) * 1.055 - 0.055) * 255.0) + 0.5);

   if (ix < 0) return 0;
   else if (ix > 255) return 255;
   else return ix;
}

/*********************************************************************************************************************

-ACTION-
Clear: Clears a bitmap's image to black.

Clearing a bitmap wipes away its graphical contents by drawing a blank area over its existing graphics.  The colour of
the blank area is determined by the #BkgdRGB field.  To clear a bitmap to a different colour, use the #DrawRectangle()
method instead.

If the bitmap supports alpha blending, the alpha blend bits will be reset to 'clear' status.

*********************************************************************************************************************/

static ERROR BITMAP_Clear(extBitmap *Self, APTR Void)
{
#ifdef _GLES_
   if ((Self->DataFlags & MEM::VIDEO) != MEM::NIL) {
      if (!lock_graphics_active(__func__)) {
         glClearColorx(Self->BkgdRGB.Red, Self->BkgdRGB.Green, Self->BkgdRGB.Blue, 255);
         glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
         unlock_graphics();
         return ERR_Okay;
      }
      else return ERR_LockFailed;
   }
#endif

   LONG opacity = Self->Opacity;
   Self->Opacity = 255;
   gfxDrawRectangle(Self, 0, 0, Self->Width, Self->Height, Self->BkgdIndex, BAF_FILL);
   Self->Opacity = opacity;
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
Compress: Compresses bitmap data to save memory.

A bitmap can be compressed with the CompressBitmap method to save memory when the bitmap is not in use.  This is useful
if a large bitmap needs to be stored in memory and it is anticipated that the bitmap will be used infrequently.

Once a bitmap is compressed, its image data is invalid.  Any attempt to access the bitmap's image data will likely
result in a memory access fault.  The image data will remain invalid until the #Decompress() method is
called to restore the bitmap to its original state.

The `BMF_COMPRESSED` bit will be set in the #Flags field after a successful call to this function to indicate that the
bitmap is compressed.

-INPUT-
int Level: Level of compression.  Zero uses a default setting (recommended), the maximum is 10.

-ERRORS-
Okay
NullArgs
AllocMemory
ReallocMemory
CreateObject: A Compression object could not be created.
-END-

*********************************************************************************************************************/

static ERROR BITMAP_Compress(extBitmap *Self, struct bmpCompress *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if ((Self->DataFlags & (MEM::VIDEO|MEM::TEXTURE)) != MEM::NIL) {
      log.warning("Cannot compress video bitmaps.");
      return ERR_Failed;
   }

   if (Self->Size < 8192) return ERR_Okay;

   log.traceBranch("");

   if (Self->prvCompress) {
      // If the original compression object still exists, all we are going to do is free up the raw bitmap data.

      if ((Self->Data) and (Self->prvAFlags & BF_DATA)) {
         FreeResource(Self->Data);
         Self->Data = NULL;
      }

      return ERR_Okay;
   }

   ERROR error = ERR_Okay;
   if (!glCompress) {
      if (!(glCompress = objCompression::create::global())) {
         return log.warning(ERR_CreateObject);
      }
      SetOwner(glCompress, glModule);
   }

   APTR buffer;
   if (!AllocMemory(Self->Size, MEM::NO_CLEAR, &buffer)) {
      struct cmpCompressBuffer cbuf;
      cbuf.Input      = Self->Data;
      cbuf.InputSize  = Self->Size;
      cbuf.Output     = buffer;
      cbuf.OutputSize = Self->Size;
      if (!Action(MT_CmpCompressBuffer, glCompress, &cbuf)) {
         if (!AllocMemory(cbuf.Result, MEM::NO_CLEAR, &Self->prvCompress)) {
            CopyMemory(buffer, Self->prvCompress, cbuf.Result);
            FreeResource(buffer);
         }
         else error = ERR_ReallocMemory;
      }
      else error = ERR_Failed;
   }
   else error = ERR_AllocMemory;

   if (!error) { // Free the original data
      if ((Self->Data) and (Self->prvAFlags & BF_DATA)) {
         FreeResource(Self->Data);
         Self->Data = NULL;
      }

      Self->Flags |= BMF_COMPRESSED;
   }

   return error;
}

/*********************************************************************************************************************
-METHOD-
ConvertToLinear: Convert a bitmap's colour space to linear RGB.

Use ConvertToLinear to convert the colour space of a bitmap from sRGB to linear RGB.  If the `BMF_ALPHA_CHANNEL` flag
is enabled on the bitmap, pixels with an alpha value of 0 are ignored.

The #ColourSpace will be set to `LINEAR_RGB` on completion.  This method returns immediately if the #ColourSpace is
already set to `LINEAR_RGB`.

For the sake of efficiency, lookup tables are used to quickly perform the conversion process.

-ERRORS-
Okay
NothingDone: The Bitmap's content is already in linear RGB format.
InvalidState: The Bitmap is not in the expected state.
InvalidDimension: The clipping region is invalid.
-END-
*********************************************************************************************************************/

ERROR BITMAP_ConvertToLinear(extBitmap *Self, APTR Void)
{
   pf::Log log;

   if (Self->ColourSpace IS CS_LINEAR_RGB) return log.warning(ERR_NothingDone);
   if (Self->BytesPerPixel != 4) return log.warning(ERR_InvalidState);

   const auto w = (LONG)(Self->Clip.Right - Self->Clip.Left);
   const auto h = (LONG)(Self->Clip.Bottom - Self->Clip.Top);

   if (Self->Clip.Left + w > Self->Width) return log.warning(ERR_InvalidDimension);
   if (Self->Clip.Top + h > Self->Height) return log.warning(ERR_InvalidDimension);

   if (Self->Flags & BMF_ALPHA_CHANNEL) {
      const UBYTE R = Self->ColourFormat->RedPos>>3;
      const UBYTE G = Self->ColourFormat->GreenPos>>3;
      const UBYTE B = Self->ColourFormat->BluePos>>3;
      const UBYTE A = Self->ColourFormat->AlphaPos>>3;

      UBYTE *data = Self->Data + (Self->LineWidth * Self->Clip.Top) + (Self->Clip.Left * Self->BytesPerPixel);
      for (LONG y=0; y < h; y++) {
         UBYTE *pixel = data;
         for (LONG x=0; x < w; x++) {
            if (pixel[A]) {
               pixel[R] = glLinearRGB.convert(pixel[R]);
               pixel[G] = glLinearRGB.convert(pixel[G]);
               pixel[B] = glLinearRGB.convert(pixel[B]);
            }
            pixel += Self->BytesPerPixel;
         }
         data += Self->LineWidth;
      }
   }
   else {
      const UBYTE R = Self->ColourFormat->RedPos>>3;
      const UBYTE G = Self->ColourFormat->GreenPos>>3;
      const UBYTE B = Self->ColourFormat->BluePos>>3;

      UBYTE *data = Self->Data + (Self->LineWidth * Self->Clip.Top) + (Self->Clip.Left * Self->BytesPerPixel);
      for (LONG y=0; y < h; y++) {
         UBYTE *pixel = data;
         for (LONG x=0; x < w; x++) {
            pixel[R] = glLinearRGB.convert(pixel[R]);
            pixel[G] = glLinearRGB.convert(pixel[G]);
            pixel[B] = glLinearRGB.convert(pixel[B]);
            pixel += Self->BytesPerPixel;
         }
         data += Self->LineWidth;
      }
   }

   Self->ColourSpace = CS_LINEAR_RGB;
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
ConvertToRGB: Convert a bitmap's colour space to standard RGB.

Use ConvertToRGB to convert the colour space of a bitmap from linear RGB to sRGB.  If the `BMF_ALPHA_CHANNEL` flag is
enabled on the bitmap, pixels with an alpha value of 0 are ignored.

The #ColourSpace will be set to `SRGB` on completion.  This method returns immediately if the #ColourSpace is
already set to `SRGB`.

For the sake of efficiency, lookup tables are used to quickly perform the conversion process.

-ERRORS-
Okay
NothingDone: The Bitmap's content is already in sRGB format.
InvalidState: The Bitmap is not in the expected state.
InvalidDimension: The clipping region is invalid.

*********************************************************************************************************************/

ERROR BITMAP_ConvertToRGB(extBitmap *Self, APTR Void)
{
   pf::Log log(__FUNCTION__);

   if (Self->ColourSpace IS CS_SRGB) return log.warning(ERR_NothingDone);
   if (Self->BytesPerPixel != 4) return log.warning(ERR_InvalidState);

   const auto w = (LONG)(Self->Clip.Right - Self->Clip.Left);
   const auto h = (LONG)(Self->Clip.Bottom - Self->Clip.Top);

   if (Self->Clip.Left + w > Self->Width) return log.warning(ERR_InvalidDimension);
   if (Self->Clip.Top + h > Self->Height) return log.warning(ERR_InvalidDimension);

   if (Self->Flags & BMF_ALPHA_CHANNEL) {
      const UBYTE R = Self->ColourFormat->RedPos>>3;
      const UBYTE G = Self->ColourFormat->GreenPos>>3;
      const UBYTE B = Self->ColourFormat->BluePos>>3;
      const UBYTE A = Self->ColourFormat->AlphaPos>>3;

      UBYTE *data = Self->Data + (Self->LineWidth * Self->Clip.Top) + (Self->Clip.Left * Self->BytesPerPixel);
      for (LONG y=0; y < h; y++) {
         UBYTE *pixel = data;
         for (LONG x=0; x < w; x++) {
            if (pixel[A]) {
               pixel[R] = glLinearRGB.invert(pixel[R]);
               pixel[G] = glLinearRGB.invert(pixel[G]);
               pixel[B] = glLinearRGB.invert(pixel[B]);
            }
            pixel += Self->BytesPerPixel;
         }
         data += Self->LineWidth;
      }
   }
   else {
      const UBYTE R = Self->ColourFormat->RedPos>>3;
      const UBYTE G = Self->ColourFormat->GreenPos>>3;
      const UBYTE B = Self->ColourFormat->BluePos>>3;

      UBYTE *data = Self->Data + (Self->LineWidth * Self->Clip.Top) + (Self->Clip.Left * Self->BytesPerPixel);
      for (LONG y=0; y < h; y++) {
         UBYTE *pixel = data;
         for (LONG x=0; x < w; x++) {
            pixel[R] = glLinearRGB.invert(pixel[R]);
            pixel[G] = glLinearRGB.invert(pixel[G]);
            pixel[B] = glLinearRGB.invert(pixel[B]);
            pixel += Self->BytesPerPixel;
         }
         data += Self->LineWidth;
      }
   }

   Self->ColourSpace = CS_SRGB;
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
CopyArea: Copies a rectangular area from one bitmap to another.

This method is a proxy for ~Display.CopyArea().

-INPUT-
obj(Bitmap) DestBitmap: The target bitmap.
int(BAF) Flags:  Optional flags.
int X: The horizontal position of the area to be copied.
int Y: The vertical position of the area to be copied.
int Width:  The width of the area.
int Height: The height of the area.
int XDest:  The horizontal position to copy the area to.
int YDest:  The vertical position to copy the area to.

-ERRORS-
Okay
NullArgs
Mismatch: The target bitmap is not a close enough match to the source bitmap in order to perform the operation.

*********************************************************************************************************************/

static ERROR BITMAP_CopyArea(objBitmap *Self, struct bmpCopyArea *Args)
{
   if (Args) return gfxCopyArea((extBitmap *)Self, (extBitmap *)Args->DestBitmap, Args->Flags, Args->X, Args->Y, Args->Width, Args->Height, Args->XDest, Args->YDest);
   else return ERR_NullArgs;
}

/*********************************************************************************************************************

-METHOD-
Decompress: Decompresses a compressed bitmap.

The Decompress method is used to restore a compressed bitmap to its original state.  If the bitmap is not compressed,
the method does nothing.

The compressed data will be terminated unless `RetainData` is TRUE.  Retaining the data will allow the client to
repeatedly restore the content of the most recent #Compress() call.

-INPUT-
int RetainData: Retains the compression data if TRUE.

-ERRORS-
Okay
AllocMemory: Insufficient memory in recreating the bitmap data buffer.

*********************************************************************************************************************/

static ERROR BITMAP_Decompress(extBitmap *Self, struct bmpDecompress *Args)
{
   pf::Log log;
   struct cmpDecompressBuffer dbuf;

   if (!Self->prvCompress) return ERR_Okay;

   log.msg(VLF::BRANCH|VLF::EXTAPI, "Size: %d, Retain: %d", Self->Size, (Args) ? Args->RetainData : FALSE);

   // Note: If the decompression fails, we'll keep the bitmap data in memory in order to stop code from failing if it
   // accesses the Data address following attempted decompression.

   if (!Self->Data) {
      if (!AllocMemory(Self->Size, MEM::NO_BLOCKING|MEM::NO_POOL|MEM::NO_CLEAR|Self->DataFlags, &Self->Data)) {
         Self->prvAFlags |= BF_DATA;
      }
      else return log.warning(ERR_AllocMemory);
   }

   if (!glCompress) {
      if (!(glCompress = objCompression::create::global())) {
         return log.warning(ERR_CreateObject);
      }
      SetOwner(glCompress, glModule);
   }

   dbuf.Input      = Self->prvCompress;
   dbuf.Output     = Self->Data;
   dbuf.OutputSize = Self->Size;
   ERROR error = Action(MT_CmpDecompressBuffer, glCompress, &dbuf);
   if (error IS ERR_BufferOverflow) error = ERR_Okay;

   if ((Args) and (Args->RetainData IS TRUE)) {
      // Keep the source compression data
   }
   else {
      FreeResource(Self->prvCompress);
      Self->prvCompress = NULL;
      Self->Flags &= ~BMF_COMPRESSED;
   }

   return error;
}

/*********************************************************************************************************************

-ACTION-
CopyData: Copies bitmap image data to other bitmaps with colour remapping enabled.

This action will copy the image of the bitmap to any other initialised bitmap that you specify.  Support for copying
the image data to other object class types is not provided.

This action features automatic clipping and remapping, for occasions where the bitmaps do not match up in size or colour.

*********************************************************************************************************************/

static ERROR BITMAP_CopyData(extBitmap *Self, struct acCopyData *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Dest)) return log.warning(ERR_NullArgs);
   if ((Args->Dest->Class->ClassID != ID_BITMAP)) return log.warning(ERR_Args);

   auto target = (extBitmap *)Args->Dest;

   LONG max_height = Self->Height > target->Height ? target->Height : Self->Height;

   if (Self->Width >= target->Width) { // Source is wider or equal to the target
      gfxCopyArea(Self, target, 0, 0, 0, target->Width, max_height, 0, 0);
   }
   else { // The target is wider than the source.  Cpoy the source first, then clear the exposed region on the right.
      gfxCopyArea(Self, target, 0, 0, 0, Self->Width, max_height, 0, 0);
      gfxDrawRectangle(target, Self->Width, 0, target->Width - Self->Width, max_height, target->BkgdIndex, BAF_FILL);
   }

   // If the target height is greater, we will need to clear the pixels trailing at the bottom.

   if (Self->Height < target->Height) {
      gfxDrawRectangle(target, 0, Self->Height, target->Width, target->Height - Self->Height, target->BkgdIndex, BAF_FILL);
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
Demultiply: Reverses the conversion process performed by Premultiply().

Use Demultiply to normalise RGB values that have previously been converted by #Premultiply().  This method will
return immediately if the bitmap values are already normalised.

-ERRORS-
Okay
NothingDone: The content is already normalised.
InvalidState: The Bitmap is not in the expected state (32-bit with an alpha channel).
InvalidDimension: The clipping region is invalid.

*********************************************************************************************************************/

static ERROR BITMAP_Demultiply(extBitmap *Self, APTR Void)
{
   pf::Log log;

   static std::mutex mutex;
   if (!glDemultiply) {
      const std::lock_guard<std::mutex> lock(mutex);
      if (!glDemultiply) {
         if (!AllocMemory(256 * 256, MEM::NO_CLEAR|MEM::UNTRACKED, &glDemultiply)) {
            for (LONG a=1; a <= 255; a++) {
               for (LONG i=0; i <= 255; i++) {
                  glDemultiply[(a<<8) + i] = (i * 0xff) / a;
               }
            }
         }
         else return ERR_AllocMemory;
      }
   }

   if (!(Self->Flags & BMF_PREMUL)) return log.warning(ERR_NothingDone);
   if (Self->BitsPerPixel != 32) return log.warning(ERR_InvalidState);
   if (!(Self->Flags & BMF_ALPHA_CHANNEL)) return log.warning(ERR_InvalidState);

   const auto w = (LONG)(Self->Clip.Right - Self->Clip.Left);
   const auto h = (LONG)(Self->Clip.Bottom - Self->Clip.Top);

   if (Self->Clip.Left + w > Self->Width) return log.warning(ERR_InvalidDimension);
   if (Self->Clip.Top + h > Self->Height) return log.warning(ERR_InvalidDimension);

   const UBYTE A = Self->ColourFormat->AlphaPos>>3;
   const UBYTE R = Self->ColourFormat->RedPos>>3;
   const UBYTE G = Self->ColourFormat->GreenPos>>3;
   const UBYTE B = Self->ColourFormat->BluePos>>3;

   UBYTE *data = Self->Data + (Self->Clip.Left * Self->BytesPerPixel) + (Self->Clip.Top * Self->LineWidth);
   for (LONG y=0; y < h; y++) {
      UBYTE *pixel = data;
      for (LONG x=0; x < w; x++) {
         const UBYTE a = pixel[A];
         if (a < 0xff) {
            if (a == 0) pixel[R] = pixel[G] = pixel[B] = 0;
            else {
               ULONG r = glDemultiply[(a<<8) + pixel[R]]; //(ULONG(pixel[R]) * 0xff) / a;
               ULONG g = glDemultiply[(a<<8) + pixel[G]]; //(ULONG(pixel[G]) * 0xff) / a;
               ULONG b = glDemultiply[(a<<8) + pixel[B]]; //(ULONG(pixel[B]) * 0xff) / a;
               pixel[R] = UBYTE((r > 0xff) ? 0xff : r);
               pixel[G] = UBYTE((g > 0xff) ? 0xff : g);
               pixel[B] = UBYTE((b > 0xff) ? 0xff : b);
            }
         }
         pixel += 4;
      }
      data += Self->LineWidth;
   }

   Self->Flags &= ~BMF_PREMUL;
   return ERR_Okay;
}

/*********************************************************************************************************************

-ACTION-
Draw: Clears a bitmap's image to its assigned background colour.

*********************************************************************************************************************/

static ERROR BITMAP_Draw(extBitmap *Self, APTR Void)
{
   gfxDrawRectangle(Self, 0, 0, Self->Width, Self->Height, Self->BkgdIndex, BAF_FILL);
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
DrawRectangle: Draws rectangles, both filled and unfilled.

This method draws both filled and unfilled rectangles.  The rectangle is drawn to the target bitmap at position (X, Y)
with dimensions determined by the specified Width and Height.  If the Fill argument is set to TRUE then the rectangle
will be filled, otherwise the rectangle's outline will be drawn.  The colour of the rectangle is determined by the
pixel value in the Colour argument.

-INPUT-
int X: The left-most coordinate of the rectangle.
int Y: The top-most coordinate of the rectangle.
int Width:  The width of the rectangle.
int Height: The height of the rectangle.
uint Colour: The colour index to use for the rectangle.
int(BAF) Flags:  Supports BAF_FILL and BAF_BLEND.

-ERRORS-
Okay
Args

*********************************************************************************************************************/

static ERROR BITMAP_DrawRectangle(extBitmap *Self, struct bmpDrawRectangle *Args)
{
   if (!Args) return ERR_NullArgs;
   gfxDrawRectangle(Self, Args->X, Args->Y, Args->Width, Args->Height, Args->Colour, Args->Flags);
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
Flip: Flips a bitmap around the horizontal or vertical axis.

This method is used to flip bitmap images on their horizontal or vertical axis.

-INPUT-
int(FLIP) Orientation: Set to either FLIP_HORIZONTAL or FLIP_VERTICAL.

-ERRORS-
Okay
Args
NullArgs

*********************************************************************************************************************/

static ERROR BITMAP_Flip(extBitmap *Self, struct bmpFlip *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   // NB: A faster way to flip a Bitmap would be to use CopyArea() to do the transfer in strips, but would require a
   // temporary memory area to hold the information.

   if (Args->Orientation IS FLIP_HORIZONTAL) {
      if (!lock_surface(Self, SURFACE_READWRITE)) {
         for (LONG y=0; y < Self->Height/2; y++) {
            for (LONG x=0; x < Self->Width; x++) {
               LONG c1 = Self->ReadUCPixel(Self, x, Self->Height - y - 1);
               LONG c2 = Self->ReadUCPixel(Self, x, y);
               Self->DrawUCPixel(Self, x, y, c1);
               Self->DrawUCPixel(Self, x, Self->Height - y - 1, c2);
            }
         }
         unlock_surface(Self);
      }
   }
   else if (Args->Orientation IS FLIP_VERTICAL) {
      if (!lock_surface(Self, SURFACE_READWRITE)) {
         // Palette based Bitmap
         for (LONG x=0; x < Self->Width/2; x++) {
            for (LONG y=0; y < Self->Height; y++) {
               LONG c1 = Self->ReadUCPixel(Self, Self->Width - x - 1, y); // Right pixel
               LONG c2 = Self->ReadUCPixel(Self, x, y); // Left pixel
               Self->DrawUCPixel(Self, Self->Width - x - 1, y, c2);
               Self->DrawUCPixel(Self, x, y, c1);
            }
         }
         unlock_surface(Self);
      }
   }
   else return log.warning(ERR_Args);

   return ERR_Okay;
}

/*********************************************************************************************************************

-ACTION-
Flush: Flushes pending graphics operations and returns when the accelerator is idle.

The Flush action is provided for you to ensure that your graphics operations are synchronised with the graphics
accelerator.  Synchronisation is essential prior to drawing to the bitmap with the CPU.  Failure to synchronise may
result in corruption in the bitmap's graphics display.

You do not have to use this function if you stick to using the graphics functions that are provided in the Bitmap class.
-END-

*********************************************************************************************************************/

static ERROR BITMAP_Flush(extBitmap *Self, APTR Void)
{
#ifdef _GLES_
   if (!lock_graphics_active(__func__)) {
      glFlush();
      unlock_graphics();
   }
#endif
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR BITMAP_Free(extBitmap *Self, APTR Void)
{
   #ifdef __xwindows__
      if (Self->x11.XShmImage) {
         // Tell the X11 server to detach from the memory block
         XShmDetach(XDisplay, &Self->x11.ShmInfo);
         Self->x11.XShmImage = false;
         free_shm(Self->Data, Self->x11.ShmInfo.shmid);
         Self->Data = NULL;
      }
   #endif

   if ((Self->Data) and (Self->prvAFlags & BF_DATA)) {
      FreeResource(Self->Data);
      Self->Data = NULL;
   }

   if (Self->prvCompress) { FreeResource(Self->prvCompress); Self->prvCompress = NULL; }

   if (Self->ResolutionChangeHandle) {
      UnsubscribeEvent(Self->ResolutionChangeHandle);
      Self->ResolutionChangeHandle = NULL;
   }

   #ifdef __xwindows__
      if (Self->x11.drawable) {
         if (XDisplay) XFreePixmap(XDisplay, Self->x11.drawable);
         Self->x11.drawable = 0;
      }

      if (Self->x11.readable) {
         XDestroyImage(Self->x11.readable);
         Self->x11.readable = NULL;
      }
   #endif

   #ifdef _WIN32
      if (Self->win.Drawable) {
         winDeleteDC(Self->win.Drawable);
         Self->win.Drawable = NULL;
      }
   #endif

   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
GetColour: Converts Red, Green, Blue components into a single colour value.

The GetColour method is used to convert Red, Green and Blue colour components into a single colour index that can be
used for directly writing colours to the bitmap.  The result is returned in the Colour parameter.

-INPUT-
int Red:    Red component from 0 - 255.
int Green:  Green component from 0 - 255.
int Blue:   Blue component value from 0 - 255.
int Alpha:  Alpha component value from 0 - 255.
&uint Colour: The resulting colour value will be returned here.

-ERRORS-
Okay
NullArgs

*********************************************************************************************************************/

static ERROR BITMAP_GetColour(extBitmap *Self, struct bmpGetColour *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Self->BitsPerPixel > 8) {
      Args->Colour = Self->packPixel(Args->Red, Args->Green, Args->Blue, Args->Alpha);
   }
   else {
      struct RGB8 rgb;
      rgb.Red   = Args->Red;
      rgb.Green = Args->Green;
      rgb.Blue  = Args->Blue;
      rgb.Alpha = Args->Alpha;
      Args->Colour = RGBToValue(&rgb, Self->Palette);
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-ACTION-
Init: Initialises a bitmap.

This action will initialise a bitmap object so that it is ready for use.  If the bitmap #Data field has not been
specified, a memory block will be allocated and placed in this field.  The type of memory that is allocated is
dependent on the bitmap #DataFlags field.  If you have not specified a memory type, you will get a default of
`MEM::DATA`.  For a display compatible bitmap use `MEM::VIDEO`.  If you just want to store a bitmap in fast writeable
memory, use `MEM::TEXTURE`.

This action will not work unless you have defined the #Width and #Height fields of the bitmap at a minimum.

*********************************************************************************************************************/

static ERROR BITMAP_Init(extBitmap *Self, APTR Void)
{
   pf::Log log;
   #ifdef __xwindows__
      WORD alignment;
   #endif
   ERROR error;

   if (acQuery(Self) != ERR_Okay) return log.warning(ERR_Query);

   log.branch("Size: %dx%d @ %d bit, %d bytes, Mem: $%.8x, Flags: $%.8x", Self->Width, Self->Height, Self->BitsPerPixel, Self->BytesPerPixel, LONG(Self->DataFlags), LONG(Self->Flags));

   if (Self->Clip.Left < 0) Self->Clip.Left = 0;
   if (Self->Clip.Top < 0)  Self->Clip.Top  = 0;
   if ((Self->Clip.Right > Self->Width)  or (Self->Clip.Right < 1)) Self->Clip.Right = Self->Width;
   if ((Self->Clip.Bottom > Self->Height) or (Self->Clip.Bottom < 1)) Self->Clip.Bottom = Self->Height;

   // If the Bitmap is 15 or 16 bit, make corrections to the background values

   if (Self->BitsPerPixel IS 16) {
      Self->TransRGB.Red   &= 0xf8;
      Self->TransRGB.Green &= 0xfc;
      Self->TransRGB.Blue  &= 0xf8;

      Self->BkgdRGB.Red   &= 0xf8;
      Self->BkgdRGB.Green &= 0xfc;
      Self->BkgdRGB.Blue  &= 0xf8;
   }
   else if (Self->BitsPerPixel IS 15) {
      Self->TransRGB.Red   &= 0xf8;
      Self->TransRGB.Green &= 0xf8;
      Self->TransRGB.Blue  &= 0xf8;

      Self->BkgdRGB.Red   &= 0xf8;
      Self->BkgdRGB.Green &= 0xf8;
      Self->BkgdRGB.Blue  &= 0xf8;
   }

#ifdef __xwindows__

   Self->DataFlags &= ~MEM::TEXTURE; // Blitter memory not available in X11

   if (!Self->Data) {
      if (!(Self->Flags & BMF_NO_DATA)) {
         Self->DataFlags &= ~MEM::VIDEO; // Video memory not available for allocation in X11 (may be set to identify X11 windows only)

         if (!Self->Size) {
            log.warning("The Bitmap has no Size (there is a dimensional error).");
            return ERR_FieldNotSet;
         }

         //if (Self->DataFlags & MEM::VIDEO) {
         //   Self->x11.drawable = XCreatePixmap(XDisplay, DefaultRootWindow(XDisplay), Self->Width, Self->Height, Self->BitsPerPixel);
         //   if (!Self->x11.drawable) {
         //      log.warning("X11 failed to allocate a Pixmap for the video based Bitmap.");
         //      return ERR_Failed;
         //   }
         //}
         //else {

            if (!Self->x11.XShmImage) {
               log.extmsg("Allocating a memory based XImage.");
               if (!alloc_shm(Self->Size, &Self->Data, &Self->x11.ShmInfo.shmid)) {
                  Self->prvAFlags |= BF_DATA;

                  if (Self->LineWidth & 0x0001) alignment = 8;
                  else if (Self->LineWidth & 0x0002) alignment = 16;
                  else alignment = 32;

                  if (!glHeadless) {
                     Self->x11.ximage.width            = Self->Width;  // Image width
                     Self->x11.ximage.height           = Self->Height; // Image height
                     Self->x11.ximage.xoffset          = 0;            // Number of pixels offset in X direction
                     Self->x11.ximage.format           = ZPixmap;      // XYBitmap, XYPixmap, ZPixmap
                     Self->x11.ximage.data             = (char *)Self->Data; // Pointer to image data
                     if (glX11ShmImage) Self->x11.ximage.obdata = (char *)&Self->x11.ShmInfo; // Magic pointer for the XShm extension
                     Self->x11.ximage.byte_order       = 0;            // LSBFirst / MSBFirst
                     Self->x11.ximage.bitmap_unit      = alignment;    // Quant. of scanline - 8, 16, 32
                     Self->x11.ximage.bitmap_bit_order = 0;            // LSBFirst / MSBFirst
                     Self->x11.ximage.bitmap_pad       = alignment;    // 8, 16, 32, either XY or Zpixmap
                     if (Self->BitsPerPixel IS 32) Self->x11.ximage.depth = 24;
                     else Self->x11.ximage.depth = Self->BitsPerPixel;            // Actual bits per pixel
                     Self->x11.ximage.bytes_per_line   = Self->LineWidth;         // Accelerator to next line
                     Self->x11.ximage.bits_per_pixel   = Self->BytesPerPixel * 8; // Bits per pixel-group
                     Self->x11.ximage.red_mask         = 0;
                     Self->x11.ximage.green_mask       = 0;
                     Self->x11.ximage.blue_mask        = 0;

                     XInitImage(&Self->x11.ximage);

                     // If the XShm extension is available, try using it.  Using XShm allows the
                     // X11 server to copy image memory straight to the display rather than
                     // having it messaged.

                     if (glX11ShmImage) {
                        Self->x11.ShmInfo.readOnly = False;
                        Self->x11.ShmInfo.shmaddr  = (char *)Self->Data;

                        // Attach the memory block to the X11 server

                        if (XShmAttach(XDisplay, &Self->x11.ShmInfo)) {
                           Self->x11.XShmImage = true;
                           XSync(XDisplay, TRUE);
                        }
                        else log.warning("XShmAttach() failed.");
                     }
                  }
               }
               else return log.warning(ERR_AllocMemory);
            }
         //}
      }
   }

   if (!glHeadless) XSync(XDisplay, False);

#elif _WIN32

   Self->DataFlags &= ~MEM::TEXTURE; // Video buffer memory not available in Win32

   if (!Self->Data) {
      if (!(Self->Flags & BMF_NO_DATA)) {
         if (!Self->Size) {
            log.warning("The Bitmap has no Size (there is a dimensional error).");
            return ERR_FieldNotSet;
         }

         if ((Self->DataFlags & MEM::VIDEO) != MEM::NIL) {
            Self->prvAFlags |= BF_WINVIDEO;
            if ((Self->win.Drawable = winCreateCompatibleDC())) {
            }
            else return log.warning(ERR_SystemCall);
         }
         else if (!AllocMemory(Self->Size, MEM::NO_BLOCKING|MEM::NO_POOL|MEM::NO_CLEAR|Self->DataFlags, &Self->Data)) {
            Self->prvAFlags |= BF_DATA;
         }
         else return log.warning(ERR_AllocMemory);
      }
      else {
         if ((Self->DataFlags & MEM::VIDEO) != MEM::NIL) Self->prvAFlags |= BF_WINVIDEO;
      }
   }

#elif _GLES_
   // MEM::VIDEO + BMF_NO_DATA: The bitmap represents the OpenGL display.  No data area will be allocated as direct access to the OpenGL video frame buffer is not possible.
   // MEM::VIDEO: Not currently used as a means of allocating a particular type of OpenGL buffer.
   // MEM::TEXTURE:  The bitmap is to be used as an OpenGL texture or off-screen buffer.  The bitmap content is temporary - i.e. the content can be dumped by the graphics driver if the video display changes.
   // MEM::DATA:  The bitmap resides in regular CPU accessible memory.

   if (!Self->Data) {
      if (!(Self->Flags & BMF_NO_DATA)) {
         if (Self->Size <= 0) {
            log.warning("The Bitmap has no Size (there is a dimensional error).");
            return ERR_FieldNotSet;
         }

         if ((Self->DataFlags & MEM::VIDEO) != MEM::NIL) {
            // Do nothing - the bitmap merely represents the video display and does not hold content.
         }
         else if ((Self->DataFlags & MEM::TEXTURE) != MEM::NIL) {
            // Blittable bitmaps are fast, but their content is temporary.  It is not possible to use the CPU on this
            // bitmap type - the developer should use MEM::DATA if that is desired.

            log.warning("Support for MEM::TEXTURE not included yet.");
            return ERR_NoSupport;
         }
         else if (!AllocMemory(Self->Size, Self->DataFlags|MEM::NO_BLOCKING|MEM::NO_POOL|MEM::NO_CLEAR, &Self->Data)) {
            Self->prvAFlags |= BF_DATA;
         }
         else return ERR_AllocMemory;
      }
   }

   if ((Self->DataFlags & (MEM::VIDEO|MEM::TEXTURE)) != MEM::NIL) Self->Flags |= BMF_2DACCELERATED;

#else
   #error Platform requires memory allocation routines for the Bitmap class.
#endif

   // Determine the correct pixel format for the bitmap

#ifdef __xwindows__

   if ((!glHeadless) and (Self->x11.drawable)) {
      XVisualInfo visual, *info;
      LONG items;
      visual.bits_per_rgb = Self->BytesPerPixel * 8;
      if ((info = XGetVisualInfo(XDisplay, VisualBitsPerRGBMask, &visual, &items))) {
         gfxGetColourFormat(Self->ColourFormat, Self->BitsPerPixel, info->red_mask, info->green_mask, info->blue_mask, 0xff000000);
         XFree(info);
      }
      else gfxGetColourFormat(Self->ColourFormat, Self->BitsPerPixel, 0, 0, 0, 0);
   }
   else gfxGetColourFormat(Self->ColourFormat, Self->BitsPerPixel, Self->x11.ximage.red_mask, Self->x11.ximage.green_mask, Self->x11.ximage.blue_mask, 0xff000000);

#elif _WIN32

   if ((Self->DataFlags & MEM::VIDEO) != MEM::NIL) {
      LONG red, green, blue, alpha;

      if (winGetPixelFormat(&red, &green, &blue, &alpha) IS ERR_Okay) {
         gfxGetColourFormat(Self->ColourFormat, Self->BitsPerPixel, red, green, blue, alpha);
      }
      else gfxGetColourFormat(Self->ColourFormat, Self->BitsPerPixel, 0, 0, 0, 0);
   }
   else gfxGetColourFormat(Self->ColourFormat, Self->BitsPerPixel, 0, 0, 0, 0);

#elif _GLES_

   if (Self->BitsPerPixel >= 24) gfxGetColourFormat(Self->ColourFormat, Self->BitsPerPixel, 0x0000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
   else if (Self->BitsPerPixel IS 16) gfxGetColourFormat(Self->ColourFormat, Self->BitsPerPixel, 0xf800, 0x07e0, 0x001f, 0x0000);
   else if (Self->BitsPerPixel IS 15) gfxGetColourFormat(Self->ColourFormat, Self->BitsPerPixel, 0x7c00, 0x03e0, 0x001f, 0x0000);
   else gfxGetColourFormat(Self->ColourFormat, Self->BitsPerPixel, 0, 0, 0, 0);

#else

   gfxGetColourFormat(Self->ColourFormat, Self->BitsPerPixel, 0, 0, 0, 0);

#endif

   if ((error = CalculatePixelRoutines(Self)) != ERR_Okay) return error;

   if (Self->BitsPerPixel > 8) {
      Self->TransIndex = (((Self->TransRGB.Red   >> Self->prvColourFormat.RedShift)   & Self->prvColourFormat.RedMask)   << Self->prvColourFormat.RedPos) |
                         (((Self->TransRGB.Green >> Self->prvColourFormat.GreenShift) & Self->prvColourFormat.GreenMask) << Self->prvColourFormat.GreenPos) |
                         (((Self->TransRGB.Blue  >> Self->prvColourFormat.BlueShift)  & Self->prvColourFormat.BlueMask)  << Self->prvColourFormat.BluePos) |
                         (((255 >> Self->prvColourFormat.AlphaShift) & Self->prvColourFormat.AlphaMask) << Self->prvColourFormat.AlphaPos);

      Self->BkgdIndex = (((Self->BkgdRGB.Red   >> Self->prvColourFormat.RedShift)   & Self->prvColourFormat.RedMask)   << Self->prvColourFormat.RedPos) |
                        (((Self->BkgdRGB.Green >> Self->prvColourFormat.GreenShift) & Self->prvColourFormat.GreenMask) << Self->prvColourFormat.GreenPos) |
                        (((Self->BkgdRGB.Blue  >> Self->prvColourFormat.BlueShift)  & Self->prvColourFormat.BlueMask)  << Self->prvColourFormat.BluePos) |
                        (((255 >> Self->prvColourFormat.AlphaShift) & Self->prvColourFormat.AlphaMask) << Self->prvColourFormat.AlphaPos);
   }

   if ((!(Self->Flags & BMF_NO_DATA)) and (Self->Flags & BMF_CLEAR)) {
      log.trace("Clearing Bitmap...");
      acClear(Self);
   }

   // Sanitise the Flags field

   if (Self->BitsPerPixel < 32) Self->Flags &= ~BMF_ALPHA_CHANNEL;

   //log.msg("Red: %.2x/%d/%d , Green: %.2x/%d/%d",
   //   Self->prvColourFormat.RedMask,   Self->prvColourFormat.RedShift,   Self->prvColourFormat.RedPos,
   //   Self->prvColourFormat.GreenMask, Self->prvColourFormat.GreenShift, Self->prvColourFormat.GreenPos);

   //log.msg("Blue: %.2x/%d/%d , Alpha: %.2x/%d/%d",
   //   Self->prvColourFormat.BlueMask,  Self->prvColourFormat.BlueShift,  Self->prvColourFormat.BluePos,
   //   Self->prvColourFormat.AlphaMask, Self->prvColourFormat.AlphaShift, Self->prvColourFormat.AlphaPos);

   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Lock: Locks the bitmap surface so that you can manipulate the graphics directly.
-END-
*********************************************************************************************************************/

static ERROR BITMAP_Lock(extBitmap *Self, APTR Void)
{
#ifdef __xwindows__
   if (Self->x11.drawable) {
      WORD alignment;
      LONG size, bpp;

      // If there is an existing readable area, try to reuse it if possible

      if (Self->x11.readable) {
         if ((Self->x11.readable->width >= Self->Width) and (Self->x11.readable->height >= Self->Height)) {
            XGetSubImage(XDisplay, Self->x11.drawable, Self->XOffset + Self->Clip.Left,
               Self->YOffset + Self->Clip.Top, Self->Clip.Right - Self->Clip.Left,
               Self->Clip.Bottom - Self->Clip.Top, 0xffffffff, ZPixmap, Self->x11.readable,
               Self->XOffset + Self->Clip.Left, Self->YOffset + Self->Clip.Top);
            return ERR_Okay;
         }
         else XDestroyImage(Self->x11.readable);
      }

      // Generate a fresh XImage from the current drawable

      if (Self->LineWidth & 0x0001) alignment = 8;
      else if (Self->LineWidth & 0x0002) alignment = 16;
      else alignment = 32;

      if (Self->Type IS BMP_PLANAR) {
         size = Self->ByteWidth * Self->Height * Self->BitsPerPixel;
      }
      else size = Self->ByteWidth * Self->Height;

      Self->Data = (UBYTE *)malloc(size);

      if ((bpp = Self->BitsPerPixel) IS 32) bpp = 24;

      if ((Self->x11.readable = XCreateImage(XDisplay, CopyFromParent, bpp,
           ZPixmap, 0, (char *)Self->Data, Self->Width, Self->Height, alignment, Self->ByteWidth))) {
         XGetSubImage(XDisplay, Self->x11.drawable, Self->XOffset + Self->Clip.Left,
            Self->YOffset + Self->Clip.Top, Self->Clip.Right - Self->Clip.Left,
            Self->Clip.Bottom - Self->Clip.Top, 0xffffffff, ZPixmap, Self->x11.readable,
            Self->XOffset + Self->Clip.Left, Self->YOffset + Self->Clip.Top);
      }
      else return ERR_Failed;
   }

   return ERR_Okay;

#else

   return lock_surface(Self, SURFACE_READWRITE);

#endif
}

//********************************************************************************************************************

static ERROR BITMAP_NewObject(extBitmap *Self, APTR Void)
{
   #define CBANK 5
   RGB8 *RGB;
   LONG i, j;

   Self->Palette      = &Self->prvPaletteArray;
   Self->ColourFormat = &Self->prvColourFormat;
   Self->ColourSpace  = CS_SRGB;

   Self->Opacity = 255;

   // Generate the standard colour palette

   Self->Palette = &Self->prvPaletteArray;
   Self->Palette->AmtColours = 256;

   RGB = Self->Palette->Col;
   RGB++; // Skip the black pixel at the start

   for (i=0; i < 6; i++) {
      for (j=0; j < CBANK; j++) {
         RGB[(i*CBANK) + j].Red   = (i * 255/CBANK);
         RGB[(i*CBANK) + j].Green = 0;
         RGB[(i*CBANK) + j].Blue  = (j + 1) * 255/CBANK;
      }
   }

   for (i=6; i < 12; i++) {
      for (j=0; j < 5; j++) {
         RGB[(i*CBANK) + j].Red   = ((i-6) * 255/CBANK);
         RGB[(i*CBANK) + j].Green = 51;
         RGB[(i*CBANK) + j].Blue  = (j + 1) * 255/CBANK;
      }
   }

   for (i=12; i < 18; i++) {
      for (j=0; j < 5; j++) {
         RGB[(i*CBANK) + j].Blue  = (j + 1) * 255/CBANK;
         RGB[(i*CBANK) + j].Red   = ((i-12) * 255/CBANK);
         RGB[(i*CBANK) + j].Green = 102;
      }
   }

   for (i=18; i < 24; i++) {
      for (j=0; j < 5; j++) {
         RGB[(i*CBANK) + j].Blue  = (j + 1) * 255/CBANK;
         RGB[(i*CBANK) + j].Red   = ((i-18) * 255/CBANK);
         RGB[(i*CBANK) + j].Green = 153;
      }
   }

   for (i=24; i < 30; i++) {
      for (j=0; j < 5; j++) {
         RGB[(i*CBANK) + j].Blue  = (j + 1) * 255/CBANK;
         RGB[(i*CBANK) + j].Red   = ((i-24) * 255/CBANK);
         RGB[(i*CBANK) + j].Green = 204;
      }
   }

   for (i=30; i < 36; i++) {
      for (j=0; j < 5; j++) {
         RGB[(i*CBANK) + j].Blue  = (j + 1) * 255/CBANK;
         RGB[(i*CBANK) + j].Red   = ((i-30) * 255/CBANK);
         RGB[(i*CBANK) + j].Green = 255;
      }
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
Premultiply: Premultiplies RGB channel values by the alpha channel.

Use Premultiply to convert all RGB values in the bitmap's clipping region to pre-multiplied values.  The
exact formula applied per channel is `(Colour * Alpha + 0xff)>>8`.  The alpha channel is not affected.

This method will only operate on 32 bit bitmaps, and an alpha channel must be present.  If the RGB values are
already pre-multiplied, the method returns immediately.

The process can be reversed with a call to #Demultiply().

-ERRORS-
Okay
NothingDone: The content is already premultiplied.
InvalidState: The Bitmap is not in the expected state (32-bit with an alpha channel)
InvalidDimension: The clipping region is invalid.

*********************************************************************************************************************/

static ERROR BITMAP_Premultiply(extBitmap *Self, APTR Void)
{
   pf::Log log;

   if (Self->Flags & BMF_PREMUL) {
      return log.warning(ERR_NothingDone);
   }

   if (Self->BitsPerPixel != 32) return log.warning(ERR_InvalidState);
   if (!(Self->Flags & BMF_ALPHA_CHANNEL)) return log.warning(ERR_InvalidState);

   const auto w = (LONG)(Self->Clip.Right - Self->Clip.Left);
   const auto h = (LONG)(Self->Clip.Bottom - Self->Clip.Top);

   if (Self->Clip.Left + w > Self->Width) return log.warning(ERR_InvalidDimension);
   if (Self->Clip.Top + h > Self->Height) return log.warning(ERR_InvalidDimension);

   const UBYTE A = Self->ColourFormat->AlphaPos>>3;
   const UBYTE R = Self->ColourFormat->RedPos>>3;
   const UBYTE G = Self->ColourFormat->GreenPos>>3;
   const UBYTE B = Self->ColourFormat->BluePos>>3;

   UBYTE *data = Self->Data + (Self->Clip.Left * Self->BytesPerPixel) + (Self->Clip.Top * Self->LineWidth);
   for (LONG y=0; y < h; y++) {
      UBYTE *pixel = data;
      for (LONG x=0; x < w; x++) {
         const UBYTE a = pixel[A];
         if (a < 0xff) {
             if (a == 0) pixel[R] = pixel[G] = pixel[B] = 0;
             else {
                pixel[R] = UBYTE((pixel[R] * a + 0xff) >> 8);
                pixel[G] = UBYTE((pixel[G] * a + 0xff) >> 8);
                pixel[B] = UBYTE((pixel[B] * a + 0xff) >> 8);
             }
         }
         pixel += 4;
      }
      data += Self->LineWidth;
   }

   Self->Flags |= BMF_PREMUL;
   return ERR_Okay;
}

/*********************************************************************************************************************

-ACTION-
Query: Fills a bitmap with pre-initialised/default values prior to initialisation.

This action will pre-initialise a bitmap object so that all the fields are filled out with default values.  It stops
short of allocating the bitmap's memory.

For this action to work properly you must have defined the Width and Height fields of the bitmap before making the
Query.  This function is intelligent enough to fill out the fields based on the information you have given it, e.g. if
you set the #BytesPerPixel field to 2 then it will determine that the bitmap is a 16 bit, 64k colour bitmap.

*********************************************************************************************************************/

static ERROR BITMAP_Query(extBitmap *Self, APTR Void)
{
   pf::Log log;
   objDisplay *display;
   OBJECTID display_id;
   LONG i;

   log.msg(VLF::BRANCH|VLF::EXTAPI, "Bitmap: %p, Depth: %d, Width: %d, Height: %d", Self, Self->BitsPerPixel, Self->Width, Self->Height);

   if ((Self->Width <= 0) or (Self->Height <= 0)) {
      return log.warning(ERR_InvalidDimension);
   }

   #ifdef _GLES_
      if ((Self->DataFlags & MEM::TEXTURE) != MEM::NIL) {
         // OpenGL requires bitmap textures to be a power of 2.

         LONG new_width = nearestPower(Self->Width);
         LONG new_height = nearestPower(Self->Height);

         if (new_width != Self->Width) {
            log.msg("Extending bitmap width from %d to %d for OpenGL.", Self->Width, new_width);
            Self->Width = new_width;
         }

         if (new_height != Self->Height) {
            log.msg("Extending bitmap height from %d to %d for OpenGL.", Self->Height, new_height);
            Self->Height = new_height;
         }
      }
   #endif

   // If the BMF_MASK flag is set then the programmer wants to use the Bitmap object as a 1 or 8-bit mask.

   if (Self->Flags & BMF_MASK) {
      if ((!Self->BitsPerPixel) and (!Self->AmtColours)) {
         Self->BitsPerPixel = 1;
         Self->AmtColours = 2;
         Self->Type = BMP_PLANAR;
      }
      else if (Self->AmtColours >= 256) {
         Self->AmtColours = 256;
         Self->Type = BMP_CHUNKY;
         // Change the palette to grey scale for alpha channel masks
         for (i=0; i < 256; i++) {
            Self->Palette->Col[i].Red   = i;
            Self->Palette->Col[i].Green = i;
            Self->Palette->Col[i].Blue  = i;
         }
      }
      Self->BytesPerPixel = 1;
   }

   // If no type has been set, use the type that is native to the system that Parasol is running on.

   if (!Self->Type) Self->Type = BMP_CHUNKY;

   if (Self->BitsPerPixel) {
      switch(Self->BitsPerPixel) {
         case 1:  Self->BytesPerPixel = 1; Self->AmtColours = 2; Self->Type = BMP_PLANAR; break;
         case 2:  Self->BytesPerPixel = 1; Self->AmtColours = 4; break;
         case 8:  Self->BytesPerPixel = 1; Self->AmtColours = 256; break;
         case 15: Self->BytesPerPixel = 2; Self->AmtColours = 32768; break;
         case 16: Self->BytesPerPixel = 2; Self->AmtColours = 65536; break;
         case 24: Self->BytesPerPixel = 3; Self->AmtColours = 16777216; break;
         case 32: Self->BytesPerPixel = 4; Self->AmtColours = 16777216; break;
      }
   }
   else if (Self->BytesPerPixel) {
      switch(Self->BytesPerPixel) {
         case 1:  Self->BitsPerPixel  = 8;  Self->AmtColours = 256; break;
         case 2:  Self->BitsPerPixel  = 16; Self->AmtColours = 65536; break;
         case 3:  Self->BitsPerPixel  = 24; Self->AmtColours = 16777216; break;
         case 4:  Self->BitsPerPixel  = 32; Self->AmtColours = 16777216; break;
         default: Self->BytesPerPixel = 1;  Self->BitsPerPixel = 8; Self->AmtColours = 256;
      }
   }

   // Ensure values for BitsPerPixel, AmtColours, BytesPerPixel are correct

   if (!Self->AmtColours) {
      if (Self->BitsPerPixel) {
         if (Self->BitsPerPixel <= 24) {
            Self->AmtColours = 1<<Self->BitsPerPixel;
            if (Self->AmtColours <= 256) Self->BytesPerPixel = 1;
            else if (Self->AmtColours <= 65536) Self->BytesPerPixel = 2;
            else Self->BytesPerPixel = 3;
         }
         else {
            Self->AmtColours = 16777216;
            Self->BytesPerPixel = 4;
         }
      }
      else {
         Self->AmtColours    = 16777216;
         Self->BitsPerPixel  = 32;
         Self->BytesPerPixel = 4;
#if 1
         if (!FindObject("SystemDisplay", ID_DISPLAY, FOF::NIL, &display_id)) {
            if (!AccessObject(display_id, 3000, &display)) {
               Self->AmtColours    = display->Bitmap->AmtColours;
               Self->BytesPerPixel = display->Bitmap->BytesPerPixel;
               Self->BitsPerPixel  = display->Bitmap->BitsPerPixel;
               ReleaseObject(display);
            }
         }
#else
         DISPLAYINFO info;
         if (!get_display_info(0, &info)) {
            Self->AmtColours    = info.AmtColours;
            Self->BytesPerPixel = info.BytesPerPixel;
            Self->BitsPerPixel  = info.BitsPerPixel;
         }
#endif
      }
   }

   // Calculate ByteWidth, make sure it's word aligned

   if (Self->Type IS BMP_PLANAR) {
      Self->ByteWidth = (Self->Width + 7) / 8;
   }
   else Self->ByteWidth = Self->Width * Self->BytesPerPixel;

   // Initialise the line and plane module fields

   Self->LineWidth = Self->ByteWidth;
   Self->LineWidth = ALIGN32(Self->LineWidth);
   Self->PlaneMod = Self->LineWidth * Self->Height;

#ifdef __xwindows__

   // If we have Direct Graphics Access, use the DGA values rather than our generic calculations for bitmap parameters.

   if (((Self->DataFlags & MEM::VIDEO) != MEM::NIL) and (Self->x11.drawable)) {
      log.trace("LineWidth: %d, PixelLine: %d, BankSize: %d", Self->LineWidth, glDGAPixelsPerLine, glDGABankSize);
      if ((glDGAAvailable) and (glDGAPixelsPerLine)) {
         Self->LineWidth = glDGAPixelsPerLine * Self->BytesPerPixel;
         Self->PlaneMod = Self->LineWidth;
      }
   }

#endif

#ifdef _GLES_
   if ((Self->BitsPerPixel IS 8) and (Self->Flags & BMF_MASK)) Self->prvGLPixel = GL_ALPHA;
   else if (Self->BitsPerPixel <= 24) Self->prvGLPixel = GL_RGB;
   else Self->prvGLPixel = GL_RGBA;

   if (Self->BitsPerPixel IS 32) Self->prvGLFormat = GL_UNSIGNED_BYTE;
   else if (Self->BitsPerPixel IS 24) Self->prvGLFormat = GL_UNSIGNED_BYTE;
   else if (Self->BitsPerPixel <= 16) Self->prvGLFormat = GL_UNSIGNED_SHORT_5_6_5;
   else Self->prvGLFormat = GL_UNSIGNED_BYTE;
#endif

   // Calculate the total size of the bitmap

   if (Self->Type IS BMP_PLANAR) {
      Self->Size = Self->LineWidth * Self->Height * Self->BitsPerPixel;
   }
   else Self->Size = Self->LineWidth * Self->Height;

   Self->Flags |= BMF_QUERIED;
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Read: Reads raw image data from a bitmap object.
-END-
*********************************************************************************************************************/

static ERROR BITMAP_Read(extBitmap *Self, struct acRead *Args)
{
   if (!Self->Data) return ERR_NoData;
   if ((!Args) or (!Args->Buffer)) return ERR_NullArgs;

   LONG len = Args->Length;
   if (Self->Position + len > Self->Size) len = Self->Size - Self->Position;
   CopyMemory(Self->Data + Self->Position, Args->Buffer, len);
   Self->Position += len;
   Args->Result = len;
   return ERR_Okay;
}

/*********************************************************************************************************************

-ACTION-
Resize: Resizes a bitmap object's dimensions.

Resizing a bitmap will change its width, height and optionally bit depth.  Existing image data is not retained after
this process.

The image data is cleared with #BkgdRGB if the CLEAR flag is defined in #Flags.

-ERRORS-
Okay
NullArgs
AllocMemory
FieldNotSet

*********************************************************************************************************************/

static ERROR BITMAP_Resize(extBitmap *Self, struct acResize *Args)
{
   pf::Log log;
   LONG width, height, bytewidth, bpp, amtcolours, size;

   if (!Args) return log.warning(ERR_NullArgs);

   LONG origbpp    = Self->BitsPerPixel;

   if (Args->Width > 0) width = (LONG)Args->Width;
   else width = Self->Width;

   if (Args->Height > 0) height = (LONG)Args->Height;
   else height = Self->Height;

   if ((Args->Depth > 0) and (!(Self->Flags & BMF_FIXED_DEPTH))) bpp = (LONG)Args->Depth;
   else bpp = Self->BitsPerPixel;

   // If the NEVER_SHRINK option is set, the width and height may not be set to anything less than what is current.

   if (Self->Flags & BMF_NEVER_SHRINK) {
      if (width < Self->Width) width = Self->Width;
      if (height < Self->Height) height = Self->Height;
   }

   // Return if there is no change in the bitmap size

   if ((Self->Width IS width) and (Self->Height IS height) and (Self->BitsPerPixel IS bpp)) {
      return ERR_Okay|ERF_Notified;
   }

   // Calculate type-dependent values

   WORD bytesperpixel;
   switch(bpp) {
      case 1:  bytesperpixel = 1; amtcolours = 2; break;
      case 8:  bytesperpixel = 1; amtcolours = 256; break;
      case 15: bytesperpixel = 2; amtcolours = 32768; break;
      case 16: bytesperpixel = 2; amtcolours = 65536; break;
      case 24: bytesperpixel = 3; amtcolours = 16777216; break;
      case 32: bytesperpixel = 4; amtcolours = 16777216; break;
      default: bytesperpixel = bpp / 8;
               amtcolours = 1<<bpp;
   }

   if (Self->Type IS BMP_PLANAR) bytewidth = (width + (width % 16))/8;
   else bytewidth = width * bytesperpixel;

   LONG linewidth = ALIGN32(bytewidth);
   LONG planemod = bytewidth * height;

   if (Self->Type IS BMP_PLANAR) size = linewidth * height * bpp;
   else size = linewidth * height;

   if (GetClassID(Self->ownerID()) IS ID_DISPLAY) goto setfields;

#ifdef __xwindows__

   //if (Self->x11.drawable) {
   //   if ((drawable = XCreatePixmap(XDisplay, DefaultRootWindow(XDisplay), width, height, bpp))) {
   //      XCopyArea(XDisplay, Self->x11.drawable, drawable, glXGC, 0, 0, Self->Width, Self->Height, 0, 0);
   //      XFreePixmap(XDisplay, Self->x11.drawable);
   //      Self->x11.drawable = drawable;
   //   }
   //   else return log.warning(ERR_AllocMemory);
   //   goto setfields;
   //}

#elif _WIN32
   if (Self->prvAFlags & BF_WINVIDEO) return ERR_NoSupport;
#endif

   if (Self->Flags & BMF_NO_DATA);
   #ifdef __xwindows__
   else if (Self->x11.XShmImage);
   #endif
   else if ((Self->Data) and (Self->prvAFlags & BF_DATA)) {
      UBYTE *data;
      if ((size <= Self->Size) and (size / Self->Size > 0.5)) { // Do nothing when shrinking unless able to save considerable resources
         size = Self->Size;
      }
      else if (!AllocMemory(size, MEM::NO_BLOCKING|MEM::NO_POOL|Self->DataFlags|MEM::NO_CLEAR, &data)) {
         if (Self->Data) FreeResource(Self->Data);
         Self->Data = data;
      }
      else return log.warning(ERR_AllocMemory);
   }
   else return log.warning(ERR_UndefinedField);

setfields:
   Self->Width         = width;
   Self->Height        = height;
   Self->Size          = size;
   Self->BitsPerPixel  = bpp;
   Self->AmtColours    = amtcolours;
   Self->BytesPerPixel = bytesperpixel;
   Self->ByteWidth     = bytewidth;
   Self->LineWidth     = linewidth;
   Self->PlaneMod      = planemod;
   Self->Clip.Left      = 0;
   Self->Clip.Top       = 0;
   Self->Clip.Right     = width;
   Self->Clip.Bottom    = height;

#ifdef __xwindows__
   WORD alignment;
   if (Self->x11.XShmImage) {
      Self->x11.XShmImage = false; // Set to FALSE in case we fail (will drop through to standard XImage support)
      XShmDetach(XDisplay, &Self->x11.ShmInfo);  // Remove the previous attachment
      XSync(XDisplay, False);

      free_shm(Self->Data, Self->x11.ShmInfo.shmid);
      Self->Data = NULL;

      alloc_shm(size, &Self->Data, &Self->x11.ShmInfo.shmid);

      Self->x11.ShmInfo.readOnly = False;
      Self->x11.ShmInfo.shmaddr  = (char *)Self->Data;
      if (XShmAttach(XDisplay, &Self->x11.ShmInfo)) {
         if (Self->LineWidth & 0x0001) alignment = 8;
         else if (Self->LineWidth & 0x0002) alignment = 16;
         else alignment = 32;

         ClearMemory(&Self->x11.ximage, sizeof(Self->x11.ximage));

         Self->x11.ximage.width       = Self->Width;  // Image width
         Self->x11.ximage.height      = Self->Height; // Image height
         Self->x11.ximage.format      = ZPixmap;      // XYBitmap, XYPixmap, ZPixmap
         Self->x11.ximage.data        = (char *)Self->Data; // Pointer to image data
         Self->x11.ximage.obdata      = (char *)&Self->x11.ShmInfo;
         Self->x11.ximage.bitmap_unit = alignment;    // Quant. of scanline - 8, 16, 32
         Self->x11.ximage.bitmap_pad  = alignment;    // 8, 16, 32, either XY or Zpixmap
         if (Self->BitsPerPixel IS 32) Self->x11.ximage.depth = 24;
         else Self->x11.ximage.depth = Self->BitsPerPixel;          // Actual bits per pixel
         Self->x11.ximage.bytes_per_line = Self->LineWidth;         // Accelerator to next line
         Self->x11.ximage.bits_per_pixel = Self->BytesPerPixel * 8; // Bits per pixel-group

         XInitImage(&Self->x11.ximage);
         Self->x11.XShmImage = TRUE;
      }
   }

   if ((!Self->x11.drawable) and (Self->x11.XShmImage != TRUE)) {
      if (Self->LineWidth & 0x0001) alignment = 8;
      else if (Self->LineWidth & 0x0002) alignment = 16;
      else alignment = 32;

      ClearMemory(&Self->x11.ximage, sizeof(XImage));

      Self->x11.ximage.width       = Self->Width;  // Image width
      Self->x11.ximage.height      = Self->Height; // Image height
      Self->x11.ximage.format      = ZPixmap;      // XYBitmap, XYPixmap, ZPixmap
      Self->x11.ximage.data        = (char *)Self->Data; // Pointer to image data
      Self->x11.ximage.bitmap_unit = alignment;    // Quant. of scanline - 8, 16, 32
      Self->x11.ximage.bitmap_pad  = alignment;    // 8, 16, 32, either XY or Zpixmap
      if (Self->BitsPerPixel IS 32) Self->x11.ximage.depth = 24;
      else Self->x11.ximage.depth = Self->BitsPerPixel;      // Actual bits per pixel
      Self->x11.ximage.bytes_per_line = Self->LineWidth;         // Accelerator to next line
      Self->x11.ximage.bits_per_pixel = Self->BytesPerPixel * 8; // Bits per pixel-group

      XInitImage(&Self->x11.ximage);
   }

#endif

   if (origbpp != Self->BitsPerPixel) {
      gfxGetColourFormat(Self->ColourFormat, Self->BitsPerPixel, 0, 0, 0, 0);
   }

   CalculatePixelRoutines(Self);

   if (Self->Flags & BMF_CLEAR) {
      gfxDrawRectangle(Self, 0, 0, Self->Width, Self->Height, Self->getColour(Self->BkgdRGB), BAF_FILL);
   }

#ifdef __xwindows__
   XSync(XDisplay, False);
#endif
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
SaveImage: Saves a bitmap's image to a data object of your choosing in PCX format.
-END-
*********************************************************************************************************************/

static ERROR BITMAP_SaveImage(extBitmap *Self, struct acSaveImage *Args)
{
   pf::Log log;
   struct {
      BYTE Signature;
      BYTE Version;
      BYTE Encoding;
      BYTE BitsPixel;
      WORD XMin, YMin;
      WORD XMax, YMax;
      WORD XDPI, YDPI; // DPI
      UBYTE palette[48];
      BYTE Reserved;
      BYTE NumPlanes;
      WORD BytesLine;
      WORD PalType;
      WORD XRes;
      WORD YRes;
      UBYTE dummy[54];
   } pcx;
   RGB8 rgb;
   UBYTE *buffer, lastpixel, newpixel;
   LONG i, j, p, size;

   if ((!Args) or (!Args->Dest)) return log.warning(ERR_NullArgs);

   log.branch("Save To #%d", Args->Dest->UID);

   LONG width = Self->Clip.Right - Self->Clip.Left;
   LONG height = Self->Clip.Bottom - Self->Clip.Top;

   // Create PCX Header

   ClearMemory(&pcx, sizeof(pcx));
   pcx.Signature = 10;       // ZSoft PCX-files
   pcx.Version   = 5;        // Version
   pcx.Encoding  = 1;        // Run Length Encoding=ON
   pcx.XMin      = 0;
   pcx.YMin      = 0;
   pcx.BitsPixel = 8;
   pcx.BytesLine = width;
   pcx.XMax      = width - 1;
   pcx.YMax      = height - 1;
   pcx.XDPI      = 300;
   pcx.YDPI      = 300;
   pcx.PalType   = 1;
   pcx.XRes      = width;
   pcx.YRes      = height;
   if (Self->AmtColours <= 256) pcx.NumPlanes = 1;
   else pcx.NumPlanes = 3;

   size = width * height * pcx.NumPlanes;
   if (!AllocMemory(size, MEM::DATA|MEM::NO_CLEAR, &buffer)) {
      acWrite(Args->Dest, &pcx, sizeof(pcx), NULL);

      LONG dp = 0;
      for (i=Self->Clip.Top; i < (Self->Clip.Bottom); i++) {
         if (pcx.NumPlanes IS 1) { // Save as a 256 colour image
            lastpixel = Self->ReadUCPixel(Self, Self->Clip.Left, i);
            UBYTE counter = 1;
            for (j=Self->Clip.Left+1; j <= width; j++) {
               newpixel = Self->ReadUCPixel(Self, j, i);

               if ((newpixel IS lastpixel) and (j != width - 1) and (counter <= 62)) {
                  counter++;
               }
               else {
                  if (!((counter IS 1) and (lastpixel < 192))) {
                     buffer[dp++] = 192 + counter;
                  }
                  buffer[dp++] = lastpixel;
                  lastpixel = newpixel;
                  counter = 1;
               }

               if (dp >= (size - 10)) {
                  FreeResource(buffer);
                  return log.warning(ERR_BufferOverflow);
               }
            }
         }
         else { // Save as a true colour image with run-length encoding
            for (p=0; p < 3; p++) {
               Self->ReadUCRPixel(Self, Self->Clip.Left, i, &rgb);

               if (Self->ColourSpace IS CS_LINEAR_RGB) {
                  rgb.Red   = conv_l2r(rgb.Red);
                  rgb.Green = conv_l2r(rgb.Green);
                  rgb.Blue  = conv_l2r(rgb.Blue);
               }

               switch(p) {
                  case 0:  lastpixel = rgb.Red;   break;
                  case 1:  lastpixel = rgb.Green; break;
                  default: lastpixel = rgb.Blue;
               }
               UBYTE counter = 1;

               for (j=Self->Clip.Left+1; j < Self->Clip.Right; j++) {
                  Self->ReadUCRPixel(Self, j, i, &rgb);
                  switch(p) {
                     case 0:  newpixel = rgb.Red;   break;
                     case 1:  newpixel = rgb.Green; break;
                     default: newpixel = rgb.Blue;
                  }

                  if (newpixel IS lastpixel) {
                     counter++;
                     if (counter IS 63) {
                        buffer[dp++] = 0xc0 | counter;
                        buffer[dp++] = lastpixel;
                        counter = 0;
                     }
                  }
                  else {
                     if ((counter IS 1) and (0xc0 != (0xc0 & lastpixel))) {
                        buffer[dp++] = lastpixel;
                     }
                     else if (counter) {
                        buffer[dp++] = 0xc0 | counter;
                        buffer[dp++] = lastpixel;
                     }
                     lastpixel = newpixel;
                     counter = 1;
                  }
               }

               // Finish line if necessary

               if ((counter IS 1) and (0xc0 != (0xc0 & lastpixel))) {
                  buffer[dp++] = lastpixel;
               }
               else if (counter) {
                  buffer[dp++] = 0xc0 | counter;
                  buffer[dp++] = lastpixel;
               }
            }
         }
      }

      acWrite(Args->Dest, buffer, dp, NULL);
      FreeResource(buffer);

      // Setup palette

      if (Self->AmtColours <= 256) {
         UBYTE palette[(256 * 3) + 1];
         LONG j = 0;
         palette[j++] = 12;          // Palette identifier
         for (LONG i=0; i < 256; i++) {
            palette[j++] = Self->Palette->Col[i].Red;
            palette[j++] = Self->Palette->Col[i].Green;
            palette[j++] = Self->Palette->Col[i].Blue;
         }

         acWrite(Args->Dest, palette, sizeof(palette), NULL);
      }

      return ERR_Okay;

   }
   else return ERR_AllocMemory;
}

/*********************************************************************************************************************
-ACTION-
Seek: Changes the current byte position for read/write operations.

*********************************************************************************************************************/

static ERROR BITMAP_Seek(extBitmap *Self, struct acSeek *Args)
{
   if (Args->Position IS SEEK_START) Self->Position = (LONG)Args->Offset;
   else if (Args->Position IS SEEK_END) Self->Position = (LONG)(Self->Size - Args->Offset);
   else if (Args->Position IS SEEK_CURRENT) Self->Position = (LONG)(Self->Position + Args->Offset);
   else return ERR_Args;

   if (Self->Position > Self->Size) Self->Position = Self->Size;
   else if (Self->Position < 0) Self->Position = 0;

   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
SetClipRegion: Sets a clipping region for a bitmap object.

This method is a proxy for ~Display.SetClipRegion().

-INPUT-
int Number:    The number of the clip region to set.
int Left:      The horizontal start of the clip region.
int Top:       The vertical start of the clip region.
int Right:     The right-most edge of the clip region.
int Bottom:    The bottom-most edge of the clip region.
int Terminate: Set to TRUE if this is the last clip region in the list, otherwise FALSE.

-ERRORS-
Okay
NullArgs

*********************************************************************************************************************/

static ERROR BITMAP_SetClipRegion(extBitmap *Self, struct bmpSetClipRegion *Args)
{
   if (!Args) return ERR_NullArgs;

   gfxSetClipRegion(Self, Args->Number, Args->Left, Args->Top, Args->Right, Args->Bottom, Args->Terminate);
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Unlock: Unlocks the bitmap surface once direct access is no longer required.

*********************************************************************************************************************/

static ERROR BITMAP_Unlock(extBitmap *Self, APTR Void)
{
#ifndef __xwindows__
   unlock_surface(Self);
#endif
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Write: Writes raw image data to a bitmap object.
-END-
*********************************************************************************************************************/

static ERROR BITMAP_Write(extBitmap *Self, struct acWrite *Args)
{
   if (Self->Data) {
      auto Data = (BYTE *)Self->Data + Self->Position;
      LONG amt_bytes = 0;
      while (Args->Length > 0) {
         Data[amt_bytes] = ((UBYTE *)Args->Buffer)[amt_bytes];
         Args->Length--;
         amt_bytes++;
      }
      Self->Position += amt_bytes;
      return ERR_Okay;
   }
   else return ERR_NoData;
}

/*********************************************************************************************************************

-FIELD-
AmtColours: The maximum number of displayable colours.

-FIELD-
BitsPerPixel: The number of bits per pixel

The BitsPerPixel field clarifies exactly how many bits are being used to manage each pixel on the display.  This
includes any 'special' bits that are in use, e.g. alpha-channel bits.

-FIELD-
Bkgd: The bitmap's background colour is defined here in RGB format.

The default background colour for a bitmap is black.  To change it, set this field with the new RGB colour.  The
background colour is used in operations that require a default colour, such as when clearing the bitmap.

The #BkgdIndex will be updated as a result of setting this field.

*********************************************************************************************************************/

static ERROR SET_Bkgd(extBitmap *Self, RGB8 *Value)
{
   Self->BkgdRGB = *Value;

   if (Self->BitsPerPixel > 8) {
      Self->BkgdIndex = (((Self->BkgdRGB.Red   >>Self->prvColourFormat.RedShift)   & Self->prvColourFormat.RedMask)   << Self->prvColourFormat.RedPos) |
                         (((Self->BkgdRGB.Green>>Self->prvColourFormat.GreenShift) & Self->prvColourFormat.GreenMask) << Self->prvColourFormat.GreenPos) |
                         (((Self->BkgdRGB.Blue >>Self->prvColourFormat.BlueShift)  & Self->prvColourFormat.BlueMask)  << Self->prvColourFormat.BluePos) |
                         (((Self->BkgdRGB.Alpha>>Self->prvColourFormat.AlphaShift) & Self->prvColourFormat.AlphaMask) << Self->prvColourFormat.AlphaPos);
   }
   else Self->BkgdIndex = RGBToValue(&Self->BkgdRGB, Self->Palette);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
BkgdIndex: The bitmap's background colour is defined here as a colour index.

The bitmap's background colour is defined in this field as a colour index.  It is recommended that the #Bkgd
field is used for altering the bitmap background unless efficiency requires that the colour index is calculated and set
directly.

*********************************************************************************************************************/

static ERROR SET_BkgdIndex(extBitmap *Self, LONG Index)
{
   if ((Index < 0) or (Index > 255)) return ERR_OutOfRange;
   Self->BkgdIndex = Index;
   Self->BkgdRGB   = Self->Palette->Col[Self->BkgdIndex];
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
BytesPerPixel: The number of bytes per pixel.

If you need to find out how many bytes are involved in the makeup of each pixel you will need to read this field.  The
maximum number of bytes you can typically expect is 4 and the minimum is 1.  If a planar bitmap is being used then you
should refer to the #BitsPerPixel field, which should yield more useful information.

-FIELD-
ByteWidth: The width of the bitmap, in bytes.

The ByteWidth of the bitmap is calculated directly from the bitmap's #Width and #Type settings. Under no circumstances
should you attempt to calculate this value in advance, as it is heavily dependent on the bitmap's #Type.

The formulas used to calculate the value of this field are:

<pre>
Planar      = Width/8
Chunky/8    = Width
Chunky/15   = Width * 2
Chunky/16   = Width * 2
Chunky/24   = Width * 3
Chunky/32   = Width * 4
</pre>

If you would like to know the total byte width per line including any padded bytes that may lie at the end of each line,
please refer to the #LineMod field.

-FIELD-
ClipBottom: The bottom-most edge of  bitmap's clipping region.

During the initialisation of a bitmap, a default clipping region will be created that matches the bitmap's dimensions.
Clipping regions define the area under which graphics can be drawn to a bitmap.  This particular field reflects the
bottom-most edge of all clipping regions that have been set or altered through the #SetClipRegion() method.

-FIELD-
ClipLeft: The left-most edge of a bitmap's clipping region.

During the initialisation of a bitmap, a default clipping region will be created that matches the bitmap's dimensions.
Clipping regions define the area under which graphics can be drawn to a bitmap.  This particular field reflects the
left-most edge of all clipping regions that have been set or altered through the #SetClipRegion() method.

-FIELD-
ClipRight: The right-most edge of a bitmap's clipping region.

During the initialisation of a bitmap, a default clipping region will be created that matches the bitmap's dimensions.
Clipping regions define the area under which graphics can be drawn to a bitmap.  This particular field reflects the
right-most edge of all clipping regions that have been set or altered through the #SetClipRegion() method.

-FIELD-
ClipTop: The top-most edge of a bitmap's clipping region.

During the initialisation of a bitmap, a default clipping region will be created that matches the bitmap's dimensions.
Clipping regions define the area under which graphics can be drawn to a bitmap.  This particular field reflects the
top-most edge of all clipping regions that have been set or altered through the #SetClipRegion() method.

-FIELD-
Clip: Defines the bitmap's clipping region.

*********************************************************************************************************************/

static ERROR GET_Clip(extBitmap *Self, ClipRectangle **Value)
{
   *Value = &Self->Clip;
   return ERR_Okay;
}

static ERROR SET_Clip(extBitmap *Self, ClipRectangle *Value)
{
   Self->Clip = *Value;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
ColourFormat: Describes the colour format used to construct each bitmap pixel.

The ColourFormat field points to a structure that defines the colour format used to construct each bitmap pixel.  It
only applies to bitmaps that use 2-bytes per colour value or better.  The structure consists of the following fields:

<struct lookup="ColourFormat">
<field type="UBYTE" name="RedShift">Right shift value for the red component (applies only to 15/16 bit formats for eliminating redundant bits).</>
<field type="UBYTE" name="BlueShift">Right shift value for the blue component.</>
<field type="UBYTE" name="GreenShift">Right shift value for the green component.</>
<field type="UBYTE" name="AlphaShift">Right shift value for the alpha component.</>
<field type="UBYTE" name="RedMask">The unshifted mask value for the red component (ranges from 0x00 to 0xff).</>
<field type="UBYTE" name="GreenMask">The unshifted mask value for the green component.</>
<field type="UBYTE" name="BlueMask">The unshifted mask value for the blue component.</>
<field type="UBYTE" name="AlphaMask">The unshifted mask value for the alpha component.</>
<field type="UBYTE" name="RedPos">Left shift/positional value for the red component.</>
<field type="UBYTE" name="GreenPos">Left shift/positional value for the green component.</>
<field type="UBYTE" name="BluePos">Left shift/positional value for the blue component.</>
<field type="UBYTE" name="AlphaPos">Left shift/positional value for the alpha component.</>
<field type="UBYTE" name="BitsPerPixel"> Number of bits per pixel for this format.</>
</>

The following C++ methods can called on any bitmap in order to build colour values from individual RGB components:

<pre>
packPixel(Red, Green, Blue)
packPixel(Red, Green, Blue, Alpha)
packAlpha(Alpha)
packPixelRGB(RGB8 &RGB)
packPixelRGBA(RGB8 &RGB)
</pre>

The following C macros are optimised versions of the above that are limited to 24 and 32-bit bitmaps:

<pre>
PackPixelWB(Red, Green, Blue)
PackPixelWBA(Red, Green, Blue, Alpha)
</pre>

The following C++ methods can be used to unpack individual colour components from any colour value read from the bitmap:

<pre>
unpackRed(Colour)
unpackGreen(Colour)
unpackBlue(Colour)
unpackAlpha(Colour)
</pre>

-FIELD-
Data: Pointer to a bitmap's data area.

This field points directly to the start of a bitmap's data area.  Allocating your own bitmap memory is acceptable
if creating a bitmap that is not based on video memory.  However, it is usually a better idea for the
initialisation process to allocate the correct amount of memory for you by not interfering with this field.

*********************************************************************************************************************/

ERROR SET_Data(extBitmap *Self, UBYTE *Value)
{
#ifdef __xwindows__
   if (Self->x11.XShmImage) return ERR_NotPossible;
#endif

   // This code gets the correct memory flags to define the pixel drawing functions
   // (i.e. functions to draw to video memory are different to drawing to normal memory).

   if (Self->Data != Value) {
      Self->Data = Value;

      if (Self->DataFlags IS MEM::NIL) {
         MemInfo info;
         if (MemoryPtrInfo(Value, &info) != ERR_Okay) {
            pf::Log log;
            log.warning("Could not obtain flags from address %p.", Value);
         }
         else if (Self->DataFlags != info.Flags) {
            Self->DataFlags = info.Flags;
            if (Self->initialised()) CalculatePixelRoutines(Self);
         }
      }
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
DataFlags: Defines the memory flags to use in allocating a bitmap's data area.

This field determines the type of memory that will be allocated for the #Data field during the initialisation process.
This field accepts the `MEM::DATA`, `MEM::VIDEO` and `MEM::TEXTURE` memory flags.

Please note that video based bitmaps may be faster than data bitmaps for certain applications, but the content is typically
read-only.  Under normal circumstances it is not possible to use the pixel reading functions, or read from the
bitmap #Data field directly with these bitmap types.  To circumvent this problem please use the #Lock() action
to enable read access when you require it.

-FIELD-
DrawUCPixel: Points to a C function that draws pixels to the bitmap using colour indexes.

This field points to an internal C function that can be used for drawing pixels to the bitmap.  It is intended that the
function is only ever called by C programs and that caution is exercised by the programmer, as no clipping checks will
be performed (meaning it is possible to supply invalid coordinates that would result in a segfault).

The prototype of the DrawUCPixel function is `Function(*Bitmap, LONG X, LONG Y, ULONG Colour)`.

The new pixel value must be defined in the Colour parameter.

-FIELD-
DrawUCRIndex: Points to a C function that draws pixels to the bitmap in RGB format.

This field points to an internal C function that can be used for drawing pixels to the bitmap.  It is intended that
the function is only ever called by C programs and that caution is exercised by the programmer, as no clipping checks
will be performed (meaning it is possible to supply an invalid address that would result in a segfault).

The prototype of the DrawUCRIndex function is `Function(*Bitmap, UBYTE *Data, RGB8 *RGB)`.

The Data parameter must point to a location within the Bitmap's graphical address space. The new pixel value must be
defined in the RGB parameter.

Note that a colour indexing equivalent of this function is not available in the Bitmap class - this is because it is
more efficient to index the Bitmap's #Data field directly.

-FIELD-
DrawUCRPixel: Points to a C function that draws pixels to the bitmap in RGB format.

This field points to an internal C function that can be used for drawing pixels to the bitmap.  It is intended that the
function is only ever called by C programs and that caution is exercised by the programmer, as no clipping checks will
be performed (meaning it is possible to supply invalid coordinates that would result in a segfault).

The prototype of the DrawUCRPixel function is `Function(*Bitmap, LONG X, LONG Y, RGB8 *RGB)`.

The new pixel value must be defined in the RGB parameter.

-FIELD-
Flags: Optional flags.

-FIELD-
Handle: Private. Platform dependent field for referencing video memory.
-END-

*********************************************************************************************************************/

static ERROR GET_Handle(extBitmap *Self, APTR *Value)
{
#ifdef _WIN32
   *Value = (APTR)Self->win.Drawable;
   return ERR_Okay;
#elif __xwindows__
   *Value = (APTR)Self->x11.drawable;
   return ERR_Okay;
#else
   return ERR_NoSupport;
#endif
}

static ERROR SET_Handle(extBitmap *Self, APTR Value)
{
   // Note: The only area of the system allowed to set this field are the Display/Surface classes for video management.

#ifdef _WIN32
   Self->win.Drawable = Value;
   return ERR_Okay;
#elif __xwindows__
   Self->x11.drawable = (MAXINT)Value;
   return ERR_Okay;
#else
   return ERR_NoSupport;
#endif
}

/*********************************************************************************************************************

-FIELD-
Height: The height of the bitmap, in pixels.

-FIELD-
LineMod: The length of each bitmap line in bytes, including alignment.

-FIELD-
Opacity: Determines the translucency setting to use in drawing operations.

Some drawing operations support the concept of applying an opacity rating to create translucent graphics.  By adjusting
the opacity rating, you can affect the level of translucency that is applied when executing certain graphics operations.

Methods that support opacity should document the fact that they support the feature.  By default the opacity rating is
set to 255 to turn off translucency effects.  Lowering the value will increase the level of translucency when drawing
graphics.

-FIELD-
Palette: Points to a bitmap's colour palette.

A palette is an array of containing colour values in standard RGB format `$RRGGBB`.  The first value must have a
header ID of `ID_PALETTE`, followed by the amount of values in the array. Following this is the actual list itself -
colour 0, then colour 1 and so on. There is no termination signal at the end of the list.

The following example is for a 32 colour palette:

<pre>
RGBPalette Palette = {
  ID_PALETTE, VER_PALETTE, 32,
  {{ 0x00,0x00,0x00 }, { 0x10,0x10,0x10 }, { 0x17,0x17,0x17 }, { 0x20,0x20,0x20 },
   { 0x27,0x27,0x27 }, { 0x30,0x30,0x30 }, { 0x37,0x37,0x37 }, { 0x40,0x40,0x40 },
   { 0x47,0x47,0x47 }, { 0x50,0x50,0x50 }, { 0x57,0x57,0x57 }, { 0x60,0x60,0x60 },
   { 0x67,0x67,0x67 }, { 0x70,0x70,0x70 }, { 0x77,0x77,0x77 }, { 0x80,0x80,0x80 },
   { 0x87,0x87,0x87 }, { 0x90,0x90,0x90 }, { 0x97,0x97,0x97 }, { 0xa0,0xa0,0xa0 },
   { 0xa7,0xa7,0xa7 }, { 0xb0,0xb0,0xb0 }, { 0xb7,0xb7,0xb7 }, { 0xc0,0xc0,0xc0 },
   { 0xc7,0xc7,0xc7 }, { 0xd0,0xd0,0xd0 }, { 0xd7,0xd7,0xd7 }, { 0xe0,0xe0,0xe0 },
   { 0xe0,0xe0,0xe0 }, { 0xf0,0xf0,0xf0 }, { 0xf7,0xf7,0xf7 }, { 0xff,0xff,0xff }
   }
};
</pre>

Palettes are created for all bitmap types, including RGB based bitmaps above 8-bit colour.  This is because a number of
drawing functions require a palette table for conversion between the bitmap types.

Although the array is dynamic, parent objects such as the Display need to be notified if you want a palette's colours
to be propagated to the video display.

*********************************************************************************************************************/

ERROR SET_Palette(extBitmap *Self, RGBPalette *SrcPalette)
{
   pf::Log log;

   // The objective here is to copy the given source palette to the bitmap's palette.  To see how the hook is set up,
   // refer to the bitmap's object definition structure that is compiled into the module.

   if (!SrcPalette) return ERR_Okay;

   if (SrcPalette->AmtColours <= 256) {
      if (!Self->Palette) {
         if (AllocMemory(sizeof(RGBPalette), MEM::NO_CLEAR, &Self->Palette) != ERR_Okay) {
            log.warning(ERR_AllocMemory);
         }
      }

      Self->Palette->AmtColours = SrcPalette->AmtColours;
      WORD i = SrcPalette->AmtColours-1;
      while (i > 0) {
         Self->Palette->Col[i] = SrcPalette->Col[i];
         i--;
      }
      return ERR_Okay;
   }
   else {
      log.warning("Corruption in Palette at %p.", SrcPalette);
      return ERR_ObjectCorrupt;
   }
}

/*********************************************************************************************************************

-FIELD-
PlaneMod: The differential between each bitmap plane.

This field specifies the distance (in bytes) between each bitplane.  For non-planar types like CHUNKY, this field will
actually reflect the total size of the bitmap.  The calculation used for PLANAR types is `ByteWidth * Height`.

-FIELD-
Position: The current read/write data position.

This field reflects the current byte position for reading and writing raw data to and from a bitmap object.  If you
need to change the current byte position, use the Seek action.

-FIELD-
ReadUCRIndex: Points to a C function that reads pixels from the bitmap in RGB format.

This field points to an internal C function that can be used for reading pixels from the bitmap.  It is intended that
the function is only ever called by C programs and that caution is exercised by the programmer, as no clipping checks
will be performed (meaning it is possible to supply an invalid address that would result in a segfault).

The prototype of the ReadUCRIndex function is `Function(*Bitmap, UBYTE *Data, RGB8 *RGB)`.

The Data parameter must point to a location within the Bitmap's graphical address space. The pixel value will be
returned in the RGB parameter.

Note that a colour indexing equivalent of this function is not available in the Bitmap class - this is because it is
more efficient to index the Bitmap's #Data field directly.

-FIELD-
ReadUCPixel: Points to a C function that reads pixels from the bitmap in colour index format.

This field points to an internal C function that can be used for reading pixels from the bitmap.  It is intended that
the function is only ever called by C programs and that caution is exercised by the programmer, as no clipping checks
will be performed (meaning it is possible to supply invalid X/Y coordinates that would result in a segfault).

The prototype of the ReadUCPixel function is `Function(*Bitmap, LONG X, LONG Y, LONG *Index)`.

The pixel value will be returned in the Index parameter.

-FIELD-
ReadUCRPixel: Points to a C function that reads pixels from the bitmap in RGB format.

This field points to an internal C function that can be used for reading pixels from the bitmap.  It is intended that
the function is only ever called by C programs and that caution is exercised by the programmer, as no clipping checks
will be performed (meaning it is possible to supply invalid X/Y coordinates that would result in a segfault).

The prototype of the ReadUCRPixel function is `Function(*Bitmap, LONG X, LONG Y, RGB8 *RGB)`.

The pixel value will be returned in the RGB parameter.  It should be noted that as this function converts the pixel
value into RGB format, #ReadUCPixel or #ReadUCRIndex should be used as faster alternatives if the
pixel value does not need to be de-constructed into its RGB components.

-FIELD-
Size: The total size of the bitmap, in bytes.

-FIELD-
TransRGB: The transparent colour of the bitmap, in RGB format.

The transparent colour of the bitmap is defined here.  Colours in the bitmap that match this value will not be copied
during drawing operations.

NOTE: This field should never be set if the bitmap utilises alpha transparency.

*********************************************************************************************************************/

static ERROR SET_Trans(extBitmap *Self, RGB8 *Value)
{
   Self->TransRGB = *Value;

   if (Self->BitsPerPixel > 8) {
      Self->TransIndex = (((Self->TransRGB.Red  >>Self->prvColourFormat.RedShift)   & Self->prvColourFormat.RedMask)   << Self->prvColourFormat.RedPos) |
                         (((Self->TransRGB.Green>>Self->prvColourFormat.GreenShift) & Self->prvColourFormat.GreenMask) << Self->prvColourFormat.GreenPos) |
                         (((Self->TransRGB.Blue >>Self->prvColourFormat.BlueShift)  & Self->prvColourFormat.BlueMask)  << Self->prvColourFormat.BluePos) |
                         (((Self->TransRGB.Alpha>>Self->prvColourFormat.AlphaShift) & Self->prvColourFormat.AlphaMask) << Self->prvColourFormat.AlphaPos);
   }
   else Self->TransIndex = RGBToValue(&Self->TransRGB, Self->Palette);

   if ((Self->DataFlags & MEM::VIDEO) IS MEM::NIL) Self->Flags |= BMF_TRANSPARENT;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
TransIndex: The transparent colour of the bitmap, represented as an index.

The transparent colour of the bitmap is defined here.  Colours in the bitmap that match this value will not be copied
during graphics operations.  It is recommended that the #TransRGB field is used for altering the bitmap
transparency unless efficiency requires that the transparency is set directly.

NOTE: This field should never be set if the bitmap utilises alpha transparency.

*********************************************************************************************************************/

static ERROR SET_TransIndex(extBitmap *Self, LONG Index)
{
   if ((Index < 0) or (Index > 255)) return ERR_OutOfRange;

   Self->TransIndex = Index;
   Self->TransRGB   = Self->Palette->Col[Self->TransIndex];

   if ((Self->DataFlags & MEM::VIDEO) IS MEM::NIL) Self->Flags |= BMF_TRANSPARENT;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Type: Defines the data type of the bitmap.

This field defines the graphics data type - either PLANAR (required for 1-bit bitmaps) or CHUNKY (the default).

-FIELD-
Width: The width of the bitmap, in pixels.

-FIELD-
XOffset: Private. Provided for surface/video drawing purposes - considered too advanced for standard use.

-FIELD-
YOffset: Private. Provided for surface/video drawing purposes - considered too advanced for standard use.
-END-

*********************************************************************************************************************/

//********************************************************************************************************************

static ERROR CalculatePixelRoutines(extBitmap *Self)
{
   pf::Log log;

   if (Self->Type IS BMP_PLANAR) {
      Self->ReadUCPixel  = MemReadPixelPlanar;
      Self->ReadUCRPixel = MemReadRGBPixelPlanar;
      Self->ReadUCRIndex = MemReadRGBIndexPlanar;
      Self->DrawUCPixel  = MemDrawPixelPlanar;
      Self->DrawUCRPixel = DrawRGBPixelPlanar;
      Self->DrawUCRIndex = NULL;
      return ERR_Okay;
   }

   if (Self->Type != BMP_CHUNKY) {
      log.warning("Unsupported Bitmap->Type %d.", Self->Type);
      return ERR_Failed;
   }

#ifdef _WIN32

   if (Self->prvAFlags & BF_WINVIDEO) {
      Self->ReadUCPixel  = &VideoReadPixel;
      Self->ReadUCRPixel = &VideoReadRGBPixel;
      Self->ReadUCRIndex = &VideoReadRGBIndex;
      Self->DrawUCPixel  = &VideoDrawPixel;
      Self->DrawUCRPixel = &VideoDrawRGBPixel;
      Self->DrawUCRIndex = &VideoDrawRGBIndex;
      return ERR_Okay;
   }

#else

   if ((Self->DataFlags & (MEM::VIDEO|MEM::TEXTURE)) != MEM::NIL) {
      switch(Self->BytesPerPixel) {
         case 1:
            Self->ReadUCPixel  = &VideoReadPixel8;
            Self->ReadUCRPixel = &VideoReadRGBPixel8;
            Self->ReadUCRIndex = &VideoReadRGBIndex8;
            Self->DrawUCPixel  = &VideoDrawPixel8;
            Self->DrawUCRPixel = &VideoDrawRGBPixel8;
            Self->DrawUCRIndex = &VideoDrawRGBIndex8;
            break;

         case 2:
            Self->ReadUCPixel  = &VideoReadPixel16;
            Self->ReadUCRPixel = &VideoReadRGBPixel16;
            Self->ReadUCRIndex = (void (*)(objBitmap *, UBYTE *, RGB8 *))&VideoReadRGBIndex16;
            Self->DrawUCPixel  = &VideoDrawPixel16;
            Self->DrawUCRPixel = &VideoDrawRGBPixel16;
            Self->DrawUCRIndex = (void (*)(objBitmap *, UBYTE *, RGB8 *))&VideoDrawRGBIndex16;
            break;

         case 3:
            Self->ReadUCPixel  = &VideoReadPixel24;
            Self->ReadUCRPixel = &VideoReadRGBPixel24;
            Self->ReadUCRIndex = &VideoReadRGBIndex24;
            Self->DrawUCPixel  = &VideoDrawPixel24;
            Self->DrawUCRPixel = &VideoDrawRGBPixel24;
            Self->DrawUCRIndex = &VideoDrawRGBIndex24;
            break;

         case 4:
            Self->ReadUCPixel  = &VideoReadPixel32;
            Self->ReadUCRPixel = &VideoReadRGBPixel32;
            Self->ReadUCRIndex = (void (*)(objBitmap *, UBYTE *, RGB8 *))&VideoReadRGBIndex32;
            Self->DrawUCPixel  = &VideoDrawPixel32;
            Self->DrawUCRPixel = &VideoDrawRGBPixel32;
            Self->DrawUCRIndex = (void (*)(objBitmap *, UBYTE *, RGB8 *))&VideoDrawRGBIndex32;
            break;

         default:
            log.warning("Unsupported Bitmap->BytesPerPixel %d.", Self->BytesPerPixel);
            return ERR_Failed;
      }
      return ERR_Okay;
   }
#endif

   switch(Self->BytesPerPixel) {
      case 1:
        Self->ReadUCPixel  = MemReadPixel8;
        Self->ReadUCRPixel = MemReadRGBPixel8;
        Self->ReadUCRIndex = MemReadRGBIndex8;
        Self->DrawUCPixel  = MemDrawPixel8;
        Self->DrawUCRPixel = MemDrawRGBPixel8;
        Self->DrawUCRIndex = MemDrawRGBIndex8;
        break;

      case 2:
         Self->ReadUCPixel  = MemReadPixel16;
         Self->ReadUCRPixel = MemReadRGBPixel16;
         Self->ReadUCRIndex = (void (*)(objBitmap *, UBYTE *, RGB8 *))MemReadRGBIndex16;
         Self->DrawUCPixel  = MemDrawPixel16;
         Self->DrawUCRPixel = MemDrawRGBPixel16;
         Self->DrawUCRIndex = (void (*)(objBitmap *, UBYTE *, RGB8 *))MemDrawRGBIndex16;
         break;

      case 3:
         if (Self->prvColourFormat.RedPos IS 16) {
            Self->ReadUCPixel  = MemReadLSBPixel24;
            Self->ReadUCRPixel = MemReadLSBRGBPixel24;
            Self->ReadUCRIndex = MemReadLSBRGBIndex24;
            Self->DrawUCPixel  = MemDrawLSBPixel24;
            Self->DrawUCRPixel = MemDrawLSBRGBPixel24;
            Self->DrawUCRIndex = MemDrawLSBRGBIndex24;
         }
         else {
            Self->ReadUCPixel  = MemReadMSBPixel24;
            Self->ReadUCRPixel = MemReadMSBRGBPixel24;
            Self->ReadUCRIndex = MemReadMSBRGBIndex24;
            Self->DrawUCPixel  = MemDrawMSBPixel24;
            Self->DrawUCRPixel = MemDrawMSBRGBPixel24;
            Self->DrawUCRIndex = MemDrawMSBRGBIndex24;
         }
         break;

      case 4:
         Self->ReadUCPixel  = MemReadPixel32;
         Self->ReadUCRPixel = MemReadRGBPixel32;
         Self->ReadUCRIndex = (void (*)(objBitmap *, UBYTE *, RGB8 *))MemReadRGBIndex32;
         Self->DrawUCPixel  = MemDrawPixel32;
         Self->DrawUCRPixel = MemDrawRGBPixel32;
         Self->DrawUCRIndex = (void (*)(objBitmap *, UBYTE *, RGB8 *))MemDrawRGBIndex32;
         break;

      default:
        log.warning("Unsupported Bitmap->BytesPerPixel %d.", Self->BytesPerPixel);
        return ERR_Failed;
   }

   return ERR_Okay;
}

//********************************************************************************************************************

#include "lib_mempixels.cpp"

#ifdef __xwindows__
#include "x11/lib_pixels.cpp"
#endif

#ifdef _WIN32
#include "win32/lib_pixels.cpp"
#endif

#ifdef __ANDROID__
#include "android/lib_pixels.cpp"
#endif

#include "class_bitmap_def.c"

static const FieldArray clBitmapFields[] = {
   { "Palette",       FDF_POINTER|FDF_RW, NULL, SET_Palette },
   { "ColourFormat",  FDF_POINTER|FDF_STRUCT|FDF_R, NULL, NULL, "ColourFormat" },
   { "DrawUCPixel",   FDF_POINTER|FDF_R, NULL, NULL, &argsDrawUCPixel },
   { "DrawUCRPixel",  FDF_POINTER|FDF_R, NULL, NULL, &argsDrawUCRPixel },
   { "ReadUCPixel",   FDF_POINTER|FDF_R, NULL, NULL, &argsReadUCPixel },
   { "ReadUCRPixel",  FDF_POINTER|FDF_R, NULL, NULL, &argsReadUCRPixel },
   { "ReadUCRIndex",  FDF_POINTER|FDF_R, NULL, NULL, &argsReadUCRIndex },
   { "DrawUCRIndex",  FDF_POINTER|FDF_R, NULL, NULL, &argsDrawUCRIndex },
   { "Data",          FDF_POINTER|FDF_RI, NULL, SET_Data },
   { "Width",         FDF_LONG|FDF_RI, NULL, NULL },
   { "ByteWidth",     FDF_LONG|FDF_R, NULL, NULL },
   { "Height",        FDF_LONG|FDF_RI, NULL, NULL },
   { "Type",          FDF_LONG|FDF_RI|FDF_LOOKUP, NULL, NULL, &clBitmapType },
   { "LineMod",       FDF_LONG|FDF_R },
   { "PlaneMod",      FDF_LONG|FDF_R },
   { "ClipLeft",      FDF_LONG|FDF_RW },
   { "ClipRight",     FDF_LONG|FDF_RW },
   { "ClipBottom",    FDF_LONG|FDF_RW },
   { "ClipTop",       FDF_LONG|FDF_RW },
   { "Size",          FDF_LONG|FDF_R },
   { "DataFlags",     FDF_LONGFLAGS|FDF_RI, NULL, NULL, &clDataFlags },
   { "AmtColours",    FDF_LONG|FDF_RI },
   { "Flags",         FDF_LONGFLAGS|FDF_RI, NULL, NULL, &clBitmapFlags },
   { "TransIndex",    FDF_LONG|FDF_RW, NULL, SET_TransIndex },
   { "BytesPerPixel", FDF_LONG|FDF_RI },
   { "BitsPerPixel",  FDF_LONG|FDF_RI },
   { "Position",      FDF_LONG|FDF_R },
   { "XOffset",       FDF_LONG|FDF_SYSTEM|FDF_RW },
   { "YOffset",       FDF_LONG|FDF_SYSTEM|FDF_RW },
   { "Opacity",       FDF_LONG|FDF_RW },
   { "DataID",        FDF_LONG|FDF_SYSTEM|FDF_R },
   { "TransRGB",      FDF_RGB|FDF_RW, NULL, SET_Trans },
   { "Bkgd",          FDF_RGB|FDF_RW, NULL, SET_Bkgd },
   { "BkgdIndex",     FDF_LONG|FDF_RW, NULL, SET_BkgdIndex },
   { "ColourSpace",   FDF_LONGFLAGS|FDF_RW, NULL, NULL, &clBitmapColourSpace },
   // Virtual fields
   { "Clip",          FDF_POINTER|FDF_STRUCT|FDF_RW, GET_Clip, SET_Clip },
   { "Handle",        FDF_POINTER|FDF_SYSTEM|FDF_RW, GET_Handle, SET_Handle },
   END_FIELD
};

//********************************************************************************************************************

ERROR create_bitmap_class(void)
{
   clBitmap = objMetaClass::create::global(
      fl::ClassVersion(VER_BITMAP),
      fl::Name("Bitmap"),
      fl::Category(CCF::GRAPHICS),
      fl::Actions(clBitmapActions),
      fl::Methods(clBitmapMethods),
      fl::Fields(clBitmapFields),
      fl::Size(sizeof(extBitmap)),
      fl::Path(MOD_PATH));

   return clBitmap ? ERR_Okay : ERR_AddClass;
}

