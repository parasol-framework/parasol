
void copy_bkgd(const std::vector<SurfaceRecord> &, LONG, LONG, LONG, WORD, WORD, WORD, WORD, extBitmap *, extBitmap *, WORD, BYTE);

ERROR _expose_surface(OBJECTID SurfaceID, const std::vector<SurfaceRecord> &list, LONG index, LONG Limit, LONG X, LONG Y, LONG Width, LONG Height, LONG Flags)
{
   pf::Log log("expose_surface");
   extBitmap *bitmap;
   ClipRectangle abs;
   LONG i, j;
   bool skip;
   OBJECTID parent_id;

   if ((Width < 1) or (Height < 1)) return ERR_Okay;
   if (!SurfaceID) return log.warning(ERR_NullArgs);
   if (index >= Limit) return log.warning(ERR_OutOfRange);

   if ((!(list[index].Flags & RNF_VISIBLE)) or ((list[index].Width < 1) or (list[index].Height < 1))) {
      log.trace("Surface %d invisible or too small to draw.", SurfaceID);
      return ERR_Okay;
   }

   // Calculate the absolute coordinates of the exposed area

   if (Flags & EXF_ABSOLUTE) {
      abs.Left   = X;
      abs.Top    = Y;
      abs.Right  = Width;
      abs.Bottom = Height;
      Flags &= ~EXF_ABSOLUTE;
   }
   else {
      abs.Left   = list[index].Left + X;
      abs.Top    = list[index].Top + Y;
      abs.Right  = abs.Left + Width;
      abs.Bottom = abs.Top  + Height;
   }

   log.traceBranch("Surface:%d, %dx%d,%dx%d Flags: $%.4x", SurfaceID, abs.Left, abs.Top, abs.Right-abs.Left, abs.Bottom-abs.Top, Flags);

   // If the object is transparent, we need to scan back to a visible parent

   if (list[index].Flags & RNF_TRANSPARENT) {
      log.trace("Surface is transparent; scan to solid starting from index %d.", index);

      OBJECTID id = list[index].SurfaceID;
      for (j=index; j > 0; j--) {
         if (list[j].SurfaceID != id) continue;
         if (list[j].Flags & RNF_TRANSPARENT) id = list[j].ParentID;
         else break;
      }
      Flags |= EXF_CHILDREN;
      index = j;

      log.trace("New index %d.", index);
   }

   // Check if the exposed dimensions are outside of our boundary and/or our parent(s) boundaries.  If so then we must
   // restrict the exposed dimensions.  NOTE: This loop looks strange but is both correct & fast.  Don't touch it!

   for (i=index, parent_id = SurfaceID; ;) {
      if (!(list[i].Flags & RNF_VISIBLE)) return ERR_Okay;
      clip_rectangle(&abs, (ClipRectangle *)&list[i].Left);
      if (!(parent_id = list[i].ParentID)) break;
      i--;
      while (list[i].SurfaceID != parent_id) i--;
   }

   if ((abs.Left >= abs.Right) or (abs.Top >= abs.Bottom)) return ERR_Okay;

   // Check that the expose area actually overlaps the target surface

   if (abs.Left   >= list[index].Right) return ERR_Okay;
   if (abs.Top    >= list[index].Bottom) return ERR_Okay;
   if (abs.Right  <= list[index].Left) return ERR_Okay;
   if (abs.Bottom <= list[index].Top) return ERR_Okay;

   // Cursor split routine.  The purpose of this is to eliminate as much flicker as possible from the cursor when
   // exposing large areas.
   //
   // We scan for the software cursor to see if the bottom of the cursor intersects with our expose region.  If it
   // does, split ExposeSurface() into top and bottom regions.

#ifndef _WIN32
   if (!(Flags & EXF_CURSOR_SPLIT)) {
      LONG cursor;
      for (cursor=index+1; (cursor < Limit) and (!(list[cursor].Flags & RNF_CURSOR)); cursor++);
      if (cursor < Limit) {
         if ((list[cursor].SurfaceID) and (list[cursor].Bottom < abs.Bottom) and (list[cursor].Bottom > abs.Top) and
             (list[cursor].Right > abs.Left) and (list[cursor].Left < abs.Right)) {
            pf::Log log("expose_surface");
            log.traceBranch("Splitting cursor.");
            _expose_surface(SurfaceID, list, index, Limit, abs.Left, abs.Top, abs.Right, list[cursor].Bottom, EXF_CURSOR_SPLIT|EXF_ABSOLUTE|Flags);
            _expose_surface(SurfaceID, list, index, Limit, abs.Left, list[cursor].Bottom, abs.Right, abs.Bottom, EXF_CURSOR_SPLIT|EXF_ABSOLUTE|Flags);
            return ERR_Okay;
         }
      }
   }
#endif

   // The expose routine starts from the front and works to the back, so if the EXF_CHILDREN flag has been specified,
   // the first thing we do is scan to the final child that is listed in this particular area.

   if (Flags & EXF_CHILDREN) {
      // Change the index to the root bitmap of the exposed object
      index = find_bitmap_owner(list, index);
      for (i=index; (i < Limit-1) and (list[i+1].Level > list[index].Level); i++); // Go all the way to the end of the list
   }
   else i = index;

   for (; i >= index; i--) {
      // Ignore non-visible surfaces

      if (list[i].Flags & RNF_TRANSPARENT) continue;
      if ((list[i].Flags & RNF_CURSOR) and (list[i].SurfaceID != SurfaceID)) continue;

      // If this is not a root bitmap object, skip it (i.e. consider it like a region)

      skip = false;
      parent_id = list[i].ParentID;
      for (j=i-1; j >= index; j--) {
         if (list[j].SurfaceID IS parent_id) {
            if (list[j].BitmapID IS list[i].BitmapID) skip = TRUE;
            break;
         }
      }
      if (skip) continue;

      ClipRectangle childexpose = abs;

      if (i != index) {
         // Check this child object and its parents to make sure they are visible

         parent_id = list[i].SurfaceID;
         for (j=i; (j >= index) and (parent_id); j--) {
            if (list[j].SurfaceID IS parent_id) {
               if (!(list[j].Flags & RNF_VISIBLE)) {
                  skip = TRUE;
                  break;
               }

               clip_rectangle(&childexpose, (ClipRectangle *)&list[j].Left);

               parent_id = list[j].ParentID;
            }
         }
         if (skip) continue;

         // Skip this surface if there is nothing to be seen (lies outside the expose boundary)

         if ((childexpose.Right <= childexpose.Left) or (childexpose.Bottom <= childexpose.Top)) continue;
      }

      // Do the expose

      ERROR error;
      if (!(error = AccessObjectID(list[i].BitmapID, 2000, &bitmap))) {
         expose_buffer(list, Limit, i, i, childexpose.Left, childexpose.Top, childexpose.Right, childexpose.Bottom, list[index].DisplayID, bitmap);
         ReleaseObject(bitmap);
      }
      else {
         log.trace("Unable to access internal bitmap, sending delayed expose message.  Error: %s", GetErrorMsg(error));

         struct drwExpose expose = {
            .X      = childexpose.Left   - list[i].Left,
            .Y      = childexpose.Top    - list[i].Top,
            .Width  = childexpose.Right  - childexpose.Left,
            .Height = childexpose.Bottom - childexpose.Top,
            .Flags  = 0
         };
         QueueAction(MT_DrwExpose, list[i].SurfaceID, &expose);
      }
   }

   // These flags should be set if the surface has had some area of it redrawn prior to the ExposeSurface() call.
   // This can be very important if the application has been writing to the surface directly rather than the more
   // conventional drawing procedures.

   // If the surface bitmap has not been changed, volatile redrawing just wastes CPU time for the user.

   if (Flags & (EXF_REDRAW_VOLATILE|EXF_REDRAW_VOLATILE_OVERLAP)) {
      // Redraw any volatile regions that intersect our expose area (such regions must be updated to reflect the new
      // background graphics).  Note that this routine does a fairly deep scan, due to the selective area copying
      // features in our system (i.e. we cannot just skim over the stuff that is immediately in front of us).
      //
      // EXF_REDRAW_VOLATILE: Redraws every single volatile object that intersects the expose, including internal
      //    volatile children.
      //
      // EXF_REDRAW_VOLATILE_OVERLAP: Only redraws volatile objects that obscure the expose from a position outside of
      //    the surface and its children.  Useful if no redrawing has occurred internally, but the surface object has
      //    been moved to a new position and the parents need to be redrawn.

      LONG level = list[index].Level + 1;

      if ((Flags & EXF_REDRAW_VOLATILE_OVERLAP)) { //OR (Flags & EXF_CHILDREN)) {
         // All children in our area have already been redrawn or do not need redrawing, so skip past them.

         for (i=index+1; (i < Limit) and (list[i].Level > list[index].Level); i++);
         if (list[i-1].Flags & RNF_CURSOR) i--; // Never skip past the cursor
      }
      else {
         i = index;
         if (i < Limit) i = i + 1;
         while ((i < Limit) and (list[i].BitmapID IS list[index].BitmapID)) i++;
      }

      pf::Log log(__FUNCTION__);
      log.traceBranch("Redraw volatiles from idx %d, area %dx%d,%dx%d", i, abs.Left, abs.Top, abs.Right - abs.Left, abs.Bottom - abs.Top);

      if (i < tlVolatileIndex) i = tlVolatileIndex; // Volatile index allows the starting point to be specified

      // Redraw and expose volatile overlaps

      for (; (i < Limit) and (list[i].Level > 1); i++) {
         if (list[i].Level < level) level = list[i].Level; // Drop the comparison level down so that we only observe objects in our general drawing space

         if (!(list[i].Flags & RNF_VISIBLE)) {
            j = list[i].Level;
            while ((i+1 < Limit) and (list[i+1].Level > j)) i++;
            continue;
         }

         if (list[i].Flags & (RNF_VOLATILE|RNF_COMPOSITE|RNF_CURSOR)) {
            if (list[i].SurfaceID IS SurfaceID) continue;

            if ((list[i].Right > abs.Left) and (list[i].Bottom > abs.Top) and
                (list[i].Left < abs.Right) and (list[i].Top < abs.Bottom)) {

               if (!(list[i].Flags & RNF_COMPOSITE)) { // Composites never require redrawing because they are not completely volatile, but we will expose them
                  _redraw_surface(list[i].SurfaceID, list, i, Limit, abs.Left, abs.Top, abs.Right, abs.Bottom, IRF_IGNORE_CHILDREN); // Redraw the volatile surface, ignore children
               }

               _expose_surface(list[i].SurfaceID, list, i, Limit, abs.Left, abs.Top, abs.Right, abs.Bottom, EXF_ABSOLUTE); // Redraw the surface, ignore children

               //while (list[i].BitmapID IS list[i+1].BitmapID) i++; This only works if the surfaces being skipped are completely intersecting one another.
            }
         }
      }
   }
   else {
      // Look for a software cursor at the end of the surfacelist and redraw it.  (We have to redraw the cursor as
      // expose_buffer() ignores it for optimisation purposes.)

      LONG i = Limit - 1;
      if ((list[i].Flags & RNF_CURSOR) and (list[i].SurfaceID != SurfaceID)) {
         if ((list[i].Right > abs.Left) and (list[i].Bottom > abs.Top) and
             (list[i].Left < abs.Right) and (list[i].Top < abs.Bottom)) {

            pf::Log log(__FUNCTION__);
            log.traceBranch("Redrawing/Exposing cursor.");

            if (!(list[i].Flags & RNF_COMPOSITE)) { // Composites never require redrawing because they are not completely volatile
               _redraw_surface(list[i].SurfaceID, list, i, Limit, abs.Left, abs.Top, abs.Right, abs.Bottom, NULL);
            }

            _expose_surface(list[i].SurfaceID, list, i, Limit, abs.Left, abs.Top, abs.Right, abs.Bottom, EXF_ABSOLUTE);
         }
      }
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-ACTION-
Draw: Redraws the contents of a surface object.

Calling the Draw action on a surface object will send redraw messages to every hook that has been attached to the
surface object's drawing system.  This has the effect of redrawing all graphics within the surface object.  The
procedure is as follows:

<list type="ordered">
<li>If the surface object's #Colour field has been set, the target bitmap will be cleared to that colour.</li>
<li>If the surface is volatile, graphics from background surfaces will be copied to the  target bitmap.</li>
<li>Subscribers to the surface object are now called via their hooks so that they can draw to the bitmap.</li>
<li>The bitmap is copied to the video display buffer to complete the process.</li>
</>

Please be aware that:

<list>
<li>If the target surface contains child surfaces, they will not be redrawn unless they are volatile (using special
effects such as transparency, or using the region flag will make a surface volatile).</li>
<li>If the surface object has not had its background colour set, or if the object is not volatile, the bitmap
contents will not be automatically cleared (this is advantageous in situations where a particular object will clear
the surface area first).</li>
</>

-END-

*********************************************************************************************************************/

ERROR SURFACE_Draw(extSurface *Self, struct acDraw *Args)
{
   pf::Log log;

   // If the Surface object is invisible, return immediately

   if ((!(Self->Flags & RNF_VISIBLE)) or (tlNoDrawing) or (Self->Width < 1) or (Self->Height < 1)) {
      log.trace("Not drawing (invisible or tlNoDrawing set).");
      return ERR_Okay|ERF_Notified;
   }

   // Do not perform manual redraws when a redraw is scheduled.

   if (Self->RedrawScheduled) return ERR_Okay|ERF_Notified;

   LONG x, y, width, height;
   if (!Args) {
      x = 0;
      y = 0;
      width  = Self->Width;
      height = Self->Height;
   }
   else {
      x      = Args->X;
      y      = Args->Y;
      width  = Args->Width;
      height = Args->Height;
      if (!width) width = Self->Width;
      if (!height) height = Self->Height;
   }

   // Check if other draw messages are queued for this object - if so, do not do anything until the final message is reached.

   MEMORYID msgqueue = GetResource(RES_MESSAGE_QUEUE);
   APTR queue;
   if (!AccessMemoryID(msgqueue, MEM_READ, 3000, &queue)) {
      UBYTE msgbuffer[sizeof(Message) + sizeof(ActionMessage) + sizeof(struct acDraw)];
      LONG msgindex = 0;
      while (!ScanMessages(queue, &msgindex, MSGID_ACTION, msgbuffer, sizeof(msgbuffer))) {
         auto action = (ActionMessage *)(msgbuffer + sizeof(Message));

         if ((action->ActionID IS MT_DrwInvalidateRegion) and (action->ObjectID IS Self->UID)) {
            if (!action->SendArgs) {
               ReleaseMemoryID(msgqueue);
               return ERR_Okay|ERF_Notified;
            }
         }
         else if ((action->ActionID IS AC_Draw) and (action->ObjectID IS Self->UID)) {
            if (action->SendArgs IS TRUE) {
               auto msgdraw = (struct acDraw *)(action + 1);

               if (!Args) { // Tell the next message to draw everything.
                  action->SendArgs = FALSE;
               }
               else {
                  DOUBLE right  = msgdraw->X + msgdraw->Width;
                  DOUBLE bottom = msgdraw->Y + msgdraw->Height;

                  if (x < msgdraw->X) msgdraw->X = x;
                  if (y < msgdraw->Y) msgdraw->Y = y;
                  if ((x + width) > right)   right  = x + width;
                  if ((y + height) > bottom) bottom = y + height;

                  msgdraw->Width  = right - msgdraw->X;
                  msgdraw->Height = bottom - msgdraw->Y;
               }

               UpdateMessage(queue, ((Message *)msgbuffer)->UniqueID, NULL, action, sizeof(ActionMessage) + sizeof(struct acDraw));
            }
            else {
               // We do nothing here because the next draw message will draw everything.
            }

            ReleaseMemoryID(msgqueue);
            return ERR_Okay|ERF_Notified;
         }
      }
      ReleaseMemoryID(msgqueue);
   }

   log.traceBranch("%dx%d,%dx%d", x, y, width, height);
   gfxRedrawSurface(Self->UID, x, y, width, height, IRF_RELATIVE|IRF_IGNORE_CHILDREN);
   gfxExposeSurface(Self->UID, x, y, width, height, EXF_REDRAW_VOLATILE);
   return ERR_Okay|ERF_Notified;
}

/*********************************************************************************************************************

-METHOD-
Expose: Redraws a surface region to the display, preferably from its graphics buffer.

Call the Expose method to copy a surface region to the display.  The functionality is identical to that of the
ExposeSurface() function in the Surface module.  Please refer to it for further documentation.

-INPUT-
int X: X coordinate of the expose area.
int Y: Y coordinate of the expose area.
int Width: Width of the expose area.
int Height: Height of the expose area.
int(EXF) Flags: Optional flags.

-ERRORS-
Okay
-END-

*********************************************************************************************************************/

static ERROR SURFACE_Expose(extSurface *Self, struct drwExpose *Args)
{
   if (tlNoExpose) return ERR_Okay;

   // Check if other draw messages are queued for this object - if so, do not do anything until the final message is reached.

   APTR queue;
   MEMORYID msgqueue = GetResource(RES_MESSAGE_QUEUE);
   if (!AccessMemoryID(msgqueue, MEM_READ_WRITE, 3000, &queue)) {
      UBYTE msgbuffer[sizeof(Message) + sizeof(ActionMessage) + sizeof(struct drwExpose)];
      LONG msgindex = 0;
      while (!ScanMessages(queue, &msgindex, MSGID_ACTION, msgbuffer, sizeof(msgbuffer))) {
         auto action = (ActionMessage *)(msgbuffer + sizeof(Message));

         if ((action->ActionID IS MT_DrwExpose) and (action->ObjectID IS Self->UID)) {
            if (action->SendArgs) {
               auto msgexpose = (struct drwExpose *)(action + 1);

               if (!Args) {
                  // Invalidate everything
                  msgexpose->X = 0;
                  msgexpose->Y = 0;
                  msgexpose->Width  = 20000;
                  msgexpose->Height = 20000;
               }
               else {
                  LONG right  = msgexpose->X + msgexpose->Width;
                  LONG bottom = msgexpose->Y + msgexpose->Height;

                  // Ignore region if it doesn't intersect

                  if ((Args->X+Args->Width < msgexpose->X) or
                      (Args->Y+Args->Height < msgexpose->Y) or
                      (Args->X > right) or (Args->Y > bottom)) continue;

                  if (Args->X < msgexpose->X) msgexpose->X = Args->X;
                  if (Args->Y < msgexpose->Y) msgexpose->Y = Args->Y;
                  if ((Args->X + Args->Width) > right)   right  = Args->X + Args->Width;
                  if ((Args->Y + Args->Height) > bottom) bottom = Args->Y + Args->Height;

                  msgexpose->Width  = right - msgexpose->X;
                  msgexpose->Height = bottom - msgexpose->Y;
                  msgexpose->Flags  |= Args->Flags;
               }

               UpdateMessage(queue, ((Message *)msgbuffer)->UniqueID, NULL, action, sizeof(ActionMessage) + sizeof(struct drwExpose));
            }
            else {
               // We do nothing here because the next expose message will draw everything.
            }

            ReleaseMemoryID(msgqueue);
            return ERR_Okay|ERF_Notified;
         }
      }
      ReleaseMemoryID(msgqueue);
   }

   ERROR error;
   if (Args) error = gfxExposeSurface(Self->UID, Args->X, Args->Y, Args->Width, Args->Height, Args->Flags);
   else error = gfxExposeSurface(Self->UID, 0, 0, Self->Width, Self->Height, 0);

   return error;
}

/*********************************************************************************************************************

-METHOD-
InvalidateRegion: Redraws all of the content in a surface object.

Invalidating a surface object will cause everything within a specified area to be redrawn.  This includes child surface
objects that intersect with the area that you have specified.  Parent regions that overlap are not included in the
redraw.

To quickly redraw an entire surface object's content, call this method directly without supplying an argument structure.
If you want to redraw a surface object and ignore all of its surface children then you should use the Draw action
instead of this method.

If you want to refresh a surface area to the display then you should use the #Expose() method instead.  Exposing
will use the graphics buffer to refresh the graphics, thus avoiding the speed loss of a complete redraw.

-INPUT-
int X: X coordinate of the region to invalidate.
int Y: Y coordinate of the region to invalidate.
int Width:  Width of the region to invalidate.
int Height: Height of the region to invalidate.

-ERRORS-
Okay:
AccessMemoryID: Failed to access the internal surface list.
-END-

*********************************************************************************************************************/

static ERROR SURFACE_InvalidateRegion(extSurface *Self, struct drwInvalidateRegion *Args)
{
   if ((!(Self->Flags & RNF_VISIBLE)) or (tlNoDrawing) or (Self->Width < 1) or (Self->Height < 1)) {
      return ERR_Okay|ERF_Notified;
   }

   // Do not perform manual redraws when a redraw is scheduled.

   if (Self->RedrawTimer) return ERR_Okay|ERF_Notified;

   // Check if other draw messages are queued for this object - if so, do not do anything until the final message is reached.

   APTR queue;
   MEMORYID msgqueue = GetResource(RES_MESSAGE_QUEUE);
   if (!AccessMemoryID(msgqueue, MEM_READ_WRITE, 3000, &queue)) {
      LONG msgindex = 0;
      UBYTE msgbuffer[sizeof(Message) + sizeof(ActionMessage) + sizeof(struct drwInvalidateRegion)];
      while (!ScanMessages(queue, &msgindex, MSGID_ACTION, msgbuffer, sizeof(msgbuffer))) {
         auto action = (ActionMessage *)(msgbuffer + sizeof(Message));
         if ((action->ActionID IS MT_DrwInvalidateRegion) and (action->ObjectID IS Self->UID)) {
            if (action->SendArgs IS TRUE) {
               auto msginvalid = (struct drwInvalidateRegion *)(action + 1);

               if (!Args) { // Invalidate everything
                  action->SendArgs = FALSE;
               }
               else {
                  DOUBLE right  = msginvalid->X + msginvalid->Width;
                  DOUBLE bottom = msginvalid->Y + msginvalid->Height;

                  if (Args->X < msginvalid->X) msginvalid->X = Args->X;
                  if (Args->Y < msginvalid->Y) msginvalid->Y = Args->Y;
                  if ((Args->X + Args->Width) > right)   right  = Args->X + Args->Width;
                  if ((Args->Y + Args->Height) > bottom) bottom = Args->Y + Args->Height;

                  msginvalid->Width  = right - msginvalid->X;
                  msginvalid->Height = bottom - msginvalid->Y;
               }

               UpdateMessage(queue, ((Message *)msgbuffer)->UniqueID, NULL, action, sizeof(ActionMessage) + sizeof(struct drwInvalidateRegion));
            }
            else { } // We do nothing here because the next invalidation message will draw everything.

            ReleaseMemoryID(msgqueue);
            return ERR_Okay|ERF_Notified;
         }
      }
      ReleaseMemoryID(msgqueue);
   }

   if (Args) {
      gfxRedrawSurface(Self->UID, Args->X, Args->Y, Args->Width, Args->Height, IRF_RELATIVE);
      gfxExposeSurface(Self->UID, Args->X, Args->Y, Args->Width, Args->Height, EXF_CHILDREN|EXF_REDRAW_VOLATILE_OVERLAP);
   }
   else {
      gfxRedrawSurface(Self->UID, 0, 0, Self->Width, Self->Height, IRF_RELATIVE);
      gfxExposeSurface(Self->UID, 0, 0, Self->Width, Self->Height, EXF_CHILDREN|EXF_REDRAW_VOLATILE_OVERLAP);
   }

   return ERR_Okay|ERF_Notified;
}

//********************************************************************************************************************

void move_layer(extSurface *Self, LONG X, LONG Y)
{
   pf::Log log(__FUNCTION__);

   // If the coordinates are unchanged, do nothing

   if ((X IS Self->X) and (Y IS Self->Y)) return;

   if (!Self->initialised()) {
      Self->X = X;
      Self->Y = Y;
      return;
   }

   // This subroutine is used if the surface object is display-based

   if (!Self->ParentID) {
      objDisplay *display;
      if (!AccessObjectID(Self->DisplayID, 2000, &display)) {
         // Subtract the host window's LeftMargin and TopMargin as MoveToPoint() is based on the coordinates of the window frame.

         LONG left_margin = display->LeftMargin;
         LONG top_margin = display->TopMargin;
         ReleaseObject(display);

         if (!acMoveToPoint(display, X - left_margin, Y - top_margin, 0, MTF_X|MTF_Y)) {
            Self->X = X;
            Self->Y = Y;
            UpdateSurfaceRecord(Self);
         }
      }
      else log.warning(ERR_AccessObject);

      return;
   }

   // If the window is invisible, set the new coordinates and return immediately.

   if (!(Self->Flags & RNF_VISIBLE)) {
      Self->X = X;
      Self->Y = Y;
      UpdateSurfaceRecord(Self);
      return;
   }

   LONG vindex, index;
   if ((index = find_surface_list(Self)) IS -1) return;

   ClipRectangle old(glSurfaces[index].Left, glSurfaces[index].Top, glSurfaces[index].Right, glSurfaces[index].Bottom);

   LONG destx = old.Left + X - Self->X;
   LONG desty = old.Top  + Y - Self->Y;

   LONG parent_index = find_parent_list(glSurfaces, Self);

   // Since we do not own our graphics buffer, we need to shift the content in the buffer first, then send an
   // expose message to have the changes displayed on screen.
   //
   // This process is made more complex if there are siblings above and intersecting our surface.

   auto volatilegfx = check_volatile(glSurfaces, index);

   log.traceBranch("MoveLayer: Using simple expose technique [%s]", (volatilegfx ? "Volatile" : "Not Volatile"));

   Self->X = X;
   Self->Y = Y;

   update_surface_copy(Self);

   bool redraw;
   if (Self->Flags & RNF_TRANSPARENT) { // Transparent surfaces are treated as volatile if they contain graphics
      if (Self->CallbackCount > 0) redraw = true;
      else redraw = false;
   }
   else if ((volatilegfx) and (!(Self->Flags & RNF_COMPOSITE))) redraw = true;
   else if (glSurfaces[index].BitmapID IS glSurfaces[parent_index].BitmapID) redraw = true;
   else redraw = false;

   if (redraw) _redraw_surface(Self->UID, glSurfaces, index, glSurfaces.size(), destx, desty, destx+Self->Width, desty+Self->Height, NULL);
   _expose_surface(Self->UID, glSurfaces, index, glSurfaces.size(), 0, 0, Self->Width, Self->Height, EXF_CHILDREN|EXF_REDRAW_VOLATILE_OVERLAP);

   // Expose underlying graphics resulting from the movement

   for (vindex=index+1; glSurfaces[vindex].Level > glSurfaces[index].Level; vindex++);
   tlVolatileIndex = vindex;
   redraw_nonintersect(Self->ParentID, glSurfaces, parent_index, (ClipRectangle *)(&glSurfaces[index].Left), &old,
      (glSurfaces[index].BitmapID IS glSurfaces[parent_index].BitmapID) ? IRF_SINGLE_BITMAP : -1,
      EXF_CHILDREN|EXF_REDRAW_VOLATILE);
   tlVolatileIndex = 0;

   refresh_pointer(Self);
}

/*********************************************************************************************************************
** Used for PRECOPY, AFTERCOPY and compositing surfaces.
**
** Self:       The surface object being drawn to.
** Index:      The index of the surface that needs its background copied.
** DestBitmap: The Bitmap related to the surface.
** Clip:       The absolute display coordinates of the expose area.
** Stage:      Either STAGE_PRECOPY or STAGE_AFTERCOPY.
*/

void prepare_background(extSurface *Self, const std::vector<SurfaceRecord> &list, LONG Limit, LONG Index, extBitmap *DestBitmap, ClipRectangle *clip, BYTE Stage)
{
   pf::Log log("prepare_bkgd");

   log.traceBranch("%d Position: %dx%d,%dx%d", list[Index].SurfaceID, clip->Left, clip->Top, clip->Right-clip->Left, clip->Bottom-clip->Top);

   LONG end = Index;
   LONG master = Index;

   // Check if a root layer is set for this object.  A RootLayer determines the layer to use when opacity and
   // background graphics have precedence.  E.g. if a Window has 50% opacity, that means that all surfaces within
   // that window need to share the opacity and the background graphics of that window.

   LONG i, j;
   if ((Self) and (list[Index].SurfaceID != Self->RootID)) {
      for (j=0; j < Limit; j++) {
         if (list[j].SurfaceID IS Self->RootID) {
            // Root layers are only considered when they are volatile (otherwise we want the current surface
            // object's own opacity settings to take precedence).  This ensures that objects like translucent
            // scrollbars can take priority if the parent is not translucent.
            //
            // If a custom root layer has been specified, then we are forced into using it as the end index.

            if (!Self->InheritedRoot) end = j; // A custom root layer has been specified by the user
            else if (list[j].Flags & RNF_VOLATILE) end = j; // The root layer is volatile and must be used
            break;
         }
      }
   }

   end = find_bitmap_owner(list, end);

   // Find the parent that owns this surface (we will use this as the starting point for our copy operation).
   // Everything that gets in the way between the parent and the location of our surface is what will be copied across.

   if (!list[end].ParentID) return;
   LONG parentindex = end;
   while ((parentindex > 0) and (list[parentindex].SurfaceID != list[end].ParentID)) parentindex--;

   // If the parent object is invisible, we need to scan back to a visible parent

   OBJECTID id = list[parentindex].SurfaceID;
   for (j=parentindex; list[parentindex].Level > 1; j--) {
      if (list[j].SurfaceID IS id) {
         if (!(list[j].Flags & RNF_TRANSPARENT)) break;
         id = list[j].ParentID;
      }
   }
   parentindex = j;

   // This loop will copy surface content to the buffered graphics area.  If the parentindex and end values are
   // correct, only siblings of the parent are considered in this loop.

   for (i=parentindex; i < end; i++) {
      if (list[i].Flags & (RNF_TRANSPARENT|RNF_CURSOR)) continue; // Ignore regions

      ClipRectangle expose(clip->Left, clip->Top, clip->Right, clip->Bottom);

      // Check the visibility of this layer and its parents

      if (restrict_region_to_parents(list, i, &expose, TRUE) <= 0) continue;

      LONG opaque;
      if (Stage IS STAGE_AFTERCOPY) {
         if (list[Index].RootID != list[Index].SurfaceID) opaque = list[Index].Opacity;
         else opaque = list[end].Opacity;
      }
      else opaque = 255;

      bool pervasive;
      if ((list[Index].Flags & RNF_PERVASIVE_COPY) and (Stage IS STAGE_AFTERCOPY)) pervasive = true;
      else pervasive = false;

      extBitmap *bitmap;
      ERROR error;
      if (!(error = AccessObjectID(list[i].BitmapID, 2000, &bitmap))) {
         copy_bkgd(list, i, end, master, expose.Left, expose.Top, expose.Right, expose.Bottom, DestBitmap, bitmap, opaque, pervasive);
         ReleaseObject(bitmap);
      }
      else {
         log.warning("prepare_bkgd: %d failed to access bitmap #%d of surface #%d (error %d).", list[Index].SurfaceID, list[i].BitmapID, list[i].SurfaceID, error);
         break;
      }
   }
}

/*********************************************************************************************************************
** Coordinates are absolute.
*/

void copy_bkgd(const std::vector<SurfaceRecord> &list, LONG Index, LONG End, LONG Master, WORD Left, WORD Top, WORD Right, WORD Bottom,
   extBitmap *DestBitmap, extBitmap *SrcBitmap, WORD Opacity, BYTE Pervasive)
{
   pf::Log log(__FUNCTION__);

   // Scan for overlapping parent/sibling regions and avoid them

   for (LONG i=Index+1; (i < End) and (list[i].Level > 1); i++) {
      if (list[i].Flags & (RNF_CURSOR|RNF_COMPOSITE)); // Ignore regions
      else if (!(list[i].Flags & RNF_VISIBLE)); // Skip hidden surfaces and their content
      else if (list[i].Flags & RNF_TRANSPARENT) continue; // Invisibles may contain important regions we have to block
      else if ((Pervasive) and (list[i].Level > list[Index].Level)); // If the copy is pervasive then all children must be ignored (so that we can copy translucent graphics over them)
      else {
         ClipRectangle listclip(list[i].Left, list[i].Top, list[i].Right, list[i].Bottom);

         if ((listclip.Left < Right) and (listclip.Top < Bottom) and (listclip.Right > Left) and (listclip.Bottom > Top)) {
            if (listclip.Left <= Left) listclip.Left = Left;
            else copy_bkgd(list, Index, End, Master, Left, Top, listclip.Left, Bottom, DestBitmap, SrcBitmap, Opacity, Pervasive); // left

            if (listclip.Right >= Right) listclip.Right = Right;
            else copy_bkgd(list, Index, End, Master, listclip.Right, Top, Right, Bottom, DestBitmap, SrcBitmap, Opacity, Pervasive); // right

            if (listclip.Top <= Top) listclip.Top = Top;
            else copy_bkgd(list, Index, End, Master, listclip.Left, Top, listclip.Right, listclip.Top, DestBitmap, SrcBitmap, Opacity, Pervasive); // top

            if (listclip.Bottom < Bottom) copy_bkgd(list, Index, End, Master, listclip.Left, listclip.Bottom, listclip.Right, Bottom, DestBitmap, SrcBitmap, Opacity, Pervasive); // bottom

            return;
         }
      }

      // Skip past any children of the overlapping object.  This ensures that we only look at immediate parents and
      // siblings that are in our way.

      LONG j = i + 1;
      while (list[j].Level > list[i].Level) j++;
      i = j - 1;
   }

   // Check if the exposed dimensions are outside of our boundary and/or our parent(s) boundaries.  If so then we must
   // restrict the exposed dimensions.

   ClipRectangle expose(Left, Top, Right, Bottom);
   if (restrict_region_to_parents(list, Index, &expose, false) IS -1) return;

   log.traceBranch("[%d] Pos: %dx%d,%dx%d Bitmap: %d, Index: %d/%d", list[Index].SurfaceID, expose.Left, expose.Top, expose.Right - expose.Left, Bottom - expose.Top, list[Index].BitmapID, Index, End);

   // The region is not obscured, so perform the redraw

   auto owner = find_bitmap_owner(list, Index);

   SrcBitmap->XOffset = 0;
   SrcBitmap->YOffset = 0;
   SrcBitmap->Clip.Left   = 0;
   SrcBitmap->Clip.Top    = 0;
   SrcBitmap->Clip.Right  = SrcBitmap->Width;
   SrcBitmap->Clip.Bottom = SrcBitmap->Height;

   if (Opacity < 255) SrcBitmap->Opacity = 255 - Opacity;

   gfxCopyArea(SrcBitmap, DestBitmap, BAF_BLEND, expose.Left - list[owner].Left, expose.Top - list[owner].Top,
      expose.Right - expose.Left, expose.Bottom - expose.Top, expose.Left - list[Master].Left, expose.Top - list[Master].Top);

   SrcBitmap->Opacity = 255;
}
