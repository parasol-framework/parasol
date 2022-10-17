/*****************************************************************************

This source code and its accompanying files are in the public domain and therefore may be distributed without
restriction.  The source is based in part on libpng, authored by Glenn Randers-Pehrson, Andreas Eric Dilger and
Guy Eric Schalnat.

******************************************************************************

-CLASS-
Picture: Loads and saves picture files in a variety of different data formats.

The purpose of the Picture class is to provide a standard interface that any program can use to load picture files.
The main advantage in using this class is that the source picture can be in any file format recognised by the user's
system, so effectively your program will be able to support file formats that it does not understand.

The default file format for loading and saving pictures is PNG.  Other formats such as JPEG are supported via
sub-classes, which can be loaded into the system at boot time or on demand.  Some rare formats such as TIFF are
also supported, but these can be removed from the system library folders if they are not required by the host system
or application, in order to save space.

<header>Technical Notes</>

The Picture class will clip any loaded picture so that it fits the size given in the #Bitmap's Width and
Height. If you specify the `RESIZE` flag, the picture will be shrunk or enlarged to fit the given dimensions.
If you leave the Width and Height at NULL, then the picture will be loaded at its default dimensions.  To
find out general information about a picture before initialising it, <action>Query</action> it first so that
the picture object can load initial details on the file format.

Images are also remapped automatically if the source palette and destination palettes do not match, or if there are
significant differences between the source and destination bitmap types.

-END-

*****************************************************************************/

#define PNG_INTERNAL
#define PRV_PNG
#include "lib/png.h"
#include "lib/pngpriv.h"

#include "picture.h"
#include <parasol/main.h>
#include <parasol/modules/picture.h>
#include <parasol/modules/display.h>
#include <parasol/linear_rgb.h>

MODULE_COREBASE;
static ModuleMaster *modPicture = NULL;
static OBJECTPTR clPicture = NULL;
static OBJECTPTR modDisplay = NULL;
static DisplayBase *DisplayBase = NULL;
static THREADVAR LONG glError = FALSE;

static ERROR decompress_png(objPicture *, objBitmap *, int, int, png_structp, png_infop, png_uint_32, png_uint_32);
static void read_row_callback(png_structp, png_uint_32, int);
static void write_row_callback(png_structp, png_uint_32, int);
static void png_error_hook(png_structp png_ptr, png_const_charp message);
static void png_warning_hook(png_structp png_ptr, png_const_charp message);
static ERROR create_picture_class(void);

rgb_to_linear glLinearRGB;

//****************************************************************************

static void conv_l2r_row32(UBYTE *Row, LONG Width) {
   for (LONG x=0; x < Width; x++) {
      Row[0] = glLinearRGB.invert(Row[0]);
      Row[1] = glLinearRGB.invert(Row[1]);
      Row[2] = glLinearRGB.invert(Row[2]);
      Row += 4;
   }
}

//****************************************************************************

static void conv_l2r_row24(UBYTE *Row, LONG Width) {
   for (LONG x=0; x < Width; x++) {
      Row[0] = glLinearRGB.invert(Row[0]);
      Row[1] = glLinearRGB.invert(Row[1]);
      Row[2] = glLinearRGB.invert(Row[2]);
      Row += 3;
   }
}

//****************************************************************************

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   if (GetPointer(argModule, FID_Master, &modPicture) != ERR_Okay) return ERR_GetField;
   if (LoadModule("display", MODVERSION_DISPLAY, &modDisplay, &DisplayBase) != ERR_Okay) return ERR_InitModule;

   return(create_picture_class());
}

//****************************************************************************

static ERROR CMDExpunge(void)
{
   if (clPicture)  { acFree(clPicture); clPicture = NULL; }
   if (modDisplay) { acFree(modDisplay); modDisplay = NULL; }
   return ERR_Okay;
}

/*****************************************************************************

-ACTION-
Activate: Loads image data into a picture object.

In order to load an image file into a picture object you will need to Activate it after initialisation.  So long as the
#Path field refers to a recognised image file, it will be loaded into the picture object and the fields
will be filled out to reflect the image content.

If you have preset the values of certain fields prior to activation, you will be placing restrictions on the image file
that is to be loaded.  For example, if the source image is wider than a restricted Bitmap Width, the image will have
its right edge clipped.  The same is true for the Bitmap Height and other restrictions apply to fields such as the
Bitmap Palette.

Once the picture is loaded, the image data will be held in the picture's Bitmap object.  You can draw to and from the
Bitmap using its available drawing methods.
-END-

*****************************************************************************/

static ERROR PIC_Activate(objPicture *Self, APTR Void)
{
   parasol::Log log;

   if (Self->Bitmap->Head::Flags & NF_INITIALISED) return ERR_Okay;

   log.branch();

   ERROR error = ERR_Failed;
   glError = FALSE;

   objBitmap *bmp = Self->Bitmap;
   png_structp read_ptr = NULL;
   png_infop info_ptr = NULL;
   png_infop end_info = NULL;

   if (!Self->prvFile) {
      STRING path;
      if (GetString(Self, FID_Path, &path) != ERR_Okay) {
         return log.warning(ERR_GetField);
      }

      if (CreateObject(ID_FILE, 0, &Self->prvFile,
            FID_Path|TSTR,   path,
            FID_Flags|TLONG, FL_READ|FL_APPROXIMATE,
            TAGEND) != ERR_Okay) goto exit;
   }

   acSeek(Self->prvFile, 0, SEEK_START);

   // Allocate PNG structures

   if (!(read_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, Self, &png_error_hook, &png_warning_hook))) goto exit;
   if (!(info_ptr = png_create_info_struct(read_ptr))) goto exit;
   if (!(end_info = png_create_info_struct(read_ptr))) goto exit;

   // Setup the PNG file

   read_ptr->io_ptr = Self->prvFile;
   read_ptr->read_data_fn = png_read_data;
   read_ptr->output_flush_fn = NULL;

   png_set_read_status_fn(read_ptr, read_row_callback); if (glError) goto exit;
   png_read_info(read_ptr, info_ptr); if (glError) goto exit;

   int bit_depth, total_bit_depth, color_type;
   png_uint_32 png_width, png_height;
   png_get_IHDR(read_ptr, info_ptr, &png_width, &png_height, &bit_depth, &color_type, NULL, NULL, NULL);
   if (glError) goto exit;

   if (!bmp->Width)  bmp->Width  = png_width;
   if (!bmp->Height) bmp->Height = png_height;
   if (!bmp->Type)   bmp->Type   = BMP_CHUNKY;

   if (!Self->DisplayWidth)  Self->DisplayWidth  = png_width;
   if (!Self->DisplayHeight) Self->DisplayHeight = png_height;

   // If the image contains a palette, load the palette into our Bitmap

   if (info_ptr->valid & PNG_INFO_PLTE) {
      for (LONG i=0; (i < info_ptr->num_palette) and (i < 256); i++) {
         bmp->Palette->Col[i].Red   = info_ptr->palette[i].red;
         bmp->Palette->Col[i].Green = info_ptr->palette[i].green;
         bmp->Palette->Col[i].Blue  = info_ptr->palette[i].blue;
         bmp->Palette->Col[i].Alpha = 255;
      }
   }
   else if (color_type IS PNG_COLOR_TYPE_GRAY) {
      for (LONG i=0; i < 256; i++) {
         bmp->Palette->Col[i].Red   = i;
         bmp->Palette->Col[i].Green = i;
         bmp->Palette->Col[i].Blue  = i;
         bmp->Palette->Col[i].Alpha = 255;
      }
   }

   // If the picture supports an alpha channel, initialise an alpha based Mask object for the Picture.

   if (color_type & PNG_COLOR_MASK_ALPHA) {
      if (Self->Flags & PCF_FORCE_ALPHA_32) {
         // Upgrade the image to 32-bit and store the alpha channel in the alpha byte of the pixel data.

         bmp->BitsPerPixel  = 32;
         bmp->BytesPerPixel = 4;
         bmp->Flags |= BMF_ALPHA_CHANNEL;
      }
      else {
         if (!NewObject(ID_BITMAP, NF_INTEGRAL, &Self->Mask)) {
            Self->Mask->Width  = Self->Bitmap->Width;
            Self->Mask->Height = Self->Bitmap->Height;
            Self->Mask->AmtColours = 256;
            Self->Mask->Flags      |= BMF_MASK;
            if (!acInit(Self->Mask)) Self->Flags |= PCF_MASK|PCF_ALPHA;
            else goto exit;
         }
      }
   }

   // If a background colour has been specified for the image (instead of an alpha channel), read it and create the
   // mask based on the data that we have read.

   if (info_ptr->valid & PNG_INFO_tRNS) {
      // The first colour index in the list is taken as the background, any others are ignored

      RGB8 rgb;
      if ((info_ptr->color_type IS PNG_COLOR_TYPE_PALETTE) or
          (info_ptr->color_type IS PNG_COLOR_TYPE_GRAY) or
          (info_ptr->color_type IS PNG_COLOR_TYPE_GRAY_ALPHA)) {
         bmp->TransIndex = info_ptr->trans_alpha[0];
         rgb = bmp->Palette->Col[bmp->TransIndex];
         rgb.Alpha = 255;
         SetPointer(bmp, FID_Transparence, &rgb);
      }
      else {
         rgb.Red   = info_ptr->trans_color.red;
         rgb.Green = info_ptr->trans_color.green;
         rgb.Blue  = info_ptr->trans_color.blue;
         rgb.Alpha = 255;
         SetPointer(bmp, FID_Transparence, &rgb);
      }
   }

   if (info_ptr->valid & PNG_INFO_bKGD) {
      png_color_16p prgb;
      prgb = &(info_ptr->background);
      if (color_type IS PNG_COLOR_TYPE_PALETTE) {
         bmp->BkgdRGB.Red   = bmp->Palette->Col[info_ptr->trans_alpha[0]].Red;
         bmp->BkgdRGB.Green = bmp->Palette->Col[info_ptr->trans_alpha[0]].Green;
         bmp->BkgdRGB.Blue  = bmp->Palette->Col[info_ptr->trans_alpha[0]].Blue;
         bmp->BkgdRGB.Alpha = 255;
      }
      else if ((color_type IS PNG_COLOR_TYPE_GRAY) or (color_type IS PNG_COLOR_TYPE_GRAY_ALPHA)) {
         bmp->BkgdRGB.Red   = prgb->gray;
         bmp->BkgdRGB.Green = prgb->gray;
         bmp->BkgdRGB.Blue  = prgb->gray;
         bmp->BkgdRGB.Alpha = 255;
      }
      else {
         bmp->BkgdRGB.Red   = prgb->red;
         bmp->BkgdRGB.Green = prgb->green;
         bmp->BkgdRGB.Blue  = prgb->blue;
         bmp->BkgdRGB.Alpha = 255;
      }
      log.trace("Background Colour: %d,%d,%d", bmp->BkgdRGB.Red, bmp->BkgdRGB.Green, bmp->BkgdRGB.Blue);
   }

   // Set the bits per pixel value

   switch (color_type) {
      case PNG_COLOR_TYPE_GRAY:       total_bit_depth = MAX(bit_depth, 8); break;
      case PNG_COLOR_TYPE_PALETTE:    total_bit_depth = MAX(bit_depth, 8); break;
      case PNG_COLOR_TYPE_RGB:        total_bit_depth = MAX(bit_depth, 8) * 3; break;
      case PNG_COLOR_TYPE_RGB_ALPHA:  total_bit_depth = MAX(bit_depth, 8) * 4; break;
      case PNG_COLOR_TYPE_GRAY_ALPHA: total_bit_depth = MAX(bit_depth, 8) * 2; break;
      default:
         log.warning("Unrecognised colour type 0x%x.", color_type);
         total_bit_depth = MAX(bit_depth, 8);
   }

   if (!bmp->BitsPerPixel) {
      if ((color_type IS PNG_COLOR_TYPE_GRAY) or (color_type IS PNG_COLOR_TYPE_PALETTE)) {
         bmp->BitsPerPixel = 8;
      }
      else bmp->BitsPerPixel = 24;
   }

   if ((Self->Flags & PCF_NO_PALETTE) and (bmp->BitsPerPixel <= 8)) {
      bmp->BitsPerPixel = 32;
   }

   if ((bmp->BitsPerPixel < 24) and
       ((bmp->BitsPerPixel < total_bit_depth) or
        ((total_bit_depth <= 8) and (bmp->BitsPerPixel > 8)))) {
      objBitmap *tmp_bitmap;

      log.msg("Destination Depth %d < Image Depth %d - Dithering.", bmp->BitsPerPixel, total_bit_depth);

      // Init our bitmap, since decompress_png() won't in this case.

      if ((error = acQuery(bmp)) != ERR_Okay) goto exit;
      if (!(bmp->Head::Flags & NF_INITIALISED)) {
         if ((error = acInit(bmp)) != ERR_Okay) goto exit;
      }

      // Create our temporary bitmap

      if (!(error = NewObject(ID_BITMAP, 0, &tmp_bitmap))) {
         SetFields(tmp_bitmap,
            FID_Width|TLONG,  bmp->Width,
            FID_Height|TLONG, bmp->Height,
            FID_BitsPerPixel, total_bit_depth,
            TAGEND);

         if (!(error = decompress_png(Self, tmp_bitmap, bit_depth, color_type, read_ptr, info_ptr, png_width, png_height))) {
            gfxCopyArea(tmp_bitmap, bmp, BAF_DITHER, 0, 0, bmp->Width, bmp->Height, 0, 0);
         }

         acFree(tmp_bitmap);
      }
   }
   else error = decompress_png(Self, bmp, bit_depth, color_type, read_ptr, info_ptr, png_width, png_height);

   if (!error) {
      png_read_end(read_ptr, end_info);
      if (Self->prvFile) { acFree(Self->prvFile); Self->prvFile = NULL; }
   }
   else {
exit:
      log.warning(error);
   }

   png_destroy_read_struct(&read_ptr, &info_ptr, &end_info);

   return error;
}

//****************************************************************************

static ERROR PIC_Free(objPicture *Self, APTR Void)
{
   if (Self->prvPath)        { FreeResource(Self->prvPath); Self->prvPath = NULL; }
   if (Self->prvDescription) { FreeResource(Self->prvDescription); Self->prvDescription = NULL; }
   if (Self->prvDisclaimer)  { FreeResource(Self->prvDisclaimer); Self->prvDisclaimer = NULL; }
   if (Self->prvFile)        { acFree(Self->prvFile); Self->prvFile = NULL; }
   if (Self->Bitmap)         { acFree(Self->Bitmap); Self->Bitmap = NULL; }
   if (Self->Mask)           { acFree(Self->Mask); Self->Mask = NULL; }
   return ERR_Okay;
}

/*****************************************************************************

-ACTION-
Init: Prepares the object for use.

Objects that belong to the Picture class can be initialised in two possible ways.  If you have not set the
#Path field or have chosen to use the NEW flag, the initialisation routine will create a
#Bitmap area that contains no image data.  This allows you to fill the picture with your own image data and
save it using the #SaveImage() or #SaveToObject() actions.  You must set the bitmap width, height
and colour specifications at a minimum, or the initialisation process will fail.

If you have set the #Path field and avoided the NEW flag, the initialisation process will analyse the
file location to determine whether or not the data is in fact a valid image file.  If the file does not match up
with a registered data format, an error code of ERR_NoSupport is returned.  You will need to use the Activate or
Query actions to load or find out more information about the image format.
-END-

*****************************************************************************/

static ERROR PIC_Init(objPicture *Self, APTR Void)
{
   parasol::Log log;

   if ((!Self->prvPath) or (Self->Flags & PCF_NEW)) {
      // If no path has been specified, assume that the picture is being created from scratch (e.g. to save an
      // image to disk).  The programmer is required to specify the dimensions and colours of the Bitmap so that we can
      // initialise it.

      if (Self->Flags & PCF_FORCE_ALPHA_32) {
         Self->Bitmap->BitsPerPixel  = 32;
         Self->Bitmap->BytesPerPixel = 4;
         Self->Bitmap->Flags |= BMF_ALPHA_CHANNEL;
      }

      Self->Flags &= ~(PCF_RESIZE_X|PCF_RESIZE_Y|PCF_LAZY|PCF_SCALABLE); // Turn off irrelevant flags that don't match these

      if (!Self->Bitmap->Width) Self->Bitmap->Width = Self->DisplayWidth;
      if (!Self->Bitmap->Height) Self->Bitmap->Height = Self->DisplayHeight;

      if ((Self->Bitmap->Width) and (Self->Bitmap->Height)) {
         if (!acInit(Self->Bitmap)) {
            if (Self->Flags & PCF_FORCE_ALPHA_32) Self->Flags &= ~(PCF_ALPHA|PCF_MASK);

            if (Self->Flags & PCF_ALPHA) {
               if (!NewObject(ID_BITMAP, NF_INTEGRAL, &Self->Mask)) {
                  Self->Mask->Width  = Self->Bitmap->Width;
                  Self->Mask->Height = Self->Bitmap->Height;
                  Self->Mask->Flags |= PCF_MASK;
                  Self->Mask->BitsPerPixel = 8;
                  if (!acInit(Self->Mask)) Self->Flags |= PCF_MASK;
                  else return log.warning(ERR_Init);
               }
               else return log.warning(ERR_NewObject);
            }
            else if (Self->Flags & PCF_MASK) {
               if (!NewObject(ID_BITMAP, NF_INTEGRAL, &Self->Mask)) {
                  Self->Mask->Width  = Self->Bitmap->Width;
                  Self->Mask->Height = Self->Bitmap->Height;
                  Self->Mask->Flags |= PCF_MASK;
                  Self->Mask->BitsPerPixel = 1;
                  if (!acInit(Self->Mask)) Self->Flags |= PCF_MASK;
                  else return log.warning(ERR_Init);
               }
               else return log.warning(ERR_NewObject);
            }

            if (Self->SubID) return ERR_Okay; // Break here to let the sub-class continue initialisation

            return ERR_Okay;
         }
         else return log.warning(ERR_Init);
      }
      else return log.warning(ERR_InvalidDimension);
   }
   else {
      if (Self->SubID) return ERR_Okay; // Break here to let the sub-class continue initialisation

      // Test the given path to see if it matches our supported file format.

      STRING res_path;
      if (!ResolvePath(Self->prvPath, RSF_APPROXIMATE, &res_path)) {
         LONG result;

         FreeResource(Self->prvPath); // Switch to the resolved path in case it was approximated
         Self->prvPath = res_path;

         if (!ReadFileToBuffer(res_path, Self->prvHeader, sizeof(Self->prvHeader)-1, &result)) {
            Self->prvHeader[result] = 0;

            auto buffer = (UBYTE *)Self->prvHeader;

            if ((buffer[0] IS 0x89) and (buffer[1] IS 0x50) and (buffer[2] IS 0x4e) and (buffer[3] IS 0x47) and
                (buffer[4] IS 0x0d) and (buffer[5] IS 0x0a) and (buffer[6] IS 0x1a) and (buffer[7] IS 0x0a)) {
               if (Self->Flags & PCF_LAZY) return ERR_Okay;
               return acActivate(Self);
            }
            else return ERR_NoSupport;
         }
         else {
            log.warning("Failed to read '%s'", res_path);
            return ERR_File;
         }
      }
      else return log.warning(ERR_FileNotFound);
   }

   return ERR_NoSupport;
}

//****************************************************************************

static ERROR PIC_NewObject(objPicture *Self, APTR Void)
{
   parasol::Log log;

   Self->Quality = 80; // 80% quality rating when saving

   if (!NewObject(ID_BITMAP, NF_INTEGRAL, &Self->Bitmap)) {
      return ERR_Okay;
   }
   else return log.warning(ERR_NewObject);
}

//****************************************************************************

static ERROR PIC_Query(objPicture *Self, APTR Void)
{
   parasol::Log log;
   STRING path;
   png_uint_32 width, height;
   int bit_depth, color_type;

   if (Self->Bitmap->Flags & BMF_QUERIED) return ERR_Okay;
   if (!Self->prvFile) return ERR_NotInitialised;

   log.branch();

   objBitmap *Bitmap = Self->Bitmap;
   ERROR error = ERR_Failed;
   png_structp read_ptr = NULL;
   png_infop info_ptr = NULL;
   png_infop end_info = NULL;
   glError  = FALSE;

   // Open the data file

   if (!Self->prvFile) {
      if (GetString(Self, FID_Path, &path) != ERR_Okay) return log.warning(ERR_GetField);

      if (CreateObject(ID_FILE, 0, &Self->prvFile,
            FID_Path|TSTR,   path,
            FID_Flags|TLONG, FL_READ|FL_APPROXIMATE,
            TAGEND) != ERR_Okay) goto exit;
   }

   acSeek(Self->prvFile, 0.0, SEEK_START);

   // Allocate PNG structures

   if (!(read_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, Self, &png_error_hook, &png_warning_hook))) goto exit;
   if (!(info_ptr = png_create_info_struct(read_ptr))) goto exit;
   if (!(end_info = png_create_info_struct(read_ptr))) goto exit;

   // Read the PNG description

   read_ptr->io_ptr = Self->prvFile;
   read_ptr->read_data_fn = png_read_data;
   read_ptr->output_flush_fn = NULL;

   png_set_read_status_fn(read_ptr, read_row_callback); if (glError) goto exit;
   png_read_info(read_ptr, info_ptr); if (glError) goto exit;
   png_get_IHDR(read_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL); if (glError) goto exit;

   if (!Bitmap->Width)  Bitmap->Width  = width;
   if (!Bitmap->Height) Bitmap->Height = height;
   if (!Bitmap->Type)   Bitmap->Type   = BMP_CHUNKY;

   if (!Self->DisplayWidth)  Self->DisplayWidth  = width;
   if (!Self->DisplayHeight) Self->DisplayHeight = height;
   if (color_type & PNG_COLOR_MASK_ALPHA) Self->Flags |= PCF_ALPHA;

   if (!Bitmap->BitsPerPixel) {
      if ((color_type IS PNG_COLOR_TYPE_GRAY) or (color_type IS PNG_COLOR_TYPE_PALETTE)) {
         Bitmap->BitsPerPixel = 8;
         Bitmap->BytesPerPixel = 1;
      }
      else {
         Bitmap->BitsPerPixel = 24;
         Bitmap->BytesPerPixel = 3;
      }
   }

//   acQuery(Bitmap);

   error = ERR_Okay;

exit:
   png_destroy_read_struct(&read_ptr, &info_ptr, &end_info);
   return error;
}

/*****************************************************************************
-ACTION-
Read: Reads raw image data from a Picture object.
-END-
*****************************************************************************/

static ERROR PIC_Read(objPicture *Self, struct acRead *Args)
{
   return Action(AC_Read, Self->Bitmap, Args);
}

/*****************************************************************************
-ACTION-
Refresh: Refreshes a loaded picture - draws the next frame.
-END-
*****************************************************************************/

static ERROR PIC_Refresh(objPicture *Self, APTR Void)
{
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
SaveImage: Saves the picture image to a data object.

If no destination is specified then the image will be saved as a new file targeting #Path.

-END-
*****************************************************************************/

static ERROR PIC_SaveImage(objPicture *Self, struct acSaveImage *Args)
{
   parasol::Log log;
   STRING path;
   LONG y, i;
   png_bytep row_pointers;

   log.branch();

   objBitmap *bmp        = Self->Bitmap;
   OBJECTPTR file        = NULL;
   png_structp write_ptr = NULL;
   png_infop info_ptr    = NULL;
   ERROR error = ERR_Failed;
   glError = FALSE;

   // Open the data file

   if ((Args) and (Args->DestID)) {
      if (AccessObject(Args->DestID, 3000, &file) != ERR_Okay) {
         log.warning("Failed to access destination object #%d.", Args->DestID);
         goto exit;
      }
   }
   else {
      if (GetString(Self, FID_Path, &path) != ERR_Okay) {
         return log.warning(ERR_MissingPath);
      }

      if (CreateObject(ID_FILE, 0, &file,
            FID_Path|TSTR,   path,
            FID_Flags|TLONG, FL_NEW|FL_WRITE,
            TAGEND) != ERR_Okay) {
         return ERR_CreateObject;
      }
   }

   // Allocate PNG structures

   if (!(write_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, Self, &png_error_hook, &png_warning_hook))) {
      log.warning("png_create_write_struct() failed.");
      goto exit;
   }

   png_set_error_fn(write_ptr, Self, &png_error_hook, &png_warning_hook);

   if (!(info_ptr = png_create_info_struct(write_ptr))) {
      log.warning("png_create_info_struct() failed.");
      goto exit;
   }

   // Setup the PNG file

   write_ptr->io_ptr = file;
   write_ptr->write_data_fn = (png_rw_ptr)png_write_data;
   write_ptr->output_flush_fn = NULL;

   png_set_write_status_fn(write_ptr, write_row_callback);
   if (glError) {
      log.warning("png_set_write_status_fn() failed.");
      goto exit;
   }

   if ((Self->Flags & (PCF_ALPHA|PCF_MASK)) and (!Self->Mask)) {
      log.warning("Illegal use of the ALPHA/MASK flags without an accompanying mask bitmap.");
      Self->Flags &= ~(PCF_ALPHA|PCF_MASK);
   }

   if (bmp->AmtColours > 256) {
      if (bmp->Flags & BMF_ALPHA_CHANNEL) {
         log.trace("Saving as 32-bit alpha.");
         png_set_IHDR(write_ptr, info_ptr, bmp->Width, bmp->Height, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
      }
      else if (Self->Flags & PCF_ALPHA) {
         log.trace("Saving with alpha-mask.");
         png_set_IHDR(write_ptr, info_ptr, bmp->Width, bmp->Height, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
      }
      else {
         log.trace("Saving in standard chunky graphics mode (no alpha).");
         png_set_IHDR(write_ptr, info_ptr, bmp->Width, bmp->Height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
      }
   }
   else {
      png_set_IHDR(write_ptr, info_ptr, bmp->Width, bmp->Height, 8, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
      png_set_PLTE(write_ptr, info_ptr, (png_colorp)bmp->Palette->Col, bmp->AmtColours);
   }

   // On Intel CPU's the pixel format is BGR

   png_set_bgr(write_ptr);

   // Set the background colour

   if (bmp->BkgdRGB.Alpha) {
      png_color_16 rgb;
      if (bmp->AmtColours < 256) rgb.index = bmp->BkgdIndex;
      else rgb.index = 0;
      rgb.red   = bmp->BkgdRGB.Red;
      rgb.green = bmp->BkgdRGB.Green;
      rgb.blue  = bmp->BkgdRGB.Blue;
      png_set_bKGD(write_ptr, info_ptr, &rgb);
   }

   // Set the transparent colour

   if (bmp->TransRGB.Alpha) {
      png_color_16 rgb;
      if (bmp->AmtColours < 256) rgb.index = bmp->TransIndex;
      else rgb.index = 0;
      rgb.red   = bmp->TransRGB.Red;
      rgb.green = bmp->TransRGB.Green;
      rgb.blue  = bmp->TransRGB.Blue;
      png_set_tRNS(write_ptr, info_ptr, &rgb.index, 1, &rgb);
   }

   // Write the header to the PNG file

   png_write_info(write_ptr, info_ptr);
   if (glError) {
      log.warning("png_write_info() failed.");
      goto exit;
   }

   // Write the image data to the PNG file

   if ((bmp->BitsPerPixel IS 8) or (bmp->BitsPerPixel IS 24)) {
      if (Self->Flags & PCF_ALPHA) {
         UBYTE row[bmp->Width * 4];
         row_pointers = row;
         UBYTE *data = bmp->Data;
         UBYTE *mask = Self->Mask->Data;
         for (LONG y=0; y < bmp->Height; y++) {
            LONG i = 0;
            WORD maskx = 0;
            for (LONG x=0; x < bmp->ByteWidth; x+=3) {
               row[i++] = data[x+0];  // Blue
               row[i++] = data[x+1];  // Green
               row[i++] = data[x+2];  // Red
               row[i++] = mask[maskx++];  // Alpha
            }
            if (bmp->ColourSpace IS CS_LINEAR_RGB) conv_l2r_row32(row, bmp->Width);
            png_write_row(write_ptr, row_pointers);
            data += bmp->LineWidth;
            mask += Self->Mask->LineWidth;
         }
      }
      else {
         for (y=0; y < bmp->Height; y++) {
            row_pointers = bmp->Data + (y * bmp->LineWidth);
            png_write_row(write_ptr, row_pointers);
         }
      }
   }
   else if (bmp->BitsPerPixel IS 32) {
      if (bmp->Flags & BMF_ALPHA_CHANNEL) {
         UBYTE row[bmp->Width * 4];
         row_pointers = row;
         UBYTE *data = bmp->Data;
         for (LONG y=0; y < bmp->Height; y++) {
            LONG i = 0;
            for (LONG x=0; x < (bmp->Width<<2); x+=4) {
               row[i++] = data[x+0];  // Blue
               row[i++] = data[x+1];  // Green
               row[i++] = data[x+2];  // Red
               row[i++] = data[x+3];  // Alpha
            }
            if (bmp->ColourSpace IS CS_LINEAR_RGB) conv_l2r_row32(row, bmp->Width);
            png_write_row(write_ptr, row_pointers);
            data += bmp->LineWidth;
         }
      }
      else if (Self->Flags & PCF_ALPHA) {
         UBYTE row[bmp->Width * 4];

         row_pointers = row;
         UBYTE *data = bmp->Data;
         UBYTE *mask = Self->Mask->Data;
         for (LONG y=0; y < bmp->Height; y++) {
            LONG i = 0;
            WORD maskx = 0;
            for (LONG x=0; x < (bmp->Width<<2); x+=4) {
               row[i++] = data[x+0];     // Blue
               row[i++] = data[x+1];     // Green
               row[i++] = data[x+2];     // Red
               row[i++] = mask[maskx++]; // Alpha
            }
            if (bmp->ColourSpace IS CS_LINEAR_RGB) conv_l2r_row32(row, bmp->Width);
            png_write_row(write_ptr, row_pointers);
            data += bmp->LineWidth;
            mask += Self->Mask->LineWidth;
         }
      }
      else {
         UBYTE row[bmp->Width * 3];
         row_pointers = row;
         UBYTE *data = bmp->Data;
         for (LONG y=0; y < bmp->Height; y++) {
            i = 0;
            for (LONG x=0; x < (bmp->Width<<2); x+=4) {
               row[i++] = data[x+0];  // Blue
               row[i++] = data[x+1];  // Green
               row[i++] = data[x+2];  // Red
            }
            if (bmp->ColourSpace IS CS_LINEAR_RGB) conv_l2r_row24(row, bmp->Width);
            png_write_row(write_ptr, row_pointers);
            data += bmp->LineWidth;
         }
      }
   }
   else if (bmp->BytesPerPixel IS 2) {
      if (Self->Flags & PCF_ALPHA) {
         UBYTE row[bmp->Width * 4];
         row_pointers = row;
         UWORD *data = (UWORD *)bmp->Data;
         UBYTE *mask = Self->Mask->Data;
         for (LONG y=0; y < bmp->Height; y++) {
            LONG i = 0;
            WORD maskx = 0;
            for (LONG x=0; x < bmp->Width; x++) {
               row[i++] = UnpackBlue(bmp, data[x]);
               row[i++] = UnpackGreen(bmp, data[x]);
               row[i++] = UnpackRed(bmp, data[x]);
               row[i++] = mask[maskx++];
            }
            if (bmp->ColourSpace IS CS_LINEAR_RGB) conv_l2r_row32(row, bmp->Width);
            png_write_row(write_ptr, row_pointers);
            data = (UWORD *)(((UBYTE *)data) + bmp->LineWidth);
            mask += Self->Mask->LineWidth;
         }
      }
      else {
         UBYTE row[bmp->Width * 3];

         row_pointers = row;
         UWORD *data = (UWORD *)bmp->Data;
         for (LONG y=0; y < bmp->Height; y++) {
            LONG i = 0;
            for (LONG x=0; x < bmp->Width; x++) {
               row[i++] = UnpackBlue(bmp, data[x]);
               row[i++] = UnpackGreen(bmp, data[x]);
               row[i++] = UnpackRed(bmp, data[x]);
            }
            if (bmp->ColourSpace IS CS_LINEAR_RGB) conv_l2r_row24(row, bmp->Width);
            png_write_row(write_ptr, row_pointers);
            data = (UWORD *)(((UBYTE *)data) + bmp->LineWidth);
         }
      }
   }

   png_write_end(write_ptr, NULL);

   error = ERR_Okay;

exit:
   png_destroy_write_struct(&write_ptr, &info_ptr);

   if ((Args) and (Args->DestID)) {
      if (file) ReleaseObject(file);
   }
   else if (file) acFree(file);

   if (error) return log.warning(error);
   else return ERR_Okay;
}

/*****************************************************************************
-ACTION-
SaveToObject: Saves the picture image to a data object.
-END-
*****************************************************************************/

static ERROR PIC_SaveToObject(objPicture *Self, struct acSaveToObject *Args)
{
   parasol::Log log;
   ERROR (**routine)(OBJECTPTR, APTR);

   if ((Args->ClassID) and (Args->ClassID != ID_PICTURE)) {
      rkMetaClass *mc = (rkMetaClass *)FindClass(Args->ClassID);
      if ((GetPointer(mc, FID_ActionTable, &routine) IS ERR_Okay) and (routine)) {
         if ((routine[AC_SaveToObject]) and (routine[AC_SaveToObject] != (APTR)PIC_SaveToObject)) {
            return routine[AC_SaveToObject](Self, Args);
         }
         else if ((routine[AC_SaveImage]) and (routine[AC_SaveImage] != (APTR)PIC_SaveImage)) {
            struct acSaveImage saveimage;
            saveimage.DestID = Args->DestID;
            return routine[AC_SaveImage](Self, &saveimage);
         }
         else return log.warning(ERR_NoSupport);
      }
      else return log.warning(ERR_GetField);
   }
   else return acSaveImage(Self, Args->DestID, Args->ClassID);
}

/*****************************************************************************
-ACTION-
Seek: Seeks to a new read/write position within a Picture object.
-END-
*****************************************************************************/

static ERROR PIC_Seek(objPicture *Self, struct acSeek *Args)
{
   return Action(AC_Seek, Self->Bitmap, Args);
}

/*****************************************************************************
-ACTION-
Write: Writes raw image data to a picture object.
-END-
*****************************************************************************/

static ERROR PIC_Write(objPicture *Self, struct acWrite *Args)
{
   return Action(AC_Write, Self->Bitmap, Args);
}

/*****************************************************************************

-FIELD-
Author: The name of the person or company that created the image.

*****************************************************************************/

static ERROR GET_Author(objPicture *Self, STRING *Value)
{
   *Value = Self->prvAuthor;
   return ERR_Okay;
}

static ERROR SET_Author(objPicture *Self, CSTRING Value)
{
   if (Value) StrCopy(Value, Self->prvAuthor, sizeof(Self->prvAuthor));
   else Self->prvAuthor[0] = 0;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Bitmap: Represents a picture's image data.

The details of a picture's graphical image and data are defined in its associated bitmap object.  It contains
information on the image dimensions and palette for example.  When loading a picture, you can place certain
constraints on the image by presetting Bitmap fields such as the Width and Height (this will have the effect
of clipping or resizing the source image). The Palette can also be preset if you want to remap the source
image to a specific set of colour values.

Please refer to the @Bitmap class for more details on the structure of bitmap objects.

-FIELD-
Copyright: Copyright details of an image.

Copyright details related to an image may be specified here.  The copyright should be short and to the point, for
example "Copyright H.R. Giger (c) 1992."

*****************************************************************************/

static ERROR GET_Copyright(objPicture *Self, STRING *Value)
{
   *Value = Self->prvCopyright;
   return ERR_Okay;
}

static ERROR SET_Copyright(objPicture *Self, CSTRING Value)
{
   if (Value) StrCopy(Value, Self->prvCopyright, sizeof(Self->prvCopyright));
   else Self->prvCopyright[0] = 0;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Description: Long description for an image.

A long description for an image may be entered in this field.  There is no strict limit on the length of the
description.

*****************************************************************************/

static ERROR GET_Description(objPicture *Self, STRING *Value)
{
   if (Self->prvDescription) {
      *Value = Self->prvDescription;
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      return ERR_FieldNotSet;
   }
}

static ERROR SET_Description(objPicture *Self, CSTRING Value)
{
   parasol::Log log;

   if (Self->prvDescription) { FreeResource(Self->prvDescription); Self->prvDescription = NULL; }

   if ((Value) and (*Value)) {
      if (!(Self->prvDescription = StrClone(Value))) return log.warning(ERR_AllocMemory);
   }
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Disclaimer: The disclaimer associated with an image.

If it is necessary to associate a disclaimer with an image, the legal text may be entered in this field.

*****************************************************************************/

static ERROR GET_Disclaimer(objPicture *Self, STRING *Value)
{
   if (Self->prvDisclaimer) {
      *Value = Self->prvDisclaimer;
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      return ERR_FieldNotSet;
   }
}

static ERROR SET_Disclaimer(objPicture *Self, CSTRING Value)
{
   parasol::Log log;

   if (Self->prvDisclaimer) { FreeResource(Self->prvDisclaimer); Self->prvDisclaimer = NULL; }

   if ((Value) and (*Value)) {
      if (!(Self->prvDisclaimer = StrClone(Value))) return log.warning(ERR_AllocMemory);
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
DisplayHeight: The preferred height to use when displaying the image.

The DisplayWidth and DisplayHeight fields define the preferred pixel dimensions to use for the display when viewing the
image in a 96DPI environment.  Both fields will be set automatically when the picture source is loaded.  If
the source does not specify a suitable value for these fields, they may be initialised to a value based on the
picture's #Bitmap Width and Height.

In the case of a scalable image source such as SVG, the DisplayWidth and DisplayHeight can be pre-configured by the
client, and the loader will scale the source image to the preferred dimensions on load.

-FIELD-
DisplayWidth: The preferred width to use when displaying the image.

The DisplayWidth and DisplayHeight fields define the preferred pixel dimensions to use for the display when viewing the
image in a 96DPI environment.  Both fields will be set automatically when the picture source is loaded.  If
the source does not specify a suitable value for these fields, they may be initialised to a value based on the
picture's #Bitmap Width and Height.

In the case of a scalable image source such as SVG, the DisplayWidth and DisplayHeight can be pre-configured by the
client, and the loader will scale the source image to the preferred dimensions on load.

-FIELD-
Flags:  Optional initialisation flags.

-FIELD-
Header: Contains the first 32 bytes of data in a picture's file header.

The Header field is a pointer to a 32 byte buffer that contains the first 32 bytes of information read from a picture
file on initialisation.  This special field is considered to be helpful only to developers writing add on components
for the picture class.

The buffer that is referred to by the Header field is not populated until the Init action is called on the picture object.

*****************************************************************************/

static ERROR GET_Header(objPicture *Self, APTR *Value)
{
   *Value = Self->prvHeader;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Path: The location of source image data.

*****************************************************************************/

static ERROR GET_Path(objPicture *Self, STRING *Value)
{
   if (Self->prvPath) {
      *Value = Self->prvPath;
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      return ERR_FieldNotSet;
   }
}

static ERROR SET_Path(objPicture *Self, CSTRING Value)
{
   parasol::Log log;

   if (Self->prvPath) { FreeResource(Self->prvPath); Self->prvPath = NULL; }

   if ((Value) and (*Value)) {
      if (!(Self->prvPath = StrClone(Value))) return log.warning(ERR_AllocMemory);
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Mask: Refers to a Bitmap that imposes a mask on the image.

If a source picture includes a mask, the Mask field will refer to a Bitmap object that contains the mask image once the
picture source has been loaded.  The mask will be expressed as either a 256 colour alpha bitmap, or a 1-bit mask with
8 pixels per byte.

If creating a picture from scratch that needs to support a mask, set the `MASK` flag prior to initialisation
and the picture class will allocate the mask bitmap automatically.

-FIELD-
Quality: Defines the quality level to use when saving the image.

The quality level to use when saving the image is defined here.  The value is expressed as a percentage between 0 and
100%, with 100% being of the highest quality.  If the picture format is loss-less, such as PNG, then the quality level
may be used to determine the compression factor.

In all cases, the impact of selecting a high level of quality will increase the time it takes to save the image.

-FIELD-
Software: The name of the application that was used to draw the image.

*****************************************************************************/

static ERROR GET_Software(objPicture *Self, STRING *Value)
{
   *Value = Self->prvSoftware;
   return ERR_Okay;
}

static ERROR SET_Software(objPicture *Self, CSTRING Value)
{
   if (Value) StrCopy(Value, Self->prvSoftware, sizeof(Self->prvSoftware));
   else Self->prvSoftware[0] = 0;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Title: The title of the image.
-END-
*****************************************************************************/

static ERROR GET_Title(objPicture *Self, STRING *Value)
{
   *Value = Self->prvTitle;
   return ERR_Okay;
}

static ERROR SET_Title(objPicture *Self, CSTRING Value)
{
   if (Value) StrCopy(Value, Self->prvTitle, sizeof(Self->prvTitle));
   else Self->prvTitle[0] = 0;
   return ERR_Okay;
}

//****************************************************************************

static void read_row_callback(png_structp read_ptr, png_uint_32 row, int pass)
{

}

static void write_row_callback(png_structp write_ptr, png_uint_32 row, int pass)
{

}

/*****************************************************************************
** Read functions
*/

void png_read_data(png_structp png, png_bytep data, png_size_t length)
{
   struct acRead read = { data, (LONG)length };
   if ((Action(AC_Read, png->io_ptr, &read) != ERR_Okay) or ((png_size_t)read.Result != length)) {
      png_error(png, "File read error");
   }
}

void png_set_read_fn(png_structp png_ptr, png_voidp io_ptr, png_rw_ptr read_data_fn)
{
   png_ptr->io_ptr = io_ptr;
   png_ptr->read_data_fn = png_read_data;
   png_ptr->output_flush_fn = NULL;
}

//****************************************************************************
// Write functions.

void png_write_data(png_structp png, png_const_bytep data, png_size_t length)
{
   struct acWrite write = { data, (LONG)length };
   if ((Action(AC_Write, png->io_ptr, &write) != ERR_Okay) or ((png_size_t)write.Result != length)) {
      png_error(png, "File write error");
   }
}

void png_flush(png_structp png_ptr)
{

}

// Required by pngwrite.c

void png_set_write_fn(png_structp png_ptr, png_voidp io_ptr, png_rw_ptr write_data_fn, png_flush_ptr output_flush_fn)
{
   png_ptr->io_ptr = io_ptr;
   png_ptr->write_data_fn = (png_rw_ptr)png_write_data;
   png_ptr->output_flush_fn = NULL;
}

/*****************************************************************************
** PNG Error Handling Functions
*/

static void png_error_hook(png_structp png_ptr, png_const_charp message)
{
   parasol::Log log;
   log.warning("%s", message);
   glError = TRUE;
}

static void png_warning_hook(png_structp png_ptr, png_const_charp message)
{
   parasol::Log log;
   log.warning("Warning: %s", message);
   glError = FALSE;
}

ZEXTERN uLong ZEXPORT crc32   OF((uLong crc, const Bytef *buf, uInt len))
{
   return GenCRC32(crc, (APTR)buf, len);
}

//****************************************************************************

static ERROR decompress_png(objPicture *Self, objBitmap *Bitmap, int BitDepth, int ColourType, png_structp ReadPtr,
                            png_infop InfoPtr, png_uint_32 PngWidth, png_uint_32 PngHeight)
{
   ERROR error;
   UBYTE *row;
   png_bytep row_pointers;
   RGB8 rgb;
   LONG i, x, y;
   parasol::Log log(__FUNCTION__);

   // Read the image data into our Bitmap

   if (ColourType & PNG_COLOR_MASK_ALPHA) png_set_expand(ReadPtr); // Alpha channel
   if (BitDepth IS 16) png_set_strip_16(ReadPtr); // Reduce bit depth to 24bpp if the image is 48bpp
   if (BitDepth < 8) png_set_packing(ReadPtr);

   log.branch("Size: %dx%dx%d", (LONG)PngWidth, (LONG)PngHeight, BitDepth);

   LONG rowsize = png_get_rowbytes(ReadPtr, InfoPtr);
   if ((error = acQuery(Bitmap)) != ERR_Okay) return error;
   if (!(Bitmap->Head::Flags & NF_INITIALISED)) {
      if ((error = acInit(Bitmap)) != ERR_Okay) return error;
   }
   if ((error = AllocMemory(rowsize, MEM_DATA|MEM_NO_CLEAR, &row, NULL)) != ERR_Okay) return error;

   if (Self->Flags & PCF_RESIZE) {
      DOUBLE fx, fy;
      LONG isrcy, isrcx, ify;

      DOUBLE xScale = (DOUBLE)PngWidth / (DOUBLE)Bitmap->Width;
      DOUBLE yScale = (DOUBLE)PngHeight / (DOUBLE)Bitmap->Height;

      row_pointers = row;
      if (ColourType IS PNG_COLOR_TYPE_GRAY) {
         isrcy = -1;
         fy = 0;
         rgb.Alpha = 255;
         for (y=0; y < Bitmap->Height; y++, fy += yScale) {
            ify = F2T(fy);
            fx = 0;
            while (isrcy != ify) {
               png_read_row(ReadPtr, row_pointers, NULL); if (glError) goto exit;
               isrcy++;
            }
            for (x=0; x < Bitmap->Width; x++, fx += xScale) {
               isrcx = F2T(fx);
               rgb.Red   = row[isrcx];
               rgb.Green = row[isrcx];
               rgb.Blue  = row[isrcx];
               Bitmap->DrawUCRPixel(Bitmap, x, y, &rgb);
            }
         }
      }
      else if (ColourType IS PNG_COLOR_TYPE_PALETTE) {
         isrcy = -1;
         fy = 0;
         rgb.Alpha = 255;
         for (y=0; y < Bitmap->Height; y++, fy += yScale) {
            fx = 0;
            ify = F2T(fy);
            while (isrcy != ify) {
               png_read_row(ReadPtr, row_pointers, NULL); if (glError) goto exit;
               isrcy++;
            }
            for (x=0; x < Bitmap->Width; x++, fx += xScale) {
               isrcx = F2T(fx);
               if (Bitmap->BitsPerPixel IS 8) Bitmap->DrawUCPixel(Bitmap, x, y, row[isrcx]);
               else Bitmap->DrawUCRPixel(Bitmap, x, y, &Bitmap->Palette->Col[row[isrcx]]);
            }
         }
      }
      else if (ColourType & PNG_COLOR_MASK_ALPHA) {
         // When decompressing images that support an alpha channel, the fourth byte of each pixel will contain the alpha data.

         isrcy = -1;
         fy = 0;
         for (y=0; y < Bitmap->Height; y++, fy += yScale) {
            fx = 0;
            ify = F2T(fy);
            while (isrcy != ify) {
               png_read_row(ReadPtr, row_pointers, NULL); if (glError) goto exit;
               isrcy++;
            }
            for (x=0; x < Bitmap->Width; x++, fx += xScale) {
               isrcx = F2T(fx);
               isrcx <<=2;
               rgb.Red   = row[isrcx];
               rgb.Green = row[isrcx+1];
               rgb.Blue  = row[isrcx+2];
               rgb.Alpha = row[isrcx+3];
               Bitmap->DrawUCRPixel(Bitmap, x, y, &rgb);

               // Set the alpha byte in the alpha mask (nb: refer to png_set_invert_alpha() if you want to reverse the
               // alpha bytes)

               if (Self->Mask) Self->Mask->Data[(y * Self->Mask->LineWidth) + x] = rgb.Alpha;
            }
         }
      }
      else {
         isrcy = -1;
         fy = 0;
         rgb.Alpha = 255;
         for (y=0; y < Bitmap->Height; y++, fy += yScale) {
            fx = 0;
            ify = F2T(fy);
            while (isrcy != ify) {
               png_read_row(ReadPtr, row_pointers, NULL); if (glError) goto exit;
               isrcy++;
            }
            for (x=0; x < Bitmap->Width; x++, fx += xScale) {
               isrcx = F2T(fx);
               isrcx *= 3;
               rgb.Red   = row[isrcx];
               rgb.Green = row[isrcx+1];
               rgb.Blue  = row[isrcx+2];
               Bitmap->DrawUCRPixel(Bitmap, x, y, &rgb);
            }
         }
      }
   }
   else {
      // Chop the image to the bitmap dimensions

      if (PngWidth > (png_uint_32)Bitmap->Width) PngWidth = Bitmap->Width;
      if (PngHeight > (png_uint_32)Bitmap->Height) PngHeight = Bitmap->Height;

      row_pointers = row;
      if (ColourType IS PNG_COLOR_TYPE_GRAY) {
         log.trace("Greyscale image source.");
         rgb.Alpha = 255;
         for (png_uint_32 y=0; y < PngHeight; y++) {
            png_read_row(ReadPtr, row_pointers, NULL); if (glError) goto exit;
            for (png_uint_32 x=0; x < PngWidth; x++) {
               rgb.Red   = row[x];
               rgb.Green = row[x];
               rgb.Blue  = row[x];
               Bitmap->DrawUCRPixel(Bitmap, x, y, &rgb);
            }
         }
      }
      else if (ColourType IS PNG_COLOR_TYPE_PALETTE) {
         log.trace("Palette-based image source.");
         if (Bitmap->BitsPerPixel IS 8) {
            for (png_uint_32 y=0; y < PngHeight; y++) {
               png_read_row(ReadPtr, row_pointers, NULL); if (glError) goto exit;
               for (png_uint_32 x=0; x < PngWidth; x++) Bitmap->DrawUCPixel(Bitmap, x, y, row[x]);
            }
         }
         else {
            rgb.Alpha = 255;
            for (png_uint_32 y=0; y < PngHeight; y++) {
               png_read_row(ReadPtr, row_pointers, NULL); if (glError) goto exit;
               for (png_uint_32 x=0; x < PngWidth; x++) {
                  Bitmap->DrawUCRPixel(Bitmap, x, y, &Bitmap->Palette->Col[row[x]]);
               }
            }
         }
      }
      else if (ColourType & PNG_COLOR_MASK_ALPHA) {
         // When decompressing images that support an alpha channel, the fourth byte of each pixel will contain the alpha data.

         log.trace("32-bit + alpha image source.");
         for (png_uint_32 y=0; y < PngHeight; y++) {
            png_read_row(ReadPtr, row_pointers, NULL); if (glError) goto exit;
            i = 0;
            for (png_uint_32 x=0; x < PngWidth; x++) {
               Bitmap->DrawUCRPixel(Bitmap, x, y, (RGB8 *)(row+i));

               // Set the alpha byte in the alpha mask (nb: refer to png_set_invert_alpha() if you want to reverse the alpha bytes)

               if (Self->Mask) Self->Mask->Data[(y * Self->Mask->LineWidth) + x] = row[3];

               i += 4;
            }
         }
      }
      else {
         log.trace("24-bit image source.");
         rgb.Alpha = 255;
         for (png_uint_32 y=0; y < PngHeight; y++) {
            png_read_row(ReadPtr, row_pointers, NULL); if (glError) goto exit;
            i = 0;
            for (png_uint_32 x=0; x < PngWidth; x++) {
               rgb.Red   = row[i++];
               rgb.Green = row[i++];
               rgb.Blue  = row[i++];
               Bitmap->DrawUCRPixel(Bitmap, x, y, &rgb);
            }
         }
      }
   }

exit:
   FreeResource(row);
   return error;
}

static const FieldDef clFlags[] = {
   { "ResizeX",   PCF_RESIZE_X   }, { "ResizeY", PCF_RESIZE_Y },
   { "Resize",    PCF_RESIZE     }, { "Stretch", PCF_RESIZE },
   { "New",       PCF_NEW        }, { "Lazy",    PCF_LAZY },
   { "Alpha",     PCF_ALPHA      },
   { "NoPalette", PCF_NO_PALETTE }, { "ForceAlpha32", PCF_FORCE_ALPHA_32 },
   { "Scalable",  PCF_SCALABLE   },
   { NULL, 0 }
};

static const FieldArray clFields[] = {
   { "Bitmap",        FDF_INTEGRAL|FDF_R,     ID_BITMAP,        NULL, NULL },
   { "Mask",          FDF_INTEGRAL|FDF_R,     ID_BITMAP,        NULL, NULL },
   { "Flags",         FDF_LONGFLAGS|FDF_RW,   (MAXINT)&clFlags, NULL, NULL },
   { "DisplayHeight", FDF_LONG|FDF_RW,        0,                NULL, NULL },
   { "DisplayWidth",  FDF_LONG|FDF_RW,        0,                NULL, NULL },
   { "Quality",       FDF_LONG|FDF_RW,        0,                NULL, NULL },
   { "FrameRate",     FDF_SYSTEM|FDF_LONG|FDF_R, 0,             NULL, NULL },
   // Virtual fields
   { "Author",        FDF_STRING|FDF_RW,  0, (APTR)GET_Author,      (APTR)SET_Author },
   { "Copyright",     FDF_STRING|FDF_RW,  0, (APTR)GET_Copyright,   (APTR)SET_Copyright },
   { "Description",   FDF_STRING|FDF_RW,  0, (APTR)GET_Description, (APTR)SET_Description },
   { "Disclaimer",    FDF_STRING|FDF_RW,  0, (APTR)GET_Disclaimer,  (APTR)SET_Disclaimer },
   { "Header",        FDF_POINTER|FDF_RI, 0, (APTR)GET_Header,      NULL },
   { "Path",          FDF_STRING|FDF_RI,  0, (APTR)GET_Path,        (APTR)SET_Path },
   { "Location",      FDF_SYNONYM|FDF_STRING|FDF_RI, 0, (APTR)GET_Path, (APTR)SET_Path },
   { "Src",           FDF_SYNONYM|FDF_STRING|FDF_RI, 0, (APTR)GET_Path, (APTR)SET_Path },
   { "Software",      FDF_STRING|FDF_RW,  0, (APTR)GET_Software,    (APTR)SET_Software },
   { "Title",         FDF_STRING|FDF_RW,  0, (APTR)GET_Title,       (APTR)SET_Title },
   END_FIELD
};

static const ActionArray clActions[] = {
   { AC_Activate,      (APTR)PIC_Activate },
   { AC_Free,          (APTR)PIC_Free },
   { AC_NewObject,     (APTR)PIC_NewObject },
   { AC_Init,          (APTR)PIC_Init },
   { AC_Query,         (APTR)PIC_Query },
   { AC_Read,          (APTR)PIC_Read },
   { AC_Refresh,       (APTR)PIC_Refresh },
   { AC_SaveImage,     (APTR)PIC_SaveImage },
   { AC_SaveToObject,  (APTR)PIC_SaveToObject },
   { AC_Seek,          (APTR)PIC_Seek },
   { AC_Write,         (APTR)PIC_Write },
   { 0, NULL }
};

static ERROR create_picture_class(void)
{
   return CreateObject(ID_METACLASS, 0, &clPicture,
      FID_ClassVersion|TFLOAT,  VER_PICTURE,
      FID_Name|TSTR,            "Picture",
      FID_Category|TLONG,       CCF_GRAPHICS,
      FID_Flags|TLONG,          CLF_PROMOTE_INTEGRAL|CLF_PRIVATE_ONLY,
      FID_FileExtension|TSTR,   "*.png",
      FID_FileDescription|TSTR, "PNG Picture",
      FID_FileHeader|TSTR,      "[0:$89504e470d0a1a0a]",
      FID_Actions|TPTR,         clActions,
      FID_Fields|TARRAY,        clFields,
      FID_Size|TLONG,           sizeof(objPicture),
      FID_Path|TSTR,            MOD_PATH,
      TAGEND);
}

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, NULL, CMDExpunge, 1.0)
