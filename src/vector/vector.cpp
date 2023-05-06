/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

*********************************************************************************************************************/

//#include "vector.h"
#include "idl.h"

struct CoreBase *CoreBase;
struct DisplayBase *DisplayBase;
struct FontBase *FontBase;

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

std::recursive_mutex glFocusLock;
std::vector<extVector *> glFocusList; // The first reference is the most foreground object with the focus

static ERROR init_clip(void);
static ERROR init_ellipse(void);
static ERROR init_group(void);
static ERROR init_path(void);
static ERROR init_polygon(void);
static ERROR init_rectangle(void);
static ERROR init_spiral(void);
static ERROR init_supershape(void);
static ERROR init_text(void);
static ERROR init_vector(void);
static ERROR init_viewport(void);
static ERROR init_wave(void);

#include "utility.cpp"

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   if (objModule::load("display", MODVERSION_DISPLAY, &modDisplay, &DisplayBase)) return ERR_InitModule;
   if (objModule::load("font", MODVERSION_FONT, &modFont, &FontBase)) return ERR_InitModule;

   ERROR error;
   if ((error = init_vectorscene())) return error; // Base class
   if ((error = init_vector())) return error;
   if ((error = init_colour())) return error;
   if ((error = init_clip())) return error;
   if ((error = init_filter())) return error;
   if ((error = init_gradient())) return error;
   if ((error = init_group())) return error;
   if ((error = init_image())) return error;
   // Shapes
   if ((error = init_path())) return error;
   if ((error = init_ellipse())) return error;
   if ((error = init_spiral())) return error;
   if ((error = init_supershape())) return error;
   if ((error = init_pattern())) return error;
   if ((error = init_polygon())) return error;
   if ((error = init_text())) return error;
   if ((error = init_rectangle())) return error;
   if ((error = init_transition())) return error;
   if ((error = init_viewport())) return error;
   if ((error = init_wave())) return error;
   // Effects
   if ((error = init_filtereffect())) return error;
   if ((error = init_imagefx())) return error;
   if ((error = init_sourcefx())) return error;
   if ((error = init_blurfx())) return error;
   if ((error = init_colourfx())) return error;
   if ((error = init_compositefx())) return error;
   if ((error = init_convolvefx())) return error;
   if ((error = init_floodfx())) return error;
   if ((error = init_mergefx())) return error;
   if ((error = init_morphfx())) return error;
   if ((error = init_offsetfx())) return error;
   if ((error = init_turbulencefx())) return error;
   if ((error = init_remapfx())) return error;
   if ((error = init_lightingfx())) return error;
   if ((error = init_displacementfx())) return error;
   return error;
}

ERROR CMDExpunge(void)
{
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
   return ERR_Okay;
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

extern ERROR CMDOpen(OBJECTPTR Module);

static STRUCTS glStructures = {
   { "GradientStop", sizeof(GradientStop) },
   { "MergeSource",  sizeof(MergeSource) },
   { "PathCommand",  sizeof(PathCommand) },
   { "Transition",   sizeof(Transition) },
   { "VectorMatrix", sizeof(VectorMatrix) },
   { "VectorPoint",  sizeof(VectorPoint) }
};

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MODVERSION_VECTOR, MOD_IDL, &glStructures)
extern "C" struct ModHeader * register_vector_module() { return &ModHeader; }
