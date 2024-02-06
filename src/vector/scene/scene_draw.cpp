
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
   std::stack<agg::trans_affine> mTransforms;

public:
   // The ClipBuffer is used to hold any alpha-masks that are generated as the scene is rendered.

   class ClipBuffer {
      VectorState *m_state;
      std::vector<UBYTE> m_bitmap;
      LONG m_width, m_height;
      extVector *m_shape;

      public:
      agg::rendering_buffer m_renderer;
      extVectorClip *m_clip;

      public:
      ClipBuffer() : m_shape(NULL), m_clip(NULL) { }
      ClipBuffer(VectorState &pState, extVectorClip *pClip, extVector *pShape) : m_state(&pState), m_shape(pShape), m_clip(pClip) { }
      void draw();
      void draw_clips(extVector *, agg::rasterizer_scanline_aa<> &, agg::renderer_scanline_aa_solid<agg::renderer_base<agg::pixfmt_gray8>> &, 
         agg::trans_affine &);
   };

private:
   constexpr DOUBLE view_width() {
      if (mView->vpDimensions & (DMF_FIXED_WIDTH|DMF_SCALED_WIDTH)) return mView->vpFixedWidth;
      else if (mView->vpViewWidth > 0) return mView->vpViewWidth;
      else return mView->Scene->PageWidth;
   }

   constexpr DOUBLE view_height() {
      if (mView->vpDimensions & (DMF_FIXED_HEIGHT|DMF_SCALED_HEIGHT)) return mView->vpFixedHeight;
      else if (mView->vpViewHeight > 0) return mView->vpViewHeight;
      else return mView->Scene->PageHeight;
   }

   void render_fill(VectorState &, extVector &, agg::rasterizer_scanline_aa<> &, extPainter &);
   void render_stroke(VectorState &, extVector &, agg::rasterizer_scanline_aa<> &);
   void draw_vectors(extVector *, VectorState &);
   static const agg::trans_affine build_fill_transform(extVector &, bool,  VectorState &);

public:
   extVectorScene *Scene; // The top-level VectorScene performing the draw.

   SceneRenderer(extVectorScene *pScene) : Scene(pScene) { }
   void draw(objBitmap *Bitmap);
};

//********************************************************************************************************************
// This class holds the current state as the vector scene is parsed for drawing.  It is most useful for managing
// inheritable values.

class VectorState {
public:
   TClipRectangle<DOUBLE> mClip; // Current clip region as defined by the viewports
   agg::line_join_e  mLineJoin;
   agg::line_cap_e   mLineCap;
   agg::inner_join_e mInnerJoin;
   std::shared_ptr<std::stack<SceneRenderer::ClipBuffer>> mClipStack;
   agg::trans_affine mTransform;
   double mOpacity;
   VIS    mVisible;
   VOF    mOverflowX, mOverflowY;
   bool   mLinearRGB;
   bool   mBackgroundActive;
   bool   mDirty;
   bool   mApplyTransform;

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
      mBackgroundActive(false),
      mDirty(false),
      mApplyTransform(false)
      { }
};

//********************************************************************************************************************
// Basic function for recursively drawing all child vectors to a bitmap mask.

void SceneRenderer::ClipBuffer::draw_clips(extVector *Shape, agg::rasterizer_scanline_aa<> &Rasterizer,
   agg::renderer_scanline_aa_solid<agg::renderer_base<agg::pixfmt_gray8>> &Solid, agg::trans_affine &Transform)
{
   agg::scanline32_p8 sl;
   for (auto scan=Shape; scan; scan=(extVector *)scan->Next) {
      if (scan->Class->BaseClassID IS ID_VECTOR) {
         if (!scan->BasePath.empty()) {
            auto t = scan->Transform * Transform;

            if (!scan->Stroked) { // Filled mask
               agg::conv_transform<agg::path_storage, agg::trans_affine> final_path(scan->BasePath, t);
               Rasterizer.reset();
               Rasterizer.add_path(final_path);
               agg::render_scanlines(Rasterizer, sl, Solid);
            }
            else { // Stroked mask
               agg::conv_stroke<agg::path_storage> stroked_path(scan->BasePath);
               configure_stroke(*scan, stroked_path);
               agg::conv_transform<agg::conv_stroke<agg::path_storage>, agg::trans_affine> final_path(stroked_path, t);

               Rasterizer.add_path(final_path);
               agg::render_scanlines(Rasterizer, sl, Solid);
            }
         }
      }

      if (scan->Child) draw_clips((extVector *)scan->Child, Rasterizer, Solid, Transform);
   }
}

//********************************************************************************************************************
// Called by the scene graph renderer to generate a bitmap mask.
// 
// TODO: Currently mask bitmaps are created and torn down on each drawing cycle.  We may be able to cache the bitmaps 
// with Vectors when they request a mask.  Bear in mind that caching has to be on a per-vector basis and not in the
// VectorClip itself due to the fact that a given VectorClip can be referenced by many vectors.

void SceneRenderer::ClipBuffer::draw()
{
   pf::Log log;

   // Ensure that the Bounds are up to date and refresh them if necessary.

   if ((m_clip->Child) and (!m_clip->RefreshBounds)) {
      if (check_branch_dirty((extVector *)m_clip->Child)) m_clip->RefreshBounds = true;
   }

   if (m_clip->RefreshBounds) {
      m_clip->RefreshBounds = false;
      m_clip->GeneratePath(m_clip);
   }

   if (m_clip->Bounds.left > m_clip->Bounds.right) return; // Return if no paths were defined.

   auto t_bound_path = m_clip->Bounds.as_path(m_shape->Transform);
   auto t_bound = get_bounds(t_bound_path);
   m_width  = F2T(t_bound.right) + 1;
   m_height = F2T(t_bound.bottom) + 1;

   if ((m_width <= 0) or (m_height <= 0)) {
      DEBUG_BREAK
      log.warning(ERR_InvalidDimension);
      return;
   }

   if (m_width > 8192)  m_width = 8192;
   if (m_height > 8192) m_height = 8192;

   #ifdef DBG_DRAW
      log.trace("%s #%d clipping mask with bounds %g %g %g %g (%dx%d)", m_shape->className(), m_shape->UID, t_bound.left, t_bound.top, t_bound.right, t_bound.bottom, m_width, m_height);
   #endif

   m_bitmap.resize(m_width * m_height);

   // Configure an 8-bit monochrome bitmap for holding the mask

   m_renderer.attach(m_bitmap.data(), m_width-1, m_height-1, m_width);
   agg::pixfmt_gray8 pixf(m_renderer);
   agg::renderer_base<agg::pixfmt_gray8> rb(pixf);
   agg::renderer_scanline_aa_solid<agg::renderer_base<agg::pixfmt_gray8>> solid(rb);
   agg::rasterizer_scanline_aa<> rasterizer;

   LONG x = F2T(t_bound.left);
   for (LONG y=t_bound.top * m_width; y < t_bound.bottom; y += m_width) {
      ClearMemory(m_bitmap.data() + y + x, m_width - x);
   }

   solid.color(agg::gray8(0xff, 0xff));

   // Every child vector of the VectorClip that exports a path will be rendered to the mask.

   auto transform = build_fill_transform(*m_shape, m_clip->ClipUnits IS VUNIT::USERSPACE, *m_state);

   if (m_clip->Child) draw_clips((extVector *)m_clip->Child, rasterizer, solid, transform);

   // A client can provide its own clipping path by setting the BasePath.  This is more optimal than
   // using child vectors - the VectorViewport is one such client that uses this feature.
   //
   // NB: The client should opt to use either BasePath or Children and not both - this is because
   // both will be additive when the intention may be for children to be additive and BasePath 
   // subtractive.

   if (!m_clip->BasePath.empty()) {
      agg::scanline32_p8 sl;
      agg::path_storage final_path(m_clip->BasePath);

      rasterizer.reset();
      rasterizer.add_path(final_path);
      agg::render_scanlines(rasterizer, sl, solid);
   }
}

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
         s->r = p[m_src->oR];
         s->g = p[m_src->oG];
         s->b = p[m_src->oB];
         s->a = p[m_src->oA];
         p = (const value_type*)next_x();
         ++s;
      } while(--len);
   }

  int8u * span(int x, int y, unsigned) {
      m_x = x;
      m_row_ptr = m_src->row_ptr(m_wrap_y(y));
      return m_row_ptr + m_wrap_x(x) * 4;
  }

  int8u * next_x() {
      int x = ++m_wrap_x;
      return m_row_ptr + x * 4;
  }

  int8u * next_y() {
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
         s->r = p[m_src->oR];
         s->g = p[m_src->oG];
         s->b = p[m_src->oB];
         s->a = p[m_src->oA];
         p = (const value_type*)next_x();
         ++s;
      } while(--len);
   }

   int8u * span(int x, int y, unsigned) {
      m_x = x;
      m_row_ptr = m_src->row_ptr(m_wrap_y(y));
      return m_row_ptr + m_wrap_x(x) * 4;
   }

   int8u * next_x() {
      int x = ++m_wrap_x;
      return m_row_ptr + x * 4;
   }

   int8u * next_y() {
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
// Check a Shape, its siblings and children for dirty markers.

static bool check_dirty(extVector *Shape) {
   while (Shape) {
      if (Shape->Class->BaseClassID != ID_VECTOR) return true;
      if (Shape->dirty()) return true;

      if (Shape->Child) {
         if (check_dirty((extVector *)Shape->Child)) return true;
      }
      Shape = (extVector *)Shape->Next;
   }
   return false;
}

//********************************************************************************************************************
// Return the correct transformation matrix for a fill operation.

const agg::trans_affine SceneRenderer::build_fill_transform(extVector &Vector, bool Userspace,  VectorState &State)
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
// Generic function for setting the clip region of an AGG rasterizer

void set_raster_clip(agg::rasterizer_scanline_aa<> &Raster, LONG X, LONG Y, LONG Width, LONG Height)
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

void set_filter(agg::image_filter_lut &Filter, VSM Method)
{
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
      case VSM::SINC3:     Filter.calculate(agg::image_filter_sinc(3.0), true); break;
      case VSM::LANCZOS3:  Filter.calculate(agg::image_filter_lanczos(3.0), true); break;
      case VSM::BLACKMAN3: Filter.calculate(agg::image_filter_blackman(3.0), true); break;
      case VSM::SINC8:     Filter.calculate(agg::image_filter_sinc(8.0), true); break;
      case VSM::LANCZOS8:  Filter.calculate(agg::image_filter_lanczos(8.0), true); break;
      case VSM::BLACKMAN8: Filter.calculate(agg::image_filter_blackman(8.0), true); break;
      default: {
         pf::Log log;
         log.warning("Unrecognised sampling method %d", LONG(Method));
         Filter.calculate(agg::image_filter_bicubic(), true);
         break;
      }
   }
}

//********************************************************************************************************************
// A generic drawing function for VMImage and VMPattern, this is used to fill vectors with bitmap images.

template <class T> void drawBitmap(T &Scanline, VSM SampleMethod, agg::renderer_base<agg::pixfmt_psl> &RenderBase, agg::rasterizer_scanline_aa<> &Raster,
   objBitmap *SrcBitmap, VSPREAD SpreadMethod, DOUBLE Opacity, agg::trans_affine *Transform = NULL, DOUBLE XOffset = 0, DOUBLE YOffset = 0)
{
   agg::pixfmt_psl pixels(*SrcBitmap);

   if ((Transform) and (Transform->is_complex())) {
      agg::span_interpolator_linear interpolator(*Transform);
      agg::image_filter_lut filter;
      set_filter(filter, SampleMethod);  // Set the interpolation filter to use.

      if (SpreadMethod IS VSPREAD::REFLECT_X) {
         agg::span_reflect_x source(pixels, XOffset, YOffset);
         agg::span_image_filter_rgba<agg::span_reflect_x, agg::span_interpolator_linear<>> spangen(source, interpolator, filter);
         drawBitmapRender(Scanline, RenderBase, Raster, spangen, Opacity);
      }
      else if (SpreadMethod IS VSPREAD::REFLECT_Y) {
         agg::span_reflect_y source(pixels, XOffset, YOffset);
         agg::span_image_filter_rgba<agg::span_reflect_y, agg::span_interpolator_linear<>> spangen(source, interpolator, filter);
         drawBitmapRender(Scanline, RenderBase, Raster, spangen, Opacity);
      }
      else if (SpreadMethod IS VSPREAD::REPEAT) {
         agg::span_repeat_rkl source(pixels, XOffset, YOffset);
         agg::span_image_filter_rgba<agg::span_repeat_rkl, agg::span_interpolator_linear<>> spangen(source, interpolator, filter);
         drawBitmapRender(Scanline, RenderBase, Raster, spangen, Opacity);
      }
      else { // VSPREAD::PAD and VSPREAD::CLIP modes.
         agg::span_once<agg::pixfmt_psl> source(pixels, XOffset, YOffset);
         agg::span_image_filter_rgba<agg::span_once<agg::pixfmt_psl>, agg::span_interpolator_linear<>> spangen(source, interpolator, filter);
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
         agg::span_repeat_rkl source(pixels, XOffset, YOffset);
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

//********************************************************************************************************************

static void stroke_brush(VectorState &State, const objVectorImage &Image, agg::renderer_base<agg::pixfmt_psl> &RenderBase,
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

void SceneRenderer::draw(objBitmap *Bitmap) 
{
   pf::Log log;

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
   }
}

//********************************************************************************************************************

void SceneRenderer::render_fill(VectorState &State, extVector &Vector, agg::rasterizer_scanline_aa<> &Raster, extPainter &Painter) 
{
   // Think of the vector's path as representing a mask for the fill algorithm.  Any transforms applied to
   // an image/gradient fill are independent of the path.

   if (Vector.FillRule IS VFR::NON_ZERO) Raster.filling_rule(agg::fill_non_zero);
   else if (Vector.FillRule IS VFR::EVEN_ODD) Raster.filling_rule(agg::fill_even_odd);

   // Solid colour.  Bitmap fonts will set DisableFill.Colour to ensure texture maps are used instead

   if ((Painter.Colour.Alpha > 0) and (!Vector.DisableFillColour)) {
      auto colour = agg::rgba(Painter.Colour, Painter.Colour.Alpha * Vector.FillOpacity * State.mOpacity);

      if ((Vector.PathQuality IS RQ::CRISP) or (Vector.PathQuality IS RQ::FAST)) {
         agg::renderer_scanline_bin_solid renderer(mRenderBase);
         renderer.color(colour);

         if (!State.mClipStack->empty()) {
            agg::alpha_mask_gray8 alpha_mask(State.mClipStack->top().m_renderer);
            agg::scanline_u8_am<agg::alpha_mask_gray8> mScanLineMasked(alpha_mask);
            agg::render_scanlines(Raster, mScanLineMasked, renderer);
         }
         else agg::render_scanlines(Raster, mScanLine, renderer);
      }
      else {
         agg::renderer_scanline_aa_solid renderer(mRenderBase);
         renderer.color(colour);

         if (!State.mClipStack->empty()) {
            agg::alpha_mask_gray8 alpha_mask(State.mClipStack->top().m_renderer);
            agg::scanline_u8_am<agg::alpha_mask_gray8> mScanLineMasked(alpha_mask);
            agg::render_scanlines(Raster, mScanLineMasked, renderer);
         }
         else agg::render_scanlines(Raster, mScanLine, renderer);
      }
   }

   if (Painter.Image) { // Bitmap image fill.  NB: The SVG class creates a standard VectorRectangle and associates an image with it in order to support <image> tags.
      fill_image(State, Vector.Bounds, Vector.BasePath, Vector.Scene->SampleMethod, build_fill_transform(Vector, Painter.Image->Units IS VUNIT::USERSPACE, State),
         view_width(), view_height(), *Painter.Image, mRenderBase, Raster, Vector.FillOpacity * State.mOpacity);
   }

   if (Painter.Gradient) {
      if (auto table = get_fill_gradient_table(Painter, State.mOpacity * Vector.FillOpacity)) {
         fill_gradient(State, Vector.Bounds, &Vector.BasePath, build_fill_transform(Vector, Painter.Gradient->Units IS VUNIT::USERSPACE, State),
            view_width(), view_height(), *((extVectorGradient *)Painter.Gradient), table, mRenderBase, Raster);
      }
   }

   if (Painter.Pattern) {
      fill_pattern(State, Vector.Bounds, &Vector.BasePath, Vector.Scene->SampleMethod, build_fill_transform(Vector, Painter.Pattern->Units IS VUNIT::USERSPACE, State),
         view_width(), view_height(), *((extVectorPattern *)Painter.Pattern), mRenderBase, Raster);
   }
}

//********************************************************************************************************************

void SceneRenderer::render_stroke(VectorState &State, extVector &Vector, agg::rasterizer_scanline_aa<> &Raster) 
{
   if (Vector.Scene->Gamma != 1.0) Raster.gamma(agg::gamma_power(Vector.Scene->Gamma));

   if (Vector.FillRule IS VFR::NON_ZERO) Raster.filling_rule(agg::fill_non_zero);
   else if (Vector.FillRule IS VFR::EVEN_ODD) Raster.filling_rule(agg::fill_even_odd);

   if (Vector.Stroke.Gradient) {
      if (auto table = get_stroke_gradient_table(Vector)) {
         fill_gradient(State, Vector.Bounds, &Vector.BasePath, build_fill_transform(Vector, ((extVectorGradient *)Vector.Stroke.Gradient)->Units IS VUNIT::USERSPACE, State),
            view_width(), view_height(), *((extVectorGradient *)Vector.Stroke.Gradient), table, mRenderBase, Raster);
      }
   }
   else if (Vector.Stroke.Pattern) {
      fill_pattern(State, Vector.Bounds, &Vector.BasePath, Vector.Scene->SampleMethod, build_fill_transform(Vector, Vector.Stroke.Pattern->Units IS VUNIT::USERSPACE, State),
         view_width(), view_height(), *((extVectorPattern *)Vector.Stroke.Pattern), mRenderBase, Raster);
   }
   else if (Vector.Stroke.Image) {
      DOUBLE stroke_width = Vector.fixed_stroke_width() * Vector.Transform.scale();
      if (stroke_width < 1) stroke_width = 1;

      auto transform = Vector.Transform * State.mTransform;
      agg::conv_transform<agg::path_storage, agg::trans_affine> stroke_path(Vector.BasePath, transform);

      stroke_brush(State, *Vector.Stroke.Image, mRenderBase, stroke_path, stroke_width);
   }
   else { // Solid colour
      if ((Vector.PathQuality IS RQ::CRISP) or (Vector.PathQuality IS RQ::FAST)) {
         agg::renderer_scanline_bin_solid renderer(mRenderBase);
         renderer.color(agg::rgba(Vector.Stroke.Colour, Vector.Stroke.Colour.Alpha * Vector.StrokeOpacity * State.mOpacity));

         if (!State.mClipStack->empty()) {
            agg::alpha_mask_gray8 alpha_mask(State.mClipStack->top().m_renderer);
            agg::scanline_u8_am<agg::alpha_mask_gray8> mScanLineMasked(alpha_mask);
            agg::render_scanlines(Raster, mScanLineMasked, renderer);
         }
         else agg::render_scanlines(Raster, mScanLine, renderer);
      }
      else {
         agg::renderer_scanline_aa_solid renderer(mRenderBase);
         renderer.color(agg::rgba(Vector.Stroke.Colour, Vector.Stroke.Colour.Alpha * Vector.StrokeOpacity * State.mOpacity));

         if (!State.mClipStack->empty()) {
            agg::alpha_mask_gray8 alpha_mask(State.mClipStack->top().m_renderer);
            agg::scanline_u8_am<agg::alpha_mask_gray8> mScanLineMasked(alpha_mask);
            agg::render_scanlines(Raster, mScanLineMasked, renderer);
         }
         else agg::render_scanlines(Raster, mScanLine, renderer);
      }
   }
}

// This is the main routine for parsing the vector tree for drawing.

void SceneRenderer::draw_vectors(extVector *CurrentVector, VectorState &ParentState) {
   for (auto shape=CurrentVector; shape; shape=(extVector *)shape->Next) {
      pf::Log log(__FUNCTION__);
      VectorState state = VectorState(ParentState);

      if (shape->Class->BaseClassID != ID_VECTOR) {
         log.trace("Non-Vector discovered in the vector tree.");
         continue;
      }
      else if (!shape->Scene) continue;

      if (shape->dirty()) {
         gen_vector_path(shape);
         shape->Dirty = RC::NIL;
      }
      else log.trace("%s: #%d, Dirty: NO, ParentView: #%d", shape->Class->ClassName, shape->UID, shape->ParentView ? shape->ParentView->UID : 0);

      // Visibility management.

      {
         bool visible = true;
         if (shape->Visibility IS VIS::INHERIT) {
            if (ParentState.mVisible != VIS::VISIBLE) visible = false;
         }
         else if (shape->Visibility != VIS::VISIBLE) visible = false;

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
            gfxCopyArea(bmp, mBitmap, BAF::BLEND|BAF::COPY, 0, 0, bmp->Width, bmp->Height, 0, 0);
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
               fl::Flags(BMF::ALPHA_CHANNEL),
               fl::ColourSpace(mBitmap->ColourSpace)))) {
            bmpSave = mBitmap;
            mBitmap = bmpBkgd;
            mFormat.setBitmap(*bmpBkgd);
            ClearMemory(bmpBkgd->Data, bmpBkgd->LineWidth * bmpBkgd->Height);
            state.mBackgroundActive = true;
         }
      }

      if (shape->Class->ClassID IS ID_VECTORVIEWPORT) {
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
               if (view->vpClipMask) {
                  state.mClipStack->emplace(state, view->vpClipMask, view);
                  state.mClipStack->top().draw();
               }

               if (view->ClipMask) {
                  state.mClipStack->emplace(state, view->ClipMask, view);
                  state.mClipStack->top().draw();
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
                  // them being pre-rasterised as they normally would be for primitive vectors.
                  //
                  // The client can expect a result that would match that of the pattern's viewport being placed
                  // as a child of this one.  NB: There is a performance penalty in that transforms will be
                  // applied in realtime.

                  auto s_transform = state.mTransform;
                  auto s_apply     = state.mApplyTransform;
                  state.mTransform      = view->Transform;
                  state.mApplyTransform = true;

                  if (view->Fill[0].Pattern->Units IS VUNIT::BOUNDING_BOX) {
                     view->Fill[0].Pattern->Scene->setPageWidth(view->vpFixedWidth);
                     view->Fill[0].Pattern->Scene->setPageHeight(view->vpFixedHeight);
                  }

                  draw_vectors(((extVectorPattern *)view->Fill[0].Pattern)->Viewport, state);

                  if ((view->FGFill) and (view->Fill[1].Pattern)) {
                     if (view->Fill[1].Pattern->Units IS VUNIT::BOUNDING_BOX) {
                        view->Fill[1].Pattern->Scene->setPageWidth(view->vpFixedWidth);
                        view->Fill[1].Pattern->Scene->setPageHeight(view->vpFixedHeight);
                     }

                     draw_vectors(((extVectorPattern *)view->Fill[1].Pattern)->Viewport, state);
                  }

                  state.mTransform      = s_transform;
                  state.mApplyTransform = s_apply;
               }

               if (view->Child) draw_vectors((extVector *)view->Child, state);

               if (view->ClipMask) state.mClipStack->pop();

               if (view->vpClipMask) state.mClipStack->pop();

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
            state.mClipStack->top().draw();
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
               if (state.mApplyTransform) {
                  // Run-time transforms in the draw process aren't ideal as they require the path to be processed
                  // every time.  It's necessary if the client wants to re-use vectors though (saving resources and gaining
                  // some conveniences).
                  auto transform = shape->Transform * state.mTransform;
                  agg::conv_transform<agg::path_storage, agg::trans_affine> final_path(shape->BasePath, transform);
                  agg::rasterizer_scanline_aa raster;
                  raster.add_path(final_path);
                  render_fill(state, *shape, raster, shape->Fill[0]);
                  if (shape->FGFill) render_fill(state, *shape, raster, shape->Fill[1]);
               }
               else {
                  render_fill(state, *shape, *shape->FillRaster, shape->Fill[0]);
                  if (shape->FGFill) render_fill(state, *shape, *shape->FillRaster, shape->Fill[1]);
               }
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

                  if ((!state.mClipStack->empty()) and (!state.mClipStack->top().m_clip->BasePath.empty())) {
                     agg::conv_transform<agg::path_storage, agg::trans_affine> path(state.mClipStack->top().m_clip->BasePath, shape->Transform);
                     b.shrinking(get_bounds(path));
                  }
               }
               else b = { -1, -1, -1, -1 };

               const DOUBLE abs_x = b.left;
               const DOUBLE abs_y = b.top;

               TClipRectangle<DOUBLE> rb_bounds = { DOUBLE(mRenderBase.xmin()), DOUBLE(mRenderBase.ymin()), DOUBLE(mRenderBase.xmax()), DOUBLE(mRenderBase.ymax()) };
               b.shrinking(rb_bounds);

               Scene->InputBoundaries.emplace_back(shape->UID, shape->Cursor, b, abs_x, abs_y, shape->InputSubscriptions ? false : true);
            }
         } // if: shape->GeneratePath

         if (shape->Child) draw_vectors((extVector *)shape->Child, state);

         if (shape->ClipMask) state.mClipStack->pop();
      }

      if (bmpBkgd) {
         agg::rasterizer_scanline_aa raster;

         agg::path_storage clip;
         clip.move_to(0, 0);
         clip.line_to(bmpBkgd->Width, 0);
         clip.line_to(bmpBkgd->Width, bmpBkgd->Height);
         clip.line_to(0, bmpBkgd->Height);
         clip.close_polygon();
         raster.add_path(clip);

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
   }
}

//********************************************************************************************************************
// For direct vector drawing

void SimpleVector::DrawPath(objBitmap *Bitmap, DOUBLE StrokeWidth, OBJECTPTR StrokeStyle, OBJECTPTR FillStyle)
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

      if (FillStyle->Class->ClassID IS ID_VECTORCOLOUR) {
         auto colour = (objVectorColour *)FillStyle;
         agg::renderer_scanline_aa_solid solid(mRenderer);
         solid.color(agg::rgba(colour->Red, colour->Green, colour->Blue, colour->Alpha));
         agg::render_scanlines(mRaster, scanline, solid);
      }
      else if (FillStyle->Class->ClassID IS ID_VECTORIMAGE) {
         objVectorImage &image = (objVectorImage &)*FillStyle;
         fill_image(state, bounds, mPath, VSM::AUTO, transform, Bitmap->Width, Bitmap->Height, image, mRenderer, mRaster);
      }
      else if (FillStyle->Class->ClassID IS ID_VECTORGRADIENT) {
         extVectorGradient &gradient = (extVectorGradient &)*FillStyle;
         fill_gradient(state, bounds, &mPath, transform, Bitmap->Width, Bitmap->Height, gradient, &gradient.Colours->table, mRenderer, mRaster);
      }
      else if (FillStyle->Class->ClassID IS ID_VECTORPATTERN) {
         fill_pattern(state, bounds, &mPath, VSM::AUTO, transform, Bitmap->Width, Bitmap->Height, (extVectorPattern &)*FillStyle, mRenderer, mRaster);
      }
      else log.warning("The FillStyle is not supported.");
   }

   if ((StrokeWidth > 0) and (StrokeStyle)){
      if (StrokeStyle->Class->ClassID IS ID_VECTORGRADIENT) {
         agg::conv_stroke<agg::path_storage> stroke_path(mPath);
         mRaster.reset();
         mRaster.add_path(stroke_path);

         extVectorGradient &gradient = (extVectorGradient &)*StrokeStyle;
         fill_gradient(state, bounds, &mPath, transform, Bitmap->Width, Bitmap->Height, gradient, &gradient.Colours->table, mRenderer, mRaster);
      }
      else if (StrokeStyle->Class->ClassID IS ID_VECTORPATTERN) {
         agg::conv_stroke<agg::path_storage> stroke_path(mPath);
         mRaster.reset();
         mRaster.add_path(stroke_path);
         fill_pattern(state, bounds, &mPath, VSM::AUTO, transform, Bitmap->Width, Bitmap->Height, (extVectorPattern &)*StrokeStyle, mRenderer, mRaster);
      }
      else if (StrokeStyle->Class->ClassID IS ID_VECTORIMAGE) {
         objVectorImage &image = (objVectorImage &)*StrokeStyle;
         agg::conv_transform<agg::path_storage, agg::trans_affine> path(mPath, transform);
         stroke_brush(state, image, mRenderer, path, StrokeWidth);
      }
      else if (StrokeStyle->Class->ClassID IS ID_VECTORCOLOUR) {
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
      pf::Log log;
      log.warning("Support for 16-bit bitmaps is deprecated.");
   }
}
