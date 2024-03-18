/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

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
#include <charconv>
#include <algorithm>
#include <parasol/main.h>
#include <parasol/modules/picture.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/vector.h>
#include <parasol/modules/display.h>
#include <parasol/strings.hpp>
#include "svg_def.c"
#include "animation.h"
#include <katana.h>
#include <math.h>

using namespace pf;

JUMPTABLE_CORE
JUMPTABLE_DISPLAY
JUMPTABLE_VECTOR

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

#include <parasol/modules/svg.h>

//********************************************************************************************************************

class extSVG : public objSVG {
   public:
   FUNCTION  FrameCallback;
   std::unordered_map<std::string, XMLTag> IDs;
   std::unordered_map<std::string, objFilterEffect *> Effects; // All effects, registered by their SVG identifier.
   DOUBLE SVGVersion;
   objXML *XML;
   objVectorScene *Scene;
   STRING    Folder;
   std::string Colour = "rgb(0,0,0)"; // Default colour, used for 'currentColor' references
   OBJECTPTR Viewport; // First viewport (the <svg> tag) to be created on parsing the SVG document.
   std::vector<svgAnimation> Animations;
   std::vector<svgInherit> Inherit;
   TIMER  AnimationTimer;
   WORD  Duplicated;  // Incremented when inside a duplicated tag space, e.g. due to a <use> tag
   bool  Animated;
   bool  PreserveWS; // Preserve white-space
};

struct svgState {
   std::string m_color;
   std::string m_fill;
   std::string m_stroke;
   std::string m_font_size;
   std::string m_font_family;
   DOUBLE  m_stroke_width;
   DOUBLE  m_fill_opacity;
   DOUBLE  m_opacity;
   LONG    m_font_weight;
   RQ      m_path_quality;

   private:
   objVectorScene *Scene;

   public:
   svgState(class extSVG *pSVG) : m_color(pSVG->Colour), m_fill("rgb(0,0,0)"), m_font_family("Noto Sans"), m_stroke_width(0),
      m_fill_opacity(-1), m_opacity(-1), m_font_weight(0), m_path_quality(RQ::AUTO), Scene(pSVG->Scene) { }

   void applyTag(const XMLTag &) noexcept;
   void applyAttribs(OBJECTPTR) const noexcept;
};

//********************************************************************************************************************

static ERROR animation_timer(extSVG *, LARGE, LARGE);
static void  convert_styles(objXML::TAGS &);
static ERROR init_svg(void);
static ERROR init_rsvg(void);
static void  process_attrib(extSVG *, const XMLTag &, svgState &, objVector *);
static void  process_children(extSVG *, svgState &, const XMLTag &, OBJECTPTR);
static void  process_rule(extSVG *, objXML::TAGS &, KatanaRule *);
static ERROR process_shape(extSVG *, CLASSID, svgState &, const XMLTag &, OBJECTPTR, objVector * &);
static ERROR save_svg_scan(extSVG *, objXML *, objVector *, LONG);
static ERROR save_svg_defs(extSVG *, objXML *, objVectorScene *, LONG);
static ERROR save_svg_scan_std(extSVG *, objXML *, objVector *, LONG);
static ERROR save_svg_transform(VectorMatrix *, std::stringstream &);
static ERROR set_property(extSVG *, objVector *, ULONG, const XMLTag &, svgState &, std::string);
static ERROR xtag_animatemotion(extSVG *, const XMLTag &, OBJECTPTR Parent);
static ERROR xtag_animatetransform(extSVG *, const XMLTag &, OBJECTPTR);
static ERROR xtag_default(extSVG *, svgState &, const XMLTag &, OBJECTPTR, objVector * &);
static ERROR xtag_defs(extSVG *, svgState &, const XMLTag &, OBJECTPTR);
static void  xtag_group(extSVG *, svgState &, const XMLTag &, OBJECTPTR, objVector * &);
static ERROR xtag_image(extSVG *, svgState &, const XMLTag &, OBJECTPTR, objVector * &);
static void  xtag_morph(extSVG *, const XMLTag &, OBJECTPTR Parent);
static void  xtag_svg(extSVG *, svgState &, const XMLTag &, OBJECTPTR, objVector * &);
static void  xtag_use(extSVG *, svgState &, const XMLTag &, OBJECTPTR);
static ERROR xtag_style(extSVG *, const XMLTag &);
static void  xtag_symbol(extSVG *, const XMLTag &);

//********************************************************************************************************************

#include "funit.cpp"
#include "utility.cpp"
#include "save_svg.cpp"

//********************************************************************************************************************

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   if (objModule::load("display", &modDisplay, &DisplayBase) != ERR_Okay) return ERR_InitModule;
   if (objModule::load("vector", &modVector, &VectorBase) != ERR_Okay) return ERR_InitModule;

   if (init_svg()) return ERR_AddClass;
   if (init_rsvg()) return ERR_AddClass;
   return ERR_Okay;
}

static ERROR CMDExpunge(void)
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

PARASOL_MOD(CMDInit, NULL, NULL, CMDExpunge, MOD_IDL, NULL)
extern "C" struct ModHeader * register_svg_module() { return &ModHeader; }
