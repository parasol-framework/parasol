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
#include <list>
#include <variant>
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

static OBJECTPTR clSVG = NULL, clRSVG = NULL, modDisplay = NULL, modVector = NULL, modPicture = NULL;

struct prvSVG { // Private variables for RSVG
   OBJECTPTR SVG;
};

struct svgInherit {
   OBJECTPTR Object;
   std::string ID;
};

struct svgLink {
   std::string ref;
};

struct svgID { // All elements using the 'id' attribute will be registered with one of these structures.
   LONG TagIndex;

   svgID(const LONG pTagIndex) {
      TagIndex = pTagIndex;
   }

   svgID() { TagIndex = -1; }
};

struct svgAnimState {
   VectorMatrix *matrix = NULL;
   std::vector<class anim_transform *> transforms;
};

#include <parasol/modules/svg.h>

//********************************************************************************************************************

class extSVG : public objSVG {
   public:
   FUNCTION FrameCallback;
   std::unordered_map<std::string, XMLTag> IDs;
   std::unordered_map<std::string, objFilterEffect *> Effects; // All effects, registered by their SVG identifier.
   DOUBLE SVGVersion;
   objXML *XML;
   objVectorScene *Scene;
   STRING Folder;
   std::string Colour = "rgb(0,0,0)"; // Default colour, used for 'currentColor' references
   OBJECTPTR Viewport; // First viewport (the <svg> tag) to be created on parsing the SVG document.
   std::list<std::variant<anim_transform, anim_motion, anim_value>> Animations; // NB: Pointer stability is a container requirement
   std::map<OBJECTID, svgAnimState> Animatrix; // For animated transforms, a vector may have one matrix only.
   std::vector<std::unique_ptr<svgLink>> Links;
   std::vector<svgInherit> Inherit;
   TIMER AnimationTimer;
   WORD  Cloning;  // Incremented when inside a duplicated tag space, e.g. due to a <use> tag
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

   void applyTag(XMLTag &) noexcept;
   void applyAttribs(OBJECTPTR) const noexcept;
};

//********************************************************************************************************************

static ERR  animation_timer(extSVG *, LARGE, LARGE);
static void convert_styles(objXML::TAGS &);
static ERR  init_svg(void);
static ERR  init_rsvg(void);
static void process_attrib(extSVG *, XMLTag &, svgState &, objVector *);
static void process_children(extSVG *, svgState &, XMLTag &, OBJECTPTR);
static void process_rule(extSVG *, objXML::TAGS &, KatanaRule *);
static ERR  process_shape(extSVG *, CLASSID, svgState &, XMLTag &, OBJECTPTR, objVector * &);
static ERR  save_svg_scan(extSVG *, objXML *, objVector *, LONG);
static ERR  save_svg_defs(extSVG *, objXML *, objVectorScene *, LONG);
static ERR  save_svg_scan_std(extSVG *, objXML *, objVector *, LONG);
static ERR  save_svg_transform(VectorMatrix *, std::stringstream &);
static ERR  set_property(extSVG *, objVector *, ULONG, XMLTag &, svgState &, std::string);
static ERR  xtag_animate(extSVG *, XMLTag &, OBJECTPTR);
static ERR  xtag_animate_colour(extSVG *, XMLTag &, OBJECTPTR);
static ERR  xtag_animate_motion(extSVG *, XMLTag &, OBJECTPTR);
static ERR  xtag_animate_transform(extSVG *, XMLTag &, OBJECTPTR);
static ERR  xtag_default(extSVG *, svgState &, XMLTag &, OBJECTPTR, objVector * &);
static ERR  xtag_defs(extSVG *, svgState &, XMLTag &, OBJECTPTR);
static void xtag_group(extSVG *, svgState &, XMLTag &, OBJECTPTR, objVector * &);
static ERR  xtag_image(extSVG *, svgState &, XMLTag &, OBJECTPTR, objVector * &);
static void xtag_link(extSVG *, svgState &, XMLTag &, OBJECTPTR, objVector * &);
static void xtag_morph(extSVG *, XMLTag &, OBJECTPTR);
static ERR  xtag_set(extSVG *, XMLTag &, OBJECTPTR);
static void xtag_svg(extSVG *, svgState &, XMLTag &, OBJECTPTR, objVector * &);
static void xtag_use(extSVG *, svgState &, XMLTag &, OBJECTPTR);
static ERR  xtag_style(extSVG *, XMLTag &);
static void xtag_symbol(extSVG *, XMLTag &);

//********************************************************************************************************************

#include "funit.cpp"
#include "utility.cpp"
#include "save_svg.cpp"

//********************************************************************************************************************

static ERR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   if (objModule::load("display", &modDisplay, &DisplayBase) != ERR::Okay) return ERR::InitModule;
   if (objModule::load("vector", &modVector, &VectorBase) != ERR::Okay) return ERR::InitModule;

   if (init_svg() != ERR::Okay) return ERR::AddClass;

   if (objModule::load("picture", &modPicture) IS ERR::Okay) { // RSVG has a Picture class dependency
      if (init_rsvg() != ERR::Okay) return ERR::AddClass;
   }

   return ERR::Okay;
}

static ERR CMDExpunge(void)
{
   if (modDisplay) { FreeResource(modDisplay); modDisplay = NULL; }
   if (modVector)  { FreeResource(modVector);  modVector = NULL; }
   if (modPicture) { FreeResource(modPicture); modPicture = NULL; }

   if (clSVG)  { FreeResource(clSVG);  clSVG = NULL; }
   if (clRSVG) { FreeResource(clRSVG); clRSVG = NULL; }
   return ERR::Okay;
}

//********************************************************************************************************************

#include "class_svg.cpp"
#include "class_rsvg.cpp"

//********************************************************************************************************************

PARASOL_MOD(CMDInit, NULL, NULL, CMDExpunge, MOD_IDL, NULL)
extern "C" struct ModHeader * register_svg_module() { return &ModHeader; }
