/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

**********************************************************************************************************************

Relevant SVG reference manuals:

https://www.w3.org/TR/SVGColor12/
https://www.w3.org/TR/SVG11/
https://www.w3.org/Graphics/SVG/Test/Overview.html

*********************************************************************************************************************/

//#define DEBUG
#define PRV_SVG
#include <unordered_map>
#include <string>
#include <sstream>
#include <algorithm>
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
struct DisplayBase *DisplayBase;
struct VectorBase *VectorBase;
static OBJECTPTR clSVG = NULL, clRSVG = NULL, modDisplay = NULL, modVector = NULL;

struct prvSVG { // Private variables for RSVG
   OBJECTPTR SVG;
};

struct svgInherit {
   OBJECTPTR Object;
   std::string ID;
};

struct svgID { // All elements using the 'id' attribute will be registered with one of these structures.
   LONG TagIndex;

   svgID(const LONG pTagIndex) {
      TagIndex = pTagIndex;
   }

   svgID() { TagIndex = -1; }
};

struct svgState {
   std::string Fill;
   std::string Stroke;
   std::string FontSize;
   std::string FontFamily;
   DOUBLE  StrokeWidth;
   DOUBLE  FillOpacity;
   DOUBLE  Opacity;
   LONG    FontWeight;
   RQ      PathQuality;

   svgState() : Fill("rgb(0,0,0)"), FontFamily("Open Sans"), StrokeWidth(0), FillOpacity(-1), Opacity(-1),
      FontWeight(0), PathQuality(RQ::AUTO) { }
};

#include <parasol/modules/svg.h>

//********************************************************************************************************************

class extSVG : public objSVG {
   public:
   FUNCTION  FrameCallback;
   std::unordered_map<std::string, XMLTag> IDs;
   std::unordered_map<std::string, objFilterEffect *> Effects; // All effects, registered by their SVG identifier.
   DOUBLE SVGVersion;
   objVectorScene *Scene;
   STRING    Folder;
   OBJECTPTR Viewport; // First viewport (the <svg> tag) to be created on parsing the SVG document.
   std::vector<svgAnimation> Animations;
   std::vector<svgInherit> Inherit;
   TIMER  AnimationTimer;
   bool  Animated;
   bool  PreserveWS; // Preserve white-space
};

//********************************************************************************************************************

static ERROR animation_timer(extSVG *, LARGE, LARGE);
static void  convert_styles(objXML::TAGS &);
static ERROR init_svg(void);
static ERROR init_rsvg(void);
static void  process_attrib(extSVG *, objXML *, const XMLTag &, objVector *);
static void  process_rule(extSVG *, objXML *, objXML::TAGS &, KatanaRule *);
static ERROR process_shape(extSVG *, CLASSID, objXML *, svgState &, const XMLTag &, OBJECTPTR, objVector **);
static ERROR save_svg_scan(extSVG *, objXML *, objVector *, LONG);
static ERROR save_svg_defs(extSVG *, objXML *, objVectorScene *, LONG);
static ERROR save_svg_scan_std(extSVG *, objXML *, objVector *, LONG);
static ERROR save_svg_transform(VectorMatrix *, std::stringstream &);
static ERROR set_property(extSVG *, objVector *, ULONG, objXML *, const XMLTag &, std::string);
static ERROR xtag_animatemotion(extSVG *, objXML *, const XMLTag &, OBJECTPTR Parent);
static ERROR xtag_animatetransform(extSVG *, objXML *, const XMLTag &, OBJECTPTR);
static ERROR xtag_default(extSVG *, objXML *, svgState &, const XMLTag &, OBJECTPTR, objVector **);
static ERROR xtag_defs(extSVG *, objXML *, svgState &, const XMLTag &, OBJECTPTR);
static void  xtag_group(extSVG *, objXML *, svgState &, const XMLTag &, OBJECTPTR, objVector **);
static ERROR xtag_image(extSVG *, objXML *, svgState &, const XMLTag &, OBJECTPTR, objVector **);
static void  xtag_morph(extSVG *, objXML *, const XMLTag &, OBJECTPTR Parent);
static void  xtag_svg(extSVG *, objXML *, svgState &, const XMLTag &, OBJECTPTR, objVector **);
static void  xtag_use(extSVG *, objXML *, svgState &, const XMLTag &, OBJECTPTR);
static ERROR xtag_style(extSVG *, objXML *, const XMLTag &);
static void  xtag_symbol(extSVG *, objXML *, const XMLTag &);

//********************************************************************************************************************

#include "utility.cpp"
#include "save_svg.cpp"

//********************************************************************************************************************

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   if (objModule::load("display", MODVERSION_DISPLAY, &modDisplay, &DisplayBase) != ERR_Okay) return ERR_InitModule;
   if (objModule::load("vector", MODVERSION_VECTOR, &modVector, &VectorBase) != ERR_Okay) return ERR_InitModule;

   if (init_svg()) return ERR_AddClass;
   if (init_rsvg()) return ERR_AddClass;
   return ERR_Okay;
}

ERROR CMDExpunge(void)
{
   if (modDisplay) { FreeResource(modDisplay); modDisplay = NULL; }
   if (modVector)  { FreeResource(modVector);  modVector = NULL; }

   if (clSVG)  { FreeResource(clSVG);  clSVG = NULL; }
   if (clRSVG) { FreeResource(clRSVG); clRSVG = NULL; }
   return ERR_Okay;
}

//********************************************************************************************************************

#include "class_svg.cpp"
#include "class_rsvg.cpp"

//********************************************************************************************************************

PARASOL_MOD(CMDInit, NULL, NULL, CMDExpunge, 1.0, MOD_IDL, NULL)
