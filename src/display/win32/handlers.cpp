/*********************************************************************************************************************

Notes
-----
* Use TrackMouseEvent() to receive notification of the mouse leaving a window.
* GetWindowThreadProcessId() can tell you the ID of the thread that created a window.  Also, GetWindowLongPtr() can
  give information on the HINSTANCE, HWNDPARENT, window ID.
* The win32 FindWindow() and FindWindowEx() functions can be used to retrieve foreign window handles.
* The IsWindow() function can be used to determine if a window handle is still valid.

*********************************************************************************************************************/

void MsgKeyPress(LONG Flags, LONG Value, LONG Printable)
{
   if (!Value) return;

   if ((Printable < 0x20) or (Printable IS 127)) Flags |= KQ_NOT_PRINTABLE;

   evKey key = {
      .EventID    = EVID_IO_KEYBOARD_KEYPRESS,
      .Qualifiers = Flags|KQ_PRESSED,
      .Code       = Value,
      .Unicode    = Printable
   };
   BroadcastEvent(&key, sizeof(key));
}

//********************************************************************************************************************

void MsgKeyRelease(LONG Flags, LONG Value)
{
   if (!Value) return;

   evKey key = {
      .EventID    = EVID_IO_KEYBOARD_KEYPRESS,
      .Qualifiers = Flags|KQ_RELEASED,
      .Code       = Value,
      .Unicode    = 0
   };
   BroadcastEvent(&key, sizeof(key));
}

//********************************************************************************************************************

void MsgMovement(OBJECTID SurfaceID, DOUBLE AbsX, DOUBLE AbsY, LONG WinX, LONG WinY)
{
   if (auto pointer = gfxAccessPointer(); pointer) {
      pointer->set(FID_Surface, SurfaceID);  // Alter the surface of the pointer so that it refers to the correct root window
      gfxReleasePointer(pointer);

      struct dcDeviceInput joy[2];
      joy[0].Type  = JET_ABS_X;
      joy[0].Flags = 0;
      joy[0].Value = AbsX;
      joy[0].Timestamp = PreciseTime();

      joy[1].Type  = JET_ABS_Y;
      joy[1].Flags = 0;
      joy[1].Value = AbsY;
      joy[1].Timestamp = joy[0].Timestamp;

      struct acDataFeed feed = {
         .ObjectID = 0,
         .DataType = DATA_DEVICE_INPUT,
         .Buffer   = &joy,
         .Size     = sizeof(struct dcDeviceInput) * 2
      };
      ActionMsg(AC_DataFeed, glPointerID, &feed);
   }
}

//********************************************************************************************************************

void MsgWheelMovement(OBJECTID SurfaceID, FLOAT Wheel)
{
   if (!glPointerID) {
      if (FindObject("SystemPointer", 0, 0, &glPointerID) != ERR_Okay) return;
   }

   struct dcDeviceInput joy;
   joy.Type      = JET_WHEEL;
   joy.Flags     = 0;
   joy.Value     = Wheel;
   joy.Timestamp = PreciseTime();

   struct acDataFeed feed;
   feed.ObjectID = 0;
   feed.DataType = DATA_DEVICE_INPUT;
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

void MsgButtonPress(LONG button, LONG State)
{
   objPointer *pointer;

   if ((pointer = gfxAccessPointer())) {
      struct dcDeviceInput joy[3];

      LONG i = 0;
      LARGE timestamp = PreciseTime();

      if (button & 0x0001) {
         joy[i].Type  = JET_BUTTON_1;
         joy[i].Flags = 0;
         joy[i].Value = State;
         joy[i].Timestamp = timestamp;
         i++;
      }

      if (button & 0x0002) {
         joy[i].Type  = JET_BUTTON_2;
         joy[i].Flags = 0;
         joy[i].Value = State;
         joy[i].Timestamp = timestamp;
         i++;
      }

      if (button & 0x0004) {
         joy[i].Type  = JET_BUTTON_3;
         joy[i].Flags = 0;
         joy[i].Value = State;
         joy[i].Timestamp = timestamp;
         i++;
      }

      gfxReleasePointer(pointer);

      if (i) {
         struct acDataFeed feed;
         feed.ObjectID = 0;
         feed.DataType = DATA_DEVICE_INPUT;
         feed.Buffer   = &joy;
         feed.Size     = sizeof(struct dcDeviceInput) * i;
         if (ActionMsg(AC_DataFeed, glPointerID, &feed) IS ERR_NoMatchingObject) {
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
   if (!AccessObject(SurfaceID, 3000, &surface)) {
      extDisplay *display;
      OBJECTID display_id = surface->DisplayID;
      if (!AccessObject(display_id, 3000, &display)) {
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
   if (!AccessObject(SurfaceID, 3000, &surface)) {
      if ((!(surface->Flags & RNF_HAS_FOCUS)) and (surface->Flags & RNF_VISIBLE)) {
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
   if (!AccessObject(SurfaceID, 3000, &surface)) {
      LONG minwidth, minheight, maxwidth, maxheight;
      LONG left, right, top, bottom;
      surface->get(FID_MinWidth, &minwidth);
      surface->get(FID_MinHeight, &minheight);
      surface->get(FID_MaxWidth, &maxwidth);
      surface->get(FID_MaxHeight, &maxheight);
      surface->get(FID_LeftMargin, &left);
      surface->get(FID_TopMargin, &top);
      surface->get(FID_BottomMargin, &bottom);
      surface->get(FID_RightMargin, &right);

      if (*Width < minwidth + left + right)   *Width  = minwidth + left + right;
      if (*Height < minheight + top + bottom) *Height = minheight + top + bottom;
      if (*Width > maxwidth + left + right)   *Width  = maxwidth + left + right;
      if (*Height > maxheight + top + bottom) *Height = maxheight + top + bottom;

      if (surface->Flags & RNF_ASPECT_RATIO) {
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

extern "C" void RepaintWindow(OBJECTID SurfaceID, LONG X, LONG Y, LONG Width, LONG Height)
{
   if ((Width) and (Height)) {
      struct drwExpose expose = { X, Y, Width, Height, EXF_CHILDREN };
      ActionMsg(MT_DrwExpose, SurfaceID, &expose);
   }
   else ActionMsg(MT_DrwExpose, SurfaceID, NULL);
}

//********************************************************************************************************************

void MsgTimer(void)
{
   ProcessMessages(0, 0);
}

//********************************************************************************************************************

void MsgWindowClose(OBJECTID SurfaceID)
{
   pf::Log log(__FUNCTION__);

   if (SurfaceID) {
      const WindowHook hook(SurfaceID, WH_CLOSE);

      if (glWindowHooks.contains(hook)) {
         auto func = &glWindowHooks[hook];
         ERROR result;

         if (func->Type IS CALL_STDC) {
            pf::SwitchContext ctx(func->StdC.Context);
            auto callback = (ERROR (*)(OBJECTID SurfaceID))func->StdC.Routine;
            result = callback(SurfaceID);
         }
         else if (func->Type IS CALL_SCRIPT) {
            ScriptArg args[] = {
               { "SurfaceID", FDF_OBJECTID, { .Long = SurfaceID } }
            };
            scCallback(func->Script.Script, func->Script.ProcedureID, args, ARRAYSIZE(args), &result);
         }
         else result = ERR_Okay;

         if (result IS ERR_Terminate) glWindowHooks.erase(hook);
         else if (result IS ERR_Cancelled) {
            log.msg("Window closure cancelled by client.");
            return;
         }
      }

      FreeResource(SurfaceID);
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
