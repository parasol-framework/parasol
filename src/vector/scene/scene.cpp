/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
VectorScene: Manages the scene graph for a collection of vectors.

The VectorScene class acts as a container and control point for the management of vector definitions.  Its main
purpose is to draw the scene to a target @Bitmap or @Surface provided by the client.

Vector scenes are created by initialising multiple Vector objects such as @VectorPath and @VectorViewport and
positioning them within a vector tree.  The VectorScene must lie at the root.

The default mode of operation is for scenes to be manually drawn, for which the client must set the target #Bitmap
and call the #Draw() action as required.  Automated drawing can be enabled by setting the target #Surface prior
to initialisation.  In automated mode the #PageWidth and #PageHeight will reflect the dimensions of the target surface
at all times.

Vector definitions can be saved and loaded from permanent storage by using the @SVG class.
-END-

*********************************************************************************************************************/

#include "agg_rasterizer_outline_aa.h"
#include "agg_curves.h"
#include "agg_image_accessors.h"
#include "agg_renderer_base.h"
#include "agg_renderer_outline_aa.h"
#include "agg_span_interpolator_linear.h"
#include "agg_renderer_outline_image.h"
#include "agg_conv_smooth_poly1.h"
#include "agg_span_gradient.h"
#include "agg_conv_contour.h"

//#include "../vector.h"

#include "scene_draw.cpp"

static ERROR VECTORSCENE_Reset(extVectorScene *, APTR);

static void scene_key_event(extVectorScene *, evKey *, LONG);
static void process_resize_msgs(extVectorScene *);

static void render_to_surface(extVectorScene *Self, objSurface *Surface, objBitmap *Bitmap)
{
   Self->Bitmap = Bitmap;

   if ((!Self->PageWidth) or (!Self->PageHeight)) {
      if (Self->Viewport) mark_dirty(Self->Viewport, RC_BASE_PATH|RC_TRANSFORM); // Base-paths need to be recomputed if they use relative coordinates.
   }

   acDraw(Self);

   Self->Bitmap = NULL;
}

//********************************************************************************************************************

static ERROR VECTORSCENE_ActionNotify(extVectorScene *Self, struct acActionNotify *Args)
{
   if (Args->ActionID IS AC_Free) {
      if (Self->SurfaceID IS Args->ObjectID) {
         Self->SurfaceID = 0;
      }
   }
   else if (Args->ActionID IS AC_Redimension) {
      auto resize = (struct acRedimension *)Args->Args;

      if (Self->Flags & VPF_RESIZE) {
         Self->PageWidth = resize->Width;
         Self->PageHeight = resize->Height;

         if (Self->Viewport) {
            mark_dirty(Self->Viewport, RC_BASE_PATH|RC_TRANSFORM); // Base-paths need to be recomputed if they use relative coordinates.
         }

         ActionMsg(MT_DrwScheduleRedraw, Self->SurfaceID, NULL);
      }
   }
   else if (Args->ActionID IS AC_LostFocus) {
      if (Self->KeyHandle) {
         UnsubscribeEvent(Self->KeyHandle);
         Self->KeyHandle = NULL;
      }
   }
   else if (Args->ActionID IS AC_Focus) {
      if (!Self->KeyHandle) {
         auto callback = make_function_stdc(scene_key_event);
         SubscribeEvent(EVID_IO_KEYBOARD_KEYPRESS, &callback, Self, &Self->KeyHandle);
      }
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
AddDef: Adds a new definition to a vector tree.

This method will add a new definition object to the root of a vector tree.  This feature is provided to support SVG
style referencing for features such as gradients, images and patterns.  By providing a name with the definition object,
the object can then be referenced in URL strings.

For instance, if creating a gradient with a name of "redGradient" it would be possible to reference it with
`url(#redGradient)` in common graphics attributes such as `fill` and `stroke`.

-INPUT-
cstr Name: The unique name to associate with the definition.
obj Def: Reference to the definition object.

-ERRORS-
Okay
NullArgs
ResourceExists: The given name is already in use as a definition.
InvalidObject: The definition is not an accepted object class.
UnsupportedOwner: The definition is not owned by the scene.

*********************************************************************************************************************/

static ERROR VECTORSCENE_AddDef(extVectorScene *Self, struct scAddDef *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Name) or (!Args->Def)) return log.warning(ERR_NullArgs);

   if (Self->HostScene) { // Defer all definitions if a hosting scene is active.
      return Action(MT_ScAddDef, Self->HostScene, Args);
   }

   OBJECTPTR def = Args->Def;

   if ((def->ClassID IS ID_VECTORSCENE) or
       (def->ClassID IS ID_VECTOR) or
       (def->ClassID IS ID_VECTORGRADIENT) or
       (def->ClassID IS ID_VECTORIMAGE) or
       (def->ClassID IS ID_VECTORPATH) or
       (def->ClassID IS ID_VECTORPATTERN) or
       (def->ClassID IS ID_VECTORFILTER) or
       (def->ClassID IS ID_VECTORTRANSITION) or
       (def->SubID IS ID_VECTORCLIP)) {
      // The use of this object as a definition is valid.
   }
   else return log.warning(ERR_InvalidObject);

   // If the resource does not belong to the Scene object, this can lead to invalid pointer references

   if (def->OwnerID != Self->UID) {
      OBJECTID owner_id = def->OwnerID;
      while ((owner_id) and (owner_id != Self->UID)) {
         owner_id = GetOwnerID(owner_id);
      }

      if (!owner_id) {
         log.warning("The %s must belong to VectorScene #%d, but is owned by object #%d.", def->Class->ClassName, Self->UID, def->OwnerID);
         return ERR_UnsupportedOwner;
      }
   }

   // TODO: Subscribe to the Free() action of the definition object so that we can avoid invalid pointer references.

   log.extmsg("Adding definition '%s' referencing %s #%d", Args->Name, def->Class->ClassName, def->UID);

   APTR data;
   if (!Self->Defs) {
      if (!(Self->Defs = VarNew(64, KSF_CASE))) {
         return log.warning(ERR_AllocMemory);
      }
   }
   else if (!VarGet(Self->Defs, Args->Name, &data, NULL)) { // Check that the definition name is unique.
      log.msg("The vector definition name '%s' is already in use.", Args->Name);
      return ERR_ResourceExists;
   }

   VectorDef vd;
   vd.Object = def;
   VarSet(Self->Defs, Args->Name, &vd, sizeof(vd));
   return ERR_Okay;
}

/*********************************************************************************************************************

-ACTION-
Draw: Renders the scene to a bitmap.

The Draw action will render the scene to the target #Bitmap immediately.  If #Bitmap is NULL, an error will be
returned.

In addition, the #RenderTime field will be updated if the `RENDER_TIME` flag is defined.

-ERRORS-
Okay
FieldNotSet: The Bitmap field is NULL.

*********************************************************************************************************************/

static ERROR VECTORSCENE_Draw(extVectorScene *Self, struct acDraw *Args)
{
   parasol::Log log;
   objBitmap *bmp;

   if (!(bmp = Self->Bitmap)) return log.warning(ERR_FieldNotSet);

   // Any pending resize messages for viewports must be processed prior to drawing.

   process_resize_msgs(Self);

   // Allocate the adaptor, or if the existing adaptor doesn't match the Bitmap pixel type, reallocate it.

   VMAdaptor *adaptor;

   const LONG type = (bmp->BitsPerPixel << 8) | (bmp->BytesPerPixel);
   if (type != Self->AdaptorType) {
      if (Self->Adaptor) {
         delete Self->Adaptor;
         Self->Adaptor = NULL;
      }

      adaptor = new (std::nothrow) VMAdaptor;
      if (!adaptor) return log.warning(ERR_AllocMemory);
      adaptor->Scene = Self;
      Self->Adaptor = adaptor;
      Self->AdaptorType = type;
   }
   else adaptor = static_cast<VMAdaptor *> (Self->Adaptor);

   if (Self->Flags & VPF_RENDER_TIME) { // Client wants to know how long the rendering takes to complete
      LARGE time = PreciseTime();
      adaptor->draw(bmp);
      if ((Self->RenderTime = PreciseTime() - time) < 1) Self->RenderTime = 1;
   }
   else adaptor->draw(bmp);

// For debugging purposes, draw a boundary around the target area.
//   static RGB8 highlightA = { .Red = 255, .Green = 0, .Blue = 0, .Alpha = 255 };
//   ULONG highlight = PackPixelRGBA(bmp, &highlightA);
//   gfxDrawRectangle(bmp, bmp->Clip.Left, bmp->Clip.Top, bmp->Clip.Right-bmp->Clip.Left, bmp->Clip.Bottom-bmp->Clip.Top, highlight, 0);

   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
FindDef: Search for a vector definition by name.

Use the FindDef method to search for a vector definition by name.  A reference to the definition will be returned if
the search is successful.

Definitions are created with the #AddDef() method.

-INPUT-
cstr Name: The name of the definition.
&obj Def: A pointer to the definition is returned here if discovered.

-ERRORS-
Okay
NullArgs
Search: A definition with the given Name was not found.
-END-

*********************************************************************************************************************/

static ERROR VECTORSCENE_FindDef(extVectorScene *Self, struct scFindDef *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Name)) return log.warning(ERR_NullArgs);

   if (Self->HostScene) return Action(MT_ScFindDef, Self->HostScene, Args);

   CSTRING name = Args->Name;

   if (*name IS '#') name = name + 1;
   else if (!StrCompare("url(#", name, 5, 0)) {
      char newname[80];
      UWORD i;
      name += 5;
      for (i=0; (name[i] != ')') and (name[i]) and (i < sizeof(newname)-1); i++) newname[i] = name[i];
      newname[i] = 0;

      VectorDef *vd;
      if (!VarGet(Self->Defs, newname, &vd, NULL)) {
         Args->Def = vd->Object;
         return ERR_Okay;
      }
      else return ERR_Search;
   }

   VectorDef *vd;
   if (!VarGet(Self->Defs, name, &vd, NULL)) {
      Args->Def = vd->Object;
      return ERR_Okay;
   }
   else return ERR_Search;
}

//********************************************************************************************************************

static ERROR VECTORSCENE_Free(extVectorScene *Self, APTR Args)
{
   Self->~extVectorScene();

   if (Self->Viewport) Self->Viewport->Parent = NULL;
   if (Self->Adaptor)  { delete Self->Adaptor; Self->Adaptor = NULL; }
   if (Self->Buffer)   { delete Self->Buffer; Self->Buffer = NULL; }
   if (Self->Defs)     { FreeResource(Self->Defs); Self->Defs = NULL; }
   if (Self->InputHandle) { gfxUnsubscribeInput(Self->InputHandle); Self->InputHandle = 0; }

   if (Self->SurfaceID) {
      OBJECTPTR surface;
      if (!AccessObject(Self->SurfaceID, 5000, &surface)) {
         UnsubscribeAction(surface, 0);
         ReleaseObject(surface);
      }
   }
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR VECTORSCENE_Init(extVectorScene *Self, APTR Void)
{
   // Setting the SurfaceID is optional and enables auto-rendering to the display.  The
   // alternative for the client is to set the Bitmap field and manage rendering manually.
   //
   // As long as PageWidth and PageHeight aren't set prior to initialisation, the scene will
   // match the width and height of the surface at all times when in this mode.

   if (Self->SurfaceID) {
      OBJECTPTR surface;
      if ((Self->SurfaceID) and (!AccessObject(Self->SurfaceID, 5000, &surface))) {
         auto callback = make_function_stdc(render_to_surface);
         struct drwAddCallback args = { &callback };
         Action(MT_DrwAddCallback, surface, &args);

         if ((!Self->PageWidth) or (!Self->PageHeight)) {
            Self->Flags |= VPF_RESIZE;
            GetLong(surface, FID_Width, &Self->PageWidth);
            GetLong(surface, FID_Height, &Self->PageHeight);
         }

         SubscribeActionTags(surface, AC_Redimension, AC_Free, AC_Focus, AC_LostFocus, TAGEND);

         if (surface->Flags & RNF_HAS_FOCUS) {
            auto callback = make_function_stdc(scene_key_event);
            SubscribeEvent(EVID_IO_KEYBOARD_KEYPRESS, &callback, Self, &Self->KeyHandle);
         }

         ReleaseObject(surface);
      }

      auto callback = make_function_stdc(scene_input_events);
      if (gfxSubscribeInput(&callback, Self->SurfaceID, JTYPE_MOVEMENT|JTYPE_FEEDBACK|JTYPE_BUTTON|JTYPE_REPEATED|JTYPE_EXT_MOVEMENT, 0, &Self->InputHandle)) {
         return ERR_Function;
      }
   }

   Self->Cursor = PTR_DEFAULT;

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR VECTORSCENE_NewObject(extVectorScene *Self, APTR Void)
{
   Self->SampleMethod = VSM_BILINEAR;

   new (Self) extVectorScene;

   // Please refer to the Reset action for setting variable defaults
   return VECTORSCENE_Reset(Self, NULL);
}

/*********************************************************************************************************************
-ACTION-
Redimension: Redefines the size of the page.
-END-
*********************************************************************************************************************/

static ERROR VECTORSCENE_Redimension(extVectorScene *Self, struct acRedimension *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Args->Width >= 1.0)  Self->PageWidth  = F2T(Args->Width);
   if (Args->Height >= 1.0) Self->PageHeight = F2T(Args->Height);

   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Reset: Clears all registered definitions and resets field values.  Child vectors are unmodified.
-END-
*********************************************************************************************************************/

static ERROR VECTORSCENE_Reset(extVectorScene *Self, APTR Void)
{
   if (Self->Adaptor) { delete Self->Adaptor; Self->Adaptor = NULL; }
   if (Self->Buffer)  { delete Self->Buffer; Self->Buffer = NULL; }
   if (Self->Defs)    { FreeResource(Self->Defs); Self->Defs = NULL; }

   Self->Gamma = 1.0;
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Resize: Redefines the size of the page.
-END-
*********************************************************************************************************************/

static ERROR VECTORSCENE_Resize(extVectorScene *Self, struct acResize *Args)
{
   if (!Args) return ERR_NullArgs;
   if (Args->Width >= 1.0)  Self->PageWidth  = F2T(Args->Width);
   if (Args->Height >= 1.0) Self->PageHeight = F2T(Args->Height);
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
SearchByID: Search for a vector by numeric ID.

This method will search a scene for an object that matches a given ID (vector ID's can be set with the NumericID and ID
fields).  If multiple vectors are using the same ID, repeated calls can be made to this method to find all of them.
This is achieved by calling this method on the vector that was last returned as a result.

Please note that searching for string-based ID's is achieved by converting the string to a case-sensitive hash
with ~Core.StrHash() and using that as the ID.

-INPUT-
int ID: The ID to search for.
&obj Result: This parameter will be updated with the discovered vector, or NULL if not found.

-ERRORS-
Okay
NullArgs
Search: A vector with a matching ID was not found.
-END-

*********************************************************************************************************************/

static ERROR VECTORSCENE_SearchByID(extVectorScene *Self, struct scSearchByID *Args)
{
   if (!Args) return ERR_NullArgs;
   Args->Result = NULL;

   objVector *vector = Self->Viewport;
   while (vector) {
      //log.msg("Search","%.3d: %p <- #%d -> %p Child %p", vector->Index, vector->Prev, vector->UID, vector->Next, vector->Child);
cont:
      if (vector->NumericID IS Args->ID) {
         Args->Result = vector;
         return ERR_Okay;
      }

      if (vector->Child) vector = vector->Child;
      else if (vector->Next) vector = vector->Next;
      else {
         while ((vector = (objVector *)get_parent(vector))) { // Unwind back up the stack, looking for the first Parent with a Next field.
            if (vector->ClassID != ID_VECTOR) return ERR_Search;
            if (vector->Next) {
               vector = vector->Next;
               goto cont;
            }
         }
         return ERR_Search;
      }
   }

   return ERR_Search;
}

/*********************************************************************************************************************
-FIELD-
Bitmap: Target bitmap for drawing vectors.

The target bitmap to use when drawing the vectors must be specified here.

*********************************************************************************************************************/

static ERROR SET_Bitmap(extVectorScene *Self, objBitmap *Value)
{
   if (Value) {
      if (Self->Buffer) delete Self->Buffer;

      Self->Buffer = new (std::nothrow) agg::rendering_buffer;
      if (Self->Buffer) {
         Self->Buffer->attach(Value->Data, Value->Width, Value->Height, Value->LineWidth);
         Self->Bitmap = Value;

         if (Self->Flags & VPF_BITMAP_SIZED) {
            Self->PageWidth = Value->Width;
            Self->PageHeight = Value->Height;
         }
      }
      else return ERR_Memory;
   }
   else Self->Buffer = NULL;

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Flags: Optional flags.

-FIELD-
Gamma: Private. Not currently implemented.

-FIELD-
HostScene: Refers to a top-level VectorScene object, if applicable.

Set HostScene to another VectorScene object if it is intended that this scene is a child of the other.  This allows
some traits such as vector definitions to be automatically inherited from the host scene.

This feature is useful in circumstances where a hidden group of vectors need to be managed separately, while retaining
access to established definitions and vectors in the main.

-FIELD-
PageHeight: The height of the page that contains the vector.

This value defines the pixel height of the page that contains the vector scene graph.  If the `RESIZE` #Flags
option is used then the viewport will be scaled to fit within the page.

****************************************************************************/

static ERROR SET_PageHeight(extVectorScene *Self, LONG Value)
{
   if (Value IS Self->PageHeight) return ERR_Okay;

   if (Value < 1) Self->PageHeight = 1;
   else Self->PageHeight = Value;

   if (Self->Viewport) mark_dirty(Self->Viewport, RC_BASE_PATH|RC_TRANSFORM); // Base-paths need to be recomputed if they use relative coordinates.
   return ERR_Okay;
}

/****************************************************************************

-FIELD-
PageWidth: The width of the page that contains the vector.

This value defines the pixel width of the page that contains the vector scene graph.  If the `RESIZE` #Flags
option is used then the viewport will be scaled to fit within the page.

****************************************************************************/

static ERROR SET_PageWidth(extVectorScene *Self, LONG Value)
{
   if (Value IS Self->PageWidth) return ERR_Okay;

   if (Value < 1) Self->PageWidth = 1;
   else Self->PageWidth = Value;

   if (Self->Viewport) mark_dirty(Self->Viewport, RC_BASE_PATH|RC_TRANSFORM);
   return ERR_Okay;
}

/****************************************************************************

-FIELD-
RenderTime: Returns the rendering time of the last scene.

RenderTime returns the rendering time of the last scene that was drawn, measured in microseconds.  This value can also
be used to compute frames-per-second with `1000000 / RenderTime`.

The `RENDER_TIME` flag should also be set before fetching this value, as it is required to enable the timing feature.  If
`RENDER_TIME` is not set, it will be set automatically so that subsequent calls succeed correctly.

****************************************************************************/

static ERROR GET_RenderTime(extVectorScene *Self, LARGE *Value)
{
   Self->Flags |= VPF_RENDER_TIME;
   *Value = Self->RenderTime;
   return ERR_Okay;
}

/****************************************************************************

-FIELD-
SampleMethod: The sampling method to use when interpolating images and patterns.

The SampleMethod controls the sampling algorithm that is used when images and patterns in the vector definition are affected
by rotate, skew and scale transforms.  The choice of method will have a significant impact on the speed and quality of
the images that are displayed in the rendered scene.  The recommended default is `BILINEAR`, which provides a
comparatively average result and execution speed.  The most advanced method is `BLACKMAN8`, which produces an excellent
level of quality at the cost of very poor execution speed.

-FIELD-
Surface: May refer to a Surface object for enabling automatic rendering.

Setting the Surface field will enable automatic rendering to a display surface.  The use of features such as input event handling
and user focus management will also require an associated surface as a pre-requisite.

*********************************************************************************************************************/

static ERROR SET_Surface(extVectorScene *Self, OBJECTID Value)
{
   Self->SurfaceID = Value;
   return ERR_Okay;
}

/****************************************************************************

-FIELD-
Viewport: References the first object in the scene, which must be a VectorViewport object.

The first object in the vector scene is referenced here.  It must belong to the @VectorViewport class, because it will
be used to define the size and location of the area rendered by the scene.

The Viewport field must not be set by the client.  The VectorViewport object will configure its ownership to
the VectorScene prior to initialisation.  The Viewport field value will then be set automatically when the
VectorViewport object is initialised.
-END-

*********************************************************************************************************************/

//********************************************************************************************************************
// Apply focus to a vector and all other vectors within that tree branch (not necessarily just the viewports).
// Also sends LostFocus notifications to vectors that previously had the focus.
// The glFocusList maintains the current focus state, with the most foreground vector at the beginning.

void apply_focus(extVectorScene *Scene, objVector *Vector)
{
   if (!Vector) return;

   const std::lock_guard<std::recursive_mutex> lock(glFocusLock);

   if ((!glFocusList.empty()) and (Vector IS glFocusList.front())) return;

   std::vector<objVector *> focus_gained; // The first reference is the most foreground object

   for (auto scan=Vector; scan; scan=(objVector *)scan->Parent) {
      if (scan->ClassID IS ID_VECTOR) {
         focus_gained.emplace_back(scan);
      }
      else break;
   }

#if 0
   std::string vlist;
   for (auto const fv : focus_gained) {
      char buffer[30];
      snprintf(buffer, sizeof(buffer), "#%d ", fv->UID);
      vlist.append(buffer);
   }

   log.msg("Vector focus list now: %s", vlist.c_str());
#endif

   // Report focus events to vector subscribers.

   auto focus_event = FM_HAS_FOCUS;
   for (auto const fgv : focus_gained) {
      bool no_focus = true, lost_focus_to_child = false, was_child_now_primary = false;

      if (!glFocusList.empty()) {
         no_focus = std::find(glFocusList.begin(), glFocusList.end(), fgv) IS glFocusList.end();
         if (!no_focus) {
            lost_focus_to_child = ((fgv IS glFocusList.front()) and (focus_event IS FM_CHILD_HAS_FOCUS));
            was_child_now_primary = ((fgv != glFocusList.front()) and (focus_event IS FM_HAS_FOCUS));
         }
      }

      if ((no_focus) or (lost_focus_to_child) or (was_child_now_primary)) {
         parasol::ScopedObjectLock<objVector> vec(fgv, 1000);
         if (vec.granted()) {
            send_feedback(fgv, focus_event);
            focus_event = FM_CHILD_HAS_FOCUS;
         }
      }
   }

   // Report lost focus events, starting from the foreground.

   for (auto const fv : glFocusList) {
      if (std::find(focus_gained.begin(), focus_gained.end(), fv) IS focus_gained.end()) {
         parasol::ScopedObjectLock<objVector> vec(fv, 1000);
         if (vec.granted()) send_feedback(fv, FM_LOST_FOCUS);
      }
      else break;
   }

   glFocusList = focus_gained;
}

//********************************************************************************************************************
// Build a list of all child viewports that have a bounding box intersecting with (X,Y).  Transforms
// are taken into account through use of BX1,BY1,BX2,BY2.  The list is sorted starting from the background to the
// foreground.

void get_viewport_at_xy_scan(objVector *Vector, std::vector<std::vector<objVectorViewport *>> &Collection, DOUBLE X, DOUBLE Y, LONG Branch)
{
   if ((size_t)Branch >= Collection.size()) Collection.resize(Branch+1);

   for (auto scan=Vector; scan; scan=scan->Next) {
      if (scan->SubID IS ID_VECTORVIEWPORT) {
         auto vp = (objVectorViewport *)scan;

         if (vp->Dirty) gen_vector_path((objVector *)vp);

         if ((X >= vp->vpBX1) and (Y >= vp->vpBY1) and (X < vp->vpBX2) and (Y < vp->vpBY2)) {
            Collection[Branch].emplace_back(vp);
         }
      }

      if (scan->Child) get_viewport_at_xy_scan(scan->Child, Collection, X, Y, Branch + 1);
   }
}

//********************************************************************************************************************

objVectorViewport * get_viewport_at_xy(extVectorScene *Scene, DOUBLE X, DOUBLE Y)
{
   std::vector<std::vector<objVectorViewport *>> viewports;
   get_viewport_at_xy_scan((objVector *)Scene->Viewport, viewports, X, Y, 0);

   // From front to back, determine the first path that the (X,Y) point resides in.

   for (auto branch = viewports.rbegin(); branch != viewports.rend(); branch++) {
      for (auto const vp : *branch) {
         // The viewport will generate a clip mask if complex transforms are applicable (other than scaling).
         // We can take advantage of this rather than generate our own path.

         if ((vp->vpClipMask) and (vp->vpClipMask->ClipPath)) {
            agg::rasterizer_scanline_aa<> raster;
            raster.add_path(vp->vpClipMask->ClipPath[0]);
            if (raster.hit_test(X, Y)) return vp;
         }
         else return vp; // If no complex transforms are present, the hit-test is passed
      }
   }

   // No child viewports were hit, revert to main

   return (objVectorViewport *)Scene->Viewport;
}

//********************************************************************************************************************

static void process_resize_msgs(extVectorScene *Self)
{
   if (Self->PendingResizeMsgs.size() > 0) {
      for (auto it=Self->PendingResizeMsgs.begin(); it != Self->PendingResizeMsgs.end(); it++) {
         objVectorViewport *view = *it;

         auto list = Self->ResizeSubscriptions[view];
         for (auto &sub : list) {
            ERROR result;
            auto vector = sub.first;
            auto func = sub.second;
            if (func.Type IS CALL_STDC) {
               parasol::SwitchContext ctx(func.StdC.Context);
               auto callback = (ERROR (*)(objVectorViewport *, objVector *, DOUBLE, DOUBLE, DOUBLE, DOUBLE))func.StdC.Routine;
               result = callback(view, vector, view->FinalX, view->FinalY, view->vpFixedWidth, view->vpFixedHeight);
            }
            else if (func.Type IS CALL_SCRIPT) {
               ScriptArg args[] = {
                  { "Viewport", FDF_OBJECT, { .Address = view } },
                  { "Vector",   FDF_OBJECT, { .Address = vector } },
                  { "X",        FDF_DOUBLE, { .Double = view->FinalX } },
                  { "Y",        FDF_DOUBLE, { .Double = view->FinalY } },
                  { "Width",    FDF_DOUBLE, { .Double = view->vpFixedWidth } },
                  { "Height",   FDF_DOUBLE, { .Double = view->vpFixedHeight } }
               };
               scCallback(func.Script.Script, func.Script.ProcedureID, args, ARRAYSIZE(args), &result);
            }
         }
      }

      Self->PendingResizeMsgs.clear();
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
// Distribute input events to any vectors that have subscribed and have the focus

static void scene_key_event(extVectorScene *Self, evKey *Event, LONG Size)
{
   const std::lock_guard<std::recursive_mutex> lock(glFocusLock);

   if (Event->Code IS K_TAB) {
      if (!(Event->Qualifiers & KQ_RELEASED)) return;

      bool reverse = ((Event->Qualifiers & KQ_QUALIFIERS) & KQ_SHIFT);

      if ((!(Event->Qualifiers & KQ_QUALIFIERS)) or (reverse)) {
         if (Self->KeyboardSubscriptions.size() > 1) {
            for (auto it=glFocusList.begin(); it != glFocusList.end(); it++) {
               auto find = Self->KeyboardSubscriptions.find(*it);
               if (find != Self->KeyboardSubscriptions.end()) {
                  if (reverse) {
                     if (find IS Self->KeyboardSubscriptions.begin()) {
                        apply_focus(Self, *Self->KeyboardSubscriptions.rbegin());
                     }
                     else apply_focus(Self, *(--find));
                  }
                  else {
                     if (++find IS Self->KeyboardSubscriptions.end()) find = Self->KeyboardSubscriptions.begin();
                     apply_focus(Self, *find);
                  }
                  return;
               }
            }
         }
      }

      if (!Self->KeyboardSubscriptions.empty()) {
         apply_focus(Self, *Self->KeyboardSubscriptions.begin());
      }

      return;
   }

   for (auto vi = Self->KeyboardSubscriptions.begin(); vi != Self->KeyboardSubscriptions.end(); vi++) {
      auto const vector = *vi;
      // Use the focus list to determine where the key event needs to be sent.
      for (auto it=glFocusList.begin(); it != glFocusList.end(); it++) {
         if (*it IS vector) {
            vector_keyboard_events(vector, Event);
            break;
         }
      }
   }
}

//********************************************************************************************************************

static void send_input_events(objVector *Vector, InputEvent *Event)
{
   parasol::Log log(__FUNCTION__);

   if (!Vector->InputSubscriptions) return;

   for (auto it=Vector->InputSubscriptions->begin(); it != Vector->InputSubscriptions->end(); ) {
      auto &sub = *it;

      if ((Event->Mask & JTYPE_REPEATED) and (!(sub.Mask & JTYPE_REPEATED))) it++;
      else if (sub.Mask & Event->Mask) {
         ERROR result;
         if (sub.Callback.Type IS CALL_STDC) {
            parasol::SwitchContext ctx(sub.Callback.StdC.Context);
            auto callback = (ERROR (*)(objVector *, InputEvent *))sub.Callback.StdC.Routine;
            result = callback(Vector, Event);
         }
         else if (sub.Callback.Type IS CALL_SCRIPT) {
            ScriptArg args[] = {
               { "Vector",            FDF_OBJECT, { .Address = Vector } },
               { "InputEvent:Events", FDF_STRUCT, { .Address = Event } }
            };
            scCallback(sub.Callback.Script.Script, sub.Callback.Script.ProcedureID, args, ARRAYSIZE(args), &result);
         }

         if (result IS ERR_Terminate) it = Vector->InputSubscriptions->erase(it);
         else it++;
      }
      else it++;
   }
}

//********************************************************************************************************************

static void send_enter_event(objVector *Vector, const InputEvent *Event, DOUBLE X = 0, DOUBLE Y = 0)
{
   InputEvent event = {
      .Next        = NULL,
      .Value       = Vector->UID,
      .Timestamp   = Event->Timestamp,
      .RecipientID = Vector->UID,
      .OverID      = Vector->UID,
      .AbsX        = Event->X,
      .AbsY        = Event->Y,
      .X           = Event->X - X,
      .Y           = Event->Y - Y,
      .DeviceID    = Event->DeviceID,
      .Type        = JET_ENTERED_SURFACE,
      .Flags       = JTYPE_FEEDBACK,
      .Mask        = JTYPE_FEEDBACK
   };
   send_input_events(Vector, &event);
}

//********************************************************************************************************************

static void send_left_event(objVector *Vector, const InputEvent *Event, DOUBLE X = 0, DOUBLE Y = 0)
{
   InputEvent event = {
      .Next        = NULL,
      .Value       = Vector->UID,
      .Timestamp   = Event->Timestamp,
      .RecipientID = Vector->UID,
      .OverID      = Vector->UID,
      .AbsX        = Event->X,
      .AbsY        = Event->Y,
      .X           = Event->X - X,
      .Y           = Event->Y - Y,
      .DeviceID    = Event->DeviceID,
      .Type        = JET_LEFT_SURFACE,
      .Flags       = JTYPE_FEEDBACK,
      .Mask        = JTYPE_FEEDBACK
   };
   send_input_events(Vector, &event);
}

//********************************************************************************************************************

static void send_wheel_event(extVectorScene *Scene, objVector *Vector, const InputEvent *Event)
{
   InputEvent event = {
      .Next        = NULL,
      .Value       = Event->Value,
      .Timestamp   = Event->Timestamp,
      .RecipientID = Vector->UID,
      .OverID      = Event->OverID,
      .AbsX        = Event->X,
      .AbsY        = Event->Y,
      .X           = Scene->ActiveVectorX,
      .Y           = Scene->ActiveVectorY,
      .DeviceID    = Event->DeviceID,
      .Type        = JET_WHEEL,
      .Flags       = JTYPE_ANALOG|JTYPE_EXT_MOVEMENT,
      .Mask        = JTYPE_EXT_MOVEMENT
   };
   send_input_events(Vector, &event);
}

//********************************************************************************************************************
// Incoming input events from the Surface hosting the scene are distributed within the scene graph.

ERROR scene_input_events(const InputEvent *Events, LONG Handle)
{
   parasol::Log log(__FUNCTION__);

   auto Self = (extVectorScene *)CurrentContext();
   if (!Self->SurfaceID) return ERR_Okay;

   LONG cursor = -1;

   // Distribute input events to any vectors that have subscribed.  Bear in mind that a consequence of calling client
   // code is that the scene's surface could be destroyed at any time.

   for (auto input=Events; input; input=input->Next) {
      if (input->Flags & (JTYPE_ANCHORED|JTYPE_MOVEMENT)) {
         while ((input->Next) and (input->Next->Flags & JTYPE_MOVEMENT)) { // Consolidate movement
            input = input->Next;
         }
      }

      // Focus management - clicking with the LMB can result in a change of focus.

      if ((input->Flags & JTYPE_BUTTON) and (input->Type IS JET_LMB) and (input->Value IS 1)) {
         apply_focus(Self, (objVector *)get_viewport_at_xy(Self, input->X, input->Y));
      }

      if (input->Type IS JET_WHEEL) {
         if (Self->ActiveVector) {
            parasol::ScopedObjectLock<objVector> lock(Self->ActiveVector);
            if (lock.granted()) send_wheel_event(Self, lock.obj, input);
         }
      }
      else if (input->Type IS JET_LEFT_SURFACE) {
         if (Self->ActiveVector) {
            parasol::ScopedObjectLock<objVector> lock(Self->ActiveVector);
            if (lock.granted()) send_left_event(lock.obj, input, Self->ActiveVectorX, Self->ActiveVectorY);
         }
      }
      else if (input->Type IS JET_ENTERED_SURFACE);
      else if (input->Flags & JTYPE_BUTTON) {
         OBJECTID target;
         if (Self->ButtonLock) target = Self->ButtonLock;
         else target = Self->ActiveVector;

         if (target) {
            parasol::ScopedObjectLock<objVector> lock(target);
            if (lock.granted()) {
               InputEvent event = *input;
               event.Next = NULL;
               event.AbsX = input->X; // Absolute coordinates are not translated.
               event.AbsY = input->Y;
               event.X    = Self->ActiveVectorX;
               event.Y    = Self->ActiveVectorY;
               send_input_events(lock.obj, &event);

               if ((input->Type IS JET_LMB) and (!(input->Flags & JTYPE_REPEATED))) {
                  Self->ButtonLock = input->Value ? target : 0;
               }
            }
         }
      }
      else if (input->Flags & (JTYPE_ANCHORED|JTYPE_MOVEMENT)) {
         if (cursor IS -1) cursor = PTR_DEFAULT;
         bool processed = false;
         for (auto it = Self->InputBoundaries.rbegin(); it != Self->InputBoundaries.rend(); it++) {
            auto &bounds = *it;

            if ((processed) and (!bounds.Cursor)) continue;

            // When the user holds a mouse button over a vector, a 'button lock' will be held.  This causes all events to
            // be captured by that vector until the button is released.

            bool in_bounds = false;
            if ((Self->ButtonLock) and (Self->ButtonLock IS bounds.VectorID));
            else if ((Self->ButtonLock) and (Self->ButtonLock != bounds.VectorID)) continue;
            else { // No button lock, perform a simple bounds check
               in_bounds = (input->X >= bounds.BX1) and (input->Y >= bounds.BY1) and
                           (input->X < bounds.BX2) and (input->Y < bounds.BY2);
               if (!in_bounds) continue;
            }

            parasol::ScopedObjectLock<objVector> lock(bounds.VectorID);
            if (!lock.granted()) continue;
            auto vector = lock.obj;

            // Additional bounds check to cater for transforms, clip masks etc.

            if (in_bounds) {
               if (vecPointInPath(vector, input->X, input->Y) != ERR_Okay) continue;
            }

            if (Self->ActiveVector != bounds.VectorID) {
               send_enter_event(vector, input, bounds.X, bounds.Y);
            }

            if ((!Self->ButtonLock) and (vector->Cursor)) cursor = vector->Cursor;

            if (!processed) {
               DOUBLE tx = input->X, ty = input->Y; // Invert the coordinates to pass localised coords to the vector.
               auto invert = ~vector->Transform; // Presume that prior path generation has configured the transform.
               invert.transform(&tx, &ty);

               InputEvent event = *input;
               event.Next = NULL;
               event.AbsX = input->X; // Absolute coordinates are not translated.
               event.AbsY = input->Y;
               event.X    = tx;
               event.Y    = ty;
               send_input_events(vector, &event);

               if (input->Flags & (JTYPE_ANCHORED|JTYPE_MOVEMENT)) {
                  if ((Self->ActiveVector) and (Self->ActiveVector != vector->UID)) {
                     parasol::ScopedObjectLock<objVector> lock(Self->ActiveVector);
                     if (lock.granted()) send_left_event(lock.obj, input, Self->ActiveVectorX, Self->ActiveVectorY);
                  }

                  Self->ActiveVector  = vector->UID;
                  Self->ActiveVectorX = tx;
                  Self->ActiveVectorY = ty;
               }

               processed = true;
            }

            if (cursor IS PTR_DEFAULT) continue; // Keep scanning in case an input boundary defines a cursor.
            else break; // Input consumed and cursor image identified.
         }

         // If no vectors received a hit for a movement message, we may need to inform the last active vector that the
         // cursor left its area.

         if ((Self->ActiveVector) and (!processed)) {
            parasol::ScopedObjectLock<objVector> lock(Self->ActiveVector);
            Self->ActiveVector = 0;
            if (lock.granted()) send_left_event(lock.obj, input, Self->ActiveVectorX, Self->ActiveVectorY);
         }
      }
      else log.warning("Unrecognised movement type %d", input->Type);
   }

   if ((cursor != -1) and (!Self->ButtonLock) and (Self->SurfaceID)) {
      Self->Cursor = cursor;
      parasol::ScopedObjectLock<objSurface> lock(Self->SurfaceID);
      if (lock.granted() and (lock.obj->Cursor != Self->Cursor)) {
         SetLong(lock.obj, FID_Cursor, cursor);
      }
   }

   return ERR_Okay;
}

//********************************************************************************************************************

#include "scene_def.c"

static const FieldArray clSceneFields[] = {
   { "RenderTime",   FDF_LARGE|FDF_R,            0, (APTR)GET_RenderTime, NULL },
   { "Gamma",        FDF_DOUBLE|FDF_RW,          0, NULL, NULL },
   { "HostScene",    FDF_OBJECT|FDF_RI,          ID_VECTORSCENE, NULL, NULL },
   { "Viewport",     FDF_OBJECT|FD_R,            ID_VECTORVIEWPORT, NULL, NULL },
   { "Bitmap",       FDF_OBJECT|FDF_RW,          ID_BITMAP, NULL, (APTR)SET_Bitmap },
   { "Defs",         FDF_STRUCT|FDF_PTR|FDF_SYSTEM|FDF_RESOURCE|FDF_R, (MAXINT)"KeyStore", NULL, NULL },
   { "Surface",      FDF_OBJECTID|FDF_RI,        ID_SURFACE, NULL, (APTR)SET_Surface },
   { "Flags",        FDF_LONGFLAGS|FDF_RW,       (MAXINT)&clVectorSceneFlags, NULL, NULL },
   { "PageWidth",    FDF_LONG|FDF_RW,            0, NULL, (APTR)SET_PageWidth },
   { "PageHeight",   FDF_LONG|FDF_RW,            0, NULL, (APTR)SET_PageHeight },
   { "SampleMethod", FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clVectorSceneSampleMethod, NULL, NULL },
   END_FIELD
};

ERROR init_vectorscene(void)
{
   return(CreateObject(ID_METACLASS, 0, &clVectorScene,
      FID_ClassVersion|TFLOAT, VER_VECTORSCENE,
      FID_Name|TSTR,      "VectorScene",
      FID_Category|TLONG, CCF_GRAPHICS,
      FID_Actions|TPTR,   clVectorSceneActions,
      FID_Methods|TARRAY, clVectorSceneMethods,
      FID_Fields|TARRAY,  clSceneFields,
      FID_Size|TLONG,     sizeof(extVectorScene),
      FID_Path|TSTR,      "modules:vector",
      TAGEND));
}
