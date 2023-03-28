
static void  copy_bkgd(SurfaceList *, WORD, WORD, WORD, WORD, WORD, WORD, WORD, objBitmap *, objBitmap *, WORD, BYTE);

/*****************************************************************************
** Redraw everything in RegionB that does not intersect with RegionA.
*/

static void redraw_nonintersect(OBJECTID SurfaceID, SurfaceList *List, WORD Index, WORD Total,
   struct ClipRectangle *Region, struct ClipRectangle *RegionB, LONG RedrawFlags, LONG ExposeFlags)
{
   parasol::Log log(__FUNCTION__);

   if (!SurfaceID) { // Implemented this check because an invalid SurfaceID has happened before.
      log.warning("SurfaceID == 0");
      return;
   }

   log.traceBranch("redraw_nonintersect: (A) %dx%d,%dx%d Vs (B) %dx%d,%dx%d", Region->Left, Region->Top, Region->Right, Region->Bottom, RegionB->Left, RegionB->Top, RegionB->Right, RegionB->Bottom);

   ExposeFlags |= EXF_ABSOLUTE;

   struct { LONG left, top, right, bottom; } rect = { RegionB->Left, RegionB->Top, RegionB->Right, RegionB->Bottom };

   if (rect.right > Region->Right) { // Right
      log.trace("redraw_nonrect: Right exposure");

      if (RedrawFlags != -1) _redraw_surface(SurfaceID, List, Index, Total, (rect.left > Region->Right) ? rect.left : Region->Right, rect.top, rect.right, rect.bottom, RedrawFlags);
      if (ExposeFlags != -1) _expose_surface(SurfaceID, List, Index, Total, (rect.left > Region->Right) ? rect.left : Region->Right, rect.top, rect.right, rect.bottom, ExposeFlags);
      rect.right = Region->Right;
      if (rect.left >= rect.right) return;
   }

   if (rect.bottom > Region->Bottom) { // Bottom
      log.trace("redraw_nonrect: Bottom exposure");
      if (RedrawFlags != -1) _redraw_surface(SurfaceID, List, Index, Total, rect.left, (rect.top > Region->Bottom) ? rect.top : Region->Bottom, rect.right, rect.bottom, RedrawFlags);
      if (ExposeFlags != -1) _expose_surface(SurfaceID, List, Index, Total, rect.left, (rect.top > Region->Bottom) ? rect.top : Region->Bottom, rect.right, rect.bottom, ExposeFlags);
      rect.bottom = Region->Bottom;
      if (rect.top >= rect.bottom) return;
   }

   if (rect.top < Region->Top) { // Top
      log.trace("redraw_nonrect: Top exposure");
      if (RedrawFlags != -1) _redraw_surface(SurfaceID, List, Index, Total, rect.left, rect.top, rect.right, (rect.bottom < Region->Top) ? rect.bottom : Region->Top, RedrawFlags);
      if (ExposeFlags != -1) _expose_surface(SurfaceID, List, Index, Total, rect.left, rect.top, rect.right, (rect.bottom < Region->Top) ? rect.bottom : Region->Top, ExposeFlags);
      rect.top = Region->Top;
   }

   if (rect.left < Region->Left) { // Left
      log.trace("redraw_nonrect: Left exposure");
      if (RedrawFlags != -1) _redraw_surface(SurfaceID, List, Index, Total, rect.left, rect.top, (rect.right < Region->Left) ? rect.right : Region->Left, rect.bottom, RedrawFlags);
      if (ExposeFlags != -1) _expose_surface(SurfaceID, List, Index, Total, rect.left, rect.top, (rect.right < Region->Left) ? rect.right : Region->Left, rect.bottom, ExposeFlags);
   }
}

/*****************************************************************************

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

*****************************************************************************/

ERROR SURFACE_Draw(objSurface *Self, struct acDraw *Args)
{
   parasol::Log log;

   // If the Surface object is invisible, return immediately

   if ((!(Self->Flags & RNF_VISIBLE)) or (tlNoDrawing) or (Self->Width < 1) or (Self->Height < 1)) {
      log.trace("Not drawing (invisible or tlNoDrawing set).");
      return ERR_Okay|ERF_Notified;
   }

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
   if (!AccessMemory(msgqueue, MEM_READ, 3000, &queue)) {
      UBYTE msgbuffer[sizeof(Message) + sizeof(ActionMessage) + sizeof(struct acDraw)];
      LONG msgindex = 0;
      while (!ScanMessages(queue, &msgindex, MSGID_ACTION, msgbuffer, sizeof(msgbuffer))) {
         auto action = (ActionMessage *)(msgbuffer + sizeof(Message));

         if ((action->ActionID IS MT_DrwInvalidateRegion) and (action->ObjectID IS Self->Head.UniqueID)) {
            if (action->SendArgs IS FALSE) {
               ReleaseMemoryID(msgqueue);
               return ERR_Okay|ERF_Notified;
            }
         }
         else if ((action->ActionID IS AC_Draw) and (action->ObjectID IS Self->Head.UniqueID)) {
            if (action->SendArgs IS TRUE) {
               struct acDraw *msgdraw = (struct acDraw *)(action + 1);

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
   drwRedrawSurface(Self->Head.UniqueID, x, y, width, height, IRF_RELATIVE|IRF_IGNORE_CHILDREN);
   drwExposeSurface(Self->Head.UniqueID, x, y, width, height, EXF_REDRAW_VOLATILE);
   return ERR_Okay|ERF_Notified;
}

/*****************************************************************************

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

*****************************************************************************/

static ERROR SURFACE_Expose(objSurface *Self, struct drwExpose *Args)
{
   if (tlNoExpose) return ERR_Okay;

   // Check if other draw messages are queued for this object - if so, do not do anything until the final message is reached.

   APTR queue;
   MEMORYID msgqueue = GetResource(RES_MESSAGE_QUEUE);
   if (!AccessMemory(msgqueue, MEM_READ_WRITE, 3000, &queue)) {
      UBYTE msgbuffer[sizeof(Message) + sizeof(ActionMessage) + sizeof(struct drwExpose)];
      LONG msgindex = 0;
      while (!ScanMessages(queue, &msgindex, MSGID_ACTION, msgbuffer, sizeof(msgbuffer))) {
         auto action = (ActionMessage *)(msgbuffer + sizeof(Message));

         if ((action->ActionID IS MT_DrwExpose) and (action->ObjectID IS Self->Head.UniqueID)) {
            if (action->SendArgs) {
               struct drwExpose *msgexpose = (struct drwExpose *)(action + 1);

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

                  if ((Args->X+Args->Width < msgexpose->X) OR
                      (Args->Y+Args->Height < msgexpose->Y) OR
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
   if (Args) error = drwExposeSurface(Self->Head.UniqueID, Args->X, Args->Y, Args->Width, Args->Height, Args->Flags);
   else error = drwExposeSurface(Self->Head.UniqueID, 0, 0, Self->Width, Self->Height, 0);

   return error;
}

/*****************************************************************************

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
AccessMemory: Failed to access the internal surface list.
-END-

*****************************************************************************/

static ERROR SURFACE_InvalidateRegion(objSurface *Self, struct drwInvalidateRegion *Args)
{
   if ((!(Self->Flags & RNF_VISIBLE)) or (tlNoDrawing) or (Self->Width < 1) or (Self->Height < 1)) {
      return ERR_Okay|ERF_Notified;
   }

   // Check if other draw messages are queued for this object - if so, do not do anything until the final message is reached.

   APTR queue;
   MEMORYID msgqueue = GetResource(RES_MESSAGE_QUEUE);
   if (!AccessMemory(msgqueue, MEM_READ_WRITE, 3000, &queue)) {
      LONG msgindex = 0;
      UBYTE msgbuffer[sizeof(Message) + sizeof(ActionMessage) + sizeof(struct drwInvalidateRegion)];
      while (!ScanMessages(queue, &msgindex, MSGID_ACTION, msgbuffer, sizeof(msgbuffer))) {
         auto action = (ActionMessage *)(msgbuffer + sizeof(Message));
         if ((action->ActionID IS MT_DrwInvalidateRegion) and (action->ObjectID IS Self->Head.UniqueID)) {
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
      drwRedrawSurface(Self->Head.UniqueID, Args->X, Args->Y, Args->Width, Args->Height, IRF_RELATIVE);
      drwExposeSurface(Self->Head.UniqueID, Args->X, Args->Y, Args->Width, Args->Height, EXF_CHILDREN|EXF_REDRAW_VOLATILE_OVERLAP);
   }
   else {
      drwRedrawSurface(Self->Head.UniqueID, 0, 0, Self->Width, Self->Height, IRF_RELATIVE);
      drwExposeSurface(Self->Head.UniqueID, 0, 0, Self->Width, Self->Height, EXF_CHILDREN|EXF_REDRAW_VOLATILE_OVERLAP);
   }

   return ERR_Okay|ERF_Notified;
}

/*****************************************************************************
** Function: move_layer()
*/

static void move_layer(objSurface *Self, LONG X, LONG Y)
{
   parasol::Log log(__FUNCTION__);

   // If the coordinates are unchanged, do nothing

   if ((X IS Self->X) and (Y IS Self->Y)) return;

   if (!(Self->Head.Flags & NF_INITIALISED)) {
      Self->X = X;
      Self->Y = Y;
      return;
   }

   // This subroutine is used if the surface object is display-based

   if (!Self->ParentID) {
      objDisplay *display;
      if (!AccessObject(Self->DisplayID, 2000, &display)) {
         // Subtract the host window's LeftMargin and TopMargin as MoveToPoint() is based on the coordinates of the window frame.

         LONG left_margin = display->LeftMargin;
         LONG top_margin = display->TopMargin;
         ReleaseObject(display);

         if (!acMoveToPoint(display, X - left_margin, Y - top_margin, 0, MTF_X|MTF_Y)) {
            Self->X = X;
            Self->Y = Y;
            UpdateSurfaceList(Self);
         }
      }
      else log.warning(ERR_AccessObject);

      return;
   }

   // If the window is invisible, set the new coordinates and return immediately.

   if (!(Self->Flags & RNF_VISIBLE)) {
      Self->X = X;
      Self->Y = Y;
      UpdateSurfaceList(Self);
      return;
   }

   SurfaceControl *ctl;
   if (!(ctl = drwAccessList(ARF_READ))) return;

   LONG total = ctl->Total;
   SurfaceList list[total];
   CopyMemory((BYTE *)ctl + ctl->ArrayIndex, list, sizeof(list[0]) * ctl->Total);

   LONG vindex, index;
   if ((index = find_own_index(ctl, Self)) IS -1) {
      drwReleaseList(ARF_READ);
      return;
   }

   drwReleaseList(ARF_READ);

   struct ClipRectangle abs, old;
   old.Left   = list[index].Left;
   old.Top    = list[index].Top;
   old.Right  = list[index].Right;
   old.Bottom = list[index].Bottom;

   LONG destx = old.Left + X - Self->X;
   LONG desty = old.Top  + Y - Self->Y;

   WORD parent_index = find_parent_list(list, total, Self);

   if (Self->Flags & RNF_REGION) {
      // Drawing code for region based surface objects.  This is achieved by redrawing the parent

      log.traceBranch("MoveLayer: Using region redraw technique.");

      Self->X = X;
      Self->Y = Y;
      UpdateSurfaceCopy(Self, list);

      abs = old;

      // Merge the old and new rectangular areas into one big rectangle

      if (list[index].Left   < abs.Left)   abs.Left   = list[index].Left;
      if (list[index].Top    < abs.Top)    abs.Top    = list[index].Top;
      if (list[index].Right  > abs.Right)  abs.Right  = list[index].Right;
      if (list[index].Bottom > abs.Bottom) abs.Bottom = list[index].Bottom;

      if ((abs.Right - abs.Left) * (abs.Bottom - abs.Top) > list[index].Width * list[index].Height * 3) {
         // Split the redraw into two parts (redraw section exposed from the move, then redraw the region at its new location)

         _redraw_surface(Self->ParentID, list, parent_index, total, old.Left, old.Top, old.Left+list[index].Width, old.Top+list[index].Height, NULL);
         _redraw_surface(Self->ParentID, list, parent_index, total, list[index].Left, list[index].Top, list[index].Right, list[index].Bottom, NULL);

         _expose_surface(Self->ParentID, list, parent_index, total, old.Left, old.Top, old.Left+list[index].Width, old.Top+list[index].Height, EXF_ABSOLUTE|EXF_REDRAW_VOLATILE_OVERLAP);
         _expose_surface(Self->ParentID, list, parent_index, total, list[index].Left, list[index].Top, list[index].Right, list[index].Bottom, EXF_ABSOLUTE|EXF_REDRAW_VOLATILE_OVERLAP);
      }
      else {
         // If the region has only moved a little bit, redraw it in one shot
         _redraw_surface(Self->ParentID, list, parent_index, total, abs.Left, abs.Top, abs.Right, abs.Bottom, NULL);
         _expose_surface(Self->ParentID, list, parent_index, total, abs.Left, abs.Top, abs.Right, abs.Bottom, EXF_ABSOLUTE|EXF_REDRAW_VOLATILE_OVERLAP);
      }
   }
   else {
      // Since we do not own our graphics buffer, we need to shift the content in the buffer first, then send an
      // expose message to have the changes displayed on screen.
      //
      // This process is made more complex if there are siblings above and intersecting our surface.

      BYTE volatilegfx = check_volatile(list, index);

      UBYTE redraw;

      log.traceBranch("MoveLayer: Using simple expose technique [%s]", (volatilegfx ? "Volatile" : "Not Volatile"));

      Self->X = X;
      Self->Y = Y;
      list[index].X = X;
      list[index].Y = Y;
      UpdateSurfaceCopy(Self, list);

      if (Self->Flags & RNF_TRANSPARENT) { // Transparent surfaces are treated as volatile if they contain graphics
         if (Self->CallbackCount > 0) redraw = TRUE;
         else redraw = FALSE;
      }
      else if ((volatilegfx) and (!(Self->Flags & RNF_COMPOSITE))) redraw = TRUE;
      else if (list[index].BitmapID IS list[parent_index].BitmapID) redraw = TRUE;
      else redraw = FALSE;

      if (redraw) _redraw_surface(Self->Head.UniqueID, list, index, total, destx, desty, destx+Self->Width, desty+Self->Height, NULL);
      _expose_surface(Self->Head.UniqueID, list, index, total, 0, 0, Self->Width, Self->Height, EXF_CHILDREN|EXF_REDRAW_VOLATILE_OVERLAP);

      // Expose underlying graphics resulting from the movement

      for (vindex=index+1; list[vindex].Level > list[index].Level; vindex++);
      tlVolatileIndex = vindex;
      redraw_nonintersect(Self->ParentID, list, parent_index, total, (struct ClipRectangle *)(&list[index].Left), &old,
         (list[index].BitmapID IS list[parent_index].BitmapID) ? IRF_SINGLE_BITMAP : -1,
         EXF_CHILDREN|EXF_REDRAW_VOLATILE);
      tlVolatileIndex = 0;
   }

   refresh_pointer(Self);
}

/*****************************************************************************
** Used for PRECOPY, AFTERCOPY and compositing surfaces.
**
** Self:       The surface object being drawn to.
** Index:      The index of the surface that needs its background copied.
** DestBitmap: The Bitmap related to the surface.
** Clip:       The absolute display coordinates of the expose area.
** Stage:      Either STAGE_PRECOPY or STAGE_AFTERCOPY.
*/

static void prepare_background(objSurface *Self, SurfaceList *list, WORD Total, WORD Index, objBitmap *DestBitmap, struct ClipRectangle *clip, BYTE Stage)
{
   parasol::Log log("prepare_bkgd");

   log.traceBranch("%d Position: %dx%d,%dx%d", list[Index].SurfaceID, clip->Left, clip->Top, clip->Right-clip->Left, clip->Bottom-clip->Top);

   LONG end = Index;
   LONG master = Index;

   // Check if a root layer is set for this object.  A RootLayer determines the layer to use when opacity and
   // background graphics have precedence.  E.g. if a Window has 50% opacity, that means that all surfaces within
   // that window need to share the opacity and the background graphics of that window.

   LONG i, j;
   if ((Self) and (list[Index].SurfaceID != Self->RootID)) {
      for (j=0; j < Total; j++) {
         if (list[j].SurfaceID IS Self->RootID) {
            // Root layers are only considered when they are volatile (otherwise we want the current surface
            // object's own opacity settings to take precedence).  This ensures that objects like translucent
            // scrollbars can take priority if the parent is not translucent.
            //
            // If a custom root layer has been specified, then we are forced into using it as the end index.

            if (Self->InheritedRoot IS FALSE) end = j; // A custom root layer has been specified by the user
            else if (list[j].Flags & RNF_VOLATILE) end = j; // The root layer is volatile and must be used
            break;
         }
      }
   }

   end = FindBitmapOwner(list, end);

   // Find the parent that owns this surface (we will use this as the starting point for our copy operation).
   // Everything that gets in the way between the parent and the location of our surface is what will be copied across.

   if (!list[end].ParentID) { LOGRETURN(); return; }
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
      if (list[i].Flags & (RNF_REGION|RNF_TRANSPARENT|RNF_CURSOR)) continue; // Ignore regions

      struct ClipRectangle expose = {
         .Left   = clip->Left, // Take a copy of the expose coordinates
         .Right  = clip->Right,
         .Bottom = clip->Bottom,
         .Top    = clip->Top
      };

      // Check the visibility of this layer and its parents

      if (restrict_region_to_parents(list, i, &expose, TRUE) <= 0) continue;

      LONG opaque;
      if (Stage IS STAGE_AFTERCOPY) {
         if (list[Index].RootID != list[Index].SurfaceID) opaque = list[Index].Opacity;
         else opaque = list[end].Opacity;
      }
      else opaque = 255;

      BYTE pervasive;
      if ((list[Index].Flags & RNF_PERVASIVE_COPY) and (Stage IS STAGE_AFTERCOPY)) pervasive = TRUE;
      else pervasive = FALSE;

      objBitmap *bitmap;
      ERROR error;
      if (!(error = AccessObject(list[i].BitmapID, 2000, &bitmap))) {
         copy_bkgd(list, i, end, master, expose.Left, expose.Top, expose.Right, expose.Bottom, DestBitmap, bitmap, opaque, pervasive);
         ReleaseObject(bitmap);
      }
      else {
         log.warning("prepare_bkgd: %d failed to access bitmap #%d of surface #%d (error %d).", list[Index].SurfaceID, list[i].BitmapID, list[i].SurfaceID, error);
         break;
      }
   }
}

/*****************************************************************************
** Coordinates are absolute.
*/

static void copy_bkgd(SurfaceList *list, WORD Index, WORD End, WORD Master, WORD Left, WORD Top, WORD Right, WORD Bottom,
   objBitmap *DestBitmap, objBitmap *SrcBitmap, WORD Opacity, BYTE Pervasive)
{
   parasol::Log log(__FUNCTION__);

   // Scan for overlapping parent/sibling regions and avoid them

   for (LONG i=Index+1; (i < End) and (list[i].Level > 1); i++) {
      if (list[i].Flags & (RNF_REGION|RNF_CURSOR|RNF_COMPOSITE)); // Ignore regions
      else if (!(list[i].Flags & RNF_VISIBLE)); // Skip hidden surfaces and their content
      else if (list[i].Flags & RNF_TRANSPARENT) continue; // Invisibles may contain important regions we have to block
      else if ((Pervasive) and (list[i].Level > list[Index].Level)); // If the copy is pervasive then all children must be ignored (so that we can copy translucent graphics over them)
      else {
         struct ClipRectangle listclip = {
            .Left   = list[i].Left,
            .Right  = list[i].Right,
            .Bottom = list[i].Bottom,
            .Top    = list[i].Top
         };

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

   struct ClipRectangle expose = {
      .Left   = Left,
      .Right  = Right,
      .Bottom = Bottom,
      .Top    = Top
   };
   if (restrict_region_to_parents(list, Index, &expose, FALSE) IS -1) return;

   log.traceBranch("[%d] Pos: %dx%d,%dx%d Bitmap: %d, Index: %d/%d", list[Index].SurfaceID, expose.Left, expose.Top, expose.Right - expose.Left, Bottom - expose.Top, list[Index].BitmapID, Index, End);

   // The region is not obscured, so perform the redraw

   LONG owner = FindBitmapOwner(list, Index);

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
