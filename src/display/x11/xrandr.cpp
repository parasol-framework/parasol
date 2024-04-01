
#ifdef XRANDR_ENABLED

//********************************************************************************************************************

ERR xrSetDisplayMode(LONG *Width, LONG *Height)
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
         return ERR::NoSupport;
      }

      XRRScreenConfiguration *scrconfig;
      if ((scrconfig = XRRGetScreenInfo(XDisplay, DefaultRootWindow(XDisplay)))) {
         if (!XRRSetScreenConfig(XDisplay, scrconfig,
             DefaultRootWindow(XDisplay), index, RR_Rotate_0, CurrentTime)) {
            *Width = sizes[index].width;
            *Height = sizes[index].height;

            log.msg("New mode: %dx%d (index %d/%d) from request %dx%d", sizes[index].width, sizes[index].height, index, count, width, height);

            XRRFreeScreenConfigInfo(scrconfig);
            return ERR::Okay;
         }
         else {
            log.warning("SetScreenConfig() failed.");
            XRRFreeScreenConfigInfo(scrconfig);
            return ERR::Failed;
         }
      }
      else return ERR::Failed;
   }
   else {
      log.warning("RandR not initialised.");
      return ERR::Failed;
   }
}

//********************************************************************************************************************

LONG xrNotify(XEvent *XEvent)
{
   if (XRRUpdateConfiguration(XEvent)) return 1;
   return 0;
}

//********************************************************************************************************************

void xrSelectInput(Window Window)
{
   XRRSelectInput(XDisplay, DefaultRootWindow(XDisplay), RRScreenChangeNotifyMask);
}

//********************************************************************************************************************
// This function returns the total number of known display modes.

LONG xrGetDisplayTotal(void)
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

//********************************************************************************************************************
// This function returns the width, height and depth of a given display mode.

struct xrMode * xrGetDisplayMode(LONG Index)
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

#endif // XRANDR_ENABLED
