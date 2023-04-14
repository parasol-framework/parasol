/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

This software is based in part on the work of the Independent JPEG Group.  Source code has been derived from the
libjpeg archive, a separate package copyright to Thomas G. Lane.  Libjpeg is publicly available on terms that are not
related to this Package.  The original libjpeg source code can be obtained from http://www.ijg.org.

*********************************************************************************************************************/

#include <parasol/main.h>
#include <parasol/modules/picture.h>
#include <parasol/modules/display.h>

#include "../picture/picture.h"

extern "C" {
#include "lib/jpeglib.h"
#include "lib/jerror.h"
#include "lib/jpegint.h"
}

struct CoreBase *CoreBase = NULL;
struct DisplayBase *DisplayBase = NULL;
static OBJECTPTR clJPEG = NULL;
static OBJECTPTR modDisplay = NULL;

static ERROR JPEG_Activate(extPicture *, APTR);
static ERROR JPEG_Init(extPicture *, APTR);
static ERROR JPEG_Query(extPicture *, APTR);
static ERROR JPEG_SaveImage(extPicture *, struct acSaveImage *);

static void decompress_jpeg(extPicture *, objBitmap *, struct jpeg_decompress_struct *);

//********************************************************************************************************************

static ERROR JPEG_Activate(extPicture *Self, APTR Void)
{
   pf::Log log;
   struct jpeg_decompress_struct cinfo;
   struct jpeg_error_mgr jerr;

   // Return if the picture object has already been activated

   if (Self->Bitmap->initialised()) return ERR_Okay;

   if (!Self->prvFile) {
      STRING path;
      if (Self->get(FID_Location, &path) != ERR_Okay) return log.warning(ERR_GetField);

      if (!(Self->prvFile = objFile::create::integral(fl::Path(path), fl::Flags(FL::READ|FL::APPROXIMATE)))) {
         log.warning("Failed to open file \"%s\".", path);
         return ERR_File;
      }
   }

   // Read the JPEG file

   acSeek(Self->prvFile, 0.0, SEEK_START);

   auto bmp = Self->Bitmap;
   cinfo.err = jpeg_std_error((struct jpeg_error_mgr *)&jerr);
   jpeg_create_decompress(&cinfo);
   jpeg_stdio_src(&cinfo, Self->prvFile);
   jpeg_read_header(&cinfo, TRUE);

   if (!bmp->Width)          bmp->Width          = cinfo.image_width;
   if (!bmp->Height)         bmp->Height         = cinfo.image_height;
   if (!Self->DisplayWidth)  Self->DisplayWidth  = bmp->Width;
   if (!Self->DisplayHeight) Self->DisplayHeight = bmp->Height;
   if (!bmp->Type)           bmp->Type           = BMP_CHUNKY;
   if (!bmp->BitsPerPixel)   bmp->BitsPerPixel   = 32;

   if ((Self->Flags & PCF_NO_PALETTE) and (bmp->BitsPerPixel <= 8)) {
      bmp->BitsPerPixel = 32;
   }

   if (!acQuery(bmp)) {
      if (InitObject(bmp) != ERR_Okay) {
         jpeg_destroy_decompress(&cinfo);
         return ERR_Init;
      }
   }
   else {
      jpeg_destroy_decompress(&cinfo);
      return ERR_Query;
   }

   if (Self->Flags & PCF_RESIZE_X) cinfo.output_width = bmp->Width;
   if (Self->Flags & PCF_RESIZE_Y) cinfo.output_height = bmp->Height;

   if (bmp->BitsPerPixel >= 24) {
      decompress_jpeg(Self, bmp, &cinfo);
   }
   else {
      log.trace("Dest BPP of %d requires dithering.", bmp->BitsPerPixel);

      objBitmap::create tmp = { fl::Width(bmp->Width), fl::Height(bmp->Height), fl::BitsPerPixel(24) };
      if (tmp.ok()) {
         decompress_jpeg(Self, *tmp, &cinfo);
         gfxCopyArea(*tmp, bmp, BAF_DITHER, 0, 0, bmp->Width, bmp->Height, 0, 0);
      }
   }

   FreeResource(Self->prvFile);
   Self->prvFile = NULL;

   return ERR_Okay;
}

static void decompress_jpeg(extPicture *Self, objBitmap *Bitmap, struct jpeg_decompress_struct *Cinfo)
{
   pf::Log log;
   RGB8 rgb;

   jpeg_start_decompress(Cinfo);

   log.trace("Unpacking data to a %dbpp Bitmap...", Bitmap->BitsPerPixel);

   LONG row_stride = Cinfo->output_width * Cinfo->output_components;
   JSAMPARRAY buffer = (*Cinfo->mem->alloc_sarray)((j_common_ptr) Cinfo, JPOOL_IMAGE, row_stride, 1);
   for (JDIMENSION y=0; Cinfo->output_scanline < Cinfo->output_height; y++) {
      jpeg_read_scanlines(Cinfo, buffer, 1);
      JSAMPROW row = buffer[0];

      if (Cinfo->output_components IS 3) {
         for (JDIMENSION x=0; x < Cinfo->output_width; x++) {
            rgb.Red   = *row++;
            rgb.Green = *row++;
            rgb.Blue  = *row++;
            rgb.Alpha = 255;
            Bitmap->DrawUCRPixel(Bitmap, x, y, &rgb);
         }
      }
      else if (Cinfo->out_color_space IS JCS_RGB) {
         for (JDIMENSION x=0; x < Cinfo->output_width; x++) {
            WORD i = *row++;
            rgb.Red   = GETJSAMPLE(Cinfo->colormap[0][i]);
            rgb.Green = GETJSAMPLE(Cinfo->colormap[1][i]);
            rgb.Blue  = GETJSAMPLE(Cinfo->colormap[2][i]);
            rgb.Alpha = 255;
            Bitmap->DrawUCRPixel(Bitmap, x, y, &rgb);
         }
      }
      else {
         // Greyscale
         for (JDIMENSION x=0; x < Cinfo->output_width; x++) {
            rgb.Red = rgb.Green = rgb.Blue = *row++;
            rgb.Alpha = 255;
            Bitmap->DrawUCRPixel(Bitmap, x, y, &rgb);
         }
      }
   }

   log.trace("Decompression complete.");
   jpeg_finish_decompress(Cinfo);
   jpeg_destroy_decompress(Cinfo);
}

/*********************************************************************************************************************
** Picture: Init
*/

static ERROR JPEG_Init(extPicture *Self, APTR Void)
{
   pf::Log log;
   UBYTE *buffer;
   STRING path = NULL;

   Self->get(FID_Location, &path);

   if ((!path) or (Self->Flags & PCF_NEW)) {
      // If no location has been specified, assume that the picture is being created from scratch (e.g. to save an image to disk).  The
      // programmer is required to specify the dimensions and colours of the Bitmap so that we can initialise it.

      if (!Self->Bitmap->Width) Self->Bitmap->Width = Self->DisplayWidth;
      if (!Self->Bitmap->Height) Self->Bitmap->Height = Self->DisplayHeight;

      if ((Self->Bitmap->Width) and (Self->Bitmap->Height)) {
         if (!InitObject(Self->Bitmap)) {
            return ERR_Okay;
         }
         else return log.warning(ERR_Init);
      }
      else return log.warning(ERR_FieldNotSet);
   }
   else if (!Self->getPtr(FID_Header, &buffer)) {
      if ((buffer[0] IS 0xff) and (buffer[1] IS 0xd8) and (buffer[2] IS 0xff) and
          ((buffer[3] IS 0xe0) or (buffer[3] IS 0xe1) or (buffer[3] IS 0xfe))) {
         log.msg("The file is a JPEG picture.");
         if (!(Self->Flags & PCF_LAZY)) acActivate(Self);
         return ERR_Okay;
      }
      else log.msg("The file is not a JPEG picture.");
   }

   return ERR_NoSupport;
}

//********************************************************************************************************************

static ERROR JPEG_Query(extPicture *Self, APTR Void)
{
   pf::Log log;
   struct jpeg_decompress_struct *cinfo;
   struct jpeg_error_mgr jerr;

   log.branch();

   if (!Self->prvFile) {
      STRING path;
      if (Self->get(FID_Location, &path) != ERR_Okay) return log.warning(ERR_GetField);

      if (!(Self->prvFile = objFile::create::integral(fl::Path(path), fl::Flags(FL::READ|FL::APPROXIMATE)))) {
         return log.warning(ERR_CreateObject);
      }
   }

   acSeek(Self->prvFile, 0.0, SEEK_START);
   if (!AllocMemory(sizeof(struct jpeg_decompress_struct), MEM_DATA, &cinfo)) {
      auto bmp = Self->Bitmap;
      cinfo->err = jpeg_std_error((struct jpeg_error_mgr *)&jerr);
      jpeg_create_decompress(cinfo);
      jpeg_stdio_src(cinfo, Self->prvFile);
      jpeg_read_header(cinfo, FALSE);

      if (!bmp->Width)          bmp->Width          = cinfo->image_width;
      if (!bmp->Height)         bmp->Height         = cinfo->image_height;
      if (!Self->DisplayWidth)  Self->DisplayWidth  = bmp->Width;
      if (!Self->DisplayHeight) Self->DisplayHeight = bmp->Height;
      if (!bmp->Type)           bmp->Type           = BMP_CHUNKY;
      if (!bmp->BitsPerPixel) {
         bmp->BitsPerPixel = 24;
         bmp->BytesPerPixel = 3;
      }

      jpeg_destroy_decompress(cinfo);
      FreeResource(cinfo);
      return acQuery(bmp);
   }
   else return log.warning(ERR_Memory);
}

/*********************************************************************************************************************
** Picture: SaveImage
*/

static ERROR JPEG_SaveImage(extPicture *Self, struct acSaveImage *Args)
{
   pf::Log log;

   log.branch();

   OBJECTPTR file = NULL;

   if ((Args) and (Args->Dest)) file = Args->Dest;
   else {
      STRING path;
      if (Self->get(FID_Location, &path) != ERR_Okay) return log.warning(ERR_MissingPath);

      if (!(file = objFile::create::integral(fl::Path(path), fl::Flags(FL::NEW|FL::WRITE)))) {
         return log.warning(ERR_CreateObject);
      }
   }

   // Allocate jpeg structures

   struct jpeg_compress_struct cinfo;
   struct jpeg_error_mgr jerr;
   cinfo.err = jpeg_std_error((struct jpeg_error_mgr *)&jerr);
   jpeg_create_compress(&cinfo);
   jpeg_stdio_dest(&cinfo, (objFile *)file);

   cinfo.image_width      = Self->Bitmap->Width; 	// image width and height, in pixels
   cinfo.image_height     = Self->Bitmap->Height;
   cinfo.input_components = 3;	// # of color components per pixel
   cinfo.in_color_space   = JCS_RGB; // colorspace of input image
   jpeg_set_defaults(&cinfo);

   jpeg_set_quality(&cinfo, (Self->Quality * 255) / 100, TRUE); // Quality is between 1 and 255 (lower is better)

   jpeg_start_compress(&cinfo, TRUE);

   {
      UBYTE buffer[3 * Self->Bitmap->Width];
      JSAMPROW row_pointer[1];
      RGB8 rgb;

      for (LONG y=0; y < Self->Bitmap->Height; y++) {
         row_pointer[0] = buffer;
         WORD index = 0;
         for (LONG x=0; x < Self->Bitmap->Width; x++) {
            Self->Bitmap->ReadUCRPixel(Self->Bitmap, x, y, &rgb);
            buffer[index++] = rgb.Red;
            buffer[index++] = rgb.Green;
            buffer[index++] = rgb.Blue;
         }
         jpeg_write_scanlines(&cinfo, row_pointer, 1);
      }
   }

   jpeg_finish_compress(&cinfo);
   jpeg_destroy_compress(&cinfo);

   if (file) {
      if ((Args) and (Args->Dest));
      else FreeResource(file);
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ActionArray clActions[] = {
   { AC_Activate,  JPEG_Activate },
   { AC_Init,      JPEG_Init },
   { AC_Query,     JPEG_Query },
   { AC_SaveImage, JPEG_SaveImage },
   { 0, NULL }
};

//********************************************************************************************************************

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   if (objModule::load("display", MODVERSION_DISPLAY, &modDisplay, &DisplayBase) != ERR_Okay) return ERR_InitModule;

   objModule::create pic = { fl::Name("picture") }; // Load our dependency ahead of class registration

   clJPEG = objMetaClass::create::global(
      fl::BaseClassID(ID_PICTURE),
      fl::ClassID(ID_JPEG),
      fl::Name("JPEG"),
      fl::Category(CCF::GRAPHICS),
      fl::FileExtension("*.jpeg|*.jpeg|*.jfif"),
      fl::FileDescription("JPEG Picture"),
      fl::FileHeader("[0:$ffd8ffe0]|[0:$ffd8ffe1]|[0:$ffd8fffe]"),
      fl::Actions(clActions),
      fl::Path(MOD_PATH));

   return clJPEG ? ERR_Okay : ERR_AddClass;
}

//********************************************************************************************************************

static ERROR CMDExpunge(void)
{
   if (modDisplay) { FreeResource(modDisplay); modDisplay = NULL; }
   if (clJPEG)     { FreeResource(clJPEG);     clJPEG = NULL; }
   return ERR_Okay;
}

//********************************************************************************************************************

PARASOL_MOD(CMDInit, NULL, NULL, CMDExpunge, 1.0, MOD_IDL, NULL)
