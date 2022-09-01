// TODO: Real-time calls to bounding_rect_single() are inefficient.  We should be caching the bounding
// box with each vector and using the dirty markers to manage updates.

//****************************************************************************
// This class holds the current state as the vector scene is parsed for drawing.  It is most useful for managing use of
// the 'inherit' attribute values.

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
   objVectorClip *mClipMask;
   agg::trans_affine mTransform;

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
      mClipMask(NULL) { }
};

//****************************************************************************
// This class is used for rendering images with a pre-defined opacity.

class spanconv_image
{
public:
   spanconv_image(DOUBLE Alpha) : alpha(Alpha) { }

   void prepare() { }

   void generate(agg::rgba8 * span, int x, int y, unsigned len) const {
      do {
         span->a = span->a * alpha;
         ++span;
      } while(--len);
   }

private:
   DOUBLE alpha;
};

//****************************************************************************

static bool check_dirty(objVector *Shape) {
   while (Shape) {
      if (Shape->Head.ClassID != ID_VECTOR) return true;
      if (Shape->Dirty) return true;

      if (Shape->Child) {
         if (check_dirty(Shape->Child)) return true;
      }
      Shape = Shape->Next;
   }
   return false;
}

//****************************************************************************
// Return the correct transformation matrix for a fill operation.

static const agg::trans_affine build_fill_transform(objVector &Vector, bool Userspace,  VectorState &State)
{
   if (Userspace) { // Userspace: Ignores the vector, but inherits parent transforms
      agg::trans_affine parents;
      apply_parent_transforms((objVector *)get_parent(&Vector), parents);
      return parents;
   }
   else if (State.mApplyTransform) { // BoundingBox with a real-time transform
      agg::trans_affine transform = Vector.Transform * State.mTransform;
      return transform;
   }
   else return Vector.Transform; // Default BoundingBox: The vector and its parent transforms apply.
}

//****************************************************************************

template <class T> static void drawBitmapRender(agg::renderer_base<agg::pixfmt_rkl> &RenderBase,
   agg::rasterizer_scanline_aa<> &Raster, T &spangen, DOUBLE Opacity)
{
   agg::span_allocator<agg::rgba8> spanalloc;
   agg::scanline_u8 scanline;
   if (Opacity < 1.0) {
      spanconv_image sci(Opacity);
      agg::span_converter<T, spanconv_image> sc(spangen, sci);
      agg::render_scanlines_aa(Raster, scanline, RenderBase, spanalloc, sc);
   }
   else agg::render_scanlines_aa(Raster, scanline, RenderBase, spanalloc, spangen);
}

//****************************************************************************

static void setRasterClip(agg::rasterizer_scanline_aa<> &Raster, LONG X, LONG Y, LONG Width, LONG Height)
{
   agg::path_storage clip;
   clip.move_to(X, Y);
   clip.line_to(X+Width, Y);
   clip.line_to(X+Width, Y+Height);
   clip.line_to(X, Y+Height);
   clip.close_polygon();
   Raster.reset();
   Raster.add_path(clip);
}

//****************************************************************************

static void set_filter(agg::image_filter_lut &Filter, UBYTE Method)
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

//****************************************************************************
// A generic drawing function for VMImage and VMPattern, this is used to fill vectors with bitmap images.

static void drawBitmap(LONG SampleMethod, agg::renderer_base<agg::pixfmt_rkl> &RenderBase, agg::rasterizer_scanline_aa<> &Raster,
   objBitmap *SrcBitmap, LONG SpreadMethod, DOUBLE Opacity, agg::trans_affine *Transform = NULL, DOUBLE XOffset = 0, DOUBLE YOffset = 0)
{
   agg::rendering_buffer imgSource;
   imgSource.attach(SrcBitmap->Data, SrcBitmap->Width, SrcBitmap->Height, SrcBitmap->LineWidth);
   agg::pixfmt_rkl pixels(*SrcBitmap);

   if ((Transform) and // Interpolate only if the transform specifies a scale, shear or rotate operation.
       ((Transform->sx != 1.0) or (Transform->sy != 1.0) or (Transform->shx != 0.0) or (Transform->shy != 0.0))) {
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
      else { // Cater for path VSPREAD_PAD and VSPREAD_CLIP modes.
         agg::span_pattern_rkl<agg::pixfmt_rkl> source(pixels, XOffset, YOffset);
         agg::span_image_filter_rgba<agg::span_pattern_rkl<agg::pixfmt_rkl>, agg::span_interpolator_linear<>> spangen(source, interpolator, filter);
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
      else { // Cater for path VSPREAD_PAD and VSPREAD_CLIP modes.
         agg::span_pattern_rkl<agg::pixfmt_rkl> source(pixels, XOffset, YOffset);
         drawBitmapRender(RenderBase, Raster, source, Opacity);
      }
   }
}

//********************************************************************************************************************
// Patterns are rendered internally as an independent bitmap.  That bitmap is then copied to the target bitmap with
// the necessary transforms applied.  One of the primary advantages of patterns is that once rendered, they remain
// cached until changed.

static void draw_pattern(objVector *Vector, agg::path_storage *Path,
   LONG SampleMethod, const agg::trans_affine &Transform, DOUBLE ViewWidth, DOUBLE ViewHeight,
   struct rkVectorPattern &Pattern, agg::renderer_base<agg::pixfmt_rkl> &RenderBase,
   agg::rasterizer_scanline_aa<> &Raster)
{
   if (Pattern.Scene) {  // Redraw the pattern source if any part of the definition is marked as dirty.
      if ((check_dirty(Pattern.Scene->Viewport)) or (!Pattern.Bitmap)) {
         acDraw(&Pattern);
      }
   }

   DOUBLE dx, dy;

   if (Pattern.Units & VUNIT_USERSPACE) { // Use fixed coordinates specified in the pattern.
      if (Pattern.Dimensions & DMF_RELATIVE_X) dx = ViewWidth * Pattern.X;
      else if (Pattern.Dimensions & DMF_FIXED_X) dx = Pattern.X;
      else dx = 0;

      if (Pattern.Dimensions & DMF_RELATIVE_Y) dy = ViewHeight * Pattern.Y;
      else if (Pattern.Dimensions & DMF_FIXED_Y) dy = Pattern.Y;
      else dy = 0;
   }
   else {
      // VUNIT_BOUNDING_BOX (align to vector).  In this mode the pattern (x,y) is an optional offset applied
      // to the base position, which is taken from the vector's path.

      DOUBLE bx1, by1, bx2, by2;
      bounding_rect_single(*Path, 0, &bx1, &by1, &bx2, &by2); // Get the boundary of the original path without transforms.
      const DOUBLE width = bx2 - bx1;
      const DOUBLE height = by2 - by1;
      dx = 0;
      dy = 0;
      if (Pattern.Dimensions & DMF_RELATIVE_X) dx += (width * Pattern.X);
      else if (Pattern.Dimensions & DMF_FIXED_X) dx += Pattern.X;

      if (Pattern.Dimensions & DMF_RELATIVE_Y) dy += (height * Pattern.Y);
      else if (Pattern.Dimensions & DMF_FIXED_Y) dy += Pattern.Y;
   }

   agg::trans_affine transform;

   if (Pattern.Matrices) {
      auto &m = *Pattern.Matrices;
      transform.load_all(m.ScaleX, m.ShearY, m.ShearX, m.ScaleY, m.TranslateX + dx, m.TranslateY + dy);
   }
   else transform.translate(dx, dy);

   if (!(Pattern.Units & VUNIT_USERSPACE)) transform *= Transform;

   transform.invert(); // Required

   drawBitmap(SampleMethod, RenderBase, Raster, Pattern.Bitmap, Pattern.SpreadMethod, Pattern.Opacity, &transform);
}

//****************************************************************************
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
         return agg::rgba8(UnpackRed(Pattern.mBitmap, p), UnpackGreen(Pattern.mBitmap, p), UnpackBlue(Pattern.mBitmap, p));
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

void draw_brush(const struct rkVectorImage &Image,
   agg::renderer_base<agg::pixfmt_rkl> &RenderBase,
   agg::conv_transform<agg::path_storage, agg::trans_affine> &Path,
   DOUBLE StrokeWidth)
{
   typedef agg::pattern_filter_bilinear_rgba8 FILTER_TYPE;
   FILTER_TYPE filter;
   agg::rendering_buffer img;
   img.attach(Image.Bitmap->Data, Image.Bitmap->Width, Image.Bitmap->Height, Image.Bitmap->LineWidth);
   pattern_rgb src(*Image.Bitmap, StrokeWidth);

   DOUBLE scale;
   if (StrokeWidth IS (DOUBLE)Image.Bitmap->Height) scale = 1.0;
   else scale = (DOUBLE)StrokeWidth / (DOUBLE)Image.Bitmap->Height;

   if (isPow2((ULONG)Image.Bitmap->Width)) { // If the image width is a power of 2, use this optimised version
      typedef agg::line_image_pattern_pow2<FILTER_TYPE> pattern_type;
      pattern_type pattern(filter);
      agg::renderer_outline_image<agg::renderer_base<agg::pixfmt_rkl>, pattern_type> ren_img(RenderBase, pattern);
      agg::rasterizer_outline_aa<agg::renderer_outline_image<agg::renderer_base<agg::pixfmt_rkl>, pattern_type>> ras_img(ren_img);

      //ren_img.start_x(scale); // Optional offset

      pattern.create(src); // Configures the line pattern
      if (scale != 1.0) ren_img.scale_x(scale);
      ras_img.add_path(Path);
   }
   else { // Slightly slower version for non-power of 2 textures.
      typedef agg::line_image_pattern<FILTER_TYPE> pattern_type;
      pattern_type pattern(filter);
      agg::renderer_outline_image<agg::renderer_base<agg::pixfmt_rkl>, pattern_type> ren_img(RenderBase, pattern);
      agg::rasterizer_outline_aa<agg::renderer_outline_image<agg::renderer_base<agg::pixfmt_rkl>, pattern_type>> ras_img(ren_img);

      //ren_img.start_x(scale);

      pattern.create(src);
      if (scale != 1.0) ren_img.scale_x(scale);
      ras_img.add_path(Path);
   }
}

//****************************************************************************
// Image extension

static void draw_image(objVector *Vector, agg::path_storage &Path, LONG SampleMethod,
   const agg::trans_affine &Transform, DOUBLE ViewWidth, DOUBLE ViewHeight,
   rkVectorImage &Image, agg::renderer_base<agg::pixfmt_rkl> &RenderBase,
   agg::rasterizer_scanline_aa<> &Raster, DOUBLE BorderWidth = 0, DOUBLE Alpha = 1.0)
{
   DOUBLE dx, dy, bx1, by1, bx2, by2;
   agg::trans_affine transform;

   if (Image.Units & VUNIT_USERSPACE) { // Align to the provided x/y coordinate in rkVectorImage.
      if (Image.Dimensions & DMF_RELATIVE_X) dx = ViewWidth * Image.X;
      else dx = Image.X;

      if (Image.Dimensions & DMF_RELATIVE_Y) dy = ViewHeight * Image.Y;
      else dy = Image.Y;

      if (Image.SpreadMethod IS VSPREAD_PAD) { // In pad mode, stretch the image to fit the boundary
         bounding_rect_single(Path, 0, &bx1, &by1, &bx2, &by2); // Get the boundary of the original path without transforms.
         transform.scale((bx2-bx1) / Image.Bitmap->Width, (by2-by1) / Image.Bitmap->Height);
      }
   }
   else {
      // VUNIT_BOUNDING_BOX (align to vector).  In this mode the image's (x,y) is an optional offset applied
      // to the base position, which is taken from the vector's path.

      bounding_rect_single(Path, 0, &bx1, &by1, &bx2, &by2); // Get the boundary of the original path without transforms.
      const DOUBLE width = bx2 - bx1;
      const DOUBLE height = by2 - by1;
      if (Image.Dimensions & DMF_RELATIVE_X) dx = (width * Image.X);
      else dx = Image.X;

      if (Image.Dimensions & DMF_RELATIVE_Y) dy = (height * Image.Y);
      else dy = Image.Y;

      if (Image.SpreadMethod IS VSPREAD_PAD) { // In pad mode, stretch the image to fit the boundary
         transform.scale(width / Image.Bitmap->Width, height / Image.Bitmap->Height);
      }
   }

   transform *= Transform;

   transform.invert();

   drawBitmap(SampleMethod, RenderBase, Raster, Image.Bitmap, Image.SpreadMethod, Alpha, &transform);
}

//*****************************************************************************
// Gradient extension
// TODO: Support gradient_xy (rounded corner), gradient_sqrt_xy

static void draw_gradient(objVector *Vector, agg::path_storage *Path, const agg::trans_affine &Transform,
   DOUBLE ViewWidth, DOUBLE ViewHeight, const rkVectorGradient &Gradient,
   GRADIENT_TABLE *Table,
   agg::renderer_base<agg::pixfmt_rkl> &RenderBase,
   agg::rasterizer_scanline_aa<> &Raster,
   DOUBLE BorderWidth)
{
   typedef agg::span_interpolator_linear<> interpolator_type;
   typedef agg::span_allocator<agg::rgba8> span_allocator_type;
   typedef agg::pod_auto_array<agg::rgba8, 256> color_array_type;
   typedef agg::renderer_base<agg::pixfmt_rkl>  RENDERER_BASE_TYPE;

   agg::scanline_u8    scanline;
   agg::trans_affine   transform;
   interpolator_type   span_interpolator(transform);
   span_allocator_type span_allocator;

   DOUBLE bx1, by1, bx2, by2;
   bounding_rect_single(*Path, 0, &bx1, &by1, &bx2, &by2);

   if (Gradient.Type IS VGT_LINEAR) {
      DOUBLE ax1, ay1, ax2, ay2;

      if (Gradient.Units & VUNIT_USERSPACE) { // Absolute positioning, ignores the vector path/transform.
         if (Gradient.Flags & VGF_RELATIVE_X1) ax1 = (ViewWidth * Gradient.X1);
         else ax1 = Gradient.X1;

         if (Gradient.Flags & VGF_RELATIVE_Y1) ay1 = (ViewHeight * Gradient.Y1);
         else ay1 = Gradient.Y1;

         if (Gradient.Flags & VGF_RELATIVE_X2) ax2 = (ViewWidth * Gradient.X2);
         else ax2 = Gradient.X2;

         if (Gradient.Flags & VGF_RELATIVE_Y2) ay2 = (ViewHeight * Gradient.Y2);
         else ay2 = Gradient.Y2;
      }
      else { // Align to vector's bounding box
         const DOUBLE boundwidth = bx2 - bx1;
         const DOUBLE boundheight = by2 - by1;

         if (Gradient.Flags & VGF_RELATIVE_X1) ax1 = (boundwidth * Gradient.X1);
         else ax1 = Gradient.X1;

         if (Gradient.Flags & VGF_RELATIVE_X2) ax2 = (boundwidth * Gradient.X2);
         else ax2 = Gradient.X2;

         if (Gradient.Flags & VGF_RELATIVE_Y1) ay1 = (boundheight * Gradient.Y1);
         else ay1 = Gradient.Y1;

         if (Gradient.Flags & VGF_RELATIVE_Y2) ay2 = (boundheight * Gradient.Y2);
         else ay2 = Gradient.Y2;
      }

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

      const DOUBLE width = bx2 - bx1;
      const DOUBLE height = by2 - by1;

      if (Gradient.Units & VUNIT_USERSPACE) {
         if (Gradient.Flags & VGF_RELATIVE_CX) cx = ViewWidth * Gradient.CenterX;
         else cx = Gradient.CenterX;

         if (Gradient.Flags & VGF_RELATIVE_CY) cy = ViewHeight * Gradient.CenterY;
         else cy = Gradient.CenterY;

         if (Gradient.Flags & VGF_RELATIVE_FX) fx = ViewWidth * Gradient.FX;
         else if (Gradient.Flags & VGF_FIXED_FX) fx = Gradient.FX;
         else fx = cx;

         if (Gradient.Flags & VGF_RELATIVE_FY) fy = ViewHeight * Gradient.FY;
         else if (Gradient.Flags & VGF_FIXED_FY) fy = Gradient.FY;
         else fy = cy;
      }
      else {
         if (Gradient.Flags & VGF_RELATIVE_CX) cx = (width * Gradient.CenterX);
         else cx = Gradient.CenterX;

         if (Gradient.Flags & VGF_RELATIVE_CY) cy = (height * Gradient.CenterY);
         else cy = Gradient.CenterY;

         if (Gradient.Flags & VGF_RELATIVE_FX) fx = (width * Gradient.FX);
         else if (Gradient.Flags & VGF_FIXED_FX) fx = Gradient.FX;
         else fx = cx;

         if (Gradient.Flags & VGF_RELATIVE_FY) fy = (height * Gradient.FY);
         else if (Gradient.Flags & VGF_FIXED_FY) fy = Gradient.FY;
         else fy = cy;
      }

      if ((cx IS fx) and (cy IS fy)) {
         // Standard radial gradient, where the focal point is the same as the gradient center

         DOUBLE length = Gradient.Radius;
         if (Gradient.Units & VUNIT_USERSPACE) { // Coordinates are relative to the viewport
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
               if (height > width) scale_y *= height / width;
               else if (width > height) scale_x *= width / height;
               scale_x *= 100.0 / length; // Adjust the scale according to the gradient length.
               scale_y *= 100.0 / length;
               transform.scale(scale_x, scale_y);
            }
            else {
               //transform *= agg::trans_affine_scaling(Gradient.Radius * 0.01 / length);
            }
         }

         if (length < 255) {   // Blending works best if the gradient span is at least 255 colours wide, so adjust it here.
            transform.scale(length / 255.0);
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

         const DOUBLE width = bx2 - bx1;
         const DOUBLE height = by2 - by1;
         DOUBLE fix_radius = Gradient.Radius;
         if (Gradient.Flags & VGF_RELATIVE_RADIUS) fix_radius *= (width + height) * 0.5; // Use the average radius of the ellipse.
         DOUBLE length = fix_radius;

         if (Gradient.Units & VUNIT_USERSPACE) {
            if (Gradient.Flags & VGF_RELATIVE_RADIUS) {
               DOUBLE scale = length * Gradient.Radius;
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
               if (height > width) scale_y *= height / width;
               else if (width > height) scale_x *= width / height;
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

      const DOUBLE width = bx2 - bx1;
      const DOUBLE height = by2 - by1;

      if (Gradient.Units & VUNIT_USERSPACE) {
         if (Gradient.Flags & VGF_RELATIVE_CX) cx = ViewWidth * Gradient.CenterX;
         else cx = Gradient.CenterX;

         if (Gradient.Flags & VGF_RELATIVE_CY) cy = ViewHeight * Gradient.CenterY;
         else cy = Gradient.CenterY;
      }
      else {
         if (Gradient.Flags & VGF_RELATIVE_CX) cx = (width * Gradient.CenterX);
         else cx = Gradient.CenterX;

         if (Gradient.Flags & VGF_RELATIVE_CY) cy = (height * Gradient.CenterY);
         else cy = Gradient.CenterY;
      }

      // Standard diamond gradient, where the focal point is the same as the gradient center

      const DOUBLE length = 255;
      if (Gradient.Units & VUNIT_USERSPACE) {
         if (Gradient.Flags & VGF_RELATIVE_RADIUS) {
            DOUBLE scale = length * Gradient.Radius;
            transform *= agg::trans_affine_scaling(sqrt((ViewWidth * ViewWidth) + (ViewHeight * ViewHeight)) / scale);
         }
         else transform *= agg::trans_affine_scaling(Gradient.Radius * 0.01);
      }
      else { // Align to vector's bounding box
         if (Gradient.Flags & VGF_RELATIVE_RADIUS) {
            // In AGG, an unscaled gradient will cover the entire bounding box according to the diagonal.
            // In SVG a radius of 50% must result in the edge of the radius meeting the edge of the bounding box.

            DOUBLE scale_x = Gradient.Radius * (1.0 / 0.707106781);
            DOUBLE scale_y = Gradient.Radius * (1.0 / 0.707106781);
            if (height > width) scale_y *= height / width;
            else if (width > height) scale_x *= width / height;
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

      const DOUBLE width = bx2 - bx1;
      const DOUBLE height = by2 - by1;

      if (Gradient.Units & VUNIT_USERSPACE) {
         if (Gradient.Flags & VGF_RELATIVE_CX) cx = ViewWidth * Gradient.CenterX;
         else cx = Gradient.CenterX;

         if (Gradient.Flags & VGF_RELATIVE_CY) cy = ViewHeight * Gradient.CenterY;
         else cy = Gradient.CenterY;
      }
      else {
         if (Gradient.Flags & VGF_RELATIVE_CX) cx = (width * Gradient.CenterX);
         else cx = Gradient.CenterX;

         if (Gradient.Flags & VGF_RELATIVE_CY) cy = (height * Gradient.CenterY);
         else cy = Gradient.CenterY;
      }

      // Standard conic gradient, where the focal point is the same as the gradient center

      const DOUBLE length = 255;
      if (Gradient.Units & VUNIT_USERSPACE) {
         if (Gradient.Flags & VGF_RELATIVE_RADIUS) {
            DOUBLE scale = length * Gradient.Radius;
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
            if (height > width) scale_y *= height / width;
            else if (width > height) scale_x *= width / height;
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
      auto x1 = (Gradient.X1 >= 0) ? Gradient.X1 : 0;
      auto x2 = (Gradient.X2 <= 512) ? Gradient.X2 : 512;

      agg::gradient_contour gradient_func;
      gradient_func.frame(0); // This value offsets the gradient, e.g. 10 adds an x,y offset of (10,10)
      gradient_func.d1(x1);   // x1 and x2 alter the coverage of the gradient colours
      gradient_func.d2(x2);   // Low values for x2 will increase the amount of repetition seen in the gradient.
      gradient_func.contour_create(Path);

      transform.translate(bx1, by1);
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

//********************************************************************************************************************

class VMAdaptor
{
private:
   agg::renderer_base<agg::pixfmt_rkl> mRenderBase;
   agg::renderer_scanline_aa_solid< agg::renderer_base<agg::pixfmt_rkl> > mSolidRender;
   agg::pixfmt_rkl mFormat;
   agg::scanline_u8 mScanLine;  // Use scanline_p for large solid polygons and scanline_u for things like text and gradients
   objVectorViewport *mView; // The current view
   objBitmap *mBitmap;

public:
   objVectorScene *Scene; // The top-level VectorScene performing the draw.

   VMAdaptor() : mSolidRender(mRenderBase) { }

   void draw(struct rkBitmap *Bitmap) {
      parasol::Log log;

      log.traceBranch("Bitmap: %dx%d,%dx%d, Viewport: %p", Bitmap->Clip.Left, Bitmap->Clip.Top, Bitmap->Clip.Right, Bitmap->Clip.Bottom, Scene->Viewport);

      if (Scene->Viewport) {
         mBitmap = Bitmap;
         mFormat.setBitmap(*Bitmap);
         mRenderBase.attach(mFormat);

         mView = NULL; // Current view
         mRenderBase.clip_box(Bitmap->Clip.Left, Bitmap->Clip.Top, Bitmap->Clip.Right-1, Bitmap->Clip.Bottom-1);

         Scene->InputBoundaries.clear();

         VectorState state;
         draw_vectors(Scene->Viewport, state);

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

   void render_fill(VectorState &State, objVector &Vector, agg::rasterizer_scanline_aa<> &Raster) {
      // Think of the vector's path as representing a mask for the fill algorithm.  Any transforms applied to
      // an image/gradient fill are independent of the path.

      if (Vector.FillRule IS VFR_NON_ZERO) Raster.filling_rule(agg::fill_non_zero);
      else if (Vector.FillRule IS VFR_EVEN_ODD) Raster.filling_rule(agg::fill_even_odd);

      if ((Vector.FillColour.Alpha > 0) and (!Vector.DisableFillColour)) { // Solid colour
         mSolidRender.color(agg::rgba(Vector.FillColour.Red, Vector.FillColour.Green, Vector.FillColour.Blue, Vector.FillColour.Alpha * Vector.FillOpacity * State.mOpacity));

         if (State.mClipMask) {
            agg::alpha_mask_gray8 alpha_mask(State.mClipMask->ClipRenderer);
            agg::scanline_u8_am<agg::alpha_mask_gray8> mScanLineMasked(alpha_mask);
            agg::render_scanlines(Raster, mScanLineMasked, mSolidRender);
         }
         else agg::render_scanlines(Raster, mScanLine, mSolidRender);
      }

      if (Vector.FillImage) { // Bitmap image fill.  NB: The SVG class creates a standard VectorRectangle and associates an image with it in order to support <image> tags.
         draw_image(&Vector, Vector.BasePath, Vector.Scene->SampleMethod, build_fill_transform(Vector, Vector.FillImage->Units & VUNIT_USERSPACE, State),
            view_width(), view_height(), *Vector.FillImage, mRenderBase, Raster, 0, Vector.FillOpacity * State.mOpacity);
      }

      if (Vector.FillGradient) {
         if (auto table = get_fill_gradient_table(Vector, State.mOpacity * Vector.FillOpacity)) {
            draw_gradient(&Vector, &Vector.BasePath, build_fill_transform(Vector, Vector.FillGradient->Units & VUNIT_USERSPACE, State),
               view_width(), view_height(), *Vector.FillGradient, table, mRenderBase, Raster, 0);
         }
      }

      if (Vector.FillPattern) {
         draw_pattern(&Vector, &Vector.BasePath, Vector.Scene->SampleMethod, build_fill_transform(Vector, Vector.FillPattern->Units & VUNIT_USERSPACE, State),
            view_width(), view_height(), *Vector.FillPattern, mRenderBase, Raster);
      }
   }

   void render_stroke(VectorState &State, objVector &Vector, agg::rasterizer_scanline_aa<> &Raster) {
      if (Vector.Scene->Gamma != 1.0) Raster.gamma(agg::gamma_power(Vector.Scene->Gamma));

      if (Vector.FillRule IS VFR_NON_ZERO) Raster.filling_rule(agg::fill_non_zero);
      else if (Vector.FillRule IS VFR_EVEN_ODD) Raster.filling_rule(agg::fill_even_odd);

      if (Vector.StrokeGradient) {
         if (auto table = get_stroke_gradient_table(Vector)) {
            draw_gradient(&Vector, &Vector.BasePath, build_fill_transform(Vector, Vector.StrokeGradient->Units & VUNIT_USERSPACE, State),
               view_width(), view_height(), *Vector.StrokeGradient, table, mRenderBase, Raster, Vector.fixed_stroke_width());
         }
      }
      else if (Vector.StrokePattern) {
         draw_pattern(&Vector, &Vector.BasePath, Vector.Scene->SampleMethod, build_fill_transform(Vector, Vector.StrokePattern->Units & VUNIT_USERSPACE, State),
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
         mSolidRender.color(agg::rgba(Vector.StrokeColour.Red, Vector.StrokeColour.Green, Vector.StrokeColour.Blue, Vector.StrokeColour.Alpha * Vector.StrokeOpacity * State.mOpacity));

         if (State.mClipMask) {
            agg::alpha_mask_gray8 alpha_mask(State.mClipMask->ClipRenderer);
            agg::scanline_u8_am<agg::alpha_mask_gray8> mScanLineMasked(alpha_mask);
            agg::render_scanlines(Raster, mScanLineMasked, mSolidRender);
         }
         else agg::render_scanlines(Raster, mScanLine, mSolidRender);
      }
   }

   // This is the main routine for parsing the vector tree for drawing.

   void draw_vectors(objVector *CurrentVector, VectorState &ParentState) {
      for (auto shape=CurrentVector; shape; shape=(objVector *)shape->Next) {
         parasol::Log log(__FUNCTION__);
         VectorState state = VectorState(ParentState);

         if (shape->Head.ClassID != ID_VECTOR) {
            log.trace("Non-Vector discovered in the vector tree.");
            continue;
         }
         else if (!shape->Scene) continue;

         if (shape->Dirty) {
            gen_vector_path(shape);
            shape->Dirty = 0;
         }
         else log.trace("%s: #%d, Dirty: NO, ParentView: #%d", shape->Head.Class->ClassName, shape->Head.UID, shape->ParentView ? shape->ParentView->Head.UID : 0);

         // Visibility management.

         {
            bool visible = true;
            if (shape->Visibility IS VIS_INHERIT) {
               if (ParentState.mVisible != VIS_VISIBLE) visible = false;
            }
            else if (shape->Visibility != VIS_VISIBLE) visible = false;

            if (!visible) {
               log.trace("%s: #%d, Not Visible", get_name(shape), shape->Head.UID);
               continue;
            }
         }

         objVectorFilter *filter;
         if ((filter = shape->Filter)) {
            #ifdef DBG_DRAW
               log.traceBranch("Processing filter.");
            #endif

            if (!SetPointer(filter, FID_Vector, shape)) { // Divert rendering of this vector through the filter.
               filter->BkgdBitmap = mBitmap;
               acDraw(filter);
            }
            else log.trace("Failed to set Vector reference on Filter.");

            continue;
         }

         #ifdef DBG_DRAW
            log.traceBranch("%s: #%d, Matrices: %p", get_name(shape), shape->Head.UID, shape->Matrices);
         #endif

         if (shape->LineJoin != agg::inherit_join)   state.mLineJoin  = shape->LineJoin;
         if (shape->InnerJoin != agg::inner_inherit) state.mInnerJoin = shape->InnerJoin;
         if (shape->LineCap != agg::inherit_cap)     state.mLineCap   = shape->LineCap;
         state.mOpacity = shape->Opacity * state.mOpacity;

         // Support for enable-background="new".  This requires the bitmap to have an alpha channel so that
         // filter blending works correctly.

         objBitmap *bmpBkgd = NULL;
         objBitmap *bmpSave = NULL;
         if (shape->EnableBkgd) { // If the current bitmap already has the alpha-channel enabled then we don't need a new bitmap.
            if (!CreateObject(ID_BITMAP, NF_INTEGRAL, &bmpBkgd,
                  FID_Width|TLONG,  mBitmap->Width,
                  FID_Height|TLONG, mBitmap->Height,
                  FID_BitsPerPixel, 32,
                  FID_Flags|TLONG,  BMF_ALPHA_CHANNEL,
                  TAGEND)) {
               bmpSave = mBitmap;
               mBitmap = bmpBkgd;
               mFormat.setBitmap(*bmpBkgd);
               ClearMemory(bmpBkgd->Data, bmpBkgd->LineWidth * bmpBkgd->Height);
            }
         }

         if (shape->Head.SubID IS ID_VECTORVIEWPORT) {
            if ((shape->Child) or (shape->InputSubscriptions) or (shape->FillPattern)) {
               auto view = (objVectorViewport *)shape;

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
                  log.traceBranch("Viewport #%d clip region (%.2f %.2f %.2f %.2f)", shape->Head.UID, state.mClip.x1, state.mClip.y1, state.mClip.x2, state.mClip.y2);
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
                     Scene->InputBoundaries.emplace_back(shape->Head.UID, view->Cursor, x1, y1, x2, y2, view->vpBX1, view->vpBY1);
                  }

                  if (Scene->Flags & VPF_OUTLINE_VIEWPORTS) { // Debug option: Draw the viewport's path with a green outline
                     mSolidRender.color(agg::rgba(0, 1, 0));
                     agg::rasterizer_scanline_aa stroke_raster;
                     agg::conv_stroke<agg::path_storage> stroked_path(view->BasePath);
                     stroked_path.width(2);
                     stroke_raster.add_path(stroked_path);
                     agg::render_scanlines(stroke_raster, mScanLine, mSolidRender);
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

                     draw_vectors((objVector *)view->FillPattern->Viewport, state);

                     state.mTransform      = s_transform;
                     state.mApplyTransform = s_apply;
                  }

                  if (view->Child) draw_vectors((objVector *)view->Child, state);

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
                  log.traceBranch("%s: #%d, Mask: %p", get_name(shape), shape->Head.UID, shape->ClipMask);
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
                        configure_stroke((objVector &)*shape, shape->DashArray->stroke);
                        agg::conv_transform<agg::conv_stroke<agg::conv_dash<agg::path_storage>>, agg::trans_affine> final_path(shape->DashArray->stroke, transform);

                        agg::rasterizer_scanline_aa raster;
                        raster.add_path(final_path);
                        render_stroke(state, *shape, raster);
                     }
                     else {
                        agg::conv_stroke<agg::path_storage> stroked_path(shape->BasePath);
                        configure_stroke((objVector &)*shape, stroked_path);
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
                     agg::conv_transform<agg::path_storage, agg::trans_affine> path(shape->BasePath, shape->Transform);
                     bounding_rect_single(path, 0, &bx1, &by1, &bx2, &by2);
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

                  Scene->InputBoundaries.emplace_back(shape->Head.UID, shape->Cursor, bx1, by1, bx2, by2, absx, absy);
               }

               state.mClipMask = saved_mask;
            }

            if (shape->Child) {
               auto saved_mask = state.mClipMask;
               if (shape->ClipMask) state.mClipMask = shape->ClipMask;

               draw_vectors((objVector *)shape->Child, state);

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

//****************************************************************************
// For direct vector drawing

void SimpleVector::DrawPath(objBitmap *Bitmap, DOUBLE StrokeWidth, OBJECTPTR StrokeStyle, OBJECTPTR FillStyle)
{
   parasol::Log log("draw_path");

   agg::scanline_u8 scanline;
   agg::pixfmt_rkl format;
   agg::trans_affine transform;

   format.setBitmap(*Bitmap);
   mRenderer.attach(format);
   mRenderer.clip_box(Bitmap->Clip.Left, Bitmap->Clip.Top, Bitmap->Clip.Right-1, Bitmap->Clip.Bottom-1);
   //if (Gamma != 1.0) mRaster.gamma(agg::gamma_power(Gamma));

   log.traceBranch("Bitmap: %p, Stroke: %p (%s), Fill: %p (%s)", Bitmap, StrokeStyle, get_name(StrokeStyle), FillStyle, get_name(FillStyle));

   if (FillStyle) {
      mRaster.reset();
      mRaster.add_path(mPath);

      if (FillStyle->ClassID IS ID_VECTORCOLOUR) {
         objVectorColour *colour = (objVectorColour *)FillStyle;
         agg::renderer_scanline_aa_solid<agg::renderer_base<agg::pixfmt_rkl>> solid(mRenderer);
         solid.color(agg::rgba(colour->Red, colour->Green, colour->Blue, colour->Alpha));
         agg::render_scanlines(mRaster, scanline, solid);
      }
      else if (FillStyle->ClassID IS ID_VECTORIMAGE) {
         objVectorImage &image = (objVectorImage &)*FillStyle;
         draw_image(NULL, mPath, VSM_AUTO, transform, Bitmap->Width, Bitmap->Height, image, mRenderer, mRaster, 0, 1.0);
      }
      else if (FillStyle->ClassID IS ID_VECTORGRADIENT) {
         objVectorGradient &gradient = (objVectorGradient &)*FillStyle;
         draw_gradient(NULL, &mPath, transform, Bitmap->Width, Bitmap->Height, gradient, &gradient.Colours->table, mRenderer, mRaster, 0);
      }
      else if (FillStyle->ClassID IS ID_VECTORPATTERN) {
         draw_pattern(NULL, &mPath, VSM_AUTO, transform, Bitmap->Width, Bitmap->Height, (objVectorPattern &)*FillStyle, mRenderer, mRaster);
      }
      else log.warning("The FillStyle is not supported.");
   }

   if ((StrokeWidth > 0) and (StrokeStyle)){
      if (StrokeStyle->ClassID IS ID_VECTORGRADIENT) {
         agg::conv_stroke<agg::path_storage> stroke_path(mPath);
         mRaster.reset();
         mRaster.add_path(stroke_path);

         objVectorGradient &gradient = (objVectorGradient &)*StrokeStyle;
         draw_gradient(NULL, &mPath, transform, Bitmap->Width, Bitmap->Height, gradient, &gradient.Colours->table, mRenderer, mRaster, 0);
      }
      else if (StrokeStyle->ClassID IS ID_VECTORPATTERN) {
         agg::conv_stroke<agg::path_storage> stroke_path(mPath);
         mRaster.reset();
         mRaster.add_path(stroke_path);
         draw_pattern(NULL, &mPath, VSM_AUTO, transform, Bitmap->Width, Bitmap->Height, (objVectorPattern &)*StrokeStyle, mRenderer, mRaster);
      }
      else if (StrokeStyle->ClassID IS ID_VECTORIMAGE) {
         objVectorImage &image = (objVectorImage &)*StrokeStyle;
         agg::trans_affine transform;
         agg::conv_transform<agg::path_storage, agg::trans_affine> path(mPath, transform);
         draw_brush(image, mRenderer, path, StrokeWidth);
      }
      else if (StrokeStyle->ClassID IS ID_VECTORCOLOUR) {
         agg::renderer_scanline_aa_solid<agg::renderer_base<agg::pixfmt_rkl>> solid(mRenderer);
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
