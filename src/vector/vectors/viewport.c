/*****************************************************************************

-CLASS-
VectorViewport: Provides support for viewport definitions within a vector tree.

This class is used to declare a viewport within a vector definition.  A master viewport is required as the first object
in a @VectorScene and it must contain all vector graphics content.

The size of the viewport is initially set to (0,0,100%,100%) so as to be all inclusive.  Setting the X, Y, Width and
Height fields will determine the position and clipping of the displayed content (the 'target area').  The ViewX, ViewY,
ViewWidth and ViewHeight fields declare the viewbox (the 'source area') that will be sampled for the target.

To configure the scaling method that is applied to the viewport content, set the #AspectRatio field.

-END-

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
      if (!ListChildren(Self->Head.UniqueID, list, &count)) {
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
   // Please refer to gen_vector_path() for the initialisation of vpFixedX/Y/Width/Height

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
   Self->vpTargetX = 0;
   Self->vpTargetY = 0;
   Self->vpDimensions = DMF_FIXED_X|DMF_FIXED_Y;
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
      else { val = Self->vpTargetHeight * Self->Scene->PageHeight;  }
   }
   else {
      if (Self->ParentView) val = Self->ParentView->vpFixedHeight;
      else val = Self->Scene->PageHeight;
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
rendered graphics in the source area will be repositioned and scaled to the area defined by (X,Y) and (Width,Height).

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
rendered graphics in the source area will be repositioned and scaled to the area defined by (X,Y) and (Width,Height).

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
rendered graphics in the source area will be repositioned and scaled to the area defined by (X,Y) and (Width,Height).

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
   else {
      if (Self->ParentView) val = Self->ParentView->vpFixedWidth;
      else val = Self->Scene->PageWidth;
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

The display position targeted by the viewport is declared in the (X,Y) fields.

*****************************************************************************/

static ERROR VIEW_GET_X(objVectorViewport *Self, Variable *Value)
{
   DOUBLE val = Self->vpTargetX;
   if (Value->Type & FD_PERCENTAGE) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
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
Y: Positions the viewport on the y-axis.

The display position targeted by the viewport is declared in the (X,Y) fields.
-END-
*****************************************************************************/

static ERROR VIEW_GET_Y(objVectorViewport *Self, Variable *Value)
{
   DOUBLE val = Self->vpTargetY;
   if (Value->Type & FD_PERCENTAGE) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
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
   { "X",           FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)VIEW_GET_X,      (APTR)VIEW_SET_X },
   { "Y",           FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)VIEW_GET_Y,      (APTR)VIEW_SET_Y },
   { "Width",       FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)VIEW_GET_Width,  (APTR)VIEW_SET_Width },
   { "Height",      FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)VIEW_GET_Height, (APTR)VIEW_SET_Height },
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
