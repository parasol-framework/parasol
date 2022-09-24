/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

*****************************************************************************/

//#define DEBUG
#define PRV_SVG
#include <unordered_map>
#include <string>
#include "../picture/picture.h"
#include <parasol/main.h>
#include <parasol/modules/picture.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/vector.h>
#include <parasol/modules/display.h>
#include "svg_def.c"
#include "animation.h"
#include <katana.h>
#include <math.h>

MODULE_COREBASE;
static DisplayBase *DisplayBase;
static VectorBase *VectorBase;
static OBJECTPTR clSVG = NULL, clRSVG = NULL, modDisplay = NULL, modVector = NULL;

struct prvSVG {
   OBJECTPTR SVG;
};

typedef struct svgInherit {
   struct svgInherit *Next;
   OBJECTPTR Object;
   char ID[60];
} svgInherit;

typedef class svgID { // All elements using the 'id' attribute will be registered with one of these structures.
   public:
   LONG TagIndex;

   svgID(const LONG pTagIndex) {
      TagIndex = pTagIndex;
   }

   svgID() { TagIndex = -1; }
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
static ERROR init_rsvg(void);
static ERROR animation_timer(objSVG *, LARGE, LARGE);
static void  convert_styles(objXML *);
static void  process_attrib(objSVG *, objXML *, const XMLTag *, OBJECTPTR);
static void  process_rule(objSVG *, objXML *, KatanaRule *);
static ERROR process_shape(objSVG *, CLASSID, objXML *, svgState *, const XMLTag *, OBJECTPTR, OBJECTPTR *);
static ERROR save_svg_scan(objSVG *, objXML *, objVector *, LONG);
static ERROR save_svg_defs(objSVG *, objXML *, objVectorScene *, LONG);
static ERROR save_svg_scan_std(objSVG *, objXML *, objVector *, LONG);
static ERROR save_svg_transform(VectorMatrix *, char *, LONG);
static ERROR set_property(objSVG *, OBJECTPTR, ULONG, objXML *, const XMLTag *, CSTRING);
static ERROR xtag_animatemotion(objSVG *, objXML *, const XMLTag *, OBJECTPTR Parent);
static ERROR xtag_animatetransform(objSVG *, objXML *, const XMLTag *, OBJECTPTR);
static ERROR xtag_default(objSVG *, ULONG, objXML *, svgState *, const XMLTag *, OBJECTPTR, OBJECTPTR *);
static ERROR xtag_defs(objSVG *, objXML *, svgState *, const XMLTag *, OBJECTPTR);
static void  xtag_group(objSVG *, objXML *, svgState *, const XMLTag *, OBJECTPTR, OBJECTPTR *);
static ERROR xtag_image(objSVG *, objXML *, svgState *, const XMLTag *, OBJECTPTR, OBJECTPTR *);
static void  xtag_morph(objSVG *, objXML *, const XMLTag *, OBJECTPTR Parent);
static void  xtag_svg(objSVG *, objXML *, svgState *, const XMLTag *, OBJECTPTR, OBJECTPTR *);
static void  xtag_use(objSVG *, objXML *, svgState *, const XMLTag *, OBJECTPTR);
static ERROR xtag_style(objSVG *, objXML *, const XMLTag *);
static void  xtag_symbol(objSVG *, objXML *, const XMLTag *);

//****************************************************************************

#include "utility.cpp"
#include "save_svg.cpp"

//****************************************************************************

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   if (LoadModule("display", MODVERSION_DISPLAY, &modDisplay, &DisplayBase) != ERR_Okay) return ERR_InitModule;
   if (LoadModule("vector", MODVERSION_VECTOR, &modVector, &VectorBase) != ERR_Okay) return ERR_InitModule;

   if (init_svg()) return ERR_AddClass;
   if (init_rsvg()) return ERR_AddClass;
   return ERR_Okay;
}

ERROR CMDExpunge(void)
{
   if (modDisplay) { acFree(modDisplay); modDisplay = NULL; }
   if (modVector)  { acFree(modVector);  modVector = NULL; }

   if (clSVG)  { acFree(clSVG);  clSVG = NULL; }
   if (clRSVG) { acFree(clRSVG); clRSVG = NULL; }
   return ERR_Okay;
}

//****************************************************************************

#include "class_svg.cpp"
#include "class_rsvg.cpp"

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, NULL, CMDExpunge, 1.0)
