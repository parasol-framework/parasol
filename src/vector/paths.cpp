
#include "agg_trans_single_path.h"

//********************************************************************************************************************

extVectorViewport * get_parent_view(extVector *Vector)
{
   if (Vector->ParentView) return Vector->ParentView;
   else {
      auto node = get_parent(Vector);
      while (node) {
         if (node->Class->ClassID IS ID_VECTORVIEWPORT) {
            Vector->ParentView = (extVectorViewport *)node;
            return Vector->ParentView;
         }
         else if (node->Parent->Class->BaseClassID IS ID_VECTOR) node = (extVector *)(node->Parent);
         else return NULL;
      }
   }
   return NULL;
}

//********************************************************************************************************************
// This 'safe' version of gen_vector_path() checks that all parent vectors have been refreshed if they are marked
// as dirty.  Generation of the paths is top-down.

void gen_vector_tree(extVector *Vector)
{
   if (!Vector->initialised()) return;

   if (Vector->dirty()) {
      std::vector<objVector *> list;
      for (auto node=(objVector *)Vector->Parent; node; node=(objVector *)node->Parent) {
         if (node->Class->BaseClassID != ID_VECTOR) break;
         list.push_back(node);
      }

      std::for_each(list.rbegin(), list.rend(), [](auto v) {
         gen_vector_path((extVector *)v);
      });
   }

   gen_vector_path(Vector);
}

//********************************************************************************************************************
// (Re)Generates the path for a vector.  Switches off most of the Dirty flag markers.  For Viewports, the vpFixed*
// and boundary field values will all be set.  There is no recursion into child vectors.
//
// NOTE: If parent vectors are marked as dirty at the time of calling this function, any relative values will be
// computed from stale information and likely to produce the wrong result.  Use gen_vector_tree() to avoid
// such problems.

void gen_vector_path(extVector *Vector)
{
   pf::Log log(__FUNCTION__);

   if ((!Vector->GeneratePath) and (Vector->Class->ClassID != ID_VECTORVIEWPORT) and (Vector->Class->ClassID != ID_VECTORGROUP)) return;

   pf::SwitchContext context(Vector);

   log.traceBranch("%s: #%d, Dirty: $%.2x, ParentView: #%d", Vector->Class->ClassName, Vector->UID, LONG(Vector->Dirty), Vector->ParentView ? Vector->ParentView->UID : 0);

   auto parent_view = get_parent_view(Vector);

   Vector->PathTimestamp++;

   if (Vector->Class->ClassID IS ID_VECTORGROUP) {
      Vector->Transform.reset();
      apply_parent_transforms(Vector, Vector->Transform);
      Vector->Dirty &= ~RC::ALL; // Making out that the group has been refreshed is important
      return;
   }
   else if (Vector->Class->ClassID IS ID_VECTORVIEWPORT) {
      auto view = (extVectorViewport *)Vector;

      DOUBLE parent_width, parent_height;
      OBJECTID parent_id;

      // vpTargetX/Y are the display position of the viewport, relative to the container that it is inside.
      // vpBX1/BY1/BX2/BY2 are fixed coordinate bounding box values from root position (0,0) and define the clip region imposed on all children of the viewport.
      // vpFixedX/Y are the fixed coordinate position of the viewport relative to root position (0,0)

      if (!(view->vpDimensions & (DMF_X|DMF_X_OFFSET))) {
         // Client failed to set a horizontal position
         view->vpTargetX = 0;
         view->vpDimensions |= DMF_FIXED_X;
      }
      else if ((view->vpDimensions & DMF_X_OFFSET) and (!(view->vpDimensions & (DMF_X|DMF_WIDTH)))) {
         // Client set an offset but failed to combine it with a width or position value.
         view->vpTargetX = 0;
         view->vpDimensions |= DMF_FIXED_X;
      }

      if (!(view->vpDimensions & (DMF_Y|DMF_Y_OFFSET))) {
         // Client failed to set a vertical position
         view->vpTargetY = 0;
         view->vpDimensions |= DMF_FIXED_Y;
      }
      else if ((view->vpDimensions & DMF_Y_OFFSET) and (!(view->vpDimensions & (DMF_Y|DMF_HEIGHT)))) {
         // Client set an offset but failed to combine it with a height or position value.
         view->vpTargetY = 0;
         view->vpDimensions |= DMF_FIXED_Y;
      }

      if (parent_view) {
         if (parent_view->vpViewWidth) parent_width = parent_view->vpViewWidth;
         else parent_width = parent_view->vpFixedWidth;

         if (parent_view->vpViewHeight) parent_height = parent_view->vpViewHeight;
         else parent_height = parent_view->vpFixedHeight;

         if ((!parent_width) or (!parent_height)) {
            // NB: It is perfectly legal, even if unlikely, that a viewport has a width/height of zero.
            log.msg("Size of parent viewport #%d is %.2fx%.2f, dimensions $%.8x", parent_view->UID, parent_view->vpFixedWidth, parent_view->vpFixedHeight, parent_view->vpDimensions);
         }

         parent_id = parent_view->UID;
      }
      else {
         parent_width  = Vector->Scene->PageWidth;
         parent_height = Vector->Scene->PageHeight;
         parent_id     = Vector->Scene->UID;
      }

      // The user's values for destination (x,y) need to be taken into account. <svg x="" y=""/>
      // NB: In SVG it is a requirement that the top level viewport is always located at (0,0), but we
      // leave that as something for the SVG parser to enforce.

      if (view->vpDimensions & DMF_SCALED_X) view->FinalX = (parent_width * view->vpTargetX);
      else view->FinalX = view->vpTargetX;

      if (view->vpDimensions & DMF_SCALED_Y) view->FinalY = (parent_height * view->vpTargetY);
      else view->FinalY = view->vpTargetY;

      if (view->vpDimensions & DMF_SCALED_WIDTH) view->vpFixedWidth = parent_width * view->vpTargetWidth;
      else if (view->vpDimensions & DMF_FIXED_WIDTH) view->vpFixedWidth = view->vpTargetWidth;
      else view->vpFixedWidth = parent_width;

      if (view->vpDimensions & DMF_SCALED_HEIGHT) view->vpFixedHeight = parent_height * view->vpTargetHeight;
      else if (view->vpDimensions & DMF_FIXED_HEIGHT) view->vpFixedHeight = view->vpTargetHeight;
      else view->vpFixedHeight = parent_height;

      if (view->vpDimensions & DMF_SCALED_X_OFFSET) {
         if (view->vpDimensions & DMF_X) {
            view->vpFixedWidth = parent_width - (parent_width * view->vpTargetXO) - view->FinalX;
         }
         else view->FinalX = parent_width - view->vpFixedWidth - (parent_width * view->vpTargetXO);
      }
      else if (view->vpDimensions & DMF_FIXED_X_OFFSET) {
         if (view->vpDimensions & DMF_X) {
            view->vpFixedWidth = parent_width - view->vpTargetXO - view->FinalX;
         }
         else view->FinalX = parent_width - view->vpFixedWidth - view->vpTargetXO;
      }

      if (view->vpDimensions & DMF_SCALED_Y_OFFSET) {
         if (view->vpDimensions & DMF_Y) {
            view->vpFixedHeight = parent_height - (parent_height * view->vpTargetYO) - view->FinalY;
         }
         else view->FinalY = parent_height - view->vpFixedHeight - (parent_height * view->vpTargetYO);
      }
      else if (view->vpDimensions & DMF_FIXED_Y_OFFSET) {
         if (view->vpDimensions & DMF_Y) {
            view->vpFixedHeight = parent_height - view->vpTargetYO - view->FinalY;
         }
         else view->FinalY = parent_height - view->vpFixedHeight - view->vpTargetYO;
      }

      // Contained vectors are normally scaled to the area defined by the viewport.

      DOUBLE target_width  = view->vpFixedWidth;
      DOUBLE target_height = view->vpFixedHeight;

      // The client can force the top-level viewport to be resized by using VPF::RESIZE and defining PageWidth/PageHeight

      if ((Vector->Scene->Viewport IS (objVectorViewport *)Vector) and ((Vector->Scene->Flags & VPF::RESIZE) != VPF::NIL)) {
         log.trace("VPF::RESIZE enabled, using target size (%.2f %.2f)", parent_width, parent_height);
         target_width  = parent_width;
         target_height = parent_height;
         view->vpFixedWidth  = parent_width;
         view->vpFixedHeight = parent_height;
      }

      log.trace("Vector: #%d, Dimensions: $%.8x, Parent: #%d %.2fw %.2fh, Target: %.2fw %.2fh, Viewbox: %.2f %.2f %.2f %.2f",
         Vector->UID, view->vpDimensions, parent_id, parent_width, parent_height, target_width, target_height, view->vpViewX, view->vpViewY, view->vpViewWidth, view->vpViewHeight);

      // This part computes the alignment of the viewbox (source) within the viewport's target area.
      // AspectRatio choices affect this, e.g. "xMinYMin slice".  Note that alignment specifically impacts
      // the position of paths within the viewport and not the position of the viewport itself.

      calc_aspectratio(__FUNCTION__, view->vpAspectRatio, target_width, target_height,
         view->vpViewWidth, view->vpViewHeight, &view->vpAlignX, &view->vpAlignY,
         &view->vpXScale, &view->vpYScale);

      log.trace("AlignXY: %.2f %.2f, ScaleXY: %.2f %.2f", view->vpAlignX, view->vpAlignY, view->vpXScale, view->vpYScale);

      // Build the path for the vector and transform it.  Note: In SVG a viewport cannot have any transform directly
      // associated with it (but it can inherit transforms).  In our implementation a viewport CAN be transformed
      // directly.  This is done before the (X,Y) position is applied because this gives reliable & consistent
      // results in cases where the (X,Y) position is manually modified by the client in a UI for instance.

      Vector->Transform.reset();

      for (auto t=Vector->Matrices; t; t=t->Next) {
         Vector->Transform.multiply(t->ScaleX, t->ShearY, t->ShearX, t->ScaleY, t->TranslateX, t->TranslateY);
      }

      Vector->Transform.tx += Vector->FinalX;
      Vector->Transform.ty += Vector->FinalY;

      apply_parent_transforms(get_parent(Vector), Vector->Transform);

      Vector->BasePath.free_all();
      Vector->BasePath.move_to(0, 0); // Top left
      Vector->BasePath.line_to(view->vpFixedWidth, 0); // Top right
      Vector->BasePath.line_to(view->vpFixedWidth, view->vpFixedHeight); // Bottom right
      Vector->BasePath.line_to(0, view->vpFixedHeight); // Bottom left
      Vector->BasePath.close_polygon();

      Vector->BasePath.transform(Vector->Transform);

      // Compute the clipping boundary of the viewport and store it in the BX/Y fields.

      view->vpBounds = get_bounds(Vector->BasePath);

      // If the viewport uses a non-rectangular transform, a clipping mask will need to be generated based on its path.  The path is
      // pre-transformed and drawn in order to speed things up.

      if (((Vector->Transform.shx) or (Vector->Transform.shy)) and
          ((view->vpOverflowX != VOF::VISIBLE) or (view->vpOverflowY != VOF::VISIBLE))) {
         view->vpClip = true;
      }

      log.trace("Clipping boundary for #%d is %g %g %g %g",
         Vector->UID, view->vpBounds.left, view->vpBounds.top, view->vpBounds.right, view->vpBounds.bottom);

      Vector->Dirty &= ~(RC::TRANSFORM | RC::FINAL_PATH | RC::BASE_PATH);

      if (((extVectorScene *)Vector->Scene)->ResizeSubscriptions.contains(view)) {
         ((extVectorScene *)Vector->Scene)->PendingResizeMsgs.insert(view);
      }
   }
   else if (Vector->Class->BaseClassID IS ID_VECTOR) {
      Vector->FinalX = 0;
      Vector->FinalY = 0;
      if (((Vector->Dirty & RC::TRANSFORM) != RC::NIL) and (Vector->Class->ClassID != ID_VECTORTEXT)) {
         Vector->Transform.reset();
         apply_parent_transforms(Vector, Vector->Transform);

         Vector->Dirty = (Vector->Dirty & (~RC::TRANSFORM)) | RC::FINAL_PATH;
      }

      // Generate base path of the vector if it hasn't been done already or has been reset.
      // NB: The base path is computed after the transform because it can be helpful to know the
      // final scale of the vector, particularly for calculating curved paths.

      if ((Vector->Dirty & RC::BASE_PATH) != RC::NIL) {
         Vector->BasePath.free_all();

         Vector->GeneratePath(Vector, Vector->BasePath);

         if (Vector->AppendPath) {
            if (Vector->AppendPath->dirty()) gen_vector_path(Vector->AppendPath);

            if (Vector->AppendPath->Matrices) {
               agg::trans_affine trans;
               trans.tx += Vector->AppendPath->FinalX;
               trans.ty += Vector->AppendPath->FinalY;
               for (auto t=Vector->AppendPath->Matrices; t; t=t->Next) {
                  trans.multiply(t->ScaleX, t->ShearY, t->ShearX, t->ScaleY, t->TranslateX, t->TranslateY);
               }

               agg::conv_transform<agg::path_storage, agg::trans_affine> tp(Vector->AppendPath->BasePath, trans);
               if ((Vector->Flags & VF::JOIN_PATHS) != VF::NIL) Vector->BasePath.join_path(tp);
               else Vector->BasePath.concat_path(tp);

               auto bound_path = Vector->AppendPath->Bounds.as_path();
               bound_path.transform(trans);
               Vector->Bounds.expanding(get_bounds(bound_path));
            }
            else {
               if ((Vector->Flags & VF::JOIN_PATHS) != VF::NIL) Vector->BasePath.join_path(Vector->AppendPath->BasePath);
               else Vector->BasePath.concat_path(Vector->AppendPath->BasePath);
               Vector->Bounds.expanding(Vector->AppendPath);
            }
         }

         if ((Vector->Morph) and (Vector->Morph->Class->BaseClassID IS ID_VECTOR)) {
            if ((Vector->Class->ClassID IS ID_VECTORTEXT) and ((Vector->MorphFlags & VMF::STRETCH) IS VMF::NIL)) {
               // Do nothing for VectorText because it applies morph and transition effects during base path generation.
            }
            else {
               auto morph = (extVector *)Vector->Morph;

               if (morph->dirty()) gen_vector_path(morph);

               if (morph->BasePath.total_vertices()) {
                  DOUBLE bx1, bx2, by1, by2;

                  if ((Vector->MorphFlags & VMF::Y_MID) != VMF::NIL) {
                     bounding_rect_single(Vector->BasePath, 0, &bx1, &by1, &bx2, &by2);
                     Vector->BasePath.translate(0, -by1 - ((by2 - by1) * 0.5));
                  }
                  else if ((Vector->MorphFlags & VMF::Y_MIN) != VMF::NIL) {
                     if (Vector->Class->ClassID != ID_VECTORTEXT) {
                        bounding_rect_single(Vector->BasePath, 0, &bx1, &by1, &bx2, &by2);
                        Vector->BasePath.translate(0, -by1 -(by2 - by1));
                     }
                  }
                  else { // VMF::Y_MAX
                     if (Vector->Class->ClassID IS ID_VECTORTEXT) { // Only VectorText needs to be reset for yMax
                        bounding_rect_single(Vector->BasePath, 0, &bx1, &by1, &bx2, &by2);
                        Vector->BasePath.translate(0, -by1);
                     }
                  }

                  agg::trans_single_path trans_path;
                  morph->BasePath.approximation_scale(Vector->Transform.scale());
                  trans_path.add_path(morph->BasePath);
                  trans_path.preserve_x_scale(true); // The default is true.  Switching to false produces a lot of scrunching and extending
                  if (morph->Class->ClassID IS ID_VECTORPATH) { // Enforcing a fixed length along the path effectively causes a resize.
                     if (((extVectorPath *)morph)->PathLength > 0) trans_path.base_length(((extVectorPath *)morph)->PathLength);
                  }

                  Vector->BasePath.transform(trans_path); // Apply manipulation to the base path.
               }
            }
         }

         Vector->Dirty = (Vector->Dirty & (~RC::BASE_PATH)) | RC::FINAL_PATH;
      }

      // VectorText transform support is handled after base-path generation.  This is because vector text can be
      // aligned, for which the width and height of the base-path must be known.

      if (Vector->Class->ClassID IS ID_VECTORTEXT) {
         set_text_final_xy((extVectorText *)Vector);
         Vector->Transform.reset();
         apply_parent_transforms(Vector, Vector->Transform);
         Vector->Dirty = (Vector->Dirty & (~RC::TRANSFORM)) | RC::FINAL_PATH;
      }

      if (Vector->Matrices) {
         DOUBLE scale = Vector->Transform.scale();
         if (scale > 1.0) Vector->BasePath.angle_tolerance(0.2); // Set in radians.  The less this value is, the more accurate it will be at sharp turns.
         else Vector->BasePath.angle_tolerance(0);
      }

      //Vector->BasePath.cusp_limit(x); // Set in radians.  If more than 0, it restricts sharpness at the cusp (presumably for awkward angles).  Do not exceed 10-15 degrees

      if ((Vector->Fill[0].Colour.Alpha > 0) or (Vector->Fill[0].Gradient) or (Vector->Fill[0].Image) or (Vector->Fill[0].Pattern)) {
         if (!Vector->FillRaster) {
            Vector->FillRaster = new (std::nothrow) agg::rasterizer_scanline_aa<>;
            if (!Vector->FillRaster) return;
         }
         else Vector->FillRaster->reset();

         Vector->BasePath.approximation_scale(Vector->Transform.scale());
         agg::conv_transform<agg::path_storage, agg::trans_affine> fill_path(Vector->BasePath, Vector->Transform);
         Vector->FillRaster->add_path(fill_path);
      }
      else if (Vector->FillRaster) {
         delete Vector->FillRaster;
         Vector->FillRaster = NULL;
      }

      if (Vector->Stroked) {
         // Configure the curve algorithm so that it generates nicer looking curves when the vector is scaled up.  This
         // is not required if the vector scale is <= 1.0 (the angle_tolerance controls this).

         if (!Vector->StrokeRaster) {
            Vector->StrokeRaster = new (std::nothrow) agg::rasterizer_scanline_aa<>;
            if (!Vector->StrokeRaster) return;
         }
         else Vector->StrokeRaster->reset();

         if (Vector->DashArray) {
            Vector->BasePath.approximation_scale(Vector->Transform.scale());
            Vector->DashArray->path.attach(Vector->BasePath);
            configure_stroke(*Vector, Vector->DashArray->stroke);
            agg::conv_transform<agg::conv_stroke<agg::conv_dash<agg::path_storage>>, agg::trans_affine> stroke_path(Vector->DashArray->stroke, Vector->Transform);
            Vector->StrokeRaster->add_path(stroke_path);
         }
         else {
            Vector->BasePath.approximation_scale(Vector->Transform.scale());
            agg::conv_stroke<agg::path_storage> stroked_path(Vector->BasePath);
            configure_stroke(*Vector, stroked_path);
            agg::conv_transform<agg::conv_stroke<agg::path_storage>, agg::trans_affine> stroke_path(stroked_path, Vector->Transform);
            Vector->StrokeRaster->add_path(stroke_path);
         }
      }
      else if (Vector->StrokeRaster) {
         delete Vector->StrokeRaster;
         Vector->StrokeRaster = NULL;
      }

      Vector->Dirty &= ~RC::FINAL_PATH;
   }
   else log.warning("Target vector is not a shape.");

   send_feedback(Vector, FM::PATH_CHANGED);

   // Changes to the path could mean that the mouse cursor needs to be refreshed.
   ((extVectorScene *)Vector->Scene)->RefreshCursor = true;
}

//********************************************************************************************************************
// Apply all transforms in the correct SVG order to a target agg::trans_affine object.  The process starts with the
// vector passed in to the function, and proceeds upwards through the parent nodes.

void apply_parent_transforms(extVector *Start, agg::trans_affine &AGGTransform)
{
   pf::Log log(__FUNCTION__);

   for (auto node=Start; node; node=(extVector *)get_parent(node)) {
      if (node->Class->BaseClassID != ID_VECTOR) continue;

      if (node->Class->ClassID IS ID_VECTORVIEWPORT) {
         // When a viewport is encountered we need to make special considerations as to its viewbox, which affects both
         // position and scaling of all children.  Alignment is another factor that is taken care of here.

         auto view = (extVectorViewport *)node;

         DBG_TRANSFORM("Parent view #%d x/y: %.2f %.2f", node->UID, view->FinalX, view->FinalY);

         AGGTransform.tx -= view->vpViewX;
         AGGTransform.ty -= view->vpViewY;

         if ((view->vpXScale != 1.0) or (view->vpYScale != 1.0)) {
            if (std::isnan(view->vpXScale) or std::isnan(view->vpYScale)) {
               log.warning("[%d] Invalid viewport scale values: %f, %f", view->UID, view->vpXScale, view->vpYScale);
            }
            else {
               DBG_TRANSFORM("Viewport scales this vector to %.2f %.2f", view->vpXScale, view->vpYScale);
               AGGTransform.scale(view->vpXScale, view->vpYScale);
            }
         }

         for (auto t=node->Matrices; t; t=t->Next) {
            AGGTransform.multiply(t->ScaleX, t->ShearY, t->ShearX, t->ScaleY, t->TranslateX, t->TranslateY);
         }

         // Children of viewports are affected by the VP's alignment values.
         AGGTransform.tx += view->FinalX + view->vpAlignX;
         AGGTransform.ty += view->FinalY + view->vpAlignY;
      }
      else {
         log.trace("Parent vector #%d x/y: %.2f %.2f", node->UID, node->FinalX, node->FinalY);

         AGGTransform.tx += node->FinalX;
         AGGTransform.ty += node->FinalY;
         for (auto t=node->Matrices; t; t=t->Next) {
            AGGTransform.multiply(t->ScaleX, t->ShearY, t->ShearX, t->ScaleY, t->TranslateX, t->TranslateY);
         }
      }
   }
}
