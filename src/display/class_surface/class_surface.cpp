/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
Surface: Manages the display and positioning of 2-Dimensional rendered graphics.

The Surface class is used to manage the positioning, drawing and interaction with layered display interfaces.  It is
supplemented by many classes in the GUI category that either draw graphics to or manage user interactivity with
the surface objects that you create.

Graphically, each surface object represents a rectangular area of space on the display.  The biggest surface object,
often referred to as the 'Master' represents the screen space, and is as big as the display itself, if not larger.
Surface objects can be placed inside the master and are typically known as children.  You can place more surface
objects inside of these children, causing a hierarchy to develop that consists of many objects that are all working
together on the display.

In order to actually draw graphics onto the display, classes that have been specifically created for drawing graphics
must be used to create a meaningful interface.  Classes such as Box, Image, Text and Gradient help to create an
assemblage of imagery that has meaning to the user. The interface is then enhanced through the use of UI functionality
that enables user feedback such as mouse and keyboard I/O.  With a little effort and imagination, a customised
interface can thus be assembled and presented to the user without much difficulty on the part of the developer.

While this is a simple concept, the Surface class is its foundation and forms one of the largest and most
sophisticated class in our system. It provides a great deal of functionality which cannot be summarised in this
introduction, but you will find a lot of technical detail on each individual field in this manual.  It is also
recommended that you refer to the demo scripts that come with the core distribution if you require a real-world view
of how the class is typically used.
-END-

Backing Stores
--------------
The Surface class uses the "backing store" technique for preserving the graphics of rendered areas.  This is an
absolute requirement when drawing regions at points where processes intersect in the layer hierarchy.  Not only does it
speed up exposes, but it is the only reasonable way to get masking and translucency effects working correctly.  Note
that backing store graphics are stored in public Bitmaps so that processes can share graphics information without
having to communicate with each other directly.

*****************************************************************************/

#undef __xwindows__
#include "../defs.h"

static ERROR SET_Opacity(extSurface *, DOUBLE);
static ERROR SET_XOffset(extSurface *, Variable *);
static ERROR SET_YOffset(extSurface *, Variable *);

#define MOVE_VERTICAL   0x0001
#define MOVE_HORIZONTAL 0x0002

static ERROR consume_input_events(const InputEvent *, LONG);
static void draw_region(extSurface *, extSurface *, extBitmap *);
static ERROR redraw_timer(extSurface *, LARGE, LARGE);

/*****************************************************************************
** This call is used to refresh the pointer image when at least one layer has been rearranged.  The timer is used to
** delay the refresh - useful if multiple surfaces are being rearranged when we only need to do the refresh once.
** The delay also prevents clashes with read/write access to the surface list.
*/

static ERROR refresh_pointer_timer(OBJECTPTR Task, LARGE Elapsed, LARGE CurrentTime)
{
   objPointer *pointer;
   if ((pointer = gfxAccessPointer())) {
      Action(AC_Refresh, pointer, NULL);
      ReleaseObject(pointer);
   }
   glRefreshPointerTimer = 0;
   return ERR_Terminate; // Timer is only called once
}

void refresh_pointer(extSurface *Self)
{
   if (!glRefreshPointerTimer) {
      parasol::SwitchContext context(glModule);
      auto call = make_function_stdc(refresh_pointer_timer);
      SubscribeTimer(0.02, &call, &glRefreshPointerTimer);
   }
}

//****************************************************************************

static ERROR access_video(OBJECTID DisplayID, objDisplay **Display, objBitmap **Bitmap)
{
   if (!AccessObjectID(DisplayID, 5000, Display)) {
      APTR winhandle;

      if (!Display[0]->getPtr(FID_WindowHandle, &winhandle)) {
         #ifdef _WIN32
            Display[0]->Bitmap->set(FID_Handle, winGetDC(winhandle));
         #else
            Display[0]->Bitmap->set(FID_Handle, winhandle);
         #endif
      }

      if (Bitmap) *Bitmap = Display[0]->Bitmap;
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

//****************************************************************************

static void release_video(objDisplay *Display)
{
   #ifdef _WIN32
      APTR surface;
      Display->Bitmap->getPtr(FID_Handle, &surface);

      APTR winhandle;
      if (!Display->getPtr(FID_WindowHandle, &winhandle)) {
         winReleaseDC(winhandle, surface);
      }

      Display->Bitmap->set(FID_Handle, (APTR)NULL);
   #endif

   acFlush(Display);

   ReleaseObject(Display);
}

/*****************************************************************************
** Used By:  MoveToBack(), move_layer()
**
** This is the best way to figure out if a surface object or its children causes it to be volatile.  Use this function
** if you don't want to do any deep scanning to determine who is volatile or not.
**
** Volatile flags are PRECOPY, AFTER_COPY and CURSOR.
**
** NOTE: Surfaces marked as COMPOSITE or TRANSPARENT are not considered volatile as they do not require redraws.  It's
** up to the caller to make a decision as to whether COMPOSITE's are volatile or not.
*/

static bool check_volatile(SurfaceList *list, LONG index)
{
   if (list[index].Flags & RNF_VOLATILE) return true;

   // If there are children with custom root layers or are volatile, that will force volatility

   LONG j;
   for (LONG i=index+1; list[i].Level > list[index].Level; i++) {
      if (!(list[i].Flags & RNF_VISIBLE)) {
         j = list[i].Level;
         while (list[i+1].Level > j) i++;
         continue;
      }

      if (list[i].Flags & RNF_VOLATILE) {
         // If a child surface is marked as volatile and is a member of our bitmap space, then effectively all members of the bitmap are volatile.

         if (list[index].BitmapID IS list[i].BitmapID) return true;

         // If this is a custom root layer, check if it refers to a surface that is going to affect our own volatility.

         if (list[i].RootID != list[i].SurfaceID) {
            for (j=i; j > index; j--) {
               if (list[i].RootID IS list[j].SurfaceID) break;
            }

            if (j <= index) return true; // Custom root of a child is outside of bounds - that makes us volatile
         }
      }
   }

   return false;
}

//****************************************************************************

static void expose_buffer(SurfaceList *list, LONG Total, LONG Index, LONG ScanIndex, LONG Left, LONG Top,
                   LONG Right, LONG Bottom, OBJECTID DisplayID, extBitmap *Bitmap)
{
   parasol::Log log(__FUNCTION__);

   // Scan for overlapping parent/sibling regions and avoid them

   LONG i, j;
   for (i=ScanIndex+1; (i < Total) and (list[i].Level > 1); i++) {
      if (!(list[i].Flags & RNF_VISIBLE)) { // Skip past non-visible areas and their content
         j = list[i].Level;
         while ((i+1 < Total) and (list[i+1].Level > j)) i++;
         continue;
      }
      else if (list[i].Flags & RNF_CURSOR); // Skip the cursor
      else {
         ClipRectangle listclip = {
            .Left   = list[i].Left,
            .Top    = list[i].Top,
            .Right  = list[i].Right,
            .Bottom = list[i].Bottom
         };

         if (restrict_region_to_parents(list, i, &listclip, FALSE) IS -1); // Skip
         else if ((listclip.Left < Right) and (listclip.Top < Bottom) and (listclip.Right > Left) and (listclip.Bottom > Top)) {
            if (list[i].BitmapID IS list[Index].BitmapID) continue; // Ignore any children that overlap & form part of our bitmap space.  Children that do not overlap are skipped.

            if (listclip.Left <= Left) listclip.Left = Left;
            else expose_buffer(list, Total, Index, ScanIndex, Left, Top, listclip.Left, Bottom, DisplayID, Bitmap); // left

            if (listclip.Right >= Right) listclip.Right = Right;
            else expose_buffer(list, Total, Index, ScanIndex, listclip.Right, Top, Right, Bottom, DisplayID, Bitmap); // right

            if (listclip.Top <= Top) listclip.Top = Top;
            else expose_buffer(list, Total, Index, ScanIndex, listclip.Left, Top, listclip.Right, listclip.Top, DisplayID, Bitmap); // top

            if (listclip.Bottom < Bottom) expose_buffer(list, Total, Index, ScanIndex, listclip.Left, listclip.Bottom, listclip.Right, Bottom, DisplayID, Bitmap); // bottom

            if (list[i].Flags & RNF_TRANSPARENT) {
               // In the case of invisible regions, we will have split the expose process as normal.  However,
               // we also need to look deeper into the invisible region to discover if there is more that
               // we can draw, depending on the content of the invisible region.

               listclip.Left   = list[i].Left;
               listclip.Top    = list[i].Top;
               listclip.Right  = list[i].Right;
               listclip.Bottom = list[i].Bottom;

               if (Left > listclip.Left)     listclip.Left   = Left;
               if (Top > listclip.Top)       listclip.Top    = Top;
               if (Right < listclip.Right)   listclip.Right  = Right;
               if (Bottom < listclip.Bottom) listclip.Bottom = Bottom;

               expose_buffer(list, Total, Index, i, listclip.Left, listclip.Top, listclip.Right, listclip.Bottom, DisplayID, Bitmap);
            }

            return;
         }
      }

      // Skip past any children of the non-overlapping object.  This ensures that we only look at immediate parents and siblings that are in our way.

      j = i + 1;
      while ((j < Total) and (list[j].Level > list[i].Level)) j++;
      i = j - 1;
   }

   log.traceBranch("[%d] %dx%d,%dx%d Bmp: %d, Idx: %d/%d", list[Index].SurfaceID, Left, Top, Right - Left, Bottom - Top, list[Index].BitmapID, Index, ScanIndex);

   // The region is not obscured, so perform the redraw

   LONG owner = find_bitmap_owner(list, Index);

   // Turn off offsets and set the clipping to match the source bitmap exactly (i.e. nothing fancy happening here).
   // The real clipping occurs in the display clip.

   Bitmap->XOffset = 0;
   Bitmap->YOffset = 0;

   Bitmap->Clip.Left   = list[Index].Left - list[owner].Left;
   Bitmap->Clip.Top    = list[Index].Top - list[owner].Top;
   Bitmap->Clip.Right  = list[Index].Right - list[owner].Left;
   Bitmap->Clip.Bottom = list[Index].Bottom - list[owner].Top;
   if (Bitmap->Clip.Right  > Bitmap->Width)  Bitmap->Clip.Right  = Bitmap->Width;
   if (Bitmap->Clip.Bottom > Bitmap->Height) Bitmap->Clip.Bottom = Bitmap->Height;

   // Set the clipping so that we are only drawing to the display area that has been exposed

   LONG iscr = Index;
   while ((iscr > 0) and (list[iscr].ParentID)) iscr--; // Find the top-level display entry

   // If COMPOSITE is in use, this means we have to do compositing on the fly.  This involves copying the background
   // graphics into a temporary buffer, then blitting the composite buffer to the display.

   // Note: On hosted displays in Windows or Linux, compositing is handled by the host's graphics system if the surface
   // is at the root level (no ParentID).

   LONG sx, sy;
   if ((list[Index].Flags & RNF_COMPOSITE) and
       ((list[Index].ParentID) or (list[Index].Flags & RNF_CURSOR))) {
      ClipRectangle clip;
      if (glComposite) {
         if (glComposite->BitsPerPixel != list[Index].BitsPerPixel) {
            acFree(glComposite);
            glComposite = NULL;
         }
         else {
            if ((glComposite->Width < list[Index].Width) or (glComposite->Height < list[Index].Height)) {
               acResize(glComposite, (list[Index].Width > glComposite->Width) ? list[Index].Width : glComposite->Width,
                                     (list[Index].Height > glComposite->Height) ? list[Index].Height : glComposite->Height,
                                     0);
            }
         }
      }

      if (!glComposite) {
         if (!(glComposite = extBitmap::create::untracked(fl::Width(list[Index].Width), fl::Height(list[Index].Height)))) {
            return;
         }

         SetOwner(glComposite, glModule);
      }

      // Build the background in our buffer

      clip.Left   = Left;
      clip.Top    = Top;
      clip.Right  = Right;
      clip.Bottom = Bottom;
      prepare_background(NULL, list, Total, Index, glComposite, &clip, STAGE_COMPOSITE);

      // Blend the surface's graphics into the composited buffer
      // NOTE: THE FOLLOWING IS NOT OPTIMISED WITH RESPECT TO CLIPPING

      gfxCopyArea(Bitmap, glComposite, BAF_BLEND, 0, 0, list[Index].Width, list[Index].Height, 0, 0);

      Bitmap = glComposite;
      sx = 0;  // Always zero as composites own their bitmap
      sy = 0;
   }
   else {
      sx = list[Index].Left - list[owner].Left;
      sy = list[Index].Top - list[owner].Top;
   }

   objDisplay *display;
   objBitmap *video_bmp;
   if (!access_video(DisplayID, &display, &video_bmp)) {
      video_bmp->XOffset = 0;
      video_bmp->YOffset = 0;

      video_bmp->Clip.Left   = Left - list[iscr].Left; // Ensure that the coords are relative to the display bitmap (important for Windows, X11)
      video_bmp->Clip.Top    = Top - list[iscr].Top;
      video_bmp->Clip.Right  = Right - list[iscr].Left;
      video_bmp->Clip.Bottom = Bottom - list[iscr].Top;
      if (video_bmp->Clip.Left < 0) video_bmp->Clip.Left = 0;
      if (video_bmp->Clip.Top  < 0) video_bmp->Clip.Top  = 0;
      if (video_bmp->Clip.Right  > video_bmp->Width) video_bmp->Clip.Right   = video_bmp->Width;
      if (video_bmp->Clip.Bottom > video_bmp->Height) video_bmp->Clip.Bottom = video_bmp->Height;

      gfxUpdateDisplay(display, Bitmap, sx, sy, // Src X/Y (bitmap relative)
         list[Index].Width, list[Index].Height,
         list[Index].Left - list[iscr].Left, list[Index].Top - list[iscr].Top); // Dest X/Y (absolute display position)

      release_video(display);
   }
   else log.warning("Unable to access display #%d.", DisplayID);
}

/*****************************************************************************
** Used by MoveToFront()
**
** This function will expose areas that are uncovered when a surface changes its position in the surface tree (e.g.
** moving towards the front).
**
** This function is only interested in siblings of the surface that we've moved.  Also, any intersecting surfaces need
** to share the same bitmap surface.
**
** All coordinates are expressed in absolute format.
*/

static void invalidate_overlap(extSurface *Self, SurfaceList *list, LONG Total, LONG OldIndex, LONG Index,
   LONG Left, LONG Top, LONG Right, LONG Bottom, objBitmap *Bitmap)
{
   parasol::Log log(__FUNCTION__);
   LONG j;

   log.traceBranch("%dx%d %dx%d, Between %d to %d", Left, Top, Right-Left, Bottom-Top, OldIndex, Index);

   if ((list[Index].Flags & RNF_TRANSPARENT) or (!(list[Index].Flags & RNF_VISIBLE))) {
      return;
   }

   for (auto i=OldIndex; i < Index; i++) {
      // A redraw is required for:
      //   Any volatile regions that were in front of our surface prior to the move-to-front (by moving to the front, their background has been changed).
      //   Areas of our surface that were obscured by surfaces that also shared our bitmap space.

      if (!(list[i].Flags & RNF_VISIBLE)) goto skipcontent;
      if (list[i].Flags & RNF_TRANSPARENT) continue;

      if (list[i].BitmapID != list[Index].BitmapID) {
         // We're not using the deep scanning technique, so use check_volatile() to thoroughly determine if the surface is volatile or not.

         if (check_volatile(list, i)) {
            // The surface is volatile and on a different bitmap - it will have to be redrawn
            // because its background has changed.  It will not have to be exposed because our
            // surface is sitting on top of it.

            _redraw_surface(list[i].SurfaceID, list, i, Total, Left, Top, Right, Bottom, NULL);
         }
         else goto skipcontent;
      }

      if ((list[i].Left < Right) and (list[i].Top < Bottom) and (list[i].Right > Left) and (list[i].Bottom > Top)) {
         // Intersecting surface discovered.  What we do now is keep scanning for other overlapping siblings to restrict our
         // exposure space (so that we don't repeat expose drawing for overlapping areas).  Then we call RedrawSurface() to draw the exposed area.

         LONG listx      = list[i].Left;
         LONG listy      = list[i].Top;
         LONG listright  = list[i].Right;
         LONG listbottom = list[i].Bottom;

         if (Left > listx)        listx      = Left;
         if (Top > listy)         listy      = Top;
         if (Bottom < listbottom) listbottom = Bottom;
         if (Right < listright)   listright  = Right;

         _redraw_surface(Self->UID, list, i, Total, listx, listy, listright, listbottom, NULL);
      }

skipcontent:
      // Skip past any children of the overlapping object

      for (j=i+1; list[j].Level > list[i].Level; j++);
      i = j - 1;
   }
}

//****************************************************************************

static BYTE check_surface_list(void)
{
   parasol::Log log(__FUNCTION__);

   log.traceBranch("Validating the surface list...");

   SurfaceControl *ctl;
   if ((ctl = gfxAccessList(ARF_WRITE))) {
      auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
      BYTE bad = FALSE;
      for (LONG i=0; i < ctl->Total; i++) {
         if ((CheckObjectExists(list[i].SurfaceID) != ERR_Okay)) {
            log.trace("Surface %d, index %d is dead.", list[i].SurfaceID, i);
            untrack_layer(list[i].SurfaceID);
            bad = TRUE;
            i--; // stay at the same index level
         }
      }

      gfxReleaseList(ARF_WRITE);
      return bad;
   }
   else return FALSE;
}

//****************************************************************************
// Handler for the display being resized.

static void display_resized(OBJECTID DisplayID, LONG X, LONG Y, LONG Width, LONG Height)
{
   OBJECTID surface_id = GetOwnerID(DisplayID);
   extSurface *surface;
   if (!AccessObjectID(surface_id, 4000, &surface)) {
      if (surface->ClassID IS ID_SURFACE) {
         if ((X != surface->X) or (Y != surface->Y)) {
            surface->X = X;
            surface->Y = Y;
            UpdateSurfaceList(surface);
         }

         if ((surface->Width != Width) or (surface->Height != Height)) {
            acResize(surface, Width, Height, 0);
         }
      }
      ReleaseObject(surface);
   }
}

//********************************************************************************************************************

static void notify_free_parent(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, APTR Void)
{
   parasol::Log log(__FUNCTION__);
   auto Self = (extSurface *)CurrentContext();

   // Free ourselves in advance if our parent is in the process of being killed.  This causes a chain reaction
   // that results in a clean deallocation of the surface hierarchy.

   Self->Flags &= ~RNF_VISIBLE;
   UpdateSurfaceField(Self, &SurfaceList::Flags, Self->Flags);
   if (Self->defined(NF::INTEGRAL)) QueueAction(AC_Free, Self->UID); // If the object is a child of something, give the parent object time to do the deallocation itself
   else acFree(Self);
}

static void notify_free_callback(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, APTR Void)
{
   parasol::Log log(__FUNCTION__);
   auto Self = (extSurface *)CurrentContext();

   for (LONG i=0; i < Self->CallbackCount; i++) {
      if (Self->Callback[i].Function.Type IS CALL_SCRIPT) {
         if (Self->Callback[i].Function.Script.Script->UID IS Object->UID) {
            Self->Callback[i].Function.Type = CALL_NONE;

            LONG j;
            for (j=i; j < Self->CallbackCount-1; j++) { // Shorten the array
               Self->Callback[j] = Self->Callback[j+1];
            }
            i--;
            Self->CallbackCount--;
         }
      }
   }
}

static void notify_draw_display(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, struct acDraw *Args)
{
   parasol::Log log(__FUNCTION__);
   auto Self = (extSurface *)CurrentContext();

   if (Self->collecting()) return;

   // Hosts will sometimes call Draw to indicate that the display has been exposed.

   log.traceBranch("Display exposure received - redrawing display.");

   if (Args) {
      struct drwExpose expose = { Args->X, Args->Y, Args->Width, Args->Height, EXF_CHILDREN };
      Action(MT_DrwExpose, Self, &expose);
   }
   else {
      struct drwExpose expose = { 0, 0, 20000, 20000, EXF_CHILDREN };
      Action(MT_DrwExpose, Self, &expose);
   }
}

static void notify_redimension_parent(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, struct acRedimension *Args)
{
   parasol::Log log(__FUNCTION__);
   auto Self = (extSurface *)CurrentContext();

   if (Self->Document) return;
   if (Self->collecting()) return;

   log.traceBranch("Redimension notification from parent #%d, currently %dx%d,%dx%d.", Self->ParentID, Self->X, Self->Y, Self->Width, Self->Height);

   // Get the width and height of our parent surface

   DOUBLE parentwidth, parentheight, width, height, x, y;

   if (Self->ParentID) {
      SurfaceControl *ctl;
      if ((ctl = gfxAccessList(ARF_READ))) {
         auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
         LONG i;
         for (i=0; (i < ctl->Total) and (list[i].SurfaceID != Self->ParentID); i++);
         if (i >= ctl->Total) {
            gfxReleaseList(ARF_READ);
            log.warning(ERR_Search);
            return;
         }
         parentwidth  = list[i].Width;
         parentheight = list[i].Height;
         gfxReleaseList(ARF_READ);
      }
      else { log.warning(ERR_AccessMemory); return; }
   }
   else {
      DISPLAYINFO *display;
      if (!gfxGetDisplayInfo(0, &display)) {
         parentwidth  = display->Width;
         parentheight = display->Height;
      }
      else return;
   }

   // Convert relative offsets to their fixed equivalent

   if (Self->Dimensions & DMF_RELATIVE_X_OFFSET) Self->XOffset = (parentwidth * Self->XOffsetPercent) * 0.01;
   if (Self->Dimensions & DMF_RELATIVE_Y_OFFSET) Self->YOffset = (parentheight * Self->YOffsetPercent) * 0.01;

   // Calculate absolute width and height values

   if (Self->Dimensions & DMF_RELATIVE_WIDTH)   width = parentwidth * Self->WidthPercent * 0.01;
   else if (Self->Dimensions & DMF_FIXED_WIDTH) width = Self->Width;
   else if (Self->Dimensions & DMF_X_OFFSET) {
      if (Self->Dimensions & DMF_FIXED_X) {
         width = parentwidth - Self->X - Self->XOffset;
      }
      else if (Self->Dimensions & DMF_RELATIVE_X) {
         width = parentwidth - (parentwidth * Self->XPercent * 0.01) - Self->XOffset;
      }
      else width = parentwidth - Self->XOffset;
   }
   else width = Self->Width;

   if (Self->Dimensions & DMF_RELATIVE_HEIGHT)   height = parentheight * Self->HeightPercent * 0.01;
   else if (Self->Dimensions & DMF_FIXED_HEIGHT) height = Self->Height;
   else if (Self->Dimensions & DMF_Y_OFFSET) {
      if (Self->Dimensions & DMF_FIXED_Y) {
         height = parentheight - Self->Y - Self->YOffset;
      }
      else if (Self->Dimensions & DMF_RELATIVE_Y) {
         height = parentheight - (parentheight * Self->YPercent * 0.01) - Self->YOffset;
      }
      else height = parentheight - Self->YOffset;
   }
   else height = Self->Height;

   // Calculate new coordinates

   if (Self->Dimensions & DMF_RELATIVE_X) x = (parentwidth * Self->XPercent * 0.01);
   else if (Self->Dimensions & DMF_X_OFFSET) x = parentwidth - Self->XOffset - width;
   else x = Self->X;

   if (Self->Dimensions & DMF_RELATIVE_Y) y = (parentheight * Self->YPercent * 0.01);
   else if (Self->Dimensions & DMF_Y_OFFSET) y = parentheight - Self->YOffset - height;
   else y = Self->Y;

   // Alignment adjustments

   if (Self->Align & ALIGN_LEFT) x = 0;
   else if (Self->Align & ALIGN_RIGHT) x = parentwidth - width;
   else if (Self->Align & ALIGN_HORIZONTAL) x = (parentwidth - width) * 0.5;

   if (Self->Align & ALIGN_TOP) y = 0;
   else if (Self->Align & ALIGN_BOTTOM) y = parentheight - height;
   else if (Self->Align & ALIGN_VERTICAL) y = (parentheight - height) * 0.5;

   if (width > Self->MaxWidth) {
      log.trace("Calculated width of %.0f exceeds max limit of %d", width, Self->MaxWidth);
      width = Self->MaxWidth;
   }

   if (height > Self->MaxHeight) {
      log.trace("Calculated height of %.0f exceeds max limit of %d", height, Self->MaxHeight);
      height = Self->MaxHeight;
   }

   // Perform the resize

   if ((Self->X != x) or (Self->Y != y) or (Self->Width != width) or (Self->Height != height) or (Args->Depth)) {
      acRedimension(Self, x, y, 0, width, height, Args->Depth);
   }
}

/*****************************************************************************
-ACTION-
Activate: Shows a surface object on the display.
-END-
*****************************************************************************/

static ERROR SURFACE_Activate(extSurface *Self, APTR Void)
{
   if (!Self->ParentID) acShow(Self);
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
AddCallback: Inserts a function hook into the drawing process of a surface object.

The AddCallback() method provides a gateway for custom functions to draw directly to a surface.  Whenever a surface
object performs a redraw event, all functions inserted by this method will be called in their original subscription
order with a direct reference to the Surface's target bitmap.  The C/C++ prototype is
`Function(APTR Context, *Surface, *Bitmap)`.

The Fluid prototype is `function draw(Surface, Bitmap)`

The subscriber can draw to the bitmap surface as it would with any freshly allocated bitmap object (refer to the
@Bitmap class).  To get the width and height of the available drawing space, please read the Width and
Height fields from the Surface object.  If writing to the bitmap directly, please observe the bitmap's clipping
region and the XOffset and YOffset values.

-INPUT-
ptr(func) Callback: Pointer to the callback routine or NULL to remove callbacks for the given Object.

-ERRORS-
Okay
NullArgs
ExecViolation: The call was not made from the process that owns the object.
AllocMemory
-END-

*****************************************************************************/

static ERROR SURFACE_AddCallback(extSurface *Self, struct drwAddCallback *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   OBJECTPTR context = GetParentContext();
   OBJECTPTR call_context = NULL;
   if (Args->Callback->Type IS CALL_STDC) call_context = (OBJECTPTR)Args->Callback->StdC.Context;
   else if (Args->Callback->Type IS CALL_SCRIPT) call_context = context; // Scripts use runtime ID resolution...

   if (context->UID < 0) {
      log.warning("Public objects may not draw directly to surfaces.");
      return ERR_Failed;
   }

   log.msg("Context: %d, Callback Context: %d, Routine: %p (Count: %d)", context->UID, call_context ? call_context->UID : 0, Args->Callback->StdC.Routine, Self->CallbackCount);

   if (call_context) context = call_context;

   if (Self->Callback) {
      // Check if the subscription is already on the list for our surface context.

      LONG i;
      for (i=0; i < Self->CallbackCount; i++) {
         if (Self->Callback[i].Object IS context) {
            if ((Self->Callback[i].Function.Type IS CALL_STDC) and (Args->Callback->Type IS CALL_STDC)) {
               if (Self->Callback[i].Function.StdC.Routine IS Args->Callback->StdC.Routine) break;
            }
            else if ((Self->Callback[i].Function.Type IS CALL_SCRIPT) and (Args->Callback->Type IS CALL_SCRIPT)) {
               if (Self->Callback[i].Function.Script.ProcedureID IS Args->Callback->Script.ProcedureID) break;
            }
         }
      }

      if (i < Self->CallbackCount) {
         log.trace("Moving existing subscription to foreground.");

         while (i < Self->CallbackCount-1) {
            Self->Callback[i] = Self->Callback[i+1];
            i++;
         }
         Self->Callback[i].Object   = context;
         Self->Callback[i].Function = *Args->Callback;
         return ERR_Okay;
      }
      else if (Self->CallbackCount < Self->CallbackSize) {
         // Add the callback routine to the cache

         Self->Callback[Self->CallbackCount].Object   = context;
         Self->Callback[Self->CallbackCount].Function = *Args->Callback;
         Self->CallbackCount++;
      }
      else if (Self->CallbackCount < 255) {
         log.extmsg("Expanding draw subscription array.");

         LONG new_size = Self->CallbackSize + 10;
         if (new_size > 255) new_size = 255;
         SurfaceCallback *scb;
         if (!AllocMemory(sizeof(SurfaceCallback) * new_size, MEM_DATA|MEM_NO_CLEAR, &scb)) {
            CopyMemory(Self->Callback, scb, sizeof(SurfaceCallback) * Self->CallbackCount);

            scb[Self->CallbackCount].Object   = context;
            scb[Self->CallbackCount].Function = *Args->Callback;
            Self->CallbackCount++;
            Self->CallbackSize = new_size;

            if (Self->Callback != Self->CallbackCache) FreeResource(Self->Callback);
            Self->Callback = scb;
         }
         else return ERR_AllocMemory;
      }
      else return ERR_ArrayFull;
   }
   else {
      Self->Callback = Self->CallbackCache;
      Self->CallbackCount = 1;
      Self->CallbackSize = ARRAYSIZE(Self->CallbackCache);
      Self->Callback[0].Object = context;
      Self->Callback[0].Function = *Args->Callback;
   }

   if (Args->Callback->Type IS CALL_SCRIPT) {
      auto callback = make_function_stdc(notify_free_callback);
      SubscribeAction(Args->Callback->Script.Script, AC_Free, &callback);
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Disable: Disables a surface object.
-END-
*****************************************************************************/

static ERROR SURFACE_Disable(extSurface *Self, APTR Void)
{
   Self->Flags |= RNF_DISABLED;
   UpdateSurfaceField(Self, &SurfaceList::Flags, Self->Flags);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Enable: Enables a disabled surface object.
-END-
*****************************************************************************/

static ERROR SURFACE_Enable(extSurface *Self, APTR Void)
{
   Self->Flags &= ~RNF_DISABLED;
   UpdateSurfaceField(Self, &SurfaceList::Flags, Self->Flags);
   return ERR_Okay;
}

/*****************************************************************************
** Event: task.removed
*/

static void event_task_removed(OBJECTID *SurfaceID, APTR Info, LONG InfoSize)
{
   parasol::Log log;

   log.function("Dead task detected - checking surfaces.");

   // Validate the surface list and then redraw the display.

   if (check_surface_list()) {
      gfxRedrawSurface(*SurfaceID, 0, 0, 4096, 4096, RNF_TOTAL_REDRAW);
      gfxExposeSurface(*SurfaceID, 0, 0, 4096, 4096, EXF_CHILDREN);
   }
}

/*****************************************************************************
** Event: user.login
*/

static void event_user_login(extSurface *Self, APTR Info, LONG InfoSize)
{
   parasol::Log log;

   log.function("User login detected - resetting screen mode.");

   objConfig::create config = { fl::Path("user:config/display.cfg") };

   if (config.ok()) {
      OBJECTPTR object;
      CSTRING str;

      DOUBLE refreshrate = -1.0;
      LONG depth         = 32;
      DOUBLE gammared    = 1.0;
      DOUBLE gammagreen  = 1.0;
      DOUBLE gammablue   = 1.0;
      LONG width         = Self->Width;
      LONG height        = Self->Height;

      cfgRead(*config, "DISPLAY", "Width", &width);
      cfgRead(*config, "DISPLAY", "Height", &height);
      cfgRead(*config, "DISPLAY", "Depth", &depth);
      cfgRead(*config, "DISPLAY", "RefreshRate", &refreshrate);
      cfgRead(*config, "DISPLAY", "GammaRed", &gammared);
      cfgRead(*config, "DISPLAY", "GammaGreen", &gammagreen);
      cfgRead(*config, "DISPLAY", "GammaBlue", &gammablue);

      if (!cfgReadValue(*config, "DISPLAY", "DPMS", &str)) {
         if (!AccessObjectID(Self->DisplayID, 3000, &object)) {
            object->set(FID_DPMS, str);
            ReleaseObject(object);
         }
      }

      if (width < 640) width = 640;
      if (height < 480) height = 480;

      struct gfxSetDisplay setdisplay = {
         .X            = 0,
         .Y            = 0,
         .Width        = width,
         .Height       = height,
         .InsideWidth  = setdisplay.Width,
         .InsideHeight = setdisplay.Height,
         .BitsPerPixel = depth,
         .RefreshRate  = refreshrate,
         .Flags        = 0
      };
      Action(MT_DrwSetDisplay, Self, &setdisplay);

      struct gfxSetGamma gamma = {
         .Red   = gammared,
         .Green = gammagreen,
         .Blue  = gammablue,
         .Flags = GMF_SAVE
      };
      ActionMsg(MT_GfxSetGamma, Self->DisplayID, &gamma);
   }
}

/*****************************************************************************
-ACTION-
Focus: Changes the primary user focus to the surface object.
-END-
*****************************************************************************/

static LARGE glLastFocusTime = 0;

static ERROR SURFACE_Focus(extSurface *Self, APTR Void)
{
   parasol::Log log;

   if (Self->Flags & RNF_DISABLED) return ERR_Okay|ERF_Notified;

   Message *msg;
   if ((msg = GetActionMsg())) {
      // This is a message - in which case it could have been delayed and thus superseded by a more recent message.

      if (msg->Time < glLastFocusTime) {
         FOCUSMSG("Ignoring superseded focus message.");
         return ERR_Okay|ERF_Notified;
      }
   }

   if (Self->Flags & RNF_IGNORE_FOCUS) {
      FOCUSMSG("Focus propagated to parent (IGNORE_FOCUS flag set).");
      acFocus(Self->ParentID);
      glLastFocusTime = PreciseTime();
      return ERR_Okay|ERF_Notified;
   }

   if (Self->Flags & RNF_NO_FOCUS) {
      FOCUSMSG("Focus cancelled (NO_FOCUS flag set).");
      glLastFocusTime = PreciseTime();
      return ERR_Okay|ERF_Notified;
   }

   FOCUSMSG("Focussing...  HasFocus: %c", (Self->Flags & RNF_HAS_FOCUS) ? 'Y' : 'N');

   if (auto modal = gfxGetModalSurface()) {
      if (modal != Self->UID) {
         ERROR error;
         error = gfxCheckIfChild(modal, Self->UID);

         if ((error != ERR_True) and (error != ERR_LimitedSuccess)) {
            // Focussing is not OK - surface is out of the modal's scope
            log.warning("Surface #%d is not within modal #%d's scope.", Self->UID, modal);
            glLastFocusTime = PreciseTime();
            return ERR_Failed|ERF_Notified;
         }
      }
   }

   OBJECTID *focuslist;
   if (!AccessMemoryID(RPM_FocusList, MEM_READ_WRITE, 1000, &focuslist)) {
      // Return immediately if this surface object already has the -primary- focus

      if ((Self->Flags & RNF_HAS_FOCUS) and (focuslist[0] IS Self->UID)) {
         FOCUSMSG("Surface already has the primary focus.");
         ReleaseMemory(focuslist);
         glLastFocusTime = PreciseTime();
         return ERR_Okay|ERF_Notified;
      }

      LONG j;
      LONG lost = 0; // Count of surfaces that have lost the focus
      LONG has_focus = 0; // Count of surfaces with the focus
      SurfaceControl *ctl;
      OBJECTID lostfocus[SIZE_FOCUSLIST];
      if ((ctl = gfxAccessList(ARF_READ))) {
         auto surfacelist = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);

         LONG surface_index;
         OBJECTID surface_id = Self->UID;
         if ((surface_index = find_own_index(ctl, Self)) IS -1) {
            // This is not a critical failure as child surfaces can be expected to disappear from the surface list
            // during the free process.

            gfxReleaseList(ARF_READ);
            ReleaseMemory(focuslist);
            glLastFocusTime = PreciseTime();
            return ERR_Failed|ERF_Notified;
         }

         // Build the new focus chain in a local focus list.  Also also reset the HAS_FOCUS flag.  Surfaces that have
         // lost the focus go in the lostfocus list.

         // Starting from the end of the list, everything leading towards the target surface will need to lose the focus.

         for (j=ctl->Total-1; j > surface_index; j--) {
            if (surfacelist[j].Flags & RNF_HAS_FOCUS) {
               if (lost < ARRAYSIZE(lostfocus)-1) lostfocus[lost++] = surfacelist[j].SurfaceID;
               surfacelist[j].Flags &= ~RNF_HAS_FOCUS;
            }
         }

         // The target surface and all its parents will need to gain the focus

         for (j=surface_index; j >= 0; j--) {
            if (surfacelist[j].SurfaceID != surface_id) {
               if (surfacelist[j].Flags & RNF_HAS_FOCUS) {
                  if (lost < ARRAYSIZE(lostfocus)-1) lostfocus[lost++] = surfacelist[j].SurfaceID;
                  surfacelist[j].Flags &= ~RNF_HAS_FOCUS;
               }
            }
            else {
               surfacelist[j].Flags |= RNF_HAS_FOCUS;
               if (has_focus < SIZE_FOCUSLIST-1) focuslist[has_focus++] = surface_id;
               surface_id = surfacelist[j].ParentID;
               if (!surface_id) {
                  j--;
                  break; // Break out of the loop when there are no more parents left
               }
            }
         }

         // This next loop is important for hosted environments where multiple windows are active.  It ensures that
         // surfaces contained by other windows also lose the focus.

         while (j >= 0) {
            if (surfacelist[j].Flags & RNF_HAS_FOCUS) {
               if (lost < ARRAYSIZE(lostfocus)-1) lostfocus[lost++] = surfacelist[j].SurfaceID;
               surfacelist[j].Flags &= ~RNF_HAS_FOCUS;
            }
            j--;
         }

         focuslist[has_focus] = 0;
         lostfocus[lost] = 0;

         gfxReleaseList(ARF_READ);
      }
      else {
         ReleaseMemory(focuslist);
         glLastFocusTime = PreciseTime();
         return log.warning(ERR_AccessMemory);
      }

      // Send a Focus action to all parent surface objects in our generated focus list.

      struct drwInheritedFocus inherit = {
         .FocusID = Self->UID,
         .Flags   = Self->Flags
      };
      for (LONG i=1; focuslist[i]; i++) { // Start from one to skip Self
         ActionMsg(MT_DrwInheritedFocus, focuslist[i], &inherit);
      }

      // Send out LostFocus actions to all objects that do not intersect with the new focus chain.

      for (LONG i=0; lostfocus[i]; i++) {
         acLostFocus(lostfocus[i]);
      }

      // Send a global focus event to all listeners

      LONG event_size = sizeof(evFocus) + (has_focus * sizeof(OBJECTID)) + (lost * sizeof(OBJECTID));
      UBYTE buffer[event_size];
      evFocus *ev = (evFocus *)buffer;
      ev->EventID        = EVID_GUI_SURFACE_FOCUS;
      ev->TotalWithFocus = has_focus;
      ev->TotalLostFocus = lost;

      OBJECTID *outlist = &ev->FocusList[0];
      LONG o = 0;
      for (LONG i=0; i < has_focus; i++) outlist[o++] = focuslist[i];
      for (LONG i=0; i < lost; i++) outlist[o++] = lostfocus[i];
      BroadcastEvent(ev, event_size);

      ReleaseMemory(focuslist);

      if (Self->Flags & RNF_HAS_FOCUS) {
         // Return without notification as we already have the focus

         if (Self->RevertFocusID) {
            Self->RevertFocusID = 0;
            ActionMsg(AC_Focus, Self->RevertFocusID, NULL);
         }
         glLastFocusTime = PreciseTime();
         return ERR_Okay|ERF_Notified;
      }
      else {
         Self->Flags |= RNF_HAS_FOCUS;
         UpdateSurfaceField(Self, &SurfaceList::Flags, Self->Flags);

         // Focussing on the display window is important in hosted environments

         if (Self->DisplayID) acFocus(Self->DisplayID);

         if (Self->RevertFocusID) {
            Self->RevertFocusID = 0;
            ActionMsg(AC_Focus, Self->RevertFocusID, NULL);
         }

         glLastFocusTime = PreciseTime();
         return ERR_Okay;
      }
   }
   else {
      glLastFocusTime = PreciseTime();
      return log.warning(ERR_AccessMemory)|ERF_Notified;
   }
}

//****************************************************************************

static ERROR SURFACE_Free(extSurface *Self, APTR Void)
{
   if (Self->ScrollTimer) { UpdateTimer(Self->ScrollTimer, 0); Self->ScrollTimer = 0; }
   if (Self->RedrawTimer) { UpdateTimer(Self->RedrawTimer, 0); Self->RedrawTimer = 0; }

   if (!Self->ParentID) {
      if (Self->TaskRemovedHandle) { UnsubscribeEvent(Self->TaskRemovedHandle); Self->TaskRemovedHandle = NULL; }
      if (Self->UserLoginHandle) { UnsubscribeEvent(Self->UserLoginHandle); Self->UserLoginHandle = NULL; }
   }

   if ((Self->Callback) and (Self->Callback != Self->CallbackCache)) {
      FreeResource(Self->Callback);
      Self->Callback = NULL;
      Self->CallbackCount = 0;
      Self->CallbackSize = 0;
   }

   if (Self->ParentID) {
      extSurface *parent;
      ERROR error;
      if (!(error = AccessObjectID(Self->ParentID, 5000, &parent))) {
         UnsubscribeAction(parent, NULL);
         if (Self->Flags & RNF_TRANSPARENT) {
            drwRemoveCallback(parent, NULL);
         }
         ReleaseObject(parent);
      }
   }

   acHide(Self);

   // Remove any references to this surface object from the global surface list

   untrack_layer(Self->UID);

   if ((!Self->ParentID) and (Self->DisplayID)) {
      acFree(Self->DisplayID);
      Self->DisplayID = NULL;
   }

   if ((Self->BufferID) and ((!Self->BitmapOwnerID) or (Self->BitmapOwnerID IS Self->UID))) {
      if (Self->Bitmap) { ReleaseObject(Self->Bitmap); Self->Bitmap = NULL; }
      acFree(Self->BufferID);
      Self->BufferID = 0;
   }

   // Give the focus to the parent if our object has the primary focus.  Do not apply this technique to surface objects
   // acting as windows, as the window class has its own focus management code.

   if ((Self->Flags & RNF_HAS_FOCUS) and (GetClassID(Self->ownerID()) != ID_WINDOW)) {
      if (Self->ParentID) acFocus(Self->ParentID);
   }

   if (Self->Flags & RNF_AUTO_QUIT) {
      parasol::Log log;
      log.msg("Posting a quit message due to use of AUTOQUIT.");
      SendMessage(NULL, MSGID_QUIT, NULL, NULL, NULL);
   }

   if (Self->InputHandle) gfxUnsubscribeInput(Self->InputHandle);

   for (auto it = glWindowHooks.begin(); it != glWindowHooks.end();) {
      if (it->first.SurfaceID IS Self->UID) {
         it = glWindowHooks.erase(it);
      }
      else it++;
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Hide: Hides a surface object from the display.
-END-
*****************************************************************************/

static ERROR SURFACE_Hide(extSurface *Self, APTR Void)
{
   parasol::Log log;

   log.traceBranch("");

   if (!(Self->Flags & RNF_VISIBLE)) return ERR_Okay|ERF_Notified;

   if (!Self->ParentID) {
      Self->Flags &= ~RNF_VISIBLE; // Important to switch off visibliity before Hide(), otherwise a false redraw will occur.
      UpdateSurfaceField(Self, &SurfaceList::Flags, Self->Flags);

      if (acHide(Self->DisplayID) != ERR_Okay) return ERR_Failed;
   }
   else {
      // Mark this surface object as invisible, then invalidate the region it was covering in order to have the background redrawn.

      Self->Flags &= ~RNF_VISIBLE;
      UpdateSurfaceField(Self, &SurfaceList::Flags, Self->Flags);

      if (Self->BitmapOwnerID != Self->UID) {
         gfxRedrawSurface(Self->ParentID, Self->X, Self->Y, Self->Width, Self->Height, IRF_RELATIVE);
      }
      gfxExposeSurface(Self->ParentID, Self->X, Self->Y, Self->Width, Self->Height, EXF_CHILDREN|EXF_REDRAW_VOLATILE);
   }

   // Check if the surface is modal, if so, switch it off

   TaskList *task;
   if (Self->PrevModalID) {
      gfxSetModalSurface(Self->PrevModalID);
      Self->PrevModalID = 0;
   }
   else if ((task = (TaskList *)GetResourcePtr(RES_TASK_CONTROL))) {
      if (task->ModalID IS Self->UID) {
         log.msg("Surface is modal, switching off modal mode.");
         task->ModalID = 0;
      }
   }

   refresh_pointer(Self);
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
InheritedFocus: Private

Private

-INPUT-
oid FocusID: Private
int Flags: Private

-ERRORS-
Okay
-END-

*****************************************************************************/

static ERROR SURFACE_InheritedFocus(extSurface *Self, struct gfxInheritedFocus *Args)
{
   Message *msg;

   if ((msg = GetActionMsg())) {
      // This is a message - in which case it could have been delayed and thus superseded by a more recent message.

      if (msg->Time < glLastFocusTime) {
         FOCUSMSG("Ignoring superseded focus message.");
         return ERR_Okay|ERF_Notified;
      }
   }

   glLastFocusTime = PreciseTime();

   if (Self->Flags & RNF_HAS_FOCUS) {
      FOCUSMSG("This surface already has focus.");
      return ERR_Okay;
   }
   else {
      FOCUSMSG("Object has received the focus through inheritance.");

      Self->Flags |= RNF_HAS_FOCUS;

      //UpdateSurfaceField(Self, Flags); // Not necessary because SURFACE_Focus sets the surfacelist

      NotifySubscribers(Self, AC_Focus, NULL, ERR_Okay);
      return ERR_Okay;
   }
}

//****************************************************************************

static ERROR SURFACE_Init(extSurface *Self, APTR Void)
{
   parasol::Log log;
   objBitmap *bitmap;

   BYTE require_store = FALSE;
   OBJECTID parent_bitmap = 0;
   OBJECTID bitmap_owner  = 0;

   if (!Self->RootID) Self->RootID = Self->UID;

   if (Self->Flags & RNF_CURSOR) Self->Flags |= RNF_STICK_TO_FRONT;

   // If no parent surface is set, check if the client has set the FULL_SCREEN flag.  If not, try to give the
   // surface a parent.

   if ((!Self->ParentID) and (gfxGetDisplayType() IS DT_NATIVE)) {
      if (!(Self->Flags & RNF_FULL_SCREEN)) {
         if (FindObject("desktop", ID_SURFACE, 0, &Self->ParentID) != ERR_Okay) {
            SurfaceControl *ctl;
            if ((ctl = gfxAccessList(ARF_READ))) {
               auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
               Self->ParentID = list[0].SurfaceID;
               gfxReleaseList(ARF_READ);
            }
         }
      }
   }

   ERROR error;
   if (Self->ParentID) {
      extSurface *parent;
      if (AccessObjectID(Self->ParentID, 3000, &parent) != ERR_Okay) {
         log.warning("Failed to access parent #%d.", Self->ParentID);
         return ERR_AccessObject;
      }

      log.trace("Initialising surface to parent #%d.", Self->ParentID);

      error = ERR_Okay;

      // If the parent has the ROOT flag set, we have to inherit whatever root layer that the parent is using, as well
      // as the PRECOPY and/or AFTERCOPY and opacity flags if they are set.

      if (parent->Type & RT_ROOT) { // The window class can set the ROOT type
         Self->Type |= RT_ROOT;
         if (Self->RootID IS Self->UID) {
            Self->InheritedRoot = TRUE;
            Self->RootID = parent->RootID; // Inherit the parent's root layer
         }
      }

      // Subscribe to the surface parent's Resize and Redimension actions

      auto callback = make_function_stdc(notify_free_parent);
      SubscribeAction(parent, AC_Free, &callback);

      callback = make_function_stdc(notify_redimension_parent);
      SubscribeAction(parent, AC_Redimension, &callback);

      // If the surface object is transparent, subscribe to the Draw action of the parent object.

      if (Self->Flags & RNF_TRANSPARENT) {
         FUNCTION func;
         struct drwAddCallback args = { &func };
         func.Type = CALL_STDC;
         func.StdC.Context = Self;
         func.StdC.Routine = (APTR)&draw_region;
         Action(MT_DrwAddCallback, parent, &args);

         // Turn off flags that should never be combined with transparent surfaces.
         Self->Flags &= ~(RNF_PRECOPY|RNF_AFTER_COPY|RNF_COMPOSITE);
         Self->Colour.Alpha = 0;
      }

      // Set FixedX/FixedY accordingly - this is used to assist in the layout process when a surface is used in a document.

      if (Self->Dimensions & 0xffff) {
         if ((Self->Dimensions & DMF_X) and (Self->Dimensions & (DMF_FIXED_WIDTH|DMF_RELATIVE_WIDTH|DMF_FIXED_X_OFFSET|DMF_RELATIVE_X_OFFSET))) {
            Self->FixedX = TRUE;
         }
         else if ((Self->Dimensions & DMF_X_OFFSET) and (Self->Dimensions & (DMF_FIXED_WIDTH|DMF_RELATIVE_WIDTH|DMF_FIXED_X|DMF_RELATIVE_X))) {
            Self->FixedX = TRUE;
         }

         if ((Self->Dimensions & DMF_Y) and (Self->Dimensions & (DMF_FIXED_HEIGHT|DMF_RELATIVE_HEIGHT|DMF_FIXED_Y_OFFSET|DMF_RELATIVE_Y_OFFSET))) {
            Self->FixedY = TRUE;
         }
         else if ((Self->Dimensions & DMF_Y_OFFSET) and (Self->Dimensions & (DMF_FIXED_HEIGHT|DMF_RELATIVE_HEIGHT|DMF_FIXED_Y|DMF_RELATIVE_Y))) {
            Self->FixedY = TRUE;
         }
      }

      // Recalculate coordinates if offsets are used

      if (Self->Dimensions & DMF_FIXED_X_OFFSET)         Self->set(FID_XOffset, Self->XOffset);
      else if (Self->Dimensions & DMF_RELATIVE_X_OFFSET) Self->setPercentage(FID_XOffset, Self->XOffsetPercent);

      if (Self->Dimensions & DMF_FIXED_Y_OFFSET)         Self->set(FID_YOffset, Self->YOffset);
      else if (Self->Dimensions & DMF_RELATIVE_Y_OFFSET) Self->setPercentage(FID_YOffset, Self->YOffsetPercent);

      if (Self->Dimensions & DMF_RELATIVE_X)       Self->setPercentage(FID_X, Self->XPercent);
      if (Self->Dimensions & DMF_RELATIVE_Y)       Self->setPercentage(FID_Y, Self->YPercent);
      if (Self->Dimensions & DMF_RELATIVE_WIDTH)   Self->setPercentage(FID_Width,  Self->WidthPercent);
      if (Self->Dimensions & DMF_RELATIVE_HEIGHT)  Self->setPercentage(FID_Height, Self->HeightPercent);

      if (!(Self->Dimensions & DMF_WIDTH)) {
         if (Self->Dimensions & (DMF_RELATIVE_X_OFFSET|DMF_FIXED_X_OFFSET)) {
            Self->Width = parent->Width - Self->X - Self->XOffset;
         }
         else {
            Self->Width = 20;
            Self->Dimensions |= DMF_FIXED_WIDTH;
         }
      }

      if (!(Self->Dimensions & DMF_HEIGHT)) {
         if (Self->Dimensions & (DMF_RELATIVE_Y_OFFSET|DMF_FIXED_Y_OFFSET)) {
            Self->Height = parent->Height - Self->Y - Self->YOffset;
         }
         else {
            Self->Height = 20;
            Self->Dimensions |= DMF_FIXED_HEIGHT;
         }
      }

      // Alignment adjustments

      if (Self->Align & ALIGN_LEFT) { Self->X = 0; Self->set(FID_X, Self->X); }
      else if (Self->Align & ALIGN_RIGHT) { Self->X = parent->Width - Self->Width; Self->set(FID_X, Self->X); }
      else if (Self->Align & ALIGN_HORIZONTAL) { Self->X = (parent->Width - Self->Width) / 2; Self->set(FID_X, Self->X); }

      if (Self->Align & ALIGN_TOP) { Self->Y = 0; Self->set(FID_Y, Self->Y); }
      else if (Self->Align & ALIGN_BOTTOM) { Self->Y = parent->Height - Self->Height; Self->set(FID_Y, Self->Y); }
      else if (Self->Align & ALIGN_VERTICAL) { Self->Y = (parent->Height - Self->Height) / 2; Self->set(FID_Y, Self->Y); }

      if (Self->Height < Self->MinHeight + Self->TopMargin  + Self->BottomMargin) Self->Height = Self->MinHeight + Self->TopMargin  + Self->BottomMargin;
      if (Self->Width  < Self->MinWidth  + Self->LeftMargin + Self->RightMargin)  Self->Width  = Self->MinWidth  + Self->LeftMargin + Self->RightMargin;
      if (Self->Height > Self->MaxHeight + Self->TopMargin  + Self->BottomMargin) Self->Height = Self->MaxHeight + Self->TopMargin  + Self->BottomMargin;
      if (Self->Width  > Self->MaxWidth  + Self->LeftMargin + Self->RightMargin)  Self->Width  = Self->MaxWidth  + Self->LeftMargin + Self->RightMargin;

      Self->DisplayID     = parent->DisplayID;
      Self->DisplayWindow = parent->DisplayWindow;
      parent_bitmap       = parent->BufferID;
      bitmap_owner        = parent->BitmapOwnerID;

      // If the parent is a host, all child surfaces within it must get their own bitmap space.
      // If not, managing layered surfaces between processes becomes more difficult.

      if (parent->Flags & RNF_HOST) require_store = TRUE;

      ReleaseObject(parent);
   }
   else {
      log.trace("This surface object will be display-based.");

      // Turn off any flags that may not be used for the top-most layer

      Self->Flags &= ~(RNF_TRANSPARENT|RNF_PRECOPY|RNF_AFTER_COPY);

      LONG scrflags = 0;

      if (Self->Type & RT_ROOT) {
         gfxSetHostOption(HOST_TASKBAR, 1);
         gfxSetHostOption(HOST_TRAY_ICON, 0);
      }
      else switch(Self->WindowType) {
         default: // SWIN_HOST
            log.trace("Enabling standard hosted window mode.");
            gfxSetHostOption(HOST_TASKBAR, 1);
            break;

         case SWIN_TASKBAR:
            log.trace("Enabling borderless taskbar based surface.");
            scrflags |= SCR_BORDERLESS; // Stop the display from creating a host window for the surface
            if (Self->Flags & RNF_HOST) scrflags |= SCR_MAXIMISE;
            gfxSetHostOption(HOST_TASKBAR, 1);
            break;

         case SWIN_ICON_TRAY:
            log.trace("Enabling borderless icon-tray based surface.");
            scrflags |= SCR_BORDERLESS; // Stop the display from creating a host window for the surface
            if (Self->Flags & RNF_HOST) scrflags |= SCR_MAXIMISE;
            gfxSetHostOption(HOST_TRAY_ICON, 1);
            break;

         case SWIN_NONE:
            log.trace("Enabling borderless, presence-less surface.");
            scrflags |= SCR_BORDERLESS; // Stop the display from creating a host window for the surface
            if (Self->Flags & RNF_HOST) scrflags |= SCR_MAXIMISE;
            gfxSetHostOption(HOST_TASKBAR, 0);
            gfxSetHostOption(HOST_TRAY_ICON, 0);
            break;
      }

      if (gfxGetDisplayType() IS DT_NATIVE) Self->Flags &= ~(RNF_COMPOSITE);

      if (((gfxGetDisplayType() IS DT_WINDOWS) or (gfxGetDisplayType() IS DT_X11)) and (Self->Flags & RNF_HOST)) {
         if (glpMaximise) scrflags |= SCR_MAXIMISE;
         if (glpFullScreen) scrflags |= SCR_MAXIMISE|SCR_BORDERLESS;
      }

      if (!(Self->Dimensions & DMF_FIXED_WIDTH)) {
         Self->Width = glpDisplayWidth;
         Self->Dimensions |= DMF_FIXED_WIDTH;
      }

      if (!(Self->Dimensions & DMF_FIXED_HEIGHT)) {
         Self->Height = glpDisplayHeight;
         Self->Dimensions |= DMF_FIXED_HEIGHT;
      }

      if (!(Self->Dimensions & DMF_FIXED_X)) {
         if (Self->Flags & RNF_HOST) Self->X = 0;
         else Self->X = glpDisplayX;
         Self->Dimensions |= DMF_FIXED_X;
      }

      if (!(Self->Dimensions & DMF_FIXED_Y)) {
         if (Self->Flags & RNF_HOST) Self->Y = 0;
         else Self->Y = glpDisplayY;
         Self->Dimensions |= DMF_FIXED_Y;
      }

      if ((Self->Width < 10) or (Self->Height < 6)) {
         Self->Width = 640;
         Self->Height = 480;
      }

      if (gfxGetDisplayType() != DT_NATIVE) {
         // Alignment adjustments

         DISPLAYINFO *display;
         if (!gfxGetDisplayInfo(0, &display)) {
            if (Self->Align & ALIGN_LEFT) { Self->X = 0; Self->set(FID_X, Self->X); }
            else if (Self->Align & ALIGN_RIGHT) { Self->X = display->Width - Self->Width; Self->set(FID_X, Self->X); }
            else if (Self->Align & ALIGN_HORIZONTAL) { Self->X = (display->Width - Self->Width) / 2; Self->set(FID_X, Self->X); }

            if (Self->Align & ALIGN_TOP) { Self->Y = 0; Self->set(FID_Y, Self->Y); }
            else if (Self->Align & ALIGN_BOTTOM) { Self->Y = display->Height - Self->Height; Self->set(FID_Y, Self->Y); }
            else if (Self->Align & ALIGN_VERTICAL) { Self->Y = (display->Height - Self->Height) / 2; Self->set(FID_Y, Self->Y); }
         }
      }

      if (Self->Height < Self->MinHeight + Self->TopMargin  + Self->BottomMargin) Self->Height = Self->MinHeight + Self->TopMargin  + Self->BottomMargin;
      if (Self->Width  < Self->MinWidth  + Self->LeftMargin + Self->RightMargin)  Self->Width  = Self->MinWidth  + Self->LeftMargin + Self->RightMargin;
      if (Self->Height > Self->MaxHeight + Self->TopMargin  + Self->BottomMargin) Self->Height = Self->MaxHeight + Self->TopMargin  + Self->BottomMargin;
      if (Self->Width  > Self->MaxWidth  + Self->LeftMargin + Self->RightMargin)  Self->Width  = Self->MaxWidth  + Self->LeftMargin + Self->RightMargin;

      if (Self->Flags & RNF_STICK_TO_FRONT) gfxSetHostOption(HOST_STICK_TO_FRONT, 1);
      else gfxSetHostOption(HOST_STICK_TO_FRONT, 0);

      if (Self->Flags & RNF_COMPOSITE) scrflags |= SCR_COMPOSITE;

      OBJECTID id;
      CSTRING name = FindObject("SystemDisplay", 0, 0, &id) ? "SystemDisplay" : (CSTRING)NULL;

      // For hosted displays:  On initialisation, the X and Y fields reflect the position at which the window will be
      // opened on the host desktop.  However, hosted surfaces operate on the absolute coordinates of client regions
      // and are ignorant of window frames, so we read the X, Y fields back from the display after initialisation (the
      // display will adjust the coordinates to reflect the absolute position of the surface on the desktop).

      objDisplay *display;
      if (!NewObject(ID_DISPLAY, NF::INTEGRAL, &display)) {
         SetFields(display,
               FID_Name|TSTR,           name,
               FID_X|TLONG,             Self->X,
               FID_Y|TLONG,             Self->Y,
               FID_Width|TLONG,         Self->Width,
               FID_Height|TLONG,        Self->Height,
               FID_BitsPerPixel|TLONG,  glpDisplayDepth,
               FID_RefreshRate|TDOUBLE, glpRefreshRate,
               FID_Flags|TLONG,         scrflags,
               FID_DPMS|TSTRING,        glpDPMS,
               FID_Opacity|TLONG,       (Self->Opacity * 100) / 255,
               FID_WindowHandle|TPTR,   (APTR)Self->DisplayWindow, // Sometimes a window may be preset, e.g. for a web plugin
               TAGEND);

         if (Self->PopOverID) {
            extSurface *popsurface;
            if (!AccessObjectID(Self->PopOverID, 2000, &popsurface)) {
               OBJECTID pop_display = popsurface->DisplayID;
               ReleaseObject(popsurface);

               if (pop_display) display->set(FID_PopOver, pop_display);
               else log.warning("Surface #%d doesn't have a display ID for pop-over.", Self->PopOverID);
            }
         }

         if (!acInit(display)) {
            gfxSetGamma(display, glpGammaRed, glpGammaGreen, glpGammaBlue, GMF_SAVE);
            gfxSetHostOption(HOST_TASKBAR, 1); // Reset display system so that windows open with a taskbar by default

            // Get the true coordinates of the client area of the surface

            Self->X = display->X;
            Self->Y = display->Y;
            Self->Width  = display->Width;
            Self->Height = display->Height;

            struct gfxSizeHints hints;

            if ((Self->MaxWidth) or (Self->MaxHeight) or (Self->MinWidth) or (Self->MinHeight)) {
               if (Self->MaxWidth > 0)  hints.MaxWidth  = Self->MaxWidth  + Self->LeftMargin + Self->RightMargin;  else hints.MaxWidth  = 0;
               if (Self->MaxHeight > 0) hints.MaxHeight = Self->MaxHeight + Self->TopMargin  + Self->BottomMargin; else hints.MaxHeight = 0;
               if (Self->MinWidth > 0)  hints.MinWidth  = Self->MinWidth  + Self->LeftMargin + Self->RightMargin;  else hints.MinWidth  = 0;
               if (Self->MinHeight > 0) hints.MinHeight = Self->MinHeight + Self->TopMargin  + Self->BottomMargin; else hints.MinHeight = 0;
               Action(MT_GfxSizeHints, display, &hints);
            }

            acFlush(display);

            // For hosted environments, record the window handle (NB: this is doubling up the display handle, we should
            // just make the window handle a virtual field so that we don't need a permanent record of it).

            display->getPtr(FID_WindowHandle, &Self->DisplayWindow);

            #ifdef _WIN32
               winSetSurfaceID(Self->DisplayWindow, Self->UID);
            #endif

            // Subscribe to Redimension notifications if the display is hosted.  Also subscribe to Draw because this
            // can be used by the host to notify of window exposures.

            if (Self->DisplayWindow) {
               FUNCTION func = { .Type = CALL_STDC, .StdC = { .Context = NULL, .Routine = (APTR)&display_resized } };
               display->set(FID_ResizeFeedback, &func);

               auto callback = make_function_stdc(notify_draw_display);
               SubscribeAction(display, AC_Draw, &callback);
            }

            Self->DisplayID = display->UID;
            error = ERR_Okay;
         }
         else error = ERR_Init;

         if (error) acFree(display);
      }
      else error = ERR_NewObject;
   }

   // Allocate a backing store if this is a host object, or the parent is foreign, or we are the child of a host object
   // (check made earlier), or surface object is masked.

   if (!Self->ParentID) require_store = TRUE;
   else if (Self->Flags & (RNF_PRECOPY|RNF_COMPOSITE|RNF_AFTER_COPY|RNF_CURSOR)) require_store = TRUE;
   else {
      if (Self->BitsPerPixel >= 8) {
         DISPLAYINFO *info;
         if (!gfxGetDisplayInfo(Self->DisplayID, &info)) {
            if (info->BitsPerPixel != Self->BitsPerPixel) require_store = TRUE;
         }
      }
   }

   if (Self->Flags & (RNF_TRANSPARENT)) require_store = FALSE;

   if (require_store) {
      Self->BitmapOwnerID = Self->UID;

      objDisplay *display;
      if (!(error = AccessObjectID(Self->DisplayID, 3000, &display))) {
         LONG memflags = MEM_DATA;

         if (Self->Flags & RNF_VIDEO) {
            // If acceleration is available then it is OK to create the buffer in video RAM.

            if (!(display->Flags & SCR_NO_ACCELERATION)) memflags = MEM_TEXTURE;
         }

         LONG bpp;
         if (Self->Flags & RNF_COMPOSITE) {
            // If dynamic compositing will be used then we must have an alpha channel
            bpp = 32;
         }
         else if (Self->BitsPerPixel) {
            bpp = Self->BitsPerPixel; // BPP has been preset by the client
            log.msg("Preset depth of %d bpp detected.", bpp);
         }
         else bpp = display->Bitmap->BitsPerPixel;

         if (!NewObject(ID_BITMAP, NF::INTEGRAL, &bitmap)) {
            SetFields(bitmap,
               FID_BitsPerPixel|TLONG, bpp,
               FID_Width|TLONG,        Self->Width,
               FID_Height|TLONG,       Self->Height,
               FID_DataFlags|TLONG,    memflags,
               FID_Flags|TLONG,        ((Self->Flags & RNF_COMPOSITE) ? (BMF_ALPHA_CHANNEL|BMF_FIXED_DEPTH) : NULL),
               TAGEND);
            if (!acInit(bitmap)) {
               if (Self->BitsPerPixel) bitmap->Flags |= BMF_FIXED_DEPTH; // This flag prevents automatic changes to the bit depth

               Self->BitsPerPixel  = bitmap->BitsPerPixel;
               Self->BytesPerPixel = bitmap->BytesPerPixel;
               Self->LineWidth     = bitmap->LineWidth;
               Self->Data          = bitmap->Data;
               Self->BufferID      = bitmap->UID;
               error = ERR_Okay;
            }
            else {
               error = ERR_Init;
               acFree(bitmap);
            }
         }
         else error = ERR_NewObject;

         ReleaseObject(display);
      }
      else error = ERR_AccessObject;

      if (error) return log.warning(error);
   }
   else {
      Self->BufferID      = parent_bitmap;
      Self->BitmapOwnerID = bitmap_owner;
   }

   // If the FIXEDBUFFER option is set, pass the NEVERSHRINK option to the bitmap

   if (Self->Flags & RNF_FIXED_BUFFER) {
      if (!AccessObjectID(Self->BufferID, 5000, &bitmap)) {
         bitmap->Flags |= BMF_NEVER_SHRINK;
         ReleaseObject(bitmap);
      }
   }

   // Track the surface object

   if (track_layer(Self) != ERR_Okay) return ERR_Failed;

   // The PopOver reference can only be managed once track_layer() has been called if this is a surface with a parent.

   if ((Self->ParentID) and (Self->PopOverID)) {
      // Ensure that the referenced surface is in front of the sibling.  Note that if we can establish that the
      // provided surface ID is not a sibling, the request is cancelled.

      OBJECTID popover_id = Self->PopOverID;
      Self->PopOverID = 0;

      acMoveToFront(Self);

      SurfaceControl *ctl;
      if ((ctl = gfxAccessList(ARF_READ))) {
         auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
         LONG index;
         if ((index = find_own_index(ctl, Self)) != -1) {
            for (LONG j=index; (j >= 0) and (list[j].SurfaceID != list[index].ParentID); j--) {
               if (list[j].SurfaceID IS popover_id) {
                  Self->PopOverID = popover_id;
                  break;
               }
            }
         }

         gfxReleaseList(ARF_READ);
      }

      if (!Self->PopOverID) {
         log.warning("PopOver surface #%d is not a sibling of this surface.", popover_id);
         UpdateSurfaceField(Self, &SurfaceList::PopOverID, Self->PopOverID);
      }
   }

   // Move the surface object to the back of the surface list when stick-to-back is enforced.

   if (Self->Flags & RNF_STICK_TO_BACK) acMoveToBack(Self);

   // Listen to the DeadTask event if we are a host surface object.  This will allow us to clean up the SurfaceList
   // when a task crashes.  Listening to the UserLogin event also allows us to switch to the user's preferred display
   // format on login.

   if ((!Self->ParentID) and (!StrMatch("SystemSurface", GetName(Self)))) {
      auto call = make_function_stdc(event_task_removed);
      SubscribeEvent(EVID_SYSTEM_TASK_REMOVED, &call, &Self->UID, &Self->TaskRemovedHandle);

      call = make_function_stdc(event_user_login);
      SubscribeEvent(EVID_USER_STATUS_LOGIN, &call, &Self->UID, &Self->UserLoginHandle);
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
LostFocus: Informs a surface object that it has lost the user focus.
-END-
*****************************************************************************/

static ERROR SURFACE_LostFocus(extSurface *Self, APTR Void)
{
#if 0
   Message *msg;

   if ((msg = GetActionMsg())) {
      // This is a message - in which case it could have been delayed and thus superseded by a more recent call.

      if (msg->Time < glLastFocusTime) {
         FOCUSMSG("Ignoring superseded focus message.");
         return ERR_Okay|ERF_Notified;
      }
   }

   glLastFocusTime = PreciseTime();
#endif

   if (Self->Flags & RNF_HAS_FOCUS) {
      Self->Flags &= ~RNF_HAS_FOCUS;
      UpdateSurfaceField(Self, &SurfaceList::Flags, Self->Flags);
      return ERR_Okay;
   }
   else return ERR_Okay | ERF_Notified;
}

/*****************************************************************************

-METHOD-
Minimise: For hosted surfaces only, this method will minimise the surface to an icon.

If a surface is hosted in a desktop window, calling the Minimise method will perform the default minimise action
on that window.  On a platform such as Microsoft Windows, this would normally result in the window being
minimised to the task bar.

Calling Minimise on a surface that is already in the minimised state may result in the host window being restored to
the desktop.  This behaviour is platform dependent and should be manually tested to confirm its reliability on the
host platform.
-END-

*****************************************************************************/

static ERROR SURFACE_Minimise(extSurface *Self, APTR Void)
{
   if (Self->DisplayID) ActionMsg(MT_GfxMinimise, Self->DisplayID, NULL);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Move: Moves a surface object to a new display position.
-END-
*****************************************************************************/

static ERROR SURFACE_Move(extSurface *Self, struct acMove *Args)
{
   parasol::Log log;
   struct acMove move;
   LONG i;

   if (!Args) return log.warning(ERR_NullArgs)|ERF_Notified;

   // Check if other move messages are queued for this object - if so, do not do anything until the final message is
   // reached.
   //
   // NOTE: This has a downside if the surface object is being fed a sequence of move messages for the purposes of
   // scrolling from one point to another.  Potentially the user may not see the intended effect or witness erratic
   // response times.

   APTR queue;
   if (!AccessMemoryID(GetResource(RES_MESSAGE_QUEUE), MEM_READ, 2000, &queue)) {
      LONG index = 0;
      UBYTE msgbuffer[sizeof(Message) + sizeof(ActionMessage) + sizeof(struct acMove)];
      while (!ScanMessages(queue, &index, MSGID_ACTION, msgbuffer, sizeof(msgbuffer))) {
         auto action = (ActionMessage *)(msgbuffer + sizeof(Message));

         if ((action->ActionID IS AC_MoveToPoint) and (action->ObjectID IS Self->UID)) {
            ReleaseMemory(queue);
            return ERR_Okay|ERF_Notified;
         }
         else if ((action->ActionID IS AC_Move) and (action->SendArgs IS TRUE) and
                  (action->ObjectID IS Self->UID)) {
            auto msgmove = (struct acMove *)(action + 1);
            msgmove->DeltaX += Args->DeltaX;
            msgmove->DeltaY += Args->DeltaY;
            msgmove->DeltaZ += Args->DeltaZ;

            UpdateMessage(queue, ((Message *)msgbuffer)->UniqueID, NULL, action, sizeof(ActionMessage) + sizeof(struct acMove));

            ReleaseMemory(queue);
            return ERR_Okay|ERF_Notified;
         }
      }
      ReleaseMemory(queue);
   }

   if (Self->Flags & RNF_STICKY) return ERR_Failed|ERF_Notified;

   LONG xchange = Args->DeltaX;
   LONG ychange = Args->DeltaY;

   if (Self->Flags & RNF_NO_HORIZONTAL) move.DeltaX = 0;
   else move.DeltaX = xchange;

   if (Self->Flags & RNF_NO_VERTICAL) move.DeltaY = 0;
   else move.DeltaY = ychange;

   move.DeltaZ = 0;

   // If there isn't any movement, return immediately

   if ((move.DeltaX < 1) and (move.DeltaX > -1) and (move.DeltaY < 1) and (move.DeltaY > -1)) {
      return ERR_Failed|ERF_Notified;
   }

   log.traceBranch("X,Y: %d,%d", xchange, ychange);

   SurfaceControl *ctl;

   // Margin/Limit handling

   if (!Self->ParentID) {
      move_layer(Self, Self->X + move.DeltaX, Self->Y + move.DeltaY);
   }
   else if ((ctl = gfxAccessList(ARF_READ))) {
      auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
      if ((i = find_parent_index(ctl, Self)) != -1) {
         // Horizontal limit handling

         if (xchange < 0) {
            if ((Self->X + xchange) < Self->LeftLimit) {
               if (Self->X < Self->LeftLimit) move.DeltaX = 0;
               else move.DeltaX = -(Self->X - Self->LeftLimit);
            }
         }
         else if (xchange > 0) {
            if ((Self->X + Self->Width) > (list[i].Width - Self->RightLimit)) move.DeltaX = 0;
            else if ((Self->X + Self->Width + xchange) > (list[i].Width - Self->RightLimit)) {
               move.DeltaX = (list[i].Width - Self->RightLimit - Self->Width) - Self->X;
            }
         }

         // Vertical limit handling

         if (ychange < 0) {
            if ((Self->Y + ychange) < Self->TopLimit) {
               if ((Self->Y + Self->Height) < Self->TopLimit) move.DeltaY = 0;
               else move.DeltaY = -(Self->Y - Self->TopLimit);
            }
         }
         else if (ychange > 0) {
            if ((Self->Y + Self->Height) > (list[i].Height - Self->BottomLimit)) move.DeltaY = 0;
            else if ((Self->Y + Self->Height + ychange) > (list[i].Height - Self->BottomLimit)) {
               move.DeltaY = (list[i].Height - Self->BottomLimit - Self->Height) - Self->Y;
            }
         }

         // Second check: If there isn't any movement, return immediately

         if ((!move.DeltaX) and (!move.DeltaY)) {
            gfxReleaseList(ARF_READ);
            return ERR_Failed|ERF_Notified;
         }
      }

      gfxReleaseList(ARF_WRITE);

      // Move the graphics layer

      move_layer(Self, Self->X + move.DeltaX, Self->Y + move.DeltaY);
   }
   else return log.warning(ERR_LockFailed)|ERF_Notified;

/* These lines cause problems for the resizing of offset surface objects.
   if (Self->Dimensions & DMF_X_OFFSET) Self->XOffset += move.DeltaX;
   if (Self->Dimensions & DMF_Y_OFFSET) Self->YOffset += move.DeltaY;
*/

   log.traceBranch("Sending redimension notifications");
   struct acRedimension redimension = { (DOUBLE)Self->X, (DOUBLE)Self->Y, 0, (DOUBLE)Self->Width, (DOUBLE)Self->Height, 0 };
   NotifySubscribers(Self, AC_Redimension, &redimension, ERR_Okay);
   return ERR_Okay|ERF_Notified;
}

/*****************************************************************************
-ACTION-
MoveToBack: Moves a surface object to the back of its container.
-END-
*****************************************************************************/

static ERROR SURFACE_MoveToBack(extSurface *Self, APTR Void)
{
   parasol::Log log;

   if (!Self->ParentID) {
      acMoveToBack(Self->DisplayID);
      return ERR_Okay|ERF_Notified;
   }

   log.branch("%s", GetName(Self));

   SurfaceControl *ctl;
   if ((ctl = gfxAccessList(ARF_WRITE))) {
      auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);

      LONG index; // Get our position within the chain
      if ((index = find_surface_list(list, ctl->Total, Self->UID)) IS -1) {
         gfxReleaseList(ARF_WRITE);
         return log.warning(ERR_Search)|ERF_Notified;
      }

      OBJECTID parent_bitmap;
      LONG i;
      if ((i = find_parent_index(ctl, Self)) != -1) parent_bitmap = list[i].BitmapID;
      else parent_bitmap = 0;

      // Find the position in the list that our surface object will be moved to

      LONG pos = index;
      LONG level = list[index].Level;
      for (i=index-1; (i >= 0) and (list[i].Level >= level); i--) {
         if (list[i].Level IS level) {
            if (Self->BitmapOwnerID IS Self->UID) { // If we own an independent bitmap, we cannot move behind surfaces that are members of the parent region
               if (list[i].BitmapID IS parent_bitmap) break;
            }
            if (list[i].SurfaceID IS Self->PopOverID) break; // Do not move behind surfaces that we must stay in front of
            if (!(Self->Flags & RNF_STICK_TO_BACK) and (list[i].Flags & RNF_STICK_TO_BACK)) break;
            pos = i;
         }
      }

      if (pos >= index) {  // If the position is unchanged, return immediately
         gfxReleaseList(ARF_READ);
         return ERR_Okay|ERF_Notified;
      }

      move_layer_pos(ctl, index, pos); // Reorder the list so that our surface object is inserted at the new position

      LONG total = ctl->Total;
      SurfaceList cplist[total];
      CopyMemory((BYTE *)ctl + ctl->ArrayIndex, cplist, sizeof(cplist[0]) * ctl->Total);

      gfxReleaseList(ARF_READ);

      if (Self->Flags & RNF_VISIBLE) {
         // Redraw our background if we are volatile
         if (check_volatile(cplist, index)) _redraw_surface(Self->UID, cplist, pos, total, cplist[pos].Left, cplist[pos].Top, cplist[pos].Right, cplist[pos].Bottom, NULL);

         // Expose changes to the display
         _expose_surface(Self->ParentID, cplist, pos, total, Self->X, Self->Y, Self->Width, Self->Height, EXF_CHILDREN|EXF_REDRAW_VOLATILE_OVERLAP);
      }
   }

   refresh_pointer(Self);

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
MoveToFront: Moves a surface object to the front of its container.
-END-
*****************************************************************************/

static ERROR SURFACE_MoveToFront(extSurface *Self, APTR Void)
{
   parasol::Log log;
   LONG currentindex, i;

   log.branch("%s", GetName(Self));

   if (!Self->ParentID) {
      acMoveToFront(Self->DisplayID);
      return ERR_Okay|ERF_Notified;
   }

   SurfaceControl *ctl;
   if (!(ctl = gfxAccessList(ARF_WRITE))) {
      return log.warning(ERR_AccessMemory)|ERF_Notified;
   }

   auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);

   if ((currentindex = find_own_index(ctl, Self)) IS -1) {
      gfxReleaseList(ARF_WRITE);
      return log.warning(ERR_Search)|ERF_Notified;
   }

   // Find the object in the list that our surface object will displace

   LONG index = currentindex;
   LONG level = list[currentindex].Level;
   for (i=currentindex+1; (list[i].Level >= list[currentindex].Level); i++) {
      if (list[i].Level IS level) {
         if (list[i].Flags & RNF_POINTER) break; // Do not move in front of the mouse cursor
         if (list[i].PopOverID IS Self->UID) break; // A surface has been discovered that has to be in front of us.

         if (Self->BitmapOwnerID != Self->UID) {
            // If we are a member of our parent's bitmap, we cannot be moved in front of bitmaps that own an independent buffer.

            if (list[i].BitmapID != Self->BufferID) break;
         }

         if (!(Self->Flags & RNF_STICK_TO_FRONT) and (list[i].Flags & RNF_STICK_TO_FRONT)) break;
         index = i;
      }
   }

   // If the position hasn't changed, return immediately

   if (index <= currentindex) {
      if (Self->PopOverID) {
         // Check if the surface that we're popped over is right behind us.  If not, move it forward.

         for (i=index-1; i > 0; i--) {
            if (list[i].Level IS level) {
               if (list[i].SurfaceID != Self->PopOverID) {
                  gfxReleaseList(ARF_WRITE);
                  acMoveToFront(Self->PopOverID);
                  return ERR_Okay|ERF_Notified;
               }
               break;
            }
         }
      }

      gfxReleaseList(ARF_WRITE);
      return ERR_Okay|ERF_Notified;
   }

   // Skip past the children that belong to the target object

   i = index;
   level = list[i].Level;
   while (list[i+1].Level > level) i++;

   // Count the number of children that have been assigned to our surface object.

   LONG total;
   for (total=1; list[currentindex+total].Level > list[currentindex].Level; total++) { };

   // Reorder the list so that our surface object is inserted at the new index.

   {
      SurfaceList tmp[total];
      CopyMemory(list + currentindex, &tmp, sizeof(SurfaceList) * total); // Copy the source entry into a buffer
      CopyMemory(list + currentindex + total, list + currentindex, sizeof(SurfaceList) * (i - currentindex - total + 1)); // Shift everything in front of us to the back
      i = i - total + 1;
      CopyMemory(&tmp, list + i, sizeof(SurfaceList) * total); // Copy our source entry to its new index
   }

   total = ctl->Total;
   SurfaceList cplist[total];
   CopyMemory((BYTE *)ctl + ctl->ArrayIndex, cplist, sizeof(cplist[0]) * ctl->Total);

   gfxReleaseList(ARF_WRITE);

   if (Self->Flags & RNF_VISIBLE) {
      // A redraw is required for:
      //   Any volatile regions that were in front of our surface prior to the move-to-front (by moving to the front, their background has been changed).
      //   Areas of our surface that were obscured by surfaces that also shared our bitmap space.

      objBitmap *bitmap;
      if (!AccessObjectID(Self->BufferID, 5000, &bitmap)) {
         invalidate_overlap(Self, cplist, total, currentindex, i, cplist[i].Left, cplist[i].Top, cplist[i].Right, cplist[i].Bottom, bitmap);
         ReleaseObject(bitmap);
      }

      if (check_volatile(cplist, i)) _redraw_surface(Self->UID, cplist, i, total, 0, 0, Self->Width, Self->Height, IRF_RELATIVE);
      _expose_surface(Self->UID, cplist, i, total, 0, 0, Self->Width, Self->Height, EXF_CHILDREN|EXF_REDRAW_VOLATILE_OVERLAP);
   }

   if (Self->PopOverID) {
      // Check if the surface that we're popped over is right behind us.  If not, move it forward.

      for (LONG i=index-1; i > 0; i--) {
         if (cplist[i].Level IS level) {
            if (cplist[i].SurfaceID != Self->PopOverID) {
               acMoveToFront(Self->PopOverID);
               return ERR_Okay;
            }
            break;
         }
      }
   }

   refresh_pointer(Self);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
MoveToPoint: Moves a surface object to an absolute coordinate.
-END-
*****************************************************************************/

static ERROR SURFACE_MoveToPoint(extSurface *Self, struct acMoveToPoint *Args)
{
   struct acMove move;

   if (Args->Flags & MTF_X) move.DeltaX = Args->X - Self->X;
   else move.DeltaX = 0;

   if (Args->Flags & MTF_Y) move.DeltaY = Args->Y - Self->Y;
   else move.DeltaY = 0;

   move.DeltaZ = 0;

   return Action(AC_Move, Self, &move)|ERF_Notified;
}

/*****************************************************************************
** Surface: NewOwner()
*/

static ERROR SURFACE_NewOwner(extSurface *Self, struct acNewOwner *Args)
{
   if ((!Self->ParentDefined) and (!Self->initialised())) {
      OBJECTID owner_id = Args->NewOwnerID;
      while ((owner_id) and (GetClassID(owner_id) != ID_SURFACE)) {
         owner_id = GetOwnerID(owner_id);
      }
      if (owner_id) Self->ParentID = owner_id;
      else Self->ParentID = NULL;
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR SURFACE_NewObject(extSurface *Self, APTR Void)
{
   Self->LeftLimit   = -1000000000;
   Self->RightLimit  = -1000000000;
   Self->TopLimit    = -1000000000;
   Self->BottomLimit = -1000000000;
   Self->MaxWidth    = 16777216;
   Self->MaxHeight   = 16777216;
   Self->MinWidth    = 1;
   Self->MinHeight   = 1;
   Self->Opacity  = 255;
   Self->RootID   = Self->UID;
   Self->WindowType  = glpWindowType;
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
RemoveCallback: Removes a callback previously inserted by AddCallback().

The RemoveCallback() method is used to remove any callback that has been previously inserted by #AddCallback().

This method is scope restricted, meaning that callbacks added by other objects will not be affected irrespective of
the parameters that are passed to it.

-INPUT-
ptr(func) Callback: Pointer to the callback routine to remove, or NULL to remove all assoicated callback routines.

-ERRORS-
Okay
Search
-END-

*****************************************************************************/

static ERROR SURFACE_RemoveCallback(extSurface *Self, struct drwRemoveCallback *Args)
{
   parasol::Log log;
   OBJECTPTR context = NULL;

   if (Args) {
      if ((Args->Callback) and (Args->Callback->Type IS CALL_STDC)) {
         context = (OBJECTPTR)Args->Callback->StdC.Context;
         log.trace("Context: %d, Routine %p, Current Total: %d", context->UID, Args->Callback->StdC.Routine, Self->CallbackCount);
      }
      else log.trace("Current Total: %d", Self->CallbackCount);
   }
   else log.trace("Current Total: %d [Remove All]", Self->CallbackCount);

   if (!context) context = GetParentContext();

   if (!Self->Callback) return ERR_Okay;

   if ((!Args) or (!Args->Callback) or (Args->Callback->Type IS CALL_NONE)) {
      // Remove everything relating to this context if no callback was specified.

      LONG i;
      LONG shrink = 0;
      for (i=0; i < Self->CallbackCount; i++) {
         if (Self->Callback[i].Object IS context) {
            shrink--;
            continue;
         }
         if (shrink) Self->Callback[i+shrink] = Self->Callback[i];
      }
      Self->CallbackCount += shrink;
      return ERR_Okay;
   }

   if (Args->Callback->Type IS CALL_SCRIPT) {
      UnsubscribeAction(Args->Callback->Script.Script, AC_Free);
   }

   // Find the callback entry, then shrink the list.

   LONG i;
   for (i=0; i < Self->CallbackCount; i++) {
      //log.msg("  %d: #%d, Routine %p", i, Self->Callback[i].Object->UID, Self->Callback[i].Function.StdC.Routine);

      if ((Self->Callback[i].Function.Type IS CALL_STDC) and
          (Self->Callback[i].Function.StdC.Context IS context) and
          (Self->Callback[i].Function.StdC.Routine IS Args->Callback->StdC.Routine)) break;

      if ((Self->Callback[i].Function.Type IS CALL_SCRIPT) and
          (Self->Callback[i].Function.Script.Script IS context) and
          (Self->Callback[i].Function.Script.ProcedureID IS Args->Callback->Script.ProcedureID)) break;
   }

   if (i < Self->CallbackCount) {
      while (i < Self->CallbackCount-1) {
         Self->Callback[i] = Self->Callback[i+1];
         i++;
      }
      Self->CallbackCount--;
      return ERR_Okay;
   }
   else {
      if (Args->Callback->Type IS CALL_STDC) log.warning("Unable to find callback for #%d, routine %p", context->UID, Args->Callback->StdC.Routine);
      else log.warning("Unable to find callback for #%d", context->UID);
      return ERR_Search;
   }
}

/*****************************************************************************

-METHOD-
ResetDimensions: Changes the dimensions of a surface.

The ResetDimensions method provides a simple way of re-declaring the dimensions of a surface object.  This is sometimes
necessary when a surface needs to make a significant alteration to its method of display.  For instance if the width of
the surface is declared through a combination of X and XOffset settings and the width needs to change to a fixed
setting, then ResetDimensions will have to be used.

It is not necessary to define a value for every parameter - only the ones that are relevant to the new dimension
settings.  For instance if X and Width are set, XOffset is ignored and the Dimensions value must include DMF_FIXED_X
and DMF_FIXED_WIDTH (or the relative equivalents).  Please refer to the #Dimensions field for a full list of
dimension flags that can be specified.

-INPUT-
double X: New X coordinate.
double Y: New Y coordinate.
double XOffset: New X offset.
double YOffset: New Y offset.
double Width: New width.
double Height: New height.
int(DMF) Dimensions: Dimension flags.

-ERRORS-
Okay
NullArgs
AccessMemoryID: Unable to access internal surface list.
-END-

*****************************************************************************/

static ERROR SURFACE_ResetDimensions(extSurface *Self, struct drwResetDimensions *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   log.branch("%.0f,%.0f %.0fx%.0f %.0fx%.0f, Flags: $%.8x", Args->X, Args->Y, Args->XOffset, Args->YOffset, Args->Width, Args->Height, Args->Dimensions);

   if (!Args->Dimensions) return log.warning(ERR_NullArgs);

   LONG dimensions = Args->Dimensions;

   Self->Dimensions = dimensions;

   LONG cx = Self->X;
   LONG cy = Self->Y;
   LONG cx2 = Self->X + Self->Width;
   LONG cy2 = Self->Y + Self->Height;

   // Turn off drawing and adjust the dimensions of the surface

   //gfxForbidDrawing();

   if (dimensions & DMF_RELATIVE_X) SetField(Self, FID_X|TDOUBLE|TREL, Args->X);
   else if (dimensions & DMF_FIXED_X) SetField(Self, FID_X|TDOUBLE, Args->X);

   if (dimensions & DMF_RELATIVE_Y) SetField(Self, FID_Y|TDOUBLE|TREL, Args->Y);
   else if (dimensions & DMF_FIXED_Y) SetField(Self, FID_Y|TDOUBLE, Args->Y);

   if (dimensions & DMF_RELATIVE_X_OFFSET) SetField(Self, FID_XOffset|TDOUBLE|TREL, Args->XOffset);
   else if (dimensions & DMF_FIXED_X_OFFSET) SetField(Self, FID_XOffset|TDOUBLE, Args->XOffset);

   if (dimensions & DMF_RELATIVE_Y_OFFSET) SetField(Self, FID_YOffset|TDOUBLE|TREL, Args->YOffset);
   else if (dimensions & DMF_FIXED_Y_OFFSET) SetField(Self, FID_YOffset|TDOUBLE, Args->YOffset);

   if (dimensions & DMF_RELATIVE_HEIGHT) SetField(Self, FID_Height|TDOUBLE|TREL, Args->Height);
   else if (dimensions & DMF_FIXED_HEIGHT) SetField(Self, FID_Height|TDOUBLE, Args->Height);

   if (dimensions & DMF_RELATIVE_WIDTH) SetField(Self, FID_Width|TDOUBLE|TREL, Args->Width);
   else if (dimensions & DMF_FIXED_WIDTH) SetField(Self, FID_Width|TDOUBLE, Args->Width);

   //gfxPermitDrawing();

   // Now redraw everything within the area that was adjusted

   LONG nx = Self->X;
   LONG ny = Self->Y;
   LONG nx2 = Self->X + Self->Width;
   LONG ny2 = Self->Y + Self->Height;
   if (cx < nx) nx = cx;
   if (cy < ny) ny = cy;
   if (cx2 > nx2) nx2 = cx2;
   if (cy2 > ny2) ny2 = cy2;

   SurfaceControl *ctl;
   if ((ctl = gfxAccessList(ARF_READ))) {
      LONG index;
      auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
      if ((index = find_surface_index(ctl, Self->ParentID ? Self->ParentID : Self->UID)) != -1) {
         _redraw_surface(Self->ParentID, list, index, ctl->Total, nx, ny, nx2-nx, ny2-ny, IRF_RELATIVE);
         _expose_surface(Self->ParentID, list, index, ctl->Total, nx, ny, nx2-nx, ny2-ny, 0);
      }

      gfxReleaseList(ARF_READ);
      return ERR_Okay;
   }
   else return log.warning(ERR_AccessMemory);
}

/*****************************************************************************

-METHOD-
ScheduleRedraw: Schedules a redraw operation for the next frame.

Use ScheduleRedraw to indicate that a surface needs to be drawn to the display.  The surface and all child surfaces
will be drawn on the next frame cycle (typically 1/60th of a second).  All manual draw operations for the target
surface are ignored until the scheduled operation is completed.

Scheduling is ideal in situations where a cluster of redraw events may occur within a tight time period, and it
would be inefficient to draw those changes to the display individually.

Note that redraw schedules do not 'see each other', meaning if a surface and a child are both scheduled, this will
trigger two redraw operations when one would suffice.  It is the client's responsibility to target the most
relevant top-level surface for scheduling.

-ERRORS-
Okay
-END-

*****************************************************************************/

static ERROR SURFACE_ScheduleRedraw(extSurface *Self, APTR Void)
{
   // TODO Currently defaults to 60FPS, we should get the correct FPS from the Display object.
   #define FPS 60.0

   if (Self->RedrawScheduled) return ERR_Okay;

   if (Self->RedrawTimer) {
      Self->RedrawScheduled = TRUE;
      return ERR_Okay;
   }

   auto call = make_function_stdc(redraw_timer);
   if (!SubscribeTimer(1.0/FPS, &call, &Self->RedrawTimer)) {
      Self->RedrawCountdown = FPS * 30;
      Self->RedrawScheduled = TRUE;
      return ERR_Okay;
   }
   else return ERR_Failed;
}

/*****************************************************************************

-ACTION-
SaveImage: Saves the graphical image of a surface object.

If you need to store the image (graphical content) of a surface object, use the SaveImage action.  Calling SaveImage on
a surface object will cause it to generate an image of its contents and save them to the given destination object.  Any
child surfaces in the region will also be included in the resulting image data.

The image data will be saved in the data format that is indicated by the setting in the ClassID argument.  Options are
limited to members of the @Picture class, for example `ID_JPEG` and `ID_PICTURE` (PNG).  If no ClassID is specified,
the user's preferred default file format is used.
-END-

*****************************************************************************/

static ERROR SURFACE_SaveImage(extSurface *Self, struct acSaveImage *Args)
{
   parasol::Log log;
   LONG i, j, level;

   if (!Args) return log.warning(ERR_NullArgs);

   log.branch();

   // Create a Bitmap that is the same size as the rendered area

   CLASSID class_id;
   if (!Args->ClassID) class_id = ID_PICTURE;
   else class_id = Args->ClassID;

   OBJECTPTR picture;
   if (!NewObject(class_id, &picture)) {
      picture->set(FID_Flags, "NEW");
      picture->set(FID_Width, Self->Width);
      picture->set(FID_Height, Self->Height);

      objDisplay *display;
      objBitmap *video_bmp;
      if (!access_video(Self->DisplayID, &display, &video_bmp)) {
         picture->set(FID_BitsPerPixel, video_bmp->BitsPerPixel);
         picture->set(FID_BytesPerPixel, video_bmp->BytesPerPixel);
         picture->set(FID_Type, video_bmp->Type);
         release_video(display);
      }

      if (!acInit(picture)) {
         // Scan through the surface list and copy each buffer to our picture

         SurfaceControl *ctl;
         if ((ctl = gfxAccessList(ARF_READ))) {
            auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);

            if ((i = find_own_index(ctl, Self)) != -1) {
               OBJECTID bitmapid = NULL;
               for (j=i; (j < ctl->Total) and ((j IS i) or (list[j].Level > list[i].Level)); j++) {
                  if ((!(list[j].Flags & RNF_VISIBLE)) or (list[j].Flags & RNF_CURSOR)) {
                     // Skip this surface area and all invisible children
                     level = list[j].Level;
                     while (list[j+1].Level > level) j++;
                     continue;
                  }

                  // If the bitmaps are different, we have found something new to copy

                  if (list[j].BitmapID != bitmapid) {
                     bitmapid = list[j].BitmapID;

                     extBitmap *picbmp;
                     picture->getPtr(FID_Bitmap, &picbmp);
                     gfxCopySurface(list[j].SurfaceID, picbmp, NULL, 0, 0, list[j].Width, list[j].Height,
                        list[j].Left - list[i].Left, list[j].Top - list[i].Top);
                  }
               }
            }

            gfxReleaseList(ARF_READ);

            if (!Action(AC_SaveImage, picture, Args)) { // Save the picture to disk
               acFree(picture);
               return ERR_Okay;
            }
         }
      }

      acFree(picture);
      return log.warning(ERR_Failed);
   }
   else return log.warning(ERR_NewObject);
}

/*****************************************************************************

-ACTION-
Scroll: Scrolls surface content to a new position.

Calling the Scroll action on a surface object with the `SCROLL_CONTENT` flag set will cause it to move its contents in the
requested direction.  The Surface class uses the Move action to achieve scrolling, so any objects that do not support
Move will remain at their given position.  Everything else will be shifted by the same amount of units as specified in
the DeltaX, DeltaY and DeltaZ arguments.

Some objects may support a 'sticky' field that can be set to TRUE to prevent them from being moved.  This feature is
present in the Surface class, amongst others.

If the surface object does not have the `SCROLL_CONTENT` flag set, the call will flow through to any objects that may be
listening for the Scroll action on the surface.
-END-

*****************************************************************************/

static ERROR SURFACE_Scroll(extSurface *Self, struct acScroll *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Self->Flags & RNF_SCROLL_CONTENT) {
      if ((Args->DeltaX >= 1) or (Args->DeltaX <= -1) or (Args->DeltaY >= 1) or (Args->DeltaY <= -1)) {
         SurfaceControl *ctl;
         if ((ctl = gfxAccessList(ARF_READ))) {
            OBJECTID surfaces[128];
            auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
            LONG i;
            LONG t = 0;
            if ((i = find_own_index(ctl, Self)) != -1) {
               // Send a move command to each child surface
               LONG level = list[i].Level + 1;
               for (++i; list[i].Level >= level; i++) {
                  if (list[i].Level IS level) surfaces[t++] = list[i].SurfaceID;
               }
            }

            gfxReleaseList(ARF_READ);

            struct acMove move = { -Args->DeltaX, -Args->DeltaY, -Args->DeltaZ };
            for (LONG i=0; i < t; i++) QueueAction(AC_Move, surfaces[i], &move);
         }
      }
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
ScrollToPoint: Moves the content of a surface object to a specific point.
-END-
*****************************************************************************/

static ERROR SURFACE_ScrollToPoint(extSurface *Self, struct acScrollToPoint *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Self->Flags & RNF_SCROLL_CONTENT) {
      SurfaceControl *ctl;
      if ((ctl = gfxAccessList(ARF_READ))) { // Find our object
         OBJECTID surfaces[128];
         LONG i;
         LONG t = 0;
         auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
         if ((i = find_own_index(ctl, Self)) != -1) {
            LONG level = list[i].Level + 1;
            for (++i; list[i].Level >= level; i++) {
               if (list[i].Level IS level) surfaces[t++] = list[i].SurfaceID;
            }
         }

         gfxReleaseList(ARF_READ);

         struct acMoveToPoint move = { -Args->X, -Args->Y, -Args->Z, Args->Flags };
         for (i=0; i < t; i++) QueueAction(AC_MoveToPoint, surfaces[i], &move);
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
SetOpacity: Alters the opacity of a surface object.

This method will change the opacity of the surface and execute a redraw to make the changes to the display.

-INPUT-
double Value: The new opacity value between 0 and 100% (ignored if you have set the Adjustment parameter).
double Adjustment: Adjustment value to add or subtract from the existing opacity (set to zero if you want to set a fixed Value instead).

-ERRORS-
Okay: The opacity of the surface object was changed.
NullArgs

*****************************************************************************/

static ERROR SURFACE_SetOpacity(extSurface *Self, struct drwSetOpacity *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if (Self->BitmapOwnerID != Self->UID) {
      log.warning("Opacity cannot be set on a surface that does not own its bitmap.");
      return ERR_NoSupport;
   }

   DOUBLE value;
   if (Args->Adjustment) {
      value = (Self->Opacity * 100 / 255) + Args->Adjustment;
      SET_Opacity(Self, value);
   }
   else {
      value = Args->Value;
      SET_Opacity(Self, value);
   }

   // Use the QueueAction() feature so that we don't end up with major lag problems when SetOpacity is being used for things like fading.

   if (Self->Flags & RNF_VISIBLE) QueueAction(MT_DrwInvalidateRegion, Self->UID);

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Show: Shows a surface object on the display.
-END-
*****************************************************************************/

static ERROR SURFACE_Show(extSurface *Self, APTR Void)
{
   parasol::Log log;

   log.traceBranch("%dx%d, %dx%d, Parent: %d, Modal: %d", Self->X, Self->Y, Self->Width, Self->Height, Self->ParentID, Self->Modal);

   LONG notified;
   if (Self->Flags & RNF_VISIBLE) {
      notified = ERF_Notified;
      return ERR_Okay|ERF_Notified;
   }
   else notified = 0;

   if (!Self->ParentID) {
      if (!acShow(Self->DisplayID)) {
         Self->Flags |= RNF_VISIBLE;
         if (Self->Flags & RNF_HAS_FOCUS) acFocus(Self->DisplayID);
      }
      else return log.warning(ERR_Failed);
   }
   else Self->Flags |= RNF_VISIBLE;

   if (Self->Modal) Self->PrevModalID = gfxSetModalSurface(Self->UID);

   if (!notified) {
      UpdateSurfaceField(Self, &SurfaceList::Flags, Self->Flags);

      gfxRedrawSurface(Self->UID, 0, 0, Self->Width, Self->Height, IRF_RELATIVE);
      gfxExposeSurface(Self->UID, 0, 0, Self->Width, Self->Height, EXF_CHILDREN|EXF_REDRAW_VOLATILE_OVERLAP);
   }

   refresh_pointer(Self);

   return ERR_Okay|notified;
}

//****************************************************************************

static ERROR redraw_timer(extSurface *Self, LARGE Elapsed, LARGE CurrentTime)
{
   if (Self->RedrawScheduled) {
      Self->RedrawScheduled = FALSE; // Done before Draw() because it tests this field.
      Action(AC_Draw, Self, NULL);
   }
   else {
      // Rather than unsubscribe from the timer immediately, we hold onto it until the countdown reaches zero.  This
      // is because there is a noticeable performance penalty if you frequently subscribe and unsubscribe from the timer
      // system.
      if (Self->RedrawCountdown > 0) Self->RedrawCountdown--;
      if (!Self->RedrawCountdown) {
         Self->RedrawTimer = NULL;
         return ERR_Terminate;
      }
   }

   return ERR_Okay;
}

//****************************************************************************

static void draw_region(extSurface *Self, extSurface *Parent, extBitmap *Bitmap)
{
   // Only region objects can respond to draw messages

   if (!(Self->Flags & RNF_TRANSPARENT)) return;

   // If the surface object is invisible, return immediately

   if (!(Self->Flags & RNF_VISIBLE)) return;

   if ((Self->Width < 1) or (Self->Height < 1)) return;

   if ((Self->X > Bitmap->Clip.Right) or (Self->Y > Bitmap->Clip.Bottom) or
       (Self->X + Self->Width <= Bitmap->Clip.Left) or
       (Self->Y + Self->Height <= Bitmap->Clip.Top)) {
      return;
   }

   // Take a copy of the current clipping and offset values

   ClipRectangle clip = Bitmap->Clip;
   LONG xoffset = Bitmap->XOffset;
   LONG yoffset = Bitmap->YOffset;

   // Adjust clipping and offset values to match the absolute coordinates of our surface object

   Bitmap->XOffset += Self->X;
   Bitmap->YOffset += Self->Y;

   // Adjust the clipping region of our parent so that it is relative to our surface area

   Bitmap->Clip.Left   -= Self->X;
   Bitmap->Clip.Top    -= Self->Y;
   Bitmap->Clip.Right  -= Self->X;
   Bitmap->Clip.Bottom -= Self->Y;

   // Make sure that the clipping values do not extend outside of our area

   if (Bitmap->Clip.Left < 0) Bitmap->Clip.Left = 0;
   if (Bitmap->Clip.Top < 0)  Bitmap->Clip.Top = 0;
   if (Bitmap->Clip.Right > Self->Width)   Bitmap->Clip.Right = Self->Width;
   if (Bitmap->Clip.Bottom > Self->Height) Bitmap->Clip.Bottom = Self->Height;

   if ((Bitmap->Clip.Left < Bitmap->Clip.Right) and (Bitmap->Clip.Top < Bitmap->Clip.Bottom)) {
      // Clear the Bitmap to the background colour if necessary

      if (Self->Colour.Alpha > 0) {
         gfxDrawRectangle(Bitmap, 0, 0, Self->Width, Self->Height, Bitmap->packPixel(Self->Colour, 255), TRUE);
      }

      process_surface_callbacks(Self, Bitmap);
   }

   Bitmap->Clip    = clip;
   Bitmap->XOffset = xoffset;
   Bitmap->YOffset = yoffset;
}

//****************************************************************************

static ERROR consume_input_events(const InputEvent *Events, LONG Handle)
{
   parasol::Log log(__FUNCTION__);

   auto Self = (extSurface *)CurrentContext();

   static DOUBLE glAnchorX = 0, glAnchorY = 0; // Anchoring is process-exclusive, so we can store the coordinates as global variables

   for (auto event=Events; event; event=event->Next) {
      // Process events that support consolidation first.

      if (event->Flags & (JTYPE_ANCHORED|JTYPE_MOVEMENT)) {
         SurfaceControl *ctl;
         DOUBLE xchange, ychange;
         LONG dragindex;

         // Dragging support

         if (Self->DragStatus) { // Consolidate movement changes
            if (Self->DragStatus IS DRAG_ANCHOR) {
               xchange = event->X;
               ychange = event->Y;
               while ((event->Next) and (event->Next->Flags & JTYPE_ANCHORED)) {
                  event = event->Next;
                  xchange += event->X;
                  ychange += event->Y;
               }
            }
            else {
               while ((event->Next) and (event->Next->Flags & JTYPE_MOVEMENT)) {
                  event = event->Next;
               }

               DOUBLE absx = event->AbsX - glAnchorX;
               DOUBLE absy = event->AbsY - glAnchorY;

               xchange = 0;
               ychange = 0;
               if ((ctl = gfxAccessList(ARF_READ))) {
                  auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
                  if ((dragindex = find_surface_index(ctl, Self->UID)) != -1) {
                     xchange = absx - list[dragindex].Left;
                     ychange = absy - list[dragindex].Top;
                  }
                  gfxReleaseList(ARF_READ);
               }
            }

            // Move the dragging surface to the new location

            if ((Self->DragID) and (Self->DragID != Self->UID)) {
               acMove(Self->DragID, xchange, ychange, 0);
            }
            else {
               LONG sticky = Self->Flags & RNF_STICKY;
               Self->Flags &= ~RNF_STICKY; // Turn off the sticky flag, as it prevents movement

               acMove(Self, xchange, ychange, 0);

               if (sticky) {
                  Self->Flags |= RNF_STICKY;
                  UpdateSurfaceField(Self, &SurfaceList::Flags, Self->Flags); // (Required to put back the sticky flag)
               }
            }

            // The new pointer position is based on the position of the surface that's being dragged.

            if (Self->DragStatus IS DRAG_ANCHOR) {
               if ((ctl = gfxAccessList(ARF_READ))) {
                  auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
                  if ((dragindex = find_surface_index(ctl, Self->UID)) != -1) {
                     DOUBLE absx = list[dragindex].Left + glAnchorX;
                     DOUBLE absy = list[dragindex].Top + glAnchorY;
                     gfxReleaseList(ARF_READ);

                     gfxSetCursorPos(absx, absy);
                  }
                  else gfxReleaseList(ARF_READ);
               }
            }
         }
      }
      else if ((event->Type IS JET_LMB) and (!(event->Flags & JTYPE_REPEATED))) {
         if (event->Value > 0) {
            if (Self->Flags & RNF_DISABLED) continue;

            // Anchor the pointer position if dragging is enabled

            if ((Self->DragID) and (Self->DragStatus IS DRAG_NONE)) {
               log.trace("Dragging object %d; Anchored to %dx%d", Self->DragID, event->X, event->Y);

               // Ask the pointer to anchor itself to our surface.  If the left mouse button is released, the
               // anchor will be released by the pointer automatically.

               glAnchorX  = event->X;
               glAnchorY  = event->Y;
               if (!gfxLockCursor(Self->UID)) {
                  Self->DragStatus = DRAG_ANCHOR;
               }
               else Self->DragStatus = DRAG_NORMAL;
            }
         }
         else { // Click released
            if (Self->DragStatus) {
               gfxUnlockCursor(Self->UID);
               Self->DragStatus = DRAG_NONE;
            }
         }
      }
   }

   return ERR_Okay;
}

//****************************************************************************

#include "surface_drawing.cpp"
#include "surface_fields.cpp"
#include "surface_dimensions.cpp"
#include "surface_resize.cpp"

//****************************************************************************

static const FieldDef MovementFlags[] = {
   { "Vertical",   MOVE_VERTICAL },
   { "Horizontal", MOVE_HORIZONTAL },
   { NULL, 0 }
};

static const FieldDef clWindowType[] = { // This table is copied from pointer_class.c
   { "Default",  SWIN_HOST },
   { "Host",     SWIN_HOST },
   { "Taskbar",  SWIN_TASKBAR },
   { "IconTray", SWIN_ICON_TRAY },
   { "None",     SWIN_NONE },
   { NULL, 0 }
};

static const FieldDef clTypeFlags[] = {
   { "Root", RT_ROOT },
   { NULL, 0 }
};

#include "surface_def.c"

static const FieldArray clSurfaceFields[] = {
   { "Drag",         FDF_OBJECTID|FDF_RW,  ID_SURFACE, NULL, (APTR)SET_Drag },
   { "Buffer",       FDF_OBJECTID|FDF_R,   ID_BITMAP,  NULL, NULL },
   { "Parent",       FDF_OBJECTID|FDF_RW,  ID_SURFACE, NULL, (APTR)SET_Parent },
   { "PopOver",      FDF_OBJECTID|FDF_RI,  0, NULL, (APTR)SET_PopOver },
   { "TopMargin",    FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "BottomMargin", FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_BottomMargin },
   { "LeftMargin",   FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "RightMargin",  FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_RightMargin },
   { "MinWidth",     FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_MinWidth },
   { "MinHeight",    FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_MinHeight },
   { "MaxWidth",     FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_MaxWidth },
   { "MaxHeight",    FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_MaxHeight },
   { "LeftLimit",    FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_LeftLimit },
   { "RightLimit",   FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_RightLimit },
   { "TopLimit",     FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_TopLimit },
   { "BottomLimit",  FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_BottomLimit },
   { "Display",      FDF_OBJECTID|FDF_R,   ID_DISPLAY, NULL, NULL },
   { "Flags",        FDF_LONGFLAGS|FDF_RW, (MAXINT)&clSurfaceFlags, NULL, (APTR)SET_Flags },
   { "X",            FD_VARIABLE|FDF_LONG|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_XCoord, (APTR)SET_XCoord },
   { "Y",            FD_VARIABLE|FDF_LONG|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_YCoord, (APTR)SET_YCoord },
   { "Width",        FD_VARIABLE|FDF_LONG|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_Width,  (APTR)SET_Width },
   { "Height",       FD_VARIABLE|FDF_LONG|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_Height, (APTR)SET_Height },
   { "RootLayer",    FDF_OBJECTID|FDF_RW,   0, NULL, (APTR)SET_RootLayer },
   { "Align",        FDF_LONGFLAGS|FDF_RW,  (MAXINT)&clSurfaceAlign, NULL, NULL },
   { "Dimensions",   FDF_LONG|FDF_RW,       (MAXINT)&clSurfaceDimensions, NULL, (APTR)SET_Dimensions },
   { "DragStatus",   FDF_LONG|FDF_LOOKUP|FDF_R,  (MAXINT)&clSurfaceDragStatus, NULL, NULL },
   { "Cursor",       FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clSurfaceCursor, NULL, (APTR)SET_Cursor },
   { "Colour",       FDF_RGB|FDF_RW,        0, NULL, NULL },
   { "Type",         FDF_SYSTEM|FDF_LONG|FDF_RI, (MAXINT)&clTypeFlags, NULL, NULL },
   { "Modal",        FDF_LONG|FDF_RW,       0, NULL, (APTR)SET_Modal },
   // Virtual fields
   { "AbsX",          FDF_VIRTUAL|FDF_LONG|FDF_RW,     0,         (APTR)GET_AbsX,           (APTR)SET_AbsX },
   { "AbsY",          FDF_VIRTUAL|FDF_LONG|FDF_RW,     0,         (APTR)GET_AbsY,           (APTR)SET_AbsY },
   { "BitsPerPixel",  FDF_VIRTUAL|FDF_LONG|FDF_RI,     0,         (APTR)GET_BitsPerPixel,   (APTR)SET_BitsPerPixel },
   { "Bottom",        FDF_VIRTUAL|FDF_LONG|FDF_R,      0,         (APTR)GET_Bottom,         NULL },
   { "InsideHeight",  FDF_VIRTUAL|FDF_LONG|FDF_RW,     0,         (APTR)GET_InsideHeight,   (APTR)SET_InsideHeight },
   { "InsideWidth",   FDF_VIRTUAL|FDF_LONG|FDF_RW,     0,         (APTR)GET_InsideWidth,    (APTR)SET_InsideWidth },
   { "Movement",      FDF_VIRTUAL|FDF_LONGFLAGS|FDF_RW,(MAXINT)&MovementFlags, NULL,        (APTR)SET_Movement },
   { "Opacity",       FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,   0,         (APTR)GET_Opacity,        (APTR)SET_Opacity },
   { "PrecopyRegion", FDF_VIRTUAL|FDF_STRING|FDF_W,    0,         NULL,                     (APTR)SET_PrecopyRegion },
   { "RevertFocus",   FDF_SYSTEM|FDF_VIRTUAL|FDF_OBJECTID|FDF_W, 0, NULL, (APTR)SET_RevertFocus },
   { "Right",         FDF_VIRTUAL|FDF_LONG|FDF_R,      0,         (APTR)GET_Right,          NULL },
   { "UserFocus",     FDF_VIRTUAL|FDF_LONG|FDF_R,      0,         (APTR)GET_UserFocus,      NULL },
   { "Visible",       FDF_VIRTUAL|FDF_LONG|FDF_RW,     0,         (APTR)GET_Visible,        (APTR)SET_Visible },
   { "VisibleHeight", FDF_VIRTUAL|FDF_LONG|FDF_R,      0,         (APTR)GET_VisibleHeight,  NULL },
   { "VisibleWidth",  FDF_VIRTUAL|FDF_LONG|FDF_R,      0,         (APTR)GET_VisibleWidth,   NULL },
   { "VisibleX",      FDF_VIRTUAL|FDF_LONG|FDF_R,      0,         (APTR)GET_VisibleX,       NULL },
   { "VisibleY",      FDF_VIRTUAL|FDF_LONG|FDF_R,      0,         (APTR)GET_VisibleY,       NULL },
   { "WindowType",    FDF_VIRTUAL|FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clWindowType, (APTR)GET_WindowType, (APTR)SET_WindowType },
   { "WindowHandle",  FDF_VIRTUAL|FDF_POINTER|FDF_RW,  0,         (APTR)GET_WindowHandle, (APTR)SET_WindowHandle },
   // Variable fields
   { "XOffset",       FDF_VIRTUAL|FDF_VARIABLE|FDF_LONG|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_XOffset, (APTR)SET_XOffset },
   { "YOffset",       FDF_VIRTUAL|FDF_VARIABLE|FDF_LONG|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_YOffset, (APTR)SET_YOffset },
   END_FIELD
};

//****************************************************************************

ERROR create_surface_class(void)
{
   clSurface = objMetaClass::create::global(
      fl::ClassVersion(VER_SURFACE),
      fl::Name("Surface"),
      fl::Category(CCF_GUI),
      fl::Actions(clSurfaceActions),
      fl::Methods(clSurfaceMethods),
      fl::Fields(clSurfaceFields),
      fl::Size(sizeof(extSurface)),
      fl::Path(MOD_PATH));

   return clSurface ? ERR_Okay : ERR_AddClass;
}
