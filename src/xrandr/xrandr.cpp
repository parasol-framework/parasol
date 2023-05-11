
#define PRV_XRANDR_MODULE

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include <parasol/main.h>
#include <parasol/modules/xrandr.h>

static LONG  xrNotify(XEvent *);
static ERROR xrSetDisplayMode(LONG *, LONG *);
static void  xrSelectInput(Window);
static LONG  xrGetDisplayTotal(void);
static struct xrMode * xrGetDisplayMode(LONG);

JUMPTABLE_CORE
static struct _XDisplay *XDisplay;
static XRRScreenSize glCustomSizes[] = { { 640,480,0,0 }, { 800,600,0,0 }, { 1024,768,0,0 }, { 1280,1024,0,0 } };
static XRRScreenSize *glSizes = glCustomSizes;
static LONG glSizeCount = ARRAYSIZE(glCustomSizes);
static LONG glActualCount = 0;

//********************************************************************************************************************

static void write_string(objFile *File, CSTRING String)
{
   struct acWrite write = { .Buffer = String, .Length = StrLength(String) };
   Action(AC_Write, File, &write);
}

//********************************************************************************************************************

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   pf::Log log("XRandR");
   WORD i;
   XRRScreenSize *sizes;
   XPixmapFormatValues *list;
   LONG errors, count;
   char buffer[512];

   CoreBase = argCoreBase;

   // Do not proceed with initialisation if the module is being probed

   if ((((objModule *)argModule)->Flags & MOF::SYSTEM_PROBE) != MOF::NIL) return ERR_ServiceUnavailable|ERF_Notified;

   if (!acGetVar(argModule, "XDisplay", buffer, sizeof(buffer))) {
      XDisplay = (struct _XDisplay *)(MAXINT)StrToInt(buffer);
      if (!XDisplay) return log.warning(ERR_FieldNotSet);
   }
   else return log.warning(ERR_FieldNotSet);

   LONG screen = DefaultScreen(XDisplay);

   bool avail = false;
   LONG events;
   if (XRRQueryExtension(XDisplay, &events, &errors)) avail = true;
   else log.msg("XRRQueryExtension() failed.");

   if (!avail) return ERR_ServiceUnavailable|ERF_Notified;

   if ((sizes = XRRSizes(XDisplay, screen, &count)) and (count)) {
      glSizes = sizes;
      glSizeCount = count;
   }
   else log.msg("XRRSizes() failed.");

   // Build the screen.xml file if this is the first task to initialise the RandR extension.

   objFile::create file = { fl::Path("user:config/screen.xml"), fl::Flags(FL::NEW|FL::WRITE) };

   if (file.ok()) {
      write_string(*file, "<?xml version=\"1.0\"?>\n\n");
      write_string(*file, "<displayinfo>\n");
      write_string(*file, "  <manufacturer value=\"XFree86\"/>\n");
      write_string(*file, "  <chipset value=\"X11\"/>\n");
      write_string(*file, "  <dac value=\"N/A\"/>\n");
      write_string(*file, "  <clock value=\"N/A\"/>\n");
      write_string(*file, "  <version value=\"1.00\"/>\n");
      write_string(*file, "  <certified value=\"February 2023\"/>\n");
      write_string(*file, "  <monitor_mfr value=\"Unknown\"/>\n");
      write_string(*file, "  <monitor_model value=\"Unknown\"/>\n");
      write_string(*file, "  <scanrates minhscan=\"0\" maxhscan=\"0\" minvscan=\"0\" maxvscan=\"0\"/>\n");
      write_string(*file, "  <gfx_output unknown/>\n");
      write_string(*file, "</displayinfo>\n\n");

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
         if ((glSizes[i].width >= 640) and (glSizes[i].height >= 480)) {
            snprintf(buffer, sizeof(buffer), "<screen name=\"%dx%d\" width=\"%d\" height=\"%d\" depth=\"%d\" colours=\"%d\"\n",
               glSizes[i].width, glSizes[i].height, glSizes[i].width, glSizes[i].height, xbpp, xcolours);
            write_string(*file, buffer);

            snprintf(buffer, sizeof(buffer), "  bytes=\"%d\" defaultrefresh=\"0\" minrefresh=\"0\" maxrefresh=\"0\">\n", xbytes);
            write_string(*file, buffer);

            write_string(*file, "</screen>\n\n");
         }
      }
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
SetDisplayMode: Change the width and height of the display.

This function changes the width and height of the display to that indicated by the Width and Height parameters.  If the requested size does not match a known mode, the closest matching mode will be chosen.

-INPUT-
&int Width: The required width of the display.
&int Height: The required height of the display.

-ERRORS-
Okay

*********************************************************************************************************************/

static ERROR xrSetDisplayMode(LONG *Width, LONG *Height)
{
   pf::Log log(__FUNCTION__);
   LONG count;
   WORD i;

   WORD width = *Width;
   WORD height = *Height;

   XRRScreenSize *sizes;
   if ((sizes = XRRSizes(XDisplay, DefaultScreen(XDisplay), &count)) and (count)) {
      WORD index    = -1;
      LONG bestweight = 0x7fffffff;

      for (i=0; i < count; i++) {
         LONG weight = std::abs(sizes[i].width - width) + std::abs(sizes[i].height - height);
         if (weight < bestweight) {
            index = i;
            bestweight = weight;
         }
      }

      if (index IS -1) {
         log.warning("No support for requested screen mode %dx%d", width, height);
         return ERR_NoSupport;
      }

      XRRScreenConfiguration *scrconfig;
      if ((scrconfig = XRRGetScreenInfo(XDisplay, DefaultRootWindow(XDisplay)))) {
         if (!XRRSetScreenConfig(XDisplay, scrconfig,
             DefaultRootWindow(XDisplay), index, RR_Rotate_0, CurrentTime)) {
            *Width = sizes[index].width;
            *Height = sizes[index].height;

            log.msg("New mode: %dx%d (index %d/%d) from request %dx%d", sizes[index].width, sizes[index].height, index, count, width, height);

            XRRFreeScreenConfigInfo(scrconfig);
            return ERR_Okay;
         }
         else {
            log.warning("SetScreenConfig() failed.");
            XRRFreeScreenConfigInfo(scrconfig);
            return ERR_Failed;
         }
      }
      else return ERR_Failed;
   }
   else {
      log.warning("RandR not initialised.");
      return ERR_Failed;
   }
}

/*********************************************************************************************************************

-FUNCTION-
Notify: Private

This is an internal function for the Display module.

-INPUT-
ptr XEvent: Pointer to an XEvent structure to be processed.

-RESULT-
int: Returns 0 or 1.

*********************************************************************************************************************/

static LONG xrNotify(XEvent *XEvent)
{
   if (XRRUpdateConfiguration(XEvent)) return 1;
   return 0;
}

/*********************************************************************************************************************

-FUNCTION-
SelectInput: Private

This is an internal function for the Display module.

-INPUT-
int Window: The X11 window to target.

*********************************************************************************************************************/

static void xrSelectInput(Window Window)
{
   XRRSelectInput(XDisplay, DefaultRootWindow(XDisplay), RRScreenChangeNotifyMask);
}

/*********************************************************************************************************************

-FUNCTION-
GetDisplayTotal: Returns the total number of display modes.

This function returns the total number of known display modes.

-RESULT-
int: Returns the total number of known display modes.

*********************************************************************************************************************/

static LONG xrGetDisplayTotal(void)
{
   pf::Log log(__FUNCTION__);

   if (!glActualCount) {
      for (LONG i=0; i < glSizeCount; i++) {
         if ((glSizes[i].width >= 640) and (glSizes[i].height >= 480)) {
            glActualCount++;
         }
      }
   }

   log.msg("%d Resolutions", glActualCount);
   return glActualCount;
}

/*********************************************************************************************************************

-FUNCTION-
GetDisplayMode: Retrieve information of a display mode.

This function returns the width, height and depth of a given display mode.

-INPUT-
int Index: Index of the display mode to retrieve.

-RESULT-
ptr: An xrMode structure is returned or NULL on failure.

*********************************************************************************************************************/

static struct xrMode * xrGetDisplayMode(LONG Index)
{
   pf::Log log(__FUNCTION__);
   static struct xrMode mode;
   LONG i, j;

   if ((Index < 0) or (Index >= glActualCount)) {
      log.warning("Index %d not within range 0 - %d", Index, glActualCount);
      return NULL;
   }

   ClearMemory(&mode, sizeof(mode));

   j = Index;
   for (i=0; i < glSizeCount; i++) {
      if ((glSizes[i].width >= 640) and (glSizes[i].height >= 480)) {
         if (!j) {
            mode.Width  = glSizes[i].width;
            mode.Height = glSizes[i].height;
            mode.Depth  = DefaultDepth(XDisplay, DefaultScreen(XDisplay));
            log.msg("Mode %d: %dx%d", Index, mode.Width, mode.Height);
            return &mode;
         }
         j--;
      }
   }

   log.warning("Failed to get mode index %d", Index);
   return NULL;
}

//********************************************************************************************************************

#include "module_def.c"

static ERROR CMDOpen(OBJECTPTR Module)
{
   Module->set(FID_FunctionList, glFunctions);
   return ERR_Okay;
}

static ERROR CMDExpunge(void)
{
   return ERR_Okay;
}

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MODVERSION_XRANDR, MOD_IDL, NULL)
extern "C" struct ModHeader * register_xrandr_module() { return &ModHeader; }
