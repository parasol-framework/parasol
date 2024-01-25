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

void apply_focus(extVectorScene *, extVector *);
static void scene_key_event(extVectorScene *, evKey *, LONG);
static void process_resize_msgs(extVectorScene *);

static void render_to_surface(extVectorScene *Self, objSurface *Surface, objBitmap *Bitmap)
{
   Self->Bitmap = Bitmap;

   if ((!Self->PageWidth) or (!Self->PageHeight)) {
      if (Self->Viewport) mark_dirty(Self->Viewport, RC::BASE_PATH|RC::TRANSFORM); // Base-paths need to be recomputed if they use relative coordinates.
   }

   acDraw(Self);

   Self->Bitmap = NULL;
}

//********************************************************************************************************************

static void notify_free(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, APTR Args)
{
   auto Self = (objVectorScene *)CurrentContext();
   if (Self->SurfaceID IS Object->UID) Self->SurfaceID = 0;
}

//********************************************************************************************************************

static void notify_redimension(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, struct acRedimension *Args)
{
   auto Self = (objVectorScene *)CurrentContext();

   if ((Self->Flags & VPF::RESIZE) != VPF::NIL) {
      Self->PageWidth  = Args->Width;
      Self->PageHeight = Args->Height;

      if (Self->Viewport) {
         mark_dirty(Self->Viewport, RC::BASE_PATH|RC::TRANSFORM); // Base-paths need to be recomputed if they use relative coordinates.
      }

      ActionMsg(MT_DrwScheduleRedraw, Self->SurfaceID, NULL);
   }
}

//********************************************************************************************************************
// Called when the subscribed Surface loses the focus.

static void notify_lostfocus(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, APTR Args)
{
   auto Self = (extVectorScene *)CurrentContext();
   if (Self->KeyHandle) { UnsubscribeEvent(Self->KeyHandle); Self->KeyHandle = NULL; }

   apply_focus(Self, NULL);
}

//********************************************************************************************************************
// Called when the subscribed Surface receives the focus.

static void notify_focus(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, APTR Args)
{
   auto Self = (extVectorScene *)CurrentContext();
   if (!Self->KeyHandle) {
      SubscribeEvent(EVID_IO_KEYBOARD_KEYPRESS, FUNCTION(scene_key_event), Self, &Self->KeyHandle);
   }
}

/*********************************************************************************************************************

-METHOD-
AddDef: Registers a named definition object within a scene graph.

This method will add a new definition object to the root of a vector tree and gives it a name.  This feature is
provided to support SVG style referencing for features such as gradients, images and patterns.  By providing a name
with the definition object, the object can then be referenced in URL strings.

For example, if creating a gradient with a name of "redGradient" it would be possible to reference it with
`url(#redGradient)` in common graphics attributes such as `fill` and `stroke`.

At the time of writing, the provided object must belong to one of the following classes to be valid: @Vector,
@VectorScene, @VectorGradient, @VectorImage, @VectorPath, @VectorPattern, @VectorFilter, @VectorTransition,
@VectorClip.

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
   pf::Log log;

   if ((!Args) or (!Args->Name) or (!Args->Def)) return log.warning(ERR_NullArgs);

   if (Self->HostScene) { // Defer all definitions if a hosting scene is active.
      return Action(MT_ScAddDef, Self->HostScene, Args);
   }

   OBJECTPTR def = Args->Def;

   if ((def->Class->ClassID IS ID_VECTORSCENE) or
       (def->Class->BaseClassID IS ID_VECTOR) or
       (def->Class->ClassID IS ID_VECTORGRADIENT) or
       (def->Class->ClassID IS ID_VECTORIMAGE) or
       (def->Class->ClassID IS ID_VECTORPATH) or
       (def->Class->ClassID IS ID_VECTORPATTERN) or
       (def->Class->BaseClassID IS ID_VECTORFILTER) or
       (def->Class->ClassID IS ID_VECTORTRANSITION) or
       (def->Class->ClassID IS ID_VECTORCLIP)) {
      // The use of this object as a definition is valid.
   }
   else return log.warning(ERR_InvalidObject);

   // If the resource does not belong to the Scene object, this can lead to invalid pointer references

   if (!def->hasOwner(Self->UID)) {
      log.warning("The %s must belong to VectorScene #%d, but is owned by object #%d.", def->Class->ClassName, Self->UID, def->OwnerID);
      return ERR_UnsupportedOwner;
   }

   // TODO: Subscribe to the Free() action of the definition object so that we can avoid invalid pointer references.

   log.extmsg("Adding definition '%s' referencing %s #%d", Args->Name, def->Class->ClassName, def->UID);

   if (Self->Defs.contains(Args->Name)) { // Check that the definition name is unique.
      log.extmsg("The vector definition name '%s' is already in use.", Args->Name);
      return ERR_ResourceExists;
   }

   Self->Defs[Args->Name] = def;
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
Debug: Internal functionality for debugging.

This internal method prints comprehensive information that describes the scene graph to the log.

-ERRORS-
Okay:

*********************************************************************************************************************/

static ERROR VECTORSCENE_Debug(extVectorScene *Self, APTR Void)
{
   pf::Log log("debug_tree");

   pf::vector<ChildEntry> list;
   if (!ListChildren(Self->UID, &list)) {
      for (auto &rec : list) {
         auto obj = GetObjectPtr(rec.ObjectID);
         if (obj IS Self->Viewport) continue;
         log.msg("#%d %s %s", rec.ObjectID, obj->Class->ClassName, obj->Name);
      }
   }

   LONG level = 0;
   debug_tree((extVector *)Self->Viewport, level);
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
   pf::Log log;
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

   if ((Self->Flags & VPF::RENDER_TIME) != VPF::NIL) { // Client wants to know how long the rendering takes to complete
      LARGE time = PreciseTime();
      adaptor->draw(bmp);
      if ((Self->RenderTime = PreciseTime() - time) < 1) Self->RenderTime = 1;
   }
   else adaptor->draw(bmp);

// For debugging purposes, draw a boundary around the target area.
//   static RGB8 highlightA = { .Red = 255, .Green = 0, .Blue = 0, .Alpha = 255 };
//   ULONG highlight = PackPixelRGBA(bmp, &highlightA);
//   gfxDrawRectangle(bmp, bmp->Clip.Left, bmp->Clip.Top, bmp->Clip.Right-bmp->Clip.Left, bmp->Clip.Bottom-bmp->Clip.Top, highlight, BAF::NIL);

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
&obj Def: A pointer to the definition object is returned here if discovered.

-ERRORS-
Okay
NullArgs
Search: A definition with the given Name was not found.
-END-

*********************************************************************************************************************/

static ERROR VECTORSCENE_FindDef(extVectorScene *Self, struct scFindDef *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Name)) return log.warning(ERR_NullArgs);

   if (Self->HostScene) return Action(MT_ScFindDef, Self->HostScene, Args);

   CSTRING name = Args->Name;

   if (*name IS '#') name = name + 1;
   else if (!StrCompare("url(#", name, 5)) {
      LONG i;
      for (i=5; (name[i] != ')') and name[i]; i++);
      std::string lookup;
      lookup.assign(name, 5, i-5);

      auto def = Self->Defs.find(lookup);
      if (def != Self->Defs.end()) {
         Args->Def = def->second;
         return ERR_Okay;
      }
      else return ERR_Search;
   }

   auto def = Self->Defs.find(name);
   if (def != Self->Defs.end()) {
      Args->Def = def->second;
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
      pf::ScopedObjectLock<objSurface> surface(Self->SurfaceID, 5000);
      if (surface.granted()) {
         drwAddCallback(*surface, APTR(render_to_surface));

         if ((!Self->PageWidth) or (!Self->PageHeight)) {
            Self->Flags |= VPF::RESIZE;
            surface->get(FID_Width, &Self->PageWidth);
            surface->get(FID_Height, &Self->PageHeight);
         }

         SubscribeAction(*surface, AC_Redimension, FUNCTION(notify_redimension));
         SubscribeAction(*surface, AC_Free, FUNCTION(notify_free));
         SubscribeAction(*surface, AC_Focus, FUNCTION(notify_focus));
         SubscribeAction(*surface, AC_LostFocus, FUNCTION(notify_lostfocus));

         if (surface->hasFocus()) {
            SubscribeEvent(EVID_IO_KEYBOARD_KEYPRESS, FUNCTION(scene_key_event), Self, &Self->KeyHandle);
         }
      }

      auto callback = FUNCTION(scene_input_events);
      if (gfxSubscribeInput(&callback, Self->SurfaceID, JTYPE::MOVEMENT|JTYPE::FEEDBACK|JTYPE::BUTTON|JTYPE::REPEATED|JTYPE::EXT_MOVEMENT, 0, &Self->InputHandle)) {
         return ERR_Function;
      }
   }

   Self->Cursor = PTC::DEFAULT;

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR VECTORSCENE_NewObject(extVectorScene *Self, APTR Void)
{
   Self->SampleMethod = VSM::BILINEAR;

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
   Self->Defs.clear();
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

   auto vector = (extVector *)Self->Viewport;
   while (vector) {
      //log.msg("Search","%.3d: %p <- #%d -> %p Child %p", vector->Index, vector->Prev, vector->UID, vector->Next, vector->Child);
cont:
      if (vector->NumericID IS Args->ID) {
         Args->Result = vector;
         return ERR_Okay;
      }

      if (vector->Child) vector = (extVector *)vector->Child;
      else if (vector->Next) vector = (extVector *)vector->Next;
      else {
         while ((vector = get_parent(vector))) { // Unwind back up the stack, looking for the first Parent with a Next field.
            if (vector->Class->BaseClassID != ID_VECTOR) return ERR_Search;
            if (vector->Next) {
               vector = (extVector *)vector->Next;
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
-END-
*********************************************************************************************************************/

static ERROR SET_Bitmap(extVectorScene *Self, objBitmap *Value)
{
   if (Value) {
      if (Self->Buffer) delete Self->Buffer;

      Self->Buffer = new (std::nothrow) agg::rendering_buffer;
      if (Self->Buffer) {
         Self->Buffer->attach(Value->Data, Value->Width, Value->Height, Value->LineWidth);
         Self->Bitmap = Value;

         if ((Self->Flags & VPF::BITMAP_SIZED) != VPF::NIL) {
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
Defs: Obtain direct access to the SVG definition table.

Reading the Defs field will return a direct pointer to the SVG definition table, which is declared as a key-value C++
type:

<pre>std::unordered_map&lt;std::string, OBJECTPTR&gt;</pre>

Direct access is provided for internal use only and not for the benefit of client programs.

*********************************************************************************************************************/

static ERROR GET_Defs(extVectorScene *Self, std::unordered_map<std::string, OBJECTPTR> **Value)
{
   *Value = &Self->Defs;
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

*********************************************************************************************************************/

static ERROR SET_PageHeight(extVectorScene *Self, LONG Value)
{
   if (Value IS Self->PageHeight) return ERR_Okay;

   if (Value < 1) Self->PageHeight = 1;
   else Self->PageHeight = Value;

   if (Self->Viewport) mark_dirty(Self->Viewport, RC::BASE_PATH|RC::TRANSFORM); // Base-paths need to be recomputed if they use relative coordinates.
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
PageWidth: The width of the page that contains the vector.

This value defines the pixel width of the page that contains the vector scene graph.  If the `RESIZE` #Flags
option is used then the viewport will be scaled to fit within the page.

*********************************************************************************************************************/

static ERROR SET_PageWidth(extVectorScene *Self, LONG Value)
{
   if (Value IS Self->PageWidth) return ERR_Okay;

   if (Value < 1) Self->PageWidth = 1;
   else Self->PageWidth = Value;

   if (Self->Viewport) mark_dirty(Self->Viewport, RC::BASE_PATH|RC::TRANSFORM);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
RenderTime: Returns the rendering time of the last scene.

RenderTime returns the rendering time of the last scene that was drawn, measured in microseconds.  This value can also
be used to compute frames-per-second with `1000000 / RenderTime`.

The `RENDER_TIME` flag should also be set before fetching this value, as it is required to enable the timing feature.  If
`RENDER_TIME` is not set, it will be set automatically so that subsequent calls succeed correctly.

*********************************************************************************************************************/

static ERROR GET_RenderTime(extVectorScene *Self, LARGE *Value)
{
   Self->Flags |= VPF::RENDER_TIME;
   *Value = Self->RenderTime;
   return ERR_Okay;
}

/*********************************************************************************************************************

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

/*********************************************************************************************************************

-FIELD-
Viewport: References the first object in the scene, which must be a VectorViewport object.

The first object in the vector scene is referenced here.  It must belong to the @VectorViewport class, which will
be used to define the size and location of the area rendered by the scene.

The Viewport value cannot be set by the client.  It will be automatically defined when the first VectorViewport 
owned by the VectorScene is initialised.
-END-

*********************************************************************************************************************/

//********************************************************************************************************************
// Apply focus to a vector and all other vectors within that tree branch (not necessarily just the viewports).
// Also sends LostFocus notifications to vectors that previously had the focus.
// The glVectorFocusList maintains the current focus state, with the most foreground vector at the beginning.
//
// If Vector is NULL then the focus is dropped from all vectors.

void apply_focus(extVectorScene *Scene, extVector *Vector)
{
   const std::lock_guard<std::recursive_mutex> lock(glVectorFocusLock);

   if ((!glVectorFocusList.empty()) and (Vector IS glVectorFocusList.front())) return;

   std::vector<extVector *> focus_gained; // The first reference is the most foreground object

   for (auto scan=Vector; scan; scan=(extVector *)scan->Parent) {
      if (scan->Class->BaseClassID IS ID_VECTOR) {
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

   auto focus_event = FM::HAS_FOCUS;
   for (auto const fgv : focus_gained) {
      bool no_focus = true, lost_focus_to_child = false, was_child_now_primary = false;

      if (!glVectorFocusList.empty()) {
         no_focus = std::find(glVectorFocusList.begin(), glVectorFocusList.end(), fgv) IS glVectorFocusList.end();
         if (!no_focus) {
            lost_focus_to_child = ((fgv IS glVectorFocusList.front()) and (focus_event IS FM::CHILD_HAS_FOCUS));
            was_child_now_primary = ((fgv != glVectorFocusList.front()) and (focus_event IS FM::HAS_FOCUS));
         }
      }

      if ((no_focus) or (lost_focus_to_child) or (was_child_now_primary)) {
         pf::ScopedObjectLock<extVector> vec(fgv, 1000);
         if (vec.granted()) {
            send_feedback((extVector *)fgv, focus_event, Vector);
            focus_event = FM::CHILD_HAS_FOCUS;
         }
      }
   }

   // Report lost focus events, starting from the foreground.

   for (auto const fv : glVectorFocusList) {
      if (std::find(focus_gained.begin(), focus_gained.end(), fv) IS focus_gained.end()) {
         pf::ScopedObjectLock<extVector> vec(fv, 1000);
         if (vec.granted()) send_feedback(fv, FM::LOST_FOCUS, Vector);
      }
      else break;
   }

   glVectorFocusList = std::move(focus_gained);
}

//********************************************************************************************************************

static void process_resize_msgs(extVectorScene *Self)
{
   if (Self->PendingResizeMsgs.size() > 0) {
      for (auto it=Self->PendingResizeMsgs.begin(); it != Self->PendingResizeMsgs.end(); it++) {
         extVectorViewport *view = *it;

         auto list = Self->ResizeSubscriptions[view]; // take copy
         for (auto &sub : list) {
            ERROR result;
            auto vector = sub.first;
            auto func   = sub.second;
            if (func.isC()) {
               pf::SwitchContext ctx(func.StdC.Context);
               auto callback = (ERROR (*)(extVectorViewport *, objVector *, DOUBLE, DOUBLE, DOUBLE, DOUBLE, APTR))func.StdC.Routine;
               result = callback(view, vector, view->FinalX, view->FinalY, view->vpFixedWidth, view->vpFixedHeight, func.StdC.Meta);
            }
            else if (func.isScript()) {
               ScriptArg args[] = {
                  { "Viewport",       view, FDF_OBJECT },
                  { "Vector",         vector, FDF_OBJECT },
                  { "ViewportX",      view->FinalX },
                  { "ViewportY",      view->FinalY },
                  { "ViewportWidth",  view->vpFixedWidth },
                  { "ViewportHeight", view->vpFixedHeight }
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

static ERROR vector_keyboard_events(extVector *Vector, const evKey *Event)
{
   for (auto it=Vector->KeyboardSubscriptions->begin(); it != Vector->KeyboardSubscriptions->end(); ) {
      ERROR result = ERR_Terminate;
      auto &sub = *it;
      if (sub.Callback.isC()) {
         pf::SwitchContext ctx(sub.Callback.StdC.Context);
         auto callback = (ERROR (*)(objVector *, KQ, KEY, LONG, APTR))sub.Callback.StdC.Routine;
         result = callback(Vector, Event->Qualifiers, Event->Code, Event->Unicode, sub.Callback.StdC.Meta);
      }
      else if (sub.Callback.isScript()) {
         // In this implementation the script function will receive all the events chained via the Next field
         ScriptArg args[] = {
            { "Vector",     Vector, FDF_OBJECT },
            { "Qualifiers", LONG(Event->Qualifiers) },
            { "Code",       LONG(Event->Code) },
            { "Unicode",    Event->Unicode }
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
   const std::lock_guard<std::recursive_mutex> lock(glVectorFocusLock);

   if (Event->Code IS KEY::TAB) {
      if ((Event->Qualifiers & KQ::RELEASED) IS KQ::NIL) return;

      bool reverse = (((Event->Qualifiers & KQ::QUALIFIERS) & KQ::SHIFT) != KQ::NIL);

      if (((Event->Qualifiers & KQ::QUALIFIERS) IS KQ::NIL) or (reverse)) {
         if (Self->KeyboardSubscriptions.size() > 1) {
            for (auto it=glVectorFocusList.begin(); it != glVectorFocusList.end(); it++) {
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
      for (auto it=glVectorFocusList.begin(); it != glVectorFocusList.end(); it++) {
         if (*it IS vector) {
            vector_keyboard_events(vector, Event);
            break;
         }
      }
   }
}

//********************************************************************************************************************

#include "scene_input.cpp"

#include "scene_def.c"

static const FieldArray clSceneFields[] = {
   { "RenderTime",   FDF_LARGE|FDF_R, GET_RenderTime },
   { "Gamma",        FDF_DOUBLE|FDF_RW },
   { "HostScene",    FDF_OBJECT|FDF_RI,    NULL, NULL, ID_VECTORSCENE },
   { "Viewport",     FDF_OBJECT|FD_R,      NULL, NULL, ID_VECTORVIEWPORT },
   { "Bitmap",       FDF_OBJECT|FDF_RW,    NULL, SET_Bitmap, ID_BITMAP },
   { "Surface",      FDF_OBJECTID|FDF_RI,  NULL, SET_Surface, ID_SURFACE },
   { "Flags",        FDF_LONGFLAGS|FDF_RW, NULL, NULL, &clVectorSceneFlags },
   { "PageWidth",    FDF_LONG|FDF_RW,      NULL, SET_PageWidth },
   { "PageHeight",   FDF_LONG|FDF_RW,      NULL, SET_PageHeight },
   { "SampleMethod", FDF_LONG|FDF_LOOKUP|FDF_RW, NULL, NULL, &clVectorSceneSampleMethod },
   // Virtual fields
   { "Defs",         FDF_PTR|FDF_SYSTEM|FDF_R, GET_Defs, NULL },
   END_FIELD
};

//********************************************************************************************************************

ERROR init_vectorscene(void)
{
   clVectorScene = objMetaClass::create::global(
      fl::ClassVersion(VER_VECTORSCENE),
      fl::Name("VectorScene"),
      fl::Category(CCF::GRAPHICS),
      fl::Actions(clVectorSceneActions),
      fl::Methods(clVectorSceneMethods),
      fl::Fields(clSceneFields),
      fl::Size(sizeof(extVectorScene)),
      fl::Path(MOD_PATH));

   return clVectorScene ? ERR_Okay : ERR_AddClass;
}
