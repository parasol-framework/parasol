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
OBJECTPTR modIconServer = NULL, modVector = NULL;

char glDefaultFace[64] = "Open Sans,Source Sans Pro:100%";
char glWindowFace[64] = "Open Sans,Source Sans Pro:100%";
char glWidgetFace[64] = "Open Sans,Source Sans Pro:100%";
char glLabelFace[64] = "Open Sans,Source Sans Pro:100%";

#define DEFAULT_RATIO  7.0
#define MIN_ICON_SIZE  4
#define DEFAULT_SIZE   16
#define MAX_ICON_SIZE  1024

static std::string glFilter = "default";
static std::string glTheme = "Default";
static STRING glIconPath = NULL;
static OBJECTPTR glIconStyle = NULL;
static objCompression *glIconArchive = NULL;

LONG glMargin = 10;

static ERROR extract_icon(LONG, std::string &, std::string &, std::string &, std::string &, std::string &, LONG *);
static void apply_filter(objBitmap *, CSTRING, std::string &, std::string &, CSTRING);

#include "widget_def.c"

/*****************************************************************************
** Extracts icon name, category and size from a path string.
**
** Valid combinations:
**
**    category/name
**    category/name(11)
**    category/name(11)+ovcategory/ovname
**    category/name(11)+ovcategory/ovname(22) - The last size is the one that counts
**    category/name+ovcategory/name
**    category/name+ovcategory/name(11)
*/

static ERROR extract_icon(LONG PixelSize, std::string &Path, std::string &Category, std::string &Icon, std::string &OvCategory, std::string &OvIcon, LONG *Size)
{
   parasol::Log log(__FUNCTION__);

   log.traceBranch("%s", Path.c_str());

   auto i = Path.find(':'); // Ignore anything like an "icons:" prefix
   if (i != std::string::npos) i++;
   else i = 0;

   // Extract the category (if defined)

   auto j = Path.find_first_of("/\\", i);
   if (j != std::string::npos) {
      Category.assign(Path, i, j-i);
      i = j + 1;
   }

   // Extract the icon name (required)

   j = Path.find_first_of("(+", i);
   Icon.assign(Path, i, j-i);

   if (j IS std::string::npos) return ERR_Okay;
   else i = j;

   if (Path[i] IS '(') { // Extract size
      i++;
      PixelSize = StrToInt(Path.c_str() + i);
      i = Path.find(')', i);
      if (i != std::string::npos) i++;
      else return ERR_Okay;
   }

   // Verify the icon size

   if (PixelSize < MIN_ICON_SIZE) PixelSize = MIN_ICON_SIZE;
   else if (PixelSize > MAX_ICON_SIZE) PixelSize = MAX_ICON_SIZE;
   *Size = PixelSize;

   // Check for an overlay before returning, indicated by a '+'

   if ((i = Path.find('+', i)) IS std::string::npos) return ERR_Okay;
   else i++;

   // Process overlay information "ovcategory/ovname(22)" or "ovname(22)"

   j = i;
   i = Path.find_first_of("/\\", i);
   if (i != std::string::npos) {
      OvCategory.assign(Path, j, i-j);
      i++;
      OvIcon.assign(Path, i, Path.find('(', i) - i);
   }
   else OvIcon.assign(Path, j, Path.find('(', j) - j);

   return ERR_Okay;
}

//****************************************************************************

static void apply_filter(objBitmap *Icon, CSTRING FilterName, std::string &Category, std::string &IconName, CSTRING ClassName)
{
   parasol::Log log(__FUNCTION__);

   log.traceBranch("Icon: #%d", Icon->Head.UID);

   parasol::SwitchContext context(modIconServer);

   if (!glIconStyle) {
      CSTRING style_path = "environment:config/icons.fluid"; // Environment takes precedence.
      if (AnalysePath(style_path, NULL)) {
         style_path = "style:icons.fluid"; // Application defined style sheet.
         if (AnalysePath(style_path, NULL)) style_path = "styles:default/icons.fluid"; // System-wide default
      }

      parasol::SwitchContext ctx(modWidget);
      if (CreateObject(ID_FLUID, 0, &glIconStyle,
            FID_Name|TSTR, "IconStyles",
            FID_Path|TSTR, style_path,
            TAGEND) != ERR_Okay) {
         return;
      }
   }

   objBitmap *scratch;
   if (!CreateObject(ID_BITMAP, NF_INTEGRAL, &scratch,
         FID_Width|TLONG,         Icon->Width,
         FID_Height|TLONG,        Icon->Height,
         FID_BitsPerPixel|TLONG,  Icon->BitsPerPixel,
         FID_BytesPerPixel|TLONG, Icon->BytesPerPixel,
         TAGEND)) {

      const ScriptArg filter_args[] = {
         { "Bitmap",   FDF_OBJECT,   { .Address = scratch } },
         { "Filter",   FDF_STRING,   { .Address = (APTR)FilterName } },
         { "Class",    FDF_STRING,   { .Address = (APTR)ClassName } },
         { "Category", FDF_STRING,   { .Address = (APTR)Category.c_str() } },
         { "Icon",     FDF_STRING,   { .Address = (APTR)IconName.c_str() } }
      };

      struct scExec applyUnderlay = {
         .Procedure = "applyUnderlay",
         .Args      = filter_args,
         .TotalArgs = ARRAYSIZE(filter_args)
      };
      ERROR underlay_error;
      if (!(underlay_error = Action(MT_ScExec, glIconStyle, &applyUnderlay))) {
         GetLong(glIconStyle, FID_Error, &underlay_error);
      }

      if (!underlay_error) {
         ULONG *mask = (ULONG *)Icon->Data;
         ULONG *bkgd = (ULONG *)scratch->Data;
         ULONG alpha_mask_out = ~(Icon->ColourFormat->AlphaMask << Icon->ColourFormat->AlphaPos);
         ULONG alpha_mask_in = Icon->ColourFormat->AlphaMask << Icon->ColourFormat->AlphaPos;
         for (LONG y=0; y < Icon->Height; y++) {
            for (LONG x=0; x < Icon->Width; x++) {
               mask[x] = (mask[x] & alpha_mask_in) | (bkgd[x] & alpha_mask_out);
            }
            mask = (ULONG *)(((UBYTE *)mask) + Icon->LineWidth);
            bkgd = (ULONG *)(((UBYTE *)bkgd) + scratch->LineWidth);
         }
      }

      struct scExec applyOverlay = {
         .Procedure = "applyOverlay",
         .Args      = filter_args,
         .TotalArgs = ARRAYSIZE(filter_args)
      };
      Action(MT_ScExec, glIconStyle, &applyOverlay);

      acFree(scratch);
   }
}

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

   if (ResolvePath("iconsource:", 0, &glIconPath) != ERR_Okay) { // The client can set iconsource: to redefine the icon origins
      glIconPath = StrClone("styles:icons/");
   }

   // Icons are stored in compressed archives, accessible via "archive:icons/<category>/<icon>.svg"

   std::string src(glIconPath);
   src.append("Default.zip");
   if (CreateObject(ID_COMPRESSION, NF_INTEGRAL, &glIconArchive,
         FID_Path|TSTR,        src.c_str(),
         FID_ArchiveName|TSTR, "icons",
         FID_Flags|TLONG,      CMF_READ_ONLY,
         TAGEND)) {
      return ERR_CreateObject;
   }

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
   if (init_button() != ERR_Okay) return ERR_AddClass;
   if (init_checkbox() != ERR_Okay) return ERR_AddClass;
   if (init_resize() != ERR_Okay) return ERR_AddClass;
   if (init_combobox() != ERR_Okay) return ERR_AddClass;
   if (init_tabfocus() != ERR_Okay) return ERR_AddClass;
   if (init_input() != ERR_Okay) return ERR_AddClass;
   if (init_text() != ERR_Okay) return ERR_AddClass;

   return ERR_Okay;
}

static ERROR CMDOpen(OBJECTPTR Module)
{
   return ERR_Okay;
}

static ERROR CMDExpunge(void)
{
   free_button();
   free_checkbox();
   free_resize();
   free_combobox();
   free_tabfocus();
   free_input();
   free_text();
   free_clipboard();

   if (glIconArchive) { acFree(glIconArchive);    glIconArchive = NULL; }
   if (glIconStyle)   { acFree(glIconStyle);      glIconStyle  = NULL; }
   if (glIconPath)    { FreeResource(glIconPath); glIconPath   = NULL; }

   if (modIconServer) { acFree(modIconServer); modIconServer = NULL; }
   if (modDisplay)    { acFree(modDisplay);    modDisplay = NULL; }
   if (modFont)       { acFree(modFont);       modFont = NULL; }
   if (modSurface)    { acFree(modSurface);    modSurface = NULL; }
   if (modVector)     { acFree(modVector);     modVector = NULL; }
   return ERR_Okay;
}

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MODVERSION_WIDGET)
