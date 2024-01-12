/*********************************************************************************************************************

-CLASS-
Pointer: Interface for mouse cursor support.

The Pointer class provides the user with a means of interacting with the graphical interface.  On a host system such
as Windows, the pointer functionality will hook into the host's capabilities.  If the display is native then the
pointer service will manage its own cursor exclusively.

Internally, a system-wide pointer object is automatically created with a name of `SystemPointer`.  This should be
used for all interactions with this service.

-END-

*********************************************************************************************************************/

#include "defs.h"

#ifdef _WIN32
using namespace display;
#endif

static ERROR GET_ButtonOrder(extPointer *, CSTRING *);
static ERROR GET_ButtonState(extPointer *, LONG *);

static ERROR SET_ButtonOrder(extPointer *, CSTRING);
static ERROR SET_MaxSpeed(extPointer *, LONG);
static ERROR PTR_SET_X(extPointer *, DOUBLE);
static ERROR PTR_SET_Y(extPointer *, DOUBLE);

#ifdef _WIN32
static ERROR PTR_SetWinCursor(extPointer *, struct ptrSetWinCursor *);
static FunctionField mthSetWinCursor[]  = { { "Cursor", FD_LONG }, { NULL, 0 } };
#endif

#ifdef __xwindows__
static ERROR PTR_GrabX11Pointer(extPointer *, struct ptrGrabX11Pointer *);
static ERROR PTR_UngrabX11Pointer(extPointer *, APTR);
static FunctionField mthGrabX11Pointer[] = { { "Surface", FD_LONG }, { NULL, 0 } };
#endif

static LONG glDefaultSpeed = 160;
static DOUBLE glDefaultAcceleration = 0.8;
static TIMER glRepeatTimer = 0;

static ERROR repeat_timer(extPointer *, LARGE, LARGE);
static void set_pointer_defaults(extPointer *);
static LONG examine_chain(extPointer *, LONG, SURFACELIST &, LONG);
static bool get_over_object(extPointer *);
static void process_ptr_button(extPointer *, struct dcDeviceInput *);
static void process_ptr_movement(extPointer *, struct dcDeviceInput *);
static void process_ptr_wheel(extPointer *, struct dcDeviceInput *);

//********************************************************************************************************************

inline void add_input(CSTRING Debug, InputEvent &input, JTYPE Flags, OBJECTID RecipientID, OBJECTID OverID,
   DOUBLE AbsX, DOUBLE AbsY, DOUBLE OverX, DOUBLE OverY)
{
   //pf::Log log(__FUNCTION__);
   //log.trace("Type: %s, Value: %.2f, Recipient: %d, Over: %d %.2fx%.2f, Abs: %.2fx%.2f %s",
   //   (input->Type < JET::END) ? glInputNames[input->Type] : (CSTRING)"", input->Value, RecipientID, OverID, OverX, OverY, AbsX, AbsY, Debug);

   input.Mask        = glInputType[LONG(input.Type)].Mask;
   input.Flags       = glInputType[LONG(input.Type)].Flags | Flags;
   input.RecipientID = RecipientID;
   input.OverID      = OverID;
   input.AbsX        = AbsX;
   input.AbsY        = AbsY;
   input.X           = OverX;
   input.Y           = OverY;

   const std::lock_guard<std::recursive_mutex> lock(glInputLock);
   glInputEvents.push_back(input);
}

//********************************************************************************************************************
#ifdef _WIN32
static ERROR PTR_SetWinCursor(extPointer *Self, struct ptrSetWinCursor *Args)
{
   winSetCursor(GetWinCursor(Args->Cursor));
   Self->CursorID = Args->Cursor;
   return ERR_Okay;
}
#endif

//********************************************************************************************************************
// Private action used to grab the window cursor under X11.  Can only be executed by the task that owns the pointer.

#ifdef __xwindows__
static ERROR PTR_GrabX11Pointer(extPointer *Self, struct ptrGrabX11Pointer *Args)
{
   APTR xwin;
   OBJECTPTR surface;

   if (!AccessObject(Self->SurfaceID, 5000, &surface)) {
      surface->getPtr(FID_WindowHandle, &xwin);
      ReleaseObject(surface);

      if (xwin) XGrabPointer(XDisplay, (Window)xwin, True, 0, GrabModeAsync, GrabModeAsync, (Window)xwin, None, CurrentTime);
   }

   return ERR_Okay;
}

static ERROR PTR_UngrabX11Pointer(extPointer *Self, APTR Void)
{
   XUngrabPointer(XDisplay, CurrentTime);
   return ERR_Okay;
}
#endif

/*********************************************************************************************************************

-ACTION-
DataFeed: This action can be used to send fake input to a pointer object.

Fake input can be sent to a pointer object with the `DATA::DEVICE_INPUT` data type, as if the user was using the mouse.
The data will be interpreted no differently to genuine user input from hardware.

Note that if a button click is used in a device input message, the client must follow up with the equivalent release
flag for that button.

-END-

*********************************************************************************************************************/

static ERROR PTR_DataFeed(extPointer *Self, struct acDataFeed *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if (Args->Datatype IS DATA::DEVICE_INPUT) {
      if (auto input = (struct dcDeviceInput *)Args->Buffer) {
         for (LONG i=0; i < ARRAYSIZE(Self->Buttons); i++) {
            if ((Self->Buttons[i].LastClicked) and (CheckObjectExists(Self->Buttons[i].LastClicked) != ERR_Okay)) Self->Buttons[i].LastClicked = 0;
         }

         for (LONG i=sizeof(struct dcDeviceInput); i <= Args->Size; i+=sizeof(struct dcDeviceInput), input++) {
            if ((LONG(input->Type) < 1) or (LONG(input->Type) >= LONG(JET::END))) continue;

            input->Flags = glInputType[LONG(input->Type)].Flags;

            //log.traceBranch("Incoming Input: %s, Value: %.2f, Flags: $%.8x, Time: %" PF64, (input->Type < JET::END) ? glInputNames[input->Type] : (STRING)"", input->Value, input->Flags, input->Timestamp);

            if (input->Type IS JET::WHEEL) process_ptr_wheel(Self, input);
            else if ((input->Flags & JTYPE::BUTTON) != JTYPE::NIL) process_ptr_button(Self, input);
            else process_ptr_movement(Self, input);
         }
      }
   }
   else return log.warning(ERR_WrongType);

   return ERR_Okay;
}

//********************************************************************************************************************

static void process_ptr_button(extPointer *Self, struct dcDeviceInput *Input)
{
   pf::Log log(__FUNCTION__);
   InputEvent userinput;
   OBJECTID target;
   LONG buttonflag, bi;

   ClearMemory(&userinput, sizeof(userinput));
   userinput.Value     = Input->Value;
   userinput.Timestamp = Input->Timestamp;
   userinput.Type      = Input->Type;
   userinput.Flags     = Input->Flags;
   userinput.DeviceID  = Input->DeviceID;

   if (!userinput.Timestamp) userinput.Timestamp = PreciseTime();

   auto uiflags = JTYPE::NIL;

   if ((userinput.Type >= JET::BUTTON_1) and (userinput.Type <= JET::BUTTON_10)) {
      bi = LONG(userinput.Type) - LONG(JET::BUTTON_1);
      buttonflag = Self->ButtonOrderFlags[bi];
   }
   else {
      // This subroutine is used when the button is not one of the regular 1-10 available button types

      add_input("IrregularButton", userinput, uiflags, Self->OverObjectID, Self->OverObjectID,
         Self->X, Self->Y, Self->OverX, Self->OverY);
      return;
   }

   if (userinput.Value <= 0) {
      // Button released.  Button releases are always reported relative to the object that received the original button press.
      // The surface immediately below the pointer does not receive any information about the release.

      log.trace("Button %d released.", bi);

      // Restore the cursor to its default state if cursor release flags have been met

      if ((Self->CursorRelease & buttonflag) and (Self->CursorOwnerID)) {
         gfxRestoreCursor(PTC::DEFAULT, 0);
      }

      if (Self->Buttons[bi].LastClicked) {
         LONG absx, absy;
         if (!get_surface_abs(Self->Buttons[bi].LastClicked, &absx, &absy, 0, 0)) {
            uiflags |= Self->DragSourceID ? JTYPE::DRAG_ITEM : JTYPE::NIL;

            if ((std::abs(Self->X - Self->LastReleaseX) > Self->ClickSlop) or
                (std::abs(Self->Y - Self->LastReleaseY) > Self->ClickSlop)) {
               uiflags |= JTYPE::DRAGGED;
            }

            if (Self->Buttons[bi].DblClick) {
               if ((uiflags & JTYPE::DRAGGED) IS JTYPE::NIL) uiflags |= JTYPE::DBL_CLICK;
            }

            add_input("ButtonRelease-LastClicked", userinput, uiflags, Self->Buttons[bi].LastClicked, Self->OverObjectID,
               Self->X, Self->Y, Self->X - absx, Self->Y - absy); // OverX/Y is reported relative to the click-held surface
         }
         Self->Buttons[bi].LastClicked = 0;
      }

      Self->LastReleaseX = Self->X;
      Self->LastReleaseY = Self->Y;
   }

   // Check for a modal surface.  The modal_id variable is set if a modal surface is active and the pointer is not
   // positioned over that surface (or its children).  The modal_id is therefore zero if the pointer is over the modal
   // surface, or if no modal surface is defined.

   auto modal_id = gfxGetModalSurface();

   if (modal_id) {
      if (modal_id IS Self->OverObjectID) {
         modal_id = 0;
      }
      else {
         // Check if the OverObject is one of the children of modal_id.

         ERROR error = gfxCheckIfChild(modal_id, Self->OverObjectID);
         if ((error IS ERR_True) or (error IS ERR_LimitedSuccess)) modal_id = 0;
      }
   }

   // Button Press Handler

   if (userinput.Value > 0) {
      log.trace("Button %d depressed @ %" PF64 " Coords: %.2fx%.2f", bi, userinput.Timestamp, Self->X, Self->Y);

      //if ((modal_id) and (modal_id != Self->OverObjectID)) {
      //   log.branch("Surface %d is modal, button click on %d cancelled.", modal_id, Self->OverObjectID);
      //   QueueAction(AC_MoveToFront, modal_id);
      //   QueueAction(AC_Focus, modal_id);
      //}

      //if (!modal_id) {
         // Before performing the click, we first check that there are no objects waiting for click-releases in the
         // designated fields.  If there are, we send them UserClickRelease() actions to retain system integrity.

         if (Self->Buttons[bi].LastClicked) {
            log.warning("Did not receive a release for button %d on surface #%d.", bi, Self->Buttons[bi].LastClicked);

            add_input("ButtonPress-ForceRelease", userinput, uiflags, Self->Buttons[bi].LastClicked, Self->OverObjectID,
               Self->X, Self->Y, Self->OverX, Self->OverY);
         }

         if (((DOUBLE)(userinput.Timestamp - Self->Buttons[bi].LastClickTime)) / 1000000.0 < Self->DoubleClick) {
            log.trace("Double click detected (under %.2fs)", Self->DoubleClick);
            Self->Buttons[bi].DblClick = TRUE;
            uiflags |= JTYPE::DBL_CLICK;
         }
         else Self->Buttons[bi].DblClick = FALSE;

         Self->Buttons[bi].LastClicked   = Self->OverObjectID;
         Self->Buttons[bi].LastClickTime = userinput.Timestamp;

         Self->LastClickX = Self->X;
         Self->LastClickY = Self->Y;

         // If a modal surface is active for the current process, the button press is reported to the modal surface only.

         if ((modal_id) and (modal_id != Self->OverObjectID)) {
            target = modal_id;
         }
         else target = Self->OverObjectID;

         QueueAction(AC_Focus, target);

         add_input("ButtonPress", userinput, uiflags, target, Self->OverObjectID,
            Self->X, Self->Y, Self->OverX, Self->OverY);
      //}

      auto call = make_function_stdc(repeat_timer);
      SubscribeTimer(0.02, &call, &glRepeatTimer); // Use a timer subscription so that repeat button clicks can be supported (the interval indicates the rate of the repeat)
   }

   if ((Self->DragSourceID) and (!Self->Buttons[bi].LastClicked)) {
      // Drag and drop has been released.  Inform the destination surface of the item's release.

      if (Self->DragSurface) {
         acHide(Self->DragSurface);
         Self->DragSurface = 0;
      }

      if (!modal_id) {
         pf::ScopedObjectLock<> src(Self->DragSourceID);
         if (src.granted()) {
            acDragDrop(Self->OverObjectID, *src, Self->DragItem, Self->DragData);
         }
      }

      Self->DragItem = 0;
      Self->DragSourceID = 0;
   }
}

//********************************************************************************************************************

static void process_ptr_wheel(extPointer *Self, struct dcDeviceInput *Input)
{
   InputEvent msg;
   msg.Type        = JET::WHEEL;
   msg.Flags       = JTYPE::ANALOG|JTYPE::EXT_MOVEMENT | Input->Flags;
   msg.Mask        = JTYPE::EXT_MOVEMENT;
   msg.Value       = Input->Value;
   msg.Timestamp   = Input->Timestamp;
   msg.DeviceID    = Input->DeviceID;
   msg.RecipientID = Self->OverObjectID;
   msg.OverID      = Self->OverObjectID;
   msg.AbsX        = Self->X;
   msg.AbsY        = Self->Y;
   msg.X           = Self->OverX;
   msg.Y           = Self->OverY;

   {
      const std::lock_guard<std::recursive_mutex> lock(glInputLock);
      glInputEvents.push_back(msg);
   }

   // Convert wheel mouse usage into scroll messages

   DOUBLE scrollrate = 0;
   DOUBLE wheel = Input->Value;
   if (wheel > 0) {
      for (LONG i=1; i <= wheel; i++) scrollrate += Self->WheelSpeed * i;
   }
   else {
      wheel = -wheel;
      for (LONG i=1; i <= wheel; i++) scrollrate -= Self->WheelSpeed * i;
   }

   struct acScroll scroll = {
      .DeltaX = 0,
      .DeltaY = scrollrate / 100, //(wheel * Self->WheelSpeed) / 100
      .DeltaZ = 0
   };
   ActionMsg(AC_Scroll, Self->OverObjectID, &scroll);
}

//********************************************************************************************************************

static void process_ptr_movement(extPointer *Self, struct dcDeviceInput *Input)
{
   pf::Log log(__FUNCTION__);
   InputEvent userinput;

   ClearMemory(&userinput, sizeof(userinput));
   userinput.Value     = Input->Value;
   userinput.Timestamp = Input->Timestamp;
   userinput.Type      = Input->Type;
   userinput.Flags     = Input->Flags;
   userinput.DeviceID  = Input->DeviceID;

   if (!userinput.Timestamp) userinput.Timestamp = PreciseTime();

   // All X/Y movement passed through the pointer object must be expressed in absolute coordinates.

   if ((userinput.Type IS JET::DIGITAL_X) or (userinput.Type IS JET::ANALOG_X)) {
      userinput.Type  = JET::ABS_X;
      userinput.Value += Self->X;
   }
   else if ((userinput.Type IS JET::DIGITAL_Y) or (userinput.Type IS JET::ANALOG_Y)) {
      userinput.Type = JET::ABS_Y;
      userinput.Value += Self->Y;
   }

   bool moved = false, underlying_change = false;
   DOUBLE current_x = Self->X;
   DOUBLE current_y = Self->Y;
   switch (userinput.Type) {
      case JET::ABS_X: current_x = userinput.Value; if (current_x != Self->X) moved = true; break;
      case JET::ABS_Y: current_y = userinput.Value; if (current_y != Self->Y) moved = true; break;
      default: break;
   }

   if (!moved) {
      // Check if the surface that we're over has changed due to hide, show or movement of surfaces in the display.

      if (get_over_object(Self)) {
         log.trace("Detected change to underlying surface.");
         underlying_change = true;
      }
   }

   if ((moved) or (underlying_change)) {
      // Movement handling.  Pointer coordinates are managed here on the basis that they are 'global', i.e. in a hosted
      // environment the coordinates are relative to the top-left of the host display.  Anchoring is enabled by calling
      // LockCursor().  Typically this support is not available on hosted environments because we can't guarantee that
      // the pointer is locked.

      if (Self->AnchorID) {
         if (CheckObjectExists(Self->AnchorID) != ERR_Okay) {
            Self->AnchorID = 0;
         }
      }

      DOUBLE xchange = current_x - Self->X;
      DOUBLE ychange = current_y - Self->Y;

      Self->X = current_x;
      Self->Y = current_y;

      if (Self->AnchorID) {
         // When anchoring is enabled we send a movement message signal to the anchored object.  NOTE: In hosted
         // environments we cannot maintain a true anchor since the pointer is out of our control, but we still must
         // perform the necessary notification.

         add_input("Movement-Anchored", userinput, JTYPE::NIL, Self->AnchorID, Self->AnchorID, current_x, current_y, xchange, ychange);
      }
      else {
         struct acMoveToPoint moveto = { Self->X, Self->Y, 0, MTF::X|MTF::Y };
         NotifySubscribers(Self, AC_MoveToPoint, &moveto, ERR_Okay);

         // Recalculate the OverObject due to cursor movement

         get_over_object(Self);
      }

      if (Self->AnchorID) {
         // Do nothing as only the anchor surface receives a message (see earlier)
      }
      else if (Self->Buttons[0].LastClicked) {
         // This routine is used when the user is holding down the left mouse button (indicated by LastClicked).  The X/Y
         // coordinates are worked out in relation to the clicked object by climbing the Surface object hierarchy.

         if (Self->DragSurface) {
            DOUBLE sx = Self->X + DRAG_XOFFSET;
            DOUBLE sy = Self->Y + DRAG_YOFFSET;
            if (Self->DragParent) {
               LONG absx, absy;
               if (!gfxGetSurfaceCoords(Self->DragParent, NULL, NULL, &absx, &absy, NULL, NULL)) {
                  sx -= absx;
                  sy -= absy;
               }
            }

            acMoveToPoint(Self->DragSurface, sx, sy, 0, MTF::X|MTF::Y);
         }

         LONG absx, absy;
         if (!get_surface_abs(Self->Buttons[0].LastClicked, &absx, &absy, 0, 0)) {
            auto uiflags = Self->DragSourceID ? JTYPE::DRAG_ITEM : JTYPE::NIL;

            // Send the movement message to the last clicked object

            add_input("Movement-LastClicked", userinput, uiflags, Self->Buttons[0].LastClicked, Self->OverObjectID,
               Self->X, Self->Y, Self->X - absx, Self->Y - absy); // OverX/Y reported relative to the click-held surface

            get_over_object(Self);

            // The surface directly under the pointer also needs notification - important for the view to highlight
            // folders during drag and drop for example.

            // JTYPE::SECONDARY indicates to the receiver of the input message that it is not the primary recipient.

            if (Self->Buttons[0].LastClicked != Self->OverObjectID) {
               add_input("Movement-LastClicked", userinput, uiflags|JTYPE::SECONDARY, Self->OverObjectID, Self->OverObjectID,
                  Self->X, Self->Y, Self->OverX, Self->OverY);
            }

         }
         else {
            log.warning("Failed to get info for surface #%d.", Self->Buttons[0].LastClicked);
            Self->Buttons[0].LastClicked = 0;
         }
      }
      else {
         if (Self->OverObjectID) {
            add_input("OverObject", userinput, JTYPE::NIL, Self->OverObjectID, Self->OverObjectID,
               Self->X, Self->Y, Self->OverX, Self->OverY);
         }

         // If the surface that we're over has changed, send a message to the previous surface to tell it that the
         // pointer has moved for one final time.

         if ((moved) and (Self->LastSurfaceID) and (Self->LastSurfaceID != Self->OverObjectID)) {
            add_input("Movement-PrevSurface", userinput, JTYPE::NIL, Self->LastSurfaceID, Self->OverObjectID,
               Self->X, Self->Y, Self->OverX, Self->OverY);
         }
      }

      Self->LastSurfaceID = Self->OverObjectID; // Reset the LastSurfaceID
   }

   // If a release object has been specified and the cursor is not positioned over it, call the RestoreCursor method.

   if ((Self->CursorReleaseID) and (Self->CursorReleaseID != Self->OverObjectID)) {
      gfxRestoreCursor(PTC::DEFAULT, 0);
   }
}

//********************************************************************************************************************

static ERROR PTR_Free(extPointer *Self, APTR Void)
{
   acHide(Self);

   if (Self->Bitmap) { FreeResource(Self->Bitmap); Self->Bitmap = NULL; }

/*
   OBJECTPTR object;
   if ((Self->SurfaceID) and (!AccessObject(Self->SurfaceID, 5000, &object))) {
      UnsubscribeFeed(object);
      ReleaseObject(object);
   }
*/
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Hide: Hides the pointer from the display.
-END-
*********************************************************************************************************************/

static ERROR PTR_Hide(extPointer *Self, APTR Void)
{
   pf::Log log;

   log.branch();

   #ifdef __xwindows__
/*
      APTR xwin;
      OBJECTPTR surface;

      if (AccessObject(Self->SurfaceID, 5000, &surface) IS ERR_Okay) {
         surface->getPtr(FID_WindowHandle, &xwin);
         XDefineCursor(XDisplay, (Window)xwin, GetX11Cursor(Self->CursorID));
         ReleaseObject(surface);
      }
*/
   #elif _WIN32
      winShowCursor(0);
   #endif

   Self->Flags &= ~PF::VISIBLE;
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR PTR_Init(extPointer *Self, APTR Void)
{
   pf::Log log;

   // Find the Surface object that we are associated with.  Note that it is okay if no surface is available at this
   // stage, but the host system must have a mechanism for setting the Surface field at a later stage or else
   // GetOverObject will not function.

   if (!Self->SurfaceID) {
      Self->SurfaceID = Self->UID;
      while ((Self->SurfaceID) and (GetClassID(Self->SurfaceID) != ID_SURFACE)) {
         Self->SurfaceID = GetOwnerID(Self->SurfaceID);
      }

      if (!Self->SurfaceID) FindObject("SystemSurface", 0, FOF::NIL, &Self->SurfaceID);
   }

   // Allocate a custom cursor bitmap

   if ((Self->Bitmap = objBitmap::create::integral(
         fl::Name("CustomCursor"),
         fl::Width(MAX_CURSOR_WIDTH),
         fl::Height(MAX_CURSOR_HEIGHT),
         fl::BitsPerPixel(32),
         fl::BytesPerPixel(4),
         fl::Flags(BMF::ALPHA_CHANNEL)))) {
   }
   else log.warning(ERR_NewObject);

   if (Self->MaxSpeed < 1) Self->MaxSpeed = 10;
   if (Self->Speed < 1)    Self->Speed    = 150;

   return ERR_Okay;
}

/*********************************************************************************************************************

-ACTION-
Move: Moves the cursor to a new location.

The Move action will move the cursor to a new location instantly.  This has the effect of bypassing the normal set
of routines for pointer movement (i.e. no UserMovement signals will be sent to applications to indicate the
change).

*********************************************************************************************************************/

static ERROR PTR_Move(extPointer *Self, struct acMove *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_Args);
   if ((!Args->DeltaX) and (!Args->DeltaY)) return ERR_Okay;
   return acMoveToPoint(Self, Self->X + Args->DeltaX, Self->Y + Args->DeltaY, 0, MTF::X|MTF::Y);
}

/*********************************************************************************************************************

-ACTION-
MoveToPoint: Moves the cursor to a new location..

The MoveToPoint action will move the cursor to a new location instantly.  This has the effect of bypassing the
normal set of routines for pointer movement (i.e. no UserMovement signals will be sent to applications to
indicate the change).

The client can subscribe to this action to listen for changes to the cursor's position.
-END-

*********************************************************************************************************************/

static ERROR PTR_MoveToPoint(extPointer *Self, struct acMoveToPoint *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs)|ERF_Notified;
/*
   if ((!(Args->Flags & MTF::X)) or ((Args->Flags & MTF::X) and (Self->X IS Args->X))) {
      if ((!(Args->Flags & MTF::Y)) or ((Args->Flags & MTF::Y) and (Self->Y IS Args->Y))) {
         return ERR_Okay|ERF_Notified;
      }
   }
*/
#ifdef __xwindows__
   OBJECTPTR surface;

   if (!AccessObject(Self->SurfaceID, 3000, &surface)) {
      APTR xwin;

      if (!surface->getPtr(FID_WindowHandle, &xwin)) {
         if ((Args->Flags & MTF::X) != MTF::NIL) Self->X = Args->X;
         if ((Args->Flags & MTF::Y) != MTF::NIL) Self->Y = Args->Y;
         if (Self->X < 0) Self->X = 0;
         if (Self->Y < 0) Self->Y = 0;

         XWarpPointer(XDisplay, None, (Window)xwin, 0, 0, 0, 0, Self->X, Self->Y);
         Self->HostX = Self->X;
         Self->HostY = Self->Y;
      }
      ReleaseObject(surface);
   }
#elif _WIN32
   OBJECTPTR surface;

   if (!AccessObject(Self->SurfaceID, 3000, &surface)) {
      if ((Args->Flags & MTF::X) != MTF::NIL) Self->X = Args->X;
      if ((Args->Flags & MTF::Y) != MTF::NIL) Self->Y = Args->Y;
      if (Self->X < 0) Self->X = 0;
      if (Self->Y < 0) Self->Y = 0;

      winSetCursorPos(Self->X, Self->Y);
      Self->HostX = Self->X;
      Self->HostY = Self->Y;
      ReleaseObject(surface);
   }
#endif

   // Determine the surface object that we are currently positioned over.  If it has set a cursor image, switch to it if the pointer is not locked.

   get_over_object(Self);

   // Customised notification (ensures that both X and Y coordinates are reported).

   struct acMoveToPoint moveto = { Self->X, Self->Y, 0, MTF::X|MTF::Y };
   NotifySubscribers(Self, AC_MoveToPoint, &moveto, ERR_Okay);

   return ERR_Okay|ERF_Notified;
}

//********************************************************************************************************************

static ERROR PTR_NewObject(extPointer *Self, APTR Void)
{
   Self->CursorID = PTC::DEFAULT;
   Self->ClickSlop = 2;
   set_pointer_defaults(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Refresh: Refreshes the pointer's cursor status.
-END-
*********************************************************************************************************************/

static ERROR PTR_Refresh(extPointer *Self, APTR Void)
{
   // Calling OverObject will refresh the cursor image from the underlying surface object.  Incidentally, the point of
   // all this is to satisfy the Surface class' need to have the pointer refreshed if a surface's cursor ID is changed.

   get_over_object(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Reset: Resets the pointer settings back to the default.
-END-
*********************************************************************************************************************/

static ERROR PTR_Reset(extPointer *Self, APTR Void)
{
   Self->Speed        = 150;
   Self->Acceleration = 0.50;
   Self->DoubleClick  = 0.30;
   Self->MaxSpeed     = 100;
   Self->WheelSpeed   = DEFAULT_WHEELSPEED;
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
SaveToObject: Saves the current pointer settings to another object.
-END-
*********************************************************************************************************************/

static ERROR PTR_SaveToObject(extPointer *Self, struct acSaveToObject *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Dest)) return log.warning(ERR_NullArgs);

   objConfig::create config = { };
   if (config.ok()) {
      config->write("POINTER", "Speed", Self->Speed);
      config->write("POINTER", "Acceleration", Self->Acceleration);
      config->write("POINTER", "DoubleClick", Self->DoubleClick);
      config->write("POINTER", "MaxSpeed", Self->MaxSpeed);
      config->write("POINTER", "WheelSpeed", Self->WheelSpeed);
      config->write("POINTER", "ButtonOrder", Self->ButtonOrder);
      config->saveToObject(Args->Dest);
   }

   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Show: Shows the pointer if it is not already on the display.
-END-
*********************************************************************************************************************/

static ERROR PTR_Show(extPointer *Self, APTR Void)
{
   pf::Log log;

   log.branch();

   #ifdef __xwindows__
/*
      APTR xwin;
      OBJECTPTR surface;

      if (!AccessObject(Self->SurfaceID, 5000, &surface)) {
         surface->getPtr(FID_WindowHandle, &xwin);
         XDefineCursor(XDisplay, (Window)xwin, GetX11Cursor(Self->CursorID));
         ReleaseObject(surface);
      }
*/
 #elif _WIN32

      winShowCursor(1);
   #endif

   Self->Flags |= PF::VISIBLE;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Acceleration: The rate of acceleration for relative pointer movement.

This field affects the rate of acceleration as the pointer is moved across the display.  It is recommended that this
field is never set manually, as the user will need to determine the best acceleration level through trial and error in
the user preferences program.

This field is not relevant in a hosted environment.

-FIELD-
Anchor: Can refer to a surface that the pointer has been anchored to.

If the pointer is anchored to a surface through ~SetCursor(), this field will refer to the surface that holds the
anchor.

-FIELD-
Bitmap: Refers to bitmap in which custom cursor images can be drawn.

The pointer graphic can be changed to a custom image if the `PTC::CUSTOM` #CursorID type is defined and an image is
drawn to the @Bitmap object referenced by this field.

-FIELD-
ButtonOrder: Defines the order in which mouse buttons are interpreted.

This field defines the order of interpretation of the mouse buttons when they are pressed.  This allows a right handed
device to have its buttons remapped to mimic a left-handed device for instance.

The default button order is defined as `123456789AB`.  The left, right and middle mouse buttons are defined as 1, 2 and
3 respectively.  The rest of the buttons are assigned by the device, preferably starting from the left of the device and
moving clockwise.

It is legal for buttons to be referenced more than once, for instance a setting of `111` will force the middle and right
mouse buttons to translate to the left mouse button.

Changes to this field will have an immediate impact on the pointing device's behaviour.

*********************************************************************************************************************/

static ERROR GET_ButtonOrder(extPointer *Self, CSTRING *Value)
{
   *Value = Self->ButtonOrder;
   return ERR_Okay;
}

static ERROR SET_ButtonOrder(extPointer *Self, CSTRING Value)
{
   pf::Log log;

   log.msg("%s", Value);

   if (!Value) return ERR_Okay;

   // Assign the buttons

   for (WORD i=0; (Value[i]) and ((size_t)i < sizeof(Self->ButtonOrder)-1); i++) Self->ButtonOrder[i] = Value[i];
   Self->ButtonOrder[sizeof(Self->ButtonOrder)-1] = 0;

   // Eliminate any invalid buttons

   for (WORD i=0; Self->ButtonOrder[i]; i++) {
      if (((Self->ButtonOrder[i] >= '1') and (Self->ButtonOrder[i] <= '9')) or
          ((Self->ButtonOrder[i] >= 'A') and (Self->ButtonOrder[i] <= 'Z'))) {
      }
      else Self->ButtonOrder[i] = ' ';
   }

   // Reduce the length of the button list if there are gaps

   LONG j = 0;
   for (WORD i=0; Self->ButtonOrder[i]; i++) {
      if (Self->ButtonOrder[i] != ' ') {
         Self->ButtonOrder[j++] = Self->ButtonOrder[i];
      }
   }

   while ((size_t)j < sizeof(Self->ButtonOrder)) Self->ButtonOrder[j++] = 0; // Clear any left-over bytes

   // Convert the button indexes into their relevant flags

   for (WORD i=0; (size_t)i < sizeof(Self->ButtonOrder); i++) {
      if ((Self->ButtonOrder[i] >= '1') and (Self->ButtonOrder[i] <= '9')) j = Self->ButtonOrder[i] - '1';
      else if ((Self->ButtonOrder[i] >= 'A') and (Self->ButtonOrder[i] <= 'Z')) j = Self->ButtonOrder[i] - 'A' + 9;
      else j = 0;
      Self->ButtonOrderFlags[i] = 1<<j;
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
ButtonState: Indicates the current button-press state.

You can read this field at any time to get an indication of the buttons that are currently being held by the user.  The
flags returned by this field are `JD_LMB`, `JD_RMB` and `JD_MMB` indicating left, right and middle mouse buttons
respectively.

*********************************************************************************************************************/

static ERROR GET_ButtonState(extPointer *Self, LONG *Value)
{
   LONG i;
   LONG state = 0;
   for (i=0; i < ARRAYSIZE(Self->Buttons); i++) {
      if (Self->Buttons[i].LastClicked) state |= 1<<i;
   }

   *Value = state;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
ClickSlop: A leniency value that assists in determining if the user intended to click or drag.

The ClickSlop value defines the allowable pixel distance between two clicks for them to be considered a double-click
(or a drag operation if they exceed the distance).

-FIELD-
CursorID: Sets the user's cursor image, selected from the pre-defined graphics bank.

-FIELD-
CursorOwner: The current owner of the cursor, as defined by SetCursor().

If the pointer is currently owned by an object, this field will refer to that object ID.  Pointer ownership is managed
by the ~SetCursor() function.

-FIELD-
DoubleClick: The maximum interval between two clicks for a double click to be recognised.

A double-click is recognised when two separate clicks occur within a pre-determined time frame.  The length of that
time frame is determined in the DoubleClick field and is measured in seconds.  The recommended interval is 0.3 seconds,
although the user can store his own preference in the pointer configuration file.

-FIELD-
DragItem: The currently dragged item, as defined by StartCursorDrag().

When the pointer is in drag-mode, the custom item number that was defined in the initial call to StartCursorDrag() will
be defined here.  At all other times this field will be set to zero.

-FIELD-
DragSource: The object managing the current drag operation, as defined by StartCursorDrag().

When the pointer is in drag-mode, the object that is managing the source data will be referenced in this field.  At all
other times this field will be set to zero.

Item dragging is managed by the StartCursorDrag() function.

-FIELD-
Flags: Optional flags.
Lookup: PF

-FIELD-
Input: Declares the I/O object to read movement from.

By default a pointer will read its input directly from the mouse port.  However it may be convenient for the pointer to
receive its information from elsewhere, in which case you can set this field to point to a different input object.  The
object that you use <i>must</i> be able to send joyport information over data channels.

-FIELD-
MaxSpeed: Restricts the maximum speed of a pointer's movement.

The maximum speed at which the pointer can move per frame is specified in this field.  This field is provided to help
the user for times where the pointer may be moving to fast (for example if the hardware driver is interpreting the mouse
movement at larger offsets than what is normal).  You can also set the value to 1 if a digital simulation is required.

*********************************************************************************************************************/

static ERROR SET_MaxSpeed(extPointer *Self, LONG Value)
{
   if (Value < 2) Self->MaxSpeed = 2;
   else if (Value > 200) Self->MaxSpeed = 200;
   else Self->MaxSpeed = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
OverObject: Readable field that gives the ID of the object under the pointer.

This field returns a reference to the object directly under the pointer's hot-spot.  NULL can be returned if there is
no surface object under the pointer.

-FIELD-
OverX: The horizontal position of the Pointer with respect to the object underneath the hot-spot.

The OverX field provides other classes with a means of finding out exactly where the pointer is positioned over their
display area.  For example, if a user click occurs on an Image and it is necessary to find out what coordinates where
affected, the OverX and OverY fields can be polled to determine the exact position of the user click.

-FIELD-
OverY: The vertical position of the Pointer with respect to the object underneath the hot-spot.

The OverY field provides other classes with a means of finding out exactly where the pointer is positioned over their
display area.  For example, if a user click occurs on an Image and it is necessary to find out what coordinates where
affected, the OverX and OverY fields can be polled to determine the exact position of the user click.

-FIELD-
OverZ: The position of the Pointer within an object.

This special field applies to 3D interfaces only.  It reflects the position of the pointer within 3-Dimensional
displays, by returning its coordinate along the Z axis.

-FIELD-
Restrict: Refers to a surface when the pointer is restricted.

If the pointer has been restricted to a surface through SetCursor(), this field refers to the ID of that surface.  If
the pointer is not restricted, this field is set to zero.

-FIELD-
Speed: Speed multiplier for Pointer movement.

The speed at which the pointer moves can be adjusted with this field.  To lower the speed, use a value between 0 and
100%.  To increase the speed, use a value between 100 and 1000%.  The Speed of the Pointer is complemented by the
MaxSpeed field, which restricts the maximum amount of pixels that a Pointer can move each time the input device is
polled.

-FIELD-
Surface: The top-most surface that is under the pointer's hot spot.

The surface that is directly under the pointer's hot spot is referenced by this field.  It is automatically updated
whenever the position of the pointer changes or a new surface appears under the pointer.

-FIELD-
WheelSpeed: Defines a multiplier to be applied to the mouse wheel.

This field defines a multiplier that is applied to values coming from the mouse wheel.  A setting of 1.0 leaves the
wheel speed unaltered, while a setting of 2.0 would double the regular speed.

-FIELD-
X: The horizontal position of the pointer within its parent display.

*********************************************************************************************************************/

static ERROR PTR_SET_X(extPointer *Self, DOUBLE Value)
{
   if (Self->initialised()) acMoveToPoint(Self, Value, 0, 0, MTF::X);
   else Self->X = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Y: The vertical position of the pointer within its parent display.
-END-

*********************************************************************************************************************/

static ERROR PTR_SET_Y(extPointer *Self, DOUBLE Value)
{
   if (Self->initialised()) acMoveToPoint(Self, 0, Value, 0, MTF::Y);
   else Self->Y = Value;
   return ERR_Okay;
}

//********************************************************************************************************************

static void set_pointer_defaults(extPointer *Self)
{
   DOUBLE speed        = glDefaultSpeed;
   DOUBLE acceleration = glDefaultAcceleration;
   LONG maxspeed     = 100;
   DOUBLE wheelspeed   = DEFAULT_WHEELSPEED;
   DOUBLE doubleclick  = 0.36;
   CSTRING buttonorder = "123456789ABCDEF";

   objConfig::create config = { fl::Path("user:config/pointer.cfg") };

   if (config.ok()) {
      DOUBLE dbl;
      CSTRING str;
      if (!cfgRead(*config, "POINTER", "Speed", &dbl)) speed = dbl;
      if (!cfgRead(*config, "POINTER", "Acceleration", &dbl)) acceleration = dbl;
      if (!cfgRead(*config, "POINTER", "MaxSpeed", &dbl)) maxspeed = dbl;
      if (!cfgRead(*config, "POINTER", "WheelSpeed", &dbl)) wheelspeed = dbl;
      if (!cfgRead(*config, "POINTER", "DoubleClick", &dbl)) doubleclick = dbl;
      if (!cfgReadValue(*config, "POINTER", "ButtonOrder", &str)) buttonorder = str;
   }

   if (doubleclick < 0.2) doubleclick = 0.2;

   Self->setFields(fl::Speed(speed),
       fl::Acceleration(acceleration),
       fl::MaxSpeed(maxspeed),
       fl::WheelSpeed(wheelspeed),
       fl::DoubleClick(doubleclick),
       fl::ButtonOrder(buttonorder));
}

//********************************************************************************************************************
// Returns true if the underlying object has changed.  The OverObjectID will reflect the current underlying surface.

static bool get_over_object(extPointer *Self)
{
   pf::Log log(__FUNCTION__);

   if ((Self->SurfaceID) and (CheckObjectExists(Self->SurfaceID) != ERR_Okay)) Self->SurfaceID = 0;

   bool changed = false;

   // Find the surface that the pointer resides in (usually SystemSurface @ index 0)

   const std::lock_guard<std::recursive_mutex> lock(glSurfaceLock);

   if (glSurfaces.empty()) return false;

   size_t index;
   if (!Self->SurfaceID) {
      Self->SurfaceID = glSurfaces[0].SurfaceID;
      index = 0;
   }
   else for (index=0; (index < glSurfaces.size()) and (glSurfaces[index].SurfaceID != Self->SurfaceID); index++);

   auto i = examine_chain(Self, index, glSurfaces, glSurfaces.size());

   OBJECTID li_objectid = glSurfaces[i].SurfaceID;
   DOUBLE li_left       = glSurfaces[i].Left;
   DOUBLE li_top        = glSurfaces[i].Top;
   auto cursor_image    = PTC(glSurfaces[i].Cursor); // Preferred cursor ID

   if (Self->OverObjectID != li_objectid) {
      pf::Log log(__FUNCTION__);

      log.traceBranch("OverObject changing from #%d to #%d.", Self->OverObjectID, li_objectid);

      changed = true;

      InputEvent input = {
         .Next        = NULL,
         .Value       = (DOUBLE)Self->OverObjectID,
         .Timestamp   = PreciseTime(),
         .RecipientID = Self->OverObjectID, // Recipient is the surface we are leaving
         .OverID      = li_objectid, // New surface (entering)
         .AbsX        = Self->X,
         .AbsY        = Self->Y,
         .X           = Self->X - li_left,
         .Y           = Self->Y - li_top,
         .DeviceID    = Self->UID,
         .Type        = JET::LEFT_AREA,
         .Flags       = JTYPE::FEEDBACK,
         .Mask        = JTYPE::FEEDBACK
      };

      const std::lock_guard<std::recursive_mutex> lock(glInputLock);
      glInputEvents.push_back(input);

      input.Type        = JET::ENTERED_AREA;
      input.Value       = li_objectid;
      input.RecipientID = li_objectid; // Recipient is the surface we are entering
      glInputEvents.push_back(input);

      Self->OverObjectID = li_objectid;
   }

   Self->OverX = Self->X - li_left;
   Self->OverY = Self->Y - li_top;

   if (cursor_image != PTC::NIL) {
      if (cursor_image != Self->CursorID) gfxSetCursor(0, CRF::NIL, cursor_image, NULL, 0);
   }
   else if ((Self->CursorID != PTC::DEFAULT) and (!Self->CursorOwnerID)) {
      // Restore the pointer to the default image if the cursor isn't locked
      gfxSetCursor(0, CRF::NIL, PTC::DEFAULT, NULL, 0);
   }

   return changed;
}

//********************************************************************************************************************

static LONG examine_chain(extPointer *Self, LONG Index, SURFACELIST &List, LONG End)
{
   // NB: Traversal is in reverse to catch the front-most objects first.

   auto objectid = List[Index].SurfaceID;
   auto x = Self->X;
   auto y = Self->Y;
   for (auto i=End-1; i >= 0; i--) {
      if ((List[i].ParentID IS objectid) and (List[i].visible())) {
         if ((x >= List[i].Left) and (x < List[i].Right) and (y >= List[i].Top) and (y < List[i].Bottom)) {
            LONG new_end;
            for (new_end=i+1; List[new_end].Level > List[i].Level; new_end++); // Recalculate the end (optimisation)
            return examine_chain(Self, i, List, new_end);
         }
      }
   }

   return Index;
}

//********************************************************************************************************************
// This timer is used for handling repeat-clicks.

static ERROR repeat_timer(extPointer *Self, LARGE Elapsed, LARGE Unused)
{
   pf::Log log(__FUNCTION__);

   // The subscription is automatically removed if no buttons are held down

   bool unsub = true;
   for (LONG i=0; i < ARRAYSIZE(Self->Buttons); i++) {
      if (Self->Buttons[i].LastClicked) {
         LARGE time = PreciseTime();
         if (Self->Buttons[i].LastClickTime + 300000LL <= time) {
            InputEvent input;
            ClearMemory(&input, sizeof(input));

            LONG surface_x, surface_y;
            if (Self->Buttons[i].LastClicked IS Self->OverObjectID) {
               input.X = Self->OverX;
               input.Y = Self->OverY;
            }
            else if (!get_surface_abs(Self->Buttons[i].LastClicked, &surface_x, &surface_y, 0, 0)) {
               input.X = Self->X - surface_x;
               input.Y = Self->Y - surface_y;
            }
            else {
               input.X = Self->OverX;
               input.Y = Self->OverY;
            }

            input.Type        = JET(LONG(JET::BUTTON_1) + i);
            input.Mask        = JTYPE::BUTTON|JTYPE::REPEATED;
            input.Flags       = JTYPE::BUTTON|JTYPE::REPEATED;
            input.Value       = 1.0; // Self->Buttons[i].LastValue
            input.Timestamp   = time;
            input.DeviceID    = 0;
            input.RecipientID = Self->Buttons[i].LastClicked;
            input.OverID      = Self->OverObjectID;
            input.AbsX        = Self->X;
            input.AbsY        = Self->Y;

            const std::lock_guard<std::recursive_mutex> lock(glInputLock);
            glInputEvents.push_back(input);
         }

         unsub = false;
      }
   }

   if (unsub) return ERR_Terminate;
   else return ERR_Okay;
}

//********************************************************************************************************************

FieldDef CursorLookup[] = {
   { "None",            0 },
   { "Default",         PTC::DEFAULT },             // Values start from 1 and go up
   { "SizeBottomLeft",  PTC::SIZE_BOTTOM_LEFT },
   { "SizeBottomRight", PTC::SIZE_BOTTOM_RIGHT },
   { "SizeTopLeft",     PTC::SIZE_TOP_LEFT },
   { "SizeTopRight",    PTC::SIZE_TOP_RIGHT },
   { "SizeLeft",        PTC::SIZE_LEFT },
   { "SizeRight",       PTC::SIZE_RIGHT },
   { "SizeTop",         PTC::SIZE_TOP },
   { "SizeBottom",      PTC::SIZE_BOTTOM },
   { "Crosshair",       PTC::CROSSHAIR },
   { "Sleep",           PTC::SLEEP },
   { "Sizing",          PTC::SIZING },
   { "SplitVertical",   PTC::SPLIT_VERTICAL },
   { "SplitHorizontal", PTC::SPLIT_HORIZONTAL },
   { "Magnifier",       PTC::MAGNIFIER },
   { "Hand",            PTC::HAND },
   { "HandLeft",        PTC::HAND_LEFT },
   { "HandRight",       PTC::HAND_RIGHT },
   { "Text",            PTC::TEXT },
   { "Paintbrush",      PTC::PAINTBRUSH },
   { "Stop",            PTC::STOP },
   { "Invisible",       PTC::INVISIBLE },
   { "Custom",          PTC::CUSTOM },
   { "Dragable",        PTC::DRAGGABLE },
   { NULL, 0 }
};

static const ActionArray clPointerActions[] = {
   { AC_DataFeed,     PTR_DataFeed },
   { AC_Free,         PTR_Free },
   { AC_Hide,         PTR_Hide },
   { AC_Init,         PTR_Init },
   { AC_Move,         PTR_Move },
   { AC_MoveToPoint,  PTR_MoveToPoint },
   { AC_NewObject,    PTR_NewObject },
   { AC_Refresh,      PTR_Refresh },
   { AC_Reset,        PTR_Reset },
   { AC_SaveToObject, PTR_SaveToObject },
   { AC_Show,         PTR_Show },
   { 0, NULL }
};

static const FieldDef clPointerFlags[] = {
   { "Visible",  PF::VISIBLE },
   { NULL, 0 }
};

static const FunctionField mthSetCursor[]     = { { "Surface", FD_LONG }, { "Flags", FD_LONG }, { "Cursor", FD_LONG }, { "Name", FD_STRING }, { "Owner", FD_LONG }, { "PreviousCursor", FD_LONG|FD_RESULT }, { NULL, 0 } };
static const FunctionField mthRestoreCursor[] = { { "Cursor", FD_LONG }, { "Owner", FD_LONG }, { NULL, 0 } };

static const MethodEntry clPointerMethods[] = {
   // Private methods
#ifdef _WIN32
   { MT_PtrSetWinCursor,     (APTR)PTR_SetWinCursor,   "SetWinCursor",   mthSetWinCursor,  sizeof(struct ptrSetWinCursor) },
#endif
#ifdef __xwindows__
   { MT_PtrGrabX11Pointer,   (APTR)PTR_GrabX11Pointer,   "GrabX11Pointer",   mthGrabX11Pointer, sizeof(struct ptrGrabX11Pointer) },
   { MT_PtrUngrabX11Pointer, (APTR)PTR_UngrabX11Pointer, "UngrabX11Pointer", NULL, 0 },
#endif
   { 0, NULL, NULL, NULL, 0 }
};

static const FieldArray clPointerFields[] = {
   { "Speed",        FDF_DOUBLE|FDF_RW },
   { "Acceleration", FDF_DOUBLE|FDF_RW },
   { "DoubleClick",  FDF_DOUBLE|FDF_RW },
   { "WheelSpeed",   FDF_DOUBLE|FDF_RW },
   { "X",            FDF_DOUBLE|FDF_RW, NULL, PTR_SET_X },
   { "Y",            FDF_DOUBLE|FDF_RW, NULL, PTR_SET_Y },
   { "OverX",        FDF_DOUBLE|FDF_R },
   { "OverY",        FDF_DOUBLE|FDF_R },
   { "OverZ",        FDF_DOUBLE|FDF_R },
   { "MaxSpeed",     FDF_LONG|FDF_RW, NULL, SET_MaxSpeed },
   { "Input",        FDF_OBJECTID|FDF_RW },
   { "Surface",      FDF_OBJECTID|FDF_RW, NULL, NULL, ID_SURFACE },
   { "Anchor",       FDF_OBJECTID|FDF_R, NULL, NULL, ID_SURFACE },
   { "CursorID",     FDF_LONG|FDF_LOOKUP|FDF_RI, NULL, NULL, &CursorLookup },
   { "CursorOwner",  FDF_OBJECTID|FDF_RW },
   { "Flags",        FDF_LONGFLAGS|FDF_RI, NULL, NULL, &clPointerFlags },
   { "Restrict",     FDF_OBJECTID|FDF_R, NULL, NULL, ID_SURFACE },
   { "HostX",        FDF_LONG|FDF_R|FDF_SYSTEM },
   { "HostY",        FDF_LONG|FDF_R|FDF_SYSTEM },
   { "Bitmap",       FDF_OBJECT|FDF_R, NULL, NULL, ID_BITMAP },
   { "DragSource",   FDF_OBJECTID|FDF_R },
   { "DragItem",     FDF_LONG|FDF_R },
   { "OverObject",   FDF_OBJECTID|FDF_R },
   { "ClickSlop",    FDF_LONG|FDF_RW },
   // Virtual Fields
   { "ButtonState",  FDF_LONG|FDF_R, GET_ButtonState },
   { "ButtonOrder",  FDF_STRING|FDF_RW, GET_ButtonOrder, SET_ButtonOrder },
   END_FIELD
};

//********************************************************************************************************************

ERROR create_pointer_class(void)
{
   clPointer = objMetaClass::create::global(
      fl::BaseClassID(ID_POINTER),
      fl::ClassVersion(VER_POINTER),
      fl::Name("Pointer"),
      fl::Category(CCF::GRAPHICS),
      fl::Actions(clPointerActions),
      fl::Methods(clPointerMethods),
      fl::Fields(clPointerFields),
      fl::Size(sizeof(extPointer)),
      fl::Path(MOD_PATH));

   return clPointer ? ERR_Okay : ERR_AddClass;
}
