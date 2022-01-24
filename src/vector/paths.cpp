
#include "agg_trans_single_path.h"

//****************************************************************************
// (Re)Generates the path for a vector.  Switches off most of the Dirty flag markers.
// For Viewports, the vpFixed* and boundary field values will all be set.

static void gen_vector_path(objVector *Vector)
{
   parasol::Log log(__FUNCTION__);

   if ((!Vector->GeneratePath) and (Vector->Head.SubID != ID_VECTORVIEWPORT)) return;

   parasol::SwitchContext context(Vector);

   log.traceBranch("%s: #%d, Dirty: $%.2x, ParentView: #%d", Vector->Head.Class->ClassName, Vector->Head.UniqueID, Vector->Dirty, Vector->ParentView ? Vector->ParentView->Head.UniqueID : 0);

   if (!Vector->ParentView) { // Find the nearest parent viewport if we don't have it already
      auto scan = get_parent(Vector);
      while (scan) {
         if (scan->SubID IS ID_VECTORVIEWPORT) {
            Vector->ParentView = (objVectorViewport *)scan;
            break;
         }
         if (scan->ClassID IS ID_VECTORSCENE) break;
         else if (scan->ClassID IS ID_VECTOR) scan = ((objVector *)scan)->Parent;
      }
   }

   if (Vector->Head.SubID IS ID_VECTORVIEWPORT) {
      auto view = (objVectorViewport *)Vector;

      DOUBLE parent_width, parent_height;
      OBJECTID parent_id;

      // vpTargetX/Y are the display position of the viewport, relative to the container that it is inside.
      // vpBX1/BY1/BX2/BY2 are fixed coordinate bounding box values from root position (0,0) and define the clip region imposed on all children of the viewport.
      // vpFixedX/Y are the fixed coordinate position of the viewport relative to root position (0,0)

      if (!(view->vpDimensions & (DMF_FIXED_X|DMF_RELATIVE_X|DMF_FIXED_X_OFFSET|DMF_RELATIVE_X_OFFSET))) {
         view->vpTargetX = 0;
         view->vpDimensions |= DMF_FIXED_X;
      }

      if (!(view->vpDimensions & (DMF_FIXED_Y|DMF_RELATIVE_Y|DMF_FIXED_Y_OFFSET|DMF_RELATIVE_Y_OFFSET))) {
         view->vpTargetY = 0;
         view->vpDimensions |= DMF_FIXED_Y;
      }

      if (Vector->ParentView) {
         if (Vector->ParentView->vpViewWidth) parent_width = Vector->ParentView->vpViewWidth;
         else parent_width = Vector->ParentView->vpFixedWidth;

         if (Vector->ParentView->vpViewHeight) parent_height = Vector->ParentView->vpViewHeight;
         else parent_height = Vector->ParentView->vpFixedHeight;

         parent_id = Vector->ParentView->Head.UniqueID;

         // The user's values for destination (x,y) need to be taken into account. <svg x="" y=""/>

         if (view->vpDimensions & DMF_RELATIVE_X) view->vpFixedRelX = (parent_width * view->vpTargetX);
         else view->vpFixedRelX = view->vpTargetX;

         if (view->vpDimensions & DMF_RELATIVE_Y) view->vpFixedRelY = (parent_height * view->vpTargetY);
         else view->vpFixedRelY = view->vpTargetY;
      }
      else {
         parent_width  = Vector->Scene->PageWidth;
         parent_height = Vector->Scene->PageHeight;
         parent_id     = Vector->Scene->Head.UniqueID;
         // SVG requirement: top level viewport always located at (0,0)
         view->vpFixedRelX = 0;
         view->vpFixedRelY = 0;
      }

      if (view->vpDimensions & DMF_RELATIVE_WIDTH) view->vpFixedWidth = parent_width * view->vpTargetWidth;
      else if (view->vpDimensions & DMF_FIXED_WIDTH) view->vpFixedWidth = view->vpTargetWidth;
      else view->vpFixedWidth = parent_width;

      if (view->vpDimensions & DMF_RELATIVE_HEIGHT) view->vpFixedHeight = parent_height * view->vpTargetHeight;
      else if (view->vpDimensions & DMF_FIXED_HEIGHT) view->vpFixedHeight = view->vpTargetHeight;
      else view->vpFixedHeight = parent_height;

      if (view->vpDimensions & DMF_RELATIVE_X_OFFSET) {
         if (view->vpDimensions & (DMF_FIXED_X|DMF_RELATIVE_X)) {
            view->vpFixedWidth = parent_width - (parent_width * view->vpTargetXO) - view->vpFixedRelX;
         }
         else view->vpFixedRelX = parent_width - view->vpFixedWidth - (parent_width * view->vpTargetXO);
      }
      else if (view->vpDimensions & DMF_FIXED_X_OFFSET) {
         if (view->vpDimensions & (DMF_FIXED_X|DMF_RELATIVE_X)) {
            view->vpFixedWidth = parent_width - view->vpTargetXO - view->vpFixedRelX;
         }
         else view->vpFixedRelX = parent_width - view->vpFixedWidth - view->vpTargetXO;
      }

      if (view->vpDimensions & DMF_RELATIVE_Y_OFFSET) {
         if (view->vpDimensions & (DMF_FIXED_Y|DMF_RELATIVE_Y)) {
            view->vpFixedHeight = parent_height - (parent_height * view->vpTargetYO) - view->vpFixedRelY;
         }
         else view->vpFixedRelY = parent_height - view->vpFixedHeight - (parent_height * view->vpTargetYO);
      }
      else if (view->vpDimensions & DMF_FIXED_Y_OFFSET) {
         if (view->vpDimensions & (DMF_FIXED_Y|DMF_RELATIVE_Y)) {
            view->vpFixedHeight = parent_height - view->vpTargetYO - view->vpFixedRelY;
         }
         else view->vpFixedRelY = parent_height - view->vpFixedHeight - view->vpTargetYO;
      }

      // Contained vectors are normally scaled to the area defined by the viewport.

      DOUBLE target_width  = view->vpFixedWidth;
      DOUBLE target_height = view->vpFixedHeight;

      // The client can force the top-level viewport to be resized by using VPF_RESIZE and defining PageWidth/PageHeight

      if ((!Vector->ParentView) and (Vector->Scene->Flags & VPF_RESIZE)) {
         log.trace("VPF_RESIZE enabled, using target size (%.2f %.2f)", parent_width, parent_height);
         target_width  = parent_width;
         target_height = parent_height;
         view->vpFixedWidth  = parent_width;
         view->vpFixedHeight = parent_height;
      }

      log.trace("Vector: #%d, Dimensions: $%.8x, Parent: #%d %.2fw %.2fh, Target: %.2fw %.2fh, Viewbox: %.2f %.2f %.2f %.2f", Vector->Head.UniqueID, view->vpDimensions, parent_id, parent_width, parent_height, target_width, target_height, view->vpViewX, view->vpViewY, view->vpViewWidth, view->vpViewHeight);

      // This part computes the alignment of the viewbox (source) within the viewport's target area.
      // AspectRatio choices affect this, e.g. "xMinYMin slice".  Note that alignment specifically impacts
      // the position of paths within the viewport and not the position of the viewport itself.

      calc_alignment("align_viewport",view->vpAspectRatio, target_width, target_height,
         view->vpViewWidth, view->vpViewHeight, &view->vpAlignX, &view->vpAlignY,
         &view->vpXScale, &view->vpYScale);

      // FinalX/Y values have no current use with respect to viewports.

      Vector->FinalX = 0;
      Vector->FinalY = 0;

      log.trace("AlignXY: %.2f %.2f, ScaleXY: %.2f %.2f", view->vpAlignX, view->vpAlignY, view->vpXScale, view->vpYScale);

      // Compute the clipping boundary of the viewport and store it in the BX/Y fields.

      agg::trans_affine transform;

      WORD applied_transforms = 0;
      apply_parent_transforms(Vector, Vector, transform, &applied_transforms);

      if (!Vector->BasePath) {
         Vector->BasePath = new (std::nothrow) agg::path_storage;
         if (!Vector->BasePath) return;
      }
      else Vector->BasePath->free_all();

      DOUBLE x = view->vpFixedRelX;
      DOUBLE y = view->vpFixedRelY;
      Vector->BasePath->move_to(x, y); // Top left
      Vector->BasePath->line_to(x+view->vpFixedWidth, y); // Top right
      Vector->BasePath->line_to(x+view->vpFixedWidth, y+view->vpFixedHeight); // Bottom right
      Vector->BasePath->line_to(x, y+view->vpFixedHeight); // Bottom left
      Vector->BasePath->close_polygon();

      Vector->BasePath->transform(transform);

      bounding_rect_single(*Vector->BasePath, 0, &view->vpBX1, &view->vpBY1, &view->vpBX2, &view->vpBY2);

      // If the viewport is transformed, a clipping mask will need to be generated based on its path.  The path is
      // pre-transformed and drawn in order to speed things up.

      if (applied_transforms & (VTF_MATRIX|VTF_ROTATE|VTF_SKEW)) {
         log.trace("A clip path will be assigned to viewport #%d.", Vector->Head.UniqueID);
         if (!view->vpClipMask) {
            CreateObject(ID_VECTORCLIP, NF_INTEGRAL, &view->vpClipMask,
               FID_Owner|TLONG, Vector->Head.UniqueID,
               TAGEND);
         }
         if (view->vpClipMask) {
            delete view->vpClipMask->ClipPath;
            view->vpClipMask->ClipPath = new (std::nothrow) agg::path_storage(*Vector->BasePath);
            acDraw(view->vpClipMask);
         }
      }
      else if (view->vpClipMask) { acFree(view->vpClipMask); view->vpClipMask = NULL; }

      log.trace("Clipping boundary for #%d is %.2f %.2f %.2f %.2f (transforms: %d)", Vector->Head.UniqueID, view->vpBX1, view->vpBY1, view->vpBX2, view->vpBY2, applied_transforms);

      Vector->Dirty &= ~(RC_TRANSFORM | RC_FINAL_PATH | RC_BASE_PATH);

      Vector->Scene->PendingResizeMsgs->insert({ Vector->Head.UniqueID, {
          view->vpFixedRelX, view->vpFixedRelY, 0,
          view->vpFixedWidth, view->vpFixedHeight, 0 }
      });
   }
   else if (Vector->Head.ClassID IS ID_VECTOR) {
      if ((Vector->Dirty & RC_TRANSFORM) AND (Vector->Head.SubID != ID_VECTORTEXT)) {
         // First, calculate the FinalX and FinalY field values, without any viewport scaling applied.  Note that
         // VectorText is excluded at this stage because the final X/Y needs to take alignment of the base path into
         // account.

         switch (Vector->Head.SubID) {
            case ID_VECTORELLIPSE:   get_ellipse_xy((rkVectorEllipse *)Vector); break;
            case ID_VECTORRECTANGLE: get_rectangle_xy((rkVectorRectangle *)Vector); break;
            case ID_VECTORSPIRAL:    get_spiral_xy((rkVectorSpiral *)Vector); break;
            case ID_VECTORSHAPE:     get_super_xy((rkVectorShape *)Vector); break;
            case ID_VECTORWAVE:      get_wave_xy((rkVectorWave *)Vector); break;
         }

         if (!Vector->Transform) Vector->Transform = new (std::nothrow) agg::trans_affine;
         else Vector->Transform->reset();

         if (Vector->Transform) apply_parent_transforms(Vector, Vector, *Vector->Transform, NULL);

         Vector->Dirty = (Vector->Dirty & (~RC_TRANSFORM)) | RC_FINAL_PATH;
      }

      // Generate base path of the vector if it hasn't been done already or has been reset.
      // NB: The base path is computed after the transform because it can be helpful to know the
      // final scale of the vector, particularly for calculating curved paths.

      if (!Vector->BasePath) {
         Vector->BasePath = new (std::nothrow) agg::path_storage;
         if (!Vector->BasePath) return;
         Vector->Dirty |= RC_BASE_PATH; // Since BasePath is brand new, ensure that the path is generated.
      }

      if (Vector->Dirty & RC_BASE_PATH) {
         Vector->BasePath->free_all();

         Vector->GeneratePath((objVector *)Vector);

         if ((Vector->Morph) and (Vector->Morph->Head.ClassID IS ID_VECTOR)) {
            if ((Vector->Head.SubID IS ID_VECTORTEXT) and (!(Vector->MorphFlags & VMF_STRETCH))) {
               // Do nothing for VectorText because it applies morph and transition effects during base path generation.
            }
            else {
               auto morph = (objVector *)Vector->Morph;

               if (morph->Dirty) { // Regenerate the target path if necessary
                  gen_vector_path((objVector *)morph);
                  morph->Dirty = 0;
               }

               if (morph->BasePath) {
                  DOUBLE bx1, bx2, by1, by2;

                  if (Vector->MorphFlags & VMF_Y_MID) {
                     bounding_rect_single(*Vector->BasePath, 0, &bx1, &by1, &bx2, &by2);
                     Vector->BasePath->translate(0, -by1 - ((by2 - by1) * 0.5));
                  }
                  else if (Vector->MorphFlags & VMF_Y_MIN) {
                     if (Vector->Head.SubID != ID_VECTORTEXT) {
                        bounding_rect_single(*Vector->BasePath, 0, &bx1, &by1, &bx2, &by2);
                        Vector->BasePath->translate(0, -by1 -(by2 - by1));
                     }
                  }
                  else { // VMF_Y_MAX
                     if (Vector->Head.SubID IS ID_VECTORTEXT) { // Only VectorText needs to be reset for yMax
                        bounding_rect_single(*Vector->BasePath, 0, &bx1, &by1, &bx2, &by2);
                        Vector->BasePath->translate(0, -by1);
                     }
                  }

                  agg::trans_single_path trans_path;
                  trans_path.add_path(*morph->BasePath);
                  trans_path.preserve_x_scale(true); // The default is true.  Switching to false produces a lot of scrunching and extending
                  if (morph->Head.SubID IS ID_VECTORPATH) { // Enforcing a fixed length along the path effectively causes a resize.
                     if (((objVectorPath *)morph)->PathLength > 0) trans_path.base_length(((objVectorPath *)morph)->PathLength);
                  }

                  Vector->BasePath->transform(trans_path); // Apply manipulation to the base path.
               }
            }
         }

         Vector->Dirty = (Vector->Dirty & (~RC_BASE_PATH)) | RC_FINAL_PATH;
      }

      // VectorText transform support is handled after base-path generation.  This is because vector text can be
      // aligned, for which the width and height of the base-path must be known.

      if ((Vector->Dirty & RC_TRANSFORM) AND (Vector->Head.SubID IS ID_VECTORTEXT)) {
         get_text_xy((rkVectorText *)Vector); // Sets FinalX/Y

         if (!Vector->Transform) Vector->Transform = new (std::nothrow) agg::trans_affine;
         else Vector->Transform->reset();

         if (Vector->Transform) apply_parent_transforms(Vector, Vector, *Vector->Transform, NULL);

         Vector->Dirty = (Vector->Dirty & (~RC_TRANSFORM)) | RC_FINAL_PATH;
      }

      if (Vector->Transform) {
         DOUBLE scale = Vector->Transform->scale();
         Vector->BasePath->approximation_scale(scale);
         if (scale > 1.0) Vector->BasePath->angle_tolerance(0.2); // Set in radians.  The less this value is, the more accurate it will be at sharp turns.
         else Vector->BasePath->angle_tolerance(0);
      }

      //Vector->BasePath->cusp_limit(x); // Set in radians.  If more than 0, it restricts sharpness at the cusp (presumably for awkward angles).  Do not exceed 10-15 degrees

      if ((Vector->FillColour.Alpha > 0) or (Vector->FillGradient) or (Vector->FillImage) or (Vector->FillPattern)) {
         if (!Vector->FillRaster) {
            Vector->FillRaster = new (std::nothrow) agg::rasterizer_scanline_aa<>;
            if (!Vector->FillRaster) return;
         }
         else Vector->FillRaster->reset();

         agg::conv_transform<agg::path_storage, agg::trans_affine> fill_path(*Vector->BasePath, *Vector->Transform);
         Vector->FillRaster->add_path(fill_path);
      }
      else if (Vector->FillRaster) {
         delete Vector->FillRaster;
         Vector->FillRaster = NULL;
      }

      if ((Vector->StrokeWidth > 0) and
          ((Vector->StrokePattern) or (Vector->StrokeGradient) or (Vector->StrokeImage) or
           (Vector->StrokeColour.Alpha * Vector->StrokeOpacity * Vector->Opacity > 0.001))){

         // Configure the curve algorithm so that it generates nicer looking curves when the vector is scaled up.  This
         // is not required if the vector scale is <= 1.0 (the angle_tolerance controls this).

         if (!Vector->StrokeRaster) {
            Vector->StrokeRaster = new (std::nothrow) agg::rasterizer_scanline_aa<>;
            if (!Vector->StrokeRaster) return;
         }
         else Vector->StrokeRaster->reset();

         if (Vector->DashArray) {
            agg::conv_dash<agg::path_storage> dashed_path(*Vector->BasePath);
            agg::conv_stroke<agg::conv_dash<agg::path_storage>> dashed_stroke(dashed_path);

            dashed_path.remove_all_dashes();
            DOUBLE total_length = 0;
            for (LONG i=0; i < Vector->DashTotal-1; i+=2) {
              dashed_path.add_dash(Vector->DashArray[i], Vector->DashArray[i+1]);
               total_length += Vector->DashArray[i] + Vector->DashArray[i+1];
            }

            // The stroke-dashoffset is used to set how far into dash pattern to start the pattern.  E.g. a
            // value of 5 means that the entire pattern is shifted 5 pixels to the left.

            if (Vector->DashOffset > 0) dashed_path.dash_start(Vector->DashOffset);
            else if (Vector->DashOffset < 0) dashed_path.dash_start(total_length + Vector->DashOffset);

            configure_stroke((objVector &)*Vector, dashed_stroke);

            agg::conv_transform<agg::conv_stroke<agg::conv_dash<agg::path_storage>>, agg::trans_affine> stroke_path(dashed_stroke, *Vector->Transform);
            Vector->StrokeRaster->add_path(stroke_path);
         }
         else {
            agg::conv_stroke<agg::path_storage> stroked_path(*Vector->BasePath);
            configure_stroke((objVector &)*Vector, stroked_path);
            agg::conv_transform<agg::conv_stroke<agg::path_storage>, agg::trans_affine> stroke_path(stroked_path, *Vector->Transform);
            Vector->StrokeRaster->add_path(stroke_path);
         }
      }
      else if (Vector->StrokeRaster) {
         delete Vector->StrokeRaster;
         Vector->StrokeRaster = NULL;
      }

      Vector->Dirty &= ~RC_FINAL_PATH;
   }
   else log.warning("Target vector is not a shape.");

   send_feedback(Vector, FM_PATH_CHANGED);
}

//****************************************************************************
// Apply all transforms in the correct SVG order to a target agg::trans_affine object.  The process starts with the
// vector passed in to the function, and proceeds upwards through the parent nodes.

static void apply_parent_transforms(objVector *Self, objVector *Start, agg::trans_affine &transform, WORD *Applied)
{
   parasol::Log log(__FUNCTION__);

   for (auto scan=Start; scan; scan=(objVector *)get_parent(scan)) {
      if (scan->Head.ClassID != ID_VECTOR) continue;

      if (scan->Head.SubID IS ID_VECTORVIEWPORT) {
         auto view = (objVectorViewport *)scan;

         DBG_TRANSFORM("Parent view #%d x/y: %.2f %.2f", scan->Head.UniqueID, view->vpFixedRelX, view->vpFixedRelY);

         if (Self IS scan) {
            // Do not apply align values.  Alignment applies to content of the viewport, not the viewport itself.
         }
         else {
            if ((view->vpViewX) or (view->vpViewY)) {
               transform.translate(-view->vpViewX, -view->vpViewY);
               if (Applied) *Applied |= VTF_TRANSLATE;
            }

            if ((view->vpXScale != 1.0) or (view->vpYScale != 1.0)) {
               DBG_TRANSFORM("Viewport scales this vector to %.2f %.2f", view->vpXScale, view->vpYScale);
               transform.scale(view->vpXScale, view->vpYScale);
               if (Applied) *Applied |= VTF_SCALE;
            }

            // Children of viewports are affected by the VP's alignment values.

            apply_transforms(scan->Transforms, view->vpFixedRelX + view->vpAlignX,
               view->vpFixedRelY + view->vpAlignY, transform, Applied);
         }
      }
      else {
         log.trace("Parent vector #%d x/y: %.2f %.2f", scan->Head.UniqueID, scan->FinalX, scan->FinalY);
         apply_transforms(scan->Transforms, scan->FinalX, scan->FinalY, transform, Applied);
      }
   }
}

//****************************************************************************
// Applies a vector's transformations to a trans_affine object, using the order in which they are listed.

static void apply_transforms(VectorTransform *t, DOUBLE X, DOUBLE Y, agg::trans_affine &Transform, WORD *Applied = NULL)
{
   parasol::Log log(__FUNCTION__);

   if ((X) or (Y)) {
      DBG_TRANSFORM("Initial translate to vector position %.2f %.2f", X, Y);
      Transform.translate(X, Y);
   }

   WORD type = 0;

   while (t) {
      switch (t->Type) {
         case VTF_MATRIX:
            DBG_TRANSFORM("Matrix: %.2f %.2f %.2f %.2f %.2f %.2f", t->Matrix[0], t->Matrix[1], t->Matrix[2], t->Matrix[3], t->Matrix[4], t->Matrix[5]);
            Transform.multiply(agg::trans_affine(t->Matrix[0], t->Matrix[1], t->Matrix[2], t->Matrix[3], t->Matrix[4], t->Matrix[5]));
            // Determine the transform type by analysing the matrix numbers.
            if ((t->Matrix[1] IS 0.0) and (t->Matrix[2] IS 0.0)) {
               type |= VTF_TRANSLATE;
               if ((t->Matrix[0] != 1.0) OR (t->Matrix[3] != 1.0)) type |= VTF_SCALE;
            }
            else type |= VTF_MATRIX|VTF_ROTATE;
            break;

         case VTF_SCALE:
            DBG_TRANSFORM("Scale: %.2f %.2f", t->X, t->Y);
            if (t->Y == 0.0) {
               Transform.scale(t->X);
            }
            else Transform.scale(t->X, t->Y);
            type |= VTF_SCALE;
            break;

         case VTF_ROTATE:
            DBG_TRANSFORM("Rotate: %.2f %.2f %.2f", t->X, t->Y, t->Angle);
            if ((t->X) or (t->Y)) {
               Transform.translate(-t->X, -t->Y);
               Transform.rotate(t->Angle * DEG2RAD);
               Transform.translate(t->X, t->Y);
            }
            else Transform.rotate(t->Angle * DEG2RAD);
            type |= VTF_ROTATE;
            break;

         case  VTF_SKEW:
            DBG_TRANSFORM("Skew: %.2f %.2f", t->X, t->Y);
            if (t->X) Transform *= agg::trans_affine_skewing(t->X * DEG2RAD, 0.0);
            if (t->Y) Transform *= agg::trans_affine_skewing(0.0, t->Y * DEG2RAD);
            type |= VTF_SKEW;
            break;

         case VTF_TRANSLATE:
            DBG_TRANSFORM("Translate: %.2f %.2f", t->X, t->Y);
            Transform.translate(t->X, t->Y);
            type |= VTF_TRANSLATE;
            break;

         default: log.warning("Transform instruction $%.8x not recognised.", t->Type);
      }

      t = t->Next;
   }

   if (Applied) *Applied |= type;
}

/*****************************************************************************
** Creates a new transformation instruction and returns it for setting additional field values.  If Create is FALSE
** then an existing transform will be returned if there is a matching type available.
**
** Every new transform is inserted at the start of the linked-list so that it receives priority.  This reflects SVG's
** ordering of transformations.
*/

static VectorTransform * add_transform(objVector *Self, LONG Type, LONG Create = TRUE)
{
   parasol::Log log(__FUNCTION__);

   DBG_TRANSFORM("Type: $%.8x, Create: %d", Type, Create);

   if ((!Create) and (Self->ActiveTransforms & Type)) { // Attempt to use an existing transform with matching type.
      for (auto transform=Self->Transforms; transform; transform=transform->Next) {
         if (transform->Type IS Type) {
            mark_dirty(Self, RC_TRANSFORM); // Transforms affect the final path of the vector.
            return transform;
         }
      }
   }

   VectorTransform *transform;
   if (!AllocMemory(sizeof(VectorTransform), MEM_DATA, &transform, NULL)) {
      // Insert transform at the start of the list.

      transform->Prev = NULL;
      transform->Next = Self->Transforms;
      if (Self->Transforms) Self->Transforms->Prev = transform;
      Self->Transforms = transform;

      transform->Type = Type;
      Self->ActiveTransforms |= Type;
      mark_dirty(Self, RC_TRANSFORM); // Transforms affect the final path of the vector.
      return transform;
   }

   return NULL;
}
