/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
VectorViewport: Provides support for viewport definitions within a vector tree.

This class is used to declare a viewport within a vector scene graph.  A master viewport is required as the first object
in a @VectorScene and it must contain all vector graphics content.

The size of the viewport is initially set to `(0,0,100%,100%)` so as to be all inclusive.  Setting the #X, #Y, #Width and
#Height fields will determine the position and clipping of the displayed content (the 'target area').  The #ViewX, #ViewY,
#ViewWidth and #ViewHeight fields declare the viewbox ('source area') that will be sampled for the target.

To configure the scaling and alignment method that is applied to the viewport content, set the #AspectRatio field.

-END-

NOTE: Refer to gen_vector_path() for the code that manages viewport dimensions in a live state.

*********************************************************************************************************************/

//********************************************************************************************************************
// Input event handler for the dragging of viewports by the user.  Requires the client to set the DragCallback field
// to be active.

static ERROR drag_callback(extVectorViewport *Viewport, const InputEvent *Events)
{
   static DOUBLE glAnchorX = 0, glAnchorY = 0; // Anchoring is process-exclusive, so we can store the coordinates as global variables
   static DOUBLE glDragOriginX = 0, glDragOriginY = 0;

   if (Viewport->Dirty) gen_vector_tree(Viewport);

   for (auto event=Events; event; event=event->Next) {
      // Process events that support consolidation first.

      if (event->Flags & (JTYPE_ANCHORED|JTYPE_MOVEMENT)) {
         if (Viewport->vpDragging) {
            while ((event->Next) and (event->Next->Flags & JTYPE_MOVEMENT)) { // Consolidate movement
               event = event->Next;
            }

            DOUBLE x = glDragOriginX + (event->AbsX - glAnchorX);
            DOUBLE y = glDragOriginY + (event->AbsY - glAnchorY);

            if (Viewport->vpDragCallback.Type IS CALL_STDC) {
               parasol::SwitchContext context(Viewport->vpDragCallback.StdC.Context);
               auto routine = (void (*)(extVectorViewport *, DOUBLE, DOUBLE, DOUBLE, DOUBLE))Viewport->vpDragCallback.StdC.Routine;
               routine(Viewport, x, y, glDragOriginX, glDragOriginY);
            }
            else if (Viewport->vpDragCallback.Type IS CALL_SCRIPT) {
               if (auto script = Viewport->vpDragCallback.Script.Script) {
                  const ScriptArg args[] = {
                     { "Viewport", FD_OBJECTPTR, { .Address = Viewport } },
                     { "X",        FD_DOUBLE, { .Double = x } },
                     { "Y",        FD_DOUBLE, { .Double = y } },
                     { "OriginX",  FD_DOUBLE, { .Double = glDragOriginX } },
                     { "OriginY",  FD_DOUBLE, { .Double = glDragOriginY } }
                  };
                  scCallback(script, Viewport->vpDragCallback.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
               }
            }
         }
      }
      else if ((event->Type IS JET_LMB) and (!(event->Flags & JTYPE_REPEATED))) {
         if (event->Value > 0) {
            if (Viewport->Visibility != VIS_VISIBLE) continue;
            Viewport->vpDragging = 1;
            glAnchorX = event->AbsX;
            glAnchorY = event->AbsY;

            GetFields(Viewport,
               FID_X|TDOUBLE, &glDragOriginX,
               FID_Y|TDOUBLE, &glDragOriginY,
               TAGEND);

            // Ensure that the X,Y coordinates are fixed.

            Viewport->vpDimensions = (Viewport->vpDimensions | DMF_FIXED_X | DMF_FIXED_Y) &
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

static ERROR VECTORVIEWPORT_Clear(extVectorViewport *Self, APTR Void)
{
   ChildEntry list[512];
   LONG count = ARRAYSIZE(list);
   do {
      if (!ListChildren(Self->UID, FALSE, list, &count)) {
         for (LONG i=0; i < count; i++) acFree(list[i].ObjectID);
      }
   } while (count IS ARRAYSIZE(list));

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR VECTORVIEWPORT_Free(extVectorViewport *Self, APTR Void)
{
   if (Self->vpClipMask) { acFree(Self->vpClipMask); Self->vpClipMask = NULL; }

   if ((Self->Scene) and (!((extVectorScene *)Self->Scene)->ResizeSubscriptions.empty())) {
      if (((extVectorScene *)Self->Scene)->ResizeSubscriptions.contains(Self)) {
         ((extVectorScene *)Self->Scene)->ResizeSubscriptions.erase(Self);
      }
   }

   if (Self->vpDragCallback.Type) {
      auto callback = make_function_stdc(drag_callback);
      vecSubscribeInput(Self, 0, &callback);
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR VECTORVIEWPORT_Init(extVectorViewport *Self, APTR Void)
{
   // Initialisation is performed by VECTOR_Init()

   // Please refer to gen_vector_path() for the initialisation of vpFixedX/Y/Width/Height, which has
   // its own section for dealing with viewports.

   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Move: Move the position of the viewport by delta X, Y.

*********************************************************************************************************************/

static ERROR VECTORVIEWPORT_Move(extVectorViewport *Self, struct acMove *Args)
{
   if (!Args) return ERR_NullArgs;

   Self->vpDimensions = (Self->vpDimensions|DMF_FIXED_X|DMF_FIXED_Y) & (~(DMF_RELATIVE_X|DMF_RELATIVE_Y));
   Self->vpTargetX += Args->DeltaX;
   Self->vpTargetY += Args->DeltaY;

   mark_dirty((extVector *)Self, RC_FINAL_PATH|RC_TRANSFORM);
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
MoveToPoint: Move the position of the viewport to a fixed point.
-END-
*********************************************************************************************************************/

static ERROR VECTORVIEWPORT_MoveToPoint(extVectorViewport *Self, struct acMoveToPoint *Args)
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

   mark_dirty((extVector *)Self, RC_FINAL_PATH|RC_TRANSFORM);
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR VECTORVIEWPORT_NewObject(extVectorViewport *Self, APTR Void)
{
   Self->vpAspectRatio = ARF_MEET|ARF_X_MID|ARF_Y_MID;
   Self->vpOverflowX   = VOF_VISIBLE;
   Self->vpOverflowY   = VOF_VISIBLE;

   // NB: vpTargetWidth and vpTargetHeight are not set to a default because we need to know if the client has
   // intentionally avoided setting the viewport and/or viewbox dimensions (which typically means that the viewport
   // will expand to fit the parent).
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Redimension: Reposition and resize a viewport to a fixed size.
-END-
*********************************************************************************************************************/

static ERROR VECTORVIEWPORT_Redimension(extVectorViewport *Self, struct acRedimension *Args)
{
   if (!Args) return ERR_NullArgs;

   Self->vpDimensions = (Self->vpDimensions|DMF_FIXED_X|DMF_FIXED_Y|DMF_FIXED_WIDTH|DMF_FIXED_HEIGHT) &
      (~(DMF_RELATIVE_X|DMF_RELATIVE_Y|DMF_RELATIVE_WIDTH|DMF_RELATIVE_HEIGHT));

   Self->vpTargetX      = Args->X;
   Self->vpTargetY      = Args->Y;
   Self->vpTargetWidth  = Args->Width;
   Self->vpTargetHeight = Args->Height;

   if (Self->vpTargetWidth < 1) Self->vpTargetWidth = 1;
   if (Self->vpTargetHeight < 1) Self->vpTargetHeight = 1;
   mark_dirty((extVector *)Self, RC_FINAL_PATH|RC_TRANSFORM);
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Resize: Resize a viewport to a fixed size.
-END-
*********************************************************************************************************************/

static ERROR VECTORVIEWPORT_Resize(extVectorViewport *Self, struct acResize *Args)
{
   if (!Args) return ERR_NullArgs;

   Self->vpDimensions = (Self->vpDimensions | DMF_FIXED_WIDTH) & (~DMF_RELATIVE_WIDTH);
   Self->vpTargetWidth = Args->Width;

   Self->vpDimensions = (Self->vpDimensions | DMF_FIXED_HEIGHT) & (~DMF_RELATIVE_HEIGHT);
   Self->vpTargetHeight = Args->Height;

   if (Self->vpTargetWidth < 1) Self->vpTargetWidth = 1;
   if (Self->vpTargetHeight < 1) Self->vpTargetHeight = 1;
   mark_dirty((extVector *)Self, RC_FINAL_PATH|RC_TRANSFORM);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
AbsX: The horizontal position of the viewport, relative to (0,0).

This field will return the left-most boundary of the viewport, relative to point (0,0) of the scene
graph.  Transforms are taken into consideration when calculating this value.

*********************************************************************************************************************/

static ERROR VIEW_GET_AbsX(extVectorViewport *Self, LONG *Value)
{
   if (Self->Dirty) gen_vector_tree(Self);

   *Value = Self->vpBX1;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
AbsY: The vertical position of the viewport, relative to (0,0).

This field will return the top-most boundary of the viewport, relative to point (0,0) of the scene
graph.  Transforms are taken into consideration when calculating this value.

*********************************************************************************************************************/

static ERROR VIEW_GET_AbsY(extVectorViewport *Self, LONG *Value)
{
   if (Self->Dirty) gen_vector_tree(Self);

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

static ERROR VIEW_GET_AspectRatio(extVectorViewport *Self, LONG *Value)
{
   *Value = Self->vpAspectRatio;
   return ERR_Okay;
}

static ERROR VIEW_SET_AspectRatio(extVectorViewport *Self, LONG Value)
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

static ERROR VIEW_GET_Dimensions(extVectorViewport *Self, LONG *Value)
{
   *Value = Self->vpDimensions;
   return ERR_Okay;
}

static ERROR VIEW_SET_Dimensions(extVectorViewport *Self, LONG Value)
{
   Self->vpDimensions = Value;
   mark_dirty((extVector *)Self, RC_ALL);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
DragCallback: Receiver for drag requests originating from the viewport.

Set the DragCallback field with a callback function to receive drag requests from the viewport's user input.  When the
user drags the viewport, the callback will receive the user's desired (X, Y) target coordinates.  For unimpeded
dragging, have the callback set the viewport's X and Y values to match the incoming coordinates, then redraw the scene.

The prototype for the callback function is as follows, where OriginX and OriginY refer to the (X,Y) position of the
vector at initiation of the drag.

`void function(*VectorViewport, DOUBLE X, DOUBLE Y, DOUBLE OriginX, DOUBLE OriginY)`

Setting this field to NULL will turn off the callback.

It is required that the parent @VectorScene is associated with a @Surface for this feature to work.

*********************************************************************************************************************/

static ERROR VIEW_GET_DragCallback(extVectorViewport *Self, FUNCTION **Value)
{
   if (Self->vpDragCallback.Type != CALL_NONE) {
      *Value = &Self->vpDragCallback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR VIEW_SET_DragCallback(extVectorViewport *Self, FUNCTION *Value)
{
   auto callback = make_function_stdc(drag_callback);

   if (Value) {
      if ((!Self->Scene) or (!Self->Scene->SurfaceID)) {
         parasol::Log log;
         return log.warning(ERR_FieldNotSet);
      }

      if (vecSubscribeInput(Self, JTYPE_MOVEMENT|JTYPE_BUTTON, &callback)) {
         return ERR_Failed;
      }

      Self->vpDragCallback = *Value;
   }
   else {
      Self->vpDragCallback.Type = CALL_NONE;
      vecSubscribeInput(Self, 0, &callback);
   }
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Height: The height of the viewport's target area.

The height of the viewport's target area is defined here as a fixed or relative value.  The default value is 100% for
full coverage.

The fixed value is always returned when retrieving the height.

*********************************************************************************************************************/

static ERROR VIEW_GET_Height(extVectorViewport *Self, Variable *Value)
{
   DOUBLE val;

   if (Self->Dirty) gen_vector_tree(Self);

   if (Self->vpDimensions & DMF_FIXED_HEIGHT) { // Working with a fixed dimension
      if (Value->Type & FD_PERCENTAGE) {
         if (Self->ParentView) val = Self->vpFixedHeight * Self->ParentView->vpFixedHeight;
         else val = Self->vpFixedHeight * Self->Scene->PageHeight;
      }
      else val = Self->vpTargetHeight;
   }
   else if (Self->vpDimensions & DMF_RELATIVE_HEIGHT) { // Working with a relative dimension
      if (Value->Type & FD_PERCENTAGE) val = Self->vpTargetHeight;
      else if (Self->ParentView) val = Self->vpTargetHeight * Self->ParentView->vpFixedHeight;
      else val = Self->vpTargetHeight * Self->Scene->PageHeight;
   }
   else if (Self->vpDimensions & (DMF_FIXED_Y_OFFSET|DMF_RELATIVE_Y_OFFSET)) {
      DOUBLE y, parent_height;

      if (Self->vpDimensions & DMF_RELATIVE_Y) y = Self->vpTargetY * Self->ParentView->vpFixedHeight;
      else y = Self->vpTargetY;

      if (Self->ParentView) Self->ParentView->get(FID_Height, &parent_height);
      else parent_height = Self->Scene->PageHeight;

      if (Self->vpDimensions & DMF_FIXED_Y_OFFSET) val = parent_height - Self->vpTargetYO - y;
      else val = parent_height - (Self->vpTargetYO * parent_height) - y;
   }
   else { // If no height set by the client, the full height is inherited from the parent
      if (Self->ParentView) return Self->ParentView->get(FID_Height, Value);
      else Self->Scene->get(FID_PageHeight, &val);
   }

   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR VIEW_SET_Height(extVectorViewport *Self, Variable *Value)
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
   mark_dirty((extVector *)Self, RC_ALL);
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

static ERROR VIEW_GET_Overflow(extVectorViewport *Self, LONG *Value)
{
   *Value = Self->vpOverflowX;
   return ERR_Okay;
}

static ERROR VIEW_SET_Overflow(extVectorViewport *Self, LONG Value)
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

static ERROR VIEW_GET_OverflowX(extVectorViewport *Self, LONG *Value)
{
   *Value = Self->vpOverflowX;
   return ERR_Okay;
}

static ERROR VIEW_SET_OverflowX(extVectorViewport *Self, LONG Value)
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

static ERROR VIEW_GET_OverflowY(extVectorViewport *Self, LONG *Value)
{
   *Value = Self->vpOverflowY;
   return ERR_Okay;
}

static ERROR VIEW_SET_OverflowY(extVectorViewport *Self, LONG Value)
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

static ERROR VIEW_GET_ViewHeight(extVectorViewport *Self, DOUBLE *Value)
{
   *Value = Self->vpViewHeight;
   return ERR_Okay;
}

static ERROR VIEW_SET_ViewHeight(extVectorViewport *Self, DOUBLE Value)
{
   if (Value > 0.0) {
      Self->vpViewHeight = Value;
      mark_dirty((extVector *)Self, RC_ALL);
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

static ERROR VIEW_GET_ViewX(extVectorViewport *Self, DOUBLE *Value)
{
   *Value = Self->vpViewX;
   return ERR_Okay;
}

static ERROR VIEW_SET_ViewX(extVectorViewport *Self, DOUBLE Value)
{
   Self->vpViewX = Value;
   mark_dirty((extVector *)Self, RC_ALL);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
ViewWidth: The width of the viewport's source area.

The area defined by (#ViewX,#ViewY) and (#ViewWidth,#ViewHeight) declare the source area covered by the viewport.  The
rendered graphics in the source area will be repositioned and scaled to the area defined by (#X,#Y) and
(#Width,#Height).

*********************************************************************************************************************/

static ERROR VIEW_GET_ViewWidth(extVectorViewport *Self, DOUBLE *Value)
{
   *Value = Self->vpViewWidth;
   return ERR_Okay;
}

static ERROR VIEW_SET_ViewWidth(extVectorViewport *Self, DOUBLE Value)
{
   if (Value > 0.0) {
      Self->vpViewWidth = Value;
      mark_dirty((extVector *)Self, RC_ALL);
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

static ERROR VIEW_GET_ViewY(extVectorViewport *Self, DOUBLE *Value)
{
   *Value = Self->vpViewY;
   return ERR_Okay;
}

static ERROR VIEW_SET_ViewY(extVectorViewport *Self, DOUBLE Value)
{
   Self->vpViewY = Value;
   mark_dirty((extVector *)Self, RC_ALL);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Width: The width of the viewport's target area.

The width of the viewport's target area is defined here as a fixed or relative value.  The default value is 100% for
full coverage.

*********************************************************************************************************************/

static ERROR VIEW_GET_Width(extVectorViewport *Self, Variable *Value)
{
   DOUBLE val;

   if (Self->Dirty) gen_vector_tree(Self);

   if (Self->vpDimensions & DMF_FIXED_WIDTH) { // Working with a fixed dimension
      if (Value->Type & FD_PERCENTAGE) {
         if (Self->ParentView) val = Self->vpFixedWidth * Self->ParentView->vpFixedWidth;
         else val = Self->vpFixedWidth * Self->Scene->PageWidth;
      }
      else val = Self->vpTargetWidth;
   }
   else if (Self->vpDimensions & DMF_RELATIVE_WIDTH) { // Working with a relative dimension
      if (Value->Type & FD_PERCENTAGE) val = Self->vpTargetWidth;
      else if (Self->ParentView) val = Self->vpTargetWidth * Self->ParentView->vpFixedWidth;
      else val = Self->vpTargetWidth * Self->Scene->PageWidth;
   }
   else if (Self->vpDimensions & (DMF_FIXED_X_OFFSET|DMF_RELATIVE_X_OFFSET)) {
      DOUBLE x, parent_width;

      if (Self->vpDimensions & DMF_RELATIVE_X) x = Self->vpTargetX * Self->ParentView->vpFixedWidth;
      else x = Self->vpTargetX;

      if (Self->ParentView) Self->ParentView->get(FID_Width, &parent_width);
      else parent_width = Self->Scene->PageWidth;

      if (Self->vpDimensions & DMF_FIXED_X_OFFSET) val = parent_width - Self->vpTargetXO - x;
      else val = parent_width - (Self->vpTargetXO * parent_width) - x;
   }
   else { // If no width set by the client, the full width is inherited from the parent
      if (Self->ParentView) return Self->ParentView->get(FID_Width, Value);
      else Self->Scene->get(FID_PageWidth, &val);
   }

   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR VIEW_SET_Width(extVectorViewport *Self, Variable *Value)
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
   mark_dirty((extVector *)Self, RC_ALL);
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

static ERROR VIEW_GET_X(extVectorViewport *Self, Variable *Value)
{
   DOUBLE width, value;

   if (Self->Dirty) gen_vector_tree(Self);

   if (Self->vpDimensions & DMF_FIXED_X) value = Self->vpTargetX;
   else if (Self->vpDimensions & DMF_RELATIVE_X) {
      value = Self->vpTargetX * Self->ParentView->vpFixedWidth;
   }
   else if ((Self->vpDimensions & DMF_WIDTH) and (Self->vpDimensions & DMF_X_OFFSET)) {
      if (Self->vpDimensions & DMF_FIXED_WIDTH) width = Self->vpTargetWidth;
      else width = Self->ParentView->vpFixedWidth * Self->vpTargetWidth;
      if (Self->vpDimensions & DMF_FIXED_X_OFFSET) value = Self->ParentView->vpFixedWidth - width - Self->vpTargetXO;
      else value = Self->ParentView->vpFixedWidth - width - (Self->ParentView->vpFixedWidth * Self->vpTargetXO);
   }
   else value = 0;

   if (Value->Type & FD_PERCENTAGE) value = (value * 100.0) / Self->ParentView->vpFixedWidth;

   if (Value->Type & FD_DOUBLE) Value->Double = value;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(value);
   else {
      parasol::Log log;
      return log.warning(ERR_FieldTypeMismatch);
   }

   return ERR_Okay;
}

static ERROR VIEW_SET_X(extVectorViewport *Self, Variable *Value)
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
   mark_dirty((extVector *)Self, RC_ALL);
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

static ERROR VIEW_GET_XOffset(extVectorViewport *Self, Variable *Value)
{
   DOUBLE width;
   DOUBLE value = 0;

   if (Self->Dirty) gen_vector_tree(Self);

   if (Self->vpDimensions & DMF_FIXED_X_OFFSET) value = Self->vpTargetXO;
   else if (Self->vpDimensions & DMF_RELATIVE_X_OFFSET) {
      value = Self->vpTargetXO * Self->ParentView->vpFixedWidth;
   }
   else if ((Self->vpDimensions & DMF_X) and (Self->vpDimensions & DMF_WIDTH)) {
      if (Self->vpDimensions & DMF_FIXED_WIDTH) width = Self->vpTargetWidth;
      else width = Self->ParentView->vpFixedWidth * Self->vpTargetWidth;

      if (Self->vpDimensions & DMF_FIXED_X) value = Self->ParentView->vpFixedWidth - (Self->vpTargetX + width);
      else value = Self->ParentView->vpFixedWidth - ((Self->vpTargetX * Self->ParentView->vpFixedWidth) + width);
   }
   else value = 0;

   if (Value->Type & FD_PERCENTAGE) value = (value * 100.0) / Self->ParentView->vpFixedWidth;

   if (Value->Type & FD_DOUBLE) Value->Double = value;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(value);
   else {
      parasol::Log log;
      return log.warning(ERR_FieldTypeMismatch);
   }

   return ERR_Okay;
}

static ERROR VIEW_SET_XOffset(extVectorViewport *Self, Variable *Value)
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
   mark_dirty((extVector *)Self, RC_ALL);
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

static ERROR VIEW_GET_Y(extVectorViewport *Self, Variable *Value)
{
   DOUBLE value, height;

   if (Self->Dirty) gen_vector_tree(Self);

   if (Self->vpDimensions & DMF_FIXED_Y) value = Self->vpTargetY;
   else if (Self->vpDimensions & DMF_RELATIVE_Y) {
      value = Self->vpTargetY * Self->ParentView->vpFixedHeight;
   }
   else if ((Self->vpDimensions & DMF_HEIGHT) and (Self->vpDimensions & DMF_Y_OFFSET)) {
      if (Self->vpDimensions & DMF_FIXED_HEIGHT) height = Self->vpTargetHeight;
      else height = Self->ParentView->vpFixedHeight * Self->vpTargetHeight;

      if (Self->vpDimensions & DMF_FIXED_Y_OFFSET) value = Self->ParentView->vpFixedHeight - height - Self->vpTargetYO;
      else value = Self->ParentView->vpFixedHeight - height - (Self->ParentView->vpFixedHeight * Self->vpTargetYO);
   }
   else value = 0;

   if (Value->Type & FD_PERCENTAGE) value = (value * 100.0) / Self->ParentView->vpFixedHeight;
   if (Value->Type & FD_DOUBLE) Value->Double = value;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(value);
   else {
      parasol::Log log;
      return log.warning(ERR_FieldTypeMismatch);
   }

   return ERR_Okay;
}

static ERROR VIEW_SET_Y(extVectorViewport *Self, Variable *Value)
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
   mark_dirty((extVector *)Self, RC_ALL);
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

static ERROR VIEW_GET_YOffset(extVectorViewport *Self, Variable *Value)
{
   DOUBLE height;
   DOUBLE value = 0;

   if (Self->Dirty) gen_vector_tree(Self);

   if (Self->vpDimensions & DMF_FIXED_Y_OFFSET) value = Self->vpTargetYO;
   else if (Self->vpDimensions & DMF_RELATIVE_Y_OFFSET) {
      value = Self->vpTargetYO * Self->ParentView->vpFixedHeight;
   }
   else if ((Self->vpDimensions & DMF_Y) and (Self->vpDimensions & DMF_HEIGHT)) {
      if (Self->vpDimensions & DMF_FIXED_HEIGHT) height = Self->vpTargetHeight;
      else height = Self->ParentView->vpFixedHeight * Self->vpTargetHeight;

      if (Self->vpDimensions & DMF_FIXED_Y) value = Self->ParentView->vpFixedHeight - (Self->vpTargetY + height);
      else value = Self->ParentView->vpFixedHeight - ((Self->vpTargetY * Self->ParentView->vpFixedHeight) + height);
   }
   else value = 0;

   if (Value->Type & FD_PERCENTAGE) value = (value * 100.0) / Self->ParentView->vpFixedHeight;

   if (Value->Type & FD_DOUBLE) Value->Double = value;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(value);
   else {
      parasol::Log log;
      return log.warning(ERR_FieldTypeMismatch);
   }

   return ERR_Okay;
}

static ERROR VIEW_SET_YOffset(extVectorViewport *Self, Variable *Value)
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
   mark_dirty((extVector *)Self, RC_ALL);
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
   { "DragCallback", FDF_VIRTUAL|FDF_FUNCTIONPTR|FDF_RW, 0, (APTR)VIEW_GET_DragCallback, (APTR)VIEW_SET_DragCallback },
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
      FID_Fields|TARRAY,     clViewFields,
      FID_Size|TLONG,        sizeof(extVectorViewport),
      FID_Path|TSTR,         MOD_PATH,
      TAGEND));
}
