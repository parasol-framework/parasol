
enum Operation {
   OP_OVER=0, OP_IN, OP_OUT, OP_ATOP, OP_XOR, OP_ARITHMETIC, OP_NORMAL, OP_SCREEN, OP_MULTIPLY, OP_LIGHTEN, OP_DARKEN,
   OP_INVERTRGB, OP_INVERT, OP_CONTRAST, OP_DODGE, OP_BURN, OP_HARDLIGHT, OP_SOFTLIGHT, OP_DIFFERENCE, OP_EXCLUSION,
   OP_PLUS, OP_MINUS, OP_OVERLAY
};

//********************************************************************************************************************
// Porter/Duff Compositing routines
// D = Dest; I = Input; M = Mix

struct composite_over {
   static inline void blend(UBYTE *D, UBYTE *I, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if (M[A]) {
         D[R] = M[R] + (((I[R] - M[R]) * I[A])>>8);
         D[G] = M[G] + (((I[G] - M[G]) * I[A])>>8);
         D[B] = M[B] + (((I[B] - M[B]) * I[A])>>8);
         D[A] = M[A] + ((I[A] * (255 - M[A]))>>8);
      }
      else ((ULONG *)D)[0] = ((ULONG *)I)[0];
   }
};

struct composite_in {
   static inline void blend(UBYTE *D, UBYTE *I, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      UBYTE alpha = (M[A] * I[A] + 0xff)>>8;
      if (alpha IS 255) ((ULONG *)D)[0] = ((ULONG *)I)[0];
      else {
         D[R] = (I[R] * alpha + 0xff)>>8;
         D[G] = (I[G] * alpha + 0xff)>>8;
         D[B] = (I[B] * alpha + 0xff)>>8;
         D[A] = (I[A] * alpha + 0xff)>>8;
      }
   }
};

struct composite_out {
   static inline void blend(UBYTE *D, UBYTE *I, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      auto alpha = M[A] ^ 0xff;
      D[R] = (I[R] * alpha + 0xff)>>8;
      D[G] = (I[G] * alpha + 0xff)>>8;
      D[B] = (I[B] * alpha + 0xff)>>8;
      D[A] = (I[A] * alpha + 0xff)>>8;
   }
};

struct composite_atop {
   static inline void blend(UBYTE *D, UBYTE *I, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      auto alpha = M[A];
      D[R] = (((I[R] * I[A] + (M[R] * (I[A] ^ 0xff)))>>8) * alpha + 0xff)>>8;
      D[G] = (((I[G] * I[A] + (M[G] * (I[A] ^ 0xff)))>>8) * alpha + 0xff)>>8;
      D[B] = (((I[B] * I[A] + (M[B] * (I[A] ^ 0xff)))>>8) * alpha + 0xff)>>8;
      D[A] = (((I[A] * I[A] + (M[A] * (I[A] ^ 0xff)))>>8) * alpha + 0xff)>>8;
   }
};

struct composite_xor {
   static inline void blend(UBYTE *D, UBYTE *I, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      auto alpha = I[A] ^ M[A];
      D[R] = (((I[R] * I[A] + (M[R] * (I[A] ^ 0xff)))>>8) * alpha + 0xff)>>8;
      D[G] = (((I[G] * I[A] + (M[G] * (I[A] ^ 0xff)))>>8) * alpha + 0xff)>>8;
      D[B] = (((I[B] * I[A] + (M[B] * (I[A] ^ 0xff)))>>8) * alpha + 0xff)>>8;
      D[A] = (((I[A] * I[A] + (M[A] * (I[A] ^ 0xff)))>>8) * alpha + 0xff)>>8;
   }
};

//********************************************************************************************************************
// Blending algorithms, refer to https://en.wikipedia.org/wiki/Blend_modes

struct blend_screen {
   static inline void blend(UBYTE *D, UBYTE *I, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      D[R] = (UBYTE)(I[R] + M[R] - ((I[R] * M[R] + 0Xff) >> 8));
      D[G] = (UBYTE)(I[G] + M[G] - ((I[G] * M[G] + 0Xff) >> 8));
      D[B] = (UBYTE)(I[B] + M[B] - ((I[B] * M[B] + 0Xff) >> 8));
      D[A] = (UBYTE)(I[A] + M[A] - ((I[A] * M[A] + 0Xff) >> 8));
   }
};

struct blend_multiply {
   static inline void blend(UBYTE *D, UBYTE *I, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((I[A]) or (M[A])) {
         const UBYTE s1a = 0xff - I[A];
         const UBYTE d1a = 0xff - M[A];
         D[R] = (UBYTE)((I[R] * M[R] + (I[R] * d1a) + (M[R] * s1a) + 0xff) >> 8);
         D[G] = (UBYTE)((I[G] * M[G] + (I[G] * d1a) + (M[G] * s1a) + 0xff) >> 8);
         D[B] = (UBYTE)((I[B] * M[B] + (I[B] * d1a) + (M[B] * s1a) + 0xff) >> 8);
         D[A] = (UBYTE)(I[A] + M[A] - ((I[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_darken {
   static inline void blend(UBYTE *D, UBYTE *I, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((I[A]) or (M[A])) {
         UBYTE d1a = 0xff - D[A];
         UBYTE s1a = 0xff - I[A];
         UBYTE da  = D[A];

         D[R] = (UBYTE)((agg::sd_min(I[R] * da, M[R] * I[A]) + I[R] * d1a + M[R] * s1a + 0xff) >> 8);
         D[G] = (UBYTE)((agg::sd_min(I[G] * da, M[G] * I[A]) + I[G] * d1a + M[G] * s1a + 0xff) >> 8);
         D[B] = (UBYTE)((agg::sd_min(I[B] * da, M[B] * I[A]) + I[B] * d1a + M[B] * s1a + 0xff) >> 8);
         D[A] = (UBYTE)(I[A] + M[A] - ((I[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_lighten {
   static inline void blend(UBYTE *D, UBYTE *I, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((I[A]) or (M[A])) {
         UBYTE d1a = 0xff - D[A];
         UBYTE s1a = 0xff - I[A];

         D[R] = (UBYTE)((agg::sd_max(I[R] * M[A], M[R] * I[A]) + I[R] * d1a + M[R] * s1a + 0xff) >> 8);
         D[G] = (UBYTE)((agg::sd_max(I[G] * M[A], M[G] * I[A]) + I[G] * d1a + M[G] * s1a + 0xff) >> 8);
         D[B] = (UBYTE)((agg::sd_max(I[B] * M[A], M[B] * I[A]) + I[B] * d1a + M[B] * s1a + 0xff) >> 8);
         D[A] = (UBYTE)(I[A] + M[A] - ((I[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_dodge {
   static inline void blend(UBYTE *D, UBYTE *I, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((I[A]) or (M[A])) {
         LONG d1a  = 0xff - M[A];
         LONG s1a  = 0xff - I[A];
         LONG drsa = M[G] * I[A];
         LONG dgsa = M[B] * I[A];
         LONG dbsa = M[B] * I[A];
         LONG srda = I[R] * M[A];
         LONG sgda = I[G] * M[A];
         LONG sbda = I[B] * M[A];
         LONG sada = I[A] * M[A];

         D[R] = (UBYTE)((srda + drsa >= sada) ?
             (sada + I[R] * d1a + M[R] * s1a + 0xff) >> 8 :
             drsa / (0xff - (I[R] << 8) / I[A]) + ((I[R] * d1a + M[R] * s1a + 0xff) >> 8));

         D[G] = (UBYTE)((sgda + dgsa >= sada) ?
             (sada + I[G] * d1a + M[G] * s1a + 0xff) >> 8 :
             dgsa / (0xff - (I[G] << 8) / I[A]) + ((I[G] * d1a + M[G] * s1a + 0xff) >> 8));

         D[B] = (UBYTE)((sbda + dbsa >= sada) ?
             (sada + I[B] * d1a + M[B] * s1a + 0xff) >> 8 :
             dbsa / (0xff - (I[B] << 8) / I[A]) + ((I[B] * d1a + M[B] * s1a + 0xff) >> 8));

         D[A] = (UBYTE)(I[A] + M[A] - ((I[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_contrast {
   static inline void blend(UBYTE *D, UBYTE *I, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      LONG d2a = M[A] >> 1;
      UBYTE s2a = I[A] >> 1;

      int r = (int)((((M[R] - d2a) * int((I[R] - s2a)*2 + 0xff)) >> 8) + d2a);
      int g = (int)((((M[G] - d2a) * int((I[G] - s2a)*2 + 0xff)) >> 8) + d2a);
      int b = (int)((((M[B] - d2a) * int((I[B] - s2a)*2 + 0xff)) >> 8) + d2a);

      r = (r < 0) ? 0 : r;
      g = (g < 0) ? 0 : g;
      b = (b < 0) ? 0 : b;

      D[R] = (UBYTE)((r > M[A]) ? M[A] : r);
      D[G] = (UBYTE)((g > M[A]) ? M[A] : g);
      D[B] = (UBYTE)((b > M[A]) ? M[A] : b);
   }
};

struct blend_overlay {
   static inline void blend(UBYTE *D, UBYTE *I, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((I[A]) or (M[A])) {
         UBYTE d1a = 0xff - M[A];
         UBYTE s1a = 0xff - I[A];
         UBYTE sada = I[A] * M[A];

         D[R] = (UBYTE)(((2*M[R] < M[A]) ?
             2*I[R]*M[R] + I[R]*d1a + M[R]*s1a :
             sada - 2*(M[A] - M[R])*(I[A] - I[R]) + I[R]*d1a + M[R]*s1a + 0xff) >> 8);

         D[G] = (UBYTE)(((2*M[G] < M[A]) ?
             2*I[G]*M[G] + I[G]*d1a + M[G]*s1a :
             sada - 2*(M[A] - M[G])*(I[A] - I[G]) + I[G]*d1a + M[G]*s1a + 0xff) >> 8);

         D[B] = (UBYTE)(((2*M[B] < M[A]) ?
             2*I[B]*M[B] + I[B]*d1a + M[B]*s1a :
             sada - 2*(M[A] - M[B])*(I[A] - I[B]) + I[B]*d1a + M[B]*s1a + 0xff) >> 8);

         D[A] = (UBYTE)(I[A] + M[A] - ((I[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_burn {
   static inline void blend(UBYTE *D, UBYTE *I, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((I[A]) or (M[A])) {
         const UBYTE d1a = 0xff - D[A];
         const UBYTE s1a = 0xff - I[A];
         const LONG drsa = M[R] * I[A];
         const LONG dgsa = M[G] * I[A];
         const LONG dbsa = M[B] * I[A];
         const LONG srda = I[R] * M[A];
         const LONG sgda = I[G] * M[A];
         const LONG sbda = I[B] * M[A];
         const LONG sada = I[A] * M[A];

         D[R] = (UBYTE)(((srda + drsa <= sada) ?
             I[R] * d1a + M[R] * s1a :
             I[A] * (srda + drsa - sada) / I[R] + I[R] * d1a + M[R] * s1a + 0xff) >> 8);

         D[G] = (UBYTE)(((sgda + dgsa <= sada) ?
             I[G] * d1a + M[G] * s1a :
             I[A] * (sgda + dgsa - sada) / I[G] + I[G] * d1a + M[G] * s1a + 0xff) >> 8);

         D[B] = (UBYTE)(((sbda + dbsa <= sada) ?
             I[B] * d1a + M[B] * s1a :
             I[A] * (sbda + dbsa - sada) / I[B] + I[B] * d1a + M[B] * s1a + 0xff) >> 8);

         D[A] = (UBYTE)(I[A] + M[A] - ((I[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_hard_light {
   static inline void blend(UBYTE *D, UBYTE *I, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((I[A]) or (M[A])) {
         UBYTE d1a  = 0xff - D[A];
         UBYTE s1a  = 0xff - I[A];
         UBYTE sada = I[A] * M[A];

         D[R] = (UBYTE)(((2*I[R] < I[A]) ?
             2*I[R]*M[R] + I[R]*d1a + M[R]*s1a :
             sada - 2*(M[A] - M[R])*(I[A] - I[R]) + I[R]*d1a + M[R]*s1a + 0xff) >> 8);

         D[G] = (UBYTE)(((2*I[G] < I[A]) ?
             2*I[G]*M[G] + I[G]*d1a + M[G]*s1a :
             sada - 2*(M[A] - M[G])*(I[A] - I[G]) + I[G]*d1a + M[G]*s1a + 0xff) >> 8);

         D[B] = (UBYTE)(((2*I[B] < I[A]) ?
             2*I[B]*M[B] + I[B]*d1a + M[B]*s1a :
             sada - 2*(M[A] - M[B])*(I[A] - I[B]) + I[B]*d1a + M[B]*s1a + 0xff) >> 8);

         D[A] = (UBYTE)(I[A] + M[A] - ((I[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_soft_light {
   static inline void blend(UBYTE *D, UBYTE *I, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((I[A]) or (M[A])) {
         double xr = double(D[R]) / 0xff;
         double xg = double(D[G]) / 0xff;
         double xb = double(D[B]) / 0xff;
         double da = double(D[A] ? D[A] : 1) / 0xff;

         if(2*I[R] < I[A])   xr = xr*(I[A] + (1 - xr/da)*(2*I[R] - I[A])) + I[R]*(1 - da) + xr*(1 - I[A]);
         else if(8*xr <= da) xr = xr*(I[A] + (1 - xr/da)*(2*I[R] - I[A])*(3 - 8*xr/da)) + I[R]*(1 - da) + xr*(1 - I[A]);
         else                xr = (xr*I[A] + (sqrt(xr/da)*da - xr)*(2*I[R] - I[A])) + I[R]*(1 - da) + xr*(1 - I[A]);

         if(2*I[G] < I[A])   xg = xg*(I[A] + (1 - xg/da)*(2*I[G] - I[A])) + I[G]*(1 - da) + xg*(1 - I[A]);
         else if(8*xg <= da) xg = xg*(I[A] + (1 - xg/da)*(2*I[G] - I[A])*(3 - 8*xg/da)) + I[G]*(1 - da) + xg*(1 - I[A]);
         else                xg = (xg*I[A] + (sqrt(xg/da)*da - xg)*(2*I[G] - I[A])) + I[G]*(1 - da) + xg*(1 - I[A]);

         if(2*I[B] < I[A])   xb = xb*(I[A] + (1 - xb/da)*(2*I[B] - I[A])) + I[B]*(1 - da) + xb*(1 - I[A]);
         else if(8*xb <= da) xb = xb*(I[A] + (1 - xb/da)*(2*I[B] - I[A])*(3 - 8*xb/da)) + I[B]*(1 - da) + xb*(1 - I[A]);
         else                xb = (xb*I[A] + (sqrt(xb/da)*da - xb)*(2*I[B] - I[A])) + I[B]*(1 - da) + xb*(1 - I[A]);

         D[R] = (UBYTE)agg::uround(xr * 0xff);
         D[G] = (UBYTE)agg::uround(xg * 0xff);
         D[B] = (UBYTE)agg::uround(xb * 0xff);
         D[A] = (UBYTE)(I[A] + D[A] - ((I[A] * D[A] + 0xff) >> 8));
      }
   }
};

struct blend_difference {
   static inline void blend(UBYTE *D, UBYTE *I, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((I[A]) or (M[A])) {
         D[R] = (UBYTE)(I[R] + M[R] - ((2 * agg::sd_min(I[R]*M[A], M[R]*I[A]) + 0xff) >> 8));
         D[G] = (UBYTE)(I[G] + M[G] - ((2 * agg::sd_min(I[G]*M[A], M[G]*I[A]) + 0xff) >> 8));
         D[B] = (UBYTE)(I[B] + M[B] - ((2 * agg::sd_min(I[B]*M[A], M[B]*I[A]) + 0xff) >> 8));
         D[A] = (UBYTE)(I[A] + M[A] - ((I[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_exclusion {
   static inline void blend(UBYTE *D, UBYTE *I, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((I[A]) or (M[A])) {
         const UBYTE d1a = 0xff - D[A];
         const UBYTE s1a = 0xff - I[A];
         D[R] = (UBYTE)((I[R]*M[A] + M[R]*I[A] - 2*I[R]*M[R] + I[R]*d1a + M[R]*s1a + 0xff) >> 8);
         D[G] = (UBYTE)((I[G]*M[A] + M[G]*I[A] - 2*I[G]*M[G] + I[G]*d1a + M[G]*s1a + 0xff) >> 8);
         D[B] = (UBYTE)((I[B]*M[A] + M[B]*I[A] - 2*I[B]*M[B] + I[B]*d1a + M[B]*s1a + 0xff) >> 8);
         D[A] = (UBYTE)(I[A] + M[A] - ((I[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_plus {
   static inline void blend(UBYTE *D, UBYTE *I, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((I[A]) or (M[A])) {
         const UBYTE xr = D[R] + I[R];
         const UBYTE xg = D[G] + I[G];
         const UBYTE xb = D[B] + I[B];
         const UBYTE xa = D[A] + I[A];
         D[R] = (xr > 0xff) ? (UBYTE)0xff : xr;
         D[G] = (xg > 0xff) ? (UBYTE)0xff : xg;
         D[B] = (xb > 0xff) ? (UBYTE)0xff : xb;
         D[A] = (xa > 0xff) ? (UBYTE)0xff : xa;
      }
   }
};

struct blend_minus {
   static inline void blend(UBYTE *D, UBYTE *I, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((I[A]) or (M[A])) {
         const UBYTE xr = D[R] - I[R];
         const UBYTE xg = D[G] - I[G];
         const UBYTE xb = D[B] - I[B];
         D[R] = (xr > 0xff) ? 0 : xr;
         D[G] = (xg > 0xff) ? 0 : xg;
         D[B] = (xb > 0xff) ? 0 : xb;
         D[A] = (UBYTE)(I[A] + D[A] - ((I[A] * D[A] + 0xff) >> 8));
         //D[A] = (UBYTE)(0xff - (((0xff - I[A]) * (0xff - D[A]) + 0xff) >> 8));
      }
   }
};

struct blend_invert {
   static inline void blend(UBYTE *D, UBYTE *I, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((I[A]) or (M[A])) {
         const UBYTE xr = ((M[A] - D[R]) * I[A] + 0xff) >> 8;
         const UBYTE xg = ((M[A] - D[G]) * I[A] + 0xff) >> 8;
         const UBYTE xb = ((M[A] - D[B]) * I[A] + 0xff) >> 8;
         const UBYTE s1a = 0xff - I[A];
         D[R] = (UBYTE)(xr + ((D[R] * s1a + 0xff) >> 8));
         D[G] = (UBYTE)(xg + ((D[G] * s1a + 0xff) >> 8));
         D[B] = (UBYTE)(xb + ((D[B] * s1a + 0xff) >> 8));
         D[A] = (UBYTE)(I[A] + M[A] - ((I[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_invert_rgb {
   static inline void blend(UBYTE *D, UBYTE *I, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if (I[A]) {
         UBYTE xr = ((M[A] - D[R]) * I[R] + 0xff) >> 8;
         UBYTE xg = ((M[A] - D[G]) * I[G] + 0xff) >> 8;
         UBYTE xb = ((M[A] - D[B]) * I[B] + 0xff) >> 8;
         UBYTE s1a = 0xff - I[A];
         D[R] = (UBYTE)(xr + ((D[R] * s1a + 0xff) >> 8));
         D[G] = (UBYTE)(xg + ((D[G] * s1a + 0xff) >> 8));
         D[B] = (UBYTE)(xb + ((D[B] * s1a + 0xff) >> 8));
         D[A] = (UBYTE)(I[A] + M[A] - ((I[A] * M[A] + 0xff) >> 8));
      }
   }
};

//********************************************************************************************************************

class CompositeEffect : public VectorEffect {
   DOUBLE K1, K2, K3, K4; // For the arithmetic operator
   Operation Operator; // OP constant

   void xml(std::stringstream &Stream) { // TODO: Support exporting attributes
      Stream << "feComposite";
   }

   template <class DrawOp>
   void doComposite(objBitmap *SrcBitmap, UBYTE *Dest, UBYTE *Src, LONG Width, LONG Height) {
      const UBYTE A = OutBitmap->ColourFormat->AlphaPos>>3;
      const UBYTE R = OutBitmap->ColourFormat->RedPos>>3;
      const UBYTE G = OutBitmap->ColourFormat->GreenPos>>3;
      const UBYTE B = OutBitmap->ColourFormat->BluePos>>3;

      for (LONG y=0; y < Height; y++) {
         auto dp = Dest;
         auto sp = Src;
         for (LONG x=0; x < Width; x++) {
            DrawOp::blend_pix(dp, sp[R], sp[G], sp[B], sp[A], 255);
            dp += 4;
            sp += 4;
         }
         Dest += OutBitmap->LineWidth;
         Src  += SrcBitmap->LineWidth;
      }
   }

   template <class CompositeOp>
   void doMix(objBitmap *InBitmap, objBitmap *MixBitmap, UBYTE *Dest, UBYTE *In, UBYTE *Mix) {
      const UBYTE A = OutBitmap->ColourFormat->AlphaPos>>3;
      const UBYTE R = OutBitmap->ColourFormat->RedPos>>3;
      const UBYTE G = OutBitmap->ColourFormat->GreenPos>>3;
      const UBYTE B = OutBitmap->ColourFormat->BluePos>>3;

      LONG height = OutBitmap->Clip.Bottom - OutBitmap->Clip.Top;
      LONG width  = OutBitmap->Clip.Right - OutBitmap->Clip.Left;
      if (InBitmap->Clip.Right - InBitmap->Clip.Left < width) width = InBitmap->Clip.Right - InBitmap->Clip.Left;
      if (InBitmap->Clip.Bottom - InBitmap->Clip.Top < height) height = InBitmap->Clip.Bottom - InBitmap->Clip.Top;

      for (LONG y=0; y < height; y++) {
         auto dp = Dest;
         auto sp = In;
         auto mp = Mix;
         for (LONG x=0; x < width; x++) {
            CompositeOp::blend(dp, sp, mp, A, R, G, B);
            dp += 4;
            sp += 4;
            mp += 4;
         }
         Dest += OutBitmap->LineWidth;
         In   += InBitmap->LineWidth;
         Mix  += MixBitmap->LineWidth;
      }
   }

   public:

   // Initialiser

   CompositeEffect(struct rkVectorFilter *Filter, XMLTag *Tag) : VectorEffect() {
      parasol::Log log(__FUNCTION__);

      Operator   = OP_OVER;
      EffectName = "feComposite";

      for (LONG a=1; a < Tag->TotalAttrib; a++) {
         CSTRING val = Tag->Attrib[a].Value;
         if (!val) continue;

         ULONG hash = StrHash(Tag->Attrib[a].Name, FALSE);
         switch (hash) {
            case SVF_MODE:
            case SVF_OPERATOR: {
               switch (StrHash(val, FALSE)) {
                  // SVG Operator types
                  case SVF_OVER: Operator = OP_OVER; break;
                  case SVF_IN:   Operator = OP_IN; break;
                  case SVF_OUT:  Operator = OP_OUT; break;
                  case SVF_ATOP: Operator = OP_ATOP; break;
                  case SVF_XOR:  Operator = OP_XOR; break;
                  case SVF_ARITHMETIC: Operator = OP_ARITHMETIC; break;
                  // SVG Mode types
                  case SVF_NORMAL:   Operator = OP_NORMAL; break;
                  case SVF_SCREEN:   Operator = OP_SCREEN; break;
                  case SVF_MULTIPLY: Operator = OP_MULTIPLY; break;
                  case SVF_LIGHTEN:  Operator = OP_LIGHTEN; break;
                  case SVF_DARKEN:   Operator = OP_DARKEN; break;
                  // Parasol modes
                  case SVF_INVERTRGB:  Operator = OP_INVERTRGB; break;
                  case SVF_INVERT:     Operator = OP_INVERT; break;
                  case SVF_CONTRAST:   Operator = OP_CONTRAST; break;
                  case SVF_DODGE:      Operator = OP_DODGE; break;
                  case SVF_BURN:       Operator = OP_BURN; break;
                  case SVF_HARDLIGHT:  Operator = OP_HARDLIGHT; break;
                  case SVF_SOFTLIGHT:  Operator = OP_SOFTLIGHT; break;
                  case SVF_DIFFERENCE: Operator = OP_DIFFERENCE; break;
                  case SVF_EXCLUSION:  Operator = OP_EXCLUSION; break;
                  case SVF_PLUS:       Operator = OP_PLUS; break;
                  case SVF_MINUS:      Operator = OP_MINUS; break;
                  case SVF_OVERLAY:    Operator = OP_OVERLAY; break;
                  default:
                     log.warning("Composite operator '%s' not recognised.", val);
                     Error = ERR_InvalidValue;
                     return;
               }
               break;
            }

            case SVF_K1:
               read_numseq(val, &K1, TAGEND);
               K1 = K1 * (1.0 / 255.0);
               break;

            case SVF_K2: read_numseq(val, &K2, TAGEND); break;

            case SVF_K3: read_numseq(val, &K3, TAGEND); break;

            case SVF_K4:
               read_numseq(val, &K4, TAGEND);
               K4 = K4 * 255.0;
               break;

            default: fe_default(Filter, this, hash, val); break;
         }
      }

      if (!MixType) {
         log.warning("Composite element requires 'in2' attribute.");
         Error = ERR_FieldNotSet;
         return;
      }
   }

   void apply(objVectorFilter *Filter, struct filter_state &State) {
      if (OutBitmap->BytesPerPixel != 4) return;

      objBitmap *inBmp;
      if (get_source_bitmap(Filter, &inBmp, SourceType, InputID, true)) return;

      UBYTE *dest = OutBitmap->Data + (OutBitmap->Clip.Left * 4) + (OutBitmap->Clip.Top * OutBitmap->LineWidth);
      UBYTE *in  = inBmp->Data + (inBmp->Clip.Left * 4) + (inBmp->Clip.Top * inBmp->LineWidth);

      LONG height = OutBitmap->Clip.Bottom - OutBitmap->Clip.Top;
      LONG width  = OutBitmap->Clip.Right - OutBitmap->Clip.Left;
      if (inBmp->Clip.Right - inBmp->Clip.Left < width) width = inBmp->Clip.Right - inBmp->Clip.Left;
      if (inBmp->Clip.Bottom - inBmp->Clip.Top < height) height = inBmp->Clip.Bottom - inBmp->Clip.Top;

      premultiply_bitmap(inBmp);

      const UBYTE A = OutBitmap->ColourFormat->AlphaPos>>3;
      const UBYTE R = OutBitmap->ColourFormat->RedPos>>3;
      const UBYTE G = OutBitmap->ColourFormat->GreenPos>>3;
      const UBYTE B = OutBitmap->ColourFormat->BluePos>>3;

      switch (Operator) {
         case OP_NORMAL:
         case OP_OVER: {
            objBitmap *mixBmp;
            if (!get_source_bitmap(Filter, &mixBmp, MixType, MixID, true)) {
               UBYTE *mix = mixBmp->Data + (mixBmp->Clip.Left * 4) + (mixBmp->Clip.Top * mixBmp->LineWidth);
               doMix<composite_over>(inBmp, mixBmp, dest, in, mix);
               demultiply_bitmap(mixBmp);
            }
            break;
         }

         case OP_IN: {
            objBitmap *mixBmp;
            if (!get_source_bitmap(Filter, &mixBmp, MixType, MixID, false)) {
               UBYTE *mix = mixBmp->Data + (mixBmp->Clip.Left * 4) + (mixBmp->Clip.Top * mixBmp->LineWidth);
               doMix<composite_in>(inBmp, mixBmp, dest, in, mix);
            }
            break;
         }

         case OP_OUT: {
            objBitmap *mixBmp;
            if (!get_source_bitmap(Filter, &mixBmp, MixType, MixID, false)) {
               UBYTE *mix = mixBmp->Data + (mixBmp->Clip.Left * 4) + (mixBmp->Clip.Top * mixBmp->LineWidth);
               doMix<composite_out>(inBmp, mixBmp, dest, in, mix);
            }
            break;
         }

         case OP_ATOP: {
            objBitmap *mixBmp;
            if (!get_source_bitmap(Filter, &mixBmp, MixType, MixID, true)) {
               UBYTE *mix = mixBmp->Data + (mixBmp->Clip.Left * 4) + (mixBmp->Clip.Top * mixBmp->LineWidth);
               doMix<composite_atop>(inBmp, mixBmp, dest, in, mix);
               demultiply_bitmap(mixBmp);
            }
            break;
         }

         case OP_XOR: {
            objBitmap *mixBmp;
            if (!get_source_bitmap(Filter, &mixBmp, MixType, MixID, true)) {
               UBYTE *mix = mixBmp->Data + (mixBmp->Clip.Left * 4) + (mixBmp->Clip.Top * mixBmp->LineWidth);
               doMix<composite_xor>(inBmp, mixBmp, dest, in, mix);
               demultiply_bitmap(mixBmp);
            }
            break;
         }

         case OP_ARITHMETIC: {
            objBitmap *mixBmp;
            if (!get_source_bitmap(Filter, &mixBmp, MixType, MixID, true)) {
               UBYTE *mix  = mixBmp->Data + (mixBmp->Clip.Left * 4) + (mixBmp->Clip.Top * mixBmp->LineWidth);
               for (LONG y=0; y < height; y++) {
                  auto dp = dest;
                  auto sp = in;
                  auto mp = mix;
                  for (LONG x=0; x < width; x++) {
                     #define i1(c) sp[c]
                     #define i2(c) mp[c]
                     LONG dr = (K1 * i1(R) * i2(R)) + (K2 * i1(R)) + (K3 * i2(R)) + K4;
                     LONG dg = (K1 * i1(G) * i2(G)) + (K2 * i1(G)) + (K3 * i2(G)) + K4;
                     LONG db = (K1 * i1(B) * i2(B)) + (K2 * i1(B)) + (K3 * i2(B)) + K4;
                     LONG da = (K1 * i1(A) * i2(A)) + (K2 * i1(A)) + (K3 * i2(A)) + K4;
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
                     mp += 4;
                  }
                  dest += OutBitmap->LineWidth;
                  in   += inBmp->LineWidth;
                  mix  += mixBmp->LineWidth;
               }
               demultiply_bitmap(mixBmp);
            }
            break;
         }

         default: {
            objBitmap *mixBmp;
            if (!get_source_bitmap(Filter, &mixBmp, MixType, MixID, true)) {
               UBYTE *mix = mixBmp->Data + (mixBmp->Clip.Left * 4) + (mixBmp->Clip.Top * mixBmp->LineWidth);
               #pragma GCC diagnostic ignored "-Wswitch"
               switch(Operator) {
                  case OP_MULTIPLY:   doMix<blend_multiply>(inBmp, mixBmp, dest, in, mix); break;
                  case OP_SCREEN:     doMix<blend_screen>(inBmp, mixBmp, dest, in, mix); break;
                  case OP_DARKEN:     doMix<blend_darken>(inBmp, mixBmp, dest, in, mix); break;
                  case OP_LIGHTEN:    doMix<blend_lighten>(inBmp, mixBmp, dest, in, mix); break;
                  case OP_OVERLAY:    doMix<blend_overlay>(inBmp, mixBmp, dest, in, mix); break;
                  case OP_BURN:       doMix<blend_burn>(inBmp, mixBmp, dest, in, mix); break;
                  case OP_DODGE:      doMix<blend_dodge>(inBmp, mixBmp, dest, in, mix); break;
                  case OP_HARDLIGHT:  doMix<blend_hard_light>(inBmp, mixBmp, dest, in, mix); break;
                  case OP_SOFTLIGHT:  doMix<blend_soft_light>(inBmp, mixBmp, dest, in, mix); break;
                  case OP_DIFFERENCE: doMix<blend_difference>(inBmp, mixBmp, dest, in, mix); break;
                  case OP_EXCLUSION:  doMix<blend_exclusion>(inBmp, mixBmp, dest, in, mix); break;
                  case OP_PLUS:       doMix<blend_plus>(inBmp, mixBmp, dest, in, mix); break;
                  case OP_MINUS:      doMix<blend_minus>(inBmp, mixBmp, dest, in, mix); break;
                  case OP_CONTRAST:   doMix<blend_contrast>(inBmp, mixBmp, dest, in, mix); break;
                  case OP_INVERT:     doMix<blend_invert>(inBmp, mixBmp, dest, in, mix); break;
                  case OP_INVERTRGB:  doMix<blend_invert_rgb>(inBmp, mixBmp, dest, in, mix); break;
               }

               demultiply_bitmap(mixBmp);
            }

            break;
         }
      }

      demultiply_bitmap(inBmp);

   }

   virtual ~CompositeEffect() { }
};
