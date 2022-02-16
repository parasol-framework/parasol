
//****************************************************************************
// NB: Considered a shape (can be transformed)

typedef struct rkVectorClip {
   OBJECT_HEADER
   SHAPE_PUBLIC
   SHAPE_PRIVATE
   UBYTE *ClipData;
   agg::path_storage *ClipPath; // Internally generated path
   agg::rendering_buffer ClipRenderer;
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
   FUNCTION vpDragCallback;
   DOUBLE vpViewX, vpViewY, vpViewWidth, vpViewHeight;     // Viewbox values determine the area of the SVG content that is being sourced.  These values are always fixed pixel units.
   DOUBLE vpTargetX, vpTargetY, vpTargetXO, vpTargetYO, vpTargetWidth, vpTargetHeight; // Target dimensions
   DOUBLE vpXScale, vpYScale;                              // Scaling factors for View -to-> Target
   DOUBLE vpFixedRelX, vpFixedRelY, vpFixedWidth, vpFixedHeight; // Fixed pixel position values, relative to parent viewport
   DOUBLE vpBX1, vpBY1, vpBX2, vpBY2; // Bounding box coordinates relative to (0,0), used for clipping
   DOUBLE vpAlignX, vpAlignY;
   struct rkVectorClip *vpClipMask; // Automatically generated if the viewport is rotated or sheared.
   LONG vpDimensions;
   LONG vpAspectRatio;
   UBYTE vpDragging:1;
   UBYTE vpOverflowX, vpOverflowY;
} objVectorViewport;

//****************************************************************************

typedef struct rkVectorPoly {
   OBJECT_HEADER
   SHAPE_PUBLIC
   SHAPE_PRIVATE

   struct VectorPoint *Points;
   LONG TotalPoints;
   DOUBLE X1,Y1,X2,Y2; // Read-only, reflects the polygon boundary.
   bool Closed:1;      // Polygons are closed (TRUE) and Polylines are open (FALSE)
} objVectorPoly;

//****************************************************************************

typedef struct rkVectorPath {
   OBJECT_HEADER
   SHAPE_PUBLIC
   SHAPE_PRIVATE

   std::vector<PathCommand> Commands;
   agg::path_storage *CustomPath;
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

class TextCursor {
private:
   LONG  mColumn, mRow; // The column is the character position after taking UTF8 sequences into account.

public:
   APTR  timer;
   objVectorPoly *vector;
   LONG  flash;
   LONG  savePos;
   LONG  endColumn, endRow; // For area selections
   LONG  selectColumn, selectRow;

   TextCursor() :
      mColumn(0), mRow(0),
      timer(NULL), vector(NULL), flash(0), savePos(0),
      endColumn(0), endRow(0),
      selectColumn(0), selectRow(0) { }

   ~TextCursor() {
      if (vector) { acFree(vector); vector = NULL; }
      if (timer) { UpdateTimer(timer, 0); timer = 0; }
   }

   LONG column() { return mColumn; }
   LONG row() { return mRow; }

   void resetFlash() { flash = 0; }

   void selectedArea(struct rkVectorText *Self, LONG *Row, LONG *Column, LONG *EndRow, LONG *EndColumn) {
      if (selectRow < mRow) {
         *Row       = selectRow;
         *EndRow    = mRow;
         *Column    = selectColumn;
         *EndColumn = mColumn;
      }
      else if (selectRow IS mRow) {
         *Row       = selectRow;
         *EndRow    = mRow;
         if (selectColumn < mColumn) {
            *Column    = selectColumn;
            *EndColumn = mColumn;
         }
         else {
            *Column    = mColumn;
            *EndColumn = selectColumn;
         }
      }
      else {
         *Row       = mRow;
         *EndRow    = selectRow;
         *Column    = mColumn;
         *EndColumn = selectColumn;
      }
   }

   void move(struct rkVectorText *, LONG, LONG, bool ValidateWidth = false);
   void resetVector(struct rkVectorText *);
   void validatePosition(struct rkVectorText *);
};

class CharPos {
public:
   DOUBLE x1, y1, x2, y2;
   CharPos(DOUBLE X1, DOUBLE Y1, DOUBLE X2, DOUBLE Y2) : x1(X1), y1(Y1), x2(X2), y2(Y2) { }
};

class TextLine : public std::string {
public:
   TextLine() : std::string { } { }
   TextLine(const char *Value) : std::string{ Value } { }
   TextLine(const char *Value, int Total) : std::string{ Value, Total } { }
   TextLine(std::string Value) : std::string{ Value } { }

   std::vector<CharPos> chars;

   LONG charLength(ULONG Offset = 0) { // Total number of bytes used by the char at Offset
      return UTF8CharLength(c_str() + Offset);
   }

   LONG utf8CharOffset(ULONG Char) { // Convert a character index to its byte offset
      return UTF8CharOffset(c_str(), Char);
   }

   LONG utf8Length() { // Total number of unicode characters in the string
      return UTF8Length(c_str());
   }

   LONG lastChar() { // Return a direct offset to the start of the last character.
      return length() - UTF8PrevLength(c_str(), length());
   }

   LONG prevChar(ULONG Offset) { // Return the direct offset to a previous character.
      return Offset - UTF8PrevLength(c_str(), Offset);
   }
};

typedef struct rkVectorText {
   OBJECT_HEADER
   SHAPE_PUBLIC

   SHAPE_PRIVATE
   FUNCTION txValidateInput;
   DOUBLE txInlineSize; // Enables word-wrapping
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
   objBitmap *txAlphaBitmap; // Host for the bitmap font texture
   struct rkVectorImage *txBitmapImage;
   FT_Size FreetypeSize;
   std::vector<TextLine> txLines;
   TextCursor txCursor;
   CSTRING txFamily;
   APTR    txKeyEvent;
   OBJECTID txFocusID;
   OBJECTID txShapeInsideID;   // Enable word-wrapping within this shape
   OBJECTID txShapeSubtractID; // Subtract this shape from the path defined by shape-inside
   LONG  txTotalLines;
   LONG  txLineLimit, txCharLimit;
   LONG  txTotalRotate, txTotalDX, txTotalDY;
   LONG  txWeight; // 100 - 300 (Light), 400 (Normal), 700 (Bold), 900 (Boldest)
   LONG  txAlignFlags;
   LONG  txFlags; // VTF flags
   char  txFontStyle[20];
   UBYTE txRelativeFontSize;
   bool txXRelative:1;
   bool txYRelative:1;
// bool txSpacingAndGlyphs:1;
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
   struct VectorMatrix Matrix;
   agg::trans_affine *AGGTransform;
};

typedef struct rkVectorTransition {
   OBJECT_HEADER
   LONG TotalStops; // Total number of stops registered.

#ifdef PRV_VECTOR
   struct TransitionStop Stops[MAX_TRANSITION_STOPS];
   bool Dirty:1;
#endif
} objVectorTransition;

//****************************************************************************

class GradientColours {
   public:
      GradientColours(struct rkVectorGradient *, DOUBLE);
      GRADIENT_TABLE table;
};
