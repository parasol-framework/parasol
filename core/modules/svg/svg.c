/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

*****************************************************************************/

//#define DEBUG
#define PRV_SVG
#include "../picture/picture.h"
#include <parasol/main.h>
#include <parasol/modules/picture.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/vector.h>
#include <parasol/modules/display.h>
#include <parasol/modules/surface.h>
#include "svg_def.c"
#include "animation.h"
#include <math.h>

MODULE_COREBASE;
static struct DisplayBase *DisplayBase;
static struct SurfaceBase *SurfaceBase;
static struct VectorBase *VectorBase;
static OBJECTPTR clSVG = NULL, clSVGImage = NULL, clRSVG = NULL, modDisplay = NULL, modSurface = NULL, modVector = NULL;

struct prvSVG {
   OBJECTPTR SVG;
};

typedef struct svgInherit {
   struct svgInherit *Next;
   OBJECTPTR Object;
   UBYTE ID[60];
} svgInherit;

typedef struct svgID { // All elements using the 'id' attribute will be registered with one of these structures.
   struct svgID *Next;
   CSTRING ID;
   ULONG IDHash;
   LONG TagIndex;
} svgID;

typedef struct svgState {
   CSTRING Fill;
   CSTRING Stroke;
   CSTRING FontSize;
   CSTRING FontFamily;
   DOUBLE StrokeWidth;
   DOUBLE FillOpacity;
   DOUBLE Opacity;
} svgState;

#include <parasol/modules/svg.h>

//****************************************************************************

static ERROR init_svg(void);
static ERROR init_svgimage(void);
static ERROR init_rsvg(void);
static ERROR animation_timer(objSVG *, LARGE, LARGE);
static void convert_styles(objXML *);
static void  process_attrib(objSVG *, objXML *, struct XMLTag *, OBJECTPTR);
static ERROR process_shape(objSVG *, CLASSID, objXML *, svgState *, struct XMLTag *, OBJECTPTR, OBJECTPTR *);
static ERROR save_svg_scan(objSVG *, objXML *, objVector *, LONG);
static ERROR save_svg_defs(objSVG *, objXML *, objVectorScene *, LONG);
static ERROR save_svg_scan_std(objSVG *, objXML *, objVector *, LONG);
static ERROR save_svg_transform(struct VectorTransform *, char *, LONG);
static ERROR set_property(objSVG *, OBJECTPTR, ULONG Hash, objXML *, struct XMLTag *, CSTRING);
static ERROR xtag_defs(objSVG *, objXML *, svgState *, struct XMLTag *, OBJECTPTR);
static void  xtag_group(objSVG *, objXML *, svgState *, struct XMLTag *, OBJECTPTR, OBJECTPTR *);
static ERROR xtag_image(objSVG *, objXML *, svgState *, struct XMLTag *, OBJECTPTR, OBJECTPTR *);
static void  xtag_svg(objSVG *, objXML *, svgState *, struct XMLTag *, OBJECTPTR, OBJECTPTR *);
static void  xtag_use(objSVG *, objXML *, svgState *, struct XMLTag *, OBJECTPTR);
static void xtag_symbol(objSVG *, objXML *, struct XMLTag *);

//****************************************************************************

#include "utility.c"
#include "save_svg.c"

//****************************************************************************

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   if (LoadModule("surface", MODVERSION_SURFACE, &modSurface, &SurfaceBase) != ERR_Okay) return ERR_InitModule;
   if (LoadModule("display", MODVERSION_DISPLAY, &modDisplay, &DisplayBase) != ERR_Okay) return ERR_InitModule;
   if (LoadModule("vector", MODVERSION_VECTOR, &modVector, &VectorBase) != ERR_Okay) return ERR_InitModule;

   if (init_svg()) return ERR_AddClass;
   if (init_svgimage()) return ERR_AddClass;
   if (init_rsvg()) return ERR_AddClass;
   return ERR_Okay;
}

ERROR CMDExpunge(void)
{
   if (modSurface) { acFree(modSurface); modSurface = NULL; }
   if (modDisplay) { acFree(modDisplay); modDisplay = NULL; }
   if (modVector)  { acFree(modVector);  modVector = NULL; }

   if (clSVG)      { acFree(clSVG);      clSVG = NULL; }
   if (clSVGImage) { acFree(clSVGImage); clSVGImage = NULL; }
   if (clRSVG)     { acFree(clRSVG);     clRSVG = NULL; }
   return ERR_Okay;
}

//****************************************************************************

#include "class_svg.c"
#include "class_svgimage.c"
#include "class_rsvg.c"

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, NULL, CMDExpunge, 1.0)
