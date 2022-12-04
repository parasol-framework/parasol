#ifndef MODULES_VECTOR
#define MODULES_VECTOR 1

// Name:      vector.h
// Copyright: Paul Manias © 2010-2022
// Generator: idl-c

#ifndef MAIN_H
#include <parasol/main.h>
#endif

#define MODVERSION_VECTOR (1)

#ifndef MODULES_DISPLAY_H
#include <parasol/modules/display.h>
#endif

class objVectorColour;
class objVectorScene;
class objVectorImage;
class objVectorPattern;
class objVectorGradient;
class objFilterEffect;
class objVectorFilter;
class objVector;
class objVectorViewport;

// Options for drawing arcs.

#define ARC_LARGE 0x00000001
#define ARC_SWEEP 0x00000002

// Optional flags and indicators for the Vector class.

#define VF_DISABLED 0x00000001
#define VF_HAS_FOCUS 0x00000002

// Light source identifiers.

#define LS_DISTANT 0
#define LS_SPOT 1
#define LS_POINT 2

// Lighting algorithm for the LightingFX class.

#define LT_DIFFUSE 0
#define LT_SPECULAR 1

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

#define EM_DUPLICATE 1
#define EM_WRAP 2
#define EM_NONE 3

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

// Component selection for RemapFX methods.

#define CMP_ALL -1
#define CMP_RED 0
#define CMP_GREEN 1
#define CMP_BLUE 2
#define CMP_ALPHA 3

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

// MorphologyFX options.

#define MOP_ERODE 0
#define MOP_DILATE 1

// Operators for CompositionFX.

#define OP_OVER 0
#define OP_IN 1
#define OP_OUT 2
#define OP_ATOP 3
#define OP_XOR 4
#define OP_ARITHMETIC 5
#define OP_SCREEN 6
#define OP_MULTIPLY 7
#define OP_LIGHTEN 8
#define OP_DARKEN 9
#define OP_INVERT_RGB 10
#define OP_INVERT 11
#define OP_CONTRAST 12
#define OP_DODGE 13
#define OP_BURN 14
#define OP_HARD_LIGHT 15
#define OP_SOFT_LIGHT 16
#define OP_DIFFERENCE 17
#define OP_EXCLUSION 18
#define OP_PLUS 19
#define OP_MINUS 20
#define OP_SUBTRACT 20
#define OP_OVERLAY 21

// Viewport overflow options.

#define VOF_VISIBLE 0
#define VOF_HIDDEN 1
#define VOF_SCROLL 2
#define VOF_INHERIT 3

// VectorText flags.

#define VTXF_UNDERLINE 0x00000001
#define VTXF_OVERLINE 0x00000002
#define VTXF_LINE_THROUGH 0x00000004
#define VTXF_BLINK 0x00000008
#define VTXF_EDIT 0x00000010
#define VTXF_EDITABLE 0x00000010
#define VTXF_AREA_SELECTED 0x00000020
#define VTXF_NO_SYS_KEYS 0x00000040
#define VTXF_OVERWRITE 0x00000080

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

#define VCS_INHERIT 0
#define VCS_SRGB 1
#define VCS_LINEAR_RGB 2

// Filter source types - these are used internally

#define VSF_IGNORE 0
#define VSF_NONE 0
#define VSF_GRAPHIC 1
#define VSF_ALPHA 2
#define VSF_BKGD 3
#define VSF_BKGD_ALPHA 4
#define VSF_FILL 5
#define VSF_STROKE 6
#define VSF_REFERENCE 7
#define VSF_PREVIOUS 8

// Wave options.

#define WVC_NONE 1
#define WVC_TOP 2
#define WVC_BOTTOM 3

// Wave style options.

#define WVS_CURVED 1
#define WVS_ANGLED 2
#define WVS_SAWTOOTH 3

// Colour modes for ColourFX.

#define CM_NONE 0
#define CM_MATRIX 1
#define CM_SATURATE 2
#define CM_HUE_ROTATE 3
#define CM_LUMINANCE_ALPHA 4
#define CM_CONTRAST 5
#define CM_BRIGHTNESS 6
#define CM_HUE 7
#define CM_DESATURATE 8
#define CM_COLOURISE 9

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
#define VPF_OUTLINE_VIEWPORTS 0x00000008

#define TB_TURBULENCE 0
#define TB_NOISE 1

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

#define RQ_AUTO 0
#define RQ_FAST 1
#define RQ_CRISP 2
#define RQ_PRECISE 3
#define RQ_BEST 4

#define RC_FINAL_PATH 0x00000001
#define RC_BASE_PATH 0x00000002
#define RC_TRANSFORM 0x00000004
#define RC_ALL 0x00000007

// Aspect ratios control alignment, scaling and clipping.

#define ARF_X_MIN 0x00000001
#define ARF_X_MID 0x00000002
#define ARF_X_MAX 0x00000004
#define ARF_Y_MIN 0x00000008
#define ARF_Y_MID 0x00000010
#define ARF_Y_MAX 0x00000020
#define ARF_MEET 0x00000040
#define ARF_SLICE 0x00000080
#define ARF_NONE 0x00000100

// Options for vecGetBoundary().

#define VBF_INCLUSIVE 0x00000001
#define VBF_NO_TRANSFORM 0x00000002

// Mask for controlling feedback events that are received.

#define FM_PATH_CHANGED 0x00000001
#define FM_HAS_FOCUS 0x00000002
#define FM_CHILD_HAS_FOCUS 0x00000004
#define FM_LOST_FOCUS 0x00000008

struct VectorDef {
   OBJECTPTR Object;    // Reference to the definition object.
};

struct GradientStop {
   DOUBLE Offset;    // An offset in the range of 0 - 1.0
   struct FRGB RGB;  // A floating point RGB value.
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
   DOUBLE X;          // The targeted X coordinate (absolute or relative) for the command
   DOUBLE Y;          // The targeted Y coordinate (absolute or relative) for the command
   DOUBLE AbsX;       // Private
   DOUBLE AbsY;       // Private
   DOUBLE X2;         // The X2 coordinate for curve commands, or RX for arcs
   DOUBLE Y2;         // The Y2 coordinate for curve commands, or RY for arcs
   DOUBLE X3;         // The X3 coordinate for curve-to or smooth-curve-to
   DOUBLE Y3;         // The Y3 coordinate for curve-to or smooth-curve-to
   DOUBLE Angle;      // Arc angle
};

struct VectorMatrix {
   struct VectorMatrix * Next;    // The next transform in the list.
   objVector * Vector;            // The vector associated with the transform.
   DOUBLE ScaleX;                 // Matrix value A
   DOUBLE ShearY;                 // Matrix value B
   DOUBLE ShearX;                 // Matrix value C
   DOUBLE ScaleY;                 // Matrix value D
   DOUBLE TranslateX;             // Matrix value E
   DOUBLE TranslateY;             // Matrix value F
};

// VectorColour class definition

#define VER_VECTORCOLOUR (1.000000)

class objVectorColour : public BaseClass {
   public:
   DOUBLE Red;    // The red component value.
   DOUBLE Green;  // The green component value.
   DOUBLE Blue;   // The blue component value.
   DOUBLE Alpha;  // The alpha component value.
};

// VectorScene class definition

#define VER_VECTORSCENE (1.000000)

// VectorScene methods

#define MT_ScAddDef -1
#define MT_ScSearchByID -2
#define MT_ScFindDef -3
#define MT_ScDebug -4

struct scAddDef { CSTRING Name; OBJECTPTR Def;  };
struct scSearchByID { LONG ID; OBJECTPTR Result;  };
struct scFindDef { CSTRING Name; OBJECTPTR Def;  };

INLINE ERROR scAddDef(APTR Ob, CSTRING Name, OBJECTPTR Def) {
   struct scAddDef args = { Name, Def };
   return(Action(MT_ScAddDef, (OBJECTPTR)Ob, &args));
}

INLINE ERROR scSearchByID(APTR Ob, LONG ID, OBJECTPTR * Result) {
   struct scSearchByID args = { ID, 0 };
   ERROR error = Action(MT_ScSearchByID, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

INLINE ERROR scFindDef(APTR Ob, CSTRING Name, OBJECTPTR * Def) {
   struct scFindDef args = { Name, 0 };
   ERROR error = Action(MT_ScFindDef, (OBJECTPTR)Ob, &args);
   if (Def) *Def = args.Def;
   return(error);
}

#define scDebug(obj) Action(MT_ScDebug,(obj),0)


class objVectorScene : public BaseClass {
   public:
   LARGE    RenderTime;           // Returns the rendering time of the last scene.
   DOUBLE   Gamma;                // Private. Not currently implemented.
   objVectorScene * HostScene;    // Refers to a top-level VectorScene object, if applicable.
   objVectorViewport * Viewport;  // References the first object in the scene, which must be a VectorViewport object.
   objBitmap * Bitmap;            // Target bitmap for drawing vectors.
   struct KeyStore * Defs;        // Stores references to gradients, images, patterns etc
   OBJECTID SurfaceID;            // May refer to a Surface object for enabling automatic rendering.
   LONG     Flags;                // Optional flags.
   LONG     PageWidth;            // The width of the page that contains the vector.
   LONG     PageHeight;           // The height of the page that contains the vector.
   LONG     SampleMethod;         // The sampling method to use when interpolating images and patterns.
   // Action stubs

   inline ERROR draw() { return Action(AC_Draw, this, NULL); }
   inline ERROR drawArea(LONG X, LONG Y, LONG Width, LONG Height) {
      struct acDraw args = { X, Y, Width, Height };
      return Action(AC_Draw, this, &args);
   }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
   inline ERROR redimension(DOUBLE X, DOUBLE Y, DOUBLE Z, DOUBLE Width, DOUBLE Height, DOUBLE Depth) {
      struct acRedimension args = { X, Y, Z, Width, Height, Depth };
      return Action(AC_Redimension, this, &args);
   }
   inline ERROR reset() { return Action(AC_Reset, this, NULL); }
   inline ERROR resize(DOUBLE Width, DOUBLE Height, DOUBLE Depth) {
      struct acResize args = { Width, Height, Depth };
      return Action(AC_Resize, this, &args);
   }
};

// VectorImage class definition

#define VER_VECTORIMAGE (1.000000)

class objVectorImage : public BaseClass {
   public:
   DOUBLE X;                // Apply a horizontal offset to the image, the origin of which is determined by the #Units value.
   DOUBLE Y;                // Apply a vertical offset to the image, the origin of which is determined by the #Units value.
   objPicture * Picture;    // Refers to a @Picture from which the source #Bitmap is acquired.
   objBitmap * Bitmap;      // Reference to a source bitmap for the rendering algorithm.
   LONG   Units;            // Declares the coordinate system to use for the #X and #Y values.
   LONG   Dimensions;       // Dimension flags define whether individual dimension fields contain fixed or relative values.
   LONG   SpreadMethod;     // Defines the drawing mode.
   LONG   AspectRatio;      // Flags that affect the aspect ratio of the image within its target vector.
};

// VectorPattern class definition

#define VER_VECTORPATTERN (1.000000)

class objVectorPattern : public BaseClass {
   public:
   DOUBLE X;                      // X coordinate for the pattern.
   DOUBLE Y;                      // Y coordinate for the pattern.
   DOUBLE Width;                  // Width of the pattern tile.
   DOUBLE Height;                 // Height of the pattern tile.
   DOUBLE Opacity;                // The opacity of the pattern.
   objVectorScene * Scene;        // Refers to the internal @VectorScene that will contain the rendered pattern.
   objVectorPattern * Inherit;    // Inherit attributes from a VectorPattern referenced here.
   LONG   SpreadMethod;           // The behaviour to use when the pattern bounds do not match the vector path.
   LONG   Units;                  // Defines the coordinate system for fields X, Y, Width and Height.
   LONG   ContentUnits;           // Private. Not yet implemented.
   LONG   Dimensions;             // Dimension flags are stored here.
};

// VectorGradient class definition

#define VER_VECTORGRADIENT (1.000000)

class objVectorGradient : public BaseClass {
   public:
   DOUBLE X1;                      // Initial X coordinate for the gradient.
   DOUBLE Y1;                      // Initial Y coordinate for the gradient.
   DOUBLE X2;                      // Final X coordinate for the gradient.
   DOUBLE Y2;                      // Final Y coordinate for the gradient.
   DOUBLE CenterX;                 // The horizontal center point of the gradient.
   DOUBLE CenterY;                 // The vertical center point of the gradient.
   DOUBLE FX;                      // The horizontal focal point for radial gradients.
   DOUBLE FY;                      // The vertical focal point for radial gradients.
   DOUBLE Radius;                  // The radius of the gradient.
   objVectorGradient * Inherit;    // Inherit attributes from the VectorGradient referenced here.
   LONG   SpreadMethod;            // The behaviour to use when the gradient bounds do not match the vector path.
   LONG   Units;                   // Defines the coordinate system for fields X1, Y1, X2 and Y2.
   LONG   Type;                    // Specifies the type of gradient (e.g. RADIAL, LINEAR)
   LONG   Flags;                   // Dimension flags are stored here.
   LONG   ColourSpace;             // Defines the colour space to use when interpolating gradient colours.
   LONG   TotalStops;              // Total number of stops defined in the Stops array.
   // Action stubs

   inline ERROR init() { return Action(AC_Init, this, NULL); }
};

// FilterEffect class definition

#define VER_FILTEREFFECT (1.000000)

class objFilterEffect : public BaseClass {
   public:
   objFilterEffect * Next;    // Next filter in the chain.
   objFilterEffect * Prev;    // Previous filter in the chain.
   objBitmap * Target;        // Target bitmap for rendering the effect.
   objFilterEffect * Input;   // Reference to another effect to be used as an input source.
   objFilterEffect * Mix;     // Reference to another effect to be used a mixer with Input.
   DOUBLE X;                  // Primitive X coordinate for the effect.
   DOUBLE Y;                  // Primitive Y coordinate for the effect.
   DOUBLE Width;              // Primitive width of the effect area.
   DOUBLE Height;             // Primitive height of the effect area.
   LONG   Dimensions;         // Dimension flags are stored here.
   LONG   SourceType;         // Specifies an input source for the effect algorithm, if required.
   LONG   MixType;            // If a secondary mix input is required for the effect, specify it here.
   // Action stubs

   inline ERROR init() { return Action(AC_Init, this, NULL); }
   inline ERROR moveToBack() { return Action(AC_MoveToBack, this, NULL); }
   inline ERROR moveToFront() { return Action(AC_MoveToFront, this, NULL); }
};

struct MergeSource {
   LONG SourceType;             // The type of the required source.
   objFilterEffect * Effect;    // Effect pointer if the SourceType is REFERENCE.
};

// ImageFX class definition

#define VER_IMAGEFX (1.000000)

// SourceFX class definition

#define VER_SOURCEFX (1.000000)

// BlurFX class definition

#define VER_BLURFX (1.000000)

// ColourFX class definition

#define VER_COLOURFX (1.000000)

// CompositeFX class definition

#define VER_COMPOSITEFX (1.000000)

// ConvolveFX class definition

#define VER_CONVOLVEFX (1.000000)

// DisplacementFX class definition

#define VER_DISPLACEMENTFX (1.000000)

// FloodFX class definition

#define VER_FLOODFX (1.000000)

// LightingFX class definition

#define VER_LIGHTINGFX (1.000000)

// LightingFX methods

#define MT_LTSetDistantLight -20
#define MT_LTSetPointLight -22
#define MT_LTSetSpotLight -21

struct ltSetDistantLight { DOUBLE Azimuth; DOUBLE Elevation;  };
struct ltSetPointLight { DOUBLE X; DOUBLE Y; DOUBLE Z;  };
struct ltSetSpotLight { DOUBLE X; DOUBLE Y; DOUBLE Z; DOUBLE PX; DOUBLE PY; DOUBLE PZ; DOUBLE Exponent; DOUBLE ConeAngle;  };

INLINE ERROR ltSetDistantLight(APTR Ob, DOUBLE Azimuth, DOUBLE Elevation) {
   struct ltSetDistantLight args = { Azimuth, Elevation };
   return(Action(MT_LTSetDistantLight, (OBJECTPTR)Ob, &args));
}

INLINE ERROR ltSetPointLight(APTR Ob, DOUBLE X, DOUBLE Y, DOUBLE Z) {
   struct ltSetPointLight args = { X, Y, Z };
   return(Action(MT_LTSetPointLight, (OBJECTPTR)Ob, &args));
}

INLINE ERROR ltSetSpotLight(APTR Ob, DOUBLE X, DOUBLE Y, DOUBLE Z, DOUBLE PX, DOUBLE PY, DOUBLE PZ, DOUBLE Exponent, DOUBLE ConeAngle) {
   struct ltSetSpotLight args = { X, Y, Z, PX, PY, PZ, Exponent, ConeAngle };
   return(Action(MT_LTSetSpotLight, (OBJECTPTR)Ob, &args));
}


// MergeFX class definition

#define VER_MERGEFX (1.000000)

// MorphologyFX class definition

#define VER_MORPHOLOGYFX (1.000000)

// OffsetFX class definition

#define VER_OFFSETFX (1.000000)

// RemapFX class definition

#define VER_REMAPFX (1.000000)

// RemapFX methods

#define MT_RFSelectGamma -20
#define MT_RFSelectTable -21
#define MT_RFSelectLinear -22
#define MT_RFSelectIdentity -23
#define MT_RFSelectDiscrete -24
#define MT_RFSelectInvert -25
#define MT_RFSelectMask -26

struct rfSelectGamma { LONG Component; DOUBLE Amplitude; DOUBLE Offset; DOUBLE Exponent;  };
struct rfSelectTable { LONG Component; DOUBLE * Values; LONG Size;  };
struct rfSelectLinear { LONG Component; DOUBLE Slope; DOUBLE Intercept;  };
struct rfSelectIdentity { LONG Component;  };
struct rfSelectDiscrete { LONG Component; DOUBLE * Values; LONG Size;  };
struct rfSelectInvert { LONG Component;  };
struct rfSelectMask { LONG Component; LONG Mask;  };

INLINE ERROR rfSelectGamma(APTR Ob, LONG Component, DOUBLE Amplitude, DOUBLE Offset, DOUBLE Exponent) {
   struct rfSelectGamma args = { Component, Amplitude, Offset, Exponent };
   return(Action(MT_RFSelectGamma, (OBJECTPTR)Ob, &args));
}

INLINE ERROR rfSelectTable(APTR Ob, LONG Component, DOUBLE * Values, LONG Size) {
   struct rfSelectTable args = { Component, Values, Size };
   return(Action(MT_RFSelectTable, (OBJECTPTR)Ob, &args));
}

INLINE ERROR rfSelectLinear(APTR Ob, LONG Component, DOUBLE Slope, DOUBLE Intercept) {
   struct rfSelectLinear args = { Component, Slope, Intercept };
   return(Action(MT_RFSelectLinear, (OBJECTPTR)Ob, &args));
}

INLINE ERROR rfSelectIdentity(APTR Ob, LONG Component) {
   struct rfSelectIdentity args = { Component };
   return(Action(MT_RFSelectIdentity, (OBJECTPTR)Ob, &args));
}

INLINE ERROR rfSelectDiscrete(APTR Ob, LONG Component, DOUBLE * Values, LONG Size) {
   struct rfSelectDiscrete args = { Component, Values, Size };
   return(Action(MT_RFSelectDiscrete, (OBJECTPTR)Ob, &args));
}

INLINE ERROR rfSelectInvert(APTR Ob, LONG Component) {
   struct rfSelectInvert args = { Component };
   return(Action(MT_RFSelectInvert, (OBJECTPTR)Ob, &args));
}

INLINE ERROR rfSelectMask(APTR Ob, LONG Component, LONG Mask) {
   struct rfSelectMask args = { Component, Mask };
   return(Action(MT_RFSelectMask, (OBJECTPTR)Ob, &args));
}


// TurbulenceFX class definition

#define VER_TURBULENCEFX (1.000000)

// VectorFilter class definition

#define VER_VECTORFILTER (1.000000)

class objVectorFilter : public BaseClass {
   public:
   DOUBLE X;                     // X coordinate for the filter.
   DOUBLE Y;                     // Y coordinate for the filter.
   DOUBLE Width;                 // The width of the filter area.  Can be expressed as a fixed or relative coordinate.
   DOUBLE Height;                // The height of the filter area.  Can be expressed as a fixed or relative coordinate.
   DOUBLE Opacity;               // The opacity of the filter.
   objVectorFilter * Inherit;    // Inherit attributes from a VectorFilter referenced here.
   LONG   ResX;                  // Width of the intermediate images, measured in pixels.
   LONG   ResY;                  // Height of the intermediate images, measured in pixels.
   LONG   Units;                 // Defines the coordinate system for fields X, Y, Width and Height.
   LONG   PrimitiveUnits;        // Alters the behaviour of some effects that support alternative position calculations.
   LONG   Dimensions;            // Dimension flags define whether individual dimension fields contain fixed or relative values.
   LONG   ColourSpace;           // The colour space of the filter graphics (SRGB or linear RGB).
   // Action stubs

   inline ERROR clear() { return Action(AC_Clear, this, NULL); }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
};

// Vector class definition

#define VER_VECTOR (1.000000)

// Vector methods

#define MT_VecPush -1
#define MT_VecTracePath -2
#define MT_VecGetBoundary -3
#define MT_VecPointInPath -4
#define MT_VecSubscribeInput -5
#define MT_VecSubscribeKeyboard -6
#define MT_VecSubscribeFeedback -7
#define MT_VecDebug -8
#define MT_VecNewMatrix -9
#define MT_VecFreeMatrix -10

struct vecPush { LONG Position;  };
struct vecTracePath { FUNCTION * Callback;  };
struct vecGetBoundary { LONG Flags; DOUBLE X; DOUBLE Y; DOUBLE Width; DOUBLE Height;  };
struct vecPointInPath { DOUBLE X; DOUBLE Y;  };
struct vecSubscribeInput { LONG Mask; FUNCTION * Callback;  };
struct vecSubscribeKeyboard { FUNCTION * Callback;  };
struct vecSubscribeFeedback { LONG Mask; FUNCTION * Callback;  };
struct vecNewMatrix { struct VectorMatrix * Transform;  };
struct vecFreeMatrix { struct VectorMatrix * Matrix;  };

INLINE ERROR vecPush(APTR Ob, LONG Position) {
   struct vecPush args = { Position };
   return(Action(MT_VecPush, (OBJECTPTR)Ob, &args));
}

INLINE ERROR vecTracePath(APTR Ob, FUNCTION * Callback) {
   struct vecTracePath args = { Callback };
   return(Action(MT_VecTracePath, (OBJECTPTR)Ob, &args));
}

INLINE ERROR vecGetBoundary(APTR Ob, LONG Flags, DOUBLE * X, DOUBLE * Y, DOUBLE * Width, DOUBLE * Height) {
   struct vecGetBoundary args = { Flags, 0, 0, 0, 0 };
   ERROR error = Action(MT_VecGetBoundary, (OBJECTPTR)Ob, &args);
   if (X) *X = args.X;
   if (Y) *Y = args.Y;
   if (Width) *Width = args.Width;
   if (Height) *Height = args.Height;
   return(error);
}

INLINE ERROR vecPointInPath(APTR Ob, DOUBLE X, DOUBLE Y) {
   struct vecPointInPath args = { X, Y };
   return(Action(MT_VecPointInPath, (OBJECTPTR)Ob, &args));
}

INLINE ERROR vecSubscribeInput(APTR Ob, LONG Mask, FUNCTION * Callback) {
   struct vecSubscribeInput args = { Mask, Callback };
   return(Action(MT_VecSubscribeInput, (OBJECTPTR)Ob, &args));
}

INLINE ERROR vecSubscribeKeyboard(APTR Ob, FUNCTION * Callback) {
   struct vecSubscribeKeyboard args = { Callback };
   return(Action(MT_VecSubscribeKeyboard, (OBJECTPTR)Ob, &args));
}

INLINE ERROR vecSubscribeFeedback(APTR Ob, LONG Mask, FUNCTION * Callback) {
   struct vecSubscribeFeedback args = { Mask, Callback };
   return(Action(MT_VecSubscribeFeedback, (OBJECTPTR)Ob, &args));
}

#define vecDebug(obj) Action(MT_VecDebug,(obj),0)

INLINE ERROR vecNewMatrix(APTR Ob, struct VectorMatrix ** Transform) {
   struct vecNewMatrix args = { 0 };
   ERROR error = Action(MT_VecNewMatrix, (OBJECTPTR)Ob, &args);
   if (Transform) *Transform = args.Transform;
   return(error);
}

INLINE ERROR vecFreeMatrix(APTR Ob, struct VectorMatrix * Matrix) {
   struct vecFreeMatrix args = { Matrix };
   return(Action(MT_VecFreeMatrix, (OBJECTPTR)Ob, &args));
}


class objVector : public BaseClass {
   public:
   objVector * Child;                 // The first child vector, or NULL.
   objVectorScene * Scene;            // Short-cut to the top-level @VectorScene.
   objVector * Next;                  // The next vector in the branch, or NULL.
   objVector * Prev;                  // The previous vector in the branch, or NULL.
   OBJECTPTR Parent;                  // The parent of the vector, or NULL if this is the top-most vector.
   struct VectorMatrix * Matrices;    // A linked list of transform matrices that have been applied to the vector.
   DOUBLE    StrokeOpacity;           // Defines the opacity of the path stroke.
   DOUBLE    FillOpacity;             // The opacity to use when filling the vector.
   DOUBLE    Opacity;                 // Defines an overall opacity for the vector's graphics.
   DOUBLE    MiterLimit;              // Imposes a limit on the ratio of the miter length to the StrokeWidth.
   DOUBLE    InnerMiterLimit;         // Private. No internal documentation exists for this feature.
   DOUBLE    DashOffset;              // The distance into the dash pattern to start the dash.  Can be a negative number.
   LONG      Visibility;              // Controls the visibility of a vector and its children.
   LONG      Flags;                   // Optional flags.
   LONG      Cursor;                  // The mouse cursor to display when the pointer is within the vector's boundary.
   LONG      PathQuality;             // Defines the quality of a path when it is rendered.
   LONG      ColourSpace;             // Defines the colour space to use when blending the vector with a target bitmap's content.
   // Action stubs

   inline ERROR disable() { return Action(AC_Disable, this, NULL); }
   inline ERROR draw() { return Action(AC_Draw, this, NULL); }
   inline ERROR drawArea(LONG X, LONG Y, LONG Width, LONG Height) {
      struct acDraw args = { X, Y, Width, Height };
      return Action(AC_Draw, this, &args);
   }
   inline ERROR enable() { return Action(AC_Enable, this, NULL); }
   inline ERROR hide() { return Action(AC_Hide, this, NULL); }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
   inline ERROR moveToBack() { return Action(AC_MoveToBack, this, NULL); }
   inline ERROR moveToFront() { return Action(AC_MoveToFront, this, NULL); }
   inline ERROR show() { return Action(AC_Show, this, NULL); }
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
   return(Action(MT_VPAddCommand, (OBJECTPTR)Ob, &args));
}

INLINE ERROR vpRemoveCommand(APTR Ob, LONG Index, LONG Total) {
   struct vpRemoveCommand args = { Index, Total };
   return(Action(MT_VPRemoveCommand, (OBJECTPTR)Ob, &args));
}

INLINE ERROR vpSetCommand(APTR Ob, LONG Index, struct PathCommand * Command, LONG Size) {
   struct vpSetCommand args = { Index, Command, Size };
   return(Action(MT_VPSetCommand, (OBJECTPTR)Ob, &args));
}

INLINE ERROR vpGetCommand(APTR Ob, LONG Index, struct PathCommand ** Command) {
   struct vpGetCommand args = { Index, 0 };
   ERROR error = Action(MT_VPGetCommand, (OBJECTPTR)Ob, &args);
   if (Command) *Command = args.Command;
   return(error);
}

INLINE ERROR vpSetCommandList(APTR Ob, APTR Commands, LONG Size) {
   struct vpSetCommandList args = { Commands, Size };
   return(Action(MT_VPSetCommandList, (OBJECTPTR)Ob, &args));
}


// VectorText class definition

#define VER_VECTORTEXT (1.000000)

// VectorText methods

#define MT_VTDeleteLine -30

struct vtDeleteLine { LONG Line;  };

INLINE ERROR vtDeleteLine(APTR Ob, LONG Line) {
   struct vtDeleteLine args = { Line };
   return(Action(MT_VTDeleteLine, (OBJECTPTR)Ob, &args));
}


// VectorWave class definition

#define VER_VECTORWAVE (1.000000)

// VectorRectangle class definition

#define VER_VECTORRECTANGLE (1.000000)

// VectorPolygon class definition

#define VER_VECTORPOLYGON (1.000000)

// VectorShape class definition

#define VER_VECTORSHAPE (1.000000)

// VectorSpiral class definition

#define VER_VECTORSPIRAL (1.000000)

// VectorEllipse class definition

#define VER_VECTORELLIPSE (1.000000)

// VectorClip class definition

#define VER_VECTORCLIP (1.000000)

// VectorViewport class definition

#define VER_VECTORVIEWPORT (1.000000)

class objVectorViewport : public objVector {
};

struct VectorBase {
   ERROR (*_DrawPath)(objBitmap *, APTR, DOUBLE, OBJECTPTR, OBJECTPTR);
   void (*_FreePath)(APTR);
   ERROR (*_GenerateEllipse)(DOUBLE, DOUBLE, DOUBLE, DOUBLE, LONG, APTR);
   ERROR (*_GeneratePath)(CSTRING, APTR);
   ERROR (*_GenerateRectangle)(DOUBLE, DOUBLE, DOUBLE, DOUBLE, APTR);
   ERROR (*_ReadPainter)(objVectorScene *, CSTRING, struct FRGB *, objVectorGradient **, objVectorImage **, objVectorPattern **);
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
   ERROR (*_ApplyPath)(APTR, OBJECTPTR);
   ERROR (*_Rotate)(struct VectorMatrix *, DOUBLE, DOUBLE, DOUBLE);
   ERROR (*_Translate)(struct VectorMatrix *, DOUBLE, DOUBLE);
   ERROR (*_Skew)(struct VectorMatrix *, DOUBLE, DOUBLE);
   ERROR (*_Multiply)(struct VectorMatrix *, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE);
   ERROR (*_MultiplyMatrix)(struct VectorMatrix *, struct VectorMatrix *);
   ERROR (*_Scale)(struct VectorMatrix *, DOUBLE, DOUBLE);
   ERROR (*_ParseTransform)(struct VectorMatrix *, CSTRING);
   ERROR (*_ResetMatrix)(struct VectorMatrix *);
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
#define vecRotate(...) (VectorBase->_Rotate)(__VA_ARGS__)
#define vecTranslate(...) (VectorBase->_Translate)(__VA_ARGS__)
#define vecSkew(...) (VectorBase->_Skew)(__VA_ARGS__)
#define vecMultiply(...) (VectorBase->_Multiply)(__VA_ARGS__)
#define vecMultiplyMatrix(...) (VectorBase->_MultiplyMatrix)(__VA_ARGS__)
#define vecScale(...) (VectorBase->_Scale)(__VA_ARGS__)
#define vecParseTransform(...) (VectorBase->_ParseTransform)(__VA_ARGS__)
#define vecResetMatrix(...) (VectorBase->_ResetMatrix)(__VA_ARGS__)
#endif

//****************************************************************************

INLINE void SET_VECTOR_COLOUR(objVectorColour *Colour, DOUBLE Red, DOUBLE Green, DOUBLE Blue, DOUBLE Alpha) {
   Colour->ClassID = ID_VECTORCOLOUR;
   Colour->Red   = Red;
   Colour->Green = Green;
   Colour->Blue  = Blue;
   Colour->Alpha = Alpha;
}
  
#define SVF_A 0x0002b606
#define SVF_ACHROMATOMALY 0xc3f37036
#define SVF_ACHROMATOPSIA 0xc3f56170
#define SVF_ALIGN 0x0f174e50
#define SVF_AMPLITUDE 0x5e60600a
#define SVF_ANIMATEMOTION 0x8a27c6ba
#define SVF_ANIMATETRANSFORM 0x6349c940
#define SVF_ARITHMETIC 0x600354ef
#define SVF_ATOP 0x7c943c79
#define SVF_B 0x0002b607
#define SVF_BACKGROUNDALPHA 0xaa3afeab
#define SVF_BACKGROUNDIMAGE 0xaacc0f28
#define SVF_BASEFREQUENCY 0xea1938b2
#define SVF_BASEPROFILE 0xca40f031
#define SVF_BEVEL 0x0f25c733
#define SVF_BIAS 0x7c949844
#define SVF_BOTTOM 0xf492ca7a
#define SVF_BRIGHTNESS 0x7bdc2cbe
#define SVF_BURN 0x7c94cd7c
#define SVF_BUTT 0x7c94cdc4
#define SVF_CIRCLE 0xf679fe97
#define SVF_CLIP_PATH 0x455423a7
#define SVF_CLIP_RULE 0x45559072
#define SVF_CLIP_RULE 0x45559072
#define SVF_CLIPPATH 0x4fd1b75a
#define SVF_CLIPPATHUNITS 0x94efb24d
#define SVF_CLOSE 0x0f3b9a5b
#define SVF_COLOR 0x0f3d3244
#define SVF_COLOUR 0xf6e37b99
#define SVF_COLOR_INTERPOLATION 0x6f2c0659
#define SVF_COLOUR_INTERPOLATION 0x5655806e
#define SVF_COLOR_INTERPOLATION_FILTERS 0x752d48ff
#define SVF_COLOUR_INTERPOLATION_FILTERS 0x51660814
#define SVF_COLOURISE 0xf3cb4eda
#define SVF_CONTOURGRADIENT 0x82a83fdd
#define SVF_CONTRAST 0x42b3b373
#define SVF_CX 0x00597780
#define SVF_CY 0x00597781
#define SVF_D 0x0002b609
#define SVF_DARKEN 0xf83e845a
#define SVF_DECAY 0x0f49a6eb
#define SVF_DEFS 0x7c95a0a7
#define SVF_DESATURATE 0x226696d7
#define SVF_DESC 0x7c95a244
#define SVF_DEUTERANOMALY 0xe42f689f
#define SVF_DEUTERANOPIA 0x1e300926
#define SVF_DIFFERENCE 0x52a92470
#define SVF_DISPLAY 0x12cd479b
#define SVF_DIVISOR 0x12ffda05
#define SVF_DODGE 0x0f4f27a8
#define SVF_DUR 0x0b886bd0
#define SVF_DX 0x005977a1
#define SVF_DY 0x005977a2
#define SVF_EDGEMODE 0xbb10b09f
#define SVF_ELLIPSE 0x66448f53
#define SVF_ENABLE_BACKGROUND 0xa1e664d9
#define SVF_EXCLUSION 0x6f499bff
#define SVF_EXTERNALRESOURCESREQUIRED 0x582d0624
#define SVF_FEBLEND 0xa2373055
#define SVF_FEBLUR 0xfd2877e5
#define SVF_FECOLORMATRIX 0x92252784
#define SVF_FECOLOURMATRIX 0x371a19f9
#define SVF_FECOMPONENTTRANSFER 0xf4fa6788
#define SVF_FECOMPOSITE 0xf71764e3
#define SVF_FECONVOLVEMATRIX 0x0b05cd91
#define SVF_FEDIFFUSELIGHTING 0xf094ecac
#define SVF_FEDISPLACEMENTMAP 0xb9cf0a67
#define SVF_FEDISTANTLIGHT 0x12a0c2ff
#define SVF_FEFLOOD 0xa27fbd04
#define SVF_FEGAUSSIANBLUR 0xfdba17c0
#define SVF_FEIMAGE 0xa2b65653
#define SVF_FEMERGE 0xa2fa9da0
#define SVF_FEMORPHOLOGY 0x8f1be720
#define SVF_FEOFFSET 0x07045a57
#define SVF_FEPOINTLIGHT 0xcebc7c12
#define SVF_FESPECULARLIGHTING 0x68af6ee5
#define SVF_FESPOTLIGHT 0xce2d968e
#define SVF_FETILE 0xfd3248be
#define SVF_FETURBULENCE 0x4eba1da9
#define SVF_FILL 0x7c96cb2c
#define SVF_FILL_OPACITY 0x59fd2152
#define SVF_FILL_RULE 0xbb9f7891
#define SVF_FILLPAINT 0xc0525d28
#define SVF_FILTER 0xfd7675ab
#define SVF_FILTERUNITS 0x5a2d0b3e
#define SVF_FLOOD_COLOR 0x37459885
#define SVF_FLOOD_COLOUR 0x1ff8a9fa
#define SVF_FLOOD_OPACITY 0xbc50167f
#define SVF_FONT 0x7c96e4fc
#define SVF_FONT_FAMILY 0x673faacb
#define SVF_FONT_SIZE 0xf1c88f84
#define SVF_FONT_SIZE_ADJUST 0x2a32397c
#define SVF_FONT_STRETCH 0x64948686
#define SVF_FONT_STYLE 0x2ae0853a
#define SVF_FONT_VARIANT 0x1f331afe
#define SVF_FONT_WEIGHT 0x8f2d84f1
#define SVF_FREQUENCY 0xffd1bad7
#define SVF_FROM 0x7c96f1d9
#define SVF_FX 0x005977e3
#define SVF_FY 0x005977e4
#define SVF_G 0x0002b60c
#define SVF_GRADIENTTRANSFORM 0x31ccfa2f
#define SVF_GRADIENTUNITS 0x6c7c4886
#define SVF_HARDLIGHT 0x022cb75c
#define SVF_HEIGHT 0x01d688de
#define SVF_HUE 0x0b887cc7
#define SVF_HUEROTATE 0xaf80b596
#define SVF_ID 0x00597832
#define SVF_IMAGE 0x0fa87ca8
#define SVF_IMAGE 0x0fa87ca8
#define SVF_IMAGE_RENDERING 0xfdb735d3
#define SVF_IN 0x0059783c
#define SVF_IN2 0x0b887fee
#define SVF_INHERIT 0x9e8d4758
#define SVF_INVERT 0x04d5a7bd
#define SVF_INVERT_X_AXIS 0xa4fb3664
#define SVF_INVERT_Y_AXIS 0xa7505f05
#define SVF_INVERTRGB 0xacb1dd38
#define SVF_JAG 0x0b8882b7
#define SVF_K1 0x00597841
#define SVF_K2 0x00597842
#define SVF_K3 0x00597843
#define SVF_K4 0x00597844
#define SVF_KERNELMATRIX 0xfb05405b
#define SVF_KERNELUNITLENGTH 0x05c04f48
#define SVF_KERNING 0x243d11f3
#define SVF_LENGTHADJUST 0x748cbc92
#define SVF_LETTER_SPACING 0x982bebc7
#define SVF_LIGHTEN 0x79c1c710
#define SVF_LINE 0x7c9a15ad
#define SVF_LINEARGRADIENT 0xe6871dce
#define SVF_LUMINANCETOALPHA 0xc6ee7d8a
#define SVF_M 0x0002b612
#define SVF_MARKER 0x0d3cf207
#define SVF_MARKER_END 0x66ff06cb
#define SVF_MARKER_MID 0x66ff282e
#define SVF_MARKER_START 0x23dc8942
#define SVF_MASK 0x7c9a80b1
#define SVF_MATRIX 0x0d3e291a
#define SVF_METHOD 0x0d866146
#define SVF_MINUS 0x0feee651
#define SVF_MITER 0x0feefdc6
#define SVF_MITER_REVERT 0x7bc9e50b
#define SVF_MITER_ROUND 0x1349a65b
#define SVF_MOD 0x0b889145
#define SVF_MODE 0x7c9aba4a
#define SVF_MULTIPLY 0x46746f05
#define SVF_N1 0x005978a4
#define SVF_N2 0x005978a5
#define SVF_N3 0x005978a6
#define SVF_NONE 0x7c9b47f5
#define SVF_NUMERIC_ID 0x3768b852
#define SVF_NUMOCTAVES 0x16f8e14a
#define SVF_OFFSET 0x123b4b4c
#define SVF_OPACITY 0x70951bfe
#define SVF_OPERATOR 0x8d9849f1
#define SVF_ORDER 0x1017da21
#define SVF_OUT 0x0b889a9d
#define SVF_OVER 0x7c9bf101
#define SVF_OVERFLOW 0x5b785259
#define SVF_OVERLAY 0x7ee4b5c7
#define SVF_PARASOL_MORPH 0x6b51bb77
#define SVF_PARASOL_PATHTRANSITION 0x9d3c64a9
#define SVF_PARASOL_SHAPE 0x6bba2f82
#define SVF_PARASOL_SPIRAL 0xe3954f3c
#define SVF_PARASOL_TRANSITION 0xc0f6617c
#define SVF_PARASOL_WAVE 0xbd7455e4
#define SVF_PATH 0x7c9c25f2
#define SVF_PATH 0x7c9c25f2
#define SVF_PATHLENGTH 0x74403974
#define SVF_PATTERN 0x9bf30a03
#define SVF_PATTERNCONTENTUNITS 0x6bc53e31
#define SVF_PATTERNTRANSFORM 0x6495503f
#define SVF_PATTERNUNITS 0x6eec1696
#define SVF_PHI 0x0b889d26
#define SVF_PLUS 0x7c9c54e9
#define SVF_POINTS 0x1534e242
#define SVF_POLYGON 0xbc0d44cd
#define SVF_POLYLINE 0x3db88331
#define SVF_PRESERVEALPHA 0xf9b49d57
#define SVF_PRIMITIVEUNITS 0xf4494b91
#define SVF_PROTANOMALY 0xd3f5b4fb
#define SVF_PROTANOPIA 0x15f03a02
#define SVF_R 0x0002b617
#define SVF_RADIALGRADIENT 0x4016b4c0
#define SVF_RADIUS 0x18df096d
#define SVF_RECT 0x7c9d4d93
#define SVF_REPEAT 0x192dec66
#define SVF_RESULT 0x192fd704
#define SVF_ROTATE 0x19e50454
#define SVF_ROUND 0x104cc7ed
#define SVF_RX 0x0059796f
#define SVF_RY 0x00597970
#define SVF_SATURATE 0xdf32bb4e
#define SVF_SCALE 0x1057f68d
#define SVF_SCREEN 0x1b5ffd45
#define SVF_SEED 0x7c9dda26
#define SVF_SHAPE_RENDERING 0xeecea7a1
#define SVF_SOFTLIGHT 0x78b6e7b9
#define SVF_SOURCEALPHA 0xbe4b853c
#define SVF_SOURCEGRAPHIC 0x5a1343b4
#define SVF_SPACING 0xa47e0e2a
#define SVF_SPIRAL 0x1c468330
#define SVF_SPREADMETHOD 0x0caafac5
#define SVF_SQUARE 0x1c5eea16
#define SVF_STARTOFFSET 0xed10629a
#define SVF_STDDEVIATION 0x861007d3
#define SVF_STEP 0x7c9e1a01
#define SVF_STITCHTILES 0x3d844d95
#define SVF_STRING 0x1c93affc
#define SVF_STYLE 0x1061af16
#define SVF_SVG 0x0b88abb5
#define SVF_SYMBOL 0x1ceb4efb
#define SVF_TARGETX 0xcfb0ab64
#define SVF_TARGETY 0xcfb0ab65
#define SVF_TEXT 0x7c9e690a
#define SVF_TEXTPATH 0x089ef477
#define SVF_THICKNESS 0x369e2871
#define SVF_TITLE 0x106daa27
#define SVF_TO 0x005979a8
#define SVF_TOP 0x0b88af18
#define SVF_TRANSITION 0x96486f70
#define SVF_TRITANOMALY 0x2e7de3f9
#define SVF_TRITANOPIA 0x9c8f8140
#define SVF_VERTEX_SCALING 0x2363f691
#define SVF_VERTICES 0xd31fda6a
#define SVF_X 0x0002b61d
#define SVF_X1 0x005979ee
#define SVF_X2 0x005979ef
#define SVF_XLINK_HREF 0x379480aa
#define SVF_XML_SPACE 0x2db612fc
#define SVF_XMLNS 0x10b81bf7
#define SVF_XOR 0x0b88c01e
#define SVF_Y 0x0002b61e
#define SVF_Y1 0x00597a0f
#define SVF_Y2 0x00597a10
#define SVF_Z 0x0002b61f

#define SVF_ACCUMULATE 0x5c660bc9
#define SVF_ADDITIVE 0x035604af
#define SVF_ALICEBLUE 0x41f60f4b
#define SVF_ANTIQUEWHITE 0x3a2d20fd
#define SVF_AQUA 0x7c94306d
#define SVF_AQUAMARINE 0x52e1f409
#define SVF_ATTRIBUTENAME 0x658ead7a
#define SVF_ATTRIBUTETYPE 0x65925e3b
#define SVF_AZURE 0x0f1f300c
#define SVF_BEGIN 0x0f2587ea
#define SVF_BEIGE 0x0f259021
#define SVF_BISQUE 0xf4259f0e
#define SVF_BLACK 0x0f294442
#define SVF_BLANCHEDALMOND 0x25a17751
#define SVF_BLINK 0x0f2967b5
#define SVF_BLUE 0x7c94a78d
#define SVF_BLUEVIOLET 0x59f4db60
#define SVF_BOLD 0x7c94b326
#define SVF_BOLDER 0xf48e221d
#define SVF_BROWN 0x0f2cccad
#define SVF_BURLYWOOD 0xd00306ac
#define SVF_CADETBLUE 0x88f15cae
#define SVF_CHARTREUSE 0xfb91543b
#define SVF_CHOCOLATE 0x487f4c37
#define SVF_CLASS 0x0f3b5edb
#define SVF_CONDENSED 0x72f37898
#define SVF_CONICGRADIENT 0x9a0996df
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
#define SVF_DIAMONDGRADIENT 0xe8db24af
#define SVF_DIMGRAY 0x125bdeb2
#define SVF_DIMGREY 0x125bdf36
#define SVF_DODGERBLUE 0x8208b222
#define SVF_END 0x0b886f1c
#define SVF_EXTRA_CONDENSED 0x4cb18509
#define SVF_FILTERRES 0xd23e0c35
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
#define SVF_HREF 0x7c98094a
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
#define SVF_LIGHTER 0x79c1c714
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
#define SVF_LINETHROUGH 0xf69720ce
#define SVF_MAGENTA 0xb4110202
#define SVF_MAROON 0x0d3d0451
#define SVF_MAX 0x0b888f8b
#define SVF_MEDIUMAQUAMARINE 0x5393448a
#define SVF_MEDIUMBLUE 0xd877eb4e
#define SVF_MEDIUMORCHID 0xf4d5d5df
#define SVF_MEDIUMPURPLE 0xf769a41e
#define SVF_MEDIUMSEAGREEN 0x453d9eb0
#define SVF_MEDIUMSLATEBLUE 0x80249267
#define SVF_MEDIUMSPRINGGREEN 0x814643ca
#define SVF_MEDIUMTURQUOISE 0x59b8aa37
#define SVF_MEDIUMVIOLETRED 0x3be46a94
#define SVF_MIDDLE 0x0dc5ebd4
#define SVF_MIDNIGHTBLUE 0x5f9313a1
#define SVF_MIN 0x0b889089
#define SVF_MINTCREAM 0x9b7533e5
#define SVF_MISTYROSE 0x1de6ab94
#define SVF_MOCCASIN 0x62609d92
#define SVF_NARROWER 0x3d07aeb5
#define SVF_NAVAJOWHITE 0xe2bc6625
#define SVF_NAVY 0x7c9b0d83
#define SVF_NORMAL 0x108f79ae
#define SVF_OLDLACE 0x677b8e19
#define SVF_OLIVE 0x1014a744
#define SVF_OLIVEDRAB 0xcd1770fd
#define SVF_ORANGE 0x13119e61
#define SVF_ORANGERED 0xdc4c011c
#define SVF_ORCHID 0x13129dde
#define SVF_OVERLINE 0x5b7b8fa9
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
#define SVF_PRESERVEASPECTRATIO 0x195673f0
#define SVF_PURPLE 0x15a66c1d
#define SVF_RED 0x0b88a540
#define SVF_REPEATCOUNT 0x53edf46f
#define SVF_REPEATDUR 0xa7b01391
#define SVF_RESTART 0x3f29fc8a
#define SVF_ROSYBROWN 0xf7e975fa
#define SVF_ROYALBLUE 0x8e773554
#define SVF_SADDLEBROWN 0x92bbf35a
#define SVF_SALMON 0x1b38a54f
#define SVF_SANDYBROWN 0xe10b172c
#define SVF_SEAGREEN 0xe5cc626f
#define SVF_SEASHELL 0xe6a00d96
#define SVF_SEMI_CONDENSED 0xbc1627b3
#define SVF_SIENNA 0x1bc596c3
#define SVF_SILVER 0x1bc98e5a
#define SVF_SKYBLUE 0x9a861064
#define SVF_SLATEBLUE 0x328bce06
#define SVF_SLATEGRAY 0x328ea2f1
#define SVF_SLATEGREY 0x328ea375
#define SVF_SNOW 0x7c9e01cc
#define SVF_SPRINGGREEN 0x6a6ae329
#define SVF_START 0x106149d3
#define SVF_STEELBLUE 0xa604b22a
#define SVF_STROKE 0x1c93c91d
#define SVF_STROKE_DASHARRAY 0x5faa6be9
#define SVF_STROKE_DASHOFFSET 0x74c0b1b1
#define SVF_STROKE_INNER_MITERLIMIT 0x8ab099f3
#define SVF_STROKE_INNERJOIN 0x1ebcf876
#define SVF_STROKE_LINECAP 0xe476e8e6
#define SVF_STROKE_LINEJOIN 0x73581762
#define SVF_STROKE_MITERLIMIT 0x49c40b8a
#define SVF_STROKE_MITERLIMIT_THETA 0x3dab0e2d
#define SVF_STROKE_OPACITY 0xdacd8043
#define SVF_STROKE_WIDTH 0xa27c3faa
#define SVF_STROKEPAINT 0x1920b9b9
#define SVF_TAN 0x0b88ad48
#define SVF_TEAL 0x7c9e660b
#define SVF_TEXT_ANCHOR 0x0c0046d2
#define SVF_TEXT_DECORATION 0x2230061f
#define SVF_TEXTLENGTH 0xa31e6e8c
#define SVF_THISTLE 0xdf68be82
#define SVF_TOMATO 0x1e8b7ef9
#define SVF_TOTAL_POINTS 0x93249a53
#define SVF_TRANSFORM 0x2393dd81
#define SVF_TURQUOISE 0x0c1fe5d6
#define SVF_TYPE 0x7c9ebd07
#define SVF_ULTRA_CONDENSED 0xba25ad8d
#define SVF_UNDERLINE 0xb8ea5b4b
#define SVF_UNITS 0x108252d8
#define SVF_USE 0x0b88b3d2
#define SVF_VALUES 0x22383ff5
#define SVF_VERSION 0x73006c4b
#define SVF_VIEW_HEIGHT 0x56219666
#define SVF_VIEW_WIDTH 0x497f2d2d
#define SVF_VIEW_X 0x22c52ea5
#define SVF_VIEW_Y 0x22c52ea6
#define SVF_VIEWBOX 0x7b6be409
#define SVF_VIOLET 0x22ca82d8
#define SVF_VISIBILITY 0x7a0f4bad
#define SVF_WHEAT 0x10a3261e
#define SVF_WHITE 0x10a33986
#define SVF_WHITESMOKE 0x2580cae5
#define SVF_WIDER 0x10a3aec0
#define SVF_WIDTH 0x10a3b0a5
#define SVF_WORD_SPACING 0x62976533
#define SVF_YELLOW 0x297ff6e1
#define SVF_YELLOWGREEN 0xda4a85b2

#define SVF_AZIMUTH 0x52cfd287
#define SVF_ELEVATION 0x0c12538c
#define SVF_FEFUNCR 0xa284a6ae
#define SVF_FEFUNCG 0xa284a6a3
#define SVF_FEFUNCB 0xa284a69e
#define SVF_FEFUNCA 0xa284a69d
#define SVF_LIGHTING_COLOR 0x020fc127
#define SVF_LIGHTING_COLOUR 0x4407e6dc
#define SVF_LIMITINGCONEANGLE 0xbb90036e
#define SVF_POINTSATX 0xf4c77f0f
#define SVF_POINTSATY 0xf4c77f10
#define SVF_POINTSATZ 0xf4c77f11
#define SVF_SPECULARCONSTANT 0x8bb3ceae
#define SVF_SPECULAREXPONENT 0x1d625135
#define SVF_TABLEVALUES 0x9de92b7d
#define SVF_EXPONENT 0xd4513596
#define SVF_SLOPE 0x105d2208
#define SVF_INTERCEPT 0x12b3db33
#define SVF_INVERT 0x04d5a7bd
#define SVF_IDENTITY 0x68144eaf
#define SVF_LINEAR 0x0b7641e0
#define SVF_TABLE 0x1068fa8d
#define SVF_GAMMA 0x0f7deae8
#define SVF_DISCRETE 0x6b8e5778
#define SVF_DIFFUSECONSTANT 0x4f5eb9d5
#define SVF_SURFACESCALE 0xbd475ab6
#define SVF_XCHANNELSELECTOR 0x57175337
#define SVF_YCHANNELSELECTOR 0x634c7918
#define SVF_ZOOMANDPAN 0xc606dfdc

#endif
