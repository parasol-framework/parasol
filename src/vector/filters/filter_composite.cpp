
enum Operation {
   OP_OVER=0, OP_IN, OP_OUT, OP_ATOP, OP_XOR, OP_ARITHMETIC, OP_NORMAL, OP_SCREEN, OP_MULTIPLY, OP_LIGHTEN, OP_DARKEN,
   OP_INVERTRGB, OP_INVERT, OP_CONTRAST, OP_DODGE, OP_BURN, OP_HARDLIGHT, OP_SOFTLIGHT, OP_DIFFERENCE, OP_EXCLUSION,
   OP_PLUS, OP_MINUS, OP_OVERLAY
};

//********************************************************************************************************************
// Porter/Duff Compositing routines
// For reference, this Wikipedia page explains it best: https://en.wikipedia.org/wiki/Alpha_compositing
//
// D = Dest; S = Source; M = Mix (equates to Dest as a pixel source)

struct composite_over {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if (!M[A]) ((ULONG *)D)[0] = ((ULONG *)S)[0];
      else if (!S[A]) ((ULONG *)D)[0] = ((ULONG *)M)[0];
      else {
         const ULONG dA = S[A] + M[A] - ((S[A] * M[A] + 0xff)>>8);
         const ULONG sA = S[A] + (S[A] >> 7); // 0..255 -> 0..256
         const ULONG cA = 256 - sA;
         const ULONG mA = M[A] + (M[A] >> 7); // 0..255 -> 0..256

         D[R] = ((S[R] * sA + ((M[R] * mA * cA)>>8))>>8) * 255 / dA;
         D[G] = ((S[G] * sA + ((M[G] * mA * cA)>>8))>>8) * 255 / dA;
         D[B] = ((S[B] * sA + ((M[B] * mA * cA)>>8))>>8) * 255 / dA;
         D[A] = dA;
      }
   }
};

struct composite_in {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if (M[A] IS 255) ((ULONG *)D)[0] = ((ULONG *)S)[0];
      else {
         D[R] = S[R];
         D[G] = S[G];
         D[B] = S[B];
         D[A] = (S[A] * M[A] + 0xff)>>8;
      }
   }
};

struct composite_out {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if (!M[A]) ((ULONG *)D)[0] = ((ULONG *)S)[0];
      else {
         D[R] = S[R];
         D[G] = S[G];
         D[B] = S[B];
         D[A] = (S[A] * (0xff - M[A]) + 0xff)>>8;
      }
   }
};

// Mix alpha has priority.  Source alpha is ignored except for blending.

struct composite_atop {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if (auto alpha = M[A]) {
         const UBYTE csalpha = 0xff - S[A];
         D[R] = (S[R] * alpha + M[R] * csalpha + 0xff) >> 8;
         D[G] = (S[G] * alpha + M[G] * csalpha + 0xff) >> 8;
         D[B] = (S[B] * alpha + M[B] * csalpha + 0xff) >> 8;
         D[A] = alpha;
      }
   }
};

struct composite_xor {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      const UBYTE s1a = 0xff - S[A];
      const UBYTE d1a = 0xff - M[A];
      D[R] = ((M[R] * s1a + S[R] * d1a + 0xff) >> 8);
      D[G] = ((M[G] * s1a + S[G] * d1a + 0xff) >> 8);
      D[B] = ((M[B] * s1a + S[B] * d1a + 0xff) >> 8);
      D[A] = (S[A] + M[A] - ((S[A] * M[A] + (0xff>>1)) >> (8 - 1)));
   }
};

//********************************************************************************************************************
// Blending algorithms, refer to https://en.wikipedia.org/wiki/Blend_modes

struct blend_screen {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      D[R] = (UBYTE)(S[R] + M[R] - ((S[R] * M[R] + 0Xff) >> 8));
      D[G] = (UBYTE)(S[G] + M[G] - ((S[G] * M[G] + 0Xff) >> 8));
      D[B] = (UBYTE)(S[B] + M[B] - ((S[B] * M[B] + 0Xff) >> 8));
      D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0Xff) >> 8));
   }
};

struct blend_multiply {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         const UBYTE s1a = 0xff - S[A];
         const UBYTE d1a = 0xff - M[A];
         D[R] = (UBYTE)((S[R] * M[R] + (S[R] * d1a) + (M[R] * s1a) + 0xff) >> 8);
         D[G] = (UBYTE)((S[G] * M[G] + (S[G] * d1a) + (M[G] * s1a) + 0xff) >> 8);
         D[B] = (UBYTE)((S[B] * M[B] + (S[B] * d1a) + (M[B] * s1a) + 0xff) >> 8);
         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_darken {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         UBYTE d1a = 0xff - D[A];
         UBYTE s1a = 0xff - S[A];
         UBYTE da  = D[A];

         D[R] = (UBYTE)((agg::sd_min(S[R] * da, M[R] * S[A]) + S[R] * d1a + M[R] * s1a + 0xff) >> 8);
         D[G] = (UBYTE)((agg::sd_min(S[G] * da, M[G] * S[A]) + S[G] * d1a + M[G] * s1a + 0xff) >> 8);
         D[B] = (UBYTE)((agg::sd_min(S[B] * da, M[B] * S[A]) + S[B] * d1a + M[B] * s1a + 0xff) >> 8);
         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_lighten {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         UBYTE d1a = 0xff - D[A];
         UBYTE s1a = 0xff - S[A];

         D[R] = (UBYTE)((agg::sd_max(S[R] * M[A], M[R] * S[A]) + S[R] * d1a + M[R] * s1a + 0xff) >> 8);
         D[G] = (UBYTE)((agg::sd_max(S[G] * M[A], M[G] * S[A]) + S[G] * d1a + M[G] * s1a + 0xff) >> 8);
         D[B] = (UBYTE)((agg::sd_max(S[B] * M[A], M[B] * S[A]) + S[B] * d1a + M[B] * s1a + 0xff) >> 8);
         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_dodge {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         LONG d1a  = 0xff - M[A];
         LONG s1a  = 0xff - S[A];
         LONG drsa = M[G] * S[A];
         LONG dgsa = M[B] * S[A];
         LONG dbsa = M[B] * S[A];
         LONG srda = S[R] * M[A];
         LONG sgda = S[G] * M[A];
         LONG sbda = S[B] * M[A];
         LONG sada = S[A] * M[A];

         D[R] = (UBYTE)((srda + drsa >= sada) ?
             (sada + S[R] * d1a + M[R] * s1a + 0xff) >> 8 :
             drsa / (0xff - (S[R] << 8) / S[A]) + ((S[R] * d1a + M[R] * s1a + 0xff) >> 8));

         D[G] = (UBYTE)((sgda + dgsa >= sada) ?
             (sada + S[G] * d1a + M[G] * s1a + 0xff) >> 8 :
             dgsa / (0xff - (S[G] << 8) / S[A]) + ((S[G] * d1a + M[G] * s1a + 0xff) >> 8));

         D[B] = (UBYTE)((sbda + dbsa >= sada) ?
             (sada + S[B] * d1a + M[B] * s1a + 0xff) >> 8 :
             dbsa / (0xff - (S[B] << 8) / S[A]) + ((S[B] * d1a + M[B] * s1a + 0xff) >> 8));

         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_contrast {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      LONG d2a = M[A] >> 1;
      UBYTE s2a = S[A] >> 1;

      int r = (int)((((M[R] - d2a) * int((S[R] - s2a)*2 + 0xff)) >> 8) + d2a);
      int g = (int)((((M[G] - d2a) * int((S[G] - s2a)*2 + 0xff)) >> 8) + d2a);
      int b = (int)((((M[B] - d2a) * int((S[B] - s2a)*2 + 0xff)) >> 8) + d2a);

      r = (r < 0) ? 0 : r;
      g = (g < 0) ? 0 : g;
      b = (b < 0) ? 0 : b;

      D[R] = (UBYTE)((r > M[A]) ? M[A] : r);
      D[G] = (UBYTE)((g > M[A]) ? M[A] : g);
      D[B] = (UBYTE)((b > M[A]) ? M[A] : b);
   }
};

struct blend_overlay {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         UBYTE d1a = 0xff - M[A];
         UBYTE s1a = 0xff - S[A];
         UBYTE sada = S[A] * M[A];

         D[R] = (UBYTE)(((2*M[R] < M[A]) ?
             2*S[R]*M[R] + S[R]*d1a + M[R]*s1a :
             sada - 2*(M[A] - M[R])*(S[A] - S[R]) + S[R]*d1a + M[R]*s1a + 0xff) >> 8);

         D[G] = (UBYTE)(((2*M[G] < M[A]) ?
             2*S[G]*M[G] + S[G]*d1a + M[G]*s1a :
             sada - 2*(M[A] - M[G])*(S[A] - S[G]) + S[G]*d1a + M[G]*s1a + 0xff) >> 8);

         D[B] = (UBYTE)(((2*M[B] < M[A]) ?
             2*S[B]*M[B] + S[B]*d1a + M[B]*s1a :
             sada - 2*(M[A] - M[B])*(S[A] - S[B]) + S[B]*d1a + M[B]*s1a + 0xff) >> 8);

         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_burn {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         const UBYTE d1a = 0xff - D[A];
         const UBYTE s1a = 0xff - S[A];
         const LONG drsa = M[R] * S[A];
         const LONG dgsa = M[G] * S[A];
         const LONG dbsa = M[B] * S[A];
         const LONG srda = S[R] * M[A];
         const LONG sgda = S[G] * M[A];
         const LONG sbda = S[B] * M[A];
         const LONG sada = S[A] * M[A];

         D[R] = (UBYTE)(((srda + drsa <= sada) ?
             S[R] * d1a + M[R] * s1a :
             S[A] * (srda + drsa - sada) / S[R] + S[R] * d1a + M[R] * s1a + 0xff) >> 8);

         D[G] = (UBYTE)(((sgda + dgsa <= sada) ?
             S[G] * d1a + M[G] * s1a :
             S[A] * (sgda + dgsa - sada) / S[G] + S[G] * d1a + M[G] * s1a + 0xff) >> 8);

         D[B] = (UBYTE)(((sbda + dbsa <= sada) ?
             S[B] * d1a + M[B] * s1a :
             S[A] * (sbda + dbsa - sada) / S[B] + S[B] * d1a + M[B] * s1a + 0xff) >> 8);

         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_hard_light {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         UBYTE d1a  = 0xff - D[A];
         UBYTE s1a  = 0xff - S[A];
         UBYTE sada = S[A] * M[A];

         D[R] = (UBYTE)(((2*S[R] < S[A]) ?
             2*S[R]*M[R] + S[R]*d1a + M[R]*s1a :
             sada - 2*(M[A] - M[R])*(S[A] - S[R]) + S[R]*d1a + M[R]*s1a + 0xff) >> 8);

         D[G] = (UBYTE)(((2*S[G] < S[A]) ?
             2*S[G]*M[G] + S[G]*d1a + M[G]*s1a :
             sada - 2*(M[A] - M[G])*(S[A] - S[G]) + S[G]*d1a + M[G]*s1a + 0xff) >> 8);

         D[B] = (UBYTE)(((2*S[B] < S[A]) ?
             2*S[B]*M[B] + S[B]*d1a + M[B]*s1a :
             sada - 2*(M[A] - M[B])*(S[A] - S[B]) + S[B]*d1a + M[B]*s1a + 0xff) >> 8);

         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_soft_light {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         double xr = double(D[R]) / 0xff;
         double xg = double(D[G]) / 0xff;
         double xb = double(D[B]) / 0xff;
         double da = double(D[A] ? D[A] : 1) / 0xff;

         if(2*S[R] < S[A])   xr = xr*(S[A] + (1 - xr/da)*(2*S[R] - S[A])) + S[R]*(1 - da) + xr*(1 - S[A]);
         else if(8*xr <= da) xr = xr*(S[A] + (1 - xr/da)*(2*S[R] - S[A])*(3 - 8*xr/da)) + S[R]*(1 - da) + xr*(1 - S[A]);
         else                xr = (xr*S[A] + (sqrt(xr/da)*da - xr)*(2*S[R] - S[A])) + S[R]*(1 - da) + xr*(1 - S[A]);

         if(2*S[G] < S[A])   xg = xg*(S[A] + (1 - xg/da)*(2*S[G] - S[A])) + S[G]*(1 - da) + xg*(1 - S[A]);
         else if(8*xg <= da) xg = xg*(S[A] + (1 - xg/da)*(2*S[G] - S[A])*(3 - 8*xg/da)) + S[G]*(1 - da) + xg*(1 - S[A]);
         else                xg = (xg*S[A] + (sqrt(xg/da)*da - xg)*(2*S[G] - S[A])) + S[G]*(1 - da) + xg*(1 - S[A]);

         if(2*S[B] < S[A])   xb = xb*(S[A] + (1 - xb/da)*(2*S[B] - S[A])) + S[B]*(1 - da) + xb*(1 - S[A]);
         else if(8*xb <= da) xb = xb*(S[A] + (1 - xb/da)*(2*S[B] - S[A])*(3 - 8*xb/da)) + S[B]*(1 - da) + xb*(1 - S[A]);
         else                xb = (xb*S[A] + (sqrt(xb/da)*da - xb)*(2*S[B] - S[A])) + S[B]*(1 - da) + xb*(1 - S[A]);

         D[R] = (UBYTE)agg::uround(xr * 0xff);
         D[G] = (UBYTE)agg::uround(xg * 0xff);
         D[B] = (UBYTE)agg::uround(xb * 0xff);
         D[A] = (UBYTE)(S[A] + D[A] - ((S[A] * D[A] + 0xff) >> 8));
      }
   }
};

struct blend_difference {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         D[R] = (UBYTE)(S[R] + M[R] - ((2 * agg::sd_min(S[R]*M[A], M[R]*S[A]) + 0xff) >> 8));
         D[G] = (UBYTE)(S[G] + M[G] - ((2 * agg::sd_min(S[G]*M[A], M[G]*S[A]) + 0xff) >> 8));
         D[B] = (UBYTE)(S[B] + M[B] - ((2 * agg::sd_min(S[B]*M[A], M[B]*S[A]) + 0xff) >> 8));
         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_exclusion {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         const UBYTE d1a = 0xff - D[A];
         const UBYTE s1a = 0xff - S[A];
         D[R] = (UBYTE)((S[R]*M[A] + M[R]*S[A] - 2*S[R]*M[R] + S[R]*d1a + M[R]*s1a + 0xff) >> 8);
         D[G] = (UBYTE)((S[G]*M[A] + M[G]*S[A] - 2*S[G]*M[G] + S[G]*d1a + M[G]*s1a + 0xff) >> 8);
         D[B] = (UBYTE)((S[B]*M[A] + M[B]*S[A] - 2*S[B]*M[B] + S[B]*d1a + M[B]*s1a + 0xff) >> 8);
         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_plus {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         const UBYTE xr = D[R] + S[R];
         const UBYTE xg = D[G] + S[G];
         const UBYTE xb = D[B] + S[B];
         const UBYTE xa = D[A] + S[A];
         D[R] = (xr > 0xff) ? (UBYTE)0xff : xr;
         D[G] = (xg > 0xff) ? (UBYTE)0xff : xg;
         D[B] = (xb > 0xff) ? (UBYTE)0xff : xb;
         D[A] = (xa > 0xff) ? (UBYTE)0xff : xa;
      }
   }
};

struct blend_minus {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         const UBYTE xr = D[R] - S[R];
         const UBYTE xg = D[G] - S[G];
         const UBYTE xb = D[B] - S[B];
         D[R] = (xr > 0xff) ? 0 : xr;
         D[G] = (xg > 0xff) ? 0 : xg;
         D[B] = (xb > 0xff) ? 0 : xb;
         D[A] = (UBYTE)(S[A] + D[A] - ((S[A] * D[A] + 0xff) >> 8));
         //D[A] = (UBYTE)(0xff - (((0xff - S[A]) * (0xff - D[A]) + 0xff) >> 8));
      }
   }
};

struct blend_invert {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         const UBYTE xr = ((M[A] - D[R]) * S[A] + 0xff) >> 8;
         const UBYTE xg = ((M[A] - D[G]) * S[A] + 0xff) >> 8;
         const UBYTE xb = ((M[A] - D[B]) * S[A] + 0xff) >> 8;
         const UBYTE s1a = 0xff - S[A];
         D[R] = (UBYTE)(xr + ((D[R] * s1a + 0xff) >> 8));
         D[G] = (UBYTE)(xg + ((D[G] * s1a + 0xff) >> 8));
         D[B] = (UBYTE)(xb + ((D[B] * s1a + 0xff) >> 8));
         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_invert_rgb {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if (S[A]) {
         UBYTE xr = ((M[A] - D[R]) * S[R] + 0xff) >> 8;
         UBYTE xg = ((M[A] - D[G]) * S[G] + 0xff) >> 8;
         UBYTE xb = ((M[A] - D[B]) * S[B] + 0xff) >> 8;
         UBYTE s1a = 0xff - S[A];
         D[R] = (UBYTE)(xr + ((D[R] * s1a + 0xff) >> 8));
         D[G] = (UBYTE)(xg + ((D[G] * s1a + 0xff) >> 8));
         D[B] = (UBYTE)(xb + ((D[B] * s1a + 0xff) >> 8));
         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
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
      K1 = K2 = K3 = K4 = 0;

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

            case SVF_K1: read_numseq(val, &K1, TAGEND); break;

            case SVF_K2: read_numseq(val, &K2, TAGEND); break;

            case SVF_K3: read_numseq(val, &K3, TAGEND); break;

            case SVF_K4: read_numseq(val, &K4, TAGEND); break;

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

      UBYTE *dest = OutBitmap->Data + (OutBitmap->Clip.Left * 4) + (OutBitmap->Clip.Top * OutBitmap->LineWidth);

      switch (Operator) {
         case OP_NORMAL:
         case OP_OVER: {
            if (!get_source_bitmap(Filter, &inBmp, SourceType, InputID, false)) {
               objBitmap *mixBmp;
               if (!get_source_bitmap(Filter, &mixBmp, MixType, MixID, false)) {
                  UBYTE *in  = inBmp->Data + (inBmp->Clip.Left * 4) + (inBmp->Clip.Top * inBmp->LineWidth);
                  UBYTE *mix = mixBmp->Data + (mixBmp->Clip.Left * 4) + (mixBmp->Clip.Top * mixBmp->LineWidth);
                  doMix<composite_over>(inBmp, mixBmp, dest, in, mix);
               }
            }
            break;
         }

         case OP_IN: {
            if (!get_source_bitmap(Filter, &inBmp, SourceType, InputID, false)) {
               objBitmap *mixBmp;
               if (!get_source_bitmap(Filter, &mixBmp, MixType, MixID, false)) {
                  UBYTE *in  = inBmp->Data + (inBmp->Clip.Left * 4) + (inBmp->Clip.Top * inBmp->LineWidth);
                  UBYTE *mix = mixBmp->Data + (mixBmp->Clip.Left * 4) + (mixBmp->Clip.Top * mixBmp->LineWidth);
                  doMix<composite_in>(inBmp, mixBmp, dest, in, mix);
               }
            }
            break;
         }

         case OP_OUT: {
            if (!get_source_bitmap(Filter, &inBmp, SourceType, InputID, false)) {
               objBitmap *mixBmp;
               if (!get_source_bitmap(Filter, &mixBmp, MixType, MixID, false)) {
                  UBYTE *in  = inBmp->Data + (inBmp->Clip.Left * 4) + (inBmp->Clip.Top * inBmp->LineWidth);
                  UBYTE *mix = mixBmp->Data + (mixBmp->Clip.Left * 4) + (mixBmp->Clip.Top * mixBmp->LineWidth);
                  doMix<composite_out>(inBmp, mixBmp, dest, in, mix);
               }
            }
            break;
         }

         case OP_ATOP: {
            if (!get_source_bitmap(Filter, &inBmp, SourceType, InputID, false)) {
               objBitmap *mixBmp;
               if (!get_source_bitmap(Filter, &mixBmp, MixType, MixID, false)) {
                  UBYTE *in  = inBmp->Data + (inBmp->Clip.Left * 4) + (inBmp->Clip.Top * inBmp->LineWidth);
                  UBYTE *mix = mixBmp->Data + (mixBmp->Clip.Left * 4) + (mixBmp->Clip.Top * mixBmp->LineWidth);
                  doMix<composite_atop>(inBmp, mixBmp, dest, in, mix);
               }
            }
            break;
         }

         case OP_XOR: {
            if (!get_source_bitmap(Filter, &inBmp, SourceType, InputID, false)) {
               objBitmap *mixBmp;
               if (!get_source_bitmap(Filter, &mixBmp, MixType, MixID, false)) {
                  UBYTE *in  = inBmp->Data + (inBmp->Clip.Left * 4) + (inBmp->Clip.Top * inBmp->LineWidth);
                  UBYTE *mix = mixBmp->Data + (mixBmp->Clip.Left * 4) + (mixBmp->Clip.Top * mixBmp->LineWidth);
                  doMix<composite_xor>(inBmp, mixBmp, dest, in, mix);
               }
            }
            break;
         }

         case OP_ARITHMETIC: {
            if (!get_source_bitmap(Filter, &inBmp, SourceType, InputID, false)) {
               objBitmap *mixBmp;
               LONG height = OutBitmap->Clip.Bottom - OutBitmap->Clip.Top;
               LONG width  = OutBitmap->Clip.Right - OutBitmap->Clip.Left;
               if (inBmp->Clip.Right - inBmp->Clip.Left < width) width = inBmp->Clip.Right - inBmp->Clip.Left;
               if (inBmp->Clip.Bottom - inBmp->Clip.Top < height) height = inBmp->Clip.Bottom - inBmp->Clip.Top;

               const UBYTE A = OutBitmap->ColourFormat->AlphaPos>>3;
               const UBYTE R = OutBitmap->ColourFormat->RedPos>>3;
               const UBYTE G = OutBitmap->ColourFormat->GreenPos>>3;
               const UBYTE B = OutBitmap->ColourFormat->BluePos>>3;

               if (!get_source_bitmap(Filter, &mixBmp, MixType, MixID, false)) {
                  UBYTE *in  = inBmp->Data + (inBmp->Clip.Left * 4) + (inBmp->Clip.Top * inBmp->LineWidth);
                  UBYTE *mix = mixBmp->Data + (mixBmp->Clip.Left * 4) + (mixBmp->Clip.Top * mixBmp->LineWidth);
                  for (LONG y=0; y < height; y++) {
                     auto dp = dest;
                     auto sp = in;
                     auto mp = mix;
                     for (LONG x=0; x < width; x++) {
                        if ((mp[A]) or (sp[A])) {
                           // Scale RGB to 0 - 1.0 and premultiply the values.
                           #define SCALE (1.0 / 255.0)
                           #define DESCALE 255.0
                           const DOUBLE sA = DOUBLE(sp[A]) * SCALE;
                           const DOUBLE sR = DOUBLE(sp[R]) * SCALE * sA;
                           const DOUBLE sG = DOUBLE(sp[G]) * SCALE * sA;
                           const DOUBLE sB = DOUBLE(sp[B]) * SCALE * sA;

                           const DOUBLE mA = DOUBLE(mp[A]) * SCALE;
                           const DOUBLE mR = DOUBLE(mp[R]) * SCALE * mA;
                           const DOUBLE mG = DOUBLE(mp[G]) * SCALE * mA;
                           const DOUBLE mB = DOUBLE(mp[B]) * SCALE * mA;

                           DOUBLE dA = (K1 * sA * mA) + (K2 * sA) + (K3 * mA) + K4;

                           if (dA > 0.0) {
                              if (dA > 1.0) dA = 1.0;

                              DOUBLE demul = 1.0 / dA;
                              LONG dr = F2T(((K1 * sR * mR) + (K2 * sR) + (K3 * mR) + K4) * demul * DESCALE);
                              LONG dg = F2T(((K1 * sG * mG) + (K2 * sG) + (K3 * mG) + K4) * demul * DESCALE);
                              LONG db = F2T(((K1 * sB * mB) + (K2 * sB) + (K3 * mB) + K4) * demul * DESCALE);

                              if (dr > 0xff) dp[R] = 0xff;
                              else if (dr < 0) dp[R] = 0;
                              else dp[R] = dr;

                              if (dg > 0xff) dp[G] = 0xff;
                              else if (dg < 0) dp[G] = 0;
                              else dp[G] = dg;

                              if (db > 0xff) dp[B] = 0xff;
                              else if (db < 0) dp[B] = 0;
                              else dp[B] = db;

                              dp[A] = F2T(dA * DESCALE);
                           }
                        }

                        dp += 4;
                        sp += 4;
                        mp += 4;
                     }
                     dest += OutBitmap->LineWidth;
                     in   += inBmp->LineWidth;
                     mix  += mixBmp->LineWidth;
                  }
               }
            }
            break;
         }

         default: { // These mix routines use pre-multiplied content.
            if (!get_source_bitmap(Filter, &inBmp, SourceType, InputID, true)) {
               objBitmap *mixBmp;
               if (!get_source_bitmap(Filter, &mixBmp, MixType, MixID, true)) {
                  UBYTE *in  = inBmp->Data + (inBmp->Clip.Left * 4) + (inBmp->Clip.Top * inBmp->LineWidth);
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

                   bmpDemultiply(mixBmp);
               }
               bmpDemultiply(inBmp);
            }

            break;
         }
      }
   }

   virtual ~CompositeEffect() { }
};
