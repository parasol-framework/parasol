/*********************************************************************************************************************
** CHUNKY32
*/

static void VideoDrawPixel32(objBitmap *Bitmap, LONG X, LONG Y, ULONG Colour)
{
}

static void VideoDrawRGBPixel32(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 *RGB)
{
}

static void VideoDrawRGBIndex32(objBitmap *Bitmap, ULONG *Data, struct RGB8 *RGB)
{

}

static ULONG VideoReadPixel32(objBitmap *Bitmap, LONG X, LONG Y)
{
   return 0;
}

static void VideoReadRGBPixel32(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 *RGB)
{
}

static void VideoReadRGBIndex32(objBitmap *Bitmap, ULONG *Data, struct RGB8 *RGB)
{
}

/*********************************************************************************************************************
** CHUNKY24
*/

static void VideoDrawPixel24(objBitmap *Bitmap, LONG X, LONG Y, ULONG Colour)
{
}

static void VideoDrawRGBPixel24(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 *RGB)
{
}

static void VideoDrawRGBIndex24(objBitmap *Bitmap, UBYTE *Data, struct RGB8 *RGB)
{

}

static ULONG VideoReadPixel24(objBitmap *Bitmap, LONG X, LONG Y)
{
   return 0;
}

static void VideoReadRGBPixel24(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 *RGB)
{

}

static void VideoReadRGBIndex24(objBitmap *Bitmap, UBYTE *Data, struct RGB8 *RGB)
{
}

/*********************************************************************************************************************
** CHUNKY16
*/

static void VideoDrawPixel16(objBitmap *Bitmap, LONG X, LONG Y, ULONG Colour)
{
}

static void VideoDrawRGBPixel16(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 *RGB)
{
}

static void VideoDrawRGBIndex16(objBitmap *Bitmap, UWORD *Data, struct RGB*RGB)
{

}

static ULONG VideoReadPixel16(objBitmap *Bitmap, LONG X, LONG Y)
{
   return 0;
}

static void VideoReadRGBPixel16(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 *RGB)
{

}

static void VideoReadRGBIndex16(objBitmap *Bitmap, UWORD *Data, struct RGB*RGB)
{
}

/*********************************************************************************************************************
** CHUNKY8
*/

static void VideoDrawPixel8(objBitmap *Bitmap, LONG X, LONG Y, ULONG Colour)
{
}

static void VideoDrawRGBPixel8(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 *RGB)
{
}

static void VideoDrawRGBIndex8(objBitmap *Bitmap, UBYTE *Data, struct RGB8 *RGB)
{

}

static ULONG VideoReadPixel8(objBitmap *Bitmap, LONG X, LONG Y)
{
   return 0;
}

static void VideoReadRGBPixel8(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 *RGB)
{
}

static void VideoReadRGBIndex8(objBitmap *Bitmap, UBYTE *Data, struct RGB*RGB)
{
   RGB->Red   = Bitmap->Palette->Col[*Data].Red;
   RGB->Green = Bitmap->Palette->Col[*Data].Green;
   RGB->Blue  = Bitmap->Palette->Col[*Data].Blue;
   RGB->Alpha = 0;
}
