/*****************************************************************************

-CLASS-
VectorViewport: Provides support for viewport definitions within a vector tree.

This class is used to declare a viewport within a vector scene graph.  A master viewport is required as the first object
in a @VectorScene and it must contain all vector graphics content.

The size of the viewport is initially set to (0,0,100%,100%) so as to be all inclusive.  Setting the #X, #Y, #Width and
#Height fields will determine the position and clipping of the displayed content (the 'target area').  The #ViewX, #ViewY,
#ViewWidth and #ViewHeight fields declare the viewbox ('source area') that will be sampled for the target.

To configure the scaling method that is applied to the viewport content, set the #AspectRatio field.

-END-

NOTE: Refer to gen_vector_path() for the code that manages viewport dimensions in a live state.

*****************************************************************************/

/*****************************************************************************
-ACTION-
Clear: Free all child objects contained by the viewport.
-END-
*****************************************************************************/

static ERROR VIEW_Clear(objVectorViewport *Self, APTR Void)
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

//****************************************************************************

static ERROR VIEW_Free(objVectorViewport *Self, APTR Void)
{
   if (Self->vpClipMask) { acFree(Self->vpClipMask); Self->vpClipMask = NULL; }
   return ERR_Okay;
}

//****************************************************************************

static ERROR VIEW_Init(objVectorViewport *Self, APTR Void)
{
   // Please refer to gen_vector_path() for the initialisation of vpFixedX/Y/Width/Height, which has
   // its own section for dealing with viewports.

   if (!(Self->vpDimensions & (DMF_FIXED_X|DMF_RELATIVE_X|DMF_FIXED_X_OFFSET|DMF_RELATIVE_X_OFFSET))) {
      Self->vpTargetX = 0;
      Self->vpDimensions |= DMF_FIXED_X;
   }

   if (!(Self->vpDimensions & (DMF_FIXED_Y|DMF_RELATIVE_Y|DMF_FIXED_Y_OFFSET|DMF_RELATIVE_Y_OFFSET))) {
      Self->vpTargetY = 0;
      Self->vpDimensions |= DMF_FIXED_Y;
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Move: Move the position of the viewport to a relative position.
-END-
*****************************************************************************/

static ERROR VIEW_Move(objVectorViewport *Self, struct acMove *Args)
{
   if (!Args) return ERR_NullArgs;

   DOUBLE x, y;
   if (!GetFields(Self, FID_X|TDOUBLE, &x, FID_Y|TDOUBLE, &y, TAGEND)) {
      Self->vpDimensions = (Self->vpDimensions | DMF_FIXED_X) & (~DMF_RELATIVE_X);
      Self->vpTargetX += x;

      Self->vpDimensions = (Self->vpDimensions | DMF_FIXED_Y) & (~DMF_RELATIVE_Y);
      Self->vpTargetY += y;

      mark_dirty((objVector *)Self, RC_FINAL_PATH|RC_TRANSFORM);
      return ERR_Okay;
   }
   else return ERR_GetField;
}

/*****************************************************************************
-ACTION-
MoveToPoint: Move the position of the viewport to a fixed point.
-END-
*****************************************************************************/

static ERROR VIEW_MoveToPoint(objVectorViewport *Self, struct acMoveToPoint *Args)
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

   mark_dirty((objVector *)Self, RC_FINAL_PATH|RC_TRANSFORM);
   return ERR_Okay;
}

//****************************************************************************

static ERROR VIEW_NewObject(objVectorViewport *Self, APTR Void)
{
   Self->vpAspectRatio = ARF_MEET|ARF_X_MID|ARF_Y_MID;

   // NB: vpTargetWidth and vpTargetHeight are not set to a default because we need to know if the client has
   // intentionally avoided setting the viewport and/or viewbox dimensions (which typically means that the viewport
   // will expand to fit the parent).
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Resize: Resize a viewport to a fixed size.
-END-
*****************************************************************************/

static ERROR VIEW_Resize(objVectorViewport *Self, struct acResize *Args)
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

/*****************************************************************************

-FIELD-
AbsX: The horizontal position of the viewport, relative to (0,0).

This field will return the left-most boundary of the viewport, relative to point (0,0) of the scene
graph.  Transforms are taken into consideration when calculating this value.

*****************************************************************************/

static ERROR VIEW_GET_AbsX(objVectorViewport *Self, LONG *Value)
{
   if (Self->Dirty) {
      gen_vector_path((objVector *)Self);
      Self->Dirty = 0;
   }

   *Value = Self->vpBX1;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
AbsY: The vertical position of the viewport, relative to (0,0).

This field will return the top-most boundary of the viewport, relative to point (0,0) of the scene
graph.  Transforms are taken into consideration when calculating this value.

*****************************************************************************/

static ERROR VIEW_GET_AbsY(objVectorViewport *Self, LONG *Value)
{
   if (Self->Dirty) {
      gen_vector_path((objVector *)Self);
      Self->Dirty = 0;
   }

   *Value = Self->vpBY1;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
AspectRatio: Flags that affect the aspect ratio of vectors within the viewport.
Lookup: ARF

Defining an aspect ratio allows finer control over the position and scale of the viewport's content within its target
area.

<types lookup="ARF"/>

*****************************************************************************/

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

/*****************************************************************************
-FIELD-
Dimensions: Dimension flags define whether individual dimension fields contain fixed or relative values.
Lookup: DMF

The supported dimension flags are currently limited to: FIXED_X, FIXED_Y, FIXED_WIDTH, FIXED_HEIGHT, RELATIVE_X,
RELATIVE_Y, RELATIVE_WIDTH, RELATIVE_HEIGHT.

<types lookup="DMF"/>

*****************************************************************************/

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

/*****************************************************************************
-FIELD-
Height: The height of the viewport's target area.

The height of the viewport's target area is defined here as a fixed or relative value.  The default value is 100% for
full coverage.

*****************************************************************************/

static ERROR VIEW_GET_Height(objVectorViewport *Self, Variable *Value)
{
   DOUBLE val;

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

/*****************************************************************************
-FIELD-
ViewHeight: The height of the viewport's source area.

The area defined by (#ViewX,#ViewY) and (#ViewWidth,#ViewHeight) declare the source area covered by the viewport.  The
rendered graphics in the source area will be repositioned and scaled to the area defined by (X,Y) and (Width,Height).

*****************************************************************************/

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

/*****************************************************************************
-FIELD-
ViewX: The horizontal position of the viewport's source area.

The area defined by (#ViewX,#ViewY) and (#ViewWidth,#ViewHeight) declare the source area covered by the viewport.  The
rendered graphics in the source area will be repositioned and scaled to the area defined by (#X,#Y) and (#Width,#Height).

*****************************************************************************/

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

/*****************************************************************************
-FIELD-
ViewWidth: The width of the viewport's source area.

The area defined by (#ViewX,#ViewY) and (#ViewWidth,#ViewHeight) declare the source area covered by the viewport.  The
rendered graphics in the source area will be repositioned and scaled to the area defined by (#X,#Y) and (#Width,#Height).

*****************************************************************************/

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

/*****************************************************************************
-FIELD-
ViewY: The vertical position of the viewport's source area.

The area defined by (#ViewX,#ViewY) and (#ViewWidth,#ViewHeight) declare the source area covered by the viewport.  The
rendered graphics in the source area will be repositioned and scaled to the area defined by (#X,#Y) and (#Width,#Height).

*****************************************************************************/

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

/*****************************************************************************
-FIELD-
Width: The width of the viewport's target area.

The width of the viewport's target area is defined here as a fixed or relative value.  The default value is 100% for
full coverage.

*****************************************************************************/

static ERROR VIEW_GET_Width(objVectorViewport *Self, Variable *Value)
{
   DOUBLE val;
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

/*****************************************************************************
-FIELD-
X: Positions the viewport on the x-axis.

The display position targeted by the viewport is declared by the (X,Y) field values.  Coordinates can be expressed as
fixed or relative pixel units.

If an offset from the edge of the parent is desired, the #XOffset field must be defined.  If a X and XOffset value are defined
together, the width of the viewport is computed on-the-fly and will change in response to the parent's width.

*****************************************************************************/

static ERROR VIEW_GET_X(objVectorViewport *Self, Variable *Value)
{
   DOUBLE width, value;

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

/*****************************************************************************
-FIELD-
XOffset: Positions the viewport on the x-axis.

The display position targeted by the viewport is declared by the (X,Y) field values.  Coordinates can be expressed as
fixed or relative pixel units.

If an offset from the edge of the parent is desired, the #XOffset field must be defined.  If a X and XOffset value are defined
together, the width of the viewport is computed on-the-fly and will change in response to the parent's width.

*****************************************************************************/

static ERROR VIEW_GET_XOffset(objVectorViewport *Self, Variable *Value)
{
   DOUBLE width;
   DOUBLE value = 0;
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

/*****************************************************************************
-FIELD-
Y: Positions the viewport on the y-axis.

The display position targeted by the viewport is declared by the (X,Y) field values.  Coordinates can be expressed as
fixed or relative pixel units.

If an offset from the edge of the parent is desired, the #YOffset must be defined.  If a Y and YOffset value are defined
together, the height of the viewport is computed on-the-fly and will change in response to the parent's height.

-END-
*****************************************************************************/

static ERROR VIEW_GET_Y(objVectorViewport *Self, Variable *Value)
{
   DOUBLE value, height;

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

/*****************************************************************************
-FIELD-
YOffset: Positions the viewport on the y-axis.

The display position targeted by the viewport is declared by the (X,Y) field values.  Coordinates can be expressed as
fixed or relative pixel units.

If an offset from the edge of the parent is desired, the #YOffset must be defined.  If a Y and YOffset value are defined
together, the height of the viewport is computed on-the-fly and will change in response to the parent's height.

*****************************************************************************/

static ERROR VIEW_GET_YOffset(objVectorViewport *Self, Variable *Value)
{
   DOUBLE height;
   DOUBLE value = 0;
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

//****************************************************************************

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

static const FieldArray clViewFields[] = {
   { "AbsX",        FDF_VIRTUAL|FDF_LONG|FDF_R, 0, (APTR)VIEW_GET_AbsX, NULL },
   { "AbsY",        FDF_VIRTUAL|FDF_LONG|FDF_R, 0, (APTR)VIEW_GET_AbsY, NULL },
   { "X",           FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)VIEW_GET_X,       (APTR)VIEW_SET_X },
   { "Y",           FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)VIEW_GET_Y,       (APTR)VIEW_SET_Y },
   { "XOffset",     FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)VIEW_GET_XOffset, (APTR)VIEW_SET_XOffset },
   { "YOffset",     FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)VIEW_GET_YOffset, (APTR)VIEW_SET_YOffset },
   { "Width",       FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)VIEW_GET_Width,   (APTR)VIEW_SET_Width },
   { "Height",      FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)VIEW_GET_Height,  (APTR)VIEW_SET_Height },
   { "ViewX",       FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, 0, (APTR)VIEW_GET_ViewX,      (APTR)VIEW_SET_ViewX },
   { "ViewY",       FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, 0, (APTR)VIEW_GET_ViewY,      (APTR)VIEW_SET_ViewY },
   { "ViewWidth",   FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, 0, (APTR)VIEW_GET_ViewWidth,  (APTR)VIEW_SET_ViewWidth },
   { "ViewHeight",  FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, 0, (APTR)VIEW_GET_ViewHeight, (APTR)VIEW_SET_ViewHeight },
   { "Dimensions",  FDF_VIRTUAL|FDF_LONGFLAGS|FDF_R,  (MAXINT)&clViewDimensions, (APTR)VIEW_GET_Dimensions, (APTR)VIEW_SET_Dimensions },
   { "AspectRatio", FDF_VIRTUAL|FDF_LONGFLAGS|FDF_RW, (MAXINT)&clAspectRatio,    (APTR)VIEW_GET_AspectRatio, (APTR)VIEW_SET_AspectRatio },
   END_FIELD
};

static const ActionArray clViewActions[] = {
   { AC_Clear,       (APTR)VIEW_Clear },
   { AC_Free,        (APTR)VIEW_Free },
   { AC_Init,        (APTR)VIEW_Init },
   { AC_NewObject,   (APTR)VIEW_NewObject },
   { AC_Move,        (APTR)VIEW_Move },
   { AC_MoveToPoint, (APTR)VIEW_MoveToPoint },
   //{ AC_Redimension, (APTR)VIEW_Redimension },
   { AC_Resize,      (APTR)VIEW_Resize },
   { 0, NULL }
};

static ERROR init_viewport(void)
{
   return(CreateObject(ID_METACLASS, 0, &clVectorViewport,
      FID_BaseClassID|TLONG, ID_VECTOR,
      FID_SubClassID|TLONG,  ID_VECTORVIEWPORT,
      FID_Name|TSTRING,      "VectorViewport",
      FID_Category|TLONG,    CCF_GRAPHICS,
      FID_Actions|TPTR,      clViewActions,
      FID_Fields|TARRAY,     clViewFields,
      FID_Size|TLONG,        sizeof(objVectorViewport),
      FID_Path|TSTR,         MOD_PATH,
      TAGEND));
}
