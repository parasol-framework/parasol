
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

static void fill_image(VectorState &State, const TClipRectangle<DOUBLE> &Bounds, agg::path_storage &Path, VSM SampleMethod,
   const agg::trans_affine &Transform, DOUBLE ViewWidth, DOUBLE ViewHeight,
   objVectorImage &Image, agg::renderer_base<agg::pixfmt_psl> &RenderBase,
   agg::rasterizer_scanline_aa<> &Raster, DOUBLE Alpha)
{
   const DOUBLE c_width  = (Image.Units IS VUNIT::USERSPACE) ? ViewWidth : Bounds.width();
   const DOUBLE c_height = (Image.Units IS VUNIT::USERSPACE) ? ViewHeight : Bounds.height();
   const DOUBLE dx = Bounds.left + (dmf::hasScaledX(Image.Dimensions) ? (c_width * Image.X) : Image.X);
   const DOUBLE dy = Bounds.top + (dmf::hasScaledY(Image.Dimensions) ? (c_height * Image.Y) : Image.Y);

   Path.approximation_scale(Transform.scale());

   DOUBLE x_scale, y_scale, x_offset, y_offset;
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

static void fill_gradient(VectorState &State, const TClipRectangle<DOUBLE> &Bounds, agg::path_storage *Path,
   const agg::trans_affine &Transform, DOUBLE ViewWidth, DOUBLE ViewHeight, const extVectorGradient &Gradient,
   GRADIENT_TABLE *Table, agg::renderer_base<agg::pixfmt_psl> &RenderBase,
   agg::rasterizer_scanline_aa<> &Raster)
{
   typedef agg::span_interpolator_linear<> interpolator_type;
   typedef agg::span_allocator<agg::rgba8> span_allocator_type;
   typedef agg::pod_auto_array<agg::rgba8, 256> color_array_type;
   typedef agg::renderer_base<agg::pixfmt_psl>  RENDERER_BASE_TYPE;

   agg::trans_affine   transform;
   interpolator_type   span_interpolator(transform);
   span_allocator_type span_allocator;

   const DOUBLE c_width = Gradient.Units IS VUNIT::USERSPACE ? ViewWidth : Bounds.width();
   const DOUBLE c_height = Gradient.Units IS VUNIT::USERSPACE ? ViewHeight : Bounds.height();
   const DOUBLE x_offset = Gradient.Units IS VUNIT::USERSPACE ? 0 : Bounds.left;
   const DOUBLE y_offset = Gradient.Units IS VUNIT::USERSPACE ? 0 : Bounds.top;

   Path->approximation_scale(Transform.scale());

   if (Gradient.Type IS VGT::LINEAR) {
      TClipRectangle<DOUBLE> area;

      if ((Gradient.Flags & VGF::SCALED_X1) != VGF::NIL) area.left = x_offset + (c_width * Gradient.X1);
      else area.left = x_offset + Gradient.X1;

      if ((Gradient.Flags & VGF::SCALED_X2) != VGF::NIL) area.right = x_offset + (c_width * Gradient.X2);
      else area.right = x_offset + Gradient.X2;

      if ((Gradient.Flags & VGF::SCALED_Y1) != VGF::NIL) area.top = y_offset + (c_height * Gradient.Y1);
      else area.top = y_offset + Gradient.Y1;

      if ((Gradient.Flags & VGF::SCALED_Y2) != VGF::NIL) area.bottom = y_offset + (c_height * Gradient.Y2);
      else area.bottom = y_offset + Gradient.Y2;

      // Calculate the gradient's transition from the point at (x1,y1) to (x2,y2)

      const DOUBLE dx = area.width();
      const DOUBLE dy = area.height();
      transform.scale(sqrt((dx * dx) + (dy * dy)) / 256.0);
      transform.rotate(atan2(dy, dx));
      transform.translate(area.left, area.top);
      apply_transforms(Gradient, transform);
      transform *= Transform;
      transform.invert();

      agg::gradient_x gradient_func; // gradient_x is a horizontal gradient with infinite height
      typedef agg::span_gradient<agg::rgba8, interpolator_type, agg::gradient_x, color_array_type> span_gradient_type;
      typedef agg::renderer_scanline_aa<RENDERER_BASE_TYPE, span_allocator_type, span_gradient_type> renderer_gradient_type;
      span_gradient_type  span_gradient(span_interpolator, gradient_func, *Table, 0, 256);
      renderer_gradient_type solidgrad(RenderBase, span_allocator, span_gradient);

      if (State.mClipStack->empty()) {
         agg::scanline_u8 scanline;
         agg::render_scanlines(Raster, scanline, solidgrad);
      }
      else { // Masked gradient
         agg::alpha_mask_gray8 alpha_mask(State.mClipStack->top().m_renderer);
         agg::scanline_u8_am<agg::alpha_mask_gray8> masked_scanline(alpha_mask);

         agg::render_scanlines(Raster, masked_scanline, solidgrad);
      }
   }
   else if (Gradient.Type IS VGT::RADIAL) {
      agg::point_d c, f;

      if ((Gradient.Flags & VGF::SCALED_CX) != VGF::NIL) c.x = x_offset + (c_width * Gradient.CenterX);
      else c.x = x_offset + Gradient.CenterX;

      if ((Gradient.Flags & VGF::SCALED_CY) != VGF::NIL) c.y = y_offset + (c_height * Gradient.CenterY);
      else c.y = y_offset + Gradient.CenterY;

      if ((Gradient.Flags & VGF::SCALED_FX) != VGF::NIL) f.x = x_offset + (c_width * Gradient.FX);
      else if ((Gradient.Flags & VGF::FIXED_FX) != VGF::NIL) f.x = x_offset + Gradient.FX;
      else f.x = x_offset + c.x;

      if ((Gradient.Flags & VGF::SCALED_FY) != VGF::NIL) f.y = y_offset + (c_height * Gradient.FY);
      else if ((Gradient.Flags & VGF::FIXED_FY) != VGF::NIL) f.y = y_offset + Gradient.FY;
      else f.y = y_offset + c.y;

      if ((c.x IS f.x) and (c.y IS f.y)) {
         // Standard radial gradient, where the focal point is the same as the gradient center

         DOUBLE length = Gradient.Radius;
         if (Gradient.Units IS VUNIT::USERSPACE) { // Coordinates are relative to the viewport
            if ((Gradient.Flags & VGF::SCALED_RADIUS) != VGF::NIL) { // Gradient is a ratio of the viewport's dimensions
               length = (ViewWidth + ViewHeight) * Gradient.Radius * 0.5;
            }
         }
         else { // Coordinates are scaled to the bounding box
            if ((Gradient.Flags & VGF::SCALED_RADIUS) != VGF::NIL) {
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

         transform.translate(c.x, c.y);
         apply_transforms(Gradient, transform);
         transform *= Transform;
         transform.invert();

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
      else {
         // Radial gradient with a displaced focal point (uses agg::gradient_radial_focus).  NB: In early versions of
         // the SVG standard, the focal point had to be within the radius.  Later specifications allowed it to
         // be placed outside of the radius.

         DOUBLE fix_radius = Gradient.Radius;
         if ((Gradient.Flags & VGF::SCALED_RADIUS) != VGF::NIL) {
            fix_radius *= (c_width + c_height) * 0.5; // Use the average radius of the ellipse.
         }
         DOUBLE length = fix_radius;

         if (Gradient.Units IS VUNIT::USERSPACE) {
            if ((Gradient.Flags & VGF::SCALED_RADIUS) != VGF::NIL) {
               const DOUBLE scale = length * Gradient.Radius;
               transform *= agg::trans_affine_scaling(sqrt((ViewWidth * ViewWidth) + (ViewHeight * ViewHeight)) / scale);
            }
            else transform *= agg::trans_affine_scaling(Gradient.Radius * 0.01);
         }
         else { // Bounding box
            if ((Gradient.Flags & VGF::SCALED_RADIUS) != VGF::NIL) {
               // In AGG, an unscaled gradient will cover the entire bounding box according to the diagonal.
               // In SVG a radius of 50% must result in the edge of the radius meeting the edge of the bounding box.

               auto  scale = agg::point_d(Gradient.Radius * (1.0 / 0.707106781), Gradient.Radius * (1.0 / 0.707106781));
               if (c_height > c_width) scale.y *= c_height / c_width;
               else if (c_width > c_height) scale.x *= c_width / c_height;
               scale.x *= 100.0 / length; // Adjust the scale according to the gradient length.
               scale.y *= 100.0 / length;
               transform.scale(scale);
            }
            else transform *= agg::trans_affine_scaling(Gradient.Radius * 0.01);
         }

         agg::gradient_radial_focus gradient_func(fix_radius, f.x - c.x, f.y - c.y);

         typedef agg::span_gradient<agg::rgba8, interpolator_type, agg::gradient_radial_focus, color_array_type> span_gradient_type;
         typedef agg::renderer_scanline_aa<RENDERER_BASE_TYPE, span_allocator_type, span_gradient_type> renderer_gradient_type;
         span_gradient_type  span_gradient(span_interpolator, gradient_func, *Table, 0, fix_radius);
         renderer_gradient_type solidrender_gradient(RenderBase, span_allocator, span_gradient);

         transform.translate(c);
         apply_transforms(Gradient, transform);
         transform *= Transform;
         transform.invert();

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
   else if (Gradient.Type IS VGT::DIAMOND) {
      agg::point_d c;

      if ((Gradient.Flags & VGF::SCALED_CX) != VGF::NIL) c.x = x_offset + (c_width * Gradient.CenterX);
      else c.x = x_offset + Gradient.CenterX;

      if ((Gradient.Flags & VGF::SCALED_CY) != VGF::NIL) c.y = y_offset + (c_height * Gradient.CenterY);
      else c.y = y_offset + Gradient.CenterY;

      // Standard diamond gradient, where the focal point is the same as the gradient center

      const DOUBLE length = 255;
      if (Gradient.Units IS VUNIT::USERSPACE) {
         if ((Gradient.Flags & VGF::SCALED_RADIUS) != VGF::NIL) {
            const DOUBLE scale = length * Gradient.Radius;
            transform *= agg::trans_affine_scaling(sqrt((c_width * c_width) + (c_height * c_height)) / scale);
         }
         else transform *= agg::trans_affine_scaling(Gradient.Radius * 0.01);
      }
      else { // Align to vector's bounding box
         if ((Gradient.Flags & VGF::SCALED_RADIUS) != VGF::NIL) {
            // In AGG, an unscaled gradient will cover the entire bounding box according to the diagonal.
            // In SVG a radius of 50% must result in the edge of the radius meeting the edge of the bounding box.

            auto scale = agg::point_d(Gradient.Radius * (1.0 / 0.707106781), Gradient.Radius * (1.0 / 0.707106781));
            if (c_height > c_width) scale.y *= c_height / c_width;
            else if (c_width > c_height) scale.x *= c_width / c_height;
            scale.x *= 100.0 / length; // Adjust the scale according to the gradient length.
            scale.y *= 100.0 / length;
            transform.scale(scale.x, scale.y);
         }
         else transform *= agg::trans_affine_scaling(Gradient.Radius * 0.01);
      }

      agg::gradient_diamond gradient_func;
      typedef agg::span_gradient<agg::rgba8, interpolator_type, agg::gradient_diamond, color_array_type> span_gradient_type;
      typedef agg::renderer_scanline_aa<RENDERER_BASE_TYPE, span_allocator_type, span_gradient_type> renderer_gradient_type;
      span_gradient_type  span_gradient(span_interpolator, gradient_func, *Table, 0, length);
      renderer_gradient_type solidrender_gradient(RenderBase, span_allocator, span_gradient);

      transform.translate(c);
      apply_transforms(Gradient, transform);
      transform *= Transform;
      transform.invert();

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
   else if (Gradient.Type IS VGT::CONIC) {
      agg::point_d c;

      if ((Gradient.Flags & VGF::SCALED_CX) != VGF::NIL) c.x = x_offset + (c_width * Gradient.CenterX);
      else c.x = x_offset + Gradient.CenterX;

      if ((Gradient.Flags & VGF::SCALED_CY) != VGF::NIL) c.y = y_offset + (c_height * Gradient.CenterY);
      else c.y = y_offset + Gradient.CenterY;

      // Standard conic gradient, where the focal point is the same as the gradient center

      const DOUBLE length = 255;
      if (Gradient.Units IS VUNIT::USERSPACE) {
         if ((Gradient.Flags & VGF::SCALED_RADIUS) != VGF::NIL) {
            const DOUBLE scale = length * Gradient.Radius;
            transform *= agg::trans_affine_scaling(sqrt((c_width * c_width) + (c_height * c_height)) / scale);
         }
         else transform *= agg::trans_affine_scaling(Gradient.Radius * 0.01);
      }
      else { // Bounding box
         if ((Gradient.Flags & VGF::SCALED_RADIUS) != VGF::NIL) {
            // In AGG, an unscaled gradient will cover the entire bounding box according to the diagonal.
            // In SVG a radius of 50% must result in the edge of the radius meeting the edge of the bounding box.

            agg::point_d scale(Gradient.Radius * (1.0 / 0.707106781), Gradient.Radius * (1.0 / 0.707106781));
            if (c_height > c_width) scale.y *= c_height / c_width;
            else if (c_width > c_height) scale.x *= c_width / c_height;
            scale.x *= 100.0 / length; // Adjust the scale according to the gradient length.
            scale.y *= 100.0 / length;
            transform.scale(scale);
         }
         else transform *= agg::trans_affine_scaling(Gradient.Radius * 0.01);
      }

      agg::gradient_conic gradient_func;
      typedef agg::span_gradient<agg::rgba8, interpolator_type, agg::gradient_conic, color_array_type> span_gradient_type;
      typedef agg::renderer_scanline_aa<RENDERER_BASE_TYPE, span_allocator_type, span_gradient_type> renderer_gradient_type;
      span_gradient_type  span_gradient(span_interpolator, gradient_func, *Table, 0, length);
      renderer_gradient_type solidrender_gradient(RenderBase, span_allocator, span_gradient);

      transform.translate(c);
      apply_transforms(Gradient, transform);
      transform *= Transform;
      transform.invert();

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

static void fill_pattern(VectorState &State, const TClipRectangle<DOUBLE> &Bounds, agg::path_storage *Path,
   VSM SampleMethod, const agg::trans_affine &Transform, DOUBLE ViewWidth, DOUBLE ViewHeight,
   extVectorPattern &Pattern, agg::renderer_base<agg::pixfmt_psl> &RenderBase,
   agg::rasterizer_scanline_aa<> &Raster)
{
   const DOUBLE c_width  = (Pattern.Units IS VUNIT::USERSPACE) ? ViewWidth : Bounds.width();
   const DOUBLE c_height = (Pattern.Units IS VUNIT::USERSPACE) ? ViewHeight : Bounds.height();
   const DOUBLE x_offset = (Pattern.Units IS VUNIT::USERSPACE) ? 0 : Bounds.left;
   const DOUBLE y_offset = (Pattern.Units IS VUNIT::USERSPACE) ? 0 : Bounds.top;

   Path->approximation_scale(Transform.scale());

   if (Pattern.Units IS VUNIT::USERSPACE) { // Use fixed coordinates specified in the pattern.
      DOUBLE dwidth, dheight;
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

      DOUBLE dwidth, dheight;

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

   DOUBLE dx, dy;
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
