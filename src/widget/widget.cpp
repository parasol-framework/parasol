/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-MODULE-
Widget: The widget module hosts common widget classes such as the Button and CheckBox.
-END-

*****************************************************************************/

#define PRV_WIDGET_MODULE
#include <parasol/modules/display.h>
#include <parasol/modules/surface.h>
#include <parasol/modules/widget.h>

#include "defs.h"

MODULE_COREBASE;

struct DisplayBase *DisplayBase;
struct SurfaceBase *SurfaceBase;

OBJECTPTR modWidget = NULL, modDisplay = NULL, modSurface = NULL;

static objCompression *glIconArchive = NULL;

#include "widget_def.c"

//****************************************************************************

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   if (LoadModule("display", MODVERSION_DISPLAY, &modDisplay, &DisplayBase) != ERR_Okay) return ERR_InitModule;
   if (LoadModule("surface", MODVERSION_SURFACE, &modSurface, &SurfaceBase) != ERR_Okay) return ERR_InitModule;

   if (GetPointer(argModule, FID_Master, &modWidget) != ERR_Okay) {
      return ERR_GetField;
   }

   STRING icon_path;
   if (ResolvePath("iconsource:", 0, &icon_path) != ERR_Okay) { // The client can set iconsource: to redefine the icon origins
      icon_path = StrClone("styles:icons/");
   }

   // Icons are stored in compressed archives, accessible via "archive:icons/<category>/<icon>.svg"

   std::string src(icon_path);
   src.append("Default.zip");
   if (CreateObject(ID_COMPRESSION, NF_INTEGRAL, &glIconArchive,
         FID_Path|TSTR,        src.c_str(),
         FID_ArchiveName|TSTR, "icons",
         FID_Flags|TLONG,      CMF_READ_ONLY,
         TAGEND)) {
      return ERR_CreateObject;
   }

   FreeResource(icon_path);

   // The icons: special volume is a simple reference to the archive path.

   if (SetVolume(AST_NAME, "icons",
      AST_PATH,  "archive:icons/",
      AST_FLAGS, VOLUME_REPLACE|VOLUME_HIDDEN,
      AST_ICON,  "misc/picture",
      TAGEND) != ERR_Okay) return ERR_SetVolume;

   if (init_clipboard() != ERR_Okay) return ERR_AddClass;

   return ERR_Okay;
}

static ERROR CMDOpen(OBJECTPTR Module)
{
   return ERR_Okay;
}

static ERROR CMDExpunge(void)
{
   free_clipboard();

   if (glIconArchive) { acFree(glIconArchive); glIconArchive = NULL; }
   if (modDisplay)    { acFree(modDisplay);    modDisplay = NULL; }
   if (modSurface)    { acFree(modSurface);    modSurface = NULL; }
   return ERR_Okay;
}

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MODVERSION_WIDGET)
