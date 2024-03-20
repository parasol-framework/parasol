// TODO: Currently mask bitmaps are created and torn down on each drawing cycle.  We may be able to cache the bitmaps
// with Vectors when they request a mask.  Bear in mind that caching has to be on a per-vector basis and not in the
// VectorClip itself due to the fact that a given VectorClip can be referenced by many vectors.

//********************************************************************************************************************
// This function recursively draws all child vectors to a bitmap mask in an additive way.
//
// TODO: Currently the paths are transformed dynamically, but we could store a transformed 'MaskPath' permanently with
// the vectors that use them.  When the vector path is dirty, we can clear the MaskPath to force recomputation when
// required.
//
// SVG stipulates that masks constructed from RGB colours use the luminance formula to convert them to a greyscale
// value: ".2126R + .7152G + .0722B".  The best way to apply this is to convert solid colour values to their
// luminesence value prior to drawing them.

void SceneRenderer::ClipBuffer::draw_clips(SceneRenderer &Render, extVector *Shape, agg::rasterizer_scanline_aa<> &Raster,
   agg::renderer_base<agg::pixfmt_gray8> &Base, const agg::trans_affine &Transform)
{
   agg::scanline32_p8 sl;
   for (auto node=Shape; node; node=(extVector *)node->Next) {
      if (node->Class->BaseClassID IS ID_VECTOR) {
         if (node->Visibility != VIS::VISIBLE);
         else if (!node->BasePath.empty()) {
            auto t = node->Transform * Transform;

            if (node->ClipRule IS VFR::NON_ZERO) Raster.filling_rule(agg::fill_non_zero);
            else if (node->ClipRule IS VFR::EVEN_ODD) Raster.filling_rule(agg::fill_even_odd);

            agg::renderer_scanline_aa_solid<agg::renderer_base<agg::pixfmt_gray8>> solid(Base);

            if ((m_clip->Flags & (VCLF::APPLY_STROKES|VCLF::APPLY_FILLS)) != VCLF::NIL) {
               if ((m_clip->Flags & VCLF::APPLY_FILLS) != VCLF::NIL) {
                  // When the APPLY_FILLS option is enabled, regular fill painting rules will be applied.

                  if ((node->Fill->Colour.Alpha > 0) and (!node->DisableFillColour)) {
                     DOUBLE value = (node->Fill->Colour.Red * 0.2126) + (node->Fill->Colour.Green * 0.7152) + (node->Fill->Colour.Blue * 0.0722);
                     value *= node->FillOpacity;
                     solid.color(agg::gray8(value * 0xff, 0xff));

                     agg::conv_transform<agg::path_storage, agg::trans_affine> final_path(node->BasePath, t);
                     Raster.reset();
                     Raster.add_path(final_path);
                     agg::render_scanlines(Raster, sl, solid);
                  }

                  if ((node->Fill->Gradient) or (node->Fill->Image) or (node->Fill->Pattern)) {
                     // The fill routines are written for 32-bit colour rendering, so we have to use active conversion
                     // of RGB colours to grey-scale.  This isn't that much of a problem if the masks remain static and
                     // we cache the results.

                     VectorState state;
                     agg::pixfmt_psl pixf;
                     agg::renderer_base<agg::pixfmt_psl> rb(pixf);
                     ColourFormat cf; // Dummy, not required
                     pixf.rawBitmap(m_bitmap.data(), m_width, m_height, m_width, 8, cf, true);
                     rb.attach(pixf);

                     agg::conv_transform<agg::path_storage, agg::trans_affine> final_path(node->BasePath, t);
                     Raster.reset();
                     Raster.add_path(final_path);

                     if (node->Fill->Gradient) {
                        if (auto table = get_fill_gradient_table(node->Fill[0], state.mOpacity * node->FillOpacity)) {
                           fill_gradient(state, node->Bounds, &node->BasePath, t, Render.view_width(), Render.view_height(),
                              *((extVectorGradient *)node->Fill->Gradient), table, rb, Raster);
                        }
                     }

                     if (node->Fill->Image) { // Bitmap image fill.  NB: The SVG class creates a standard VectorRectangle and associates an image with it in order to support <image> tags.
                        fill_image(state, node->Bounds, node->BasePath, node->Scene->SampleMethod, t,
                           Render.view_width(), Render.view_height(), *node->Fill->Image, rb, Raster,
                           node->FillOpacity);
                     }

                     if (node->Fill->Pattern) {
                        fill_pattern(state, node->Bounds, &node->BasePath, node->Scene->SampleMethod, t,
                           Render.view_width(), Render.view_height(), *((extVectorPattern *)node->Fill->Pattern), rb, Raster);
                     }
                  }
               }

               if ((m_clip->Flags & VCLF::APPLY_STROKES) != VCLF::NIL) {
                  if (node->StrokeRaster) {
                     DOUBLE value = (node->Stroke.Colour.Red * 0.2126) + (node->Stroke.Colour.Green * 0.7152) + (node->Stroke.Colour.Blue * 0.0722);
                     value *= node->StrokeOpacity;
                     solid.color(agg::gray8(value * 0xff, 0xff));

                     agg::conv_stroke<agg::path_storage> stroked_path(node->BasePath);
                     configure_stroke(*node, stroked_path);
                     agg::conv_transform<agg::conv_stroke<agg::path_storage>, agg::trans_affine> final_path(stroked_path, t);

                     Raster.reset();
                     Raster.add_path(final_path);
                     agg::render_scanlines(Raster, sl, solid);
                  }
               }
            }
            else {
               // Regular 'clipping path' rules enabled.  SVG states that all paths are filled and stroking is not
               // supported in this mode.

               solid.color(agg::gray8(0xff, 0xff));
               agg::conv_transform<agg::path_storage, agg::trans_affine> final_path(node->BasePath, t);
               Raster.reset();
               Raster.add_path(final_path);
               agg::render_scanlines(Raster, sl, solid);
            }
         }
      }

      if (node->Child) draw_clips(Render, (extVector *)node->Child, Raster, Base, Transform);
   }
}

//********************************************************************************************************************

void SceneRenderer::ClipBuffer::resize_bitmap(LONG X, LONG Y, LONG Width, LONG Height)
{
   if ((Width <= 0) or (Height <= 0)) Width = Height = 1;

   if (Width > 8192)  Width = 8192;
   if (Height > 8192) Height = 8192;

   m_bitmap.resize(Width * Height);

   m_width  = Width;
   m_height = Height;

   Y *= m_width;

   if (X < 0) X = 0;
   if (Y < 0) Y = 0;

   if ((X < m_width) and (Y < m_height)) {
      for (; Y < m_height; Y += m_width) {
         ClearMemory(m_bitmap.data() + Y + X, m_width - X);
      }
   }
}

//********************************************************************************************************************
// Called by the scene graph renderer to generate a bitmap mask for a non-rectangular (transformed) viewport.

void SceneRenderer::ClipBuffer::draw_viewport(SceneRenderer &Render)
{
   auto vp = (extVectorViewport *)m_shape;
   if (vp->dirty()) {
      gen_vector_path(vp);
      vp->Dirty = RC::NIL;
   }

   resize_bitmap(F2T(vp->vpBounds.left), F2T(vp->vpBounds.top), F2T(vp->vpBounds.right) + 2, F2T(vp->vpBounds.bottom) + 2);

   m_renderer.attach(m_bitmap.data(), m_width-1, m_height-1, m_width);
   agg::pixfmt_gray8 pixf(m_renderer);
   agg::renderer_base<agg::pixfmt_gray8> rb(pixf);
   agg::renderer_scanline_aa_solid<agg::renderer_base<agg::pixfmt_gray8>> solid(rb);
   agg::rasterizer_scanline_aa<> rasterizer;

   solid.color(agg::gray8(0xff, 0xff));

   if (!vp->BasePath.empty()) {
      agg::scanline32_p8 sl;
      agg::path_storage final_path(vp->BasePath);

      rasterizer.reset();
      rasterizer.add_path(final_path);
      agg::render_scanlines(rasterizer, sl, solid);
   }
}

//********************************************************************************************************************

void SceneRenderer::ClipBuffer::draw(SceneRenderer &Render)
{
   if (!m_clip->Viewport->Child) {
      pf::Log log;
      log.warning("Clipping viewport has no assigned children.");
      return;
   }

   if (!m_clip->Viewport->Matrices) {
      VectorMatrix *matrix;
      vecNewMatrix(m_clip->Viewport, &matrix);
   }

   if (m_clip->Units IS VUNIT::BOUNDING_BOX) draw_bounding_box(Render);
   else draw_userspace(Render);
}

//********************************************************************************************************************

void SceneRenderer::ClipBuffer::draw_userspace(SceneRenderer &Render)
{
   // The target area is the viewport that owns m_shape

   acRedimension(m_clip->Viewport, m_shape->ParentView->vpViewX, m_shape->ParentView->vpViewY, 0,
      get_parent_width(m_shape), get_parent_height(m_shape), 0);

   // The source area (viewbox) matches the dimensions of m_shape's parent viewport

   m_clip->Viewport->setFields(fl::ViewWidth(get_parent_width(m_shape)), fl::ViewHeight(get_parent_height(m_shape)));

   // Transforms: Client transforms for the shape are included, but not its (X,Y) position.
   // All parent transforms are then applied.

   agg::trans_affine transform;
   apply_transforms(*m_shape, transform);
   apply_parent_transforms(get_parent(m_shape), transform);

   m_clip->Viewport->Matrices->ScaleX = transform.sx;
   m_clip->Viewport->Matrices->ScaleY = transform.sy;
   m_clip->Viewport->Matrices->ShearX = transform.shx;
   m_clip->Viewport->Matrices->ShearY = transform.shy;
   m_clip->Viewport->Matrices->TranslateX = transform.tx;
   m_clip->Viewport->Matrices->TranslateY = transform.ty;

   m_clip->Bounds = TCR_EXPANDING;
   calc_full_boundary((extVector *)m_clip->Viewport, m_clip->Bounds, false, true, true);

   if (m_clip->Bounds.left > m_clip->Bounds.right) return; // Return if no paths were defined.

   agg::path_storage clip_bound_path = m_clip->Bounds.as_path();
   auto clip_bound_final = get_bounds(clip_bound_path);

   resize_bitmap(F2T(clip_bound_final.left), F2T(clip_bound_final.top), F2T(clip_bound_final.right) + 2, F2T(clip_bound_final.bottom) + 2);

   // Every child vector of the VectorClip that exports a path will be rendered to the mask.

   m_renderer.attach(m_bitmap.data(), m_width-1, m_height-1, m_width);
   agg::pixfmt_gray8 pixf(m_renderer);
   agg::renderer_base<agg::pixfmt_gray8> rb(pixf);
   agg::rasterizer_scanline_aa<> rasterizer;

   draw_clips(Render, (extVector *)m_clip->Viewport->Child, rasterizer, rb, agg::trans_affine());
}

//********************************************************************************************************************

void SceneRenderer::ClipBuffer::draw_bounding_box(SceneRenderer &Render)
{
   TClipRectangle<DOUBLE> shape_bounds = TCR_EXPANDING; // Bounds *without transforms*
   calc_full_boundary(m_shape, shape_bounds, false, false, false);

   // Set the target area to mock the shape.  The viewbox will remain at (0 0 1 1), or whatever the
   // client has defined if the default is overridden.

   acRedimension(m_clip->Viewport, shape_bounds.left, shape_bounds.top, 0,
      shape_bounds.width(), shape_bounds.height(), 0);

   if (m_shape->Matrices) {
      reset_matrix(*m_clip->Viewport->Matrices);
      for (auto t=m_shape->Matrices; t; t=t->Next) {
         *m_clip->Viewport->Matrices *= *t;
      }
   }

   m_clip->Bounds = TCR_EXPANDING;
   calc_full_boundary((extVector *)m_clip->Viewport, m_clip->Bounds, false, true, true);

   if (m_clip->Bounds.left > m_clip->Bounds.right) return; // Return if no paths were defined.

   auto clip_bound_path = m_clip->Bounds.as_path(m_shape->Transform);
   auto clip_bound_final = get_bounds(clip_bound_path);

   resize_bitmap(F2T(clip_bound_final.left), F2T(clip_bound_final.top), F2T(clip_bound_final.right) + 2, F2T(clip_bound_final.bottom) + 2);

   // Every child vector of the VectorClip that exports a path will be rendered to the mask.

   m_renderer.attach(m_bitmap.data(), m_width-1, m_height-1, m_width);
   agg::pixfmt_gray8 pixf(m_renderer);
   agg::renderer_base<agg::pixfmt_gray8> rb(pixf);
   agg::rasterizer_scanline_aa<> rasterizer;

   draw_clips(Render, (extVector *)m_clip->Viewport->Child, rasterizer, rb, m_shape->Transform);
}
