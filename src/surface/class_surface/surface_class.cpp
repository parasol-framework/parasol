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

// Set Functions

static ERROR SET_Opacity(objSurface *, DOUBLE);
static ERROR SET_XOffset(objSurface *, Variable *);
static ERROR SET_YOffset(objSurface *, Variable *);

// Movement Flags

#define MOVE_VERTICAL   0x0001
#define MOVE_HORIZONTAL 0x0002

static ERROR consume_input_events(const InputEvent *, LONG);
static void draw_region(objSurface *, objSurface *, objBitmap *);
static ERROR scroll_timer(objSurface *, LARGE, LARGE);

//****************************************************************************
// Handler for the display being resized.

static void display_resized(OBJECTID DisplayID, LONG X, LONG Y, LONG Width, LONG Height)
{
   OBJECTID surface_id = GetOwnerID(DisplayID);
   objSurface *surface;
   if (!AccessObject(surface_id, 4000, &surface)) {
      if (surface->Head.ClassID IS ID_SURFACE) {
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

//****************************************************************************

static ERROR SURFACE_ActionNotify(objSurface *Self, struct acActionNotify *NotifyArgs)
{
   parasol::Log log;

   if ((Self->Head.Flags & (NF_FREE_MARK|NF_FREE))) { // Do nothing if the surface is being terminated.
      return ERR_Okay;
   }

   if (NotifyArgs->ActionID IS AC_Free) {
      if (NotifyArgs->ObjectID IS Self->ProgramID) {
         // Terminate if our linked task has disappeared
         acFree(Self);
      }
      else if (NotifyArgs->ObjectID IS Self->ParentID) {
         // Free ourselves in advance if our parent is in the process of being killed.  This causes a chain reaction
         // that results in a clean deallocation of the surface hierarchy.

         Self->Flags &= ~RNF_VISIBLE;
         UpdateSurfaceField(Self, Flags);
         if (Self->Head.Flags & NF_INTEGRAL) DelayMsg(AC_Free, Self->Head.UniqueID, NULL); // If the object is a child of something, give the parent object time to do the deallocation itself
         else acFree(Self);
      }
      else {
         for (WORD i=0; i < Self->CallbackCount; i++) {
            if (Self->Callback[i].Function.Type IS CALL_SCRIPT) {
               if (Self->Callback[i].Function.Script.Script->UniqueID IS NotifyArgs->ObjectID) {
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
   }
   else if ((NotifyArgs->ActionID IS AC_Draw) and (NotifyArgs->Error IS ERR_Okay)) {
      // Hosts will sometimes call Draw to indicate that the display has been exposed.

      if (NotifyArgs->ObjectID IS Self->DisplayID) {
         auto draw = (struct acDraw *)NotifyArgs->Args;

         log.traceBranch("Display exposure received - redrawing display.");

         if (draw) {
            struct drwExpose expose = { draw->X, draw->Y, draw->Width, draw->Height, EXF_CHILDREN };
            Action(MT_DrwExpose, Self, &expose);
         }
         else {
            struct drwExpose expose = { 0, 0, 20000, 20000, EXF_CHILDREN };
            Action(MT_DrwExpose, Self, &expose);
         }
      }
   }
   else if ((NotifyArgs->ActionID IS AC_Redimension) and (NotifyArgs->Error IS ERR_Okay)) {
      auto resize = (struct acRedimension *)NotifyArgs->Args;

      if (Self->Document) return ERR_Okay;

      log.traceBranch("Redimension notification from parent #%d, currently %dx%d,%dx%d.", Self->ParentID, Self->X, Self->Y, Self->Width, Self->Height);

      // Get the width and height of our parent surface

      DOUBLE parentwidth, parentheight, width, height, x, y;

      if (Self->ParentID) {
         SurfaceControl *ctl;
         if ((ctl = drwAccessList(ARF_READ))) {
            auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
            LONG i;
            for (i=0; (i < ctl->Total) and (list[i].SurfaceID != Self->ParentID); i++);
            if (i >= ctl->Total) {
               drwReleaseList(ARF_READ);
               return log.warning(ERR_Search);
            }
            parentwidth  = list[i].Width;
            parentheight = list[i].Height;
            drwReleaseList(ARF_READ);
         }
         else return log.warning(ERR_AccessMemory);
      }
      else {
         DISPLAYINFO *display;
         if (!gfxGetDisplayInfo(0, &display)) {
            parentwidth  = display->Width;
            parentheight = display->Height;
         }
         else return ERR_Okay;
      }

      // Convert relative offsets to their fixed equivalent

      if (Self->Dimensions & DMF_RELATIVE_X_OFFSET) Self->XOffset = (parentwidth * Self->XOffsetPercent) / 100.0;
      if (Self->Dimensions & DMF_RELATIVE_Y_OFFSET) Self->YOffset = (parentheight * Self->YOffsetPercent) / 100.0;

      // Calculate absolute width and height values

      if (Self->Dimensions & DMF_RELATIVE_WIDTH)   width = parentwidth * Self->WidthPercent / 100.0;
      else if (Self->Dimensions & DMF_FIXED_WIDTH) width = Self->Width;
      else if (Self->Dimensions & DMF_X_OFFSET) {
         if (Self->Dimensions & DMF_FIXED_X) {
            width = parentwidth - Self->X - Self->XOffset;
         }
         else if (Self->Dimensions & DMF_RELATIVE_X) {
            width = parentwidth - (parentwidth * Self->XPercent / 100.0) - Self->XOffset;
         }
         else width = parentwidth - Self->XOffset;
      }
      else width = Self->Width;

      if (Self->Dimensions & DMF_RELATIVE_HEIGHT)   height = parentheight * Self->HeightPercent / 100.0;
      else if (Self->Dimensions & DMF_FIXED_HEIGHT) height = Self->Height;
      else if (Self->Dimensions & DMF_Y_OFFSET) {
         if (Self->Dimensions & DMF_FIXED_Y) {
            height = parentheight - Self->Y - Self->YOffset;
         }
         else if (Self->Dimensions & DMF_RELATIVE_Y) {
            height = parentheight - (parentheight * Self->YPercent / 100.0) - Self->YOffset;
         }
         else height = parentheight - Self->YOffset;
      }
      else height = Self->Height;

      // Calculate new coordinates

      if (Self->Dimensions & DMF_RELATIVE_X) x = (parentwidth * Self->XPercent / 100.0);
      else if (Self->Dimensions & DMF_X_OFFSET) x = parentwidth - Self->XOffset - width;
      else x = Self->X;

      if (Self->Dimensions & DMF_RELATIVE_Y) y = (parentheight * Self->YPercent / 100.0);
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

      if ((Self->X != x) or (Self->Y != y) or (Self->Width != width) or (Self->Height != height) or (resize->Depth)) {
         acRedimension(Self, x, y, 0, width, height, resize->Depth);
      }
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Activate: Shows a surface object on the display.
-END-
*****************************************************************************/

static ERROR SURFACE_Activate(objSurface *Self, APTR Void)
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

static ERROR SURFACE_AddCallback(objSurface *Self, struct drwAddCallback *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   OBJECTPTR context = GetParentContext();
   OBJECTPTR call_context = NULL;
   if (Args->Callback->Type IS CALL_STDC) call_context = (OBJECTPTR)Args->Callback->StdC.Context;
   else if (Args->Callback->Type IS CALL_SCRIPT) call_context = context; // Scripts use runtime ID resolution...

   if (context->UniqueID < 0) {
      log.warning("Public objects may not draw directly to surfaces.");
      return ERR_Failed;
   }

   log.msg("Context: %d, Callback Context: %d, Routine: %p (Count: %d)", context->UniqueID, call_context ? call_context->UniqueID : 0, Args->Callback->StdC.Routine, Self->CallbackCount);

   if (call_context) context = call_context;

   if (Self->Head.TaskID != CurrentTaskID()) return log.warning(ERR_ExecViolation);

   if (Self->Callback) {
      // Check if the subscription is already on the list for our surface context.

      WORD i;
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
         if (!AllocMemory(sizeof(SurfaceCallback) * new_size, MEM_DATA|MEM_NO_CLEAR, &scb, NULL)) {
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

   if (Args->Callback->Type IS CALL_SCRIPT) SubscribeAction(Args->Callback->Script.Script, AC_Free);

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Disable: Disables a surface object.
-END-
*****************************************************************************/

static ERROR SURFACE_Disable(objSurface *Self, APTR Void)
{
   Self->Flags |= RNF_DISABLED;
   UpdateSurfaceField(Self, Flags);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Enable: Enables a disabled surface object.
-END-
*****************************************************************************/

static ERROR SURFACE_Enable(objSurface *Self, APTR Void)
{
   Self->Flags &= ~RNF_DISABLED;
   UpdateSurfaceField(Self, Flags);
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
      drwRedrawSurface(*SurfaceID, 0, 0, 4096, 4096, RNF_TOTAL_REDRAW);
      drwExposeSurface(*SurfaceID, 0, 0, 4096, 4096, EXF_CHILDREN);
   }
}

/*****************************************************************************
** Event: user.login
*/

static void event_user_login(objSurface *Self, APTR Info, LONG InfoSize)
{
   parasol::Log log;

   log.function("User login detected - resetting screen mode.");

   OBJECTPTR config;
   if (!CreateObject(ID_CONFIG, NF_INTEGRAL, &config, FID_Path|TSTR, "user:config/display.cfg", TAGEND)) {
      OBJECTPTR object;
      CSTRING str;

      DOUBLE refreshrate = -1.0;
      LONG depth         = 32;
      DOUBLE gammared    = 1.0;
      DOUBLE gammagreen  = 1.0;
      DOUBLE gammablue   = 1.0;
      LONG width         = Self->Width;
      LONG height        = Self->Height;

      cfgReadInt(config, "DISPLAY", "Width", &width);
      cfgReadInt(config, "DISPLAY", "Height", &height);
      cfgReadInt(config, "DISPLAY", "Depth", &depth);
      cfgReadFloat(config, "DISPLAY", "RefreshRate", &refreshrate);
      cfgReadFloat(config, "DISPLAY", "GammaRed", &gammared);
      cfgReadFloat(config, "DISPLAY", "GammaGreen", &gammagreen);
      cfgReadFloat(config, "DISPLAY", "GammaBlue", &gammablue);

      if (!cfgReadValue(config, "DISPLAY", "DPMS", &str)) {
         if (!AccessObject(Self->DisplayID, 3000, &object)) {
            SetString(object, FID_DPMS, str);
            ReleaseObject(object);
         }
      }

      if (width < 640) width = 640;
      if (height < 480) height = 480;

      struct drwSetDisplay setdisplay = {
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

      acFree(config);
   }
}

/*****************************************************************************
-ACTION-
Focus: Changes the primary user focus to the surface object.
-END-
*****************************************************************************/

static LARGE glLastFocusTime = 0;

static ERROR SURFACE_Focus(objSurface *Self, APTR Void)
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
      acFocusID(Self->ParentID);
      glLastFocusTime = PreciseTime();
      return ERR_Okay|ERF_Notified;
   }

   if (Self->Flags & RNF_NO_FOCUS) {
      FOCUSMSG("Focus cancelled (NO_FOCUS flag set).");
      glLastFocusTime = PreciseTime();
      return ERR_Okay|ERF_Notified;
   }

   FOCUSMSG("Focussing...  HasFocus: %c", (Self->Flags & RNF_HAS_FOCUS) ? 'Y' : 'N');

   OBJECTID modal;
   if ((modal = drwGetModalSurface(Self->Head.TaskID))) {
      if (modal != Self->Head.UniqueID) {
         ERROR error;
         error = drwCheckIfChild(modal, Self->Head.UniqueID);

         if ((error != ERR_True) and (error != ERR_LimitedSuccess)) {
            // Focussing is not OK - surface is out of the modal's scope
            log.warning("Surface #%d is not within modal #%d's scope.", Self->Head.UniqueID, modal);
            glLastFocusTime = PreciseTime();
            return ERR_Failed|ERF_Notified;
         }
      }
   }

   OBJECTID *focuslist;
   if (!AccessMemory(RPM_FocusList, MEM_READ_WRITE, 1000, &focuslist)) {
      // Return immediately if this surface object already has the -primary- focus

      if ((Self->Flags & RNF_HAS_FOCUS) and (focuslist[0] IS Self->Head.UniqueID)) {
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
      if ((ctl = drwAccessList(ARF_READ))) {
         auto surfacelist = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);

         LONG surface_index;
         OBJECTID surface_id = Self->Head.UniqueID;
         if ((surface_index = find_own_index(ctl, Self)) IS -1) {
            // This is not a critical failure as child surfaces can be expected to disappear from the surface list
            // during the free process.

            drwReleaseList(ARF_READ);
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

         drwReleaseList(ARF_READ);
      }
      else {
         ReleaseMemory(focuslist);
         glLastFocusTime = PreciseTime();
         return log.warning(ERR_AccessMemory);
      }

      // Send a Focus action to all parent surface objects in our generated focus list.

      struct drwInheritedFocus inherit = {
         .FocusID = Self->Head.UniqueID,
         .Flags   = Self->Flags
      };
      for (LONG i=1; focuslist[i]; i++) { // Start from one to skip Self
         ActionMsg(MT_DrwInheritedFocus, focuslist[i], &inherit);
      }

      // Send out LostFocus actions to all objects that do not intersect with the new focus chain.

      for (LONG i=0; lostfocus[i]; i++) {
         acLostFocusID(lostfocus[i]);
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
         UpdateSurfaceField(Self, Flags);

         // Focussing on the display window is important in hosted environments

         if (Self->DisplayID) acFocusID(Self->DisplayID);

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

static ERROR SURFACE_Free(objSurface *Self, APTR Void)
{
   if (Self->ScrollTimer) { UpdateTimer(Self->ScrollTimer, 0); Self->ScrollTimer = 0; }

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
      objSurface *parent;
      ERROR error;
      if (!(error = AccessObject(Self->ParentID, 5000, &parent))) {
         UnsubscribeAction(parent, NULL);
         if (Self->Flags & (RNF_REGION|RNF_TRANSPARENT)) {
            drwRemoveCallback(parent, NULL);
         }
         ReleaseObject(parent);
      }
   }

   acHide(Self);

   // Remove any references to this surface object from the global surface list

   untrack_layer(Self->Head.UniqueID);

   if ((!Self->ParentID) and (Self->DisplayID)) {
      acFreeID(Self->DisplayID);
      Self->DisplayID = NULL;
   }

   if ((Self->BufferID) and ((!Self->BitmapOwnerID) or (Self->BitmapOwnerID IS Self->Head.UniqueID))) {
      if (Self->Bitmap) { ReleaseObject(Self->Bitmap); Self->Bitmap = NULL; }
      acFreeID(Self->BufferID);
      Self->BufferID = 0;
   }

   // Give the focus to the parent if our object has the primary focus.  Do not apply this technique to surface objects
   // acting as windows, as the window class has its own focus management code.

   if ((Self->Flags & RNF_HAS_FOCUS) and (GetClassID(Self->Head.OwnerID) != ID_WINDOW)) {
      if (Self->ParentID) acFocusID(Self->ParentID);
   }

   if (Self->Flags & RNF_AUTO_QUIT) {
      parasol::Log log;
      log.msg("Posting a quit message due to use of AUTOQUIT.");
      if ((Self->Head.TaskID IS Self->ProgramID) or (!Self->ProgramID)) {
         SendMessage(NULL, MSGID_QUIT, NULL, NULL, NULL);
      }
      else {
         ListTasks *list;
         if (!ListTasks(NULL, &list)) {
            for (LONG i=0; list[i].TaskID; i++) {
               if (list[i].TaskID IS Self->ProgramID) {
                  SendMessage(list[i].MessageID, MSGID_QUIT, NULL, NULL, NULL);
                  break;
               }
            }
            FreeResource(list);
         }
      }
   }

   if (Self->InputHandle) gfxUnsubscribeInput(Self->InputHandle);

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Hide: Hides a surface object from the display.
-END-
*****************************************************************************/

static ERROR SURFACE_Hide(objSurface *Self, APTR Void)
{
   parasol::Log log;

   log.traceBranch("");

   if (!(Self->Flags & RNF_VISIBLE)) return ERR_Okay|ERF_Notified;

   if (!Self->ParentID) {
      Self->Flags &= ~RNF_VISIBLE; // Important to switch off visibliity before Hide(), otherwise a false redraw will occur.
      UpdateSurfaceField(Self, Flags);

      if (acHideID(Self->DisplayID) != ERR_Okay) return ERR_Failed;
   }
   else {
      // Mark this surface object as invisible, then invalidate the region it was covering in order to have the background redrawn.

      Self->Flags &= ~RNF_VISIBLE;
      UpdateSurfaceField(Self, Flags);

      if (Self->Flags & RNF_REGION) {
         drwRedrawSurface(Self->ParentID, Self->X, Self->Y, Self->Width, Self->Height, IRF_RELATIVE);
         drwExposeSurface(Self->ParentID, Self->X, Self->Y, Self->Width, Self->Height, NULL);
      }
      else {
         if (Self->BitmapOwnerID != Self->Head.UniqueID) {
            drwRedrawSurface(Self->ParentID, Self->X, Self->Y, Self->Width, Self->Height, IRF_RELATIVE);
         }
         drwExposeSurface(Self->ParentID, Self->X, Self->Y, Self->Width, Self->Height, EXF_CHILDREN|EXF_REDRAW_VOLATILE);
      }
   }

   // Check if the surface is modal, if so, switch it off

   TaskList *task;
   if (Self->PrevModalID) {
      drwSetModalSurface(Self->PrevModalID);
      Self->PrevModalID = 0;
   }
   else if ((task = (TaskList *)GetResourcePtr(RES_TASK_CONTROL))) {
      if (task->ModalID IS Self->Head.UniqueID) {
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

static ERROR SURFACE_InheritedFocus(objSurface *Self, struct drwInheritedFocus *Args)
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

      NotifySubscribers(Self, AC_Focus, NULL, NULL, ERR_Okay);
      return ERR_Okay;
   }
}

//****************************************************************************

static ERROR SURFACE_Init(objSurface *Self, APTR Void)
{
   parasol::Log log;
   objBitmap *bitmap;

   BYTE require_store = FALSE;
   OBJECTID parent_bitmap = 0;
   OBJECTID bitmap_owner  = 0;

   if (!Self->RootID) Self->RootID = Self->Head.UniqueID;

   if (Self->Flags & RNF_CURSOR) Self->Flags |= RNF_STICK_TO_FRONT;

   // If no parent surface is set, check if the client has set the FULL_SCREEN flag.  If not, try to give the
   // surface a parent.

   if ((!Self->ParentID) and (glDisplayType IS DT_NATIVE)) {
      if (!(Self->Flags & RNF_FULL_SCREEN)) {
         LONG count = 1;
         if (FindObject("desktop", ID_SURFACE, FOF_INCLUDE_SHARED, &Self->ParentID, &count) != ERR_Okay) {
            SurfaceControl *ctl;
            if ((ctl = drwAccessList(ARF_READ))) {
               auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
               Self->ParentID = list[0].SurfaceID;
               drwReleaseList(ARF_READ);
            }
         }
      }
   }

   ERROR error;
   if (Self->ParentID) {
      objSurface *parent;
      if (AccessObject(Self->ParentID, 3000, &parent) != ERR_Okay) {
         log.warning("Failed to access parent #%d.", Self->ParentID);
         return ERR_AccessObject;
      }

      log.trace("Initialising surface to parent #%d.", Self->ParentID);

      error = ERR_Okay;

      if (Self->Flags & RNF_REGION) {
         // Regions must share the same task space with their parent.  If we can't meet this requirement, turn off the region flag.

         if (parent->Head.TaskID != CurrentTaskID()) {
            log.warning("Region cannot initialise to parent #%d - not in our task space.", Self->ParentID);
            Self->Flags &= ~RNF_REGION;
         }
      }

      // If the parent surface is a region, the child must also be a region or our drawing system will get confused.


      if ((parent->Flags & RNF_REGION) and (!(Self->Flags & RNF_REGION))) {
         Self->Flags |= RNF_REGION;
      }

      // If the parent has the ROOT flag set, we have to inherit whatever root layer that the parent is using, as well
      // as the PRECOPY and/or AFTERCOPY and opacity flags if they are set.

      if (parent->Type & RT_ROOT) { // The window class can set the ROOT type
         Self->Type |= RT_ROOT;
         if (Self->RootID IS Self->Head.UniqueID) {
            Self->InheritedRoot = TRUE;
            Self->RootID = parent->RootID; // Inherit the parent's root layer
         }
      }

      // Subscribe to the surface parent's Resize and Redimension actions

      SubscribeActionTags(parent, AC_Free, AC_Redimension, TAGEND);

      // If the surface object is a simple region, subscribe to the Draw action of the parent object.

      if (Self->Flags & (RNF_REGION|RNF_TRANSPARENT)) {
         FUNCTION func;
         struct drwAddCallback args = { &func };
         func.Type = CALL_STDC;
         func.StdC.Context = Self;
         func.StdC.Routine = (APTR)&draw_region;
         Action(MT_DrwAddCallback, parent, &args);

         if (Self->Flags & RNF_REGION) { // Turn off flags that should never be combined with regions.
            if (Self->Flags & RNF_PRECOPY) Self->Colour.Alpha = 0;
            Self->Flags &= ~(RNF_TRANSPARENT|RNF_AFTER_COPY|RNF_COMPOSITE);
         }
         else { // Turn off flags that should never be combined with transparent surfaces.
            Self->Flags &= ~(RNF_REGION|RNF_PRECOPY|RNF_AFTER_COPY|RNF_COMPOSITE);
            Self->Colour.Alpha = 0;
         }
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

      if (Self->Dimensions & DMF_FIXED_X_OFFSET)         SetLong(Self, FID_XOffset, Self->XOffset);
      else if (Self->Dimensions & DMF_RELATIVE_X_OFFSET) SetPercentage(Self, FID_XOffset, Self->XOffsetPercent);

      if (Self->Dimensions & DMF_FIXED_Y_OFFSET)         SetLong(Self, FID_YOffset, Self->YOffset);
      else if (Self->Dimensions & DMF_RELATIVE_Y_OFFSET) SetPercentage(Self, FID_YOffset, Self->YOffsetPercent);

      if (Self->Dimensions & DMF_RELATIVE_X)       SetPercentage(Self, FID_X, Self->XPercent);
      if (Self->Dimensions & DMF_RELATIVE_Y)       SetPercentage(Self, FID_Y, Self->YPercent);
      if (Self->Dimensions & DMF_RELATIVE_WIDTH)   SetPercentage(Self, FID_Width,  Self->WidthPercent);
      if (Self->Dimensions & DMF_RELATIVE_HEIGHT)  SetPercentage(Self, FID_Height, Self->HeightPercent);

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

      if (Self->Align & ALIGN_LEFT) { Self->X = 0; SetLong(Self, FID_X, Self->X); }
      else if (Self->Align & ALIGN_RIGHT) { Self->X = parent->Width - Self->Width; SetLong(Self, FID_X, Self->X); }
      else if (Self->Align & ALIGN_HORIZONTAL) { Self->X = (parent->Width - Self->Width) / 2; SetLong(Self, FID_X, Self->X); }

      if (Self->Align & ALIGN_TOP) { Self->Y = 0; SetLong(Self, FID_Y, Self->Y); }
      else if (Self->Align & ALIGN_BOTTOM) { Self->Y = parent->Height - Self->Height; SetLong(Self, FID_Y, Self->Y); }
      else if (Self->Align & ALIGN_VERTICAL) { Self->Y = (parent->Height - Self->Height) / 2; SetLong(Self, FID_Y, Self->Y); }

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

      Self->Flags &= ~(RNF_REGION|RNF_TRANSPARENT|RNF_PRECOPY|RNF_AFTER_COPY);

      LONG scrflags = 0;

      if (GetClassID(Self->Head.OwnerID) IS ID_WINDOW) {
         objWindow *window;
         gfxSetHostOption(HOST_TASKBAR, 1);
         gfxSetHostOption(HOST_TRAY_ICON, 0);
         if (!AccessObject(Self->Head.OwnerID, 4000, &window)) {
            if (window->Flags & WNF_BORDERLESS) scrflags |= SCR_BORDERLESS; // Stop the display from creating a window on host displays
            ReleaseObject(window);
         }
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
            log.trace("Enabling borderless icontray based surface.");
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

      if (glDisplayType IS DT_NATIVE) Self->Flags &= ~(RNF_COMPOSITE);

      if (((glDisplayType IS DT_WINDOWS) or (glDisplayType IS DT_X11)) and (Self->Flags & RNF_HOST)) {
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

      if (glDisplayType != DT_NATIVE) {
         // Alignment adjustments

         DISPLAYINFO *display;
         if (!gfxGetDisplayInfo(0, &display)) {
            if (Self->Align & ALIGN_LEFT) { Self->X = 0; SetLong(Self, FID_X, Self->X); }
            else if (Self->Align & ALIGN_RIGHT) { Self->X = display->Width - Self->Width; SetLong(Self, FID_X, Self->X); }
            else if (Self->Align & ALIGN_HORIZONTAL) { Self->X = (display->Width - Self->Width) / 2; SetLong(Self, FID_X, Self->X); }

            if (Self->Align & ALIGN_TOP) { Self->Y = 0; SetLong(Self, FID_Y, Self->Y); }
            else if (Self->Align & ALIGN_BOTTOM) { Self->Y = display->Height - Self->Height; SetLong(Self, FID_Y, Self->Y); }
            else if (Self->Align & ALIGN_VERTICAL) { Self->Y = (display->Height - Self->Height) / 2; SetLong(Self, FID_Y, Self->Y); }
         }
      }

      if (Self->Height < Self->MinHeight + Self->TopMargin  + Self->BottomMargin) Self->Height = Self->MinHeight + Self->TopMargin  + Self->BottomMargin;
      if (Self->Width  < Self->MinWidth  + Self->LeftMargin + Self->RightMargin)  Self->Width  = Self->MinWidth  + Self->LeftMargin + Self->RightMargin;
      if (Self->Height > Self->MaxHeight + Self->TopMargin  + Self->BottomMargin) Self->Height = Self->MaxHeight + Self->TopMargin  + Self->BottomMargin;
      if (Self->Width  > Self->MaxWidth  + Self->LeftMargin + Self->RightMargin)  Self->Width  = Self->MaxWidth  + Self->LeftMargin + Self->RightMargin;

      if (Self->Flags & RNF_STICK_TO_FRONT) gfxSetHostOption(HOST_STICK_TO_FRONT, 1);
      else gfxSetHostOption(HOST_STICK_TO_FRONT, 0);

      if (Self->Flags & RNF_COMPOSITE) scrflags |= SCR_COMPOSITE;

      CSTRING name;
      if (!CheckObjectNameExists("SystemDisplay")) name = NULL;
      else name = "SystemDisplay";

      // For hosted displays:  On initialisation, the X and Y fields reflect the position at which the window will be
      // opened on the host desktop.  However, hosted surfaces operate on the absolute coordinates of client regions
      // and are ignorant of window frames, so we read the X, Y fields back from the display after initialisation (the
      // display will adjust the coordinates to reflect the absolute position of the surface on the desktop).

      objDisplay *display;
      if (!NewLockedObject(ID_DISPLAY, NF_INTEGRAL|Self->Head.Flags, &display, &Self->DisplayID)) {
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
            objSurface *popsurface;
            if (!AccessObject(Self->PopOverID, 2000, &popsurface)) {
               OBJECTID pop_display = popsurface->DisplayID;
               ReleaseObject(popsurface);

               if (pop_display) SetLong(display, FID_PopOver, pop_display);
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

            GetPointer(display, FID_WindowHandle, &Self->DisplayWindow);

            #ifdef _WIN32
               winSetSurfaceID(Self->DisplayWindow, Self->Head.UniqueID);
            #endif

            // Subscribe to Redimension notifications if the display is hosted.  Also subscribe to Draw because this
            // can be used by the host to notify of window exposures.

            if (Self->DisplayWindow) {
               FUNCTION func = { .Type = CALL_STDC, .StdC = { .Context = NULL, .Routine = (APTR)&display_resized } };
               SetFunction(display, FID_ResizeFeedback, &func);
               SubscribeActionTags(display, AC_Draw, TAGEND);
            }

            error = ERR_Okay;
         }
         else error = ERR_Init;

         if (error) { acFree(display); Self->DisplayID = 0; }
         ReleaseObject(display);
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

      if ((require_store IS FALSE) and (Self->ParentID)) {
         MemInfo info;
         if (!MemoryIDInfo(Self->ParentID, &info)) {
            if (info.TaskID != CurrentTaskID()) require_store = TRUE;
         }
      }
   }

   if (Self->Flags & (RNF_REGION|RNF_TRANSPARENT)) require_store = FALSE;

   if (require_store) {
      Self->BitmapOwnerID = Self->Head.UniqueID;

      objDisplay *display;
      if (!(error = AccessObject(Self->DisplayID, 3000, &display))) {
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

         if (!(NewLockedObject(ID_BITMAP, NF_INTEGRAL|Self->Head.Flags, &bitmap, &Self->BufferID))) {
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
               Self->DataMID       = bitmap->DataMID;
               error = ERR_Okay;
            }
            else error = ERR_Init;

            if (error) { acFree(bitmap); Self->BufferID = 0; }

            ReleaseObject(bitmap);
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
      if (!AccessObject(Self->BufferID, 5000, &bitmap)) {
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
      if ((ctl = drwAccessList(ARF_READ))) {
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

         drwReleaseList(ARF_READ);
      }

      if (!Self->PopOverID) {
         log.warning("PopOver surface #%d is not a sibling of this surface.", popover_id);
         UpdateSurfaceField(Self, PopOverID);
      }
   }

   // Move the surface object to the back of the surface list when stick-to-back is enforced.

   if (Self->Flags & RNF_STICK_TO_BACK) acMoveToBack(Self);

   // Listen to the DeadTask event if we are a host surface object.  This will allow us to clean up the SurfaceList
   // when a task crashes.  Listening to the UserLogin event also allows us to switch to the user's preferred display
   // format on login.

   if ((!Self->ParentID) and (!StrMatch("SystemSurface", GetName(Self)))) {
      FUNCTION call;

      SET_FUNCTION_STDC(call, (APTR)event_task_removed);
      SubscribeEvent(EVID_SYSTEM_TASK_REMOVED, &call, &Self->Head.UniqueID, &Self->TaskRemovedHandle);

      SET_FUNCTION_STDC(call, (APTR)event_user_login);
      SubscribeEvent(EVID_USER_STATUS_LOGIN, &call, &Self->Head.UniqueID, &Self->UserLoginHandle);
   }

   if (!Self->ProgramID) Self->ProgramID = Self->Head.TaskID;
   else if (Self->ProgramID != Self->Head.TaskID) {
      OBJECTPTR task;
      if (!AccessObject(Self->ProgramID, 4000, &task)) {
         SubscribeActionTags(task, AC_Free, TAGEND);
         ReleaseObject(task);
      }
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
LostFocus: Informs a surface object that it has lost the user focus.
-END-
*****************************************************************************/

static ERROR SURFACE_LostFocus(objSurface *Self, APTR Void)
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
   // Drop the focus

   Self->Flags &= ~RNF_HAS_FOCUS;
   UpdateSurfaceField(Self, Flags);
   return ERR_Okay;
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

static ERROR SURFACE_Minimise(objSurface *Self, APTR Void)
{
   if (Self->DisplayID) ActionMsg(MT_GfxMinimise, Self->DisplayID, NULL);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Move: Moves a surface object to a new display position.
-END-
*****************************************************************************/

static ERROR SURFACE_Move(objSurface *Self, struct acMove *Args)
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
   if (!AccessMemory(GetResource(RES_MESSAGE_QUEUE), MEM_READ, 2000, &queue)) {
      LONG index = 0;
      UBYTE msgbuffer[sizeof(Message) + sizeof(ActionMessage) + sizeof(struct acMove)];
      while (!ScanMessages(queue, &index, MSGID_ACTION, msgbuffer, sizeof(msgbuffer))) {
         auto action = (ActionMessage *)(msgbuffer + sizeof(Message));

         if ((action->ActionID IS AC_MoveToPoint) and (action->ObjectID IS Self->Head.UniqueID)) {
            ReleaseMemory(queue);
            return ERR_Okay|ERF_Notified;
         }
         else if ((action->ActionID IS AC_Move) and (action->SendArgs IS TRUE) and
                  (action->ObjectID IS Self->Head.UniqueID)) {
            struct acMove *msgmove = (struct acMove *)(action + 1);
            msgmove->XChange += Args->XChange;
            msgmove->YChange += Args->YChange;
            msgmove->ZChange += Args->ZChange;

            UpdateMessage(queue, ((Message *)msgbuffer)->UniqueID, NULL, action, sizeof(ActionMessage) + sizeof(struct acMove));

            ReleaseMemory(queue);
            return ERR_Okay|ERF_Notified;
         }
      }
      ReleaseMemory(queue);
   }

   if (Self->Flags & RNF_STICKY) return ERR_Failed|ERF_Notified;

   LONG xchange = Args->XChange;
   LONG ychange = Args->YChange;

   if (Self->Flags & RNF_NO_HORIZONTAL) move.XChange = 0;
   else move.XChange = xchange;

   if (Self->Flags & RNF_NO_VERTICAL) move.YChange = 0;
   else move.YChange = ychange;

   move.ZChange = 0;

   // If there isn't any movement, return immediately

   if ((move.XChange < 1) and (move.XChange > -1) and (move.YChange < 1) and (move.YChange > -1)) {
      return ERR_Failed|ERF_Notified;
   }

   log.traceBranch("X,Y: %d,%d", xchange, ychange);

   SurfaceControl *ctl;

   // Margin/Limit handling

   if (!Self->ParentID) {
      move_layer(Self, Self->X + move.XChange, Self->Y + move.YChange);
   }
   else if ((ctl = drwAccessList(ARF_READ))) {
      auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
      if ((i = find_parent_index(ctl, Self)) != -1) {
         // Horizontal limit handling

         if (xchange < 0) {
            if ((Self->X + xchange) < Self->LeftLimit) {
               if (Self->X < Self->LeftLimit) move.XChange = 0;
               else move.XChange = -(Self->X - Self->LeftLimit);
            }
         }
         else if (xchange > 0) {
            if ((Self->X + Self->Width) > (list[i].Width - Self->RightLimit)) move.XChange = 0;
            else if ((Self->X + Self->Width + xchange) > (list[i].Width - Self->RightLimit)) {
               move.XChange = (list[i].Width - Self->RightLimit - Self->Width) - Self->X;
            }
         }

         // Vertical limit handling

         if (ychange < 0) {
            if ((Self->Y + ychange) < Self->TopLimit) {
               if ((Self->Y + Self->Height) < Self->TopLimit) move.YChange = 0;
               else move.YChange = -(Self->Y - Self->TopLimit);
            }
         }
         else if (ychange > 0) {
            if ((Self->Y + Self->Height) > (list[i].Height - Self->BottomLimit)) move.YChange = 0;
            else if ((Self->Y + Self->Height + ychange) > (list[i].Height - Self->BottomLimit)) {
               move.YChange = (list[i].Height - Self->BottomLimit - Self->Height) - Self->Y;
            }
         }

         // Second check: If there isn't any movement, return immediately

         if ((!move.XChange) and (!move.YChange)) {
            drwReleaseList(ARF_READ);
            return ERR_Failed|ERF_Notified;
         }
      }

      drwReleaseList(ARF_WRITE);

      // Move the graphics layer

      move_layer(Self, Self->X + move.XChange, Self->Y + move.YChange);
   }
   else return log.warning(ERR_LockFailed)|ERF_Notified;

/* These lines cause problems for the resizing of offset surface objects.
   if (Self->Dimensions & DMF_X_OFFSET) Self->XOffset += move.XChange;
   if (Self->Dimensions & DMF_Y_OFFSET) Self->YOffset += move.YChange;
*/

   log.traceBranch("Sending redimension notifications");
   struct acRedimension redimension = { (DOUBLE)Self->X, (DOUBLE)Self->Y, 0, (DOUBLE)Self->Width, (DOUBLE)Self->Height, 0 };
   NotifySubscribers(Self, AC_Redimension, &redimension, NULL, ERR_Okay);
   return ERR_Okay|ERF_Notified;
}

/*****************************************************************************
-ACTION-
MoveToBack: Moves a surface object to the back of its container.
-END-
*****************************************************************************/

static ERROR SURFACE_MoveToBack(objSurface *Self, APTR Void)
{
   parasol::Log log;

   if (!Self->ParentID) {
      acMoveToBackID(Self->DisplayID);
      return ERR_Okay|ERF_Notified;
   }

   log.branch("%s", GetName(Self));

   SurfaceControl *ctl;
   if ((ctl = drwAccessList(ARF_WRITE))) {
      auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);

      WORD index; // Get our position within the chain
      if ((index = find_surface_list(list, ctl->Total, Self->Head.UniqueID)) IS -1) {
         drwReleaseList(ARF_WRITE);
         return log.warning(ERR_Search)|ERF_Notified;
      }

      OBJECTID parent_bitmap;
      WORD i;
      if ((i = find_parent_index(ctl, Self)) != -1) parent_bitmap = list[i].BitmapID;
      else parent_bitmap = 0;

      // Find the position in the list that our surface object will be moved to

      WORD pos = index;
      WORD level = list[index].Level;
      for (i=index-1; (i >= 0) and (list[i].Level >= level); i--) {
         if (list[i].Level IS level) {
            if (Self->BitmapOwnerID IS Self->Head.UniqueID) { // If we own an independent bitmap, we cannot move behind surfaces that are members of the parent region
               if (list[i].BitmapID IS parent_bitmap) break;
            }
            if (list[i].SurfaceID IS Self->PopOverID) break; // Do not move behind surfaces that we must stay in front of
            if (!(Self->Flags & RNF_STICK_TO_BACK) and (list[i].Flags & RNF_STICK_TO_BACK)) break;
            pos = i;
         }
      }

      if (pos >= index) {  // If the position is unchanged, return immediately
         drwReleaseList(ARF_READ);
         return ERR_Okay|ERF_Notified;
      }

      move_layer_pos(ctl, index, pos); // Reorder the list so that our surface object is inserted at the new position

      LONG total = ctl->Total;
      SurfaceList cplist[total];
      CopyMemory((BYTE *)ctl + ctl->ArrayIndex, cplist, sizeof(cplist[0]) * ctl->Total);

      drwReleaseList(ARF_READ);

      if (Self->Flags & RNF_VISIBLE) {
         // Redraw our background if we are volatile
         if (check_volatile(cplist, index)) _redraw_surface(Self->Head.UniqueID, cplist, pos, total, cplist[pos].Left, cplist[pos].Top, cplist[pos].Right, cplist[pos].Bottom, NULL);

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

static ERROR SURFACE_MoveToFront(objSurface *Self, APTR Void)
{
   parasol::Log log;
   OBJECTPTR parent;
   WORD currentindex, i;

   log.branch("%s", GetName(Self));

   if (!Self->ParentID) {
      acMoveToFrontID(Self->DisplayID);
      return ERR_Okay|ERF_Notified;
   }

   SurfaceControl *ctl;
   if (!(ctl = drwAccessList(ARF_WRITE))) {
      return log.warning(ERR_AccessMemory)|ERF_Notified;
   }

   auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);

   if ((currentindex = find_own_index(ctl, Self)) IS -1) {
      drwReleaseList(ARF_WRITE);
      return log.warning(ERR_Search)|ERF_Notified;
   }

   // Find the object in the list that our surface object will displace

   WORD index = currentindex;
   WORD level = list[currentindex].Level;
   for (i=currentindex+1; (list[i].Level >= list[currentindex].Level); i++) {
      if (list[i].Level IS level) {
         if (list[i].Flags & RNF_POINTER) break; // Do not move in front of the mouse cursor

         if (list[i].PopOverID IS Self->Head.UniqueID) {
            // A surface has been discovered that has to be in front of us.

            break;
         }

         if (Self->BitmapOwnerID != Self->Head.UniqueID) {
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
                  drwReleaseList(ARF_WRITE);
                  acMoveToFrontID(Self->PopOverID);
                  return ERR_Okay|ERF_Notified;
               }
               break;
            }
         }
      }

      drwReleaseList(ARF_WRITE);
      return ERR_Okay|ERF_Notified;
   }

   // Skip past the children that belong to the target object

   i = index;
   level = list[i].Level;
   while (list[i+1].Level > level) i++;

   // Count the number of children that have been assigned to our surface object.

   WORD total;
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

   drwReleaseList(ARF_WRITE);

   // If the surface object is a region, resubscribe to the Draw action to move our surface region to the front of the subscription list.

   if (Self->Flags & RNF_REGION) {
      if (!AccessObject(Self->ParentID, 3000, &parent)) {
         FUNCTION func;
         SET_FUNCTION_STDC(func, (APTR)&draw_region);
         struct drwAddCallback args = { &func };
         Action(MT_DrwAddCallback, parent, &args);
         ReleaseObject(parent);
      }
   }

   if (Self->Flags & RNF_VISIBLE) {
      // A redraw is required for:
      //   Any volatile regions that were in front of our surface prior to the move-to-front (by moving to the front, their background has been changed).
      //   Areas of our surface that were obscured by surfaces that also shared our bitmap space.

      objBitmap *bitmap;
      if (!AccessObject(Self->BufferID, 5000, &bitmap)) {
         invalidate_overlap(Self, cplist, total, currentindex, i, cplist[i].Left, cplist[i].Top, cplist[i].Right, cplist[i].Bottom, bitmap);
         ReleaseObject(bitmap);
      }

      if (check_volatile(cplist, i)) _redraw_surface(Self->Head.UniqueID, cplist, i, total, 0, 0, Self->Width, Self->Height, IRF_RELATIVE);
      _expose_surface(Self->Head.UniqueID, cplist, i, total, 0, 0, Self->Width, Self->Height, EXF_CHILDREN|EXF_REDRAW_VOLATILE_OVERLAP);
   }

   if (Self->PopOverID) {
      // Check if the surface that we're popped over is right behind us.  If not, move it forward.

      for (LONG i=index-1; i > 0; i--) {
         if (cplist[i].Level IS level) {
            if (cplist[i].SurfaceID != Self->PopOverID) {
               acMoveToFrontID(Self->PopOverID);
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

static ERROR SURFACE_MoveToPoint(objSurface *Self, struct acMoveToPoint *Args)
{
   if (Args->Flags & MTF_ANIM) {
      Self->ScrollToX   = (Args->Flags & MTF_X) ? F2I(Args->X) : 0;
      Self->ScrollToY   = (Args->Flags & MTF_Y) ? F2I(Args->Y) : 0;
      Self->ScrollFromX = Self->X;
      Self->ScrollFromY = Self->Y;
      Self->ScrollProgress = 0;
      FUNCTION callback;
      SET_FUNCTION_STDC(callback, (APTR)&scroll_timer);
      SubscribeTimer(0.02, &callback, &Self->ScrollTimer);
      return ERR_Okay;
   }
   else {
      struct acMove move;

      if (Args->Flags & MTF_X) move.XChange = Args->X - Self->X;
      else move.XChange = 0;

      if (Args->Flags & MTF_Y) move.YChange = Args->Y - Self->Y;
      else move.YChange = 0;

      move.ZChange = 0;

      return Action(AC_Move, Self, &move)|ERF_Notified;
   }
}

/*****************************************************************************
** Surface: NewOwner()
*/

static ERROR SURFACE_NewOwner(objSurface *Self, struct acNewOwner *Args)
{
   if ((!Self->ParentDefined) and (!(Self->Head.Flags & NF_INITIALISED))) {
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

static ERROR SURFACE_NewObject(objSurface *Self, APTR Void)
{
   Self->LeftLimit   = -1000000000;
   Self->RightLimit  = -1000000000;
   Self->TopLimit    = -1000000000;
   Self->BottomLimit = -1000000000;
   Self->MaxWidth    = 16777216;
   Self->MaxHeight   = 16777216;
   Self->MinWidth    = 1;
   Self->MinHeight   = 1;
   Self->Frame       = 1;
   Self->ScrollSpeed = 5;
   Self->Opacity  = 255;
   Self->RootID   = Self->Head.UniqueID;
   Self->ProgramID   = Self->Head.TaskID;
   Self->WindowType  = glpWindowType;
   return ERR_Okay;
}

//****************************************************************************

static ERROR SURFACE_ReleaseObject(objSurface *Self, APTR Void)
{
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

static ERROR SURFACE_RemoveCallback(objSurface *Self, struct drwRemoveCallback *Args)
{
   parasol::Log log;
   OBJECTPTR context = NULL;

   if (Args) {
      if ((Args->Callback) and (Args->Callback->Type IS CALL_STDC)) {
         context = (OBJECTPTR)Args->Callback->StdC.Context;
         log.trace("Context: %d, Routine %p, Current Total: %d", context->UniqueID, Args->Callback->StdC.Routine, Self->CallbackCount);
      }
      else log.trace("Current Total: %d", Self->CallbackCount);
   }
   else log.trace("Current Total: %d [Remove All]", Self->CallbackCount);

   if (!context) context = GetParentContext();

   if (!Self->Callback) return ERR_Okay;

   if ((!Args) or (!Args->Callback) or (Args->Callback->Type IS CALL_NONE)) {
      // Remove everything relating to this context if no callback was specified.

      WORD i;
      WORD shrink = 0;
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

   WORD i;
   for (i=0; i < Self->CallbackCount; i++) {
      //log.msg("  %d: #%d, Routine %p", i, Self->Callback[i].Object->UniqueID, Self->Callback[i].Function.StdC.Routine);

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
      if (Args->Callback->Type IS CALL_STDC) log.warning("Unable to find callback for #%d, routine %p", context->UniqueID, Args->Callback->StdC.Routine);
      else log.warning("Unable to find callback for #%d", context->UniqueID);
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
AccessMemory: Unable to access internal surface list.
-END-

*****************************************************************************/

static ERROR SURFACE_ResetDimensions(objSurface *Self, struct drwResetDimensions *Args)
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

   drwForbidDrawing();

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

   drwPermitDrawing();

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
   if ((ctl = drwAccessList(ARF_READ))) {
      LONG index;
      auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
      if ((index = find_surface_index(ctl, Self->ParentID ? Self->ParentID : Self->Head.UniqueID)) != -1) {
         _redraw_surface(Self->ParentID, list, index, ctl->Total, nx, ny, nx2-nx, ny2-ny, IRF_RELATIVE);
         _expose_surface(Self->ParentID, list, index, ctl->Total, nx, ny, nx2-nx, ny2-ny, 0);
      }

      drwReleaseList(ARF_READ);
      return ERR_Okay;
   }
   else return log.warning(ERR_AccessMemory);
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

static ERROR SURFACE_SaveImage(objSurface *Self, struct acSaveImage *Args)
{
   parasol::Log log;
   WORD i, j, level;

   if (!Args) return log.warning(ERR_NullArgs);

   log.branch();

   // Create a Bitmap that is the same size as the rendered area

   CLASSID class_id;
   if (!Args->ClassID) class_id = ID_PICTURE;
   else class_id = Args->ClassID;

   OBJECTPTR picture;
   if (!NewObject(class_id, 0, &picture)) {
      SetString(picture, FID_Flags, "NEW");
      SetLong(picture, FID_Width, Self->Width);
      SetLong(picture, FID_Height, Self->Height);

      objDisplay *display;
      objBitmap *video_bmp;
      if (!access_video(Self->DisplayID, &display, &video_bmp)) {
         SetLong(picture, FID_BitsPerPixel, video_bmp->BitsPerPixel);
         SetLong(picture, FID_BytesPerPixel, video_bmp->BytesPerPixel);
         SetLong(picture, FID_Type, video_bmp->Type);
         release_video(display);
      }

      if (!acInit(picture)) {
         // Scan through the surface list and copy each buffer to our picture

         SurfaceControl *ctl;
         if ((ctl = drwAccessList(ARF_READ))) {
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
                     if (list[j].Flags & RNF_REGION) continue;

                     objBitmap *picbmp;
                     GetPointer(picture, FID_Bitmap, &picbmp);
                     drwCopySurface(list[j].SurfaceID, picbmp, NULL, 0, 0, list[j].Width, list[j].Height,
                        list[j].Left - list[i].Left, list[j].Top - list[i].Top);
                  }
               }
            }

            drwReleaseList(ARF_READ);

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
the XChange, YChange and ZChange arguments.

Some objects may support a 'sticky' field that can be set to TRUE to prevent them from being moved.  This feature is
present in the Surface class, amongst others.

If the surface object does not have the `SCROLL_CONTENT` flag set, the call will flow through to any objects that may be
listening for the Scroll action on the surface.
-END-

*****************************************************************************/

static ERROR SURFACE_Scroll(objSurface *Self, struct acScroll *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Self->Flags & RNF_SCROLL_CONTENT) {
      if ((Args->XChange >= 1) or (Args->XChange <= -1) or (Args->YChange >= 1) or (Args->YChange <= -1)) {
         SurfaceControl *ctl;
         if ((ctl = drwAccessList(ARF_READ))) {
            OBJECTID surfaces[128];
            auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
            LONG i;
            LONG t = 0;
            if ((i = find_own_index(ctl, Self)) != -1) {
               // Send a move command to each child surface
               WORD level = list[i].Level + 1;
               for (++i; list[i].Level >= level; i++) {
                  if (list[i].Level IS level) surfaces[t++] = list[i].SurfaceID;
               }
            }

            drwReleaseList(ARF_READ);

            struct acMove move = { -Args->XChange, -Args->YChange, -Args->ZChange };
            for (LONG i=0; i < t; i++) DelayMsg(AC_Move, surfaces[i], &move);
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

static ERROR SURFACE_ScrollToPoint(objSurface *Self, struct acScrollToPoint *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Self->Flags & RNF_SCROLL_CONTENT) {
      SurfaceControl *ctl;
      if ((ctl = drwAccessList(ARF_READ))) { // Find our object
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

         drwReleaseList(ARF_READ);

         struct acMoveToPoint move = { -Args->X, -Args->Y, -Args->Z, Args->Flags };
         for (i=0; i < t; i++) DelayMsg(AC_MoveToPoint, surfaces[i], &move);
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

static ERROR SURFACE_SetOpacity(objSurface *Self, struct drwSetOpacity *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if (Self->BitmapOwnerID != Self->Head.UniqueID) {
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

   // Use the DelayMsg() feature so that we don't end up with major lag problems when SetOpacity is being used for things like fading.

   if (Self->Flags & RNF_VISIBLE) DelayMsg(MT_DrwInvalidateRegion, Self->Head.UniqueID, NULL);

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Show: Shows a surface object on the display.
-END-
*****************************************************************************/

static ERROR SURFACE_Show(objSurface *Self, APTR Void)
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
      if (!acShowID(Self->DisplayID)) {
         Self->Flags |= RNF_VISIBLE;
         if (Self->Flags & RNF_HAS_FOCUS) acFocusID(Self->DisplayID);
      }
      else return log.warning(ERR_Failed);
   }
   else Self->Flags |= RNF_VISIBLE;

   if (Self->Modal) Self->PrevModalID = drwSetModalSurface(Self->Head.UniqueID);

   if (!notified) {
      UpdateSurfaceField(Self, Flags);

      if (Self->Flags & RNF_REGION) {
         drwRedrawSurface(Self->ParentID, Self->X, Self->Y, Self->Width, Self->Height, IRF_RELATIVE);
         drwExposeSurface(Self->ParentID, Self->X, Self->Y, Self->Width, Self->Height, NULL);
      }
      else {
         drwRedrawSurface(Self->Head.UniqueID, 0, 0, Self->Width, Self->Height, IRF_RELATIVE);
         drwExposeSurface(Self->Head.UniqueID, 0, 0, Self->Width, Self->Height, EXF_CHILDREN|EXF_REDRAW_VOLATILE_OVERLAP);
      }
   }

   refresh_pointer(Self);

   return ERR_Okay|notified;
}

//****************************************************************************

static ERROR scroll_timer(objSurface *Self, LARGE Elapsed, LARGE CurrentTime)
{
   if (Self->ScrollSpeed < 1) Self->ScrollSpeed = 1;
   else if (Self->ScrollSpeed > 30) Self->ScrollSpeed = 30;

   Self->ScrollProgress += Self->ScrollSpeed;
   if (Self->ScrollProgress > 100) Self->ScrollProgress = 100;

   LONG x = Self->ScrollFromX + (((Self->ScrollToX - Self->ScrollFromX) * Self->ScrollProgress) / 100);
   LONG y = Self->ScrollFromY + (((Self->ScrollToY - Self->ScrollFromY) * Self->ScrollProgress) / 100);

   x = x - Self->X;
   y = y - Self->Y;

   if ((x) or (y)) {
      acMove(Self, x, y, 0);

      if (Self->ScrollProgress >= 100) {
         Self->ScrollTimer = 0;
         Self->ScrollProgress = 0;
         return ERR_Terminate;
      }
   }
   else {
      Self->ScrollTimer = 0;
      Self->ScrollProgress = 0;
      return ERR_Terminate;
   }

   return ERR_Okay;
}

//****************************************************************************

static void draw_region(objSurface *Self, objSurface *Parent, objBitmap *Bitmap)
{
   // Only region objects can respond to draw messages

   if (!(Self->Flags & (RNF_REGION|RNF_TRANSPARENT))) return;

   // If the surface object is invisible, return immediately

   if (!(Self->Flags & RNF_VISIBLE)) return;

   if ((Self->Width < 1) or (Self->Height < 1)) return;

   if ((Self->X > Bitmap->Clip.Right) or (Self->Y > Bitmap->Clip.Bottom) or
       (Self->X + Self->Width <= Bitmap->Clip.Left) or
       (Self->Y + Self->Height <= Bitmap->Clip.Top)) {
      return;
   }

   // Take a copy of the current clipping and offset values

   struct ClipRectangle clip = Bitmap->Clip;
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
         gfxDrawRectangle(Bitmap, 0, 0, Self->Width, Self->Height, PackPixelA(Bitmap, Self->Colour.Red, Self->Colour.Green, Self->Colour.Blue, 255), TRUE);
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

   auto Self = (objSurface *)CurrentContext();

   static LONG glAnchorX = 0, glAnchorY = 0; // Anchoring is process-exclusive, so we can store the coordinates as global variables

   for (auto event=Events; event; event=event->Next) {
      // Process events that support consolidation first.

      if (event->Flags & (JTYPE_ANCHORED|JTYPE_MOVEMENT)) {
         SurfaceControl *ctl;
         LONG xchange, ychange, dragindex;

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

               LONG absx = event->AbsX - glAnchorX;
               LONG absy = event->AbsY - glAnchorY;

               xchange = 0;
               ychange = 0;
               if ((ctl = drwAccessList(ARF_READ))) {
                  auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
                  if ((dragindex = find_surface_index(ctl, Self->Head.UniqueID)) != -1) {
                     xchange = absx - list[dragindex].Left;
                     ychange = absy - list[dragindex].Top;
                  }
                  drwReleaseList(ARF_READ);
               }
            }

            // Move the dragging surface to the new location

            if ((Self->DragID) and (Self->DragID != Self->Head.UniqueID)) {
               acMoveID(Self->DragID, xchange, ychange, 0);
            }
            else {
               LONG sticky = Self->Flags & RNF_STICKY;
               Self->Flags &= ~RNF_STICKY; // Turn off the sticky flag, as it prevents movement

               acMove(Self, xchange, ychange, 0);

               if (sticky) {
                  Self->Flags |= RNF_STICKY;
                  UpdateSurfaceField(Self, Flags); // (Required to put back the sticky flag)
               }
            }

            // The new pointer position is based on the position of the surface that's being dragged.

            if (Self->DragStatus IS DRAG_ANCHOR) {
               if ((ctl = drwAccessList(ARF_READ))) {
                  auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
                  if ((dragindex = find_surface_index(ctl, Self->Head.UniqueID)) != -1) {
                     LONG absx = list[dragindex].Left + glAnchorX;
                     LONG absy = list[dragindex].Top + glAnchorY;
                     drwReleaseList(ARF_READ);

                     gfxSetCursorPos(absx, absy);
                  }
                  else drwReleaseList(ARF_READ);
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
               if (!gfxLockCursor(Self->Head.UniqueID)) {
                  Self->DragStatus = DRAG_ANCHOR;
               }
               else Self->DragStatus = DRAG_NORMAL;
            }
         }
         else { // Click released
            if (Self->DragStatus) {
               gfxUnlockCursor(Self->Head.UniqueID);
               Self->DragStatus = DRAG_NONE;
            }
         }
      }
   }

   return ERR_Okay;
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

static void invalidate_overlap(objSurface *Self, SurfaceList *list, WORD Total, LONG OldIndex, LONG Index,
   LONG Left, LONG Top, LONG Right, LONG Bottom, objBitmap *Bitmap)
{
   parasol::Log log(__FUNCTION__);
   LONG j;

   log.traceBranch("%dx%d %dx%d, Between %d to %d", Left, Top, Right-Left, Bottom-Top, OldIndex, Index);

   if ((list[Index].Flags & (RNF_REGION|RNF_TRANSPARENT)) or (!(list[Index].Flags & RNF_VISIBLE))) {
      return;
   }

   for (auto i=OldIndex; i < Index; i++) {
      // A redraw is required for:
      //   Any volatile regions that were in front of our surface prior to the move-to-front (by moving to the front, their background has been changed).
      //   Areas of our surface that were obscured by surfaces that also shared our bitmap space.

      if (!(list[i].Flags & RNF_VISIBLE)) goto skipcontent;
      if (list[i].Flags & RNF_REGION) goto skipcontent;
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

         _redraw_surface(Self->Head.UniqueID, list, i, Total, listx, listy, listright, listbottom, NULL);
      }

skipcontent:
      // Skip past any children of the overlapping object

      for (j=i+1; list[j].Level > list[i].Level; j++);
      i = j - 1;
   }
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
   { "Frame",        FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_Frame },
   { "Display",      FDF_OBJECTID|FDF_R,   ID_DISPLAY, NULL, NULL },
   { "Flags",        FDF_LONGFLAGS|FDF_RW, (MAXINT)&clSurfaceFlags, NULL, (APTR)SET_Flags },
   { "X",            FD_VARIABLE|FDF_LONG|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_XCoord, (APTR)SET_XCoord },
   { "Y",            FD_VARIABLE|FDF_LONG|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_YCoord, (APTR)SET_YCoord },
   { "Width",        FD_VARIABLE|FDF_LONG|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_Width,  (APTR)SET_Width },
   { "Height",       FD_VARIABLE|FDF_LONG|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_Height, (APTR)SET_Height },
   { "RootLayer",    FDF_OBJECTID|FDF_RW,   0, NULL, (APTR)SET_RootLayer },
   { "Program",      FDF_SYSTEM|FDF_LONG|FDF_RI, 0, NULL, NULL },
   { "Align",        FDF_LONGFLAGS|FDF_RW,  (MAXINT)&clSurfaceAlign, NULL, NULL },
   { "Dimensions",   FDF_LONG|FDF_RW,       (MAXINT)&clSurfaceDimensions, NULL, (APTR)SET_Dimensions },
   { "DragStatus",   FDF_LONG|FDF_LOOKUP|FDF_R,  (MAXINT)&clSurfaceDragStatus, NULL, NULL },
   { "Cursor",       FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clSurfaceCursor, NULL, (APTR)SET_Cursor },
   { "ScrollSpeed",  FDF_LONG|FDF_RW,       0, NULL, NULL },
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
   { "LayoutStyle",   FDF_VIRTUAL|FDF_SYSTEM|FDF_POINTER|FDF_W, 0, NULL,                    (APTR)SET_LayoutStyle },
   { "LayoutSurface", FDF_VIRTUAL|FDF_OBJECTID|FDF_R,  0,         (APTR)GET_LayoutSurface,  NULL },
   { "Movement",      FDF_VIRTUAL|FDF_LONGFLAGS|FDF_RW,(MAXINT)&MovementFlags, NULL,        (APTR)SET_Movement },
   { "Opacity",       FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,   0,         (APTR)GET_Opacity,        (APTR)SET_Opacity },
   { "PrecopyRegion", FDF_VIRTUAL|FDF_STRING|FDF_W,    0,         NULL,                     (APTR)SET_PrecopyRegion },
   { "Region",        FDF_VIRTUAL|FDF_LONG|FDF_RI,     0,         (APTR)GET_Region,         (APTR)SET_Region },
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

static ERROR create_surface_class(void)
{
   return CreateObject(ID_METACLASS, 0, &SurfaceClass,
      FID_ClassVersion|TDOUBLE, VER_SURFACE,
      FID_Name|TSTRING,   "Surface",
      FID_Category|TLONG, CCF_GUI,
      FID_Actions|TPTR,   clSurfaceActions,
      FID_Methods|TARRAY, clSurfaceMethods,
      FID_Fields|TARRAY,  clSurfaceFields,
      FID_Size|TLONG,     sizeof(objSurface),
      FID_Flags|TLONG,    glClassFlags,
      FID_Path|TSTR,      MOD_PATH,
      TAGEND);
}
