// Video pixel drawing routines.

static void VideoDrawPixel(objBitmap *Bitmap, LONG X, LONG Y, ULONG Colour)
{
   SetPixel(((extBitmap *)Bitmap)->win.Drawable, X, Y, Colour);
}

static void VideoDrawRGBPixel(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 *RGB)
{
   SetPixel(((extBitmap *)Bitmap)->win.Drawable, X, Y, ((RGB->Blue)<<16) | ((RGB->Green)<<8) | RGB->Red);
}

static void VideoDrawRGBIndex(objBitmap *Bitmap, UBYTE *Data, struct RGB8 *RGB)
{

}

static ULONG VideoReadPixel(objBitmap *Bitmap, LONG X, LONG Y)
{
   return GetPixel(((extBitmap *)Bitmap)->win.Drawable, X, Y);
}

static void VideoReadRGBPixel(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 *RGB)
{
   LONG col = GetPixel(((extBitmap *)Bitmap)->win.Drawable, X, Y);
   RGB->Red   = col;
   RGB->Green = col>>8;
   RGB->Blue  = col>>16;
}

static void VideoReadRGBIndex(objBitmap *Bitmap, UBYTE *Data, struct RGB8 *RGB)
{
   RGB->Red = 0;
   RGB->Green = 0;
   RGB->Blue = 0;
}
