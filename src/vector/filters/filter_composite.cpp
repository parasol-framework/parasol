
enum {
   OP_OVER=0, OP_IN, OP_OUT, OP_ATOP, OP_XOR, OP_ARITHMETIC, OP_NORMAL, OP_SCREEN, OP_MULTIPLY, OP_LIGHTEN, OP_DARKEN,
   OP_INVERTRGB, OP_INVERT, OP_CONTRAST, OP_DODGE, OP_BURN, OP_HARDLIGHT, OP_SOFTLIGHT, OP_DIFFERENCE, OP_EXCLUSION,
   OP_PLUS, OP_MINUS, OP_OVERLAY
};

template <class DrawOp>
void doComposite(VectorEffect *Effect, objBitmap *SrcBitmap, UBYTE *Dest, UBYTE *Src, LONG Width, LONG Height)
{
   const UBYTE A = Effect->Bitmap->ColourFormat->AlphaPos>>3;
   const UBYTE R = Effect->Bitmap->ColourFormat->RedPos>>3;
   const UBYTE G = Effect->Bitmap->ColourFormat->GreenPos>>3;
   const UBYTE B = Effect->Bitmap->ColourFormat->BluePos>>3;

   for (LONG y=0; y < Height; y++) {
      UBYTE *dp = Dest;
      UBYTE *sp = Src;
      for (LONG x=0; x < Width; x++) {
         DrawOp::blend_pix(dp, sp[R], sp[G], sp[B], sp[A], 255);
         dp += 4;
         sp += 4;
      }
      Dest += Effect->Bitmap->LineWidth;
      Src  += SrcBitmap->LineWidth;
   }
}

/*****************************************************************************
** Internal: apply_composite()
*/

static void apply_composite(objVectorFilter *Self, VectorEffect *Effect)
{
   objBitmap *bmp = Effect->Bitmap;
   if (bmp->BytesPerPixel != 4) return;

   objBitmap *srcbmp;

   if (!get_bitmap(Self, &srcbmp, Effect->Composite.Source, Effect->Input)) {
      UBYTE *dest = bmp->Data    + (bmp->Clip.Left * bmp->BytesPerPixel)       + (bmp->Clip.Top * bmp->LineWidth);
      UBYTE *src  = srcbmp->Data + (srcbmp->Clip.Left * srcbmp->BytesPerPixel) + (srcbmp->Clip.Top * srcbmp->LineWidth);

      LONG height = bmp->Clip.Bottom - bmp->Clip.Top;
      LONG width  = bmp->Clip.Right - bmp->Clip.Left;
      if (srcbmp->Clip.Right-srcbmp->Clip.Left < width) width = srcbmp->Clip.Right - srcbmp->Clip.Left;
      if (srcbmp->Clip.Bottom-srcbmp->Clip.Top < height) height = srcbmp->Clip.Bottom - srcbmp->Clip.Top;

      premultiply_bitmap(srcbmp);
      premultiply_bitmap(bmp);

      const UBYTE A = bmp->ColourFormat->AlphaPos>>3;
      const UBYTE R = bmp->ColourFormat->RedPos>>3;
      const UBYTE G = bmp->ColourFormat->GreenPos>>3;
      const UBYTE B = bmp->ColourFormat->BluePos>>3;

      if (Effect->Composite.Operator IS OP_OVER) {
         for (LONG y=0; y < height; y++) {
            UBYTE *dp = dest;
            UBYTE *sp = src;
            for (LONG x=0; x < width; x++) {
               if (dp[3]) {
                  dp[R] = dp[R] + (((sp[R] - dp[R]) * sp[A])>>8);
                  dp[G] = dp[G] + (((sp[G] - dp[G]) * sp[A])>>8);
                  dp[B] = dp[B] + (((sp[B] - dp[B]) * sp[A])>>8);
                  dp[A] = dp[A] + ((sp[A] * (255 - dp[A]))>>8);
               }
               else {
                  dp[R] = sp[R];
                  dp[G] = sp[G];
                  dp[B] = sp[B];
                  dp[A] = sp[A];
               }

               dp += 4;
               sp += 4;
            }
            dest += bmp->LineWidth;
            src  += srcbmp->LineWidth;
         }
      }
      else if (Effect->Composite.Operator IS OP_IN) {
         for (LONG y=0; y < height; y++) {
            UBYTE *dp = dest;
            UBYTE *sp = src;
            for (LONG x=0; x < width; x++) {
               dp[R] = (sp[R] * dp[A] + 0xff)>>8;
               dp[G] = (sp[G] * dp[A] + 0xff)>>8;
               dp[B] = (sp[B] * dp[A] + 0xff)>>8;
               dp[A] = (sp[A] * dp[A] + 0xff)>>8;
               dp += 4;
               sp += 4;
            }
            dest += bmp->LineWidth;
            src  += srcbmp->LineWidth;
         }
      }
      else if (Effect->Composite.Operator IS OP_OUT) {
         doComposite<agg::comp_op_rgba_src_out <agg::rgba8, agg::order_bgra>>(Effect, srcbmp, dest, src, width, height);
      }
      else if (Effect->Composite.Operator IS OP_ATOP) {
         doComposite<agg::comp_op_rgba_src_atop <agg::rgba8, agg::order_bgra>>(Effect, srcbmp, dest, src, width, height);
      }
      else if (Effect->Composite.Operator IS OP_XOR) {
         doComposite<agg::comp_op_rgba_xor <agg::rgba8, agg::order_bgra>>(Effect, srcbmp, dest, src, width, height);
      }
      else if (Effect->Composite.Operator IS OP_ARITHMETIC) {
         DOUBLE k1=Effect->Composite.K1, k2=Effect->Composite.K2, k3=Effect->Composite.K3, k4=Effect->Composite.K4;
         k1 = k1 * (1.0 / 255.0);
         k4 = k4 * 255.0;

         for (LONG y=0; y < height; y++) {
            UBYTE *dp = dest;
            UBYTE *sp = src;
            for (LONG x=0; x < width; x++) {
               #define i1(c) sp[c]
               #define i2(c) dp[c]
               LONG dr = (k1 * i1(R) * i2(R)) + (k2 * i1(R)) + (k3 * i2(R)) + k4;
               LONG dg = (k1 * i1(G) * i2(G)) + (k2 * i1(G)) + (k3 * i2(G)) + k4;
               LONG db = (k1 * i1(B) * i2(B)) + (k2 * i1(B)) + (k3 * i2(B)) + k4;
               LONG da = (k1 * i1(A) * i2(A)) + (k2 * i1(A)) + (k3 * i2(A)) + k4;
               if (dr > 0xff) dp[R] = 0xff;
               else if (dr < 0) dp[R] = 0;
               else dp[R] = dr;

               if (dg > 0xff) dp[G] = 0xff;
               else if (dg < 0) dp[G] = 0;
               else dp[G] = dg;

               if (db > 0xff) dp[B] = 0xff;
               else if (db < 0) dp[B] = 0;
               else dp[B] = db;

               if (da > 0xff) dp[A] = 0xff;
               else if (da < 0) dp[A] = 0;
               else dp[A] = da;

               dp += 4;
               sp += 4;
            }
            dest += bmp->LineWidth;
            src  += srcbmp->LineWidth;
         }
      }
      else if (Effect->Composite.Operator IS OP_MULTIPLY) {
         doComposite<agg::comp_op_rgba_multiply <agg::rgba8, agg::order_bgra>>(Effect, srcbmp, dest, src, width, height);
      }
      else if (Effect->Composite.Operator IS OP_SCREEN) {
         doComposite<agg::comp_op_rgba_screen <agg::rgba8, agg::order_bgra>>(Effect, srcbmp, dest, src, width, height);
      }
      else if (Effect->Composite.Operator IS OP_DARKEN) {
         doComposite<agg::comp_op_rgba_multiply <agg::rgba8, agg::order_bgra>>(Effect, srcbmp, dest, src, width, height);
      }
      else if (Effect->Composite.Operator IS OP_LIGHTEN) {
         doComposite<agg::comp_op_rgba_multiply <agg::rgba8, agg::order_bgra>>(Effect, srcbmp, dest, src, width, height);
      }
      else if (Effect->Composite.Operator IS OP_OVERLAY) {
         doComposite<agg::comp_op_rgba_overlay <agg::rgba8, agg::order_bgra>>(Effect, srcbmp, dest, src, width, height);
      }
      else if (Effect->Composite.Operator IS OP_BURN) {
         doComposite<agg::comp_op_rgba_color_burn <agg::rgba8, agg::order_bgra>>(Effect, srcbmp, dest, src, width, height);
      }
      else if (Effect->Composite.Operator IS OP_DODGE) {
         doComposite<agg::comp_op_rgba_color_dodge <agg::rgba8, agg::order_bgra>>(Effect, srcbmp, dest, src, width, height);
      }
      else if (Effect->Composite.Operator IS OP_HARDLIGHT) {
         doComposite<agg::comp_op_rgba_hard_light <agg::rgba8, agg::order_bgra>>(Effect, srcbmp, dest, src, width, height);
      }
      else if (Effect->Composite.Operator IS OP_SOFTLIGHT) {
         doComposite<agg::comp_op_rgba_soft_light <agg::rgba8, agg::order_bgra>>(Effect, srcbmp, dest, src, width, height);
      }
      else if (Effect->Composite.Operator IS OP_DIFFERENCE) {
         doComposite<agg::comp_op_rgba_difference <agg::rgba8, agg::order_bgra>>(Effect, srcbmp, dest, src, width, height);
      }
      else if (Effect->Composite.Operator IS OP_EXCLUSION) {
         doComposite<agg::comp_op_rgba_exclusion <agg::rgba8, agg::order_bgra>>(Effect, srcbmp, dest, src, width, height);
      }
      else if (Effect->Composite.Operator IS OP_PLUS) {
         doComposite<agg::comp_op_rgba_plus <agg::rgba8, agg::order_bgra>>(Effect, srcbmp, dest, src, width, height);
      }
      else if (Effect->Composite.Operator IS OP_MINUS) {
         doComposite<agg::comp_op_rgba_minus <agg::rgba8, agg::order_bgra>>(Effect, srcbmp, dest, src, width, height);
      }
      else if (Effect->Composite.Operator IS OP_CONTRAST) {
         doComposite<agg::comp_op_rgba_multiply <agg::rgba8, agg::order_bgra>>(Effect, srcbmp, dest, src, width, height);
      }
      else if (Effect->Composite.Operator IS OP_INVERT) {
         doComposite<agg::comp_op_rgba_invert <agg::rgba8, agg::order_bgra>>(Effect, srcbmp, dest, src, width, height);
      }
      else if (Effect->Composite.Operator IS OP_INVERTRGB) {
         doComposite<agg::comp_op_rgba_invert_rgb <agg::rgba8, agg::order_bgra>>(Effect, srcbmp, dest, src, width, height);
      }

      demultiply_bitmap(srcbmp);
      demultiply_bitmap(bmp);
   }
}

//****************************************************************************
// Create a new composite filter.

static ERROR create_composite(objVectorFilter *Self, XMLTag *Tag)
{
   VectorEffect *effect;

   if (!(effect = add_effect(Self, FE_COMPOSITE))) return ERR_AllocMemory;
   effect->Composite.Operator = OP_OVER;

   for (LONG a=1; a < Tag->TotalAttrib; a++) {
      CSTRING val = Tag->Attrib[a].Value;
      if (!val) continue;

      ULONG hash = StrHash(Tag->Attrib[a].Name, FALSE);
      switch(hash) {
         case SVF_IN: {
            switch (StrHash(val, FALSE)) {
               case SVF_SOURCEGRAPHIC:   effect->Composite.Source = VSF_GRAPHIC; break;
               case SVF_SOURCEALPHA:     effect->Composite.Source = VSF_ALPHA; break;
               case SVF_BACKGROUNDIMAGE: effect->Composite.Source = VSF_BKGD; break;
               case SVF_BACKGROUNDALPHA: effect->Composite.Source = VSF_BKGD_ALPHA; break;
               case SVF_FILLPAINT:       effect->Composite.Source = VSF_FILL; break;
               case SVF_STROKEPAINT:     effect->Composite.Source = VSF_STROKE; break;
               default:  {
                  VectorEffect *e;
                  if ((e = find_effect(Self, val))) {
                     if (e != effect) {
                        effect->Composite.Source = VSF_REFERENCE;
                        effect->Input = e;
                     }
                  }
                  break;
               }
            }
            break;
         }

         case SVF_IN2: { // 'In2' is usually the BackgroundImage that 'in' will be copied over.
            switch (StrHash(val, FALSE)) {
               case SVF_SOURCEGRAPHIC:   effect->Source = VSF_GRAPHIC; break;
               case SVF_SOURCEALPHA:     effect->Source = VSF_ALPHA; break;
               case SVF_BACKGROUNDIMAGE: effect->Source = VSF_BKGD; break;
               case SVF_BACKGROUNDALPHA: effect->Source = VSF_BKGD_ALPHA; break;
               case SVF_FILLPAINT:       effect->Source = VSF_FILL; break;
               case SVF_STROKEPAINT:     effect->Source = VSF_STROKE; break;
               default:  {
                  VectorEffect *e;
                  if ((e = find_effect(Self, val))) {
                     if (e != effect) {
                        effect->Source = VSF_REFERENCE;
                        effect->Input = e;
                     }
                  }
                  break;
               }
            }
            break;
         }

         case SVF_MODE:
         case SVF_OPERATOR: {
            switch (StrHash(val, FALSE)) {
               // SVG Operator types
               case SVF_OVER: effect->Composite.Operator = OP_OVER; break;
               case SVF_IN:   effect->Composite.Operator = OP_IN; break;
               case SVF_OUT:  effect->Composite.Operator = OP_OUT; break;
               case SVF_ATOP: effect->Composite.Operator = OP_ATOP; break;
               case SVF_XOR:  effect->Composite.Operator = OP_XOR; break;
               case SVF_ARITHMETIC: effect->Composite.Operator = OP_ARITHMETIC; break;
               // SVG Mode types
               case SVF_NORMAL:   effect->Composite.Operator = OP_NORMAL; break;
               case SVF_SCREEN:   effect->Composite.Operator = OP_SCREEN; break;
               case SVF_MULTIPLY: effect->Composite.Operator = OP_MULTIPLY; break;
               case SVF_LIGHTEN:  effect->Composite.Operator = OP_LIGHTEN; break;
               case SVF_DARKEN:   effect->Composite.Operator = OP_DARKEN; break;
               // Parasol modes
               case SVF_INVERTRGB:  effect->Composite.Operator = OP_INVERTRGB; break;
               case SVF_INVERT:     effect->Composite.Operator = OP_INVERT; break;
               case SVF_CONTRAST:   effect->Composite.Operator = OP_CONTRAST; break;
               case SVF_DODGE:      effect->Composite.Operator = OP_DODGE; break;
               case SVF_BURN:       effect->Composite.Operator = OP_BURN; break;
               case SVF_HARDLIGHT:  effect->Composite.Operator = OP_HARDLIGHT; break;
               case SVF_SOFTLIGHT:  effect->Composite.Operator = OP_SOFTLIGHT; break;
               case SVF_DIFFERENCE: effect->Composite.Operator = OP_DIFFERENCE; break;
               case SVF_EXCLUSION:  effect->Composite.Operator = OP_EXCLUSION; break;
               case SVF_PLUS:       effect->Composite.Operator = OP_PLUS; break;
               case SVF_MINUS:      effect->Composite.Operator = OP_MINUS; break;
               case SVF_OVERLAY:    effect->Composite.Operator = OP_OVERLAY; break;
               default:
                  LogErrorMsg("Composite operator '%s' not recognised.", val);
                  remove_effect(Self, effect);
                  return ERR_InvalidValue;
            }
            break;
         }

         case SVF_K1: read_numseq(val, &effect->Composite.K1, TAGEND); break;
         case SVF_K2: read_numseq(val, &effect->Composite.K2, TAGEND); break;
         case SVF_K3: read_numseq(val, &effect->Composite.K3, TAGEND); break;
         case SVF_K4: read_numseq(val, &effect->Composite.K4, TAGEND); break;
         default: fe_default(Self, effect, hash, val); break;
      }
   }

   if (!effect->Composite.Source) {
      LogErrorMsg("Composite element requires 'in2' attribute.");
      return ERR_FieldNotSet;
   }

   return ERR_Okay;
}
