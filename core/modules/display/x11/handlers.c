
static void process_movement(Window Window, LONG X, LONG Y);

static inline OBJECTID get_display(Window Window)
{
   OBJECTID *data, display_id;
   unsigned long nitems, nbytes;
   int format;
   Atom atom;

   if (!XDisplay) return 0;

   if (XGetWindowProperty(XDisplay, Window, atomSurfaceID, 0, 1,
         False, AnyPropertyType, &atom, &format, &nitems, &nbytes, (APTR)&data) IS Success) {
      display_id = data[0];
      XFree(data);
      return display_id;
   }
   return 0;
}

//****************************************************************************

static void X11ManagerLoop(HOSTHANDLE FD, APTR Data)
{
   XEvent xevent;
   OBJECTID surface_id, display_id, owner_id, *list;
   WORD i;

   if (!XDisplay) return;

   while (XPending(XDisplay)) {
     XNextEvent(XDisplay, &xevent);
     //FMSG("Event", "%d", xevent.type);
     switch (xevent.type) {
         case ButtonPress:      handle_button_press(&xevent); break;
         case ButtonRelease:    handle_button_release(&xevent); break;
         case ConfigureNotify:  handle_configure_notify(&xevent.xconfigure); break;
         case EnterNotify:      handle_enter_notify(&xevent.xcrossing); break;
         case Expose:           handle_exposure(&xevent.xexpose); break;
         case KeyPress:         handle_key_press(&xevent); break;
         case KeyRelease:       handle_key_release(&xevent); break;
         case MotionNotify:     handle_motion_notify(&xevent.xmotion); break;
         case CirculateNotify:  handle_stack_change(&xevent.xcirculate); break;

         case FocusIn:
            if ((display_id = get_display(xevent.xany.window))) {
               surface_id = GetOwnerID(display_id);
               FMSG("XFocusIn","Surface: %d", surface_id);
               acFocusID(surface_id);
            }
            else FMSG("XFocusIn","Failed to get window display ID.");
            break;

         case FocusOut:
            FMSG("XFocusOut()","");
            if (!AccessMemory(RPM_FocusList, MEM_READ_WRITE, 1000, &list)) {
               for (i=0; list[i]; i++) {
                  acLostFocusID(list[i]);
               }
               list[0] = 0;
               ReleaseMemory(list);
            }
            break;

         case ClientMessage:
            if (xevent.xclient.data.l[0] IS XWADeleteWindow) {
               if ((display_id = get_display(xevent.xany.window))) {
                  surface_id = GetOwnerID(display_id);
                  if ((owner_id = GetOwnerID(surface_id))) {
                     if (GetClassID(owner_id) IS ID_WINDOW) {
                        ActionMsg(MT_WinClose, owner_id, NULL);
                        break;
                     }
                  }

                  LogErrorMsg("Freeing surface %d from display %d.", surface_id, display_id);
                  acFreeID(surface_id);
               }
               else {
                  LogMsg("Failed to retrieve display ID for window $%x.", (unsigned int)xevent.xany.window);
                  XDestroyWindow(XDisplay, (Window)xevent.xany.window);
               }
            }
            break;

         case DestroyNotify:
            if (glPlugin) {
               if ((display_id = get_display(xevent.xany.window))) {
                  surface_id = GetOwnerID(display_id);
                  acFreeID(surface_id);
               }
            }
            break;
      }

      if ((XRandRBase) AND (xrNotify(&xevent))) {
         // If randr indicates that the display has been resized, we must adjust the system display to match.  Refer to
         // SetDisplay() for more information.

         XRRScreenChangeNotifyEvent *notify;
         objDisplay *display;
         objSurface *surface;

         notify = (XRRScreenChangeNotifyEvent *)&xevent;

         if ((display_id = get_display(xevent.xany.window))) {
            surface_id = GetOwnerID(display_id);
            if (!AccessObject(surface_id, 5000, &surface)) {
               // Update the display width/height so that we don't recursively post further display mode updates to the
               // X server.

               if (!AccessObject(display_id, 5000, &display)) {
                  display->Width  = notify->width;
                  display->Height = notify->height;
                  acResize(surface, notify->width, notify->height, 0);
                  ReleaseObject(display);
               }
               ReleaseObject(surface);
            }
         }
      }
   }

   XFlush(XDisplay);
   if (XDisplay) XSync(XDisplay, False);
}

//****************************************************************************

static void handle_button_press(XEvent *xevent)
{
   struct acDataFeed feed;
   struct dcDeviceInput input;
   objPointer *pointer;
   DOUBLE value;

   FMSG("~handle_button_press()","Button: %d", xevent->xbutton.button);

   if ((xevent->xbutton.button IS 4) OR (xevent->xbutton.button IS 5)) {
      // Mouse wheel movement
      if (xevent->xbutton.button IS 4) value = -9;
      else value = 9;

      SET_DEVICE(&input, JET_WHEEL, JTYPE_EXT_MOVEMENT|JTYPE_DIGITAL, value, PreciseTime());

      feed.ObjectID = 0;
      feed.DataType = DATA_DEVICE_INPUT;
      feed.Buffer   = &input;
      feed.Size     = sizeof(input);
      ActionMsg(AC_DataFeed, glPointerID, &feed);
      LOGRETURN();
      return;
   }

   input.Type = 0;

   if ((pointer = gfxAccessPointer())) {
      if (xevent->xbutton.button IS 1) {
         input.Type  = JET_BUTTON_1;
         input.Value = 1;
      }
      else if (xevent->xbutton.button IS 2) {
         input.Type  = JET_BUTTON_3;
         input.Value = 1;
      }
      else if (xevent->xbutton.button IS 3) {
         input.Type  = JET_BUTTON_2;
         input.Value = 1;
      }
      ReleaseObject(pointer);
   }

   if (input.Type) {
      input.Flags = glInputType[input.Type].Flags;
      input.Timestamp = PreciseTime();

      feed.ObjectID = 0;
      feed.DataType = DATA_DEVICE_INPUT;
      feed.Buffer   = &input;
      feed.Size     = sizeof(input);
      if (ActionMsg(AC_DataFeed, glPointerID, &feed) IS ERR_NoMatchingObject) {
         glPointerID = NULL;
      }
   }

   XFlush(XDisplay);
   LOGRETURN();
}

//****************************************************************************

static void handle_button_release(XEvent *xevent)
{
   FMSG("~handle_button_release()","Button: %d", xevent->xbutton.button);

   if (!glPointerID) {
      if (FastFindObject("SystemPointer", NULL, &glPointerID, 1, NULL) != ERR_Okay) return;
   }

   struct dcDeviceInput input;
   struct acDataFeed feed;
   feed.ObjectID = 0;
   feed.DataType = DATA_DEVICE_INPUT;
   feed.Buffer   = &input;
   feed.Size     = sizeof(input);
   input.Type  = 0;
   input.Flags = 0;
   input.Value = 0;
   input.Timestamp = PreciseTime();

   objPointer *pointer;
   if ((pointer = gfxAccessPointer())) {
      if (xevent->xbutton.button IS 1) {
         input.Type  = JET_BUTTON_1;
         input.Value = 0;
      }
      else if (xevent->xbutton.button IS 2) {
         input.Type  = JET_BUTTON_3;
         input.Value = 0;
      }
      else if (xevent->xbutton.button IS 3) {
         input.Type  = JET_BUTTON_2;
         input.Value = 0;
      }
      ReleaseObject(pointer);
   }

   if (ActionMsg(AC_DataFeed, glPointerID, &feed) IS ERR_NoMatchingObject) {
      glPointerID = NULL;
   }

   XFlush(XDisplay);

   XSetInputFocus(XDisplay, xevent->xany.window, RevertToNone, CurrentTime);

   LOGRETURN();
}

//****************************************************************************

static void handle_stack_change(XCirculateEvent *xevent)
{
   MSG("Window %d stack position has changed.", (int)xevent->window);
}

//****************************************************************************

static void handle_configure_notify(XConfigureEvent *xevent)
{
   objDisplay *display;
   OBJECTID display_id;

   LONG x = xevent->x;
   LONG y = xevent->y;
   LONG width = xevent->width;
   LONG height = xevent->height;

   XEvent event;
   while (XCheckTypedWindowEvent(XDisplay, xevent->window, ConfigureNotify, &event) IS True) {
      x = event.xconfigure.x;
      y = event.xconfigure.y;
      width = event.xconfigure.width;
      height = event.xconfigure.height;
   }

   FMSG("XConfigureNotify()","Win: %d, Pos: %dx%d,%dx%d", (int)xevent->window, x, y, width, height);

   if ((display_id = get_display(xevent->window))) {
      // Delete expose events that were generated by X11 during the resize

      // REMOVED: Sometimes ConfigureNotify is called during a window mapping and by deleting expose events,
      // we may accidentally kill an expose that we actually need.
      //
      //while (XCheckTypedWindowEvent(XDisplay, xevent->window, Expose, &event) IS True);

      // Update the display dimensions

      if (!AccessObject(display_id, 3000, &display)) {
         Window childwin;
         LONG absx, absy;

         XTranslateCoordinates(XDisplay, (Window)display->WindowHandle, DefaultRootWindow(XDisplay), 0, 0, &absx, &absy, &childwin);

         display->X = absx;
         display->Y = absy;
         display->Width  = width;
         display->Height = height;
         acResize(display->Bitmap, width, height, 0.0);

         FUNCTION feedback = display->ResizeFeedback;

         ReleaseObject(display);

         // Notification occurs with the display and surface released so as to reduce the potential for dead-locking.

         FMSG("XConfigureNotify","Sending redimension notification: %dx%d,%dx%d", absx, absy, width, height);

         resize_feedback(&feedback, display_id, absx, absy, width, height);
      }
      else LogErrorMsg("Failed to get display ID for window %u.", (ULONG)xevent->window);
   }
   else LogErrorMsg("Failed to get display ID.");
}

//****************************************************************************

static void handle_exposure(XExposeEvent *event)
{
   OBJECTID display_id;

   if ((display_id = get_display(event->window))) {
      OBJECTID surface_id = GetOwnerID(display_id);

      XEvent xevent;
      while (XCheckWindowEvent(XDisplay, event->window, ExposureMask, &xevent) IS True);
      struct drwExpose region = { .X = 0, .Y = 0, .Width = 20000, .Height = 20000, .Flags = EXF_CHILDREN };
      DelayMsg(MT_DrwExpose, surface_id, &region); // Redraw everything
      return;
   }
   else LogErrorMsg("XEvent.Expose: Failed to find a Surface ID for window %u.", (ULONG)event->window);
}

//****************************************************************************
// XK symbols are defined in X11/keysymdef.h

static LONG xkeysym_to_pkey(KeySym KSym)
{
   switch(KSym) {
      case XK_A: return K_A;
      case XK_B: return K_B;
      case XK_C: return K_C;
      case XK_D: return K_D;
      case XK_E: return K_E;
      case XK_F: return K_F;
      case XK_G: return K_G;
      case XK_H: return K_H;
      case XK_I: return K_I;
      case XK_J: return K_J;
      case XK_K: return K_K;
      case XK_L: return K_L;
      case XK_M: return K_M;
      case XK_N: return K_N;
      case XK_O: return K_O;
      case XK_P: return K_P;
      case XK_Q: return K_Q;
      case XK_R: return K_R;
      case XK_S: return K_S;
      case XK_T: return K_T;
      case XK_U: return K_U;
      case XK_V: return K_V;
      case XK_W: return K_W;
      case XK_X: return K_X;
      case XK_Y: return K_Y;
      case XK_Z: return K_Z;
      case XK_a: return K_A;
      case XK_b: return K_B;
      case XK_c: return K_C;
      case XK_d: return K_D;
      case XK_e: return K_E;
      case XK_f: return K_F;
      case XK_g: return K_G;
      case XK_h: return K_H;
      case XK_i: return K_I;
      case XK_j: return K_J;
      case XK_k: return K_K;
      case XK_l: return K_L;
      case XK_m: return K_M;
      case XK_n: return K_N;
      case XK_o: return K_O;
      case XK_p: return K_P;
      case XK_q: return K_Q;
      case XK_r: return K_R;
      case XK_s: return K_S;
      case XK_t: return K_T;
      case XK_u: return K_U;
      case XK_v: return K_V;
      case XK_w: return K_W;
      case XK_x: return K_X;
      case XK_y: return K_Y;
      case XK_z: return K_Z;

      case XK_bracketleft:  return K_L_SQUARE;
      case XK_backslash:    return K_BACK_SLASH;
      case XK_bracketright: return K_R_SQUARE;
      case XK_asciicircum:  return K_SIX; // US conversion
      case XK_underscore:   return K_MINUS; // US conversion
      case XK_grave:        return K_REVERSE_QUOTE;
      case XK_space:        return K_SPACE;
      case XK_exclam:       return K_ONE; // US conversion
      case XK_quotedbl:     return K_APOSTROPHE; // US conversion
      case XK_numbersign:   return K_THREE; // US conversion
      case XK_dollar:       return K_FOUR; // US conversion
      case XK_percent:      return K_FIVE; // US conversion
      case XK_ampersand:    return K_SEVEN; // US conversion
      case XK_apostrophe:   return K_APOSTROPHE;
      case XK_parenleft:    return K_NINE; // US conversion
      case XK_parenright:   return K_ZERO; // US conversion
      case XK_asterisk:     return K_EIGHT; // US conversion
      case XK_plus:         return K_EQUALS; // US conversion
      case XK_comma:        return K_COMMA;
      case XK_minus:        return K_MINUS;
      case XK_period:       return K_PERIOD;
      case XK_slash:        return K_SLASH;
      case XK_0:            return K_ZERO;
      case XK_1:            return K_ONE;
      case XK_2:            return K_TWO;
      case XK_3:            return K_THREE;
      case XK_4:            return K_FOUR;
      case XK_5:            return K_FIVE;
      case XK_6:            return K_SIX;
      case XK_7:            return K_SEVEN;
      case XK_8:            return K_EIGHT;
      case XK_9:            return K_NINE;
      case XK_KP_0:         return K_NP_0;
      case XK_KP_1:         return K_NP_1;
      case XK_KP_2:         return K_NP_2;
      case XK_KP_3:         return K_NP_3;
      case XK_KP_4:         return K_NP_4;
      case XK_KP_5:         return K_NP_5;
      case XK_KP_6:         return K_NP_6;
      case XK_KP_7:         return K_NP_7;
      case XK_KP_8:         return K_NP_8;
      case XK_KP_9:         return K_NP_9;
      case XK_colon:        return K_SEMI_COLON; // US conversion
      case XK_semicolon:    return K_SEMI_COLON;
      case XK_less:         return K_COMMA; // US conversion
      case XK_equal:        return K_EQUALS;
      case XK_greater:      return K_PERIOD; // US conversion
      case XK_question:     return K_SLASH; // US conversion
      case XK_at:           return K_AT;
      case XK_KP_Multiply:  return K_NP_MULTIPLY;
      case XK_KP_Add:       return K_NP_PLUS;
      case XK_KP_Separator: return K_NP_BAR;
      case XK_KP_Subtract:  return K_NP_MINUS;
      case XK_KP_Decimal:   return K_NP_DOT;
      case XK_KP_Divide:    return K_NP_DIVIDE;
      case XK_KP_Enter:     return K_NP_ENTER;

      case XK_Shift_L:      return K_L_SHIFT;
      case XK_Shift_R:      return K_R_SHIFT;
      case XK_Control_L:    return K_L_CONTROL;
      case XK_Control_R:    return K_R_CONTROL;
      case XK_Caps_Lock:    return K_CAPS_LOCK;
      //case XK_Shift_Lock:   return K_SHIFT_LOCK;

      case XK_Meta_L:       return K_L_COMMAND;
      case XK_Meta_R:       return K_R_COMMAND;
      case XK_Alt_L:        return K_L_ALT;
      case XK_Alt_R:        return K_R_ALT;
      //case XK_Super_L:      return K_;
      //case XK_Super_R:      return K_;
      //case XK_Hyper_L:      return K_;
      //case XK_Hyper_R:      return K_;

      case XK_BackSpace:    return K_BACKSPACE;
      case XK_Tab:          return K_TAB;
      case XK_Linefeed:     return K_ENTER;
      case XK_Clear:        return K_CLEAR;
      case XK_Return:       return K_ENTER;
      case XK_Pause:        return K_PAUSE;
      case XK_Scroll_Lock:  return K_SCR_LOCK;
      case XK_Sys_Req:      return K_SYSRQ;
      case XK_Escape:       return K_ESCAPE;
      case XK_Delete:       return K_DELETE;

      case XK_Home:         return K_HOME;
      case XK_Left:         return K_LEFT;
      case XK_Up:           return K_UP;
      case XK_Right:        return K_RIGHT;
      case XK_Down:         return K_DOWN;
      case XK_Page_Up:      return K_PAGE_UP;
      case XK_Page_Down:    return K_PAGE_DOWN;
      case XK_End:          return K_END;

      case XK_Select:        return K_SELECT;
      //case XK_3270_PrintScreen: return K_PRT_SCR;
      case XK_Print:         return K_PRINT;
      case XK_Execute:       return K_EXECUTE;
      case XK_Insert:        return K_INSERT;
      case XK_Undo:          return K_UNDO;
      case XK_Redo:          return K_REDO;
      case XK_Menu:          return K_MENU;
      case XK_Find:          return K_FIND;
      case XK_Cancel:        return K_CANCEL;
      case XK_Help:          return K_HELP;
      case XK_Break:         return K_BREAK;
      case XK_Num_Lock:      return K_NUM_LOCK;
      //case XK_Mode_switch:   return K_;  /* Character set switch */
      //case XK_script_switch: return K_;  /* Alias for mode_switch */

      case XK_F1:           return K_F1;
      case XK_F2:           return K_F2;
      case XK_F3:           return K_F3;
      case XK_F4:           return K_F4;
      case XK_F5:           return K_F5;
      case XK_F6:           return K_F6;
      case XK_F7:           return K_F7;
      case XK_F8:           return K_F8;
      case XK_F9:           return K_F9;
      case XK_F10:          return K_F10;
      case XK_F11:          return K_F11;
      case XK_F12:          return K_F12;
      case XK_F13:          return K_F13;
      case XK_F14:          return K_F14;
      case XK_F15:          return K_F15;
      case XK_F16:          return K_F16;
      case XK_F17:          return K_F17;
      case XK_F18:          return K_F18;
      case XK_F19:          return K_F19;
      case XK_F20:          return K_F20;
      default: return 0;
   }
}

/*****************************************************************************
** Refer: man page XKeyEvent
*/

static void handle_key_press(XEvent *xevent)
{
   LONG unicode = 0;
   KeySym mod_sym; // A KeySym is an encoding of a symbol on the cap of a key.  See X11/keysym.h
   static XComposeStatus glXComposeStatus = { 0, 0 };
   char buffer[12];
   int out;
   if ((out = XLookupString(&xevent->xkey, buffer, sizeof(buffer)-1, &mod_sym, &glXComposeStatus)) > 0) {
      if (buffer[0] >= 0x20) {
         buffer[out] = 0;
         unicode = UTF8ReadValue(buffer, NULL);
      }
   }
   else if ((mod_sym = XkbKeycodeToKeysym(XDisplay, xevent->xkey.keycode, 0, xevent->xkey.state & ShiftMask ? 1 : 0)) != NoSymbol) {
   }
   else {
      FMSG("handle_key_press","Failed to convert keycode to keysym.");
      return;
   }

   KeySym sym = XkbKeycodeToKeysym(XDisplay, xevent->xkey.keycode, 0, 0);

   FMSG("~handle_key_press()","XCode: $%x, XSym: $%x, ModSym: $%x, XState: $%x", xevent->xkey.keycode, (int)sym, (int)mod_sym, xevent->xkey.state);

   LONG value = xkeysym_to_pkey(sym);
   LONG flags = KQ_PRESSED;

   if (xevent->xkey.state & LockMask) flags |= KQ_CAPS_LOCK;
   if (((value >= K_NP_0) AND (value <= K_NP_DIVIDE)) OR (value IS K_NP_ENTER)) {
      flags |= KQ_NUM_PAD;
   }

   if ((value) AND (value < ARRAYSIZE(KeyHeld))) {
      if (KeyHeld[value]) flags |= KQ_REPEAT;
      else KeyHeld[value] = 1;

      if (value IS K_L_COMMAND)       glKeyFlags |= KQ_L_COMMAND;
      else if (value IS K_R_COMMAND) glKeyFlags |= KQ_R_COMMAND;
      else if (value IS K_L_SHIFT)   glKeyFlags |= KQ_L_SHIFT;
      else if (value IS K_R_SHIFT)   glKeyFlags |= KQ_R_SHIFT;
      else if (value IS K_L_CONTROL) glKeyFlags |= KQ_L_CONTROL;
      else if (value IS K_R_CONTROL) glKeyFlags |= KQ_R_CONTROL;
      else if (value IS K_L_ALT)     glKeyFlags |= KQ_L_ALT;
      else if (value IS K_R_ALT)     glKeyFlags |= KQ_R_ALT;
   }

   if ((value) OR (unicode != 0xffffffff)) {
     if ((unicode < 0x20) OR (unicode IS 127)) flags |= KQ_NOT_PRINTABLE;
      evKey key = {
         .EventID    = EVID_IO_KEYBOARD_KEYPRESS,
         .Qualifiers = glKeyFlags|flags,
         .Code       = value,
         .Unicode    = unicode
      };
      BroadcastEvent(&key, sizeof(key));
   }

   LOGRETURN();
}

//****************************************************************************

static void handle_key_release(XEvent *xevent)
{
   // Check if the key is -really- released (when keys are held down, X11 annoyingly generates a stream of release
   // events until it is really released).

   if (XPending(XDisplay)) {
      XEvent peekevent;
      XPeekEvent(XDisplay, &peekevent);
      if ((peekevent.type IS KeyPress) AND
          (peekevent.xkey.keycode IS xevent->xkey.keycode) AND
          ((peekevent.xkey.time - xevent->xkey.time) < 2)) {
         // The key is held and repeated, so do not release it
         FMSG("handle_key_release","XKey $%x is held and repeated, not releasing.", xevent->xkey.keycode);
         return;
      }
   }

   // A KeySym is an encoding of a symbol on the cap of a key.  See X11/keysym.h

   ULONG unicode = 0;
   KeySym mod_sym;
   static XComposeStatus glXComposeStatus = { 0, 0 };
   char buf[12];
   int out;
   if ((out = XLookupString(&xevent->xkey, buf, sizeof(buf)-1, &mod_sym, &glXComposeStatus)) > 0) {
      buf[out] = 0;
      unicode = UTF8ReadValue(buf, NULL);
   }
   else if ((mod_sym = XkbKeycodeToKeysym(XDisplay, xevent->xkey.keycode, 0, xevent->xkey.state & ShiftMask ? 1 : 0)) != NoSymbol) {
   }
   else {
      FMSG("handle_key_release","XLookupString() failed to convert keycode to keysym.");
      return;
   }

   KeySym sym = XkbKeycodeToKeysym(XDisplay, xevent->xkey.keycode, 0, 0);

   LONG value = xkeysym_to_pkey(sym);
   LONG flags = KQ_RELEASED;

   if ((value) AND (value < ARRAYSIZE(KeyHeld))) {
      KeyHeld[value] = 0;

      if (value IS K_L_COMMAND)      glKeyFlags &= ~KQ_L_COMMAND;
      else if (value IS K_R_COMMAND) glKeyFlags &= ~KQ_R_COMMAND;
      else if (value IS K_L_SHIFT)   glKeyFlags &= ~KQ_L_SHIFT;
      else if (value IS K_R_SHIFT)   glKeyFlags &= ~KQ_R_SHIFT;
      else if (value IS K_L_CONTROL) glKeyFlags &= ~KQ_L_CONTROL;
      else if (value IS K_R_CONTROL) glKeyFlags &= ~KQ_R_CONTROL;
      else if (value IS K_L_ALT)     glKeyFlags &= ~KQ_L_ALT;
      else if (value IS K_R_ALT)     glKeyFlags &= ~KQ_R_ALT;
   }

  if ((value) OR (unicode != 0xffffffff)) {
     if ((unicode < 0x20) OR (unicode IS 127)) flags |= KQ_NOT_PRINTABLE;
      evKey key = {
         .EventID    = EVID_IO_KEYBOARD_KEYPRESS,
         .Qualifiers = glKeyFlags|flags,
         .Code       = value,
         .Unicode    = unicode
      };
      BroadcastEvent(&key, sizeof(key));
   }
}

//****************************************************************************

static void handle_enter_notify(XCrossingEvent *xevent)
{
   process_movement(xevent->window, xevent->x_root, xevent->y_root);
}

//****************************************************************************

static void handle_motion_notify(XMotionEvent *xevent)
{
   // If the user is moving the X11 pointer quite rapidly, a queue of motion events can build up very quickly.  Our
   // solution to this is to read all the motion events up to the most recent one, because we're only interested in the
   // most current position of the mouse pointer.

   XCrossingEvent enter_event;
   while (XCheckTypedEvent(XDisplay, EnterNotify, (XEvent *)&enter_event) IS True);
   while (XCheckTypedEvent(XDisplay, MotionNotify, (XEvent *)xevent) IS True);
   process_movement(xevent->window, xevent->x_root, xevent->y_root);
}

//****************************************************************************

static void process_movement(Window Window, LONG X, LONG Y)
{
   objPointer *pointer;

   if ((pointer = gfxAccessPointer())) {
      // Refer to the Pointer class to see how this works
      pointer->HostX = X;
      pointer->HostY = Y;

      OBJECTID display_id;
      if ((display_id = get_display(Window))) {
         SetLong(pointer, FID_Surface, GetOwnerID(display_id)); // Alter the surface of the pointer so that it refers to the correct root window
      }

      // Refer to the handler code in the Screen class to see how the HostX and HostY fields are updated from afar.

      struct acDataFeed feed;
      struct dcDeviceInput input[2];
      feed.ObjectID = 0;
      feed.DataType = DATA_DEVICE_INPUT;
      feed.Buffer   = &input;
      feed.Size     = sizeof(input);
      input[0].Type      = JET_ABS_X;
      input[0].Flags     = 0;
      input[0].Value     = X;
      input[0].Timestamp = PreciseTime();
      input[1].Type      = JET_ABS_Y;
      input[1].Flags     = 0;
      input[1].Value     = Y;
      input[1].Timestamp = input[0].Timestamp;
      Action(AC_DataFeed, pointer, &feed);

      ReleaseObject(pointer);
   }
}
