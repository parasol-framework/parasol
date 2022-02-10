
enum {
   OP_OVER=0, OP_IN, OP_OUT, OP_ATOP, OP_XOR, OP_ARITHMETIC, OP_NORMAL, OP_SCREEN, OP_MULTIPLY, OP_LIGHTEN, OP_DARKEN,
   OP_INVERTRGB, OP_INVERT, OP_CONTRAST, OP_DODGE, OP_BURN, OP_HARDLIGHT, OP_SOFTLIGHT, OP_DIFFERENCE, OP_EXCLUSION,
   OP_PLUS, OP_MINUS, OP_OVERLAY
};

//****************************************************************************

class CompositeEffect : public VectorEffect {
   DOUBLE K1, K2, K3, K4;
   UBYTE Operator;
   UBYTE Source;

   template <class DrawOp>
   void doComposite(objBitmap *SrcBitmap, UBYTE *Dest, UBYTE *Src, LONG Width, LONG Height) {
      const UBYTE A = Bitmap->ColourFormat->AlphaPos>>3;
      const UBYTE R = Bitmap->ColourFormat->RedPos>>3;
      const UBYTE G = Bitmap->ColourFormat->GreenPos>>3;
      const UBYTE B = Bitmap->ColourFormat->BluePos>>3;

      for (LONG y=0; y < Height; y++) {
         auto dp = Dest;
         auto sp = Src;
         for (LONG x=0; x < Width; x++) {
            DrawOp::blend_pix(dp, sp[R], sp[G], sp[B], sp[A], 255);
            dp += 4;
            sp += 4;
         }
         Dest += Bitmap->LineWidth;
         Src  += SrcBitmap->LineWidth;
      }
   }

public:
   CompositeEffect(struct rkVectorFilter *Filter, XMLTag *Tag) : VectorEffect() {
      parasol::Log log(__FUNCTION__);

      Operator = OP_OVER;

      for (LONG a=1; a < Tag->TotalAttrib; a++) {
         CSTRING val = Tag->Attrib[a].Value;
         if (!val) continue;

         ULONG hash = StrHash(Tag->Attrib[a].Name, FALSE);
         switch (hash) {
            case SVF_IN: {
               switch (StrHash(val, FALSE)) {
                  case SVF_SOURCEGRAPHIC:   Source = VSF_GRAPHIC; break;
                  case SVF_SOURCEALPHA:     Source = VSF_ALPHA; break;
                  case SVF_BACKGROUNDIMAGE: Source = VSF_BKGD; break;
                  case SVF_BACKGROUNDALPHA: Source = VSF_BKGD_ALPHA; break;
                  case SVF_FILLPAINT:       Source = VSF_FILL; break;
                  case SVF_STROKEPAINT:     Source = VSF_STROKE; break;
                  default:  {
                     VectorEffect *ref;
                     if ((ref = find_effect(Filter, val))) {
                        if (ref != this) {
                           Source = VSF_REFERENCE;
                           InputID = ref->ID;
                        }
                     }
                     break;
                  }
               }
               break;
            }

            case SVF_IN2: { // 'In2' is usually the BackgroundImage that 'in' will be copied over.
               switch (StrHash(val, FALSE)) {
                  case SVF_SOURCEGRAPHIC:   Source = VSF_GRAPHIC; break;
                  case SVF_SOURCEALPHA:     Source = VSF_ALPHA; break;
                  case SVF_BACKGROUNDIMAGE: Source = VSF_BKGD; break;
                  case SVF_BACKGROUNDALPHA: Source = VSF_BKGD_ALPHA; break;
                  case SVF_FILLPAINT:       Source = VSF_FILL; break;
                  case SVF_STROKEPAINT:     Source = VSF_STROKE; break;
                  default:  {
                     VectorEffect *ref;
                     if ((ref = find_effect(Filter, val))) {
                        if (ref != this) {
                           Source  = VSF_REFERENCE;
                           InputID = ref->ID;
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

      if (!Source) {
         log.warning("Composite element requires 'in2' attribute.");
         Error = ERR_FieldNotSet;
         return;
      }
   }

   void apply(objVectorFilter *Filter) {
      if (Bitmap->BytesPerPixel != 4) return;

      objBitmap *srcBmp;

      if (!get_bitmap(Filter, &srcBmp, Source, InputID)) {
         UBYTE *dest = Bitmap->Data + (Bitmap->Clip.Left * Bitmap->BytesPerPixel) + (Bitmap->Clip.Top * Bitmap->LineWidth);
         UBYTE *src  = srcBmp->Data + (srcBmp->Clip.Left * srcBmp->BytesPerPixel) + (srcBmp->Clip.Top * srcBmp->LineWidth);

         LONG height = Bitmap->Clip.Bottom - Bitmap->Clip.Top;
         LONG width  = Bitmap->Clip.Right - Bitmap->Clip.Left;
         if (srcBmp->Clip.Right - srcBmp->Clip.Left < width) width = srcBmp->Clip.Right - srcBmp->Clip.Left;
         if (srcBmp->Clip.Bottom - srcBmp->Clip.Top < height) height = srcBmp->Clip.Bottom - srcBmp->Clip.Top;

         premultiply_bitmap(srcBmp);
         premultiply_bitmap(Bitmap);

         const UBYTE A = Bitmap->ColourFormat->AlphaPos>>3;
         const UBYTE R = Bitmap->ColourFormat->RedPos>>3;
         const UBYTE G = Bitmap->ColourFormat->GreenPos>>3;
         const UBYTE B = Bitmap->ColourFormat->BluePos>>3;

         switch (Operator) {
            case OP_OVER:
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
                  dest += Bitmap->LineWidth;
                  src  += srcBmp->LineWidth;
               }
               break;

            case OP_IN:
               for (LONG y=0; y < height; y++) {
                  auto dp = dest;
                  auto sp = src;
                  for (LONG x=0; x < width; x++) {
                     dp[R] = (sp[R] * dp[A] + 0xff)>>8;
                     dp[G] = (sp[G] * dp[A] + 0xff)>>8;
                     dp[B] = (sp[B] * dp[A] + 0xff)>>8;
                     dp[A] = (sp[A] * dp[A] + 0xff)>>8;
                     dp += 4;
                     sp += 4;
                  }
                  dest += Bitmap->LineWidth;
                  src  += srcBmp->LineWidth;
               }
               break;

            case OP_OUT:
               doComposite<agg::comp_op_rgba_src_out <agg::rgba8, agg::order_bgra>>(srcBmp, dest, src, width, height);
               break;

            case OP_ATOP:
               doComposite<agg::comp_op_rgba_src_atop <agg::rgba8, agg::order_bgra>>(srcBmp, dest, src, width, height);
               break;

            case OP_XOR:
               doComposite<agg::comp_op_rgba_xor <agg::rgba8, agg::order_bgra>>(srcBmp, dest, src, width, height);
               break;

            case OP_ARITHMETIC: {
               DOUBLE k1=K1, k2=K2, k3=K3, k4=K4;
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
                  dest += Bitmap->LineWidth;
                  src  += srcBmp->LineWidth;
               }
               break;
            }

            case OP_MULTIPLY:
               doComposite<agg::comp_op_rgba_multiply <agg::rgba8, agg::order_bgra>>(srcBmp, dest, src, width, height);
               break;

            case OP_SCREEN:
               doComposite<agg::comp_op_rgba_screen <agg::rgba8, agg::order_bgra>>(srcBmp, dest, src, width, height);
               break;

            case OP_DARKEN:
               doComposite<agg::comp_op_rgba_multiply <agg::rgba8, agg::order_bgra>>(srcBmp, dest, src, width, height);
               break;

            case OP_LIGHTEN:
               doComposite<agg::comp_op_rgba_multiply <agg::rgba8, agg::order_bgra>>(srcBmp, dest, src, width, height);
               break;

            case OP_OVERLAY:
               doComposite<agg::comp_op_rgba_overlay <agg::rgba8, agg::order_bgra>>(srcBmp, dest, src, width, height);
               break;

            case OP_BURN:
               doComposite<agg::comp_op_rgba_color_burn <agg::rgba8, agg::order_bgra>>(srcBmp, dest, src, width, height);
               break;

            case OP_DODGE:
               doComposite<agg::comp_op_rgba_color_dodge <agg::rgba8, agg::order_bgra>>(srcBmp, dest, src, width, height);
               break;

            case OP_HARDLIGHT:
               doComposite<agg::comp_op_rgba_hard_light <agg::rgba8, agg::order_bgra>>(srcBmp, dest, src, width, height);
               break;

            case OP_SOFTLIGHT:
               doComposite<agg::comp_op_rgba_soft_light <agg::rgba8, agg::order_bgra>>(srcBmp, dest, src, width, height);
               break;

            case OP_DIFFERENCE:
               doComposite<agg::comp_op_rgba_difference <agg::rgba8, agg::order_bgra>>(srcBmp, dest, src, width, height);
               break;

            case OP_EXCLUSION:
               doComposite<agg::comp_op_rgba_exclusion <agg::rgba8, agg::order_bgra>>(srcBmp, dest, src, width, height);
               break;

            case OP_PLUS:
               doComposite<agg::comp_op_rgba_plus <agg::rgba8, agg::order_bgra>>(srcBmp, dest, src, width, height);
               break;

            case OP_MINUS:
               doComposite<agg::comp_op_rgba_minus <agg::rgba8, agg::order_bgra>>(srcBmp, dest, src, width, height);
               break;

            case OP_CONTRAST:
               doComposite<agg::comp_op_rgba_multiply <agg::rgba8, agg::order_bgra>>(srcBmp, dest, src, width, height);
               break;

            case OP_INVERT:
               doComposite<agg::comp_op_rgba_invert <agg::rgba8, agg::order_bgra>>(srcBmp, dest, src, width, height);
               break;

            case OP_INVERTRGB:
               doComposite<agg::comp_op_rgba_invert_rgb <agg::rgba8, agg::order_bgra>>(srcBmp, dest, src, width, height);
               break;
         }

         demultiply_bitmap(srcBmp);
         demultiply_bitmap(Bitmap);
      }
   }

   virtual ~CompositeEffect() { }
};
