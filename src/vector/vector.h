
class InputSubscription {
public:
   FUNCTION Callback;
   LONG Mask;
   InputSubscription(FUNCTION pCallback, LONG pMask) : Callback(pCallback), Mask(pMask) { }
};

class KeyboardSubscription {
public:
   FUNCTION Callback;
   KeyboardSubscription(FUNCTION pCallback) : Callback(pCallback) { }
};

#define SHAPE_PRIVATE \
   DOUBLE FinalX, FinalY; \
   DOUBLE FillGradientAlpha, StrokeGradientAlpha; \
   DOUBLE BX1, BY1, BX2, BY2; \
   objVectorFilter *Filter; \
   struct rkVectorViewport *ParentView; \
   STRING ID; \
   void   (*GeneratePath)(struct rkVector *); \
   agg::path_storage *BasePath; \
   agg::rasterizer_scanline_aa<> *StrokeRaster; \
   agg::rasterizer_scanline_aa<> *FillRaster; \
   agg::trans_affine *Transform; \
   struct rkVectorClip     *ClipMask; \
   struct rkVectorGradient *StrokeGradient, *FillGradient; \
   struct rkVectorImage    *FillImage, *StrokeImage; \
   struct rkVectorPattern  *FillPattern, *StrokePattern; \
   GRADIENT_TABLE *FillGradientTable, *StrokeGradientTable; \
   struct rkVectorTransition *Transition; \
   struct rkVector *Morph; \
   CSTRING FilterString, StrokeString, FillString; \
   struct DRGB StrokeColour, FillColour; \
   DOUBLE *DashArray; \
   std::vector<InputSubscription> *InputSubscriptions; \
   std::vector<KeyboardSubscription> *KeyboardSubscriptions; \
   LONG   InputMask; \
   LONG   NumericID; \
   LONG   PathLength; \
   UBYTE  MorphFlags; \
   UBYTE  FillRule; \
   UBYTE  ClipRule; \
   UBYTE  Dirty; \
   UBYTE  EnableBkgd:1; \
   UBYTE  UserHovering:1; \
   UBYTE  DisableFillColour:1; \
   agg::line_join_e  LineJoin; \
   agg::line_cap_e   LineCap; \
   agg::inner_join_e InnerJoin; \
   struct RGB8 rgbStroke, rgbFill;

class VectorEffect {
public:
   ULONG ID;         // Case sensitive hash identifier for the filter, if anything needs to reference it.
   UBYTE Type;       // Filter effect - FE_OFFSET, etc
   UBYTE Source;     // VSF_REFERENCE, VSF_GRAPHIC...
   UBYTE UsageCount; // Total number of other effects utilising this effect to build a pipeline
   struct rkBitmap *Bitmap;
   VectorEffect *Input; // The effect uses another effect as an input.
   LONG XOffset, YOffset; // In SVG only feOffset can use offsets, however in our framework any effect may define an offset when copying from a source.
   LONG DestX, DestY;
   union {
      struct {
         DOUBLE RX, RY;
      } Blur;
      struct {
         UBYTE Mode;
         class ColourMatrix *Matrix;
      } Colour;
      struct {
         class ConvolveMatrix *Matrix;
      } Convolve;
      struct {
         struct RGB8 Colour;
         DOUBLE X, Y, Width, Height;
         DOUBLE Opacity;
         LONG Dimensions;
      } Flood;
      struct {
         DOUBLE X, Y, Width, Height;
         struct rkPicture *Picture;
         LONG Dimensions;
         LONG AspectRatio;
         UBYTE ResampleMethod;
         UBYTE Units; // VUNIT
      } Image;
      struct {
         DOUBLE K1, K2, K3, K4;
         UBYTE Operator;
         UBYTE Source;
      } Composite;
      struct {
         DOUBLE FX, FY;
         struct ttable *Tables;
         LONG Octaves;
         LONG Seed;
         LONG TileWidth, TileHeight;
         UBYTE Type;
         UBYTE Stitch:1;
         LONG StitchWidth;
         LONG StitchHeight;
         LONG WrapX;
         LONG WrapY;
      } Turbulence;
      struct {
         LONG RX, RY;
         UBYTE Type;
      } Morph;
   };

   VectorEffect(LONG pType);
   ~VectorEffect();
};

typedef agg::pod_auto_array<agg::rgba8, 256> GRADIENT_TABLE;

static ERROR vecApplyPath(class SimpleVector *, struct rkVectorPath *);
static ERROR vecDrawPath(struct rkBitmap *, class SimpleVector *, DOUBLE StrokeWidth, OBJECTPTR StrokeStyle, OBJECTPTR FillStyle);
static void  vecFreePath(APTR Path);
static ERROR vecGenerateEllipse(DOUBLE X, DOUBLE Y, DOUBLE RX, DOUBLE RY, LONG Vertices, APTR *Path);
static ERROR vecGenerateRectangle(DOUBLE X, DOUBLE Y, DOUBLE Width, DOUBLE Height, APTR *Path);
static ERROR vecGeneratePath(CSTRING String, APTR *Path);
static void  vecReadPainter(OBJECTPTR, CSTRING, struct DRGB *, struct rkVectorGradient **, struct rkVectorImage **, struct rkVectorPattern **);
static void  vecTranslatePath(class SimpleVector *, DOUBLE, DOUBLE);
static void  vecMoveTo(class SimpleVector *, DOUBLE x, DOUBLE y);
static void  vecLineTo(class SimpleVector *, DOUBLE x, DOUBLE y);
static void  vecArcTo(class SimpleVector *, DOUBLE RX, DOUBLE RY, DOUBLE Angle, DOUBLE X, DOUBLE Y, LONG Flags);
static void  vecCurve3(class SimpleVector *, DOUBLE CtrlX, DOUBLE CtrlY, DOUBLE X, DOUBLE Y);
static void  vecSmooth3(class SimpleVector *, DOUBLE X, DOUBLE Y);
static void  vecCurve4(class SimpleVector *, DOUBLE CtrlX1, DOUBLE CtrlY1, DOUBLE CtrlX2, DOUBLE CtrlY2, DOUBLE X, DOUBLE Y);
static void  vecSmooth4(class SimpleVector *, DOUBLE CtrlX2, DOUBLE CtrlY2, DOUBLE X, DOUBLE Y);
static void  vecClosePath(class SimpleVector *);
static void  vecRewindPath(class SimpleVector *);
static LONG  vecGetVertex(class SimpleVector *, DOUBLE *, DOUBLE *);
