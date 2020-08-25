/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

*****************************************************************************/

#define PRV_VECTOR
#define PRV_VECTORSCENE
#define PRV_VECTORPATTERN
#define PRV_VECTORGRADIENT
#define PRV_VECTORFILTER
#define PRV_VECTORPATH
#define PRV_VECTOR_MODULE
#define DBG_TRANSFORM(args...) //MSG(args)

#include "agg_alpha_mask_u8.h"
#include "agg_basics.h"
#include "agg_bounding_rect.h"
#include "agg_curves.h"
#include "agg_conv_stroke.h"
#include "agg_conv_dash.h"
#include "agg_conv_contour.h"
//#include "agg_conv_marker.h"
#include "agg_conv_smooth_poly1.h"
#include "agg_conv_transform.h"
#include "agg_curves.h"
#include "agg_gamma_lut.h"
#include "agg_image_accessors.h"
#include "agg_path_storage.h"
#include "agg_pattern_filters_rgba.h"
#include "agg_pixfmt_gray.h"
#include "agg_pixfmt_rgba.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_rasterizer_outline_aa.h"
#include "agg_renderer_base.h"
#include "agg_renderer_scanline.h"
#include "agg_renderer_outline_aa.h"
#include "agg_renderer_outline_image.h"
#include "agg_rendering_buffer.h"
#include "agg_scanline_p.h"
#include "agg_scanline_u.h"
#include "agg_span_allocator.h"
#include "agg_span_converter.h"
#include "agg_span_image_filter_rgba.h"
#include "agg_span_gradient.h"
#include "agg_span_gradient_contour.h"
#include "agg_span_interpolator_linear.h"
#include "agg_trans_affine.h"
//#include "agg_vcgen_markers_term.h"

#include <parasol/main.h>
#include <parasol/modules/xml.h>
#include "vector.h"
#include <parasol/modules/picture.h>

#include <parasol/modules/display.h>
#include <parasol/modules/font.h>
#include <parasol/modules/vector.h>

#include <math.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <float.h>

#include <ft2build.h>
#include <freetype/freetype.h>

#include <array>
#include <memory>

#include "vectors/vector.h"
#include "idl.h"

#define FIXED_DPI 96 // Freetype measurements are based on this DPI.
#define FT_DOWNSIZE 6
#define FT_UPSIZE 6

struct CoreBase *CoreBase;
static struct DisplayBase *DisplayBase;
static struct FontBase *FontBase;

static OBJECTPTR clVectorScene = NULL, clVectorViewport = NULL, clVectorGroup = NULL, clVectorColour = NULL;
static OBJECTPTR clVectorEllipse = NULL, clVectorRectangle = NULL, clVectorPath = NULL, clVectorWave = NULL;
static OBJECTPTR clVectorFilter = NULL, clVectorPolygon = NULL, clVectorText = NULL, clVectorClip = NULL;
static OBJECTPTR clVectorGradient = NULL, clVectorImage = NULL, clVectorPattern = NULL, clVector = NULL;
static OBJECTPTR clVectorSpiral = NULL, clVectorShape = NULL, clVectorTransition = NULL;

static OBJECTPTR modDisplay = NULL;
static OBJECTPTR modFont = NULL;

#define DEG2RAD 0.0174532925 // Multiple any angle by this value to convert to radians

#include "colours.cpp"

static ERROR init_clip(void);
static ERROR init_colour(void);
static ERROR init_filter(void);
static ERROR init_ellipse(void);
static ERROR init_gradient(void);
static ERROR init_pattern(void);
static ERROR init_group(void);
static ERROR init_image(void);
static ERROR init_path(void);
static ERROR init_polygon(void);
static ERROR init_rectangle(void);
static ERROR init_spiral(void);
static ERROR init_supershape(void);
static ERROR init_text(void);
static ERROR init_transition(void);
static ERROR init_vectorscene(void);
static ERROR init_vector(void);
static ERROR init_viewport(void);
static ERROR init_wave(void);

static void get_ellipse_xy(struct rkVectorEllipse *);
static void get_rectangle_xy(struct rkVectorRectangle *);
static void get_spiral_xy(struct rkVectorSpiral *);
static void get_super_xy(struct rkVectorShape *);
static void get_text_xy(struct rkVectorText *);
static void get_wave_xy(struct rkVectorWave *);

static struct VectorTransform * add_transform(objVector *, LONG Type, LONG Create);
static void apply_parent_transforms(objVector *, objVector *, agg::trans_affine &, WORD *);
static void apply_transforms(struct VectorTransform *, DOUBLE, DOUBLE, agg::trans_affine &, WORD *);
static void convert_to_aggpath(struct PathCommand *Paths, LONG TotalPoints, agg::path_storage *BasePath);
static void gen_vector_path(objVector *);
static GRADIENT_TABLE * get_fill_gradient_table(objVector &, DOUBLE);
static GRADIENT_TABLE * get_stroke_gradient_table(objVector &);
static CSTRING read_numseq(CSTRING Value, ...);
static ERROR read_path(struct PathCommand **, LONG *, CSTRING);
static void apply_transition(objVectorTransition *, DOUBLE, agg::trans_affine &);
static void apply_transition_xy(objVectorTransition *, DOUBLE, DOUBLE *X, DOUBLE *Y);

FT_Error (*EFT_Set_Pixel_Sizes)(FT_Face, FT_UInt pixel_width, FT_UInt pixel_height );
FT_Error (*EFT_Set_Char_Size)(FT_Face, FT_F26Dot6 char_width, FT_F26Dot6 char_height, FT_UInt horz_resolution, FT_UInt vert_resolution );
FT_Error (*EFT_Get_Kerning)(FT_Face, FT_UInt left_glyph, FT_UInt right_glyph, FT_UInt kern_mode, FT_Vector *akerning);
FT_Error (*EFT_Get_Char_Index)(FT_Face, FT_ULong charcode);
FT_Error (*EFT_Load_Glyph)(FT_Face, FT_UInt glyph_index, FT_Int32  load_flags);

#include "utility.cpp"

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   if (LoadModule("display", MODVERSION_DISPLAY, &modDisplay, &DisplayBase)) return ERR_InitModule;
   if (LoadModule("font", MODVERSION_FONT, &modFont, &FontBase)) return ERR_InitModule;

   if (modResolveSymbol(modFont, "FT_Set_Pixel_Sizes", (APTR *)&EFT_Set_Pixel_Sizes)) return ERR_ResolveSymbol;
   if (modResolveSymbol(modFont, "FT_Set_Char_Size", (APTR *)&EFT_Set_Char_Size)) return ERR_ResolveSymbol;
   if (modResolveSymbol(modFont, "FT_Get_Kerning", (APTR *)&EFT_Get_Kerning)) return ERR_ResolveSymbol;
   if (modResolveSymbol(modFont, "FT_Get_Char_Index", (APTR *)&EFT_Get_Char_Index)) return ERR_ResolveSymbol;
   if (modResolveSymbol(modFont, "FT_Load_Glyph", (APTR *)&EFT_Load_Glyph)) return ERR_ResolveSymbol;

   FID_FreetypeFace = StrHash("FreetypeFace", FALSE);

   ERROR error;
   if ((error = init_vectorscene())) return error; // Base class
   if ((error = init_vector())) return error;
   if ((error = init_colour())) return error;
   if ((error = init_clip())) return error;
   if ((error = init_ellipse())) return error;
   if ((error = init_filter())) return error;
   if ((error = init_gradient())) return error;
   if ((error = init_group())) return error;
   if ((error = init_image())) return error;
   if ((error = init_spiral())) return error;
   if ((error = init_supershape())) return error;
   if ((error = init_path())) return error;
   if ((error = init_pattern())) return error;
   if ((error = init_polygon())) return error;
   if ((error = init_text())) return error;
   if ((error = init_rectangle())) return error;
   if ((error = init_transition())) return error;
   if ((error = init_viewport())) return error;
   if ((error = init_wave())) return error;

   return error;
}

ERROR CMDExpunge(void)
{
   if (modDisplay)    { acFree(modDisplay); modDisplay = NULL; }
   if (modFont)       { acFree(modFont); modFont = NULL; }

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

   return ERR_Okay;
}

//****************************************************************************

#include "paths.cpp"
#include "scene/scene_pixels.cpp"
#include "vector_functions.cpp"
#include "scene/scene_draw.cpp"
#include "scene/scene.cpp"

#include "vectors/vector.cpp"
#include "vectors/viewport.c"
#include "vectors/clip.c"
#include "vectors/path.c"
#include "vectors/text.c"
#include "vectors/group.c"
#include "vectors/ellipse.c"
#include "vectors/polygon.c"
#include "vectors/rectangle.c"
#include "vectors/spiral.cpp"
#include "vectors/supershape.cpp"
#include "vectors/wave.cpp"

#include "filters/filter.cpp"

#include "defs/colour.cpp"
#include "defs/gradient.cpp"
#include "defs/image.cpp"
#include "defs/pattern.cpp"
#include "defs/transition.cpp"

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MODVERSION_VECTOR)
