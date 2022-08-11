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
#include <parasol/modules/font.h>
#include <parasol/modules/surface.h>
#include <parasol/modules/widget.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/vector.h>

#include "defs.h"

MODULE_COREBASE;

struct FontBase *FontBase;
struct VectorBase *VectorBase;
struct DisplayBase *DisplayBase;
struct SurfaceBase *SurfaceBase;

OBJECTPTR modFont = NULL, modWidget = NULL, modDisplay = NULL, modSurface = NULL;
OBJECTPTR modVector = NULL;

char glDefaultFace[64] = "Open Sans,Source Sans Pro:100%";
char glWindowFace[64] = "Open Sans,Source Sans Pro:100%";
char glWidgetFace[64] = "Open Sans,Source Sans Pro:100%";
char glLabelFace[64] = "Open Sans,Source Sans Pro:100%";

#define DEFAULT_RATIO  7.0
#define DEFAULT_SIZE   16

static std::string glFilter = "default";
static std::string glTheme = "Default";
static objCompression *glIconArchive = NULL;

LONG glMargin = 10;

#include "widget_def.c"

//****************************************************************************

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   if (LoadModule("display", MODVERSION_DISPLAY, &modDisplay, &DisplayBase) != ERR_Okay) return ERR_InitModule;
   if (LoadModule("font", MODVERSION_FONT, &modFont, &FontBase) != ERR_Okay) return ERR_InitModule;
   if (LoadModule("surface", MODVERSION_SURFACE, &modSurface, &SurfaceBase) != ERR_Okay) return ERR_InitModule;
   if (LoadModule("vector", MODVERSION_VECTOR, &modVector, &VectorBase) != ERR_Okay) return ERR_InitModule;

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

   char fontstyle[] = "[glStyle./fonts/font[@name='default']/@face]:[glStyle./fonts/font[@name='default']/@size]";
   if (!StrEvaluate(fontstyle, sizeof(fontstyle), SEF_STRICT, 0)) {
      StrCopy(fontstyle, glDefaultFace, sizeof(glDefaultFace));
   }

   char widget[] = "[glStyle./fonts/font[@name='widget']/@face]:[glStyle./fonts/font[@name='widget']/@size]";
   if (!StrEvaluate(widget, sizeof(widget), SEF_STRICT, 0)) {
      StrCopy(widget, glWidgetFace, sizeof(glWidgetFace));
   }

   char window[] = "[glStyle./fonts/font[@name='window']/@face]:[glStyle./fonts/font[@name='window']/@size]";
   if (!StrEvaluate(window, sizeof(window), SEF_STRICT, 0)) {
      StrCopy(window, glWindowFace, sizeof(glWindowFace));
   }

   char label[] = "[glStyle./fonts/font[@name='label']/@face]:[glStyle./fonts/font[@name='label']/@size]";
   if (!StrEvaluate(label, sizeof(label), SEF_STRICT, 0)) {
      StrCopy(label, glLabelFace, sizeof(glLabelFace));
   }

   // Get the widget margin, which affects button height.

   OBJECTID style_id;
   objXML *style;

   LONG count = 1;
   if (!FindObject("glStyle", ID_XML, FOF_INCLUDE_SHARED, &style_id, &count)) {
      if (!AccessObject(style_id, 500, &style)) {
         WORD i;
         char buffer[100];
         if (!acGetVar(style, "/interface/@widgetmargin", buffer, sizeof(buffer))) {
            // If the margin is expressed as 'px' then it's fixed.  Otherwise scale it according to the display DPI.
            glMargin = StrToInt(buffer);
            for (i=0; (buffer[i]) and (buffer[i] >= '0') and (buffer[i] <= '9'); i++);
            if ((buffer[i] IS 'p') and (buffer[i+1] IS 'x'));
            else {
               // Scale the value to match DPI.
               glMargin = F2I(gfxScaleToDPI(glMargin));
            }

            if (glMargin < 3) glMargin = 3;
            else if (glMargin > 60) glMargin = 60;
         }
         ReleaseObject(style);
      }
   }

   if (init_clipboard() != ERR_Okay) return ERR_AddClass;
   if (init_resize() != ERR_Okay) return ERR_AddClass;
   if (init_tabfocus() != ERR_Okay) return ERR_AddClass;

   return ERR_Okay;
}

static ERROR CMDOpen(OBJECTPTR Module)
{
   return ERR_Okay;
}

static ERROR CMDExpunge(void)
{
   free_resize();
   free_tabfocus();
   free_clipboard();

   if (glIconArchive) { acFree(glIconArchive); glIconArchive = NULL; }
   if (modDisplay)    { acFree(modDisplay);    modDisplay = NULL; }
   if (modFont)       { acFree(modFont);       modFont = NULL; }
   if (modSurface)    { acFree(modSurface);    modSurface = NULL; }
   if (modVector)     { acFree(modVector);     modVector = NULL; }
   return ERR_Okay;
}

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MODVERSION_WIDGET)
