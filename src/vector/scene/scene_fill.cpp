
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
// Image extension
// Path: The original vector path without transforms.
// Transform: Transforms to be applied to the path and to align the image.

static void fill_image(VectorState &State, const TClipRectangle<double> &Bounds, agg::path_storage &Path, VSM SampleMethod,
   const agg::trans_affine &Transform, double ViewWidth, double ViewHeight,
   objVectorImage &Image, agg::renderer_base<agg::pixfmt_psl> &RenderBase,
   agg::rasterizer_scanline_aa<> &Raster, double Alpha)
{
   const double c_width  = (Image.Units IS VUNIT::USERSPACE) ? ViewWidth : Bounds.width();
   const double c_height = (Image.Units IS VUNIT::USERSPACE) ? ViewHeight : Bounds.height();
   const double dx = Bounds.left + (dmf::hasScaledX(Image.Dimensions) ? (c_width * Image.X) : Image.X);
   const double dy = Bounds.top + (dmf::hasScaledY(Image.Dimensions) ? (c_height * Image.Y) : Image.Y);

   Path.approximation_scale(Transform.scale());

   double x_scale, y_scale, x_offset, y_offset;
   calc_aspectratio("fill_image", Image.AspectRatio, c_width, c_height,
      Image.Bitmap->Width, Image.Bitmap->Height, &x_offset, &y_offset, &x_scale, &y_scale);

   agg::trans_affine transform;
   transform.scale(x_scale, y_scale);
   transform.translate(dx + x_offset, dy + y_offset);
   transform *= Transform;

   transform.invert();

   if (!State.mClipStack->empty()) {
      agg::alpha_mask_gray8 alpha_mask(State.mClipStack->top().m_renderer);
      agg::scanline_u8_am<agg::alpha_mask_gray8> masked_scanline(alpha_mask);
      drawBitmap(masked_scanline, SampleMethod, RenderBase, Raster, Image.Bitmap, Image.SpreadMethod, Alpha, &transform);
   }
   else {
      agg::scanline_u8 scanline;
      drawBitmap(scanline, SampleMethod, RenderBase, Raster, Image.Bitmap, Image.SpreadMethod, Alpha, &transform);
   }
}

//********************************************************************************************************************
// Gradient fills
// The Raster must contain the shape's path.
// TODO: Support gradient_xy (rounded corner), gradient_sqrt_xy

static void fill_gradient(VectorState &State, const TClipRectangle<double> &Bounds, agg::path_storage *Path,
   const agg::trans_affine &Transform, double ViewWidth, double ViewHeight, extVectorGradient &Gradient,
   GRADIENT_TABLE *Table, agg::renderer_base<agg::pixfmt_psl> &RenderBase,
   agg::rasterizer_scanline_aa<> &Raster)
{
   constexpr LONG MAX_SPAN = 256;
   typedef agg::span_interpolator_linear<> interpolator_type;
   typedef agg::span_allocator<agg::rgba8> span_allocator_type;
   typedef agg::pod_auto_array<agg::rgba8, MAX_SPAN> color_array_type;
   typedef agg::renderer_base<agg::pixfmt_psl>  RENDERER_BASE_TYPE;

   agg::trans_affine   transform;
   interpolator_type   span_interpolator(transform);
   span_allocator_type span_allocator;

   const double c_width = Gradient.Units IS VUNIT::USERSPACE ? ViewWidth : Bounds.width();
   const double c_height = Gradient.Units IS VUNIT::USERSPACE ? ViewHeight : Bounds.height();
   const double x_offset = Gradient.Units IS VUNIT::USERSPACE ? 0 : Bounds.left;
   const double y_offset = Gradient.Units IS VUNIT::USERSPACE ? 0 : Bounds.top;

   Path->approximation_scale(Transform.scale());

   auto render_gradient = [&]<typename S>(S SpreadMethod, double Span) {
      typedef agg::span_gradient<agg::rgba8, interpolator_type, S, color_array_type> span_gradient_type;
      typedef agg::renderer_scanline_aa<RENDERER_BASE_TYPE, span_allocator_type, span_gradient_type> renderer_gradient_type;

      span_gradient_type  span_gradient(span_interpolator, SpreadMethod, *Table, 0, Span);
      renderer_gradient_type solidrender_gradient(RenderBase, span_allocator, span_gradient);

      if (State.mClipStack->empty()) {
         agg::scanline_u8 scanline;
         agg::render_scanlines(Raster, scanline, solidrender_gradient);
      }
      else { // Masked gradient
         agg::alpha_mask_gray8 alpha_mask(State.mClipStack->top().m_renderer);
         agg::scanline_u8_am<agg::alpha_mask_gray8> masked_scanline(alpha_mask);
         agg::render_scanlines(Raster, masked_scanline, solidrender_gradient);
      }
   };

   auto span = MAX_SPAN;
   if (Gradient.Type IS VGT::LINEAR) {
      if (Gradient.Units IS VUNIT::BOUNDING_BOX) {
         // NOTE: In this mode we are mapping a 1x1 gradient square into the target path, which means
         // the gradient is stretched into position as a square.  We don't map the (X,Y) points to the
         // bounding box and draw point-to-point.

         const double x = x_offset + (c_width * Gradient.X1);
         const double y = y_offset + (c_height * Gradient.Y1);

         if (Gradient.CalcAngle) {
            const double dx = Gradient.X2 - Gradient.X1;
            const double dy = Gradient.Y2 - Gradient.Y1;
            Gradient.Angle     = atan2(dy, dx);
            Gradient.Length    = sqrt((dx * dx) + (dy * dy));
            Gradient.CalcAngle = false;        
         }

         transform.scale(Gradient.Length);
         transform.rotate(Gradient.Angle);
         transform.scale(c_width / span, c_height / span);
         transform.translate(x, y);
      }
      else {
         TClipRectangle<double> area;
         if ((Gradient.Flags & VGF::SCALED_X1) != VGF::NIL) area.left = x_offset + (c_width * Gradient.X1);
         else area.left = x_offset + Gradient.X1;

         if ((Gradient.Flags & VGF::SCALED_X2) != VGF::NIL) area.right = x_offset + (c_width * Gradient.X2);
         else area.right = x_offset + Gradient.X2;

         if ((Gradient.Flags & VGF::SCALED_Y1) != VGF::NIL) area.top = y_offset + (c_height * Gradient.Y1);
         else area.top = y_offset + Gradient.Y1;

         if ((Gradient.Flags & VGF::SCALED_Y2) != VGF::NIL) area.bottom = y_offset + (c_height * Gradient.Y2);
         else area.bottom = y_offset + Gradient.Y2;
         
         if (Gradient.CalcAngle) {
            const double dx = area.width();
            const double dy = area.height();
            Gradient.Angle     = atan2(dy, dx);
            Gradient.Length    = sqrt((dx * dx) + (dy * dy));
            Gradient.CalcAngle = false;        
         }

         transform.scale(Gradient.Length / span);
         transform.rotate(Gradient.Angle);
         transform.translate(area.left, area.top);
      }

      apply_transforms(Gradient, transform);
      transform *= Transform;
      transform.invert();

      agg::gradient_x gradient_func;

      if (Gradient.SpreadMethod IS VSPREAD::REFLECT) {
         agg::gradient_reflect_adaptor<agg::gradient_x> spread_method(gradient_func);
         render_gradient(spread_method, span);
      }
      else if (Gradient.SpreadMethod IS VSPREAD::REPEAT) {
         agg::gradient_repeat_adaptor<agg::gradient_x> spread_method(gradient_func);
         render_gradient(spread_method, span);
      }
      else render_gradient(gradient_func, span);
   }
   else if (Gradient.Type IS VGT::RADIAL) {
      agg::point_d c, f;

      if ((Gradient.Flags & VGF::SCALED_CX) != VGF::NIL) c.x = x_offset + (c_width * Gradient.CenterX);
      else c.x = x_offset + Gradient.CenterX;

      if ((Gradient.Flags & VGF::SCALED_CY) != VGF::NIL) c.y = y_offset + (c_height * Gradient.CenterY);
      else c.y = y_offset + Gradient.CenterY;

      if ((Gradient.Flags & VGF::SCALED_FX) != VGF::NIL) f.x = x_offset + (c_width * Gradient.FocalX);
      else if ((Gradient.Flags & VGF::FIXED_FX) != VGF::NIL) f.x = x_offset + Gradient.FocalX;
      else f.x = c.x;

      if ((Gradient.Flags & VGF::SCALED_FY) != VGF::NIL) f.y = y_offset + (c_height * Gradient.FocalY);
      else if ((Gradient.Flags & VGF::FIXED_FY) != VGF::NIL) f.y = y_offset + Gradient.FocalY;
      else f.y = c.y;

      if ((c.x IS f.x) and (c.y IS f.y)) {
         // Standard radial gradient, where the focal point is the same as the gradient center

         double radial_col_span = Gradient.Radius;
         if (Gradient.Units IS VUNIT::USERSPACE) { // Coordinates are relative to the viewport
            if ((Gradient.Flags & VGF::SCALED_RADIUS) != VGF::NIL) { // Gradient is a ratio of the viewport's dimensions
               radial_col_span = (ViewWidth + ViewHeight) * Gradient.Radius * 0.5;
            }
         }
         else { // Coordinates are scaled to the bounding box
            // Set radial_col_span to the wider of the width/height
            if (c_height > c_width) {
               radial_col_span = c_height * Gradient.Radius;
               transform.scaleX(c_width / c_height);
            }
            else {
               radial_col_span = c_width * Gradient.Radius;
               transform.scaleY(c_height / c_width);
            }
         }

         constexpr double MIN_SPAN = 32;
         if (radial_col_span < MIN_SPAN) { // Blending looks best if it meets a minimum span (radius) value.
            transform.scale(radial_col_span * (1.0 / MIN_SPAN));
            radial_col_span = MIN_SPAN;
         }
         else if (radial_col_span > MAX_SPAN) {
            transform.scale(radial_col_span * (1.0 / MAX_SPAN));
            radial_col_span = MAX_SPAN;
         }

         transform.translate(c);
         apply_transforms(Gradient, transform);
         transform *= Transform;
         transform.invert();

         agg::gradient_radial gradient_func;

         if (Gradient.SpreadMethod IS VSPREAD::REFLECT) {
            agg::gradient_reflect_adaptor<agg::gradient_radial> spread_method(gradient_func);
            render_gradient(spread_method, radial_col_span);
         }
         else if (Gradient.SpreadMethod IS VSPREAD::REPEAT) {
            agg::gradient_repeat_adaptor<agg::gradient_radial> spread_method(gradient_func);
            render_gradient(spread_method, radial_col_span);
         }
         else render_gradient(gradient_func, radial_col_span);
      }
      else {
         // Radial gradient with a displaced focal point (uses agg::gradient_radial_focus).  NB: In early versions of
         // the SVG standard, the focal point had to be within the base radius.  Later specifications allowed it to
         // be placed outside of that radius.

         double radial_col_span = Gradient.Radius;
         double focal_radius = Gradient.FocalRadius;
         if (focal_radius <= 0) focal_radius = Gradient.Radius;

         if (Gradient.Units IS VUNIT::USERSPACE) { // Coordinates are relative to the viewport
            if ((Gradient.Flags & VGF::SCALED_RADIUS) != VGF::NIL) { // Gradient is a ratio of the viewport's dimensions
               radial_col_span = (ViewWidth + ViewHeight) * radial_col_span * 0.5;
               focal_radius = (ViewWidth + ViewHeight) * focal_radius * 0.5;
            }
         }
         else { // Coordinates are scaled to the bounding box
            // Set radial_col_span to the wider of the width/height
            if (c_height > c_width) {
               radial_col_span = c_height * radial_col_span;
               focal_radius = c_height * focal_radius;
               transform.scaleX(c_width / c_height);
            }
            else {
               radial_col_span = c_width * radial_col_span;
               focal_radius = c_width * focal_radius;
               transform.scaleY(c_height / c_width);
            }
         }

         // Changing the focal radius allows the client to increase or decrease the border to which the focal
         // calculations are being made.  If not supported by the underlying implementation (e.g. SVG) then
         // it must match the base radius.

         agg::gradient_radial_focus gradient_func(focal_radius, f.x - c.x, f.y - c.y);

         transform.translate(c);
         apply_transforms(Gradient, transform);
         transform *= Transform;
         transform.invert();

         if (Gradient.SpreadMethod IS VSPREAD::REFLECT) {
            agg::gradient_reflect_adaptor<agg::gradient_radial_focus> spread_method(gradient_func);
            render_gradient(spread_method, radial_col_span);
         }
         else if (Gradient.SpreadMethod IS VSPREAD::REPEAT) {
            agg::gradient_repeat_adaptor<agg::gradient_radial_focus> spread_method(gradient_func);
            render_gradient(spread_method, radial_col_span);
         }
         else render_gradient(gradient_func, radial_col_span);
      }
   }
   else if (Gradient.Type IS VGT::DIAMOND) {
      agg::point_d c;

      if ((Gradient.Flags & VGF::SCALED_CX) != VGF::NIL) c.x = x_offset + (c_width * Gradient.CenterX);
      else c.x = x_offset + Gradient.CenterX;

      if ((Gradient.Flags & VGF::SCALED_CY) != VGF::NIL) c.y = y_offset + (c_height * Gradient.CenterY);
      else c.y = y_offset + Gradient.CenterY;

      // Standard diamond gradient, where the focal point is the same as the gradient center

      double radial_col_span = Gradient.Radius;
      if (Gradient.Units IS VUNIT::USERSPACE) {
         if ((Gradient.Flags & VGF::SCALED_RADIUS) != VGF::NIL) {
            radial_col_span = (ViewWidth + ViewHeight) * Gradient.Radius * 0.5;
         }
         else transform *= agg::trans_affine_scaling(Gradient.Radius * 0.01);
      }
      else { // Align to vector's bounding box
         // Set radial_col_span to the wider of the width/height
         if (c_height > c_width) {
            radial_col_span = c_height * Gradient.Radius;
            transform.scaleX(c_width / c_height);
         }
         else {
            radial_col_span = c_width * Gradient.Radius;
            transform.scaleY(c_height / c_width);
         }
      }

      agg::gradient_diamond gradient_func;

      transform.translate(c);
      apply_transforms(Gradient, transform);
      transform *= Transform;
      transform.invert();

      if (Gradient.SpreadMethod IS VSPREAD::REFLECT) {
         agg::gradient_reflect_adaptor<agg::gradient_diamond> spread_method(gradient_func);
         render_gradient(spread_method, radial_col_span);
      }
      else if (Gradient.SpreadMethod IS VSPREAD::REPEAT) {
         agg::gradient_repeat_adaptor<agg::gradient_diamond> spread_method(gradient_func);
         render_gradient(spread_method, radial_col_span);
      }
      else render_gradient(gradient_func, radial_col_span);
   }
   else if (Gradient.Type IS VGT::CONIC) {
      agg::point_d c;

      if ((Gradient.Flags & VGF::SCALED_CX) != VGF::NIL) c.x = x_offset + (c_width * Gradient.CenterX);
      else c.x = x_offset + Gradient.CenterX;

      if ((Gradient.Flags & VGF::SCALED_CY) != VGF::NIL) c.y = y_offset + (c_height * Gradient.CenterY);
      else c.y = y_offset + Gradient.CenterY;

      // Standard conic gradient, where the focal point is the same as the gradient center

      double radial_col_span = Gradient.Radius;
      if (Gradient.Units IS VUNIT::USERSPACE) {
         if ((Gradient.Flags & VGF::SCALED_RADIUS) != VGF::NIL) {
            radial_col_span = (ViewWidth + ViewHeight) * Gradient.Radius * 0.5;
         }
         else transform *= agg::trans_affine_scaling(Gradient.Radius * 0.01);
      }
      else { // Bounding box
         // Set radial_col_span to the wider of the width/height
         if (c_height > c_width) {
            radial_col_span = c_height * Gradient.Radius;
            transform.scaleX(c_width / c_height);
         }
         else {
            radial_col_span = c_width * Gradient.Radius;
            transform.scaleY(c_height / c_width);
         }
      }

      agg::gradient_conic gradient_func;
      transform.translate(c);
      apply_transforms(Gradient, transform);
      transform *= Transform;
      transform.invert();

      render_gradient(gradient_func, radial_col_span);
   }
   else if (Gradient.Type IS VGT::CONTOUR) { // NOTE: Contouring requires a bounding box and is thus incompatible with UserSpaceOnUse
      if (Gradient.Units != VUNIT::BOUNDING_BOX) return;

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

      if (State.mClipStack->empty()) {
         agg::scanline_u8 scanline;
         agg::render_scanlines(Raster, scanline, solidrender_gradient);
      }
      else { // Masked gradient
         agg::alpha_mask_gray8 alpha_mask(State.mClipStack->top().m_renderer);
         agg::scanline_u8_am<agg::alpha_mask_gray8> masked_scanline(alpha_mask);
         agg::render_scanlines(Raster, masked_scanline, solidrender_gradient);
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

static void fill_pattern(VectorState &State, const TClipRectangle<double> &Bounds, agg::path_storage *Path,
   VSM SampleMethod, const agg::trans_affine &Transform, double ViewWidth, double ViewHeight,
   extVectorPattern &Pattern, agg::renderer_base<agg::pixfmt_psl> &RenderBase,
   agg::rasterizer_scanline_aa<> &Raster)
{
   const double c_width  = (Pattern.Units IS VUNIT::USERSPACE) ? ViewWidth : Bounds.width();
   const double c_height = (Pattern.Units IS VUNIT::USERSPACE) ? ViewHeight : Bounds.height();
   const double x_offset = (Pattern.Units IS VUNIT::USERSPACE) ? 0 : Bounds.left;
   const double y_offset = (Pattern.Units IS VUNIT::USERSPACE) ? 0 : Bounds.top;

   Path->approximation_scale(Transform.scale());

   if (Pattern.Units IS VUNIT::USERSPACE) { // Use fixed coordinates specified in the pattern.
      double dwidth, dheight;
      if (dmf::hasScaledWidth(Pattern.Dimensions)) dwidth = c_width * Pattern.Width;
      else if (dmf::hasWidth(Pattern.Dimensions)) dwidth = Pattern.Width;
      else dwidth = 1;

      if (dmf::hasScaledHeight(Pattern.Dimensions)) dheight = c_height * Pattern.Height;
      else if (dmf::hasHeight(Pattern.Dimensions)) dheight = Pattern.Height;
      else dheight = 1;

      if ((dwidth != Pattern.Scene->PageWidth) or (dheight != Pattern.Scene->PageHeight)) {
         Pattern.Scene->PageWidth = dwidth;
         Pattern.Scene->PageHeight = dheight;
         mark_dirty(Pattern.Scene->Viewport, RC::ALL);
      }
   }
   else {
      // BOUNDING_BOX.  The pattern (x,y) is an optional offset applied to the base position of the vector's
      // path.  The area is relative to the vector's bounds.

      double dwidth, dheight;

      if (dmf::hasScaledWidth(Pattern.Dimensions)) dwidth = Pattern.Width * c_width;
      else if (dmf::hasWidth(Pattern.Dimensions)) {
         if (Pattern.Viewport->vpViewWidth) dwidth = (Pattern.Width / Pattern.Viewport->vpViewWidth) * c_width;
         else dwidth = Pattern.Width * c_width; // A quirk of SVG is that the fixed value has to be interpreted as a multiplier if the viewbox is unspecified.
      }
      else dwidth = 1;

      if (dmf::hasScaledHeight(Pattern.Dimensions)) dheight = Pattern.Height * c_height;
      else if (dmf::hasHeight(Pattern.Dimensions)) {
         if (Pattern.Viewport->vpViewHeight) dheight = (Pattern.Height / Pattern.Viewport->vpViewHeight) * c_height;
         else dheight = Pattern.Height * c_height;
      }
      else dheight = 1;

      if ((dwidth != Pattern.Scene->PageWidth) or (dheight != Pattern.Scene->PageHeight)) {
         if ((dwidth < 1) or (dheight < 1) or (dwidth > 8192) or (dheight > 8192)) {
            // Dimensions in excess of reasonable values can occur if the user is confused over the application
            // of bounding-box values that are being scaled.
            pf::Log log(__FUNCTION__);
            log.warning("Invalid pattern dimensions of %gx%g detected.", dwidth, dheight);
            dwidth  = 1;
            dheight = 1;
         }
         Pattern.Scene->PageWidth  = dwidth;
         Pattern.Scene->PageHeight = dheight;
         mark_dirty(Pattern.Scene->Viewport, RC::ALL);
      }
   }

   // Redraw the pattern source if any part of the definition is marked as dirty.
   if ((check_dirty((extVector *)Pattern.Scene->Viewport)) or (!Pattern.Bitmap)) {
      if (acDraw(&Pattern) != ERR::Okay) return;
   }

   agg::trans_affine transform;

   double dx, dy;
   if (dmf::hasScaledX(Pattern.Dimensions)) dx = x_offset + (c_width * Pattern.X);
   else if (dmf::hasX(Pattern.Dimensions)) dx = x_offset + Pattern.X;
   else dx = x_offset;

   if (dmf::hasScaledY(Pattern.Dimensions)) dy = y_offset + c_height * Pattern.Y;
   else if (dmf::hasY(Pattern.Dimensions)) dy = y_offset + Pattern.Y;
   else dy = y_offset;

   if (Pattern.Matrices) {
      auto &m = *Pattern.Matrices;
      transform.load_all(m.ScaleX, m.ShearY, m.ShearX, m.ScaleY, m.TranslateX + dx, m.TranslateY + dy);
   }
   else transform.translate(dx, dy);

   if (Pattern.Units != VUNIT::USERSPACE) transform *= Transform;

   transform.invert(); // Required

   if (!State.mClipStack->empty()) {
      agg::alpha_mask_gray8 alpha_mask(State.mClipStack->top().m_renderer);
      agg::scanline_u8_am<agg::alpha_mask_gray8> masked_scanline(alpha_mask);
      drawBitmap(masked_scanline, SampleMethod, RenderBase, Raster, Pattern.Bitmap, Pattern.SpreadMethod, Pattern.Opacity, &transform);
   }
   else {
      agg::scanline_u8 scanline;
      drawBitmap(scanline, SampleMethod, RenderBase, Raster, Pattern.Bitmap, Pattern.SpreadMethod, Pattern.Opacity, &transform);
   }
}
