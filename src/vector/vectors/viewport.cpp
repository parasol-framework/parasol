/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
VectorViewport: Provides support for viewport definitions within a vector tree.

This class is used to declare a viewport within a vector scene graph.  A master viewport is required as the first object
in a @VectorScene and it must contain all vector graphics content.

The size of the viewport is initially set to (0,0,100%,100%) so as to be all inclusive.  Setting the #X, #Y, #Width and
#Height fields will determine the position and clipping of the displayed content (the 'target area').  The #ViewX, #ViewY,
#ViewWidth and #ViewHeight fields declare the viewbox ('source area') that will be sampled for the target.

To configure the scaling and alignment method that is applied to the viewport content, set the #AspectRatio field.

-END-

NOTE: Refer to gen_vector_path() for the code that manages viewport dimensions in a live state.

*********************************************************************************************************************/

static void observe_limits(objVectorViewport *Viewport)
{
   objVectorViewport *target;

   if (!Viewport->vpLimits) return;

   auto &limits = *Viewport->vpLimits;
   if (Viewport->DragViewport) target = Viewport->DragViewport;
   else target = Viewport;

   auto parent = (objVectorViewport *)target->Parent;
   if ((parent) and (parent->Head.SubID IS ID_VECTORVIEWPORT)) {
      if (Viewport->vpRelativeLimits) {
         auto left   = limits.Left * parent->vpFixedWidth;
         auto top    = limits.Top * parent->vpFixedHeight;
         auto right  = limits.Right * parent->vpFixedWidth;
         auto bottom = limits.Bottom * parent->vpFixedHeight;

         if (target->vpTargetX + target->vpFixedWidth > parent->vpFixedWidth - right) {
            target->vpTargetX = parent->vpFixedWidth - right - target->vpFixedWidth;
         }

         if (target->vpTargetY + target->vpFixedHeight > parent->vpFixedHeight - bottom) {
            target->vpTargetY = parent->vpFixedHeight - bottom - target->vpFixedHeight;
         }

         if (target->vpTargetX < left) target->vpTargetX = left;
         if (target->vpTargetY < top)  target->vpTargetY = top;
      }
      else {
         if (target->vpTargetX + target->vpFixedWidth > parent->vpFixedWidth - limits.Right) {
            target->vpTargetX = parent->vpFixedWidth - limits.Right - target->vpFixedWidth;
         }

         if (target->vpTargetY + target->vpFixedHeight > parent->vpFixedHeight - limits.Bottom) {
            target->vpTargetY = parent->vpFixedHeight - limits.Bottom - target->vpFixedHeight;
         }

         if (target->vpTargetX < limits.Left) target->vpTargetX = limits.Left;
         if (target->vpTargetY < limits.Top)  target->vpTargetY = limits.Top;
      }
   }
}

//********************************************************************************************************************
// Input event handler for the dragging of viewports by the user.  Requires the client to set the Drag field to be
// active.

static ERROR drag_input_events(objVectorViewport *Viewport, const InputEvent *Events)
{
   static DOUBLE glAnchorX = 0, glAnchorY = 0; // Anchoring is process-exclusive, so we can store the coordinates as global variables
   static DOUBLE glDragOriginX = 0, glDragOriginY = 0;

   objVectorViewport *target;
   if (Viewport->DragViewport) target = Viewport->DragViewport;
   else target = Viewport;

   if (target->Dirty) gen_vector_tree((objVector *)target);

   for (auto event=Events; event; event=event->Next) {
      // Process events that support consolidation first.

      if (event->Flags & (JTYPE_ANCHORED|JTYPE_MOVEMENT)) {
         if (Viewport->vpDragging) {
            while ((event->Next) and (event->Next->Flags & JTYPE_MOVEMENT)) { // Consolidate movement
               event = event->Next;
            }

            target->vpTargetX = glDragOriginX + (event->AbsX - glAnchorX);
            target->vpTargetY = glDragOriginY + (event->AbsY - glAnchorY);

            observe_limits(Viewport);

            mark_dirty((objVector *)target, RC_FINAL_PATH|RC_TRANSFORM);

            acDraw(target);
         }
      }
      else if ((event->Type IS JET_LMB) and (!(event->Flags & JTYPE_REPEATED))) {
         if (event->Value > 0) {
            if (Viewport->Visibility != VIS_VISIBLE) continue;
            Viewport->vpDragging = 1;
            glAnchorX  = event->AbsX;
            glAnchorY  = event->AbsY;

            GetFields(target,
               FID_X|TDOUBLE, &glDragOriginX,
               FID_Y|TDOUBLE, &glDragOriginY,
               TAGEND);

            // Ensure that the X,Y coordinates are fixed.

            target->vpDimensions = (target->vpDimensions | DMF_FIXED_X | DMF_FIXED_Y) &
               (~(DMF_RELATIVE_X|DMF_RELATIVE_Y|DMF_RELATIVE_X_OFFSET|DMF_RELATIVE_Y_OFFSET));

         }
         else Viewport->vpDragging = 0; // Released
      }
   }

   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Clear: Free all child objects contained by the viewport.
-END-
*********************************************************************************************************************/

static ERROR VECTORVIEWPORT_Clear(objVectorViewport *Self, APTR Void)
{
   ChildEntry list[512];
   LONG count = ARRAYSIZE(list);
   do {
      if (!ListChildren(Self->Head.UniqueID, FALSE, list, &count)) {
         for (WORD i=0; i < count; i++) acFreeID(list[i].ObjectID);
      }
   } while (count IS ARRAYSIZE(list));

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR VECTORVIEWPORT_Free(objVectorViewport *Self, APTR Void)
{
   if (Self->vpClipMask) { acFree(Self->vpClipMask); Self->vpClipMask = NULL; }
   if (Self->vpLimits)   { delete Self->vpLimits; Self->vpLimits = NULL; }

   if (Self->DragViewport) {
      auto callback = make_function_stdc(drag_input_events);
      vecInputSubscription(Self, 0, &callback);
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR VECTORVIEWPORT_Init(objVectorViewport *Self, APTR Void)
{
   // Please refer to gen_vector_path() for the initialisation of vpFixedX/Y/Width/Height, which has
   // its own section for dealing with viewports.

   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Move: Move the position of the viewport by delta X, Y.

*********************************************************************************************************************/

static ERROR VECTORVIEWPORT_Move(objVectorViewport *Self, struct acMove *Args)
{
   if (!Args) return ERR_NullArgs;

   Self->vpDimensions = (Self->vpDimensions|DMF_FIXED_X|DMF_FIXED_Y) & (~(DMF_RELATIVE_X|DMF_RELATIVE_Y));
   Self->vpTargetX += Args->XChange;
   Self->vpTargetY += Args->YChange;

   observe_limits(Self);

   mark_dirty((objVector *)Self, RC_FINAL_PATH|RC_TRANSFORM);
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
MoveToPoint: Move the position of the viewport to a fixed point.
-END-
*********************************************************************************************************************/

static ERROR VECTORVIEWPORT_MoveToPoint(objVectorViewport *Self, struct acMoveToPoint *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Args->Flags & MTF_X) {
      Self->vpDimensions = (Self->vpDimensions | DMF_FIXED_X) & (~DMF_RELATIVE_X);
      Self->vpTargetX = Args->X;
   }

   if (Args->Flags & MTF_Y) {
      Self->vpDimensions = (Self->vpDimensions | DMF_FIXED_Y) & (~DMF_RELATIVE_Y);
      Self->vpTargetY = Args->Y;
   }

   observe_limits(Self);

   mark_dirty((objVector *)Self, RC_FINAL_PATH|RC_TRANSFORM);
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR VECTORVIEWPORT_NewObject(objVectorViewport *Self, APTR Void)
{
   Self->vpAspectRatio = ARF_MEET|ARF_X_MID|ARF_Y_MID;
   Self->vpOverflowX = VOF_VISIBLE;
   Self->vpOverflowY = VOF_VISIBLE;

   // NB: vpTargetWidth and vpTargetHeight are not set to a default because we need to know if the client has
   // intentionally avoided setting the viewport and/or viewbox dimensions (which typically means that the viewport
   // will expand to fit the parent).
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Resize: Resize a viewport to a fixed size.
-END-
*********************************************************************************************************************/

static ERROR VECTORVIEWPORT_Resize(objVectorViewport *Self, struct acResize *Args)
{
   if (!Args) return ERR_NullArgs;

   Self->vpDimensions = (Self->vpDimensions | DMF_FIXED_WIDTH) & (~DMF_RELATIVE_WIDTH);
   Self->vpTargetWidth = Args->Width;

   Self->vpDimensions = (Self->vpDimensions | DMF_FIXED_HEIGHT) & (~DMF_RELATIVE_HEIGHT);
   Self->vpTargetHeight = Args->Height;

   if (Self->vpTargetWidth < 1) Self->vpTargetWidth = 1;
   if (Self->vpTargetHeight < 1) Self->vpTargetHeight = 1;
   mark_dirty((objVector *)Self, RC_FINAL_PATH|RC_TRANSFORM);
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
SetLimits: Prevents a viewport from moving into the boundary of its parent.

A viewport can be prevented from moving into areas at the edge of its parent by setting limits.  Values are expressed
in fixed pixel units by default, or a relative ratio of 0 - 1.0 if Relative is TRUE.

Note that limits are imposed when moving the viewport only.  It is possible for a client to ignore limits by setting
coordinate fields directly.

-INPUT-
double Left:   Total whitespace at the left edge.
double Top:    Total whitespace at the top edge.
double Right:  Total whitespace at the right edge.
double Bottom: Total whitespace at the bottom edge.
int Relative:  Set to TRUE if the values are relative to the available width or height of their axis.

-RESULT-
Okay:
NullArgs:
AllocMemory:

*********************************************************************************************************************/

static ERROR VECTORVIEWPORT_SetLimits(objVectorViewport *Self, struct viewSetLimits *Args)
{
   if (!Args) return ERR_NullArgs;

   Self->vpLimits = new (std::nothrow) Edges(Args->Left, Args->Top, Args->Right, Args->Bottom);
   if (!Self->vpLimits) return ERR_AllocMemory;

   Self->vpRelativeLimits = Args->Relative;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
AbsX: The horizontal position of the viewport, relative to (0,0).

This field will return the left-most boundary of the viewport, relative to point (0,0) of the scene
graph.  Transforms are taken into consideration when calculating this value.

*********************************************************************************************************************/

static ERROR VIEW_GET_AbsX(objVectorViewport *Self, LONG *Value)
{
   if (Self->Dirty) gen_vector_tree((objVector *)Self);

   *Value = Self->vpBX1;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
AbsY: The vertical position of the viewport, relative to (0,0).

This field will return the top-most boundary of the viewport, relative to point (0,0) of the scene
graph.  Transforms are taken into consideration when calculating this value.

*********************************************************************************************************************/

static ERROR VIEW_GET_AbsY(objVectorViewport *Self, LONG *Value)
{
   if (Self->Dirty) gen_vector_tree((objVector *)Self);

   *Value = Self->vpBY1;
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
AspectRatio: Flags that affect the aspect ratio of vectors within the viewport.
Lookup: ARF

Defining an aspect ratio allows finer control over the position and scale of the viewport's content within its target
area.

<types lookup="ARF"/>

*********************************************************************************************************************/

static ERROR VIEW_GET_AspectRatio(objVectorViewport *Self, LONG *Value)
{
   *Value = Self->vpAspectRatio;
   return ERR_Okay;
}

static ERROR VIEW_SET_AspectRatio(objVectorViewport *Self, LONG Value)
{
   Self->vpAspectRatio = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Dimensions: Dimension flags define whether individual dimension fields contain fixed or relative values.
Lookup: DMF

<types lookup="DMF"/>

*********************************************************************************************************************/

static ERROR VIEW_GET_Dimensions(objVectorViewport *Self, LONG *Value)
{
   *Value = Self->vpDimensions;
   return ERR_Okay;
}

static ERROR VIEW_SET_Dimensions(objVectorViewport *Self, LONG Value)
{
   Self->vpDimensions = Value;
   mark_dirty((objVector *)Self, RC_ALL);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Drag: Enables click-dragging of viewports around the display.

Click-dragging of viewports is enabled by utilising the Drag field.  To use, set this field with reference to a
VectorViewport that is to be dragged when the user starts a click-drag operation.

For example, a window with a titlebar would have the titlebar's Drag field set to the window's viewport.  If
necessary, a viewport's Drag field can point back to itself (useful for creating icons and similar draggable widgets).

Set the field to zero to turn off dragging.

It is required that the parent @VectorScene is associated with a @Surface for this feature to work.

*********************************************************************************************************************/

static ERROR VIEW_GET_Drag(objVectorViewport *Self, objVectorViewport **Value)
{
   *Value = Self->DragViewport;
   return ERR_Okay;
}

static ERROR VIEW_SET_Drag(objVectorViewport *Self, objVectorViewport *Value)
{
   auto callback = make_function_stdc(drag_input_events);

   if (Value) {
      parasol::Log log;

      if (Value->Head.SubID != ID_VECTORVIEWPORT) return log.warning(ERR_WrongClass);
      if ((!Self->Scene) or (!Self->Scene->SurfaceID)) return log.warning(ERR_FieldNotSet);

      if (vecInputSubscription(Self, JTYPE_MOVEMENT|JTYPE_BUTTON, &callback)) {
         return ERR_Failed;
      }
   }
   else vecInputSubscription(Self, 0, &callback);

   Self->DragViewport = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Height: The height of the viewport's target area.

The height of the viewport's target area is defined here as a fixed or relative value.  The default value is 100% for
full coverage.

*********************************************************************************************************************/

static ERROR VIEW_GET_Height(objVectorViewport *Self, Variable *Value)
{
   DOUBLE val;

   if (Self->Dirty) gen_vector_tree((objVector *)Self);

   if (Self->vpDimensions & DMF_FIXED_HEIGHT) { // Working with a fixed dimension
      if (Value->Type & FD_PERCENTAGE) {
         if (Self->ParentView) val = Self->vpFixedHeight * Self->ParentView->vpFixedHeight * 0.01;
         else val = Self->vpFixedHeight * Self->Scene->PageHeight * 0.01;
      }
      else val = Self->vpTargetHeight;
   }
   else if (Self->vpDimensions & DMF_RELATIVE_HEIGHT) { // Working with a relative dimension
      if (Value->Type & FD_PERCENTAGE) val = Self->vpTargetHeight;
      else if (Self->ParentView) val = Self->vpTargetHeight * Self->ParentView->vpFixedHeight;
      else val = Self->vpTargetHeight * Self->Scene->PageHeight;
   }
   else if (Self->vpDimensions & DMF_FIXED_Y_OFFSET) {
      DOUBLE y, parent_height;
      if (Self->vpDimensions & DMF_RELATIVE_Y) {
         y = (DOUBLE)Self->vpTargetY * (DOUBLE)Self->ParentView->vpFixedHeight * 0.01;
      }
      else y = Self->vpTargetY;

      if (Self->ParentView) GetDouble(Self->ParentView, FID_Height, &parent_height);
      else parent_height = Self->Scene->PageHeight;

      val = parent_height - Self->vpTargetYO - y;
   }
   else if (Self->vpDimensions & DMF_RELATIVE_Y_OFFSET) {
      DOUBLE y, parent_height;
      if (Self->vpDimensions & DMF_RELATIVE_Y) {
         y = (DOUBLE)Self->vpTargetY * (DOUBLE)Self->ParentView->vpFixedHeight * 0.01;
      }
      else y = Self->vpTargetY;

      if (Self->ParentView) GetDouble(Self->ParentView, FID_Height, &parent_height);
      else parent_height = Self->Scene->PageHeight;

      val = parent_height - (Self->vpTargetYO * parent_height * 0.01) - y;
   }
   else { // If no height set by the client, the full height is inherited from the parent
      if (Self->ParentView) return GetVariable(Self->ParentView, FID_Height, Value);
      else GetDouble(Self->Scene, FID_PageHeight, &val);
   }

   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR VIEW_SET_Height(objVectorViewport *Self, Variable *Value)
{
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      Self->vpDimensions = (Self->vpDimensions | DMF_RELATIVE_HEIGHT) & (~DMF_FIXED_HEIGHT);
      Self->vpTargetHeight = val * 0.01;
   }
   else {
      Self->vpDimensions = (Self->vpDimensions | DMF_FIXED_HEIGHT) & (~DMF_RELATIVE_HEIGHT);
      Self->vpTargetHeight = val;
   }
   mark_dirty((objVector *)Self, RC_ALL);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Overflow: Clipping options for the viewport's boundary.

Choose an overflow option to enforce or disable clipping of the viewport's content.  The default state is `VISIBLE`.
Altering the overflow state affects both the X and Y axis.  To set either axis independently, set #OverflowX and
#OverflowY.

If the viewport's #AspectRatio is set to `SLICE` then it will have priority over the overflow setting.

*********************************************************************************************************************/

static ERROR VIEW_GET_Overflow(objVectorViewport *Self, LONG *Value)
{
   *Value = Self->vpOverflowX;
   return ERR_Okay;
}

static ERROR VIEW_SET_Overflow(objVectorViewport *Self, LONG Value)
{
   Self->vpOverflowX = Value;
   Self->vpOverflowY = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
OverflowX: Clipping options for the viewport's boundary on the x axis.

Choose an overflow option to enforce or disable clipping of the viewport's content.  The default state is `VISIBLE`.
If the viewport's #AspectRatio is set to `SLICE` then it will have priority over the overflow setting.

This option controls the x axis only.

*********************************************************************************************************************/

static ERROR VIEW_GET_OverflowX(objVectorViewport *Self, LONG *Value)
{
   *Value = Self->vpOverflowX;
   return ERR_Okay;
}

static ERROR VIEW_SET_OverflowX(objVectorViewport *Self, LONG Value)
{
   Self->vpOverflowX = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
OverflowY: Clipping options for the viewport's boundary on the y axis.

Choose an overflow option to enforce or disable clipping of the viewport's content.  The default state is `VISIBLE`.
If the viewport's #AspectRatio is set to `SLICE` then it will have priority over the overflow setting.

This option controls the y axis only.

*********************************************************************************************************************/

static ERROR VIEW_GET_OverflowY(objVectorViewport *Self, LONG *Value)
{
   *Value = Self->vpOverflowY;
   return ERR_Okay;
}

static ERROR VIEW_SET_OverflowY(objVectorViewport *Self, LONG Value)
{
   Self->vpOverflowY = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
ViewHeight: The height of the viewport's source area.

The area defined by (#ViewX,#ViewY) and (#ViewWidth,#ViewHeight) declare the source area covered by the viewport.  The
rendered graphics in the source area will be repositioned and scaled to the area defined by (X,Y) and (Width,Height).

*********************************************************************************************************************/

static ERROR VIEW_GET_ViewHeight(objVectorViewport *Self, DOUBLE *Value)
{
   *Value = Self->vpViewHeight;
   return ERR_Okay;
}

static ERROR VIEW_SET_ViewHeight(objVectorViewport *Self, DOUBLE Value)
{
   if (Value > 0.0) {
      Self->vpViewHeight = Value;
      mark_dirty((objVector *)Self, RC_ALL);
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
}

/*********************************************************************************************************************
-FIELD-
ViewX: The horizontal position of the viewport's source area.

The area defined by (#ViewX,#ViewY) and (#ViewWidth,#ViewHeight) declare the source area covered by the viewport.  The
rendered graphics in the source area will be repositioned and scaled to the area defined by (#X,#Y) and
(#Width,#Height).

*********************************************************************************************************************/

static ERROR VIEW_GET_ViewX(objVectorViewport *Self, DOUBLE *Value)
{
   *Value = Self->vpViewX;
   return ERR_Okay;
}

static ERROR VIEW_SET_ViewX(objVectorViewport *Self, DOUBLE Value)
{
   Self->vpViewX = Value;
   mark_dirty((objVector *)Self, RC_ALL);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
ViewWidth: The width of the viewport's source area.

The area defined by (#ViewX,#ViewY) and (#ViewWidth,#ViewHeight) declare the source area covered by the viewport.  The
rendered graphics in the source area will be repositioned and scaled to the area defined by (#X,#Y) and
(#Width,#Height).

*********************************************************************************************************************/

static ERROR VIEW_GET_ViewWidth(objVectorViewport *Self, DOUBLE *Value)
{
   *Value = Self->vpViewWidth;
   return ERR_Okay;
}

static ERROR VIEW_SET_ViewWidth(objVectorViewport *Self, DOUBLE Value)
{
   if (Value > 0.0) {
      Self->vpViewWidth = Value;
      mark_dirty((objVector *)Self, RC_ALL);
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
}

/*********************************************************************************************************************
-FIELD-
ViewY: The vertical position of the viewport's source area.

The area defined by (#ViewX,#ViewY) and (#ViewWidth,#ViewHeight) declare the source area covered by the viewport.  The
rendered graphics in the source area will be repositioned and scaled to the area defined by (#X,#Y) and
(#Width,#Height).

*********************************************************************************************************************/

static ERROR VIEW_GET_ViewY(objVectorViewport *Self, DOUBLE *Value)
{
   *Value = Self->vpViewY;
   return ERR_Okay;
}

static ERROR VIEW_SET_ViewY(objVectorViewport *Self, DOUBLE Value)
{
   Self->vpViewY = Value;
   mark_dirty((objVector *)Self, RC_ALL);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Width: The width of the viewport's target area.

The width of the viewport's target area is defined here as a fixed or relative value.  The default value is 100% for
full coverage.

*********************************************************************************************************************/

static ERROR VIEW_GET_Width(objVectorViewport *Self, Variable *Value)
{
   DOUBLE val;

   if (Self->Dirty) gen_vector_tree((objVector *)Self);

   if (Self->vpDimensions & DMF_FIXED_WIDTH) { // Working with a fixed dimension
      if (Value->Type & FD_PERCENTAGE) {
         if (Self->ParentView) val = Self->vpFixedWidth * Self->ParentView->vpFixedWidth * 0.01;
         else val = Self->vpFixedWidth * Self->Scene->PageWidth * 0.01;
      }
      else val = Self->vpTargetWidth;
   }
   else if (Self->vpDimensions & DMF_RELATIVE_WIDTH) { // Working with a relative dimension
      if (Value->Type & FD_PERCENTAGE) val = Self->vpTargetWidth;
      else if (Self->ParentView) val = Self->vpTargetWidth * Self->ParentView->vpFixedWidth;
      else val = Self->vpTargetWidth * Self->Scene->PageWidth;
   }
   else if (Self->vpDimensions & DMF_FIXED_X_OFFSET) {
      DOUBLE x, parent_width;
      if (Self->vpDimensions & DMF_RELATIVE_X) {
         x = (DOUBLE)Self->vpTargetX * (DOUBLE)Self->ParentView->vpFixedWidth * 0.01;
      }
      else x = Self->vpTargetX;

      if (Self->ParentView) GetDouble(Self->ParentView, FID_Width, &parent_width);
      else parent_width = Self->Scene->PageWidth;

      val = parent_width - Self->vpTargetXO - x;
   }
   else if (Self->vpDimensions & DMF_RELATIVE_X_OFFSET) {
      DOUBLE x, parent_width;
      if (Self->vpDimensions & DMF_RELATIVE_X) {
         x = (DOUBLE)Self->vpTargetX * (DOUBLE)Self->ParentView->vpFixedWidth * 0.01;
      }
      else x = Self->vpTargetX;

      if (Self->ParentView) GetDouble(Self->ParentView, FID_Width, &parent_width);
      else parent_width = Self->Scene->PageWidth;

      val = parent_width - (Self->vpTargetXO * parent_width * 0.01) - x;
   }
   else { // If no width set by the client, the full width is inherited from the parent
      if (Self->ParentView) return GetVariable(Self->ParentView, FID_Width, Value);
      else GetDouble(Self->Scene, FID_PageWidth, &val);
   }

   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR VIEW_SET_Width(objVectorViewport *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      Self->vpDimensions = (Self->vpDimensions | DMF_RELATIVE_WIDTH) & (~DMF_FIXED_WIDTH);
      Self->vpTargetWidth = val * 0.01;
   }
   else {
      Self->vpDimensions = (Self->vpDimensions | DMF_FIXED_WIDTH) & (~DMF_RELATIVE_WIDTH);
      Self->vpTargetWidth = val;
   }
   mark_dirty((objVector *)Self, RC_ALL);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
X: Positions the viewport on the x-axis.

The display position targeted by the viewport is declared by the (X,Y) field values.  Coordinates can be expressed as
fixed or relative pixel units.

If an offset from the edge of the parent is desired, the #XOffset field must be defined.  If a X and XOffset value
are defined together, the width of the viewport is computed on-the-fly and will change in response to the parent's
width.

*********************************************************************************************************************/

static ERROR VIEW_GET_X(objVectorViewport *Self, Variable *Value)
{
   DOUBLE width, value;

   if (Self->Dirty) gen_vector_tree((objVector *)Self);

   if (Self->vpDimensions & DMF_FIXED_X) value = Self->vpTargetX;
   else if (Self->vpDimensions & DMF_RELATIVE_X) {
      value = (DOUBLE)Self->vpTargetX * (DOUBLE)Self->ParentView->vpFixedWidth * 0.01;
   }
   else if ((Self->vpDimensions & DMF_WIDTH) and (Self->vpDimensions & DMF_X_OFFSET)) {
      if (Self->vpDimensions & DMF_FIXED_WIDTH) width = Self->vpTargetWidth;
      else width = (DOUBLE)Self->ParentView->vpFixedWidth * (DOUBLE)Self->vpTargetWidth * 0.01;
      if (Self->vpDimensions & DMF_FIXED_X_OFFSET) value = Self->ParentView->vpFixedWidth - width - Self->vpTargetXO;
      else value = (DOUBLE)Self->ParentView->vpFixedWidth - (DOUBLE)width - ((DOUBLE)Self->ParentView->vpFixedWidth * (DOUBLE)Self->vpTargetXO * 0.01);
   }
   else value = 0;

   if (Value->Type & FD_PERCENTAGE) value = ((DOUBLE)value * 100.0) / (DOUBLE)Self->ParentView->vpFixedWidth;

   if (Value->Type & FD_DOUBLE) Value->Double = value;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(value);
   else {
      parasol::Log log;
      return log.warning(ERR_FieldTypeMismatch);
   }

   return ERR_Okay;
}

static ERROR VIEW_SET_X(objVectorViewport *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      Self->vpDimensions = (Self->vpDimensions | DMF_RELATIVE_X) & (~DMF_FIXED_X);
      Self->vpTargetX = val * 0.01;
   }
   else {
      Self->vpDimensions = (Self->vpDimensions | DMF_FIXED_X) & (~DMF_RELATIVE_X);
      Self->vpTargetX = val;
   }
   mark_dirty((objVector *)Self, RC_ALL);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
XOffset: Positions the viewport on the x-axis.

The display position targeted by the viewport is declared by the (X,Y) field values.  Coordinates can be expressed as
fixed or relative pixel units.

If an offset from the edge of the parent is desired, the #XOffset field must be defined.  If a X and XOffset value
are defined together, the width of the viewport is computed on-the-fly and will change in response to the parent's
width.

*********************************************************************************************************************/

static ERROR VIEW_GET_XOffset(objVectorViewport *Self, Variable *Value)
{
   DOUBLE width;
   DOUBLE value = 0;

   if (Self->Dirty) gen_vector_tree((objVector *)Self);

   if (Self->vpDimensions & DMF_FIXED_X_OFFSET) value = Self->vpTargetXO;
   else if (Self->vpDimensions & DMF_RELATIVE_X_OFFSET) {
      value = (DOUBLE)Self->vpTargetXO * (DOUBLE)Self->ParentView->vpFixedWidth * 0.01;
   }
   else if ((Self->vpDimensions & DMF_X) and (Self->vpDimensions & DMF_WIDTH)) {
      if (Self->vpDimensions & DMF_FIXED_WIDTH) width = Self->vpTargetWidth;
      else width = (DOUBLE)Self->ParentView->vpFixedWidth * (DOUBLE)Self->vpTargetWidth * 0.01;

      if (Self->vpDimensions & DMF_FIXED_X) value = Self->ParentView->vpFixedWidth - (Self->vpTargetX + width);
      else value = (DOUBLE)Self->ParentView->vpFixedWidth - (((DOUBLE)Self->vpTargetX * (DOUBLE)Self->ParentView->vpFixedWidth * 0.01) + (DOUBLE)width);
   }
   else value = 0;

   if (Value->Type & FD_PERCENTAGE) value = ((DOUBLE)value * 100.0) / (DOUBLE)Self->ParentView->vpFixedWidth;

   if (Value->Type & FD_DOUBLE) Value->Double = value;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(value);
   else {
      parasol::Log log;
      return log.warning(ERR_FieldTypeMismatch);
   }

   return ERR_Okay;
}

static ERROR VIEW_SET_XOffset(objVectorViewport *Self, Variable *Value)
{
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      Self->vpDimensions = (Self->vpDimensions | DMF_RELATIVE_X_OFFSET) & (~DMF_FIXED_X_OFFSET);
      Self->vpTargetXO = val * 0.01;
   }
   else {
      Self->vpDimensions = (Self->vpDimensions | DMF_FIXED_X_OFFSET) & (~DMF_RELATIVE_X_OFFSET);
      Self->vpTargetXO = val;
   }
   mark_dirty((objVector *)Self, RC_ALL);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Y: Positions the viewport on the y-axis.

The display position targeted by the viewport is declared by the (X,Y) field values.  Coordinates can be expressed as
fixed or relative pixel units.

If an offset from the edge of the parent is desired, the #YOffset must be defined.  If a Y and YOffset value are
defined together, the height of the viewport is computed on-the-fly and will change in response to the parent's
height.

*********************************************************************************************************************/

static ERROR VIEW_GET_Y(objVectorViewport *Self, Variable *Value)
{
   DOUBLE value, height;

   if (Self->Dirty) gen_vector_tree((objVector *)Self);

   if (Self->vpDimensions & DMF_FIXED_Y) value = Self->vpTargetY;
   else if (Self->vpDimensions & DMF_RELATIVE_Y) {
      value = (DOUBLE)Self->vpTargetY * (DOUBLE)Self->ParentView->vpFixedHeight * 0.01;
   }
   else if ((Self->vpDimensions & DMF_HEIGHT) and (Self->vpDimensions & DMF_Y_OFFSET)) {
      if (Self->vpDimensions & DMF_FIXED_HEIGHT) height = Self->vpTargetHeight;
      else height = (DOUBLE)Self->ParentView->vpFixedHeight * (DOUBLE)Self->vpTargetHeight * 0.01;

      if (Self->vpDimensions & DMF_FIXED_Y_OFFSET) value = Self->ParentView->vpFixedHeight - height - Self->vpTargetYO;
      else value = (DOUBLE)Self->ParentView->vpFixedHeight - height - ((DOUBLE)Self->ParentView->vpFixedHeight * (DOUBLE)Self->vpTargetYO * 0.01);
   }
   else value = 0;

   if (Value->Type & FD_PERCENTAGE) value = (value * 100.0) / (DOUBLE)Self->ParentView->vpFixedHeight;
   if (Value->Type & FD_DOUBLE) Value->Double = value;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(value);
   else {
      parasol::Log log;
      return log.warning(ERR_FieldTypeMismatch);
   }

   return ERR_Okay;
}

static ERROR VIEW_SET_Y(objVectorViewport *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      Self->vpDimensions = (Self->vpDimensions | DMF_RELATIVE_Y) & (~DMF_FIXED_Y);
      Self->vpTargetY = val * 0.01;
   }
   else {
      Self->vpDimensions = (Self->vpDimensions | DMF_FIXED_Y) & (~DMF_RELATIVE_Y);
      Self->vpTargetY = val;
   }
   mark_dirty((objVector *)Self, RC_ALL);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
YOffset: Positions the viewport on the y-axis.

The display position targeted by the viewport is declared by the (X,Y) field values.  Coordinates can be expressed as
fixed or relative pixel units.

If an offset from the edge of the parent is desired, the #YOffset must be defined.  If a Y and YOffset value are
defined together, the height of the viewport is computed on-the-fly and will change in response to the parent's
height.

*********************************************************************************************************************/

static ERROR VIEW_GET_YOffset(objVectorViewport *Self, Variable *Value)
{
   DOUBLE height;
   DOUBLE value = 0;

   if (Self->Dirty) gen_vector_tree((objVector *)Self);

   if (Self->vpDimensions & DMF_FIXED_Y_OFFSET) value = Self->vpTargetYO;
   else if (Self->vpDimensions & DMF_RELATIVE_Y_OFFSET) {
      value = (DOUBLE)Self->vpTargetYO * (DOUBLE)Self->ParentView->vpFixedHeight * 0.01;
   }
   else if ((Self->vpDimensions & DMF_Y) and (Self->vpDimensions & DMF_HEIGHT)) {
      if (Self->vpDimensions & DMF_FIXED_HEIGHT) height = Self->vpTargetHeight;
      else height = (DOUBLE)Self->ParentView->vpFixedHeight * (DOUBLE)Self->vpTargetHeight * 0.01;

      if (Self->vpDimensions & DMF_FIXED_Y) value = Self->ParentView->vpFixedHeight - (Self->vpTargetY + height);
      else value = (DOUBLE)Self->ParentView->vpFixedHeight - (((DOUBLE)Self->vpTargetY * (DOUBLE)Self->ParentView->vpFixedHeight * 0.01) + (DOUBLE)height);
   }
   else value = 0;

   if (Value->Type & FD_PERCENTAGE) value = ((DOUBLE)value * 100.0) / (DOUBLE)Self->ParentView->vpFixedHeight;

   if (Value->Type & FD_DOUBLE) Value->Double = value;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(value);
   else {
      parasol::Log log;
      return log.warning(ERR_FieldTypeMismatch);
   }

   return ERR_Okay;
}

static ERROR VIEW_SET_YOffset(objVectorViewport *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      Self->vpDimensions = (Self->vpDimensions | DMF_RELATIVE_Y_OFFSET) & (~DMF_FIXED_Y_OFFSET);
      Self->vpTargetYO = val * 0.01;
   }
   else {
      Self->vpDimensions = (Self->vpDimensions | DMF_FIXED_Y_OFFSET) & (~DMF_RELATIVE_Y_OFFSET);
      Self->vpTargetYO = val;
   }
   mark_dirty((objVector *)Self, RC_ALL);
   return ERR_Okay;
}

//********************************************************************************************************************

#include "viewport_def.cpp"

static const FieldDef clViewDimensions[] = {
   { "RelativeX",      DMF_RELATIVE_X },
   { "RelativeY",      DMF_RELATIVE_Y },
   { "RelativeWidth",  DMF_RELATIVE_WIDTH },
   { "RelativeHeight", DMF_RELATIVE_HEIGHT },
   { "FixedX",         DMF_FIXED_X },
   { "FixedY",         DMF_FIXED_Y },
   { "FixedWidth",     DMF_FIXED_WIDTH },
   { "FixedHeight",    DMF_FIXED_HEIGHT },
   { NULL, 0 }
};

static const FieldDef clViewOverflow[] = {
   { "Hidden",  VOF_HIDDEN },
   { "Visible", VOF_VISIBLE },
   { "Scroll",  VOF_SCROLL },
   { "Inherit", VOF_INHERIT },
   { NULL, 0 }
};

static const FieldArray clViewFields[] = {
   { "AbsX",         FDF_VIRTUAL|FDF_LONG|FDF_R,        0, (APTR)VIEW_GET_AbsX, NULL },
   { "AbsY",         FDF_VIRTUAL|FDF_LONG|FDF_R,        0, (APTR)VIEW_GET_AbsY, NULL },
   { "AspectRatio",  FDF_VIRTUAL|FDF_LONGFLAGS|FDF_RW,  (MAXINT)&clAspectRatio,    (APTR)VIEW_GET_AspectRatio, (APTR)VIEW_SET_AspectRatio },
   { "Dimensions",   FDF_VIRTUAL|FDF_LONGFLAGS|FDF_R,   (MAXINT)&clViewDimensions, (APTR)VIEW_GET_Dimensions, (APTR)VIEW_SET_Dimensions },
   { "Drag",         FDF_VIRTUAL|FDF_OBJECT|FDF_RW,     ID_VECTORVIEWPORT, (APTR)VIEW_GET_Drag, (APTR)VIEW_SET_Drag },
   { "Overflow",     FDF_VIRTUAL|FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clViewOverflow, (APTR)VIEW_GET_Overflow, (APTR)VIEW_SET_Overflow },
   { "OverflowX",    FDF_VIRTUAL|FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clViewOverflow, (APTR)VIEW_GET_OverflowX, (APTR)VIEW_SET_OverflowX },
   { "OverflowY",    FDF_VIRTUAL|FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clViewOverflow, (APTR)VIEW_GET_OverflowY, (APTR)VIEW_SET_OverflowY },
   { "X",            FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)VIEW_GET_X,       (APTR)VIEW_SET_X },
   { "Y",            FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)VIEW_GET_Y,       (APTR)VIEW_SET_Y },
   { "XOffset",      FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)VIEW_GET_XOffset, (APTR)VIEW_SET_XOffset },
   { "YOffset",      FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)VIEW_GET_YOffset, (APTR)VIEW_SET_YOffset },
   { "Width",        FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)VIEW_GET_Width,   (APTR)VIEW_SET_Width },
   { "Height",       FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)VIEW_GET_Height,  (APTR)VIEW_SET_Height },
   { "ViewX",        FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,  0, (APTR)VIEW_GET_ViewX,      (APTR)VIEW_SET_ViewX },
   { "ViewY",        FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,  0, (APTR)VIEW_GET_ViewY,      (APTR)VIEW_SET_ViewY },
   { "ViewWidth",    FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,  0, (APTR)VIEW_GET_ViewWidth,  (APTR)VIEW_SET_ViewWidth },
   { "ViewHeight",   FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,  0, (APTR)VIEW_GET_ViewHeight, (APTR)VIEW_SET_ViewHeight },
   END_FIELD
};

static ERROR init_viewport(void)
{
   return(CreateObject(ID_METACLASS, 0, &clVectorViewport,
      FID_BaseClassID|TLONG, ID_VECTOR,
      FID_SubClassID|TLONG,  ID_VECTORVIEWPORT,
      FID_Name|TSTRING,      "VectorViewport",
      FID_Category|TLONG,    CCF_GRAPHICS,
      FID_Actions|TPTR,      clVectorViewportActions,
      FID_Methods|TARRAY,    clVectorViewportMethods,
      FID_Fields|TARRAY,     clViewFields,
      FID_Size|TLONG,        sizeof(objVectorViewport),
      FID_Path|TSTR,         MOD_PATH,
      TAGEND));
}
