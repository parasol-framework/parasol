/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

This code utilises the work of the FreeType Project under the FreeType License.  For more information please refer to
the FreeType home page at www.freetype.org.

*********************************************************************************************************************/

//#include "vector.h"
//#include "font.h"
#include "idl.h"

JUMPTABLE_DISPLAY
JUMPTABLE_CORE
JUMPTABLE_FONT

OBJECTPTR clVectorScene = NULL, clVectorViewport = NULL, clVectorGroup = NULL, clVectorColour = NULL;
OBJECTPTR clVectorEllipse = NULL, clVectorRectangle = NULL, clVectorPath = NULL, clVectorWave = NULL;
OBJECTPTR clVectorFilter = NULL, clVectorPolygon = NULL, clVectorText = NULL, clVectorClip = NULL;
OBJECTPTR clVectorGradient = NULL, clVectorImage = NULL, clVectorPattern = NULL, clVector = NULL;
OBJECTPTR clVectorSpiral = NULL, clVectorShape = NULL, clVectorTransition = NULL, clImageFX = NULL;
OBJECTPTR clBlurFX = NULL, clColourFX = NULL, clCompositeFX = NULL, clConvolveFX = NULL, clFilterEffect = NULL;
OBJECTPTR clFloodFX = NULL, clMergeFX = NULL, clMorphologyFX = NULL, clOffsetFX = NULL, clTurbulenceFX = NULL;
OBJECTPTR clSourceFX = NULL, clRemapFX = NULL, clLightingFX = NULL, clDisplacementFX = NULL;

static OBJECTPTR modDisplay = NULL;
static OBJECTPTR modFont = NULL;
OBJECTPTR glVectorModule = NULL;
FT_Library glFTLibrary = NULL;

std::recursive_mutex glVectorFocusLock;
std::vector<extVector *> glVectorFocusList; // The first reference is the most foreground object with the focus

std::recursive_mutex glFontMutex;
std::unordered_map<ULONG, bmp_font> glBitmapFonts;
std::unordered_map<ULONG, freetype_font> glFreetypeFonts;

static ERR init_clip(void);
static ERR init_ellipse(void);
static ERR init_group(void);
static ERR init_path(void);
static ERR init_polygon(void);
static ERR init_rectangle(void);
static ERR init_spiral(void);
static ERR init_supershape(void);
static ERR init_text(void);
static ERR init_vector(void);
static ERR init_viewport(void);
static ERR init_wave(void);

#include "utility.cpp"

static ERR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   pf::Log log;

   CoreBase = argCoreBase;

   argModule->getPtr(FID_Root, &glVectorModule);

   if (FT_Init_FreeType(&glFTLibrary)) {
      log.warning("Failed to initialise the FreeType font library.");
      return ERR::Failed;
   }

   if (objModule::load("display", &modDisplay, &DisplayBase) != ERR::Okay) return ERR::InitModule;
   if (objModule::load("font", &modFont, &FontBase) != ERR::Okay) return ERR::InitModule;

   ERR error;
   if ((error = init_vectorscene()) != ERR::Okay) return error; // Base class
   if ((error = init_vector()) != ERR::Okay) return error;
   if ((error = init_colour()) != ERR::Okay) return error;
   if ((error = init_clip()) != ERR::Okay) return error;
   if ((error = init_filter()) != ERR::Okay) return error;
   if ((error = init_gradient()) != ERR::Okay) return error;
   if ((error = init_group()) != ERR::Okay) return error;
   if ((error = init_image()) != ERR::Okay) return error;
   // Shapes
   if ((error = init_path()) != ERR::Okay) return error;
   if ((error = init_ellipse()) != ERR::Okay) return error;
   if ((error = init_spiral()) != ERR::Okay) return error;
   if ((error = init_supershape()) != ERR::Okay) return error;
   if ((error = init_pattern()) != ERR::Okay) return error;
   if ((error = init_polygon()) != ERR::Okay) return error;
   if ((error = init_text()) != ERR::Okay) return error;
   if ((error = init_rectangle()) != ERR::Okay) return error;
   if ((error = init_transition()) != ERR::Okay) return error;
   if ((error = init_viewport()) != ERR::Okay) return error;
   if ((error = init_wave()) != ERR::Okay) return error;
   // Effects
   if ((error = init_filtereffect()) != ERR::Okay) return error;
   if ((error = init_imagefx()) != ERR::Okay) return error;
   if ((error = init_sourcefx()) != ERR::Okay) return error;
   if ((error = init_blurfx()) != ERR::Okay) return error;
   if ((error = init_colourfx()) != ERR::Okay) return error;
   if ((error = init_compositefx()) != ERR::Okay) return error;
   if ((error = init_convolvefx()) != ERR::Okay) return error;
   if ((error = init_floodfx()) != ERR::Okay) return error;
   if ((error = init_mergefx()) != ERR::Okay) return error;
   if ((error = init_morphfx()) != ERR::Okay) return error;
   if ((error = init_offsetfx()) != ERR::Okay) return error;
   if ((error = init_turbulencefx()) != ERR::Okay) return error;
   if ((error = init_remapfx()) != ERR::Okay) return error;
   if ((error = init_lightingfx()) != ERR::Okay) return error;
   if ((error = init_displacementfx()) != ERR::Okay) return error;

   update_dpi();
   return error;
}

static ERR CMDExpunge(void)
{
   glBitmapFonts.clear();
   glFreetypeFonts.clear();

   if (glFTLibrary) { FT_Done_FreeType(glFTLibrary); glFTLibrary = NULL; }

   if (modDisplay) { FreeResource(modDisplay); modDisplay = NULL; }
   if (modFont)    { FreeResource(modFont); modFont = NULL; }

   if (clVectorShape)      { FreeResource(clVectorShape);      clVectorShape = NULL; }
   if (clVectorSpiral)     { FreeResource(clVectorSpiral);     clVectorSpiral = NULL; }
   if (clVectorScene)      { FreeResource(clVectorScene);      clVectorScene = NULL; }
   if (clVector)           { FreeResource(clVector);           clVector = NULL; }
   if (clVectorClip)       { FreeResource(clVectorClip);       clVectorClip = NULL; }
   if (clVectorColour)     { FreeResource(clVectorColour);     clVectorColour = NULL; }
   if (clVectorRectangle)  { FreeResource(clVectorRectangle);  clVectorRectangle = NULL; }
   if (clVectorEllipse)    { FreeResource(clVectorEllipse);    clVectorEllipse = NULL; }
   if (clVectorPath)       { FreeResource(clVectorPath);       clVectorPath = NULL; }
   if (clVectorPolygon)    { FreeResource(clVectorPolygon);    clVectorPolygon = NULL; }
   if (clVectorText)       { FreeResource(clVectorText);       clVectorText = NULL; }
   if (clVectorGradient)   { FreeResource(clVectorGradient);   clVectorGradient = NULL; }
   if (clVectorGroup)      { FreeResource(clVectorGroup);      clVectorGroup = NULL; }
   if (clVectorViewport)   { FreeResource(clVectorViewport);   clVectorViewport = NULL; }
   if (clVectorPattern)    { FreeResource(clVectorPattern);    clVectorPattern = NULL; }
   if (clVectorFilter)     { FreeResource(clVectorFilter);     clVectorFilter = NULL; }
   if (clVectorImage)      { FreeResource(clVectorImage);      clVectorImage = NULL; }
   if (clVectorWave)       { FreeResource(clVectorWave);       clVectorWave = NULL; }
   if (clVectorTransition) { FreeResource(clVectorTransition); clVectorTransition = NULL; }
   if (clFilterEffect)     { FreeResource(clFilterEffect);     clFilterEffect = NULL; }
   if (clImageFX)          { FreeResource(clImageFX);          clImageFX = NULL; }
   if (clSourceFX)         { FreeResource(clSourceFX);         clSourceFX = NULL; }
   if (clBlurFX)           { FreeResource(clBlurFX);           clBlurFX = NULL; }
   if (clColourFX)         { FreeResource(clColourFX);         clColourFX = NULL; }
   if (clCompositeFX)      { FreeResource(clCompositeFX);      clCompositeFX = NULL; }
   if (clConvolveFX)       { FreeResource(clConvolveFX);       clConvolveFX = NULL; }
   if (clFloodFX)          { FreeResource(clFloodFX);          clFloodFX = NULL; }
   if (clMergeFX)          { FreeResource(clMergeFX);          clMergeFX = NULL; }
   if (clMorphologyFX)     { FreeResource(clMorphologyFX);     clMorphologyFX = NULL; }
   if (clOffsetFX)         { FreeResource(clOffsetFX);         clOffsetFX = NULL; }
   if (clTurbulenceFX)     { FreeResource(clTurbulenceFX);     clTurbulenceFX = NULL; }
   if (clRemapFX)          { FreeResource(clRemapFX);          clRemapFX = NULL; }
   if (clLightingFX)       { FreeResource(clLightingFX);       clLightingFX = NULL; }
   if (clDisplacementFX)   { FreeResource(clDisplacementFX);   clDisplacementFX = NULL; }
   return ERR::Okay;
}

//********************************************************************************************************************

#include "paths.cpp"

#include "vectors/vector.cpp"
#include "vectors/viewport.cpp"
#include "vectors/clip.cpp"
#include "vectors/path.cpp"
#include "vectors/text.cpp"
#include "vectors/group.cpp"
#include "vectors/ellipse.cpp"
#include "vectors/polygon.cpp"
#include "vectors/rectangle.cpp"
#include "vectors/spiral.cpp"
#include "vectors/supershape.cpp"
#include "vectors/wave.cpp"

//********************************************************************************************************************

extern ERR CMDOpen(OBJECTPTR Module);

static STRUCTS glStructures = {
   { "GradientStop", sizeof(GradientStop) },
   { "MergeSource",  sizeof(MergeSource) },
   { "PathCommand",  sizeof(PathCommand) },
   { "Transition",   sizeof(Transition) },
   { "VectorMatrix", sizeof(VectorMatrix) },
   { "VectorPoint",  sizeof(VectorPoint) }
};

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MOD_IDL, &glStructures)
extern "C" struct ModHeader * register_vector_module() { return &ModHeader; }
