/*********************************************************************************************************************

-CLASS-
CompositeFX: Composite two sources together with a mixing algorithm.

This filter combines the Input and Mix sources using either one of the Porter-Duff compositing operations, or a colour
blending algorithm.  The Input has priority and will be placed in the foreground for ordered operations such as Atop
and Over.

-END-

*********************************************************************************************************************/

class extCompositeFX : public extFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = ID_COMPOSITEFX;
   static constexpr CSTRING CLASS_NAME = "CompositeFX";
   using create = pf::Create<extCompositeFX>;

   DOUBLE K1, K2, K3, K4; // For the arithmetic operator
   OP Operator; // OP constant

   template <class CompositeOp>
   void doMix(objBitmap *InBitmap, objBitmap *MixBitmap, UBYTE *Dest, UBYTE *In, UBYTE *Mix) {
      const UBYTE A = Target->ColourFormat->AlphaPos>>3;
      const UBYTE R = Target->ColourFormat->RedPos>>3;
      const UBYTE G = Target->ColourFormat->GreenPos>>3;
      const UBYTE B = Target->ColourFormat->BluePos>>3;

      LONG height = Target->Clip.Bottom - Target->Clip.Top;
      LONG width  = Target->Clip.Right - Target->Clip.Left;
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
         Dest += Target->LineWidth;
         In   += InBitmap->LineWidth;
         Mix  += MixBitmap->LineWidth;
      }
   }

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

         D[R] = glLinearRGB.invert(((glLinearRGB.convert(S[R]) * sA + ((glLinearRGB.convert(M[R]) * mA * cA)>>8))>>8) * 255 / dA);
         D[G] = glLinearRGB.invert(((glLinearRGB.convert(S[G]) * sA + ((glLinearRGB.convert(M[G]) * mA * cA)>>8))>>8) * 255 / dA);
         D[B] = glLinearRGB.invert(((glLinearRGB.convert(S[B]) * sA + ((glLinearRGB.convert(M[B]) * mA * cA)>>8))>>8) * 255 / dA);
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

// S is on top and blended with M as a background.  S is obscured if M is not present.  Mix alpha has priority in the
// output.  S alpha is ignored except for blending with M.

struct composite_atop {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if (auto m_alpha = M[A]) {
         auto sR = glLinearRGB.convert(S[R]);
         auto sG = glLinearRGB.convert(S[G]);
         auto sB = glLinearRGB.convert(S[B]);

         auto mR = glLinearRGB.convert(M[R]);
         auto mG = glLinearRGB.convert(M[G]);
         auto mB = glLinearRGB.convert(M[B]);

         const UBYTE sA  = S[A];
         const UBYTE scA = 0xff - sA;

         D[R] = glLinearRGB.invert(((sR * sA) + (mR * scA) + 0xff)>>8);
         D[G] = glLinearRGB.invert(((sG * sA) + (mG * scA) + 0xff)>>8);
         D[B] = glLinearRGB.invert(((sB * sA) + (mB * scA) + 0xff)>>8);
         D[A] = m_alpha;
      }
   }
};

struct composite_xor {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      auto sR = glLinearRGB.convert(S[R]);
      auto sG = glLinearRGB.convert(S[G]);
      auto sB = glLinearRGB.convert(S[B]);

      auto mR = glLinearRGB.convert(M[R]);
      auto mG = glLinearRGB.convert(M[G]);
      auto mB = glLinearRGB.convert(M[B]);

      const UBYTE s1a = 0xff - S[A];
      const UBYTE d1a = 0xff - M[A];
      D[R] = glLinearRGB.invert(((mR * s1a) + (sR * d1a) + 0xff) >> 8);
      D[G] = glLinearRGB.invert(((mG * s1a) + (sG * d1a) + 0xff) >> 8);
      D[B] = glLinearRGB.invert(((mB * s1a) + (sB * d1a) + 0xff) >> 8);
      D[A] = (S[A] + M[A] - ((S[A] * M[A] + (0xff>>1)) >> (8 - 1)));
   }
};

//********************************************************************************************************************
// Blending algorithms, refer to https://en.wikipedia.org/wiki/Blend_modes

struct blend_screen {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      auto sR = glLinearRGB.convert(S[R]);
      auto sG = glLinearRGB.convert(S[G]);
      auto sB = glLinearRGB.convert(S[B]);

      auto mR = glLinearRGB.convert(M[R]);
      auto mG = glLinearRGB.convert(M[G]);
      auto mB = glLinearRGB.convert(M[B]);

      D[R] = glLinearRGB.invert(sR + mR - ((sR * mR + 0Xff) >> 8));
      D[G] = glLinearRGB.invert(sG + mG - ((sG * mG + 0Xff) >> 8));
      D[B] = glLinearRGB.invert(sB + mB - ((sB * mB + 0Xff) >> 8));
      D[A] = UBYTE(S[A] + M[A] - ((S[A] * M[A] + 0Xff) >> 8));
   }
};

struct blend_multiply {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         auto sR = glLinearRGB.convert(S[R]);
         auto sG = glLinearRGB.convert(S[G]);
         auto sB = glLinearRGB.convert(S[B]);

         auto mR = glLinearRGB.convert(M[R]);
         auto mG = glLinearRGB.convert(M[G]);
         auto mB = glLinearRGB.convert(M[B]);

         const UBYTE s1a = 0xff - S[A];
         const UBYTE d1a = 0xff - M[A];
         D[R] = glLinearRGB.invert((sR * mR + (sR * d1a) + (mR * s1a) + 0xff) >> 8);
         D[G] = glLinearRGB.invert((sG * mG + (sG * d1a) + (mG * s1a) + 0xff) >> 8);
         D[B] = glLinearRGB.invert((sB * mB + (sB * d1a) + (mB * s1a) + 0xff) >> 8);
         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_darken {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         auto sR = glLinearRGB.convert(S[R]);
         auto sG = glLinearRGB.convert(S[G]);
         auto sB = glLinearRGB.convert(S[B]);

         auto mR = glLinearRGB.convert(M[R]);
         auto mG = glLinearRGB.convert(M[G]);
         auto mB = glLinearRGB.convert(M[B]);

         UBYTE d1a = 0xff - D[A];
         UBYTE s1a = 0xff - S[A];
         UBYTE da  = D[A];

         D[R] = glLinearRGB.invert((agg::sd_min(sR * da, mR * S[A]) + sR * d1a + mR * s1a + 0xff) >> 8);
         D[G] = glLinearRGB.invert((agg::sd_min(sG * da, mG * S[A]) + sG * d1a + mG * s1a + 0xff) >> 8);
         D[B] = glLinearRGB.invert((agg::sd_min(sB * da, mB * S[A]) + sB * d1a + mB * s1a + 0xff) >> 8);
         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_lighten {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         auto sR = glLinearRGB.convert(S[R]);
         auto sG = glLinearRGB.convert(S[G]);
         auto sB = glLinearRGB.convert(S[B]);

         auto mR = glLinearRGB.convert(M[R]);
         auto mG = glLinearRGB.convert(M[G]);
         auto mB = glLinearRGB.convert(M[B]);

         UBYTE d1a = 0xff - D[A];
         UBYTE s1a = 0xff - S[A];

         D[R] = glLinearRGB.invert((agg::sd_max(sR * M[A], mR * S[A]) + sR * d1a + mR * s1a + 0xff) >> 8);
         D[G] = glLinearRGB.invert((agg::sd_max(sG * M[A], mG * S[A]) + sG * d1a + mG * s1a + 0xff) >> 8);
         D[B] = glLinearRGB.invert((agg::sd_max(sB * M[A], mB * S[A]) + sB * d1a + mB * s1a + 0xff) >> 8);
         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_dodge {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         auto sR = glLinearRGB.convert(S[R]);
         auto sG = glLinearRGB.convert(S[G]);
         auto sB = glLinearRGB.convert(S[B]);

         auto mR = glLinearRGB.convert(M[R]);
         auto mG = glLinearRGB.convert(M[G]);
         auto mB = glLinearRGB.convert(M[B]);

         LONG d1a  = 0xff - M[A];
         LONG s1a  = 0xff - S[A];
         LONG drsa = mG * S[A];
         LONG dgsa = mB * S[A];
         LONG dbsa = mB * S[A];
         LONG srda = sR * M[A];
         LONG sgda = sG * M[A];
         LONG sbda = sB * M[A];
         LONG sada = S[A] * M[A];

         D[R] = glLinearRGB.invert((srda + drsa >= sada) ?
             (sada + sR * d1a + mR * s1a + 0xff) >> 8 :
             drsa / (0xff - (sR << 8) / S[A]) + ((sR * d1a + mR * s1a + 0xff) >> 8));

         D[G] = glLinearRGB.invert((sgda + dgsa >= sada) ?
             (sada + sG * d1a + mG * s1a + 0xff) >> 8 :
             dgsa / (0xff - (sG << 8) / S[A]) + ((sG * d1a + mG * s1a + 0xff) >> 8));

         D[B] = glLinearRGB.invert((sbda + dbsa >= sada) ?
             (sada + sB * d1a + mB * s1a + 0xff) >> 8 :
             dbsa / (0xff - (sB << 8) / S[A]) + ((sB * d1a + mB * s1a + 0xff) >> 8));

         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_contrast {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      auto sR = glLinearRGB.convert(S[R]);
      auto sG = glLinearRGB.convert(S[G]);
      auto sB = glLinearRGB.convert(S[B]);

      auto mR = glLinearRGB.convert(M[R]);
      auto mG = glLinearRGB.convert(M[G]);
      auto mB = glLinearRGB.convert(M[B]);

      LONG d2a = M[A] >> 1;
      UBYTE s2a = S[A] >> 1;

      auto r = LONG((((mR - d2a) * int((sR - s2a)*2 + 0xff)) >> 8) + d2a);
      auto g = LONG((((mG - d2a) * int((sG - s2a)*2 + 0xff)) >> 8) + d2a);
      auto b = LONG((((mB - d2a) * int((sB - s2a)*2 + 0xff)) >> 8) + d2a);

      r = (r < 0) ? 0 : r;
      g = (g < 0) ? 0 : g;
      b = (b < 0) ? 0 : b;

      D[R] = glLinearRGB.invert((r > M[A]) ? M[A] : r);
      D[G] = glLinearRGB.invert((g > M[A]) ? M[A] : g);
      D[B] = glLinearRGB.invert((b > M[A]) ? M[A] : b);
   }
};

struct blend_overlay {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         auto sR = glLinearRGB.convert(S[R]);
         auto sG = glLinearRGB.convert(S[G]);
         auto sB = glLinearRGB.convert(S[B]);

         auto mR = glLinearRGB.convert(M[R]);
         auto mG = glLinearRGB.convert(M[G]);
         auto mB = glLinearRGB.convert(M[B]);

         UBYTE d1a = 0xff - M[A];
         UBYTE s1a = 0xff - S[A];
         UBYTE sada = S[A] * M[A];

         D[R] = glLinearRGB.invert(((2*mR < M[A]) ?
             2*sR*mR + sR*d1a + mR*s1a :
             sada - 2*(M[A] - mR)*(S[A] - sR) + sR*d1a + mR*s1a + 0xff) >> 8);

         D[G] = glLinearRGB.invert(((2*mG < M[A]) ?
             2*sG*mG + sG*d1a + mG*s1a :
             sada - 2*(M[A] - mG)*(S[A] - sG) + sG*d1a + mG*s1a + 0xff) >> 8);

         D[B] = glLinearRGB.invert(((2*mB < M[A]) ?
             2*sB*mB + sB*d1a + mB*s1a :
             sada - 2*(M[A] - mB)*(S[A] - sB) + sB*d1a + mB*s1a + 0xff) >> 8);

         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_burn {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         auto sR = glLinearRGB.convert(S[R]);
         auto sG = glLinearRGB.convert(S[G]);
         auto sB = glLinearRGB.convert(S[B]);

         auto mR = glLinearRGB.convert(M[R]);
         auto mG = glLinearRGB.convert(M[G]);
         auto mB = glLinearRGB.convert(M[B]);

         const UBYTE d1a = 0xff - D[A];
         const UBYTE s1a = 0xff - S[A];
         const LONG drsa = mR * S[A];
         const LONG dgsa = mG * S[A];
         const LONG dbsa = mB * S[A];
         const LONG srda = sR * M[A];
         const LONG sgda = sG * M[A];
         const LONG sbda = sB * M[A];
         const LONG sada = S[A] * M[A];

         D[R] = glLinearRGB.invert(((srda + drsa <= sada) ?
             sR * d1a + mR * s1a :
             S[A] * (srda + drsa - sada) / sR + sR * d1a + mR * s1a + 0xff) >> 8);

         D[G] = glLinearRGB.invert(((sgda + dgsa <= sada) ?
             sG * d1a + mG * s1a :
             S[A] * (sgda + dgsa - sada) / sG + sG * d1a + mG * s1a + 0xff) >> 8);

         D[B] = glLinearRGB.invert(((sbda + dbsa <= sada) ?
             sB * d1a + mB * s1a :
             S[A] * (sbda + dbsa - sada) / sB + sB * d1a + mB * s1a + 0xff) >> 8);

         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_hard_light {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         auto sR = glLinearRGB.convert(S[R]);
         auto sG = glLinearRGB.convert(S[G]);
         auto sB = glLinearRGB.convert(S[B]);

         auto mR = glLinearRGB.convert(M[R]);
         auto mG = glLinearRGB.convert(M[G]);
         auto mB = glLinearRGB.convert(M[B]);

         UBYTE d1a  = 0xff - D[A];
         UBYTE s1a  = 0xff - S[A];
         UBYTE sada = S[A] * M[A];

         D[R] = glLinearRGB.invert(((2*sR < S[A]) ?
             2*sR*mR + sR*d1a + mR*s1a :
             sada - 2*(M[A] - mR)*(S[A] - sR) + sR*d1a + mR*s1a + 0xff) >> 8);

         D[G] = glLinearRGB.invert(((2*sG < S[A]) ?
             2*sG*mG + sG*d1a + mG*s1a :
             sada - 2*(M[A] - mG)*(S[A] - sG) + sG*d1a + mG*s1a + 0xff) >> 8);

         D[B] = glLinearRGB.invert(((2*sB < S[A]) ?
             2*sB*mB + sB*d1a + mB*s1a :
             sada - 2*(M[A] - mB)*(S[A] - sB) + sB*d1a + mB*s1a + 0xff) >> 8);

         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_soft_light {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         auto sR = glLinearRGB.convert(S[R]);
         auto sG = glLinearRGB.convert(S[G]);
         auto sB = glLinearRGB.convert(S[B]);

         double xr = double(glLinearRGB.convert(D[R])) / 0xff;
         double xg = double(glLinearRGB.convert(D[G])) / 0xff;
         double xb = double(glLinearRGB.convert(D[B])) / 0xff;
         double da = double(D[A] ? D[A] : 1) / 0xff;

         if(2*sR < S[A])     xr = xr*(S[A] + (1 - xr/da)*(2*sR - S[A])) + sR*(1 - da) + xr*(1 - S[A]);
         else if(8*xr <= da) xr = xr*(S[A] + (1 - xr/da)*(2*sR - S[A])*(3 - 8*xr/da)) + sR*(1 - da) + xr*(1 - S[A]);
         else                xr = (xr*S[A] + (sqrt(xr/da)*da - xr)*(2*sR - S[A])) + sR*(1 - da) + xr*(1 - S[A]);

         if(2*sG < S[A])     xg = xg*(S[A] + (1 - xg/da)*(2*sG - S[A])) + sG*(1 - da) + xg*(1 - S[A]);
         else if(8*xg <= da) xg = xg*(S[A] + (1 - xg/da)*(2*sG - S[A])*(3 - 8*xg/da)) + sG*(1 - da) + xg*(1 - S[A]);
         else                xg = (xg*S[A] + (sqrt(xg/da)*da - xg)*(2*sG - S[A])) + sG*(1 - da) + xg*(1 - S[A]);

         if(2*sB < S[A])     xb = xb*(S[A] + (1 - xb/da)*(2*sB - S[A])) + sB*(1 - da) + xb*(1 - S[A]);
         else if(8*xb <= da) xb = xb*(S[A] + (1 - xb/da)*(2*sB - S[A])*(3 - 8*xb/da)) + sB*(1 - da) + xb*(1 - S[A]);
         else                xb = (xb*S[A] + (sqrt(xb/da)*da - xb)*(2*sB - S[A])) + sB*(1 - da) + xb*(1 - S[A]);

         D[R] = glLinearRGB.invert(agg::uround(xr * 0xff));
         D[G] = glLinearRGB.invert(agg::uround(xg * 0xff));
         D[B] = glLinearRGB.invert(agg::uround(xb * 0xff));
         D[A] = (UBYTE)(S[A] + D[A] - ((S[A] * D[A] + 0xff) >> 8));
      }
   }
};

struct blend_difference {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         auto sR = glLinearRGB.convert(S[R]);
         auto sG = glLinearRGB.convert(S[G]);
         auto sB = glLinearRGB.convert(S[B]);

         auto mR = glLinearRGB.convert(M[R]);
         auto mG = glLinearRGB.convert(M[G]);
         auto mB = glLinearRGB.convert(M[B]);

         D[R] = glLinearRGB.invert(sR + mR - ((2 * agg::sd_min(sR*M[A], mR*S[A]) + 0xff) >> 8));
         D[G] = glLinearRGB.invert(sG + mG - ((2 * agg::sd_min(sG*M[A], mG*S[A]) + 0xff) >> 8));
         D[B] = glLinearRGB.invert(sB + mB - ((2 * agg::sd_min(sB*M[A], mB*S[A]) + 0xff) >> 8));
         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_exclusion {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         auto sR = glLinearRGB.convert(S[R]);
         auto sG = glLinearRGB.convert(S[G]);
         auto sB = glLinearRGB.convert(S[B]);

         auto mR = glLinearRGB.convert(M[R]);
         auto mG = glLinearRGB.convert(M[G]);
         auto mB = glLinearRGB.convert(M[B]);

         const UBYTE d1a = 0xff - D[A];
         const UBYTE s1a = 0xff - S[A];
         D[R] = glLinearRGB.invert((sR*M[A] + mR*S[A] - 2*sR*mR + sR*d1a + mR*s1a + 0xff) >> 8);
         D[G] = glLinearRGB.invert((sG*M[A] + mG*S[A] - 2*sG*mG + sG*d1a + mG*s1a + 0xff) >> 8);
         D[B] = glLinearRGB.invert((sB*M[A] + mB*S[A] - 2*sB*mB + sB*d1a + mB*s1a + 0xff) >> 8);
         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_plus {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         auto sR = glLinearRGB.convert(S[R]);
         auto sG = glLinearRGB.convert(S[G]);
         auto sB = glLinearRGB.convert(S[B]);

         const UBYTE xr = glLinearRGB.convert(D[R]) + sR;
         const UBYTE xg = glLinearRGB.convert(D[G]) + sG;
         const UBYTE xb = glLinearRGB.convert(D[B]) + sB;
         const UBYTE xa = D[A] + S[A];
         D[R] = glLinearRGB.invert((xr > 0xff) ? (UBYTE)0xff : xr);
         D[G] = glLinearRGB.invert((xg > 0xff) ? (UBYTE)0xff : xg);
         D[B] = glLinearRGB.invert((xb > 0xff) ? (UBYTE)0xff : xb);
         D[A] = (xa > 0xff) ? (UBYTE)0xff : xa;
      }
   }
};

struct blend_minus {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         auto sR = glLinearRGB.convert(S[R]);
         auto sG = glLinearRGB.convert(S[G]);
         auto sB = glLinearRGB.convert(S[B]);

         const UBYTE xr = glLinearRGB.convert(D[R]) - sR;
         const UBYTE xg = glLinearRGB.convert(D[G]) - sG;
         const UBYTE xb = glLinearRGB.convert(D[B]) - sB;
         D[R] = glLinearRGB.invert((xr > 0xff) ? 0 : xr);
         D[G] = glLinearRGB.invert((xg > 0xff) ? 0 : xg);
         D[B] = glLinearRGB.invert((xb > 0xff) ? 0 : xb);
         D[A] = (UBYTE)(S[A] + D[A] - ((S[A] * D[A] + 0xff) >> 8));
         //D[A] = (UBYTE)(0xff - (((0xff - S[A]) * (0xff - D[A]) + 0xff) >> 8));
      }
   }
};

struct blend_invert {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if ((S[A]) or (M[A])) {
         auto dR = glLinearRGB.convert(D[R]);
         auto dG = glLinearRGB.convert(D[G]);
         auto dB = glLinearRGB.convert(D[B]);

         const UBYTE xr = ((M[A] - dR) * S[A] + 0xff) >> 8;
         const UBYTE xg = ((M[A] - dG) * S[A] + 0xff) >> 8;
         const UBYTE xb = ((M[A] - dB) * S[A] + 0xff) >> 8;
         const UBYTE s1a = 0xff - S[A];
         D[R] = glLinearRGB.invert(xr + ((dR * s1a + 0xff) >> 8));
         D[G] = glLinearRGB.invert(xg + ((dG * s1a + 0xff) >> 8));
         D[B] = glLinearRGB.invert(xb + ((dB * s1a + 0xff) >> 8));
         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
      }
   }
};

struct blend_invert_rgb {
   static inline void blend(UBYTE *D, UBYTE *S, UBYTE *M, UBYTE A, UBYTE R, UBYTE G, UBYTE B) {
      if (S[A]) {
         auto sR = glLinearRGB.convert(S[R]);
         auto sG = glLinearRGB.convert(S[G]);
         auto sB = glLinearRGB.convert(S[B]);

         auto dR = glLinearRGB.convert(D[R]);
         auto dG = glLinearRGB.convert(D[G]);
         auto dB = glLinearRGB.convert(D[B]);

         UBYTE xr = ((M[A] - dR) * sR + 0xff) >> 8;
         UBYTE xg = ((M[A] - dG) * sG + 0xff) >> 8;
         UBYTE xb = ((M[A] - dB) * sB + 0xff) >> 8;
         UBYTE s1a = 0xff - S[A];
         D[R] = glLinearRGB.invert(xr + ((dR * s1a + 0xff) >> 8));
         D[G] = glLinearRGB.invert(xg + ((dG * s1a + 0xff) >> 8));
         D[B] = glLinearRGB.invert(xb + ((dB * s1a + 0xff) >> 8));
         D[A] = (UBYTE)(S[A] + M[A] - ((S[A] * M[A] + 0xff) >> 8));
      }
   }
};

/*********************************************************************************************************************
-ACTION-
Draw: Render the effect to the target bitmap.
-END-
*********************************************************************************************************************/

static ERR COMPOSITEFX_Draw(extCompositeFX *Self, struct acDraw *Args)
{
   pf::Log log;

   if (Self->Target->BytesPerPixel != 4) return ERR::Failed;

   objBitmap *inBmp;

   UBYTE *dest = Self->Target->Data + (Self->Target->Clip.Left * 4) + (Self->Target->Clip.Top * Self->Target->LineWidth);

   switch (Self->Operator) {
      case OP::OVER: {
         if (get_source_bitmap(Self->Filter, &inBmp, Self->SourceType, Self->Input, false) IS ERR::Okay) {
            objBitmap *mixBmp;
            if (get_source_bitmap(Self->Filter, &mixBmp, Self->MixType, Self->Mix, false) IS ERR::Okay) {
               UBYTE *in  = inBmp->Data + (inBmp->Clip.Left * 4) + (inBmp->Clip.Top * inBmp->LineWidth);
               UBYTE *mix = mixBmp->Data + (mixBmp->Clip.Left * 4) + (mixBmp->Clip.Top * mixBmp->LineWidth);
               Self->doMix<composite_over>(inBmp, mixBmp, dest, in, mix);
            }
         }
         break;
      }

      case OP::IN: {
         if (get_source_bitmap(Self->Filter, &inBmp, Self->SourceType, Self->Input, false) IS ERR::Okay) {
            objBitmap *mixBmp;
            if (get_source_bitmap(Self->Filter, &mixBmp, Self->MixType, Self->Mix, false) IS ERR::Okay) {
               UBYTE *in  = inBmp->Data + (inBmp->Clip.Left * 4) + (inBmp->Clip.Top * inBmp->LineWidth);
               UBYTE *mix = mixBmp->Data + (mixBmp->Clip.Left * 4) + (mixBmp->Clip.Top * mixBmp->LineWidth);
               Self->doMix<composite_in>(inBmp, mixBmp, dest, in, mix);
            }
         }
         break;
      }

      case OP::OUT: {
         if (get_source_bitmap(Self->Filter, &inBmp, Self->SourceType, Self->Input, false) IS ERR::Okay) {
            objBitmap *mixBmp;
            if (get_source_bitmap(Self->Filter, &mixBmp, Self->MixType, Self->Mix, false) IS ERR::Okay) {
               UBYTE *in  = inBmp->Data + (inBmp->Clip.Left * 4) + (inBmp->Clip.Top * inBmp->LineWidth);
               UBYTE *mix = mixBmp->Data + (mixBmp->Clip.Left * 4) + (mixBmp->Clip.Top * mixBmp->LineWidth);
               Self->doMix<composite_out>(inBmp, mixBmp, dest, in, mix);
            }
         }
         break;
      }

      case OP::ATOP: {
         if (get_source_bitmap(Self->Filter, &inBmp, Self->SourceType, Self->Input, false) IS ERR::Okay) {
            objBitmap *mixBmp;
            if (get_source_bitmap(Self->Filter, &mixBmp, Self->MixType, Self->Mix, false) IS ERR::Okay) {
               UBYTE *in  = inBmp->Data + (inBmp->Clip.Left * 4) + (inBmp->Clip.Top * inBmp->LineWidth);
               UBYTE *mix = mixBmp->Data + (mixBmp->Clip.Left * 4) + (mixBmp->Clip.Top * mixBmp->LineWidth);
               Self->doMix<composite_atop>(inBmp, mixBmp, dest, in, mix);
            }
         }
         break;
      }

      case OP::XOR: {
         if (get_source_bitmap(Self->Filter, &inBmp, Self->SourceType, Self->Input, false) IS ERR::Okay) {
            objBitmap *mixBmp;
            if (get_source_bitmap(Self->Filter, &mixBmp, Self->MixType, Self->Mix, false) IS ERR::Okay) {
               UBYTE *in  = inBmp->Data + (inBmp->Clip.Left * 4) + (inBmp->Clip.Top * inBmp->LineWidth);
               UBYTE *mix = mixBmp->Data + (mixBmp->Clip.Left * 4) + (mixBmp->Clip.Top * mixBmp->LineWidth);
               Self->doMix<composite_xor>(inBmp, mixBmp, dest, in, mix);
            }
         }
         break;
      }

      case OP::ARITHMETIC: {
         if (get_source_bitmap(Self->Filter, &inBmp, Self->SourceType, Self->Input, false) IS ERR::Okay) {
            objBitmap *mixBmp;
            LONG height = Self->Target->Clip.Bottom - Self->Target->Clip.Top;
            LONG width  = Self->Target->Clip.Right - Self->Target->Clip.Left;
            if (inBmp->Clip.Right - inBmp->Clip.Left < width) width = inBmp->Clip.Right - inBmp->Clip.Left;
            if (inBmp->Clip.Bottom - inBmp->Clip.Top < height) height = inBmp->Clip.Bottom - inBmp->Clip.Top;

            const UBYTE A = Self->Target->ColourFormat->AlphaPos>>3;
            const UBYTE R = Self->Target->ColourFormat->RedPos>>3;
            const UBYTE G = Self->Target->ColourFormat->GreenPos>>3;
            const UBYTE B = Self->Target->ColourFormat->BluePos>>3;

            if (get_source_bitmap(Self->Filter, &mixBmp, Self->MixType, Self->Mix, false) IS ERR::Okay) {
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
                        const DOUBLE sR = DOUBLE(glLinearRGB.convert(sp[R])) * SCALE * sA;
                        const DOUBLE sG = DOUBLE(glLinearRGB.convert(sp[G])) * SCALE * sA;
                        const DOUBLE sB = DOUBLE(glLinearRGB.convert(sp[B])) * SCALE * sA;

                        const DOUBLE mA = DOUBLE(mp[A]) * SCALE;
                        const DOUBLE mR = DOUBLE(glLinearRGB.convert(mp[R])) * SCALE * mA;
                        const DOUBLE mG = DOUBLE(glLinearRGB.convert(mp[G])) * SCALE * mA;
                        const DOUBLE mB = DOUBLE(glLinearRGB.convert(mp[B])) * SCALE * mA;

                        DOUBLE dA = (Self->K1 * sA * mA) + (Self->K2 * sA) + (Self->K3 * mA) + Self->K4;

                        if (dA > 0.0) {
                           if (dA > 1.0) dA = 1.0;

                           DOUBLE demul = 1.0 / dA;
                           LONG dr = F2T(((Self->K1 * sR * mR) + (Self->K2 * sR) + (Self->K3 * mR) + Self->K4) * demul * DESCALE);
                           LONG dg = F2T(((Self->K1 * sG * mG) + (Self->K2 * sG) + (Self->K3 * mG) + Self->K4) * demul * DESCALE);
                           LONG db = F2T(((Self->K1 * sB * mB) + (Self->K2 * sB) + (Self->K3 * mB) + Self->K4) * demul * DESCALE);

                           if (dr > 0xff) dp[R] = 0xff;
                           else if (dr < 0) dp[R] = 0;
                           else dp[R] = glLinearRGB.invert(dr);

                           if (dg > 0xff) dp[G] = 0xff;
                           else if (dg < 0) dp[G] = 0;
                           else dp[G] = glLinearRGB.invert(dg);

                           if (db > 0xff) dp[B] = 0xff;
                           else if (db < 0) dp[B] = 0;
                           else dp[B] = glLinearRGB.invert(db);

                           dp[A] = F2T(dA * DESCALE);
                        }
                     }

                     dp += 4;
                     sp += 4;
                     mp += 4;
                  }
                  dest += Self->Target->LineWidth;
                  in   += inBmp->LineWidth;
                  mix  += mixBmp->LineWidth;
               }
            }
         }
         break;
      }

      default: { // These mix routines use pre-multiplied content.
         if (get_source_bitmap(Self->Filter, &inBmp, Self->SourceType, Self->Input, true) IS ERR::Okay) {
            objBitmap *mixBmp;
            if (get_source_bitmap(Self->Filter, &mixBmp, Self->MixType, Self->Mix, true) IS ERR::Okay) {
               UBYTE *in  = inBmp->Data + (inBmp->Clip.Left * 4) + (inBmp->Clip.Top * inBmp->LineWidth);
               UBYTE *mix = mixBmp->Data + (mixBmp->Clip.Left * 4) + (mixBmp->Clip.Top * mixBmp->LineWidth);
               #pragma GCC diagnostic ignored "-Wswitch"
               switch(Self->Operator) {
                  case OP::MULTIPLY:    Self->doMix<blend_multiply>(inBmp, mixBmp, dest, in, mix); break;
                  case OP::SCREEN:      Self->doMix<blend_screen>(inBmp, mixBmp, dest, in, mix); break;
                  case OP::DARKEN:      Self->doMix<blend_darken>(inBmp, mixBmp, dest, in, mix); break;
                  case OP::LIGHTEN:     Self->doMix<blend_lighten>(inBmp, mixBmp, dest, in, mix); break;
                  case OP::OVERLAY:     Self->doMix<blend_overlay>(inBmp, mixBmp, dest, in, mix); break;
                  case OP::BURN:        Self->doMix<blend_burn>(inBmp, mixBmp, dest, in, mix); break;
                  case OP::DODGE:       Self->doMix<blend_dodge>(inBmp, mixBmp, dest, in, mix); break;
                  case OP::HARD_LIGHT:  Self->doMix<blend_hard_light>(inBmp, mixBmp, dest, in, mix); break;
                  case OP::SOFT_LIGHT:  Self->doMix<blend_soft_light>(inBmp, mixBmp, dest, in, mix); break;
                  case OP::DIFFERENCE:  Self->doMix<blend_difference>(inBmp, mixBmp, dest, in, mix); break;
                  case OP::EXCLUSION:   Self->doMix<blend_exclusion>(inBmp, mixBmp, dest, in, mix); break;
                  case OP::PLUS:        Self->doMix<blend_plus>(inBmp, mixBmp, dest, in, mix); break;
                  case OP::MINUS:       Self->doMix<blend_minus>(inBmp, mixBmp, dest, in, mix); break;
                  case OP::CONTRAST:    Self->doMix<blend_contrast>(inBmp, mixBmp, dest, in, mix); break;
                  case OP::INVERT:      Self->doMix<blend_invert>(inBmp, mixBmp, dest, in, mix); break;
                  case OP::INVERT_RGB:  Self->doMix<blend_invert_rgb>(inBmp, mixBmp, dest, in, mix); break;
               }

                bmpDemultiply(mixBmp);
            }
            bmpDemultiply(inBmp);
         }

         break;
      }
   }

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR COMPOSITEFX_Init(extCompositeFX *Self, APTR Void)
{
   pf::Log log;

   if (Self->MixType IS VSF::NIL) {
      log.warning("A mix input is required.");
      return ERR::FieldNotSet;
   }

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR COMPOSITEFX_NewObject(extCompositeFX *Self, APTR Void)
{
   Self->Operator = OP::OVER;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
K1: Input value for the arithmetic operation.

*********************************************************************************************************************/

static ERR COMPOSITEFX_GET_K1(extCompositeFX *Self, DOUBLE *Value)
{
   *Value = Self->K1;
   return ERR::Okay;
}

static ERR COMPOSITEFX_SET_K1(extCompositeFX *Self, DOUBLE Value)
{
   Self->K1 = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
K2: Input value for the arithmetic operation.

*********************************************************************************************************************/

static ERR COMPOSITEFX_GET_K2(extCompositeFX *Self, DOUBLE *Value)
{
   *Value = Self->K2;
   return ERR::Okay;
}

static ERR COMPOSITEFX_SET_K2(extCompositeFX *Self, DOUBLE Value)
{
   Self->K2 = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
K3: Input value for the arithmetic operation.

*********************************************************************************************************************/

static ERR COMPOSITEFX_GET_K3(extCompositeFX *Self, DOUBLE *Value)
{
   *Value = Self->K3;
   return ERR::Okay;
}

static ERR COMPOSITEFX_SET_K3(extCompositeFX *Self, DOUBLE Value)
{
   Self->K3 = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
K4: Input value for the arithmetic operation.

*********************************************************************************************************************/

static ERR COMPOSITEFX_GET_K4(extCompositeFX *Self, DOUBLE *Value)
{
   *Value = Self->K4;
   return ERR::Okay;
}

static ERR COMPOSITEFX_SET_K4(extCompositeFX *Self, DOUBLE Value)
{
   Self->K4 = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Operator: The compositing algorithm to use for rendering.
Lookup: OP

Setting the Operator will determine the algorithm that is used for compositing.  The default is `OVER`.

*********************************************************************************************************************/

static ERR COMPOSITEFX_GET_Operator(extCompositeFX *Self, OP *Value)
{
   *Value = Self->Operator;
   return ERR::Okay;
}

static ERR COMPOSITEFX_SET_Operator(extCompositeFX *Self, OP Value)
{
   Self->Operator = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
XMLDef: Returns an SVG compliant XML string that describes the filter.
-END-

*********************************************************************************************************************/

static ERR COMPOSITEFX_GET_XMLDef(extCompositeFX *Self, STRING *Value)
{
   *Value = StrClone("feComposite");
   return ERR::Okay;
}

//********************************************************************************************************************

#include "filter_composite_def.c"

static const FieldDef clCompositeOperator[] = {
   { "Over",       OP::OVER },
   { "In",         OP::IN },
   { "Out",        OP::OUT },
   { "Atop",       OP::ATOP },
   { "Xor",        OP::XOR },
   { "Arithmetic", OP::ARITHMETIC },
   { "Screen",     OP::SCREEN },
   { "Multiply",   OP::MULTIPLY },
   { "Lighten",    OP::LIGHTEN },
   { "Darken",     OP::DARKEN },
   { "InvertRGB",  OP::INVERT_RGB },
   { "Invert",     OP::INVERT },
   { "Contrast",   OP::CONTRAST },
   { "Dodge",      OP::DODGE },
   { "Burn",       OP::BURN },
   { "HardLight",  OP::HARD_LIGHT },
   { "SoftLight",  OP::SOFT_LIGHT },
   { "Difference", OP::DIFFERENCE },
   { "Exclusion",  OP::EXCLUSION },
   { "Plus",       OP::PLUS },
   { "Minus",      OP::MINUS },
   { "Subtract",   OP::SUBTRACT },
   { "Overlay",    OP::OVERLAY },
   { NULL, 0 }
};

static const FieldArray clCompositeFXFields[] = {
   { "Operator", FDF_VIRTUAL|FDF_LONG|FDF_LOOKUP|FDF_RW, COMPOSITEFX_GET_Operator, COMPOSITEFX_SET_Operator, &clCompositeOperator },
   { "K1",       FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, COMPOSITEFX_GET_K1, COMPOSITEFX_SET_K1 },
   { "K2",       FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, COMPOSITEFX_GET_K2, COMPOSITEFX_SET_K2 },
   { "K3",       FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, COMPOSITEFX_GET_K3, COMPOSITEFX_SET_K3 },
   { "K4",       FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, COMPOSITEFX_GET_K4, COMPOSITEFX_SET_K4 },
   { "XMLDef",   FDF_VIRTUAL|FDF_STRING|FDF_ALLOC|FDF_R, COMPOSITEFX_GET_XMLDef },
   END_FIELD
};

//********************************************************************************************************************

ERR init_compositefx(void)
{
   clCompositeFX = objMetaClass::create::global(
      fl::BaseClassID(ID_FILTEREFFECT),
      fl::ClassID(ID_COMPOSITEFX),
      fl::Name("CompositeFX"),
      fl::Category(CCF::GRAPHICS),
      fl::Actions(clCompositeFXActions),
      fl::Fields(clCompositeFXFields),
      fl::Size(sizeof(extCompositeFX)),
      fl::Path(MOD_PATH));

   return clCompositeFX ? ERR::Okay : ERR::AddClass;
}
