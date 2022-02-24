/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
VectorScene: Manages the scene graph for a collection of vectors.

The VectorScene class acts as a container and control point for the management of vector definitions.  Its main
purpose is to draw the scene to a target @Bitmap or @Surface provided by the client.

Vector scenes are created by initialising multiple Vector objects such as @VectorPath and
@VectorViewport and positioning them within a vector tree.  The VectorScene must lie at the root.

The default mode of operation is for scenes to be manually drawn, for which the client must set the target #Bitmap
and call the #Draw() action as required.  Automated drawing can be enabled by setting the target #Surface prior
to initialisation.  In automated mode the #PageWidth and #PageHeight will reflect the dimensions of the target surface
at all times.

Vector definitions can be saved and loaded from permanent storage by using the @SVG class.
-END-

*********************************************************************************************************************/

static ERROR VECTORSCENE_Reset(objVectorScene *, APTR);

static void scene_key_event(objVectorScene *, evKey *, LONG);
static void process_resize_msgs(objVectorScene *);

static std::vector<OBJECTID> glFocusList; // The first reference is the most foreground object with the focus

//********************************************************************************************************************

static ERROR VECTORSCENE_ActionNotify(objVectorScene *Self, struct acActionNotify *Args)
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

This method will add a new definition to the root of a vector tree.  This feature is provided with the intention of
supporting SVG style references to definitions such as gradients, images and other vectors.  By providing a name with
the definition object, the object can then be referenced in strings that support definition referencing.

For instance, creating a gradient and associating it with the definition "redGradient" it would be possible to
reference it with the string "url(#redGradient)" from common graphics attributes such as "fill".

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

static ERROR VECTORSCENE_AddDef(objVectorScene *Self, struct scAddDef *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Name) or (!Args->Def)) return log.warning(ERR_NullArgs);

   OBJECTPTR def = (OBJECTPTR)Args->Def;

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

   if (def->OwnerID != Self->Head.UID) {
      log.warning("The %s must belong to VectorScene #%d, but is owned by object #%d.", def->Class->ClassName, Self->Head.UID, def->OwnerID);
      return ERR_UnsupportedOwner;
   }

   // TO DO: Subscribe to the Free() action of the definition object so that we can avoid invalid pointer references.

   log.trace("Adding definition '%s' for object #%d", Args->Name, def->UID);

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

The Draw action will render the scene to the target #Bitmap.  If #Bitmap is NULL, an error will be
returned.

In addition, the #RenderTime field will be updated if the `RENDER_TIME` flag is defined.

-ERRORS-
Okay
FieldNotSet: The Bitmap field is NULL.

*********************************************************************************************************************/

static ERROR VECTORSCENE_Draw(objVectorScene *Self, struct acDraw *Args)
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

static ERROR VECTORSCENE_FindDef(objVectorScene *Self, struct scFindDef *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Name)) return log.warning(ERR_NullArgs);

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

static ERROR VECTORSCENE_Free(objVectorScene *Self, APTR Args)
{
   Self->~objVectorScene();

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

static ERROR VECTORSCENE_Init(objVectorScene *Self, APTR Void)
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
      if (gfxSubscribeInput(&callback, Self->SurfaceID, JTYPE_MOVEMENT|JTYPE_FEEDBACK|JTYPE_BUTTON|JTYPE_REPEATED, 0, &Self->InputHandle)) {
         return ERR_Function;
      }
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR VECTORSCENE_NewObject(objVectorScene *Self, APTR Void)
{
   Self->SampleMethod = VSM_BILINEAR;

   new (Self) objVectorScene;

   // Please refer to the Reset action for setting variable defaults
   return VECTORSCENE_Reset(Self, NULL);
}

/*********************************************************************************************************************
-ACTION-
Redimension: Redefines the size of the page.
-END-
*********************************************************************************************************************/

static ERROR VECTORSCENE_Redimension(objVectorScene *Self, struct acRedimension *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Args->Width >= 1.0)  Self->PageWidth  = F2T(Args->Width);
   if (Args->Height >= 1.0) Self->PageHeight = F2T(Args->Height);

   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Reset: Clears all registered definitions and resets field values.  Child vectors are untouched.
-END-
*********************************************************************************************************************/

static ERROR VECTORSCENE_Reset(objVectorScene *Self, APTR Void)
{
   if (Self->Adaptor) { delete Self->Adaptor; Self->Adaptor = NULL; }
   if (Self->Buffer)  { delete Self->Buffer; Self->Buffer = NULL; }
   if (Self->Defs)    { FreeResource(Self->Defs); Self->Defs = NULL; }

   if (!(Self->Head.Flags & NF_FREE)) { // Reset all variables
      Self->Gamma = 1.0;
   }

   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Resize: Redefines the size of the page.
-END-
*********************************************************************************************************************/

static ERROR VECTORSCENE_Resize(objVectorScene *Self, struct acResize *Args)
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

static ERROR VECTORSCENE_SearchByID(objVectorScene *Self, struct scSearchByID *Args)
{
   if (!Args) return ERR_NullArgs;
   Args->Result = NULL;

   objVector *vector = Self->Viewport;
   while (vector) {
      //log.msg("Search","%.3d: %p <- #%d -> %p Child %p", vector->Index, vector->Prev, vector->Head.UID, vector->Next, vector->Child);
cont:
      if (vector->NumericID IS Args->ID) {
         Args->Result = (OBJECTPTR)vector;
         return ERR_Okay;
      }

      if (vector->Child) vector = vector->Child;
      else if (vector->Next) vector = vector->Next;
      else {
         while ((vector = (objVector *)get_parent(vector))) { // Unwind back up the stack, looking for the first Parent with a Next field.
            if (vector->Head.ClassID != ID_VECTOR) return ERR_Search;
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

static ERROR SET_Bitmap(objVectorScene *Self, objBitmap *Value)
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
PageHeight: The height of the page that contains the vector.

This value defines the pixel height of the page that contains the vector scene graph.  If the `RESIZE` #Flags
option is used then the viewport will be scaled to fit within the page.

****************************************************************************/

static ERROR SET_PageHeight(objVectorScene *Self, LONG Value)
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

static ERROR SET_PageWidth(objVectorScene *Self, LONG Value)
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

The RENDER_TIME flag should also be set before fetching this value, as it is required to enable the timing feature.  If
RENDER_TIME is not set, it will be set automatically so that subsequent calls succeed correctly.

****************************************************************************/

static ERROR GET_RenderTime(objVectorScene *Self, LARGE *Value)
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

static ERROR SET_Surface(objVectorScene *Self, OBJECTID Value)
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

static void render_to_surface(objVectorScene *Self, objSurface *Surface, objBitmap *Bitmap)
{
   Self->Bitmap = Bitmap;

   if ((!Self->PageWidth) or (!Self->PageHeight)) {
      if (Self->Viewport) mark_dirty(Self->Viewport, RC_BASE_PATH|RC_TRANSFORM); // Base-paths need to be recomputed if they use relative coordinates.
   }

   acDraw(Self);
}

//********************************************************************************************************************
// Apply focus to a vector and all other vectors within that tree branch (not necessarily just the viewports).
// Also sends LostFocus notifications to vectors that previously had the focus.

void apply_focus(objVectorScene *Scene, objVector *Vector)
{
   if ((!glFocusList.empty()) and (Vector->Head.UID IS glFocusList.front())) return;

   std::vector<OBJECTID> focus_gained; // The first reference is the most foreground object

   for (auto scan=Vector; scan; scan=(objVector *)scan->Parent) {
      if (scan->Head.ClassID IS ID_VECTOR) {
         focus_gained.emplace_back(scan->Head.UID);
      }
      else break;
   }

   // Report focus events to vector subscribers.

   for (auto const id : focus_gained) {
      bool has_focus = false;
      for (auto check_id : glFocusList) {
         if (check_id IS id) { has_focus = true; break; }
      }

      if (!has_focus) {
         OBJECTPTR vec;
         if (!AccessObject(id, 5000, &vec)) {
            NotifySubscribers(vec, AC_Focus, 0, 0, ERR_Okay);
            ReleaseObject(vec);
         }
      }
   }

   // Process lost focus events

   for (auto const id : glFocusList) { // Starting from the foreground...
      bool in_list = false;
      for (auto check_id : focus_gained) {
         if (check_id IS id) { in_list = true; break; }
      }

      if (!in_list) {
         OBJECTPTR vec;
         if (!AccessObject(id, 5000, &vec)) {
            NotifySubscribers(vec, AC_LostFocus, 0, 0, ERR_Okay);
            ReleaseObject(vec);
         }
      }
      else break;
   }

   glFocusList = focus_gained;
}

//********************************************************************************************************************
// Build a list of all child viewports that have a bounding box intersecting with (X,Y).  Transforms
// are taken into account through use of BX1,BY1,BX2,BY2.  The list is sorted starting from the background to the
// foreground.

void get_viewport_at_xy_scan(objVector *Vector, std::vector<std::vector<objVectorViewport *>> &Collection, LONG X, LONG Y, LONG Branch)
{
   if ((size_t)Branch >= Collection.size()) Collection.resize(Branch+1);

   for (auto scan=Vector; scan; scan=scan->Next) {
      if (scan->Head.SubID IS ID_VECTORVIEWPORT) {
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

objVectorViewport * get_viewport_at_xy(objVectorScene *Scene, LONG X, LONG Y)
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

static void process_resize_msgs(objVectorScene *Self)
{
   if (Self->PendingResizeMsgs.size() > 0) {
      for (auto it=Self->PendingResizeMsgs.begin(); it != Self->PendingResizeMsgs.end(); it++) {
         objVectorViewport *viewport;
         if (!AccessObject(it->first, 1000, &viewport)) {
            NotifySubscribers(viewport, AC_Redimension, &it->second, 0, ERR_Okay);
            ReleaseObject(viewport);
         }
      }

      Self->PendingResizeMsgs.clear();
   }
}

//********************************************************************************************************************
// Distribute input events to any vectors that have subscribed and have the focus

static void scene_key_event(objVectorScene *Self, evKey *Event, LONG Size)
{
   for (auto const vector : Self->KeyboardSubscriptions) {
      for (auto it=glFocusList.begin(); it != glFocusList.end(); it++) {
         if (*it IS vector->Head.UID) {
            vector_keyboard_events(vector, Event);
            break;
         }
      }
   }
}

//********************************************************************************************************************

static void send_input_event(objVector *Vector, InputEvent *Event)
{
   parasol::Log log(__FUNCTION__);

   if (!Vector->InputSubscriptions) return;

   for (auto it=Vector->InputSubscriptions->begin(); it != Vector->InputSubscriptions->end(); ) {
      auto &sub = *it;

      if (sub.Mask & Event->Mask) {
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
      .Value       = Event->OverID,
      .Timestamp   = Event->Timestamp,
      .RecipientID = Vector->Head.UID,
      .OverID      = Event->OverID,
      .AbsX        = Event->AbsX,
      .AbsY        = Event->AbsY,
      .X           = Event->X - X,
      .Y           = Event->Y - Y,
      .DeviceID    = Event->DeviceID,
      .Type        = JET_ENTERED_SURFACE,
      .Flags       = JTYPE_FEEDBACK,
      .Mask        = JTYPE_FEEDBACK
   };
   send_input_event(Vector, &event);
}

//********************************************************************************************************************

static void send_left_event(objVector *Vector, const InputEvent *Event)
{
   InputEvent event = {
      .Next        = NULL,
      .Value       = Event->OverID,
      .Timestamp   = Event->Timestamp,
      .RecipientID = Vector->Head.UID,
      .OverID      = Event->OverID,
      .AbsX        = Event->AbsX,
      .AbsY        = Event->AbsY,
      .X           = Event->X, // TODO Should be relative to the vector
      .Y           = Event->Y,
      .DeviceID    = Event->DeviceID,
      .Type        = JET_LEFT_SURFACE,
      .Flags       = JTYPE_FEEDBACK,
      .Mask        = JTYPE_FEEDBACK
   };
   send_input_event(Vector, &event);
}

//********************************************************************************************************************
// Input events within the scene are distributed within the scene graph.

static ERROR scene_input_events(const InputEvent *Events, LONG Handle)
{
   parasol::Log log(__FUNCTION__);

   auto Self = (objVectorScene *)CurrentContext();
   if (!Self->SurfaceID) return ERR_Okay;

   LONG received_events = 0;
   for (auto e=Events; e; e=e->Next) {
      received_events |= e->Mask;

      // Focus management - clicking with the LMB can result in a change of focus.

      if ((e->Mask & JTYPE_BUTTON) and (e->Type IS JET_LMB) and (e->Value IS 1)) {
         auto vp = get_viewport_at_xy(Self, e->X, e->Y);
         apply_focus(Self, (objVector *)vp);
      }
   }

   LONG cursor = PTR_DEFAULT;

   // Distribute input events to any vectors that have subscribed.  Bear in mind that a consequence of calling client
   // code is that the scene's surface could be removed at any time.

   // NOTE: For the time being this routine does not perform advanced hit tests to confirm if the (x,y) position of the
   // cursor is within paths.  Ideally we want the client to choose if the additional cost of hit testing is worth it
   // or not.  The fast way to do hit testing would be to generate a 1-bit mask of the shape in gen_vector_path() as this
   // would be cached and take advantage of the dirty marker.

   for (auto input=Events; input; input=input->Next) {
      if (input->Flags & (JTYPE_ANCHORED|JTYPE_MOVEMENT)) {
         while ((input->Next) and (input->Next->Flags & JTYPE_MOVEMENT)) { // Consolidate movement
            input = input->Next;
         }
      }

      bool processed = false;

      if (input->Type IS JET_LEFT_SURFACE) {
         parasol::ScopedObjectLock<objVector> lock(Self->LastMovementVector);
         if (lock.granted()) send_left_event(lock.obj, input);
      }
      else if (input->Type IS JET_ENTERED_SURFACE);
      else for (auto it = Self->InputBoundaries.rbegin(); it != Self->InputBoundaries.rend(); it++) {
         auto &bounds = *it;

         // When the user holds a mouse button over a vector, a 'button lock' will be held.  This causes all events to
         // be captured by that vector until the button is released.

         if ((Self->ButtonLock) and (Self->ButtonLock IS bounds.VectorID));
         else if ((Self->ButtonLock) and (Self->ButtonLock != bounds.VectorID)) continue;
         else { // No button lock, perform a simple bounds check
            bool in_bounds = (input->X >= bounds.BX1) and (input->Y >= bounds.BY1) and
                             (input->X < bounds.BX2) and (input->Y < bounds.BY2);
            if (!in_bounds) continue;
         }

         parasol::ScopedObjectLock<objVector> lock(bounds.VectorID);
         if (!lock.granted()) continue;
         auto vector = lock.obj;

         // Additional bounds check to cater for transforms, clip masks etc.

         if (Self->ButtonLock != vector->Head.UID) {
            if (vecPointInPath(vector, input->X, input->Y) != ERR_Okay) continue;
         }

         if (Self->LastMovementVector != vector->Head.UID) {
            send_enter_event(vector, input, bounds.X, bounds.Y);
         }

         // Determine status of the button lock.

         if ((input->Type IS JET_LMB) and (!(input->Flags & JTYPE_REPEATED))) {
            Self->ButtonLock = input->Value ? vector->Head.UID : 0;
         }

         if (vector->Cursor) cursor = vector->Cursor;

         InputEvent event = *input;
         event.Next = NULL;
         event.X = input->X - bounds.X; // Coords to be relative to the vector, not the surface
         event.Y = input->Y - bounds.Y;
         send_input_event(vector, &event);

         if (input->Flags & (JTYPE_ANCHORED|JTYPE_MOVEMENT)) {
            if ((Self->LastMovementVector) and (Self->LastMovementVector != vector->Head.UID)) {
               parasol::ScopedObjectLock<objVector> lock(Self->LastMovementVector);
               if (lock.granted()) send_left_event(lock.obj, input);
            }

            Self->LastMovementVector = vector->Head.UID;
         }

         processed = true;
         break; // Input consumed
      }

      // If no vectors received a hit for a movement message, we may need to inform the last active vector that the
      // cursor left its area.

      if ((Self->LastMovementVector) and (input->Flags & (JTYPE_ANCHORED|JTYPE_MOVEMENT)) and (!processed)) {
         parasol::ScopedObjectLock<objVector> lock(Self->LastMovementVector);
         Self->LastMovementVector = 0;
         if (lock.granted()) send_left_event(lock.obj, input);
      }
   }

   if (Self->SurfaceID) {
      parasol::ScopedObjectLock<objSurface> lock(Self->SurfaceID);
      if (lock.granted() and (lock.obj->Cursor != cursor)) {
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
   { "Viewport",     FDF_OBJECT|FD_R,            ID_VECTORVIEWPORT, NULL, NULL },
   { "Bitmap",       FDF_OBJECT|FDF_RW,          ID_BITMAP, NULL, (APTR)SET_Bitmap },
   { "Defs",         FDF_STRUCT|FDF_PTR|FDF_SYSTEM|FDF_R, (MAXINT)"KeyStore", NULL, NULL },
   { "Surface",      FDF_OBJECTID|FDF_RI,        ID_SURFACE, NULL, (APTR)SET_Surface },
   { "Flags",        FDF_LONGFLAGS|FDF_RW,       (MAXINT)&clVectorSceneFlags, NULL, NULL },
   { "PageWidth",    FDF_LONG|FDF_RW,            0, NULL, (APTR)SET_PageWidth },
   { "PageHeight",   FDF_LONG|FDF_RW,            0, NULL, (APTR)SET_PageHeight },
   { "SampleMethod", FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clVectorSceneSampleMethod, NULL, NULL },
   END_FIELD
};

static ERROR init_vectorscene(void)
{
   return(CreateObject(ID_METACLASS, 0, &clVectorScene,
      FID_ClassVersion|TFLOAT, VER_VECTORSCENE,
      FID_Name|TSTR,      "VectorScene",
      FID_Category|TLONG, CCF_GRAPHICS,
      FID_Actions|TPTR,   clVectorSceneActions,
      FID_Methods|TARRAY, clVectorSceneMethods,
      FID_Fields|TARRAY,  clSceneFields,
      FID_Size|TLONG,     sizeof(objVectorScene),
      FID_Path|TSTR,      "modules:vector",
      TAGEND));
}
