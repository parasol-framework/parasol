
INLINE void BLEND32(UBYTE *p, UBYTE r, UBYTE g, UBYTE b, UBYTE a, UBYTE cr, UBYTE cg, UBYTE cb, UBYTE ca)
{
   p[r] = p[r] + (((cr - p[r]) * ca)>>8);
   p[g] = p[g] + (((cg - p[g]) * ca)>>8);
   p[b] = p[b] + (((cb - p[b]) * ca)>>8);
   p[a] = p[a] + ((ca * (255-p[a]))>>8);
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

//****************************************************************************
// These functions convert bitmaps between linear and RGB format with a pre-calculated gamma table.

static agg::gamma_lut<UBYTE, UWORD, 8, 12> glGamma(2.2);

static void rgb2linear(objBitmap &Bitmap)
{
   if (Bitmap.BytesPerPixel < 4) return;

   const UBYTE R = Bitmap.ColourFormat->RedPos>>3;
   const UBYTE G = Bitmap.ColourFormat->GreenPos>>3;
   const UBYTE B = Bitmap.ColourFormat->BluePos>>3;
   const UBYTE A = Bitmap.ColourFormat->AlphaPos>>3;

   UBYTE *start_y = Bitmap.Data + (Bitmap.LineWidth * Bitmap.Clip.Top) + (Bitmap.Clip.Left * Bitmap.BytesPerPixel);
   for (LONG y=Bitmap.Clip.Top; y < Bitmap.Clip.Bottom; y++) {
      UBYTE *pixel = start_y;
      for (LONG x=Bitmap.Clip.Left; x < Bitmap.Clip.Right; x++) {
         // The normal formula with no lookup is as follows.  The GammaRatio is '2.2' or '1.0/2.2' depending
         // on the direction of the conversion.
         // ULONG r = fastPow((DOUBLE)data[R] * (1.0 / 255.0), GammaRatio) * 255.0;
         // ULONG g = fastPow((DOUBLE)data[G] * (1.0 / 255.0), GammaRatio) * 255.0;
         // ULONG b = fastPow((DOUBLE)data[B] * (1.0 / 255.0), GammaRatio) * 255.0;

         if (pixel[A]) {
            pixel[R] = glGamma.dir(pixel[R])>>4;
            pixel[G] = glGamma.dir(pixel[G])>>4;
            pixel[B] = glGamma.dir(pixel[B])>>4;
         }
         pixel += Bitmap.BytesPerPixel;
      }
      start_y += Bitmap.LineWidth;
   }
}

static void linear2RGB(objBitmap &Bitmap)
{
   if (Bitmap.BytesPerPixel < 4) return;

   const UBYTE R = Bitmap.ColourFormat->RedPos>>3;
   const UBYTE G = Bitmap.ColourFormat->GreenPos>>3;
   const UBYTE B = Bitmap.ColourFormat->BluePos>>3;
   const UBYTE A = Bitmap.ColourFormat->AlphaPos>>3;

   UBYTE *start_y = Bitmap.Data + (Bitmap.LineWidth * Bitmap.Clip.Top) + (Bitmap.Clip.Left * Bitmap.BytesPerPixel);
   for (LONG y=Bitmap.Clip.Top; y < Bitmap.Clip.Bottom; y++) {
      UBYTE *pixel = start_y;
      for (LONG x=Bitmap.Clip.Left; x < Bitmap.Clip.Right; x++) {
         if (pixel[A]) {
            pixel[R] = glGamma.inv(pixel[R]<<4);
            pixel[G] = glGamma.inv(pixel[G]<<4);
            pixel[B] = glGamma.inv(pixel[B]<<4);
         }
         pixel += Bitmap.BytesPerPixel;
      }
      start_y += Bitmap.LineWidth;
   }
}

namespace agg {

class pixfmt_rkl
{
public:
   typedef agg::rgba8 color_type;
   typedef typename agg::rendering_buffer::row_data row_data;

   pixfmt_rkl() :  oR(0), oG(0), oB(0), oA(0) {}
   explicit pixfmt_rkl(objBitmap &Bitmap) : oR(0), oG(0), oB(0), oA(0) {
      setBitmap(Bitmap);
   }

   void setBitmap(struct rkBitmap &Bitmap) {
      mBitmap = &Bitmap;

      mData = Bitmap.Data + (Bitmap.XOffset * Bitmap.BytesPerPixel) + (Bitmap.YOffset * Bitmap.LineWidth);

      if (Bitmap.BitsPerPixel IS 32) {
         fBlendHLine      = &blendHLine32;
         fBlendSolidHSpan = &blendSolidHSpan32;
         fBlendColorHSpan = &blendColorHSpan32;
         fCopyColorHSpan  = &copyColorHSpan32;

         if (Bitmap.ColourFormat->AlphaPos IS 24) {
            if (Bitmap.ColourFormat->BluePos IS 0) {
               pixel_order(2, 1, 0, 3); // BGRA
               fBlendPix = &blend32BGRA;
               fCopyPix  = &copy32BGRA;
               fCoverPix = &cover32BGRA;
            }
            else {
               pixel_order(0, 1, 2, 3); // RGBA
               fBlendPix = &blend32RGBA;
               fCopyPix  = &copy32RGBA;
               fCoverPix = &cover32RGBA;
            }
         }
         else if (Bitmap.ColourFormat->RedPos IS 24) {
            pixel_order(3, 1, 2, 0); // AGBR
            fBlendPix = &blend32AGBR;
            fCopyPix  = &copy32AGBR;
            fCoverPix = &cover32AGBR;
         }
         else {
            pixel_order(1, 2, 3, 0); // ARGB
            fBlendPix = &blend32ARGB;
            fCopyPix  = &copy32ARGB;
            fCoverPix = &cover32ARGB;
         }
      }
      else if (Bitmap.BitsPerPixel IS 24) {
         fBlendHLine      = &blendHLine24;
         fBlendSolidHSpan = &blendSolidHSpan24;
         fBlendColorHSpan = &blendColorHSpan24;
         fCopyColorHSpan  = &copyColorHSpan24;

         if (Bitmap.ColourFormat->BluePos IS 0) {
            pixel_order(2, 1, 0, 0); // BGR
            fBlendPix = &blend24BGR;
            fCopyPix  = &copy24BGR;
            fCoverPix = &cover24BGR;
         }
         else {
            pixel_order(0, 1, 2, 0); // RGB
            fBlendPix = &blend24RGB;
            fCopyPix  = &copy24RGB;
            fCoverPix = &cover24RGB;
         }
      }
      else if (Bitmap.BitsPerPixel IS 16) {
         fBlendHLine      = &blendHLine16;
         fBlendSolidHSpan = &blendSolidHSpan16;
         fBlendColorHSpan = &blendColorHSpan16;
         fCopyColorHSpan  = &copyColorHSpan16;

         if ((Bitmap.ColourFormat->BluePos IS 0) and (Bitmap.ColourFormat->RedPos IS 11)) { // BGR
            fBlendPix = &blend16bgr;
            fCopyPix  = &copy16bgr;
            fCoverPix = &cover16bgr;
         }
         else if ((Bitmap.ColourFormat->RedPos IS 0) and (Bitmap.ColourFormat->BluePos IS 11)) { // RGB
            fBlendPix = &blend16rgb;
            fCopyPix  = &copy16rgb;
            fCoverPix = &cover16rgb;
         }
         else { // RGB
            fBlendPix = &blend16;
            fCopyPix  = &copy16;
            fCoverPix = &cover16;
         }
      }
   }

   void (*fBlendPix)(agg::pixfmt_rkl *, UBYTE *, ULONG cr, ULONG cg, ULONG cb, ULONG alpha);
   void (*fCopyPix)(agg::pixfmt_rkl *,  UBYTE *, ULONG cr, ULONG cg, ULONG cb, ULONG alpha);
   void (*fCoverPix)(agg::pixfmt_rkl *, UBYTE *, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG);
   void (*fBlendHLine)(agg::pixfmt_rkl *, int x, int y, unsigned len, const agg::rgba8 &c, int8u cover);
   void (*fBlendSolidHSpan)(agg::pixfmt_rkl *, int x, int y, ULONG len, const agg::rgba8 &c, const UBYTE *covers);
   void (*fBlendColorHSpan)(agg::pixfmt_rkl *, int x, int y, ULONG len, const agg::rgba8 *colors, const UBYTE *covers, UBYTE cover);
   void (*fCopyColorHSpan)(agg::pixfmt_rkl *, int x, int y, ULONG len, const agg::rgba8 *colors);

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

   static void blend32BGRA(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
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

   static void blend32RGBA(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
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

   static void blend32AGBR(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
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

   static void blend32ARGB(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
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

   inline static void copy32BGRA(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
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

   static void cover32BGRA(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
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

   inline static void copy32RGBA(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
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

   static void cover32RGBA(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
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

   inline static void copy32AGBR(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
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

   static void cover32AGBR(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
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

   inline static void copy32ARGB(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
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

   static void cover32ARGB(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
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

   static void blendHLine32(agg::pixfmt_rkl *Self, int x, int y, unsigned len, const agg::rgba8 &c, int8u cover) noexcept
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

   static void blendSolidHSpan32(agg::pixfmt_rkl *Self, int x, int y, ULONG len, const agg::rgba8 &c, const UBYTE *covers) noexcept
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

   static void blendColorHSpan32(agg::pixfmt_rkl *Self, int x, int y, ULONG len, const agg::rgba8 *colors, const UBYTE *covers, UBYTE cover) noexcept
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

   static void copyColorHSpan32(agg::pixfmt_rkl *Self, int x, int y, ULONG len, const agg::rgba8 *colors) noexcept
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

   static void blendHLine24(agg::pixfmt_rkl *Self, int x, int y, unsigned len, const agg::rgba8 &c, int8u cover) noexcept
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

   static void blendSolidHSpan24(agg::pixfmt_rkl *Self, int x, int y, ULONG len, const agg::rgba8 &c, const UBYTE *covers) noexcept
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

   static void blendColorHSpan24(agg::pixfmt_rkl *Self, int x, int y, ULONG len, const agg::rgba8 *colors, const UBYTE *covers, UBYTE cover) noexcept
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

   static void copyColorHSpan24(agg::pixfmt_rkl *Self, int x, int y, ULONG len, const agg::rgba8 *colors) noexcept
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

   static void blend24RGB(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      p[0] = (UBYTE)(((cr - p[0]) * alpha + (p[0] << 8)) >> 8);
      p[1] = (UBYTE)(((cg - p[1]) * alpha + (p[1] << 8)) >> 8);
      p[2] = (UBYTE)(((cb - p[2]) * alpha + (p[2] << 8)) >> 8);
   }

   static void blend24BGR(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      p[2] = (UBYTE)(((cr - p[2]) * alpha + (p[2] << 8)) >> 8);
      p[1] = (UBYTE)(((cg - p[1]) * alpha + (p[1] << 8)) >> 8);
      p[0] = (UBYTE)(((cb - p[0]) * alpha + (p[0] << 8)) >> 8);
   }

   inline static void copy24BGR(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
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

   inline static void copy24RGB(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
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

   static void cover24RGB(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
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

   static void cover24BGR(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
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

   static void blend16(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
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

   static void copy16(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (alpha) {
         if (alpha == 0xff) {
            ((UWORD *)p)[0] = CFPackPixel(Self->mBitmap->ColourFormat, cr, cg, cb);
         }
         else blend16(Self, p, cr, cg, cb, alpha);
      }
   }

   static void cover16(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
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

   static void blendHLine16(agg::pixfmt_rkl *Self, int x, int y, unsigned len, const agg::rgba8 &c, int8u cover) noexcept
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

   static void copyColorHSpan16(agg::pixfmt_rkl *Self, int x, int y, ULONG len, const agg::rgba8 *colors) noexcept
   {
      UBYTE *p = Self->mData + (y * Self->mBitmap->LineWidth) + (x<<1);
      do {
          ((UWORD *)p)[0] = CFPackPixel(Self->mBitmap->ColourFormat, colors->r, colors->g, colors->b);
          ++colors;
          p += sizeof(UWORD);
      } while(--len);
   }

   static void blendSolidHSpan16(agg::pixfmt_rkl *Self, int x, int y, ULONG len, const agg::rgba8 &c, const UBYTE *covers) noexcept
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

   static void blendColorHSpan16(agg::pixfmt_rkl *Self, int x, int y, ULONG len, const agg::rgba8 *colors, const UBYTE *covers, UBYTE cover) noexcept
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

   inline static void blend16bgr(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
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

   inline static void copy16bgr(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (alpha == 0xff) ((UWORD *)p)[0] = ((cr & 0xf8) << 8) | ((cg & 0xfc) << 3) | (cb>>3);
      else if (alpha) blend16bgr(Self, p, cr, cg, cb, alpha);
   }

   static void cover16bgr(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
   {
      if (cover == 255) copy16bgr(Self, p, cr, cg, cb, alpha);
      else if (alpha) {
         alpha = (alpha * (cover + 1)) >> 8;
         if (alpha == 0xff) ((UWORD *)p)[0] = ((cr & 0xf8) << 8) | ((cg & 0xfc) << 3) | (cb>>3);
         else blend16bgr(Self, p, cr, cg, cb, alpha);
      }
   }

   // 16-bit RGB specific routines.

   inline static void blend16rgb(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
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

   inline static void copy16rgb(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha) noexcept
   {
      if (alpha) {
         if (alpha == 0xff) ((UWORD *)p)[0] = ((cb & 0xf8) << 8) | ((cg & 0xfc) << 3) | (cr>>3);
         else blend16rgb(Self, p, cr, cg, cb, alpha);
      }
   }

   static void cover16rgb(agg::pixfmt_rkl *Self, UBYTE *p, ULONG cr, ULONG cg, ULONG cb, ULONG alpha, ULONG cover) noexcept
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

//****************************************************************************

class span_reflect_y
{
private:
   span_reflect_y();
public:
   typedef typename agg::rgba8::value_type value_type;
   typedef agg::rgba8 color_type;

   span_reflect_y(agg::pixfmt_rkl & pixf, unsigned offset_x, unsigned offset_y) :
       m_src(&pixf),
       m_wrap_x(pixf.mBitmap->Width),
       m_wrap_y(pixf.mBitmap->Height),
       m_offset_x(offset_x),
       m_offset_y(offset_y)
   {
      m_bk_buf[0] = m_bk_buf[1] = m_bk_buf[2] = m_bk_buf[3] = 0;
   }

   void prepare() {}

   void generate(agg::rgba8 *s, int x, int y, unsigned len)
   {
      x += m_offset_x;
      y += m_offset_y;
      const value_type* p = (const value_type*)span(x, y, len);
      do {
         s->r = p[m_src->oR];
         s->g = p[m_src->oG];
         s->b = p[m_src->oB];
         s->a = p[m_src->oA];
         p = (const value_type*)next_x();
         ++s;
      } while(--len);
   }

   int8u* span(int x, int y, unsigned)
   {
       m_x = x;
       m_row_ptr = m_src->row_ptr(m_wrap_y(y));
       return m_row_ptr + m_wrap_x(x) * 4;
   }

   int8u* next_x()
   {
       int x = ++m_wrap_x;
       return m_row_ptr + x * 4;
   }

   int8u* next_y()
   {
       m_row_ptr = m_src->row_ptr(++m_wrap_y);
       return m_row_ptr + m_wrap_x(m_x) * 4;
   }

   agg::pixfmt_rkl *m_src;

private:
   wrap_mode_repeat_auto_pow2 m_wrap_x;
   wrap_mode_reflect_auto_pow2 m_wrap_y;
   UBYTE *m_row_ptr;
   unsigned m_offset_x;
   unsigned m_offset_y;
   UBYTE m_bk_buf[4];
   int m_x;
};

//****************************************************************************

class span_reflect_x
{
private:
   span_reflect_x();
public:
   typedef typename agg::rgba8::value_type value_type;
   typedef agg::rgba8 color_type;

   span_reflect_x(agg::pixfmt_rkl & pixf, unsigned offset_x, unsigned offset_y) :
       m_src(&pixf),
       m_wrap_x(pixf.mBitmap->Width),
       m_wrap_y(pixf.mBitmap->Height),
       m_offset_x(offset_x),
       m_offset_y(offset_y)
   {
      m_bk_buf[0] = m_bk_buf[1] = m_bk_buf[2] = m_bk_buf[3] = 0;
   }

   void prepare() {}

   void generate(agg::rgba8 *s, int x, int y, unsigned len)
   {
      x += m_offset_x;
      y += m_offset_y;
      const value_type* p = (const value_type*)span(x, y, len);
      do {
         s->r = p[m_src->oR];
         s->g = p[m_src->oG];
         s->b = p[m_src->oB];
         s->a = p[m_src->oA];
         p = (const value_type*)next_x();
         ++s;
      } while(--len);
   }

  int8u* span(int x, int y, unsigned)
  {
      m_x = x;
      m_row_ptr = m_src->row_ptr(m_wrap_y(y));
      return m_row_ptr + m_wrap_x(x) * 4;
  }

  int8u* next_x()
  {
      int x = ++m_wrap_x;
      return m_row_ptr + x * 4;
  }

  int8u* next_y()
  {
      m_row_ptr = m_src->row_ptr(++m_wrap_y);
      return m_row_ptr + m_wrap_x(m_x) * 4;
  }

   agg::pixfmt_rkl *m_src;

private:
   wrap_mode_reflect_auto_pow2 m_wrap_x;
   wrap_mode_repeat_auto_pow2 m_wrap_y;
   UBYTE *m_row_ptr;
   unsigned m_offset_x;
   unsigned m_offset_y;
   UBYTE m_bk_buf[4];
   int m_x;
};

//****************************************************************************

class span_repeat_rkl
{
private:
   span_repeat_rkl();
public:
   typedef typename agg::rgba8::value_type value_type;
   typedef agg::rgba8 color_type;

   span_repeat_rkl(agg::pixfmt_rkl & pixf, unsigned offset_x, unsigned offset_y) :
       m_src(&pixf),
       m_wrap_x(pixf.mBitmap->Width),
       m_wrap_y(pixf.mBitmap->Height),
       m_offset_x(offset_x),
       m_offset_y(offset_y)
   {
      m_bk_buf[0] = m_bk_buf[1] = m_bk_buf[2] = m_bk_buf[3] = 0;
   }

   void prepare() {}

   void generate(agg::rgba8 *s, int x, int y, unsigned len)
   {
      x += m_offset_x;
      y += m_offset_y;
      const value_type* p = (const value_type*)span(x, y, len);
      do {
         s->r = p[m_src->oR];
         s->g = p[m_src->oG];
         s->b = p[m_src->oB];
         s->a = p[m_src->oA];
         p = (const value_type*)next_x();
         ++s;
      } while(--len);
   }

  int8u* span(int x, int y, unsigned)
  {
      m_x = x;
      m_row_ptr = m_src->row_ptr(m_wrap_y(y));
      return m_row_ptr + m_wrap_x(x) * 4;
  }

  int8u* next_x()
  {
      int x = ++m_wrap_x;
      return m_row_ptr + x * 4;
  }

  int8u* next_y()
  {
      m_row_ptr = m_src->row_ptr(++m_wrap_y);
      return m_row_ptr + m_wrap_x(m_x) * 4;
  }

  agg::pixfmt_rkl *m_src;

private:
   wrap_mode_repeat_auto_pow2 m_wrap_x;
   wrap_mode_repeat_auto_pow2 m_wrap_y;
   UBYTE *m_row_ptr;
   unsigned m_offset_x;
   unsigned m_offset_y;
   UBYTE m_bk_buf[4];
   int m_x;
};

//****************************************************************************
// This class is used for clipped images (no tiling)

template<class Source> class span_pattern_rkl // Based on span_pattern_rgba
{
private:
   span_pattern_rkl() {}
public:
   typedef typename agg::rgba8::value_type value_type;
   typedef agg::rgba8 color_type;

   span_pattern_rkl(Source & src, unsigned offset_x, unsigned offset_y) :
       m_src(&src),
       m_offset_x(offset_x),
       m_offset_y(offset_y)
   {
      m_bk_buf[0] = m_bk_buf[1] = m_bk_buf[2] = m_bk_buf[3] = 0;
   }

   void prepare() {}

   void generate(agg::rgba8 *s, int x, int y, unsigned len)
   {
      x += m_offset_x;
      y += m_offset_y;
      const value_type* p = (const value_type*)span(x, y, len);
      do {
         s->r = p[m_src->oR];
         s->g = p[m_src->oG];
         s->b = p[m_src->oB];
         s->a = p[m_src->oA];
         p = (const value_type*)next_x();
         ++s;
      } while(--len);
   }

    int8u* span(int x, int y, unsigned len)
   {
      m_x = m_x0 = x;
      m_y = y;
      if ((y >= 0) and (y < (int)m_src->mBitmap->Clip.Bottom) and (x >= 0) and (x+(int)len <= (int)m_src->mBitmap->Clip.Right)) {
         return m_pix_ptr = m_src->row_ptr(y) + (x * m_src->mBitmap->BytesPerPixel);
      }
      m_pix_ptr = 0;
      if ((m_y >= 0) and (m_y < (int)m_src->mBitmap->Clip.Bottom) and (m_x >= 0) and (m_x < (int)m_src->mBitmap->Clip.Right)) {
         return m_src->row_ptr(m_y) + (m_x * m_src->mBitmap->BytesPerPixel);
      }
      return m_bk_buf;
   }

   int8u* next_x()
   {
      if (m_pix_ptr) return m_pix_ptr += m_src->mBitmap->BytesPerPixel;
      ++m_x;
      if ((m_y >= 0) and (m_y < (int)m_src->mBitmap->Clip.Bottom) and (m_x >= 0) and (m_x < (int)m_src->mBitmap->Clip.Right)) {
         return m_src->row_ptr(m_y) + (m_x * m_src->mBitmap->BytesPerPixel);
      }
      return m_bk_buf;
  }

   int8u* next_y()
   {
      ++m_y;
      m_x = m_x0;
      if (m_pix_ptr and m_y >= 0 and m_y < (int)m_src->height()) {
         return m_pix_ptr = m_src->row_ptr(m_y) + (m_x * m_src->mBitmap->BytesPerPixel);
      }
      m_pix_ptr = 0;
      if ((m_y >= 0) and (m_y < (int)m_src->mBitmap->Clip.Bottom) and (m_x >= 0) and (m_x < (int)m_src->mBitmap->Clip.Right)) {
         return m_src->row_ptr(m_y) + (m_x * m_src->mBitmap->BytesPerPixel);
      }
      return m_bk_buf;
   }
   Source *m_src;

private:
   unsigned m_offset_x;
   unsigned m_offset_y;
   UBYTE m_bk_buf[4];
   int m_x, m_x0, m_y;
   UBYTE *m_pix_ptr;
};

}
