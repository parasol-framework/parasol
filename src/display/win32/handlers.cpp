/*********************************************************************************************************************

Notes
-----
* Use TrackMouseEvent() to receive notification of the mouse leaving a window.
* GetWindowThreadProcessId() can tell you the ID of the thread that created a window.  Also, GetWindowLongPtr() can
  give information on the HINSTANCE, HWNDPARENT, window ID.
* The win32 FindWindow() and FindWindowEx() functions can be used to retrieve foreign window handles.
* The IsWindow() function can be used to determine if a window handle is still valid.

*********************************************************************************************************************/

namespace display {

void MsgKeyPress(KQ Flags, KEY Value, LONG Printable)
{
   if (Value IS KEY::NIL) return;

   if ((Printable < 0x20) or (Printable IS 127)) Flags |= KQ::NOT_PRINTABLE;

   evKey key = {
      .EventID    = EVID_IO_KEYBOARD_KEYPRESS,
      .Qualifiers = Flags|KQ::PRESSED,
      .Code       = Value,
      .Unicode    = Printable
   };
   BroadcastEvent(&key, sizeof(key));
}

void MsgKeyPress(int Flags, int Value, int Printable) { MsgKeyPress(KQ(Flags), KEY(Value), Printable); }

//********************************************************************************************************************

void MsgKeyRelease(KQ Flags, KEY Value)
{
   if (Value IS KEY::NIL) return;

   evKey key = {
      .EventID    = EVID_IO_KEYBOARD_KEYPRESS,
      .Qualifiers = Flags|KQ::RELEASED,
      .Code       = Value,
      .Unicode    = 0
   };
   BroadcastEvent(&key, sizeof(key));
}

void MsgKeyRelease(int Flags, int Value) { MsgKeyRelease(KQ(Flags), KEY(Value)); }

//********************************************************************************************************************

void MsgMovement(OBJECTID SurfaceID, DOUBLE AbsX, DOUBLE AbsY, LONG WinX, LONG WinY, bool NonClient)
{
   if (auto pointer = gfxAccessPointer(); pointer) {
      pointer->set(FID_Surface, SurfaceID);  // Alter the surface of the pointer so that it refers to the correct root window
      gfxReleasePointer(pointer);

      struct dcDeviceInput joy[1];
      joy[0].Type  = JET::ABS_XY;
      joy[0].Flags = NonClient ? JTYPE::SECONDARY : JTYPE::NIL;
      joy[0].Values[0] = AbsX;
      joy[0].Values[1] = AbsY;
      joy[0].Timestamp = PreciseTime();

      struct acDataFeed feed = {
         .Object   = NULL,
         .Datatype = DATA::DEVICE_INPUT,
         .Buffer   = &joy,
         .Size     = sizeof(struct dcDeviceInput)
      };
      ActionMsg(AC_DataFeed, glPointerID, &feed);
   }
}

//********************************************************************************************************************

void MsgWheelMovement(OBJECTID SurfaceID, FLOAT Wheel)
{
   if (!glPointerID) {
      if (FindObject("SystemPointer", 0, FOF::NIL, &glPointerID) != ERR::Okay) return;
   }

   struct dcDeviceInput joy;
   joy.Type      = JET::WHEEL;
   joy.Flags     = JTYPE::NIL;
   joy.Values[0] = Wheel;
   joy.Timestamp = PreciseTime();

   struct acDataFeed feed;
   feed.Object   = NULL;
   feed.Datatype = DATA::DEVICE_INPUT;
   feed.Buffer   = &joy;
   feed.Size     = sizeof(struct dcDeviceInput);
   ActionMsg(AC_DataFeed, glPointerID, &feed);
}

//********************************************************************************************************************

void MsgFocusState(OBJECTID SurfaceID, LONG State)
{
   //log.msg("Windows focus state for surface #%d: %d", SurfaceID, State);

   if (State) acFocus(SurfaceID);
   else {
      acLostFocus(SurfaceID);
      // for (auto &id : glFocusList) acLostFocus(id);
   }
}

//********************************************************************************************************************
// If a button press is incoming from the non-client area (e.g. titlebar, resize edge) then the SECONDARY flag is
// applied.

void MsgButtonPress(LONG button, LONG State)
{
   if (auto pointer = gfxAccessPointer()) {
      struct dcDeviceInput joy[3];

      LONG i = 0;
      LARGE timestamp = PreciseTime();

      if (button & 0x0001) {
         joy[i].Type  = JET::BUTTON_1;
         joy[i].Flags = (button & 0x4000) ? JTYPE::SECONDARY : JTYPE::NIL;
         joy[i].Values[0] = State;
         joy[i].Timestamp = timestamp;
         i++;
      }

      if (button & 0x0002) {
         joy[i].Type  = JET::BUTTON_2;
         joy[i].Flags = (button & 0x4000) ? JTYPE::SECONDARY : JTYPE::NIL;
         joy[i].Values[0] = State;
         joy[i].Timestamp = timestamp;
         i++;
      }

      if (button & 0x0004) {
         joy[i].Type  = JET::BUTTON_3;
         joy[i].Flags = (button & 0x4000) ? JTYPE::SECONDARY : JTYPE::NIL;
         joy[i].Values[0] = State;
         joy[i].Timestamp = timestamp;
         i++;
      }

      gfxReleasePointer(pointer);

      if (i) {
         struct acDataFeed feed;
         feed.Object   = NULL;
         feed.Datatype = DATA::DEVICE_INPUT;
         feed.Buffer   = &joy;
         feed.Size     = sizeof(struct dcDeviceInput) * i;
         if (ActionMsg(AC_DataFeed, glPointerID, &feed) IS ERR::NoMatchingObject) {
            glPointerID = 0;
         }
      }
   }
}

//********************************************************************************************************************

void MsgResizedWindow(OBJECTID SurfaceID, LONG WinX, LONG WinY, LONG WinWidth, LONG WinHeight,
   LONG ClientX, LONG ClientY, LONG ClientWidth, LONG ClientHeight)
{
   pf::Log log("ResizedWindow");
   //log.branch("#%d, Window: %dx%d,%dx%d, Client: %dx%d,%dx%d", SurfaceID, WinX, WinY, WinWidth, WinHeight, ClientX, ClientY, ClientWidth, ClientHeight);

   if ((!SurfaceID) or (WinWidth < 1) or (WinHeight < 1)) return;

   objSurface *surface;
   if (AccessObject(SurfaceID, 3000, &surface) IS ERR::Okay) {
      extDisplay *display;
      OBJECTID display_id = surface->DisplayID;
      if (AccessObject(display_id, 3000, &display) IS ERR::Okay) {
         FUNCTION feedback = display->ResizeFeedback;
         display->X = WinX;
         display->Y = WinY;
         display->Width  = WinWidth;
         display->Height = WinHeight;
         ReleaseObject(display);
         ReleaseObject(surface);

         // Notification occurs with the display and surface released so as to reduce the potential for dead-locking.

         resize_feedback(&feedback, display_id, ClientX, ClientY, ClientWidth, ClientHeight);
         return;
      }
      ReleaseObject(surface);
   }
}

/*********************************************************************************************************************
** We're interested in this message only when Windows soft-sets one of our windows.  A 'soft-set' means that our Window
** has received the focus without direct user interaction (typically a window on the desktop has closed and our
** window is inheriting the focus).
**
** Being able to tell the difference between a soft-set and a hard-set is difficult, but checking for visibility seems
** to be enough in preventing confusion.
*/

void MsgSetFocus(OBJECTID SurfaceID)
{
   pf::Log log;
   objSurface *surface;
   if (AccessObject(SurfaceID, 3000, &surface) IS ERR::Okay) {
      if ((!surface->hasFocus()) and (surface->visible())) {
         log.msg("WM_SETFOCUS: Sending focus to surface #%d.", SurfaceID);
         QueueAction(AC_Focus, SurfaceID);
      }
      else log.trace("WM_SETFOCUS: Surface #%d already has the focus, or is hidden.", SurfaceID);
      ReleaseObject(surface);
   }
}

//********************************************************************************************************************
// The width and height arguments must reflect the dimensions of the client area.

void CheckWindowSize(OBJECTID SurfaceID, LONG *Width, LONG *Height)
{
   if ((!SurfaceID) or (!Width) or (!Height)) return;

   objSurface *surface;
   if (AccessObject(SurfaceID, 3000, &surface) IS ERR::Okay) {
      auto minwidth  = surface->get<LONG>(FID_MinWidth);
      auto minheight = surface->get<LONG>(FID_MinHeight);
      auto maxwidth  = surface->get<LONG>(FID_MaxWidth);
      auto maxheight = surface->get<LONG>(FID_MaxHeight);
      auto left      = surface->get<LONG>(FID_LeftMargin);
      auto top       = surface->get<LONG>(FID_TopMargin);
      auto bottom    = surface->get<LONG>(FID_BottomMargin);
      auto right     = surface->get<LONG>(FID_RightMargin);

      if (*Width < minwidth + left + right)   *Width  = minwidth + left + right;
      if (*Height < minheight + top + bottom) *Height = minheight + top + bottom;
      if (*Width > maxwidth + left + right)   *Width  = maxwidth + left + right;
      if (*Height > maxheight + top + bottom) *Height = maxheight + top + bottom;

      if ((surface->Flags & RNF::ASPECT_RATIO) != RNF::NIL) {
         if (minwidth > minheight) {
            DOUBLE scale = (DOUBLE)minheight / (DOUBLE)minwidth;
            *Height = *Width * scale;
         }
         else {
            DOUBLE scale = (DOUBLE)minwidth / (DOUBLE)minheight;
            *Width = *Height * scale;
         }
      }

      ReleaseObject(surface);
   }
}

//********************************************************************************************************************

void RepaintWindow(OBJECTID SurfaceID, LONG X, LONG Y, LONG Width, LONG Height)
{
   if ((Width) and (Height)) {
      struct drwExpose expose = { X, Y, Width, Height, EXF::CHILDREN };
      ActionMsg(MT_DrwExpose, SurfaceID, &expose);
   }
   else ActionMsg(MT_DrwExpose, SurfaceID, NULL);
}

//********************************************************************************************************************

void MsgTimer(void)
{
   ProcessMessages(PMF::NIL, 0);
}

//********************************************************************************************************************

void MsgWindowClose(OBJECTID SurfaceID)
{
   pf::Log log(__FUNCTION__);

   if (SurfaceID) {
      const WindowHook hook(SurfaceID, WH::CLOSE);

      if (glWindowHooks.contains(hook)) {
         auto func = &glWindowHooks[hook];
         ERR result;

         if (func->isC()) {
            pf::SwitchContext ctx(func->Context);
            auto callback = (ERR (*)(OBJECTID SurfaceID, APTR))func->Routine;
            result = callback(SurfaceID, func->Meta);
         }
         else if (func->isScript()) {
            scCall(func, std::to_array<ScriptArg>({ { "SurfaceID", SurfaceID, FDF_OBJECTID } }), result);
         }
         else result = ERR::Okay;

         if (result IS ERR::Terminate) glWindowHooks.erase(hook);
         else if (result IS ERR::Cancelled) {
            log.msg("Window closure cancelled by client.");
            return;
         }
      }

      if (CheckMemoryExists(SurfaceID) IS ERR::Okay) FreeResource(SurfaceID);
   }
}

//********************************************************************************************************************

void MsgWindowDestroyed(OBJECTID SurfaceID)
{
   if (SurfaceID) {
      pf::Log log("WinMgr");
      log.branch("Freeing window surface #%d.", SurfaceID);
      FreeResource(SurfaceID);
   }
}

//********************************************************************************************************************

void MsgShowObject(OBJECTID ObjectID)
{
   acShow(ObjectID);
   acMoveToFront(ObjectID);
}

} // namespace
