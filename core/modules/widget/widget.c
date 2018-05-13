/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-MODULE-
Widget: The widget module hosts common widget classes such as the Button, Scrollbar and CheckBox.
-END-

*****************************************************************************/

#include <parasol/modules/display.h>
#include <parasol/modules/font.h>
#include <parasol/modules/surface.h>
#include <parasol/modules/iconserver.h>
#include <parasol/modules/widget.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/vector.h>

#include "defs.h"
#include "widget_def.c"

MODULE_COREBASE;

struct FontBase *FontBase;
struct VectorBase *VectorBase;
struct DisplayBase *DisplayBase;
struct SurfaceBase *SurfaceBase;
struct IconServerBase *IconServerBase;

OBJECTPTR modFont = NULL, modWidget = NULL, modDisplay = NULL, modSurface = NULL;
OBJECTPTR modIconServer = NULL, modVector = NULL;

char glDefaultFace[64] = "Open Sans,Source Sans Pro:100%";
char glWindowFace[64] = "Open Sans,Source Sans Pro:100%";
char glWidgetFace[64] = "Open Sans,Source Sans Pro:100%";
char glLabelFace[64] = "Open Sans,Source Sans Pro:100%";

LONG glMargin = 10;

//****************************************************************************

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   if (LoadModule("display", MODVERSION_DISPLAY, &modDisplay, &DisplayBase) != ERR_Okay) return ERR_InitModule;
   if (LoadModule("font", MODVERSION_FONT, &modFont, &FontBase) != ERR_Okay) return ERR_InitModule;
   if (LoadModule("surface", MODVERSION_SURFACE, &modSurface, &SurfaceBase) != ERR_Okay) return ERR_InitModule;
   if (LoadModule("iconserver", MODVERSION_ICONSERVER, &modIconServer, &IconServerBase) != ERR_Okay) return ERR_InitModule;
   if (LoadModule("vector", MODVERSION_VECTOR, &modVector, &VectorBase) != ERR_Okay) return ERR_InitModule;

   if (GetPointer(argModule, FID_Master, &modWidget) != ERR_Okay) {
      return ERR_GetField;
   }

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

   if (!FastFindObject("glStyle", ID_XML, &style_id, 1, NULL)) {
      if (!AccessObject(style_id, 500, &style)) {
         WORD i;
         char buffer[100];
         if (!acGetVar(style, "/interface/@widgetmargin", buffer, sizeof(buffer))) {
            // If the margin is expressed as 'px' then it's fixed.  Otherwise scale it according to the display DPI.
            glMargin = StrToInt(buffer);
            for (i=0; (buffer[i]) AND (buffer[i] >= '0') AND (buffer[i] <= '9'); i++);
            if ((buffer[i] IS 'p') AND (buffer[i+1] IS 'x'));
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
   if (init_button() != ERR_Okay) return ERR_AddClass;
   if (init_checkbox() != ERR_Okay) return ERR_AddClass;
   if (init_dialog() != ERR_Okay) return ERR_AddClass;
   if (init_resize() != ERR_Okay) return ERR_AddClass;
   if (init_scrollbar() != ERR_Okay) return ERR_AddClass;
   if (init_combobox() != ERR_Okay) return ERR_AddClass;
   if (init_tabfocus() != ERR_Okay) return ERR_AddClass;
   if (init_input() != ERR_Okay) return ERR_AddClass;
   if (init_scroll() != ERR_Okay) return ERR_AddClass;
   if (init_image() != ERR_Okay) return ERR_AddClass;
   if (init_text() != ERR_Okay) return ERR_AddClass;
   if (init_menu() != ERR_Okay) return ERR_AddClass;
   if (init_menuitem() != ERR_Okay) return ERR_AddClass;
   if (init_view() != ERR_Okay) return ERR_AddClass;
   if (init_fileview() != ERR_Okay) return ERR_AddClass;

   return ERR_Okay;
}

static ERROR CMDExpunge(void)
{
   free_button();
   free_checkbox();
   free_resize();
   free_scrollbar();
   free_combobox();
   free_tabfocus();
   free_input();
   free_scroll();
   free_image();
   free_text();
   free_menu();
   free_menuitem();
   free_dialog();
   free_view();
   free_clipboard();
   free_fileview();

   if (modIconServer) { acFree(modIconServer); modIconServer = NULL; }
   if (modDisplay)    { acFree(modDisplay);    modDisplay = NULL; }
   if (modFont)       { acFree(modFont);       modFont = NULL; }
   if (modSurface)    { acFree(modSurface);    modSurface = NULL; }
   if (modVector)     { acFree(modVector);     modVector = NULL; }
   return ERR_Okay;
}

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, NULL, CMDExpunge, 1.0)
