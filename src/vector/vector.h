
class InputBoundary {
public:
   OBJECTID VectorID;
   LONG Cursor; // This value buffers the Vector.Cursor field for optimisation purposes.
   DOUBLE BX1, BY1, BX2, BY2; // Collision boundary
   DOUBLE X, Y; // Absolute X,Y without collision

   InputBoundary(OBJECTID pV, LONG pC, DOUBLE p1, DOUBLE p2, DOUBLE p3, DOUBLE p4, DOUBLE p5, DOUBLE p6) :
      VectorID(pV), Cursor(pC), BX1(p1), BY1(p2), BX2(p3), BY2(p4), X(p5), Y(p6) {};
};

class InputSubscription {
public:
   FUNCTION Callback;
   LONG Mask;
   InputSubscription(FUNCTION pCallback, LONG pMask) : Callback(pCallback), Mask(pMask) { }
};

class FeedbackSubscription {
public:
   FUNCTION Callback;
   LONG Mask;
   FeedbackSubscription(FUNCTION pCallback, LONG pMask) : Callback(pCallback), Mask(pMask) { }
};

class KeyboardSubscription {
public:
   FUNCTION Callback;
   KeyboardSubscription(FUNCTION pCallback) : Callback(pCallback) { }
};

class DashedStroke {
public:
   agg::conv_dash<agg::path_storage> path;
   agg::conv_stroke<agg::conv_dash<agg::path_storage>> stroke;
   std::vector<DOUBLE> values;

   DashedStroke(agg::path_storage &pPath, LONG Elements=2) : path(pPath), stroke(path), values(Elements) { }
};

//   DOUBLE BX1, BY1, BX2, BY2;  TODO: Cache path boundaries on path generation

#define SHAPE_PRIVATE \
   DOUBLE FinalX, FinalY; \
   DOUBLE FillGradientAlpha, StrokeGradientAlpha; \
   DOUBLE StrokeWidth; \
   agg::path_storage BasePath; \
   agg::trans_affine Transform; \
   struct RGB8 rgbStroke, rgbFill; \
   objVectorFilter *Filter; \
   struct rkVectorViewport *ParentView; \
   CSTRING FilterString, StrokeString, FillString; \
   STRING ID; \
   void   (*GeneratePath)(struct rkVector *); \
   agg::rasterizer_scanline_aa<> *StrokeRaster; \
   agg::rasterizer_scanline_aa<> *FillRaster; \
   struct rkVectorClip     *ClipMask; \
   struct rkVectorGradient *StrokeGradient, *FillGradient; \
   struct rkVectorImage    *FillImage, *StrokeImage; \
   struct rkVectorPattern  *FillPattern, *StrokePattern; \
   struct rkVectorTransition *Transition; \
   struct rkVector *Morph; \
   DashedStroke *DashArray; \
   GRADIENT_TABLE *FillGradientTable, *StrokeGradientTable; \
   struct DRGB StrokeColour, FillColour; \
   std::vector<FeedbackSubscription> *FeedbackSubscriptions; \
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
   UBYTE  DisableFillColour:1; \
   UBYTE  ButtonLock:1; \
   UBYTE  RelativeStrokeWidth:1; \
   UBYTE  DisableHitTesting:1; \
   UBYTE  ResizeSubscription:1; \
   agg::line_join_e  LineJoin; \
   agg::line_cap_e   LineCap; \
   agg::inner_join_e InnerJoin; \
   DOUBLE fixed_stroke_width();

class VectorEffect {
public:
   ULONG ID;         // Case sensitive hash identifier for the filter, if anything needs to reference it.
   UBYTE Source;     // VSF_REFERENCE, VSF_GRAPHIC...
   UBYTE UsageCount; // Total number of other effects utilising this effect to build a pipeline
   UBYTE Pad;
   struct rkBitmap *Bitmap;
   ULONG InputID; // The effect uses another effect as an input (referenced by hash ID).
   LONG XOffset, YOffset; // In SVG only feOffset can use offsets, however in our framework any effect may define an offset when copying from a source.
   LONG DestX, DestY;
   ERROR Error;
   bool Blank;     // True if no graphics are produced by this effect.

   // Defined in filter.cpp
   VectorEffect();
   VectorEffect(struct rkVectorFilter *, XMLTag *);

   virtual void apply(struct rkVectorFilter *) = 0; // Required
   virtual void applyInput(VectorEffect &) { }; // Optional
   virtual ~VectorEffect() = default;
};

#define TB_NOISE 1

typedef agg::pod_auto_array<agg::rgba8, 256> GRADIENT_TABLE;

static void  vecArcTo(class SimpleVector *, DOUBLE RX, DOUBLE RY, DOUBLE Angle, DOUBLE X, DOUBLE Y, LONG Flags);
static ERROR vecApplyPath(class SimpleVector *, struct rkVectorPath *);
static void  vecClosePath(class SimpleVector *);
static void  vecCurve3(class SimpleVector *, DOUBLE CtrlX, DOUBLE CtrlY, DOUBLE X, DOUBLE Y);
static void  vecCurve4(class SimpleVector *, DOUBLE CtrlX1, DOUBLE CtrlY1, DOUBLE CtrlX2, DOUBLE CtrlY2, DOUBLE X, DOUBLE Y);
static ERROR vecDrawPath(struct rkBitmap *, class SimpleVector *, DOUBLE StrokeWidth, OBJECTPTR StrokeStyle, OBJECTPTR FillStyle);
static void  vecFreePath(APTR);
static ERROR vecGenerateEllipse(DOUBLE, DOUBLE, DOUBLE, DOUBLE, LONG, APTR *);
static ERROR vecGenerateRectangle(DOUBLE, DOUBLE, DOUBLE, DOUBLE, APTR *);
static ERROR vecGeneratePath(CSTRING, APTR *);
static LONG  vecGetVertex(class SimpleVector *, DOUBLE *, DOUBLE *);
static void  vecLineTo(class SimpleVector *, DOUBLE, DOUBLE);
static void  vecMoveTo(class SimpleVector *, DOUBLE, DOUBLE);
static ERROR vecMultiply(struct VectorMatrix *, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE);
static ERROR vecMultiplyMatrix(struct VectorMatrix *, struct VectorMatrix *);
static ERROR vecParseTransform(struct VectorMatrix *, CSTRING Commands);
static void  vecReadPainter(OBJECTPTR, CSTRING, struct DRGB *, struct rkVectorGradient **, struct rkVectorImage **, struct rkVectorPattern **);
static ERROR vecResetMatrix(struct VectorMatrix *);
static void  vecRewindPath(class SimpleVector *);
static ERROR vecRotate(struct VectorMatrix *, DOUBLE, DOUBLE, DOUBLE);
static ERROR vecScale(struct VectorMatrix *, DOUBLE, DOUBLE);
static ERROR vecSkew(struct VectorMatrix *, DOUBLE, DOUBLE);
static void  vecSmooth3(class SimpleVector *, DOUBLE, DOUBLE);
static void  vecSmooth4(class SimpleVector *, DOUBLE, DOUBLE, DOUBLE, DOUBLE);
static ERROR vecTranslate(struct VectorMatrix *, DOUBLE, DOUBLE);
static void  vecTranslatePath(class SimpleVector *, DOUBLE, DOUBLE);
