
//****************************************************************************
// NB: Considered a shape (can be transformed)

typedef struct rkVectorClip {
   OBJECT_HEADER
   SHAPE_PUBLIC
   SHAPE_PRIVATE
   UBYTE *ClipData;
   agg::path_storage *ClipPath; // Internally generated path
   agg::rendering_buffer *ClipRenderer;
   struct rkVector *TargetVector;
   LONG ClipUnits;
   LONG ClipSize;
} objVectorClip;

//****************************************************************************
// NB: Considered a shape (can be transformed).

typedef struct rkVectorViewport {
   OBJECT_HEADER
   SHAPE_PUBLIC
   SHAPE_PRIVATE
   DOUBLE vpViewX, vpViewY, vpViewWidth, vpViewHeight;     // Viewbox values determine the area of the SVG content that is being sourced.  These values are always fixed pixel units.
   DOUBLE vpTargetX, vpTargetY, vpTargetWidth, vpTargetHeight; // Target X,Y,Width,Height
   DOUBLE vpXScale, vpYScale;                              // Scaling factors for View -to-> Target
   DOUBLE vpFixedRelX, vpFixedRelY, vpFixedWidth, vpFixedHeight; // Fixed pixel position values, relative to parent viewport
   DOUBLE vpBX1, vpBY1, vpBX2, vpBY2; // Bounding box coordinates relative to (0,0), used for clipping
   DOUBLE vpAlignX, vpAlignY;
   struct rkVectorClip *vpClipMask; // Automatically generated if the viewport is rotated or sheared.
   LONG vpDimensions;
   LONG vpAspectRatio;
} objVectorViewport;

//****************************************************************************

typedef struct rkVectorPoly {
   OBJECT_HEADER
   SHAPE_PUBLIC
   SHAPE_PRIVATE

   struct VectorPoint *Points;
   LONG TotalPoints;
   DOUBLE X1,Y1,X2,Y2; // Read-only, reflects the polygon boundary.
   UBYTE Closed:1; // Polygons are closed (TRUE) and Polylines are open (FALSE)
} objVectorPoly;

//****************************************************************************

typedef struct rkVectorPath {
   OBJECT_HEADER
   SHAPE_PUBLIC
   SHAPE_PRIVATE

   struct PathCommand *Commands;
   agg::path_storage *CustomPath;
   LONG TotalCommands;
   LONG Capacity;
} objVectorPath;

//****************************************************************************

enum { // Filter effects
   FE_BLEND=1,
   FE_COLOURMATRIX,
   FE_COMPONENTTRANSFER,
   FE_COMPOSITE,
   FE_CONVOLVEMATRIX,
   FE_DIFFUSELIGHTING,
   FE_DISPLACEMENTMAP,
   FE_FLOOD,
   FE_BLUR,
   FE_IMAGE,
   FE_MERGE,
   FE_MORPHOLOGY,
   FE_OFFSET,
   FE_SPECULARLIGHTING,
   FE_TILE,
   FE_TURBULENCE,
   FE_DISTANTLIGHT,
   FE_POINTLIGHT,
   FE_SPOTLIGHT
};

//****************************************************************************

typedef struct rkVectorText {
   OBJECT_HEADER
   SHAPE_PUBLIC

   SHAPE_PRIVATE
   DOUBLE txX, txY;
   DOUBLE txTextLength;
   DOUBLE txFontSize;  // Font size measured in pixels.  Multiply by 3/4 to convert to point size.
   DOUBLE txKerning;
   DOUBLE txLetterSpacing;
   DOUBLE txWidth; // Width of the text computed by path generation.  Not for client use as GetBoundary() can be used for that.
   DOUBLE txStartOffset;
   DOUBLE txSpacing;
   DOUBLE *txDX, *txDY; // A series of spacing adjustments that apply on a per-character level.
   DOUBLE *txRotate;  // A series of angles that will rotate each individual character.
   struct rkFont *txFont;
   FT_Size FreetypeSize;
   CSTRING txString;
   CSTRING txFamily;
   LONG  txTotalRotate, txTotalDX, txTotalDY;
   LONG  txWeight; // 100 - 300 (Light), 400 (Normal), 700 (Bold), 900 (Boldest)
   LONG  txAlignFlags;
   LONG  txFlags; // VTF flags
   UBYTE txRelativeFontSize;
   UBYTE txXRelative:1;
   UBYTE txYRelative:1;
// UBYTE txSpacingAndGlyphs:1;
} objVectorText;

//****************************************************************************

typedef struct rkVectorRectangle {
   OBJECT_HEADER
   SHAPE_PUBLIC
   SHAPE_PRIVATE
   DOUBLE rX, rY;
   DOUBLE rWidth, rHeight;
   DOUBLE rRoundX, rRoundY;
   LONG   rDimensions;
} objVectorRectangle;

//****************************************************************************

#define MAX_TRANSITION_STOPS 10

struct TransitionStop { // Passed to the Stops field.
   DOUBLE Offset;
   struct VectorTransform *Transforms;
   agg::trans_affine *AGGTransform;
};

typedef struct rkVectorTransition {
   OBJECT_HEADER
   LONG TotalStops; // Total number of stops registered.

#ifdef PRV_VECTOR
   struct TransitionStop Stops[MAX_TRANSITION_STOPS];
   UBYTE Dirty:1;
#endif
} objVectorTransition;

//****************************************************************************

class GradientColours {
   public:
      GradientColours(struct rkVectorGradient *, DOUBLE);
      GRADIENT_TABLE table;
};
