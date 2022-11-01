
#define PRV_BITMAP
#include <parasol/main.h>
#include "../defs.h"

//****************************************************************************
// This is a front-end to DrawUCLine() to allow line clipping.

void DrawLine(objBitmap *Bitmap, WORD x1, WORD y1, WORD x2, WORD y2, ULONG Colour, LONG Mask)
{

   WORD i, dx, dy, dz, l, m, n, x_inc, y_inc, z_inc;
   WORD err_1, err_2, dx2, dy2, dz2;
   WORD drawx, drawy, drawz;
   WORD z1=0, z2=0;

   if (Bitmap->AmtColours <= 256) {
      drawx = x1;
      drawy = y1;
      drawz = z1;
      dx    = x2 - x1;
      dy    = y2 - y1;
      dz    = z2 - z1;
      x_inc = (dx < 0) ? -1 : 1;
      l     = abs(dx);
      y_inc = (dy < 0) ? -1 : 1;
      m     = abs(dy);
      z_inc = (dz < 0) ? -1 : 1;
      n     = abs(dz);
      dx2   = l << 1;
      dy2   = m << 1;
      dz2   = n << 1;

      if ((l >= m) and (l >= n)) {
         err_1 = dy2 - l;
         err_2 = dz2 - l;
         for (i = 0; i < l; i++) {
             DrawPixel(Bitmap, drawx, drawy, Colour);
             if (err_1 > 0) {
                drawy += y_inc;
                err_1 -= dx2;
             }

             if (err_2 > 0) {
                drawz += z_inc;
                err_2 -= dx2;
             }

             err_1 += dy2;
             err_2 += dz2;
             drawx += x_inc;
         }
      } else if ((m >= l) and (m >= n)) {
         err_1 = dx2 - m;
         err_2 = dz2 - m;
         for (i = 0; i < m; i++) {
            DrawPixel(Bitmap, drawx, drawy, Colour);
            if (err_1 > 0) {
               drawx += x_inc;
               err_1 -= dy2;
            }

            if (err_2 > 0) {
               drawz += z_inc;
               err_2 -= dy2;
            }

            err_1 += dx2;
            err_2 += dz2;
            drawy += y_inc;
         }
      } else {
         err_1 = dy2 - n;
         err_2 = dx2 - n;
         for (i = 0; i < n; i++) {
            DrawPixel(Bitmap,drawx,drawy,Colour);
            if (err_1 > 0) {
                drawy += y_inc;
                err_1 -= dz2;
            }
            if (err_2 > 0) {
                drawx += x_inc;
                err_2 -= dz2;
            }
            err_1 += dy2;
            err_2 += dx2;
            drawz += z_inc;
         }
      }

      DrawPixel(Bitmap,drawx,drawy,Colour);
   }
   else DrawRGBLine(Bitmap, x1, y1, x2, y2, &Bitmap->Palette->Col[Colour], Mask);
}

//****************************************************************************
// Draws a line.  You are not allowed to give this function negative or extreme co-ordinates that do not fit the screen!
// It is your responsibility to check for such situations.

void DrawUCLine(objBitmap *Bitmap, WORD x1, WORD y1, WORD x2, WORD y2, ULONG Colour, LONG Mask)
{
   WORD i, dx, dy, dz, l, m, n, x_inc, y_inc, z_inc;
   WORD err_1, err_2, dx2, dy2, dz2;
   WORD drawx, drawy, drawz;
   WORD z1=0, z2=0;

   drawx = x1;
   drawy = y1;
   drawz = z1;
   dx    = x2 - x1;
   dy    = y2 - y1;
   dz    = z2 - z1;
   x_inc = (dx < 0) ? -1 : 1;
   l     = abs(dx);
   y_inc = (dy < 0) ? -1 : 1;
   m     = abs(dy);
   z_inc = (dz < 0) ? -1 : 1;
   n     = abs(dz);
   dx2   = l << 1;
   dy2   = m << 1;
   dz2   = n << 1;

   if ((l >= m) and (l >= n)) {
       err_1 = dy2 - l;
       err_2 = dz2 - l;
       for (i = 0; i < l; i++) {
           Bitmap->DrawUCPixel(Bitmap, drawx, drawy, Colour);
           if (err_1 > 0) {
               drawy += y_inc;
                err_1 -= dx2;
            }
            if (err_2 > 0) {
                drawz += z_inc;
                err_2 -= dx2;
            }
            err_1 += dy2;
            err_2 += dz2;
            drawx += x_inc;
        }
    } else if ((m >= l) and (m >= n)) {
        err_1 = dx2 - m;
        err_2 = dz2 - m;
        for (i = 0; i < m; i++) {
            Bitmap->DrawUCPixel(Bitmap, drawx, drawy, Colour);
            if (err_1 > 0) {
                drawx += x_inc;
                err_1 -= dy2;
            }
            if (err_2 > 0) {
                drawz += z_inc;
                err_2 -= dy2;
            }
            err_1 += dx2;
            err_2 += dz2;
            drawy += y_inc;
        }
    } else {
        err_1 = dy2 - n;
        err_2 = dx2 - n;
        for (i = 0; i < n; i++) {
            Bitmap->DrawUCPixel(Bitmap,drawx,drawy,Colour);
            if (err_1 > 0) {
                drawy += y_inc;
                err_1 -= dz2;
            }
            if (err_2 > 0) {
                drawx += x_inc;
                err_2 -= dz2;
            }
            err_1 += dy2;
            err_2 += dx2;
            drawz += z_inc;
        }
    }

    Bitmap->DrawUCPixel(Bitmap, drawx, drawy, Colour);
}

//****************************************************************************

void DrawRGBLine(objBitmap *Bitmap, WORD x1, WORD y1, WORD x2, WORD y2,
                 struct RGB8 *RGB, LONG Mask)
{
   WORD i, dx, dy, dz, l, m, n, x_inc, y_inc, z_inc;
   WORD err_1, err_2, dx2, dy2, dz2;
   WORD drawx, drawy, drawz;
   WORD z1 = 0, z2 = 0;

   drawx = x1;
   drawy = y1;
   drawz = z1;
   dx    = x2 - x1;
   dy    = y2 - y1;
   dz    = z2 - z1;
   x_inc = (dx < 0) ? -1 : 1;
   l     = abs(dx);
   y_inc = (dy < 0) ? -1 : 1;
   m     = abs(dy);
   z_inc = (dz < 0) ? -1 : 1;
   n     = abs(dz);
   dx2   = l << 1;
   dy2   = m << 1;
   dz2   = n << 1;

   if ((l >= m) and (l >= n)) {
       err_1 = dy2 - l;
       err_2 = dz2 - l;
       for (i = 0; i < l; i++) {
           DrawRGBPixel(Bitmap, drawx, drawy, RGB);
           if (err_1 > 0) {
               drawy += y_inc;
                err_1 -= dx2;
            }
            if (err_2 > 0) {
                drawz += z_inc;
                err_2 -= dx2;
            }
            err_1 += dy2;
            err_2 += dz2;
            drawx += x_inc;
        }
    } else if ((m >= l) and (m >= n)) {
        err_1 = dx2 - m;
        err_2 = dz2 - m;
        for (i = 0; i < m; i++) {
            DrawRGBPixel(Bitmap, drawx, drawy, RGB);
            if (err_1 > 0) {
                drawx += x_inc;
                err_1 -= dy2;
            }
            if (err_2 > 0) {
                drawz += z_inc;
                err_2 -= dy2;
            }
            err_1 += dx2;
            err_2 += dz2;
            drawy += y_inc;
        }
    } else {
        err_1 = dy2 - n;
        err_2 = dx2 - n;
        for (i = 0; i < n; i++) {
            DrawRGBPixel(Bitmap,drawx,drawy,RGB);
            if (err_1 > 0) {
                drawy += y_inc;
                err_1 -= dz2;
            }
            if (err_2 > 0) {
                drawx += x_inc;
                err_2 -= dz2;
            }
            err_1 += dy2;
            err_2 += dx2;
            drawz += z_inc;
        }
    }
    DrawRGBPixel(Bitmap,drawx,drawy,RGB);
}

//*****************************************************************************

void DrawUCRGBLine(objBitmap *Bitmap, WORD x1, WORD y1, WORD x2, WORD y2,
                   struct RGB8 *RGB, LONG Mask)
{
   WORD i, dx, dy, dz, l, m, n, x_inc, y_inc, z_inc;
   WORD err_1, err_2, dx2, dy2, dz2;
   WORD drawx, drawy, drawz;
   WORD z1=0, z2=0;

   drawx = x1;
   drawy = y1;
   drawz = z1;
   dx = x2 - x1;
   dy = y2 - y1;
   dz = z2 - z1;
   x_inc = (dx < 0) ? -1 : 1;
   l = abs(dx);
   y_inc = (dy < 0) ? -1 : 1;
   m = abs(dy);
   z_inc = (dz < 0) ? -1 : 1;
   n = abs(dz);
   dx2 = l << 1;
   dy2 = m << 1;
   dz2 = n << 1;

   if ((l >= m) and (l >= n)) {
       err_1 = dy2 - l;
       err_2 = dz2 - l;
       for (i = 0; i < l; i++) {
           Bitmap->DrawUCRPixel(Bitmap, drawx, drawy, RGB);
           if (err_1 > 0) {
               drawy += y_inc;
                err_1 -= dx2;
            }
            if (err_2 > 0) {
                drawz += z_inc;
                err_2 -= dx2;
            }
            err_1 += dy2;
            err_2 += dz2;
            drawx += x_inc;
        }
    } else if ((m >= l) and (m >= n)) {
        err_1 = dx2 - m;
        err_2 = dz2 - m;
        for (i = 0; i < m; i++) {
            Bitmap->DrawUCRPixel(Bitmap, drawx, drawy, RGB);
            if (err_1 > 0) {
                drawx += x_inc;
                err_1 -= dy2;
            }
            if (err_2 > 0) {
                drawz += z_inc;
                err_2 -= dy2;
            }
            err_1 += dx2;
            err_2 += dz2;
            drawy += y_inc;
        }
    } else {
        err_1 = dy2 - n;
        err_2 = dx2 - n;
        for (i = 0; i < n; i++) {
            Bitmap->DrawUCRPixel(Bitmap, drawx, drawy, RGB);
            if (err_1 > 0) {
                drawy += y_inc;
                err_1 -= dz2;
            }
            if (err_2 > 0) {
                drawx += x_inc;
                err_2 -= dz2;
            }
            err_1 += dy2;
            err_2 += dx2;
            drawz += z_inc;
        }
    }
    Bitmap->DrawUCRPixel(Bitmap, drawx, drawy, RGB);
}
