/*****************************************************************************
** SurfacePan class
*/

class SurfacePan : public Scintilla::Surface
{
   int penx;
   int peny;
   objBitmap *bitmap;
   BYTE own_bitmap:1; /* True if this object owns the bitmap, and will free it */
   LONG pencol;
   Scintilla::PRectangle cliprect;

public:
   SurfacePan();
   virtual ~SurfacePan();

   void Init(Scintilla::WindowID wid);
   void Init(Scintilla::SurfaceID sid, Scintilla::WindowID wid);
   void InitPixMap(int width, int height, Scintilla::Surface *surface_, Scintilla::WindowID wid);

   void Release();
   bool Initialised();
   void PenColour(Scintilla::ColourAllocated fore);
   int LogPixelsY();
   int DeviceHeightFont(int points);
   void MoveTo(int x_, int y_);
   void LineTo(int x_, int y_);
   void Polygon(Scintilla::Point *pts, int npts, Scintilla::ColourAllocated fore, Scintilla::ColourAllocated back);
   void RectangleDraw(Scintilla::PRectangle rc, Scintilla::ColourAllocated fore, Scintilla::ColourAllocated back);
   void FillRectangle(Scintilla::PRectangle rc, Scintilla::ColourAllocated back);
   void FillRectangle(Scintilla::PRectangle rc, Scintilla::Surface &surfacePattern);
   void RoundedRectangle(Scintilla::PRectangle rc, Scintilla::ColourAllocated fore, Scintilla::ColourAllocated back);
   void Ellipse(Scintilla::PRectangle rc, Scintilla::ColourAllocated fore, Scintilla::ColourAllocated back);
   void Copy(Scintilla::PRectangle rc, Scintilla::Point from, Scintilla::Surface &surfaceSource);
   void AlphaRectangle(Scintilla::PRectangle rc, int cornerSize, Scintilla::ColourAllocated fill, int alphaFill, Scintilla::ColourAllocated outline, int alphaOutline, int flags);
	void DrawRGBAImage(Scintilla::PRectangle rc, int width, int height, const unsigned char *pixelsImage);

   void DrawTextBase(Scintilla::PRectangle rc, Scintilla::Font &font_, int ybase, const char *s, int len, Scintilla::ColourAllocated fore);
   void DrawTextNoClip(Scintilla::PRectangle rc, Scintilla::Font &font_, int ybase, const char *s, int len, Scintilla::ColourAllocated fore, Scintilla::ColourAllocated back);
   void DrawTextClipped(Scintilla::PRectangle rc, Scintilla::Font &font_, int ybase, const char *s, int len, Scintilla::ColourAllocated fore, Scintilla::ColourAllocated back);
   void DrawTextTransparent(Scintilla::PRectangle rc, Scintilla::Font &font_, int ybase, const char *s, int len, Scintilla::ColourAllocated fore);
   void MeasureWidths(Scintilla::Font &font_, const char *s, int len, int *positions);
   int WidthText(Scintilla::Font &font_, const char *s, int len);
   int WidthChar(Scintilla::Font &font_, char ch);
   int Ascent(Scintilla::Font &font_);
   int Descent(Scintilla::Font &font_);
   int InternalLeading(Scintilla::Font &font_);
   int ExternalLeading(Scintilla::Font &font_);
   int Height(Scintilla::Font &font_);
   int AverageCharWidth(Scintilla::Font &font_);

   int SetPalette(Scintilla::Palette *pal, bool inBackGround);
   void SetClip(Scintilla::PRectangle rc);
   void FlushCachedState();

   void SetUnicodeMode(bool unicodeMode_);
   void SetDBCSMode(int codePage);

private:
   OBJECTPTR GetFont(Scintilla::Font& font);
};

SurfacePan::SurfacePan()
:  cliprect(0,0,0,0)
{
   bitmap = NULL;
   own_bitmap = FALSE;
   penx   = 0;
   peny   = 0;
   pencol = 0;
}

SurfacePan::~SurfacePan()
{
   Release();
}

void SurfacePan::Release()
{
   if ((bitmap) AND (own_bitmap)) {
      acFree(bitmap);
      bitmap = NULL;
   }
}

bool SurfacePan::Initialised()
{
   return bitmap != NULL;
}

void SurfacePan::Init(Scintilla::WindowID WinID)
{

}

void SurfacePan::Init(Scintilla::SurfaceID sid, Scintilla::WindowID WID_NAME)
{
   if (bitmap) return;

   // Surface id will be a bitmap object
   bitmap = static_cast<struct rkBitmap *>(sid);

   // Set the Clipping rect to the dimensions of the bitmap
   cliprect.left = 0;
   cliprect.right = bitmap->Width;
   cliprect.top = 0;
   cliprect.bottom = bitmap->Height;
}

void SurfacePan::InitPixMap(int width, int height, Scintilla::Surface *surface_, Scintilla::WindowID WID_NAME)
{
   if (bitmap) return;

   FMSG("~panInitPixMap()","Size: %dx%d", width, height);

   //DebugOff();
   ERROR error = CreateObject(ID_BITMAP, NULL, &bitmap,
      FID_Name|TSTR,    "sciPixmap",
      FID_Width|TLONG,  width,
      FID_Height|TLONG, height,
      TAGEND);
   //DebugOn();

   STEP();

   if (error != ERR_Okay) {
      LogF("@panInitPixMap","Failed to create offscreen surface object.");
      return;
   }

   own_bitmap = TRUE; // This object owns the bitmap and will delete it on destruction.

   // Set the Clipping rect to the dimensions of the bitmap

   cliprect.left   = 0;
   cliprect.top    = 0;
   cliprect.right  = bitmap->Width;
   cliprect.bottom = bitmap->Height;

   STEP();
}

/****************************************************************************/

INLINE ULONG to_pan_col(struct rkBitmap *bitmap, const Scintilla::ColourAllocated& colour)
{
   ULONG col32 = colour.AsLong();
   return PackPixel(bitmap, SCIRED(col32), SCIGREEN(col32), SCIBLUE(col32));
}

/****************************************************************************/

void SurfacePan::PenColour(Scintilla::ColourAllocated fore)
{
   if (bitmap) {
      this->pencol = to_pan_col(bitmap, fore);
   }
}

/****************************************************************************/

int SurfacePan::LogPixelsY()
{
   return 100;
}

/****************************************************************************/

int SurfacePan::DeviceHeightFont(int pointsize)
{
   int dpi = LogPixelsY();

   //MSG("DeviceHeightFont(%d, DPI %d)", pointsize, dpi);

   return (pointsize * dpi + (dpi / 2)) / 72;
}

/****************************************************************************/

void SurfacePan::MoveTo(int x, int y)
{
   penx = x;
   peny = y;
}

/****************************************************************************/

void SurfacePan::LineTo(int x, int y)
{
   if (bitmap) {
      BitmapClipper clipper(bitmap, cliprect);

      //DBGDRAW("panLineTo:","%dx%d - %dx%d", penx, peny, x, y);

      gfxDrawLine(bitmap, penx, peny, x, y, pencol);
      penx = x;
      peny = y;
   }
}

/****************************************************************************/

void SurfacePan::DrawRGBAImage(Scintilla::PRectangle rc, int width, int height, const unsigned char *pixelsImage)
{
   LogF("@DrawRGBAImage()","Unsupported.");
}

/****************************************************************************/

void SurfacePan::Polygon(Scintilla::Point *pts, int npts, Scintilla::ColourAllocated fore, Scintilla::ColourAllocated back)
{
   // TEMP: For now, just draw the outline as lines (don't fill)

   if (bitmap) {
      BitmapClipper clipper(bitmap, cliprect);

      DBGDRAW("panPolygon","");

      ULONG col = to_pan_col(bitmap, fore);

      LONG i;
      for (i=0; i<npts-1; ++i) {
         gfxDrawLine(bitmap, pts[i].x, pts[i].y, pts[i+1].x, pts[i+1].y, col);
      }
   }
}

/****************************************************************************/

void SurfacePan::RectangleDraw(Scintilla::PRectangle rc, Scintilla::ColourAllocated fore, Scintilla::ColourAllocated back)
{
   if (bitmap) {
      BitmapClipper clipper(bitmap, cliprect);

      ULONG bk32 = to_pan_col(bitmap, back);
      ULONG fr32 = to_pan_col(bitmap, fore);

      DBGDRAW("panRectangleDraw()","#%.8x, #%.8x", bk32, fr32);

      gfxDrawRectangle(bitmap, rc.left, rc.top, rc.Width(), rc.Height(), bk32, TRUE);
      gfxDrawRectangle(bitmap, rc.left, rc.top, rc.Width(), rc.Height(), fr32, FALSE);
   }
}

/****************************************************************************/

void SurfacePan::FillRectangle(Scintilla::PRectangle rc, Scintilla::ColourAllocated back)
{
   if (bitmap) {
      ULONG colour;

      BitmapClipper clipper(bitmap, cliprect);
      colour = to_pan_col(bitmap, back);

      DBGDRAW("panFillRectangle()","Bitmap: %p, Size: %dx%d,%dx%d, Colour: $%.8x, Clipping: %dx%d,%dx%d", bitmap, rc.left, rc.top, rc.Width(), rc.Height(), colour, bitmap->Clip.Left, bitmap->Clip.Right, bitmap->Clip.Top, bitmap->Clip.Bottom);

      gfxDrawRectangle(bitmap, rc.left, rc.top, rc.Width(), rc.Height(), colour, TRUE);
   }
}

/****************************************************************************/

void SurfacePan::FillRectangle(Scintilla::PRectangle rc, Scintilla::Surface &surfacePattern)
{
   DBGDRAW("panFillRectangle(2):","UNIMPLEMENTED");
}

/****************************************************************************/

void SurfacePan::RoundedRectangle(Scintilla::PRectangle rc, Scintilla::ColourAllocated fore, Scintilla::ColourAllocated back)
{
   DBGDRAW("panRoundedRectangle","");

   if (((rc.right - rc.left) > 4) && ((rc.bottom - rc.top) > 4)) {
      // Approximate a round rect with some cut off corners
      Scintilla::Point pts[] = {
         Scintilla::Point(rc.left + 2, rc.top),
         Scintilla::Point(rc.right - 2, rc.top),
         Scintilla::Point(rc.right, rc.top + 2),
         Scintilla::Point(rc.right, rc.bottom - 2),
         Scintilla::Point(rc.right - 2, rc.bottom),
         Scintilla::Point(rc.left + 2, rc.bottom),
         Scintilla::Point(rc.left, rc.bottom - 2),
         Scintilla::Point(rc.left, rc.top + 2),
      };
      Polygon(pts, sizeof(pts) / sizeof(pts[0]), fore, back);
   }
   else RectangleDraw(rc, fore, back);
}

/****************************************************************************/

void SurfacePan::Ellipse(Scintilla::PRectangle rc, Scintilla::ColourAllocated fore, Scintilla::ColourAllocated back)
{
   DBGDRAW("panEllipse:","UNIMPLEMENTED");
}

/****************************************************************************/

void SurfacePan::Copy(Scintilla::PRectangle rc, Scintilla::Point from, Scintilla::Surface &surfaceSource)
{
   SurfacePan &src_surface = static_cast<SurfacePan&>(surfaceSource);

   if ((bitmap) AND (src_surface.bitmap)) {
//      BitmapClipper clipper(bitmap, cliprect);

      DBGDRAW("panCopy:","From: %dx%d To: %dx%d,%dx%d, Clip: %dx%d,%dx%d", from.x, from.y, rc.left, rc.top, rc.Width(), rc.Height(), bitmap->Clip.Left, bitmap->Clip.Top, bitmap->Clip.Right, bitmap->Clip.Bottom);
      DBGDRAW("panCopy:","Bitmap: %d, Offset: %dx%d", bitmap->Head.UniqueID, bitmap->XOffset, bitmap->YOffset);
      gfxCopyArea(src_surface.bitmap, bitmap, 0,
         from.x, from.y, rc.Width(), rc.Height(), /* source */
         rc.left, rc.top); /* dest */

      //bmpDrawRectangle(bitmap, rc.left, rc.top, rc.Width(), rc.Height(), PackPixel(bitmap, 255, 0, 0, 255), 0);
   }
   else LogF("@panCopy","Bad arguments.");
}

/****************************************************************************/

void SurfacePan::DrawTextBase(Scintilla::PRectangle rc, Scintilla::Font &font_, int ybase, const char *String, int len, Scintilla::ColourAllocated fore)
{
   objFont *font;
   ULONG col32;
   LONG i;
   char nstr[len+1];

   for (i=0; i < len; ++i) nstr[i] = String[i];
   nstr[i] = 0;

   col32 = fore.AsLong();

   DBGDRAW("panDrawTextBase()","Bitmap: %p, #%.8x, String: %.10s TO %dx%d", bitmap, col32, nstr, rc.left, rc.top);

   if (!bitmap) return;

   font = (objFont *)GetFont(font_);//static_cast<OBJECTPTR>(font_.GetID());

   if (!font) {
      LogF("@panDrawTextBase","Font was NULL.");
      return;
   }

   BitmapClipper clipper(bitmap, cliprect);

   SetFields(font, FID_String|TSTRING, nstr, TAGEND);
   font->Bitmap = bitmap;
   font->X = rc.left;
   font->Y = rc.top + font->Leading;
   font->Colour.Red   = SCIRED(col32);
   font->Colour.Green = SCIGREEN(col32);
   font->Colour.Blue  = SCIBLUE(col32);
   font->Colour.Alpha = 255;

   acDraw(font);
}

/****************************************************************************/

void SurfacePan::DrawTextNoClip(Scintilla::PRectangle rc, Scintilla::Font &font_, int ybase, const char *s, int len,
                                 Scintilla::ColourAllocated fore, Scintilla::ColourAllocated back)
{
   DBGDRAW("panDrawTextNoClip()","");
   FillRectangle(rc, back);
   DrawTextBase(rc, font_, ybase, s, len, fore);
}

/****************************************************************************/
// On GTK+, exactly same as DrawTextNoClip

void SurfacePan::DrawTextClipped(Scintilla::PRectangle rc, Scintilla::Font &font_, int ybase, const char *s, int len,
                                  Scintilla::ColourAllocated fore, Scintilla::ColourAllocated back)
{
   DBGDRAW("panDrawTextClipped()","");
   FillRectangle(rc, back);
   DrawTextBase(rc, font_, ybase, s, len, fore);
}

/*****************************************************************************
** Avoid drawing spaces in transparent mode; i.e. only draw if there is at least one non-space char.
*/

void SurfacePan::DrawTextTransparent(Scintilla::PRectangle rc, Scintilla::Font &font_, int ybase,
   const char *s, int len, Scintilla::ColourAllocated fore)
{
   LONG i;

   for (i=0; i < len; i++) {
      if (s[i] != ' ') {
         DrawTextBase(rc, font_, ybase, s, len, fore);
         return;
      }
   }
}

/****************************************************************************/

void SurfacePan::MeasureWidths(Scintilla::Font &font_, const char *string, int len, int *positions)
{
   objFont *font = (objFont *)GetFont(font_);
   ULONG unicode;
   UBYTE *str;
   LONG i, charpos, copy;

   str = (UBYTE *)string;
   if (font) {
      charpos = 0;
      for (i=0; i < len; ) {
         if (font->FixedWidth) {
            if ((UBYTE)(str[i]) < 128) copy = 1;
            else if ((str[i] & 0xe0) IS 0xc0) copy = 2;
            else if ((str[i] & 0xf0) IS 0xe0) copy = 3;
            else if ((str[i] & 0xf8) IS 0xf0) copy = 4;
            else if ((str[i] & 0xfc) IS 0xf8) copy = 5;
            else if ((str[i] & 0xfc) IS 0xfc) copy = 6;
            else copy = 1;

            charpos += font->FixedWidth;
         }
         else {
            if (str[i] < 128) {
               unicode = str[i];
               if ((UBYTE)(str[i]) < 128) copy = 1;
               else if ((str[i] & 0xe0) IS 0xc0) copy = 2;
               else if ((str[i] & 0xf0) IS 0xe0) copy = 3;
               else if ((str[i] & 0xf8) IS 0xf0) copy = 4;
               else if ((str[i] & 0xfc) IS 0xf8) copy = 5;
               else if ((str[i] & 0xfc) IS 0xfc) copy = 6;
               else copy = 1;
            }
            else {
               unicode = UTF8ReadValue((STRING)(str+i), &copy);
            }

            charpos += fntCharWidth(font, unicode, 0, NULL);
         }
         positions[i++] = charpos;

         // Supporting UTF8 characters are assigned the same pixel offset

         while ((copy > 1) AND (i < len)) {
            positions[i++] = charpos;
            copy--;
         }
      }
   }
   else for (int i = 0; i < len; ++i) positions[i] = i + 1;
}

/****************************************************************************/

int SurfacePan::WidthText(Scintilla::Font &font_, const char *s, int len)
{
   OBJECTPTR font = GetFont(font_);

   if (font) {
      // Note: The length must be passed as the number of characters, not bytes
      return fntStringWidth((objFont *)font, (STRING)s, len);
   }

   return 5; // crashes if you return 0 here !!!
}

/****************************************************************************/

int SurfacePan::WidthChar(Scintilla::Font &font_, char ch)
{
   objFont *font = (objFont *)GetFont(font_);
   return fntCharWidth(font, ch, 0, NULL);
}

/****************************************************************************/

int SurfacePan::Ascent(Scintilla::Font &font_)
{
   OBJECTPTR font = GetFont(font_);

   if (font) return GetFontHeight(font) + GetFontLeading(font) - GetFontGutter(font);
   else return 10;
}

/****************************************************************************/

int SurfacePan::Descent(Scintilla::Font &font_)
{
   OBJECTPTR font = GetFont(font_);

   if (font) return GetFontGutter(font);
   else return 3;
}

/****************************************************************************/

int SurfacePan::InternalLeading(Scintilla::Font &font_)
{
   OBJECTPTR font = GetFont(font_);//reinterpret_cast<OBJECTPTR>(font_.GetID());

   if (font) return GetFontLeading(font);
   else return 0;
}

/****************************************************************************/

int SurfacePan::ExternalLeading(Scintilla::Font &font_)
{
   /* NOTE: this right? */
   OBJECTPTR font = GetFont(font_);//reinterpret_cast<OBJECTPTR>(font_.GetID());

   if (font) return GetFontGutter(font);
   else return 0;
}

/****************************************************************************/

int SurfacePan::Height(Scintilla::Font &font_)
{
   return Ascent(font_) + Descent(font_);
}

/****************************************************************************/

int SurfacePan::AverageCharWidth(Scintilla::Font &font_)
{
   return WidthChar(font_, 'x');
}

/*****************************************************************************
** Functionality not required.
*/

int SurfacePan::SetPalette(Scintilla::Palette *, bool)
{
   return 0;
}

/****************************************************************************/

void SurfacePan::SetClip(Scintilla::PRectangle rc)
{
   DBGDRAW("panSetClip","%dx%d,%dx%d", rc.left, rc.top, rc.Width(), rc.Height());
   cliprect = rc;

   /*if (bitmap) {
//        NOTE: just blindly set the clipping rect here, or check it?

      bitmap->ClipLeft = MAX(bitmap->ClipLeftrc.left;
      bitmap->ClipRight = rc.right;
      bitmap->ClipTop = rc.top;
      bitmap->ClipBottom = rc.bottom;
   }*/
}

/****************************************************************************/

void SurfacePan::FlushCachedState()
{
   FMSG("panFlushCachedState()","UNSUPPORTED");
}

/****************************************************************************/

void SurfacePan::SetUnicodeMode(bool unicodeMode_)
{
//   FMSG("panSetUnicodeMode()","%d [UNSUPPORTED]", unicodeMode_);
}

/****************************************************************************/

void SurfacePan::SetDBCSMode(int codePage)
{
//   FMSG("panSetDBCSMode()","CodePage: %d [UNSUPPORTED]", codePage);
}

/****************************************************************************/

OBJECTPTR SurfacePan::GetFont(Scintilla::Font& font_)
{
   if (font_.bold) {
      if ((font_.italic) AND (glBIFont)) return glBIFont;
      else if (glBoldFont) return glBoldFont;
   }
   else if ((font_.italic) AND (glItalicFont)) {
      return glItalicFont;
   }

   return glFont;
}

/****************************************************************************/

void SurfacePan::AlphaRectangle(Scintilla::PRectangle rc, int cornerSize, Scintilla::ColourAllocated fill, int alphaFill,
		Scintilla::ColourAllocated outline, int alphaOutline, int flags)
{
   FMSG("panAlpharectangle()","UNSUPPORTED");

   if (bitmap) {
      BitmapClipper clipper(bitmap, cliprect);

      gfxDrawRectangle(bitmap, rc.left, rc.top, rc.Width(), rc.Height(), to_pan_col(bitmap, fill), TRUE);

      gfxDrawRectangle(bitmap, rc.left, rc.top, rc.Width(), rc.Height(), to_pan_col(bitmap, outline), FALSE);
   }
   else LogF("@panAlphaRectangle","Bitmap was NULL.");

#if 0
		int width = rc.Width();
		int height = rc.Height();

		guint8 pixVal[4] = {0};
		guint32 valEmpty = *(reinterpret_cast<guint32 *>(pixVal));
		pixVal[0] = GetRValue(fill.AsLong());
		pixVal[1] = GetGValue(fill.AsLong());
		pixVal[2] = GetBValue(fill.AsLong());
		pixVal[3] = alphaFill;

		guint32 valFill = *(reinterpret_cast<guint32 *>(pixVal));
		pixVal[0] = GetRValue(outline.AsLong());
		pixVal[1] = GetGValue(outline.AsLong());
		pixVal[2] = GetBValue(outline.AsLong());
		pixVal[3] = alphaOutline;

		guint32 valOutline = *(reinterpret_cast<guint32 *>(pixVal));
		guint32 *pixels = reinterpret_cast<guint32 *>(gdk_pixbuf_get_pixels(pixalpha));

		int stride = gdk_pixbuf_get_rowstride(pixalpha) / 4;
		for (int y=0; y<height; y++) {
			for (int x=0; x<width; x++) {
				if ((x==0) || (x==width-1) || (y == 0) || (y == height-1)) {
					pixels[y*stride+x] = valOutline;
				}
            else pixels[y*stride+x] = valFill;
			}
		}
		for (int c=0;c<cornerSize; c++) {
			for (int x=0;x<c+1; x++) {
				AllFour(pixels, stride, width, height, x, c-x, valEmpty);
			}
		}
		for (int x=1;x<cornerSize; x++) {
			AllFour(pixels, stride, width, height, x, cornerSize-x, valOutline);
		}

		// Draw with alpha
		gdk_draw_pixbuf(drawable, gc, pixalpha,
			0,0, rc.left,rc.top, width,height, GDK_RGB_DITHER_NORMAL, 0, 0);

		g_object_unref(pixalpha);
#endif
}

/****************************************************************************/

Scintilla::Surface * Scintilla::Surface::Allocate()
{
   return new SurfacePan();
}
