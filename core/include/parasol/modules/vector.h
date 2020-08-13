#ifndef MODULES_VECTOR
#define MODULES_VECTOR 1

// Name:      vector.h
// Copyright: Paul Manias © 2010-2017
// Generator: idl-c

#ifndef MAIN_H
#include <parasol/main.h>
#endif

#define MODVERSION_VECTOR (1)

#ifndef MODULES_DISPLAY_H
#include <parasol/modules/display.h>
#endif

// Options for drawing arcs.

#define ARC_LARGE 0x00000001
#define ARC_SWEEP 0x00000002

#define VUNIT_UNDEFINED 0
#define VUNIT_BOUNDING_BOX 1
#define VUNIT_USERSPACE 2
#define VUNIT_END 3

// Spread method options define the method to use for tiling filled graphics.

#define VSPREAD_UNDEFINED 0
#define VSPREAD_PAD 1
#define VSPREAD_REFLECT 2
#define VSPREAD_REPEAT 3
#define VSPREAD_REFLECT_X 4
#define VSPREAD_REFLECT_Y 5
#define VSPREAD_CLIP 6
#define VSPREAD_END 7

#define PE_Move 1
#define PE_MoveRel 2
#define PE_Line 3
#define PE_LineRel 4
#define PE_HLine 5
#define PE_HLineRel 6
#define PE_VLine 7
#define PE_VLineRel 8
#define PE_Curve 9
#define PE_CurveRel 10
#define PE_Smooth 11
#define PE_SmoothRel 12
#define PE_QuadCurve 13
#define PE_QuadCurveRel 14
#define PE_QuadSmooth 15
#define PE_QuadSmoothRel 16
#define PE_Arc 17
#define PE_ArcRel 18
#define PE_ClosePath 19

// Vector fill rules for the FillRule field in the Vector class.

#define VFR_NON_ZERO 1
#define VFR_EVEN_ODD 2
#define VFR_INHERIT 3
#define VFR_END 4

// Options for the Vector class' Visibility field.

#define VIS_HIDDEN 0
#define VIS_VISIBLE 1
#define VIS_COLLAPSE 2
#define VIS_INHERIT 3

// Options for the look of line joins.

#define VLJ_MITER 0
#define VLJ_MITER_REVERT 1
#define VLJ_ROUND 2
#define VLJ_BEVEL 3
#define VLJ_MITER_ROUND 4
#define VLJ_INHERIT 5

// Line-cap options.

#define VLC_BUTT 1
#define VLC_SQUARE 2
#define VLC_ROUND 3
#define VLC_INHERIT 4

// Inner join options for angled lines.

#define VIJ_BEVEL 1
#define VIJ_MITER 2
#define VIJ_JAG 3
#define VIJ_ROUND 4
#define VIJ_INHERIT 5

// VectorGradient options.

#define VGT_LINEAR 0
#define VGT_RADIAL 1
#define VGT_CONIC 2
#define VGT_DIAMOND 3
#define VGT_CONTOUR 4

// Options for stretching text in VectorText.

#define VTS_INHERIT 0
#define VTS_NORMAL 1
#define VTS_WIDER 2
#define VTS_NARROWER 3
#define VTS_ULTRA_CONDENSED 4
#define VTS_EXTRA_CONDENSED 5
#define VTS_CONDENSED 6
#define VTS_SEMI_CONDENSED 7
#define VTS_EXPANDED 8
#define VTS_SEMI_EXPANDED 9
#define VTS_ULTRA_EXPANDED 10
#define VTS_EXTRA_EXPANDED 11

// VectorText flags.

#define VTXF_UNDERLINE 0x00000001
#define VTXF_OVERLINE 0x00000002
#define VTXF_LINE_THROUGH 0x00000004
#define VTXF_BLINK 0x00000008

// Types of vector transforms.

#define VTF_MATRIX 0x0001
#define VTF_TRANSLATE 0x0002
#define VTF_SCALE 0x0004
#define VTF_ROTATE 0x0008
#define VTF_SKEW 0x0010

// Morph flags

#define VMF_STRETCH 0x00000001
#define VMF_AUTO_SPACING 0x00000002
#define VMF_X_MIN 0x00000004
#define VMF_X_MID 0x00000008
#define VMF_X_MAX 0x00000010
#define VMF_Y_MIN 0x00000020
#define VMF_Y_MID 0x00000040
#define VMF_Y_MAX 0x00000080

// Colour space options.

#define CS_SRGB 1
#define CS_LINEAR_RGB 2
#define CS_INHERIT 3

// Filter source types - these are used internally

#define VSF_GRAPHIC 1
#define VSF_ALPHA 2
#define VSF_BKGD 3
#define VSF_BKGD_ALPHA 4
#define VSF_FILL 5
#define VSF_STROKE 6
#define VSF_REFERENCE 7
#define VSF_IGNORE 8

// VectorWave options

#define WVC_NONE 1
#define WVC_TOP 2
#define WVC_BOTTOM 3

// Gradient flags

#define VGF_RELATIVE_X1 0x00000001
#define VGF_RELATIVE_Y1 0x00000002
#define VGF_RELATIVE_X2 0x00000004
#define VGF_RELATIVE_Y2 0x00000008
#define VGF_RELATIVE_CX 0x00000010
#define VGF_RELATIVE_CY 0x00000020
#define VGF_RELATIVE_FX 0x00000040
#define VGF_RELATIVE_FY 0x00000080
#define VGF_RELATIVE_RADIUS 0x00000100
#define VGF_FIXED_X1 0x00000200
#define VGF_FIXED_Y1 0x00000400
#define VGF_FIXED_X2 0x00000800
#define VGF_FIXED_Y2 0x00001000
#define VGF_FIXED_CX 0x00002000
#define VGF_FIXED_CY 0x00004000
#define VGF_FIXED_FX 0x00008000
#define VGF_FIXED_FY 0x00010000
#define VGF_FIXED_RADIUS 0x00020000

// Optional flags for the VectorScene object.

#define VPF_BITMAP_SIZED 0x00000001
#define VPF_RENDER_TIME 0x00000002
#define VPF_RESIZE 0x00000004

#define VSM_AUTO 0
#define VSM_NEIGHBOUR 1
#define VSM_BILINEAR 2
#define VSM_BICUBIC 3
#define VSM_SPLINE16 4
#define VSM_KAISER 5
#define VSM_QUADRIC 6
#define VSM_GAUSSIAN 7
#define VSM_BESSEL 8
#define VSM_MITCHELL 9
#define VSM_SINC3 10
#define VSM_LANCZOS3 11
#define VSM_BLACKMAN3 12
#define VSM_SINC8 13
#define VSM_LANCZOS8 14
#define VSM_BLACKMAN8 15

#define RC_FINAL_PATH 0x00000001
#define RC_BASE_PATH 0x00000002
#define RC_TRANSFORM 0x00000004
#define RC_ALL 0x000000ff

// Aspect ratios

#define ARF_X_MIN 0x00000001
#define ARF_X_MID 0x00000002
#define ARF_X_MAX 0x00000004
#define ARF_Y_MIN 0x00000008
#define ARF_Y_MID 0x00000010
#define ARF_Y_MAX 0x00000020
#define ARF_MEET 0x00000040
#define ARF_SLICE 0x00000080
#define ARF_NONE 0x00000100

// For vecGetBoundary()

#define VBF_INCLUSIVE 0x00000001
#define VBF_NO_TRANSFORM 0x00000002

struct VectorDef {
   OBJECTPTR Object;    // Reference to the definition object.
};

struct GradientStop {
   DOUBLE Offset;    // An offset in the range of 0 - 1.0
   struct DRGB RGB;  // A floating point RGB value.
};

struct Transition {
   DOUBLE  Offset;       // An offset from 0.0 to 1.0 at which to apply the transform.
   CSTRING Transform;    // A transform string, as per SVG guidelines.
};

struct VectorPoint {
   DOUBLE X;             // The X coordinate of this point.
   DOUBLE Y;             // The Y coordinate of this point.
   UBYTE  XRelative:1;   // TRUE if the X value is relative to its viewport (between 0 and 1.0).
   UBYTE  YRelative:1;   // TRUE if the Y value is relative to its viewport (between 0 and 1.0).
};

struct PathCommand {
   UBYTE  Type;       // The command type (PE value)
   UBYTE  Curved;     // Private
   UBYTE  LargeArc;   // Equivalent to the large-arc-flag in SVG, it ensures that the arc follows the longest drawing path when TRUE.
   UBYTE  Sweep;      // Equivalent to the sweep-flag in SVG, it inverts the default behaviour in generating arc paths.
   LONG   Pad;        // Private
   DOUBLE X;          // The targeted X coordinate for the command
   DOUBLE Y;          // The targeted Y coordinate for the command
   DOUBLE AbsX;       // Private
   DOUBLE AbsY;       // Private
   DOUBLE X2;         // The X2 coordinate for curve commands, or RX for arcs
   DOUBLE Y2;         // The Y2 coordinate for curve commands, or RY for arcs
   DOUBLE X3;         // The X3 coordinate for curve-to or smooth-curve-to
   DOUBLE Y3;         // The Y3 coordinate for curve-to or smooth-curve-to
   DOUBLE Angle;      // Arc angle
};

struct VectorTransform {
   struct VectorTransform * Next;    // The next transform in the list.
   struct VectorTransform * Prev;    // The previous transform in the list.
   DOUBLE X;                         // The X value, the meaning of which is defined by the Type
   DOUBLE Y;                         // The Y value, the meaning of which is defined by the Type
   DOUBLE Angle;                     // Requires VTF_ROTATE.  A rotation by Angle degrees about a given point.  If optional parameters X and Y are not specified, the rotate is about the origin of the current user coordinate system.
   DOUBLE Matrix[6];                 // Requires VTF_MATRIX.  A transformation expressed as a matrix of six values.
   WORD   Type;                      // The VTF indicates the type of transformation: rotate, skew etc
};

// VectorPath class definition

#define VER_VECTORPATH (1.000000)

// VectorPath methods

#define MT_VPAddCommand -30
#define MT_VPRemoveCommand -31
#define MT_VPSetCommand -32
#define MT_VPGetCommand -33
#define MT_VPSetCommandList -34

struct vpAddCommand { struct PathCommand * Commands; LONG Size;  };
struct vpRemoveCommand { LONG Index; LONG Total;  };
struct vpSetCommand { LONG Index; struct PathCommand * Command; LONG Size;  };
struct vpGetCommand { LONG Index; struct PathCommand * Command;  };
struct vpSetCommandList { APTR Commands; LONG Size;  };

INLINE ERROR vpAddCommand(APTR Ob, struct PathCommand * Commands, LONG Size) {
   struct vpAddCommand args = { Commands, Size };
   return(Action(MT_VPAddCommand, Ob, &args));
}

INLINE ERROR vpRemoveCommand(APTR Ob, LONG Index, LONG Total) {
   struct vpRemoveCommand args = { Index, Total };
   return(Action(MT_VPRemoveCommand, Ob, &args));
}

INLINE ERROR vpSetCommand(APTR Ob, LONG Index, struct PathCommand * Command, LONG Size) {
   struct vpSetCommand args = { Index, Command, Size };
   return(Action(MT_VPSetCommand, Ob, &args));
}

INLINE ERROR vpGetCommand(APTR Ob, LONG Index, struct PathCommand ** Command) {
   struct vpGetCommand args = { Index, 0 };
   ERROR error = Action(MT_VPGetCommand, Ob, &args);
   if (Command) *Command = args.Command;
   return(error);
}

INLINE ERROR vpSetCommandList(APTR Ob, APTR Commands, LONG Size) {
   struct vpSetCommandList args = { Commands, Size };
   return(Action(MT_VPSetCommandList, Ob, &args));
}


// VectorColour class definition

#define VER_VECTORCOLOUR (1.000000)

typedef struct rkVectorColour {
   OBJECT_HEADER
   DOUBLE Red;
   DOUBLE Green;
   DOUBLE Blue;
   DOUBLE Alpha;
} objVectorColour;

// VectorScene class definition

#define VER_VECTORSCENE (1.000000)

typedef struct rkVectorScene {
   OBJECT_HEADER
   LARGE  RenderTime;             // Microseconds elapsed during the last rendering operation.
   DOUBLE Gamma;                  // Not currently implemented.
   struct rkVector * Viewport;    // Reference to the VectorViewport that contains the VectorScene.
   struct rkBitmap * Bitmap;      // Target bitmap.
   struct KeyStore * Defs;        // Stores references to gradients, images, patterns etc
   LONG   Flags;                  // Optional flags.
   LONG   PageWidth;              // Fixed page width - vector viewport width will be stretched to fit this if resizing is enabled.
   LONG   PageHeight;             // Fixed page height - vector viewport height will be stretched to fit this if resizing is enabled.
   LONG   SampleMethod;           // VSM: Method to use for resampling images and patterns.

#ifdef PRV_VECTORSCENE
   class VMAdaptor *Adaptor;
   agg::rendering_buffer *Buffer;
   UBYTE  AdaptorType;
  
#endif
} objVectorScene;

// VectorScene methods

#define MT_ScAddDef -1
#define MT_ScSearchByID -2
#define MT_ScFindDef -3

struct scAddDef { CSTRING Name; OBJECTPTR Def;  };
struct scSearchByID { LONG ID; OBJECTPTR Result;  };
struct scFindDef { CSTRING Name; OBJECTPTR Def;  };

INLINE ERROR scAddDef(APTR Ob, CSTRING Name, OBJECTPTR Def) {
   struct scAddDef args = { Name, Def };
   return(Action(MT_ScAddDef, Ob, &args));
}

INLINE ERROR scSearchByID(APTR Ob, LONG ID, OBJECTPTR * Result) {
   struct scSearchByID args = { ID, 0 };
   ERROR error = Action(MT_ScSearchByID, Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

INLINE ERROR scFindDef(APTR Ob, CSTRING Name, OBJECTPTR * Def) {
   struct scFindDef args = { Name, 0 };
   ERROR error = Action(MT_ScFindDef, Ob, &args);
   if (Def) *Def = args.Def;
   return(error);
}


// VectorImage class definition

#define VER_VECTORIMAGE (1.000000)

typedef struct rkVectorImage {
   OBJECT_HEADER
   DOUBLE X;
   DOUBLE Y;
   struct rkPicture * Picture;
   struct rkBitmap * Bitmap;
   LONG   Units;                  // VUNIT constant, defines the coordinate system for (X,Y)
   LONG   Dimensions;
   LONG   SpreadMethod;
} objVectorImage;

// VectorPattern class definition

#define VER_VECTORPATTERN (1.000000)

typedef struct rkVectorPattern {
   OBJECT_HEADER
   DOUBLE X;
   DOUBLE Y;
   DOUBLE Opacity;
   struct rkVectorScene * Scene;          // Internal scene
   struct rkVectorViewport * Viewport;    // Internal viewport
   struct rkVectorPattern * Inherit;      // Reference to another pattern from which to inherit attributes
   LONG   SpreadMethod;
   LONG   Units;                          // VUNIT constant
   LONG   ContentUnits;                   // VUNIT constant
   LONG   Dimensions;

#ifdef PRV_VECTORPATTERN
   struct VectorTransform *Transforms;
   objBitmap *Bitmap;
  
#endif
} objVectorPattern;

// VectorGradient class definition

#define VER_VECTORGRADIENT (1.000000)

typedef struct rkVectorGradient {
   OBJECT_HEADER
   DOUBLE X1;                            // Starting X coordinate of the gradient 'line'
   DOUBLE Y1;                            // Starting Y coordinate of the gradient 'line'
   DOUBLE X2;                            // Ending X of the gradient 'line'
   DOUBLE Y2;                            // Ending Y of the gradient 'line'
   DOUBLE CenterX;                       // Center X coordinate of radial gradient shapes.
   DOUBLE CenterY;                       // Center Y coordinate of radial gradient shapes.
   DOUBLE FX;                            // Focal X coordinate for radial gradient shapes.
   DOUBLE FY;                            // Focal Y coordinate for radial gradient shapes.
   DOUBLE Radius;                        // The size of a radial gradient radius.
   struct rkVectorGradient * Inherit;    // Reference to another gradient from which to inherit attributes
   LONG   SpreadMethod;                  // Defines the spread method to use for gradient fills.
   LONG   Units;                         // Defines the coordinate system for (x1,y1),(x2,y2)
   LONG   Type;
   LONG   Flags;                         // Optional flags
   LONG   TotalStops;                    // The total number of records in the Stops array.

#ifdef PRV_VECTORGRADIENT
   struct GradientStop *Stops;  // An array of gradient stop colours.
   struct VectorTransform *Transforms;
   class GradientColours *Colours;
   STRING ID;
   LONG NumericID;
   WORD ChangeCounter;
  
#endif
} objVectorGradient;

// VectorFilter class definition

#define VER_VECTORFILTER (1.000000)

typedef struct rkVectorFilter {
   OBJECT_HEADER
   DOUBLE X;                              // Left-most position of filter area
   DOUBLE Y;                              // Top-most position of filter area
   DOUBLE Width;                          // Width of filter area
   DOUBLE Height;                         // Height of filter area
   DOUBLE Opacity;                        // Level of opacity from 0 - 1.0
   struct rkVectorScene * Scene;          // Internal scene
   struct rkVectorViewport * Viewport;    // Internal viewport
   struct rkVectorFilter * Inherit;       // Reference to another pattern from which to inherit attributes
   struct rkXML * EffectXML;              // The XML object used to parse incoming effects
   LONG   Units;                          // VUNIT constant
   LONG   PrimitiveUnits;                 // VUNIT constant
   LONG   Dimensions;                     // Flags for detailing area values
   LONG   ColourSpace;

#ifdef PRV_VECTORFILTER
   LARGE DrawStamp; // Timestamp at which this filter was last rendered
   struct effect *Effects;
   struct effect *LastEffect;
   struct effect **Merge;
   objBitmap *SrcBitmap; // A temporary alpha enabled drawing of the vector that is targeted by the filter.
   objBitmap *BkgdBitmap;
   objBitmap *MergeBitmap;
   STRING Path; // Affix this path to file references (e.g. feImage).
   struct {
      objBitmap *Bitmap;
      UBYTE *Data;
      LONG DataSize;
   } Bank[10];
   struct effect SrcGraphic;
   struct effect BkgdGraphic;
   LONG BoundX, BoundY, BoundWidth, BoundHeight;  // Calculated pixel boundary for the entire filter and its effects.
   LONG ViewX, ViewY, ViewWidth, ViewHeight; // Boundary of the target area (for user space coordinate mode)
   UBYTE BankIndex;
  
#endif
} objVectorFilter;

#define SHAPE_PUBLIC \
   struct rkVector *Child; \
   struct rkVectorScene *Scene; \
   struct rkVector *Next; \
   struct rkVector *Prev; \
   OBJECTPTR Parent; \
   struct VectorTransform *Transforms; \
   DOUBLE StrokeWidth; \
   DOUBLE StrokeOpacity; \
   DOUBLE FillOpacity; \
   DOUBLE Opacity; \
   DOUBLE MiterLimit; \
   DOUBLE InnerMiterLimit; \
   DOUBLE DashOffset; \
   LONG   ActiveTransforms; \
   LONG   DashTotal; \
   LONG   Visibility;
  
// Vector class definition

#define VER_VECTOR (1.000000)

typedef struct rkVector {
   OBJECT_HEADER
   struct rkVector * Child;                // The first child vector, or NULL.
   struct rkVectorScene * Scene;           // Short-cut to the top-level VectorScene.
   struct rkVector * Next;                 // The next vector in the branch, or NULL.
   struct rkVector * Prev;                 // The previous vector in the branch, or NULL.
   OBJECTPTR Parent;                       // The parent vector, or NULL if this is the top-most vector.
   struct VectorTransform * Transforms;    // A list of transforms to apply to the vector.
   DOUBLE    StrokeWidth;                  // The width to use when stroking the path.
   DOUBLE    StrokeOpacity;                // Defines the opacity of the path stroke.
   DOUBLE    FillOpacity;                  // The opacity to use when filling the vector.
   DOUBLE    Opacity;                      // An overall opacity value for the vector.
   DOUBLE    MiterLimit;                   // Imposes a limit on the ratio of the miter length to the StrokeWidth.
   DOUBLE    InnerMiterLimit;              // A special limit to apply when the MITER_ROUND line-join effect is in use.
   DOUBLE    DashOffset;                   // For the DashArray, applies an initial dash offset.
   LONG      ActiveTransforms;             // Indicates the transforms that are currently applied to a vector.
   LONG      DashTotal;                    // The total number of values in the DashArray.
   LONG      Visibility;                   // Controls the visibility of a vector and its children.

#ifdef PRV_VECTOR
 SHAPE_PRIVATE 
#endif
} objVector;

// Vector methods

#define MT_VecPush -1
#define MT_VecTracePath -2
#define MT_VecGetBoundary -3
#define MT_VecRotate -4
#define MT_VecTransform -5
#define MT_VecApplyMatrix -6
#define MT_VecTranslate -7
#define MT_VecScale -8
#define MT_VecSkew -9
#define MT_VecPointInPath -10
#define MT_VecClearTransforms -11
#define MT_VecGetTransform -12

struct vecPush { LONG Position;  };
struct vecTracePath { FUNCTION * Callback;  };
struct vecGetBoundary { LONG Flags; DOUBLE X; DOUBLE Y; DOUBLE Width; DOUBLE Height;  };
struct vecRotate { DOUBLE Angle; DOUBLE CenterX; DOUBLE CenterY;  };
struct vecTransform { CSTRING Transform;  };
struct vecApplyMatrix { DOUBLE A; DOUBLE B; DOUBLE C; DOUBLE D; DOUBLE E; DOUBLE F;  };
struct vecTranslate { DOUBLE X; DOUBLE Y;  };
struct vecScale { DOUBLE X; DOUBLE Y;  };
struct vecSkew { DOUBLE X; DOUBLE Y;  };
struct vecPointInPath { DOUBLE X; DOUBLE Y;  };
struct vecGetTransform { LONG Type; struct VectorTransform * Transform;  };

INLINE ERROR vecPush(APTR Ob, LONG Position) {
   struct vecPush args = { Position };
   return(Action(MT_VecPush, Ob, &args));
}

INLINE ERROR vecTracePath(APTR Ob, FUNCTION * Callback) {
   struct vecTracePath args = { Callback };
   return(Action(MT_VecTracePath, Ob, &args));
}

INLINE ERROR vecGetBoundary(APTR Ob, LONG Flags, DOUBLE * X, DOUBLE * Y, DOUBLE * Width, DOUBLE * Height) {
   struct vecGetBoundary args = { Flags, 0, 0, 0, 0 };
   ERROR error = Action(MT_VecGetBoundary, Ob, &args);
   if (X) *X = args.X;
   if (Y) *Y = args.Y;
   if (Width) *Width = args.Width;
   if (Height) *Height = args.Height;
   return(error);
}

INLINE ERROR vecRotate(APTR Ob, DOUBLE Angle, DOUBLE CenterX, DOUBLE CenterY) {
   struct vecRotate args = { Angle, CenterX, CenterY };
   return(Action(MT_VecRotate, Ob, &args));
}

INLINE ERROR vecTransform(APTR Ob, CSTRING Transform) {
   struct vecTransform args = { Transform };
   return(Action(MT_VecTransform, Ob, &args));
}

INLINE ERROR vecApplyMatrix(APTR Ob, DOUBLE A, DOUBLE B, DOUBLE C, DOUBLE D, DOUBLE E, DOUBLE F) {
   struct vecApplyMatrix args = { A, B, C, D, E, F };
   return(Action(MT_VecApplyMatrix, Ob, &args));
}

INLINE ERROR vecTranslate(APTR Ob, DOUBLE X, DOUBLE Y) {
   struct vecTranslate args = { X, Y };
   return(Action(MT_VecTranslate, Ob, &args));
}

INLINE ERROR vecScale(APTR Ob, DOUBLE X, DOUBLE Y) {
   struct vecScale args = { X, Y };
   return(Action(MT_VecScale, Ob, &args));
}

INLINE ERROR vecSkew(APTR Ob, DOUBLE X, DOUBLE Y) {
   struct vecSkew args = { X, Y };
   return(Action(MT_VecSkew, Ob, &args));
}

INLINE ERROR vecPointInPath(APTR Ob, DOUBLE X, DOUBLE Y) {
   struct vecPointInPath args = { X, Y };
   return(Action(MT_VecPointInPath, Ob, &args));
}

#define vecClearTransforms(obj) Action(MT_VecClearTransforms,(obj),0)

INLINE ERROR vecGetTransform(APTR Ob, LONG Type, struct VectorTransform ** Transform) {
   struct vecGetTransform args = { Type, 0 };
   ERROR error = Action(MT_VecGetTransform, Ob, &args);
   if (Transform) *Transform = args.Transform;
   return(error);
}


struct VectorBase {
   ERROR (*_DrawPath)(struct rkBitmap *, APTR, DOUBLE, APTR, APTR);
   void (*_FreePath)(APTR);
   ERROR (*_GenerateEllipse)(DOUBLE, DOUBLE, DOUBLE, DOUBLE, LONG, APTR);
   ERROR (*_GeneratePath)(CSTRING, APTR);
   ERROR (*_GenerateRectangle)(DOUBLE, DOUBLE, DOUBLE, DOUBLE, APTR);
   void (*_ReadPainter)(APTR, CSTRING, struct DRGB *, struct rkVectorGradient **, struct rkVectorImage **, struct rkVectorPattern **);
   void (*_TranslatePath)(APTR, DOUBLE, DOUBLE);
   void (*_MoveTo)(APTR, DOUBLE, DOUBLE);
   void (*_LineTo)(APTR, DOUBLE, DOUBLE);
   void (*_ArcTo)(APTR, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, LONG);
   void (*_Curve3)(APTR, DOUBLE, DOUBLE, DOUBLE, DOUBLE);
   void (*_Smooth3)(APTR, DOUBLE, DOUBLE);
   void (*_Curve4)(APTR, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE);
   void (*_Smooth4)(APTR, DOUBLE, DOUBLE, DOUBLE, DOUBLE);
   void (*_ClosePath)(APTR);
   void (*_RewindPath)(APTR);
   LONG (*_GetVertex)(APTR, DOUBLE *, DOUBLE *);
   ERROR (*_ApplyPath)(APTR, APTR);
};

#ifndef PRV_VECTOR_MODULE
#define vecDrawPath(...) (VectorBase->_DrawPath)(__VA_ARGS__)
#define vecFreePath(...) (VectorBase->_FreePath)(__VA_ARGS__)
#define vecGenerateEllipse(...) (VectorBase->_GenerateEllipse)(__VA_ARGS__)
#define vecGeneratePath(...) (VectorBase->_GeneratePath)(__VA_ARGS__)
#define vecGenerateRectangle(...) (VectorBase->_GenerateRectangle)(__VA_ARGS__)
#define vecReadPainter(...) (VectorBase->_ReadPainter)(__VA_ARGS__)
#define vecTranslatePath(...) (VectorBase->_TranslatePath)(__VA_ARGS__)
#define vecMoveTo(...) (VectorBase->_MoveTo)(__VA_ARGS__)
#define vecLineTo(...) (VectorBase->_LineTo)(__VA_ARGS__)
#define vecArcTo(...) (VectorBase->_ArcTo)(__VA_ARGS__)
#define vecCurve3(...) (VectorBase->_Curve3)(__VA_ARGS__)
#define vecSmooth3(...) (VectorBase->_Smooth3)(__VA_ARGS__)
#define vecCurve4(...) (VectorBase->_Curve4)(__VA_ARGS__)
#define vecSmooth4(...) (VectorBase->_Smooth4)(__VA_ARGS__)
#define vecClosePath(...) (VectorBase->_ClosePath)(__VA_ARGS__)
#define vecRewindPath(...) (VectorBase->_RewindPath)(__VA_ARGS__)
#define vecGetVertex(...) (VectorBase->_GetVertex)(__VA_ARGS__)
#define vecApplyPath(...) (VectorBase->_ApplyPath)(__VA_ARGS__)
#endif

//****************************************************************************

INLINE void SET_VECTOR_COLOUR(objVectorColour *Colour, DOUBLE Red, DOUBLE Green, DOUBLE Blue, DOUBLE Alpha) {
   Colour->Head.ClassID = ID_VECTORCOLOUR;
   Colour->Red   = Red;
   Colour->Green = Green;
   Colour->Blue  = Blue;
   Colour->Alpha = Alpha;
}
  
#define SVF_ANIMATETRANSFORM 0x6349c940
#define SVF_ANIMATEMOTION 0x8a27c6ba
#define SVF_CIRCLE 0xf679fe97
#define SVF_DEFS 0x7c95a0a7
#define SVF_ELLIPSE 0x66448f53
#define SVF_LINE 0x7c9a15ad
#define SVF_IMAGE 0x0fa87ca8
#define SVF_TEXT 0x7c9e690a
#define SVF_FX 0x005977e3
#define SVF_FY 0x005977e4
#define SVF_IMAGE 0x0fa87ca8
#define SVF_TO 0x005979a8
#define SVF_DUR 0x0b886bd0
#define SVF_DESC 0x7c95a244
#define SVF_PATH 0x7c9c25f2
#define SVF_X 0x0002b61d
#define SVF_Y 0x0002b61e
#define SVF_RX 0x0059796f
#define SVF_RY 0x00597970
#define SVF_CX 0x00597780
#define SVF_CY 0x00597781
#define SVF_R 0x0002b617
#define SVF_X1 0x005979ee
#define SVF_Y1 0x00597a0f
#define SVF_X2 0x005979ef
#define SVF_Y2 0x00597a10
#define SVF_D 0x0002b609
#define SVF_DX 0x005977a1
#define SVF_DY 0x005977a2
#define SVF_IN 0x0059783c
#define SVF_IN2 0x0b887fee
#define SVF_OPERATOR 0x8d9849f1
#define SVF_K1 0x00597841
#define SVF_K2 0x00597842
#define SVF_K3 0x00597843
#define SVF_K4 0x00597844
#define SVF_N1 0x005978a4
#define SVF_N2 0x005978a5
#define SVF_N3 0x005978a6
#define SVF_PHI 0x0b889d26
#define SVF_M 0x0002b612
#define SVF_MOD 0x0b889145
#define SVF_A 0x0002b606
#define SVF_B 0x0002b607
#define SVF_ALIGN 0x0f174e50
#define SVF_MASK 0x7c9a80b1
#define SVF_CLOSE 0x0f3b9a5b
#define SVF_TOP 0x0b88af18
#define SVF_SPIRAL 0x1c468330
#define SVF_BOTTOM 0xf492ca7a
#define SVF_AMPLITUDE 0x5e60600a
#define SVF_REPEAT 0x192dec66
#define SVF_TRANSITION 0x96486f70
#define SVF_PARASOL_TRANSITION 0xc0f6617c
#define SVF_PARASOL_PATHTRANSITION 0x9d3c64a9
#define SVF_FREQUENCY 0xffd1bad7
#define SVF_THICKNESS 0x369e2871
#define SVF_DECAY 0x0f49a6eb
#define SVF_VERTICES 0xd31fda6a
#define SVF_SCALE 0x1057f68d
#define SVF_PARASOL_SHAPE 0x6bba2f82
#define SVF_CLIPPATHUNITS 0x94efb24d
#define SVF_CLIPPATH 0x4fd1b75a
#define SVF_CLIP_PATH 0x455423a7
#define SVF_CLIP_RULE 0x45559072
#define SVF_RADIUS 0x18df096d
#define SVF_TEXTPATH 0x089ef477
#define SVF_MODE 0x7c9aba4a
#define SVF_OVERLAY 0x7ee4b5c7
#define SVF_PLUS 0x7c9c54e9
#define SVF_MINUS 0x0feee651
#define SVF_BURN 0x7c94cd7c
#define SVF_SCREEN 0x1b5ffd45
#define SVF_STEP 0x7c9e1a01
#define SVF_OFFSET 0x123b4b4c
#define SVF_BASEFREQUENCY 0xea1938b2
#define SVF_STITCHTILES 0x3d844d95
#define SVF_PRIMITIVEUNITS 0xf4494b91
#define SVF_IMAGE_RENDERING 0xfdb735d3
#define SVF_SEED 0x7c9dda26
#define SVF_MULTIPLY 0x46746f05
#define SVF_NUMOCTAVES 0x16f8e14a
#define SVF_LIGHTEN 0x79c1c710
#define SVF_DARKEN 0xf83e845a
#define SVF_INVERTRGB 0xacb1dd38
#define SVF_INVERT 0x04d5a7bd
#define SVF_DODGE 0x0f4f27a8
#define SVF_HARDLIGHT 0x022cb75c
#define SVF_SOFTLIGHT 0x78b6e7b9
#define SVF_DIFFERENCE 0x52a92470
#define SVF_EXCLUSION 0x6f499bff
#define SVF_FLOOD_COLOR 0x37459885
#define SVF_FLOOD_COLOUR 0x1ff8a9fa
#define SVF_FLOOD_OPACITY 0xbc50167f
#define SVF_OUT 0x0b889a9d
#define SVF_ORDER 0x1017da21
#define SVF_OVER 0x7c9bf101
#define SVF_ATOP 0x7c943c79
#define SVF_XOR 0x0b88c01e
#define SVF_ARITHMETIC 0x600354ef
#define SVF_COLOR_INTERPOLATION_FILTERS 0x752d48ff
#define SVF_COLOR_INTERPOLATION 0x6f2c0659
#define SVF_PRESERVEALPHA 0xf9b49d57
#define SVF_KERNELMATRIX 0xfb05405b
#define SVF_DIVISOR 0x12ffda05
#define SVF_BIAS 0x7c949844
#define SVF_TARGETX 0xcfb0ab64
#define SVF_TARGETY 0xcfb0ab65
#define SVF_EDGEMODE 0xbb10b09f
#define SVF_KERNELUNITLENGTH 0x05c04f48
#define SVF_CONTRAST 0x42b3b373
#define SVF_BRIGHTNESS 0x7bdc2cbe
#define SVF_HUE 0x0b887cc7
#define SVF_COLOURISE 0xf3cb4eda
#define SVF_DESATURATE 0x226696d7
#define SVF_PROTANOPIA 0x15f03a02
#define SVF_PROTANOMALY 0xd3f5b4fb
#define SVF_DEUTERANOPIA 0x1e300926
#define SVF_DEUTERANOMALY 0xe42f689f
#define SVF_TRITANOPIA 0x9c8f8140
#define SVF_TRITANOMALY 0x2e7de3f9
#define SVF_ACHROMATOPSIA 0xc3f56170
#define SVF_ACHROMATOMALY 0xc3f37036
#define SVF_MATRIX 0x0d3e291a
#define SVF_HUEROTATE 0xaf80b596
#define SVF_SATURATE 0xdf32bb4e
#define SVF_LUMINANCETOALPHA 0xc6ee7d8a
#define SVF_SOURCEGRAPHIC 0x5a1343b4
#define SVF_SOURCEALPHA 0xbe4b853c
#define SVF_BACKGROUNDIMAGE 0xaacc0f28
#define SVF_BACKGROUNDALPHA 0xaa3afeab
#define SVF_FILLPAINT 0xc0525d28
#define SVF_STROKEPAINT 0x1920b9b9
#define SVF_RESULT 0x192fd704
#define SVF_FILTERUNITS 0x5a2d0b3e
#define SVF_FEBLUR 0xfd2877e5
#define SVF_FEBLEND 0xa2373055
#define SVF_FECOLORMATRIX 0x92252784
#define SVF_FECOLOURMATRIX 0x371a19f9
#define SVF_FECOMPONENTTRANSFER 0xf4fa6788
#define SVF_FECOMPOSITE 0xf71764e3
#define SVF_FECONVOLVEMATRIX 0x0b05cd91
#define SVF_FEDIFFUSELIGHTING 0xf094ecac
#define SVF_FEDISPLACEMENTMAP 0xb9cf0a67
#define SVF_FEFLOOD 0xa27fbd04
#define SVF_FEGAUSSIANBLUR 0xfdba17c0
#define SVF_FEIMAGE 0xa2b65653
#define SVF_FEMERGE 0xa2fa9da0
#define SVF_FEMORPHOLOGY 0x8f1be720
#define SVF_FEOFFSET 0x07045a57
#define SVF_FESPECULARLIGHTING 0x68af6ee5
#define SVF_FETILE 0xfd3248be
#define SVF_FETURBULENCE 0x4eba1da9
#define SVF_FEDISTANTLIGHT 0x12a0c2ff
#define SVF_FEPOINTLIGHT 0xcebc7c12
#define SVF_FESPOTLIGHT 0xce2d968e
#define SVF_STDDEVIATION 0x861007d3
#define SVF_XMLNS 0x10b81bf7
#define SVF_TITLE 0x106daa27
#define SVF_SYMBOL 0x1ceb4efb
#define SVF_BEVEL 0x0f25c733
#define SVF_BUTT 0x7c94cdc4
#define SVF_COLOR 0x0f3d3244
#define SVF_COLOUR 0xf6e37b99
#define SVF_DISPLAY 0x12cd479b
#define SVF_OPACITY 0x70951bfe
#define SVF_CLIP_RULE 0x45559072
#define SVF_FILTER 0xfd7675ab
#define SVF_PARASOL_MORPH 0x6b51bb77
#define SVF_PARASOL_SPIRAL 0xe3954f3c
#define SVF_PARASOL_WAVE 0xbd7455e4
#define SVF_XLINK_HREF 0x379480aa
#define SVF_BASEPROFILE 0xca40f031
#define SVF_PATTERNTRANSFORM 0x6495503f
#define SVF_EXTERNALRESOURCESREQUIRED 0x582d0624
#define SVF_XML_SPACE 0x2db612fc
#define SVF_PATTERNUNITS 0x6eec1696
#define SVF_PATTERNCONTENTUNITS 0x6bc53e31
#define SVF_CONTOURGRADIENT 0x82a83fdd
#define SVF_PATTERN 0x9bf30a03
#define SVF_FILL 0x7c96cb2c
#define SVF_FILL_OPACITY 0x59fd2152
#define SVF_FILL_RULE 0xbb9f7891
#define SVF_ENABLE_BACKGROUND 0xa1e664d9
#define SVF_VERTEX_SCALING 0x2363f691
#define SVF_FONT_SIZE 0xf1c88f84
#define SVF_FONT_FAMILY 0x673faacb
#define SVF_FONT_WEIGHT 0x8f2d84f1
#define SVF_FONT_STRETCH 0x64948686
#define SVF_FONT_SIZE_ADJUST 0x2a32397c
#define SVF_FONT 0x7c96e4fc
#define SVF_FONT_STYLE 0x2ae0853a
#define SVF_FONT_VARIANT 0x1f331afe
#define SVF_FROM 0x7c96f1d9
#define SVF_G 0x0002b60c
#define SVF_GRADIENTUNITS 0x6c7c4886
#define SVF_GRADIENTTRANSFORM 0x31ccfa2f
#define SVF_HEIGHT 0x01d688de
#define SVF_ID 0x00597832
#define SVF_INHERIT 0x9e8d4758
#define SVF_INVERT_X_AXIS 0xa4fb3664
#define SVF_INVERT_Y_AXIS 0xa7505f05
#define SVF_JAG 0x0b8882b7
#define SVF_KERNING 0x243d11f3
#define SVF_LENGTHADJUST 0x748cbc92
#define SVF_LETTER_SPACING 0x982bebc7
#define SVF_LINEARGRADIENT 0xe6871dce
#define SVF_MARKER 0x0d3cf207
#define SVF_MARKER_END 0x66ff06cb
#define SVF_MARKER_MID 0x66ff282e
#define SVF_MARKER_START 0x23dc8942
#define SVF_METHOD 0x0d866146
#define SVF_MITER 0x0feefdc6
#define SVF_MITER_REVERT 0x7bc9e50b
#define SVF_MITER_ROUND 0x1349a65b
#define SVF_NONE 0x7c9b47f5
#define SVF_NUMERIC_ID 0x3768b852
#define SVF_OVERFLOW 0x5b785259
#define SVF_PATHLENGTH 0x74403974
#define SVF_POINTS 0x1534e242
#define SVF_PATH 0x7c9c25f2
#define SVF_POLYLINE 0x3db88331
#define SVF_POLYGON 0xbc0d44cd
#define SVF_RECT 0x7c9d4d93
#define SVF_RADIALGRADIENT 0x4016b4c0
#define SVF_ROTATE 0x19e50454
#define SVF_ROUND 0x104cc7ed
#define SVF_SPACING 0xa47e0e2a
#define SVF_SPREADMETHOD 0x0caafac5
#define SVF_STARTOFFSET 0xed10629a
#define SVF_STRING 0x1c93affc
#define SVF_STROKE 0x1c93c91d
#define SVF_STROKE_OPACITY 0xdacd8043
#define SVF_STROKE_WIDTH 0xa27c3faa
#define SVF_STROKE_LINECAP 0xe476e8e6
#define SVF_STROKE_LINEJOIN 0x73581762
#define SVF_STROKE_MITERLIMIT 0x49c40b8a
#define SVF_STROKE_MITERLIMIT_THETA 0x3dab0e2d
#define SVF_STROKE_INNER_MITERLIMIT 0x8ab099f3
#define SVF_STROKE_INNERJOIN 0x1ebcf876
#define SVF_STROKE_DASHARRAY 0x5faa6be9
#define SVF_STROKE_DASHOFFSET 0x74c0b1b1
#define SVF_STYLE 0x1061af16
#define SVF_SQUARE 0x1c5eea16
#define SVF_SVG 0x0b88abb5

#define SVF_TEXTLENGTH 0xa31e6e8c
#define SVF_TEXT_ANCHOR 0x0c0046d2
#define SVF_TEXT_DECORATION 0x2230061f
#define SVF_TOTAL_POINTS 0x93249a53
#define SVF_TRANSFORM 0x2393dd81
#define SVF_USE 0x0b88b3d2
#define SVF_UNITS 0x108252d8
#define SVF_VIEWBOX 0x7b6be409
#define SVF_VERSION 0x73006c4b
#define SVF_VIEW_X 0x22c52ea5
#define SVF_VIEW_Y 0x22c52ea6
#define SVF_VIEW_WIDTH 0x497f2d2d
#define SVF_VIEW_HEIGHT 0x56219666
#define SVF_VISIBILITY 0x7a0f4bad
#define SVF_WIDTH 0x10a3b0a5
#define SVF_WORD_SPACING 0x62976533
#define SVF_ALICEBLUE 0x41f60f4b
#define SVF_ANTIQUEWHITE 0x3a2d20fd
#define SVF_AQUA 0x7c94306d
#define SVF_AQUAMARINE 0x52e1f409
#define SVF_AZURE 0x0f1f300c
#define SVF_BEIGE 0x0f259021
#define SVF_BISQUE 0xf4259f0e
#define SVF_BLACK 0x0f294442
#define SVF_BLANCHEDALMOND 0x25a17751
#define SVF_BLUE 0x7c94a78d
#define SVF_BLUEVIOLET 0x59f4db60
#define SVF_BROWN 0x0f2cccad
#define SVF_BURLYWOOD 0xd00306ac
#define SVF_CADETBLUE 0x88f15cae
#define SVF_CHARTREUSE 0xfb91543b
#define SVF_CHOCOLATE 0x487f4c37
#define SVF_CORAL 0x0f3d49f6
#define SVF_CORNFLOWERBLUE 0x68196cee
#define SVF_CORNSILK 0x4b9c706a
#define SVF_CRIMSON 0xda1afde0
#define SVF_CYAN 0x7c9568b0
#define SVF_DARKBLUE 0x01ef64af
#define SVF_DARKCYAN 0x01f025d2
#define SVF_DARKGOLDENROD 0xc6d90285
#define SVF_DARKGRAY 0x01f2399a
#define SVF_DARKGREEN 0x40397bb8
#define SVF_DARKGREY 0x01f23a1e
#define SVF_DARKKHAKI 0x407c51af
#define SVF_DARKMAGENTA 0xdae143e4
#define SVF_DARKOLIVEGREEN 0x092c7a97
#define SVF_DARKORANGE 0x5a102c03
#define SVF_DARKORCHID 0x5a112b80
#define SVF_DARKRED 0x000f4622
#define SVF_DARKSALMON 0x623732f1
#define SVF_DARKSEAGREEN 0xe6a4e091
#define SVF_DARKSLATEBLUE 0x4e741068
#define SVF_DARKSLATEGRAY 0x4e76e553
#define SVF_DARKSLATEGREY 0x4e76e5d7
#define SVF_DARKTURQUOISE 0x28082838
#define SVF_DARKVIOLET 0x69c9107a
#define SVF_DEEPPINK 0x17e761b5
#define SVF_DEEPSKYBLUE 0x84780222
#define SVF_DIMGRAY 0x125bdeb2
#define SVF_DIMGREY 0x125bdf36
#define SVF_DODGERBLUE 0x8208b222
#define SVF_FIREBRICK 0x7ce7a736
#define SVF_FLORALWHITE 0xa97767c6
#define SVF_FORESTGREEN 0x8eda0a29
#define SVF_FUCHSIA 0xc799dc48
#define SVF_GAINSBORO 0xf0b2b209
#define SVF_GHOSTWHITE 0x44ab668b
#define SVF_GOLD 0x7c97710b
#define SVF_GOLDENROD 0xaaf0c023
#define SVF_GRAY 0x7c977c78
#define SVF_GREEN 0x0f871a56
#define SVF_GREENYELLOW 0xc0a3f4f2
#define SVF_GREY 0x7c977cfc
#define SVF_HONEYDEW 0xdef14de8
#define SVF_HOTPINK 0x54c73bc2
#define SVF_INDIANRED 0x4b374f13
#define SVF_INDIGO 0x04cbd87f
#define SVF_IVORY 0x0fada91e
#define SVF_KHAKI 0x0fc9f04d
#define SVF_LAVENDER 0x6cec8bb6
#define SVF_LAVENDERBLUSH 0x4d30e8b4
#define SVF_LAWNGREEN 0x6bffad68
#define SVF_LEMONCHIFFON 0x1aa3ab7d
#define SVF_LIGHTBLUE 0xf14e2ce5
#define SVF_LIGHTCORAL 0x1b277a4e
#define SVF_LIGHTCYAN 0xf14eee08
#define SVF_LIGHTGOLDENRODYELLOW 0x269c7ed7
#define SVF_LIGHTGRAY 0xf15101d0
#define SVF_LIGHTGREEN 0x1b714aae
#define SVF_LIGHTGREY 0xf1510254
#define SVF_LIGHTPINK 0xf155cc8f
#define SVF_LIGHTSALMON 0xa468e0a7
#define SVF_LIGHTSEAGREEN 0x7bf8d3c7
#define SVF_LIGHTSKYBLUE 0x49bdb6bc
#define SVF_LIGHTSLATEGRAY 0x8e493f49
#define SVF_LIGHTSLATEGREY 0x8e493fcd
#define SVF_LIGHTSTEELBLUE 0x01bf4e82
#define SVF_LIGHTYELLOW 0xb2b03239
#define SVF_LIME 0x7c9a158c
#define SVF_LIMEGREEN 0xb749873d
#define SVF_LINEN 0x0fdccbbb
#define SVF_MAGENTA 0xb4110202
#define SVF_MAROON 0x0d3d0451
#define SVF_MEDIUMAQUAMARINE 0x5393448a
#define SVF_MEDIUMBLUE 0xd877eb4e
#define SVF_MEDIUMORCHID 0xf4d5d5df
#define SVF_MEDIUMPURPLE 0xf769a41e
#define SVF_MEDIUMSEAGREEN 0x453d9eb0
#define SVF_MEDIUMSLATEBLUE 0x80249267
#define SVF_MEDIUMSPRINGGREEN 0x814643ca
#define SVF_MEDIUMTURQUOISE 0x59b8aa37
#define SVF_MEDIUMVIOLETRED 0x3be46a94
#define SVF_MIDNIGHTBLUE 0x5f9313a1
#define SVF_MINTCREAM 0x9b7533e5
#define SVF_MISTYROSE 0x1de6ab94
#define SVF_MOCCASIN 0x62609d92
#define SVF_NAVAJOWHITE 0xe2bc6625
#define SVF_NAVY 0x7c9b0d83
#define SVF_OLDLACE 0x677b8e19
#define SVF_OLIVE 0x1014a744
#define SVF_OLIVEDRAB 0xcd1770fd
#define SVF_ORANGE 0x13119e61
#define SVF_ORANGERED 0xdc4c011c
#define SVF_ORCHID 0x13129dde
#define SVF_PALEGOLDENROD 0x46e1ce45
#define SVF_PALEGREEN 0xda326778
#define SVF_PALETURQUOISE 0xa810f3f8
#define SVF_PALEVIOLETRED 0x8a3cb455
#define SVF_PAPAYAWHIP 0xc670dd19
#define SVF_PEACHPUFF 0x37e01157
#define SVF_PERU 0x7c9c36c1
#define SVF_PINK 0x7c9c4737
#define SVF_PLUM 0x7c9c54e3
#define SVF_POWDERBLUE 0x547b961e
#define SVF_PURPLE 0x15a66c1d
#define SVF_RED 0x0b88a540
#define SVF_ROSYBROWN 0xf7e975fa
#define SVF_ROYALBLUE 0x8e773554
#define SVF_SADDLEBROWN 0x92bbf35a
#define SVF_SALMON 0x1b38a54f
#define SVF_SANDYBROWN 0xe10b172c
#define SVF_SEAGREEN 0xe5cc626f
#define SVF_SEASHELL 0xe6a00d96
#define SVF_SIENNA 0x1bc596c3
#define SVF_SILVER 0x1bc98e5a
#define SVF_SKYBLUE 0x9a861064
#define SVF_SLATEBLUE 0x328bce06
#define SVF_SLATEGRAY 0x328ea2f1
#define SVF_SLATEGREY 0x328ea375
#define SVF_SNOW 0x7c9e01cc
#define SVF_SPRINGGREEN 0x6a6ae329
#define SVF_STEELBLUE 0xa604b22a
#define SVF_TAN 0x0b88ad48
#define SVF_TEAL 0x7c9e660b
#define SVF_THISTLE 0xdf68be82
#define SVF_TOMATO 0x1e8b7ef9
#define SVF_TURQUOISE 0x0c1fe5d6
#define SVF_VIOLET 0x22ca82d8
#define SVF_WHEAT 0x10a3261e
#define SVF_WHITE 0x10a33986
#define SVF_WHITESMOKE 0x2580cae5
#define SVF_YELLOW 0x297ff6e1
#define SVF_YELLOWGREEN 0xda4a85b2
#define SVF_VALUES 0x22383ff5
#define SVF_START 0x106149d3
#define SVF_MIDDLE 0x0dc5ebd4
#define SVF_END 0x0b886f1c
#define SVF_NORMAL 0x108f79ae
#define SVF_WIDER 0x10a3aec0
#define SVF_NARROWER 0x3d07aeb5
#define SVF_ULTRA_CONDENSED 0xba25ad8d
#define SVF_EXTRA_CONDENSED 0x4cb18509
#define SVF_SEMI_CONDENSED 0xbc1627b3
#define SVF_LIGHTER 0x79c1c714
#define SVF_BOLD 0x7c94b326
#define SVF_BOLDER 0xf48e221d
#define SVF_BLINK 0x0f2967b5
#define SVF_UNDERLINE 0xb8ea5b4b
#define SVF_OVERLINE 0x5b7b8fa9
#define SVF_LINETHROUGH 0xf69720ce
#define SVF_CONDENSED 0x72f37898
#define SVF_DIAMONDGRADIENT 0xe8db24af
#define SVF_CONICGRADIENT 0x9a0996df
#define SVF_ATTRIBUTENAME 0x658ead7a
#define SVF_ATTRIBUTETYPE 0x65925e3b
#define SVF_BEGIN 0x0f2587ea
#define SVF_TYPE 0x7c9ebd07
#define SVF_MIN 0x0b889089
#define SVF_MAX 0x0b888f8b
#define SVF_RESTART 0x3f29fc8a
#define SVF_REPEATDUR 0xa7b01391
#define SVF_REPEATCOUNT 0x53edf46f
#define SVF_ADDITIVE 0x035604af
#define SVF_ACCUMULATE 0x5c660bc9
#define SVF_PRESERVEASPECTRATIO 0x195673f0

#endif
