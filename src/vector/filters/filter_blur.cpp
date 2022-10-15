/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
BlurFX: Applies a Gaussian blur effect to an input source.

The BlurFX class performs a Gaussian blur on the input source.  The Gaussian blur kernel is an approximation of the
normalized convolution `G(x,y) = H(x)I(y)` where `H(x) = exp(-x2/ (2s2)) / sqrt(2* pi*s2)` and
`I(y) = exp(-y2/ (2t2)) / sqrt(2* pi*t2)` with 's' being the standard deviation in the x direction and 't' being the
standard deviation in the y direction, as specified by #SX and #SY.

At least one of #SX or #SY should be greater than 0, otherwise no rendering is performed.

-END-

Frequently this operation will take place on alpha-only images, such as that produced by the built-in input,
SourceAlpha.  The implementation may notice this and optimize the single channel case. If the input has infinite
extent and is constant (e.g  FillPaint where the fill is a solid color), this operation has no effect. If the input
has infinite extent and the filter result is the input to an feTile, the filter is evaluated with periodic boundary
conditions.

*********************************************************************************************************************/

template<class T> struct stack_blur_tables
{
  static UWORD const g_stack_blur8_mul[255];
  static UBYTE  const g_stack_blur8_shr[255];
};

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

typedef class rkBlurFX : public rkFilterEffect {
   public:
   DOUBLE SX, SY;
} objBlurFX;

//********************************************************************************************************************
// This is the stack blur algorithm originally implemented in AGG.  It is intended to produce a near identical output
// to that of a standard gaussian blur algorithm.

static ERROR BLURFX_Draw(objBlurFX *Self, struct acDraw *Args)
{
   auto bmp = Self->Target;
   if (bmp->BytesPerPixel != 4) return ERR_Failed;

   LONG rx = F2T(Self->SX * 2);
   LONG ry = F2T(Self->SY * 2);

   if ((rx < 1) and (ry < 1)) return ERR_Okay;

   const UBYTE * src_pix_ptr;
   UBYTE * dst_pix_ptr;
   agg::rgba8 *  stack_pix_ptr;

   const LONG w = (bmp->Clip.Right - bmp->Clip.Left);
   const LONG h = (bmp->Clip.Bottom - bmp->Clip.Top);
   const LONG wm  = w - 1;
   const LONG hm  = h - 1;

   agg::pod_vector<agg::rgba8> stack;

   UBYTE A = bmp->ColourFormat->AlphaPos>>3;
   UBYTE R = bmp->ColourFormat->RedPos>>3;
   UBYTE G = bmp->ColourFormat->GreenPos>>3;
   UBYTE B = bmp->ColourFormat->BluePos>>3;

   UBYTE *data = bmp->Data + (bmp->Clip.Left<<2) + (bmp->Clip.Top * bmp->LineWidth);

   // Premultiply all the pixels.  This process is required to prevent the blur from picking up colour values in pixels
   // where the alpha = 0.  If there's no alpha channel present then a pre-multiply isn't required.  NB: Demultiply
   // takes place at the end of the routine.

   bmpPremultiply(bmp);

   ULONG sum_r, sum_g, sum_b, sum_a;
   ULONG sum_in_r, sum_in_g, sum_in_b, sum_in_a;
   ULONG sum_out_r, sum_out_g, sum_out_b, sum_out_a;
   LONG x, y, xp, yp;
   ULONG stack_ptr;
   ULONG stack_start;
   ULONG div;
   ULONG mul_sum;
   ULONG shr_sum;

   if (rx > 0) {
      if (rx > 254) rx = 254;
      div = rx * 2 + 1;
      mul_sum = stack_blur_tables<int>::g_stack_blur8_mul[rx];
      shr_sum = stack_blur_tables<int>::g_stack_blur8_shr[rx];
      stack.allocate(div);

      for (y=0; y < h; y++) {
         sum_r = sum_g = sum_b = sum_a = sum_in_r = sum_in_g = sum_in_b = sum_in_a = sum_out_r = sum_out_g = sum_out_b = sum_out_a = 0;

         src_pix_ptr = data + (bmp->LineWidth * y);
         for (LONG i=0; i <= rx; i++) {
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

         for (LONG i=1; i <= rx; i++) {
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
         if (xp > wm) xp = wm;
         src_pix_ptr = data + (bmp->LineWidth * y) + (xp<<2);
         dst_pix_ptr = data + (bmp->LineWidth * y);
         for (LONG x=0; x < w; x++) {
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
            if (stack_start >= div) stack_start -= div;
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
            if (stack_ptr >= div) stack_ptr = 0;
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
      if (ry > 254) ry = 254;
      div = ry * 2 + 1;
      mul_sum = stack_blur_tables<int>::g_stack_blur8_mul[ry];
      shr_sum = stack_blur_tables<int>::g_stack_blur8_shr[ry];
      stack.allocate(div);

      int stride = bmp->LineWidth;
      for (x = 0; x < w; x++) {
         sum_r = sum_g = sum_b = sum_a = sum_in_r = sum_in_g = sum_in_b = sum_in_a = sum_out_r = sum_out_g = sum_out_b = sum_out_a = 0;

         src_pix_ptr = data + (x<<2);
         for (LONG i = 0; i <= ry; i++) {
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

         for (LONG i = 1; i <= ry; i++) {
             if (i <= hm) src_pix_ptr += stride;
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
         if (yp > hm) yp = hm;
         src_pix_ptr = data + (x<<2) + (bmp->LineWidth * yp);
         dst_pix_ptr = data + (x<<2);
         for (LONG y = 0; y < h; y++) {
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
            if (stack_start >= div) stack_start -= div;

            stack_pix_ptr = &stack[stack_start];
            sum_out_r -= stack_pix_ptr->r;
            sum_out_g -= stack_pix_ptr->g;
            sum_out_b -= stack_pix_ptr->b;
            sum_out_a -= stack_pix_ptr->a;

            if (yp < hm) {
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
            if (stack_ptr >= div) stack_ptr = 0;
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

   bmpDemultiply(bmp);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
SX: The standard deviation of the blur on the x axis.

The (SX,SY) field values define the standard deviation of the gaussian blur along each axis.

If either value is 0 or less, the effect is disabled on that axis.

*********************************************************************************************************************/

static ERROR BLURFX_GET_SX(objBlurFX *Self, DOUBLE *Value)
{
   *Value = Self->SX;
   return ERR_Okay;
}

static ERROR BLURFX_SET_SX(objBlurFX *Self, DOUBLE Value)
{
   Self->SX = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
SY: The standard deviation of the blur on the x axis.

The (SX,SY) field values define the standard deviation of the gaussian blur along each axis.

If either value is 0 or less, the effect is disabled on that axis.

*********************************************************************************************************************/

static ERROR BLURFX_GET_SY(objBlurFX *Self, DOUBLE *Value)
{
   *Value = Self->SY;
   return ERR_Okay;
}

static ERROR BLURFX_SET_SY(objBlurFX *Self, DOUBLE Value)
{
   Self->SY = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
XMLDef: Returns an SVG compliant XML string that describes the effect.
-END-

*********************************************************************************************************************/

static ERROR BLURFX_GET_XMLDef(objBlurFX *Self, STRING *Value)
{
   std::stringstream stream;
   stream << "feGaussianBlur stdDeviation=\"" << Self->SX << " " << Self->SY << "\"";
   *Value = StrClone(stream.str().c_str());
   return ERR_Okay;
}

//********************************************************************************************************************

#include "filter_blur_def.c"

static const FieldArray clBlurFXFields[] = {
   { "SX",     FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, 0, (APTR)BLURFX_GET_SX, (APTR)BLURFX_SET_SX },
   { "SY",     FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, 0, (APTR)BLURFX_GET_SY, (APTR)BLURFX_SET_SY },
   { "XMLDef", FDF_VIRTUAL|FDF_STRING|FDF_ALLOC|FDF_R, 0, (APTR)BLURFX_GET_XMLDef, NULL },
   END_FIELD
};

//********************************************************************************************************************

ERROR init_blurfx(void)
{
   return(CreateObject(ID_METACLASS, 0, &clBlurFX,
      FID_BaseClassID|TLONG, ID_FILTEREFFECT,
      FID_SubClassID|TLONG,  ID_BLURFX,
      FID_Name|TSTRING,      "BlurFX",
      FID_Category|TLONG,    CCF_GRAPHICS,
      FID_Flags|TLONG,       CLF_PRIVATE_ONLY|CLF_PROMOTE_INTEGRAL,
      FID_Actions|TPTR,      clBlurFXActions,
      FID_Fields|TARRAY,     clBlurFXFields,
      FID_Size|TLONG,        sizeof(objBlurFX),
      FID_Path|TSTR,         MOD_PATH,
      TAGEND));
}
