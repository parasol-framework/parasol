/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

*****************************************************************************/

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
OBJECTPTR clSourceFX = NULL, clRemapFX = NULL, clLightingFX = NULL;

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

   if (LoadModule("display", MODVERSION_DISPLAY, &modDisplay, &DisplayBase)) return ERR_InitModule;
   if (LoadModule("font", MODVERSION_FONT, &modFont, &FontBase)) return ERR_InitModule;

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
   return error;
}

ERROR CMDExpunge(void)
{
   if (modDisplay) { acFree(modDisplay); modDisplay = NULL; }
   if (modFont)    { acFree(modFont); modFont = NULL; }

   if (clVectorShape)      { acFree(clVectorShape);      clVectorShape = NULL; }
   if (clVectorSpiral)     { acFree(clVectorSpiral);     clVectorSpiral = NULL; }
   if (clVectorScene)      { acFree(clVectorScene);      clVectorScene = NULL; }
   if (clVector)           { acFree(clVector);           clVector = NULL; }
   if (clVectorClip)       { acFree(clVectorClip);       clVectorClip = NULL; }
   if (clVectorColour)     { acFree(clVectorColour);     clVectorColour = NULL; }
   if (clVectorRectangle)  { acFree(clVectorRectangle);  clVectorRectangle = NULL; }
   if (clVectorEllipse)    { acFree(clVectorEllipse);    clVectorEllipse = NULL; }
   if (clVectorPath)       { acFree(clVectorPath);       clVectorPath = NULL; }
   if (clVectorPolygon)    { acFree(clVectorPolygon);    clVectorPolygon = NULL; }
   if (clVectorText)       { acFree(clVectorText);       clVectorText = NULL; }
   if (clVectorGradient)   { acFree(clVectorGradient);   clVectorGradient = NULL; }
   if (clVectorGroup)      { acFree(clVectorGroup);      clVectorGroup = NULL; }
   if (clVectorViewport)   { acFree(clVectorViewport);   clVectorViewport = NULL; }
   if (clVectorPattern)    { acFree(clVectorPattern);    clVectorPattern = NULL; }
   if (clVectorFilter)     { acFree(clVectorFilter);     clVectorFilter = NULL; }
   if (clVectorImage)      { acFree(clVectorImage);      clVectorImage = NULL; }
   if (clVectorWave)       { acFree(clVectorWave);       clVectorWave = NULL; }
   if (clVectorTransition) { acFree(clVectorTransition); clVectorTransition = NULL; }
   if (clFilterEffect)     { acFree(clFilterEffect);     clFilterEffect = NULL; }
   if (clImageFX)          { acFree(clImageFX);          clImageFX = NULL; }
   if (clSourceFX)         { acFree(clSourceFX);         clSourceFX = NULL; }
   if (clBlurFX)           { acFree(clBlurFX);           clBlurFX = NULL; }
   if (clColourFX)         { acFree(clColourFX);         clColourFX = NULL; }
   if (clCompositeFX)      { acFree(clCompositeFX);      clCompositeFX = NULL; }
   if (clConvolveFX)       { acFree(clConvolveFX);       clConvolveFX = NULL; }
   if (clFloodFX)          { acFree(clFloodFX);          clFloodFX = NULL; }
   if (clMergeFX)          { acFree(clMergeFX);          clMergeFX = NULL; }
   if (clMorphologyFX)     { acFree(clMorphologyFX);     clMorphologyFX = NULL; }
   if (clOffsetFX)         { acFree(clOffsetFX);         clOffsetFX = NULL; }
   if (clTurbulenceFX)     { acFree(clTurbulenceFX);     clTurbulenceFX = NULL; }
   if (clRemapFX)          { acFree(clRemapFX);          clRemapFX = NULL; }
   if (clLightingFX)       { acFree(clLightingFX);       clLightingFX = NULL; }
   return ERR_Okay;
}

//****************************************************************************

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

//****************************************************************************

extern ERROR CMDOpen(OBJECTPTR Module);
PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MODVERSION_VECTOR)
