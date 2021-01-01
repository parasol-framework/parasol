/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
Image: Draws images to surface areas.

The Image class is designed for object scripting and closely follows the general definition of the 'img' tag in HTML.
The main difference that you will notice is that it sports a few extra effects, and you have complete control over
the positioning of the image graphic.

There are no restrictions on the data format of the picture file, but it must be supported by one of the
@Picture classes in the system.  If for example the file format is JPEG, but the system does not have a JPEG
Picture class installed, it will not be possible to load the file.  To obtain a list of supported file formats, you
need to scan the list of @Picture classes in the Graphics category.

All pictures that are loaded via the Image class are cached into a shared memory pool.  If an image file is loaded
multiple times by a program, the data will be stored only once to save on memory and load times. Image files are
automatically unloaded when their reference count reaches zero.

-END-

*****************************************************************************/

//#define DEBUG

#define PRV_IMAGE
#define PRV_WIDGET_MODULE
#include <parasol/main.h>
#include <parasol/modules/picture.h>
#include <parasol/modules/display.h>
#include <parasol/modules/surface.h>
#include <parasol/modules/widget.h>
#include "defs.h"

static OBJECTPTR clImage = NULL;
static UBYTE glSixBit = FALSE;

static void draw_image(objImage *, objSurface *, objBitmap *);
static void get_image_size(objBitmap *, LONG, LONG, LONG, LONG *, LONG *);
static ERROR load_picture(objImage *);
static void resample_image(objImage *, OBJECTID, LONG, LONG, LONG);
static void render_script(objImage *, STRING);
static void calc_pic_size(objImage *, LONG, LONG);
static ERROR frame_timer(objImage *, LARGE, LARGE);

void free_image(void)
{
   if (clImage) { acFree(clImage); clImage = NULL; }
}

//****************************************************************************

static void resize_surface(objImage *Self)
{
   objBitmap *srcbitmap;
   if (!(srcbitmap = Self->RawBitmap)) srcbitmap = Self->Bitmap;
   if (!srcbitmap) return;

   if ((Self->Layout->BitsPerPixel) and (Self->Layout->BitsPerPixel != srcbitmap->BitsPerPixel)) {
      resample_image(Self, 0, Self->Layout->BoundWidth, Self->Layout->BoundHeight, Self->Layout->BitsPerPixel);
   }
   else resample_image(Self, 0, Self->Layout->BoundWidth, Self->Layout->BoundHeight, srcbitmap->BitsPerPixel);
}

/*****************************************************************************
-ACTION-
DataFeed: Accepts script code for dynamic rendering of the source image.
-END-
*****************************************************************************/

static ERROR IMAGE_DataFeed(objImage *Self, struct acDataFeed *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Args->DataType IS DATA_XML) {
      if (Self->RenderString) FreeResource(Self->RenderString);
      Self->RenderString = StrClone((CSTRING)Args->Buffer);

      render_script(Self, Self->RenderString);
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR IMAGE_Free(objImage *Self, APTR Void)
{
   if (Self->FrameTimer) { UpdateTimer(Self->FrameTimer, 0); Self->FrameTimer = 0; }

   if (Self->Picture) {
      if (Self->Bitmap IS Self->Picture->Bitmap) Self->Bitmap = NULL;
      if (Self->RawBitmap IS Self->Picture->Bitmap) Self->RawBitmap = NULL;

      acFree(Self->Picture);
      Self->Picture = NULL;
   }

   if (Self->Path)         { FreeResource(Self->Path); Self->Path = NULL; }
   if (Self->RenderString) { FreeResource(Self->RenderString); Self->RenderString = NULL; }
   if (Self->Bitmap)       { acFree(Self->Bitmap); Self->Bitmap = NULL; }
   if (Self->RawBitmap)    { acFree(Self->RawBitmap); Self->RawBitmap = NULL; }
   if (Self->Layout)       { acFree(Self->Layout); Self->Layout = NULL; }
   if (Self->Hint)         { FreeResource(Self->Hint); Self->Hint = NULL; }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Hide: Hides an image from view.
-END-
*****************************************************************************/

static ERROR IMAGE_Hide(objImage *Self, APTR Void)
{
   return acHide(Self->Layout);
}

//****************************************************************************

static ERROR IMAGE_Init(objImage *Self, APTR Void)
{
   parasol::Log log;

   SetFunctionPtr(Self->Layout, FID_DrawCallback, (APTR)&draw_image);
   SetFunctionPtr(Self->Layout, FID_ResizeCallback, (APTR)&resize_surface);
   if (acInit(Self->Layout) != ERR_Okay) return ERR_Init;

   if ((!Self->Path) and (!(Self->Flags & IMF_NO_FAIL))) return log.warning(ERR_MissingPath);

   if (!Self->Path) {
      if (!(Self->Flags & IMF_NO_FAIL)) return log.warning(ERR_GetField);
   }

   ERROR error = load_picture(Self);

   if (!error) {
      if ((Self->Picture) and (Self->Picture->FrameRate > 0)) {
         log.msg("Picture frame rate: %dfps", Self->Picture->FrameRate);
         Self->FrameRate = Self->Picture->FrameRate;

         FUNCTION callback;
         SET_FUNCTION_STDC(callback, (APTR)&frame_timer);
         SubscribeTimer(1.0/(DOUBLE)Self->FrameRate, &callback, &Self->FrameTimer);
      }
   }

   return error;
}

/*****************************************************************************
-ACTION-
Move: Moves the image to a new position.
-END-
*****************************************************************************/

static ERROR IMAGE_Move(objImage *Self, struct acMove *Args)
{
   if (!Args) return ERR_NullArgs;
   if (Self->Flags & IMF_STICKY) return ERR_Okay;
   if ((!Args->XChange) and (!Args->YChange)) return ERR_Okay;

   Self->Layout->X -= Args->XChange;
   Self->Layout->Y -= Args->YChange;

   Self->Layout->Dimensions = (Self->Layout->Dimensions & ~DMF_RELATIVE_X) | DMF_FIXED_X;
   Self->Layout->Dimensions = (Self->Layout->Dimensions & ~DMF_RELATIVE_Y) | DMF_FIXED_Y;

   acDrawID(Self->Layout->SurfaceID);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
MoveToFront: Brings the image graphics to the front of the surface.
-END-
*****************************************************************************/

static ERROR IMAGE_MoveToFront(objImage *Self, APTR Void)
{
   return acMoveToFront(Self->Layout);
}

/*****************************************************************************
-ACTION-
MoveToPoint: Moves the image to a new position.
-END-
*****************************************************************************/

static ERROR IMAGE_MoveToPoint(objImage *Self, struct acMoveToPoint *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Self->Flags & IMF_STICKY) return ERR_Okay;

   LONG oldx = Self->Layout->X;
   LONG oldy = Self->Layout->Y;

   if (Args->Flags & MTF_X) {
      Self->Layout->X = Args->X;
      Self->Layout->Dimensions = (Self->Layout->Dimensions & ~DMF_RELATIVE_X) | DMF_FIXED_X;
   }

   if (Args->Flags & MTF_Y) {
      Self->Layout->Y = Args->Y;
      Self->Layout->Dimensions = (Self->Layout->Dimensions & ~DMF_RELATIVE_Y) | DMF_FIXED_Y;
   }

   if ((oldx != Self->Layout->X) or (oldy != Self->Layout->Y)) ActionMsg(AC_Draw, Self->Layout->SurfaceID, NULL);

   return ERR_Okay;
}

//****************************************************************************

static ERROR IMAGE_NewObject(objImage *Self, APTR Void)
{
   Self->Opacity = 255;
   Self->FrameRate = 50;

   if (!NewObject(ID_LAYOUT, NF_INTEGRAL, &Self->Layout)) {
      return ERR_Okay;
   }
   else return ERR_NewObject;
}

/*****************************************************************************
-ACTION-
ScrollToPoint: Scrolls an image within its allocated drawing space.
-END-
*****************************************************************************/

static ERROR IMAGE_ScrollToPoint(objImage *Self, struct acScrollToPoint *Args)
{
   if (!Args) return ERR_NullArgs;

   if ((Args->X IS Self->Layout->GraphicX) and (Args->Y IS Self->Layout->GraphicY)) return ERR_Okay;

   objSurface *surface;
   if (!AccessObject(Self->Layout->SurfaceID, 5000, &surface)) {
      LONG x, y;
      if (Args->Flags & STP_X) x = -Args->X;
      else x = Self->Layout->GraphicX;

      if (Args->Flags & STP_Y) y = -Args->Y;
      else y = Self->Layout->GraphicY;

      Self->Layout->GraphicX = x;
      Self->Layout->GraphicY = y;

      acDraw(surface);

      ReleaseObject(surface);
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Show: Shows an image.
-END-
*****************************************************************************/

static ERROR IMAGE_Show(objImage *Self, APTR Void)
{
   return acShow(Self->Layout);
}

/*****************************************************************************

-FIELD-
Background: Defines a background colour for the image.

A background colour may be defined for an image.  Use of a background will clear everything behind the image where
there are transparent areas, as well as clearing the borders surrounding the image.

-FIELD-
Flags: Optional flags can be defined here.
Lookup: IMF

Optional image flags can be defined here.  In addition to the standard options, note that images can be tiled by
using LAYOUT_TILE in the Layout object; and alignment can be managed from the @Layout.Align field.

*****************************************************************************/

static ERROR SET_Flags(objImage *Self, LONG Value)
{
   Self->Flags = (Self->Flags & 0xffff0000) | (Value & 0x0000ffff);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Frame: Determines the frame that an image will be applied to.

Setting the Frame field to any value other than zero will force the Image to be drawn only when the surface's frame
matches the specified value.  For instance, if the surface container has a Frame setting of 2, and the Image has a
Frame of 1, then the Image graphic will not be drawn as the numbers do not match.

-FIELD-
FrameRate: Indicates the frame rate to use for animated image scrolling.

This field defines the frame rate to use for image-based animations.  The default setting is for 50 frames per second,
which is the maximum that we recommend.

*****************************************************************************/

static ERROR SET_FrameRate(objImage *Self, LONG Value)
{
   if ((Value > 0) and (Value <= 1000)) {
      Self->FrameRate = Value;
      return ERR_Okay;
   }
   else return ERR_OutOfRange;
}

/*****************************************************************************

-FIELD-
Hint: Defines a user hint to be automatically displayed if the pointer hovers on the image.

If the mouse pointer hovers over the image, the text defined in this field will be displayed near the image.  It will
remain on the display until the pointer is moved by the user.

*****************************************************************************/

static ERROR SET_Hint(objImage *Self, CSTRING Value)
{
   if (Self->Hint) { FreeResource(Self->Hint); Self->Hint = NULL; }
   Self->Hint = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
IconFilter: Sets the preferred icon filter.

Setting the IconFilter will change the default graphics filter when loading an icon (identified when using the 'icons:'
volume name).

*****************************************************************************/

static ERROR GET_IconFilter(objImage *Self, STRING *Value)
{
   if (Self->IconFilter[0]) *Value = Self->IconFilter;
   else *Value = NULL;
   return ERR_Okay;
}

static ERROR SET_IconFilter(objImage *Self, CSTRING Value)
{
   if (!Value) Self->IconFilter[0] = 0;
   else StrCopy(Value, Self->IconFilter, sizeof(Self->IconFilter));
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
IconTheme: Sets the preferred icon theme.

Setting the IconTheme will define the default theme used when loading an icon (identified when using the 'icons:'
volume name).

*****************************************************************************/

static ERROR GET_IconTheme(objImage *Self, STRING *Value)
{
   if (Self->IconTheme[0]) *Value = Self->IconTheme;
   else *Value = NULL;
   return ERR_Okay;
}

static ERROR SET_IconTheme(objImage *Self, CSTRING Value)
{
   if (!Value) Self->IconTheme[0] = 0;
   else StrCopy(Value, Self->IconTheme, sizeof(Self->IconTheme));
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Path: Identifies the location of the image graphic file (e.g. pcx, jpeg, gif).

The location of the picture file that is to be used for the Image must be specified in this field.  If the location is
not specified then the initialisation process will fail.  The file format must be recognised by at least one of the
Picture classes loaded into the system.

*****************************************************************************/

static ERROR GET_Path(objImage *Self, STRING *Value)
{
   *Value = Self->Path;
   return ERR_Okay;
}

static ERROR SET_Path(objImage *Self, CSTRING Value)
{
   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }
   if ((Value) and (*Value)) Self->Path = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Mask: Identifies the colour to use for masking an image (string format).

If the image source uses a masking colour to define transparent areas, set this field so that the image object knows
what the masking colour is. The mask must be specified in hexadecimal or separated-decimal format - for example a pure
red mask would be defined as `#ff0000` or `255,0,0`.

-FIELD-
Opacity: Determines the level of translucency applied to an image.

This field determines the translucency level of an image.  The default setting is 100%, which means that the image will
be solid.  Any other value that you set here will alter the impact of an image over the destination surface.  High
values will retain the boldness of the image, while low values reduce visibility.

Please note that the use of translucency will always have an impact on the time it normally takes to draw an image.
The use of translucency also requires that the surface area is buffered, as read access is required to perform the
blending algorithm.

****************************************************************************/

static ERROR GET_Opacity(objImage *Self, DOUBLE *Value)
{
   *Value = Self->Opacity * 100 / 255;
   return ERR_Okay;
}

static ERROR SET_Opacity(objImage *Self, DOUBLE Value)
{
   if (Value < 0) Self->Opacity = 0;
   else if (Value > 100) Self->Opacity = 255;
   else Self->Opacity = Value * 255 / 100;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
PixelSize: Reflects the pixel size of the image at its widest point (horizontally or vertically).

*****************************************************************************/

static ERROR GET_PixelSize(objImage *Self, LONG *Value)
{
   if (Self->Layout->GraphicWidth > Self->Layout->GraphicHeight) *Value = Self->Layout->GraphicWidth;
   else *Value = Self->Layout->GraphicHeight;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Tile: Set this field to TRUE to turn on image tiling (wallpaper).

To tile the Image within its container (also known as the wallpaper effect), set this field to TRUE.  It works in
conjunction with the LAYOUT_TILE option in the Layout object.
-END-

*****************************************************************************/

static ERROR GET_Tile(objImage *Self, LONG *Value)
{
   if (Self->Layout->Layout & LAYOUT_TILE) *Value = TRUE;
   else *Value = FALSE;
   return ERR_Okay;
}

static ERROR SET_Tile(objImage *Self, LONG Value)
{
   if (Value IS TRUE) Self->Layout->Layout |= LAYOUT_TILE;
   else Self->Layout->Layout &= ~LAYOUT_TILE;
   return ERR_Okay;
}

/*****************************************************************************
** Bitmap: Source bitmap.
** Width/Height: New width and height that is being requested.
** ImageWidth/ImageHeight: Computed width and height based on flag options.
*/

static void get_image_size(objBitmap *Bitmap, LONG Flags, LONG Width, LONG Height, LONG *ImageWidth, LONG *ImageHeight)
{
   parasol::Log log(__FUNCTION__);

   *ImageWidth = Bitmap->Width;
   *ImageHeight = Bitmap->Height;

   if ((!Width) and (!Height)) return;

   if (Flags & IMF_ENLARGE) {
      if (Flags & IMF_11_RATIO) {
         DOUBLE hratio = (DOUBLE)Width / (DOUBLE)Bitmap->Width;
         DOUBLE vratio = (DOUBLE)Height / (DOUBLE)Bitmap->Height;

         if (Flags & IMF_FIT) { // Enlarge the image to fit the display
            if (hratio > vratio) hratio = vratio;
         }
         else { // Enlarge the image to cover the entirety of the display
            if (hratio < vratio) hratio = vratio;
         }

         if (hratio >= 1.0) {
            *ImageWidth  = F2T((DOUBLE)Bitmap->Width * hratio);
            *ImageHeight = F2T((DOUBLE)Bitmap->Height * hratio);
         }
      }
      else {
         if (*ImageWidth < Width) *ImageWidth = Width;
         if (*ImageHeight < Height) *ImageHeight = Height;
      }
   }

   if (Flags & IMF_SHRINK) {
      if (Flags & IMF_11_RATIO) {
         DOUBLE hratio = (DOUBLE)Width / (DOUBLE)Bitmap->Width;
         DOUBLE vratio = (DOUBLE)Height / (DOUBLE)Bitmap->Height;

         if (Flags & IMF_FIT) { // Shrink the image to fit the display
            if (hratio > vratio) hratio = vratio;
         }
         else { // Shrink the image to cover the entirety of the display
            if (hratio < vratio) hratio = vratio;
         }

         if (hratio <= 1.0) {
            *ImageWidth  = F2T((DOUBLE)Bitmap->Width * hratio);
            *ImageHeight = F2T((DOUBLE)Bitmap->Height * hratio);
         }
      }
      else {
         if (*ImageWidth > Width) *ImageWidth = Width;
         if (*ImageHeight > Height) *ImageHeight = Height;
      }
   }

   if (!(Flags & (IMF_SHRINK|IMF_ENLARGE))) {
      if (Flags & IMF_11_RATIO) {
         if ((Width) and (!Height)) {
            Height = F2T(((DOUBLE)Width / (DOUBLE)Bitmap->Width) * Bitmap->Height);
         }
         else if ((Height) and (!Width)) {
            Width = F2T(((DOUBLE)Height / (DOUBLE)Bitmap->Height) * Bitmap->Width);
         }
      }
   }

   log.trace("Bitmap: %d $%.8x, Req: %dx%d, Bmp: %dx%d, Result: %dx%d", Bitmap->Head.UniqueID, Flags, Width, Height, Bitmap->Width, Bitmap->Height, *ImageWidth, *ImageHeight);
}

//****************************************************************************

static void draw_image(objImage *Self, objSurface *Surface, objBitmap *Bitmap)
{
   parasol::Log log(__FUNCTION__);

   if (Self->Layout->Visible IS FALSE) return;

   log.trace("Pos: %dx%d, Area: %dx%d,%dx%d", Self->Layout->GraphicX, Self->Layout->GraphicY, Self->Layout->BoundX, Self->Layout->BoundY, Self->Layout->BoundWidth, Self->Layout->BoundHeight);

   if ((Bitmap->Clip.Right <= Self->Layout->BoundX) or (Bitmap->Clip.Top >= Self->Layout->BoundY+Self->Layout->BoundHeight) or
       (Bitmap->Clip.Bottom <= Self->Layout->BoundY) or (Bitmap->Clip.Left >= Self->Layout->BoundX+Self->Layout->BoundWidth)) return;

   // Determine whether or not we need to draw this image based on our object's frame settings.

   if (Self->Frame) {
      if (Surface->Frame != Self->Frame) return;
   }

   objBitmap *picbitmap, *newbitmap;
   if (Self->Bitmap) picbitmap = Self->Bitmap;
   else picbitmap = Self->RawBitmap;

   if (!picbitmap) {
      // If no picture is available, the only thing that we are capable of doing is clearing the background.

      if (Self->Background.Alpha > 0) {
         gfxDrawRectangle(Bitmap, Self->Layout->BoundX, Self->Layout->BoundY, Self->Layout->BoundWidth,
            Self->Layout->BoundHeight, PackPixelRGBA(Bitmap, &Self->Background), BAF_FILL);
      }

      return;
   }

   if ((picbitmap->BitsPerPixel != Bitmap->BitsPerPixel) and (!(picbitmap->Flags & BMF_ALPHA_CHANNEL))) {
      // Resample the bitmap to the destination surface due to display depth changes.

      if (!CreateObject(ID_BITMAP, NF_INTEGRAL, &newbitmap,
            FID_Width|TLONG,        picbitmap->Width,
            FID_Height|TLONG,       picbitmap->Height,
            FID_Flags|TLONG,        picbitmap->Flags,
            FID_BitsPerPixel|TLONG, Bitmap->BitsPerPixel,
            TAGEND)) {

         gfxCopyArea(picbitmap, newbitmap, BAF_DITHER, 0, 0, picbitmap->Width, picbitmap->Height, 0, 0);

         if (Self->Bitmap) acFree(Self->Bitmap);
         Self->Bitmap = newbitmap;
         picbitmap = newbitmap;
      }
   }

   // Activate alpha-blending to the destination if the picture supports an alpha channel and our blend flag has been set.

   if (picbitmap->Flags & BMF_ALPHA_CHANNEL) {
      if ((Self->Flags & IMF_NO_BLEND) and (Surface->Flags & RNF_COMPOSITE)) {
         // Do nothing if the surface is a composite and noblend is enabled
      }
   }

   ClipRectangle clip = Bitmap->Clip; // Save current clipping boundary

   if (Bitmap->Clip.Left < Self->Layout->BoundX) Bitmap->Clip.Left = Self->Layout->BoundX;
   if (Bitmap->Clip.Top  < Self->Layout->BoundY) Bitmap->Clip.Top  = Self->Layout->BoundY;
   if (Bitmap->Clip.Right  > Self->Layout->BoundX + Self->Layout->BoundWidth)  Bitmap->Clip.Right = Self->Layout->BoundX + Self->Layout->BoundWidth;
   if (Bitmap->Clip.Bottom > Self->Layout->BoundY + Self->Layout->BoundHeight) Bitmap->Clip.Bottom = Self->Layout->BoundY + Self->Layout->BoundHeight;

   WORD opacity = picbitmap->Opacity;
   picbitmap->Opacity = Self->Opacity;

   // Draw the image

   LONG x, y, imagex, imagey;
   if (Self->Layout->Layout & LAYOUT_TILE) {
      y = Self->Layout->BoundY;

      LONG offx = Self->Layout->BoundX;
      LONG vstop = Self->Layout->BoundY + Self->Layout->BoundHeight;
      LONG hstop = Self->Layout->BoundX + Self->Layout->BoundWidth;

      if (vstop > Bitmap->Clip.Bottom) vstop = Bitmap->Clip.Bottom;
      if (hstop > Bitmap->Clip.Right) hstop = Bitmap->Clip.Right;

      for (; y < vstop; y += Self->Layout->GraphicHeight) {
         LONG bh = Self->Layout->GraphicHeight;
         if (y + bh > vstop) bh = vstop - y;

         for (x=offx; x < hstop; x += Self->Layout->GraphicWidth) {
            LONG bw = Self->Layout->GraphicWidth;
            if (x + bw > hstop) bw = hstop - x;
            gfxCopyArea(picbitmap, Bitmap, (Self->Flags & IMF_NO_BLEND) ? 0 : BAF_BLEND, 0, 0, bw, bh, x, y);
         }
      }
   }
   else {
      if (Self->Layout->Align & ALIGN_HORIZONTAL) imagex = Self->Layout->BoundX + ((Self->Layout->BoundWidth - Self->Layout->GraphicWidth) / 2);
      else if (Self->Layout->Align & ALIGN_RIGHT) imagex = Self->Layout->BoundX + (Self->Layout->BoundWidth  - Self->Layout->GraphicWidth);
      else imagex = Self->Layout->GraphicX + Self->Layout->BoundX;

      if (Self->Layout->Align & ALIGN_VERTICAL)    imagey = Self->Layout->BoundY + ((Self->Layout->BoundHeight - Self->Layout->GraphicHeight) / 2);
      else if (Self->Layout->Align & ALIGN_BOTTOM) imagey = Self->Layout->BoundY + (Self->Layout->BoundHeight  - Self->Layout->GraphicHeight);
      else imagey = Self->Layout->GraphicY + Self->Layout->BoundY;

      if (Self->Layout->Layout & LAYOUT_LOCK) {
         // In lock mode, the image is placed relative to the viewing surface and not the page.  Thus when the page is
         // scrolled, the image stays locked in its display position.

         imagex -= Surface->X;
         imagey -= Surface->Y;
      }

      if (Self->Background.Alpha > 0) {
         ULONG bkgd = PackPixelRGBA(Bitmap, &Self->Background);
         if ((picbitmap->Flags & (BMF_ALPHA_CHANNEL|BMF_TRANSPARENT)) or
             ((picbitmap->Width < 64) and (picbitmap->Height < 64))) {
            gfxDrawRectangle(Bitmap, Self->Layout->BoundX, Self->Layout->BoundY, Self->Layout->BoundWidth,
               Self->Layout->BoundHeight, bkgd, BAF_FILL);
         }
         else {
            if (imagey > Self->Layout->BoundY) { // Top
               gfxDrawRectangle(Bitmap, Self->Layout->BoundX, Self->Layout->BoundY, Self->Layout->BoundWidth,
                  imagey - Self->Layout->BoundY, bkgd, BAF_FILL);
            }

            if (imagey + Self->Layout->GraphicHeight < Self->Layout->BoundY + Self->Layout->BoundHeight) { // Bottom
               gfxDrawRectangle(Bitmap, Self->Layout->BoundX, imagey + Self->Layout->GraphicHeight,
                 Self->Layout->BoundWidth,
                 (Self->Layout->BoundY + Self->Layout->BoundHeight) - (imagey + Self->Layout->GraphicHeight),
                 bkgd, BAF_FILL);
            }

            if (imagex > Self->Layout->BoundX) { // Left
               gfxDrawRectangle(Bitmap, Self->Layout->BoundX, imagey, imagex - Self->Layout->BoundX,
                  Self->Layout->GraphicHeight, bkgd, BAF_FILL);
            }

            if (imagex + Self->Layout->GraphicWidth < Self->Layout->BoundX + Self->Layout->BoundWidth) { // Right
               gfxDrawRectangle(Bitmap, imagex + Self->Layout->GraphicWidth, imagey,
                 (Self->Layout->BoundX + Self->Layout->BoundWidth) - (imagex + Self->Layout->GraphicWidth),
                 Self->Layout->GraphicHeight, bkgd, BAF_FILL);
            }
         }
      }

      if ((Self->Layout->GraphicWidth != picbitmap->Width) or (Self->Layout->GraphicHeight != picbitmap->Height)) {
         gfxCopyStretch(picbitmap, Bitmap, CSTF_BILINEAR, 0, 0,
            picbitmap->Width, picbitmap->Height, imagex, imagey, Self->Layout->GraphicWidth, Self->Layout->GraphicHeight);
      }
      else gfxCopyArea(picbitmap, Bitmap, (Self->Flags & IMF_NO_BLEND) ? 0 : BAF_BLEND, 0, 0, Self->Layout->GraphicWidth, Self->Layout->GraphicHeight, imagex, imagey);
   }

   picbitmap->Opacity = opacity;

   Bitmap->Clip = clip;
}

//****************************************************************************

static void resample_image(objImage *Self, OBJECTID BufferID, LONG Width, LONG Height, LONG BitsPerPixel)
{
   parasol::Log log(__FUNCTION__);
   objBitmap *bitmap, *srcbitmap;
   LONG new_width, new_height;

   log.trace("resample_image()","Width: %d, Height: %d, BPP: %d", Width, Height, BitsPerPixel);

   if (Self->RenderString) {
      render_script(Self, Self->RenderString);
      return;
   }

   if ((Self->Picture) and (Self->Picture->Flags & PCF_SCALABLE)) {
      if ((Width != Self->Picture->Bitmap->Width) or (Height != Self->Picture->Bitmap->Height)) {
         acResize(Self->Picture, Width, Height, BitsPerPixel);
         Self->Layout->GraphicWidth  = Self->Picture->Bitmap->Width;
         Self->Layout->GraphicHeight = Self->Picture->Bitmap->Height;
      }
      return;
   }

   if (Self->Layout->GraphicRelWidth) new_width = F2T((DOUBLE)Self->Layout->GraphicRelWidth * (DOUBLE)Width);
   else new_width = Width;

   if (Self->Layout->GraphicRelHeight) new_height = F2T((DOUBLE)Self->Layout->GraphicRelHeight * (DOUBLE)Height);
   else new_height = Height;

   if (!(srcbitmap = Self->RawBitmap)) srcbitmap = Self->Bitmap;
   if (!srcbitmap) return;

   if (srcbitmap->BitsPerPixel IS BitsPerPixel) {
      // Use dynamic stretching because dithering is not required

      get_image_size(srcbitmap, Self->Flags, new_width, new_height, &new_width, &new_height);

      Self->Layout->GraphicWidth = new_width;
      Self->Layout->GraphicHeight = new_height;
   }
   else if (!Self->RawBitmap) {
      // The depth of the destination surface has changed (we know this because the RawBitmap is null but we have a
      // Bitmap already loaded), so we need to move the vanilla Bitmap to the RawBitmap pointer and then we can
      // dither it.

      log.msg("Surface depth changed - switching Bitmap to RawBitmap.");
      Self->RawBitmap = srcbitmap;
      Self->Bitmap = NULL;
   }

   get_image_size(srcbitmap, Self->Flags, new_width, new_height, &new_width, &new_height);

   // Check if we need to resample the image

   if (!(bitmap = Self->Bitmap)) bitmap = Self->RawBitmap;

   if ((BitsPerPixel IS bitmap->BitsPerPixel) or (bitmap->Flags & BMF_ALPHA_CHANNEL)) {
      if ((new_width IS bitmap->Width) and (new_height IS bitmap->Height)) return;
   }

   log.branch("Resizing bitmap %dx%d / %dx%d", Width, Height, new_width, new_height);

   // Decompress the original raw image bitmap

   if (!gfxDecompress(srcbitmap, TRUE)) {
      if (!Self->Bitmap) {
         if (CreateObject(ID_BITMAP, NF_INTEGRAL, &Self->Bitmap,
               FID_Width|TLONG,        new_width,
               FID_Height|TLONG,       new_height,
               FID_BitsPerPixel|TLONG, srcbitmap->BitsPerPixel,
               FID_Flags|TLONG,        srcbitmap->Flags,
               TAGEND) != ERR_Okay) {
            log.warning(ERR_CreateObject);
            return;
         }
      }

      // Resize our bitmap canvas to match new dimensions

      if (!acResize(Self->Bitmap, new_width, new_height, srcbitmap->BitsPerPixel)) {
         if ((srcbitmap->Palette) and (srcbitmap->BitsPerPixel <= 8)) {
            SetPointer(Self->Bitmap, FID_Palette, srcbitmap->Palette);
         }

         // Resize the image

         gfxCopyStretch(srcbitmap, Self->Bitmap, CSTF_BILINEAR, 0, 0, srcbitmap->Width, srcbitmap->Height, 0, 0,
            new_width, new_height);

         Self->Layout->GraphicWidth  = new_width;
         Self->Layout->GraphicHeight = new_height;

         // Dither the image / convert to correct bit depth unless it is going to be alpha-blended.

         if (BitsPerPixel != Self->Bitmap->BitsPerPixel) {
            if (Self->Bitmap->Flags & BMF_ALPHA_CHANNEL) {
               if (BitsPerPixel <= 16) {
                  ColourFormat format;
                  if ((BufferID) and (!AccessObject(BufferID, 1000, &bitmap))) {
                     CopyMemory(bitmap->ColourFormat, &format, sizeof(format));
                     ReleaseObject(bitmap);
                  }
                  else gfxGetColourFormat(&format, BitsPerPixel, 0, 0, 0, 0);

                  gfxResample(Self->Bitmap, &format);
               }
            }
            else if (!CreateObject(ID_BITMAP, NF_INTEGRAL, &bitmap,
                  FID_Width|TLONG,        new_width,
                  FID_Height|TLONG,       new_height,
                  FID_Flags|TLONG,        Self->Bitmap->Flags,
                  FID_BitsPerPixel|TLONG, BitsPerPixel,
                  TAGEND)) {

               gfxCopyArea(Self->Bitmap, bitmap, BAF_DITHER, 0, 0, new_width, new_height, 0, 0);

               acFree(Self->Bitmap);
               Self->Bitmap = bitmap;
            }
         }
         else if ((glSixBit) and (BitsPerPixel >= 24)) {
            ColourFormat format;
            gfxGetColourFormat(&format, 0, 0x3f, 0x3f, 0x3f, 0);
            gfxResample(Self->Bitmap, &format);
         }
      }
      else log.warning("Failed to resize bitmap for resampling.");

      // Recompress the original bitmap - since we didn't change anything, this will simply get rid of the raw data.

      gfxCompress(srcbitmap, 0);
   }
}

//****************************************************************************

static ERROR frame_timer(objImage *Self, LARGE Elapsed, LARGE CurrentTime)
{
   if ((Self->Picture) and (Self->Picture->FrameRate)) {
      acRefresh(Self->Picture);
   }

   if (!(Self->Flags & IMF_NO_DRAW)) {
      struct acDraw draw = { Self->Layout->BoundX, Self->Layout->BoundY, Self->Layout->BoundWidth, Self->Layout->BoundHeight };
      DelayMsg(AC_Draw, Self->Layout->SurfaceID, &draw);
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR load_picture(objImage *Self)
{
   parasol::Log log(__FUNCTION__);
   objPicture *picture;
   objBitmap *bitmap;
   ERROR error;

   log.branch("%s", Self->Path);

   LONG cwidth, cheight; // Dimensions of the Image's container
   GetFields(Self, FID_Width|TLONG,  &cwidth,
                   FID_Height|TLONG, &cheight,
                   TAGEND);

   // Convert relative image dimensions to absolute values

   SURFACEINFO *info;
   if (drwGetSurfaceInfo(Self->Layout->SurfaceID, &info) != ERR_Okay) {
      if (!(Self->Flags & IMF_NO_FAIL)) {
         log.warning(ERR_GetSurfaceInfo);
         return ERR_GetSurfaceInfo;
      }
      else return ERR_Okay;
   }

   if (!StrCompare("icons:", Self->Path, 6, 0)) {
      if (!(error = widgetCreateIcon(Self->Path+6, "Image", Self->IconFilter, Self->Layout->GraphicWidth, &bitmap))) {
         if (!(error = NewObject(ID_PICTURE, 0, &Self->Picture))) {
            SetFields(Self->Picture,
               FID_DisplayWidth|TLONG,  bitmap->Width,
               FID_DisplayHeight|TLONG, bitmap->Height,
               FID_Flags|TLONG,         PCF_NEW,
               TAGEND);

            SetFields(Self->Picture->Bitmap,
               FID_Width|TLONG,  bitmap->Width,
               FID_Height|TLONG, bitmap->Height,
               FID_Flags|TLONG,  BMF_ALPHA_CHANNEL,
               FID_BitsPerPixel|TLONG, 32,
               TAGEND);

            if (!(error = acInit(Self->Picture))) {
               Self->Bitmap = Self->Picture->Bitmap;
               gfxCopyArea(bitmap, Self->Picture->Bitmap, 0, 0, 0, bitmap->Width, bitmap->Height, 0, 0);
            }
         }
      }

      acFree(bitmap);

      if ((error != ERR_Okay) and (!(Self->Flags & IMF_NO_FAIL))) {
         return error;
      }

      picture = Self->Picture;
      bitmap  = Self->Picture->Bitmap;
   }
   else {
      error = ERR_Okay;
      if (!NewObject(ID_PICTURE, 0, &picture)) {
         // Load the image at its original bit depth and dither it later because poor quality resizing will otherwise
         // result (remember, dithering causes loss of image information).

         picture->Flags |= PCF_FORCE_ALPHA_32|PCF_NO_PALETTE|PCF_LAZY;
         SetString(picture, FID_Path, Self->Path);
         picture->DisplayWidth = cwidth;   // Preset display sizes are used if the source is SVG.
         picture->DisplayHeight = cheight;

         // If mask colour is defined, force our preset mask colour on the picture

         if (Self->Mask.Alpha > 0) SetPointer(picture->Bitmap, FID_Bkgd, &Self->Mask);

         if (!acInit(picture)) {
            acQuery(picture);
            Self->Picture = picture;
            Self->Bitmap = picture->Bitmap;
         }
         else {
            log.warning("Failed to read picture \"%s\".", Self->Path);
            error = ERR_Init;
         }

         if (error != ERR_Okay) acFree(picture);
      }
      else if (!(Self->Flags & IMF_NO_FAIL)) return log.warning(ERR_NewObject);

      if ((error != ERR_Okay) and (!(Self->Flags & IMF_NO_FAIL))) {
         return error;
      }

      if (!Self->Picture) {
         if (!(Self->Flags & IMF_NO_FAIL)) return ERR_Failed;
         else return ERR_Okay;
      }

      // Retrieve the mask colour from the loaded picture

      Self->Mask = picture->Bitmap->TransRGB;

      if (picture->Flags & PCF_SCALABLE) {
         // The picture is scalable (e.g. vector) which makes it easier to resize the image on the fly.
         // NB: SVG's can also be defined with fixed viewport sizes in some cases.

         if ((picture->Bitmap->Width IS picture->DisplayWidth) and (picture->Bitmap->Height IS picture->DisplayHeight)) {
            log.msg("Managing the image as a scalable picture.");

            if (Self->Flags & (IMF_ENLARGE|IMF_SHRINK)) {
               Self->Layout->GraphicWidth  = cwidth;
               Self->Layout->GraphicHeight = cheight;

               Self->Picture->Bitmap->Width = Self->Layout->GraphicWidth;
               Self->Picture->Bitmap->Height = Self->Layout->GraphicHeight;
            }
            else {
               Self->Layout->GraphicWidth  = Self->Picture->Bitmap->Width;
               Self->Layout->GraphicHeight = Self->Picture->Bitmap->Height;
            }

            Self->Picture->Bitmap->BitsPerPixel = info->BitsPerPixel;

            Self->Flags |= IMF_SCALABLE;

            if (acActivate(Self->Picture) != ERR_Okay) {
               if (Self->Bitmap IS Self->Picture->Bitmap) Self->Bitmap = NULL;
               acFree(Self->Picture);
               Self->Picture = NULL;

               if (Self->Flags & IMF_NO_FAIL) return ERR_Okay;
               else return ERR_Activate;
            }

            return ERR_Okay;
         }
      }

      if (acActivate(Self->Picture) != ERR_Okay) {
         if (Self->Bitmap IS Self->Picture->Bitmap) Self->Bitmap = NULL;
         acFree(Self->Picture);
         Self->Picture = NULL;

         if (Self->Flags & IMF_NO_FAIL) return ERR_Okay;
         else return ERR_Activate;
      }
   }

   // Fixed size is not compatible with dynamic stretch options
/*
   if (Self->Flags & IMF_FIXED_SIZE) {
      Self->Flags &= ~(IMF_ENLARGE|IMF_SHRINK);
   }
*/
   // Calculate the image size

   if (Self->Flags & (IMF_ENLARGE|IMF_SHRINK)) {
      calc_pic_size(Self, cwidth, cheight);

      // In dynamic stretching mode, retain the original image data in a RawBitmap structure if post-dithering is
      // required (otherwise we may as well perform the stretching dynamically to save memory).

      if (Self->Picture->FrameRate > 0) {
         // We need to keep the original picture
      }
      else if (!(Self->Flags & IMF_FIXED_SIZE)) {
         if ((Self->Bitmap->Flags & BMF_ALPHA_CHANNEL) or
             ((info->BitsPerPixel < 24) and
              ((info->BitsPerPixel < Self->Bitmap->BitsPerPixel) or
              ((Self->Bitmap->BitsPerPixel <= 8) and (info->BitsPerPixel > 8))))) {

            log.msg("Original image will be retained for dithering.");

            if (!CreateObject(ID_BITMAP, NF_INTEGRAL, &Self->RawBitmap,
                  FID_Width|TLONG,        Self->Bitmap->Width,
                  FID_Height|TLONG,       Self->Bitmap->Height,
                  FID_BitsPerPixel|TLONG, Self->Bitmap->BitsPerPixel,
                  FID_Flags|TLONG,        Self->Bitmap->Flags,
                  FID_Palette|TPTR,       (Self->Bitmap->BitsPerPixel <= 8) ? Self->Bitmap->Palette : (APTR)0,
                  TAGEND)) {

               gfxCopyArea(Self->Bitmap, Self->RawBitmap, 0, 0, 0, Self->Bitmap->Width, Self->Bitmap->Height, 0, 0);

               acFree(Self->Picture);
               Self->Picture = NULL;
               Self->Bitmap = NULL;

               if (!CreateObject(ID_BITMAP, NF_INTEGRAL, &Self->Bitmap,
                     FID_Width|TLONG,        Self->Layout->GraphicWidth,
                     FID_Height|TLONG,       Self->Layout->GraphicHeight,
                     FID_BitsPerPixel|TLONG, Self->RawBitmap->BitsPerPixel,
                     FID_Flags|TLONG,        Self->RawBitmap->Flags,
                     FID_Palette|TPTR,       (Self->RawBitmap->BitsPerPixel <= 8) ? Self->RawBitmap->Palette : (APTR)0,
                     TAGEND)) {
                  // Resample to the destination bitmap.  Dithering occurs later in this routine.

                  gfxCopyStretch(Self->RawBitmap, Self->Bitmap, CSTF_BILINEAR, 0, 0,
                     Self->RawBitmap->Width, Self->RawBitmap->Height, 0, 0, Self->Layout->GraphicWidth, Self->Layout->GraphicHeight);

                  // Compress the original data to save memory

                  gfxCompress(Self->RawBitmap, 0);
               }
            }
         }
         else log.msg("Original image will not be retained for dithering.");
      }
   }
   else {
      if (Self->Layout->GraphicRelWidth) Self->Layout->GraphicWidth = F2T((DOUBLE)Self->Layout->GraphicRelWidth * (DOUBLE)cwidth);
      //else if (!Self->Layout->GraphicWidth) Self->Layout->GraphicWidth = picture->Bitmap->Width;

      if (Self->Layout->GraphicRelHeight) Self->Layout->GraphicHeight = F2T((DOUBLE)Self->Layout->GraphicRelHeight * (DOUBLE)cheight);
      //else if (!Self->Layout->GraphicHeight) Self->Layout->GraphicHeight = picture->Bitmap->Height;

      if ((Self->Layout->GraphicWidth != Self->Bitmap->Width) or (Self->Layout->GraphicHeight != Self->Bitmap->Height)) {
         // User has preset the image width and height settings
         if (Self->Flags & (IMF_11_RATIO|IMF_FIT)) {
            get_image_size(picture->Bitmap, IMF_ENLARGE|IMF_SHRINK|(Self->Flags & (IMF_11_RATIO|IMF_FIT)),
               Self->Layout->GraphicWidth, Self->Layout->GraphicHeight, &Self->Layout->GraphicWidth, &Self->Layout->GraphicHeight);
         }
         else {
            if (!Self->Layout->GraphicWidth)  Self->Layout->GraphicWidth  = picture->Bitmap->Width;
            if (!Self->Layout->GraphicHeight) Self->Layout->GraphicHeight = picture->Bitmap->Height;
         }
      }
      else {
         get_image_size(picture->Bitmap, IMF_ENLARGE|IMF_SHRINK|(Self->Flags & (IMF_11_RATIO|IMF_FIT)),
            Self->Layout->GraphicWidth, Self->Layout->GraphicHeight, &Self->Layout->GraphicWidth, &Self->Layout->GraphicHeight);
      }

      Self->Flags |= IMF_FIXED_SIZE; // Force fixed size when stretching is not enabled
   }

   if (Self->Flags & IMF_FIXED_SIZE) {
      // In fixed size (no stretching) mode, we can filter the source image for better quality and store the bitmap at
      // the fixed size.  All of this saves memory and speed when redrawing.

      if ((Self->Bitmap->Width != Self->Layout->GraphicWidth) or (Self->Bitmap->Height != Self->Layout->GraphicHeight)) {
         log.trace("Commencing fixed size stretching.");
         if (!CreateObject(ID_BITMAP, NF_INTEGRAL, &bitmap,
               FID_Width|TLONG,        Self->Layout->GraphicWidth,
               FID_Height|TLONG,       Self->Layout->GraphicHeight,
               FID_BitsPerPixel|TLONG, Self->Bitmap->BitsPerPixel,
               FID_Flags|TLONG,        Self->Bitmap->Flags,
               TAGEND)) {

            //WARNING/BUG: Filtering on the source is being applied to a cached picture bitmap here!

            gfxCopyStretch(Self->Bitmap, bitmap, CSTF_BILINEAR|CSTF_FILTER_SOURCE, 0, 0,
              Self->Bitmap->Width, Self->Bitmap->Height, 0, 0, bitmap->Width, bitmap->Height);

            if ((Self->Picture) and (Self->Picture->FrameRate <= 0)) {
               acFree(Self->Picture);
               Self->Picture = NULL;
            }
            Self->Bitmap = bitmap;
         }
      }
   }

   // If the target display uses a different bit depth, use dithering to convert to it.

   if ((Self->Bitmap) and (Self->Bitmap->BitsPerPixel != info->BitsPerPixel)) {
      log.trace("Image requires depth conversion.");

      if (Self->Bitmap->BitsPerPixel IS 8); // 8 bit image sources don't need to be dithered or resampled
      else if (Self->Bitmap->Flags & BMF_ALPHA_CHANNEL) {
         if (info->BitsPerPixel <= 16) {
            ColourFormat format;
            log.trace("Resampling the image.");
            if ((info->BitmapID) and (!AccessObject(info->BitmapID, 1000, &bitmap))) {
               CopyMemory(bitmap->ColourFormat, &format, sizeof(format));
               ReleaseObject(bitmap);
            }
            else gfxGetColourFormat(&format, info->BitsPerPixel, 0, 0, 0, 0);
            gfxResample(Self->Bitmap, &format);
         }
      }
      else {
         log.trace("Dithering the image to a new bitmap.");

         if (!CreateObject(ID_BITMAP, NF_INTEGRAL, &bitmap,
               FID_Width|TLONG,        Self->Bitmap->Width,
               FID_Height|TLONG,       Self->Bitmap->Height,
               FID_Flags|TLONG,        Self->Bitmap->Flags,
               FID_BitsPerPixel|TLONG, info->BitsPerPixel,
               FID_Bkgd|TPTR,          &Self->Bitmap->BkgdRGB,
               TAGEND)) {

            if (Self->Bitmap->Flags & BMF_TRANSPARENT) {
               SetPointer(bitmap, FID_Transparence,  &Self->Bitmap->TransRGB);
            }

            Self->Bitmap->Flags &= ~BMF_TRANSPARENT;

            gfxCopyArea(Self->Bitmap, bitmap, (bitmap->Flags & BMF_TRANSPARENT) ? 0 : BAF_DITHER, 0, 0, Self->Bitmap->Width, Self->Bitmap->Height, 0, 0);

            if ((picture) and (picture->Bitmap IS Self->Bitmap));
            else acFree(Self->Bitmap);

            Self->Bitmap = bitmap;
         }
      }
   }

   if ((Self->Bitmap) and (glSixBit) and (Self->Bitmap->BitsPerPixel >= 24)) {
      ColourFormat format;
      log.trace("Resampling to 6 bit graphics.");
      gfxGetColourFormat(&format, 0, 0x3f, 0x3f, 0x3f, 0);
      gfxResample(Self->Bitmap, &format);
   }

   if (!Self->RawBitmap) {
      log.trace("The bitmap will be referenced only in the RawBitmap field.");
      Self->RawBitmap = Self->Bitmap;
      Self->Bitmap = NULL;
   }

   return ERR_Okay;
}

static void calc_pic_size(objImage *Self, LONG SurfaceWidth, LONG SurfaceHeight)
{
   parasol::Log log(__FUNCTION__);

   if ((Self->Layout->GraphicRelWidth) or (Self->Layout->GraphicRelHeight)) {
      log.warning("Relative image width/height has been set in conjunction with stretch flags (stretching takes precedence).");
      Self->Layout->GraphicRelWidth = 0;
      Self->Layout->GraphicRelHeight = 0;
   }
   else log.msg("Stretching image to fit the container #%d.", Self->Layout->SurfaceID);

   if (Self->Flags & IMF_ENLARGE) {
      if (Self->Layout->GraphicWidth < SurfaceWidth)   Self->Layout->GraphicWidth  = SurfaceWidth;
      if (Self->Layout->GraphicHeight < SurfaceHeight) Self->Layout->GraphicHeight = SurfaceHeight;
   }

   if (Self->Flags & IMF_SHRINK) {
      if (Self->Layout->GraphicWidth > SurfaceWidth)   Self->Layout->GraphicWidth  = SurfaceWidth;
      if (Self->Layout->GraphicHeight > SurfaceHeight) Self->Layout->GraphicHeight = SurfaceHeight;
   }

   get_image_size(Self->Picture->Bitmap, Self->Flags, Self->Layout->GraphicWidth, Self->Layout->GraphicHeight, &Self->Layout->GraphicWidth, &Self->Layout->GraphicHeight);
}

/*****************************************************************************
** Render a script as a bitmap image.  The rendering is done in 32-bit and will be downscaled as required in the other
** image functions.  This provides the best quality image when considering the advantage of dithering at the final step.
*/

static void render_script(objImage *Self, STRING Statement)
{
   parasol::Log log(__FUNCTION__);

   log.branch();

   if ((!Self->Layout->GraphicWidth) and (!Self->Layout->GraphicHeight)) Self->Flags |= IMF_STRETCH;

   if (Self->Flags & (IMF_ENLARGE|IMF_SHRINK)) {
      Self->Layout->GraphicWidth  = Self->Layout->BoundWidth;
      Self->Layout->GraphicHeight = Self->Layout->BoundHeight;
   }

   if (!Self->Layout->GraphicWidth)  Self->Layout->GraphicWidth  = Self->Layout->BoundWidth;
   if (!Self->Layout->GraphicHeight) Self->Layout->GraphicHeight = Self->Layout->BoundHeight;

   if (Self->Bitmap) { acFree(Self->Bitmap); Self->Bitmap = NULL; }

   if (!CreateObject(ID_BITMAP, NF_INTEGRAL, &Self->Bitmap,
         FID_Width|TLONG,        Self->Layout->GraphicWidth,
         FID_Height|TLONG,       Self->Layout->GraphicHeight,
         FID_BitsPerPixel|TLONG, 32,
         TAGEND)) {

      objSurface *surface;
      ERROR error;
      OBJECTID surface_id;
      if (!NewLockedObject(ID_SURFACE, NF_INTEGRAL, &surface, &surface_id)) {
         SetFields(surface,
            FID_Width|TLONG,        Self->Layout->GraphicWidth,
            FID_Height|TLONG,       Self->Layout->GraphicHeight,
            FID_Parent|TLONG,       0,
            FID_BitsPerPixel|TLONG, 32,
            TAGEND);
         if (!acInit(surface)) {
            OBJECTPTR script;
            if (!CreateObject(ID_SCRIPT, NF_INTEGRAL, &script,
                  FID_Statement|TSTR, Statement,
                  FID_Target|TLONG,   surface_id,
                  TAGEND)) {
               if (!acActivate(script)) {
                  drwCopySurface(surface_id, Self->Bitmap, BDF_REDRAW, 0, 0, Self->Layout->GraphicWidth, Self->Layout->GraphicHeight, 0, 0);
               }
               else error = ERR_Activate;

               acFree(script);
            }
            else error = ERR_CreateObject;
         }
         else error = ERR_Init;

         acFree(surface);
         ReleaseObject(surface);
      }
      else error = ERR_NewObject;
   }
}

//****************************************************************************

#include "class_image_def.c"

static const FieldArray clFields[] = {
   { "Layout",     FDF_INTEGRAL|FDF_SYSTEM|FDF_R, 0, NULL, NULL },
   { "Hint",       FDF_STRING|FDF_RW,             0, NULL, (APTR)SET_Hint },
   { "Frame",      FDF_LONG|FDF_RW,               0, NULL, NULL },
   { "Flags",      FDF_LONGFLAGS|FDF_RW,          (MAXINT)&clImageFlags, NULL, (APTR)SET_Flags },
   { "Mask",       FDF_RGB|FDF_RW,                0, NULL, NULL },
   { "Background", FDF_RGB|FDF_RW,                0, NULL, NULL },
   { "FrameRate",  FDF_LONG|FDF_RW,               0, NULL, (APTR)SET_FrameRate },
   // Virtual Fields
   { "IconFilter", FDF_STRING|FDF_RW,  0, (APTR)GET_IconFilter, (APTR)SET_IconFilter },
   { "IconTheme",  FDF_STRING|FDF_RW,  0, (APTR)GET_IconTheme, (APTR)SET_IconTheme },
   { "Path",       FDF_STRING|FDF_RW,  0, (APTR)GET_Path, (APTR)SET_Path },
   { "Opacity",    FDF_DOUBLE|FDF_RW,  0, (APTR)GET_Opacity, (APTR)SET_Opacity },
   { "PixelSize",  FDF_LONG|FDF_R,     0, (APTR)GET_PixelSize, NULL },
   { "Src",        FDF_SYNONYM|FDF_STRING|FDF_RW, 0, (APTR)GET_Path, (APTR)SET_Path },
   { "Location",   FDF_SYNONYM|FDF_STRING|FDF_RW, 0, (APTR)GET_Path, (APTR)SET_Path },
   { "Tile",       FDF_LONG|FDF_RI,    0, (APTR)GET_Tile, (APTR)SET_Tile },
   END_FIELD
};

//****************************************************************************

ERROR init_image(void)
{
   parasol::Log log;
   objDisplay *display;
   OBJECTID display_id;
   LONG count = 1;
   if (!FindObject("SystemDisplay", ID_DISPLAY, FOF_INCLUDE_SHARED, &display_id, &count)) {
      if (!AccessObject(display_id, 3000, &display)) {
         if (display->Flags & SCR_BIT_6) {
            log.msg("Images will be downsampled to 6-bits per channel.");
            glSixBit = TRUE;
         }
         ReleaseObject(display);
      }
   }

   return(CreateObject(ID_METACLASS, 0, &clImage,
      FID_Name|TSTRING, "Image",
      FID_ClassVersion|TFLOAT, 1.0,
      FID_Category|TLONG, CCF_GUI,
      FID_Actions|TPTR,   clImageActions,
      FID_Fields|TARRAY,  clFields,
      FID_Size|TLONG,     sizeof(objImage),
      FID_Flags|TLONG,    CLF_PRIVATE_ONLY|CLF_PROMOTE_INTEGRAL,
      FID_Path|TSTR,      MOD_PATH,
      TAGEND));
}
