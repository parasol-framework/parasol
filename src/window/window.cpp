/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
Window: Creates user controllable windows on the desktop.

The Window class provides a simple API for the creation and management of application windows within the host's UI.
Windows are designed to act as containers that are physically represented by a @Surface.  The characteristics
of the Surface class are inherited by the window, thereby allowing the client to read and manipulate surface
fields (such as x, y, width and height) through the window object.
-END-

STYLE INFORMATION

It is typical to define preferred values for the window coordinates and set the resize parameters.  The window
margins should also be configured so that it is clear how much space around the edges of the window is for custom
graphics.  Here is an example:

<pre>
&lt;values&gt;
  &lt;resize value="left|bottomleft|bottom|bottomright|right"/&gt;
  &lt;xcoord value="[=[[self.parent].leftmargin]+10]"/&gt;
  &lt;ycoord value="[=[[self.parent].topmargin]+10]"/&gt;
  &lt;resizeborder value="4"/&gt;
  &lt;topmargin value="22"/&gt;
  &lt;leftmargin value="6"/&gt;
  &lt;rightmargin value="6"/&gt;
  &lt;bottommargin value="6"/&gt;
&lt;/values&gt;
</pre>

The bulk of the window graphics are defined in the 'graphics' tag.  You are expected to create graphics objects that
will be initialised directly to the window's surface.  The surface can be referenced via the [@owner] argument and
the window referenced through the [@window] argument.  Your script should also take note of the InsideBorder field
and create a window border if the border has a value of 1.  The following example illustrates:

<pre>
&lt;graphics&gt;
  &lt;box colour="[glStyle./colours/@colour]" shadow="[glStyle./colours/@shadow]"
    highlight="[glStyle./colours/@highlight]" raised/&gt;

  &lt;if statement="[[@window].insideborder] = 1"&gt;
    &lt;box sunken x="[=[owner.leftmargin]-1]" y="[=[owner.topmargin]-1]" xoffset="[=[owner.rightmargin]-1]"
      yoffset="[=[owner.bottommargin]-1]" highlight="#dfdfdf" shadow="#202020"/&gt;

    &lt;box sunken x="[=[owner.leftmargin]-2]" y="[=[owner.topmargin]-2]"
      xoffset="[=[owner.rightmargin]-2]" yoffset="[=[owner.bottommargin]-2]"
      highlight="#ffffff" shadow="#808080"/&gt;
  &lt;/if&gt;
&lt;/graphics&gt;
</pre>

The window title bar is configured in the 'titlebar' tag.  The title bar should allow the user to drag the window
around the display and provide a series of gadgets that control the window (such as minimise and maximise buttons).
You should also define an object that will control the window title.  You will need to communicate with the window
object for the purpose of determining the titlebar configuration, plus you will need to set certain window fields
for communication purposes.  For examples on how to generate the titlebar, please refer to your style:window.xml file.

Finally, a number of optional tags may be used for window control purposes.  These tags can contain Fluid scripts that
will be executed when certain actions occur to the window.  The following table describes the available tags:

<types type="Tag">
<type name="Maximise">Executed when the Maximise method is called on the window.</>
<type name="Minimise">Executed when the Minimise method is called on the window.</>
</table>

*****************************************************************************/

#define PRV_WINDOW
#define PRV_WINDOW_FIELDS \
   FUNCTION  CloseFeedback; \
   FUNCTION  MaximiseCallback; \
   FUNCTION  MinimiseCallback; \
   objXML    *TemplateXML; \
   OBJECTPTR Resize;            /* Resize object */ \
   OBJECTID  SurfaceID;         /* Surface region created by the window object */ \
   OBJECTID  InputID;           /* Keyboard object to use for receiving input */ \
   LONG      InsideWidth;       /* Internal window dimension.  For initialisation purposes only. */ \
   LONG      InsideHeight;      /* Internal window dimension.  For initialisation purposes only. */ \
   UBYTE     KeyboardChannel:1; /* TRUE if the keyboard is active */ \
   UBYTE     Shown:1;           /* TRUE if window is on display */ \
   char      Title[120];        /* Window title */ \
   char      Icon[60];          /* Window icon */ \
   char      Menu[180];         /* Location of an XML menu file */

//#define DEBUG

#include <parasol/modules/xml.h>
#include <parasol/modules/window.h>
#include <parasol/modules/surface.h>
#include <parasol/modules/display.h>

#undef NULL
#define NULL 0

struct CoreBase *CoreBase;
static struct SurfaceBase *SurfaceBase;
static struct DisplayBase *DisplayBase;
static OBJECTPTR clWindow = NULL;
static OBJECTPTR modSurface = NULL, modDisplay = NULL;
static OBJECTID glDefaultDisplay = NULL;
static LONG glDisplayType = 0;

struct VarString {
   STRING Field;
   STRING Value;
};

static ERROR add_window_class(void);
static void calc_surface_center(objWindow *, LONG *, LONG *);
static ERROR check_overlap(objWindow *, LONG *, LONG *, LONG *, LONG *);
static void smart_limits(objWindow *);
static void draw_border(objWindow *, objSurface *, objBitmap *);

//****************************************************************************

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   if (LoadModule("surface", MODVERSION_SURFACE, &modSurface, &SurfaceBase) != ERR_Okay) return ERR_InitModule;
   if (LoadModule("display", MODVERSION_DISPLAY, &modDisplay, &DisplayBase) != ERR_Okay) return ERR_InitModule;

   glDisplayType = gfxGetDisplayType();

   return add_window_class();
}

//****************************************************************************

static ERROR CMDExpunge(void)
{
   if (clWindow)   { acFree(clWindow);   clWindow   = NULL; }
   if (modSurface) { acFree(modSurface); modSurface = NULL; }
   if (modDisplay) { acFree(modDisplay); modDisplay = NULL; }
   return ERR_Okay;
}

//****************************************************************************

static ERROR WINDOW_AccessObject(objWindow *Self, APTR Void)
{
   if (Self->SurfaceID) {
      if (AccessObject(Self->SurfaceID, 4000, &Self->Surface) != ERR_Okay) {
         parasol::Log log;
         log.msg("Failed to access surface #%d.", Self->SurfaceID);
         Self->Surface = NULL;
      }
   }
   return ERR_Okay;
}

//****************************************************************************

static ERROR WINDOW_ActionNotify(objWindow *Self, struct acActionNotify *Args)
{
   if (!Args) return ERR_NullArgs;
   if (Args->Error != ERR_Okay) return ERR_Okay;

   if (Args->ActionID IS AC_Disable) {
      Self->Flags |= WNF_DISABLED;
      DelayMsg(AC_Draw, Self->SurfaceID, NULL);
   }
   else if (Args->ActionID IS AC_Enable) {
      Self->Flags &= ~WNF_DISABLED;
      DelayMsg(AC_Draw, Self->SurfaceID, NULL);
   }
   else if (Args->ActionID IS AC_Free) {
      if (Args->ObjectID IS Self->SurfaceID) {
         acFree(Self);
      }
      else if ((Self->MaximiseCallback.Type IS CALL_SCRIPT) and
               (Self->MaximiseCallback.Script.Script->UniqueID IS Args->ObjectID)) {
         Self->MaximiseCallback.Type = CALL_NONE;
      }
      else if ((Self->MinimiseCallback.Type IS CALL_SCRIPT) and
               (Self->MinimiseCallback.Script.Script->UniqueID IS Args->ObjectID)) {
         Self->MinimiseCallback.Type = CALL_NONE;
      }
   }
   else if ((Args->ActionID IS AC_Focus) and (Args->ObjectID IS Self->Surface->Head.UniqueID)) {
      if (!(Self->Head.Flags & NF_INITIALISED)) return ERR_Okay;

      parasol::Log log;
      log.traceBranch("Responding to window surface receiving the focus.");

      // Move the window to the front when the focus is received

      log.trace("Moving window to the front due to focus.");
      acMoveToFront(Self);

      // Ensure that the window is visible when the focus is received.  This only occurs if the surface is hidden
      // directly (surface.acHide was used and not window.acHide).

      if ((!(Self->Surface->Flags & RNF_VISIBLE)) and (Self->Shown)) {
         log.trace("Received focus, window hidden, will show.");
         acShow(Self);
      }

      // Check if a child wants the focus

      if (Self->UserFocusID) {
         // If the current focus has GRABFOCUS defined, do not attempt to divert the user focus.

         OBJECTID userfocus_id;
         BYTE grab = TRUE;
         if ((userfocus_id = drwGetUserFocus())) {
            LONG flags;
            if (!drwGetSurfaceFlags(userfocus_id, &flags)) {
               if (flags & RNF_GRAB_FOCUS) {
                  log.trace("Current focus surface #%d has GRAB flag set.", userfocus_id);
                  grab = FALSE;
               }
            }
         }

         if (grab) {
            log.trace("Passing primary focus through to #%d.", Self->UserFocusID);
            DelayMsg(AC_Focus, Self->UserFocusID, NULL);
         }
         else if (userfocus_id IS Self->UserFocusID) {
            // Reinstate the current focus in order to prevent it from being lost when the user clicks on a surface
            // that isn't defined with GRABFOCUS.

            log.trace("Passing primary focus through to #%d.", Self->UserFocusID);
            DelayMsg(AC_Focus, Self->UserFocusID, NULL);
         }
      }

      NotifySubscribers(Self, AC_Focus, NULL, NULL, ERR_Okay);
   }
   else if ((Args->ActionID IS MT_DrwInheritedFocus) and (Args->ObjectID IS Self->Surface->Head.UniqueID)) {
      // InheritedFocus is reported if one of the children in the window has received the focus.

      // If Inheritance->Flags has RNF_GRAB_FOCUS, the window updates its UserFocus field.  If it doesn't have
      // RNF_GRAB_FOCUS then the window calls DelayMsg(AC_Focus, Self->UserFocusID) to forcibly put the current
      // UserFocus back.

      parasol::Log log;
      struct drwInheritedFocus *inherit;
      if ((inherit = (struct drwInheritedFocus *)Args->Args)) {
         if (inherit->Flags & RNF_GRAB_FOCUS) {
            if (Self->UserFocusID != inherit->FocusID) {
               log.trace("(InheritedFocus) User focus switched to #%d from #%d.", inherit->FocusID, Self->UserFocusID);
               Self->UserFocusID = inherit->FocusID;
            }
         }
         else if ((Self->UserFocusID) and (Self->UserFocusID != inherit->FocusID)) {
            log.trace("(InheritedFocus) Focus reverting from requested #%d to #%d", inherit->FocusID, Self->UserFocusID);
            //DelayMsg(AC_Focus, Self->UserFocusID, NULL);
            SetField(Self, FID_RevertFocus, Self->UserFocusID);
         }
      }
   }
   else if (Args->ActionID IS AC_LostFocus) {
      NotifySubscribers(Self, AC_LostFocus, NULL, NULL, ERR_Okay);
   }
   else if (Args->ActionID IS AC_Redimension) {
      smart_limits(Self);
   }
   else return ERR_NoSupport;

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Activate: Shows the window.
-END-
*****************************************************************************/

static ERROR WINDOW_Activate(objWindow *Self, APTR Void)
{
   acShow(Self);
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
Close: Closes the window according to application requirements.

This method will initiate a window's close process.  This is preferred to a forced removal that would occur with
the #Free() action.  By using the Close method, the application will have the opportunity to respond in a way
that is appropriate for that window.

A client can receive a close notification by placing a feedback function in #CloseFeedback.  Alternatively
the #Quit field can be set to TRUE, in which case a QUIT message is sent to the task's message queue and
application closure will commence when that message is processed.

The close process can be completely disabled if the #Close field is set to FALSE.

-ERRORS-
Okay
-END-

*****************************************************************************/

static ERROR WINDOW_Close(objWindow *Self, APTR Void)
{
   parasol::Log log;

   if (!Self->Close) {
      log.msg("Window.Close is disabled.");
      return ERR_Okay; // Developer has requested that closing the window not be possible
   }

   if (Self->CloseFeedback.Type) {
      if (Self->CloseFeedback.Type IS CALL_STDC) {
         auto routine = (void (*)(objWindow *))Self->CloseFeedback.StdC.Routine;
         parasol::SwitchContext context(Self->CloseFeedback.StdC.Context);
         routine(Self);
      }
      else if (Self->CloseFeedback.Type IS CALL_SCRIPT) {
         OBJECTPTR script;
         if ((script = Self->CloseFeedback.Script.Script)) {
            const ScriptArg args[] = { { "Window", FD_OBJECTPTR, { .Address = Self } } };
            scCallback(script, Self->CloseFeedback.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
         }
      }
   }

   if (Self->Quit) {
      log.msg("Sending the application a quit message.");

      if (Self->Head.TaskID IS CurrentTaskID()) {
         SendMessage(NULL, MSGID_QUIT, NULL, NULL, NULL);
      }
      else {
         ListTasks *list;
         if (!ListTasks(NULL, &list)) {
            OBJECTID task_id = CurrentTaskID();
            for (LONG i=0; list[i].TaskID; i++) {
               if (list[i].TaskID IS task_id) {
                  SendMessage(list[i].MessageID, MSGID_QUIT, NULL, NULL, NULL);
                  break;
               }
            }
            FreeResource(list);
         }
      }
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Disable: Disables the user's ability to interact with the window.
-END-
*****************************************************************************/

static ERROR WINDOW_Disable(objWindow *Self, APTR Void)
{
   // See the ActionNotify routine to see what happens when the surface is disabled.

   acDisable(Self->Surface);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Enable: Enables user interactivity after prior disablement.
-END-
*****************************************************************************/

static ERROR WINDOW_Enable(objWindow *Self, APTR Void)
{
   // See the ActionNotify routine to see what happens when the surface is enabled.

   acEnable(Self->Surface);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Focus: Sets the user focus to the window's surface.
-END-
*****************************************************************************/

static ERROR WINDOW_Focus(objWindow *Self, APTR Void)
{
   // See our ActionNotify routine for notification support
   return acFocus(Self->Surface)|ERF_Notified;
}

//****************************************************************************

static ERROR WINDOW_Free(objWindow *Self, APTR Void)
{
   parasol::Log log;

   acHide(Self);

   if (Self->Surface) UnsubscribeAction(Self->Surface, NULL);

   if (Self->SurfaceID) {
      if (Self->Surface) {
         acFree(Self->Surface);
         if (Self->SurfaceID < 0) ReleaseObject(Self->Surface);
         Self->Surface = NULL;
      }
      else acFreeID(Self->SurfaceID);
      Self->SurfaceID = 0;
   }

   if (Self->Quit) {
      log.msg("Sending the application a quit message.");

      if (Self->Head.TaskID IS CurrentTaskID()) {
         SendMessage(0, MSGID_QUIT, 0, 0, 0);
      }
      else {
         OBJECTID task_id = CurrentTaskID();
         ListTasks *list;
         if (!ListTasks(0, &list)) {
            for (LONG i=0; list[i].TaskID; i++) {
               if (list[i].TaskID IS task_id) {
                  SendMessage(list[i].MessageID, MSGID_QUIT, 0, 0, 0);
                  break;
               }
            }
            FreeResource(list);
         }
      }
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Hide: Removes the window from the display.
-END-
*****************************************************************************/

static ERROR WINDOW_Hide(objWindow *Self, APTR Void)
{
   if (!Self->Surface) return ERR_Okay; // Sometimes there is no surface if this routine is called from Free()

   if (Self->Surface->Flags & RNF_HAS_FOCUS) {
      parasol::Log log;

      log.branch();

      acHide(Self->Surface);

      // Find the top-most window in our container and change the focus to it

      OBJECTID parent_id, window_id;
      if ((parent_id = Self->Surface->ParentID)) {
         SurfaceControl *ctl;
         if ((ctl = drwAccessList(ARF_READ))) {
            auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex + ((ctl->Total-1) * ctl->EntrySize));
            if (list->ParentID) {
               for (auto i=ctl->Total-1; i >= 0; i--, list=(SurfaceList *)((BYTE *)list - ctl->EntrySize)) {
                  if ((list->ParentID IS parent_id) and (list->SurfaceID != Self->Surface->Head.UniqueID)) {
                     if (list->Flags & RNF_VISIBLE) {
                        if ((window_id = GetOwnerID(list->SurfaceID))) {
                           if (GetClassID(window_id) IS ID_WINDOW) {
                              acFocusID(window_id);
                              break;
                           }
                        }
                     }
                  }
               }
            }

            drwReleaseList(ARF_READ);
         }
         else log.warning(ERR_AccessMemory);
      }
      else {
         // There are no other windows in our container - it's highly likely that we're in a hosted environment.

      }
   }
   else acHide(Self->Surface);

   Self->Shown = FALSE;
   return ERR_Okay;
}

//****************************************************************************

static ERROR WINDOW_Init(objWindow *Self, APTR Void)
{
   parasol::Log log;

   if (Self->Surface->PopOverID) {
      // If the surface that we're popping over has the stick-to-front flag set, then we also need to be stick-to-front
      // or else we'll end up being situated behind the window.

      log.trace("Checking if popover surface is stick-to-front");

      SURFACEINFO *info;
      if (!drwGetSurfaceInfo(Self->Surface->PopOverID, &info)) {
         if (info->Flags & RNF_STICK_TO_FRONT) {
            Self->Surface->Flags |= RNF_STICK_TO_FRONT;
         }
      }
   }

   // If @matchdpi is used, the window dimensions will be scaled so that on the display, it appears close to a physical
   // match to the target device.

   OBJECTID style_id;
   if (!FastFindObject("glStyle", ID_XML, &style_id, 1, NULL)) {
      objXML *style;
      if (!AccessObject(style_id, 3000, &style)) {
         char strdpi[32];
         if (!acGetVar(style, "/interface/@matchdpi", strdpi, sizeof(strdpi))) {
            char dummy[2];
            if (!acGetVar(style, "/interface/@dpi", dummy, sizeof(dummy))) {
               log.warning("/interface/@matchdpi and /interface/@dpi cannot be set together.  @matchdpi will be ignored.");
            }
            else {
               DISPLAYINFO *display;
               LONG mydpi = 96;
               if (!gfxGetDisplayInfo(0, &display)) mydpi = display->HDensity;
               LONG matchdpi = StrToInt(strdpi);
               Self->Surface->Width  = Self->Surface->Width * mydpi / matchdpi;
               Self->Surface->Height = Self->Surface->Height * mydpi / matchdpi;
            }
         }
         ReleaseObject(style);
      }
   }

   if (!Self->Surface->ParentID) {
      // There is no parent for the window object.

      if (Self->Flags & (WNF_NO_MARGINS|WNF_BORDERLESS)) {
         Self->Surface->LeftMargin   = 0;
         Self->Surface->TopMargin    = 0;
         Self->Surface->RightMargin  = 0;
         Self->Surface->BottomMargin = 0;
      }
      else {
         // When in hosted mode (Windows, X11), force default window margins
         Self->Surface->LeftMargin   = 6;
         Self->Surface->TopMargin    = 6;
         Self->Surface->RightMargin  = 6;
         Self->Surface->BottomMargin = 6;
      }

      // Allow video surface buffers when in full screen mode
      if (Self->Flags & WNF_VIDEO) Self->Surface->Flags |= RNF_VIDEO;
   }
   else { // If the NOMARGINS flag is used, adjust the margins to expectations
      if (Self->Flags & WNF_NO_MARGINS) {
         Self->Surface->LeftMargin   = 0;
         Self->Surface->TopMargin    = 0;
         Self->Surface->RightMargin  = 0;
         Self->Surface->BottomMargin = 0;
      }
   }

   if (Self->Surface->ParentID) {
      Self->Surface->Flags |= RNF_PERVASIVE_COPY;
      Self->Surface->Type |= RT_ROOT;
   }

   log.msg("Dimensions: %dx%d,%dx%d, Margins: %d,%d,%d,%d, Parent: %d", Self->Surface->X, Self->Surface->Y, Self->Surface->Width, Self->Surface->Height, Self->Surface->LeftMargin, Self->Surface->TopMargin, Self->Surface->RightMargin, Self->Surface->BottomMargin, Self->Surface->ParentID);

   if (!acInit(Self->Surface)) {
      SubscribeActionTags(Self->Surface,
         AC_Disable,
         AC_Enable,
         AC_Focus,
         AC_Free,
         AC_LostFocus,
         AC_Redimension,
         MT_DrwInheritedFocus,
         TAGEND);
   }

   if ((!Self->Surface->ParentID) and (Self->Surface->DisplayID)) { // On X11 and Windows, we need to retrieve the client border information from the host window.
      objDisplay *display;
      if (!AccessObject(Self->Surface->DisplayID, 3000, &display)) {
         Self->ClientLeft   = display->LeftMargin;
         Self->ClientRight  = display->RightMargin;
         Self->ClientTop    = display->TopMargin;
         Self->ClientBottom = display->BottomMargin;
         ReleaseObject(display);
      }
   }
   else {
      // On the native desktop, the margins will need to be adjusted to include the client area.  This is because
      // the main window surface includes all the client graphics.
      Self->Surface->LeftMargin   += Self->ClientLeft;
      Self->Surface->TopMargin    += Self->ClientTop;
      Self->Surface->RightMargin  += Self->ClientRight;
      Self->Surface->BottomMargin += Self->ClientBottom;
   }

   // Turn off the maximise gadget if the maximium and minimum values are equal

   if ((Self->Surface->MaxHeight IS Self->Surface->MinHeight) and (Self->Surface->MaxWidth IS Self->Surface->MinWidth)) {
      Self->Maximise = FALSE;
   }

   if (Self->Surface->ParentID) { // Run the graphics script
      ERROR error = drwApplyStyleGraphics(Self, Self->SurfaceID, NULL, NULL);

      if (!error) {
         error = drwApplyStyleGraphics(Self, Self->SurfaceID, "window", "titlebar");
         if (error) log.warning("Failed to process window titlebar graphics.");
      }
      else log.warning("Failed to process window style graphics.");

      if (error) return error;
   }
   else if (glDisplayType IS DT_NATIVE) {
      // The window is full-screen in the native environment.  Do not create background graphics when in full screen
      // mode, unless WNF_BACKGROUND is set, which indicates that a background is absolutely required.

      if (Self->Flags & WNF_BACKGROUND) {
         char colour[80] = "[glStyle./colours/@colour]";
         if ((!StrEvaluate(colour, sizeof(colour), 0, 0)) and (colour[0])) {
            SetString(Self->Surface, FID_Colour, colour);
         }
         else SetString(Self->Surface, FID_Colour, "230,230,230");
      }
   }
   else if ((!(Self->Flags & WNF_BORDERLESS)) or (Self->Flags & WNF_BACKGROUND)) {
      // This is the standard code for when a window has no parent (i.e. is not in the native desktop).

      char colour[80] = "[glStyle./colours/@colour]";
      if ((!StrEvaluate(colour, sizeof(colour), 0, 0)) and (colour[0])) {
         SetString(Self->Surface, FID_Colour, colour);
      }
      else SetString(Self->Surface, FID_Colour, "230,230,230");

      if (Self->InsideBorder) drwAddCallback(Self->Surface, (APTR)&draw_border);
   }

   if ((Self->ResizeFlags) and (Self->ResizeBorder > 0) and (Self->Surface->ParentID)) {
      CreateObject(ID_RESIZE, 0, &Self->Resize,
         FID_Object|TLONG,     Self->SurfaceID,
         FID_Surface|TLONG,    Self->SurfaceID,
         FID_BorderSize|TLONG, Self->ResizeBorder,
         FID_Border|TLONG,     Self->ResizeFlags,
         TAGEND);
   }

   // If we are running in a hosted environment, set the window titlebar

   if (!Self->Surface->ParentID) {
      OBJECTID display_id;
      if (!GetLong(Self->Surface, FID_Display, &display_id)) {
         OBJECTPTR display;
         if (!AccessObject(display_id, 4000, &display)) {
            SetString(display, FID_Title, Self->Title);
            ReleaseObject(display);
         }
      }
   }

   // If InsideWidth or InsideHeight were defined for initialisation, we need to correct the window size by taking
   // into account the client border values.

   if ((Self->InsideWidth) or (Self->InsideHeight)) {
      LONG width, height;

      if (Self->InsideWidth) {
         if ((!Self->Surface->ParentID) and (Self->Surface->DisplayID)) width = Self->InsideWidth;
         else width = Self->InsideWidth + Self->ClientLeft + Self->ClientRight;
      }
      else width = Self->Surface->Width;

      if (Self->InsideHeight) {
         if ((!Self->Surface->ParentID) and (Self->Surface->DisplayID)) height = Self->InsideHeight;
         else height = Self->InsideHeight + Self->ClientTop + Self->ClientBottom;
      }
      else height = Self->Surface->Height;

      acResize(Self->Surface, width, height, 0);
   }

   if (Self->Center) {
      // Move the window to the center of the display if centering is turned on
      LONG x, y;
      calc_surface_center(Self, &x, &y);
      if ((x != Self->Surface->X) or (y != Self->Surface->Y)) {
         acMoveToPoint(Self->Surface, x, y, 0, MTF_X|MTF_Y);
      }
   }

   // Check if the window's top left corner overlaps with another.  If so, adjust the coordinates slightly.

   LONG x = Self->Surface->X;
   LONG y = Self->Surface->Y;
   LONG width  = Self->Surface->Width;
   LONG height = Self->Surface->Height;
   if (check_overlap(Self, &x, &y, &width, &height) IS ERR_True) {
      acRedimension(Self->Surface, x, y, 0, width, height, 0);
   }

   // Ensure that the window is within the display area

   if (Self->Surface->ParentID) {
      SURFACEINFO *info;
      if (!drwGetSurfaceInfo(Self->Surface->ParentID, &info)) {
         // Check position against limits

         if (Self->Surface->X + Self->Surface->Width > info->Width - Self->Surface->RightLimit) {
            SetLong(Self->Surface, FID_X, info->Width - Self->Surface->RightLimit - Self->Surface->Width);
         }

         if (Self->Surface->Y + Self->Surface->Height > info->Height - Self->Surface->BottomLimit) {
            SetLong(Self->Surface, FID_Y, info->Height - Self->Surface->BottomLimit - Self->Surface->Height);
         }

         if (Self->Surface->X < Self->Surface->LeftLimit) {
            SetLong(Self->Surface, FID_X, Self->Surface->LeftLimit);
         }

         if (Self->Surface->Y < Self->Surface->TopLimit) {
            SetLong(Self->Surface, FID_Y, Self->Surface->TopLimit);
         }

         // Check position against basic width/height dimensions of the parent

         if (Self->Surface->X + Self->Surface->Width > info->Width) {
            SetLong(Self->Surface, FID_X, info->Width - Self->Surface->Width);
         }

         if (Self->Surface->Y + Self->Surface->Height > info->Height) {
            SetLong(Self->Surface, FID_Y, info->Height - Self->Surface->Height);
         }

         if (Self->Surface->X < 0) SetLong(Self->Surface, FID_X, 0);

         if (Self->Surface->Y < 0) SetLong(Self->Surface, FID_Y, 0);
      }
   }

   smart_limits(Self);

   // Recalculate the window center, just in case the dimensions of the window were modified between the opening and
   // closing tag statements.

   if (Self->Center) {
      LONG x, y;
      calc_surface_center(Self, &x, &y);
      if ((x != Self->Surface->X) or (y != Self->Surface->Y)) {
         acMoveToPoint(Self->Surface, x, y, 0, MTF_X|MTF_Y);
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
Maximise: Maximises the window to its available display area.

Executing the Maximise method will run the maximise procedure for the window. The specifics of the maximisation
process may be environment specific, but typically maximisation will result in the window being expanded to the
maximum dimensions allowed by its container.  The process will take into account the hints provided by the margin
settings of the parent surface (so if the margins are all set to 10, the window will be maximised to the size of
the container minus 10 pixels from its edge).

An optional Toggle argument allows restoration of the window to its original position if the method routine discovers
that the window is already maximised.

-INPUT-
int Toggle: Set to TRUE to toggle the window back to its original dimensions if the window is already maximised.

-ERRORS-
Okay: The window was maximised successfully.
AccessObject: Failed to access the window's parent surface.

*****************************************************************************/

static ERROR WINDOW_Maximise(objWindow *Self, struct winMaximise *Args)
{
   parasol::Log log;

   if (!Self->Maximise) {
      log.warning("Maximisation for this window is turned off.");
      return ERR_Okay;
   }

   if (Self->MaximiseCallback.Type) {
      if (Self->MaximiseCallback.Type IS CALL_STDC) {
         auto routine = (void (*)(objWindow *))Self->MaximiseCallback.StdC.Routine;
         parasol::SwitchContext context(Self->MaximiseCallback.StdC.Context);
         routine(Self);
      }
      else if (Self->MaximiseCallback.Type IS CALL_SCRIPT) {
         OBJECTPTR script;
         if ((script = Self->MaximiseCallback.Script.Script)) {
            const ScriptArg args[] = {
               { "Window", FD_OBJECTPTR, { .Address = Self } }
            };
            scCallback(script, Self->MaximiseCallback.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
         }
      }
   }

   if (!Self->Surface->ParentID) {
      // If the window is hosted, send the maximisation request to the display

      objDisplay *display;
      if (!AccessObject(Self->Surface->DisplayID, 3000, &display)) {
         SetLong(display, FID_Flags, display->Flags | SCR_MAXIMISE);
         ReleaseObject(display);
      }
      return ERR_Okay;
   }

   ClipRectangle margins;
   objSurface *parent;
   if (!AccessObject(Self->Surface->ParentID, 5000, &parent)) {
      if ((margins.Left = parent->LeftMargin) < 0) margins.Left = 0;
      if ((margins.Top = parent->TopMargin) < 0) margins.Top = 0;
      margins.Right  = parent->Width - parent->RightMargin;
      margins.Bottom = parent->Height - parent->BottomMargin;

      ReleaseObject(parent);

      LONG vx, vy, vwidth, vheight;
      if (drwGetVisibleArea(Self->Surface->ParentID, &vx, &vy, NULL, NULL, &vwidth, &vheight)) return ERR_Failed;

      LONG x = vx;
      LONG y = vy;
      LONG x2 = vx + vwidth;
      LONG y2 = vy + vheight;

      if (margins.Left > x)    x = margins.Left;
      if (margins.Top > y)     y = margins.Top;
      if (margins.Right < x2)  x2 = margins.Right;
      if (margins.Bottom < y2) y2 = margins.Bottom;

      if ((Args) and (Args->Toggle IS TRUE)) {
         log.msg("Toggle-check.");

         // If the window is already maximised, restore it

         if ((Self->RestoreWidth) and (Self->RestoreHeight)) {
            if ((Self->Surface->X IS x) and (Self->Surface->Y IS y) and
                (Self->Surface->Width IS (x2-x)) and (Self->Surface->Height IS (y2-y))) {
               log.msg("Restoring the window area.");
               acRedimension(Self->Surface, Self->RestoreX, Self->RestoreY, 0.0, Self->RestoreWidth, Self->RestoreHeight, 0.0);
               return ERR_Okay;
            }
         }
      }

      if (((x2-x) IS Self->Surface->Width) and ((y2-y) IS Self->Surface->Height)) {
         // If the window is already at the required width and height, simply move the window rather than going through
         // with the maximise process.

         acMoveToPoint(Self->Surface, x, y, 0.0, MTF_X|MTF_Y);
      }
      else {
         // Save current values

         Self->RestoreX = Self->Surface->X;
         Self->RestoreY = Self->Surface->Y;
         Self->RestoreWidth  = Self->Surface->Width;
         Self->RestoreHeight = Self->Surface->Height;

         log.trace("Maximising the window area.");

         acRedimension(Self->Surface, x, y, 0.0, x2-x, y2-y, 0.0);
      }
   }
   else return ERR_AccessObject;

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
Minimise: Minimises the size of the window, or hides it from the display.

Executing the Minimise method will run the minimise procedure for the window. The specifics of the minimisation
process are defined by the window style of the current environment.  It is typical for the window to iconify
itself to the desktop in some way, so that it is removed from the sight of the user.

-ERRORS-
Okay

*****************************************************************************/

static ERROR WINDOW_Minimise(objWindow *Self, APTR Void)
{
   parasol::Log log;

   if (Self->Minimise IS FALSE) return ERR_Okay;

   log.branch();

   if (Self->MinimiseCallback.Type) {
      if (Self->MinimiseCallback.Type IS CALL_STDC) {
         auto routine = (void (*)(objWindow *))Self->MinimiseCallback.StdC.Routine;
         parasol::SwitchContext context(Self->MinimiseCallback.StdC.Context);
         routine(Self);
      }
      else if (Self->MinimiseCallback.Type IS CALL_SCRIPT) {
         OBJECTPTR script;
         if ((script = Self->MinimiseCallback.Script.Script)) {
            const ScriptArg args[] = { { "Window", FD_OBJECTPTR, { .Address = Self } } };
            scCallback(script, Self->MinimiseCallback.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
         }
      }
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
MoveToBack: Moves the window to the back of the display area.
-END-
*****************************************************************************/

static ERROR WINDOW_Move(objWindow *Self, struct acMove *Args)
{
   return Action(AC_Move, Self->Surface, Args);
}

/*****************************************************************************
-ACTION-
MoveToBack: Moves the window to the back of the display area.
-END-
*****************************************************************************/

static ERROR WINDOW_MoveToBack(objWindow *Self, APTR Void)
{
   return Action(AC_MoveToBack, Self->Surface, NULL);
}

/*****************************************************************************
-ACTION-
MoveToFront: Moves the window to the front of the display area.
-END-
*****************************************************************************/

static ERROR WINDOW_MoveToFront(objWindow *Self, APTR Void)
{
   acMoveToFront(Self->Surface);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
MoveToPoint: Moves the window to preset coordinates.
-END-
*****************************************************************************/

static ERROR WINDOW_MoveToPoint(objWindow *Self, struct acMoveToPoint *Args)
{
   if ((Self->Surface->DisplayID) and (!Self->Surface->ParentID)) {
      return ActionMsg(AC_MoveToPoint, Self->Surface->DisplayID, Args);
   }
   else return Action(AC_MoveToPoint, Self->Surface, Args);
}

//****************************************************************************
// All new child objects are re-targeted to the Window surface.

static ERROR WINDOW_NewChild(objWindow *Self, struct acNewChild *Args)
{
   if (!(Self->Head.Flags & NF_INITIALISED)) return ERR_Okay;

   OBJECTPTR newchild;
   if (!AccessObject(Args->NewChildID, 4000, &newchild)) {
      SetOwner(newchild, Self->Surface);
      ReleaseObject(newchild);
      return ERR_OwnerPassThrough;
   }
   else return ERR_AccessObject;
}

//****************************************************************************

static ERROR WINDOW_NewObject(objWindow *Self, APTR Void)
{
   if ((!glDefaultDisplay) or (CheckObjectExists(glDefaultDisplay, NULL) != ERR_Okay)) {
      FastFindObject("Desktop", ID_SURFACE, &glDefaultDisplay, 1, NULL);
   }

   ERROR error;
   if (Self->Head.Flags & NF_PUBLIC) {
      error = NewLockedObject(ID_SURFACE, NF_INTEGRAL|Self->Head.Flags, &Self->Surface, &Self->SurfaceID);
   }
   else if (!(error = NewObject(ID_SURFACE, NF_INTEGRAL|Self->Head.Flags, &Self->Surface))) {
      Self->SurfaceID = GetUniqueID(Self->Surface);
   }

   if (error) return ERR_NewObject;

   SetFields(Self->Surface,
      FID_Name|TSTR,       "winsurface",
      FID_Parent|TLONG,    glDefaultDisplay,
      FID_Width|TLONG,     300,
      FID_Height|TLONG,    300,
      FID_MinWidth|TLONG,  80,
      FID_MinHeight|TLONG, 40,
      FID_MaxWidth|TLONG,  4096,
      FID_MaxHeight|TLONG, 4096,
      TAGEND);

   Self->ResizeBorder = 4;
   Self->Minimise     = TRUE;
   Self->Maximise     = TRUE;
   Self->MoveToBack   = TRUE;
   Self->Close        = TRUE;
   Self->Focus        = TRUE;
   Self->Quit         = TRUE;
   StrCopy("Window", Self->Title, sizeof(Self->Title));
   StrCopy("icons:devices/monitor", Self->Icon, sizeof(Self->Icon));

   drwApplyStyleValues(Self, NULL);
   return ERR_Okay;
}

//****************************************************************************

static ERROR WINDOW_NewOwner(objWindow *Self, struct acNewOwner *Args)
{
   // If the new owner of the window is a surface and we have not been through the initialisation process
   // yet, switch to that surface as our new window parent.

   if (!(Self->Head.Flags & NF_INITIALISED)) {
      if ((CLASSID)Args->ClassID IS ID_SURFACE) {
         Self->Surface->ParentID = Args->NewOwnerID;
      }
      else if (Args->ClassID IS ID_WINDOW) {
         objWindow *window;
         if (!AccessObject(Args->NewOwnerID, 4000, &window)) {
            Self->Surface->ParentID = window->SurfaceID;
            ReleaseObject(window);
         }
      }
   }
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Redimension: Changes the size and position of the window.
-END-
*****************************************************************************/

static ERROR WINDOW_Redimension(objWindow *Self, struct acRedimension *Args)
{
   if (!Self->Surface->ParentID) {
      struct acRedimension redim = *Args;
      redim.X += Self->ClientLeft;
      redim.Y += Self->ClientTop;
      redim.Width  = redim.Width - Self->ClientLeft - Self->ClientRight;
      redim.Height = redim.Height - Self->ClientTop - Self->ClientBottom;
      return Action(AC_Redimension, Self->Surface, &redim);
   }
   else return Action(AC_Redimension, Self->Surface, Args);
}

/*****************************************************************************
-ACTION-
Resize: Changes the size and position of the window.
-END-
*****************************************************************************/

static ERROR WINDOW_Resize(objWindow *Self, struct acResize *Args)
{
   if (!Self->Surface->ParentID) {
      struct acResize resize = *Args;
      resize.Width = resize.Width - Self->ClientLeft - Self->ClientRight;
      resize.Height = resize.Height - Self->ClientTop - Self->ClientBottom;
      return Action(AC_Resize, Self->Surface, &resize);
   }
   else return Action(AC_Resize, Self->Surface, Args);
}

//****************************************************************************

static ERROR WINDOW_ReleaseObject(objWindow *Self, APTR Void)
{
   if (Self->Surface) { ReleaseObject(Self->Surface); Self->Surface = NULL; }
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Show: Puts the window on display.
-END-
*****************************************************************************/

static ERROR WINDOW_Show(objWindow *Self, APTR Void)
{
   parasol::Log log;

   log.branch("%dx%d,%dx%d", Self->Surface->X, Self->Surface->Y, Self->Surface->Width, Self->Surface->Height);

   if (Self->Focus) {
      if (!(Self->Surface->Flags & RNF_HAS_FOCUS)) {
         acFocus(Self->Surface);
      }
   }

   acShow(Self->Surface);
   Self->Shown = TRUE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Canvas: Allocates a surface canvas inside the window when read.

To automatically allocate a surface inside a window after it has been initialised, read the Canvas field.  A basic
surface that matches the internal dimensions of the window will be created and its object ID will be returned as
the field value.

Adjustments to the canvas surface are permitted, however if modifications are extensive then it is recommended
that a suitable canvas is created manually.

The Canvas field can only be used following initialisation.  Once the window has been shown on the display, the
Canvas field will cease to operate in order to prevent other tasks from accidentally creating canvasses when this
field is read.

*****************************************************************************/

static ERROR GET_Canvas(objWindow *Self, OBJECTID *Value)
{
   parasol::Log log;

   if (!(Self->Head.Flags & NF_INITIALISED)) return log.warning(ERR_NotInitialised);

   if ((Self->CanvasID) or (Self->Shown)) {
      *Value = Self->CanvasID;
      return ERR_Okay;
   }

   OBJECTPTR surface;
   ERROR error;

   if (Self->Head.Flags & NF_PUBLIC) {
      error = NewLockedObject(ID_SURFACE, 0, &surface, &Self->CanvasID);
   }
   else {
      error = NewObject(ID_SURFACE, 0, &surface);
      Self->CanvasID = GetUniqueID(surface);
   }

   if (error) return ERR_NewObject;

   SetFields(surface,
      FID_Name|TSTR,     "winCanvas",
      FID_Parent|TLONG,  Self->Surface->Head.UniqueID,
      FID_X|TLONG,       Self->Surface->LeftMargin,
      FID_Y|TLONG,       Self->Surface->TopMargin,
      FID_XOffset|TLONG, Self->Surface->RightMargin,
      FID_YOffset|TLONG, Self->Surface->BottomMargin,
      TAGEND);

   if (!acInit(surface)) {
      acShow(surface);
      *Value = Self->CanvasID;
      error = ERR_Okay;
   }
   else error = ERR_Init;

   if (error) { acFree(surface); Self->CanvasID = 0; }
   ReleaseObject(surface);
   return error;
}

/*****************************************************************************

-FIELD-
Center: Displays the window in the center of the display if TRUE on initialisation.

-FIELD-
ClientBottom: The bottom coordinate of the client window (the zone that includes the window border).

-FIELD-
ClientLeft: The left-side coordinate of the client window (the zone that includes the window border).

-FIELD-
ClientRight: The right-side coordinate of the client window (the zone that includes the window border).

-FIELD-
ClientTop: The top coordinate of the client window (the zone that includes the window border).

-FIELD-
Close: Control switch for the window's close gadget.

The Close field controls the close widget and any activity that proceeds when the widget is activated.  The
default value for this field is TRUE.  If changed to FALSE, it will not be possible for the user to close the window
unless it is forcibly removed with the #Free() action.

*****************************************************************************/

static ERROR SET_Close(objWindow *Self, LONG Value)
{
   if (Value) {
      Self->Close = TRUE;
      if ((Self->Head.Flags & NF_INITIALISED) and (Self->CloseID)) {
         acEnableID(Self->CloseID);
      }
   }
   else {
      Self->Close = FALSE;
      if ((Self->Head.Flags & NF_INITIALISED) and (Self->CloseID)) {
         acDisableID(Self->CloseID);
      }
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
CloseFeedback: A callback for receiving notification of a dialog's user response.

The callback function set in this field will be called if the user attempts to close the window.  The prototype for the
function is `ERROR Function(*Window)`

*****************************************************************************/

static ERROR GET_CloseFeedback(objWindow *Self, FUNCTION **Value)
{
   if (Self->CloseFeedback.Type != CALL_NONE) {
      *Value = &Self->CloseFeedback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_CloseFeedback(objWindow *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->CloseFeedback.Type IS CALL_SCRIPT) UnsubscribeAction(Self->CloseFeedback.Script.Script, AC_Free);
      Self->CloseFeedback = *Value;
      if (Self->CloseFeedback.Type IS CALL_SCRIPT) SubscribeAction(Self->CloseFeedback.Script.Script, AC_Free);
   }
   else Self->CloseFeedback.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
CloseObject: The surface that controls the Close gadget.

The surface that represents the window's close gadget may be referenced through this field.  If no close gadget is
available then this field will be empty.

Theme Developers: The window style must set this field with the ID of the surface that has been created for the close
gadget.

-FIELD-
Flags: Optional flags may be defined here.

-FIELD-
Focus: May be set to FALSE if the window should open without automatically receiving the focus.

By default the window will be given the focus whenever it is shown on the display via the #Show() action.
If Focus is FALSE then this behaviour is disabled and the focus can be handled manually.

-FIELD-
Height: The full height of the window, including the top and bottom borders and titlebar.

*****************************************************************************/

static ERROR GET_Height(objWindow *Self, LONG *Value)
{
   if (Self->Surface->ParentID) *Value = Self->Surface->Height; // Height already includes client border graphics if in native desktop
   else *Value = Self->Surface->Height + Self->ClientTop + Self->ClientBottom; // Height requires the host's client border to be added.
   return ERR_Okay;
}

static ERROR SET_Height(objWindow *Self, LONG Value)
{
   if (Self->Surface->ParentID) return SetLong(Self->Surface, FID_Height, Value);
   else return SetLong(Self->Surface, FID_Height, Value - Self->ClientTop - Self->ClientBottom);
}

/*****************************************************************************

-FIELD-
Icon: A graphical icon to be applied to the window.

A suitable icon that will apply to the window may be specified here.  The window theme will choose how the
icon is used and displayed to the user.  It is common for windows to use the referenced icon in the window title bar
and when iconifying the window.

It is preferred that icon is chosen from the existing icon database (if a customised icon is required, install it to
the icon database on installation).  The window system will automatically scale the icon graphic to the required
dimensions when it is loaded.

*****************************************************************************/

static ERROR GET_Icon(objWindow *Self, STRING *Value)
{
   if (Self->Icon[0]) {
      *Value = Self->Icon;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Icon(objWindow *Self, CSTRING Value)
{
   if ((!Value) or (!Value[0])) return ERR_Okay;

   if (!StrCompare("icons:", Value, 0, NULL)) {
      StrCopy(Value, Self->Icon, sizeof(Self->Icon));
   }
   else {
      LONG i = StrCopy("icons:", Self->Icon, sizeof(Self->Icon));
      StrCopy(Value, Self->Icon + i, sizeof(Self->Icon)-i);
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
InsideBorder: Enables a custom graphics border that surrounds the internal window.

Set the InsideBorder field to TRUE to create a simple graphics border around the edges of the surface window area.  The
benefit of using this field to create a graphics border instead of a customised border is that it maintains consistency
with the other window graphics for the loaded environment.

-FIELD-
InsideHeight: Defines the amount of vertical space in the window's drawable area.

The InsideHeight defines the amount of vertical space, in pixels, of the drawable area inside the window.  That means
the entire window height, minus the size of any border decorations and title bar.

Please note that window margins have no effect on the calculated value.

*****************************************************************************/

static ERROR GET_InsideHeight(objWindow *Self, LONG *Value)
{
   if (Self->Surface->ParentID) *Value = Self->Surface->Height - Self->ClientTop - Self->ClientBottom; // Window is in the native desktop, so compensate for client border graphics
   else *Value = Self->Surface->Height; // Window is hosted, client border graphics do not apply.
   return ERR_Okay;
}

static ERROR SET_InsideHeight(objWindow *Self, LONG Value)
{
   Self->InsideHeight = Value;
   if (Self->Head.Flags & NF_INITIALISED) {
      if (Self->Surface->ParentID) { // Window is in the native desktop, so compensate for client border graphics.
         SetLong(Self->Surface, FID_Height, Value + Self->ClientTop + Self->ClientBottom);
      }
      else SetLong(Self->Surface, FID_Height, Value);
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
InsideWidth: The amount of horizontal space in the window's drawable area.

The InsideWidth defines the amount of horizontal space, in pixels, of the window's drawable area.  That means the
entire window width, minus the size of any border decorations.

Please note that window margins have no effect on the calculated value.

*****************************************************************************/

static ERROR GET_InsideWidth(objWindow *Self, LONG *Value)
{
   if (Self->Surface->ParentID) *Value = Self->Surface->Width - Self->ClientLeft - Self->ClientRight; // Window is in the native desktop, so compensate for client border graphics
   else *Value = Self->Surface->Width; // Window is hosted, client border graphics do not apply.
   return ERR_Okay;
}

static ERROR SET_InsideWidth(objWindow *Self, LONG Value)
{
   Self->InsideWidth = Value;
   if (Self->Head.Flags & NF_INITIALISED) {
      if (Self->Surface->ParentID) { // Window is in the native desktop, so compensate for client border graphics.
         SetLong(Self->Surface, FID_Width, Value + Self->ClientLeft + Self->ClientRight);
      }
      else SetLong(Self->Surface, FID_Width, Value);
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Maximise: Operator for the window's maximise gadget.

The Maximise field controls the activity of the maximise gadget, which is typically found in the window title bar.
The default value for the gadget is TRUE (environment dependent) and may be switched off at any time by setting
this field to FALSE.

*****************************************************************************/

static ERROR SET_Maximise(objWindow *Self, LONG Value)
{
   if (Value) {
      Self->Maximise = TRUE;
      if ((Self->Head.Flags & NF_INITIALISED) and (Self->MaximiseID)) {
         acEnableID(Self->MaximiseID);
      }
   }
   else {
      Self->Maximise = FALSE;
      if ((Self->Head.Flags & NF_INITIALISED) and (Self->MaximiseID)) {
         acDisableID(Self->MaximiseID);
      }
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
MaximiseObject: The surface that controls the maximise gadget.

The surface that represents the window's maximise gadget may be referenced through this field.  If no maximise gadget
is available then this field will be empty.

Theme Developers: The window style must set this field with the ID of the surface that has been created for the
maximise gadget.

-FIELD-
Menu: References the location of an XML menu file to be applied to the window.

To display a menu bar in the window, set this field with the location of a menu file.  The file must be in XML format
and contain at least one menu tag.  Each menu tag that is specified will create a new option in the menu bar.  Non-menu
tags will be ignored.

Any unlisted arguments that were have specified for the window will be passed on to each created menu.

For information on the specifics of menu customisation, please refer to the @Menu class.

*****************************************************************************/

static ERROR GET_Menu(objWindow *Self, STRING *Value)
{
   if (Self->Menu[0]) {
      *Value = Self->Menu;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Menu(objWindow *Self, CSTRING Value)
{
   StrCopy(Value, Self->Menu, sizeof(Self->Menu));
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Minimise: Operator for the title bar's minimise gadget.

The Minimise field controls the activity of the minimise gadget, which is typically found in the window title bar.  The
default value for the gadget is TRUE (environment dependent) and may be switched off at any time by setting this field
to FALSE.

*****************************************************************************/

static ERROR SET_Minimise(objWindow *Self, LONG Value)
{
   if (Value) {
      Self->Minimise = TRUE;
      if ((Self->Head.Flags & NF_INITIALISED) and (Self->MinimiseID)) {
         acEnableID(Self->MinimiseID);
      }
   }
   else {
      Self->Minimise = FALSE;
      if ((Self->Head.Flags & NF_INITIALISED) and (Self->MinimiseID)) {
         acDisableID(Self->MinimiseID);
      }
   }

   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
MaximiseCallback: Private. This routine will be called when a maximise event occurs.
-END-
*****************************************************************************/

static ERROR SET_MaximiseCallback(objWindow *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->MaximiseCallback.Type IS CALL_SCRIPT) UnsubscribeAction(Self->MaximiseCallback.Script.Script, AC_Free);
      Self->MaximiseCallback = *Value;
      if (Self->MaximiseCallback.Type IS CALL_SCRIPT) SubscribeAction(Self->MaximiseCallback.Script.Script, AC_Free);
   }
   else Self->MaximiseCallback.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
MinimiseCallback: Private. This routine will be called when a minimise event occurs.
-END-
*****************************************************************************/

static ERROR SET_MinimiseCallback(objWindow *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->MinimiseCallback.Type IS CALL_SCRIPT) UnsubscribeAction(Self->MinimiseCallback.Script.Script, AC_Free);
      Self->MinimiseCallback = *Value;
      if (Self->MinimiseCallback.Type IS CALL_SCRIPT) SubscribeAction(Self->MinimiseCallback.Script.Script, AC_Free);
   }
   else Self->MinimiseCallback.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
MinimiseObject: The surface that controls the minimise gadget.

The surface that represents the window's minimise gadget may be referenced through this field.  If no minimise gadget
is available then this field will be empty.

Theme Developers: The window style must set this field with the ID of the surface that has been created for the
minimise gadget.

-FIELD-
MoveToBack: Operator for the title bar's move-to-back gadget.

The MoveToBack field controls the activity of the move-to-back gadget, which is typically found in the window title
bar.  The default value for the gadget is TRUE (environment dependent) and may be switched off at any time by setting
this field to FALSE.

*****************************************************************************/

static ERROR SET_MoveToBack(objWindow *Self, LONG Value)
{
   if (Value) {
      Self->MoveToBack = TRUE;
      if ((Self->Head.Flags & NF_INITIALISED) and (Self->MoveToBackID)) {
         acEnableID(Self->MoveToBackID);
      }
   }
   else {
      Self->MoveToBack = FALSE;
      if ((Self->Head.Flags & NF_INITIALISED) and (Self->MoveToBackID)) {
         acDisableID(Self->MoveToBackID);
      }
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
MoveToBackObject: The surface that controls the move-to-back gadget.

The surface that represents the window's move-to-back gadget may be referenced through this field.  If no move-to-back
gadget is available then this field will be empty.

Theme Developers: The window style must set this field with the ID of the surface that has been created for the
move-to-back gadget.

-FIELD-
Orientation: The orientation to use for the display when the window is maximised.

The orientation allows for a preferred display orientation to be used when the window is maximised or operating in
full-screen mode.  This feature is typically used on mobile devices.  If an orientation is not defined, then the
window will use the user's preferred orientation (for hand-held devices, this can change dynamically according to how
the device is held).

*****************************************************************************/

static ERROR SET_Orientation(objWindow *Self, LONG Value)
{
   if ((Value >= 0) and (Value <= 2)) {
      Self->Orientation = Value;
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
}

/*****************************************************************************

-FIELD-
ParentHeight: The height of the window's parent surface.

The height of the window's parent surface can be read from this field.  On hosted systems such as Microsoft Windows,
the value will reflect the height of the user's desktop.

*****************************************************************************/

static ERROR GET_ParentHeight(objWindow *Self, LONG *Value)
{
   if (Self->Surface->ParentID) {
      LONG height;
      if (!drwGetSurfaceCoords(Self->Surface->ParentID, NULL, NULL, NULL, NULL, NULL, &height)) {
         *Value = height;
         return ERR_Okay;
      }
   }
   else {
      DISPLAYINFO *display;
      if (!gfxGetDisplayInfo(NULL, &display)) {
         *Value = display->Height;
         return ERR_Okay;
      }
   }
   return ERR_Failed;
}

/*****************************************************************************

-FIELD-
ParentWidth: The width of the window's parent surface.

The width of the window's parent surface can be read from this field.  On hosted systems such as Microsoft Windows, the
value will reflect the width of the user's desktop.

*****************************************************************************/

static ERROR GET_ParentWidth(objWindow *Self, LONG *Value)
{
   if (Self->Surface->ParentID) {
      LONG width;
      if (!drwGetSurfaceCoords(Self->Surface->ParentID, NULL, NULL, NULL, NULL, &width, NULL)) {
         *Value = width;
         return ERR_Okay;
      }
   }
   else {
      DISPLAYINFO *display;
      if (!gfxGetDisplayInfo(NULL, &display)) {
         *Value = display->Width;
         return ERR_Okay;
      }
   }
   return ERR_Failed;
}

/*****************************************************************************

-FIELD-
Quit: Set to FALSE to prevent application termination when the window is closed.

By default this field is set to TRUE, which will force the program to quit when the window is closed (the application
will be sent a QUIT message).  Set this field to FALSE to disable this behaviour.  Window closure can be detected
by subscribing to the #Close() method.

-FIELD-
Resize: Determines what sides of the window are resizeable.

This field defines what sides of the window are resizeable.  It can only be set from the style script that is
defined for the window.  The string format is defined as a series of flags separated with the or character.

-FIELD-
ResizeBorder: Defines the extent of the resize area at the sides of the window.

If the #Resize field is defined, the size of the border to use for each window edge can be specified here.
Recommended values range between 4 and 8 pixels.

This field can only be set from the style script that is defined for the window.

-FIELD-
RestoreHeight: Controls the height of the window when restoring to its previous state.

This field assists in the restoration of the window when it is minimised, maximised or has its dimensions changed in a
customised way.  The value that is set here will reflect the new height of the window when it is restored to its
'previous' state.  This field is provided for control purposes and should only be used from the window's style script.

-FIELD-
RestoreWidth: Controls the width of the window when restoring to its previous state.

This field assists in the restoration of the window when it is minimised, maximised or has its dimensions changed in a
customised way.  The value that is set here will reflect the new width of the window when it is restored to its
'previous' state.  This field is provided for control purposes and should only be used from the window's style script.

-FIELD-
RestoreX: Controls the horizontal position of the window when restoring to its previous state.

This field assists in the restoration of the window when it is minimised, maximised or has its dimensions changed in a
customised way.  The value that is set here will reflect the new horizontal coordinate of the window when it is
restored to its 'previous' state.  This field is provided for control purposes and should only be used from the
window's style script.

-FIELD-
RestoreY: Controls the vertical position of the window when restoring to its previous state.

This field assists in the restoration of the window when it is minimised, maximised or has its dimensions changed in a
customised way.  The value that is set here will reflect the new vertical coordinate of the window when it is
restored to its 'previous' state.  This field is provided for control purposes and should only be used from the window's
style script.

-FIELD-
StickToFront: Forces the window to stick to the front of the display.

Set this field to TRUE to force the window to stay at the front of its display.  This option should be used leniently.
It is recommended for use in small windows and presenting warning or error messages to the user.

*****************************************************************************/

static ERROR GET_StickToFront(objWindow *Self, LONG *Value)
{
   if (Self->Surface->Flags & RNF_STICK_TO_FRONT) *Value = TRUE;
   else *Value = FALSE;
   return ERR_Okay;
}

static ERROR SET_StickToFront(objWindow *Self, LONG Value)
{
   SetLong(Self->Surface, FID_Flags, Self->Surface->Flags | RNF_STICK_TO_FRONT);
   if (Value) Self->MoveToBack = FALSE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Surface: The window's surface.

The surface that represents the window on the display can be retrieved through this field.  The reference is a direct
pointer to the @Surface object in question. It is possible to manipulate the surface without encountering
adverse effects with the window object, although it is wise to refrain from doing so where functionality is duplicated
in the Window class.  For instance, #Hide() and #Show() actions should not be called on the surface object as this
functionality is managed by the window system.

-FIELD-
Title: The title string to display in the window's title bar.

The Title field controls the text that is displayed in the window's title bar.  Set this field at any time in
order to change the title that is displayed inside the window.  A standard UTF-8 text string is acceptable and will be
trimmed if it exceeds the maximum number of possible characters.

*****************************************************************************/

static ERROR GET_Title(objWindow *Self, STRING *Value)
{
   if (Self->Title[0]) {
      *Value = Self->Title;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Title(objWindow *Self, CSTRING Value)
{
   parasol::Log log;

   log.branch("%s", Value);

   if (Value) StrCopy(StrTranslateText(Value), Self->Title, sizeof(Self->Title));
   else Self->Title[0] = 0;

   if (Self->TitleID) {
      OBJECTPTR title;
      if (!AccessObject(Self->TitleID, 5000, &title)) {
         SetString(title, FID_String, Self->Title);
         ReleaseObject(title);
      }
   }

   if (Self->Head.Flags & NF_INITIALISED) {
      if (!Self->Surface->ParentID) {
         OBJECTID display_id;
         if (!GetLong(Self->Surface, FID_Display, &display_id)) {
            OBJECTPTR display;
            if (!AccessObject(display_id, 4000, &display)) {
               SetString(display, FID_Title, Self->Title);
               ReleaseObject(display);
            }
         }
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
TitleObject: The object that controls the window title is referenced here.

The object that controls the window's title bar text is referenced through this field.  If no title bar is configured
then this field will be empty.

Theme Developers: The window style must set this field with the ID of the object that manages the title bar.  A Text
object is typically used for this purpose.

-FIELD-
UserFocus: Indicates the object that should have the user focus when the window is active.

The UserFocus field is internally managed by the Window class.  It is acceptable to read this value to get the ID of
the object that has the user focus.

-FIELD-
Width: The width of the window.

This field defines the width of the window area.  It includes the window borders imposed by the desktop, if present.
To get the internal width of the window excluding the borders, use #InsideWidth.

*****************************************************************************/

static ERROR GET_Width(objWindow *Self, LONG *Value)
{
   if (Self->Surface->ParentID) *Value = Self->Surface->Width; // Width already includes client border graphics if in native desktop
   else *Value = Self->Surface->Width + Self->ClientLeft + Self->ClientRight; // Width requires the host's client border to be added.
   return ERR_Okay;
}

static ERROR SET_Width(objWindow *Self, LONG Value)
{
   if (Self->Surface->ParentID) return SetLong(Self->Surface, FID_Width, Value);
   else return SetLong(Self->Surface, FID_Width, Value - Self->ClientLeft - Self->ClientRight);
}

/*****************************************************************************
-FIELD-
X: The horizontal position of the window, relative to its container.

*****************************************************************************/

static ERROR GET_X(objWindow *Self, LONG *Value)
{
   if (Self->Surface->ParentID) *Value = Self->Surface->X;
   else *Value = Self->Surface->X - Self->ClientLeft; // Adjust for client border on hosted displays.
   return ERR_Okay;
}

static ERROR SET_X(objWindow *Self, LONG Value)
{
   if (Self->Surface->ParentID) return SetLong(Self->Surface, FID_X, Value);
   else return SetLong(Self->Surface, FID_X, Value + Self->ClientLeft);
}

/*****************************************************************************
-FIELD-
Y: The vertical position of the window, relative to its container.
-END-
*****************************************************************************/

static ERROR GET_Y(objWindow *Self, LONG *Value)
{
   if (Self->Surface->ParentID) *Value = Self->Surface->Y;
   else *Value = Self->Surface->Y - Self->ClientTop; // Adjust for client border on hosted displays.
   return ERR_Okay;
}

static ERROR SET_Y(objWindow *Self, LONG Value)
{
   if (Self->Surface->ParentID) return SetLong(Self->Surface, FID_Y, Value);
   else return SetLong(Self->Surface, FID_Y, Value + Self->ClientTop);
}

//****************************************************************************

static ERROR check_overlap(objWindow *Self, LONG *X, LONG *Y, LONG *Width, LONG *Height)
{
   if (Self->Flags & WNF_FORCE_POS) return ERR_False;
   if (!Self->Surface->ParentID) return ERR_False;

   LONG x = *X;
   LONG y = *Y;

   if (x < 0) x = 0;
   if (y < 0) y = 0;

   SurfaceControl *ctl;
   if ((ctl = drwAccessList(ARF_READ))) {
restart:
      {
         auto surfacelist = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
         auto list = surfacelist;
         for (LONG i=0; i < ctl->Total; i++, list=(SurfaceList *)((BYTE *)list + ctl->EntrySize)) {
            if (list->ParentID IS Self->Surface->ParentID) {
               if ((list->X IS x) and (list->Y IS y)) {
                  x += 20;
                  y += 20;
                  goto restart;
               }
            }
         }

         if ((x != Self->Surface->X) or (y != Self->Surface->Y)) {
            list = surfacelist;
            for (LONG i=0; i < ctl->Total; i++, list=(SurfaceList *)((BYTE *)list + ctl->EntrySize)) {
               if (list->SurfaceID IS Self->Surface->ParentID) break;
            }

            LONG list_width = list->Width;
            LONG list_height = list->Height;
            drwReleaseList(ARF_READ);

            if ((x + Self->Surface->Width < list_width) and (y + Self->Surface->Height < list_height)) {
               acMoveToPoint(Self->Surface, x, y, 0, MTF_X|MTF_Y);
            }
         }
         else drwReleaseList(ARF_READ);
      }
   }

   // Check the bounds of the window - this is mainly for applications that simply can't behave themselves when it
   // comes to window positioning.

   if ((Width) and (Height)) {
      if ((x < 0) and (x + *Width > 0)) x = 0; // The window is partially outside the parent surface
      if ((y < 0) and (y + *Height > 0)) y = 0;

      LONG vx, vy, vwidth, vheight;
      if (!drwGetVisibleArea(Self->Surface->ParentID, &vx, &vy, NULL, NULL, &vwidth, &vheight)) {
         if (*Width > vwidth) *Width = vwidth; // The window is wider than the visible display
         if (*Height > vheight) *Height = vheight; // The window is taller than the visible display

         if (x + *Width > vx + vwidth) {  // Window is within the desktop zone, but is partially outside of the visible frame.
            x = vx + vwidth - *Width;
         }

         if (y + *Height > vy + vheight) {
            y = vy + vheight - *Height;
         }

         if ((x >= 0) and (x < vx)) {  // Window is within the desktop zone, but is partially outside of the visible frame.
            if (x <= 100) x = vx + x;
            else x = vx;
         }

         if ((y >= 0) and (y < vy)) {
            if (y <= 100) y = vy + y;
            else y = vy;
         }
      }
   }

   if ((x != *X) or (y != *Y)) {
      *X = x;
      *Y = y;
      return ERR_True;
   }
   else return ERR_False;
}

//****************************************************************************

static void calc_surface_center(objWindow *Self, LONG *X, LONG *Y)
{
   parasol::Log log(__FUNCTION__);

   if (Self->Surface->PopOverID) {
      log.msg("Centering the window [PopOver]");

      LONG x, y, width, height;
      if (!drwGetSurfaceCoords(Self->Surface->PopOverID, NULL, NULL, &x, &y, &width, &height)) {
         *X = x + ((width - Self->Surface->Width)>>1);
         *Y = y + ((height - Self->Surface->Height)>>1);

         if (!drwGetSurfaceCoords(Self->Surface->ParentID, NULL, NULL, &x, &y, NULL, NULL)) {
            *X -= x;
            *Y -= y;
         }
      }
   }
   else if (Self->Surface->ParentID) {
      log.msg("Centering the window [Within Parent]");
      LONG vx, vy, vwidth, vheight;
      if (!drwGetVisibleArea(Self->Surface->ParentID, &vx, &vy, NULL, NULL, &vwidth, &vheight)) {
         *X = vx + ((vwidth - Self->Surface->Width)>>1);
         *Y = vy + ((vheight - Self->Surface->Height)>>1);
         check_overlap(Self, X, Y, 0, 0);
      }
   }
   else {
      DISPLAYINFO *display;
      log.msg("Centering the window [Within Host]");
      if (!gfxGetDisplayInfo(0, &display)) {
         *X = (display->Width - Self->Surface->Width) / 2;
         *Y = (display->Height - Self->Surface->Height) / 2;
      }
   }
}

/*****************************************************************************
** Smart limits are used to prevent the window from moving outside of the visible display area.
*/

static void smart_limits(objWindow *Self)
{
   if ((Self->Flags & WNF_SMART_LIMITS) and (Self->Surface->ParentID)) {
      SURFACEINFO *info;
      if (!drwGetSurfaceInfo(Self->Surface->ParentID, &info)) {
         Self->Surface->TopLimit    = 0;
         Self->Surface->BottomLimit = -Self->Surface->Height + Self->Surface->TopMargin;
         Self->Surface->LeftLimit   = -(Self->Surface->Width * 0.75);
         Self->Surface->RightLimit  = -(Self->Surface->Width * 0.75);
      }
   }
}

//****************************************************************************

static void draw_border(objWindow *Self, objSurface *Surface, objBitmap *Bitmap)
{
   LONG lm = Surface->LeftMargin - 1;
   LONG tm = Surface->TopMargin - 1;
   LONG rm = Surface->Width - Surface->RightMargin + 1;
   LONG bm = Surface->Height - Surface->BottomMargin + 1;

   static RGB8 highlightA = { .Red = 255, .Green = 255, .Blue = 255, .Alpha = 0x70 };
   static RGB8 highlightB = { .Red = 255, .Green = 255, .Blue = 255, .Alpha = 0xa0 };
   static RGB8 shadowA    = { .Red = 0, .Green = 0, .Blue = 0, .Alpha = 0x80 };
   static RGB8 shadowB    = { .Red = 0, .Green = 0, .Blue = 0, .Alpha = 0x40 };

   // Top, Bottom, Left, Right
   ULONG shadow    = PackPixelRGBA(Bitmap, &shadowA);
   ULONG highlight = PackPixelRGBA(Bitmap, &highlightA);

   gfxDrawRectangle(Bitmap, lm, tm, rm-lm, 1, shadow, BAF_FILL|BAF_BLEND);
   gfxDrawRectangle(Bitmap, lm, bm, rm-lm, 1, highlight, BAF_FILL|BAF_BLEND);
   gfxDrawRectangle(Bitmap, lm, tm, 1, bm-tm, shadow, BAF_FILL|BAF_BLEND);
   gfxDrawRectangle(Bitmap, rm, tm, 1, bm-tm, highlight, BAF_FILL|BAF_BLEND);

   // Top, Bottom, Left, Right
   shadow    = PackPixelRGBA(Bitmap, &shadowB);
   highlight = PackPixelRGBA(Bitmap, &highlightB);
   lm--; tm--; rm++; bm++;
   gfxDrawRectangle(Bitmap, lm, tm, rm-lm, 1, shadow, BAF_FILL|BAF_BLEND);
   gfxDrawRectangle(Bitmap, lm, bm, rm-lm, 1, highlight, BAF_FILL|BAF_BLEND);
   gfxDrawRectangle(Bitmap, lm, tm, 1, bm-tm, shadow, BAF_FILL|BAF_BLEND);
   gfxDrawRectangle(Bitmap, rm, tm, 1, bm-tm, highlight, BAF_FILL|BAF_BLEND);
}

//****************************************************************************

#include "window_def.c"

static const FieldArray clWindowFields[] = {
   { "Surface",          FDF_INTEGRAL|FDF_R,   ID_SURFACE,NULL, NULL },
   { "Flags",            FDF_LONGFLAGS|FDF_RW, (MAXINT)&clWindowFlags,NULL, NULL },
   { "InsideBorder",     FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "Center",           FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "Minimise",         FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_Minimise },
   { "Maximise",         FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_Maximise },
   { "MoveToBack",       FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_MoveToBack },
   { "Close",            FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_Close },
   { "Quit",             FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "RestoreX",         FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "RestoreY",         FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "RestoreWidth",     FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "RestoreHeight",    FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "Focus",            FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "TitleObject",      FDF_OBJECTID|FDF_RI,  0, NULL, NULL },
   { "MinimiseObject",   FDF_OBJECTID|FDF_RI,  0, NULL, NULL },
   { "MaximiseObject",   FDF_OBJECTID|FDF_RI,  0, NULL, NULL },
   { "MoveToBackObject", FDF_OBJECTID|FDF_RI,  0, NULL, NULL },
   { "CloseObject",      FDF_OBJECTID|FDF_RI,  0, NULL, NULL },
   { "Resize",           FDF_LONGFLAGS|FDF_I,  (MAXINT)&clWindowResizeFlags, NULL, NULL },
   { "ResizeBorder",     FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "Canvas",           FDF_OBJECTID|FDF_R,   0, (APTR)GET_Canvas, NULL },
   { "UserFocus",        FDF_OBJECTID|FDF_RW,  0, NULL, NULL },
   { "Orientation",      FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clWindowOrientation, NULL, (APTR)SET_Orientation },
   { "ClientLeft",       FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "ClientRight",      FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "ClientTop",        FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "ClientBottom",     FDF_LONG|FDF_RI,      0, NULL, NULL },
   // Virtual fields
   { "CloseFeedback", FDF_FUNCTIONPTR|FDF_RW, 0, (APTR)GET_CloseFeedback, (APTR)SET_CloseFeedback },
   { "MinimiseCallback", FDF_FUNCTIONPTR|FDF_I, 0, NULL, (APTR)SET_MinimiseCallback },
   { "MaximiseCallback", FDF_FUNCTIONPTR|FDF_I, 0, NULL, (APTR)SET_MaximiseCallback },
   { "Icon",          FDF_STRING|FDF_RW, 0, (APTR)GET_Icon,         (APTR)SET_Icon },
   { "Menu",          FDF_STRING|FDF_RW, 0, (APTR)GET_Menu,         (APTR)SET_Menu },
   { "InsideWidth",   FDF_LONG|FDF_RW,   0, (APTR)GET_InsideWidth,  (APTR)SET_InsideWidth },
   { "InsideHeight",  FDF_LONG|FDF_RW,   0, (APTR)GET_InsideHeight, (APTR)SET_InsideHeight },
   { "ParentWidth",   FDF_LONG|FDF_R,    0, (APTR)GET_ParentWidth,  NULL },
   { "ParentHeight",  FDF_LONG|FDF_R,    0, (APTR)GET_ParentHeight, NULL },
   { "StickToFront",  FDF_LONG|FDF_RW,   0, (APTR)GET_StickToFront, (APTR)SET_StickToFront },
   { "Title",         FDF_STRING|FDF_RW, 0, (APTR)GET_Title,        (APTR)SET_Title },
   { "X",             FDF_LONG|FDF_RW,   0, (APTR)GET_X,            (APTR)SET_X },
   { "Y",             FDF_LONG|FDF_RW,   0, (APTR)GET_Y,            (APTR)SET_Y },
   { "Width",         FDF_LONG|FDF_RW,   0, (APTR)GET_Width,        (APTR)SET_Width },
   { "Height",        FDF_LONG|FDF_RW,   0, (APTR)GET_Height,       (APTR)SET_Height },
   END_FIELD
};

static ERROR add_window_class(void)
{
   LONG class_flags;

   if (GetResource(RES_GLOBAL_INSTANCE)) class_flags = CLF_SHARED_ONLY|CLF_PUBLIC_OBJECTS;
   else class_flags = 0; // When operating stand-alone, do not share surfaces by default.

   return(CreateObject(ID_METACLASS, NULL, &clWindow,
      FID_ClassVersion|TFLOAT, VER_WINDOW,
      FID_Name|TSTRING,   "Window",
      FID_Category|TLONG, CCF_GUI,
      FID_Flags|TLONG,    CLF_PROMOTE_INTEGRAL|class_flags,
      FID_Actions|TPTR,   clWindowActions,
      FID_Methods|TARRAY, clWindowMethods,
      FID_Fields|TARRAY,  clWindowFields,
      FID_Size|TLONG,     sizeof(objWindow),
      FID_Path|TSTR,      MOD_PATH,
      TAGEND));
}

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, NULL, CMDExpunge, VER_WINDOW)
