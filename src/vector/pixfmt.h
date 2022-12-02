
INLINE void BLEND32(UBYTE *p, UBYTE oR, UBYTE oG, UBYTE oB, UBYTE oA, UBYTE cr, UBYTE cg, UBYTE cb, UBYTE ca)
{
   p[oR] = ((p[oR] * (0xff-ca)) + (cr * ca) + 0xff)>>8;
   p[oG] = ((p[oG] * (0xff-ca)) + (cg * ca) + 0xff)>>8;
   p[oB] = ((p[oB] * (0xff-ca)) + (cb * ca) + 0xff)>>8;
   p[oA] = 0xff - (((0xff - ca) * (0xff - p[oA]))>>8); // The W3C's SVG sanctioned method for the alpha channel :)
}

INLINE void LINEAR32(UBYTE *p, UBYTE oR, UBYTE oG, UBYTE oB, UBYTE oA, UBYTE cr, UBYTE cg, UBYTE cb, UBYTE ca)
{
   p[oR] = glLinearRGB.invert(((glLinearRGB.convert(p[oR]) * (0xff-ca)) + (glLinearRGB.convert(cr) * ca) + 0xff)>>8);
   p[oG] = glLinearRGB.invert(((glLinearRGB.convert(p[oG]) * (0xff-ca)) + (glLinearRGB.convert(cg) * ca) + 0xff)>>8);
   p[oB] = glLinearRGB.invert(((glLinearRGB.convert(p[oB]) * (0xff-ca)) + (glLinearRGB.convert(cb) * ca) + 0xff)>>8);
   p[oA] = 0xff - (((0xff - ca) * (0xff - p[oA]))>>8);
}

INLINE void COPY32(UBYTE *p, ULONG r, ULONG g, ULONG b, ULONG a, ULONG cr, ULONG cg, ULONG cb, ULONG ca)
{
   p[r] = cr;
   p[g] = cg;
   p[b] = cb;
   p[a] = ca;
}

/*
INLINE void BLEND32(UBYTE *p, UBYTE r, UBYTE g, UBYTE b, UBYTE a, UBYTE cr, UBYTE cg, UBYTE cb, UBYTE ca)
{
   const ULONG a1 = p[a];
   const ULONG &a2 = ca;
   const ULONG a2inv = 0xff - a2;
   const ULONG a5 = a2inv * a1;
   const ULONG a3 = a2 + a5 / 0xff;

   if (a3 > 0) {
       const ULONG r1 = glGamma.dir(p[r]);
       const ULONG g1 = glGamma.dir(p[g]);
       const ULONG b1 = glGamma.dir(p[b]);

       const ULONG r2 = glGamma.dir(cr);
       const ULONG g2 = glGamma.dir(cg);
       const ULONG b2 = glGamma.dir(cb);

       const ULONG a4 = 0xff * a2;
       const ULONG a6 = 0xff * a3;

       const ULONG r3 = (r2 * a4 + r1 * a5) / a6;
       const ULONG g3 = (g2 * a4 + g1 * a5) / a6;
       const ULONG b3 = (b2 * a4 + b1 * a5) / a6;

       p[r] = glGamma.inv(r3 < glGamma.hi_res_mask ? r3 : glGamma.hi_res_mask);
       p[g] = glGamma.inv(g3 < glGamma.hi_res_mask ? g3 : glGamma.hi_res_mask);
       p[b] = glGamma.inv(b3 < glGamma.hi_res_mask ? b3 : glGamma.hi_res_mask);
       p[a] = a3;
   }
   else {
      p[r] = 0;
      p[g] = 0;
      p[b] = 0;
      p[a] = 0;
   }
}

INLINE void COPY32(UBYTE *p, UBYTE r, UBYTE g, UBYTE b, UBYTE a, UBYTE cr, UBYTE cg, UBYTE cb, UBYTE ca)
{
   p[R] = glGamma.dir(p[cr])>>4;
   p[G] = glGamma.dir(p[cg])>>4;
   p[B] = glGamma.dir(p[cb])>>4;
   p[a] = ca;
}
*/

namespace agg {

class pixfmt_psl
{
public:
   typedef agg::rgba8 color_type;
   typedef typename agg::rendering_buffer::row_data row_data;

   pixfmt_psl() :  oR(0), oG(0), oB(0), oA(0) {}
   explicit pixfmt_psl(objBitmap &Bitmap, bool Linear = false) : oR(0), oG(0), oB(0), oA(0) {
      setBitmap(Bitmap, Linear);
   }

   void setBitmap(objBitmap &Bitmap, bool Linear = false);

   // The setBitmap() code in scene_draw.cpp defines the following functions.
   void (*fBlendPix)(agg::pixfmt_psl *, UBYTE *, ULONG cr, ULONG cg, ULONG cb, ULONG alpha);
   void (*fCopyPix)(agg::pixfmt_psl *,  UBYTE *, ULONG cr, ULONG cg, ULONG cb, ULONG alpha);
   void (*fCoverPix)(agg::pixfmt_psl *, UBYTE *, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG);
   void (*fBlendHLine)(agg::pixfmt_psl *, int x, int y, unsigned len, const agg::rgba8 &c, int8u cover);
   void (*fBlendSolidHSpan)(agg::pixfmt_psl *, int x, int y, ULONG len, const agg::rgba8 &, const UBYTE *covers);
   void (*fBlendColorHSpan)(agg::pixfmt_psl *, int x, int y, ULONG len, const agg::rgba8 *, const UBYTE *covers, UBYTE cover);
   void (*fCopyColorHSpan)(agg::pixfmt_psl *, int x, int y, ULONG len, const agg::rgba8 *); // copy_color_hspan

   void linearMode(bool pEnable = true) {
      setBitmap(*mBitmap, pEnable);
   }

   AGG_INLINE unsigned width()  const { return mBitmap->Clip.Right;  }
   AGG_INLINE unsigned height() const { return mBitmap->Clip.Bottom; }
   AGG_INLINE int      stride() const { return mBitmap->LineWidth; }
   AGG_INLINE UBYTE *   row_ptr(int y) { return mData + (y * mBitmap->LineWidth); }
   AGG_INLINE const UBYTE * row_ptr(int y) const { return mData + (y * mBitmap->LineWidth); }

private:
   void pixel_order(UBYTE aoR, UBYTE aoG, UBYTE aoB, UBYTE aoA)
   {
      oR = aoR;
      oG = aoG;
      oB = aoB;
      oA = aoA;
   }

   // Blend the pixel at (p) with the provided colour values and store the result back in (p)

   static void blend32BGRA(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (p[3]) BLEND32(p,2,1,0,3,cr,cg,cb,alpha);
      else {
         p[2] = cr;
         p[1] = cg;
         p[0] = cb;
         p[3] = alpha;
      }
   }

   static void blend32RGBA(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (p[3]) BLEND32(p,0,1,2,3,cr,cg,cb,alpha);
      else {
         p[0] = cr;
         p[1] = cg;
         p[2] = cb;
         p[3] = alpha;
      }
   }

   static void blend32AGBR(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (p[0]) BLEND32(p,3,1,2,0,cr,cg,cb,alpha);
      else {
         p[3] = cr;
         p[1] = cg;
         p[2] = cb;
         p[0] = alpha;
      }
   }

   static void blend32ARGB(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (p[0]) BLEND32(p,1,2,3,0,cr,cg,cb,alpha);
      else {
         p[1] = cr;
         p[2] = cg;
         p[3] = cb;
         p[0] = alpha;
      }
   }

   // Linear version of the blend operations

   static void linear32BGRA(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (p[3]) LINEAR32(p,2,1,0,3,cr,cg,cb,alpha);
      else {
         p[2] = cr;
         p[1] = cg;
         p[0] = cb;
         p[3] = alpha;
      }
   }

   static void linear32RGBA(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (p[3]) LINEAR32(p,0,1,2,3,cr,cg,cb,alpha);
      else {
         p[0] = cr;
         p[1] = cg;
         p[2] = cb;
         p[3] = alpha;
      }
   }

   static void linear32AGBR(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (p[0]) LINEAR32(p,3,1,2,0,cr,cg,cb,alpha);
      else {
         p[3] = cr;
         p[1] = cg;
         p[2] = cb;
         p[0] = alpha;
      }
   }

   static void linear32ARGB(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (p[0]) LINEAR32(p,1,2,3,0,cr,cg,cb,alpha);
      else {
         p[1] = cr;
         p[2] = cg;
         p[3] = cb;
         p[0] = alpha;
      }
   }

   // Direct copy pixel if possible.

   inline static void copy32BGRA(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (alpha) {
         if ((alpha == 0xff) or (!p[3])) {
            p[2] = cr;
            p[1] = cg;
            p[0] = cb;
            p[3] = alpha;
         }
         else BLEND32(p,2,1,0,3,cr,cg,cb,alpha);
      }
   }

   static void cover32BGRA(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
   {
      if (cover == 255) copy32BGRA(Self, p, cr, cg, cb, alpha);
      else if (alpha) {
         alpha = (alpha * (cover + 1)) >> 8;
         if ((alpha == 0xff) or (!p[3])) {
            p[2] = cr;
            p[1] = cg;
            p[0] = cb;
            p[3] = alpha;
         }
         else BLEND32(p,2,1,0,3,cr,cg,cb,alpha);
      }
   }

   inline static void copy32RGBA(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (alpha) {
         if ((alpha == 0xff) or (!p[3])) {
            p[0] = cr;
            p[1] = cg;
            p[2] = cb;
            p[3] = alpha;
         }
         else BLEND32(p,0,1,2,3,cr,cg,cb,alpha);
      }
   }

   static void cover32RGBA(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
   {
      if (cover == 255) copy32RGBA(Self, p, cr, cg, cb, alpha);
      else if (alpha) {
         alpha = (alpha * (cover + 1)) >> 8;
         if ((alpha == 0xff) or (!p[3])) {
            p[0] = cr;
            p[1] = cg;
            p[2] = cb;
            p[3] = alpha;
         }
         else BLEND32(p,0,1,2,3,cr,cg,cb,alpha);
      }
   }

   inline static void copy32AGBR(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (alpha) {
         if ((alpha == 0xff) or (!p[3])) {
            p[3] = cr;
            p[1] = cg;
            p[2] = cb;
            p[0] = alpha;
         }
         else BLEND32(p,3,1,2,0,cr,cg,cb,alpha);
      }
   }

   static void cover32AGBR(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
   {
      if (cover == 255) copy32AGBR(Self, p, cr, cg, cb, alpha);
      else if (alpha) {
         alpha = (alpha * (cover + 1)) >> 8;
         if ((alpha == 0xff) or (!p[3])) {
            p[3] = cr;
            p[1] = cg;
            p[2] = cb;
            p[0] = alpha;
         }
         else BLEND32(p,3,1,2,0,cr,cg,cb,alpha);
      }
   }

   inline static void copy32ARGB(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (alpha) {
         if ((alpha == 0xff) or (!p[3])) {
            p[1] = cr;
            p[2] = cg;
            p[3] = cb;
            p[0] = alpha;
         }
         else BLEND32(p,1,2,3,0,cr,cg,cb,alpha);
      }
   }

   static void cover32ARGB(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
   {
      if (cover == 255) copy32ARGB(Self, p, cr, cg, cb, alpha);
      else if (alpha) {
         alpha = (alpha * (cover + 1)) >> 8;
         if ((alpha == 0xff) or (!p[3])) {
            p[1] = cr;
            p[2] = cg;
            p[3] = cb;
            p[0] = alpha;
         }
         else BLEND32(p,1,2,3,0,cr,cg,cb,alpha);
      }
   }

   // Linear copy and cover operations

   inline static void linearCopy32BGRA(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (alpha) {
         if ((alpha == 0xff) or (!p[3])) {
            p[2] = cr;
            p[1] = cg;
            p[0] = cb;
            p[3] = alpha;
         }
         else LINEAR32(p,2,1,0,3,cr,cg,cb,alpha);
      }
   }

   static void linearCover32BGRA(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
   {
      if (cover == 255) {
         linearCopy32BGRA(Self, p, cr, cg, cb, alpha);
      }
      else if (alpha) {
         alpha = (alpha * (cover + 1)) >> 8;
         if ((alpha == 0xff) or (!p[3])) {
            p[2] = cr;
            p[1] = cg;
            p[0] = cb;
            p[3] = alpha;
         }
         else LINEAR32(p,2,1,0,3,cr,cg,cb,alpha);
      }
   }

   inline static void linearCopy32RGBA(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (alpha) {
         if ((alpha == 0xff) or (!p[3])) {
            p[0] = cr;
            p[1] = cg;
            p[2] = cb;
            p[3] = alpha;
         }
         else LINEAR32(p,0,1,2,3,cr,cg,cb,alpha);
      }
   }

   static void linearCover32RGBA(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
   {
      if (cover == 255) linearCopy32RGBA(Self, p, cr, cg, cb, alpha);
      else if (alpha) {
         alpha = (alpha * (cover + 1)) >> 8;
         if ((alpha == 0xff) or (!p[3])) {
            p[0] = cr;
            p[1] = cg;
            p[2] = cb;
            p[3] = alpha;
         }
         else LINEAR32(p,0,1,2,3,cr,cg,cb,alpha);
      }
   }

   inline static void linearCopy32AGBR(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (alpha) {
         if ((alpha == 0xff) or (!p[3])) {
            p[3] = cr;
            p[1] = cg;
            p[2] = cb;
            p[0] = alpha;
         }
         else LINEAR32(p,3,1,2,0,cr,cg,cb,alpha);
      }
   }

   static void linearCover32AGBR(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
   {
      if (cover == 255) linearCopy32AGBR(Self, p, cr, cg, cb, alpha);
      else if (alpha) {
         alpha = (alpha * (cover + 1)) >> 8;
         if ((alpha == 0xff) or (!p[3])) {
            p[3] = cr;
            p[1] = cg;
            p[2] = cb;
            p[0] = alpha;
         }
         else LINEAR32(p,3,1,2,0,cr,cg,cb,alpha);
      }
   }

   inline static void linearCopy32ARGB(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (alpha) {
         if ((alpha == 0xff) or (!p[3])) {
            p[1] = cr;
            p[2] = cg;
            p[3] = cb;
            p[0] = alpha;
         }
         else LINEAR32(p,1,2,3,0,cr,cg,cb,alpha);
      }
   }

   static void linearCover32ARGB(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
   {
      if (cover == 255) linearCopy32ARGB(Self, p, cr, cg, cb, alpha);
      else if (alpha) {
         alpha = (alpha * (cover + 1)) >> 8;
         if ((alpha == 0xff) or (!p[3])) {
            p[1] = cr;
            p[2] = cg;
            p[3] = cb;
            p[0] = alpha;
         }
         else LINEAR32(p,1,2,3,0,cr,cg,cb,alpha);
      }
   }

   // Generic 32-bit routines.

   static void blendHLine32(agg::pixfmt_psl *Self, int x, int y, unsigned len, const agg::rgba8 &c, int8u cover) noexcept
   {
      if (c.a) {
         UBYTE *p = Self->mData + (y * Self->mBitmap->LineWidth) + (x<<2);
         ULONG alpha = (ULONG(c.a) * (cover + 1)) >> 8;
         if (alpha == 0xff) {
            ULONG v;
            ((UBYTE *)&v)[Self->oR] = c.r;
            ((UBYTE *)&v)[Self->oG] = c.g;
            ((UBYTE *)&v)[Self->oB] = c.b;
            ((UBYTE *)&v)[Self->oA] = c.a;
            do {
               *(ULONG *)p = v;
               p += sizeof(ULONG);
            } while(--len);
         }
         else {
            do {
               Self->fBlendPix(Self, p, c.r, c.g, c.b, alpha);
               p += sizeof(ULONG);
            } while(--len);
         }
      }
   }

   static void blendSolidHSpan32(agg::pixfmt_psl *Self, int x, int y, ULONG len, const agg::rgba8 &c, const UBYTE *covers) noexcept
   {
      if (c.a) {
         UBYTE *p = Self->mData + (y * Self->mBitmap->LineWidth) + (x<<2);
         do {
            ULONG alpha = (ULONG(c.a) * (ULONG(*covers) + 1)) >> 8;
            if (alpha == 0xff) {
               p[Self->oR] = c.r;
               p[Self->oG] = c.g;
               p[Self->oB] = c.b;
               p[Self->oA] = 0xff;
            }
            else Self->fBlendPix(Self, p, c.r, c.g, c.b, alpha);
            p += sizeof(ULONG);
            ++covers;
         } while(--len);
      }
   }

   static void blendColorHSpan32(agg::pixfmt_psl *Self, int x, int y, ULONG len, const agg::rgba8 *colors, const UBYTE *covers, UBYTE cover) noexcept
   {
      UBYTE *p = Self->mData + (y * Self->mBitmap->LineWidth) + (x<<2);
      if (covers) {
         do {
            Self->fCoverPix(Self, p, colors->r, colors->g, colors->b, colors->a, *covers++);
            p += 4;
            ++colors;
         } while(--len);
      }
      else if (cover == 255) {
         do {
            Self->fCopyPix(Self, p, colors->r, colors->g, colors->b, colors->a);
            p += 4;
            ++colors;
         } while(--len);
      }
      else {
         do {
            Self->fCoverPix(Self, p, colors->r, colors->g, colors->b, colors->a, cover);
            p += 4;
            ++colors;
         } while(--len);
      }
   }

   static void copyColorHSpan32(agg::pixfmt_psl *Self, int x, int y, ULONG len, const agg::rgba8 *colors) noexcept
   {
      UBYTE *p = Self->mData + (y * Self->mBitmap->LineWidth) + (x<<2);
      do {
          p[Self->oR] = colors->r;
          p[Self->oG] = colors->g;
          p[Self->oB] = colors->b;
          p[Self->oA] = colors->a;
          ++colors;
          p += sizeof(ULONG);
      } while(--len);
   }

   // Generic 24-bit routines

   static void blendHLine24(agg::pixfmt_psl *Self, int x, int y, unsigned len, const agg::rgba8 &c, int8u cover) noexcept
   {
      if (c.a) {
         UBYTE *p = Self->mData + (y * Self->mBitmap->LineWidth) + (x / 3);
         ULONG alpha = (ULONG(c.a) * (cover + 1)) >> 8;
         if (alpha == 0xff) {
            ULONG v;
            ((UBYTE *)&v)[Self->oR] = c.r;
            ((UBYTE *)&v)[Self->oG] = c.g;
            ((UBYTE *)&v)[Self->oB] = c.b;
            do {
               *(ULONG *)p = v;
               p += 3;
            } while(--len);
         }
         else {
            do {
               Self->fBlendPix(Self, p, c.r, c.g, c.b, alpha);
               p += 3;
            } while(--len);
         }
      }
   }

   static void blendSolidHSpan24(agg::pixfmt_psl *Self, int x, int y, ULONG len, const agg::rgba8 &c, const UBYTE *covers) noexcept
   {
      if (c.a) {
         UBYTE *p = Self->mData + (y * Self->mBitmap->LineWidth) + (x / 3);
         do {
            ULONG alpha = (ULONG(c.a) * (ULONG(*covers) + 1)) >> 8;
            if (alpha == 0xff) {
               p[Self->oR] = c.r;
               p[Self->oG] = c.g;
               p[Self->oB] = c.b;
            }
            else Self->fBlendPix(Self, p, c.r, c.g, c.b, alpha);
            p += 3;
            ++covers;
         } while(--len);
      }
   }

   static void blendColorHSpan24(agg::pixfmt_psl *Self, int x, int y, ULONG len, const agg::rgba8 *colors, const UBYTE *covers, UBYTE cover) noexcept
   {
      UBYTE *p = Self->mData + (y * Self->mBitmap->LineWidth) + (x / 3);
      if (covers) {
         do {
            Self->fCoverPix(Self, p, colors->r, colors->g, colors->b, colors->a, *covers++);
            p += 3;
            ++colors;
         } while(--len);
      }
      else if (cover == 255) {
         do {
            Self->fCopyPix(Self, p, colors->r, colors->g, colors->b, colors->a);
            p += 3;
            ++colors;
         } while(--len);
      }
      else {
         do {
            Self->fCoverPix(Self, p, colors->r, colors->g, colors->b, colors->a, cover);
            p += 3;
            ++colors;
         } while(--len);
      }
   }

   static void copyColorHSpan24(agg::pixfmt_psl *Self, int x, int y, ULONG len, const agg::rgba8 *colors) noexcept
   {
      UBYTE *p = Self->mData + (y * Self->mBitmap->LineWidth) + (x / 3);
      do {
          p[Self->oR] = colors->r;
          p[Self->oG] = colors->g;
          p[Self->oB] = colors->b;
          ++colors;
          p += 3;
      } while(--len);
   }

   // Standard 24-bit routines

   static void blend24RGB(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      p[0] = ((p[0] * (0xff-alpha)) + (cr * alpha) + 0xff)>>8;
      p[1] = ((p[1] * (0xff-alpha)) + (cg * alpha) + 0xff)>>8;
      p[2] = ((p[2] * (0xff-alpha)) + (cb * alpha) + 0xff)>>8;
   }

   static void blend24BGR(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      p[2] = ((p[2] * (0xff-alpha)) + (cr * alpha) + 0xff)>>8;
      p[1] = ((p[1] * (0xff-alpha)) + (cg * alpha) + 0xff)>>8;
      p[0] = ((p[0] * (0xff-alpha)) + (cb * alpha) + 0xff)>>8;
   }

   inline static void copy24BGR(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (alpha) {
         if (alpha == 0xff) {
            p[0] = cb;
            p[1] = cg;
            p[2] = cr;
         }
         else {
            p[2] = ((p[2] * (0xff-alpha)) + (cr * alpha) + 0xff)>>8;
            p[1] = ((p[1] * (0xff-alpha)) + (cg * alpha) + 0xff)>>8;
            p[0] = ((p[0] * (0xff-alpha)) + (cb * alpha) + 0xff)>>8;
         }
      }
   }

   inline static void copy24RGB(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (alpha) {
         if (alpha == 0xff) {
            p[0] = cr;
            p[1] = cg;
            p[2] = cb;
         }
         else {
            p[0] = ((p[0] * (0xff-alpha)) + (cr * alpha) + 0xff)>>8;
            p[1] = ((p[1] * (0xff-alpha)) + (cg * alpha) + 0xff)>>8;
            p[2] = ((p[2] * (0xff-alpha)) + (cb * alpha) + 0xff)>>8;
         }
      }
   }

   static void cover24RGB(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
   {
      if (cover == 255) copy24RGB(Self, p, cr, cg, cb, alpha);
      else if (alpha) {
         alpha = (alpha * (cover + 1)) >> 8;
         if (alpha == 0xff) {
            p[0] = cr;
            p[1] = cg;
            p[2] = cb;
         }
         else {
            p[0] = ((p[0] * (0xff-alpha)) + (cr * alpha) + 0xff)>>8;
            p[1] = ((p[1] * (0xff-alpha)) + (cg * alpha) + 0xff)>>8;
            p[2] = ((p[2] * (0xff-alpha)) + (cb * alpha) + 0xff)>>8;
         }
      }
   }

   static void cover24BGR(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
   {
      if (cover == 255) copy24BGR(Self, p, cr, cg, cb, alpha);
      else if (alpha) {
         alpha = (alpha * (cover + 1)) >> 8;
         if (alpha == 0xff) {
            p[0] = cb;
            p[1] = cg;
            p[2] = cr;
         }
         else {
            p[2] = ((p[2] * (0xff-alpha)) + (cr * alpha) + 0xff)>>8;
            p[1] = ((p[1] * (0xff-alpha)) + (cg * alpha) + 0xff)>>8;
            p[0] = ((p[0] * (0xff-alpha)) + (cb * alpha) + 0xff)>>8;
         }
      }
   }

   // Standard 16-bit routines

   static void blend16(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      UWORD pixel = ((UWORD *)p)[0];
      UBYTE red   = Self->mBitmap->unpackRed(pixel);
      UBYTE green = Self->mBitmap->unpackGreen(pixel);
      UBYTE blue  = Self->mBitmap->unpackBlue(pixel);
      red   = red   + (((cr - red) * alpha)>>8);
      green = green + (((cg - green) * alpha)>>8);
      blue  = blue  + (((cb - blue) * alpha)>>8);
      ((UWORD *)p)[0] = CFPackPixel(Self->mBitmap->ColourFormat, red, green, blue);
   }

   static void copy16(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (alpha) {
         if (alpha == 0xff) ((UWORD *)p)[0] = CFPackPixel(Self->mBitmap->ColourFormat, cr, cg, cb);
         else blend16(Self, p, cr, cg, cb, alpha);
      }
   }

   static void cover16(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
   {
      if (cover == 255) copy16(Self, p, cr, cg, cb, alpha);
      else if (alpha) {
         alpha = (alpha * (cover + 1)) >> 8;
         if (alpha == 0xff) ((UWORD *)p)[0] = CFPackPixel(Self->mBitmap->ColourFormat, cr, cg, cb);
         else blend16(Self, p, cr, cg, cb, alpha);
      }
   }

   static void blendHLine16(agg::pixfmt_psl *Self, int x, int y, unsigned len, const agg::rgba8 &c, int8u cover) noexcept
   {
      if (c.a) {
         UBYTE *p = Self->mData + (y * Self->mBitmap->LineWidth) + (x<<1);
         ULONG alpha = (ULONG(c.a) * (cover + 1)) >> 8;
         if (alpha == 0xff) {
            UWORD v = CFPackPixel(Self->mBitmap->ColourFormat, c.r, c.g, c.b);
            do {
               *(UWORD *)p = v;
               p += sizeof(UWORD);
            } while(--len);
         }
         else {
            do {
               Self->fBlendPix(Self, p, c.r, c.g, c.b, alpha);
               p += sizeof(UWORD);
            } while(--len);
         }
      }
   }

   static void copyColorHSpan16(agg::pixfmt_psl *Self, int x, int y, ULONG len, const agg::rgba8 *colors) noexcept
   {
      UBYTE *p = Self->mData + (y * Self->mBitmap->LineWidth) + (x<<1);
      do {
          ((UWORD *)p)[0] = CFPackPixel(Self->mBitmap->ColourFormat, colors->r, colors->g, colors->b);
          ++colors;
          p += sizeof(UWORD);
      } while(--len);
   }

   static void blendSolidHSpan16(agg::pixfmt_psl *Self, int x, int y, ULONG len, const agg::rgba8 &c, const UBYTE *covers) noexcept
   {
      if (c.a) {
         UBYTE *p = Self->mData + (y * Self->mBitmap->LineWidth) + (x<<1);
         UWORD colour = CFPackPixel(Self->mBitmap->ColourFormat, c.r, c.g, c.b);
         do {
            ULONG alpha = (ULONG(c.a) * (ULONG(*covers) + 1)) >> 8;
            if (alpha == 0xff) *(UWORD *)p = colour;
            else Self->fBlendPix(Self, p, c.r, c.g, c.b, alpha);
            p += sizeof(UWORD);
            ++covers;
         } while(--len);
      }
   }

   static void blendColorHSpan16(agg::pixfmt_psl *Self, int x, int y, ULONG len, const agg::rgba8 *colors, const UBYTE *covers, UBYTE cover) noexcept
   {
      UBYTE *p = Self->mData + (y * Self->mBitmap->LineWidth) + (x<<1);
      if (covers) {
         do {
            Self->fCoverPix(Self, p, colors->r, colors->g, colors->b, colors->a, *covers++);
            p += sizeof(UWORD);
            ++colors;
         } while(--len);
      }
      else if (cover == 255) {
         do {
            Self->fCopyPix(Self, p, colors->r, colors->g, colors->b, colors->a);
            p += sizeof(UWORD);
            ++colors;
         } while(--len);
      }
      else {
         do {
            Self->fCoverPix(Self, p, colors->r, colors->g, colors->b, colors->a, cover);
            p += sizeof(UWORD);
            ++colors;
         } while(--len);
      }
   }

   // 16-bit BGR specific routines.

   inline static void blend16bgr(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      UWORD pixel = ((UWORD *)p)[0];
      UBYTE red   = (pixel >> 8) & 0xf8;
      UBYTE green = (pixel >> 3) & 0xf8;
      UBYTE blue  = pixel << 3;
      red   = red   + (((cr - red) * alpha)>>8);
      green = green + (((cg - green) * alpha)>>8);
      blue  = blue  + (((cb - blue) * alpha)>>8);
      ((UWORD *)p)[0] = ((red & 0xf8) << 8) | ((green & 0xfc) << 3) | (blue>>3);
   }

   inline static void copy16bgr(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (alpha == 0xff) ((UWORD *)p)[0] = ((cr & 0xf8) << 8) | ((cg & 0xfc) << 3) | (cb>>3);
      else if (alpha) blend16bgr(Self, p, cr, cg, cb, alpha);
   }

   static void cover16bgr(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
   {
      if (cover == 255) copy16bgr(Self, p, cr, cg, cb, alpha);
      else if (alpha) {
         alpha = (alpha * (cover + 1)) >> 8;
         if (alpha == 0xff) ((UWORD *)p)[0] = ((cr & 0xf8) << 8) | ((cg & 0xfc) << 3) | (cb>>3);
         else blend16bgr(Self, p, cr, cg, cb, alpha);
      }
   }

   // 16-bit RGB specific routines.

   inline static void blend16rgb(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      UWORD pixel = ((UWORD *)p)[0];
      UBYTE blue   = (pixel >> 8) & 0xf8;
      UBYTE green  = (pixel >> 3) & 0xf8;
      UBYTE red   = pixel << 3;
      red   = red   + (((cr - red) * alpha)>>8);
      green = green + (((cg - green) * alpha)>>8);
      blue  = blue  + (((cb - blue) * alpha)>>8);
      ((UWORD *)p)[0] = ((blue & 0xf8) << 8) | ((green & 0xfc) << 3) | (red>>3);
   }

   inline static void copy16rgb(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (alpha) {
         if (alpha == 0xff) ((UWORD *)p)[0] = ((cb & 0xf8) << 8) | ((cg & 0xfc) << 3) | (cr>>3);
         else blend16rgb(Self, p, cr, cg, cb, alpha);
      }
   }

   static void cover16rgb(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
   {
      if (cover == 255) copy16rgb(Self, p, cr, cg, cb, alpha);
      else if (alpha) {
         alpha = (alpha * (cover + 1)) >> 8;
         if (alpha == 0xff) ((UWORD *)p)[0] = ((cb & 0xf8) << 8) | ((cg & 0xfc) << 3) | (cr>>3);
         else blend16rgb(Self, p, cr, cg, cb, alpha);
      }
   }

public:

   inline void blend_hline(int x, int y, unsigned len, const agg::rgba8 &c, int8u cover) noexcept
   {
      fBlendHLine(this, x, y, len, c, cover);
   }

   inline void blend_solid_hspan(int x, int y, ULONG len, const agg::rgba8 &c, const UBYTE *covers) noexcept
   {
      fBlendSolidHSpan(this, x, y, len, c, covers);
   }

   inline void copy_color_hspan(int x, int y, ULONG len, const agg::rgba8 *colors) noexcept
   {
      fCopyColorHSpan(this, x, y, len, colors);
   }

   inline void blend_color_hspan(int x, int y, ULONG len, const agg::rgba8 *colors, const UBYTE *covers, UBYTE cover) noexcept
   {
      fBlendColorHSpan(this, x, y, len, colors, covers, cover);
   }

   inline void blend_color_vspan(int x, int y, ULONG len, const agg::rgba8 *colors, const UBYTE *covers, UBYTE cover) noexcept
   {
      UBYTE *p = (UBYTE *)mData + (y * mBitmap->LineWidth) + (x * mBitmap->BytesPerPixel);
      if (covers) {
         do {
            fCoverPix(this, p, colors->r, colors->g, colors->b, colors->a, *covers++);
            p += mBitmap->LineWidth;
            ++colors;
         } while(--len);
      }
      else if (cover == 255) {
         do {
            fCopyPix(this, p, colors->r, colors->g, colors->b, colors->a);
            p += mBitmap->LineWidth;
            ++colors;
         } while(--len);
      }
      else {
         do {
            fCoverPix(this, p, colors->r, colors->g, colors->b, colors->a, cover);
            p += mBitmap->LineWidth;
            ++colors;
         } while(--len);
      }
   }

public:
   UBYTE *mData;
   objBitmap *mBitmap;
   UBYTE oR, oG, oB, oA;
};

} // namespace