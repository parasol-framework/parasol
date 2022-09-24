
namespace agg {

class pixfmt_psl
{
public:
   typedef agg::rgba8 color_type;
   typedef typename agg::rendering_buffer::row_data row_data;

   pixfmt_psl() :  oR(0), oG(0), oB(0), oA(0) {}
   explicit pixfmt_psl(objBitmap &Bitmap) : oR(0), oG(0), oB(0), oA(0) {
      setBitmap(Bitmap);
   }

   void setBitmap(struct rkBitmap &Bitmap);
   void (*fBlendPix)(agg::pixfmt_psl *, UBYTE *, ULONG cr, ULONG cg, ULONG cb, ULONG alpha);
   void (*fCopyPix)(agg::pixfmt_psl *,  UBYTE *, ULONG cr, ULONG cg, ULONG cb, ULONG alpha);
   void (*fCoverPix)(agg::pixfmt_psl *, UBYTE *, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG);
   void (*fBlendHLine)(agg::pixfmt_psl *, int x, int y, unsigned len, const agg::rgba8 &c, int8u cover);
   void (*fBlendSolidHSpan)(agg::pixfmt_psl *, int x, int y, ULONG len, const agg::rgba8 &c, const UBYTE *covers);
   void (*fBlendColorHSpan)(agg::pixfmt_psl *, int x, int y, ULONG len, const agg::rgba8 *colors, const UBYTE *covers, UBYTE cover);
   void (*fCopyColorHSpan)(agg::pixfmt_psl *, int x, int y, ULONG len, const agg::rgba8 *colors);

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
      if (p[3]) {
         BLEND32(p,2,1,0,3,cr,cg,cb,alpha);
      }
      else {
         p[2] = cr;
         p[1] = cg;
         p[0] = cb;
         p[3] = alpha;
      }
   }

   static void blend32RGBA(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (p[3]) {
         BLEND32(p,0,1,2,3,cr,cg,cb,alpha);
      }
      else {
         p[0] = cr;
         p[1] = cg;
         p[2] = cb;
         p[3] = alpha;
      }
   }

   static void blend32AGBR(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (p[0]) {
         BLEND32(p,3,1,2,0,cr,cg,cb,alpha);
      }
      else {
         p[3] = cr;
         p[1] = cg;
         p[2] = cb;
         p[0] = alpha;
      }
   }

   static void blend32ARGB(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (p[0]) {
         BLEND32(p,1,2,3,0,cr,cg,cb,alpha);
      }
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
         else {
            BLEND32(p,2,1,0,3,cr,cg,cb,alpha);
         }
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
         else {
            BLEND32(p,2,1,0,3,cr,cg,cb,alpha);
         }
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
         else {
            BLEND32(p,0,1,2,3,cr,cg,cb,alpha);
         }
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
         else {
            BLEND32(p,0,1,2,3,cr,cg,cb,alpha);
         }
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
         else {
            BLEND32(p,3,1,2,0,cr,cg,cb,alpha);
         }
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
         else {
            BLEND32(p,3,1,2,0,cr,cg,cb,alpha);
         }
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
         else {
            BLEND32(p,1,2,3,0,cr,cg,cb,alpha);
         }
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
         else {
            BLEND32(p,1,2,3,0,cr,cg,cb,alpha);
         }
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

   // --- Generic 24-bit routines

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

   // --- Standard 24-bit routines

   static void blend24RGB(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      p[0] = (UBYTE)(((cr - p[0]) * alpha + (p[0] << 8)) >> 8);
      p[1] = (UBYTE)(((cg - p[1]) * alpha + (p[1] << 8)) >> 8);
      p[2] = (UBYTE)(((cb - p[2]) * alpha + (p[2] << 8)) >> 8);
   }

   static void blend24BGR(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      p[2] = (UBYTE)(((cr - p[2]) * alpha + (p[2] << 8)) >> 8);
      p[1] = (UBYTE)(((cg - p[1]) * alpha + (p[1] << 8)) >> 8);
      p[0] = (UBYTE)(((cb - p[0]) * alpha + (p[0] << 8)) >> 8);
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
            p[0] = (UBYTE)(((cb - p[0]) * alpha + (p[0] << 8)) >> 8);
            p[1] = (UBYTE)(((cg - p[1]) * alpha + (p[1] << 8)) >> 8);
            p[2] = (UBYTE)(((cr - p[2]) * alpha + (p[2] << 8)) >> 8);
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
            p[0] = (UBYTE)(((cr - p[0]) * alpha + (p[0] << 8)) >> 8);
            p[1] = (UBYTE)(((cg - p[1]) * alpha + (p[1] << 8)) >> 8);
            p[2] = (UBYTE)(((cb - p[2]) * alpha + (p[2] << 8)) >> 8);
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
            p[0] = (UBYTE)(((cr - p[0]) * alpha + (p[0] << 8)) >> 8);
            p[1] = (UBYTE)(((cg - p[1]) * alpha + (p[1] << 8)) >> 8);
            p[2] = (UBYTE)(((cb - p[2]) * alpha + (p[2] << 8)) >> 8);
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
            p[0] = (UBYTE)(((cb - p[0]) * alpha + (p[0] << 8)) >> 8);
            p[1] = (UBYTE)(((cg - p[1]) * alpha + (p[1] << 8)) >> 8);
            p[2] = (UBYTE)(((cr - p[2]) * alpha + (p[2] << 8)) >> 8);
         }
      }
   }


   // --- Standard 16-bit routines

   static void blend16(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      UWORD pixel = ((UWORD *)p)[0];
      UBYTE red   = UnpackRed(Self->mBitmap, pixel);
      UBYTE green = UnpackGreen(Self->mBitmap, pixel);
      UBYTE blue  = UnpackBlue(Self->mBitmap, pixel);
      red   = red + (((cr - red) * alpha)>>8);
      green = green + (((cg - green) * alpha)>>8);
      blue  = blue + (((cb - blue) * alpha)>>8);
      ((UWORD *)p)[0] = CFPackPixel(Self->mBitmap->ColourFormat, red, green, blue);
   }

   static void copy16(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (alpha) {
         if (alpha == 0xff) {
            ((UWORD *)p)[0] = CFPackPixel(Self->mBitmap->ColourFormat, cr, cg, cb);
         }
         else blend16(Self, p, cr, cg, cb, alpha);
      }
   }

   static void cover16(agg::pixfmt_psl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
   {
      if (cover == 255) copy16(Self, p, cr, cg, cb, alpha);
      else if (alpha) {
         alpha = (alpha * (cover + 1)) >> 8;
         if (alpha == 0xff) {
            ((UWORD *)p)[0] = CFPackPixel(Self->mBitmap->ColourFormat, cr, cg, cb);
         }
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
      red   = red + (((cr - red) * alpha)>>8);
      green = green + (((cg - green) * alpha)>>8);
      blue  = blue + (((cb - blue) * alpha)>>8);
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
      red   = red + (((cr - red) * alpha)>>8);
      green = green + (((cg - green) * alpha)>>8);
      blue  = blue + (((cb - blue) * alpha)>>8);
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
   struct rkBitmap *mBitmap;
   UBYTE oR, oG, oB, oA;
};

} // namespace