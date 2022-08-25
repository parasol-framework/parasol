
#include "defs.h"

/*****************************************************************************

-FUNCTION-
ApplyStyleGraphics: Applies pre-defined graphics to a GUI object.

This is an internal function created for use by classes in the GUI category.  It finds the style definition for the
target Object and executes the procedure with the Surface as the graphics target.

-INPUT-
obj Object:  The object that requires styling.
oid Target: The surface or vector that will receive the style graphics.
cstr StyleName: Optional.  Reference to a style that is alternative to the default.
cstr StyleType: Optional.  Name of the type of style decoration to be applied.  Use in conjunction with StyleName.

-ERRORS-
Okay:
NullArgs:
BadState: The Object is not initialised.
NothingDone: No style information is defined for the object's class.
-END-

*****************************************************************************/

ERROR gfxApplyStyleGraphics(OBJECTPTR Object, OBJECTID TargetID, CSTRING StyleName, CSTRING StyleType)
{
   parasol::Log log(__FUNCTION__);

   if ((!Object) or (!TargetID)) return log.warning(ERR_NullArgs);

   log.branch("Object: %d, Target: %d, Style: %s, StyleType: %s", Object->UID, TargetID, StyleName, StyleType);

   ERROR error;
   if ((error = load_styles())) return error;

   // Try the app's style preference first.
/*
   OBJECTPTR script = glAppStyle;
   if (glAppStyle) {
      if (!xmlFindTag(xml, xpath, NULL, NULL)) {
         SetString(script, FID_Procedure, xpath);
         SetLong(script, FID_Target, TargetID);
         if (!acActivate(script)) return ERR_Okay;
      }
   }
*/
   // Now try the desktop preference.

   if (glDesktopStyleScript) {
      const ScriptArg args[] = {
         { "Class",     FDF_STRING,   { .Address = StyleName ? (APTR)StyleName : (APTR)Object->Class->ClassName } },
         { "Object",    FDF_OBJECT,   { .Address = Object } },
         { "Target",    FDF_OBJECTID, { .Long = TargetID } },
         { "StyleType", FDF_STRING,   { .Address = (APTR)StyleType } }
      };

      struct scExec exec = {
         .Procedure = "applyDecoration",
         .Args      = args,
         .TotalArgs = ARRAYSIZE(args)
      };

      Action(MT_ScExec, glDesktopStyleScript, &exec);
      GetLong(glDesktopStyleScript, FID_Error, &error);
      if (!error) return ERR_Okay;
   }

   // Still no luck.  Try the default.

   if (glDefaultStyleScript) {
      const ScriptArg args[] = {
         { "Class",   FDF_STRING,   { .Address = StyleName ? (APTR)StyleName : (APTR)Object->Class->ClassName } },
         { "Object",  FDF_OBJECT,   { .Address = Object } },
         { "Target",  FDF_OBJECTID, { .Long = TargetID } },
         { "StyleType", FDF_STRING, { .Address = (APTR)StyleType } }
      };

      struct scExec exec = {
         .Procedure = "applyDecoration",
         .Args      = args,
         .TotalArgs = ARRAYSIZE(args)
      };

      Action(MT_ScExec, glDefaultStyleScript, &exec);
      GetLong(glDefaultStyleScript, FID_Error, &error);
      if (!error) return ERR_Okay;
   }

   return ERR_NothingDone;
}

/*****************************************************************************

-FUNCTION-
ApplyStyleValues: Applies default values to a GUI object before initialisation.

The ApplyStyleValues() function is reserved for the use of GUI classes that need to pre-initialise their objects with
default values.

Styles are defined in the order of the application's preference, the desktop preference, and then the default if no
preference has been specified or a failure occurred.

An application can define its preferred style by calling ~SetCurrentStyle() with the path of the XML style
file.  This function can be called at any time, allowing the style to be changed on the fly.

A desktop can set its preferred style by storing style information at `environment:config/style.xml`.

-INPUT-
obj Object: The object that will receive the default values.
cstr Name:  Optional.  Reference to an alternative style to be applied.

-ERRORS-
Okay: Values have been preset successfully.

*****************************************************************************/

ERROR gfxApplyStyleValues(OBJECTPTR Object, CSTRING StyleName)
{
   parasol::Log log(__FUNCTION__);

   if (!Object) return log.warning(ERR_NullArgs);

   log.branch("#%d, Style: %s", Object->UID, StyleName);

   ERROR error;
   if ((error = load_styles())) return error;
   if (Object->Flags & NF_INITIALISED) return log.warning(ERR_BadState);
   if (glDefaultStyleScript) apply_style(Object, glDefaultStyleScript, StyleName);

   if (glAppStyle) {
      //if (!apply_style(Object, glAppStyle, StyleName)) return ERR_Okay;
   }

   if (glDesktopStyleScript) apply_style(Object, glDesktopStyleScript, StyleName);

   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
GetDisplayInfo: Retrieves display information.

The GetDisplayInfo() function returns information about a display, which includes information such as its size and bit
depth.  If the system is running on a hosted display (e.g. Windows or X11) then GetDisplayInfo() can also be used to
retrieve information about the default monitor by using a Display of zero.

The resulting `DISPLAYINFO` structure values remain good until the next call to this function, at which point they will
be overwritten.

-INPUT-
oid Display: Object ID of the display to be analysed.
&struct(*DisplayInfo) Info: This reference will receive a pointer to a DISPLAYINFO structure.

-ERRORS-
Okay:
NullArgs:
AllocMemory:

*****************************************************************************/

ERROR gfxGetDisplayInfo(OBJECTID DisplayID, DISPLAYINFO **Result)
{
   static THREADVAR DISPLAYINFO *t_info = NULL;

   if (!Result) return ERR_NullArgs;

   if (!t_info) {
      // Each thread gets an allocation that can't be resource tracked, so MEM_HIDDEN is used in this case.
      // Note that this could conceivably cause memory leaks if temporary threads were to use this function.
      if (AllocMemory(sizeof(DISPLAYINFO), MEM_NO_CLEAR|MEM_HIDDEN, &t_info, NULL)) {
         return ERR_AllocMemory;
      }
   }

   ERROR error;
   if (!(error = get_display_info(DisplayID, t_info, sizeof(DISPLAYINFO)))) {
      *Result = t_info;
      return ERR_Okay;
   }
   else return error;
}

/*****************************************************************************

-FUNCTION-
GetDisplayType: Returns the type of display supported.

This function returns the type of display supported by the loaded Display module.  Current return values are:

<types lookup="DT"/>

-RESULT-
int(DT): Returns an integer indicating the display type.

*****************************************************************************/

LONG gfxGetDisplayType(void)
{
#ifdef _WIN32
   return DT_WINDOWS;
#elif __xwindows__
   return DT_X11;
#elif _GLES_
   return DT_GLES;
#else
   return DT_NATIVE;
#endif
}

/*****************************************************************************

-FUNCTION-
ScanDisplayModes: Private. Returns formatted resolution information from the display database.

For internal use only.

<pre>
DISPLAYINFO info;
ClearMemory(&info, sizeof(info));
while (!scrScanDisplayModes("depth=32", &info, sizeof(info))) {
   ...
}
</pre>

-INPUT-
cstr Filter: The filter to apply to the resolution database.  May be NULL for no filter.
struct(*DisplayInfo) Info: A pointer to a screenINFO structure must be referenced here.  The structure will be filled with information when the function returns.
structsize Size: Size of the screenINFO structure.

-ERRORS-
Okay: The resolution information was retrieved.
Args:
NoSupport: Native graphics system not available (e.g. hosted on Windows or X11).
Search: There are no more display modes to return that are a match for the Filter.

*****************************************************************************/

ERROR gfxScanDisplayModes(CSTRING Filter, DISPLAYINFO *Info, LONG Size)
{
#ifdef __snap__

   GA_modeInfo modeinfo;
   UWORD *modes;
   LONG colours, bytes, i, j, minrefresh, maxrefresh, refresh, display;
   WORD f_depth, c_depth; // f = filter, c = condition (0 = equal; -1 <, -2 <=; +1 >, +2 >=)
   WORD f_bytes, c_bytes;
   WORD f_width, c_width, f_height, c_height;
   WORD f_refresh, c_refresh;
   WORD f_minrefresh, c_minrefresh;
   WORD f_maxrefresh, c_maxrefresh;
   BYTE interlace, matched;

   if ((!Info) or (Size < sizeof(DisplayInfoV3))) return ERR_Args;

   // Reset all filters

   f_depth   = c_depth   = 0;
   f_bytes   = c_bytes   = 0;
   f_width   = c_width   = 0;
   f_height  = c_height  = 0;
   f_refresh = c_refresh = 0;
   f_minrefresh = c_minrefresh = 0;
   f_maxrefresh = c_maxrefresh = 0;

   if (Filter) {
      while (*Filter) {
         while ((*Filter) and (*Filter <= 0x20)) Filter++;
         while (*Filter IS ',') Filter++;
         while ((*Filter) and (*Filter <= 0x20)) Filter++;

         if (!StrCompare("depth", Filter, 5, 0))   extract_value(Filter, &f_depth, &c_depth);
         if (!StrCompare("bytes", Filter, 5, 0))   extract_value(Filter, &f_bytes, &c_bytes);
         if (!StrCompare("width", Filter, 5, 0))   extract_value(Filter, &f_width, &c_width);
         if (!StrCompare("height", Filter, 6, 0))  extract_value(Filter, &f_height, &c_height);
         if (!StrCompare("refresh", Filter, 7, 0)) extract_value(Filter, &f_refresh, &c_refresh);
         if (!StrCompare("minrefresh", Filter, 10, 0)) extract_value(Filter, &f_minrefresh, &c_minrefresh);
         if (!StrCompare("maxrefresh", Filter, 10, 0)) extract_value(Filter, &f_maxrefresh, &c_maxrefresh);

         while ((*Filter) and (*Filter != ',')) Filter++;
      }
   }

   modes = glSNAPDevice->AvailableModes;
   display = glSNAP->Init.GetDisplayOutput() & gaOUTPUT_SELECTMASK;
   for (i=Info->Index; modes[i] != 0xffff; i++) {
      modeinfo.dwSize = sizeof(modeinfo);
      if (glSNAP->Init.GetVideoModeInfoExt(modes[i], &modeinfo, display, glSNAP->Init.GetActiveHead())) continue;

      if (modeinfo.AttributesExt & gaIsPanningMode) continue;
      if (modeinfo.Attributes & gaIsTextMode) continue;
      if (modeinfo.BitsPerPixel < 8) continue;

      if (modeinfo.BitsPerPixel <= 8) bytes = 1;
      else if (modeinfo.BitsPerPixel <= 16) bytes = 2;
      else if (modeinfo.BitsPerPixel <= 24) bytes = 3;
      else bytes = 4;

      if (modeinfo.BitsPerPixel <= 24) colours = 1<<modeinfo.BitsPerPixel;
      else colours = 1<<24;

      minrefresh = 0x7fffffff;
      maxrefresh = 0;
      for (j=0; modeinfo.RefreshRateList[j] != -1; j++) {
         if ((refresh = modeinfo.RefreshRateList[j]) < 0) refresh = -refresh;
         if (refresh > maxrefresh) maxrefresh = refresh;
         if (refresh < minrefresh) minrefresh = refresh;
      }

      if (minrefresh IS 0x7fffffff) minrefresh = 0;

      refresh = modeinfo.DefaultRefreshRate;
      if (refresh < 0) {
         refresh = -refresh;
         interlace = TRUE;
      }
      else interlace = TRUE;

      // Check if this mode meets the filters, if so then reduce the index

      if (Filter) {
         matched = TRUE;
         if ((f_depth)   and (!compare_values(f_depth,   c_depth,   modeinfo.BitsPerPixel))) matched = FALSE;
         if ((f_bytes)   and (!compare_values(f_bytes,   c_bytes,   bytes))) matched = FALSE;
         if ((f_width)   and (!compare_values(f_width,   c_width,   modeinfo.XResolution)))  matched = FALSE;
         if ((f_height)  and (!compare_values(f_height,  c_height,  modeinfo.YResolution)))  matched = FALSE;
         if ((f_refresh) and (!compare_values(f_refresh, c_refresh, modeinfo.BitsPerPixel))) matched = FALSE;
         if ((f_minrefresh) and (!compare_values(f_minrefresh, c_minrefresh, minrefresh)))   matched = FALSE;
         if ((f_maxrefresh) and (!compare_values(f_maxrefresh, c_maxrefresh, maxrefresh)))   matched = FALSE;

         if (matched IS FALSE) continue;
      }

      // Return information for this mode

      Info->Width         = modeinfo.XResolution;
      Info->Height        = modeinfo.YResolution;
      Info->Depth         = modeinfo.BitsPerPixel;
      Info->BytesPerPixel = bytes;
      Info->AmtColours    = colours;
      Info->MinRefresh    = minrefresh;
      Info->MaxRefresh    = maxrefresh;
      Info->RefreshRate   = refresh;
      Info->Index         = i + 1;
      return ERR_Okay;
   }

   return ERR_Search;

#else

   return ERR_NoSupport;

#endif
}

/*****************************************************************************

-FUNCTION-
SetCurrentStyle: Sets the current style script for the application.

This function changes the current style script for the application.  A path to the location of the script is required.

The script does not need to provide definitions for all GUI components.  Any component not represented in the script
will receive the default style settings.

The style definition does not affect default style values (i.e. fonts, colours and interface).  Style values can be set
by accessing the glStyle XML object directly and updating the values (do this as early as possible in the startup
process).

-INPUT-
cstr Path: Location of the style script.

-ERRORS-
Okay:
NullArgs:
EmptyString: The Path string is empty.
CreateObject: Failed to load the script.

*****************************************************************************/

ERROR gfxSetCurrentStyle(CSTRING Path)
{
   parasol::Log log(__FUNCTION__);

   if (!Path) return log.warning(ERR_NullArgs);
   if (!Path[0]) return log.warning(ERR_EmptyString);

   if (glAppStyle) { acFree(glAppStyle); glAppStyle = NULL; }

   parasol::SwitchContext context(glModule);
   ERROR error = CreateObject(ID_SCRIPT, 0, &glAppStyle, FID_Path|TSTR, Path, TAGEND);
   if (error) return ERR_CreateObject;
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
SetHostOption: Alter options associated with the host display system.

For internal usage only.

-INPUT-
int(HOST) Option: One of HOST_TRAY_ICON, HOST_TASKBAR or HOST_STICK_TO_FRONT.
large Value: The value to be applied to the option.

-ERRORS-
Okay

*****************************************************************************/

ERROR gfxSetHostOption(LONG Option, LARGE Value)
{
#if defined(_WIN32) || defined(__xwindows__)
   parasol::Log log(__FUNCTION__);

   switch (Option) {
      case HOST_TRAY_ICON:
         glTrayIcon += Value;
         if (glTrayIcon) glTaskBar = 0;
         break;

      case HOST_TASKBAR:
         glTaskBar = Value;
         if (glTaskBar) glTrayIcon = 0;
         break;

      case HOST_STICK_TO_FRONT:
         glStickToFront += Value;
         break;

      default:
         log.warning("Invalid option %d, Data " PF64(), Option, Value);
   }
#endif

   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
ScaleToDPI: Scales a value to the active display's DPI.

ScaleToDPI() is a convenience function for scaling any value to the active display's current DPI setting.  The value
that you provide must be fixed in relation to the system wide default of 96 DPI.  If the display's DPI varies differs
to that, your value will be scaled to match.  For instance, an 8 point font at 96 DPI would be scaled to 20 points if
the display was 240 DPI.

If the DPI of the display is unknown, your value will be returned unscaled.

-INPUT-
double Value: The number to be scaled.

-RESULT-
double: The scaled value is returned.
-END-

*****************************************************************************/

DOUBLE gfxScaleToDPI(DOUBLE Value)
{
   if ((!glDisplayInfo->HDensity) or (!glDisplayInfo->VDensity)) return Value;
   else return 96.0 / (((DOUBLE)glDisplayInfo->HDensity + (DOUBLE)glDisplayInfo->VDensity) * 0.5) * Value;
}
