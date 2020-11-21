
template<class T> struct stack_blur_tables
{
  static UWORD const g_stack_blur8_mul[255];
  static UBYTE  const g_stack_blur8_shr[255];
};

//------------------------------------------------------------------------
template<class T>
UWORD const stack_blur_tables<T>::g_stack_blur8_mul[255] =
{
  512,512,456,512,328,456,335,512,405,328,271,456,388,335,292,512,
  454,405,364,328,298,271,496,456,420,388,360,335,312,292,273,512,
  482,454,428,405,383,364,345,328,312,298,284,271,259,496,475,456,
  437,420,404,388,374,360,347,335,323,312,302,292,282,273,265,512,
  497,482,468,454,441,428,417,405,394,383,373,364,354,345,337,328,
  320,312,305,298,291,284,278,271,265,259,507,496,485,475,465,456,
  446,437,428,420,412,404,396,388,381,374,367,360,354,347,341,335,
  329,323,318,312,307,302,297,292,287,282,278,273,269,265,261,512,
  505,497,489,482,475,468,461,454,447,441,435,428,422,417,411,405,
  399,394,389,383,378,373,368,364,359,354,350,345,341,337,332,328,
  324,320,316,312,309,305,301,298,294,291,287,284,281,278,274,271,
  268,265,262,259,257,507,501,496,491,485,480,475,470,465,460,456,
  451,446,442,437,433,428,424,420,416,412,408,404,400,396,392,388,
  385,381,377,374,370,367,363,360,357,354,350,347,344,341,338,335,
  332,329,326,323,320,318,315,312,310,307,304,302,299,297,294,292,
  289,287,285,282,280,278,275,273,271,269,267,265,263,261,259
};

//------------------------------------------------------------------------
template<class T>
UBYTE const stack_blur_tables<T>::g_stack_blur8_shr[255] =
{
    9, 11, 12, 13, 13, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 17,
   17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 18, 19,
   19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20,
   20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 21,
   21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
   21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22,
   22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
   22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 23,
   23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
   23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
   23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
   23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
   24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
   24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
   24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
   24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24
};


//****************************************************************************
// Create a new blur filter.

static ERROR create_blur(objVectorFilter *Self, XMLTag *Tag)
{
   effect *effect;

   if (!(effect = add_effect(Self, FE_BLUR))) return ERR_AllocMemory;
   effect->Blur.RX = 0; // SVG default values are zero
   effect->Blur.RY = 0;

   for (LONG a=1; a < Tag->TotalAttrib; a++) {
      CSTRING val = Tag->Attrib[a].Value;
      if (!val) continue;

      ULONG hash = StrHash(Tag->Attrib[a].Name, FALSE);
      switch(hash) {
         case SVF_STDDEVIATION: {
            effect->Blur.RY = -1;
            read_numseq(val, &effect->Blur.RX, &effect->Blur.RY, TAGEND);
            if (effect->Blur.RX < 0) effect->Blur.RX = 0;
            if (effect->Blur.RY < 0) effect->Blur.RY = effect->Blur.RX;
            break;
         }
         default: fe_default(Self, effect, hash, val); break;
      }
   }
   return ERR_Okay;
}

//****************************************************************************
// This is the stack blur algorithm originally implemented in AGG.

static void apply_blur(objVectorFilter *Self, effect *Effect)
{
   objBitmap *bmp = Effect->Bitmap;
   if (bmp->BytesPerPixel != 4) return;

   ULONG rx = Effect->Blur.RX * 2;
   ULONG ry = Effect->Blur.RY * 2;

   if ((rx < 1) AND (ry < 1)) return;

   unsigned x, y, xp, yp, i;
   unsigned stack_ptr;
   unsigned stack_start;

   const UBYTE * src_pix_ptr;
   UBYTE * dst_pix_ptr;
   agg::rgba8 *  stack_pix_ptr;

   unsigned sum_r;
   unsigned sum_g;
   unsigned sum_b;
   unsigned sum_a;
   unsigned sum_in_r;
   unsigned sum_in_g;
   unsigned sum_in_b;
   unsigned sum_in_a;
   unsigned sum_out_r;
   unsigned sum_out_g;
   unsigned sum_out_b;
   unsigned sum_out_a;

   unsigned w   = (ULONG)(bmp->Clip.Right - bmp->Clip.Left);
   unsigned h   = (ULONG)(bmp->Clip.Bottom - bmp->Clip.Top);

   unsigned wm  = w - 1;
   unsigned hm  = h - 1;

   unsigned div;
   unsigned mul_sum;
   unsigned shr_sum;

   agg::pod_vector<agg::rgba8> stack;

   UBYTE A = bmp->ColourFormat->AlphaPos>>3;
   UBYTE R = bmp->ColourFormat->RedPos>>3;
   UBYTE G = bmp->ColourFormat->GreenPos>>3;
   UBYTE B = bmp->ColourFormat->BluePos>>3;

   UBYTE *data = bmp->Data + (bmp->Clip.Left<<2) + (bmp->Clip.Top * bmp->LineWidth);

   // Premultiply all the pixels.  This process is required to prevent the blur from picking up colour values in pixels
   // where the alpha = 0.  If there's no alpha channel present then a pre-multiply isn't required.  NB: Demultiply
   // takes place at the end of the routine.

   premultiply_bitmap(bmp);

   if (rx > 0) {
      if (rx > 254) rx = 254;
      div = rx * 2 + 1;
      mul_sum = stack_blur_tables<int>::g_stack_blur8_mul[rx];
      shr_sum = stack_blur_tables<int>::g_stack_blur8_shr[rx];
      stack.allocate(div);

      for (y=0; y < h; y++) {
         sum_r = sum_g = sum_b = sum_a = sum_in_r = sum_in_g = sum_in_b = sum_in_a = sum_out_r = sum_out_g = sum_out_b = sum_out_a = 0;

         src_pix_ptr = data + (bmp->LineWidth * y);
         for (i=0; i <= rx; i++) {
             stack_pix_ptr    = &stack[i];
             stack_pix_ptr->r = src_pix_ptr[R];
             stack_pix_ptr->g = src_pix_ptr[G];
             stack_pix_ptr->b = src_pix_ptr[B];
             stack_pix_ptr->a = src_pix_ptr[A];
             sum_r += src_pix_ptr[R] * (i + 1);
             sum_g += src_pix_ptr[G] * (i + 1);
             sum_b += src_pix_ptr[B] * (i + 1);
             sum_a += src_pix_ptr[A] * (i + 1);
             sum_out_r += src_pix_ptr[R];
             sum_out_g += src_pix_ptr[G];
             sum_out_b += src_pix_ptr[B];
             sum_out_a += src_pix_ptr[A];
         }

         for (i=1; i <= rx; i++) {
            if (i <= wm) src_pix_ptr += 4;
            stack_pix_ptr = &stack[i + rx];
            stack_pix_ptr->r = src_pix_ptr[R];
            stack_pix_ptr->g = src_pix_ptr[G];
            stack_pix_ptr->b = src_pix_ptr[B];
            stack_pix_ptr->a = src_pix_ptr[A];
            sum_r    += src_pix_ptr[R] * (rx + 1 - i);
            sum_g    += src_pix_ptr[G] * (rx + 1 - i);
            sum_b    += src_pix_ptr[B] * (rx + 1 - i);
            sum_a    += src_pix_ptr[A] * (rx + 1 - i);
            sum_in_r += src_pix_ptr[R];
            sum_in_g += src_pix_ptr[G];
            sum_in_b += src_pix_ptr[B];
            sum_in_a += src_pix_ptr[A];
         }

           stack_ptr = rx;
           xp = rx;
           if(xp > wm) xp = wm;
           src_pix_ptr = data + (bmp->LineWidth * y) + (xp<<2);
           dst_pix_ptr = data + (bmp->LineWidth * y);
           for (x=0; x < w; x++) {
               dst_pix_ptr[R] = (sum_r * mul_sum) >> shr_sum;
               dst_pix_ptr[G] = (sum_g * mul_sum) >> shr_sum;
               dst_pix_ptr[B] = (sum_b * mul_sum) >> shr_sum;
               dst_pix_ptr[A] = (sum_a * mul_sum) >> shr_sum;
               dst_pix_ptr += 4;

               sum_r -= sum_out_r;
               sum_g -= sum_out_g;
               sum_b -= sum_out_b;
               sum_a -= sum_out_a;

               stack_start = stack_ptr + div - rx;
               if(stack_start >= div) stack_start -= div;
               stack_pix_ptr = &stack[stack_start];

               sum_out_r -= stack_pix_ptr->r;
               sum_out_g -= stack_pix_ptr->g;
               sum_out_b -= stack_pix_ptr->b;
               sum_out_a -= stack_pix_ptr->a;

               if (xp < wm) {
                  src_pix_ptr += 4;
                  ++xp;
               }

               stack_pix_ptr->r = src_pix_ptr[R];
               stack_pix_ptr->g = src_pix_ptr[G];
               stack_pix_ptr->b = src_pix_ptr[B];
               stack_pix_ptr->a = src_pix_ptr[A];

               sum_in_r += src_pix_ptr[R];
               sum_in_g += src_pix_ptr[G];
               sum_in_b += src_pix_ptr[B];
               sum_in_a += src_pix_ptr[A];
               sum_r    += sum_in_r;
               sum_g    += sum_in_g;
               sum_b    += sum_in_b;
               sum_a    += sum_in_a;

               ++stack_ptr;
               if(stack_ptr >= div) stack_ptr = 0;
               stack_pix_ptr = &stack[stack_ptr];

               sum_out_r += stack_pix_ptr->r;
               sum_out_g += stack_pix_ptr->g;
               sum_out_b += stack_pix_ptr->b;
               sum_out_a += stack_pix_ptr->a;
               sum_in_r  -= stack_pix_ptr->r;
               sum_in_g  -= stack_pix_ptr->g;
               sum_in_b  -= stack_pix_ptr->b;
               sum_in_a  -= stack_pix_ptr->a;
           }
       }
   }

   if (ry > 0) {
      if(ry > 254) ry = 254;
      div = ry * 2 + 1;
      mul_sum = stack_blur_tables<int>::g_stack_blur8_mul[ry];
      shr_sum = stack_blur_tables<int>::g_stack_blur8_shr[ry];
      stack.allocate(div);

      int stride = bmp->LineWidth;
      for (x = 0; x < w; x++) {
          sum_r = sum_g = sum_b = sum_a = sum_in_r = sum_in_g = sum_in_b = sum_in_a = sum_out_r = sum_out_g = sum_out_b = sum_out_a = 0;

          src_pix_ptr = data + (x<<2);
          for(i = 0; i <= ry; i++) {
              stack_pix_ptr    = &stack[i];
              stack_pix_ptr->r = src_pix_ptr[R];
              stack_pix_ptr->g = src_pix_ptr[G];
              stack_pix_ptr->b = src_pix_ptr[B];
              stack_pix_ptr->a = src_pix_ptr[A];
              sum_r += src_pix_ptr[R] * (i + 1);
              sum_g += src_pix_ptr[G] * (i + 1);
              sum_b += src_pix_ptr[B] * (i + 1);
              sum_a += src_pix_ptr[A] * (i + 1);
              sum_out_r += src_pix_ptr[R];
              sum_out_g += src_pix_ptr[G];
              sum_out_b += src_pix_ptr[B];
              sum_out_a += src_pix_ptr[A];
          }

          for(i = 1; i <= ry; i++) {
              if(i <= hm) src_pix_ptr += stride;
              stack_pix_ptr = &stack[i + ry];
              stack_pix_ptr->r = src_pix_ptr[R];
              stack_pix_ptr->g = src_pix_ptr[G];
              stack_pix_ptr->b = src_pix_ptr[B];
              stack_pix_ptr->a = src_pix_ptr[A];
              sum_r += src_pix_ptr[R] * (ry + 1 - i);
              sum_g += src_pix_ptr[G] * (ry + 1 - i);
              sum_b += src_pix_ptr[B] * (ry + 1 - i);
              sum_a += src_pix_ptr[A] * (ry + 1 - i);
              sum_in_r += src_pix_ptr[R];
              sum_in_g += src_pix_ptr[G];
              sum_in_b += src_pix_ptr[B];
              sum_in_a += src_pix_ptr[A];
          }

          stack_ptr = ry;
          yp = ry;
          if(yp > hm) yp = hm;
          src_pix_ptr = data + (x<<2) + (bmp->LineWidth * yp);
          dst_pix_ptr = data + (x<<2);
          for(y = 0; y < h; y++) {
              dst_pix_ptr[R] = (sum_r * mul_sum) >> shr_sum;
              dst_pix_ptr[G] = (sum_g * mul_sum) >> shr_sum;
              dst_pix_ptr[B] = (sum_b * mul_sum) >> shr_sum;
              dst_pix_ptr[A] = (sum_a * mul_sum) >> shr_sum;
              dst_pix_ptr += stride;

              sum_r -= sum_out_r;
              sum_g -= sum_out_g;
              sum_b -= sum_out_b;
              sum_a -= sum_out_a;

              stack_start = stack_ptr + div - ry;
              if(stack_start >= div) stack_start -= div;

              stack_pix_ptr = &stack[stack_start];
              sum_out_r -= stack_pix_ptr->r;
              sum_out_g -= stack_pix_ptr->g;
              sum_out_b -= stack_pix_ptr->b;
              sum_out_a -= stack_pix_ptr->a;

              if(yp < hm) {
                  src_pix_ptr += stride;
                  ++yp;
              }

              stack_pix_ptr->r = src_pix_ptr[R];
              stack_pix_ptr->g = src_pix_ptr[G];
              stack_pix_ptr->b = src_pix_ptr[B];
              stack_pix_ptr->a = src_pix_ptr[A];

              sum_in_r += src_pix_ptr[R];
              sum_in_g += src_pix_ptr[G];
              sum_in_b += src_pix_ptr[B];
              sum_in_a += src_pix_ptr[A];
              sum_r    += sum_in_r;
              sum_g    += sum_in_g;
              sum_b    += sum_in_b;
              sum_a    += sum_in_a;

              ++stack_ptr;
              if(stack_ptr >= div) stack_ptr = 0;
              stack_pix_ptr = &stack[stack_ptr];

              sum_out_r += stack_pix_ptr->r;
              sum_out_g += stack_pix_ptr->g;
              sum_out_b += stack_pix_ptr->b;
              sum_out_a += stack_pix_ptr->a;
              sum_in_r  -= stack_pix_ptr->r;
              sum_in_g  -= stack_pix_ptr->g;
              sum_in_b  -= stack_pix_ptr->b;
              sum_in_a  -= stack_pix_ptr->a;
          }
      }
   }

   demultiply_bitmap(bmp);
}
