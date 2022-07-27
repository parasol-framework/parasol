/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

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
manuals from the W3C.  In cases where we are missing support for an SVG feature, assume that future support is intended
unless otherwise documented.

-END-

*********************************************************************************************************************/

static ERROR VECTOR_Push(objVector *, struct vecPush *);

//********************************************************************************************************************

static void debug_tree(objVector *Vector, LONG &Level)
{
   parasol::Log log(__FUNCTION__);
   char indent[Level + 1];
   char buffer[120];
   LONG i;

   Level++;
   for (i=0; i < Level; i++) indent[i] = ' '; // Indenting
   indent[i] = 0;

   for (auto v=Vector; v; v=(objVector *)v->Next) {
      if (FindField(v, FID_Dimensions, NULL)) {
         GetFieldVariable(v, "$Dimensions", buffer, sizeof(buffer));
      }
      else buffer[0] = 0;

      if (v->Child) {
         parasol::Log blog(__FUNCTION__);
         blog.branch("Vector #%d%s %s %s %s", v->Head.UID, indent, v->Head.Class->ClassName, GetName(v) ? GetName(v) : "", buffer);
         debug_tree(v->Child, Level);
      }
      else log.msg("Vector #%d%s %s %s %s", v->Head.UID, indent, v->Head.Class->ClassName, GetName(v) ? GetName(v) : "", buffer);
   }

   Level--;
}

//********************************************************************************************************************
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

//********************************************************************************************************************

static ERROR VECTOR_ActionNotify(objVector *Self, struct acActionNotify *Args)
{
   if (Args->ActionID IS AC_Free) {
      if ((Self->ClipMask) and (Args->ObjectID IS Self->ClipMask->Head.UID)) Self->ClipMask = NULL;
      else if ((Self->Morph) and (Args->ObjectID IS Self->Morph->Head.UID)) Self->Morph = NULL;
      else if ((Self->Transition) and (Args->ObjectID IS Self->Transition->Head.UID)) Self->Transition = NULL;
      else if (Self->FeedbackSubscriptions) {
         for (auto it=Self->FeedbackSubscriptions->begin(); it != Self->FeedbackSubscriptions->end(); ) {
            auto &sub = *it;
            if ((sub.Callback.Type IS CALL_SCRIPT) and (sub.Callback.Script.Script->UID IS Args->ObjectID)) {
               it = Self->FeedbackSubscriptions->erase(it);
            }
            else it++;
         }
      }
   }
   else return ERR_NoSupport;

   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
Debug: Internal functionality for debugging.

This internal method prints comprehensive debugging information to the log.

-ERRORS-
Okay:

*********************************************************************************************************************/

static ERROR VECTOR_Debug(objVector *Self, APTR Void)
{
   LONG level = 0;
   debug_tree(Self, level);
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Disable: Disabling a vector can be used to trigger style changes and prevent user input.
-END-
*********************************************************************************************************************/

static ERROR VECTOR_Disable(objVector *Self, APTR Void)
{
   // It is up to the client to monitor the Disable action if any reaction is required.
   Self->Flags |= VF_DISABLED;
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Draw: Draws the surface associated with the vector.

Calling the Draw action on a vector will schedule a redraw of the scene graph if it is associated with a @Surface.
Internally, drawing is scheduled for the next frame and is not immediate.

-ERROR-
Okay
FieldNotSet: The vector's scene graph is not associated with a Surface.
-END-
*********************************************************************************************************************/

static ERROR VECTOR_Draw(objVector *Self, struct acDraw *Args)
{
   if ((Self->Scene) and (Self->Scene->SurfaceID)) {
      if (Self->Dirty) gen_vector_tree((objVector *)Self);
      //if (!Self->BasePath.total_vertices()) return ERR_NoData;
#if 0
      // Retrieve bounding box, post-transformations.
      // TODO: Would need to account for client defined brush stroke widths and stroke scaling.

      DOUBLE bx1, by1, bx2, by2;
      bounding_rect_single(Self->BasePath, 0, &bx1, &by1, &bx2, &by2);

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

      struct drwScheduleRedraw area = { .X = F2T(bx1), .Y = F2T(by1), .Width = F2T(bx2 - bx1), .Height = F2T(by2 - by1) };
#endif

      objSurface *surface;
      if (!AccessObject(Self->Scene->SurfaceID, 1000, &surface)) {
         Action(MT_DrwScheduleRedraw, surface, NULL);
         ReleaseObject(surface);
         return ERR_Okay;
      }
      else return ERR_AccessObject;
   }
   else {
      parasol::Log log;
      return log.warning(ERR_FieldNotSet);
   }
}

/*********************************************************************************************************************
-ACTION-
Enable: Reverses the effects of disabling the vector.
-END-
*********************************************************************************************************************/

static ERROR VECTOR_Enable(objVector *Self, APTR Void)
{
  // It is up to the client to subscribe to the Enable action if any activity needs to take place.
  Self->Flags &= ~VF_DISABLED;
  return ERR_Okay;
}

//********************************************************************************************************************

static ERROR VECTOR_Free(objVector *Self, APTR Args)
{
   Self->~objVector();

   if (Self->ID)           { FreeResource(Self->ID); Self->ID = NULL; }
   if (Self->FillString)   { FreeResource(Self->FillString); Self->FillString = NULL; }
   if (Self->StrokeString) { FreeResource(Self->StrokeString); Self->StrokeString = NULL; }
   if (Self->FilterString) { FreeResource(Self->FilterString); Self->FilterString = NULL; }

   if (Self->FillGradientTable)   { delete Self->FillGradientTable; Self->FillGradientTable = NULL; }
   if (Self->StrokeGradientTable) { delete Self->StrokeGradientTable; Self->StrokeGradientTable = NULL; }
   if (Self->DashArray)           { delete Self->DashArray; Self->DashArray = NULL; }

   // Patch the nearest vectors that are linked to this one.
   if (Self->Next) Self->Next->Prev = Self->Prev;
   if (Self->Prev) Self->Prev->Next = Self->Next;
   if ((Self->Parent) and (!Self->Prev)) {
      if (Self->Parent->ClassID IS ID_VECTORSCENE) ((objVectorScene *)Self->Parent)->Viewport = Self->Next;
      else ((objVector *)Self->Parent)->Child = Self->Next;
   }
   if (Self->Child) Self->Child->Parent = NULL;

   if ((Self->Scene) and (!(Self->Scene->Head.Flags & (NF_FREE|NF_FREE_MARK)))) {
      Self->Scene->InputSubscriptions.erase(Self);
      Self->Scene->KeyboardSubscriptions.erase(Self);
   }

   if (Self->Matrices) {
      VectorMatrix *next;
      for (auto t=Self->Matrices; t; t=next) {
         next = t->Next;
         FreeResource(t);
      }
   }

   delete Self->StrokeRaster; Self->StrokeRaster = NULL;
   delete Self->FillRaster;   Self->FillRaster   = NULL;
   delete Self->InputSubscriptions;    Self->InputSubscriptions    = NULL;
   delete Self->KeyboardSubscriptions; Self->KeyboardSubscriptions = NULL;
   delete Self->FeedbackSubscriptions; Self->FeedbackSubscriptions = NULL;

   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
FreeMatrix: Remove an allocated VectorMatrix structure.

Transformations allocated from ~Vector.NewMatrix() can be removed with this method.  If multiple transforms are
attached to the vector then it should be noted that this will affect downstream transformations.

-INPUT-
struct(*VectorMatrix) Matrix: Reference to the structure that requires removal.

-ERRORS-
Okay:
NullArgs:

*********************************************************************************************************************/

static ERROR VECTOR_FreeMatrix(objVector *Self, struct vecFreeMatrix *Args)
{
   if ((!Args) or (!Args->Matrix)) return ERR_NullArgs;

   // Clean up the linked list

   if (Self->Matrices IS Args->Matrix) {
      Self->Matrices = Args->Matrix->Next;
   }
   else {
      for (auto t = Self->Matrices; t; t=t->Next) {
         if (t->Next IS Args->Matrix) {
            t->Next = Args->Matrix->Next;
            break;
         }
      }
   }

   FreeResource(Args->Matrix);

   mark_dirty(Self, RC_TRANSFORM);
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
GetBoundary: Returns the graphical boundary of a vector.

This method will return the boundary of a vector's path in terms of its top-left position, width and height.  All
transformations and position information that applies to the vector will be taken into account when computing the
boundary.

If the `VBF_INCLUSIVE` flag is used, the result will include an analysis of all paths that belong to children of the
target vector, including transforms.

If the `VBF_NO_TRANSFORM` flag is used, the transformation step is not applied to the vector's path.

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

*********************************************************************************************************************/

static ERROR VECTOR_GetBoundary(objVector *Self, struct vecGetBoundary *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if (!Self->Scene) return log.warning(ERR_NotInitialised);

   if (Self->GeneratePath) { // Path generation must be supported by the vector.
      if (Self->Dirty) gen_vector_tree((objVector *)Self);
      if (!Self->BasePath.total_vertices()) return ERR_NoData;

      std::array<DOUBLE, 4> bounds = { DBL_MAX, DBL_MAX, -1000000, -1000000 };
      DOUBLE bx1, by1, bx2, by2;

      if (Args->Flags & VBF_NO_TRANSFORM) {
         bounding_rect_single(Self->BasePath, 0, &bx1, &by1, &bx2, &by2);
         bounds[0] = bx1 + Self->FinalX;
         bounds[1] = by1 + Self->FinalY;
         bounds[2] = bx2 + Self->FinalX;
         bounds[3] = by2 + Self->FinalY;
      }
      else {
         agg::conv_transform<agg::path_storage, agg::trans_affine> path(Self->BasePath, Self->Transform);
         bounding_rect_single(path, 0, &bx1, &by1, &bx2, &by2);
         bounds[0] = bx1;
         bounds[1] = by1;
         bounds[2] = bx2;
         bounds[3] = by2;
      }

      if (Args->Flags & VBF_INCLUSIVE) calc_full_boundary(Self->Child, bounds);

      Args->X      = bounds[0];
      Args->Y      = bounds[1];
      Args->Width  = bounds[2] - bounds[0];
      Args->Height = bounds[3] - bounds[1];
      return ERR_Okay;
   }
   else return ERR_NotPossible;
}

/*********************************************************************************************************************
-ACTION-
Hide: Changes the vector's visibility setting to hidden.
-END-
*********************************************************************************************************************/

static ERROR VECTOR_Hide(objVector *Self, APTR Void)
{
   Self->Visibility = VIS_HIDDEN;
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR VECTOR_Init(objVector *Self, APTR Void)
{
   parasol::Log log;

   if ((!Self->Head.SubID) or (Self->Head.SubID IS ID_VECTOR)) {
      log.warning("Vector cannot be instantiated directly (use a sub-class).");
      return ERR_Failed;
   }

   if (!Self->Parent) set_parent(Self, Self->Head.OwnerID);

   log.trace("Parent: #%d, Siblings: #%d #%d, Vector: %p", Self->Parent ? Self->Parent->UID : 0,
      Self->Prev ? Self->Prev->Head.UID : 0, Self->Next ? Self->Next->Head.UID : 0, Self);

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

   Self->ParentView = get_parent_view(Self);

   // Reapply the filter if it couldn't be set prior to initialisation.
   if ((!Self->Filter) and (Self->FilterString)) {
      SetString(Self, FID_Filter, Self->FilterString);
   }

   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
MoveToBack: Move a vector to the back of its stack.

*********************************************************************************************************************/

static ERROR VECTOR_MoveToBack(objVector *Self, APTR Void)
{
   struct vecPush push = { -32768 };
   return VECTOR_Push(Self, &push);
}

/*********************************************************************************************************************
-ACTION-
MoveToFront: Move a vector to the front of its stack.
-END-
*********************************************************************************************************************/

static ERROR VECTOR_MoveToFront(objVector *Self, APTR Void)
{
   struct vecPush push = { 32767 };
   return VECTOR_Push(Self, &push);
}

//********************************************************************************************************************

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
   new (Self) objVector;
   return ERR_Okay;
}

//********************************************************************************************************************

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

/*********************************************************************************************************************

-METHOD-
NewMatrix: Returns a VectorMatrix structure that allows transformations to be applied to the vector.

Call NewMatrix() to allocate a transformation matrix that allows transforms to be applied to a vector.  Manipulating
the transformation matrix is supported by functions in the Vector module, such as ~Vector.Scale() and
~Vector.Rotate().

Note that if multiple matrices are allocated by the client, they will be applied to the vector in the
order of their creation.

The structure will be owned by the Vector object and is automatically terminated when the Vector is destroyed.  If the
transform is no longer required before then, it can be manually removed with ~Vector.FreeMatrix().

-INPUT-
&resource(*VectorMatrix) Transform: A reference to the new transform structure is returned here.

-ERRORS-
Okay:
NullArgs:

*********************************************************************************************************************/

static ERROR VECTOR_NewMatrix(objVector *Self, struct vecNewMatrix *Args)
{
   if (!Args) return ERR_NullArgs;

   VectorMatrix *transform;
   if (!AllocMemory(sizeof(VectorMatrix), MEM_DATA|MEM_NO_CLEAR, &transform, NULL)) {
      // Insert transform at the start of the list.

      transform->Vector = Self;
      transform->Next   = Self->Matrices;
      transform->ScaleX = 1.0;
      transform->ScaleY = 1.0;
      transform->ShearX = 0;
      transform->ShearY = 0;
      transform->TranslateX = 0;
      transform->TranslateY = 0;

      Self->Matrices = transform;
      Args->Transform = transform;

      mark_dirty(Self, RC_TRANSFORM);
      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

/*********************************************************************************************************************

-METHOD-
PointInPath: Checks if point at (X,Y) is within a vector's path.

This method provides an accurate means of determining if a specific coordinate is inside the path of a vector.
Transforms are taken into account, as are clip masks.

-INPUT-
double X: The X coordinate of the point.
double Y: The Y coordinate of the point.

-ERRORS-
Okay: The point is in the path.
False: The point is not in the path.
NullArgs:
NoData: The vector is unable to generate a path based on its current values.
NoSupport: The vector type does not support path generation.

*********************************************************************************************************************/

static ERROR VECTOR_PointInPath(objVector *Self, struct vecPointInPath *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if (Self->Dirty) gen_vector_tree((objVector *)Self);
   if (!Self->BasePath.total_vertices()) return ERR_NoData;

   if (Self->Head.SubID IS ID_VECTORVIEWPORT) {
      agg::vertex_d w, x, y, z;

      auto &vertices = Self->BasePath.vertices(); // Note: Viewport BasePath is fully transformed.
      vertices.vertex(0, &x.x, &x.y);
      vertices.vertex(1, &y.x, &y.y);
      vertices.vertex(2, &z.x, &z.y);
      vertices.vertex(3, &w.x, &w.y);

      agg::vertex_d pt = agg::vertex_d(Args->X, Args->Y, 0);

      // Test assumes clockwise points; for counter-clockwise you'd use < 0.
      bool inside = (is_left(x, y, pt) > 0) and (is_left(y, z, pt) > 0) and
                    (is_left(z, w, pt) > 0) and (is_left(w, x, pt) > 0);

      if (inside) return ERR_Okay;
   }
   else if (Self->Head.SubID IS ID_VECTORRECTANGLE) {
      agg::vertex_d w, x, y, z;
      agg::conv_transform<agg::path_storage, agg::trans_affine> base_path(Self->BasePath, Self->Transform);

      base_path.rewind(0);
      base_path.vertex(&x.x, &x.y);
      base_path.vertex(&y.x, &y.y);
      base_path.vertex(&z.x, &z.y);
      base_path.vertex(&w.x, &w.y);

      agg::vertex_d pt = agg::vertex_d(Args->X, Args->Y, 0);

      bool inside = (is_left(x, y, pt) > 0) and (is_left(y, z, pt) > 0) and
                    (is_left(z, w, pt) > 0) and (is_left(w, x, pt) > 0);

      if (inside) return ERR_Okay;
   }
   else {
      // Quick check to see if (X,Y) is within the path's boundary, then follow-up with a hit test.

      agg::conv_transform<agg::path_storage, agg::trans_affine> base_path(Self->BasePath, Self->Transform);
      DOUBLE bx1, by1, bx2, by2;
      bounding_rect_single(base_path, 0, &bx1, &by1, &bx2, &by2);
      if ((Args->X >= bx1) and (Args->Y >= by1) and (Args->X < bx2) and (Args->Y < by2)) {
         if (Self->DisableHitTesting) return ERR_Okay;
         else {
            // Do the hit testing.  TODO: There is potential for more sophisticated & optimal hit testing methods.
            agg::rasterizer_scanline_aa<> raster;
            raster.add_path(base_path);
            if (raster.hit_test(Args->X, Args->Y)) return ERR_Okay;
         }
      }
   }

   return ERR_False;
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

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

/*********************************************************************************************************************
-ACTION-
Show: Changes the vector's visibility setting to visible.
-END-
*********************************************************************************************************************/

static ERROR VECTOR_Show(objVector *Self, APTR Void)
{
   Self->Visibility = VIS_VISIBLE;
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
SubscribeFeedback: Subscribe to events that relate to the vector.

Use this method to receive feedback for events that have affected the state of a vector.

To remove an existing subscription, call this method again with the same Callback and an empty Mask.
Alternatively have the callback function return `ERR_Terminate`.

The synopsis for the Callback is:

```
ERROR callback(*Vector, LONG Event)
```

-INPUT-
int(FM) Mask: Combine FM flags to define the feedback events required by the client.  Set to 0xffffffff if all messages are desirable.
ptr(func) Callback: The function that will receive feedback events.

-ERRORS-
Okay:
NullArgs:

****************************************************************************/

static ERROR VECTOR_SubscribeFeedback(objVector *Self, struct vecSubscribeFeedback *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Callback)) return log.warning(ERR_NullArgs);

   if (Args->Mask) {
      if (!Self->FeedbackSubscriptions) {
         Self->FeedbackSubscriptions = new (std::nothrow) std::vector<FeedbackSubscription>;
         if (!Self->FeedbackSubscriptions) return log.warning(ERR_AllocMemory);
      }

      Self->FeedbackSubscriptions->emplace_back(*Args->Callback, Args->Mask);
   }
   else if (Self->FeedbackSubscriptions) { // Remove existing subscriptions for this callback
      for (auto it=Self->FeedbackSubscriptions->begin(); it != Self->FeedbackSubscriptions->end(); ) {
         if (*Args->Callback IS it->Callback) it = Self->FeedbackSubscriptions->erase(it);
         else it++;
      }
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
SubscribeInput: Create a subscription for input events that relate to the vector.

The SubscribeInput method is provided as an extension to gfxSubscribeInput(), whereby the user's input events
will be restricted to those within the vector's scene graph.  The original events are transferred as-is, although the
`ENTERED_SURFACE` and `LEFT_SURFACE` events are modified so that they trigger during passage through the scene's
boundaries.

It is a pre-requisite that the associated @VectorScene has been linked to a @Surface.

To remove an existing subscription, call this method again with the same Callback and an empty Mask.
Alternatively have the function return `ERR_Terminate`.

Please refer to gfxSubscribeInput() for further information on event management and message handling.

The synopsis for the Callback is:

```
ERROR callback(*Vector, *InputEvent)
```

-INPUT-
int(JTYPE) Mask: Combine JTYPE flags to define the input messages required by the client.  Set to zero to remove an existing subscription.
ptr(func) Callback: Reference to a function that will receive input messages.

-ERRORS-
Okay:
NullArgs:
FieldNotSet: The VectorScene has no reference to a Surface.
AllocMemory:
Function: A call to gfxSubscribeInput() failed.

*********************************************************************************************************************/

static ERROR VECTOR_SubscribeInput(objVector *Self, struct vecSubscribeInput *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Callback)) return log.warning(ERR_NullArgs);

   if (Args->Mask) {
      if ((!Self->Scene) or (!Self->Scene->SurfaceID)) return log.warning(ERR_FieldNotSet);

      if (!Self->InputSubscriptions) {
         Self->InputSubscriptions = new (std::nothrow) std::vector<InputSubscription>;
         if (!Self->InputSubscriptions) return log.warning(ERR_AllocMemory);
      }

      LONG mask = Args->Mask;
      if (mask & JTYPE_FEEDBACK) mask |= JTYPE_MOVEMENT;

      Self->InputMask |= mask;
      Self->Scene->InputSubscriptions[Self] = Self->InputMask;
      Self->InputSubscriptions->emplace_back(*Args->Callback, mask);
   }
   else if (Self->InputSubscriptions) { // Remove existing subscriptions for this callback
      for (auto it=Self->InputSubscriptions->begin(); it != Self->InputSubscriptions->end(); ) {
         if (*Args->Callback IS it->Callback) it = Self->InputSubscriptions->erase(it);
         else it++;
      }

      if (Self->InputSubscriptions->empty()) {
         if ((Self->Scene) and (!(Self->Scene->Head.Flags & (NF_FREE|NF_FREE_MARK)))) {
            Self->Scene->InputSubscriptions.erase(Self);
         }
      }
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
SubscribeKeyboard: Create a subscription for input events that relate to the vector.

The SubscribeKeyboard method provides a callback mechanism for handling keyboard events.  Events are reported when the
vector or one of its children has the user focus.  It is a pre-requisite that the associated @VectorScene has been
linked to a @Surface.

The prototype for the callback is as follows, whereby Qualifers are `KQ` flags and the Code is a `K` constant
representing the raw key value.  The Unicode value is the resulting character when the qualifier and code are
translated through the user's keymap.

```
ERROR callback(*Viewport, LONG Qualifiers, LONG Code, LONG Unicode);
```

If the callback returns `ERR_Terminate` then the subscription will be ended.  All other error codes are ignored.

-INPUT-
ptr(func) Callback: Reference to a callback function that will receive input messages.

-ERRORS-
Okay:
NullArgs:
FieldNotSet: The VectorScene has no reference to a Surface.
AllocMemory:
Function: A call to gfxSubscribeInput() failed.

*********************************************************************************************************************/

static ERROR VECTOR_SubscribeKeyboard(objVector *Self, struct vecSubscribeKeyboard *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Callback)) return log.warning(ERR_NullArgs);

   if (!Self->Scene->SurfaceID) return log.warning(ERR_FieldNotSet);

   if (!Self->KeyboardSubscriptions) {
      Self->KeyboardSubscriptions = new (std::nothrow) std::vector<KeyboardSubscription>;
      if (!Self->KeyboardSubscriptions) return log.warning(ERR_AllocMemory);
   }

   Self->Scene->KeyboardSubscriptions.emplace(Self);
   Self->KeyboardSubscriptions->emplace_back(*Args->Callback);
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
TracePath: Returns the coordinates for a vector path, using callbacks.

Any vector that generates a path can be traced by calling this method.  Tracing allows the caller to follow the path for
each pixel that would be drawn if the path were to be rendered with a stroke size of 1.  The prototype of the callback
function is `ERROR Function(OBJECTPTR Vector, LONG Index, LONG Command, DOUBLE X, DOUBLE Y)`.

The Vector parameter refers to the vector targeted by the method.  The Index is an incrementing counter that reflects
the currently plotted point.  The X and Y parameters reflect the coordinate of a point on the path.

If the Callback returns `ERR_Terminate`, then no further coordinates will be processed.

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

   if (Self->Dirty) gen_vector_tree((objVector *)Self);
   if (!Self->BasePath.total_vertices()) return ERR_NoData;

   Self->BasePath.rewind(0);

   DOUBLE x, y;
   LONG cmd = -1;

  if (Args->Callback->Type IS CALL_STDC) {
      auto routine = ((void (*)(objVector *, LONG, LONG, DOUBLE, DOUBLE))(Args->Callback->StdC.Routine));

      parasol::SwitchContext context(GetParentContext());

      LONG index = 0;
      do {
        cmd = Self->BasePath.vertex(&x, &y);
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
      args[0].Long = Self->Head.UID;

      OBJECTPTR script;
      if ((script = Args->Callback->Script.Script)) {
         LONG index = 0;
         do {
           cmd = Self->BasePath.vertex(&x, &y);
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

/*********************************************************************************************************************

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

*********************************************************************************************************************/

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

/*********************************************************************************************************************

-FIELD-
Cursor: The mouse cursor to display when the pointer is within the vector's boundary.

The Cursor field declares the pointer's cursor image to display within the vector's boundary.  The cursor will
automatically switch to the specified image when it enters the boundary defined by the vector's path.  This effect
lasts until the cursor vacates the area.

It is a pre-requisite that the associated @VectorScene has been linked to a @Surface.

****************************************************************************/

static ERROR VECTOR_SET_Cursor(objVector *Self, LONG Value)
{
   Self->Cursor = Value;

   if (Self->Head.Flags & NF_INITIALISED) {
      // Send a dummy input event to refresh the cursor

      DOUBLE x, y, absx, absy;

      gfxGetRelativeCursorPos(Self->Scene->SurfaceID, &x, &y);
      gfxGetCursorPos(&absx, &absy);

      const InputEvent event = {
         .Next        = NULL,
         .Value       = 0,
         .Timestamp   = 0,
         .RecipientID = Self->Scene->SurfaceID,
         .OverID      = Self->Scene->SurfaceID,
         .AbsX        = absx,
         .AbsY        = absy,
         .X           = x,
         .Y           = y,
         .DeviceID    = 0,
         .Type        = JET_ABS_X,
         .Flags       = JTYPE_MOVEMENT,
         .Mask        = JTYPE_MOVEMENT
      };
      scene_input_events(&event, 0);
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
DashArray: Controls the pattern of dashes and gaps used to stroke paths.

The DashArray is a list of lengths that alternate between dashes and gaps.  If an odd number of values is provided,
then the list of values is repeated to yield an even number of values.  Thus `5,3,2` is equivalent to
`5,3,2,5,3,2`.

*********************************************************************************************************************/

static ERROR VECTOR_GET_DashArray(objVector *Self, DOUBLE **Value, LONG *Elements)
{
   if (Self->DashArray) {
      *Value    = Self->DashArray->values.data();
      *Elements = Self->DashArray->values.size();
   }
   else {
      *Value    = NULL;
      *Elements = 0;
   }
   return ERR_Okay;
}

static ERROR VECTOR_SET_DashArray(objVector *Self, DOUBLE *Value, LONG Elements)
{
   if (Self->DashArray) { delete Self->DashArray; Self->DashArray = NULL; }

   if ((Value) and (Elements >= 2)) {
      LONG total = Elements;
      if (total & 1) total++; // There must be an even count of dashes and gaps.

      Self->DashArray = new (std::nothrow) DashedStroke(Self->BasePath, total);
      if (Self->DashArray) {
         Self->DashArray->values.assign(*Value, Elements);
         if (total > Elements) Self->DashArray->values[Elements] = 0;

         DOUBLE total_length = 0;
         for (LONG i=0; i < total-1; i+=2) {
            Self->DashArray->path.add_dash(Value[i], Value[i+1]);
            total_length += Value[i] + Value[i+1];
         }

         // The stroke-dashoffset is used to set how far into dash pattern to start the pattern.  E.g. a
         // value of 5 means that the entire pattern is shifted 5 pixels to the left.

         if (Self->DashOffset > 0) Self->DashArray->path.dash_start(Self->DashOffset);
         else if (Self->DashOffset < 0) Self->DashArray->path.dash_start(total_length + Self->DashOffset);
      }
      else return ERR_AllocMemory;
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
DashOffset: The distance into the dash pattern to start the dash.  Can be a negative number.

The DashOffset can be set in conjunction with the #DashArray to shift the dash pattern to the left.  If the offset is
negative then the shift will be to the right.

*********************************************************************************************************************/

static ERROR VECTOR_SET_DashOffset(objVector *Self, DOUBLE Value)
{
   Self->DashOffset = Value;
   if (Self->DashArray) {
      if (Self->DashOffset > 0) Self->DashArray->path.dash_start(Self->DashOffset);
      else Self->DashArray->path.dash_start(Self->DashArray->path.dash_length() + Self->DashOffset);
   }
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
EnableBkgd: If true, allows filters to use BackgroundImage and BackgroundAlpha source types.

The EnableBkgd option must be set to true if a section of the vector tree uses filters that have 'BackgroundImage' or
'BackgroundAlpha' as a source.  If it is not set, then filters using BackgroundImage and BackgroundAlpha references
will not produce the expected behaviour.

The EnableBkgd option can be enabled on Vector sub-classes @VectorGroup, @VectorPattern and @VectorViewport.  All other
sub-classes will ignore the option if used.

*********************************************************************************************************************/

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

/*********************************************************************************************************************

-FIELD-
Fill: Defines the fill painter using SVG's IRI format.

The painter used for filling a vector path can be defined through this field.  The string is parsed through the
~ReadPainter() function in the Vector module.  Please refer to it for further details on valid formatting.

*********************************************************************************************************************/

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

/*********************************************************************************************************************

-FIELD-
FillColour: Defines a solid colour for filling the vector path.

Set the FillColour field to define a solid colour for filling the vector path.  The colour is defined as an array
of four 32-bit floating point values between 0 and 1.0.  The array elements consist of Red, Green, Blue and Alpha
values in that order.

If the Alpha component is set to zero then the FillColour will be ignored by the renderer.

*********************************************************************************************************************/

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

/*********************************************************************************************************************

-FIELD-
FillOpacity: The opacity to use when filling the vector.

The FillOpacity value is used by the painting algorithm when it is rendering a filled vector.  It is multiplied with
the #Opacity to determine a final opacity value for the render.

*********************************************************************************************************************/

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

/*********************************************************************************************************************

-FIELD-
Filter: Assign a post-effects filter to a vector.

This field assigns a graphics filter to the rendering pipeline of the vector.  The filter must initially be created
using the @VectorFilter class and added to a VectorScene using @VectorScene.AddDef().  The filter can
then be referenced by ID in the Filter field of any vector object.  Please refer to the @VectorFilter class
for further details on filter configuration.

The Filter value can be in the format `ID` or `url(#ID)` according to client preference.

*********************************************************************************************************************/

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

/*********************************************************************************************************************
-FIELD-
FillRule: Determines the algorithm to use when filling the shape.

The FillRule field indicates the algorithm which is to be used to determine what parts of the canvas are included
when filling the shape. For a simple, non-intersecting path, it is intuitively clear what region lies "inside";
however, for a more complex path, such as a path that intersects itself or where one sub-path encloses another, the
interpretation of "inside" is not so obvious.

*********************************************************************************************************************/

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

/*********************************************************************************************************************
-FIELD-
ID: String identifier for a vector.

The ID field is provided for the purpose of SVG support.  Where possible we would recommend that you use the
existing object name and automatically assigned ID's for identifiers.

*********************************************************************************************************************/

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

/*********************************************************************************************************************
-FIELD-
InnerJoin: Adjusts the handling of thickly stroked paths that cross back at the join.
Lookup: VIJ

The InnerJoin value is used to make very technical adjustments to the way that paths are stroked when they form
corners.  Visually, the impact of this setting is only noticeable when a path forms an awkward corner that crosses
over itself - usually due to the placement of bezier control points.

The available settings are `MITER`, `ROUND`, `BEVEL`, `JAG` and `INHERIT`.  The default of `MITER` is recommended as
it is the fastest, but `ROUND` produces the best results in ensuring that the stroked path is filled correctly.  The
most optimal approach is to use the default setting and switch to `ROUND` if issues are noted near the corners of the
path.

*********************************************************************************************************************/

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

/*********************************************************************************************************************

-FIELD-
InnerMiterLimit: Private. No internal documentation exists for this feature.

-FIELD-
LineCap: The shape to be used at the start and end of a stroked path.
Lookup: VLC

LineCap is the equivalent of SVG's stroke-linecap attribute.  It defines the shape to be used at the start and end
of a stroked path.

*********************************************************************************************************************/

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

/*********************************************************************************************************************
-FIELD-
LineJoin: The shape to be used at path corners that are stroked.
Lookup: VLJ

LineJoin is the equivalent of SVG's stroke-linejoin attribute.  It defines the shape to be used at path corners
that are being stroked.

*********************************************************************************************************************/

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

/*********************************************************************************************************************
-FIELD-
Mask: Reference a VectorClip object here to apply a clipping mask to the rendered vector.

A mask can be applied to a vector by setting the Mask field with a reference to a @VectorClip object.  Please
refer to the @VectorClip class for further information.

*********************************************************************************************************************/

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

/*********************************************************************************************************************

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

*********************************************************************************************************************/

static ERROR VECTOR_SET_MiterLimit(objVector *Self, DOUBLE Value)
{
   parasol::Log log;

   if (Value >= 1.0) {
      Self->MiterLimit = Value;
      return ERR_Okay;
   }
   else return log.warning(ERR_InvalidValue);
}

/*********************************************************************************************************************
-FIELD-
Morph: Enables morphing of the vector to a target path.

If the Morph field is set to a Vector object that generates a path, the vector will be morphed to follow the target
vector's path shape.  This works particularly well for text and shapes that follow a horizontal path that is much wider
than it is tall.

Squat shapes will fare poorly if morphed, so experimentation may be necessary to understand how the morph feature is
best utilised.

*********************************************************************************************************************/

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

/*********************************************************************************************************************

-FIELD-
MorphFlags: Optional flags that affect morphing.

*********************************************************************************************************************/

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

/*********************************************************************************************************************

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

*********************************************************************************************************************/

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

/*********************************************************************************************************************

-FIELD-
NumericID: A unique identifier for the vector.

This field assigns a numeric ID to a vector.  Alternatively it can also reflect a case-sensitive hash of the
#ID field if that has been defined previously.

If NumericID is set by the client, then any value in #ID will be immediately cleared.

*********************************************************************************************************************/

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

/*********************************************************************************************************************

-FIELD-
Opacity: Defines an overall opacity for the vector's graphics.

The overall opacity of a vector can be defined here using a value between 0 and 1.0.  The value will be multiplied
with other opacity settings as required during rendering.  For instance, when filling a vector the opacity will be
calculated as `#FillOpacity * Opacity`.

*********************************************************************************************************************/

static ERROR VECTOR_SET_Opacity(objVector *Self, DOUBLE Value)
{
   if ((Value >= 0) and (Value <= 1.0)) {
      Self->Opacity = Value;
      return ERR_Okay;
   }
   else return ERR_OutOfRange;
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

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

/*********************************************************************************************************************

-FIELD-
Scene: Short-cut to the top-level @VectorScene.

All vectors are required to be grouped within the hierarchy of a @VectorScene.  This requirement is enforced
on initialisation and a reference to the top-level @VectorScene is recorded in this field.

-FIELD-
Sequence: Convert the vector's path to the equivalent SVG path string.

The Sequence is a string of points and instructions that define the path.  It is based on the SVG standard for the path
element `d` attribute, but also provides some additional features that are present in the vector engine.  Commands are
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

*********************************************************************************************************************/

static ERROR VECTOR_GET_Sequence(objVector *Self, STRING *Value)
{
   parasol::Log log;

   if (!Self->GeneratePath) return log.warning(ERR_Mismatch); // Path generation must be supported by the vector.

   if (Self->Dirty) gen_vector_tree((objVector *)Self);
   if (!Self->BasePath.total_vertices()) return ERR_NoData;

   char seq[4096] = "";

   // See agg_path_storage.h for vertex traversal
   // All vertex coordinates are stored in absolute format.

   agg::path_storage &base = Self->BasePath;

   // TODO: Decide what to do with bounding box information, if anything.
   DOUBLE bx1, by1, bx2, by2;
   bounding_rect_single(base, 0, &bx1, &by1, &bx2, &by2);
   bx1 += Self->FinalX;
   bx2 += Self->FinalX;
   by1 += Self->FinalY;
   by2 += Self->FinalY;

   DOUBLE x, y, x2, y2, x3, y3, last_x = 0, last_y = 0;
   LONG p = 0;
   for (ULONG i=0; i < base.total_vertices(); i++) {
      LONG cmd = base.command(i);
      //LONG cmd_flags = cmd & (~agg::path_cmd_mask);
      cmd &= agg::path_cmd_mask;

      // NB: A Z closes the path by drawing a line to the start of the first point.  A 'dead stop' is defined by
      // leaving out the Z.

      switch(cmd) {
         case agg::path_cmd_stop: // PE_ClosePath
            seq[p++] = 'Z';
            break;

         case agg::path_cmd_move_to: // PE_Move
            base.vertex(i, &x, &y);
            p += StrFormat(seq+p, sizeof(seq)-p, "M%g,%g", x, y);
            last_x = x;
            last_y = y;
            break;

         case agg::path_cmd_line_to: // PE_Line
            base.vertex(i, &x, &y);
            p += StrFormat(seq+p, sizeof(seq)-p, "L%g,%g", x, y);
            last_x = x;
            last_y = y;
            break;

         case agg::path_cmd_curve3: // PE_QuadCurve
            base.vertex(i, &x, &y);
            base.vertex(i+1, &x2, &y2); // End of line
            p += StrFormat(seq+p, sizeof(seq)-p, "q%g,%g,%g,%g", x - last_x, y - last_y, x2 - last_x, y2 - last_y);
            last_x = x;
            last_y = y;
            i += 1;
            break;

         case agg::path_cmd_curve4: // PE_Curve
            base.vertex(i, &x, &y);
            base.vertex(i+1, &x2, &y2);
            base.vertex(i+2, &x3, &y3); // End of line
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

/*********************************************************************************************************************

-FIELD-
Stroke: Defines the stroke of a path using SVG's IRI format.

The stroker used for rendering a vector path can be defined through this field.  The string is parsed through
the ~ReadPainter() function in the Vector module.  Please refer to it for further details on valid formatting.

*********************************************************************************************************************/

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

/*********************************************************************************************************************

-FIELD-
StrokeColour: Defines the colour of the path stroke in RGB float format.

The colour that will be used in stroking a path is defined by the StrokeColour field.  The colour is composed of
4 floating point values comprising Red, Green, Blue and Alpha.  The intensity of each colour component is determined
by a value range between 0 and 1.0.  If the Alpha value is zero, a coloured stroke will not be applied when drawing
the vector.

This field is complemented by the #StrokeOpacity and #Stroke fields.

*********************************************************************************************************************/

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

/*********************************************************************************************************************
-FIELD-
StrokeOpacity: Defines the opacity of the path stroke.

The StrokeOpacity value expresses the opacity of a path stroke as a value between 0 and 1.0.  A value of zero would
render the stroke invisible and the maximum value of one would render it opaque.

Please note that thinly stroked paths may not be able to appear as fully opaque in some cases due to anti-aliased
rendering.

*********************************************************************************************************************/

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

/*********************************************************************************************************************
-FIELD-
StrokeWidth: The width to use when stroking the path.

The StrokeWidth defines the pixel width of a path when it is stroked.  The path will not be stroked if the value is
zero.  A percentage can be used to define the stroke width if it should be relative to the size of the viewbox
(along its diagonal).  Note that this incurs a slight computational penalty when drawing.

The size of the stroke is also affected by scaling factors imposed by transforms and viewports.

*********************************************************************************************************************/

static ERROR VECTOR_GET_StrokeWidth(objVector *Self, Variable *Value)
{
   DOUBLE val;

   if (Value->Type & FD_PERCENTAGE) {
      if (Self->RelativeStrokeWidth) val = Self->StrokeWidth * 100.0;
      else val = 0;
   }
   else val = Self->fixed_stroke_width();

   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR VECTOR_SET_StrokeWidth(objVector *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if ((val >= 0.0) and (val <= 1000.0)) {
      if (Value->Type & FD_PERCENTAGE) {
         Self->StrokeWidth = val * 0.01;
         Self->RelativeStrokeWidth = true;
      }
      else {
         Self->StrokeWidth = val;
         Self->RelativeStrokeWidth = false;
      }
      return ERR_Okay;
   }
   else return ERR_OutOfRange;
}

/*********************************************************************************************************************

-FIELD-
Matrices: A linked list of transform matrices that have been applied to the vector.

All transforms that have been allocated via ~Vector.NewMatrix() can be read from the Matrices field.  Each transform is
represented by the `VectorMatrix` structure, and are linked in the order in which they are added to the vector.

&VectorMatrix

-FIELD-
Transition: Reference a VectorTransition object here to apply multiple transforms over the vector's path.

A transition can be applied by setting this field with a reference to a @VectorTransition object.  Please
refer to the @VectorTransition class for further information.

Not all vector types are well-suited or adapted to the use of transitions.  At the time of writing, only @VectorText
and @VectorWave are able to take full advantage of this feature.

*********************************************************************************************************************/

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

/*********************************************************************************************************************

-FIELD-
Visibility: Controls the visibility of a vector and its children.
-END-

*********************************************************************************************************************/

//********************************************************************************************************************
// For sending events to the client

static void send_feedback(objVector *Vector, LONG Event)
{
   if (!(Vector->Head.Flags & NF_INITIALISED)) return;
   if (!Vector->FeedbackSubscriptions) return;

   for (auto it=Vector->FeedbackSubscriptions->begin(); it != Vector->FeedbackSubscriptions->end(); ) {
      ERROR result;
      auto &sub = *it;
      if (sub.Mask & Event) {
         sub.Mask &= ~Event; // Turned off to prevent recursion

         if (sub.Callback.Type IS CALL_STDC) {
            parasol::SwitchContext ctx(sub.Callback.StdC.Context);
            auto callback = (ERROR (*)(objVector *, LONG))sub.Callback.StdC.Routine;
            result = callback(Vector, Event);
         }
         else if (sub.Callback.Type IS CALL_SCRIPT) {
            // In this implementation the script function will receive all the events chained via the Next field
            ScriptArg args[] = {
               { "Vector", FDF_OBJECT, { .Address = Vector } },
               { "Event",  FDF_LONG,   { .Long = Event } }
            };
            scCallback(sub.Callback.Script.Script, sub.Callback.Script.ProcedureID, args, ARRAYSIZE(args), &result);
         }

         sub.Mask |= Event;

         if (result IS ERR_Terminate) Vector->FeedbackSubscriptions->erase(it);
         else it++;
      }
      else it++;
   }
}

//********************************************************************************************************************
// Receiver for keyboard events

static ERROR vector_keyboard_events(objVector *Vector, const evKey *Event)
{
   for (auto it=Vector->KeyboardSubscriptions->begin(); it != Vector->KeyboardSubscriptions->end(); ) {
      ERROR result;
      auto &sub = *it;
      if (sub.Callback.Type IS CALL_STDC) {
         parasol::SwitchContext ctx(sub.Callback.StdC.Context);
         auto callback = (ERROR (*)(objVector *, LONG, LONG, LONG))sub.Callback.StdC.Routine;
         result = callback(Vector, Event->Qualifiers, Event->Code, Event->Unicode);
      }
      else if (sub.Callback.Type IS CALL_SCRIPT) {
         // In this implementation the script function will receive all the events chained via the Next field
         ScriptArg args[] = {
            { "Vector",     FDF_OBJECT, { .Address = Vector } },
            { "Qualifiers", FDF_LONG,   { .Long = Event->Qualifiers } },
            { "Code",       FDF_LONG,   { .Long = Event->Code } },
            { "Unicode",    FDF_LONG,   { .Long = Event->Unicode } }
         };
         scCallback(sub.Callback.Script.Script, sub.Callback.Script.ProcedureID, args, ARRAYSIZE(args), &result);
      }

      if (result IS ERR_Terminate) Vector->KeyboardSubscriptions->erase(it);
      else it++;
   }

   return ERR_Okay;
}

//********************************************************************************************************************

DOUBLE rkVector::fixed_stroke_width()
{
   if (this->RelativeStrokeWidth) {
      return get_parent_diagonal(this) * this->StrokeWidth;
   }
   else return this->StrokeWidth;
}

//********************************************************************************************************************

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

#include "vector_def.c"

static const FieldArray clVectorFields[] = {
   { "Child",            FDF_OBJECT|FD_R,              ID_VECTOR, NULL, NULL },
   { "Scene",            FDF_OBJECT|FD_R,              ID_VECTORSCENE, NULL, NULL },
   { "Next",             FDF_OBJECT|FD_RW,             ID_VECTOR, NULL, (APTR)VECTOR_SET_Next },
   { "Prev",             FDF_OBJECT|FD_RW,             ID_VECTOR, NULL, (APTR)VECTOR_SET_Prev },
   { "Parent",           FDF_OBJECT|FD_R,              0, NULL, NULL },
   { "Matrices",         FDF_POINTER|FDF_STRUCT|FDF_R, (MAXINT)"VectorMatrix", NULL, NULL },
   { "StrokeOpacity",    FDF_DOUBLE|FDF_RW,            0, (APTR)VECTOR_GET_StrokeOpacity, (APTR)VECTOR_SET_StrokeOpacity },
   { "FillOpacity",      FDF_DOUBLE|FDF_RW,            0, (APTR)VECTOR_GET_FillOpacity, (APTR)VECTOR_SET_FillOpacity },
   { "Opacity",          FDF_DOUBLE|FD_RW,             0, NULL, (APTR)VECTOR_SET_Opacity },
   { "MiterLimit",       FDF_DOUBLE|FD_RW,             0, NULL, (APTR)VECTOR_SET_MiterLimit },
   { "InnerMiterLimit",  FDF_DOUBLE|FD_RW,             0, NULL, NULL },
   { "DashOffset",       FDF_DOUBLE|FD_RW,             0, NULL, (APTR)VECTOR_SET_DashOffset },
   { "Visibility",       FDF_LONG|FDF_LOOKUP|FDF_RW,   (MAXINT)&clVectorVisibility, NULL, NULL },
   { "Flags",            FDF_LONGFLAGS|FDF_RI,         (MAXINT)&clVectorFlags, NULL, NULL },
   { "Cursor",           FDF_LONG|FDF_LOOKUP|FDF_RW,   (MAXINT)&clVectorCursor, NULL, (APTR)VECTOR_SET_Cursor },
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
   { "StrokeWidth",  FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)VECTOR_GET_StrokeWidth, (APTR)VECTOR_SET_StrokeWidth },
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
