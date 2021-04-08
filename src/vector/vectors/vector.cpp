/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
Vector: An abstract class for supporting vector graphics objects and functionality.

Vector is an abstract class that is used as a blueprint for other vector classes that provide specific functionality
for a vector scene.  At this time the classes are @VectorClip, @VectorEllipse, @VectorGroup, @VectorPath,
@VectorPolygon, @VectorRectangle, @VectorSpiral, @VectorText, @VectorViewport and @VectorWave.

The majority of sub-classes support all of the functionality provided by Vector.  The general exception is that
graphics functions will not be supported by non-graphical classes, for instance @VectorGroup and @VectorViewport do not
produce a vector path and therefore cannot be rendered.

To simplify the creation of complex vector graphics and maximise compatibility, we have designed the vector management
code to use data structures that closely match SVG definitions.  For this reason we do not provide exhaustive
documentation on the properties that can be applied to each vector type.  Instead, please refer to the SVG reference
manuals from the W3C.  In cases where we are missing support for an SVG feature, we most likely intend to support that
feature unless otherwise documented.

-END-

*****************************************************************************/

static ERROR vector_input_events(objVector *, const InputEvent *);

static ERROR VECTOR_Reset(objVector *, APTR);

static ERROR VECTOR_ActionNotify(objVector *Self, struct acActionNotify *Args)
{
   if (Args->ActionID IS AC_Free) {
      if ((Self->ClipMask) and (Args->ObjectID IS Self->ClipMask->Head.UniqueID)) Self->ClipMask = NULL;
      else if ((Self->Morph) and (Args->ObjectID IS Self->Morph->Head.UniqueID)) Self->Morph = NULL;
      else if ((Self->Transition) and (Args->ObjectID IS Self->Transition->Head.UniqueID)) Self->Transition = NULL;
   }
   else return ERR_NoSupport;

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
ApplyMatrix: Applies a 3x2 transform matrix to the vector.

This method will apply a 3x2 transformation matrix to the vector.  If the matrix is preceded with the application of
other transforms, the outcome is that the matrix is multiplied with the combination of the former transforms.

-INPUT-
double A: Matrix value at (0,0)
double B: Matrix value at (1,0)
double C: Matrix value at (2,0)
double D: Matrix value at (0,1)
double E: Matrix value at (1,1)
double F: Matrix value at (2,1)

-ERRORS-
Okay
NullArgs
AllocMemory
-END-

*****************************************************************************/

static ERROR VECTOR_ApplyMatrix(objVector *Self, struct vecApplyMatrix *Args)
{
   if (!Args) return ERR_NullArgs;

   VectorTransform *transform;
   if ((transform = add_transform(Self, VTF_MATRIX, FALSE))) {
      transform->Matrix[0] = Args->A;
      transform->Matrix[1] = Args->B;
      transform->Matrix[2] = Args->C;
      transform->Matrix[3] = Args->D;
      transform->Matrix[4] = Args->E;
      transform->Matrix[5] = Args->F;
      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

/*****************************************************************************

-METHOD-
ClearTransforms: Clear all transform instructions currently associated with the vector.

This method will clear all transform instructions that have been applied to the vector.

-ERRORS-
Okay
-END-

*****************************************************************************/

static ERROR VECTOR_ClearTransforms(objVector *Self, APTR Void)
{
   parasol::Log log;

   log.traceBranch();

   if (Self->Transforms) {
      VectorTransform *next;
      for (auto scan=Self->Transforms; scan; scan=next) {
         next = scan->Next;
         FreeResource(scan);
      }
      Self->Transforms = NULL;
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Disable: Disabling a vector can be used to trigger style changes and prevent user input.
-END-
*****************************************************************************/

static ERROR VECTOR_Disable(objVector *Self, APTR Void)
{
   // It is up to the client to subscribe to the Disable action if any activity needs to take place.
   return ERR_Okay;
}
/*****************************************************************************
-ACTION-
Draw: Draws the surface associated with the vector.

Using the Draw action on a specific vector will redraw its area within the @Surface associated with the @VectorScene.  This is
the most optimal method of drawing if it can be assured that changes within the scene are limited to the target vector's boundary.

Support for restricting the drawing area is not provided and we recommend that no parameters are passed when calling this action.

-END-
*****************************************************************************/

static ERROR VECTOR_Draw(objVector *Self, struct acDraw *Args)
{
   if ((Self->Scene) and (Self->Scene->SurfaceID)) {
      if ((!Self->BasePath) or (Self->Dirty)) {
         gen_vector_path(Self);
         Self->Dirty = 0;
      }

      if (!Self->BasePath) return ERR_NoData;

      // Retrieve bounding box, post-transformations.
      // TODO: Needs to account for client defined brush stroke widths and stroke scaling.

      DOUBLE bx1, by1, bx2, by2;
      bounding_rect_single(*Self->BasePath, 0, &bx1, &by1, &bx2, &by2);

      if (Self->Head.SubID IS ID_VECTORTEXT) {
         bx1 += Self->FinalX;
         by1 += Self->FinalY;
         bx2 += Self->FinalX;
         by2 += Self->FinalY;
      }

      const LONG STROKE_WIDTH = 2;
      bx1 -= STROKE_WIDTH;
      by1 -= STROKE_WIDTH;
      bx2 += STROKE_WIDTH;
      by2 += STROKE_WIDTH;

      struct acDraw area = { .X = F2T(bx1), .Y = F2T(by1), .Width = F2T(bx2 - bx1), .Height = F2T(by2 - by1) };
      return ActionMsg(AC_Draw, Self->Scene->SurfaceID, &area);
   }
   else {
      parasol::Log log;
      return log.warning(ERR_FieldNotSet);
   }
}

/*****************************************************************************
-ACTION-
Enable: Reverses the effects of disabling the vector.
-END-
*****************************************************************************/

static ERROR VECTOR_Enable(objVector *Self, APTR Void)
{
  // It is up to the client to subscribe to the Enable action if any activity needs to take place.
  return ERR_Okay;
}

//****************************************************************************

static ERROR VECTOR_Free(objVector *Self, APTR Args)
{
   if (Self->ID)           { FreeResource(Self->ID); Self->ID = NULL; }
   if (Self->DashArray)    { FreeResource(Self->DashArray); Self->DashArray = NULL; }
   if (Self->FillString)   { FreeResource(Self->FillString); Self->FillString = NULL; }
   if (Self->StrokeString) { FreeResource(Self->StrokeString); Self->StrokeString = NULL; }
   if (Self->FilterString) { FreeResource(Self->FilterString); Self->FilterString = NULL; }

   if (Self->FillGradientTable) { delete Self->FillGradientTable; Self->FillGradientTable = NULL; }
   if (Self->StrokeGradientTable) { delete Self->StrokeGradientTable; Self->StrokeGradientTable = NULL; }

   VECTOR_ClearTransforms(Self, NULL);

   // Patch the nearest vectors that are linked to ours.
   if (Self->Next) Self->Next->Prev = Self->Prev;
   if (Self->Prev) Self->Prev->Next = Self->Next;
   if ((Self->Parent) and (!Self->Prev)) {
      if (Self->Parent->ClassID IS ID_VECTORSCENE) ((objVectorScene *)Self->Parent)->Viewport = Self->Next;
      else ((objVector *)Self->Parent)->Child = Self->Next;
   }
   if (Self->Child) Self->Child->Parent = NULL;

   if ((Self->Scene) and (Self->Scene->InputSubscriptions)) {
      Self->Scene->InputSubscriptions->erase(Self);
   }

   if ((Self->Scene) and (Self->Scene->KeyboardSubscriptions)) {
      Self->Scene->KeyboardSubscriptions->erase(Self);
   }

   delete Self->Transform;    Self->Transform = NULL;
   delete Self->BasePath;     Self->BasePath = NULL;
   delete Self->StrokeRaster; Self->StrokeRaster = NULL;
   delete Self->FillRaster;   Self->FillRaster = NULL;
   delete Self->InputSubscriptions; Self->InputSubscriptions = NULL;
   delete Self->KeyboardSubscriptions; Self->KeyboardSubscriptions = NULL;

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
GetBoundary: Returns the graphical boundary of a vector.

This function will return the boundary of a vector's path in terms of its top-left position, width and height.  All
transformations and position information that applies to the vector will be taken into account when computing the
boundary.

If the VBF_INCLUSIVE flag is used, the result will include an analysis of all paths that belong to children of the
target vector, including transforms.

If the VBF_NO_TRANSFORM flag is used, the transformation step is not applied to the vector's path.

It is recommended that this method is not called until at least one rendering pass has been made, as some vector
dimensions may not be computed before then.

-INPUT-
int(VBF) Flags: Optional flags.
&double X: The left-most position of the boundary is returned here.
&double Y: The top-most position of the boundary is returned here.
&double Width: The width of the boundary is returned here.
&double Height: The height of the boundary is returned here.

-ERRORS-
Okay
NullArgs
NoData: The vector does not have a computable path.
NotPossible: The vector does not support path generation.
-END-

*****************************************************************************/

static ERROR VECTOR_GetBoundary(objVector *Self, struct vecGetBoundary *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if (!Self->Scene) return log.warning(ERR_NotInitialised);

   if (Self->GeneratePath) { // Path generation must be supported by the vector.
      if ((!Self->BasePath) or (Self->Dirty)) {
         gen_vector_path(Self);
         Self->Dirty = 0;
      }

      if (Self->BasePath) {
         std::array<DOUBLE, 4> bounds = { DBL_MAX, DBL_MAX, -1000000, -1000000 };
         DOUBLE bx1, by1, bx2, by2;

         if (Args->Flags & VBF_NO_TRANSFORM) {
            bounding_rect_single(*Self->BasePath, 0, &bx1, &by1, &bx2, &by2);
            bounds[0] = bx1 + Self->FinalX;
            bounds[1] = by1 + Self->FinalY;
            bounds[2] = bx2 + Self->FinalX;
            bounds[3] = by2 + Self->FinalY;
         }
         else {
            agg::conv_transform<agg::path_storage, agg::trans_affine> path(*Self->BasePath, *Self->Transform);
            bounding_rect_single(path, 0, &bx1, &by1, &bx2, &by2);
            bounds[0] = bx1;
            bounds[1] = by1;
            bounds[2] = bx2;
            bounds[3] = by2;
         }

         if (Args->Flags & VBF_INCLUSIVE) {
            calc_full_boundary(Self->Child, bounds);
         }

         Args->X      = bounds[0];
         Args->Y      = bounds[1];
         Args->Width  = bounds[2] - bounds[0];
         Args->Height = bounds[3] - bounds[1];
         return ERR_Okay;
      }
      else return ERR_NoData;
   }
   else return ERR_NotPossible;
}

/*****************************************************************************

-METHOD-
GetTransform: Returns the values of applied transformation effects.

This method returns a VECTOR_TRANSFORM structure for any given transform that has been applied to a vector.  It works
for MATRIX, TRANSLATE, SCALE, ROTATE and SKEW transformations.  The structure of VECTOR_TRANSFORM is described in the
#Transforms field.

If the requested transform is not applied to the vector, the method will fail with an ERR_Search return code.

-INPUT-
int Type: Type of transform to retrieve.  If set to zero, the first transformation is returned.
&struct(*VectorTransform) Transform: A matching VECTOR_TRANSFORM structure is returned in this parameter, if found.

-ERRORS-
Okay:
NullArgs:
Search: The requested transform type is not applied.
NoData: No transformations are applied to the vector.
-END-

*****************************************************************************/

static ERROR VECTOR_GetTransform(objVector *Self, struct vecGetTransform *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if (Args->Type & Self->ActiveTransforms) {
      for (auto t=Self->Transforms; t; t=t->Next) {
         if (t->Type IS Args->Type) {
            Args->Transform = t;
            return ERR_Okay;
         }
      }
   }
   else if (!Args->Type) { // If no type specified, return the first transformation.
      if ((Args->Transform = Self->Transforms)) return ERR_Okay;
      else return ERR_NoData;
   }

   return ERR_Search;
}

/*****************************************************************************
-ACTION-
Hide: Changes the vector's visibility setting to hidden.
-END-
*****************************************************************************/

static ERROR VECTOR_Hide(objVector *Self, APTR Void)
{
   Self->Visibility = VIS_HIDDEN;
   return ERR_Okay;
}

//****************************************************************************
// Determine the parent object, based on the owner.

void set_parent(objVector *Self, OBJECTID OwnerID)
{
   // Objects that don't belong to the Vector class will be ignored (i.e. they won't appear in the tree).

   CLASSID class_id = GetClassID(OwnerID);
   if ((class_id != ID_VECTORSCENE) and (class_id != ID_VECTOR)) return;

   Self->Parent = (OBJECTPTR)GetObjectPtr(OwnerID);

   // Ensure that the sibling fields are valid, if not then clear them.

   if ((Self->Prev) and (Self->Prev->Parent != Self->Parent)) Self->Prev = NULL;
   if ((Self->Next) and (Self->Next->Parent != Self->Parent)) Self->Next = NULL;

   if (Self->Parent->ClassID IS ID_VECTOR) {
      if ((!Self->Prev) and (!Self->Next)) {
         if (((objVector *)Self->Parent)->Child) { // Insert at the end
            auto end = ((objVector *)Self->Parent)->Child;
            while (end->Next) end = end->Next;
            end->Next = Self;
            Self->Prev = end;
         }
         else ((objVector *)Self->Parent)->Child = Self;
      }

      Self->Scene = ((objVector *)Self->Parent)->Scene;
   }
   else if (Self->Parent->ClassID IS ID_VECTORSCENE) {
      if ((!Self->Prev) and (!Self->Next)) {
         if (((objVectorScene *)Self->Parent)->Viewport) { // Insert at the end
            auto end = ((objVectorScene *)Self->Parent)->Viewport;
            while (end->Next) end = end->Next;
            end->Next = Self;
            Self->Prev = end;
         }
         else ((objVectorScene *)Self->Parent)->Viewport = Self;
      }

      Self->Scene = (objVectorScene *)Self->Parent;
   }
}

//****************************************************************************

static ERROR VECTOR_Init(objVector *Self, APTR Void)
{
   parasol::Log log;

   if ((!Self->Head.SubID) or (Self->Head.SubID IS ID_VECTOR)) {
      log.warning("Vector cannot be instantiated directly (use a sub-class).");
      return ERR_Failed;
   }

   if (!Self->Parent) set_parent(Self, Self->Head.OwnerID);

   log.trace("Parent: #%d, Siblings: #%d #%d, Vector: %p", Self->Parent ? Self->Parent->UniqueID : 0,
      Self->Prev ? Self->Prev->Head.UniqueID : 0, Self->Next ? Self->Next->Head.UniqueID : 0, Self);

   OBJECTPTR parent;
   if ((parent = Self->Parent)) {
      if (parent->ClassID IS ID_VECTOR) {
         auto parent_shape = (objVector *)parent;
         Self->Scene = parent_shape->Scene;

         // Check if this object is already present in the parent's branch.
         auto scan = parent_shape->Child;
         while (scan) {
            if (scan IS Self) break;
            scan = (objVector *)scan->Next;
         }

         if (!scan) {
            Self->Prev = NULL;
            Self->Next = NULL;
            if (parent_shape->Child) {
               parent_shape->Child->Prev = Self;
               parent_shape->Child->Parent = NULL;
               Self->Next = parent_shape->Child;
            }
            parent_shape->Child = Self;
            Self->Parent = (OBJECTPTR)parent_shape;
         }
      }
      else if (parent->ClassID IS ID_VECTORSCENE) {
         Self->Scene = (objVectorScene *)parent;
      }
      else return log.warning(ERR_UnsupportedOwner);
   }
   else return log.warning(ERR_UnsupportedOwner);

   // Find the nearest parent viewport.

   OBJECTPTR scan = get_parent(Self);
   while (scan) {
      if (scan->SubID IS ID_VECTORVIEWPORT) {
         Self->ParentView = (objVectorViewport *)scan;
         break;
      }
      if (scan->ClassID IS ID_VECTOR) scan = ((objVector *)scan)->Parent;
      else break;
   }

   // Reapply the filter if it couldn't be set prior to initialisation.
   if ((!Self->Filter) and (Self->FilterString)) {
      SetString(Self, FID_Filter, Self->FilterString);
   }

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
InputSubscription: Create a subscription for input events that relate to the vector.

The InputSubscription method is provided as an extension to gfxSubscribeInput(), whereby the user's input events
will be filtered down to those that occur within the vector's graphics area only.  The original events are
transferred as-is, although the ENTERED_SURFACE and LEFT_SURFACE events are modified so that they trigger during
passage through the vector boundaries.

It is a pre-requisite that the associated @VectorScene has been linked to a @Surface.

To remove an existing subscription, call this function again with the same Callback and an empty Mask.
Alternatively have the function return ERR_Terminate.

Please refer to gfxSubscribeInput() for further information on event management and message handling.

-INPUT-
int(JTYPE) Mask: Combine JTYPE flags to define the input messages required by the client.  Set to 0xffffffff if all messages are desirable.
ptr(func) Callback: Reference to a callback function that will receive input messages.

-ERRORS-
Okay:
NullArgs:
FieldNotSet: The VectorScene has no reference to a Surface.
AllocMemory:
Function: A call to gfxSubscribeInput() failed.

*****************************************************************************/

static ERROR VECTOR_InputSubscription(objVector *Self, struct vecInputSubscription *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Callback)) return log.warning(ERR_NullArgs);

   if (!Self->Scene->SurfaceID) return log.warning(ERR_FieldNotSet);

   if (Args->Mask) {
      if (!Self->InputSubscriptions) {
         Self->InputSubscriptions = new (std::nothrow) std::vector<InputSubscription>;
         if (!Self->InputSubscriptions) return log.warning(ERR_AllocMemory);
      }

      LONG mask = Args->Mask;
      if (mask & JTYPE_FEEDBACK) mask |= JTYPE_MOVEMENT;

      Self->InputMask |= mask;
      Self->Scene->InputSubscriptions[0][Self] = Self->InputMask;
      Self->InputSubscriptions->emplace_back(*Args->Callback, mask);
      return ERR_Okay;
   }
   else { // Remove existing subscriptions for this callback
      for (auto it=Self->InputSubscriptions->begin(); it != Self->InputSubscriptions->end(); ) {
         if (*Args->Callback IS it->Callback) it = Self->InputSubscriptions->erase(it);
         else it++;
      }

      if (Self->InputSubscriptions->empty()) {
         Self->Scene->InputSubscriptions->erase(Self);
      }

      return ERR_Okay;
   }
}

/*****************************************************************************

-METHOD-
KeyboardSubscription: Create a subscription for input events that relate to the vector.

The KeyboardSubscription method is provided to simplify the handling of keyboard messages for the client.  It is a
pre-requisite that the associated @VectorScene has been linked to a @Surface.

A callback is required and this will receive input messages as they arrive from the user.  The prototype for the
callback is as follows, whereby Flags are keyboard qualifiers `KQ` and the Value will be a `K` constant.

```
ERROR callback(*Viewport, LONG Flags, LONG Value);
```

To remove the subscription the function can return ERR_Terminate.

-INPUT-
ptr(func) Callback: Reference to a callback function that will receive input messages.

-ERRORS-
Okay:
NullArgs:
FieldNotSet: The VectorScene has no reference to a Surface.
AllocMemory:
Function: A call to gfxSubscribeInput() failed.

*****************************************************************************/

static ERROR VECTOR_KeyboardSubscription(objVector *Self, struct vecKeyboardSubscription *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Callback)) return log.warning(ERR_NullArgs);

   if (!Self->Scene->SurfaceID) return log.warning(ERR_FieldNotSet);

   if (!Self->KeyboardSubscriptions) {
      Self->KeyboardSubscriptions = new (std::nothrow) std::vector<KeyboardSubscription>;
      if (!Self->KeyboardSubscriptions) return log.warning(ERR_AllocMemory);
   }

   Self->Scene->KeyboardSubscriptions[0].emplace(Self);
   Self->KeyboardSubscriptions->emplace_back(*Args->Callback);
   return ERR_Okay;
}

//****************************************************************************

static ERROR VECTOR_NewObject(objVector *Self, APTR Void)
{
   Self->StrokeOpacity = 1.0;
   Self->FillOpacity   = 1.0;
   Self->Opacity       = 1.0;              // Overall opacity multiplier
   Self->MiterLimit    = 4;                // SVG default is 4;
   Self->LineJoin      = agg::miter_join;  // SVG default is miter
   Self->LineCap       = agg::butt_cap;    // SVG default is butt
   Self->InnerJoin     = agg::inner_miter; // AGG only
   Self->NumericID     = 0x7fffffff;
   Self->StrokeWidth   = 1.0; // SVG default is 1, note that an actual stroke colour needs to be defined for this value to actually matter.
   Self->Visibility    = VIS_VISIBLE;
   Self->FillRule      = VFR_NON_ZERO;
   Self->ClipRule      = VFR_NON_ZERO;
   Self->Dirty         = RC_ALL;
   return ERR_Okay;
}

//****************************************************************************

static ERROR VECTOR_NewOwner(objVector *Self, struct acNewOwner *Args)
{
   parasol::Log log;

   if (!Self->Head.SubID) return ERR_Okay;

   // Modifying the owner after the root vector has been established is not permitted.
   // The client should instead create a new object under the target and transfer the field values.

   if (Self->Head.Flags & NF_INITIALISED) return log.warning(ERR_AlreadyDefined);

   set_parent(Self, Args->NewOwnerID);

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
PointInPath: Checks if point at (X,Y) is within a vector's path.

This method provides an accurate means of determining if a specific coordinate is inside the path of a vector.  It is
important to note that in some cases this operation may be computationally expensive, as each pixel normally drawn in
the path may need to be calculated until the (X,Y) point is hit.

-INPUT-
double X: The X coordinate of the point.
double Y: The Y coordinate of the point.

-ERRORS-
Okay: The point is in the path.
False: The point is not in the path.
NullArgs:
NoData: The vector is unable to generate a path based on its current values.
NoSupport: The vector type does not support path generation.

*****************************************************************************/

static ERROR VECTOR_PointInPath(objVector *Self, struct vecPointInPath *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if (Self->GeneratePath) {
      if ((!Self->BasePath) or (Self->Dirty)) {
         gen_vector_path(Self);
         Self->Dirty = 0;
      }

      if (!Self->BasePath) return ERR_NoData;

      // Quick check to see if (X,Y) is within the path's boundary.

      agg::conv_transform<agg::path_storage, agg::trans_affine> base_path(*Self->BasePath, *Self->Transform);

      DOUBLE bx1, by1, bx2, by2;
      bounding_rect_single(base_path, 0, &bx1, &by1, &bx2, &by2);
      if ((Args->X >= bx1) and (Args->Y >= by1) and (Args->X < bx2) and (Args->Y < by2)) {
         // Do the hit testing.

         agg::rasterizer_scanline_aa<> raster;
         raster.add_path(base_path);
         if (raster.hit_test(Args->X, Args->Y)) return ERR_Okay;
      }

      return ERR_False;
   }
   else return ERR_NoSupport;
}

/*****************************************************************************

-METHOD-
Push: Push a vector to a new position within its area of the vector stack.

This method moves the position of a vector within its branch of the vector stack.  Repositioning is relative
to the current position of the vector.  Every unit specified in the Position parameter will move the vector by one
index in the stack frame.  Negative values will move the vector backwards; positive values move it forward.

It is not possible for an vector to move outside of its branch, i.e. it cannot change its parent.  If the vector
reaches the edge of its branch with excess units remaining, the method will return immediately with an ERR_Okay error
code.

-INPUT-
int Position: Specify a relative position index here (-ve to move backwards, +ve to move forwards)

-ERRORS-
Okay:
NullArgs:

*****************************************************************************/

static ERROR VECTOR_Push(objVector *Self, struct vecPush *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if (Args->Position < 0) { // Move backward through the stack.
      if (!Self->Prev) return ERR_Okay; // Return if the vector is at the top of its branch

      LONG i = -Args->Position;
      Self->Prev->Next = Self->Next;
      if (Self->Next) Self->Next->Prev = Self->Prev;
      auto scan = Self;
      while ((i > 0) and (scan->Prev)) {
         scan = scan->Prev;
         i--;
      }
      Self->Next = scan;
      Self->Prev = scan->Prev;
      if (!Self->Prev) {
         if (scan->Parent->ClassID IS ID_VECTOR) ((objVector *)scan->Parent)->Child = Self;
         else if (scan->Parent->ClassID IS ID_VECTORSCENE) ((objVectorScene *)scan->Parent)->Viewport = Self;
         Self->Parent = scan->Parent;
      }
   }
   else if (Args->Position > 0) { // Move forward through the stack.
      if (!Self->Next) return ERR_Okay;

      LONG i = Args->Position;
      if (Self->Prev) Self->Prev->Next = Self->Next;
      Self->Next->Prev = Self->Prev;
      objVector *scan = Self;
      while ((i > 0) and (scan->Next)) {
         scan = scan->Next;
         i--;
      }
      if ((!Self->Prev) and (scan != Self)) {
         if (Self->Parent->ClassID IS ID_VECTOR) ((objVector *)Self->Parent)->Child = Self->Next;
         else if (Self->Parent->ClassID IS ID_VECTORSCENE) ((objVectorScene *)Self->Parent)->Viewport = Self->Next;
      }
      Self->Prev = scan;
      Self->Next = scan->Next;
      scan->Next = Self;
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Reset: Clears all transform settings from the vector.
-END-
*****************************************************************************/

static ERROR VECTOR_Reset(objVector *Self, APTR Void)
{
   Self->ActiveTransforms = 0;
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
Rotate: Applies a rotation transformation to the vector.

This method will apply a rotation transformation to a vector.  The rotation will be computed on a run-time
basis and does not affect the path stored with the vector.  Any children associated with the vector will also be
affected by the transformation.

If a rotation already exists for the vector, it will be replaced with the new specifications.

The transformation can be removed at any time by calling the #ClearTransforms() method.

-INPUT-
double Angle: Angle of rotation
double CenterX: Center of rotation on the horizontal axis.
double CenterY: Center of rotation on the vertical axis.

-ERRORS-
Okay:
NullArgs:
AllocMemory:

****************************************************************************/

static ERROR VECTOR_Rotate(objVector *Self, struct vecRotate *Args)
{
   if (!Args) return ERR_NullArgs;

   VectorTransform *t;
   if ((t = add_transform(Self, VTF_ROTATE, FALSE))) {
      t->Angle = Args->Angle;
      t->X = Args->CenterX;
      t->Y = Args->CenterY;
      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

/*****************************************************************************

-METHOD-
Scale: Scale the size of the vector by (x,y)

This method will add a scale transformation to the vector's transform commands.  Values of less than 1.0 will shrink
the path along the target axis, while values greater than 1.0 will enlarge it.

The scale factors are applied to every path point, and scaling is relative to position (0,0).  If the width and height
of the vector shape needs to be transformed without affecting its top-left position, the client must translate the
vector to (0,0) around its center point.  The vector should then be scaled and transformed back to its original
top-left coordinate.

The scale transform can also be formed to flip the vector path if negative values are used.  For instance, a value of
-1.0 on the x axis would result in a 1:1 flip across the horizontal.

-INPUT-
double X: The scale factor on the x-axis.
double Y: The scale factor on the y-axis.

-ERRORS-
Okay
NullArgs
AllocMemory
-END-

*****************************************************************************/

static ERROR VECTOR_Scale(objVector *Self, struct vecScale *Args)
{
   if (!Args) return ERR_NullArgs;

   VectorTransform *t;
   if ((t = add_transform(Self, VTF_SKEW, FALSE))) {
      t->X = Args->X;
      t->Y = Args->Y;
      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

/*****************************************************************************
-ACTION-
Show: Changes the vector's visibility setting to visible.
-END-
*****************************************************************************/

static ERROR VECTOR_Show(objVector *Self, APTR Void)
{
   Self->Visibility = VIS_VISIBLE;
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
Skew: Skews the vector along the horizontal and/or vertical axis.

The Skew method applies a skew transformation to the horizontal and/or vertical axis of the vector and its children.
Valid X and Y values are in the range of -90 < Angle < 90.

-INPUT-
double X: The angle to skew along the horizontal.
double Y: The angle to skew along the vertical.

-ERRORS-
Okay:
NullArgs:
OutOfRange: At least one of the angles is out of the allowable range.
-END-

*****************************************************************************/

static ERROR VECTOR_Skew(objVector *Self, struct vecSkew *Args)
{
   parasol::Log log;

   if ((!Args) or ((!Args->X) and (!Args->Y))) return log.warning(ERR_NullArgs);

   VectorTransform *transform;
   if ((transform = add_transform(Self, VTF_SKEW, FALSE))) {
      if ((Args->X > -90) and (Args->X < 90)) {
         transform->X = Args->X;
      }
      else return log.warning(ERR_OutOfRange);

      if ((Args->Y > -90) and (Args->Y < 90)) transform->Y = Args->Y;
      else return log.warning(ERR_OutOfRange);

      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

/*****************************************************************************

-METHOD-
TracePath: Returns the coordinates for a vector path, using callbacks.

Any vector that generates a path can be traced by calling this method.  Tracing allows the caller to follow the path for
each pixel that would be drawn if the path were to be rendered with a stroke size of 1.  The prototype of the callback
function is `ERROR Function(OBJECTPTR Vector, LONG Index, LONG Command, DOUBLE X, DOUBLE Y)`.

The Vector parameter refers to the vector targeted by the method.  The Index is an incrementing counter that reflects
the currently plotted point.  The X and Y parameters reflect the coordinate of a point on the path.

If the Callback returns ERR_Terminate, then no further coordinates will be processed.

-INPUT-
ptr(func) Callback: The function to call with each coordinate of the path.

-ERRORS-
Okay:
NullArgs:

****************************************************************************/

static ERROR VECTOR_TracePath(objVector *Self, struct vecTracePath *Args)
{
   parasol::Log log;

   if ((!Args) or (Args->Callback)) return log.warning(ERR_NullArgs);

   if (Self->Dirty) {
      gen_vector_path(Self);
      Self->Dirty = 0;
   }

   if (Self->BasePath) {
      Self->BasePath->rewind(0);

      DOUBLE x, y;
      LONG cmd = -1;

     if (Args->Callback->Type IS CALL_STDC) {
         auto routine = ((void (*)(objVector *, LONG, LONG, DOUBLE, DOUBLE))(Args->Callback->StdC.Routine));

         parasol::SwitchContext context(GetParentContext());

         LONG index = 0;
         do {
           cmd = Self->BasePath->vertex(&x, &y);
           if (agg::is_vertex(cmd)) routine(Self, index++, cmd, x, y);
         } while (cmd != agg::path_cmd_stop);
      }
      else if (Args->Callback->Type IS CALL_SCRIPT) {
         ScriptArg args[] = {
            { "Vector",  FD_OBJECTID },
            { "Index",   FD_LONG },
            { "Command", FD_LONG },
            { "X",       FD_DOUBLE },
            { "Y",       FD_DOUBLE }
         };
         args[0].Long = Self->Head.UniqueID;

         OBJECTPTR script;
         if ((script = Args->Callback->Script.Script)) {
            LONG index = 0;
            do {
              cmd = Self->BasePath->vertex(&x, &y);
              if (agg::is_vertex(cmd)) {
                 args[1].Long = index++;
                 args[2].Long = cmd;
                 args[3].Double = x;
                 args[4].Double = y;
                 scCallback(script, Args->Callback->Script.ProcedureID, args, ARRAYSIZE(args), NULL);
              }
            } while (cmd != agg::path_cmd_stop);
         }
      }

      return ERR_Okay;
   }
   else return ERR_NoData;
}

/*****************************************************************************

-METHOD-
Transform: Apply a transformation to a vector.

This method parses a sequence of transformation instructions and applies them to the vector.  The transformation will
be computed on a run-time basis and does not affect the path stored with the vector.  Any children associated with the
vector will be affected by the transformation.

The transform string must be written using SVG guidelines for the transform attribute, for example
`skewX(20) rotate(45 50 50)` would be valid.

Any existing transformation instructions for the vector will be replaced by this operation.

The transformation can be removed at any time by calling the #ClearTransforms() method.

-INPUT-
cstr Transform: The transform to apply, expressed as a string instruction.

-ERRORS-
Okay:
NullArgs:
AllocMemory:
-END-

*****************************************************************************/

static ERROR VECTOR_Transform(objVector *Self, struct vecTransform *Args)
{
   if ((!Args) or (!Args->Transform)) return ERR_NullArgs;

   VECTOR_ClearTransforms(Self, NULL);

   VectorTransform *transform;

   CSTRING str = Args->Transform;
   while (*str) {
      if (!StrCompare(str, "matrix", 6, 0)) {
         if ((transform = add_transform(Self, VTF_MATRIX, FALSE))) {
            str = read_numseq(str+6, &transform->Matrix[0], &transform->Matrix[1], &transform->Matrix[2], &transform->Matrix[3], &transform->Matrix[4], &transform->Matrix[5], TAGEND);
         }
         else return ERR_AllocMemory;
      }
      else if (!StrCompare(str, "translate", 9, 0)) {
         if ((transform = add_transform(Self, VTF_TRANSLATE, FALSE))) {
            DOUBLE x = 0;
            DOUBLE y = 0;
            str = read_numseq(str+9, &x, &y, TAGEND);
            transform->X += x;
            transform->Y += y;
         }
         else return ERR_AllocMemory;
      }
      else if (!StrCompare(str, "rotate", 6, 0)) {
         if ((transform = add_transform(Self, VTF_ROTATE, FALSE))) {
            str = read_numseq(str+6, &transform->Angle, &transform->X, &transform->Y, TAGEND);
         }
         else return ERR_AllocMemory;
      }
      else if (!StrCompare(str, "scale", 5, 0)) {
         if ((transform = add_transform(Self, VTF_SCALE, FALSE))) {
            str = read_numseq(str+5, &transform->X, &transform->Y, TAGEND);
         }
         else return ERR_AllocMemory;
      }
      else if (!StrCompare(str, "skewX", 5, 0)) {
         if ((transform = add_transform(Self, VTF_SKEW, FALSE))) {
            transform->X = 0;
            str = read_numseq(str+5, &transform->X, TAGEND);
         }
         else return ERR_AllocMemory;
      }
      else if (!StrCompare(str, "skewY", 5, 0)) {
         if ((transform = add_transform(Self, VTF_SKEW, FALSE))) {
            transform->Y = 0;
            str = read_numseq(str+5, &transform->Y, TAGEND);
         }
         else return ERR_AllocMemory;
      }
      else str++;
   }

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
Translate: Translates the vector by (X,Y).

This method will apply a translation along (X,Y) to the vector's transform command sequence.

-INPUT-
double X: Translation along the x-axis.
double Y: Translation along the y-axis.

-ERRORS-
Okay:
NullArgs:
AllocMemory:
-END-

*****************************************************************************/

static ERROR VECTOR_Translate(objVector *Self, struct vecTranslate *Args)
{
   if (!Args) return ERR_NullArgs;

   VectorTransform *transform;
   if ((transform = add_transform(Self, VTF_TRANSLATE, FALSE))) {
      transform->X = Args->X;
      transform->Y = Args->Y;
      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

/*****************************************************************************

-FIELD-
ActiveTransforms: Indicates the transforms that are currently applied to a vector.

Each time that a transform is applied to a vector through methods such as #Scale() and #Translate(), a
flag will be set in ActiveTransforms that indicates the type of transform that was applied.

-FIELD-
Child: The first child vector, or NULL.

The Child value refers to the first vector that forms a branch under this object.  This field cannot be
set directly as it is managed internally.  Instead, use object ownership when a vector needs to be associated with a
new parent.

-FIELD-
ClipRule:  Determines the algorithm to use when clipping the shape.
Lookup: VFR

The ClipRule attribute only applies to vector shapes when they are contained within a @VectorClip object.  In
terms of outcome, the ClipRule works similarly to #FillRule.

*****************************************************************************/

static ERROR VECTOR_GET_ClipRule(objVector *Self, LONG *Value)
{
   *Value = Self->ClipRule;
   return ERR_Okay;
}

static ERROR VECTOR_SET_ClipRule(objVector *Self, LONG Value)
{
   Self->ClipRule = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
DashArray: Controls the pattern of dashes and gaps used to stroke paths.

The DashArray is a list of lengths that alternate between dashes and gaps.  If an odd number of values is provided,
then the list of values is repeated to yield an even number of values.  Thus `5,3,2` is equivalent to
`5,3,2,5,3,2`.

*****************************************************************************/

static ERROR VECTOR_GET_DashArray(objVector *Self, DOUBLE **Value, LONG *Elements)
{
   *Value = Self->DashArray;
   *Elements = Self->DashTotal;
   return ERR_Okay;
}

static ERROR VECTOR_SET_DashArray(objVector *Self, DOUBLE *Value, LONG Elements)
{
   if (Self->DashArray) { FreeResource(Self->DashArray); Self->DashArray = NULL; Self->DashTotal = 0; }

   if ((Value) and (Elements >= 2)) {
      LONG total = Elements;
      if (total & 1) total++; // There must be an even count of dashes and gaps.
      if (!AllocMemory(sizeof(DOUBLE) * total, MEM_DATA|MEM_NO_CLEAR, &Self->DashArray, NULL)) {
         CopyMemory(Value, Self->DashArray, sizeof(DOUBLE) * Elements);
         if (total > Elements) Self->DashArray[Elements] = 0;
         Self->DashTotal = total;
      }
      else return ERR_AllocMemory;
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
DashOffset: The distance into the dash pattern to start the dash.  Can be a negative number.

-FIELD-
DashTotal: The total number of values in the #DashArray.

-FIELD-
EnableBkgd: If true, allows filters to use BackgroundImage and BackgroundAlpha source types.

The EnableBkgd option must be set to true if a section of the vector tree uses filters that have 'BackgroundImage' or
'BackgroundAlpha' as a source.  If it is not set, then filters using BackgroundImage and BackgroundAlpha references
will not produce the expected behaviour.

The EnableBkgd option can be enabled on Vector sub-classes @VectorGroup, @VectorPattern and @VectorViewport.  All other
sub-classes will ignore the option if used.
-END-

SVG expects support for 'a', 'defs', 'glyph', 'g', 'marker', 'mask', 'missing-glyph', 'pattern', 'svg', 'switch' and 'symbol'.

*****************************************************************************/

static ERROR VECTOR_GET_EnableBkgd(objVector *Self, LONG *Value)
{
   *Value = Self->EnableBkgd;
   return ERR_Okay;
}

static ERROR VECTOR_SET_EnableBkgd(objVector *Self, LONG Value)
{
   if (Value) Self->EnableBkgd = TRUE;
   else Self->EnableBkgd = FALSE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Fill: Defines the fill painter using SVG's IRI format.

The painter used for filling a vector path can be defined through this field.  The string is parsed through the
~ReadPainter() function in the Vector module.  Please refer to it for further details on valid formatting.

*****************************************************************************/

static ERROR VECTOR_GET_Fill(objVector *Self, CSTRING *Value)
{
   *Value = Self->FillString;
   return ERR_Okay;
}

static ERROR VECTOR_SET_Fill(objVector *Self, CSTRING Value)
{
   if (Self->FillString) { FreeResource(Self->FillString); Self->FillString = NULL; }
   Self->FillString = StrClone(Value);
   vecReadPainter(&Self->Head, Value, &Self->FillColour, &Self->FillGradient, &Self->FillImage, &Self->FillPattern);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
FillColour: Defines a solid colour for filling the vector path.

Set the FillColour field to define a solid colour for filling the vector path.  The colour is defined as an array
of four 32-bit floating point values between 0 and 1.0.  The array elements consist of Red, Green, Blue and Alpha
values in that order.

If the Alpha component is set to zero then the FillColour will be ignored by the renderer.

*****************************************************************************/

static ERROR VECTOR_GET_FillColour(objVector *Self, FLOAT **Value, LONG *Elements)
{
   *Value = (FLOAT *)&Self->FillColour;
   *Elements = 4;
   return ERR_Okay;
}

static ERROR VECTOR_SET_FillColour(objVector *Self, FLOAT *Value, LONG Elements)
{
   if (Value) {
      if (Elements >= 1) Self->FillColour.Red   = Value[0];
      if (Elements >= 2) Self->FillColour.Green = Value[1];
      if (Elements >= 3) Self->FillColour.Blue  = Value[2];
      if (Elements >= 4) Self->FillColour.Alpha = Value[3];
      else Self->FillColour.Alpha = 1;
   }
   else Self->FillColour.Alpha = 0;

   if (Self->FillString) { FreeResource(Self->FillString); Self->FillString = NULL; }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
FillOpacity: The opacity to use when filling the vector.

The FillOpacity value is used by the painting algorithm when it is rendering a filled vector.  It is multiplied with
the #Opacity to determine a final opacity value for the render.

*****************************************************************************/

static ERROR VECTOR_GET_FillOpacity(objVector *Self, DOUBLE *Value)
{
   *Value = Self->FillOpacity;
   return ERR_Okay;
}

static ERROR VECTOR_SET_FillOpacity(objVector *Self, DOUBLE Value)
{
   parasol::Log log;

   if ((Value >= 0) and (Value <= 1.0)) {
      Self->FillOpacity = Value;
      return ERR_Okay;
   }
   else return log.warning(ERR_OutOfRange);
}

/*****************************************************************************

-FIELD-
Filter: Assign a post-effects filter to a vector.

This field assigns a graphics filter to the rendering pipeline of the vector.  The filter must initially be created
using the @VectorFilter class and added to a VectorScene using @VectorScene.AddDef().  The filter can
then be referenced by ID in the Filter field of any vector object.  Please refer to the @VectorFilter class
for further details on filter configuration.

The Filter value can be in the format `ID` or `url(#ID)` according to client preference.

*****************************************************************************/

static ERROR VECTOR_GET_Filter(objVector *Self, CSTRING *Value)
{
   *Value = Self->FilterString;
   return ERR_Okay;
}

static ERROR VECTOR_SET_Filter(objVector *Self, CSTRING Value)
{
   parasol::Log log;

   if ((!Value) or (!Value[0])) {
      if (Self->FilterString) { FreeResource(Self->FilterString); Self->FilterString = NULL; }
      Self->Filter = NULL;
      return ERR_Okay;
   }

   if (!Self->Scene) { // Vector is not yet initialised, so store the filter string for later.
      if (Self->FilterString) { FreeResource(Self->FilterString); Self->FilterString = NULL; }
      Self->FilterString = StrClone(Value);
      return ERR_Okay;
   }

   VectorDef *def = NULL;
   if (!StrCompare("url(#", Value, 5, 0)) {
      CSTRING str = Value + 5;
      char name[80];
      LONG i;
      for (i=0; (str[i] != ')') and (str[i]) and ((size_t)i < sizeof(name)-1); i++) name[i] = str[i];
      name[i] = 0;
      VarGet(Self->Scene->Defs, name, &def, NULL);
   }
   else VarGet(Self->Scene->Defs, Value, &def, NULL);

   if (!def) return log.warning(ERR_Search);

   if (def->Object->ClassID IS ID_VECTORFILTER) {
      if (Self->FilterString) { FreeResource(Self->FilterString); Self->FilterString = NULL; }
      Self->FilterString = StrClone(Value);
      Self->Filter = (objVectorFilter *)def->Object;
      return ERR_Okay;
   }
   else return log.warning(ERR_InvalidValue);
}

/*****************************************************************************
-FIELD-
FillRule: Determines the algorithm to use when filling the shape.

The FillRule field indicates the algorithm which is to be used to determine what parts of the canvas are included
when filling the shape. For a simple, non-intersecting path, it is intuitively clear what region lies "inside";
however, for a more complex path, such as a path that intersects itself or where one sub-path encloses another, the
interpretation of "inside" is not so obvious.

*****************************************************************************/

static ERROR VECTOR_GET_FillRule(objVector *Self, LONG *Value)
{
   *Value = Self->FillRule;
   return ERR_Okay;
}

static ERROR VECTOR_SET_FillRule(objVector *Self, LONG Value)
{
   Self->FillRule = Value;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
ID: String identifier for a vector.

The ID field is provided for the purpose of SVG support.  Where possible we would recommend that you use the
existing object name and automatically assigned ID's for identifiers.

*****************************************************************************/

static ERROR VECTOR_GET_ID(objVector *Self, STRING *Value)
{
   *Value = Self->ID;
   return ERR_Okay;
}

static ERROR VECTOR_SET_ID(objVector *Self, CSTRING Value)
{
   if (Self->ID) FreeResource(Self->ID);

   if (Value) {
      Self->ID = StrClone(Value);
      Self->NumericID = StrHash(Value, TRUE);
   }
   else {
      Self->ID = NULL;
      Self->NumericID = 0;
   }
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
InnerJoin: Adjusts the handling of thickly stroked paths that cross back at the join.
Lookup: VIJ

The InnerJoin value is used to make very technical adjustments to the way that paths are stroked when they form
corners.  Visually, the impact of this setting is only noticeable when a path forms an awkward corner that crosses
over itself - usually due to the placement of bezier control points.

The available settings are MITER, ROUND, BEVEL, JAG and INHERIT.  The default of MITER is recommended as it is the
fastest, but ROUND produces the best results in ensuring that the stroked path is filled correctly.  The most optimal
approach is to use the default setting and switch to ROUND if issues are noted near the corners of the path.

*****************************************************************************/

// See the AGG bezier_div demo to get a better understanding of what is affected by this field value.

static ERROR VECTOR_GET_InnerJoin(objVector *Self, LONG *Value)
{
   if (Self->InnerJoin IS agg::inner_miter)      *Value = VIJ_MITER;
   else if (Self->InnerJoin IS agg::inner_round) *Value = VIJ_ROUND;
   else if (Self->InnerJoin IS agg::inner_bevel) *Value = VIJ_BEVEL;
   else if (Self->InnerJoin IS agg::inner_jag)   *Value = VIJ_JAG;
   else if (Self->InnerJoin IS agg::inner_inherit) *Value = VIJ_INHERIT;
   else *Value = 0;
   return ERR_Okay;
}

static ERROR VECTOR_SET_InnerJoin(objVector *Self, LONG Value)
{
   switch(Value) {
      case VIJ_MITER: Self->InnerJoin = agg::inner_miter; break;
      case VIJ_ROUND: Self->InnerJoin = agg::inner_round; break;
      case VIJ_BEVEL: Self->InnerJoin = agg::inner_bevel; break;
      case VIJ_JAG:   Self->InnerJoin = agg::inner_jag; break;
      case VIJ_INHERIT: Self->InnerJoin = agg::inner_inherit; break;
      default: return ERR_Failed;
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
InnerMiterLimit: Private. No internal documentation exists for this feature.

-FIELD-
LineCap: The shape to be used at the start and end of a stroked path.
Lookup: VLC

LineCap is the equivalent of SVG's stroke-linecap attribute.  It defines the shape to be used at the start and end
of a stroked path.

*****************************************************************************/

static ERROR VECTOR_GET_LineCap(objVector *Self, LONG *Value)
{
   if (Self->LineCap IS agg::butt_cap)         *Value = VLC_BUTT;
   else if (Self->LineCap IS agg::square_cap)  *Value = VLC_SQUARE;
   else if (Self->LineCap IS agg::round_cap)   *Value = VLC_ROUND;
   else if (Self->LineCap IS agg::inherit_cap) *Value = VLC_INHERIT;
   else *Value = 0;
   return ERR_Okay;
}

static ERROR VECTOR_SET_LineCap(objVector *Self, LONG Value)
{
   switch(Value) {
      case VLC_BUTT:    Self->LineCap = agg::butt_cap; break;
      case VLC_SQUARE:  Self->LineCap = agg::square_cap; break;
      case VLC_ROUND:   Self->LineCap = agg::round_cap; break;
      case VLC_INHERIT: Self->LineCap = agg::inherit_cap; break;
      default: return ERR_Failed;
   }
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
LineJoin: The shape to be used at path corners that are stroked.
Lookup: VLJ

LineJoin is the equivalent of SVG's stroke-linejoin attribute.  It defines the shape to be used at path corners
that are being stroked.

*****************************************************************************/

static ERROR VECTOR_GET_LineJoin(objVector *Self, LONG *Value)
{
   if (Self->LineJoin IS agg::miter_join)        *Value = VLJ_MITER;
   else if (Self->LineJoin IS agg::round_join)   *Value = VLJ_ROUND;
   else if (Self->LineJoin IS agg::bevel_join)   *Value = VLJ_BEVEL;
   else if (Self->LineJoin IS agg::inherit_join) *Value = VLJ_INHERIT;
   else if (Self->LineJoin IS agg::miter_join_revert) *Value = VLJ_MITER_REVERT;
   else if (Self->LineJoin IS agg::miter_join_round)  *Value = VLJ_MITER_ROUND;
   else *Value = 0;

   return ERR_Okay;
}

static ERROR VECTOR_SET_LineJoin(objVector *Self, LONG Value)
{
   switch (Value) {
      case VLJ_MITER:        Self->LineJoin = agg::miter_join; break;
      case VLJ_ROUND:        Self->LineJoin = agg::round_join; break;
      case VLJ_BEVEL:        Self->LineJoin = agg::bevel_join; break;
      case VLJ_MITER_REVERT: Self->LineJoin = agg::miter_join_revert; break;
      case VLJ_MITER_ROUND:  Self->LineJoin = agg::miter_join_round; break;
      case VLJ_INHERIT:      Self->LineJoin = agg::inherit_join; break;
      default: return ERR_Failed;
   }
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Mask: Reference a VectorClip object here to apply a clipping mask to the rendered vector.

A mask can be applied to a vector by setting the Mask field with a reference to a @VectorClip object.  Please
refer to the @VectorClip class for further information.

*****************************************************************************/

static ERROR VECTOR_GET_Mask(objVector *Self, objVectorClip **Value)
{
   *Value = Self->ClipMask;
   return ERR_Okay;
}

static ERROR VECTOR_SET_Mask(objVector *Self, objVectorClip *Value)
{
   parasol::Log log;

   if (!Value) {
      if (Self->ClipMask) {
         UnsubscribeAction(Self->ClipMask, AC_Free);
         Self->ClipMask = NULL;
      }
      return ERR_Okay;
   }
   else if (Value->Head.SubID IS ID_VECTORCLIP) {
      if (Self->ClipMask) UnsubscribeAction(Self->ClipMask, AC_Free);
      if (Value->Head.Flags & NF_INITIALISED) { // Ensure that the mask is initialised.
         SubscribeAction(Value, AC_Free);
         Self->ClipMask = Value;
         return ERR_Okay;
      }
      else return log.warning(ERR_NotInitialised);
   }
   else return log.warning(ERR_InvalidObject);
}

/*****************************************************************************

-FIELD-
MiterLimit: Imposes a limit on the ratio of the miter length to the StrokeWidth.

When two line segments meet at a sharp angle and miter joins have been specified in #LineJoin, it is possible
for the miter to extend far beyond the thickness of the line stroking the path. The MiterLimit imposes a limit
on the ratio of the miter length to the #StrokeWidth. When the limit is exceeded, the join is converted from a
miter to a bevel.

The ratio of miter length (distance between the outer tip and the inner corner of the miter) to #StrokeWidth
is directly related to the angle (theta) between the segments in user space by the formula:
`MiterLength / StrokeWidth = 1 / sin ( theta / 2 )`.

For example, a miter limit of 1.414 converts miters to bevels for theta less than 90 degrees, a limit of 4.0 converts
them for theta less than approximately 29 degrees, and a limit of 10.0 converts them for theta less than approximately
11.5 degrees.

*****************************************************************************/

static ERROR VECTOR_SET_MiterLimit(objVector *Self, DOUBLE Value)
{
   parasol::Log log;

   if (Value >= 1.0) {
      Self->MiterLimit = Value;
      return ERR_Okay;
   }
   else return log.warning(ERR_InvalidValue);
}

/*****************************************************************************
-FIELD-
Morph: Enables morphing of the vector to a target path.

If the Morph field is set to a Vector object that generates a path, the vector will be morphed to follow the target
vector's path shape.  This works particularly well for text and shapes that follow a horizontal path that is much wider
than it is tall.

Squat shapes will fare poorly if morphed, so experimentation may be necessary to understand how the morph feature is
best utilised.

*****************************************************************************/

static ERROR VECTOR_GET_Morph(objVector *Self, objVector **Value)
{
   *Value = Self->Morph;
   return ERR_Okay;
}

static ERROR VECTOR_SET_Morph(objVector *Self, objVector *Value)
{
   parasol::Log log;

   if (!Value) {
      if (Self->Morph) {
         UnsubscribeAction(Self->Morph, AC_Free);
         Self->Morph = NULL;
      }
      return ERR_Okay;
   }
   else if (Value->Head.ClassID IS ID_VECTOR) {
      if (Self->Morph) UnsubscribeAction(Self->Morph, AC_Free);
      if (Value->Head.Flags & NF_INITIALISED) { // The object must be initialised.
         SubscribeAction(Value, AC_Free);
         Self->Morph = Value;
         return ERR_Okay;
      }
      else return log.warning(ERR_NotInitialised);
   }
   else return log.warning(ERR_InvalidObject);
}

/*****************************************************************************

-FIELD-
MorphFlags: Optional flags that affect morphing.

*****************************************************************************/

static ERROR VECTOR_GET_MorphFlags(objVector *Self, LONG *Value)
{
   *Value = Self->MorphFlags;
   return ERR_Okay;
}

static ERROR VECTOR_SET_MorphFlags(objVector *Self, LONG Value)
{
    Self->MorphFlags = Value;
    return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Next: The next vector in the branch, or NULL.

The Next value refers to the next vector in the branch.  If the value is NULL, the vector is positioned at the end of
the branch.

The Next value can be set to another vector at any time, on the condition that both vectors share the same owner.  If
this is not true, change the current owner before setting the Next field.  Changing the Next value will result in
updates to the #Parent and #Prev fields.

-ERRORS-
InvalidObject: The value is not a member of the Vector class.
InvalidValue: The provided value is either NULL or refers to itself.
UnsupportedOwner: The referenced vector does not share the same owner.

*****************************************************************************/

static ERROR VECTOR_SET_Next(objVector *Self, objVector *Value)
{
   parasol::Log log;

   if (Value->Head.ClassID != ID_VECTOR) return log.warning(ERR_InvalidObject);
   if ((!Value) or (Value IS Self)) return log.warning(ERR_InvalidValue);
   if (Self->Head.OwnerID != Value->Head.OwnerID) return log.warning(ERR_UnsupportedOwner); // Owners must match

   if (Self->Next) Self->Next->Prev = NULL; // Detach from the current Next object.
   if (Self->Prev) Self->Prev->Next = NULL; // Detach from the current Prev object.

   Self->Next  = Value; // Patch the chain
   Value->Prev = Self;
   Self->Prev  = Value->Prev;
   if (Value->Prev) Value->Prev->Next = Self;

   if (Value->Parent) { // Patch into the parent if we are at the start of the branch
      Self->Parent = Value->Parent;
      if (Self->Parent->ClassID IS ID_VECTORSCENE) ((objVectorScene *)Self->Parent)->Viewport = Self;
      else if (Self->Parent->ClassID IS ID_VECTOR) ((objVector *)Self->Parent)->Child = Self;
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
NumericID: A unique identifier for the vector.

This field assigns a numeric ID to a vector.  Alternatively it can also reflect a case-sensitive hash of the
#ID field if that has been defined previously.

If NumericID is set by the client, then any value in #ID will be immediately cleared.

*****************************************************************************/

static ERROR VECTOR_GET_NumericID(objVector *Self, LONG *Value)
{
   *Value = Self->NumericID;
   return ERR_Okay;
}

static ERROR VECTOR_SET_NumericID(objVector *Self, LONG Value)
{
   Self->NumericID = Value;
   if (Self->ID) { FreeResource(Self->ID); Self->ID = NULL; }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Opacity: Defines an overall opacity for the vector's graphics.

The overall opacity of a vector can be defined here using a value between 0 and 1.0.  The value will be multiplied
with other opacity settings as required during rendering.  For instance, when filling a vector the opacity will be
calculated as #FillOpacity * Opacity.

*****************************************************************************/

static ERROR VECTOR_SET_Opacity(objVector *Self, DOUBLE Value)
{
   if ((Value >= 0) and (Value <= 1.0)) {
      Self->Opacity = Value;
      return ERR_Okay;
   }
   else return ERR_OutOfRange;
}

/*****************************************************************************

-FIELD-
Parent: The parent of the vector, or NULL if this is the top-most vector.

The Parent value will refer to the owner of the vector within its respective branch.  To check if the vector is at the
top or bottom of its branch, please refer to the #Prev and #Next fields.

-FIELD-
Prev: The previous vector in the branch, or NULL.

The Prev value refers to the previous vector in the branch.  If the value is NULL, then the vector is positioned at the
top of the branch.

The Prev value can be set to another vector at any time, on the condition that both vectors share the same owner.  If
this is not true, change the current owner before setting the Prev field.  Changing the value will result in updates to
the #Parent and #Next values.

-ERRORS-
InvalidObject: The value is not a member of the Vector class.
InvalidValue: The provided value is either NULL or refers to itself.
UnsupportedOwner: The referenced vector does not share the same owner.

*****************************************************************************/

static ERROR VECTOR_SET_Prev(objVector *Self, objVector *Value)
{
   parasol::Log log;

   if (Value->Head.ClassID != ID_VECTOR) return log.warning(ERR_InvalidObject);
   if (!Value) return log.warning(ERR_InvalidValue);
   if (Self->Head.OwnerID != Value->Head.OwnerID) return log.warning(ERR_UnsupportedOwner); // Owners must match

   if (Self->Next) Self->Next->Prev = NULL; // Detach from the current Next object.
   if (Self->Prev) Self->Prev->Next = NULL; // Detach from the current Prev object.

   if (Self->Parent) { // Detach from the parent
      if (Self->Parent->ClassID IS ID_VECTORSCENE) {
         ((objVectorScene *)Self->Parent)->Viewport = Self->Next;
         Self->Next->Parent = Self->Parent;
      }
      else if (Self->Parent->ClassID IS ID_VECTOR) {
         ((objVector *)Self->Parent)->Child = Self->Next;
         Self->Next->Parent = Self->Parent;
      }
      Self->Parent = NULL;
   }

   Self->Prev = Value; // Patch the chain
   Self->Next = Value->Next;
   Self->Parent = Value->Parent;
   if (Value->Next) Value->Next->Prev = Self;
   Value->Next = Self;

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Scene: Short-cut to the top-level @VectorScene.

All vectors are required to be grouped within the hierarchy of a @VectorScene.  This requirement is enforced
on initialisation and a reference to the top-level @VectorScene is recorded in this field.

-FIELD-
Sequence: Convert the vector's path to the equivalent SVG path string.

The Sequence is a string of points and instructions that define the path.  It is based on the SVG standard for the path
element 'd' attribute, but also provides some additional features that are present in the vector engine.  Commands are
case insensitive.

The following commands are supported:

<pre>
M: Move To
L: Line To
V: Vertical Line To
H: Horizontal Line To
Q: Quadratic Curve To
T: Quadratic Smooth Curve To
C: Curve To
S: Smooth Curve To
A: Arc
Z: Close Path
</pre>

The use of lower case characters will indicate that the provided coordinates are relative (based on the coordinate
of the previous command).

*****************************************************************************/

static ERROR VECTOR_GET_Sequence(objVector *Self, STRING *Value)
{
   parasol::Log log;

   if (!Self->GeneratePath) return log.warning(ERR_Mismatch); // Path generation must be supported by the vector.

   if ((!Self->BasePath) or (Self->Dirty)) {
      gen_vector_path(Self);
      Self->Dirty = 0;
   }

   if (!Self->BasePath) return ERR_NoData;

   char seq[4096] = "";

   // See agg_path_storage.h for vertex traversal
   // All vertex coordinates are stored in absolute format.

   agg::path_storage *base = Self->BasePath;

   // TODO: Decide what to do with bounding box information, if anything.
   DOUBLE bx1, by1, bx2, by2;
   bounding_rect_single(*base, 0, &bx1, &by1, &bx2, &by2);
   bx1 += Self->FinalX;
   bx2 += Self->FinalX;
   by1 += Self->FinalY;
   by2 += Self->FinalY;

   DOUBLE x, y, x2, y2, x3, y3, last_x = 0, last_y = 0;
   LONG p = 0;
   for (ULONG i=0; i < base->total_vertices(); i++) {
      LONG cmd = base->command(i);
      //LONG cmd_flags = cmd & (~agg::path_cmd_mask);
      cmd &= agg::path_cmd_mask;

      // NB: A Z closes the path by drawing a line to the start of the first point.  A 'dead stop' is defined by
      // leaving out the Z.

      switch(cmd) {
         case agg::path_cmd_stop: // PE_ClosePath
            seq[p++] = 'Z';
            break;

         case agg::path_cmd_move_to: // PE_Move
            base->vertex(i, &x, &y);
            p += StrFormat(seq+p, sizeof(seq)-p, "M%g,%g", x, y);
            last_x = x;
            last_y = y;
            break;

         case agg::path_cmd_line_to: // PE_Line
            base->vertex(i, &x, &y);
            p += StrFormat(seq+p, sizeof(seq)-p, "L%g,%g", x, y);
            last_x = x;
            last_y = y;
            break;

         case agg::path_cmd_curve3: // PE_QuadCurve
            base->vertex(i, &x, &y);
            base->vertex(i+1, &x2, &y2); // End of line
            p += StrFormat(seq+p, sizeof(seq)-p, "q%g,%g,%g,%g", x - last_x, y - last_y, x2 - last_x, y2 - last_y);
            last_x = x;
            last_y = y;
            i += 1;
            break;

         case agg::path_cmd_curve4: // PE_Curve
            base->vertex(i, &x, &y);
            base->vertex(i+1, &x2, &y2);
            base->vertex(i+2, &x3, &y3); // End of line
            p += StrFormat(seq+p, sizeof(seq)-p, "c%g,%g,%g,%g,%g,%g", x - last_x, y - last_y, x2 - last_x, y2 - last_y, x3 - last_x, y3 - last_y);
            last_x = x3;
            last_y = y3;
            i += 2;
            break;

         case agg::path_cmd_end_poly: // PE_ClosePath
            seq[p++] = 'Z';
            break;

         default:
            log.warning("Unrecognised vertice, path command %d", cmd);
            break;
      }
   }
   seq[p] = 0;

   if (seq[0]) {
      *Value = StrClone(seq);
      return ERR_Okay;
   }
   else return ERR_NoData;
}

/*****************************************************************************

-FIELD-
Stroke: Defines the stroke of a path using SVG's IRI format.

The stroker used for rendering a vector path can be defined through this field.  The string is parsed through
the ~ReadPainter() function in the Vector module.  Please refer to it for further details on valid formatting.

*****************************************************************************/

static ERROR VECTOR_GET_Stroke(objVector *Self, CSTRING *Value)
{
   *Value = Self->StrokeString;
   return ERR_Okay;
}

static ERROR VECTOR_SET_Stroke(objVector *Self, STRING Value)
{
   if (Self->StrokeString) { FreeResource(Self->StrokeString); Self->StrokeString = NULL; }
   Self->StrokeString = StrClone(Value);
   vecReadPainter(&Self->Head, Value, &Self->StrokeColour, &Self->StrokeGradient, &Self->StrokeImage, &Self->StrokePattern);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
StrokeColour: Defines the colour of the path stroke in RGB float format.

The colour that will be used in stroking a path is defined by the StrokeColour field.  The colour is composed of
4 floating point values comprising Red, Green, Blue and Alpha.  The intensity of each colour component is determined
by a value range between 0 and 1.0.  If the Alpha value is zero, a coloured stroke will not be applied when drawing
the vector.

This field is complemented by the #StrokeOpacity and #Stroke fields.

*****************************************************************************/

static ERROR VECTOR_GET_StrokeColour(objVector *Self, FLOAT **Value, LONG *Elements)
{
   *Value = (FLOAT *)&Self->StrokeColour;
   *Elements = 4;
   return ERR_Okay;
}

static ERROR VECTOR_SET_StrokeColour(objVector *Self, FLOAT *Value, LONG Elements)
{
   if (Value) {
      if (Elements >= 1) Self->StrokeColour.Red   = Value[0];
      if (Elements >= 2) Self->StrokeColour.Green = Value[1];
      if (Elements >= 3) Self->StrokeColour.Blue  = Value[2];
      if (Elements >= 4) Self->StrokeColour.Alpha = Value[3];
      else Self->StrokeColour.Alpha = 1;
   }
   else Self->StrokeColour.Alpha = 0;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
StrokeOpacity: Defines the opacity of the path stroke.

The StrokeOpacity value expresses the opacity of a path stroke as a value between 0 and 1.0.  A value of zero would
render the stroke invisible and the maximum value of one would render it opaque.

Please note that thinly stroked paths may not be able to appear as fully opaque in some cases due to anti-aliased
rendering.

*****************************************************************************/

static ERROR VECTOR_GET_StrokeOpacity(objVector *Self, DOUBLE *Value)
{
   *Value = Self->StrokeOpacity;
   return ERR_Okay;
}

static ERROR VECTOR_SET_StrokeOpacity(objVector *Self, DOUBLE Value)
{
   if ((Value >= 0) and (Value <= 1.0)) {
      Self->StrokeOpacity = Value;
      return ERR_Okay;
   }
   else return ERR_OutOfRange;
}

/*****************************************************************************
-FIELD-
StrokeWidth: The width to use when stroking the path.

The StrokeWidth defines the pixel width of a path when it is stroked.  If this field is set to zero, the path will not
be stroked.

The StrokeWidth is affected by scaling factors imposed by transforms and viewports.

*****************************************************************************/

static ERROR VECTOR_SET_StrokeWidth(objVector *Self, DOUBLE Value)
{
   if ((Value >= 0.0) and (Value <= 1000.0)) {
      Self->StrokeWidth = Value;
      return ERR_Okay;
   }
   else return ERR_OutOfRange;
}

/*****************************************************************************

-FIELD-
Transforms: A linked list of transforms that have been applied to the vector.

Any transforms that have been applied to the vector can be read from the Transforms field.  Each transform is
represented by the VECTOR_TRANSFORM structure, and are linked in the order in which they are applied to the vector.

&VectorTransform

-FIELD-
Transition: Reference a VectorTransition object here to apply multiple transforms over the vector's path.

A transition can be applied by setting this field with a reference to a @VectorTransition object.  Please
refer to the @VectorTransition class for further information.

Not all vector types are well-suited or adapted to the use of transitions.  At the time of writing, only @VectorText
and @VectorWave are able to take full advantage of this feature.

*****************************************************************************/

static ERROR VECTOR_GET_Transition(objVector *Self, rkVectorTransition **Value)
{
   *Value = Self->Transition;
   return ERR_Okay;
}

static ERROR VECTOR_SET_Transition(objVector *Self, rkVectorTransition *Value)
{
   parasol::Log log;

   if (!Value) {
      if (Self->Transition) {
         UnsubscribeAction(Self->Transition, AC_Free);
         Self->Transition = NULL;
      }
      return ERR_Okay;
   }
   else if (Value->Head.ClassID IS ID_VECTORTRANSITION) {
      if (Self->Transition) UnsubscribeAction(Self->Transition, AC_Free);
      if (Value->Head.Flags & NF_INITIALISED) { // The object must be initialised.
         SubscribeAction(Value, AC_Free);
         Self->Transition = Value;
         return ERR_Okay;
      }
      else return log.warning(ERR_NotInitialised);
   }
   else return log.warning(ERR_InvalidObject);
}

/*****************************************************************************

-FIELD-
Visibility: Controls the visibility of a vector and its children.
-END-

*****************************************************************************/

static ERROR vector_input_events(objVector *Self, const InputEvent *Events)
{
   parasol::Log log(__FUNCTION__);

   LONG total_events = 0;
   for (auto ev=Events; ev; ev=ev->Next) total_events++;

   InputEvent filtered_events[total_events+2]; // +2 in case of JET_ENTERED/JET_LEFT

   // Retrieve the full vector bounds, accounting for all transforms and children.

   std::array<DOUBLE, 4> bounds = { DBL_MAX, DBL_MAX, DBL_MIN, DBL_MIN };
   calc_full_boundary(Self->Child, bounds);

   // Filter for events that occur within the vector's bounds

   LONG e = 0;
   for (auto input=Events; input; input=input->Next) {
      if ((input->Type IS JET_LEFT_SURFACE) or (input->Type IS JET_ENTERED_SURFACE)) continue;

      if ((input->X >= bounds[0]) and (input->Y >= bounds[1]) and
          (input->X < bounds[2]) and (input->Y < bounds[3])) {
         if (!Self->UserHovering) { // Inject JET_ENTERED_SURFACE if this is the first activity
            filtered_events[e++] = {
               .Next        = NULL,
               .Value       = input->OverID,
               .Timestamp   = input->Timestamp,
               .RecipientID = input->RecipientID,
               .OverID      = input->OverID,
               .AbsX        = input->AbsX,
               .AbsY        = input->AbsY,
               .X           = input->X,
               .Y           = input->Y,
               .DeviceID    = input->DeviceID,
               .Type        = JET_ENTERED_SURFACE,
               .Flags       = JTYPE_FEEDBACK,
               .Mask        = JTYPE_FEEDBACK
            };
            Self->UserHovering = TRUE;
         }

         filtered_events[e] = *input;
         e++;
      }
      else if (Self->UserHovering) {
         filtered_events[e++] = { // Inject JET_LEFT_SURFACE if this is the last activity
            .Next        = NULL,
            .Value       = input->OverID,
            .Timestamp   = input->Timestamp,
            .RecipientID = input->RecipientID,
            .OverID      = input->OverID,
            .AbsX        = input->AbsX,
            .AbsY        = input->AbsY,
            .X           = input->X,
            .Y           = input->Y,
            .DeviceID    = input->DeviceID,
            .Type        = JET_LEFT_SURFACE,
            .Flags       = JTYPE_FEEDBACK,
            .Mask        = JTYPE_FEEDBACK
         };
         Self->UserHovering = FALSE;
      }
   }

   if (!e) return ERR_Okay;

   for (auto it=Self->InputSubscriptions->begin(); it != Self->InputSubscriptions->end(); ) {
      // Patch the Next fields to construct a custom chain of events based on this subscripton's mask filter.

      auto &sub = *it;
      InputEvent *first = NULL, *last = NULL;
      for (LONG i=0; i < e; i++) {
         if (filtered_events[i].Mask & sub.Mask) {
            if (!first) first = &filtered_events[i];
            if (last) last->Next = &filtered_events[i];
            last = &filtered_events[i];
         }
      }

      if (first) {
         last->Next = NULL;

         ERROR result;
         if (sub.Callback.Type IS CALL_STDC) {
            parasol::SwitchContext ctx(sub.Callback.StdC.Context);
            auto callback = (ERROR (*)(objVector *, InputEvent *))sub.Callback.StdC.Routine;
            result = callback(Self, first);
         }
         else if (sub.Callback.Type IS CALL_SCRIPT) {
            // In this implementation the script function will receive all the events chained via the Next field
            ScriptArg args[] = {
               { "Vector",            FDF_OBJECT, { .Address = Self } },
               { "InputEvent:Events", FDF_STRUCT, { .Address = first } }
            };
            scCallback(sub.Callback.Script.Script, sub.Callback.Script.ProcedureID, args, ARRAYSIZE(args), &result);
         }

         if (result IS ERR_Terminate) it = Self->InputSubscriptions->erase(it);
         else it++;
      }
      else it++;
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR vector_keyboard_events(objVector *Self, const evKey *Event)
{
   for (auto it=Self->KeyboardSubscriptions->begin(); it != Self->KeyboardSubscriptions->end(); ) {
      ERROR result;
      auto &sub = *it;
      if (sub.Callback.Type IS CALL_STDC) {
         parasol::SwitchContext ctx(sub.Callback.StdC.Context);
         auto callback = (ERROR (*)(objVector *, LONG, LONG, LONG))sub.Callback.StdC.Routine;
         result = callback(Self, Event->Qualifiers, Event->Code, Event->Unicode);
      }
      else if (sub.Callback.Type IS CALL_SCRIPT) {
         // In this implementation the script function will receive all the events chained via the Next field
         ScriptArg args[] = {
            { "Vector",     FDF_OBJECT, { .Address = Self } },
            { "Qualifiers", FDF_LONG,   { .Long = Event->Qualifiers } },
            { "Code",       FDF_LONG,   { .Long = Event->Code } },
            { "Unicode",    FDF_LONG,   { .Long = Event->Unicode } }
         };
         scCallback(sub.Callback.Script.Script, sub.Callback.Script.ProcedureID, args, ARRAYSIZE(args), &result);
      }

      if (result IS ERR_Terminate) Self->KeyboardSubscriptions->erase(it);
      else it++;
   }

   return ERR_Okay;
}

//****************************************************************************

static const FieldDef clFlags[] = {
   { "Disabled", VF_DISABLED },
   { "HasFocus", VF_HAS_FOCUS },
   { NULL, 0 }
};

static const FieldDef clTransformFlags[] = {
   { "Matrix",    VTF_MATRIX },
   { "Translate", VTF_TRANSLATE },
   { "Scale",     VTF_SCALE },
   { "Rotate",    VTF_ROTATE },
   { "Skew",      VTF_SKEW },
   { NULL, 0 }
};

static const FieldDef clMorphFlags[] = {
   { "Stretch",     VMF_STRETCH },
   { "AutoSpacing", VMF_AUTO_SPACING },
   { "XMin",        VMF_X_MIN },
   { "XMid",        VMF_X_MID },
   { "XMax",        VMF_X_MAX },
   { "YMin",        VMF_Y_MIN },
   { "YMid",        VMF_Y_MID },
   { "YMax",        VMF_Y_MAX },
   { NULL, 0 }
};

static const FieldDef clLineJoin[] = {
   { "Miter",       VLJ_MITER },
   { "Round",       VLJ_ROUND },
   { "Bevel",       VLJ_BEVEL },
   { "MiterRevert", VLJ_MITER_REVERT },
   { "MiterRound",  VLJ_MITER_ROUND },
   { "Inherit",     VLJ_INHERIT },
   { NULL, 0 }
};

static const FieldDef clLineCap[] = {
   { "Butt",    VLC_BUTT },
   { "Square",  VLC_SQUARE },
   { "Round",   VLC_ROUND },
   { "Inherit", VLC_INHERIT },
   { NULL, 0 }
};

static const FieldDef clInnerJoin[] = {
   { "Miter",   VIJ_MITER },
   { "Round",   VIJ_ROUND },
   { "Bevel",   VIJ_BEVEL },
   { "Jag",     VIJ_JAG },
   { "Inherit", VIJ_INHERIT },
   { NULL, 0 }
};

static const FieldDef clFillRule[] = {
   { "EvenOdd", VFR_EVEN_ODD },
   { "NonZero", VFR_NON_ZERO },
   { "Inherit", VFR_INHERIT },
   { NULL, 0 }
};

static const FieldDef clVisibility[] = {
   { "Hidden",   VIS_HIDDEN },
   { "Visible",  VIS_VISIBLE },
   { "Collapse", VIS_COLLAPSE },
   { "Inherit",  VIS_INHERIT },
   { NULL, 0 }
};

static const FieldArray clVectorFields[] = {
   { "Child",            FDF_OBJECT|FD_R,              ID_VECTOR, NULL, NULL },
   { "Scene",            FDF_OBJECT|FD_R,              ID_VECTORSCENE, NULL, NULL },
   { "Next",             FDF_OBJECT|FD_RW,             ID_VECTOR, NULL, (APTR)VECTOR_SET_Next },
   { "Prev",             FDF_OBJECT|FD_RW,             ID_VECTOR, NULL, (APTR)VECTOR_SET_Prev },
   { "Parent",           FDF_OBJECT|FD_R,              0, NULL, NULL },
   { "Transforms",       FDF_POINTER|FDF_STRUCT|FDF_R, (MAXINT)"VectorTransform", NULL, NULL },
   { "StrokeWidth",      FDF_DOUBLE|FD_RW,             0, NULL, (APTR)VECTOR_SET_StrokeWidth },
   { "StrokeOpacity",    FDF_DOUBLE|FDF_RW,            0, (APTR)VECTOR_GET_StrokeOpacity, (APTR)VECTOR_SET_StrokeOpacity },
   { "FillOpacity",      FDF_DOUBLE|FDF_RW,            0, (APTR)VECTOR_GET_FillOpacity, (APTR)VECTOR_SET_FillOpacity },
   { "Opacity",          FDF_DOUBLE|FD_RW,             0, NULL, (APTR)VECTOR_SET_Opacity },
   { "MiterLimit",       FDF_DOUBLE|FD_RW,             0, NULL, (APTR)VECTOR_SET_MiterLimit },
   { "InnerMiterLimit",  FDF_DOUBLE|FD_RW,             0, NULL, NULL },
   { "DashOffset",       FDF_DOUBLE|FD_RW,             0, NULL, NULL },
   { "ActiveTransforms", FDF_LONGFLAGS|FD_R,           (MAXINT)&clTransformFlags, NULL, NULL },
   { "DashTotal",        FDF_LONG|FDF_R,               0, NULL, NULL },
   { "Visibility",       FDF_LONG|FDF_LOOKUP|FDF_RW,   (MAXINT)&clVisibility, NULL, NULL },
   { "Flags",            FDF_LONG|FDF_RI,              (MAXINT)&clFlags, NULL, NULL },
   // Virtual fields
   { "ClipRule",     FDF_VIRTUAL|FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clFillRule, (APTR)VECTOR_GET_ClipRule, (APTR)VECTOR_SET_ClipRule },
   { "DashArray",    FDF_VIRTUAL|FDF_ARRAY|FDF_DOUBLE|FD_RW, 0, (APTR)VECTOR_GET_DashArray, (APTR)VECTOR_SET_DashArray },
   { "Mask",         FDF_VIRTUAL|FDF_OBJECT|FDF_RW,          0, (APTR)VECTOR_GET_Mask, (APTR)VECTOR_SET_Mask },
   { "Morph",        FDF_VIRTUAL|FDF_OBJECT|FDF_RW,          0, (APTR)VECTOR_GET_Morph, (APTR)VECTOR_SET_Morph },
   { "MorphFlags",   FDF_VIRTUAL|FDF_LONGFLAGS|FDF_RW,       (MAXINT)&clMorphFlags, (APTR)VECTOR_GET_MorphFlags, (APTR)VECTOR_SET_MorphFlags },
   { "NumericID",    FDF_VIRTUAL|FDF_LONG|FDF_RW,            0, (APTR)VECTOR_GET_NumericID, (APTR)VECTOR_SET_NumericID },
   { "ID",           FDF_VIRTUAL|FDF_STRING|FDF_RW,          0, (APTR)VECTOR_GET_ID, (APTR)VECTOR_SET_ID },
   { "Sequence",     FDF_VIRTUAL|FDF_STRING|FDF_ALLOC|FDF_R, 0, (APTR)VECTOR_GET_Sequence, NULL },
   { "Stroke",       FDF_VIRTUAL|FDF_STRING|FDF_RW,          0, (APTR)VECTOR_GET_Stroke, (APTR)VECTOR_SET_Stroke },
   { "StrokeColour", FDF_VIRTUAL|FD_FLOAT|FDF_ARRAY|FD_RW,   0, (APTR)VECTOR_GET_StrokeColour, (APTR)VECTOR_SET_StrokeColour },
   { "Transition",   FDF_VIRTUAL|FDF_OBJECT|FDF_RW,          0, (APTR)VECTOR_GET_Transition, (APTR)VECTOR_SET_Transition },
   { "EnableBkgd",   FDF_VIRTUAL|FDF_LONG|FDF_RW,            0, (APTR)VECTOR_GET_EnableBkgd, (APTR)VECTOR_SET_EnableBkgd },
   { "Fill",         FDF_VIRTUAL|FDF_STRING|FDF_RW,          0, (APTR)VECTOR_GET_Fill, (APTR)VECTOR_SET_Fill },
   { "FillColour",   FDF_VIRTUAL|FD_FLOAT|FDF_ARRAY|FDF_RW,  0, (APTR)VECTOR_GET_FillColour, (APTR)VECTOR_SET_FillColour },
   { "FillRule",     FDF_VIRTUAL|FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clFillRule, (APTR)VECTOR_GET_FillRule, (APTR)VECTOR_SET_FillRule },
   { "Filter",       FDF_VIRTUAL|FDF_STRING|FDF_RW,          0, (APTR)VECTOR_GET_Filter, (APTR)VECTOR_SET_Filter },
   { "LineJoin",     FDF_VIRTUAL|FD_LONG|FD_LOOKUP|FDF_RW,   (MAXINT)&clLineJoin,  (APTR)VECTOR_GET_LineJoin, (APTR)VECTOR_SET_LineJoin },
   { "LineCap",      FDF_VIRTUAL|FD_LONG|FD_LOOKUP|FDF_RW,   (MAXINT)&clLineCap,   (APTR)VECTOR_GET_LineCap, (APTR)VECTOR_SET_LineCap },
   { "InnerJoin",    FDF_VIRTUAL|FD_LONG|FD_LOOKUP|FDF_RW,   (MAXINT)&clInnerJoin, (APTR)VECTOR_GET_InnerJoin, (APTR)VECTOR_SET_InnerJoin },
   END_FIELD
};

#include "vector_def.c"

static ERROR init_vector(void)
{
   return(CreateObject(ID_METACLASS, 0, &clVector,
      FID_ClassVersion|TFLOAT, VER_VECTOR,
      FID_Name|TSTR,      "Vector",
      FID_Category|TLONG, CCF_GRAPHICS,
      FID_Actions|TPTR,   clVectorActions,
      FID_Methods|TARRAY, clVectorMethods,
      FID_Fields|TARRAY,  clVectorFields,
      FID_Size|TLONG,     sizeof(objVector),
      FID_Path|TSTR,      "modules:vector",
      TAGEND));
}
