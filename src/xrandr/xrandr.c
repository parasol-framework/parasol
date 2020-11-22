
#define PRV_XRANDR_MODULE

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include <parasol/main.h>
#include <parasol/modules/xrandr.h>

static LONG  xrNotify(XEvent *);
static ERROR xrSetDisplayMode(LONG *, LONG *);
static void  xrSelectInput(Window);
static LONG xrGetDisplayTotal(void);
static struct xrMode * xrGetDisplayMode(LONG);

const struct Function glFunctions[];

MODULE_COREBASE;
static struct X11Globals *glX11 = 0;
static struct _XDisplay *XDisplay;
static XRRScreenSize glCustomSizes[] = { { 640,480,0,0 }, { 800,600,0,0 }, { 1024,768,0,0 }, { 1280,1024,0,0 } };
static XRRScreenSize *glSizes = glCustomSizes;
static LONG glSizeCount = ARRAYSIZE(glCustomSizes);
static LONG glActualCount = 0;

//****************************************************************************

static void write_string(objFile *File, CSTRING String)
{
   struct acWrite write = { .Buffer = String, .Length = StrLength(String) };
   Action(AC_Write, File, &write);
}

//****************************************************************************

ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   WORD i;
   XRRScreenSize *sizes;
   XPixmapFormatValues *list;
   LONG errors, count;
   UBYTE buffer[512];

   CoreBase = argCoreBase;

   // Do not proceed with initialisation if the module is being probed

   if (((objModule *)argModule)->Flags & MOF_SYSTEM_PROBE) return ERR_ServiceUnavailable|ERF_Notified;

   if (!acGetVar(argModule, "XDisplay", buffer, sizeof(buffer))) {
      XDisplay = (struct _XDisplay *)(MAXINT)StrToInt(buffer);
      if (!XDisplay) return PostError(ERR_FieldNotSet);
   }
   else return PostError(ERR_FieldNotSet);

   // Access the global x11 variables

   if (AccessMemory(RPM_X11, MEM_READ_WRITE, 1000, &glX11) != ERR_Okay) {
      return LogError(ERH_InitModule, ERR_AccessMemory);
   }

   glX11->RRInitialised++;

   LONG screen = DefaultScreen(XDisplay);

   BYTE avail = FALSE;
   if (glX11->Manager) {
      LONG events;
      if (XRRQueryExtension(XDisplay, &events, &errors)) {
         avail = TRUE;
      }
      else LogF("XRandR","XRRQueryExtension() failed.");
   }
   else LogF("XRandR","X11 display ownership is not enabled.");

   if (avail) {
      if ((sizes = XRRSizes(XDisplay, screen, &count)) AND (count)) {
         glSizes = sizes;
         glSizeCount = count;
      }
      else LogF("XRandR","XRRSizes() failed.");
   }

   if ((avail) AND (glX11->RRInitialised <= 1)) {
      // Build the screen.xml file if this is the first task to initialise the RandR extension.

      objFile *file;
      if (!CreateObject(ID_FILE, 0, &file, NULL,
            FID_Path|TSTR,   "user:config/screen.xml",
            FID_Flags|TLONG, FL_NEW|FL_WRITE,
            TAGEND)) {

         write_string(file, "<?xml version=\"1.0\"?>\n\n");
         write_string(file, "<displayinfo>\n");
         write_string(file, "  <manufacturer value=\"XFree86\"/>\n");
         write_string(file, "  <chipset value=\"X11\"/>\n");
         write_string(file, "  <dac value=\"N/A\"/>\n");
         write_string(file, "  <clock value=\"N/A\"/>\n");
         write_string(file, "  <version value=\"1.00\"/>\n");
         write_string(file, "  <certified value=\"February 2007\"/>\n");
         write_string(file, "  <monitor_mfr value=\"Unknown\"/>\n");
         write_string(file, "  <monitor_model value=\"Unknown\"/>\n");
         write_string(file, "  <scanrates minhscan=\"0\" maxhscan=\"0\" minvscan=\"0\" maxvscan=\"0\"/>\n");
         write_string(file, "  <gfx_output unknown/>\n");
         write_string(file, "</displayinfo>\n\n");

         WORD xbpp = DefaultDepth(XDisplay, screen);

         WORD xbytes;
         if (xbpp <= 8) xbytes = 1;
         else if (xbpp <= 16) xbytes = 2;
         else if (xbpp <= 24) xbytes = 3;
         else xbytes = 4;

         if ((list = XListPixmapFormats(XDisplay, &count))) {
            for (i=0; i < count; i++) {
               if (list[i].depth IS xbpp) {
                  xbytes = list[i].bits_per_pixel;
                  if (list[i].bits_per_pixel <= 8) xbytes = 1;
                  else if (list[i].bits_per_pixel <= 16) xbytes = 2;
                  else if (list[i].bits_per_pixel <= 24) xbytes = 3;
                  else xbytes = 4;
               }
            }
         }

         if (xbytes IS 4) xbpp = 32;

         LONG xcolours;
         switch(xbpp) {
            case 1:  xcolours = 2; break;
            case 8:  xcolours = 256; break;
            case 15: xcolours = 32768; break;
            case 16: xcolours = 65536; break;
            default: xcolours = 16777216; break;
         }

         for (i=0; i < glSizeCount; i++) {
            if ((glSizes[i].width >= 640) AND (glSizes[i].height >= 480)) {
               StrFormat(buffer, sizeof(buffer), "<screen name=\"%dx%d\" width=\"%d\" height=\"%d\" depth=\"%d\" colours=\"%d\"\n",
                  glSizes[i].width, glSizes[i].height, glSizes[i].width, glSizes[i].height, xbpp, xcolours);
               write_string(file, buffer);

               StrFormat(buffer, sizeof(buffer), "  bytes=\"%d\" defaultrefresh=\"0\" minrefresh=\"0\" maxrefresh=\"0\">\n", xbytes);
               write_string(file, buffer);

               write_string(file, "</screen>\n\n");
            }
         }

         acFree(file);
      }
   }

   if (avail) return ERR_Okay;
   else return ERR_ServiceUnavailable|ERF_Notified;
}

ERROR CMDOpen(OBJECTPTR Module)
{
   SetPointer(Module, FID_FunctionList, glFunctions);
   return ERR_Okay;
}

static ERROR CMDExpunge(void)
{
   if (glX11) { ReleaseMemory(glX11); glX11 = NULL; }
   return ERR_Okay;
}


/******************************************************************************

-FUNCTION-
SetDisplayMode: Change the width and height of the display.

This function changes the width and height of the display to that indicated by the Width and Height parameters.  If the requested size does not match a known mode, the closest matching mode will be chosen.

-INPUT-
&int Width: The required width of the display.
&int Height: The required height of the display.

-ERRORS-
Okay

******************************************************************************/

static ERROR xrSetDisplayMode(LONG *Width, LONG *Height)
{
   LONG count;
   WORD i;

   WORD width = *Width;
   WORD height = *Height;

   XRRScreenSize *sizes;
   if ((sizes = XRRSizes(XDisplay, DefaultScreen(XDisplay), &count)) AND (count)) {
      WORD index    = -1;
      LONG bestweight = 0x7fffffff;

      for (i=0; i < count; i++) {
         LONG weight = ABS(sizes[i].width - width) + ABS(sizes[i].height - height);
         if (weight < bestweight) {
            index = i;
            bestweight = weight;
         }
      }

      if (index IS -1) {
         LogF("@SetX11DisplayMode:","No support for requested screen mode %dx%d", width, height);
         LogReturn();
         return ERR_NoSupport;
      }

      XRRScreenConfiguration *scrconfig;
      if ((scrconfig = XRRGetScreenInfo(XDisplay, DefaultRootWindow(XDisplay)))) {
         if (!XRRSetScreenConfig(XDisplay, scrconfig,
             DefaultRootWindow(XDisplay), index, RR_Rotate_0, CurrentTime)) {
            *Width = sizes[index].width;
            *Height = sizes[index].height;

            LogF("SetX11DisplayMode:","New mode: %dx%d (index %d/%d) from request %dx%d", sizes[index].width, sizes[index].height, index, count, width, height);

            XRRFreeScreenConfigInfo(scrconfig);
            return ERR_Okay;
         }
         else {
            LogF("@SetX11DisplayMode:","SetScreenConfig() failed.");
            XRRFreeScreenConfigInfo(scrconfig);
            return ERR_Failed;
         }
      }
      else return ERR_Failed;
   }
   else {
      LogF("@SetX11DisplayMode:","RandR not initialised.");
      return ERR_Failed;
   }
}

/******************************************************************************

-FUNCTION-
Notify: Private

This is an internal function for the Display module.

-INPUT-
ptr XEvent: Pointer to an XEvent structure to be processed.

-RESULT-
int: Returns 0 or 1.

******************************************************************************/

static LONG xrNotify(XEvent *XEvent)
{
   if (XRRUpdateConfiguration(XEvent)) return 1;
   return 0;
}

/******************************************************************************

-FUNCTION-
SelectInput: Private

This is an internal function for the Display module.

-INPUT-
int Window: The X11 window to target.

******************************************************************************/

static void xrSelectInput(Window Window)
{
   XRRSelectInput(XDisplay, DefaultRootWindow(XDisplay), RRScreenChangeNotifyMask);
}

/******************************************************************************

-FUNCTION-
GetDisplayTotal: Returns the total number of display modes.

This function returns the total number of known display modes.

-RESULT-
int: Returns the total number of known display modes.

******************************************************************************/

static LONG xrGetDisplayTotal(void)
{
   LONG i;

   if (!glActualCount) {
      for (i=0; i < glSizeCount; i++) {
         if ((glSizes[i].width >= 640) AND (glSizes[i].height >= 480)) {
            glActualCount++;
         }
      }
   }

   LogF("xrGetDisplayTotal()","%d Resolutions", glActualCount);
   return glActualCount;
}

/******************************************************************************

-FUNCTION-
GetDisplayMode: Retrieve information of a display mode.

This function returns the width, height and depth of a given display mode.

-INPUT-
int Index: Index of the display mode to retrieve.

-RESULT-
ptr: An xrMode structure is returned or NULL on failure.

******************************************************************************/

static struct xrMode * xrGetDisplayMode(LONG Index)
{
   static struct xrMode mode;
   LONG i, j;

   if ((Index < 0) OR (Index >= glActualCount)) {
      LogF("@xrGetDisplayMode:","Index %d not within range 0 - %d", Index, glActualCount);
      return NULL;
   }

   ClearMemory(&mode, sizeof(mode));

   j = Index;
   for (i=0; i < glSizeCount; i++) {
      if ((glSizes[i].width >= 640) AND (glSizes[i].height >= 480)) {
         if (!j) {
            mode.Width  = glSizes[i].width;
            mode.Height = glSizes[i].height;
            mode.Depth  = DefaultDepth(XDisplay, DefaultScreen(XDisplay));
            LogF("xrGetDisplayMode:","Mode %d: %dx%d", Index, mode.Width, mode.Height);
            return &mode;
         }
         j--;
      }
   }

   LogF("@xrGetDisplayMode:","Failed to get mode index %d", Index);
   return NULL;
}

#include "module_def.c"

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MODVERSION_XRANDR)
