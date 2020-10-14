/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

*****************************************************************************/

//#define DEBUG
#define PRV_ICONSERVER
#define PRV_ICONSERVER_MODULE
#define __system__
#include <parasol/modules/xml.h>
#include <parasol/modules/picture.h>
#include <parasol/modules/display.h>
#include <parasol/modules/iconserver.h>
#include <parasol/modules/surface.h>

#define VER_IconServer 1.0

#define BUFFERSIZE 60

const struct Function glFunctions[];

MODULE_COREBASE;
static struct SurfaceBase *SurfaceBase;
static struct DisplayBase *DisplayBase;
static OBJECTPTR modSurface = NULL;
static OBJECTPTR modDisplay = NULL;
static OBJECTPTR modIconServer = NULL;
static OBJECTPTR clIconServer = NULL;
static STRING glIconPath = NULL;
static char glFilterID[40] = "default";
static UBYTE *glDatabase = NULL;
static LONG glDataSize = 0;
static OBJECTID glIconServerID = 0;
static OBJECTPTR glIconStyle = NULL;

enum {
   CODE_CATEGORY=1,
   CODE_ICON,
   CODE_END
};

#define MIN_SIZE     4
#define DEFAULT_SIZE 16
#define MAX_SIZE     1024

static const struct FieldArray clFields[];
static const struct ActionArray clActions[];

static ERROR load_icon_db(objIconServer *);
static void write_icon_category(objIconServer *, OBJECTPTR, STRING);
static ERROR extract_icon(LONG, CSTRING, STRING, STRING, STRING, STRING, LONG *);
static ERROR find_icon_category(STRING, CSTRING);
static void apply_filter(objBitmap *, CSTRING, STRING, STRING, CSTRING);
static void get_style(void);
static ERROR add_iconserver(void);

//****************************************************************************

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   GetPointer(argModule, FID_Master, &modIconServer);
   if (LoadModule("surface", MODVERSION_SURFACE, &modSurface, &SurfaceBase) != ERR_Okay) return ERR_InitModule;
   if (LoadModule("display", MODVERSION_DISPLAY, &modDisplay, &DisplayBase) != ERR_Okay) return ERR_InitModule;

   if (add_iconserver() != ERR_Okay) return ERR_AddClass;

   // Add the icons: special assignment

   SetVolume(AST_NAME, "icons",
             AST_PATH, ":SystemIcons", // Tells the filesystem resolver to use 'SystemIcons' to perform resolution.
             AST_FLAGS, VOLUME_REPLACE|VOLUME_HIDDEN,
             AST_ICON, "programs/iconthemes",
             TAGEND);

   if (ResolvePath("iconsource:", 0, &glIconPath) != ERR_Okay) {
      glIconPath = StrClone("system:icons/");
   }

   get_style();

   return ERR_Okay;
}

static ERROR CMDOpen(OBJECTPTR Module)
{
   SetPointer(Module, FID_FunctionList, glFunctions);
   return ERR_Okay;
}

static ERROR CMDExpunge(void)
{
   if (glDatabase)   { FreeResource(glDatabase); glDatabase   = NULL; }
   if (glIconStyle)  { acFree(glIconStyle);    glIconStyle  = NULL; }
   if (clIconServer) { acFree(clIconServer);   clIconServer = NULL; }
   if (modSurface)   { acFree(modSurface);     modSurface   = NULL; }
   if (modDisplay)   { acFree(modDisplay);     modDisplay   = NULL; }
   if (glIconPath)   { FreeResource(glIconPath); glIconPath   = NULL; }
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
CreateIcon: Create an icon from a given path.

TBA

-INPUT-
cstr Path:   The path to the icon, e.g. 'tools/magnifier'
cstr Class:  The name of the class requesting the filter (optional).
cstr Theme:  The icon theme to use.   Usually set to NULL for the default.
cstr Filter: The graphics filter to apply to the icon.  Usually set to NULL for the default.
int Size:    The pixel size (width and height) of the resulting bitmap.  If zero, the default will be returned.
&!obj(Bitmap) Bitmap: The resulting bitmap will be returned in this parameter.  It is the responsibility of the client to terminate the bitmap.

-ERRORS-
Okay
NullArgs
-END-

*****************************************************************************/

static ERROR iconCreateIcon(CSTRING Path, CSTRING Class, CSTRING Theme, CSTRING Filter, LONG Size, objBitmap **BitmapResult)
{
   if ((!Path) OR (!BitmapResult)) return PostError(ERR_NullArgs);

   if (!StrCompare("icons:", Path, 6, 0)) Path += 6;
   *BitmapResult = NULL;

   char theme_buffer[80] = "Default";
   if (!glIconServerID) FastFindObject("systemicons", ID_ICONSERVER, &glIconServerID, 1, NULL);
   objIconServer *server;
   if ((glIconServerID) AND (!AccessObject(glIconServerID, 3000, &server))) {
      if (Size <= 0) {
         Size = server->FixedSize;
         if (Size < 12) {
            SURFACEINFO *info;
            drwGetSurfaceInfo(0, &info);
            if (info->Width < info->Height) Size = ((DOUBLE)info->Width) * server->IconRatio / 100.0;
            else Size = ((DOUBLE)info->Height) * server->IconRatio / 100.0;
         }
      }

      if ((!Theme) OR (!Theme[0])) {
         Theme = theme_buffer;
         StrCopy(server->prvTheme, theme_buffer, sizeof(theme_buffer));
      }

      ReleaseObject(server);
   }
   else if ((!Theme) OR (!Theme[0])) Theme = theme_buffer;

   FMSG("~CreateIcon()","Path: %s, Class: %s, Theme: %s, Filter: %s, Size: %d", Path, Class, Theme, Filter, Size);

   AdjustLogLevel(1);

   // Extract the icon name, theme and size

   ERROR error;
   char category[BUFFERSIZE], icon[BUFFERSIZE], ovcategory[BUFFERSIZE], ovicon[BUFFERSIZE]; // NB: path extraction code assumes these are all the same array size

   if ((error = extract_icon(Size, Path, category, icon, ovcategory, ovicon, &Size)) != ERR_Okay) goto end;
   if ((error = find_icon_category(category, icon)) != ERR_Okay) goto end;

   char filepath[BUFFERSIZE];
   StrFormat(filepath, sizeof(filepath), "%s%s/%s/%s", glIconPath, Theme, category, icon);

   MSG("Resolved '%s' to '%s', overlay '%s/%s', size %d", Path, filepath, ovcategory[0] ? (STRING)ovcategory : "-", ovicon[0] ? (STRING)ovicon : "-", Size);

   if (ovcategory[0]) {
      if (find_icon_category(ovcategory, ovicon) != ERR_Okay) ovcategory[0] = 0;
      MSG("Overlay category '%s', icon '%s'", ovcategory, ovicon);
   }

   objBitmap *bmp = NULL;
   error = ERR_Okay;
   objPicture *picture;
   if (!CreateObject(ID_PICTURE, NF_INTEGRAL, &picture,
         FID_Path|TSTR,   filepath,
         FID_Flags|TLONG, PCF_FORCE_ALPHA_32|PCF_LAZY, // Lazy option avoids activation on initialisation.
         TAGEND)) {

      if (picture->Flags & PCF_SCALABLE) {
         picture->DisplayWidth = Size;
         picture->DisplayHeight = Size;
         if (!acActivate(picture)) {
            if (!CreateObject(ID_BITMAP, NF_INTEGRAL, &bmp,
                  FID_Name|TSTR,          icon,
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
               FID_Name|TSTR,          icon,
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

         if ((ovcategory[0]) AND (ovicon[0])) { // Load an overlay on top of the icon if requested.  Errors here are not fatal.
            objPicture *ovpic;
            char overlay[256];

            MSG("Loading overlay %s/%s", ovcategory, ovicon);

            StrFormat(overlay, sizeof(overlay), "%s%s/%s/%s", glIconPath, Theme, ovcategory, ovicon);

            if (!CreateObject(ID_PICTURE, NF_INTEGRAL, &ovpic,
                  FID_Path|TSTR,   overlay,
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
      LogF("!CreateIcon","Failed to open icon image at \"%s\".", filepath);
      error = ERR_CreateObject;
   }

   if (!error) *BitmapResult = bmp;
   else if (bmp) acFree(bmp);

end:
   AdjustLogLevel(-1);
   STEP();
   return error;
}

//****************************************************************************
// Finds the correct category and path that should be used to load/save the icon.

static ERROR find_icon_category(STRING Category, CSTRING Icon)
{
   LogF("~7find_icon","Category: %s, Icon: %s", Category, Icon);

   // Check the database to see if the requested icon exists at the specified category.  If it doesn't, clear the
   // category so that we can scan for the icon in other areas.

   if (Category[0]) {
      UBYTE *data = glDatabase;
      while (data) {
         WORD code = ((LONG *)data)[0];

         if (code IS CODE_END) {
            LogF("@find_icon","Category '%s' does not exist for icon '%s'.", Category, Icon);
            Category[0] = 0;
            break;
         }

         if (code IS CODE_CATEGORY) {
            if (!StrMatch(Category, data + (sizeof(LONG)*2))) {
               // Scan within this category to see if the icon lives there

               data = data + ((LONG *)data)[1];
               while (((LONG *)data)[0] IS CODE_ICON) {
                  if (!StrMatch(Icon, data + (sizeof(LONG)*2))) break;
                  data = data + ((LONG *)data)[1];
               }

               if (((LONG *)data)[0] != CODE_ICON) {
                  LogF("@find_icon","Icon '%s' does not exist in category '%s'", Icon, Category);
                  Category[0] = 0;
               }
               break;
            }
         }

         data = data + ((LONG *)data)[1];
      }
   }

   // If no category was specified, scan the icon database for the icon

   ERROR error = ERR_Okay;
   if (!Category[0]) {
      LogMsg("Scanning database for a category for icon '%s'.", Icon);

      UBYTE *data = glDatabase;
      while (data) {
         WORD code = ((LONG *)data)[0];
         if (code IS CODE_END) { error = ERR_Search; goto end; } // Return if we got to the end without any result
         if (code IS CODE_CATEGORY) StrCopy(data + (sizeof(LONG)*2), Category, BUFFERSIZE);
         if (code IS CODE_ICON) {
            if (!StrMatch(Icon, data + (sizeof(LONG)*2))) {
               // We have the correct category, so we just need to break the loop
               goto end;
            }
         }

         data = data + ((LONG *)data)[1];
      }

      error = ERR_Search;
   }
end:
   LogBack();
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

static ERROR extract_icon(LONG PixelSize, CSTRING Path, STRING category, STRING icon, STRING ovcategory, STRING ovicon, LONG *Size)
{
   FMSG("extract_icon()","%s", Path);

   category[0]   = 0;
   icon[0]       = 0;
   ovcategory[0] = 0;
   ovicon[0]     = 0;

   WORD size = PixelSize;

   // Extract the category and the name of the icon

   WORD i;
   for (i=0; (Path[i] != ':') AND (Path[i]); i++);
   if (Path[i]) i++;
   else i = 0;

   WORD j = 0;
   while ((Path[i] != '/') AND (Path[i] != '\\') AND (Path[i] != '(') AND (Path[i] != '+') AND (Path[i])) category[j++] = Path[i++];
   category[j] = 0;

   if (Path[i] IS '(') {
      i++;
      size = StrToInt(Path+i);
      while ((Path[i]) AND (Path[i] != ')')) i++;
      if (Path[i] IS ')') i++;
   }

   if ((Path[i]) AND (Path[i] != '+')) {
      // Extract the icon name
      while ((Path[i]) AND (Path[i] != '+')) i++;
      while ((i > 0) AND (Path[i-1] != '/') AND (Path[i-1] != '\\')) i--;
      j = 0;
      while ((Path[i] != '/') AND (Path[i] != '\\') AND (Path[i] != '(') AND (Path[i] != '+') AND (Path[i])) icon[j++] = Path[i++];
      icon[j] = 0;
   }
   else {
      // There is no listed category (the first string was a reference to the icon name)
      for (j=0; category[j]; j++) icon[j] = category[j];
      icon[j] = 0;
      category[0] = 0;
   }

   // Strip the file extension if one is specified

   WORD k = j;
   while ((k > 0) AND (icon[k] != '.') AND (icon[k] != ':') AND (icon[k] != '/') AND (icon[k] != '\\')) k--;
   if (icon[k] IS '.') { j = k; icon[k] = 0; }

   // Extract the icon size if one is specified

   FMSG("extract_icon","Checking for size (default %d) from %s", size, Path);

   if (Path[i] IS '(') size = StrToInt(Path + i);
   else {
      for (j=i; Path[j]; j++) {
         if (Path[j] IS '(') {
            size = StrToInt(Path + j);
            break;
         }
      }
   }

   if (size < MIN_SIZE) size = MIN_SIZE;
   else if (size > MAX_SIZE) size = MAX_SIZE;
   *Size = size;

   // Check for an overlay before returning

   while ((Path[i]) AND (Path[i] != '+')) i++;
   if (Path[i] != '+') return ERR_Okay;
   i++; // Skip ;

   // Process overlay information

   for (j=i; Path[j]; j++) {
      if (Path[j] IS ':') {
         i = j + 1;
         break;
      }
   }

   j = 0;
   while ((Path[i] != '/') AND (Path[i] != '\\') AND (Path[i] != '(') AND (Path[i])) ovcategory[j++] = Path[i++];
   ovcategory[j] = 0;

   if (Path[i]) { // Extract the icon name
      while (Path[i]) i++;
      while ((i > 0) AND (Path[i-1] != '/') AND (Path[i-1] != '\\')) i--;
      j = 0;
      while ((Path[i] != '/') AND (Path[i] != '\\') AND (Path[i] != '(') AND (Path[i])) ovicon[j++] = Path[i++];
      ovicon[j] = 0;
   }
   else { // There is no listed category (the first string was a reference to the icon name)
      for (j=0; ovcategory[j]; j++) ovicon[j] = ovcategory[j];
      ovicon[j] = 0;
      ovcategory[0] = 0;
   }

   // Strip the .png extension from the icon name if one is specified

   while ((j > 0) AND (ovicon[j] != '.') AND (ovicon[j] != ':') AND (ovicon[j] != '/') AND (ovicon[j] != '\\')) j--;
   if (ovicon[j] IS '.') ovicon[j] = 0;

   return ERR_Okay;
}

//****************************************************************************

static void get_style(void)
{
   //styleName()

   if (!glFilterID[0]) StrCopy("default", glFilterID, sizeof(glFilterID));
}

//****************************************************************************

static void apply_filter(objBitmap *Icon, CSTRING FilterName, STRING Category, STRING IconName, CSTRING ClassName)
{
   FMSG("~apply_filter()","Icon: #%d", Icon->Head.UniqueID);

   OBJECTPTR context = SetContext(modIconServer);

   if (!glIconStyle) {
      CSTRING style_path = "environment:config/icons.fluid"; // Environment takes precedence.
      if (AnalysePath(style_path, NULL)) {
         style_path = "style:icons.fluid"; // Application defined style sheet.
         if (AnalysePath(style_path, NULL)) style_path = "styles:default/icons.fluid"; // System-wide default
      }

      if (CreateObject(ID_FLUID, 0, &glIconStyle,
            FID_Name|TSTR, "IconStyles",
            FID_Path|TSTR, style_path,
            TAGEND) != ERR_Okay) {
         SetContext(context);
         STEP();
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

      const struct ScriptArg filter_args[] = {
         { "Bitmap",   FDF_OBJECT,   { .Address = scratch } },
         { "Filter",   FDF_STRING,   { .Address = (APTR)FilterName } },
         { "Class",    FDF_STRING,   { .Address = (APTR)ClassName } },
         { "Category", FDF_STRING,   { .Address = (APTR)Category } },
         { "Icon",     FDF_STRING,   { .Address = (APTR)IconName } }
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
         LONG x, y;
         for (y=0; y < Icon->Height; y++) {
            for (x=0; x < Icon->Width; x++) {
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

   SetContext(context);
   STEP();
}

//****************************************************************************

#include "module_def.c"

#include "iconserver_class.c"

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MODVERSION_ICONSERVER)
