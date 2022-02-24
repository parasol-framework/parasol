/*****************************************************************************

Notes
-----
* Use TrackMouseEvent() to receive notification of the mouse leaving a window.
* GetWindowThreadProcessId() can tell you the ID of the thread that created a window.  Also, GetWindowLongPtr() can
  give information on the HINSTANCE, HWNDPARENT, window ID.
* The win32 FindWindow() and FindWindowEx() functions can be used to retrieve foreign window handles.
* The IsWindow() function can be used to determine if a window handle is still valid.

*****************************************************************************/

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

//****************************************************************************

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

//****************************************************************************

void MsgMovement(OBJECTID SurfaceID, DOUBLE AbsX, DOUBLE AbsY, LONG WinX, LONG WinY)
{
   ERROR error;
   objPointer *pointer;
   if ((pointer = gfxAccessPointer())) {
      SetLong(pointer, FID_Surface, SurfaceID);  // Alter the surface of the pointer so that it refers to the correct root window
      ReleaseObject(pointer);

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
   else if (error IS ERR_NoMatchingObject) {
      glPointerID = 0;
   }
}

//****************************************************************************

void MsgWheelMovement(OBJECTID SurfaceID, FLOAT Wheel)
{
   if (!glPointerID) {
      LONG count = 1;
      if (FindObject("SystemPointer", 0, FOF_INCLUDE_SHARED, &glPointerID, &count) != ERR_Okay) return;
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

//****************************************************************************

void MsgFocusState(OBJECTID SurfaceID, LONG State)
{
   //LogMsg("Windows focus state for surface #%d: %d", SurfaceID, State);

   if (State) acFocusID(SurfaceID);
   else {
      acLostFocusID(SurfaceID);

      /*
      OBJECTID *list;
      WORD i;

      if (!AccessMemory(RPM_FocusList, MEM_READ_WRITE, &list)) {
         for (i=0; list[i]; i++) {
            LogMsg("Lost Focus: %d: %d", i, list[i]);
            acLostFocusID(list[i]);
         }
         list[0] = 0;
         ReleaseMemory(list);
      }
      */
   }
}

//****************************************************************************

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

//****************************************************************************

void MsgResizedWindow(OBJECTID SurfaceID, LONG WinX, LONG WinY, LONG WinWidth, LONG WinHeight,
   LONG ClientX, LONG ClientY, LONG ClientWidth, LONG ClientHeight)
{
   parasol::Log log("ResizedWindow");
   //log.branch("#%d, Window: %dx%d,%dx%d, Client: %dx%d,%dx%d", SurfaceID, WinX, WinY, WinWidth, WinHeight, ClientX, ClientY, ClientWidth, ClientHeight);

   if ((!SurfaceID) or (WinWidth < 1) or (WinHeight < 1)) return;

   objSurface *surface;
   if (!AccessObject(SurfaceID, 3000, &surface)) {
      objDisplay *display;
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

/*****************************************************************************
** We're interested in this message only when Windows soft-sets one of our windows.  A 'soft-set' means that our Window
** has received the focus without direct user interaction (typically a window on the desktop has closed and our
** window is inheriting the focus).
**
** Being able to tell the difference between a soft-set and a hard-set is difficult, but checking for visibility seems
** to be enough in preventing confusion.
*/

void MsgSetFocus(OBJECTID SurfaceID)
{
   parasol::Log log;
   objSurface *surface;
   if (!AccessObject(SurfaceID, 3000, &surface)) {
      if ((!(surface->Flags & RNF_HAS_FOCUS)) and (surface->Flags & RNF_VISIBLE)) {
         log.msg("WM_SETFOCUS: Sending focus to surface #%d.", SurfaceID);
         DelayMsg(AC_Focus, SurfaceID, 0);
      }
      else log.trace("WM_SETFOCUS: Surface #%d already has the focus, or is hidden.", SurfaceID);
      ReleaseObject(surface);
   }
}

//****************************************************************************
// The width and height arguments must reflect the dimensions of the client area.

void CheckWindowSize(OBJECTID SurfaceID, LONG *Width, LONG *Height)
{
   if ((!SurfaceID) or (!Width) or (!Height)) return;

   objSurface *surface;
   if (!AccessObject(SurfaceID, 3000, &surface)) {
      LONG minwidth, minheight, maxwidth, maxheight;
      LONG left, right, top, bottom;
      if (!GetFields(surface, FID_MinWidth|TLONG,     &minwidth,
                              FID_MinHeight|TLONG,    &minheight,
                              FID_MaxWidth|TLONG,     &maxwidth,
                              FID_MaxHeight|TLONG,    &maxheight,
                              FID_LeftMargin|TLONG,   &left,
                              FID_TopMargin|TLONG,    &top,
                              FID_BottomMargin|TLONG, &bottom,
                              FID_RightMargin|TLONG,  &right,
                              TAGEND)) {
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
      }
      ReleaseObject(surface);
   }
}

//****************************************************************************

extern "C" void RepaintWindow(OBJECTID SurfaceID, LONG X, LONG Y, LONG Width, LONG Height)
{
   if ((Width) and (Height)) {
      struct drwExpose expose = { X, Y, Width, Height, EXF_CHILDREN };
      ActionMsg(MT_DrwExpose, SurfaceID, &expose);
   }
   else ActionMsg(MT_DrwExpose, SurfaceID, NULL);
}

//****************************************************************************

void MsgTimer(void)
{
   ProcessMessages(0, 0);
}

//****************************************************************************

void MsgWindowClose(OBJECTID SurfaceID)
{
   if (SurfaceID) {
      OBJECTID owner_id;
      if ((owner_id = GetOwnerID(SurfaceID))) {
         if (GetClassID(owner_id) IS ID_WINDOW) {
            ActionMsg(MT_WinClose, owner_id, NULL);
            return;
         }
      }

      parasol::Log log("WinMgr");
      log.branch("Freeing window surface #%d.", SurfaceID);
      acFreeID(SurfaceID);
   }
}

//****************************************************************************

void MsgWindowDestroyed(OBJECTID SurfaceID)
{
   if (SurfaceID) {
      parasol::Log log("WinMgr");
      log.branch("Freeing window surface #%d.", SurfaceID);
      acFreeID(SurfaceID);
   }
}

//****************************************************************************

void MsgShowObject(OBJECTID ObjectID)
{
   acShowID(ObjectID);
   acMoveToFrontID(ObjectID);
}
