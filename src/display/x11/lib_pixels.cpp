/*****************************************************************************
** CHUNKY32
*/

static void VideoDrawPixel32(objBitmap *Bitmap, LONG X, LONG Y, ULONG Colour)
{
   XSetForeground(XDisplay, glXGC, Colour);
   XDrawPoint(XDisplay, Bitmap->x11.drawable, glXGC, X, Y);
}

static void VideoDrawRGBPixel32(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 *RGB)
{
   XSetForeground(XDisplay, glXGC, PackPixelWB(Bitmap, RGB->Red, RGB->Green, RGB->Blue));
   XDrawPoint(XDisplay, Bitmap->x11.drawable, glXGC, X, Y);
}

static void VideoDrawRGBIndex32(objBitmap *Bitmap, ULONG *Data, struct RGB8 *RGB)
{

}

static ULONG VideoReadPixel32(objBitmap *Bitmap, LONG X, LONG Y)
{
   return ((ULONG *)((UBYTE *)Bitmap->x11.readable->data + (Bitmap->x11.readable->bytes_per_line * Y) + (X<<2)))[0];
}

static void VideoReadRGBPixel32(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 *RGB)
{
   ULONG colour = ((ULONG *)((UBYTE *)Bitmap->x11.readable->data + (Bitmap->x11.readable->bytes_per_line * Y) + (X<<2)))[0];
   RGB->Red   = (UBYTE)(colour >> Bitmap->prvColourFormat.RedPos);
   RGB->Green = (UBYTE)(colour >> Bitmap->prvColourFormat.GreenPos);
   RGB->Blue  = (UBYTE)(colour >> Bitmap->prvColourFormat.BluePos);
   RGB->Alpha = (UBYTE)(colour >> Bitmap->prvColourFormat.AlphaPos);
}

static void VideoReadRGBIndex32(objBitmap *Bitmap, ULONG *Data, struct RGB8 *RGB)
{
   ULONG colour = Data[0];
   RGB->Red   = (UBYTE)(colour >> Bitmap->prvColourFormat.RedPos);
   RGB->Green = (UBYTE)(colour >> Bitmap->prvColourFormat.GreenPos);
   RGB->Blue  = (UBYTE)(colour >> Bitmap->prvColourFormat.BluePos);
   RGB->Alpha = (UBYTE)(colour >> Bitmap->prvColourFormat.AlphaPos);
}
/*****************************************************************************
** CHUNKY24
*/

static void VideoDrawPixel24(objBitmap *Bitmap, LONG X, LONG Y, ULONG Colour)
{
   XSetForeground(XDisplay, glXGC, Colour);
   XDrawPoint(XDisplay, Bitmap->x11.drawable, glXGC, X, Y);
}

static void VideoDrawRGBPixel24(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 *RGB)
{
   ULONG Colour = (RGB->Red<<16) | (RGB->Green<<8) | (RGB->Blue);
   XSetForeground(XDisplay, glXGC, Colour);
   XDrawPoint(XDisplay, Bitmap->x11.drawable, glXGC, X, Y);
}

static void VideoDrawRGBIndex24(objBitmap *Bitmap, UBYTE *Data, struct RGB8 *RGB)
{

}

static ULONG VideoReadPixel24(objBitmap *Bitmap, LONG X, LONG Y)
{
   UBYTE *data = (UBYTE *)Bitmap->x11.readable->data + (Bitmap->LineWidth * Y) + (X + X + X);
   return (data[2]<<16)|(data[1]<<8)|data[0];
}

static void VideoReadRGBPixel24(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 *RGB)
{
   UBYTE *data = (UBYTE *)Bitmap->x11.readable->data;
   data += Bitmap->x11.readable->bytes_per_line * Y;
   data += X + X + X;
   RGB->Red   = data[2];
   RGB->Green = data[1];
   RGB->Blue  = data[0];
   RGB->Alpha = 0;
}

static void VideoReadRGBIndex24(objBitmap *Bitmap, UBYTE *Data, struct RGB8 *RGB)
{
   RGB->Red   = Data[2];
   RGB->Green = Data[1];
   RGB->Blue  = Data[0];
   RGB->Alpha = 0;
}

/*****************************************************************************
** CHUNKY16
*/

static void VideoDrawPixel16(objBitmap *Bitmap, LONG X, LONG Y, ULONG Colour)
{
   XSetForeground(XDisplay, glXGC, Colour);
   XDrawPoint(XDisplay, Bitmap->x11.drawable, glXGC, X, Y);
}

static void VideoDrawRGBPixel16(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 *RGB)
{
   XSetForeground(XDisplay, glXGC, PackPixel(Bitmap, RGB->Red, RGB->Green, RGB->Blue));
   XDrawPoint(XDisplay, Bitmap->x11.drawable, glXGC, X, Y);
}

static void VideoDrawRGBIndex16(objBitmap *Bitmap, UWORD *Data, struct RGB8 *RGB)
{

}

static ULONG VideoReadPixel16(objBitmap *Bitmap, LONG X, LONG Y)
{
   return ((UWORD *)((BYTE *)Bitmap->x11.readable->data + (Bitmap->x11.readable->bytes_per_line * Y) + (X<<1)))[0];
}

static void VideoReadRGBPixel16(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 *RGB)
{
   UWORD data;
   data  = ((UWORD *)((BYTE *)Bitmap->x11.readable->data + (Bitmap->x11.readable->bytes_per_line * Y) + (X<<1)))[0];
   RGB->Red   = UnpackRed(Bitmap, data);
   RGB->Green = UnpackGreen(Bitmap, data);
   RGB->Blue  = UnpackBlue(Bitmap, data);
   RGB->Alpha = 0;
}

static void VideoReadRGBIndex16(objBitmap *Bitmap, UWORD *Data, struct RGB8 *RGB)
{
   RGB->Red   = UnpackRed(Bitmap, Data[0]);
   RGB->Green = UnpackGreen(Bitmap, Data[0]);
   RGB->Blue  = UnpackBlue(Bitmap, Data[0]);
   RGB->Alpha = 0;
}

/*****************************************************************************
** CHUNKY8
*/

static void VideoDrawPixel8(objBitmap *Bitmap, LONG X, LONG Y, ULONG Colour)
{
   XDrawPoint(XDisplay, Bitmap->x11.drawable, glXGC, X, Y);
}

static void VideoDrawRGBPixel8(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 *RGB)
{
   ULONG colour;
   colour = RGBToValue(RGB, Bitmap->Palette);
   XDrawPoint(XDisplay, Bitmap->x11.drawable, glXGC, X, Y);
}

static void VideoDrawRGBIndex8(objBitmap *Bitmap, UBYTE *Data, struct RGB8 *RGB)
{

}

static ULONG VideoReadPixel8(objBitmap *Bitmap, LONG X, LONG Y)
{
   return (Bitmap->x11.readable->data + (Bitmap->x11.readable->bytes_per_line * Y) + X)[0];
}

static void VideoReadRGBPixel8(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 *RGB)
{
   UBYTE index, *data;
   data  = (UBYTE *)Bitmap->x11.readable->data;
   index = data[(Bitmap->x11.readable->bytes_per_line * Y) + X];
   RGB->Red   = Bitmap->Palette->Col[index].Red;
   RGB->Green = Bitmap->Palette->Col[index].Green;
   RGB->Blue  = Bitmap->Palette->Col[index].Blue;
   RGB->Alpha = 0;
}

static void VideoReadRGBIndex8(objBitmap *Bitmap, UBYTE *Data, struct RGB8 *RGB)
{
   RGB->Red   = Bitmap->Palette->Col[*Data].Red;
   RGB->Green = Bitmap->Palette->Col[*Data].Green;
   RGB->Blue  = Bitmap->Palette->Col[*Data].Blue;
   RGB->Alpha = 0;
}
