/*****************************************************************************

-CLASS-
Display: Manages the video display and graphics hardware.

A Display object represents an area of displayable video memory.  Although it is a very complex structure, it is fairly
simple to initialise.  In fact it is possible to initialise a display using an empty structure and accept all the user
defaults (which we recommended if possible).  For more demanding applications however you may often need to specify a
few fields.  Before doing so, make sure that you understand how each field operates and what implications setting them
may bring.  Where possible try to avoid setting field values, as the user default should always be considered as
acceptable.

It is perfectly acceptable to initialise multiple display objects and add them to the viewport, but due to memory
restrictions, chances of failure could be high when doing this in certain environments.  When programming for Parasol,
it is recommended that the utilisation of display objects is avoided in favour of using the @Surface class.
which is much lighter on memory usage.

Displays that are created as shared objects are available for any application or object to draw graphics to, so bear in
mind the implications of creating a shared display.

-END-

*****************************************************************************/

#include "defs.h"

// Class definition at end of this source file.

static ERROR DISPLAY_Resize(objDisplay *, struct acResize *);
static CSTRING dpms_name(LONG Index);

static void alloc_display_buffer(objDisplay *Self);

#ifdef _GLES_
static const int attributes[] = {
   EGL_BUFFER_SIZE,
   EGL_ALPHA_SIZE,
   EGL_BLUE_SIZE,
   EGL_GREEN_SIZE,
   EGL_RED_SIZE,
   EGL_DEPTH_SIZE,
   EGL_STENCIL_SIZE,
   EGL_CONFIG_CAVEAT,
   EGL_CONFIG_ID,
   EGL_LEVEL,
   EGL_MAX_PBUFFER_HEIGHT,
   EGL_MAX_PBUFFER_PIXELS,
   EGL_MAX_PBUFFER_WIDTH,
   EGL_NATIVE_RENDERABLE,
   EGL_NATIVE_VISUAL_ID,
   EGL_NATIVE_VISUAL_TYPE,
   0x3030, // EGL10.EGL_PRESERVED_RESOURCES,
   EGL_SAMPLES,
   EGL_SAMPLE_BUFFERS,
   EGL_SURFACE_TYPE,
   EGL_TRANSPARENT_TYPE,
   EGL_TRANSPARENT_RED_VALUE,
   EGL_TRANSPARENT_GREEN_VALUE,
   EGL_TRANSPARENT_BLUE_VALUE,
   0x3039, // EGL10.EGL_BIND_TO_TEXTURE_RGB,
   0x303A, // EGL10.EGL_BIND_TO_TEXTURE_RGBA,
   0x303B, // EGL10.EGL_MIN_SWAP_INTERVAL,
   0x303C, // EGL10.EGL_MAX_SWAP_INTERVAL,
   EGL_LUMINANCE_SIZE,
   EGL_ALPHA_MASK_SIZE,
   EGL_COLOR_BUFFER_TYPE,
   EGL_RENDERABLE_TYPE,
   0x3042 // EGL10.EGL_CONFORMANT
};

static const CSTRING names[] = {
  "EGL_BUFFER_SIZE",         "EGL_ALPHA_SIZE",            "EGL_BLUE_SIZE",               "EGL_GREEN_SIZE",
  "EGL_RED_SIZE",            "EGL_DEPTH_SIZE",            "EGL_STENCIL_SIZE",            "EGL_CONFIG_CAVEAT",
  "EGL_CONFIG_ID",           "EGL_LEVEL",                 "EGL_MAX_PBUFFER_HEIGHT",      "EGL_MAX_PBUFFER_PIXELS",
  "EGL_MAX_PBUFFER_WIDTH",   "EGL_NATIVE_RENDERABLE",     "EGL_NATIVE_VISUAL_ID",        "EGL_NATIVE_VISUAL_TYPE",
  "EGL_PRESERVED_RESOURCES", "EGL_SAMPLES",               "EGL_SAMPLE_BUFFERS",          "EGL_SURFACE_TYPE",
  "EGL_TRANSPARENT_TYPE",    "EGL_TRANSPARENT_RED_VALUE", "EGL_TRANSPARENT_GREEN_VALUE", "EGL_TRANSPARENT_BLUE_VALUE",
  "EGL_BIND_TO_TEXTURE_RGB", "EGL_BIND_TO_TEXTURE_RGBA",  "EGL_MIN_SWAP_INTERVAL",       "EGL_MAX_SWAP_INTERVAL",
  "EGL_LUMINANCE_SIZE",      "EGL_ALPHA_MASK_SIZE",       "EGL_COLOR_BUFFER_TYPE",       "EGL_RENDERABLE_TYPE",
  "EGL_CONFORMANT"
};

static void printConfig(EGLDisplay display, EGLConfig config) __attribute__ ((unused));
static void printConfig(EGLDisplay display, EGLConfig config) {
   parasol::Log log(__FUNCTION__);
   LONG value[1];

   log.branch();

   for (LONG i=0; i < ARRAYSIZE(attributes); i++) {
      int attribute = attributes[i];
      CSTRING name = names[i];
      if (eglGetConfigAttrib(display, config, attribute, value)) {
         log.msg("%d: %s: %d", i, name, value[0]);
      }
      else {
         while (eglGetError() != EGL_SUCCESS);
      }
   }
}

#endif

//****************************************************************************
// Build a list of valid resolutions.

static resolution * get_resolutions(objDisplay *Self)
{
#ifdef __xwindows__

   parasol::Log log(__FUNCTION__);

   if (XRandRBase) {
      static resolution resolutions[30];
      struct xrMode *mode;

      if (Self->TotalResolutions) return resolutions;

      Self->TotalResolutions = xrGetDisplayTotal();

      if (Self->TotalResolutions > ARRAYSIZE(resolutions) - 1) {
         Self->TotalResolutions = ARRAYSIZE(resolutions) - 1;
      }

      LONG i;
      for (i=0; i < Self->TotalResolutions; i++) {
         if ((mode = (xrMode *)xrGetDisplayMode(i))) {
            resolutions[i].width  = mode->Width;
            resolutions[i].height = mode->Height;
            resolutions[i].bpp    = mode->Depth;
         }
      }
      resolutions[i].width  = 0;
      resolutions[i].height = 0;
      resolutions[i].bpp    = 0;

      return resolutions;
   }
   else {
      static resolution resolutions[2] = {
         { 1024, 768, 32 },
         { 0, 0, 0 }
      };

      log.msg("RandR extension not available.");

      resolutions[0].width  = glRootWindow.width;
      resolutions[0].height = glRootWindow.height;
      resolutions[0].bpp    = DefaultDepth(XDisplay, DefaultScreen(XDisplay));

      Self->TotalResolutions = 1;
      return resolutions;
   }

#else

   static resolution resolutions[] = {
      { 640, 480, 32 },
      { 800, 600, 32 },
      { 1024, 768, 32 },
      { 1152, 864, 32 },
      { 1280, 960, 32 },
      { 0, 0, 0 }
   };

   Self->TotalResolutions = ARRAYSIZE(resolutions);
   return resolutions;

#endif
}

//*****************************************************************************

static void update_displayinfo(objDisplay *Self)
{
   if (StrMatch("SystemDisplay", GetName(Self)) != ERR_Okay) return;

   glDisplayInfo->DisplayID = 0;
   get_display_info(Self->UID, glDisplayInfo, sizeof(DISPLAYINFO));
}

//****************************************************************************

void resize_feedback(FUNCTION *Feedback, OBJECTID DisplayID, LONG X, LONG Y, LONG Width, LONG Height)
{
   parasol::Log log(__FUNCTION__);

   log.traceBranch("%dx%d, %dx%d", X, Y, Width, Height);

   if (Feedback->Type IS CALL_STDC) {
      auto routine = (ERROR (*)(OBJECTID, LONG, LONG, LONG, LONG))Feedback->StdC.Routine;
      parasol::SwitchContext ctx(Feedback->StdC.Context);
      routine(DisplayID, X, Y, Width, Height);
   }
   else if (Feedback->Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = Feedback->Script.Script)) {
         const ScriptArg args[] = {
            { "Display", FD_OBJECTID, { .Long = DisplayID } },
            { "X",       FD_LONG, { .Long = X } },
            { "Y",       FD_LONG, { .Long = Y } },
            { "Width",   FD_LONG, { .Long = Width } },
            { "Height",  FD_LONG, { .Long = Height } }
         };
         scCallback(script, Feedback->Script.ProcedureID, args, ARRAYSIZE(args), NULL);
      }
   }
}

//****************************************************************************

static ERROR DISPLAY_AccessObject(objDisplay *Self, APTR Void)
{
   parasol::Log log;

   if (Self->BitmapID) {
      if (AccessObject(Self->BitmapID, 2000, &Self->Bitmap) != ERR_Okay) return log.warning(ERR_AccessObject);
   }
   else Self->Bitmap = NULL;

   if (Self->ResolutionsMID) {
      if (AccessMemory(Self->ResolutionsMID, MEM_READ, 2000, &Self->Resolutions) != ERR_Okay) return ERR_AccessMemory;
   }
   else Self->Resolutions = NULL;

   #ifdef _GLES_
      if (glEGLRefreshDisplay) {
         refresh_display_from_egl(Self);
      }
   #endif

   return ERR_Okay;
}

//****************************************************************************

static ERROR DISPLAY_ActionNotify(objDisplay *Self, struct acActionNotify *Args)
{
   if (!Args) return ERR_NullArgs;
   if (Args->Error != ERR_Okay) return ERR_Okay;

   if (Args->ActionID IS AC_Free) {
      if ((Self->ResizeFeedback.Type IS CALL_SCRIPT) and (Self->ResizeFeedback.Script.Script->UID IS Args->ObjectID)) {
         Self->ResizeFeedback.Type = CALL_NONE;
         return ERR_Okay;
      }
   }

   return ERR_NoSupport;
}

/*****************************************************************************
-ACTION-
Activate: Activating a display has the same effect as calling the Show action.
-END-
*****************************************************************************/

static ERROR DISPLAY_Activate(objDisplay *Self, APTR Void)
{
   return acShow(Self);
}

/*****************************************************************************
-METHOD-
CheckXWindow: Private. Checks that the Display dimensions match the X11 window dimensions.

Private

-END-
*****************************************************************************/

static ERROR DISPLAY_CheckXWindow(objDisplay *Self, APTR Void)
{
#ifdef __xwindows__

   Window childwin;
   LONG absx, absy;

   XTranslateCoordinates(XDisplay, Self->XWindowHandle, DefaultRootWindow(XDisplay), 0, 0, &absx, &absy, &childwin);

   if ((Self->X != absx) or (Self->Y != absy)) {
      parasol::Log log;
      log.msg("Repairing coordinates, pos is %dx%d, was %dx%d", absx, absy, Self->X, Self->Y);

      Self->X = absx;
      Self->Y = absy;

      resize_feedback(&Self->ResizeFeedback, Self->UID, absx, absy, Self->Width, Self->Height);
   }

#endif
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Clear: Clears a display's image data and hardware buffers (e.g. OpenGL)
-END-
*****************************************************************************/

static ERROR DISPLAY_Clear(objDisplay *Self, APTR Void)
{
#ifdef _GLES_
   if (!lock_graphics_active(__func__)) {
      glClearColorx(Self->Bitmap->BkgdRGB.Red, Self->Bitmap->BkgdRGB.Green, Self->Bitmap->BkgdRGB.Blue, 255);
      glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
      unlock_graphics();
      return ERR_Okay;
   }
   else return ERR_LockFailed;
#else
   return acClear(Self->Bitmap);
#endif
}

/*****************************************************************************
-ACTION-
DataFeed: Declared for internal purposes - do not call.
-END-
*****************************************************************************/

static ERROR DISPLAY_DataFeed(objDisplay *Self, struct acDataFeed *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

#ifdef _WIN32
   if (Args->DataType IS DATA_REQUEST) {
      // Supported for handling the windows clipboard

      auto request = (struct dcRequest *)Args->Buffer;

      log.traceBranch("Received data request from object %d, item %d", Args->ObjectID, request->Item);

      #ifdef WIN_DRAGDROP
      struct WinDT *data;
      LONG total_items;
      if (!winGetData(request->Preference, &data, &total_items)) {
         LONG xmlsize = 100; // Receipt header and tail

         for (LONG i=0; i < total_items; i++) {
            if (data[i].Datatype IS DATA_FILE) xmlsize += 30 + data[i].Length;
            else if (data[i].Datatype IS DATA_TEXT) xmlsize += 30 + data[i].Length;
         }

         STRING xml;
         if (!AllocMemory(xmlsize, MEM_STRING|MEM_NO_CLEAR, &xml, NULL)) {
            LONG pos = StrFormat(xml, xmlsize, "<receipt totalitems=\"%d\" id=\"%d\">", total_items, request->Item);

            for (LONG i=0; i < total_items; i++) {
               if (data[i].Datatype IS DATA_FILE) {
                  pos += StrFormat(xml+pos, xmlsize-pos, "<file path=\"%s\"/>", (STRING)data[i].Data);
               }
               else if (data[i].Datatype IS DATA_TEXT) {
                  pos += StrFormat(xml+pos, xmlsize-pos, "<text>%s</text>", (STRING)data[i].Data);
               }
               //else TODO: other types like images need their data saved to disk and referenced as a path, e.g. <image path="clipboard:abc.001"/>
            }
            pos += StrCopy("</receipt>", xml+pos, xmlsize-pos);

            struct acDataFeed dc;
            dc.ObjectID = Self->UID;
            dc.Datatype = DATA_RECEIPT;
            dc.Buffer   = xml;
            dc.Size     = pos+1;
            ActionMsg(AC_DataFeed, Args->ObjectID, &dc);

            FreeResource(xml);
         }
         else return log.warning(ERR_AllocMemory);
      }
      else return log.warning(ERR_NoSupport);
      #endif
   }
#endif

   return log.warning(ERR_NoSupport);
}

/*****************************************************************************

-ACTION-
Disable: Disables the display (goes into power saving mode).

Disabling a display will put the display into power saving mode.  The DPMS mode is determined by the user's system
settings and cannot be changed by the developer.  The display will remain off until the Enable action is called.

This action does nothing if the display is in hosted mode.

-ERRORS-
Okay: The display was disabled.
NoSupport: The display or graphics card does not support DPMS.
-END-

*****************************************************************************/

static ERROR DISPLAY_Disable(objDisplay *Self, APTR Void)
{
#ifdef __snap__

   parasol::Log log;
   LONG cap = 0;
   if (glSNAP->gsDPMS.DPMSdetect(&cap)) {
      if ((Self->DPMS IS DPMS_SUSPEND) and (cap & DPMS_suspend)) cap = DPMS_suspend;
      else if ((Self->DPMS IS DPMS_STANDBY) and (cap & DPMS_standby)) cap = DPMS_standby;
      else if ((Self->DPMS IS DPMS_OFF) and (cap & DPMS_off)) cap = DPMS_off;

      error = ERR_Okay;
      if (cap & DPMS_off) { // Turn the display off and stop sending output to the display
         log.msg("Initiating DPMS Off state.");
         glSNAP->gsDPMS.DPMSsetState(DPMS_off);
      }
      else if (cap & DPMS_suspend) { // Put the display into power-saving mode
         log.msg("Initiating DPMS Suspend state.");
         glSNAP->gsDPMS.DPMSsetState(DPMS_suspend);
      }
      else if (cap & DPMS_standby) { // Standby usually just blanks the display and leaves power on
         log.msg("Initiating DPMS Standby state.");
         glSNAP->gsDPMS.DPMSsetState(DPMS_standby);
      }
      else error = ERR_NoSupport;

      return error;
   }
   else return ERR_NoSupport;

#else

   return ERR_NoSupport;

#endif
}

/*****************************************************************************
-ACTION-
Enable: Restores the screen display from power saving mode.
-END-
*****************************************************************************/

static ERROR DISPLAY_Enable(objDisplay *Self, APTR Void)
{
#ifdef __snap__

   parasol::Log log;
   log.branch();

   if (!glSNAP->gsDPMS.DPMSsetState) {
      log.msg("DPMS not available.");
      return ERR_NoSupport;
   }

   glSNAP->gsDPMS.DPMSsetState(DPMS_on);
   return ERR_Okay;

#else

   return ERR_NoSupport;

#endif
}

//****************************************************************************
// On hosted systems like Android, the system may call Draw() on a display as a means of informing a program that a
// redraw is required.  It is the responsibility of the program that created the Display object to subscribe to the
// Draw action and act on it.

static ERROR DISPLAY_Draw(objDisplay *Self, APTR Void)
{
   return ERR_Okay;
}

//****************************************************************************

static void user_login(objDisplay *Self, APTR Info, LONG Data)
{
   // Regenerate the screen.xml file for the user:config/ directory.

   #ifdef __snap__
      GenerateDisplayXML();
   #endif
}

/*****************************************************************************
-ACTION-
Flush: Flush pending graphics operations to the display.
-END-
*****************************************************************************/

static ERROR DISPLAY_Flush(objDisplay *Self, APTR Void)
{
#ifdef __xwindows__
   XSync(XDisplay, False);
#elif _GLES_
   if (!lock_graphics_active(__func__)) {
      glFlush();
      unlock_graphics();
   }
#endif
   return ERR_Okay;
}

//****************************************************************************

static ERROR DISPLAY_Focus(objDisplay *Self, APTR Void)
{
   parasol::Log log;

   log.traceBranch("");
#ifdef _WIN32
   winFocus(Self->WindowHandle);
#elif __xwindows__
   if (Self->Flags & SCR_BORDERLESS) XSetInputFocus(XDisplay, Self->XWindowHandle, RevertToNone, CurrentTime);
#endif
   return ERR_Okay;
}

//****************************************************************************

static ERROR DISPLAY_Free(objDisplay *Self, APTR Void)
{
   parasol::Log log;

   if (Self->Flags & SCR_AUTO_SAVE) {
      log.trace("Autosave enabled.");
      acSaveSettings(Self);
   }
   else log.trace("Autosave disabled.");

#ifdef __xwindows__
   XEvent xevent;

   if (Self->WindowHandle IS (APTR)glDisplayWindow) glDisplayWindow = NULL;

   // Kill all expose events associated with the X Window owned by the display

   if (XDisplay) {
      while (XCheckWindowEvent(XDisplay, Self->XWindowHandle, ExposureMask, &xevent) IS True);

      if (!(Self->Flags & SCR_CUSTOM_WINDOW)) {
         if (Self->WindowHandle) {
            XDestroyWindow(XDisplay, Self->XWindowHandle);
            Self->WindowHandle = NULL;
         }
      }
   }
#endif

#ifdef _WIN32
   if (!(Self->Flags & SCR_CUSTOM_WINDOW)) {
      if (Self->WindowHandle) {
         winDestroyWindow(Self->WindowHandle);
         Self->WindowHandle = NULL;
      }
   }
#endif

#ifdef _GLES_
   glActiveDisplayID = 0;
#endif

   UnsubscribeEvent(Self->UserLoginHandle);

   acHide(Self);  // Hide the display.  In OpenGL this will remove the display resources.

   // Free the display's bitmap buffer

   if (Self->BufferID) { acFreeID(Self->BufferID); Self->BufferID = NULL; }

   // Free the display's video bitmap

   if (Self->BitmapID) {
      if (Self->Bitmap) {
         acFree(Self->Bitmap);
         if (Self->BitmapID < 0) ReleaseObject(Self->Bitmap);
         Self->Bitmap = NULL;
      }
      else acFreeID(Self->BitmapID);
      Self->BitmapID = NULL;
   }

   if (Self->ResolutionsMID) {
      if (Self->Resolutions) { ReleaseMemory(Self->Resolutions); Self->Resolutions = NULL; }
      FreeResourceID(Self->ResolutionsMID);
      Self->ResolutionsMID = 0;
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
GetVar: Retrieve formatted information from the display.
-END-
*****************************************************************************/

static ERROR DISPLAY_GetVar(objDisplay *Self, struct acGetVar *Args)
{
   parasol::Log log;
   ULONG colours;

   if (!Args) return log.warning(ERR_NullArgs);
   if ((!Args->Field) or (!Args->Buffer) or (Args->Size < 1)) return log.warning(ERR_Args);

   STRING buffer = Args->Buffer;
   buffer[0] = 0;

   if (!StrCompare("resolution(", Args->Field, 11, 0)) {
      resolution *list;

      // Field is in the format:  Resolution(Index, Format) Where 'Format' contains % symbols to indicate variable references.

      CSTRING str = Args->Field + 11;
      LONG index = StrToInt(str);
      while ((*str) and (*str != ')') and (*str != ',')) str++;
      if (*str IS ',') str++;
      while ((*str) and (*str <= 0x20)) str++;

      if ((list = get_resolutions(Self))) {
         if (index >= Self->TotalResolutions) return ERR_OutOfRange;

         LONG i = 0;
         while ((*str) and (*str != ')') and (i < Args->Size-1)) {
            if (*str != '%') {
               buffer[i++] = *str++;
               continue;
            }

            if (str[1] IS '%') {
               buffer[i++] = '%';
               continue;
            }

            str++;

            switch (*str) {
               case 'w': i += IntToStr(list[index].width, buffer+i, Args->Size-i); break;
               case 'h': i += IntToStr(list[index].height, buffer+i, Args->Size-i); break;
               case 'd': i += IntToStr(list[index].bpp, buffer+i, Args->Size-i); break;
               case 'c': if (list[index].bpp <= 24) colours = 1<<list[index].bpp;
                         else colours = 1<<24;
                         i += IntToStr(colours, buffer+i, Args->Size-i);
                         break;
            }
            str++;
         }
         buffer[i] = 0;

         return ERR_Okay;
      }
      else return ERR_NoData;
   }
   else return ERR_NoSupport;
}

/*****************************************************************************
-ACTION-
Hide: Hides a display from the user's view.

Calling this action will hide a display from the user's view.  If the hidden display was at the front of the display
and there is a display object behind it, then the next underlying display will be displayed.  If there are no other
displays available then the user's viewport will be blank after calling this action.
-END-
*****************************************************************************/

static ERROR DISPLAY_Hide(objDisplay *Self, APTR Void)
{
   parasol::Log log;

   log.branch();

#ifdef _WIN32
   winHideWindow(Self->WindowHandle);
#elif __xwindows__
   if (XDisplay) XUnmapWindow(XDisplay, Self->XWindowHandle);
#elif __snap__
   // If the system is shutting down, don't touch the display.  This makes things look tidier when the system shuts down.

   LONG state = GetResource(RES_SYSTEM_STATE);
   if ((state IS STATE_SHUTDOWN) or (state IS STATE_RESTART)) {
      log.msg("Not doing anything because system is shutting down.");
   }
   else sciCloseVideoMode(Self->VideoHandle);

#elif _GLES_
   if (Self->Flags & SCR_VISIBLE) {
      adHideDisplay(Self->UID);
   }
#endif

   Self->Flags &= ~SCR_VISIBLE;
   return ERR_Okay;
}

//****************************************************************************

static ERROR DISPLAY_Init(objDisplay *Self, APTR Void)
{
   parasol::Log log;
   struct gfxUpdatePalette pal;
   objBitmap *bmp;
   #ifdef __xwindows__
      XSetWindowAttributes swa;
      XWindowAttributes winattrib;
      XPixmapFormatValues *list;
      LONG xbpp, xbytes;
      LONG count;
   #endif

   #ifdef __xwindows__
      // Figure out how many bits and bytes are used per pixel on this XDisplay

      xbpp = DefaultDepth(XDisplay, DefaultScreen(XDisplay));

      if (xbpp <= 8) {
         log.msg(VLF_CRITICAL, "Please change your X11 setup so that it runs in 15 bit mode or better.");
         log.msg(VLF_CRITICAL, "Currently X11 is configured to use %d bit graphics.", xbpp);
         return ERR_Failed;
      }

      if (xbpp <= 8) xbytes = 1;
      else if (xbpp <= 16) xbytes = 2;
      else if (xbpp <= 24) xbytes = 3;
      else xbytes = 4;

      if ((list = XListPixmapFormats(XDisplay, &count))) {
         for (LONG i=0; i < count; i++) {
            if (list[i].depth IS xbpp) {
               xbytes = list[i].bits_per_pixel;
               if (list[i].bits_per_pixel <= 8) xbytes = 1;
               else if (list[i].bits_per_pixel <= 16) xbytes = 2;
               else if (list[i].bits_per_pixel <= 24) xbytes = 3;
               else xbytes = 4;
            }
         }
         XFree(list);
      }

      if (XRandRBase) {
         // Set the refresh rate to zero to indicate that we have some control of the display (the default is -1 if there is no control).
         Self->RefreshRate = 0;
      }
   #endif

   // Set defaults

   bmp = Self->Bitmap;

   DISPLAYINFO info;
   if (get_display_info(0, &info, sizeof(info))) return log.warning(ERR_Failed);

   if (!Self->Width) {
      Self->Width = info.Width;
      #ifdef _WIN32
         Self->Width -= 60;
      #endif
   }

   if (!Self->Height) {
      Self->Height = info.Height;
      #ifdef _WIN32
         Self->Height -= 80;
      #endif
   }

   if (Self->Width  < 4)  Self->Width  = 4;
   if (Self->Height < 4)  Self->Height = 4;

   if (info.Flags & SCR_MAXSIZE) {
      if (Self->Width > info.Width) {
         log.msg("Limiting requested width of %d to %d", Self->Width, info.Width);
         Self->Width = info.Width;
      }
      if (Self->Height > info.Height) {
         log.msg("Limiting requested height of %d to %d", Self->Height, info.Height);
         Self->Height = info.Height;
      }
   }
   else {
      if (Self->Width  > 4096) Self->Width  = 4096;
      if (Self->Height > 4096) Self->Height = 4096;
   }

   #ifdef __xwindows__
      // If the display object will act as window manager, the dimensions must match that of the root window.

      if ((x11WindowManager() IS TRUE) or (Self->Flags & SCR_MAXIMISE)) {
         Self->Width  = glRootWindow.width;
         Self->Height = glRootWindow.height;
      }

      if (Self->Width > glRootWindow.width) Self->Width = glRootWindow.width;
      if (Self->Height > glRootWindow.height) Self->Height = glRootWindow.height;
   #endif

   if (bmp->Width  < Self->Width)  bmp->Width = Self->Width;
   if (bmp->Height < Self->Height) bmp->Height = Self->Height;

   // Fix up the bitmap dimensions

   if (!bmp->Width) bmp->Width  = Self->Width;
   else if (Self->Width > bmp->Width) bmp->Width  = Self->Width;

   if (!bmp->Height) bmp->Height = Self->Height;
   else if (Self->Height > bmp->Height) bmp->Height = Self->Height;

   bmp->Type = BMP_CHUNKY;

   #ifdef __xwindows__
      if (xbytes IS 4) bmp->BitsPerPixel = 32;
      else bmp->BitsPerPixel = xbpp;
      bmp->BytesPerPixel = xbytes;
   #elif _WIN32
      if (Self->Flags & SCR_COMPOSITE) {
         log.msg("Composite mode will force a 32-bit window area.");
         bmp->BitsPerPixel = 32;
         bmp->BytesPerPixel = 4;
      }
   #endif

   if (!bmp->BitsPerPixel) {
      bmp->BitsPerPixel = info.BitsPerPixel;
      bmp->BytesPerPixel = info.BytesPerPixel;
   }

   #ifdef __xwindows__

      bmp->Flags |= BMF_NO_DATA;
      bmp->DataFlags = MEM_VIDEO;

      // Set the Window Attributes structure

      swa.bit_gravity = CenterGravity;
      swa.win_gravity = CenterGravity;
      swa.cursor      = C_Default;
      swa.override_redirect = (Self->Flags & SCR_BORDERLESS) ? 1 : 0;
      swa.event_mask  = ExposureMask|EnterWindowMask|LeaveWindowMask|PointerMotionMask|StructureNotifyMask
                        |KeyPressMask|KeyReleaseMask|ButtonPressMask|ButtonReleaseMask|FocusChangeMask;

      if (x11WindowManager() IS FALSE) {
         Atom protocols[2];
         LONG cwflags;

         // If we are running inside a foreign window manager, use the following routine to create a new X11 window for us to run in.

         log.msg("Creating X11 window %dx%d,%dx%d, Override: %d, XDisplay: %p, Parent: " PF64(), Self->X, Self->Y, Self->Width, Self->Height, swa.override_redirect, XDisplay, (LARGE)Self->XWindowHandle);

         cwflags = CWEventMask|CWOverrideRedirect;

         if (!Self->XWindowHandle) {
            if (!(Self->XWindowHandle = XCreateWindow(XDisplay, DefaultRootWindow(XDisplay),
               Self->X, Self->Y,
               Self->Width, Self->Height, 0 /* Border */, CopyFromParent /* Depth */, InputOutput /* Class */,
               CopyFromParent /* Visual */, cwflags, &swa))) {
               log.warning("Failed in call to XCreateWindow().");
               return ERR_Failed;
            }
         }
         else {
            // If the WindowHandle field is already set, use it as the parent for the new window.

            if (!(Self->XWindowHandle = XCreateWindow(XDisplay, Self->XWindowHandle,
               0, 0, Self->Width, Self->Height, 0, CopyFromParent, InputOutput,
               CopyFromParent, cwflags, &swa))) {
               log.warning("Failed in call to XCreateWindow().");
               return ERR_Failed;
            }
         }

         log.trace("X-Window created successfully: " PF64(), (LARGE)Self->XWindowHandle);

         SetPointer(bmp, FID_Handle, (APTR)Self->XWindowHandle);

         CSTRING name;
         if ((GetPointer(CurrentTask(), FID_Name, &name) IS ERR_Okay) and (name)) {
            XStoreName(XDisplay, Self->XWindowHandle, name);
         }
         else XStoreName(XDisplay, Self->XWindowHandle, "Parasol");

         protocols[0] = XWADeleteWindow;
         XSetWMProtocols(XDisplay, Self->XWindowHandle, protocols, 1);

         Self->Flags |= SCR_HOSTED;

         bmp->Width  = Self->Width;
         bmp->Height = Self->Height;

         if (glStickToFront) {
            // KDE doesn't honour this request, not sure how many window managers would but it's worth a go.

            XSetTransientForHint(XDisplay, Self->XWindowHandle, DefaultRootWindow(XDisplay));
         }

         // Indicate that the window position is not to be meddled with by the window manager.

         XSizeHints hints;
         hints.flags = USPosition|USSize;
         XSetWMNormalHints(XDisplay, Self->XWindowHandle, &hints);

         if (acInit(bmp) != ERR_Okay) {
            return log.warning(ERR_Init);
         }
      }
      else {
         // If we are the window manager, set up the root window as our display.

         if (!Self->WindowHandle) Self->XWindowHandle = DefaultRootWindow(XDisplay);
         SetPointer(bmp, FID_Handle, (APTR)Self->XWindowHandle);
         XChangeWindowAttributes(XDisplay, Self->XWindowHandle, CWEventMask|CWCursor, &swa);

         if (XRandRBase) xrSelectInput(Self->XWindowHandle);

         XGetWindowAttributes(XDisplay, Self->XWindowHandle, &winattrib);
         Self->Width    = winattrib.width;
         Self->Height   = winattrib.height;
         bmp->Width  = Self->Width;
         bmp->Height = Self->Height;

         if (acInit(bmp) != ERR_Okay) {
            return log.warning(ERR_Init);
         }

         if (glDGAAvailable IS TRUE) {
            bmp->Flags |= BMF_X11_DGA;
            bmp->Data = (UBYTE *)glDGAVideo;
         }
      }

      glDisplayWindow = Self->XWindowHandle;

      XChangeProperty(XDisplay, Self->XWindowHandle, atomSurfaceID, atomSurfaceID, 32, PropModeReplace, (UBYTE *)&Self->UID, 1);

   #elif _WIN32

      // Initialise the Bitmap.  We will set the Bitmap->Data field later on.  The Drawable field
      // in the Bitmap object will also be pointed to the window that we have created, but this
      // will be managed by the Surface class.

      bmp->Flags |= BMF_NO_DATA;
      bmp->DataFlags = MEM_VIDEO;

      if (acInit(bmp) != ERR_Okay) {
         return log.warning(ERR_Init);
      }

      if (!Self->WindowHandle) {
         bool desktop = FALSE;
         if (Self->Flags & SCR_COMPOSITE) {
            // Not a desktop
         }
         else {
            OBJECTID surface_id;
            LONG count = 1;
            if (!FindObject("SystemSurface", ID_SURFACE, FOF_INCLUDE_SHARED, &surface_id, &count)) {
               if (surface_id IS GetOwner(Self)) desktop = TRUE;
            }
         }

         STRING name = NULL;
         GetString(CurrentTask(), FID_Name, &name);
         HWND popover = 0;
         if (Self->PopOverID) {
            objDisplay *other_display;
            if (!AccessObject(Self->PopOverID, 3000, &other_display)) {
               popover = other_display->WindowHandle;
               ReleaseObject(other_display);
            }
            else log.warning(ERR_AccessObject);
         }

         if (!(Self->WindowHandle = (APTR)winCreateScreen(popover, &Self->X, &Self->Y, &Self->Width, &Self->Height,
               (Self->Flags & SCR_MAXIMISE) ? 1 : 0, (Self->Flags & SCR_BORDERLESS) ? 1 : 0, name,
               (Self->Flags & SCR_COMPOSITE) ? 1 : 0, Self->Opacity, desktop))) {
            return log.warning(ERR_SystemCall);
         }
      }
      else {
         // If we have been passed a foreign window handle, we need to set the procedure for it so that we can process
         // window related messages.

         if (!(Self->WindowHandle = (APTR)winCreateChild(Self->WindowHandle, Self->X, Self->Y, Self->Width, Self->Height))) {
            return log.warning(ERR_SystemCall);
         }
      }

      Self->Flags |= SCR_HOSTED;

      // Get the size of the host window frame.  Note that the winCreateScreen() function we called earlier
      // would have already reset the X/Y fields so that they reflect the absolute client position of the window.

      winGetMargins(Self->WindowHandle, &Self->LeftMargin, &Self->TopMargin, &Self->RightMargin, &Self->BottomMargin);

   #elif _GLES_
      ERROR error;

      if (Self->Bitmap->BitsPerPixel) glEGLPreferredDepth = Self->Bitmap->BitsPerPixel;
      else glEGLPreferredDepth = 0;

      if (!pthread_mutex_lock(&glGraphicsMutex)) {
         error = init_egl();
         eglMakeCurrent(glEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT); // Give up our access to EGL because we're releasing the graphics mutex.
         pthread_mutex_unlock(&glGraphicsMutex);
      }
      if (error) return error;

      refresh_display_from_egl(Self);

      // Initialise the video bitmap that will represent the OpenGL surface

      bmp->Flags |= BMF_NO_DATA;
      bmp->DataFlags = MEM_VIDEO;
      if (acInit(bmp) != ERR_Okay) {
         return log.warning(ERR_Init);
      }

   #else
      #error This platform requires display initialisation code.
   #endif

   if (Self->Flags & SCR_BUFFER) alloc_display_buffer(Self);

   pal.NewPalette = bmp->Palette;
   Action(MT_GfxUpdatePalette, Self, &pal);

   // Take a record of the pixel format for GetDisplayInfo()

   CopyMemory(bmp->ColourFormat, &glColourFormat, sizeof(glColourFormat));

   FUNCTION call;
   SET_FUNCTION_STDC(call, (APTR)user_login);
   SubscribeEvent(EVID_USER_STATUS_LOGIN, &call, Self, &Self->UserLoginHandle);  // Get notifications of user login because changes may be required to the display.

   if (glSixBitDisplay) Self->Flags |= SCR_BIT_6;

   update_displayinfo(Self); // Update the glDisplayInfo cache.

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
Minimise: Minimise the desktop window hosting the display.

If a display is hosted in a desktop window, calling the Minimise method will perform the default minimise action
on that window.  On a platform such as Microsoft Windows, this would normally result in the window being
minimised to the task bar.

Calling Minimise on a display that is already in the minimised state may result in the host window being restored to
the desktop.  This behaviour is platform dependent and should be manually tested to confirm its reliability on the
host platform.

-ERRORS-
Okay
-END-

*****************************************************************************/

static ERROR DISPLAY_Minimise(objDisplay *Self, APTR Void)
{
   parasol::Log log;
   log.branch();
#ifdef _WIN32
   winMinimiseWindow(Self->WindowHandle);
#elif __xwindows__
   if (XDisplay) XUnmapWindow(XDisplay, Self->XWindowHandle);
#endif
   return ERR_Okay;
}

/*****************************************************************************

Bitmap moving should be supported by listening to the Bitmap's Move() action
and responding to it.

MoveBitmap(): Moves a display's bitmap to specified X/Y values.

This routine has two uses: Moving the Bitmap to any position on the display, and for Hardware Scrolling.  It takes the
BmpX and BmpY arguments and uses them to set the new Bitmap position. This method will execute at the same speed for
all offset values.

You must have set the HSCROLL flag for horizontal scrolling and the VSCROLL flag for vertical scrolling if you wish to
use this method.  If you try and move the Bitmap without setting at least one of these flags, the method will fail
immediately.

If you want to perform hardware scrolling suitable for games that need to scroll in any direction, initialise a display
that has a bitmap of twice the size of the display. You can then scroll around in this area and create an infinite
scrolling map.  Because today's game programs typically run in high resolution true colour displays, be aware that the
host graphics card may need a large amount of memory to support this method of scrolling.

*****************************************************************************/

/*****************************************************************************
-ACTION-
Move: Move the display to a new display position (relative coordinates).
-END-
*****************************************************************************/

static ERROR DISPLAY_Move(objDisplay *Self, struct acMove *Args)
{
   parasol::Log log;

   if (!Args) return ERR_NullArgs;

   //log.branch("Moving display by %dx%d", (LONG)Args->DeltaX, (LONG)Args->DeltaY);

#ifdef _WIN32

   if (!winMoveWindow(Self->WindowHandle,
      Self->X + Self->LeftMargin + Args->DeltaX,
      Self->Y + Self->TopMargin + Args->DeltaY)) return ERR_Failed;

   return ERR_Okay;

#elif __xwindows__

   // Handling margins isn't necessary as the window manager will take that into account when it receives the move request.

   if (!XDisplay) return ERR_Failed;

   XMoveWindow(XDisplay, Self->XWindowHandle, Self->X + Args->DeltaX, Self->Y + Args->DeltaY);
   return ERR_Okay;

#elif __snap__

   Self->X += Args->DeltaX;
   Self->Y += Args->DeltaY;
   return ERR_Okay;

#else

   return ERR_NoSupport;

#endif

}

/*****************************************************************************
-ACTION-
MoveToBack: Move the display to the back of the display list.
-END-
*****************************************************************************/

static ERROR DISPLAY_MoveToBack(objDisplay *Self, APTR Void)
{
   parasol::Log log;
   log.branch("%s", GetName(Self));

#ifdef _WIN32
   winMoveToBack(Self->WindowHandle);
#elif __xwindows__
   if (XDisplay) XLowerWindow(XDisplay, Self->XWindowHandle);
#endif

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
MoveToFront: Move the display to the front of the display list.
-END-
*****************************************************************************/

static ERROR DISPLAY_MoveToFront(objDisplay *Self, APTR Void)
{
   parasol::Log log;
   log.branch("%s", GetName(Self));
#ifdef _WIN32
   winMoveToFront(Self->WindowHandle);
#elif __xwindows__
   if (XDisplay) XRaiseWindow(XDisplay, Self->XWindowHandle);
#endif
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
MoveToPoint: Move the display to a new position.

The MoveToPoint action moves the display to a new position.

In a hosted environment, the supplied coordinates are treated as being indicative of the absolute position of the host
window (not the client area).

For full-screen displays, MoveToPoint can alter the screen position for the hardware device managing the display
output.  This is a rare feature that requires hardware support.  ERR_NoSupport is returned if this feature is
unavailable.
-END-
*****************************************************************************/

static ERROR DISPLAY_MoveToPoint(objDisplay *Self, struct acMoveToPoint *Args)
{
   parasol::Log log;

   if (!Args) return ERR_NullArgs;

   log.traceBranch("Moving display to %dx%d", F2T(Args->X), F2T(Args->Y));

#ifdef _WIN32

   // winMoveWindow() treats the coordinates as being indicative of the client area.

   if (!winMoveWindow(Self->WindowHandle,
         (Args->Flags & MTF_X) ? Args->X : F2T(Self->X) + Self->LeftMargin,
         (Args->Flags & MTF_Y) ? Args->Y : F2T(Self->Y) + Self->TopMargin)) return ERR_Failed;

   if (Args->Flags & MTF_X) Self->X = F2T(Args->X) + Self->LeftMargin;
   if (Args->Flags & MTF_Y) Self->Y = F2T(Args->Y) + Self->TopMargin;
   return ERR_Okay;

#elif __xwindows__

   // Handling margins isn't necessary as the window manager will take that into account when it receives the move request.

   XMoveWindow(XDisplay, Self->XWindowHandle,
      (Args->Flags & MTF_X) ? F2T(Args->X) : Self->X,
      (Args->Flags & MTF_Y) ? F2T(Args->Y) : Self->Y);

   if (Args->Flags & MTF_X) Self->X = F2T(Args->X);
   if (Args->Flags & MTF_Y) Self->Y = F2T(Args->Y);
   return ERR_Okay;

#else

   return ERR_NoSupport;

#endif
}

//****************************************************************************

static ERROR DISPLAY_NewObject(objDisplay *Self, APTR Void)
{
   parasol::Log log;
   ERROR error;

   if (Self->Head::Flags & NF_PUBLIC) {
      error = NewLockedObject(ID_BITMAP, Self->Head::Flags|NF_INTEGRAL, &Self->Bitmap, &Self->BitmapID);
   }
   else {
      error = NewObject(ID_BITMAP, Self->Head::Flags|NF_INTEGRAL, &Self->Bitmap);
      Self->BitmapID = GetUID(Self->Bitmap);
   }

   if (!error) {
      if (CheckObjectNameExists("SystemVideo") != ERR_True) {
         SetName(Self->Bitmap, "SystemVideo");
      }

      if (!(GetName(Self)[0])) {
         if (CheckObjectNameExists("SystemDisplay") != ERR_True) {
            SetName(Self, "SystemDisplay");
         }
      }

      #ifdef __xwindows__

         StrCopy("X11", Self->Chipset, sizeof(Self->Chipset));
         StrCopy("X Windows", Self->Display, sizeof(Self->Display));
         StrCopy("N/A", Self->DisplayManufacturer, sizeof(Self->DisplayManufacturer));
         StrCopy("N/A", Self->Manufacturer, sizeof(Self->Manufacturer));

      #elif _WIN32

         StrCopy("Windows", Self->Chipset, sizeof(Self->Chipset));
         StrCopy("Windows", Self->Display, sizeof(Self->Display));
         StrCopy("N/A", Self->DisplayManufacturer, sizeof(Self->DisplayManufacturer));
         StrCopy("N/A", Self->Manufacturer, sizeof(Self->Manufacturer));

      #elif _GLES_

         StrCopy("OpenGLES", Self->Chipset, sizeof(Self->Chipset));
         StrCopy("OpenGL", Self->Display, sizeof(Self->Display));
         StrCopy("N/A", Self->DisplayManufacturer, sizeof(Self->DisplayManufacturer));
         StrCopy("N/A", Self->Manufacturer, sizeof(Self->Manufacturer));

      #else

         StrCopy("Unknown", Self->CertificationDate, sizeof(Self->CertificationDate));
         StrCopy("Unknown", Self->Chipset, sizeof(Self->Chipset));
         StrCopy("Unknown", Self->Display, sizeof(Self->Display));
         StrCopy("Unknown", Self->DisplayManufacturer, sizeof(Self->DisplayManufacturer));
         StrCopy("Unknown", Self->DriverCopyright, sizeof(Self->DriverCopyright));
         StrCopy("Unknown", Self->DriverVendor, sizeof(Self->DriverVendor));
         StrCopy("Unknown", Self->DriverVersion, sizeof(Self->DriverVersion));
         StrCopy("Unknown", Self->Manufacturer, sizeof(Self->Manufacturer));

      #endif

      Self->Width       = 800;
      Self->Height      = 600;
      Self->RefreshRate = -1;
      Self->Gamma[0]    = 1.0;
      Self->Gamma[1]    = 1.0;
      Self->Gamma[2]    = 1.0;
      Self->Opacity     = 255;

      #ifdef __xwindows__
         Self->DisplayType = DT_X11;
      #elif _WIN32
         Self->DisplayType = DT_WINDOWS;
      #elif _GLES_
         Self->DisplayType = DT_GLES;
      #else
         Self->DisplayType = DT_NATIVE;
      #endif

      return ERR_Okay;
   }
   else return log.warning(ERR_NewObject);
}

/*****************************************************************************
-ACTION-
Redimension: Moves and resizes a display object in a single action call.
-END-
*****************************************************************************/

static ERROR DISPLAY_Redimension(objDisplay *Self, struct acRedimension *Args)
{
   if (!Args) return ERR_NullArgs;

   struct acMoveToPoint moveto = { Args->X, Args->Y, 0, MTF_X|MTF_Y };
   DISPLAY_MoveToPoint(Self, &moveto);

   struct acResize resize = { Args->Width, Args->Height, Args->Depth };
   DISPLAY_Resize(Self, &resize);
   return ERR_Okay;
}

//****************************************************************************

static ERROR DISPLAY_ReleaseObject(objDisplay *Self, APTR Void)
{
   if (Self->Bitmap) { ReleaseObject(Self->Bitmap); Self->Bitmap = NULL; }
   if (Self->Resolutions) { ReleaseMemory(Self->Resolutions); Self->Resolutions = NULL; }
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Resize: Resizes the dimensions of a display object.

If the display is hosted, the Width and Height values will determine the size of the inside area of the window.
-END-
*****************************************************************************/

static ERROR DISPLAY_Resize(objDisplay *Self, struct acResize *Args)
{
   parasol::Log log;

   log.branch();

#ifdef _WIN32

   if (!Args) return log.warning(ERR_NullArgs);

   if (!winResizeWindow(Self->WindowHandle, 0x7fffffff, 0x7fffffff, Args->Width, Args->Height)) {
      return ERR_Failed;
   }

   Action(AC_Resize, Self->Bitmap, Args);
   Self->Width = Self->Bitmap->Width;
   Self->Height = Self->Bitmap->Height;

#elif __xwindows__

   if (!Args) return log.warning(ERR_NullArgs);

   if (XDisplay) XResizeWindow(XDisplay, Self->XWindowHandle, Args->Width, Args->Height);
   Action(AC_Resize, Self->Bitmap, Args);
   Self->Width = Self->Bitmap->Width;
   Self->Height = Self->Bitmap->Height;

#elif __snap__

   UWORD gfxmode;
   GA_modeInfo modeinfo;
   LONG i, vx, vy, bytesperline, width, height;
   LONG bestweight, weight, display;

   if (!Args) return log.warning(ERR_Args);

   // Scan the available display modes and choose the one that most closely matches the requested display dimensions.

   if (!(width = Args->Width)) width = Self->Width;
   if (!(height = Args->Height)) height = Self->Height;

   UWORD *modes = glSNAPDevice->AvailableModes;
   if (glSNAP->Init.GetDisplayOutput) display = glSNAP->Init.GetDisplayOutput() & gaOUTPUT_SELECTMASK;
   else display = gaOUTPUT_CRT;
   gfxmode = -1;
   bestweight = 0x7fffffff;
   for (i=0; modes[i] != 0xffff; i++) {
      modeinfo.dwSize = sizeof(modeinfo);
      if (!glSNAP->Init.GetVideoModeInfoExt(modes[i], &modeinfo, display, NULL)) {
         if (modeinfo.AttributesExt & gaIsPanningMode) continue;
         if (modeinfo.Attributes & gaIsTextMode) continue;

         if (modeinfo.BitsPerPixel IS glSNAP->VideoMode.BitsPerPixel) {
            weight = ABS(modeinfo.XResolution - width) + ABS(modeinfo.YResolution - height);

            if (weight < bestweight) {
               gfxmode = modes[i];
               bestweight = weight;
            }
         }
      }
   }

   // Broadcast the change in resolution so that all video buffered bitmaps can move their graphics out of video memory.

   evResolutionChange ev = { EVID_DISPLAY_RESOLUTION_CHANGE };
   BroadcastEvent(&ev, sizeof(ev));

   log.msg("Opening display mode: %dx%d", width, height);

   vx = -1;
   vy = -1;
   bytesperline = -1;
   if (sciOpenVideoMode(gfxmode, &modeinfo, &vx, &vy, &bytesperline, &Self->VideoHandle, 0) != ERR_Okay) {
      log.warning("Failed to set the requested video mode.");
      return ERR_NoSupport;
   }

   Self->GfxMode = gfxmode;
   Self->Width  = modeinfo.XResolution;
   Self->Height = modeinfo.YResolution;
   Self->RefreshRate = (glSNAP->Init.GetCurrentRefreshRate() + 50) / 100;

   acResize(Self->Bitmap, Self->Width, Self->Height, 0);

#endif

   // If a display buffer is in use, reallocate it from scratch.

   if (Self->Flags & SCR_BUFFER) alloc_display_buffer(Self);

   update_displayinfo(Self);

   Self->HDensity = 0; // DPI needs to be recalculated.
   Self->VDensity = 0;

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
SaveImage: Saves the image of a display to a data object.
-END-
*****************************************************************************/

static ERROR DISPLAY_SaveImage(objDisplay *Self, struct acSaveImage *Args)
{
   return Action(AC_SaveImage, Self->Bitmap, Args);
}

/*****************************************************************************
-ACTION-
SaveSettings: Saves the current display settings as the default.
-END-
*****************************************************************************/

static ERROR DISPLAY_SaveSettings(objDisplay *Self, APTR Void)
{
   parasol::Log log;

#ifdef __xwindows__

   log.branch();

   OBJECTPTR config;
   if (!CreateObject(ID_CONFIG, 0, &config, FID_Path|TSTR, "user:config/display.cfg", TAGEND)) {
      if (!(Self->Flags & SCR_BORDERLESS)) {
         cfgWriteInt(config, "DISPLAY", "WindowX", Self->X);
         cfgWriteInt(config, "DISPLAY", "WindowY", Self->Y);

         if (Self->Width >= 600) cfgWriteInt(config, "DISPLAY", "WindowWidth", Self->Width);
         else cfgWriteInt(config, "DISPLAY", "WindowWidth", 600);

         if (Self->Height >= 480) cfgWriteInt(config, "DISPLAY", "WindowHeight", Self->Height);
         else cfgWriteInt(config, "DISPLAY", "WindowHeight", 480);
      }

      cfgWriteValue(config, "DISPLAY", "DPMS", dpms_name(Self->DPMS));
      cfgWriteValue(config, "DISPLAY", "FullScreen", (Self->Flags & SCR_BORDERLESS) ? "1" : "0");

      acSaveSettings(config);
      acFree(config);
   }

#elif _WIN32

   if ((Self->WindowHandle) and (Self->Width >= 640) and (Self->Height > 480)) {
      // Save the current window status to file, but only if it is large enough to be considered 'screen sized'.

      OBJECTPTR config;
      if (!CreateObject(ID_CONFIG, 0, &config, FID_Path|TSTR, "user:config/display.cfg", TAGEND)) {
         LONG x, y, width, height, maximise;

         if (winGetWindowInfo(Self->WindowHandle, &x, &y, &width, &height, &maximise)) {
            cfgWriteInt(config, "DISPLAY", "WindowWidth", width);
            cfgWriteInt(config, "DISPLAY", "WindowHeight", height);
            cfgWriteInt(config, "DISPLAY", "WindowX", x);
            cfgWriteInt(config, "DISPLAY", "WindowY", y);
            cfgWriteInt(config, "DISPLAY", "Maximise", maximise);
            cfgWriteValue(config, "DISPLAY", "DPMS", dpms_name(Self->DPMS));
            cfgWriteValue(config, "DISPLAY", "FullScreen", (Self->Flags & SCR_BORDERLESS) ? "1" : "0");
            acSaveSettings(config);
         }
         acFree(config);
      }
      else return log.warning(ERR_CreateObject);
   }

#endif

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
SizeHints: Sets the width and height restrictions for the host window (hosted environments only).

If a display is hosted in a desktop window, it may be possible to enforce size restrictions that prevent the window
from being shrunk below, or expanded beyond a certain size.  This feature is platform dependent and ERR_NoSupport
will be returned if it is not implemented.

-INPUT-
int MinWidth: The minimum width of the window.
int MinHeight: The minimum height of the window.
int MaxWidth: The maximum width of the window.
int MaxHeight: The maximum width of the window.

-ERRORS-
Okay
NoSupport: The host platform does not support this feature.
-END-

*****************************************************************************/

static ERROR DISPLAY_SizeHints(objDisplay *Self, struct gfxSizeHints *Args)
{
#ifdef __xwindows__
   XSizeHints hints;

   hints.flags = 0;

   if ((Args->MaxWidth >= 0) or (Args->MaxHeight >= 0)) hints.flags |= PMaxSize;
   if ((Args->MinWidth >= 0) or (Args->MinHeight >= 0)) hints.flags |= PMinSize;

   if (Args->MaxWidth > 0)  hints.max_width  = Args->MaxWidth;  else hints.max_width  = 0;
   if (Args->MaxHeight > 0) hints.max_height = Args->MaxHeight; else hints.max_height = 0;
   if (Args->MinWidth > 0)  hints.min_width  = Args->MinWidth;  else hints.min_width  = 0;
   if (Args->MinHeight > 0) hints.min_height = Args->MinHeight; else hints.min_height = 0;

   XSetWMNormalHints(XDisplay, Self->XWindowHandle, &hints);
   return ERR_Okay;
#else
   return ERR_NoSupport;
#endif
}

/*****************************************************************************

-METHOD-
SetDisplay: Changes the current display mode.

The SetDisplay method changes the current display settings for the screen. It can alter the position and screen
dimensions and the display refresh rate. The new settings are applied immediately, although minor delays are possible
while the graphics card and monitor adjust to the changes.

To keep any of the display settings at their current value, set the appropriate parameters to zero to leave them
unchanged.  Only the parameters that you set will be used.

If the display parameters do not match with a valid display mode - for instance if you request a screen size of
1280x1024 and the nearest equivalent is 1024x768, the SetDisplay method will automatically adjust to match against the
nearest screen size.

Only the original owner of the display object is allowed to change the display settings.

-INPUT-
int X: Horizontal offset of the display, relative to its default position.
int Y: Vertical offset of the display, relative to its default position.
int Width: Width of the display.
int Height: Height of the display.
int InsideWidth: Internal display width (must be equal to or greater than the display width).
int InsideHeight: Internal display height (must be equal to or greater than the display height).
int BitsPerPixel: The desired display depth (15, 16, 24 or 32).
double RefreshRate: Refresh rate, measured in floating point format for precision.
int Flags: Optional flags.

-ERRORS-
Okay:
NullArgs:
Failed: Failed to switch to the requested display mode.
-END-

*****************************************************************************/

static ERROR DISPLAY_SetDisplay(objDisplay *Self, struct gfxSetDisplay *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

#ifdef _WIN32
   // NOTE: Dimensions are measured relative to the client area, not the window including its borders.

   log.msg(VLF_BRANCH|VLF_EXTAPI, "%dx%d, %dx%d", Args->X, Args->Y, Args->Width, Args->Height);

   if (!winResizeWindow(Self->WindowHandle, Args->X, Args->Y, Args->Width, Args->Height)) {
      return log.warning(ERR_Failed);
   }

   log.trace("Resizing the video bitmap.");

   acResize(Self->Bitmap, Args->Width, Args->Height, 0);
   Self->Width = Self->Bitmap->Width;
   Self->Height = Self->Bitmap->Height;

#elif __xwindows__
   // NOTE: Dimensions are measured relative to the client area, not the window.

   log.branch("%dx%d,%dx%d @ %.2fHz, %d bit", Args->X, Args->Y, Args->Width, Args->Height, Args->RefreshRate, Args->BitsPerPixel);

   if ((Args->Width IS Self->Width) and (Args->Height IS Self->Height)) return ERR_Okay;

   LONG width = Args->Width;
   LONG height = Args->Height;

   if (x11WindowManager() IS TRUE) { // The video mode can only be changed with the XRandR extension
      if ((XRandRBase) and (xrSetDisplayMode(&width, &height) IS ERR_Okay)) {
         Self->RefreshRate = 0;
         Self->Width  = width;
         Self->Height = height;

         // x11SetDisplayMode() posts a request to the X server.  Our response to display mode changes can be found in handler.c.

         // Note: The RandR extension changes the video mode without actually changing the size of the bitmap area, so we don't resize the bitmap.

         return ERR_Okay;
      }
      else return ERR_Failed;
   }
   else {
      XResizeWindow(XDisplay, Self->XWindowHandle, width, height);
      acResize(Self->Bitmap, width, height, 0.0);
      Self->Width  = width;
      Self->Height = height;
   }

#elif __snap__

   // Broadcast the change in resolution so that all video buffered bitmaps can move their graphics out of video memory.

   evResolutionChange ev = { EVID_DISPLAY_RESOLUTION_CHANGE };
   BroadcastEvent(&ev, sizeof(ev));

   if (sciOpenVideoMode(gfxmode, &modeinfo, &vx, &vy, &bytesperline, &Self->VideoHandle, Args->RefreshRate) != ERR_Okay) {
      log.warning("Failed to set the requested video mode.");
      return ERR_Failed;
   }

   Self->GfxMode = gfxmode;
   Self->Width   = modeinfo.XResolution;
   Self->Height  = modeinfo.YResolution;
   Self->RefreshRate = (glSNAP->Init.GetCurrentRefreshRate() + 50) / 100;

   if (bpp != Self->Bitmap->BitsPerPixel) {
      acFree(Self->Bitmap);
      ReleaseObject(Self->Bitmap);
      Self->Bitmap = NULL;

      if (!NewObject(ID_BITMAP, NF_INTEGRAL|Self->Head::Flags, &Self->Bitmap, (Self->Head::Flags & NF_PUBLIC) ? &Self->BitmapID : NULL)) {
         Self->BitmapID = Self->Bitmap->Head.UID;
         Self->Bitmap->BitsPerPixel = bpp;
         Self->Bitmap->Width        = Self->Width;
         Self->Bitmap->Height       = Self->Height;
         Self->Bitmap->Flags        = BMF_NO_DATA;
         Self->Bitmap->DataFlags    = MEM_VIDEO;
         if (!acInit(Self->Bitmap)) {
            Self->Bitmap->LineWidth = modeinfo.BytesPerScanLine;
         }
      }
   }
   else acResize(Self->Bitmap, Self->Width, Self->Height, 0);

#endif

   // If a display buffer is in use, reallocate it from scratch.  Note: A failure to allocate a display buffer is not
   // considered terminal.

   if (Self->Flags & SCR_BUFFER) alloc_display_buffer(Self);

   update_displayinfo(Self);
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
SetGamma: Sets the display gamma levels.

The SetGamma method controls the gamma correction levels for the display.  Gamma levels for the red, green and blue
colour components can be set at floating point precision.  The default gamma level for each component is 1.0; the
minimum value is 0.0 and the maximum value is 100.

Optional flags include `GMF_SAVE`.  This option will save the requested settings as the user default when future displays
are opened.

If you would like to know the default gamma correction settings for a display, please refer to the #Gamma
field.

-INPUT-
double Red:   Gamma correction for the red gun.
double Green: Gamma correction for the green gun.
double Blue:  Gamma correction for the blue gun.
int(GMF) Flags: Optional flags.

-ERRORS-
Okay
NullArgs
NoSupport: The graphics hardware does not support gamma correction.
-END-

*****************************************************************************/

static ERROR DISPLAY_SetGamma(objDisplay *Self, struct gfxSetGamma *Args)
{
#ifdef __snap__
   parasol::Log log;
   GA_palette palette[256];
   DOUBLE intensity, red, green, blue;

   if (!Args) return log.warning(ERR_NullArgs);

   red   = Args->Red;
   green = Args->Green;
   blue  = Args->Blue;

   if (red   < 0.00) red   = 0.00;
   if (green < 0.00) green = 0.00;
   if (blue  < 0.00) blue  = 0.00;

   if (red   > 100.0) red   = 100.0;
   if (green > 100.0) green = 100.0;
   if (blue  > 100.0) blue  = 100.0;

   if (Args->Flags & GMF_SAVE) {
      Self->Gamma[0]   = red;
      Self->Gamma[1] = green;
      Self->Gamma[2]  = blue;
   }

   for (LONG i=0; i < ARRAYSIZE(palette); i++) {
      intensity = (DOUBLE)i / 255.0;
      palette[i].Red   = F2T(pow(intensity, 1.0 / red)   * 255.0);
      palette[i].Green = F2T(pow(intensity, 1.0 / green) * 255.0);
      palette[i].Blue  = F2T(pow(intensity, 1.0 / blue)  * 255.0);
   }

   SetGammaCorrectData(palette, ARRAYSIZE(palette), 0, TRUE);
   return ERR_Okay;
#else
   return ERR_NoSupport;
#endif
}

/*****************************************************************************

-METHOD-
SetGammaLinear: Sets the display gamma level using a linear algorithm.

Call SetGammaLinear to update a target display's gamma values with a linear algorithm that takes input from Red, Green
and Blue parameters provided by the client.

-INPUT-
double Red: New red gamma value.
double Green: New green gamma value.
double Blue: New blue gamma value.
int(GMF) Flags: Use GMF_SAVE to store the new settings.

-ERRORS-
Okay:
NullArgs:
-END-

*****************************************************************************/

static ERROR DISPLAY_SetGammaLinear(objDisplay *Self, struct gfxSetGammaLinear *Args)
{
#ifdef __snap__
   parasol::Log log;
   GA_palette palette[256];

   if (!Args) return log.warning(ERR_NullArgs);

   DOUBLE red   = Args->Red;
   DOUBLE green = Args->Green;
   DOUBLE blue  = Args->Blue;

   if (red   < 0.00) red   = 0.00;
   if (green < 0.00) green = 0.00;
   if (blue  < 0.00) blue  = 0.00;

   if (red   > 100.0) red   = 100.0;
   if (green > 100.0) green = 100.0;
   if (blue  > 100.0) blue  = 100.0;

   if (Args->Flags & GMF_SAVE) {
      Self->Gamma[0]   = red;
      Self->Gamma[1] = green;
      Self->Gamma[2]  = blue;
   }

   for (WORD i=0; i < ARRAYSIZE(palette); i++) {
      DOUBLE intensity = (DOUBLE)i / 255.0;

      if (red > 1.0) palette[i].Red = F2T(pow(intensity, 1.0 / red) * 255.0);
      else palette[i].Red = F2T((DOUBLE)i * red);

      if (green > 1.0) palette[i].Green = F2T(pow(intensity, 1.0 / green) * 255.0);
      else palette[i].Green = F2T((DOUBLE)i * green);

      if (blue > 1.0) palette[i].Blue = F2T(pow(intensity, 1.0 / blue) * 255.0);
      else palette[i].Blue = F2T((DOUBLE)i * blue);
   }

   glSNAP->Driver.SetGammaCorrectData(palette, ARRAYSIZE(palette), 0, TRUE);

   return ERR_Okay;
#else
   return ERR_NoSupport;
#endif
}

/*****************************************************************************

-METHOD-
SetMonitor: Changes the default monitor settings.

Use the SetMonitor method to change the settings that configure the user's monitor display.  You can set the model name
of the monitor and the frequencies that are supported by it.  Altering the display frequencies will affect the
available display resolutions, as well as the maximum allowable refresh rate.

An AutoDetect option is available, which if defined will cause the display settings to be automatically detected when
the desktop is loaded at startup. If it is not possible to detect the correct settings for the plugged-in display, it
reverts to the default display settings.

This method does not work on hosted platforms.  All parameters passed to this method are optional (set a value to zero
if it should not be changed).

-INPUT-
cstr Name: The name of the display.
int MinH: The minimum horizontal scan rate.  Usually set to 31.
int MaxH: The maximum horizontal scan rate.
int MinV: The minimum vertical scan rate.  Usually set to 50.
int MaxV: The maximum vertical scan rate.
int(SMF) Flags: Set to SMF_AUTO_DETECT if the monitor settings should be auto-detected on startup.  Set SMF_BIT_6 if the device is limited to 6-bit colour output.

-ERRORS-
Okay
NullArgs
-END-

*****************************************************************************/

static ERROR DISPLAY_SetMonitor(objDisplay *Self, struct gfxSetMonitor *Args)
{
#ifdef __snap__
   parasol::Log log;
   OBJECTPTR config;
   GA_monitor monitor;
   ERROR priverror;

   if (!Args) return log.warning(ERR_NullArgs);

   if (CurrentTaskID() != Self->Head.TaskID) {
      log.warning("Only the owner of the display may call this method.");
      return ERR_Failed;
   }

   log.branch("%s", Args->Name);

   glSixBitDisplay = (Args->Flags & SMF_BIT_6) ? 1 : 0;
   if (glSixBitDisplay) Self->Flags |= SCR_BIT_6;
   else Self->Flags &= ~SCR_BIT_6;

   if (Args->Name) StrCopy(Args->Name, Self->Display, sizeof(Self->Display));

   // Get the current monitor record, then set the new scan rates against it.

   ClearMemory(&monitor, sizeof(monitor));
   glSNAP->Init.GetMonitorInfo(&monitor, glSNAP->Init.GetActiveHead());

   monitor.maxResolution = 0;  // Must be zero for SNAP to filter display modes

   if (Args->MinH) monitor.minHScan = Args->MinH;
   if (Args->MaxH) monitor.maxHScan = Args->MaxH;
   if (Args->MinV) monitor.minVScan = Args->MinV;
   if (Args->MaxV) monitor.maxVScan = Args->MaxV;

   if (monitor.minHScan < 31) monitor.minHScan = 31;
   if (monitor.minVScan < 50) monitor.minVScan = 50;
   if (monitor.maxHScan < 35) monitor.maxHScan = 35;
   if (monitor.maxVScan < 61) monitor.maxVScan = 61;

   // Apply the scan-rate changes to SNAP

   glSNAP->Init.SetMonitorInfo(&monitor, glSNAP->Init.GetActiveHead());

   // Refresh our display information from SNAP

   glSNAP->Init.GetMonitorInfo(&monitor, glSNAP->Init.GetActiveHead());
   Self->MinHScan = monitor.minHScan;
   Self->MaxHScan = monitor.maxHScan;
   Self->MinVScan = monitor.minVScan;
   Self->MaxVScan = monitor.maxVScan;

   // Mark the resolution list for regeneration

   if (Self->ResolutionsMID) {
      if (Self->Resolutions) { ReleaseMemory(Self->Resolutions); Self->Resolutions = NULL; }
      FreeResourceID(Self->ResolutionsMID);
      Self->ResolutionsMID = 0;
      Self->TotalResolutions = 0;
   }

   // Regenerate the screen.xml file

   GenerateDisplayXML();

   // Save the changes to the monitor.cfg file.  This requires admin privileges, so this is only going to work if
   // SetMonitor() is messaged to the core desktop process.

   priverror = SetResource(RES_PRIVILEGEDUSER, 1);

   if (!CreateObject(ID_CONFIG, NF_INTEGRAL, &config, FID_Path|TSTR, "config:hardware/monitor.cfg", TAGEND)) {
      cfgWriteValue(config, "MONITOR", "Name", Self->Display);
      cfgWriteInt(config, "MONITOR", "MinH", Self->MinHScan);
      cfgWriteInt(config, "MONITOR", "MaxH", Self->MaxHScan);
      cfgWriteInt(config, "MONITOR", "MinV", Self->MinVScan);
      cfgWriteInt(config, "MONITOR", "MaxV", Self->MaxVScan);
      cfgWriteInt(config, "MONITOR", "AutoDetect", (Args->Flags & SMF_AUTODETECT) ? 1 : 0);
      cfgWriteInt(config, "MONITOR", "6Bit", glSixBitDisplay);
      acSaveSettings(config);
      acFree(config);
   }

   if (!priverror) SetResource(RES_PRIVILEGEDUSER, 0);
   return ERR_Okay;
#else
   return ERR_NoSupport;
#endif
}

/*****************************************************************************

-ACTION-
Show: Presents a display object to the user.

This method presents a display object to the user.  On a hosted platform, this will result in a window appearing on
screen.  By default the window will be hosted within a window border which may contain regular window gadgets such as a
titlebar and buttons for close, maximise and minimise operations.  The position of the window is determined by the
#X and #Y fields.  In Parasol's native platform, the user's screen display will be altered to match the required
resolution and the graphics of the display's #Bitmap object will take up the entirety of the screen.

If the `BORDERLESS` flag has been set in the #Flags field, the window will appear without the surrounding border
and gadgets normally associated with new windows.

In Microsoft Windows, the #LeftMargin, #RightMargin, #TopMargin and #BottomMargin fields will be updated to reflect
the position of the client area within the hosted window.  In X11 these field values are all set to zero.

If the window is minimised at the time this action is called, the window will be restored to its original position if
the code for the host platform supports this capability.

The `VISIBLE` flag in the #Flags field will be set if the Show operation is successful.
-END-

*****************************************************************************/

ERROR DISPLAY_Show(objDisplay *Self, APTR Void)
{
   parasol::Log log;

   log.branch();

   #ifdef __xwindows__
      if (!XDisplay) {
         log.error("No X11 display has been found for this machine.");
         return ERR_Failed;
      }

      // Some window managers fool with our position when mapping, so we use XMoveWindow() before and after to be
      // certain that we get the position that we want.

      if (!(Self->Flags & SCR_BORDERLESS)) {
         XMoveWindow(XDisplay, Self->XWindowHandle, Self->X, Self->Y);
      }

      XMapWindow(XDisplay, Self->XWindowHandle);

      if (!(Self->Flags & SCR_BORDERLESS)) {
         XMoveWindow(XDisplay, Self->XWindowHandle, Self->X, Self->Y);
      }

      // Mapping a window may cause the window manager to resize it without sending a notification event, so check the
      // window size here.

      XFlush(XDisplay);
      XSync(XDisplay, False);

      Self->LeftMargin   = 0;
      Self->TopMargin    = 0;
      Self->RightMargin  = 0;
      Self->BottomMargin = 0;

      // Post a delayed CheckXWindow() message so that we can respond to changes by the window manager.

      DelayMsg(MT_GfxCheckXWindow, Self->UID, NULL);

      // This really shouldn't be here, but until the management of menu focussing is fixed, we need it.

      if (!StrMatch("SystemDisplay", GetName(Self))) {
         XSetInputFocus(XDisplay, Self->XWindowHandle, RevertToNone, CurrentTime);
      }

   #elif _WIN32

      if (Self->Flags & SCR_MAXIMISE) winShowWindow(Self->WindowHandle, TRUE);
      else winShowWindow(Self->WindowHandle, FALSE);

      winUpdateWindow(Self->WindowHandle);
      winGetMargins(Self->WindowHandle, &Self->LeftMargin, &Self->TopMargin, &Self->RightMargin, &Self->BottomMargin);

   #elif __snap__

      if (glSNAP->Init.GetCurrentRefreshRate) Self->RefreshRate = (glSNAP->Init.GetCurrentRefreshRate() + 50) / 100;
      else Self->RefreshRate = -1;

      ActionTags(MT_GfxSetGamma, Self, Self->Gamma[0], Self->Gamma[1], Self->Gamma[2]);

   #elif _GLES_

      #warning TODO: Bring back the native window if it is hidden.
      glActiveDisplayID = Self->UID;
      Self->Flags &= ~SCR_NOACCELERATION;

   #else
      #error Display code is required for this platform.
   #endif

   Self->Flags |= SCR_VISIBLE;

   if (CheckObjectExists(NULL, "SystemPointer") != ERR_Okay) {
      objPointer *pointer;
      OBJECTID pointer_id;
      if (!NewNamedObject(ID_POINTER, NF_NO_TRACK|NF_PUBLIC|NF_UNIQUE, &pointer, &pointer_id, "SystemPointer")) {
         OBJECTID owner = GetOwner(Self);
         if (GetClassID(owner) IS ID_SURFACE) SetLong(pointer, FID_Surface, owner);

         #ifdef __ANDROID__
            AConfiguration *config;
            if (!adGetConfig(&config)) {
               DOUBLE dp_factor = 160.0 / AConfiguration_getDensity(config);
               pointer->ClickSlop = F2I(8.0 * dp_factor);
               log.trace("Click-slop calculated as %d.", pointer->ClickSlop);
            }
            else log.warning("Failed to get Android Config object.");
         #endif

         if (acInit(pointer) != ERR_Okay) acFree(pointer);
         else acShow(pointer);
         ReleaseObject(pointer);
      }
   }
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
UpdateDisplay: Private. Updates the display using content from a source bitmap.

Called by the Surface class when a surface buffer needs to be exposed to the display.

-INPUT-
obj(Bitmap) Bitmap: Source bitmap.
int X: Source coordinate X.
int Y: Source coordinate Y.
int Width: Source dimension Width.
int Height: Source dimension Height.
int XDest: Destination coordinate X.
int YDest: Destination coordinate Y.

-ERRORS-
Okay
NullArgs
-END-

*****************************************************************************/

static ERROR DISPLAY_UpdateDisplay(objDisplay *Self, struct gfxUpdateDisplay *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Bitmap)) return log.warning(ERR_NullArgs);

   //log.trace("START: %dx%d, %dx%d TO %dx%d.  SBmp: %dx%d", x, y, width, height, xdest, ydest, bmp->Width, bmp->Height);
#ifdef _WIN32
   objBitmap *bmp  = Args->Bitmap;
   objBitmap *dest = Self->Bitmap;
   LONG x      = Args->X;
   LONG y      = Args->Y;
   LONG width  = Args->Width;
   LONG height = Args->Height;
   LONG xdest  = Args->XDest;
   LONG ydest  = Args->YDest;

   // Check if the destination that we are copying to is within the drawable area.

   if ((xdest < dest->Clip.Left)) {
      width = width - (dest->Clip.Left - xdest);
      if (width < 1) return ERR_Okay;
      x = x + (dest->Clip.Left - xdest);
      xdest = dest->Clip.Left;
   }
   else if (xdest >= dest->Clip.Right) return ERR_Okay;

   if ((ydest < dest->Clip.Top)) {
      height = height - (dest->Clip.Top - ydest);
      if (height < 1) return ERR_Okay;
      y = y + (dest->Clip.Top - ydest);
      ydest = dest->Clip.Top;
   }
   else if (ydest >= dest->Clip.Bottom) return ERR_Okay;

   // Check if the source that we are copying from is within its own drawable area.

   if (x < 0) {
      if ((width += x) < 1) return ERR_Okay;
      x = 0;
   }
   else if (x >= bmp->Width) return ERR_Okay;

   if (y < 0) {
      if ((height += y) < 1) return ERR_Okay;
      y = 0;
   }
   else if (y >= bmp->Height) return ERR_Okay;

   // Clip the Width and Height

   if ((xdest + width)  >= dest->Clip.Right)  width  = dest->Clip.Right - xdest;
   if ((ydest + height) >= dest->Clip.Bottom) height = dest->Clip.Bottom - ydest;

   if ((x + width)  >= bmp->Width)  width  = bmp->Width - x;
   if ((y + height) >= bmp->Height) height = bmp->Height - y;

   if (width < 1) return ERR_Okay;
   if (height < 1) return ERR_Okay;

   // Adjust coordinates by offset values

   x += bmp->XOffset;
   y += bmp->YOffset;
   xdest += dest->XOffset;
   ydest += dest->YOffset;

   APTR drawable;
   GetPointer(dest, FID_Handle, &drawable);

   win32RedrawWindow(Self->WindowHandle, drawable,
      x, y,
      width, height,
      xdest, ydest,
      bmp->Width, bmp->Height,
      bmp->BitsPerPixel, bmp->Data,
      bmp->ColourFormat->RedMask   << bmp->ColourFormat->RedPos,
      bmp->ColourFormat->GreenMask << bmp->ColourFormat->GreenPos,
      bmp->ColourFormat->BlueMask  << bmp->ColourFormat->BluePos,
      (Self->Flags & SCR_COMPOSITE) ? (bmp->ColourFormat->AlphaMask << bmp->ColourFormat->AlphaPos) : 0,
      Self->Opacity);
   return ERR_Okay;
#else
   return(gfxCopyArea(Args->Bitmap, Self->Bitmap, NULL,
      Args->X, Args->Y, Args->Width, Args->Height, Args->XDest, Args->YDest));
#endif
}

/*****************************************************************************

-METHOD-
UpdatePalette: Updates the video display palette to new colour values if in 256 colour mode.

Call UpdatePalette to copy a new palette to the display bitmap's internal palette.  If the video display is running in
256 colour mode, the new palette colours will also be reflected in the display.

This method has no visible effect on RGB pixel displays.

-INPUT-
struct(*RGBPalette) NewPalette: The new palette to apply to the display bitmap.

-ERRORS-
Okay
NullArgs

*****************************************************************************/

static ERROR DISPLAY_UpdatePalette(objDisplay *Self, struct gfxUpdatePalette *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->NewPalette)) return ERR_NullArgs;

   log.branch("Palette: %p, Colours: %d", Args->NewPalette, Args->NewPalette->AmtColours);

   if (Args->NewPalette->AmtColours > 256) {
      log.warning("Bad setting of %d colours in the new palette.", Args->NewPalette->AmtColours);
      Args->NewPalette->AmtColours = 256;
   }

   CopyMemory(Args->NewPalette, Self->Bitmap->Palette, sizeof(*Args->NewPalette));

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
WaitVBL: Waits for a vertical blank.

This method waits for the strobe to reach the vertical blank area at the bottom of the display.  Not all graphics
hardware will support this method.  If this is the case, WaitVBL() will return immediately with ERR_NoSupport.

-ERRORS-
Okay
NoSupport

*****************************************************************************/

ERROR DISPLAY_WaitVBL(objDisplay *Self, APTR Void)
{
   return ERR_NoSupport;
}

/*****************************************************************************

-FIELD-
Bitmap: Reference to the display's bitmap information.

The Bitmap object describes the video region that will be used for displaying graphics. It holds details on the width,
height, type, number of colours and so on.  The display class inherits the bitmap's attributes, so it is not necessary
to retrieve a direct reference to the bitmap object in order to make adjustments.

The bitmap's width and height can be larger than the display area, but must never be smaller than the display area.

-FIELD-
BmpX: The horizontal coordinate of the bitmap within a display.

This field defines the horizontal offset for the #Bitmap, which is positioned 'behind' the display. To achieve
hardware scrolling, call the #Move() action on the Bitmap in order to change this value and update the display.

-FIELD-
BmpY: The vertical coordinate of the Bitmap within a display.

This field defines the vertical offset for the #Bitmap, which is positioned 'behind' the display. If you want
to achieve hardware scrolling, you will need to call the Move action on the Bitmap in order to change this value and
update the display.

-FIELD-
BottomMargin: In hosted mode, indicates the bottom margin of the client window.

If the display is hosted in a client window, the BottomMargin indicates the number of pixels between the client area
and the bottom window edge.

-FIELD-
CertificationDate: String describing the date of the graphics driver's certification.

The string in this field describes the date on which the graphics card driver was certified.  If this information is
not available from the driver, a NULL pointer is returned.

*****************************************************************************/

static ERROR GET_CertificationDate(objDisplay *Self, STRING *Value)
{
   *Value = Self->CertificationDate;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Chipset: String describing the graphics chipset.

The string in this field describes the graphic card's chipset.  If this information is not retrievable, a NULL pointer
is returned.

*****************************************************************************/

static ERROR GET_Chipset(objDisplay *Self, STRING *Value)
{
   *Value = Self->Chipset;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
HDensity: Returns the horizontal pixel density for the display.

Reading the HDensity field will return the horizontal pixel density for the display (pixels per inch).  If the physical
size of the display is unknown, a default value based on knowledge of the platform will be retuned.  For standard PC's
this will usually be 96.

A custom density value can be enforced by setting the /interface/@dpi value in the loaded style, or by setting the
HDensity field in the Display object.

Reading this field always succeeds.

*****************************************************************************/

ERROR GET_HDensity(objDisplay *Self, LONG *Value)
{
   if (Self->HDensity) {
      *Value = Self->HDensity;
      return ERR_Okay;
   }

   #ifdef __ANDROID__
      Self->HDensity = 160; // Android devices tend to have a high DPI by default (compared to monitors)
   #else
      Self->HDensity = 96; // Standard PC DPI, matches Windows
   #endif

   // If the user has overridden the DPI with a preferred value, we have to use it.

   OBJECTID style_id;
   LONG count = 1;
   if (!FindObject("glStyle", ID_XML, 0, &style_id, &count)) {
      objXML *style;
      if (!AccessObject(style_id, 3000, &style)) {
         char strdpi[32];
         if (!acGetVar(style, "/interface/@dpi", strdpi, sizeof(strdpi))) {
            *Value = StrToInt(strdpi);
            Self->HDensity = *Value; // Store for future use.
            if (!Self->VDensity) Self->VDensity = Self->HDensity;
         }
         ReleaseObject(style);
         if (*Value >= 96) return ERR_Okay;
      }
   }

   #ifdef __ANDROID__
      AConfiguration *config;
      if (!adGetConfig(&config)) {
         LONG density = AConfiguration_getDensity(config);
         if ((density > 60) and (density < 20000)) {
            Self->HDensity = density;
            Self->VDensity = density;
         }
      }
   #elif _WIN32
      winGetDPI(&Self->HDensity, &Self->VDensity);
      if (Self->HDensity < 96) Self->HDensity = 96;
      if (Self->VDensity < 96) Self->VDensity = 96;
   #endif

   *Value = Self->HDensity;
   return ERR_Okay;
}

static ERROR SET_HDensity(objDisplay *Self, LONG Value)
{
   Self->HDensity = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
VDensity: Returns the vertical pixel density for the display.

Reading the VDensity field will return the vertical pixel density for the display (pixels per inch).  If the physical
size of the display is unknown, a default value based on knowledge of the platform will be retuned.  For standard PC's
this will usually be 96.

A custom density value can be enforced by setting the /interface/@dpi value in the loaded style, or by setting the
VDensity field in the Display object.

Reading this field always succeeds.

*****************************************************************************/

ERROR GET_VDensity(objDisplay *Self, LONG *Value)
{
   if (Self->VDensity) {
      *Value = Self->VDensity;
      return ERR_Okay;
   }

   #ifdef __ANDROID__
      Self->VDensity = 160; // Android devices tend to have a high DPI by default (compared to monitors)
   #else
      Self->VDensity = 96; // Standard PC DPI, matches Windows
   #endif

   // If the user has overridden the DPI with a preferred value, we have to use it.

   OBJECTID style_id;
   LONG count = 1;
   if (!FindObject("glStyle", ID_XML, 0, &style_id, &count)) {
      objXML *style;
      if (!AccessObject(style_id, 3000, &style)) {
         char strdpi[32];
         if (!acGetVar(style, "/interface/@dpi", strdpi, sizeof(strdpi))) {
            *Value = StrToInt(strdpi);
            Self->VDensity = *Value;
            if (!Self->HDensity) Self->HDensity = Self->VDensity;
         }
         ReleaseObject(style);
         if (*Value >= 96) return ERR_Okay;
      }
   }

   #ifdef __ANDROID__
      AConfiguration *config;
      if (!adGetConfig(&config)) {
         LONG density = AConfiguration_getDensity(config);
         if ((density > 60) and (density < 20000)) {
            Self->HDensity = density;
            Self->VDensity = density;
         }
      }
   #elif _WIN32
      winGetDPI(&Self->HDensity, &Self->VDensity);
      if (Self->HDensity < 96) Self->HDensity = 96;
      if (Self->VDensity < 96) Self->VDensity = 96;
   #endif

   *Value = Self->VDensity;
   return ERR_Okay;
}

static ERROR SET_VDensity(objDisplay *Self, LONG Value)
{
   Self->VDensity = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Display: String describing the display (e.g. model name of the monitor).

The string in this field describes the display device that is connected to the user's graphics card.  If this
information is not detectable, a NULL pointer is returned.

*****************************************************************************/

static ERROR GET_Display(objDisplay *Self, CSTRING *Value)
{
   if (Self->Display[0]) *Value = Self->Display;
   else *Value = NULL;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
DisplayManufacturer: String describing the display manufacturer.

The string in this field returns the name of the manufacturer that created the user's display device.  If this
information is not detectable, a NULL pointer is returned.

*****************************************************************************/

static ERROR GET_DisplayManufacturer(objDisplay *Self, CSTRING *Value)
{
   if (Self->DisplayManufacturer[0]) *Value = Self->DisplayManufacturer;
   else *Value = NULL;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
DisplayType: In hosted mode, indicates the bottom margin of the client window.

If the display is hosted in a client window, the BottomMargin indicates the number of pixels between the client area
and the bottom window edge.

-FIELD-
DPMS: Holds the default display power management method.

When DPMS is enabled via a call to #Disable(), the DPMS method that is applied is controlled by this field.

DPMS is a user configurable option and it is not recommended that the DPMS value is changed manually.

-FIELD-
DriverCopyright: String containing copyright information on the graphics driver software.

The string in this field returns copyright information related to the graphics driver.  If this information is not
available, a NULL pointer is returned.

*****************************************************************************/

static ERROR GET_DriverCopyright(objDisplay *Self, CSTRING *Value)
{
   if (Self->DriverCopyright[0]) *Value = Self->DriverCopyright;
   else *Value = NULL;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
DriverVersion: String describing the version of the graphics hardware driver.

The string in this field describes the graphic driver's version number. If this information is not detectable, a NULL
pointer is returned.

*****************************************************************************/

static ERROR GET_DriverVersion(objDisplay *Self, CSTRING *Value)
{
   if (Self->DriverVersion[0]) *Value = Self->DriverVersion;
   else *Value = NULL;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
DriverVendor: String describing the vendor of the graphics driver.

The string in this field returns the name of the vendor that supplied the graphics card driver.  If this information is
not available, a NULL pointer is returned.

*****************************************************************************/

static ERROR GET_DriverVendor(objDisplay *Self, CSTRING *Value)
{
   *Value = Self->DriverVendor;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Flags: Optional flag settings.

Optional display flags can be defined here.  Post-initialisation, the only flags that can be set are AUTO_SAVE and
BORDERLESS.

*****************************************************************************/

static ERROR SET_Flags(objDisplay *Self, LONG Value)
{
   parasol::Log log;

   if (Self->Head::Flags & NF_INITIALISED) {
      // Only flags that are explicitly supported here may be set post-initialisation.

      #define ACCEPT_FLAGS (SCR_AUTO_SAVE)
      LONG accept = Value & ACCEPT_FLAGS;
      Self->Flags = (Self->Flags & (~ACCEPT_FLAGS)) | accept;

      if (((Self->Flags & SCR_BORDERLESS) and (!(Value & SCR_BORDERLESS))) or
          (!(Self->Flags & SCR_BORDERLESS) and (Value & SCR_BORDERLESS))) {
      #ifdef _WIN32

         log.msg("Switching window type.");

         bool maximise = TRUE;
         STRING title;
         GetString(Self, FID_Title, &title); // Get the window title before we kill it

         OBJECTID surface_id = winLookupSurfaceID(Self->WindowHandle);
         winSetSurfaceID(Self->WindowHandle, 0); // Nullify the surface ID to prevent WM_DESTROY from being acted upon
         winDestroyWindow(Self->WindowHandle);

         HWND popover = 0;
         if ((Self->WindowHandle = winCreateScreen(popover, &Self->X, &Self->Y, &Self->Width, &Self->Height,
               maximise, (Self->Flags & SCR_BORDERLESS) ? FALSE : TRUE, title, FALSE, 255, TRUE))) {

            Self->Flags ^= SCR_BORDERLESS;

            winSetSurfaceID(Self->WindowHandle, surface_id);
            winGetMargins(Self->WindowHandle, &Self->LeftMargin, &Self->TopMargin, &Self->RightMargin, &Self->BottomMargin);

            // Report the new window dimensions

            int winx, winy, winwidth, winheight, cx, cy, cwidth, cheight;
            winGetCoords(Self->WindowHandle, &winx, &winy, &winwidth, &winheight, &cx, &cy, &cwidth, &cheight);

            Self->X = winx;
            Self->Y = winy;
            Self->Width  = winwidth;
            Self->Height = winheight;

            resize_feedback(&Self->ResizeFeedback, Self->UID, cx, cy, cwidth, cheight);

            if (Self->Flags & SCR_VISIBLE) {
               winShowWindow(Self->WindowHandle, TRUE);
               DelayMsg(AC_Focus, Self->UID, NULL);
            }
         }

      #elif __xwindows__

         if (x11WindowManager()) return ERR_NoSupport;

         Atom protocols[2];
         XSizeHints hints;
         XSetWindowAttributes swa;
         LONG cwflags;
         STRING name;

         log.msg("Destroying current window.");

         swa.event_mask  = 0;
         XChangeWindowAttributes(XDisplay, Self->XWindowHandle, CWEventMask, &swa);

         XDestroyWindow(XDisplay, Self->XWindowHandle);
         Self->WindowHandle = NULL;

         Self->Flags ^= SCR_BORDERLESS;

         swa.bit_gravity = CenterGravity;
         swa.win_gravity = CenterGravity;
         swa.cursor      = C_Default;
         swa.override_redirect = (Self->Flags & SCR_BORDERLESS) ? 1 : 0;
         swa.event_mask  = ExposureMask|EnterWindowMask|LeaveWindowMask|PointerMotionMask|StructureNotifyMask
                           |KeyPressMask|KeyReleaseMask|ButtonPressMask|ButtonReleaseMask|FocusChangeMask;

         cwflags = CWEventMask|CWOverrideRedirect;

         if (Self->Flags & SCR_BORDERLESS) {
            Self->X = 0;
            Self->Y = 0;
            Self->Width  = glRootWindow.width;
            Self->Height = glRootWindow.height;
         }
         else {
            Self->X = 50;
            Self->Y = 70;
            Self->Width  = glRootWindow.width - 100;
            Self->Height = glRootWindow.height - 140;
            if (Self->X+Self->Width > glRootWindow.width) Self->Width = glRootWindow.width - Self->X;
            if (Self->Y+Self->Height > glRootWindow.height) Self->Height = glRootWindow.height - Self->Y;
         }

         if (!(Self->WindowHandle = (APTR)XCreateWindow(XDisplay, DefaultRootWindow(XDisplay),
            Self->X, Self->Y,
            Self->Width, Self->Height, 0, CopyFromParent, InputOutput,
            CopyFromParent, cwflags, &swa))) {
            log.warning("Failed in call to XCreateWindow().");
            return ERR_Failed;
         }

         if ((GetPointer(CurrentTask(), FID_Name, &name) IS ERR_Okay) and (name)) {
            XStoreName(XDisplay, Self->XWindowHandle, name);
         }
         else XStoreName(XDisplay, Self->XWindowHandle, "Parasol");

         protocols[0] = XWADeleteWindow;
         XSetWMProtocols(XDisplay, Self->XWindowHandle, protocols, 1);

         if (glStickToFront) {
            XSetTransientForHint(XDisplay, Self->XWindowHandle, DefaultRootWindow(XDisplay));
         }

         XChangeProperty(XDisplay, Self->XWindowHandle, atomSurfaceID, atomSurfaceID, 32, PropModeReplace, (UBYTE *)&Self->UID, 1);

         // Indicate that the window position is not to be meddled with by the window manager.

         hints.flags = USPosition|USSize;
         XSetWMNormalHints(XDisplay, Self->XWindowHandle, &hints);

         // The keyboard qualifiers need to be reset, because if the user is holding down any keys we will lose any
         // key-release messages due on the window that we've terminated.

         glKeyFlags = 0;

         SetPointer(Self->Bitmap, FID_Handle, Self->WindowHandle);
         acResize(Self->Bitmap, Self->Width, Self->Height, 0);

         if (Self->Flags & SCR_VISIBLE) {
            acShow(Self);
            XSetInputFocus(XDisplay, Self->XWindowHandle, RevertToNone, CurrentTime);
            DelayMsg(AC_Focus, Self->UID, NULL);
         }

         resize_feedback(&Self->ResizeFeedback, Self->UID, Self->X, Self->Y, Self->Width, Self->Height);
      #endif
      }

      if ((Self->Flags & SCR_MAXIMISE) and (!(Value & SCR_MAXIMISE))) { // Turn maximise off
         #ifdef _WIN32
            if (Self->Flags & SCR_VISIBLE) winShowWindow(Self->WindowHandle, FALSE);
            Self->Flags |= SCR_MAXIMISE;
         #elif __xwindows__

         #endif
      }

      if (!(Self->Flags & SCR_MAXIMISE) and (Value & SCR_MAXIMISE)) { // Turn maximise on
         #ifdef _WIN32
            if (Self->Flags & SCR_VISIBLE) winShowWindow(Self->WindowHandle, TRUE);
            Self->Flags |= SCR_MAXIMISE;
         #elif __xwindows__

         #endif
      }
   }
   else Self->Flags = (Value) & (~SCR_READ_ONLY);

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Gamma: Contains red, green and blue values for the display's gamma setting.

The gamma settings for the display are stored in this field.  The settings are stored in an array of 3 floating point
values that represent red, green and blue colours guns.  The default gamma value for each colour gun is 1.0.

To modify the display gamma values, please refer to the #SetGamma() and #SetGammaLinear() methods.

*****************************************************************************/

static ERROR GET_Gamma(objDisplay *Self, DOUBLE **Value, LONG *Elements)
{
   *Elements = 3;
   *Value = Self->Gamma;
   return ERR_Okay;
}

static ERROR SET_Gamma(objDisplay *Self, DOUBLE *Value, LONG Elements)
{
   if (Value) {
      if (Elements > 3) Elements = 3;
      WORD i;
      for (i=0; i < Elements; i++) Self->Gamma[i] = Value[i];
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Height: Defines the height of the display.

This field defines the height of a display.  This is known as the 'viewport' that the bitmap data is displayed through.
If the height exceeds allowable limits, it will be restricted to a value that the display hardware can handle.

If the display is hosted, the height reflects the internal height of the host window.  On some hosted systems, the true
height of the window can be calculated by reading the #TopMargin and #BottomMargin fields.

*****************************************************************************/

static ERROR SET_Height(objDisplay *Self, LONG Value)
{
   if (Value > 0) Self->Height = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
InsideHeight: Represents the internal height of the display.

On full-screen displays, the video data area can exceed the height of the screen display.  The InsideHeight reflects
the height of the video data in pixels.  If this feature is not in use or is unavailable, the InsideWidth is equal to
the display #Height.

*****************************************************************************/

static ERROR GET_InsideHeight(objDisplay *Self, LONG *Value)
{
   *Value = Self->Bitmap->Height;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
InsideWidth: Represents the internal width of the display.

On full-screen displays, the video data area can exceed the width of the screen display.  The InsideWidth reflects the
width of the video data in pixels.  If this feature is not in use or is unavailable, the InsideWidth is equal to the
display #Width.

*****************************************************************************/

static ERROR GET_InsideWidth(objDisplay *Self, LONG *Value)
{
   *Value = Self->Bitmap->Width;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
LeftMargin: In hosted mode, indicates the left-hand margin of the client window.

If the display is hosted in a client window, the LeftMargin indicates the number of pixels between the client area and
the left window edge.

-FIELD-
Manufacturer: String describing the manufacturer of the graphics hardware.

The string in this field returns the name of the manufacturer that created the user's graphics card.  If this
information is not detectable, a NULL pointer is returned.

*****************************************************************************/

static ERROR GET_Manufacturer(objDisplay *Self, STRING *Value)
{
   if (Self->Manufacturer[0]) *Value = Self->Manufacturer;
   else *Value = NULL;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
MaxHScan: The maximum horizontal scan rate of the display output device.

If the display output device supports variable refresh rates, this field will refer to the maximum horizontal scan rate
supported by the device.  If variable refresh rates are not supported, this field is set to zero.

-FIELD-
MaxVScan: The maximum vertical scan rate of the display output device.

If the display output device supports variable refresh rates, this field will refer to the maximum vertical scan rate
supported by the device.  If variable refresh rates are not supported, this field is set to zero.

-FIELD-
MinHScan: The minimum horizontal scan rate of the display output device.

If the display output device supports variable refresh rates, this field will refer to the minimum horizontal scan rate
supported by the device.  If variable refresh rates are not supported, this field is set to zero.

-FIELD-
MinVScan: The minimum vertical scan rate of the display output device.

If the display output device supports variable refresh rates, this field will refer to the minimum vertical scan rate
supported by the device.  If variable refresh rates are not supported, this field is set to zero.

-FIELD-
Opacity: Determines the level of translucency applied to the display (hosted displays only).

This field determines the translucency level applied to a display. Its support level is limited to hosted displays that
support translucent windows (for example, Windows XP).  The default setting is 100%, which means that the display will
be solid.  High values will retain the boldness of the display, while low values reduce visibility.

****************************************************************************/

static ERROR GET_Opacity(objDisplay *Self, DOUBLE *Value)
{
   *Value = Self->Opacity * 100 / 255;
   return ERR_Okay;
}

static ERROR SET_Opacity(objDisplay *Self, DOUBLE Value)
{
#ifdef _WIN32
   if (Value < 0) Self->Opacity = 0;
   else if (Value > 100) Self->Opacity = 255;
   else Self->Opacity = Value * 255 / 100;
   return ERR_Okay;
#else
   return ERR_NoSupport;
#endif
}

/*****************************************************************************

-FIELD-
PopOver: Enables pop-over support for hosted display windows.

The PopOver field can be used when a display is hosted as a window.  Setting the PopOver field to refer to the object
ID of another display will ensure that the host window is always in front of the other display's window (assuming both
windows are visible on the desktop).

The ERR_NoSupport error code is returned if the host does not support this functionality or if the display owns the
output device.

*****************************************************************************/

static ERROR SET_PopOver(objDisplay *Self, OBJECTID Value)
{
   parasol::Log log;

#ifdef __xwindows__

   if (Self->Head::Flags & NF_INITIALISED) {
      objDisplay *popover;
      if (!Value) {
         Self->PopOverID = 0;
         XSetTransientForHint(XDisplay, Self->XWindowHandle, (Window)0);
      }
      else if (!AccessObject(Value, 2000, &popover)) {
         if (popover->Head.ClassID IS ID_DISPLAY) {
            Self->PopOverID = Value;
            XSetTransientForHint(XDisplay, Self->XWindowHandle, (Window)popover->WindowHandle);
         }
         ReleaseObject(popover);
      }
      else return ERR_AccessObject;
   }
   else if (Value) {
      if (GetClassID(Value) IS ID_DISPLAY) {
         Self->PopOverID = Value;
      }
      else return log.warning(ERR_WrongClass);
   }
   else Self->PopOverID = 0;

   return ERR_Okay;

#elif _WIN32

   if (Value) {
      if (GetClassID(Value) IS ID_DISPLAY) Self->PopOverID = Value;
      else return log.warning(ERR_WrongClass);
   }
   else Self->PopOverID = 0;

   return ERR_Okay;

#else

   return ERR_NoSupport;

#endif
}

/*****************************************************************************

-FIELD-
RefreshRate: This field manages the display refresh rate.

The value in this field reflects the refresh rate of the currently active display, if operating in full-screen mode.

*****************************************************************************/

static ERROR SET_RefreshRate(objDisplay *Self, DOUBLE Value)
{
   return ERR_NoSupport;
}

/*****************************************************************************

-FIELD-
ResizeFeedback: This field manages the display refresh rate.

The value in this field reflects the refresh rate of the currently active display, if operating in full-screen mode.

*****************************************************************************/

static ERROR GET_ResizeFeedback(objDisplay *Self, FUNCTION **Value)
{
   if (Self->ResizeFeedback.Type != CALL_NONE) {
      *Value = &Self->ResizeFeedback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_ResizeFeedback(objDisplay *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->ResizeFeedback.Type IS CALL_SCRIPT) UnsubscribeAction(Self->ResizeFeedback.Script.Script, AC_Free);
      Self->ResizeFeedback = *Value;
      if (Self->ResizeFeedback.Type IS CALL_SCRIPT) SubscribeAction(Self->ResizeFeedback.Script.Script, AC_Free);
   }
   else Self->ResizeFeedback.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
RightMargin: In hosted mode, indicates the pixel margin between the client window and right window edge.

-FIELD-
TopMargin: In hosted mode, indicates the pixel margin between the client window and top window edge.

-FIELD-
TotalMemory: The total amount of user accessible RAM installed on the video card, or zero if unknown.

-FIELD-
TotalResolutions: The total number of resolutions supported by the display.

*****************************************************************************/

static ERROR GET_TotalResolutions(objDisplay *Self, LONG *Value)
{
   if (Self->TotalResolutions) {
      *Value = Self->TotalResolutions;
      return ERR_Okay;
   }
   else if (get_resolutions(Self)) {
      *Value = Self->TotalResolutions;
      return ERR_Okay;
   }
   else return ERR_Failed;
}

/*****************************************************************************

-FIELD-
Width: Defines the width of the display.

This field defines the width of a display.  This is known as the 'viewport' that the bitmap data is displayed through.
If the width exceeds allowable limits, it will be restricted to a value that the display hardware can handle.

If the display is hosted, the width reflects the internal width of the host window.  On some hosted systems, the true
width of the window can be calculated by reading the #LeftMargin and #RightMargin fields.

*****************************************************************************/

static ERROR SET_Width(objDisplay *Self, LONG Value)
{
   if (Value > 0) {
      if (Self->Head::Flags & NF_INITIALISED) {
         acResize(Self, Value, Self->Height, 0);
      }
      else Self->Width = Value;
      return ERR_Okay;
   }
   else return ERR_OutOfRange;
}

/*****************************************************************************
-FIELD-
WindowHandle: Refers to a display object's window handle, if relevant.

This field refers to the window handle of a display object, but only if such a thing is relevant to the platform that
the system is running on.  Currently, this field is only usable when creating a display within an X11 window manager or
Microsoft Windows.

It is possible to set the WindowHandle field prior to initialisation if you want a display object to be based on a
window that already exists.

*****************************************************************************/

static ERROR GET_WindowHandle(objDisplay *Self, APTR *Value)
{
   *Value = Self->WindowHandle;
   return ERR_Okay;
}

static ERROR SET_WindowHandle(objDisplay *Self, APTR Value)
{
   if (Self->Head::Flags & NF_INITIALISED) return ERR_Failed;

   if (Value) {
      Self->WindowHandle = Value;
      Self->Flags |= SCR_CUSTOM_WINDOW;
      #ifdef __xwindows__
         glPlugin = TRUE;
      #endif
   }

   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Title: Sets the window title (hosted environments only).

*****************************************************************************/

#if defined(_WIN32)
static STRING glWindowTitle = NULL;
#endif

static ERROR GET_Title(objDisplay *Self, CSTRING *Value)
{
#ifdef __xwindows__
   return ERR_NoSupport;
#elif _WIN32
   char buffer[128];
   STRING str;

   buffer[0] = 0;
   winGetWindowTitle(Self->WindowHandle, buffer, sizeof(buffer));
   if (!AllocMemory(StrLength(buffer) + 1, MEM_STRING|MEM_UNTRACKED, &str, NULL)) {
      StrCopy(buffer, str, COPY_ALL);
      if (glWindowTitle) FreeResource(glWindowTitle);
      glWindowTitle = str;
      *Value = glWindowTitle;
      return ERR_Okay;
   }
   else return ERR_AllocMemory;
#else
   return ERR_NoSupport;
#endif
}

static ERROR SET_Title(objDisplay *Self, CSTRING Value)
{
#ifdef __xwindows__
   XStoreName(XDisplay, Self->XWindowHandle, Value);
   return ERR_Okay;
#elif _WIN32
   winSetWindowTitle(Self->WindowHandle, Value);
   return ERR_Okay;
#else
   return ERR_NoSupport;
#endif
}

/*****************************************************************************
-FIELD-
X: Defines the horizontal coordinate of the display.

The X field defines the horizontal hardware coordinate for a display.  This field should be set to zero unless the
screen requires adjustment.  Most hardware drivers and output devices do not support this feature.

On hosted displays, prior to initialisation the coordinate will reflect the position of the display window when it is
created.  After initialisation, the coordinate is altered to reflect the absolute position of the client area of the
display window.  The #LeftMargin can be used to determine the actual position of the host window.

To adjust the position of the display, use the #MoveToPoint() action rather than setting this field directly.

*****************************************************************************/

static ERROR SET_X(objDisplay *Self, LONG Value)
{
   if (!(Self->Head::Flags & NF_INITIALISED)) {
      Self->X = Value;
      return ERR_Okay;
   }
   else return acMoveToPoint(Self, Value, 0, 0, MTF_X);
}

/*****************************************************************************
-FIELD-
Y: Defines the vertical coordinate of the display.

The Y field defines the vertical hardware coordinate for a display.  This field should be set to zero unless the
screen requires adjustment.  Most hardware drivers and output devices do not support this feature.

On hosted displays, prior to initialisation the coordinate will reflect the position of the display window  when it is
created.  After initialisation, the coordinate is altered to reflect the absolute position of the client area of the
display window.  The #TopMargin can be used to determine the actual position of the host window.

To adjust the position of the display, use the #MoveToPoint() action rather than setting this field directly.
-END-
*****************************************************************************/

static ERROR SET_Y(objDisplay *Self, LONG Value)
{
   if (!(Self->Head::Flags & NF_INITIALISED)) {
      Self->Y = Value;
      return ERR_Okay;
   }
   else return acMoveToPoint(Self, 0, Value, 0, MTF_Y);
}

//****************************************************************************
// Attempt to create a display buffer (process is not guaranteed, programmer has to check the Buffer field to know if
// this succeeded or not).

void alloc_display_buffer(objDisplay *Self)
{
   parasol::Log log(__FUNCTION__);

   log.branch("Allocating a video based buffer bitmap.");

   if (Self->BufferID) { acFreeID(Self->BufferID); Self->BufferID = 0; }

   objBitmap *buffer;
   ERROR error;
   if (!NewLockedObject(ID_BITMAP, NF_INTEGRAL|Self->Head::Flags, &buffer, &Self->BufferID)) {
      if (!SetFields(buffer,
            FID_Name|TSTR,           "SystemBuffer",
            FID_BitsPerPixel|TLONG,  Self->Bitmap->BitsPerPixel,
            FID_BytesPerPixel|TLONG, Self->Bitmap->BytesPerPixel,
            FID_Width|TLONG,         Self->Bitmap->Width,
            FID_Height|TLONG,        Self->Bitmap->Height,
            #ifdef __xwindows__
               FID_DataFlags|TLONG,  MEM_DATA,
            #else
               FID_DataFlags|TLONG,  MEM_TEXTURE,
            #endif
            TAGEND)) {
         if (!acInit(buffer)) error = ERR_Okay;
         else error = ERR_Init;
      }
      else error = ERR_SetField;

      if (error) { acFree(buffer); Self->BufferID = 0; }

      ReleaseObject(buffer);
   }
   else error = ERR_NewObject;
}

//****************************************************************************

#include "class_display_def.c"

static const FieldArray DisplayFields[] = {
   { "RefreshRate",    FDF_DOUBLE|FDF_RW,     0, NULL, (APTR)SET_RefreshRate },
   { "Bitmap",         FDF_INTEGRAL|FDF_R,      ID_BITMAP, NULL, NULL },
   { "Flags",          FDF_LONGFLAGS|FDF_RW, (MAXINT)&clDisplayFlags, NULL, (APTR)SET_Flags },
   { "Width",          FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_Width },
   { "Height",         FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_Height },
   { "X",              FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_X },
   { "Y",              FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_Y },
   { "BmpX",           FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "BmpY",           FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "Buffer",         FDF_OBJECTID|FDF_R|FDF_SYSTEM, ID_BITMAP, NULL, NULL },
   { "TotalMemory",    FDF_LONG|FDF_R,       0, NULL, NULL },
   { "MinHScan",       FDF_LONG|FDF_R,       0, NULL, NULL },
   { "MaxHScan",       FDF_LONG|FDF_R,       0, NULL, NULL },
   { "MinVScan",       FDF_LONG|FDF_R,       0, NULL, NULL },
   { "MaxVScan",       FDF_LONG|FDF_R,       0, NULL, NULL },
   { "DisplayType",    FDF_LONG|FDF_LOOKUP|FDF_R,  (MAXINT)&clDisplayDisplayType, NULL, NULL },
   { "DPMS",           FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clDisplayDPMS, NULL, NULL },
   { "PopOver",        FDF_OBJECTID|FDF_W,   0, NULL, (APTR)SET_PopOver },
   { "LeftMargin",     FDF_LONG|FDF_R,       0, NULL, NULL },
   { "RightMargin",    FDF_LONG|FDF_R,       0, NULL, NULL },
   { "TopMargin",      FDF_LONG|FDF_R,       0, NULL, NULL },
   { "BottomMargin",   FDF_LONG|FDF_R,       0, NULL, NULL },
   // Virtual fields
   { "CertificationDate",   FDF_VIRTUAL|FDF_STRING|FDF_R,    0, (APTR)GET_CertificationDate,   NULL },
   { "Chipset",             FDF_VIRTUAL|FDF_STRING|FDF_R,    0, (APTR)GET_Chipset,             NULL },
   { "Gamma",               FDF_VIRTUAL|FDF_DOUBLE|FDF_ARRAY|FDF_RI, 0, (APTR)GET_Gamma,       (APTR)SET_Gamma },
   { "HDensity",            FDF_VIRTUAL|FDF_LONG|FDF_RW,     0, (APTR)GET_HDensity,            (APTR)SET_HDensity },
   { "VDensity",            FDF_VIRTUAL|FDF_LONG|FDF_RW,     0, (APTR)GET_VDensity,            (APTR)SET_VDensity },
   { "Display",             FDF_VIRTUAL|FDF_STRING|FDF_R,    0, (APTR)GET_Display,             NULL },
   { "DisplayManufacturer", FDF_VIRTUAL|FDF_STRING|FDF_R,    0, (APTR)GET_DisplayManufacturer, NULL },
   { "DriverCopyright",     FDF_VIRTUAL|FDF_STRING|FDF_R,    0, (APTR)GET_DriverCopyright,     NULL },
   { "DriverVendor",        FDF_VIRTUAL|FDF_STRING|FDF_R,    0, (APTR)GET_DriverVendor,        NULL },
   { "DriverVersion",       FDF_VIRTUAL|FDF_STRING|FDF_R,    0, (APTR)GET_DriverVersion,       NULL },
   { "InsideWidth",         FDF_VIRTUAL|FDF_LONG|FDF_R,      0, (APTR)GET_InsideWidth,         NULL },
   { "InsideHeight",        FDF_VIRTUAL|FDF_LONG|FDF_R,      0, (APTR)GET_InsideHeight,        NULL },
   { "Manufacturer",        FDF_VIRTUAL|FDF_STRING|FDF_R,    0, (APTR)GET_Manufacturer,        NULL },
   { "Opacity",             FDF_VIRTUAL|FDF_DOUBLE|FDF_W,    0, (APTR)GET_Opacity,             (APTR)SET_Opacity },
   { "ResizeFeedback",      FDF_VIRTUAL|FDF_FUNCTION|FDF_RW, 0, (APTR)GET_ResizeFeedback,      (APTR)SET_ResizeFeedback },
   { "WindowHandle",        FDF_VIRTUAL|FDF_POINTER|FDF_RW,  0, (APTR)GET_WindowHandle,        (APTR)SET_WindowHandle },
   { "Title",               FDF_VIRTUAL|FDF_STRING|FDF_RW,   0, (APTR)GET_Title,               (APTR)SET_Title },
   { "TotalResolutions",    FDF_VIRTUAL|FDF_LONG|FDF_R,      0, (APTR)GET_TotalResolutions,    NULL },
   END_FIELD
};

//****************************************************************************

CSTRING dpms_name(LONG Index)
{
   return clDisplayDPMS[Index].Name;
}

//****************************************************************************

ERROR create_display_class(void)
{
   return(CreateObject(ID_METACLASS, 0, &clDisplay,
      FID_ClassVersion|TFLOAT, VER_DISPLAY,
      FID_Name|TSTRING,   "Display",
      FID_Category|TLONG, CCF_GRAPHICS,
      FID_Flags|TLONG,    CLF_PROMOTE_INTEGRAL,
      FID_Actions|TPTR,   clDisplayActions,
      FID_Methods|TARRAY, clDisplayMethods,
      FID_Fields|TARRAY,  DisplayFields,
      FID_Size|TLONG,     sizeof(objDisplay),
      FID_Path|TSTR,      MOD_PATH,
      TAGEND));
}
