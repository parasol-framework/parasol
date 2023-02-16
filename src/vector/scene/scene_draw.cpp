//********************************************************************************************************************
// This class holds the current state as the vector scene is parsed for drawing.  It is most useful for managing use of
// the 'inherit' attribute values.

#include "agg_span_gradient_contour.h"

class VectorState
{
public:
   struct vsclip {
      double x1, y1, x2, y2;
      vsclip(double a1, double a2, double a3, double a4) : x1(a1), y1(a2), x2(a3), y2(a4) { };
   } mClip; // Current clip region as defined by the viewports
   agg::line_join_e  mLineJoin;
   agg::line_cap_e   mLineCap;
   agg::inner_join_e mInnerJoin;
   double mOpacity;
   bool mDirty;
   bool mApplyTransform;
   UBYTE mVisible;
   UBYTE mOverflowX;
   UBYTE mOverflowY;
   extVectorClip *mClipMask;
   agg::trans_affine mTransform;
   bool mLinearRGB;
   bool mBackgroundActive;

   VectorState() :
      mClip(0, 0, DBL_MAX, DBL_MAX),
      mLineJoin(agg::miter_join),
      mLineCap(agg::butt_cap),
      mInnerJoin(agg::inner_miter),
      mOpacity(1.0),
      mDirty(false),
      mApplyTransform(false),
      mVisible(VIS_VISIBLE),
      mOverflowX(VOF_VISIBLE),
      mOverflowY(VOF_VISIBLE),
      mClipMask(NULL),
      mLinearRGB(false),
      mBackgroundActive(false)
      { }
};

//********************************************************************************************************************

namespace agg {
class span_reflect_y
{
private:
   span_reflect_y();
public:
   typedef typename agg::rgba8::value_type value_type;
   typedef agg::rgba8 color_type;

   span_reflect_y(agg::pixfmt_psl & pixf, unsigned offset_x, unsigned offset_y) :
       m_src(&pixf),
       m_wrap_x(pixf.mWidth),
       m_wrap_y(pixf.mHeight),
       m_offset_x(offset_x),
       m_offset_y(offset_y)
   {
      m_bk_buf[0] = m_bk_buf[1] = m_bk_buf[2] = m_bk_buf[3] = 0;
   }

   void prepare() {}

   void generate(agg::rgba8 *s, int x, int y, unsigned len) {
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

   int8u* span(int x, int y, unsigned) {
       m_x = x;
       m_row_ptr = m_src->row_ptr(m_wrap_y(y));
       return m_row_ptr + m_wrap_x(x) * 4;
   }

   int8u* next_x() {
       int x = ++m_wrap_x;
       return m_row_ptr + x * 4;
   }

   int8u* next_y() {
       m_row_ptr = m_src->row_ptr(++m_wrap_y);
       return m_row_ptr + m_wrap_x(m_x) * 4;
   }

   agg::pixfmt_psl *m_src;

private:
   wrap_mode_repeat_auto_pow2 m_wrap_x;
   wrap_mode_reflect_auto_pow2 m_wrap_y;
   UBYTE *m_row_ptr;
   unsigned m_offset_x;
   unsigned m_offset_y;
   UBYTE m_bk_buf[4];
   int m_x;
};

//********************************************************************************************************************

class span_reflect_x
{
private:
   span_reflect_x();
public:
   typedef typename agg::rgba8::value_type value_type;
   typedef agg::rgba8 color_type;

   span_reflect_x(agg::pixfmt_psl & pixf, unsigned offset_x, unsigned offset_y) :
       m_src(&pixf),
       m_wrap_x(pixf.mWidth),
       m_wrap_y(pixf.mHeight),
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

   agg::pixfmt_psl *m_src;

private:
   wrap_mode_reflect_auto_pow2 m_wrap_x;
   wrap_mode_repeat_auto_pow2 m_wrap_y;
   UBYTE *m_row_ptr;
   unsigned m_offset_x;
   unsigned m_offset_y;
   UBYTE m_bk_buf[4];
   int m_x;
};

//********************************************************************************************************************

class span_repeat_rkl
{
private:
   span_repeat_rkl();
public:
   typedef typename agg::rgba8::value_type value_type;
   typedef agg::rgba8 color_type;

   span_repeat_rkl(agg::pixfmt_psl & pixf, unsigned offset_x, unsigned offset_y) :
       m_src(&pixf),
       m_wrap_x(pixf.mWidth),
       m_wrap_y(pixf.mHeight),
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

  agg::pixfmt_psl *m_src;

private:
   wrap_mode_repeat_auto_pow2 m_wrap_x;
   wrap_mode_repeat_auto_pow2 m_wrap_y;
   UBYTE *m_row_ptr;
   unsigned m_offset_x;
   unsigned m_offset_y;
   UBYTE m_bk_buf[4];
   int m_x;
};
} // namespace

//********************************************************************************************************************

static bool check_dirty(extVector *Shape) {
   while (Shape) {
      if (Shape->ClassID != ID_VECTOR) return true;
      if (Shape->Dirty) return true;

      if (Shape->Child) {
         if (check_dirty((extVector *)Shape->Child)) return true;
      }
      Shape = (extVector *)Shape->Next;
   }
   return false;
}

//********************************************************************************************************************
// Return the correct transformation matrix for a fill operation.

static const agg::trans_affine build_fill_transform(extVector &Vector, bool Userspace,  VectorState &State)
{
   if (Userspace) { // Userspace: The vector's (x,y) position is ignored, but its transforms and all parent transforms will apply.
      agg::trans_affine transform;
      apply_transforms(Vector, transform);
      apply_parent_transforms(get_parent(&Vector), transform);
      return transform;
   }
   else if (State.mApplyTransform) { // BoundingBox with a real-time transform
      agg::trans_affine transform = Vector.Transform * State.mTransform;
      return transform;
   }
   else return Vector.Transform; // Default BoundingBox: The vector's position, transforms, and parent transforms apply.
}

//********************************************************************************************************************

void setRasterClip(agg::rasterizer_scanline_aa<> &Raster, LONG X, LONG Y, LONG Width, LONG Height)
{
   if (Width < 0) Width = 0;
   if (Height < 0) Height = 0;

   agg::path_storage clip;
   clip.move_to(X, Y);
   clip.line_to(X+Width, Y);
   clip.line_to(X+Width, Y+Height);
   clip.line_to(X, Y+Height);
   clip.close_polygon();
   Raster.reset();
   Raster.add_path(clip);
}

//********************************************************************************************************************

void set_filter(agg::image_filter_lut &Filter, UBYTE Method)
{
   switch(Method) {
      case VSM_AUTO:
      case VSM_NEIGHBOUR: // There is a 'span_image_filter_rgb_nn' class but no equivalent image_filter_neighbour() routine?
      case VSM_BILINEAR:  Filter.calculate(agg::image_filter_bilinear(), true); break;
      case VSM_BICUBIC:   Filter.calculate(agg::image_filter_bicubic(), true); break;
      case VSM_SPLINE16:  Filter.calculate(agg::image_filter_spline16(), true); break;
      case VSM_KAISER:    Filter.calculate(agg::image_filter_kaiser(), true); break;
      case VSM_QUADRIC:   Filter.calculate(agg::image_filter_quadric(), true); break;
      case VSM_GAUSSIAN:  Filter.calculate(agg::image_filter_gaussian(), true); break;
      case VSM_BESSEL:    Filter.calculate(agg::image_filter_bessel(), true); break;
      case VSM_MITCHELL:  Filter.calculate(agg::image_filter_mitchell(), true); break;
      case VSM_SINC3:     Filter.calculate(agg::image_filter_sinc(3.0), true); break;
      case VSM_LANCZOS3:  Filter.calculate(agg::image_filter_lanczos(3.0), true); break;
      case VSM_BLACKMAN3: Filter.calculate(agg::image_filter_blackman(3.0), true); break;
      case VSM_SINC8:     Filter.calculate(agg::image_filter_sinc(8.0), true); break;
      case VSM_LANCZOS8:  Filter.calculate(agg::image_filter_lanczos(8.0), true); break;
      case VSM_BLACKMAN8: Filter.calculate(agg::image_filter_blackman(8.0), true); break;
      default: {
         parasol::Log log;
         log.warning("Unrecognised sampling method %d", Method);
         Filter.calculate(agg::image_filter_bicubic(), true);
         break;
      }
   }
}

//********************************************************************************************************************
// A generic drawing function for VMImage and VMPattern, this is used to fill vectors with bitmap images.

static void drawBitmap(LONG SampleMethod, agg::renderer_base<agg::pixfmt_psl> &RenderBase, agg::rasterizer_scanline_aa<> &Raster,
   objBitmap *SrcBitmap, LONG SpreadMethod, DOUBLE Opacity, agg::trans_affine *Transform = NULL, DOUBLE XOffset = 0, DOUBLE YOffset = 0)
{
   agg::pixfmt_psl pixels(*SrcBitmap);

   if ((Transform) and (Transform->is_complex())) {
      agg::span_interpolator_linear interpolator(*Transform);
      agg::image_filter_lut filter;
      set_filter(filter, SampleMethod);  // Set the interpolation filter to use.

      if (SpreadMethod IS VSPREAD_REFLECT_X) {
         agg::span_reflect_x source(pixels, XOffset, YOffset);
         agg::span_image_filter_rgba<agg::span_reflect_x, agg::span_interpolator_linear<>> spangen(source, interpolator, filter);
         drawBitmapRender(RenderBase, Raster, spangen, Opacity);
      }
      else if (SpreadMethod IS VSPREAD_REFLECT_Y) {
         agg::span_reflect_y source(pixels, XOffset, YOffset);
         agg::span_image_filter_rgba<agg::span_reflect_y, agg::span_interpolator_linear<>> spangen(source, interpolator, filter);
         drawBitmapRender(RenderBase, Raster, spangen, Opacity);
      }
      else if (SpreadMethod IS VSPREAD_REPEAT) {
         agg::span_repeat_rkl source(pixels, XOffset, YOffset);
         agg::span_image_filter_rgba<agg::span_repeat_rkl, agg::span_interpolator_linear<>> spangen(source, interpolator, filter);
         drawBitmapRender(RenderBase, Raster, spangen, Opacity);
      }
      else { // VSPREAD_PAD and VSPREAD_CLIP modes.
         agg::span_once<agg::pixfmt_psl> source(pixels, XOffset, YOffset);
         agg::span_image_filter_rgba<agg::span_once<agg::pixfmt_psl>, agg::span_interpolator_linear<>> spangen(source, interpolator, filter);
         drawBitmapRender(RenderBase, Raster, spangen, Opacity);
      }
   }
   else {
      // 1:1 copy with no transforms that require interpolation

      if (Transform) {
         XOffset += Transform->tx;
         YOffset += Transform->ty;
      }

      if (SpreadMethod IS VSPREAD_REFLECT_X) {
         agg::span_reflect_x source(pixels, XOffset, YOffset);
         drawBitmapRender(RenderBase, Raster, source, Opacity);
      }
      else if (SpreadMethod IS VSPREAD_REFLECT_Y) {
         agg::span_reflect_y source(pixels, XOffset, YOffset);
         drawBitmapRender(RenderBase, Raster, source, Opacity);
      }
      else if (SpreadMethod IS VSPREAD_REPEAT) {
         agg::span_repeat_rkl source(pixels, XOffset, YOffset);
         drawBitmapRender(RenderBase, Raster, source, Opacity);
      }
      else { // VSPREAD_PAD and VSPREAD_CLIP modes.
         agg::span_once<agg::pixfmt_psl> source(pixels, XOffset, YOffset);
         drawBitmapRender(RenderBase, Raster, source, Opacity);
      }
   }
}

//********************************************************************************************************************
// Fixed-size patterns can be rendered internally as a separate bitmap for tiling.  That bitmap is copied to the
// target bitmap with the necessary transforms applied.  USERSPACE patterns are suitable for this method.  If the
// client needs the pattern to maintain a fixed alignment with the associated vector, they must set the X,Y field
// values manually when that vector changes position.
//
// Patterns rendered with BOUNDING_BOX require real-time calculation as they have a dependency on the target
// vector's dimensions.

static void draw_pattern(DOUBLE *Bounds, agg::path_storage *Path,
   LONG SampleMethod, const agg::trans_affine &Transform, DOUBLE ViewWidth, DOUBLE ViewHeight,
   extVectorPattern &Pattern, agg::renderer_base<agg::pixfmt_psl> &RenderBase,
   agg::rasterizer_scanline_aa<> &Raster)
{
   const DOUBLE c_width  = (Pattern.Units IS VUNIT_USERSPACE) ? ViewWidth : Bounds[2] - Bounds[0];
   const DOUBLE c_height = (Pattern.Units IS VUNIT_USERSPACE) ? ViewHeight : Bounds[3] - Bounds[1];
   const DOUBLE x_offset = (Pattern.Units IS VUNIT_USERSPACE) ? 0 : Bounds[0];
   const DOUBLE y_offset = (Pattern.Units IS VUNIT_USERSPACE) ? 0 : Bounds[1];

   if (Pattern.Units IS VUNIT_USERSPACE) { // Use fixed coordinates specified in the pattern.
      DOUBLE dwidth, dheight;
      if (Pattern.Dimensions & DMF_RELATIVE_WIDTH) dwidth = c_width * Pattern.Width;
      else if (Pattern.Dimensions & DMF_FIXED_WIDTH) dwidth = Pattern.Width;
      else dwidth = 1;

      if (Pattern.Dimensions & DMF_RELATIVE_HEIGHT) dheight = c_height * Pattern.Height;
      else if (Pattern.Dimensions & DMF_FIXED_HEIGHT) dheight = Pattern.Height;
      else dheight = 1;

      if ((dwidth != Pattern.Scene->PageWidth) or (dheight != Pattern.Scene->PageHeight)) {
         Pattern.Scene->PageWidth = dwidth;
         Pattern.Scene->PageHeight = dheight;
         mark_dirty(Pattern.Scene->Viewport, RC_ALL);
      }
   }
   else {
      // BOUNDING_BOX.  The pattern (x,y) is an optional offset applied to the base position of the vector's
      // path.  The area is relative to the vector's bounds.

      DOUBLE dwidth, dheight;

      if (Pattern.Dimensions & DMF_RELATIVE_WIDTH) dwidth = Pattern.Width * c_width;
      else if (Pattern.Dimensions & DMF_FIXED_WIDTH) {
         if (Pattern.Viewport->vpViewWidth) dwidth = (Pattern.Width / Pattern.Viewport->vpViewWidth) * c_width;
         else dwidth = Pattern.Width * c_width; // A quirk of SVG is that the fixed value has to be interpreted as a multiplier if the viewbox is unspecified.
      }
      else dwidth = 1;

      if (Pattern.Dimensions & DMF_RELATIVE_HEIGHT) dheight = Pattern.Height * c_height;
      else if (Pattern.Dimensions & DMF_FIXED_HEIGHT) {
         if (Pattern.Viewport->vpViewHeight) dheight = (Pattern.Height / Pattern.Viewport->vpViewHeight) * c_height;
         else dheight = Pattern.Height * c_height;
      }
      else dheight = 1;

      if ((dwidth != Pattern.Scene->PageWidth) or (dheight != Pattern.Scene->PageHeight)) {
         Pattern.Scene->PageWidth  = dwidth;
         Pattern.Scene->PageHeight = dheight;
         mark_dirty(Pattern.Scene->Viewport, RC_ALL);
      }
   }

   // Redraw the pattern source if any part of the definition is marked as dirty.
   if ((check_dirty((extVector *)Pattern.Scene->Viewport)) or (!Pattern.Bitmap)) {
      if (acDraw(&Pattern) != ERR_Okay) return;
   }

   agg::trans_affine transform;

   DOUBLE dx, dy;
   if (Pattern.Dimensions & DMF_RELATIVE_X) dx = x_offset + (c_width * Pattern.X);
   else if (Pattern.Dimensions & DMF_FIXED_X) dx = x_offset + Pattern.X;
   else dx = x_offset;

   if (Pattern.Dimensions & DMF_RELATIVE_Y) dy = y_offset + c_height * Pattern.Y;
   else if (Pattern.Dimensions & DMF_FIXED_Y) dy = y_offset + Pattern.Y;
   else dy = y_offset;

   if (Pattern.Matrices) {
      auto &m = *Pattern.Matrices;
      transform.load_all(m.ScaleX, m.ShearY, m.ShearX, m.ScaleY, m.TranslateX + dx, m.TranslateY + dy);
   }
   else transform.translate(dx, dy);

   if (Pattern.Units != VUNIT_USERSPACE) transform *= Transform;

   transform.invert(); // Required

   drawBitmap(SampleMethod, RenderBase, Raster, Pattern.Bitmap, Pattern.SpreadMethod, Pattern.Opacity, &transform);
}

//********************************************************************************************************************
// Use for drawing stroked paths with texture brushes.  Source images should have width of ^2 if maximum efficiency
// is desired.

class pattern_rgb {
   public:
      typedef agg::rgba8 color_type;

      pattern_rgb(objBitmap &Bitmap, DOUBLE Height) : mBitmap(&Bitmap) {
         mScale = ((DOUBLE)Bitmap.Height) / Height;
         mHeight = Height;

         if (Bitmap.BitsPerPixel IS 32) {
            if (Bitmap.ColourFormat->AlphaPos IS 24) {
               if (Bitmap.ColourFormat->BluePos IS 0) pixel = &pixel32BGRA;
               else pixel = &pixel32RGBA;
            }
            else if (Bitmap.ColourFormat->RedPos IS 24) pixel = &pixel32AGBR;
            else pixel = &pixel32ARGB;
         }
         else if (Bitmap.BitsPerPixel IS 24) {
            if (Bitmap.ColourFormat->BluePos IS 0) pixel = &pixel24BGR;
            else pixel = &pixel24RGB;
         }
         else if (Bitmap.BitsPerPixel IS 16) {
            if ((Bitmap.ColourFormat->BluePos IS 0) and (Bitmap.ColourFormat->RedPos IS 11)) pixel = &pixel16BGR;
            else if ((Bitmap.ColourFormat->RedPos IS 0) and (Bitmap.ColourFormat->BluePos IS 11)) pixel = &pixel16RGB;
            else pixel = &pixel16;
         }

         if (Height != (DOUBLE)mBitmap->Height) {
            ipixel = pixel;
            pixel = &pixelScaled;
         }
      }

      unsigned width()  const { return mBitmap->Width;  }
      unsigned height() const { return mHeight; }

      static agg::rgba8 pixel32BGRA(const pattern_rgb &Pattern, int x, int y) {
         UBYTE *p = Pattern.mBitmap->Data + (y * Pattern.mBitmap->LineWidth) + (x<<2);
         return agg::rgba8(p[2], p[1], p[0], p[3]);
      }

      static agg::rgba8 pixel32RGBA(const pattern_rgb &Pattern, int x, int y) {
         UBYTE *p = Pattern.mBitmap->Data + (y * Pattern.mBitmap->LineWidth) + (x<<2);
         return agg::rgba8(p[0], p[1], p[2], p[3]);
      }

      static agg::rgba8 pixel32AGBR(const pattern_rgb &Pattern, int x, int y) {
         UBYTE *p = Pattern.mBitmap->Data + (y * Pattern.mBitmap->LineWidth) + (x<<2);
         return agg::rgba8(p[3], p[1], p[2], p[0]);
      }

      static agg::rgba8 pixel32ARGB(const pattern_rgb &Pattern, int x, int y) {
         UBYTE *p = Pattern.mBitmap->Data + (y * Pattern.mBitmap->LineWidth) + (x<<2);
         return agg::rgba8(p[1], p[2], p[3], p[0]);
      }

      static agg::rgba8 pixel24BGR(const pattern_rgb &Pattern, int x, int y) {
         UBYTE *p = Pattern.mBitmap->Data + (y * Pattern.mBitmap->LineWidth) + (x*3);
         return agg::rgba8(p[2], p[1], p[0], p[3]);
      }

      static agg::rgba8 pixel24RGB(const pattern_rgb &Pattern, int x, int y) {
         UBYTE *p = Pattern.mBitmap->Data + (y * Pattern.mBitmap->LineWidth) + (x*3);
         return agg::rgba8(p[0], p[1], p[2]);
      }

      static agg::rgba8 pixel16BGR(const pattern_rgb &Pattern, int x, int y) {
         UWORD p = ((UWORD *)(Pattern.mBitmap->Data + (y * Pattern.mBitmap->LineWidth) + (x<<1)))[0];
         return agg::rgba8((p>>8) & 0xf8, (p>>3) & 0xf8, p<<3);
      }

      static agg::rgba8 pixel16RGB(const pattern_rgb &Pattern, int x, int y) {
         UWORD p = ((UWORD *)(Pattern.mBitmap->Data + (y * Pattern.mBitmap->LineWidth) + (x<<1)))[0];
         return agg::rgba8(p<<3, (p>>3) & 0xf8, (p>>8) & 0xf8);
      }

      static agg::rgba8 pixel16(const pattern_rgb &Pattern, int x, int y) {
         UWORD p = ((UWORD *)(Pattern.mBitmap->Data + (y * Pattern.mBitmap->LineWidth) + (x<<1)))[0];
         return agg::rgba8(Pattern.mBitmap->unpackRed(p), Pattern.mBitmap->unpackGreen(p), Pattern.mBitmap->unpackBlue(p));
      }

      static agg::rgba8 pixelScaled(const pattern_rgb &Pattern, int x, int y) {
         DOUBLE src_y = (y + 0.5) * Pattern.mScale - 0.5;
         int h  = Pattern.mBitmap->Height - 1;
         int y1 = agg::ufloor(src_y);
         int y2 = y1 + 1;
         agg::rgba8 pix1 = (y1 < 0) ? agg::rgba8::no_color() : Pattern.ipixel(Pattern, x, y1);
         agg::rgba8 pix2 = (y2 > h) ? agg::rgba8::no_color() : Pattern.ipixel(Pattern, x, y2);
         return pix1.gradient(pix2, src_y - y1);
      }

      agg::rgba8 (*pixel)(const pattern_rgb &, int x, int y);

   private:
      agg::rgba8 (*ipixel)(const pattern_rgb &, int x, int y);
      objBitmap *mBitmap;
      DOUBLE mScale;
      DOUBLE mHeight;
};

void draw_brush(const objVectorImage &Image, agg::renderer_base<agg::pixfmt_psl> &RenderBase,
   agg::conv_transform<agg::path_storage, agg::trans_affine> &Path, DOUBLE StrokeWidth)
{
   typedef agg::pattern_filter_bilinear_rgba8 FILTER_TYPE;
   FILTER_TYPE filter;
   pattern_rgb src(*Image.Bitmap, StrokeWidth);

   DOUBLE scale;
   if (StrokeWidth IS (DOUBLE)Image.Bitmap->Height) scale = 1.0;
   else scale = (DOUBLE)StrokeWidth / (DOUBLE)Image.Bitmap->Height;

   if (isPow2((ULONG)Image.Bitmap->Width)) { // If the image width is a power of 2, use this optimised version
      typedef agg::line_image_pattern_pow2<FILTER_TYPE> pattern_type;
      pattern_type pattern(filter);
      agg::renderer_outline_image<agg::renderer_base<agg::pixfmt_psl>, pattern_type> ren_img(RenderBase, pattern);
      agg::rasterizer_outline_aa<agg::renderer_outline_image<agg::renderer_base<agg::pixfmt_psl>, pattern_type>> ras_img(ren_img);

      //ren_img.start_x(scale); // Optional offset

      pattern.create(src); // Configures the line pattern
      if (scale != 1.0) ren_img.scale_x(scale);
      ras_img.add_path(Path);
   }
   else { // Slightly slower version for non-power of 2 textures.
      typedef agg::line_image_pattern<FILTER_TYPE> pattern_type;
      pattern_type pattern(filter);
      agg::renderer_outline_image<agg::renderer_base<agg::pixfmt_psl>, pattern_type> ren_img(RenderBase, pattern);
      agg::rasterizer_outline_aa<agg::renderer_outline_image<agg::renderer_base<agg::pixfmt_psl>, pattern_type>> ras_img(ren_img);

      //ren_img.start_x(scale);

      pattern.create(src);
      if (scale != 1.0) ren_img.scale_x(scale);
      ras_img.add_path(Path);
   }
}

//********************************************************************************************************************
// Image extension
// Path: The original vector path without transforms.
// Transform: Transforms to be applied to the path and to align the image.

static void draw_image(DOUBLE *Bounds, agg::path_storage &Path, LONG SampleMethod,
   const agg::trans_affine &Transform, DOUBLE ViewWidth, DOUBLE ViewHeight,
   objVectorImage &Image, agg::renderer_base<agg::pixfmt_psl> &RenderBase,
   agg::rasterizer_scanline_aa<> &Raster, DOUBLE Alpha = 1.0)
{
   const DOUBLE c_width  = (Image.Units IS VUNIT_USERSPACE) ? ViewWidth : Bounds[2] - Bounds[0];
   const DOUBLE c_height = (Image.Units IS VUNIT_USERSPACE) ? ViewHeight : Bounds[3] - Bounds[1];
   const DOUBLE dx = Bounds[0] + ((Image.Dimensions & DMF_RELATIVE_X) ? (c_width * Image.X) : Image.X);
   const DOUBLE dy = Bounds[1] + ((Image.Dimensions & DMF_RELATIVE_Y) ? (c_height * Image.Y) : Image.Y);

   agg::trans_affine transform;
   if (Image.SpreadMethod IS VSPREAD_PAD) { // In pad mode, stretch the image to fit the boundary
      transform.scale((Bounds[2] - Bounds[0]) / Image.Bitmap->Width, (Bounds[3] - Bounds[1]) / Image.Bitmap->Height);
   }

   transform.translate(dx, dy);
   transform *= Transform;

   transform.invert();

   drawBitmap(SampleMethod, RenderBase, Raster, Image.Bitmap, Image.SpreadMethod, Alpha, &transform);
}

//********************************************************************************************************************
// Gradient fills
// TODO: Support gradient_xy (rounded corner), gradient_sqrt_xy

static void draw_gradient(DOUBLE *Bounds, agg::path_storage *Path, const agg::trans_affine &Transform,
   DOUBLE ViewWidth, DOUBLE ViewHeight, const extVectorGradient &Gradient, GRADIENT_TABLE *Table,
   agg::renderer_base<agg::pixfmt_psl> &RenderBase,
   agg::rasterizer_scanline_aa<> &Raster,
   DOUBLE BorderWidth)
{
   typedef agg::span_interpolator_linear<> interpolator_type;
   typedef agg::span_allocator<agg::rgba8> span_allocator_type;
   typedef agg::pod_auto_array<agg::rgba8, 256> color_array_type;
   typedef agg::renderer_base<agg::pixfmt_psl>  RENDERER_BASE_TYPE;

   agg::scanline_u8    scanline;
   agg::trans_affine   transform;
   interpolator_type   span_interpolator(transform);
   span_allocator_type span_allocator;

   const DOUBLE c_width = Gradient.Units IS VUNIT_USERSPACE ? ViewWidth : Bounds[2] - Bounds[0];
   const DOUBLE c_height = Gradient.Units IS VUNIT_USERSPACE ? ViewHeight : Bounds[3] - Bounds[1];
   const DOUBLE x_offset = Gradient.Units IS VUNIT_USERSPACE ? 0 : Bounds[0];
   const DOUBLE y_offset = Gradient.Units IS VUNIT_USERSPACE ? 0 : Bounds[1];

   if (Gradient.Type IS VGT_LINEAR) {
      DOUBLE ax1, ay1, ax2, ay2;

      if (Gradient.Flags & VGF_RELATIVE_X1) ax1 = x_offset + (c_width * Gradient.X1);
      else ax1 = x_offset + Gradient.X1;

      if (Gradient.Flags & VGF_RELATIVE_X2) ax2 = x_offset + (c_width * Gradient.X2);
      else ax2 = x_offset + Gradient.X2;

      if (Gradient.Flags & VGF_RELATIVE_Y1) ay1 = y_offset + (c_height * Gradient.Y1);
      else ay1 = y_offset + Gradient.Y1;

      if (Gradient.Flags & VGF_RELATIVE_Y2) ay2 = y_offset + (c_height * Gradient.Y2);
      else ay2 = y_offset + Gradient.Y2;

      // Calculate the gradient's transition from the point at (x1,y1) to (x2,y2)

      const DOUBLE dx = ax2 - ax1;
      const DOUBLE dy = ay2 - ay1;
      transform.scale(sqrt((dx * dx) + (dy * dy)) / 256.0);
      transform.rotate(atan2(dy, dx));

      transform.translate(ax1, ay1);
      apply_transforms(Gradient, transform);

      transform *= Transform;

      transform.invert();

      agg::gradient_x gradient_func; // gradient_x is a horizontal gradient with infinite height
      typedef agg::span_gradient<agg::rgba8, interpolator_type, agg::gradient_x, color_array_type> span_gradient_type;
      typedef agg::renderer_scanline_aa<RENDERER_BASE_TYPE, span_allocator_type, span_gradient_type> renderer_gradient_type;
      span_gradient_type  span_gradient(span_interpolator, gradient_func, *Table, 0, 256);
      renderer_gradient_type solidgrad(RenderBase, span_allocator, span_gradient);

      agg::render_scanlines(Raster, scanline, solidgrad);
   }
   else if (Gradient.Type IS VGT_RADIAL) {
      DOUBLE cx, cy, fx, fy;

      if (Gradient.Flags & VGF_RELATIVE_CX) cx = x_offset + (c_width * Gradient.CenterX);
      else cx = x_offset + Gradient.CenterX;

      if (Gradient.Flags & VGF_RELATIVE_CY) cy = y_offset + (c_height * Gradient.CenterY);
      else cy = y_offset + Gradient.CenterY;

      if (Gradient.Flags & VGF_RELATIVE_FX) fx = x_offset + (c_width * Gradient.FX);
      else if (Gradient.Flags & VGF_FIXED_FX) fx = x_offset + Gradient.FX;
      else fx = x_offset + cx;

      if (Gradient.Flags & VGF_RELATIVE_FY) fy = y_offset + (c_height * Gradient.FY);
      else if (Gradient.Flags & VGF_FIXED_FY) fy = y_offset + Gradient.FY;
      else fy = y_offset + cy;

      if ((cx IS fx) and (cy IS fy)) {
         // Standard radial gradient, where the focal point is the same as the gradient center

         DOUBLE length = Gradient.Radius;
         if (Gradient.Units IS VUNIT_USERSPACE) { // Coordinates are relative to the viewport
            if (Gradient.Flags & VGF_RELATIVE_RADIUS) { // Gradient is a ratio of the viewport's dimensions
               length = (ViewWidth + ViewHeight) * Gradient.Radius * 0.5;
            }
         }
         else { // Coordinates are relative to the bounding box
            if (Gradient.Flags & VGF_RELATIVE_RADIUS) {
               // In AGG, an unscaled gradient will cover the entire bounding box according to the diagonal.
               // In SVG a radius of 50% must result in the edge of the radius meeting the edge of the bounding box.

               DOUBLE scale_x = Gradient.Radius * (1.0 / 0.707106781); //(width * Gradient.Radius) / (sqrt((width * width) + (width * width)) * 0.5);
               DOUBLE scale_y = Gradient.Radius * (1.0 / 0.707106781); //(height * Gradient.Radius) / (sqrt((height * height) + (height * height)) * 0.5);
               if (c_height > c_width) scale_y *= c_height / c_width;
               else if (c_width > c_height) scale_x *= c_width / c_height;
               scale_x *= 100.0 / length; // Adjust the scale according to the gradient length.
               scale_y *= 100.0 / length;
               transform.scale(scale_x, scale_y);
            }
            else {
               //transform *= agg::trans_affine_scaling(Gradient.Radius * 0.01 / length);
            }
         }

         if (length < 255) { // Blending works best if the gradient span is at least 255 colours wide, so adjust it here.
            transform.scale(length * (1.0 / 255.0));
            length = 255;
         }

         agg::gradient_radial  gradient_func;
         typedef agg::span_gradient<agg::rgba8, interpolator_type, agg::gradient_radial, color_array_type> span_gradient_type;
         typedef agg::renderer_scanline_aa<RENDERER_BASE_TYPE, span_allocator_type, span_gradient_type> renderer_gradient_type;
         span_gradient_type  span_gradient(span_interpolator, gradient_func, *Table, 0, length);
         renderer_gradient_type solidrender_gradient(RenderBase, span_allocator, span_gradient);

         transform.translate(cx, cy);
         apply_transforms(Gradient, transform);
         transform *= Transform;
         transform.invert();

         agg::render_scanlines(Raster, scanline, solidrender_gradient);
      }
      else {
         // Radial gradient with a displaced focal point (uses agg::gradient_radial_focus).  NB: In early versions of
         // the SVG standard, the focal point had to be within the radius.  Later specifications allowed it to
         // be placed outside of the radius.

         DOUBLE fix_radius = Gradient.Radius;
         if (Gradient.Flags & VGF_RELATIVE_RADIUS) fix_radius *= (c_width + c_height) * 0.5; // Use the average radius of the ellipse.
         DOUBLE length = fix_radius;

         if (Gradient.Units IS VUNIT_USERSPACE) {
            if (Gradient.Flags & VGF_RELATIVE_RADIUS) {
               const DOUBLE scale = length * Gradient.Radius;
               transform *= agg::trans_affine_scaling(sqrt((ViewWidth * ViewWidth) + (ViewHeight * ViewHeight)) / scale);
            }
            else transform *= agg::trans_affine_scaling(Gradient.Radius * 0.01);
         }
         else { // Bounding box
            if (Gradient.Flags & VGF_RELATIVE_RADIUS) {
               // In AGG, an unscaled gradient will cover the entire bounding box according to the diagonal.
               // In SVG a radius of 50% must result in the edge of the radius meeting the edge of the bounding box.

               DOUBLE scale_x = Gradient.Radius * (1.0 / 0.707106781);
               DOUBLE scale_y = Gradient.Radius * (1.0 / 0.707106781);
               if (c_height > c_width) scale_y *= c_height / c_width;
               else if (c_width > c_height) scale_x *= c_width / c_height;
               scale_x *= 100.0 / length; // Adjust the scale according to the gradient length.
               scale_y *= 100.0 / length;
               transform.scale(scale_x, scale_y);
            }
            else transform *= agg::trans_affine_scaling(Gradient.Radius * 0.01);
         }

         agg::gradient_radial_focus gradient_func(fix_radius, fx - cx, fy - cy);

         typedef agg::span_gradient<agg::rgba8, interpolator_type, agg::gradient_radial_focus, color_array_type> span_gradient_type;
         typedef agg::renderer_scanline_aa<RENDERER_BASE_TYPE, span_allocator_type, span_gradient_type> renderer_gradient_type;
         span_gradient_type  span_gradient(span_interpolator, gradient_func, *Table, 0, fix_radius);
         renderer_gradient_type solidrender_gradient(RenderBase, span_allocator, span_gradient);

         transform.translate(cx, cy);
         apply_transforms(Gradient, transform);
         transform *= Transform;
         transform.invert();

         agg::render_scanlines(Raster, scanline, solidrender_gradient);
      }
   }
   else if (Gradient.Type IS VGT_DIAMOND) {
      DOUBLE cx, cy;

      if (Gradient.Flags & VGF_RELATIVE_CX) cx = x_offset + (c_width * Gradient.CenterX);
      else cx = x_offset + Gradient.CenterX;

      if (Gradient.Flags & VGF_RELATIVE_CY) cy = y_offset + (c_height * Gradient.CenterY);
      else cy = y_offset + Gradient.CenterY;

      // Standard diamond gradient, where the focal point is the same as the gradient center

      const DOUBLE length = 255;
      if (Gradient.Units IS VUNIT_USERSPACE) {
         if (Gradient.Flags & VGF_RELATIVE_RADIUS) {
            const DOUBLE scale = length * Gradient.Radius;
            transform *= agg::trans_affine_scaling(sqrt((c_width * c_width) + (c_height * c_height)) / scale);
         }
         else transform *= agg::trans_affine_scaling(Gradient.Radius * 0.01);
      }
      else { // Align to vector's bounding box
         if (Gradient.Flags & VGF_RELATIVE_RADIUS) {
            // In AGG, an unscaled gradient will cover the entire bounding box according to the diagonal.
            // In SVG a radius of 50% must result in the edge of the radius meeting the edge of the bounding box.

            DOUBLE scale_x = Gradient.Radius * (1.0 / 0.707106781);
            DOUBLE scale_y = Gradient.Radius * (1.0 / 0.707106781);
            if (c_height > c_width) scale_y *= c_height / c_width;
            else if (c_width > c_height) scale_x *= c_width / c_height;
            scale_x *= 100.0 / length; // Adjust the scale according to the gradient length.
            scale_y *= 100.0 / length;
            transform.scale(scale_x, scale_y);
         }
         else transform *= agg::trans_affine_scaling(Gradient.Radius * 0.01);
      }

      agg::gradient_diamond gradient_func;
      typedef agg::span_gradient<agg::rgba8, interpolator_type, agg::gradient_diamond, color_array_type> span_gradient_type;
      typedef agg::renderer_scanline_aa<RENDERER_BASE_TYPE, span_allocator_type, span_gradient_type> renderer_gradient_type;
      span_gradient_type  span_gradient(span_interpolator, gradient_func, *Table, 0, length);
      renderer_gradient_type solidrender_gradient(RenderBase, span_allocator, span_gradient);

      transform.translate(cx, cy);
      apply_transforms(Gradient, transform);
      transform *= Transform;
      transform.invert();

      agg::render_scanlines(Raster, scanline, solidrender_gradient);
   }
   else if (Gradient.Type IS VGT_CONIC) {
      DOUBLE cx, cy;

      if (Gradient.Flags & VGF_RELATIVE_CX) cx = x_offset + (c_width * Gradient.CenterX);
      else cx = x_offset + Gradient.CenterX;

      if (Gradient.Flags & VGF_RELATIVE_CY) cy = y_offset + (c_height * Gradient.CenterY);
      else cy = y_offset + Gradient.CenterY;

      // Standard conic gradient, where the focal point is the same as the gradient center

      const DOUBLE length = 255;
      if (Gradient.Units IS VUNIT_USERSPACE) {
         if (Gradient.Flags & VGF_RELATIVE_RADIUS) {
            const DOUBLE scale = length * Gradient.Radius;
            transform *= agg::trans_affine_scaling(sqrt((c_width * c_width) + (c_height * c_height)) / scale);
         }
         else transform *= agg::trans_affine_scaling(Gradient.Radius * 0.01);
      }
      else { // Bounding box
         if (Gradient.Flags & VGF_RELATIVE_RADIUS) {
            // In AGG, an unscaled gradient will cover the entire bounding box according to the diagonal.
            // In SVG a radius of 50% must result in the edge of the radius meeting the edge of the bounding box.

            DOUBLE scale_x = Gradient.Radius * (1.0 / 0.707106781);
            DOUBLE scale_y = Gradient.Radius * (1.0 / 0.707106781);
            if (c_height > c_width) scale_y *= c_height / c_width;
            else if (c_width > c_height) scale_x *= c_width / c_height;
            scale_x *= 100.0 / length; // Adjust the scale according to the gradient length.
            scale_y *= 100.0 / length;
            transform.scale(scale_x, scale_y);
         }
         else transform *= agg::trans_affine_scaling(Gradient.Radius * 0.01);
      }

      agg::gradient_conic gradient_func;
      typedef agg::span_gradient<agg::rgba8, interpolator_type, agg::gradient_conic, color_array_type> span_gradient_type;
      typedef agg::renderer_scanline_aa<RENDERER_BASE_TYPE, span_allocator_type, span_gradient_type> renderer_gradient_type;
      span_gradient_type  span_gradient(span_interpolator, gradient_func, *Table, 0, length);
      renderer_gradient_type solidrender_gradient(RenderBase, span_allocator, span_gradient);

      transform.translate(cx, cy);
      apply_transforms(Gradient, transform);
      transform *= Transform;
      transform.invert();

      agg::render_scanlines(Raster, scanline, solidrender_gradient);
   }
   else if (Gradient.Type IS VGT_CONTOUR) { // NOTE: Contouring requires a bounding box and is thus incompatible with UserSpaceOnUse
      if (Gradient.Units != VUNIT_BOUNDING_BOX) return;

      auto x1 = (Gradient.X1 >= 0) ? Gradient.X1 : 0;
      auto x2 = (Gradient.X2 <= 512) ? Gradient.X2 : 512;

      agg::gradient_contour gradient_func;
      gradient_func.frame(0); // This value offsets the gradient, e.g. 10 adds an x,y offset of (10,10)
      gradient_func.d1(x1);   // x1 and x2 alter the coverage of the gradient colours
      gradient_func.d2(x2);   // Low values for x2 will increase the amount of repetition seen in the gradient.
      gradient_func.contour_create(Path);

      transform.translate(x_offset, y_offset);
      apply_transforms(Gradient, transform);
      transform *= Transform;
      transform.invert();

      typedef agg::span_gradient<agg::rgba8, interpolator_type, agg::gradient_contour, color_array_type> span_gradient_type;
      typedef agg::renderer_scanline_aa<RENDERER_BASE_TYPE, span_allocator_type, span_gradient_type> renderer_gradient_type;
      span_gradient_type  span_gradient(span_interpolator, gradient_func, *Table, x1, x2);
      renderer_gradient_type solidrender_gradient(RenderBase, span_allocator, span_gradient);

      agg::render_scanlines(Raster, scanline, solidrender_gradient);
   }
}

//********************************************************************************************************************

class VMAdaptor
{
private:
   agg::renderer_base<agg::pixfmt_psl> mRenderBase;
   agg::pixfmt_psl mFormat;
   agg::scanline_u8 mScanLine;  // Use scanline_p for large solid polygons and scanline_u for things like text and gradients
   extVectorViewport *mView; // The current view
   objBitmap *mBitmap;

public:
   extVectorScene *Scene; // The top-level VectorScene performing the draw.

   VMAdaptor() { }

   void draw(objBitmap *Bitmap) {
      parasol::Log log;

      log.traceBranch("Bitmap: %dx%d,%dx%d, Viewport: %p", Bitmap->Clip.Left, Bitmap->Clip.Top, Bitmap->Clip.Right, Bitmap->Clip.Bottom, Scene->Viewport);

      if ((Bitmap->Clip.Bottom > Bitmap->Height) or (Bitmap->Clip.Right > Bitmap->Width)) {
         // NB: Any code that triggers this warning needs to be fixed.
         log.warning("Invalid Bitmap clip region: %d %d %d %d; W/H: %dx%d", Bitmap->Clip.Left, Bitmap->Clip.Top, Bitmap->Clip.Right, Bitmap->Clip.Bottom, Bitmap->Width, Bitmap->Height);
         return;
      }

      if (Scene->Viewport) {
         mBitmap = Bitmap;
         mFormat.setBitmap(*Bitmap);
         mRenderBase.attach(mFormat);

         mView = NULL; // Current view
         mRenderBase.clip_box(Bitmap->Clip.Left, Bitmap->Clip.Top, Bitmap->Clip.Right-1, Bitmap->Clip.Bottom-1);

         Scene->InputBoundaries.clear();

         VectorState state;
         draw_vectors((extVector *)Scene->Viewport, state);

         // Visually debug input boundaries
         //for (auto const &bounds : Scene->InputBoundaries) {
         //   gfxDrawRectangle(Bitmap, bounds.BX1, bounds.BY1, bounds.BX2-bounds.BX1, bounds.BY2-bounds.BY1, 0xff00ff, FALSE);
         //}
      }
   }

private:

   constexpr DOUBLE view_width() {
      if (mView->vpDimensions & (DMF_FIXED_WIDTH|DMF_RELATIVE_WIDTH)) return mView->vpFixedWidth;
      else if (mView->vpViewWidth > 0) return mView->vpViewWidth;
      else return mView->Scene->PageWidth;
   }

   constexpr DOUBLE view_height() {
      if (mView->vpDimensions & (DMF_FIXED_HEIGHT|DMF_RELATIVE_HEIGHT)) return mView->vpFixedHeight;
      else if (mView->vpViewHeight > 0) return mView->vpViewHeight;
      else return mView->Scene->PageHeight;
   }

   void render_fill(VectorState &State, extVector &Vector, agg::rasterizer_scanline_aa<> &Raster) {
      // Think of the vector's path as representing a mask for the fill algorithm.  Any transforms applied to
      // an image/gradient fill are independent of the path.

      if (Vector.FillRule IS VFR_NON_ZERO) Raster.filling_rule(agg::fill_non_zero);
      else if (Vector.FillRule IS VFR_EVEN_ODD) Raster.filling_rule(agg::fill_even_odd);

      if ((Vector.FillColour.Alpha > 0) and (!Vector.DisableFillColour)) { // Solid colour
         auto colour = agg::rgba(Vector.FillColour, Vector.FillColour.Alpha * Vector.FillOpacity * State.mOpacity);

         if ((Vector.PathQuality IS RQ_CRISP) or (Vector.PathQuality IS RQ_FAST)) {
            agg::renderer_scanline_bin_solid renderer(mRenderBase);
            renderer.color(colour);

            if (State.mClipMask) {
               agg::alpha_mask_gray8 alpha_mask(State.mClipMask->ClipRenderer);
               agg::scanline_u8_am<agg::alpha_mask_gray8> mScanLineMasked(alpha_mask);
               agg::render_scanlines(Raster, mScanLineMasked, renderer);
            }
            else agg::render_scanlines(Raster, mScanLine, renderer);
         }
         else {
            agg::renderer_scanline_aa_solid renderer(mRenderBase);
            renderer.color(colour);

            if (State.mClipMask) {
               agg::alpha_mask_gray8 alpha_mask(State.mClipMask->ClipRenderer);
               agg::scanline_u8_am<agg::alpha_mask_gray8> mScanLineMasked(alpha_mask);
               agg::render_scanlines(Raster, mScanLineMasked, renderer);
            }
            else agg::render_scanlines(Raster, mScanLine, renderer);
         }
      }

      if (Vector.FillImage) { // Bitmap image fill.  NB: The SVG class creates a standard VectorRectangle and associates an image with it in order to support <image> tags.
         draw_image((DOUBLE *)&Vector.BX1, Vector.BasePath, Vector.Scene->SampleMethod, build_fill_transform(Vector, Vector.FillImage->Units IS VUNIT_USERSPACE, State),
            view_width(), view_height(), *Vector.FillImage, mRenderBase, Raster, Vector.FillOpacity * State.mOpacity);
      }

      if (Vector.FillGradient) {
         if (auto table = get_fill_gradient_table(Vector, State.mOpacity * Vector.FillOpacity)) {
            draw_gradient((DOUBLE *)&Vector.BX1, &Vector.BasePath, build_fill_transform(Vector, Vector.FillGradient->Units IS VUNIT_USERSPACE, State),
               view_width(), view_height(), *((extVectorGradient *)Vector.FillGradient), table, mRenderBase, Raster, 0);
         }
      }

      if (Vector.FillPattern) {
         draw_pattern((DOUBLE *)&Vector.BX1, &Vector.BasePath, Vector.Scene->SampleMethod, build_fill_transform(Vector, Vector.FillPattern->Units IS VUNIT_USERSPACE, State),
            view_width(), view_height(), *Vector.FillPattern, mRenderBase, Raster);
      }
   }

   void render_stroke(VectorState &State, extVector &Vector, agg::rasterizer_scanline_aa<> &Raster) {
      if (Vector.Scene->Gamma != 1.0) Raster.gamma(agg::gamma_power(Vector.Scene->Gamma));

      if (Vector.FillRule IS VFR_NON_ZERO) Raster.filling_rule(agg::fill_non_zero);
      else if (Vector.FillRule IS VFR_EVEN_ODD) Raster.filling_rule(agg::fill_even_odd);

      if (Vector.StrokeGradient) {
         if (auto table = get_stroke_gradient_table(Vector)) {
            draw_gradient((DOUBLE *)&Vector.BX1, &Vector.BasePath, build_fill_transform(Vector, ((extVectorGradient *)Vector.StrokeGradient)->Units IS VUNIT_USERSPACE, State),
               view_width(), view_height(), *((extVectorGradient *)Vector.StrokeGradient), table, mRenderBase, Raster, Vector.fixed_stroke_width());
         }
      }
      else if (Vector.StrokePattern) {
         draw_pattern((DOUBLE *)&Vector.BX1, &Vector.BasePath, Vector.Scene->SampleMethod, build_fill_transform(Vector, Vector.StrokePattern->Units IS VUNIT_USERSPACE, State),
            view_width(), view_height(), *Vector.StrokePattern, mRenderBase, Raster);
      }
      else if (Vector.StrokeImage) {
         DOUBLE stroke_width = Vector.fixed_stroke_width() * Vector.Transform.scale();
         if (stroke_width < 1) stroke_width = 1;

         auto transform = Vector.Transform * State.mTransform;
         agg::conv_transform<agg::path_storage, agg::trans_affine> stroke_path(Vector.BasePath, transform);

         draw_brush(*Vector.StrokeImage, mRenderBase, stroke_path, stroke_width);
      }
      else {
         if ((Vector.PathQuality IS RQ_CRISP) or (Vector.PathQuality IS RQ_FAST)) {
            agg::renderer_scanline_bin_solid renderer(mRenderBase);
            renderer.color(agg::rgba(Vector.StrokeColour, Vector.StrokeColour.Alpha * Vector.StrokeOpacity * State.mOpacity));

            if (State.mClipMask) {
               agg::alpha_mask_gray8 alpha_mask(State.mClipMask->ClipRenderer);
               agg::scanline_u8_am<agg::alpha_mask_gray8> mScanLineMasked(alpha_mask);
               agg::render_scanlines(Raster, mScanLineMasked, renderer);
            }
            else agg::render_scanlines(Raster, mScanLine, renderer);
         }
         else {
            agg::renderer_scanline_aa_solid renderer(mRenderBase);
            renderer.color(agg::rgba(Vector.StrokeColour, Vector.StrokeColour.Alpha * Vector.StrokeOpacity * State.mOpacity));

            if (State.mClipMask) {
               agg::alpha_mask_gray8 alpha_mask(State.mClipMask->ClipRenderer);
               agg::scanline_u8_am<agg::alpha_mask_gray8> mScanLineMasked(alpha_mask);
               agg::render_scanlines(Raster, mScanLineMasked, renderer);
            }
            else agg::render_scanlines(Raster, mScanLine, renderer);
         }
      }
   }

   // This is the main routine for parsing the vector tree for drawing.

   void draw_vectors(extVector *CurrentVector, VectorState &ParentState) {
      for (auto shape=CurrentVector; shape; shape=(extVector *)shape->Next) {
         parasol::Log log(__FUNCTION__);
         VectorState state = VectorState(ParentState);

         if (shape->ClassID != ID_VECTOR) {
            log.trace("Non-Vector discovered in the vector tree.");
            continue;
         }
         else if (!shape->Scene) continue;

         if (shape->Dirty) {
            gen_vector_path(shape);
            shape->Dirty = 0;
         }
         else log.trace("%s: #%d, Dirty: NO, ParentView: #%d", shape->Class->ClassName, shape->UID, shape->ParentView ? shape->ParentView->UID : 0);

         // Visibility management.

         {
            bool visible = true;
            if (shape->Visibility IS VIS_INHERIT) {
               if (ParentState.mVisible != VIS_VISIBLE) visible = false;
            }
            else if (shape->Visibility != VIS_VISIBLE) visible = false;

            if (!visible) {
               log.trace("%s: #%d, Not Visible", get_name(shape), shape->UID);
               continue;
            }
         }

         auto filter = (extVectorFilter *)shape->Filter;
         if ((filter) and (!filter->Disabled)) {
            #ifdef DBG_DRAW
               log.traceBranch("Rendering filter for %s.", get_name(shape));
            #endif

            objBitmap *bmp;
            if (!render_filter(filter, mView, shape, mBitmap, &bmp)) {
               bmp->Opacity = (filter->Opacity < 1.0) ? (255.0 * filter->Opacity) : 255;
               gfxCopyArea(bmp, mBitmap, BAF_BLEND|BAF_COPY, 0, 0, bmp->Width, bmp->Height, 0, 0);
            }
            continue;
         }

         #ifdef DBG_DRAW
            log.traceBranch("%s: #%d, Matrices: %p", get_name(shape), shape->UID, shape->Matrices);
         #endif

         if (mBitmap->ColourSpace IS CS_LINEAR_RGB) state.mLinearRGB = true; // The target bitmap's colour space has priority if linear.
         else if (shape->ColourSpace IS VCS_LINEAR_RGB) state.mLinearRGB = true; // Use the parent value unless a specific CS is required by the client
         else if (shape->ColourSpace IS VCS_SRGB) state.mLinearRGB = false;

         if (shape->LineJoin != agg::inherit_join)   state.mLineJoin  = shape->LineJoin;
         if (shape->InnerJoin != agg::inner_inherit) state.mInnerJoin = shape->InnerJoin;
         if (shape->LineCap != agg::inherit_cap)     state.mLineCap   = shape->LineCap;
         state.mOpacity = shape->Opacity * state.mOpacity;

         // Support for enable-background="new".  This requires the bitmap to have an alpha channel so that
         // blending will work correctly, and the bitmap will be cleared to accept fresh content.  It acts as
         // a placeholder over the existing target bitmap, and the new content will be rendered to the target
         // after processing the current branch.  The background is then discarded.

         // TODO: The allocation of this bitmap during rendering isn't optimal.  Perhaps we could allocate it as a permanent
         // dummy bitmap to be retained with the Vector, and the Data would be allocated dynamically during rendering.
         //
         // TODO: The clipping area of the bitmap should be declared so that unnecessary pixel scanning is avoided.

         objBitmap *bmpBkgd = NULL;
         objBitmap *bmpSave = NULL;
         if (shape->EnableBkgd) {
            if ((bmpBkgd = objBitmap::create::integral(fl::Name("scene_temp_bkgd"),
                  fl::Width(mBitmap->Width),
                  fl::Height(mBitmap->Height),
                  fl::BitsPerPixel(32),
                  fl::Flags(BMF_ALPHA_CHANNEL),
                  fl::ColourSpace(mBitmap->ColourSpace)))) {
               bmpSave = mBitmap;
               mBitmap = bmpBkgd;
               mFormat.setBitmap(*bmpBkgd);
               ClearMemory(bmpBkgd->Data, bmpBkgd->LineWidth * bmpBkgd->Height);
               state.mBackgroundActive = true;
            }
         }

         if (shape->SubID IS ID_VECTORVIEWPORT) {
            if ((shape->Child) or (shape->InputSubscriptions) or (shape->FillPattern)) {
               auto view = (extVectorViewport *)shape;

               if (view->vpOverflowX != VOF_INHERIT) state.mOverflowX = view->vpOverflowX;
               if (view->vpOverflowY != VOF_INHERIT) state.mOverflowY = view->vpOverflowY;

               auto save_clip = state.mClip;
               DOUBLE x1 = state.mClip.x1, y1 = state.mClip.y1, x2 = state.mClip.x2, y2 = state.mClip.y2;

               if ((state.mOverflowX IS VOF_HIDDEN) or (state.mOverflowX IS VOF_SCROLL) or (view->vpAspectRatio & ARF_SLICE)) {
                  if (view->vpBX1 > state.mClip.x1) state.mClip.x1 = view->vpBX1;
                  if (view->vpBX2 < state.mClip.x2) state.mClip.x2 = view->vpBX2;
               }

               if ((state.mOverflowY IS VOF_HIDDEN) or (state.mOverflowY IS VOF_SCROLL) or (view->vpAspectRatio & ARF_SLICE)) {
                  if (view->vpBY1 > state.mClip.y1) state.mClip.y1 = view->vpBY1;
                  if (view->vpBY2 < state.mClip.y2) state.mClip.y2 = view->vpBY2;
               }

               #ifdef DBG_DRAW
                  log.traceBranch("Viewport #%d clip region (%.2f %.2f %.2f %.2f)", shape->UID, state.mClip.x1, state.mClip.y1, state.mClip.x2, state.mClip.y2);
               #endif

               if ((state.mClip.x2 > state.mClip.x1) and (state.mClip.y2 > state.mClip.y1)) { // Continue only if the clipping region is visible
                  auto saved_mask = state.mClipMask;
                  if (view->vpClipMask) state.mClipMask = view->vpClipMask;

                  auto save_rb_clip = mRenderBase.clip_box();
                  if (state.mClip.x1 > save_rb_clip.x1) mRenderBase.m_clip_box.x1 = state.mClip.x1;
                  if (state.mClip.y1 > save_rb_clip.y1) mRenderBase.m_clip_box.y1 = state.mClip.y1;
                  if (state.mClip.x2 < save_rb_clip.x2) mRenderBase.m_clip_box.x2 = state.mClip.x2;
                  if (state.mClip.y2 < save_rb_clip.y2) mRenderBase.m_clip_box.y2 = state.mClip.y2;

                  log.trace("ViewBox (%.2f %.2f %.2f %.2f) Scale (%.2f %.2f) Fix (%.2f %.2f %.2f %.2f)",
                    view->vpViewX, view->vpViewY, view->vpViewWidth, view->vpViewHeight,
                    view->vpXScale, view->vpYScale,
                    view->FinalX, view->FinalY, view->vpFixedWidth, view->vpFixedHeight);

                  auto saved_viewport = mView;  // Save current viewport state and switch to the new viewport state
                  mView = view;

                  // For vectors that read user input, we record the collision box for the cursor.

                  if ((shape->InputSubscriptions) or ((shape->Cursor) and (shape->Cursor != PTR_DEFAULT))) {
                     if (view->vpBX1 > x1) x1 = view->vpBX1;
                     if (view->vpBX2 < x2) x2 = view->vpBX2;
                     if (view->vpBY1 > y1) y1 = view->vpBY1;
                     if (view->vpBY2 < y2) y2 = view->vpBY2;
                     Scene->InputBoundaries.emplace_back(shape->UID, view->Cursor, x1, y1, x2, y2, view->vpBX1, view->vpBY1);
                  }

                  if (Scene->Flags & VPF_OUTLINE_VIEWPORTS) { // Debug option: Draw the viewport's path with a green outline
                     agg::renderer_scanline_bin_solid renderer(mRenderBase);
                     renderer.color(agg::rgba(0, 1, 0));
                     agg::rasterizer_scanline_aa stroke_raster;
                     agg::conv_stroke<agg::path_storage> stroked_path(view->BasePath);
                     stroked_path.width(2);
                     stroke_raster.add_path(stroked_path);
                     agg::render_scanlines(stroke_raster, mScanLine, renderer);
                  }

                  if (view->FillPattern) {
                     // Viewports can use FillPattern objects to render a different scene graph internally.
                     // This is useful for creating common graphics that can be re-used multiple times without
                     // them being pre-rasterised as they normally would be for primitive vectors.
                     //
                     // The client can expect a result that would match that of the pattern's viewport being placed
                     // as a child of this one.  NB: There is a performance penalty in that transforms will be
                     // applied in realtime.

                     auto s_transform = state.mTransform;
                     auto s_apply     = state.mApplyTransform;
                     state.mTransform      = view->Transform;
                     state.mApplyTransform = true;

                     draw_vectors(view->FillPattern->Viewport, state);

                     state.mTransform      = s_transform;
                     state.mApplyTransform = s_apply;
                  }

                  if (view->Child) draw_vectors((extVector *)view->Child, state);

                  state.mClipMask = saved_mask;

                  mView = saved_viewport;

                  mRenderBase.clip_box_naked(save_rb_clip);
               }
               else log.trace("Clipping boundary results in invisible viewport.");

               state.mClip = save_clip;
            }
         }
         else {
            // Clip masks are redrawn every cycle and for each vector, due to the fact that their look is dependent on the
            // vector to which they are applied (e.g. transforms that are active for the target vector will also affect the
            // clip path).

            if (shape->ClipMask) {
               shape->ClipMask->TargetVector = shape;
               acDraw(shape->ClipMask);
               shape->ClipMask->TargetVector = NULL;
            }

            if (shape->GeneratePath) { // A vector that generates a path can be drawn
               #ifdef DBG_DRAW
                  log.traceBranch("%s: #%d, Mask: %p", get_name(shape), shape->UID, shape->ClipMask);
               #endif

               if (!mView) {
                  // Vectors not inside a viewport cannot be drawn (they may exist as definitions for other objects,
                  // e.g. as morph paths).
                  return;
               }

               auto saved_mask = state.mClipMask;
               if (shape->ClipMask) state.mClipMask = shape->ClipMask;

               if (shape->FillRaster) {
                  if (state.mApplyTransform) {
                     // Run-time transforms in the draw process aren't ideal as they require the path to be processed
                     // every time.  It's necessary if the client wants to re-use vectors though (saving resources and gaining
                     // some conveniences).
                     auto transform = shape->Transform * state.mTransform;
                     agg::conv_transform<agg::path_storage, agg::trans_affine> final_path(shape->BasePath, transform);
                     agg::rasterizer_scanline_aa raster;
                     raster.add_path(final_path);
                     render_fill(state, *shape, raster);
                  }
                  else render_fill(state, *shape, *shape->FillRaster);
               }

               if (shape->StrokeRaster) {
                  if (state.mApplyTransform) {
                     auto transform = shape->Transform * state.mTransform;

                     if (shape->DashArray) {
                        configure_stroke(*shape, shape->DashArray->stroke);
                        agg::conv_transform<agg::conv_stroke<agg::conv_dash<agg::path_storage>>, agg::trans_affine> final_path(shape->DashArray->stroke, transform);

                        agg::rasterizer_scanline_aa raster;
                        raster.add_path(final_path);
                        render_stroke(state, *shape, raster);
                     }
                     else {
                        agg::conv_stroke<agg::path_storage> stroked_path(shape->BasePath);
                        configure_stroke(*shape, stroked_path);
                        agg::conv_transform<agg::conv_stroke<agg::path_storage>, agg::trans_affine> final_path(stroked_path, transform);

                        agg::rasterizer_scanline_aa raster;
                        raster.add_path(final_path);
                        render_stroke(state, *shape, raster);
                     }
                  }
                  else render_stroke(state, *shape, *shape->StrokeRaster);
               }

               if (shape->InputSubscriptions) {
                  // If the vector receives user input events then we record the collision box for the mouse cursor.
                  DOUBLE xmin = mRenderBase.xmin(), xmax = mRenderBase.xmax();
                  DOUBLE ymin = mRenderBase.ymin(), ymax = mRenderBase.ymax();
                  DOUBLE bx1, by1, bx2, by2;

                  if ((shape->ClipMask) and (shape->ClipMask->ClipPath)) {
                     agg::conv_transform<agg::path_storage, agg::trans_affine> path(*shape->ClipMask->ClipPath, shape->Transform);
                     bounding_rect_single(path, 0, &bx1, &by1, &bx2, &by2);
                  }
                  else if (shape->BasePath.total_vertices()) {
                     if (shape->Transform.is_normal()) {
                        bx1 = shape->BX1;
                        by1 = shape->BY1;
                        bx2 = shape->BX2;
                        by2 = shape->BY2;
                     }
                     else {
                        auto simple_path = basic_path(shape->BX1, shape->BY1, shape->BX2, shape->BY2);
                        agg::conv_transform<agg::path_storage, agg::trans_affine> path(simple_path, shape->Transform);
                        bounding_rect_single(path, 0, &bx1, &by1, &bx2, &by2);
                     }
                  }
                  else {
                     bx1 = -1;
                     by1 = -1;
                     bx2 = -1;
                     by2 = -1;
                  }

                  DOUBLE absx = bx1;
                  DOUBLE absy = by1;

                  if (xmin > bx1) bx1 = xmin;
                  if (ymin > by1) by1 = ymin;
                  if (xmax < bx2) bx2 = xmax;
                  if (ymax < by2) by2 = ymax;

                  Scene->InputBoundaries.emplace_back(shape->UID, shape->Cursor, bx1, by1, bx2, by2, absx, absy);
               }

               state.mClipMask = saved_mask;
            }

            if (shape->Child) {
               auto saved_mask = state.mClipMask;
               if (shape->ClipMask) state.mClipMask = shape->ClipMask;

               draw_vectors((extVector *)shape->Child, state);

               state.mClipMask = saved_mask;
            }
         }

         if (bmpBkgd) {
            agg::rasterizer_scanline_aa raster;
            setRasterClip(raster, 0, 0, bmpBkgd->Width, bmpBkgd->Height);

            mBitmap = bmpSave;
            mFormat.setBitmap(*mBitmap);
            drawBitmap(shape->Scene->SampleMethod, mRenderBase, raster, bmpBkgd, VSPREAD_CLIP, 1.0);
            acFree(bmpBkgd);
         }
      }
   }
};

//********************************************************************************************************************
// For direct vector drawing

void SimpleVector::DrawPath(objBitmap *Bitmap, DOUBLE StrokeWidth, OBJECTPTR StrokeStyle, OBJECTPTR FillStyle)
{
   parasol::Log log("draw_path");

   agg::scanline_u8 scanline;
   agg::pixfmt_psl format;
   agg::trans_affine transform;

   format.setBitmap(*Bitmap);
   mRenderer.attach(format);
   mRenderer.clip_box(Bitmap->Clip.Left, Bitmap->Clip.Top, Bitmap->Clip.Right-1, Bitmap->Clip.Bottom-1);
   //if (Gamma != 1.0) mRaster.gamma(agg::gamma_power(Gamma));

   log.traceBranch("Bitmap: %p, Stroke: %p (%s), Fill: %p (%s)", Bitmap, StrokeStyle, get_name(StrokeStyle), FillStyle, get_name(FillStyle));

   DOUBLE bounds[4];
   bounding_rect_single(mPath, 0, &bounds[0], &bounds[1], &bounds[2], &bounds[3]);

   if (FillStyle) {
      mRaster.reset();
      mRaster.add_path(mPath);

      if (FillStyle->ClassID IS ID_VECTORCOLOUR) {
         auto colour = (objVectorColour *)FillStyle;
         agg::renderer_scanline_aa_solid solid(mRenderer);
         solid.color(agg::rgba(colour->Red, colour->Green, colour->Blue, colour->Alpha));
         agg::render_scanlines(mRaster, scanline, solid);
      }
      else if (FillStyle->ClassID IS ID_VECTORIMAGE) {
         objVectorImage &image = (objVectorImage &)*FillStyle;
         draw_image(bounds, mPath, VSM_AUTO, transform, Bitmap->Width, Bitmap->Height, image, mRenderer, mRaster);
      }
      else if (FillStyle->ClassID IS ID_VECTORGRADIENT) {
         extVectorGradient &gradient = (extVectorGradient &)*FillStyle;
         draw_gradient(bounds, &mPath, transform, Bitmap->Width, Bitmap->Height, gradient, &gradient.Colours->table, mRenderer, mRaster, 0);
      }
      else if (FillStyle->ClassID IS ID_VECTORPATTERN) {
         draw_pattern(bounds, &mPath, VSM_AUTO, transform, Bitmap->Width, Bitmap->Height, (extVectorPattern &)*FillStyle, mRenderer, mRaster);
      }
      else log.warning("The FillStyle is not supported.");
   }

   if ((StrokeWidth > 0) and (StrokeStyle)){
      if (StrokeStyle->ClassID IS ID_VECTORGRADIENT) {
         agg::conv_stroke<agg::path_storage> stroke_path(mPath);
         mRaster.reset();
         mRaster.add_path(stroke_path);

         extVectorGradient &gradient = (extVectorGradient &)*StrokeStyle;
         draw_gradient(bounds, &mPath, transform, Bitmap->Width, Bitmap->Height, gradient, &gradient.Colours->table, mRenderer, mRaster, 0);
      }
      else if (StrokeStyle->ClassID IS ID_VECTORPATTERN) {
         agg::conv_stroke<agg::path_storage> stroke_path(mPath);
         mRaster.reset();
         mRaster.add_path(stroke_path);
         draw_pattern(bounds, &mPath, VSM_AUTO, transform, Bitmap->Width, Bitmap->Height, (extVectorPattern &)*StrokeStyle, mRenderer, mRaster);
      }
      else if (StrokeStyle->ClassID IS ID_VECTORIMAGE) {
         objVectorImage &image = (objVectorImage &)*StrokeStyle;
         agg::trans_affine transform;
         agg::conv_transform<agg::path_storage, agg::trans_affine> path(mPath, transform);
         draw_brush(image, mRenderer, path, StrokeWidth);
      }
      else if (StrokeStyle->ClassID IS ID_VECTORCOLOUR) {
         agg::renderer_scanline_aa_solid solid(mRenderer);
         agg::conv_stroke<agg::path_storage> stroke_path(mPath);
         mRaster.reset();
         mRaster.add_path(stroke_path);
         objVectorColour *colour = (objVectorColour *)FillStyle;
         solid.color(agg::rgba(colour->Red, colour->Green, colour->Blue, colour->Alpha));
         agg::render_scanlines(mRaster, scanline, solid);
      }
      else log.warning("The StrokeStyle is not supported.");
   }
}

//********************************************************************************************************************

void agg::pixfmt_psl::setBitmap(objBitmap &Bitmap, bool Linear)
{
   auto data = Bitmap.Data + (Bitmap.XOffset * Bitmap.BytesPerPixel) + (Bitmap.YOffset * Bitmap.LineWidth);
   rawBitmap(data, Bitmap.Clip.Right, Bitmap.Clip.Bottom, Bitmap.LineWidth, Bitmap.BitsPerPixel, *Bitmap.ColourFormat, Linear);
}

void agg::pixfmt_psl::rawBitmap(UBYTE *Data, LONG Width, LONG Height, LONG Stride, LONG BitsPerPixel, ColourFormat &ColourFormat, bool Linear)
{
   mData   = Data;
   mWidth  = Width;
   mHeight = Height;
   mStride = Stride;
   mBytesPerPixel = BitsPerPixel/8;

   if (BitsPerPixel IS 32) {
      fBlendHLine      = &blendHLine32;
      fBlendSolidHSpan = &blendSolidHSpan32;
      fBlendColorHSpan = &blendColorHSpan32;
      fCopyColorHSpan  = &copyColorHSpan32;

      if (ColourFormat.AlphaPos IS 24) {
         if (ColourFormat.BluePos IS 0) {
            pixel_order(2, 1, 0, 3); // BGRA
            fBlendPix = Linear ? &linear32BGRA : &blend32BGRA;
            fCopyPix  = Linear ? &linearCopy32BGRA : &copy32BGRA;
            fCoverPix = Linear ? &linearCover32BGRA : &cover32BGRA;
         }
         else {
            pixel_order(0, 1, 2, 3); // RGBA
            fBlendPix = Linear ? &linear32RGBA : &blend32RGBA;
            fCopyPix  = Linear ? &linearCopy32RGBA : &copy32RGBA;
            fCoverPix = Linear ? &linearCover32RGBA : &cover32RGBA;
         }
      }
      else if (ColourFormat.RedPos IS 24) {
         pixel_order(3, 1, 2, 0); // AGBR
         fBlendPix = Linear ? &linear32AGBR : &blend32AGBR;
         fCopyPix  = Linear ? &linearCopy32AGBR : &copy32AGBR;
         fCoverPix = Linear ? &linearCover32AGBR : &cover32AGBR;
      }
      else {
         pixel_order(1, 2, 3, 0); // ARGB
         fBlendPix = Linear ? &linear32ARGB : &blend32ARGB;
         fCopyPix  = Linear ? &linearCopy32ARGB : &copy32ARGB;
         fCoverPix = Linear ? &linearCover32ARGB : &cover32ARGB;
      }
   }
   else if (BitsPerPixel IS 24) {
      fBlendHLine      = &blendHLine24;
      fBlendSolidHSpan = &blendSolidHSpan24;
      fBlendColorHSpan = &blendColorHSpan24;
      fCopyColorHSpan  = &copyColorHSpan24;

      if (ColourFormat.BluePos IS 0) {
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
   else if (BitsPerPixel IS 16) {
      // Deprecated.  16-bit client code should use 24-bit and downscale instead.
      parasol::Log log;
      log.warning("Support for 16-bit bitmaps is deprecated.");
   }
}
