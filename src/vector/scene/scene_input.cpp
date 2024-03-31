// Input event handling for VectorScene

//********************************************************************************************************************
// Build a list of all child viewports that have a bounding box intersecting with (X,Y).  Transforms
// are taken into account through use of BX1,BY1,BX2,BY2.  The list is sorted starting from the background to the
// foreground.

void get_viewport_at_xy_node(extVector *Vector, std::vector<std::vector<extVectorViewport *>> &Collection, DOUBLE X, DOUBLE Y, LONG Branch)
{
   if ((size_t)Branch >= Collection.size()) Collection.resize(Branch+1);

   for (auto node=Vector; node; node=(extVector *)node->Next) {
      if (node->Class->ClassID IS ID_VECTORVIEWPORT) {
         auto vp = (extVectorViewport *)node;

         if (vp->dirty()) gen_vector_path(vp);

         if (vp->vpBounds.hit_test(X, Y)) Collection[Branch].emplace_back(vp);
      }

      if (node->Child) get_viewport_at_xy_node((extVector *)node->Child, Collection, X, Y, Branch + 1);
   }
}

//********************************************************************************************************************

extVectorViewport * get_viewport_at_xy(extVectorScene *Scene, DOUBLE X, DOUBLE Y)
{
   std::vector<std::vector<extVectorViewport *>> viewports;
   get_viewport_at_xy_node((extVector *)Scene->Viewport, viewports, X, Y, 0);

   // From front to back, determine the first path that the (X,Y) point resides in.

   for (auto branch = viewports.rbegin(); branch != viewports.rend(); branch++) {
      for (auto const vp : *branch) {
         if (vp->vpClip) { // A non-rectangular clipping path applies.
            agg::rasterizer_scanline_aa<> raster;
            raster.add_path(vp->BasePath); // NB: Path is already transformed
            if (raster.hit_test(X, Y)) return vp;
         }
         else return vp; // If no complex transforms are present, the hit-test is passed
      }
   }

   // No child viewports were hit, revert to main

   return (extVectorViewport *)Scene->Viewport;
}

//********************************************************************************************************************
// Send input event(s) to client subscribers

static void send_input_events(extVector *Vector, InputEvent *Event, bool Propagate = false)
{
   if (!Vector->InputSubscriptions) {
      if ((Propagate) and (Vector->Parent) and (Vector->Parent->Class->BaseClassID IS ID_VECTOR)) {
         send_input_events((extVector *)Vector->Parent, Event, true);
      }
      return;
   }

   bool consumed = false;
   for (auto it=Vector->InputSubscriptions->begin(); it != Vector->InputSubscriptions->end(); ) {
      auto &sub = *it;

      if (((Event->Mask & JTYPE::REPEATED) != JTYPE::NIL) and ((sub.Mask & JTYPE::REPEATED) IS JTYPE::NIL)) it++;
      else if ((sub.Mask & Event->Mask) != JTYPE::NIL) {
         ERR result = ERR::Terminate;
         consumed = true;

         if (sub.Callback.isC()) {
            pf::SwitchContext ctx(sub.Callback.StdC.Context);
            auto callback = (ERR (*)(objVector *, InputEvent *, APTR))sub.Callback.StdC.Routine;
            result = callback(Vector, Event, sub.Callback.StdC.Meta);
         }
         else if (sub.Callback.isScript()) {
            ScriptArg args[] = {
               ScriptArg("Vector", Vector, FDF_OBJECT),
               ScriptArg("InputEvent:Events", Event, FDF_STRUCT)
            };
            scCallback(sub.Callback.Script.Script, sub.Callback.Script.ProcedureID, args, std::ssize(args), &result);
         }

         if (result IS ERR::Terminate) it = Vector->InputSubscriptions->erase(it);
         else it++;
      }
      else it++;
   }

   // Some events can bubble-up if they are not intercepted by the target vector.

   if ((!consumed) and (Event->Type IS JET::WHEEL)) {
      if ((Vector->Parent) and (Vector->Parent->Class->BaseClassID IS ID_VECTOR)) {
         send_input_events((extVector *)Vector->Parent, Event, true);
      }
   }
}

//********************************************************************************************************************

static void send_enter_event(extVector *Vector, const InputEvent *Event, DOUBLE X = 0, DOUBLE Y = 0)
{
   InputEvent event = {
      .Next        = NULL,
      .Value       = DOUBLE(Vector->UID),
      .Timestamp   = Event->Timestamp,
      .RecipientID = Vector->UID,
      .OverID      = Vector->UID,
      .AbsX        = Event->X,
      .AbsY        = Event->Y,
      .X           = Event->X - X,
      .Y           = Event->Y - Y,
      .DeviceID    = Event->DeviceID,
      .Type        = JET::CROSSED_IN,
      .Flags       = JTYPE::CROSSING,
      .Mask        = JTYPE::CROSSING
   };
   send_input_events(Vector, &event);
}

//********************************************************************************************************************

static void send_left_event(extVector *Vector, const InputEvent *Event, DOUBLE X = 0, DOUBLE Y = 0)
{
   InputEvent event = {
      .Next        = NULL,
      .Value       = DOUBLE(Vector->UID),
      .Timestamp   = Event->Timestamp,
      .RecipientID = Vector->UID,
      .OverID      = Vector->UID,
      .AbsX        = Event->X,
      .AbsY        = Event->Y,
      .X           = Event->X - X,
      .Y           = Event->Y - Y,
      .DeviceID    = Event->DeviceID,
      .Type        = JET::CROSSED_OUT,
      .Flags       = JTYPE::CROSSING,
      .Mask        = JTYPE::CROSSING
   };
   send_input_events(Vector, &event);
}

//********************************************************************************************************************

static void send_wheel_event(extVectorScene *Scene, extVector *Vector, const InputEvent *Event)
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
      .Type        = JET::WHEEL,
      .Flags       = JTYPE::ANALOG|JTYPE::EXT_MOVEMENT,
      .Mask        = JTYPE::EXT_MOVEMENT
   };
   send_input_events(Vector, &event);
}

//********************************************************************************************************************
// Receiver for input events from the Surface that hosts the scene graph.  Events are distributed to input
// subscribers.

ERR scene_input_events(const InputEvent *Events, LONG Handle)
{
   pf::Log log(__FUNCTION__);

   auto Self = (extVectorScene *)CurrentContext();
   if (!Self->SurfaceID) return ERR::Okay; // Sanity check

   auto cursor = PTC::NIL;

   // Distribute input events to vectors that have subscriptions.
   // Be mindful that client code can potentially destroy the scene's surface at any time.
   //
   // NOTE: The ActiveVector refers to the vector that received the most recent input movement event.  It
   // receives wheel events and button presses.

   for (auto input=Events; input; input=input->Next) {
      if ((input->Flags & (JTYPE::ANCHORED|JTYPE::MOVEMENT)) != JTYPE::NIL) {
         while ((input->Next) and ((input->Next->Flags & JTYPE::MOVEMENT) != JTYPE::NIL)) { // Consolidate movement
            input = input->Next;
         }
      }

      if (input->OverID != Self->SurfaceID) {
         // Activity occurring on another surface may be reported to us in circumstances where our surface is modal.
         continue;
      }

      // Focus management - clicking with the LMB can result in a change of focus.

      if (((input->Flags & JTYPE::BUTTON) != JTYPE::NIL) and (input->Type IS JET::LMB) and (input->Value)) {
         apply_focus(Self, (extVector *)get_viewport_at_xy(Self, input->X, input->Y));
      }

      if (input->Type IS JET::WHEEL) {
         if (Self->ActiveVector) {
            pf::ScopedObjectLock<extVector> lock(Self->ActiveVector);
            if (lock.granted()) send_wheel_event(Self, lock.obj, input);
         }
      }
      else if (input->Type IS JET::CROSSED_OUT) {
         if (Self->ActiveVector) {
            pf::ScopedObjectLock<extVector> lock(Self->ActiveVector);
            if (lock.granted()) send_left_event(lock.obj, input, Self->ActiveVectorX, Self->ActiveVectorY);
         }
      }
      else if (input->Type IS JET::CROSSED_IN);
      else if ((input->Flags & JTYPE::BUTTON) != JTYPE::NIL) {
         OBJECTID target = Self->ButtonLock ? Self->ButtonLock : Self->ActiveVector;

         if (target) {
            pf::ScopedObjectLock<extVector> lk_vector(target);
            if (lk_vector.granted()) {
               InputEvent event = *input;
               event.Next   = NULL;
               event.OverID = Self->ActiveVector;
               event.AbsX   = input->X; // Absolute coordinates are not translated.
               event.AbsY   = input->Y;
               event.X      = Self->ActiveVectorX;
               event.Y      = Self->ActiveVectorY;
               send_input_events(lk_vector.obj, &event);

               if ((input->Type IS JET::LMB) and ((input->Flags & JTYPE::REPEATED) IS JTYPE::NIL)) {
                  Self->ButtonLock = input->Value ? target : 0;
               }
            }

            if (!Self->ButtonLock) {
               // If the button has been released then we need to compute the correct cursor and check if
               // an enter event is required.  This code has been pulled from the JTYPE::MOVEMENT handler
               // and reduced appropriately.

               if (cursor IS PTC::NIL) cursor = PTC::DEFAULT;
               bool processed = false;
               for (auto it = Self->InputBoundaries.rbegin(); it != Self->InputBoundaries.rend(); it++) {
                  auto &bounds = *it;

                  if ((processed) and (bounds.cursor IS PTC::NIL)) continue;

                  if (!bounds.bounds.hit_test(input->X, input->Y)) continue;

                  pf::ScopedObjectLock<extVector> lock(bounds.vector_id);
                  if (!lock.granted()) continue;
                  auto vector = lock.obj;

                  if (vecPointInPath(vector, input->X, input->Y) != ERR::Okay) continue;

                  if ((!Self->ButtonLock) and (vector->Cursor != PTC::NIL)) cursor = vector->Cursor;

                  if (bounds.pass_through) {
                     // For pass-through subscriptions input events are ignored, but cursor changes still apply.
                     continue;
                  }

                  if (Self->ActiveVector != bounds.vector_id) {
                     send_enter_event(vector, input, bounds.x, bounds.y);
                  }

                  if (!processed) {
                     DOUBLE tx = input->X, ty = input->Y; // Invert the coordinates to pass localised coords to the vector.
                     auto invert = ~vector->Transform; // Presume that prior path generation has configured the transform.
                     invert.transform(&tx, &ty);

                     if ((Self->ActiveVector) and (Self->ActiveVector != vector->UID)) {
                        pf::ScopedObjectLock<extVector> lock(Self->ActiveVector);
                        if (lock.granted()) send_left_event(lock.obj, input, Self->ActiveVectorX, Self->ActiveVectorY);
                     }

                     Self->ActiveVector  = vector->UID;
                     Self->ActiveVectorX = tx;
                     Self->ActiveVectorY = ty;

                     processed = true;
                  }

                  if (cursor IS PTC::DEFAULT) continue; // Keep scanning in case an input boundary defines a cursor.
                  else break; // Input consumed and cursor image identified.
               }

               // If no vectors received a hit for a movement message, we may need to inform the last active vector that the
               // cursor left its area.

               if ((Self->ActiveVector) and (!processed)) {
                  pf::ScopedObjectLock<extVector> lock(Self->ActiveVector);
                  Self->ActiveVector = 0;
                  if (lock.granted()) send_left_event(lock.obj, input, Self->ActiveVectorX, Self->ActiveVectorY);
               }
            }
         }
      }
      else if ((input->Flags & (JTYPE::ANCHORED|JTYPE::MOVEMENT)) != JTYPE::NIL) {
         if (cursor IS PTC::NIL) cursor = PTC::DEFAULT;
         bool processed = false;
         for (auto it = Self->InputBoundaries.rbegin(); it != Self->InputBoundaries.rend(); it++) {
            auto &bounds = *it;

            if ((processed) and (bounds.cursor IS PTC::NIL)) continue;

            // When the user holds a mouse button over a vector, a 'button lock' will be held.  This causes all events to
            // be captured by that vector until the button is released.

            bool in_bounds = false;
            if ((Self->ButtonLock) and (Self->ButtonLock IS bounds.vector_id));
            else if ((Self->ButtonLock) and (Self->ButtonLock != bounds.vector_id)) continue;
            else { // No button lock, perform a simple bounds check
               in_bounds = bounds.bounds.hit_test(input->X, input->Y);
               if (!in_bounds) continue;
            }

            pf::ScopedObjectLock<extVector> lock(bounds.vector_id);
            if (!lock.granted()) continue;
            auto vector = lock.obj;

            // Additional bounds check to cater for transforms, clip masks etc.

            if (in_bounds) {
               if (vecPointInPath(vector, input->X, input->Y) != ERR::Okay) continue;
            }

            if (Self->ActiveVector != bounds.vector_id) {
               send_enter_event(vector, input, bounds.x, bounds.y);
            }

            if ((!Self->ButtonLock) and (vector->Cursor != PTC::NIL)) cursor = vector->Cursor;

            if (bounds.pass_through) {
               // For pass-through subscriptions input events are ignored, but cursor changes still apply.
               continue;
            }

            if (!processed) {
               DOUBLE tx = input->X, ty = input->Y; // Invert the coordinates to pass localised coords to the vector.
               auto invert = ~vector->Transform; // Presume that prior path generation has configured the transform.
               invert.transform(&tx, &ty);

               InputEvent event = *input;
               event.Next   = NULL;
               event.OverID = vector->UID;
               event.AbsX   = input->X; // Absolute coordinates are not translated.
               event.AbsY   = input->Y;
               event.X      = tx;
               event.Y      = ty;
               send_input_events(vector, &event);

               if ((Self->ActiveVector) and (Self->ActiveVector != vector->UID)) {
                  pf::ScopedObjectLock<extVector> lock(Self->ActiveVector);
                  if (lock.granted()) send_left_event(lock.obj, input, Self->ActiveVectorX, Self->ActiveVectorY);
               }

               Self->ActiveVector  = vector->UID;
               Self->ActiveVectorX = tx;
               Self->ActiveVectorY = ty;

               processed = true;
            }

            if (cursor IS PTC::DEFAULT) continue; // Keep scanning in case an input boundary defines a cursor.
            else break; // Input consumed and cursor image identified.
         }

         // If no vectors received a hit for a movement message, we may need to inform the last active vector that the
         // cursor left its area.

         if ((Self->ActiveVector) and (!processed)) {
            pf::ScopedObjectLock<extVector> lock(Self->ActiveVector);
            Self->ActiveVector = 0;
            if (lock.granted()) send_left_event(lock.obj, input, Self->ActiveVectorX, Self->ActiveVectorY);
         }
      }
      else log.warning("Unrecognised movement type %d", LONG(input->Type));
   }

   if (!Self->ButtonLock) {
      if (cursor IS PTC::NIL) cursor = PTC::DEFAULT; // Revert the cursor to the default if nothing is defined

      if (Self->Cursor != cursor) {
         Self->Cursor = cursor;
         pf::ScopedObjectLock<objSurface> surface(Self->SurfaceID);
         if (surface.granted() and (surface.obj->Cursor != Self->Cursor)) {
            surface.obj->setCursor(cursor);
         }
      }
   }

   return ERR::Okay;
}
