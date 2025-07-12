
#include "agg_span_gradient_contour.h"

class VectorState;

//********************************************************************************************************************

class SceneRenderer
{
private:
   agg::renderer_base<agg::pixfmt_psl> mRenderBase;
   agg::pixfmt_psl   mFormat;
   agg::scanline_u8  mScanLine; // Use scanline_p for large solid polygons/rectangles and scanline_u for complex shapes like text
   extVectorViewport *mView;    // The current view
   objBitmap         *mBitmap;

public:
   // The ClipBuffer is used to hold any alpha-masks that are generated as the scene is rendered.

   class ClipBuffer {
      VectorState *m_state;
      std::vector<uint8_t> m_bitmap;
      int m_width, m_height;
      extVector *m_shape;

      public:
      agg::rendering_buffer m_renderer;
      extVectorClip *m_clip;

      public:

      ClipBuffer() : m_shape(nullptr), m_clip(nullptr) { }

      ClipBuffer(VectorState &pState, extVectorClip *pClip, extVector *pShape) :
         m_state(&pState), m_shape(pShape), m_clip(pClip) { }

      void draw(SceneRenderer &);
      void draw_viewport(SceneRenderer &);
      void draw_clips(SceneRenderer &, extVector *, agg::rasterizer_scanline_aa<> &,
         agg::renderer_base<agg::pixfmt_gray8> &, const agg::trans_affine &);
      void draw_bounding_box(SceneRenderer &);
      void draw_userspace(SceneRenderer &);
      void resize_bitmap(int, int, int, int);
   };

private:
   constexpr double view_width() {
      if (mView->vpViewWidth > 0) return mView->vpViewWidth;
      else if (dmf::hasAnyWidth(mView->vpDimensions)) return mView->vpFixedWidth;
      else return mView->Scene->PageWidth;
   }

   constexpr double view_height() {
      if (mView->vpViewHeight > 0) return mView->vpViewHeight;
      else if (dmf::hasAnyHeight(mView->vpDimensions)) return mView->vpFixedHeight;
      else return mView->Scene->PageHeight;
   }

   void render_fill(VectorState &, extVector &, agg::rasterizer_scanline_aa<> &, extPainter &);
   void render_stroke(VectorState &, extVector &);
   void draw_vectors(extVector *, VectorState &);
   static const agg::trans_affine build_fill_transform(extVector &, bool,  VectorState &);

public:
   extVectorScene *Scene; // The top-level VectorScene performing the draw.
   int mObjectCount;     // The number of objects drawn

   SceneRenderer(extVectorScene *pScene) : Scene(pScene) { }
   void draw(objBitmap *, objVectorViewport *);

   ~SceneRenderer() {
      Scene->ShareModified = false;
   }
};

//********************************************************************************************************************
// This class holds the current state as the vector scene is parsed for drawing.  It is most useful for managing
// inheritable values that arise as part of the drawing process (transformation management being an obvious example).
//
// NOTE: This feature is not intended to manage inheritable features that cross-over with SVG.  For instance, fill
// values are not inheritable.  Wherever it is possible to do so, inheritance should be managed by the client, with
// the goal of building a scene graph that has static properties.

class VectorState {
public:
   TClipRectangle<double> mClip; // Current clip region as defined by the viewports
   agg::line_join_e  mLineJoin;
   agg::line_cap_e   mLineCap;
   agg::inner_join_e mInnerJoin;
   std::shared_ptr<std::stack<SceneRenderer::ClipBuffer>> mClipStack;
   double mOpacity;
   VIS    mVisible;
   VOF    mOverflowX, mOverflowY;
   bool   mLinearRGB;
   bool   mIsolated;
   bool   mDirty;

   VectorState() :
      mClip(0, 0, DBL_MAX, DBL_MAX),
      mLineJoin(agg::miter_join),
      mLineCap(agg::butt_cap),
      mInnerJoin(agg::inner_miter),
      mClipStack(std::make_shared<std::stack<SceneRenderer::ClipBuffer>>()),
      mOpacity(1.0),
      mVisible(VIS::VISIBLE),
      mOverflowX(VOF::VISIBLE), mOverflowY(VOF::VISIBLE),
      mLinearRGB(false),
      mIsolated(false),
      mDirty(false)
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
         s->r = p[m_src->mPixelOrder.Red];
         s->g = p[m_src->mPixelOrder.Green];
         s->b = p[m_src->mPixelOrder.Blue];
         s->a = p[m_src->mPixelOrder.Alpha];
         p = (const value_type*)next_x();
         ++s;
      } while(--len);
   }

   uint8_t * span(int x, int y, unsigned) {
       m_x = x;
       m_row_ptr = m_src->row_ptr(m_wrap_y(y));
       return m_row_ptr + m_wrap_x(x) * 4;
   }

   uint8_t * next_x() {
       int x = ++m_wrap_x;
       return m_row_ptr + x * 4;
   }

   uint8_t * next_y() {
       m_row_ptr = m_src->row_ptr(++m_wrap_y);
       return m_row_ptr + m_wrap_x(m_x) * 4;
   }

   agg::pixfmt_psl *m_src;

private:
   wrap_mode_repeat_auto_pow2 m_wrap_x;
   wrap_mode_reflect_auto_pow2 m_wrap_y;
   uint8_t *m_row_ptr;
   unsigned m_offset_x;
   unsigned m_offset_y;
   uint8_t m_bk_buf[4];
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
       m_src(&pixf), m_wrap_x(pixf.mWidth), m_wrap_y(pixf.mHeight),
       m_offset_x(offset_x), m_offset_y(offset_y)
   {
      m_bk_buf[0] = m_bk_buf[1] = m_bk_buf[2] = m_bk_buf[3] = 0;
   }

   void prepare() {}

   void generate(agg::rgba8 *s, int x, int y, unsigned len) {
      x += m_offset_x;
      y += m_offset_y;
      const value_type* p = (const value_type*)span(x, y, len);
      do {
         s->r = p[m_src->mPixelOrder.Red];
         s->g = p[m_src->mPixelOrder.Green];
         s->b = p[m_src->mPixelOrder.Blue];
         s->a = p[m_src->mPixelOrder.Alpha];
         p = (const value_type*)next_x();
         ++s;
      } while(--len);
   }

   uint8_t * span(int x, int y, unsigned) {
      m_x = x;
      m_row_ptr = m_src->row_ptr(m_wrap_y(y));
      return m_row_ptr + m_wrap_x(x) * 4;
   }

   uint8_t * next_x() {
      int x = ++m_wrap_x;
      return m_row_ptr + x * 4;
   }

   uint8_t * next_y() {
      m_row_ptr = m_src->row_ptr(++m_wrap_y);
      return m_row_ptr + m_wrap_x(m_x) * 4;
   }

   agg::pixfmt_psl *m_src;

private:
   wrap_mode_reflect_auto_pow2 m_wrap_x;
   wrap_mode_repeat_auto_pow2 m_wrap_y;
   uint8_t *m_row_ptr;
   unsigned m_offset_x;
   unsigned m_offset_y;
   uint8_t m_bk_buf[4];
   int m_x;
};

//********************************************************************************************************************

class span_repeat_pf
{
private:
   span_repeat_pf();
public:
   typedef typename agg::rgba8::value_type value_type;
   typedef agg::rgba8 color_type;

   span_repeat_pf(agg::pixfmt_psl & pixf, unsigned offset_x, unsigned offset_y) :
       m_src(&pixf), m_wrap_x(pixf.mWidth), m_wrap_y(pixf.mHeight),
       m_offset_x(offset_x), m_offset_y(offset_y) {
      m_bk_buf[0] = m_bk_buf[1] = m_bk_buf[2] = m_bk_buf[3] = 0;
   }

   void prepare() {}

   void generate(agg::rgba8 *s, int x, int y, unsigned len) {
      x += m_offset_x;
      y += m_offset_y;
      const value_type* p = (const value_type*)span(x, y, len);
      do {
         s->r = p[m_src->mPixelOrder.Red];
         s->g = p[m_src->mPixelOrder.Green];
         s->b = p[m_src->mPixelOrder.Blue];
         s->a = p[m_src->mPixelOrder.Alpha];
         p = (const value_type*)next_x();
         ++s;
      } while(--len);
   }

   uint8_t * span(int x, int y, unsigned) {
      m_x = x;
      m_row_ptr = m_src->row_ptr(m_wrap_y(y));
      return m_row_ptr + m_wrap_x(x) * 4;
   }

   uint8_t * next_x() {
      int x = ++m_wrap_x;
      return m_row_ptr + x * 4;
   }

   uint8_t * next_y() {
      m_row_ptr = m_src->row_ptr(++m_wrap_y);
      return m_row_ptr + m_wrap_x(m_x) * 4;
   }

   agg::pixfmt_psl *m_src;

private:
   wrap_mode_repeat_auto_pow2 m_wrap_x, m_wrap_y;
   uint8_t *m_row_ptr;
   unsigned m_offset_x, m_offset_y;
   uint8_t m_bk_buf[4];
   int m_x;
};
} // namespace

//********************************************************************************************************************
// Check a Shape, its siblings and children for dirty markers.

static bool check_dirty(extVector *Shape) {
   while (Shape) {
      if (Shape->Class->BaseClassID != CLASSID::VECTOR) return true;
      if (Shape->dirty()) return true;

      if (Shape->Child) {
         if (check_dirty((extVector *)Shape->Child)) return true;
      }
      Shape = (extVector *)Shape->Next;
   }
   return false;
}

//********************************************************************************************************************
// Return the correct transformation matrix for a fill operation.  Requires that the vector's path has been generated.

const agg::trans_affine SceneRenderer::build_fill_transform(extVector &Vector, bool Userspace,  VectorState &State)
{
   if (Vector.dirty()) { // Sanity check: If the path is dirty then this function has been called out-of-sequence.
      DEBUG_BREAK
   }

   if (Userspace) { // Userspace: The vector's (x,y) position is ignored, but its transforms and all parent transforms will apply.
      agg::trans_affine transform;
      apply_transforms(Vector, transform);
      apply_parent_transforms(get_parent(&Vector), transform);
      return transform;
   }
   else return Vector.Transform; // Default BoundingBox: The vector's position, transforms, and parent transforms apply.
}

//********************************************************************************************************************

void set_filter(agg::image_filter_lut &Filter, VSM Method, agg::trans_affine &Transform, double Kernel)
{
   auto compute_kernel = [&Transform, &Kernel]() {
      // For auto kernel calculation, use larger kernel sizes when shrinking.  A base-level of 3.0 is used so that
      // the use of advanced filter algorithms is justified for the client.
      double k;
      if (Kernel > 0.0) k = Kernel;
      else k = 3.0 + (1.0 / svg_diag(Transform.sx, Transform.sy));
      return std::clamp(k, 2.0, 8.0);
   };

   switch(Method) {
      case VSM::AUTO:
      case VSM::NEIGHBOUR: // There is a 'span_image_filter_rgb_nn' class but no equivalent image_filter_neighbour() routine?
      case VSM::BILINEAR:  Filter.calculate(agg::image_filter_bilinear(), true); break;
      case VSM::BICUBIC:   Filter.calculate(agg::image_filter_bicubic(), true); break;
      case VSM::SPLINE16:  Filter.calculate(agg::image_filter_spline16(), true); break;
      case VSM::KAISER:    Filter.calculate(agg::image_filter_kaiser(), true); break;
      case VSM::QUADRIC:   Filter.calculate(agg::image_filter_quadric(), true); break;
      case VSM::GAUSSIAN:  Filter.calculate(agg::image_filter_gaussian(), true); break;
      case VSM::BESSEL:    Filter.calculate(agg::image_filter_bessel(), true); break;
      case VSM::MITCHELL:  Filter.calculate(agg::image_filter_mitchell(), true); break;
      case VSM::SINC:      Filter.calculate(agg::image_filter_sinc(compute_kernel()), true); break;
      case VSM::LANCZOS:   Filter.calculate(agg::image_filter_lanczos(compute_kernel()), true); break;
      case VSM::BLACKMAN:  Filter.calculate(agg::image_filter_blackman(compute_kernel()), true); break;
      default:             Filter.calculate(agg::image_filter_bicubic(), true); break;
   }
}

//********************************************************************************************************************
// A generic drawing function for VMImage and VMPattern, this is used to fill vectors with bitmap images.
// Optimium drawing speed is ensured by only using the chosen SampleMethod if the transform is complex.

template <class T> void drawBitmap(T &Scanline, VSM SampleMethod, agg::renderer_base<agg::pixfmt_psl> &RenderBase, agg::rasterizer_scanline_aa<> &Raster,
   objBitmap *SrcBitmap, VSPREAD SpreadMethod, double Opacity, agg::trans_affine *Transform = nullptr, double XOffset = 0, double YOffset = 0)
{
   agg::pixfmt_psl pixels(*SrcBitmap);

   if ((Transform) and (Transform->is_complex())) {
      agg::span_interpolator_linear interpolator(*Transform);
      agg::image_filter_lut filter;
      set_filter(filter, SampleMethod, *Transform);  // Set the interpolation filter to use.

      if (SpreadMethod IS VSPREAD::REFLECT_X) {
         agg::span_reflect_x source(pixels, XOffset, YOffset);
         agg::span_image_filter_rgba<agg::span_reflect_x, agg::span_interpolator_linear<>> spangen(source, interpolator, filter, true);
         drawBitmapRender(Scanline, RenderBase, Raster, spangen, Opacity);
      }
      else if (SpreadMethod IS VSPREAD::REFLECT_Y) {
         agg::span_reflect_y source(pixels, XOffset, YOffset);
         agg::span_image_filter_rgba<agg::span_reflect_y, agg::span_interpolator_linear<>> spangen(source, interpolator, filter, true);
         drawBitmapRender(Scanline, RenderBase, Raster, spangen, Opacity);
      }
      else if (SpreadMethod IS VSPREAD::REPEAT) {
         agg::span_repeat_pf source(pixels, XOffset, YOffset);
         agg::span_image_filter_rgba<agg::span_repeat_pf, agg::span_interpolator_linear<>> spangen(source, interpolator, filter, true);
         drawBitmapRender(Scanline, RenderBase, Raster, spangen, Opacity);
      }
      else { // VSPREAD::PAD and VSPREAD::CLIP modes.
         agg::span_once<agg::pixfmt_psl> source(pixels, XOffset, YOffset);
         agg::span_image_filter_rgba<agg::span_once<agg::pixfmt_psl>, agg::span_interpolator_linear<>> spangen(source, interpolator, filter, true);
         drawBitmapRender(Scanline, RenderBase, Raster, spangen, Opacity);
      }
   }
   else {
      // 1:1 copy with no transforms that require interpolation

      if (Transform) {
         XOffset += Transform->tx;
         YOffset += Transform->ty;
      }

      if (SpreadMethod IS VSPREAD::REFLECT_X) {
         agg::span_reflect_x source(pixels, XOffset, YOffset);
         drawBitmapRender(Scanline, RenderBase, Raster, source, Opacity);
      }
      else if (SpreadMethod IS VSPREAD::REFLECT_Y) {
         agg::span_reflect_y source(pixels, XOffset, YOffset);
         drawBitmapRender(Scanline, RenderBase, Raster, source, Opacity);
      }
      else if (SpreadMethod IS VSPREAD::REPEAT) {
         agg::span_repeat_pf source(pixels, XOffset, YOffset);
         drawBitmapRender(Scanline, RenderBase, Raster, source, Opacity);
      }
      else { // VSPREAD::PAD and VSPREAD::CLIP modes.
         agg::span_once<agg::pixfmt_psl> source(pixels, XOffset, YOffset);
         drawBitmapRender(Scanline, RenderBase, Raster, source, Opacity);
      }
   }
}

//********************************************************************************************************************
// Use for drawing stroked paths with texture brushes.  Source images should have width of ^2 if maximum efficiency
// is desired.

class pattern_rgb {
   public:
      typedef agg::rgba8 color_type;

      pattern_rgb(objBitmap &Bitmap, double Height) : mBitmap(&Bitmap) {
         mScale = ((double)Bitmap.Height) / Height;
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
         else {
            pf::Log log;
            log.warning("pattern_rgb: Unsupported bitmap format %dbpp", Bitmap.BitsPerPixel);
         }

         if (Height != (double)mBitmap->Height) {
            ipixel = pixel;
            pixel = &pixelScaled;
         }
      }

      unsigned width()  const { return mBitmap->Width;  }
      unsigned height() const { return mHeight; }

      static agg::rgba8 pixel32BGRA(const pattern_rgb &Pattern, int x, int y) {
         auto p = PIXEL_DATA(Pattern.mBitmap->Data + (y * Pattern.mBitmap->LineWidth) + (x<<2), pxBGRA);
         return p.getRGB();
      }

      static agg::rgba8 pixel32RGBA(const pattern_rgb &Pattern, int x, int y) {
         auto p = PIXEL_DATA(Pattern.mBitmap->Data + (y * Pattern.mBitmap->LineWidth) + (x<<2), pxRGBA);
         return p.getRGB();
      }

      static agg::rgba8 pixel32AGBR(const pattern_rgb &Pattern, int x, int y) {
         auto p = PIXEL_DATA(Pattern.mBitmap->Data + (y * Pattern.mBitmap->LineWidth) + (x<<2), pxAGBR);
         return p.getRGB();
      }

      static agg::rgba8 pixel32ARGB(const pattern_rgb &Pattern, int x, int y) {
         auto p = PIXEL_DATA(Pattern.mBitmap->Data + (y * Pattern.mBitmap->LineWidth) + (x<<2), pxARGB);
         return p.getRGB();
      }

      static agg::rgba8 pixel24BGR(const pattern_rgb &Pattern, int x, int y) {
         auto p = PIXEL_DATA(Pattern.mBitmap->Data + (y * Pattern.mBitmap->LineWidth) + (x<<2), pxBGR);
         return p.getRGB();
      }

      static agg::rgba8 pixel24RGB(const pattern_rgb &Pattern, int x, int y) {
         auto p = PIXEL_DATA(Pattern.mBitmap->Data + (y * Pattern.mBitmap->LineWidth) + (x<<2), pxRGB);
         return p.getRGB();
      }

      static agg::rgba8 pixelScaled(const pattern_rgb &Pattern, int x, int y) {
         double src_y = (y + 0.5) * Pattern.mScale - 0.5;
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
      double mScale;
      double mHeight;
};

//********************************************************************************************************************

static void stroke_brush(VectorState &State, const extVectorImage &Image, agg::renderer_base<agg::pixfmt_psl> &RenderBase,
   agg::conv_transform<agg::path_storage, agg::trans_affine> &Path, double StrokeWidth)
{
   typedef agg::pattern_filter_bilinear_rgba8 FILTER_TYPE;
   FILTER_TYPE filter;
   pattern_rgb src(*Image.Bitmap, StrokeWidth);

   double scale;
   if (StrokeWidth IS (double)Image.Bitmap->Height) scale = 1.0;
   else scale = (double)StrokeWidth / (double)Image.Bitmap->Height;

   if (isPow2((uint32_t)Image.Bitmap->Width)) { // If the image width is a power of 2, use this optimised version
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

void SceneRenderer::draw(objBitmap *Bitmap, objVectorViewport *Viewport)
{
   pf::Log log;

   mObjectCount = 0;

   log.traceBranch("Bitmap: %dx%d,%dx%d, Viewport: %p", Bitmap->Clip.Left, Bitmap->Clip.Top, Bitmap->Clip.Right, Bitmap->Clip.Bottom, Scene->Viewport);

   if ((Bitmap->Clip.Bottom > Bitmap->Height) or (Bitmap->Clip.Right > Bitmap->Width)) {
      // NB: Any code that triggers this warning needs to be fixed.
      log.warning("Invalid Bitmap clip region: %d %d %d %d; W/H: %dx%d", Bitmap->Clip.Left, Bitmap->Clip.Top, Bitmap->Clip.Right, Bitmap->Clip.Bottom, Bitmap->Width, Bitmap->Height);
      return;
   }

   if (Viewport) {
      mBitmap = Bitmap;
      mFormat.setBitmap(*Bitmap);
      mRenderBase.attach(mFormat);

      mView = nullptr; // Current view
      mRenderBase.clip_box(Bitmap->Clip.Left, Bitmap->Clip.Top, Bitmap->Clip.Right-1, Bitmap->Clip.Bottom-1);

      Scene->InputBoundaries.clear();

      VectorState state;
      draw_vectors((extVector *)Viewport, state);
   }
}

//********************************************************************************************************************
// Refer to configure_stroke() for the configuration of StrokeRaster.

void SceneRenderer::render_stroke(VectorState &State, extVector &Vector)
{
   auto &raster = *Vector.StrokeRaster;

   if (Vector.Scene->Gamma != 1.0) raster.gamma(agg::gamma_power(Vector.Scene->Gamma));

   if (Vector.FillRule IS VFR::NON_ZERO) raster.filling_rule(agg::fill_non_zero);
   else if (Vector.FillRule IS VFR::EVEN_ODD) raster.filling_rule(agg::fill_even_odd);

   // Regarding this validation check, SVG requires that stroked vectors have a size > 0 when a paint server is used
   // as a stroker.  If the size is zero, the paint server is ignored and the solid colour can be used as a stroker
   // if the client has specified one.  Ref: W3C SVG Coordinate chapter, last paragraph of 7.11

   if (Vector.Bounds.valid()) {
      if (Vector.Stroke.Gradient) {
         if (auto table = get_stroke_gradient_table(Vector)) {
            fill_gradient(State, Vector.Bounds, &Vector.BasePath, build_fill_transform(Vector, ((extVectorGradient *)Vector.Stroke.Gradient)->Units IS VUNIT::USERSPACE, State),
               view_width(), view_height(), *((extVectorGradient *)Vector.Stroke.Gradient), table, mRenderBase, raster);
         }
         return;
      }

      if (Vector.Stroke.Pattern) {
         fill_pattern(State, Vector.Bounds, &Vector.BasePath, Vector.Scene->SampleMethod, build_fill_transform(Vector, Vector.Stroke.Pattern->Units IS VUNIT::USERSPACE, State),
            view_width(), view_height(), *((extVectorPattern *)Vector.Stroke.Pattern), mRenderBase, raster);
         return;
      }

      if (Vector.Stroke.Image) {
         double stroke_width = Vector.fixed_stroke_width() * Vector.Transform.scale();
         if (stroke_width < 1) stroke_width = 1;

         auto transform = Vector.Transform;
         Vector.BasePath.approximation_scale(transform.scale());
         agg::conv_transform<agg::path_storage, agg::trans_affine> stroke_path(Vector.BasePath, transform);

         stroke_brush(State, *((extVectorImage *)Vector.Stroke.Image), mRenderBase, stroke_path, stroke_width);
         return;
      }
   }

   // Solid colour

   if ((Vector.PathQuality IS RQ::CRISP) or (Vector.PathQuality IS RQ::FAST)) {
      agg::renderer_scanline_bin_solid renderer(mRenderBase);
      renderer.color(agg::rgba(Vector.Stroke.Colour, Vector.Stroke.Colour.Alpha * Vector.StrokeOpacity * State.mOpacity));

      if (!State.mClipStack->empty()) {
         agg::alpha_mask_gray8 alpha_mask(State.mClipStack->top().m_renderer);
         agg::scanline_u8_am<agg::alpha_mask_gray8> mScanLineMasked(alpha_mask);
         agg::render_scanlines(raster, mScanLineMasked, renderer);
      }
      else agg::render_scanlines(raster, mScanLine, renderer);
   }
   else {
      agg::renderer_scanline_aa_solid renderer(mRenderBase);
      renderer.color(agg::rgba(Vector.Stroke.Colour, Vector.Stroke.Colour.Alpha * Vector.StrokeOpacity * State.mOpacity));

      if (!State.mClipStack->empty()) {
         agg::alpha_mask_gray8 alpha_mask(State.mClipStack->top().m_renderer);
         agg::scanline_u8_am<agg::alpha_mask_gray8> mScanLineMasked(alpha_mask);
         agg::render_scanlines(raster, mScanLineMasked, renderer);
      }
      else agg::render_scanlines(raster, mScanLine, renderer);
   }
}

// This is the main routine for parsing the vector tree for drawing.

void SceneRenderer::draw_vectors(extVector *CurrentVector, VectorState &ParentState) {
   for (auto shape=CurrentVector; shape; shape=(extVector *)shape->Next) {
      pf::Log log(__FUNCTION__);
      VectorState state = VectorState(ParentState);

      if (shape->baseClassID() != CLASSID::VECTOR) {
         log.trace("Non-Vector discovered in the vector tree.");
         continue;
      }
      else if (!shape->Scene) continue;

      if (shape->dirty()) gen_vector_path(shape);
      else log.trace("%s: #%d, Dirty: NO, ParentView: #%d", shape->Class->ClassName, shape->UID, shape->ParentView ? shape->ParentView->UID : 0);

      if (shape->RequiresRedraw) {
         state.mDirty = true; // Carry-forward dirty marker for children
         shape->RequiresRedraw = false;
      }

      // Visibility management.  NB: Under SVG rules VectorGroup objects are always visible as they are not
      // classed as a graphics element.

      {
         bool visible = true;
         if (shape->Visibility IS VIS::INHERIT) {
            if (ParentState.mVisible != VIS::VISIBLE) visible = false;
         }
         else if (shape->Visibility != VIS::VISIBLE) visible = false;

         if (((!visible) or (!shape->ValidState)) and (shape->classID() != CLASSID::VECTORGROUP)) {
            log.trace("%s: #%d, Not Visible", get_name(shape), shape->UID);
            continue;
         }
      }

      mObjectCount++;

      auto filter = (extVectorFilter *)shape->Filter;
      if ((filter) and (!filter->Disabled)) {
         #ifdef DBG_DRAW
            log.traceBranch("Rendering filter for %s.", get_name(shape));
         #endif

         objBitmap *bmp;
         if (render_filter(filter, mView, shape, mBitmap, &bmp) IS ERR::Okay) {
            bmp->Opacity = (filter->Opacity < 1.0) ? (255.0 * filter->Opacity) : 255;
            gfx::CopyArea(bmp, mBitmap, BAF::BLEND|BAF::COPY, 0, 0, bmp->Width, bmp->Height, 0, 0);
         }
         continue;
      }

      #ifdef DBG_DRAW
         log.traceBranch("%s: #%d, Matrices: %p", get_name(shape), shape->UID, shape->Matrices);
      #endif

      if (mBitmap->ColourSpace IS CS::LINEAR_RGB) state.mLinearRGB = true; // The target bitmap's colour space has priority if linear.
      else if (shape->ColourSpace IS VCS::LINEAR_RGB) state.mLinearRGB = true; // Use the parent value unless a specific CS is required by the client
      else if (shape->ColourSpace IS VCS::SRGB) state.mLinearRGB = false;

      if (shape->LineJoin  != agg::inherit_join)  state.mLineJoin  = shape->LineJoin;
      if (shape->InnerJoin != agg::inner_inherit) state.mInnerJoin = shape->InnerJoin;
      if (shape->LineCap   != agg::inherit_cap)   state.mLineCap   = shape->LineCap;
      state.mOpacity = shape->Opacity * state.mOpacity;

      // Support for isolated vectors.  A vector will be isolated if it has children using a filter that uses BackgroundImage
      // or BackgroundAlpha as an input.  This feature requires the bitmap to have an alpha channel so that
      // blending will work correctly, and the bitmap will be cleared to accept fresh content.  It acts as
      // a placeholder over the existing target bitmap, and the new content will be rendered to the target
      // after processing the current branch.  The background is then discarded.

      // TODO: The allocation of this bitmap during rendering isn't optimal.  Perhaps we could allocate it as a permanent
      // dummy bitmap to be retained with the Vector, and the Data would be allocated dynamically during rendering.
      //
      // TODO: The clipping area of the bitmap should be declared so that unnecessary pixel interaction is avoided.

      objBitmap *bmpBkgd = nullptr;
      objBitmap *bmpSave = nullptr;
      if ((shape->Flags & VF::ISOLATED) != VF::NIL) {
         if ((bmpBkgd = objBitmap::create::local(fl::Name("scene_temp_bkgd"),
               fl::Width(mBitmap->Width),
               fl::Height(mBitmap->Height),
               fl::BitsPerPixel(32),
               fl::Flags(BMF::ALPHA_CHANNEL),
               fl::ColourSpace(mBitmap->ColourSpace)))) {
            bmpSave = mBitmap;
            mBitmap = bmpBkgd;
            mFormat.setBitmap(*bmpBkgd);
            clearmem(bmpBkgd->Data, bmpBkgd->LineWidth * bmpBkgd->Height);
            state.mIsolated = true;
         }
      }

      if (shape->classID() IS CLASSID::VECTORVIEWPORT) {
         if ((shape->Child) or (shape->InputSubscriptions) or (shape->Fill[0].Pattern)) {
            auto view = (extVectorViewport *)shape;

            if (view->vpOverflowX != VOF::INHERIT) state.mOverflowX = view->vpOverflowX;
            if (view->vpOverflowY != VOF::INHERIT) state.mOverflowY = view->vpOverflowY;

            auto save_clip = state.mClip;
            auto clip = state.mClip;

            if ((state.mOverflowX IS VOF::HIDDEN) or (state.mOverflowX IS VOF::SCROLL) or ((view->vpAspectRatio & ARF::SLICE) != ARF::NIL)) {
               if (view->vpBounds.left > state.mClip.left) state.mClip.left = view->vpBounds.left;
               if (view->vpBounds.right < state.mClip.right) state.mClip.right = view->vpBounds.right;
            }

            if ((state.mOverflowY IS VOF::HIDDEN) or (state.mOverflowY IS VOF::SCROLL) or ((view->vpAspectRatio & ARF::SLICE) != ARF::NIL)) {
               if (view->vpBounds.top > state.mClip.top) state.mClip.top = view->vpBounds.top;
               if (view->vpBounds.bottom < state.mClip.bottom) state.mClip.bottom = view->vpBounds.bottom;
            }

            #ifdef DBG_DRAW
               log.traceBranch("Viewport #%d clip region (%.2f %.2f %.2f %.2f)", shape->UID, state.mClip.left, state.mClip.top, state.mClip.right, state.mClip.bottom);
            #endif

            if ((state.mClip.right > state.mClip.left) and (state.mClip.bottom > state.mClip.top)) { // Continue only if the clipping region is visible
               if (view->vpClip) {
                  state.mClipStack->emplace(state, (extVectorClip *)nullptr, view);
                  state.mClipStack->top().draw_viewport(*this);
               }

               if (view->ClipMask) {
                  state.mClipStack->emplace(state, view->ClipMask, view);
                  state.mClipStack->top().draw(*this);
               }

               auto save_rb_clip = mRenderBase.clip_box();
               if (state.mClip.left   > save_rb_clip.x1) mRenderBase.m_clip_box.x1 = state.mClip.left;
               if (state.mClip.top    > save_rb_clip.y1) mRenderBase.m_clip_box.y1 = state.mClip.top;
               if (state.mClip.right  < save_rb_clip.x2) mRenderBase.m_clip_box.x2 = state.mClip.right;
               if (state.mClip.bottom < save_rb_clip.y2) mRenderBase.m_clip_box.y2 = state.mClip.bottom;

               log.trace("ViewBox (%g %g %g %g) Scale (%g %g) Fix (%g %g %g %g)",
                  view->vpViewX, view->vpViewY, view->vpViewWidth, view->vpViewHeight,
                  view->vpXScale, view->vpYScale,
                  view->FinalX, view->FinalY, view->vpFixedWidth, view->vpFixedHeight);

               auto saved_viewport = mView;  // Save current viewport state and switch to the new viewport state
               mView = view;

               // For viewports that read user input, we record the collision box for the cursor.

               if ((shape->InputSubscriptions) or ((shape->Cursor != PTC::NIL) and (shape->Cursor != PTC::DEFAULT))) {
                  clip.shrinking(view);
                  Scene->InputBoundaries.emplace_back(shape->UID, view->Cursor, clip, view->vpBounds.left, view->vpBounds.top);
               }

               if ((Scene->Flags & VPF::OUTLINE_VIEWPORTS) != VPF::NIL) { // Debug option: Draw the viewport's path with a green outline
                  agg::renderer_scanline_bin_solid renderer(mRenderBase);
                  renderer.color(agg::rgba(0, 1, 0));
                  agg::rasterizer_scanline_aa stroke_raster;
                  agg::conv_stroke<agg::path_storage> stroked_path(view->BasePath);
                  stroked_path.width(2);
                  stroke_raster.add_path(stroked_path);
                  agg::render_scanlines(stroke_raster, mScanLine, renderer);
               }

               if (view->Fill[0].Pattern) {
                  // Viewports can use FillPattern objects to render a different scene graph internally.
                  // This is useful for creating common graphics that can be re-used multiple times without
                  // them being pre-rendered to a cache as they would be for filled vector paths.
                  //
                  // The client can expect a result that is equivalent to the pattern's viewport being a child of
                  // the current viewport.  NB: There is a performance penalty in that transforms will be
                  // applied in realtime.

                  if (!view->Fill[0].Pattern->Scene->Viewport->Matrices) {
                     view->Fill[0].Pattern->Scene->Viewport->newMatrix(nullptr, false);
                  }

                  // Use transforms for the purpose of placing the pattern correctly

                  auto &matrix = view->Fill[0].Pattern->Scene->Viewport->Matrices;
                  auto &t = view->Transform;

                  matrix->ScaleX = t.sx;
                  matrix->ScaleY = t.sy;
                  matrix->ShearX = t.shx;
                  matrix->ShearY = t.shy;
                  matrix->TranslateX = t.tx;
                  matrix->TranslateY = t.ty;

                  mark_dirty(view->Fill[0].Pattern->Scene->Viewport, RC::TRANSFORM);

                  if (view->Fill[0].Pattern->Units IS VUNIT::BOUNDING_BOX) {
                     view->Fill[0].Pattern->Scene->setPageWidth(view->Scene->PageWidth);
                     view->Fill[0].Pattern->Scene->setPageHeight(view->Scene->PageHeight);
                     view->Fill[0].Pattern->Scene->Viewport->setFields(fl::Width(view->vpFixedWidth), fl::Height(view->vpFixedHeight));
                  }

                  draw_vectors((extVectorViewport *)((extVectorPattern *)view->Fill[0].Pattern)->Viewport, state);

                  matrix->ScaleX = 1.0;
                  matrix->ScaleY = 1.0;
                  matrix->ShearX = 0;
                  matrix->ShearY = 0;
                  matrix->TranslateX = 0;
                  matrix->TranslateY = 0;
                  mark_dirty(view->Fill[0].Pattern->Scene->Viewport, RC::TRANSFORM);

                  if ((view->FGFill) and (view->Fill[1].Pattern)) {
                     // Support for foreground fill patterns
                     if (!view->Fill[1].Pattern->Scene->Viewport->Matrices) {
                        view->Fill[1].Pattern->Scene->Viewport->newMatrix(nullptr, false);
                     }

                     auto &matrix = view->Fill[1].Pattern->Scene->Viewport->Matrices;
                     matrix->ScaleX = t.sx;
                     matrix->ScaleY = t.sy;
                     matrix->ShearX = t.shx;
                     matrix->ShearY = t.shy;
                     matrix->TranslateX = t.tx;
                     matrix->TranslateY = t.ty;

                     mark_dirty(view->Fill[1].Pattern->Scene->Viewport, RC::TRANSFORM);

                     if (view->Fill[1].Pattern->Units IS VUNIT::BOUNDING_BOX) {
                        view->Fill[1].Pattern->Scene->setPageWidth(view->Scene->PageWidth);
                        view->Fill[1].Pattern->Scene->setPageHeight(view->Scene->PageHeight);
                        view->Fill[1].Pattern->Scene->Viewport->setFields(fl::Width(view->vpFixedWidth), fl::Height(view->vpFixedHeight));
                     }

                     draw_vectors((extVectorViewport *)((extVectorPattern *)view->Fill[1].Pattern)->Viewport, state);

                     matrix->ScaleX = 1.0;
                     matrix->ScaleY = 1.0;
                     matrix->ShearX = 0;
                     matrix->ShearY = 0;
                     matrix->TranslateX = 0;
                     matrix->TranslateY = 0;
                     mark_dirty(view->Fill[1].Pattern->Scene->Viewport, RC::TRANSFORM);
                  }
               }

               if (view->Child) {
                  constexpr int MAX_AREA = 4096 * 4096; // Maximum allowable area for enabling a viewport buffer

                  if ((view->vpBuffered) and (view->vpFixedWidth * view->vpFixedHeight < MAX_AREA)) {
                     // In buffered mode, children will be drawn to an independent bitmap that is permanently
                     // cached.

                     bool redraw = view->vpRefreshBuffer or state.mDirty;
                     view->vpRefreshBuffer = false;

                     if ((!redraw) and (Scene->ShareModified)) redraw = true;

                     if (view->vpBuffer) {
                        if ((view->vpBuffer->Width != view->vpFixedWidth) or (view->vpBuffer->Height != view->vpFixedHeight)) {
                           view->vpBuffer->resize(view->vpFixedWidth, view->vpFixedHeight);
                           redraw = true;
                        }
                     }
                     else {
                        view->vpBuffer = objBitmap::create::local(fl::Name("vp_buffer"),
                           fl::Owner(view->UID),
                           fl::Width(view->vpFixedWidth),
                           fl::Height(view->vpFixedHeight),
                           fl::BitsPerPixel(32),
                           fl::Flags(BMF::ALPHA_CHANNEL));
                           //fl::ColourSpace(view->vpColourSpace));
                        redraw = true;
                     }

                     if (redraw) {
                        view->vpBuffer->BkgdIndex = 0;
                        view->vpBuffer->clear();

                        // A new state is allocated for drawing the children.  Clipping to the bitmap is enforced with
                        // the overflow values.

                        VectorState child_state;
                        child_state.mOverflowX = VOF::HIDDEN;
                        child_state.mOverflowY = VOF::HIDDEN;

                        auto save_bmp    = mBitmap;
                        auto save_format = mFormat;
                        auto save_rb     = mRenderBase;

                        // The vector paths will target the coordinate space of their parents, so we
                        // make adjustments to the bitmap to orient to (0,0).

                        mBitmap = view->vpBuffer;
                        auto save_data = mBitmap->offset(-view->vpBounds.left, -view->vpBounds.top);
                        mFormat.setBitmap(*view->vpBuffer);

                        draw_vectors((extVector *)view->Child, child_state);

                        mBitmap->Data = save_data;

                        mRenderBase = save_rb;
                        mBitmap     = save_bmp;
                        mFormat     = save_format;

                        //save_bitmap(view->vpBuffer, std::string("viewport"));
                     }

                     // Draw the cached bitmap buffer

                     view->BasePath.approximation_scale(view->Transform.scale());

                     auto transform = view->Transform;
                     transform.invert();

                     agg::rasterizer_scanline_aa<> raster;
                     raster.add_path(view->BasePath);

                     if (!state.mClipStack->empty()) {
                        agg::alpha_mask_gray8 alpha_mask(state.mClipStack->top().m_renderer);
                        agg::scanline_u8_am<agg::alpha_mask_gray8> masked_scanline(alpha_mask);
                        drawBitmap(masked_scanline, VSM::AUTO, mRenderBase, raster, view->vpBuffer, VSPREAD::REPEAT, view->Opacity, &transform);
                     }
                     else {
                        agg::scanline_u8 scanline;
                        drawBitmap(scanline, VSM::AUTO, mRenderBase, raster, view->vpBuffer, VSPREAD::REPEAT, view->Opacity, &transform);
                     }
                  }
                  else draw_vectors((extVector *)view->Child, state);
               }

               if (view->ClipMask) state.mClipStack->pop();

               if (view->vpClip) state.mClipStack->pop();

               mView = saved_viewport;

               mRenderBase.clip_box_naked(save_rb_clip);
            }
            else log.trace("Clipping boundary results in invisible viewport.");

            state.mClip = save_clip;
         }
      }
      else {
         if (shape->ClipMask) {
            state.mClipStack->emplace(state, shape->ClipMask, shape);
            state.mClipStack->top().draw(*this);
         }

         if (shape->GeneratePath) { // A vector that generates a path is one that can be drawn
            #ifdef DBG_DRAW
               log.traceBranch("%s: #%d, Mask: %p", get_name(shape), shape->UID, shape->ClipMask);
            #endif

            if (!mView) {
               // Vector shapes not inside a viewport cannot be drawn (they may exist as definitions for other objects,
               // e.g. as morph paths).
               return;
            }

            if (shape->FillRaster) {
               render_fill(state, *shape, *shape->FillRaster, shape->Fill[0]);
               if (shape->FGFill) render_fill(state, *shape, *shape->FillRaster, shape->Fill[1]);
            }

            if (shape->StrokeRaster) {
               render_stroke(state, *shape);
            }

            if ((shape->InputSubscriptions) or ((shape->Cursor != PTC::NIL) and (shape->Cursor != PTC::DEFAULT))) {
               // If the vector receives user input events then we record the collision box for the mouse cursor.

               TClipRectangle b;

               if (!shape->BasePath.empty()) {
                  if (shape->Transform.is_normal()) b = shape;
                  else {
                     auto path = shape->Bounds.as_path(shape->Transform);
                     b = get_bounds(path);
                  }

                  // Clipping masks can reduce the boundary further.

                  if (!state.mClipStack->empty()) {
                     auto &top = state.mClipStack->top();
                     if ((top.m_clip) and (top.m_clip->Bounds.valid())) {
                        // NB: This hasn't had much testing and doesn't consider nested clips.
                        // The Clip bounds should be post-transform
                        b.shrinking(top.m_clip->Bounds);
                     }
                  }
               }
               else b = { -1, -1, -1, -1 };

               const double abs_x = b.left;
               const double abs_y = b.top;

               TClipRectangle<double> rb_bounds = { double(mRenderBase.xmin()), double(mRenderBase.ymin()), double(mRenderBase.xmax()), double(mRenderBase.ymax()) };
               b.shrinking(rb_bounds);

               Scene->InputBoundaries.emplace_back(shape->UID, shape->Cursor, b, abs_x, abs_y, shape->InputSubscriptions ? false : true);
            }
         } // if: shape->GeneratePath

         if (shape->Child) draw_vectors((extVector *)shape->Child, state);

         if (shape->ClipMask) state.mClipStack->pop();
      }

      if (bmpBkgd) {
         agg::rasterizer_scanline_aa raster;

         basic_path(raster, 0, 0, bmpBkgd->Width, bmpBkgd->Height);

         mBitmap = bmpSave;
         mFormat.setBitmap(*mBitmap);
         if (!state.mClipStack->empty()) {
            agg::alpha_mask_gray8 alpha_mask(state.mClipStack->top().m_renderer);
            agg::scanline_u8_am<agg::alpha_mask_gray8> masked_scanline(alpha_mask);
            drawBitmap(masked_scanline, shape->Scene->SampleMethod, mRenderBase, raster, bmpBkgd, VSPREAD::CLIP, 1.0);
         }
         else {
            agg::scanline_u8 scanline;
            drawBitmap(scanline, shape->Scene->SampleMethod, mRenderBase, raster, bmpBkgd, VSPREAD::CLIP, 1.0);
         }
         FreeResource(bmpBkgd);
      }
   } // for loop
}

//********************************************************************************************************************
// For direct vector drawing via the API, no transforms.

void SimpleVector::DrawPath(objBitmap *Bitmap, double StrokeWidth, OBJECTPTR StrokeStyle, OBJECTPTR FillStyle)
{
   pf::Log log("draw_path");

   agg::scanline_u8  scanline;
   agg::pixfmt_psl   format;
   agg::trans_affine transform; // Dummy transform

   format.setBitmap(*Bitmap);
   mRenderer.attach(format);
   mRenderer.clip_box(Bitmap->Clip.Left, Bitmap->Clip.Top, Bitmap->Clip.Right-1, Bitmap->Clip.Bottom-1);
   //if (Gamma != 1.0) mRaster.gamma(agg::gamma_power(Gamma));

   log.traceBranch("Bitmap: %p, Stroke: %p (%s), Fill: %p (%s)", Bitmap, StrokeStyle, get_name(StrokeStyle), FillStyle, get_name(FillStyle));

   auto bounds = get_bounds(mPath);
   VectorState state;
   if (FillStyle) {
      mRaster.reset();
      mRaster.add_path(mPath);

      if (FillStyle->classID() IS CLASSID::VECTORCOLOUR) {
         auto colour = (objVectorColour *)FillStyle;
         agg::renderer_scanline_aa_solid solid(mRenderer);
         solid.color(agg::rgba(colour->Red, colour->Green, colour->Blue, colour->Alpha));
         agg::render_scanlines(mRaster, scanline, solid);
      }
      else if (FillStyle->classID() IS CLASSID::VECTORIMAGE) {
         extVectorImage &image = (extVectorImage &)*FillStyle;
         fill_image(state, bounds, mPath, VSM::AUTO, transform, Bitmap->Width, Bitmap->Height, image, mRenderer, mRaster);
      }
      else if (FillStyle->classID() IS CLASSID::VECTORGRADIENT) {
         extVectorGradient &gradient = (extVectorGradient &)*FillStyle;
         fill_gradient(state, bounds, &mPath, transform, Bitmap->Width, Bitmap->Height, gradient, &gradient.Colours->table, mRenderer, mRaster);
      }
      else if (FillStyle->classID() IS CLASSID::VECTORPATTERN) {
         fill_pattern(state, bounds, &mPath, VSM::AUTO, transform, Bitmap->Width, Bitmap->Height, (extVectorPattern &)*FillStyle, mRenderer, mRaster);
      }
      else log.warning("The FillStyle is not supported.");
   }

   if ((StrokeWidth > 0) and (StrokeStyle)) {
      if (StrokeStyle->classID() IS CLASSID::VECTORGRADIENT) {
         agg::conv_stroke<agg::path_storage> stroke_path(mPath);
         mRaster.reset();
         mRaster.add_path(stroke_path);

         extVectorGradient &gradient = (extVectorGradient &)*StrokeStyle;
         fill_gradient(state, bounds, &mPath, transform, Bitmap->Width, Bitmap->Height, gradient, &gradient.Colours->table, mRenderer, mRaster);
      }
      else if (StrokeStyle->classID() IS CLASSID::VECTORPATTERN) {
         agg::conv_stroke<agg::path_storage> stroke_path(mPath);
         mRaster.reset();
         mRaster.add_path(stroke_path);
         fill_pattern(state, bounds, &mPath, VSM::AUTO, transform, Bitmap->Width, Bitmap->Height, (extVectorPattern &)*StrokeStyle, mRenderer, mRaster);
      }
      else if (StrokeStyle->classID() IS CLASSID::VECTORIMAGE) {
         extVectorImage &image = (extVectorImage &)*StrokeStyle;
         agg::conv_transform<agg::path_storage, agg::trans_affine> path(mPath, transform);
         stroke_brush(state, image, mRenderer, path, StrokeWidth);
      }
      else if (StrokeStyle->classID() IS CLASSID::VECTORCOLOUR) {
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

void agg::pixfmt_psl::setBitmap(objBitmap &Bitmap, BLM BlendMode) noexcept
{
   if (BlendMode IS BLM::AUTO) {
      if (Bitmap.ColourSpace IS CS::LINEAR_RGB) BlendMode = BLM::LINEAR;
      else BlendMode = Bitmap.BlendMode;
   }

   rawBitmap(Bitmap.Data, Bitmap.Clip.Right, Bitmap.Clip.Bottom, Bitmap.LineWidth, Bitmap.BitsPerPixel, *Bitmap.ColourFormat, BlendMode);
}

void agg::pixfmt_psl::rawBitmap(uint8_t *Data, int Width, int Height, int Stride, int BitsPerPixel, ColourFormat &ColourFormat, BLM BlendMode) noexcept
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

      if (BlendMode IS BLM::LINEAR) {
         if (ColourFormat.AlphaPos IS 24) {
            if (ColourFormat.BluePos IS 0) {
               pixel_order(pxBGRA);
               fBlendPix = &linear32BGRA;
               fCopyPix  = &linearCopy32BGRA;
               fCoverPix = &linearCover32BGRA;
            }
            else {
               pixel_order(pxRGBA);
               fBlendPix = &linear32RGBA;
               fCopyPix  = &linearCopy32RGBA;
               fCoverPix = &linearCover32RGBA;
            }
         }
         else if (ColourFormat.RedPos IS 24) {
            pixel_order(pxAGBR);
            fBlendPix = &linear32AGBR;
            fCopyPix  = &linearCopy32AGBR;
            fCoverPix = &linearCover32AGBR;
         }
         else {
            pixel_order(pxARGB);
            fBlendPix = &linear32ARGB;
            fCopyPix  = &linearCopy32ARGB;
            fCoverPix = &linearCover32ARGB;
         }
      }
      else if (BlendMode IS BLM::SRGB) {
         if (ColourFormat.AlphaPos IS 24) {
            if (ColourFormat.BluePos IS 0) {
               pixel_order(pxBGRA);
               fBlendPix = &srgb32BGRA;
               fCopyPix  = &srgbCopy32BGRA;
               fCoverPix = &srgbCover32BGRA;
            }
            else {
               pixel_order(pxRGBA);
               fBlendPix = &srgb32RGBA;
               fCopyPix  = &srgbCopy32RGBA;
               fCoverPix = &srgbCover32RGBA;
            }
         }
         else if (ColourFormat.RedPos IS 24) {
            pixel_order(pxAGBR);
            fBlendPix = &srgb32AGBR;
            fCopyPix  = &srgbCopy32AGBR;
            fCoverPix = &srgbCover32AGBR;
         }
         else {
            pixel_order(pxARGB);
            fBlendPix = &srgb32ARGB;
            fCopyPix  = &srgbCopy32ARGB;
            fCoverPix = &srgbCover32ARGB;
         }
      }
      else { // BLM::GAMMA
         if (ColourFormat.AlphaPos IS 24) {
            if (ColourFormat.BluePos IS 0) {
               pixel_order(pxBGRA);
               fBlendPix = &gamma32BGRA;
               fCopyPix  = &gammaCopy32BGRA;
               fCoverPix = &gammaCover32BGRA;
            }
            else {
               pixel_order(pxRGBA);
               fBlendPix = &gamma32RGBA;
               fCopyPix  = &gammaCopy32RGBA;
               fCoverPix = &gammaCover32RGBA;
            }
         }
         else if (ColourFormat.RedPos IS 24) {
            pixel_order(pxAGBR);
            fBlendPix = &gamma32AGBR;
            fCopyPix  = &gammaCopy32AGBR;
            fCoverPix = &gammaCover32AGBR;
         }
         else {
            pixel_order(pxARGB);
            fBlendPix = &gamma32ARGB;
            fCopyPix  = &gammaCopy32ARGB;
            fCoverPix = &gammaCover32ARGB;
         }
      }
   }
   else if (BitsPerPixel IS 24) {
      pf::Log log;
      log.warning("Support for 24-bit bitmaps is deprecated.");
   }
   else if (BitsPerPixel IS 16) {
      // Deprecated.  16-bit client code should use 32-bit and downscale instead.
      pf::Log log;
      log.warning("Support for 16-bit bitmaps is deprecated.");
   }
   else if (BitsPerPixel IS 8) {
      // For generating grey-scale alpha masks
      fBlendHLine      = &blendHLine8;
      fBlendSolidHSpan = &blendSolidHSpan8;
      fBlendColorHSpan = &blendColorHSpan8;
      fCopyColorHSpan  = &copyColorHSpan8;
      fBlendPix        = &blend8;
      fCopyPix         = &copy8;
      fCoverPix        = &cover8;
   }
}
