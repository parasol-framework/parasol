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
struct IconServerBase *IconServerBase;

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
#include "module_def.c"

/*****************************************************************************

-FUNCTION-
CreateIcon: Generate an icon bitmap from a given path.

Use CreateIcon() to generate an icon image that is scaled to fit a new bitmap.

The referenced icon Path must refer to an icon that exists in the icon dictionary, and be in a recognised format
such as:

```
category/icon
category/icon(size)
```

If the call is made from an internal class then specify the name in the Class parameter (this may be used by the
logic filter).  The Filter parameter should not be set unless an alternative filter style is needed.

If a size is specified in the icon Path then that value will take precedence over the Size parameter.

The resulting Bitmap must be freed once it is no longer required.

-INPUT-
cstr Path:   The path to the icon, e.g. 'tools/magnifier'
cstr Class:  The name of the class requesting the filter (optional).
cstr Filter: The graphics filter to apply to the icon.  Usually set to NULL for the default.
int Size:    The pixel size (width and height) of the resulting bitmap.  If zero, the size will be automatically calculated from the display DPI values.
&!obj(Bitmap) Bitmap: The resulting bitmap will be returned in this parameter.  It is the responsibility of the client to terminate the bitmap.

-ERRORS-
Okay
NullArgs
CreateObject
Activate
-END-

*****************************************************************************/

ERROR widgetCreateIcon(CSTRING Path, CSTRING Class, CSTRING Filter, LONG Size, objBitmap **BitmapResult)
{
   parasol::Log log("CreateIcon");

   if ((!Path) or (!BitmapResult)) return log.warning(ERR_NullArgs);

   if (!StrCompare("icons:", Path, 6, 0)) Path += 6;
   *BitmapResult = NULL;

   if (Size <= 0) {
      SURFACEINFO *info;
      drwGetSurfaceInfo(0, &info);
      if (info->Width < info->Height) Size = ((DOUBLE)info->Width) * DEFAULT_RATIO / 100.0;
      else Size = ((DOUBLE)info->Height) * DEFAULT_RATIO / 100.0;
   }

   log.traceBranch("Path: %s, Class: %s, Filter: %s, Size: %d", Path, Class, Filter, Size);

   ERROR error;
   objBitmap *bmp = NULL;
   error = ERR_Okay;

   std::string tpath(Path);
   std::string category, icon, ovcategory, ovicon;

   category.reserve(30);
   icon.reserve(30);
   ovcategory.reserve(20);
   ovicon.reserve(20);

   if ((error = extract_icon(Size, tpath, category, icon, ovcategory, ovicon, &Size)) != ERR_Okay) return error;

   auto filepath = "archive:icons/" + category + "/" + icon;

   AdjustLogLevel(1);

   log.trace("Resolved '%s' to '%s', overlay '%s/%s', size %d", Path, filepath.c_str(), ovcategory.empty() ? "-" : ovcategory.c_str(), ovicon.empty() ? "-" : ovicon.c_str(), Size);

   objPicture *picture;
   if (!CreateObject(ID_PICTURE, NF_INTEGRAL, &picture,
         FID_Path|TSTR,   filepath.c_str(),
         FID_Flags|TLONG, PCF_FORCE_ALPHA_32|PCF_LAZY, // Lazy option avoids activation on initialisation.
         TAGEND)) {

      if (picture->Flags & PCF_SCALABLE) {
         picture->DisplayWidth = Size;
         picture->DisplayHeight = Size;
         if (!acActivate(picture)) {
            if (!CreateObject(ID_BITMAP, NF_INTEGRAL, &bmp,
                  FID_Name|TSTR,          icon.c_str(),
                  FID_Flags|TLONG,        BMF_ALPHA_CHANNEL,
                  FID_BitsPerPixel|TLONG, 32,
                  FID_Width|TLONG,        picture->Bitmap->Width,
                  FID_Height|TLONG,       picture->Bitmap->Height,
                  TAGEND)) {

               gfxCopyArea(picture->Bitmap, bmp, 0, 0, 0, picture->Bitmap->Width, picture->Bitmap->Height, 0, 0);
            }
            else error = ERR_CreateObject;
         }
         else error = ERR_Activate;
      }
      else if (!acActivate(picture)) {
         // Initialise the destination picture that we are going to use for resizing

         DOUBLE sizeratio;
         if (picture->Bitmap->Width > picture->Bitmap->Height) sizeratio = (DOUBLE)Size / (DOUBLE)picture->Bitmap->Width;
         else sizeratio = (DOUBLE)Size / (DOUBLE)picture->Bitmap->Height;

         if (!CreateObject(ID_BITMAP, NF_INTEGRAL, &bmp,
               FID_Name|TSTR,          icon.c_str(),
               FID_Flags|TLONG,        BMF_ALPHA_CHANNEL,
               FID_BitsPerPixel|TLONG, picture->Bitmap->BitsPerPixel,
               FID_Width|TLONG,        F2T(((DOUBLE)picture->Bitmap->Width)  * sizeratio),
               FID_Height|TLONG,       F2T(((DOUBLE)picture->Bitmap->Height) * sizeratio),
               TAGEND)) {

            // Stretch the source into the destination
            gfxCopyStretch(picture->Bitmap, bmp, CSTF_BILINEAR|CSTF_FILTER_SOURCE, 0, 0,
               picture->Bitmap->Width, picture->Bitmap->Height, 0, 0, bmp->Width, bmp->Height);
         }
         else error = ERR_CreateObject;
      }
      else error = ERR_Activate;

      if (!error) {
         apply_filter(bmp, Filter, category, icon, Class);

         if ((!ovcategory.empty()) and (!ovicon.empty())) { // Load an overlay on top of the icon if requested.  Errors here are not fatal.
            auto overlay = "archive:icons/" + ovcategory + "/" + ovicon;
            log.trace("Loading overlay %s", overlay.c_str());

            objPicture *ovpic;
            if (!CreateObject(ID_PICTURE, NF_INTEGRAL, &ovpic,
                  FID_Path|TSTR,   overlay.c_str(),
                  FID_Flags|TLONG, PCF_FORCE_ALPHA_32,
                  TAGEND)) {
               objBitmap *temp;
               if (!CreateObject(ID_BITMAP, NF_INTEGRAL, &temp,
                     FID_Width|TLONG,        bmp->Width,
                     FID_Height|TLONG,       bmp->Height,
                     FID_BitsPerPixel|TLONG, 32,
                     FID_Flags|TLONG,        BMF_ALPHA_CHANNEL,
                     TAGEND)) {
                  gfxCopyStretch(ovpic->Bitmap, temp, CSTF_BILINEAR|CSTF_FILTER_SOURCE, 0, 0,
                     ovpic->Bitmap->Width, ovpic->Bitmap->Height, 0, 0, temp->Width, temp->Height);
                  gfxCopyArea(temp, bmp, BAF_BLEND, 0, 0, temp->Width, temp->Height, 0, 0);
                  acFree(temp);
               }
            }
         }
      }

      acFree(picture);
   }
   else {
      log.error("Failed to open icon image at \"%s\".", filepath.c_str());
      error = ERR_CreateObject;
   }

   if (!error) *BitmapResult = bmp;
   else if (bmp) acFree(bmp);

   AdjustLogLevel(-1);
   return error;
}

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
      AST_ICON,  "programs/iconthemes",
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
   if (init_menu() != ERR_Okay) return ERR_AddClass;
   if (init_menuitem() != ERR_Okay) return ERR_AddClass;

   return ERR_Okay;
}

static ERROR CMDOpen(OBJECTPTR Module)
{
   SetPointer(Module, FID_FunctionList, glFunctions);
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
   free_menu();
   free_menuitem();
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
