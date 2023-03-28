//****************************************************************************
// nb: this implementation assumes the region to be filled is not bounded 
// by the edge of the screen, but is bounded on all sides by pixels of 
// the specified boundary color.  This exercise is left to the student...
 
void pushSeed(int x, int y);
LONG popSeed(int *x, int *y); // returns false iff stack was empty

void SeedFill(WORD x, WORD y, Color bound, Color fill)
{
   pushSeed(x, y);
   FillSeedsOnStack(bound, fill);
}

void FillSeedsOnStack(Color bound, Color fill)
{
   Color col1, col2;
   WORD x, y;              // current seed pixel
   WORD xLeft, xRight;     // current span boundary locations 
   WORD i;

   while (popSeed(&x, &y)) {
      if (GetPixel(x, y) != bound) {
         FillContiguousSpan(x, y, bound, fill, &xLeft, &xRight);

         // single pixel spans handled as a special case in the else clause
         if (xLeft != xRight) {
            /*** handle the row above you ***/
            y++;
            for(i=xLeft+1; i<=xRight; i++) {
               col1 = GetPixel(i-1, y);
               col2 = GetPixel(i, y);
               if (col1 != bound AND col1 != fill AND col2 == bound)
                  pushSeed(i-1, y);
            }
            if (col2 != bound AND col2 != fill)
               pushSeed(xRight, y); 

            /*** handle the row below you ***/
            y -= 2;
            for(i=xLeft+1; i<=xRight; i++) {
               col1 = GetPixel(i-1, y);
               col2 = GetPixel(i, y);
               if (col1 != bound AND col1 != fill AND col2 == bound) pushSeed(i-1, y);
            }
            if (col2 != bound AND col2 != fill) pushSeed(xRight, y); 
         }
         else {
            col1 = GetPixel(xLeft, y+1);
            col2 = GetPixel(xLeft, y-1);
            if (col1 != fill) pushSeed(xLeft, y+1);
            if (col2 != fill) pushSeed(xLeft, y-1);
         }

      }
   }
}

/* Fill pixels to the left and right of the seed pixel until you hit
** boundary pixels.  Return the locations of the leftmost and rightmost
** filled pixels.
*/

void FillContiguousSpan(WORD x, WORD y, Color bound, Color fill, WORD *xLeft, WORD *xRight)
{
   Color col;
   WORD i;

   /*** Fill pixels to the right until you reach a boundary pixel ***/

   i = x;
   col = GetPixel(i, y);
   while (col != bound) {
      SetPixel(i, y, fill);
      i++;
      col = GetPixel(i, y);
   }
   *xRight = i-1;

   /*** Fill pixels to the left until you reach a boundary pixel ***/

   i = x-1;
   col = GetPixel(i, y);
   while (col != bound) {
      SetPixel(i, y, fill);
      i--;
      col = GetPixel(i, y);
   }
   *xLeft = i+1;
}
