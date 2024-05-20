#pragma once

// Name:      vector.h
// Copyright: Paul Manias © 2010-2024
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_VECTOR (1)

#include <parasol/modules/display.h>
#include <parasol/modules/picture.h>

class objVectorColour;
class objVectorTransition;
class objVectorScene;
class objVectorImage;
class objVectorPattern;
class objVectorGradient;
class objFilterEffect;
class objImageFX;
class objSourceFX;
class objBlurFX;
class objColourFX;
class objCompositeFX;
class objConvolveFX;
class objDisplacementFX;
class objFloodFX;
class objLightingFX;
class objMergeFX;
class objMorphologyFX;
class objOffsetFX;
class objRemapFX;
class objTurbulenceFX;
class objVectorClip;
class objVectorFilter;
class objVector;
class objVectorPath;
class objVectorText;
class objVectorGroup;
class objVectorWave;
class objVectorRectangle;
class objVectorPolygon;
class objVectorShape;
class objVectorSpiral;
class objVectorEllipse;
class objVectorViewport;

// Options for drawing arcs.

enum class ARC : ULONG {
   NIL = 0,
   LARGE = 0x00000001,
   SWEEP = 0x00000002,
};

DEFINE_ENUM_FLAG_OPERATORS(ARC)

// Options for VectorClip.

enum class VCLF : ULONG {
   NIL = 0,
   APPLY_FILLS = 0x00000001,
   APPLY_STROKES = 0x00000002,
};

DEFINE_ENUM_FLAG_OPERATORS(VCLF)

// Optional flags and indicators for the Vector class.

enum class VF : ULONG {
   NIL = 0,
   DISABLED = 0x00000001,
   HAS_FOCUS = 0x00000002,
   JOIN_PATHS = 0x00000004,
};

DEFINE_ENUM_FLAG_OPERATORS(VF)

// Define the aspect ratio for VectorFilter unit scaling.

enum class VFA : LONG {
   NIL = 0,
   MEET = 0,
   NONE = 1,
};

// Light source identifiers.

enum class LS : LONG {
   NIL = 0,
   DISTANT = 0,
   SPOT = 1,
   POINT = 2,
};

// Lighting algorithm for the LightingFX class.

enum class LT : LONG {
   NIL = 0,
   DIFFUSE = 0,
   SPECULAR = 1,
};

enum class VUNIT : LONG {
   NIL = 0,
   UNDEFINED = 0,
   BOUNDING_BOX = 1,
   USERSPACE = 2,
   END = 3,
};

// Spread method options define the method to use for tiling filled graphics.

enum class VSPREAD : LONG {
   NIL = 0,
   UNDEFINED = 0,
   PAD = 1,
   REFLECT = 2,
   REPEAT = 3,
   REFLECT_X = 4,
   REFLECT_Y = 5,
   CLIP = 6,
   END = 7,
};

enum class EM : LONG {
   NIL = 0,
   DUPLICATE = 1,
   WRAP = 2,
   NONE = 3,
};

enum class PE : LONG {
   NIL = 0,
   Move = 1,
   MoveRel = 2,
   Line = 3,
   LineRel = 4,
   HLine = 5,
   HLineRel = 6,
   VLine = 7,
   VLineRel = 8,
   Curve = 9,
   CurveRel = 10,
   Smooth = 11,
   SmoothRel = 12,
   QuadCurve = 13,
   QuadCurveRel = 14,
   QuadSmooth = 15,
   QuadSmoothRel = 16,
   Arc = 17,
   ArcRel = 18,
   ClosePath = 19,
};

// Vector fill rules for the FillRule field in the Vector class.

enum class VFR : LONG {
   NIL = 0,
   NON_ZERO = 1,
   EVEN_ODD = 2,
   INHERIT = 3,
   END = 4,
};

// Options for the Vector class' Visibility field.

enum class VIS : LONG {
   NIL = 0,
   HIDDEN = 0,
   VISIBLE = 1,
   COLLAPSE = 2,
   INHERIT = 3,
};

// Viewport overflow options.

enum class VOF : LONG {
   NIL = 0,
   VISIBLE = 0,
   HIDDEN = 1,
   SCROLL = 2,
   INHERIT = 3,
};

// Component selection for RemapFX methods.

enum class CMP : LONG {
   NIL = 0,
   ALL = -1,
   RED = 0,
   GREEN = 1,
   BLUE = 2,
   ALPHA = 3,
};

// Options for the look of line joins.

enum class VLJ : LONG {
   NIL = 0,
   MITER = 0,
   MITER_REVERT = 1,
   ROUND = 2,
   BEVEL = 3,
   MITER_ROUND = 4,
   INHERIT = 5,
};

// Line-cap options.

enum class VLC : LONG {
   NIL = 0,
   BUTT = 1,
   SQUARE = 2,
   ROUND = 3,
   INHERIT = 4,
};

// Inner join options for angled lines.

enum class VIJ : LONG {
   NIL = 0,
   BEVEL = 1,
   MITER = 2,
   JAG = 3,
   ROUND = 4,
   INHERIT = 5,
};

// VectorGradient options.

enum class VGT : LONG {
   NIL = 0,
   LINEAR = 0,
   RADIAL = 1,
   CONIC = 2,
   DIAMOND = 3,
   CONTOUR = 4,
};

// Options for stretching text in VectorText.

enum class VTS : LONG {
   NIL = 0,
   INHERIT = 0,
   NORMAL = 1,
   WIDER = 2,
   NARROWER = 3,
   ULTRA_CONDENSED = 4,
   EXTRA_CONDENSED = 5,
   CONDENSED = 6,
   SEMI_CONDENSED = 7,
   EXPANDED = 8,
   SEMI_EXPANDED = 9,
   ULTRA_EXPANDED = 10,
   EXTRA_EXPANDED = 11,
};

// MorphologyFX options.

enum class MOP : LONG {
   NIL = 0,
   ERODE = 0,
   DILATE = 1,
};

// Operators for CompositionFX.

enum class OP : LONG {
   NIL = 0,
   OVER = 0,
   IN = 1,
   OUT = 2,
   ATOP = 3,
   XOR = 4,
   ARITHMETIC = 5,
   SCREEN = 6,
   MULTIPLY = 7,
   LIGHTEN = 8,
   DARKEN = 9,
   INVERT_RGB = 10,
   INVERT = 11,
   CONTRAST = 12,
   DODGE = 13,
   BURN = 14,
   HARD_LIGHT = 15,
   SOFT_LIGHT = 16,
   DIFFERENCE = 17,
   EXCLUSION = 18,
   PLUS = 19,
   MINUS = 20,
   SUBTRACT = 20,
   OVERLAY = 21,
};

// VectorText flags.

enum class VTXF : ULONG {
   NIL = 0,
   UNDERLINE = 0x00000001,
   OVERLINE = 0x00000002,
   LINE_THROUGH = 0x00000004,
   BLINK = 0x00000008,
   EDITABLE = 0x00000010,
   EDIT = 0x00000010,
   AREA_SELECTED = 0x00000020,
   NO_SYS_KEYS = 0x00000040,
   OVERWRITE = 0x00000080,
   SECRET = 0x00000100,
   RASTER = 0x00000200,
};

DEFINE_ENUM_FLAG_OPERATORS(VTXF)

// Morph flags

enum class VMF : ULONG {
   NIL = 0,
   STRETCH = 0x00000001,
   AUTO_SPACING = 0x00000002,
   X_MIN = 0x00000004,
   X_MID = 0x00000008,
   X_MAX = 0x00000010,
   Y_MIN = 0x00000020,
   Y_MID = 0x00000040,
   Y_MAX = 0x00000080,
};

DEFINE_ENUM_FLAG_OPERATORS(VMF)

// Colour space options.

enum class VCS : LONG {
   NIL = 0,
   INHERIT = 0,
   SRGB = 1,
   LINEAR_RGB = 2,
};

// Filter source types - these are used internally

enum class VSF : LONG {
   NIL = 0,
   IGNORE = 0,
   NONE = 0,
   GRAPHIC = 1,
   ALPHA = 2,
   BKGD = 3,
   BKGD_ALPHA = 4,
   FILL = 5,
   STROKE = 6,
   REFERENCE = 7,
   PREVIOUS = 8,
};

// Wave options.

enum class WVC : LONG {
   NIL = 0,
   NONE = 1,
   TOP = 2,
   BOTTOM = 3,
};

// Wave style options.

enum class WVS : LONG {
   NIL = 0,
   CURVED = 1,
   ANGLED = 2,
   SAWTOOTH = 3,
};

// Colour modes for ColourFX.

enum class CM : LONG {
   NIL = 0,
   NONE = 0,
   MATRIX = 1,
   SATURATE = 2,
   HUE_ROTATE = 3,
   LUMINANCE_ALPHA = 4,
   CONTRAST = 5,
   BRIGHTNESS = 6,
   HUE = 7,
   DESATURATE = 8,
   COLOURISE = 9,
};

// Gradient flags

enum class VGF : ULONG {
   NIL = 0,
   SCALED_X1 = 0x00000001,
   SCALED_Y1 = 0x00000002,
   SCALED_X2 = 0x00000004,
   SCALED_Y2 = 0x00000008,
   SCALED_CX = 0x00000010,
   SCALED_CY = 0x00000020,
   SCALED_FX = 0x00000040,
   SCALED_FY = 0x00000080,
   SCALED_RADIUS = 0x00000100,
   FIXED_X1 = 0x00000200,
   FIXED_Y1 = 0x00000400,
   FIXED_X2 = 0x00000800,
   FIXED_Y2 = 0x00001000,
   FIXED_CX = 0x00002000,
   FIXED_CY = 0x00004000,
   FIXED_FX = 0x00008000,
   FIXED_FY = 0x00010000,
   FIXED_RADIUS = 0x00020000,
};

DEFINE_ENUM_FLAG_OPERATORS(VGF)

// Optional flags for the VectorScene object.

enum class VPF : ULONG {
   NIL = 0,
   BITMAP_SIZED = 0x00000001,
   RENDER_TIME = 0x00000002,
   RESIZE = 0x00000004,
   OUTLINE_VIEWPORTS = 0x00000008,
};

DEFINE_ENUM_FLAG_OPERATORS(VPF)

enum class TB : LONG {
   NIL = 0,
   TURBULENCE = 0,
   NOISE = 1,
};

enum class VSM : LONG {
   NIL = 0,
   AUTO = 0,
   NEIGHBOUR = 1,
   BILINEAR = 2,
   BICUBIC = 3,
   SPLINE16 = 4,
   KAISER = 5,
   QUADRIC = 6,
   GAUSSIAN = 7,
   BESSEL = 8,
   MITCHELL = 9,
   SINC3 = 10,
   LANCZOS3 = 11,
   BLACKMAN3 = 12,
   SINC8 = 13,
   LANCZOS8 = 14,
   BLACKMAN8 = 15,
};

enum class RQ : LONG {
   NIL = 0,
   AUTO = 0,
   FAST = 1,
   CRISP = 2,
   PRECISE = 3,
   BEST = 4,
};

enum class RC : UBYTE {
   NIL = 0,
   FINAL_PATH = 0x00000001,
   BASE_PATH = 0x00000002,
   TRANSFORM = 0x00000004,
   ALL = 0x00000007,
};

DEFINE_ENUM_FLAG_OPERATORS(RC)

// Aspect ratios control alignment, scaling and clipping.

enum class ARF : ULONG {
   NIL = 0,
   X_MIN = 0x00000001,
   X_MID = 0x00000002,
   X_MAX = 0x00000004,
   Y_MIN = 0x00000008,
   Y_MID = 0x00000010,
   Y_MAX = 0x00000020,
   MEET = 0x00000040,
   SLICE = 0x00000080,
   NONE = 0x00000100,
};

DEFINE_ENUM_FLAG_OPERATORS(ARF)

// Options for vecGetBoundary().

enum class VBF : ULONG {
   NIL = 0,
   INCLUSIVE = 0x00000001,
   NO_TRANSFORM = 0x00000002,
};

DEFINE_ENUM_FLAG_OPERATORS(VBF)

// Mask for controlling feedback events that are received.

enum class FM : ULONG {
   NIL = 0,
   PATH_CHANGED = 0x00000001,
   HAS_FOCUS = 0x00000002,
   CHILD_HAS_FOCUS = 0x00000004,
   LOST_FOCUS = 0x00000008,
};

DEFINE_ENUM_FLAG_OPERATORS(FM)

struct GradientStop {
   DOUBLE Offset;    // An offset in the range of 0 - 1.0
   struct FRGB RGB;  // A floating point RGB value.
};

struct Transition {
   DOUBLE  Offset;       // An offset from 0.0 to 1.0 at which to apply the transform.
   CSTRING Transform;    // A transform string, as per SVG guidelines.
};

struct VectorPoint {
   DOUBLE X;           // The X coordinate of this point.
   DOUBLE Y;           // The Y coordinate of this point.
   UBYTE  XScaled:1;   // TRUE if the X value is scaled to its viewport (between 0 and 1.0).
   UBYTE  YScaled:1;   // TRUE if the Y value is scaled to its viewport (between 0 and 1.0).
};

struct VectorPainter {
   objVectorPattern * Pattern;    // A VectorPattern object, suitable for pattern based fills.
   objVectorImage * Image;        // A VectorImage object, suitable for image fills.
   objVectorGradient * Gradient;  // A VectorGradient object, suitable for gradient fills.
   struct FRGB Colour;            // A single RGB colour definition, suitable for block colour fills.
};

struct PathCommand {
   PE     Type;       // The command type (PE value)
   UBYTE  LargeArc;   // Equivalent to the large-arc-flag in SVG, it ensures that the arc follows the longest drawing path when TRUE.
   UBYTE  Sweep;      // Equivalent to the sweep-flag in SVG, it inverts the default behaviour in generating arc paths.
   UBYTE  Pad1;       // Private
   DOUBLE X;          // The targeted X coordinate (absolute or scaled) for the command
   DOUBLE Y;          // The targeted Y coordinate (absolute or scaled) for the command
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
   LONG   Tag;                    // An optional tag value defined by the client for matrix identification.
};

#define MTAG_ANIMATE_MOTION 0x8b929127
#define MTAG_ANIMATE_TRANSFORM 0x5374188d
#define MTAG_SCENE_GRAPH 0xacc188f2
#define MTAG_USE_TRANSFORM 0x35a3f7fb
#define MTAG_SVG_TRANSFORM 0x3479679e

struct FontMetrics {
   LONG Height;         // Capitalised font height
   LONG LineSpacing;    // Vertical advance from one line to the next
   LONG Ascent;         // Height from the baseline to the top of the font, including accents.
   LONG Descent;        // Height from the baseline to the bottom of the font
};

// VectorColour class definition

#define VER_VECTORCOLOUR (1.000000)

class objVectorColour : public Object {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORCOLOUR;
   static constexpr CSTRING CLASS_NAME = "VectorColour";

   using create = pf::Create<objVectorColour>;

   DOUBLE Red;    // The red component value.
   DOUBLE Green;  // The green component value.
   DOUBLE Blue;   // The blue component value.
   DOUBLE Alpha;  // The alpha component value.

   // Customised field setting

   inline ERR setRed(const DOUBLE Value) noexcept {
      this->Red = Value;
      return ERR::Okay;
   }

   inline ERR setGreen(const DOUBLE Value) noexcept {
      this->Green = Value;
      return ERR::Okay;
   }

   inline ERR setBlue(const DOUBLE Value) noexcept {
      this->Blue = Value;
      return ERR::Okay;
   }

   inline ERR setAlpha(const DOUBLE Value) noexcept {
      this->Alpha = Value;
      return ERR::Okay;
   }

};

// VectorTransition class definition

#define VER_VECTORTRANSITION (1.000000)

class objVectorTransition : public Object {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORTRANSITION;
   static constexpr CSTRING CLASS_NAME = "VectorTransition";

   using create = pf::Create<objVectorTransition>;
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

inline ERR scAddDef(APTR Ob, CSTRING Name, OBJECTPTR Def) noexcept {
   struct scAddDef args = { Name, Def };
   return(Action(MT_ScAddDef, (OBJECTPTR)Ob, &args));
}

inline ERR scSearchByID(APTR Ob, LONG ID, OBJECTPTR * Result) noexcept {
   struct scSearchByID args = { ID, (OBJECTPTR)0 };
   ERR error = Action(MT_ScSearchByID, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

inline ERR scFindDef(APTR Ob, CSTRING Name, OBJECTPTR * Def) noexcept {
   struct scFindDef args = { Name, (OBJECTPTR)0 };
   ERR error = Action(MT_ScFindDef, (OBJECTPTR)Ob, &args);
   if (Def) *Def = args.Def;
   return(error);
}

#define scDebug(obj) Action(MT_ScDebug,(obj),0)


class objVectorScene : public Object {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORSCENE;
   static constexpr CSTRING CLASS_NAME = "VectorScene";

   using create = pf::Create<objVectorScene>;

   LARGE    RenderTime;           // Returns the rendering time of the last scene.
   DOUBLE   Gamma;                // Private. Not currently implemented.
   objVectorScene * HostScene;    // Refers to a top-level VectorScene object, if applicable.
   objVectorViewport * Viewport;  // References the first object in the scene, which must be a @VectorViewport object.
   objBitmap * Bitmap;            // Target bitmap for drawing vectors.
   OBJECTID SurfaceID;            // May refer to a @Surface object for enabling automatic rendering.
   VPF      Flags;                // Optional flags.
   LONG     PageWidth;            // The width of the page that contains the vector.
   LONG     PageHeight;           // The height of the page that contains the vector.
   VSM      SampleMethod;         // The sampling method to use when interpolating images and patterns.

   // Action stubs

   inline ERR draw() noexcept { return Action(AC_Draw, this, NULL); }
   inline ERR drawArea(LONG X, LONG Y, LONG Width, LONG Height) noexcept {
      struct acDraw args = { X, Y, Width, Height };
      return Action(AC_Draw, this, &args);
   }
   inline ERR init() noexcept { return InitObject(this); }
   inline ERR redimension(DOUBLE X, DOUBLE Y, DOUBLE Z, DOUBLE Width, DOUBLE Height, DOUBLE Depth) noexcept {
      struct acRedimension args = { X, Y, Z, Width, Height, Depth };
      return Action(AC_Redimension, this, &args);
   }
   inline ERR redimension(DOUBLE X, DOUBLE Y, DOUBLE Width, DOUBLE Height) noexcept {
      struct acRedimension args = { X, Y, 0, Width, Height, 0 };
      return Action(AC_Redimension, this, &args);
   }
   inline ERR reset() noexcept { return Action(AC_Reset, this, NULL); }
   inline ERR resize(DOUBLE Width, DOUBLE Height, DOUBLE Depth = 0) noexcept {
      struct acResize args = { Width, Height, Depth };
      return Action(AC_Resize, this, &args);
   }

   // Customised field setting

   inline ERR setGamma(const DOUBLE Value) noexcept {
      this->Gamma = Value;
      return ERR::Okay;
   }

   inline ERR setHostScene(objVectorScene * Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->HostScene = Value;
      return ERR::Okay;
   }

   inline ERR setBitmap(objBitmap * Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, 0x08000301, Value, 1);
   }

   inline ERR setSurface(OBJECTID Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setFlags(const VPF Value) noexcept {
      this->Flags = Value;
      return ERR::Okay;
   }

   inline ERR setPageWidth(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[4];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setPageHeight(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setSampleMethod(const VSM Value) noexcept {
      this->SampleMethod = Value;
      return ERR::Okay;
   }

};

// VectorImage class definition

#define VER_VECTORIMAGE (1.000000)

class objVectorImage : public Object {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORIMAGE;
   static constexpr CSTRING CLASS_NAME = "VectorImage";

   using create = pf::Create<objVectorImage>;

   DOUBLE  X;               // Apply a horizontal offset to the image, the origin of which is determined by the #Units value.
   DOUBLE  Y;               // Apply a vertical offset to the image, the origin of which is determined by the #Units value.
   objPicture * Picture;    // Refers to a @Picture from which the source #Bitmap is acquired.
   objBitmap * Bitmap;      // Reference to a source bitmap for the rendering algorithm.
   VUNIT   Units;           // Declares the coordinate system to use for the #X and #Y values.
   LONG    Dimensions;      // Dimension flags define whether individual dimension fields contain fixed or scaled values.
   VSPREAD SpreadMethod;    // Defines image tiling behaviour, if desired.
   ARF     AspectRatio;     // Flags that affect the aspect ratio of the image within its target vector.

   // Customised field setting

   inline ERR setX(const DOUBLE Value) noexcept {
      this->X = Value;
      return ERR::Okay;
   }

   inline ERR setY(const DOUBLE Value) noexcept {
      this->Y = Value;
      return ERR::Okay;
   }

   inline ERR setPicture(objPicture * Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, 0x08000301, Value, 1);
   }

   inline ERR setBitmap(objBitmap * Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, 0x08000301, Value, 1);
   }

   inline ERR setUnits(const VUNIT Value) noexcept {
      this->Units = Value;
      return ERR::Okay;
   }

   inline ERR setDimensions(const LONG Value) noexcept {
      this->Dimensions = Value;
      return ERR::Okay;
   }

   inline ERR setSpreadMethod(const VSPREAD Value) noexcept {
      this->SpreadMethod = Value;
      return ERR::Okay;
   }

   inline ERR setAspectRatio(const ARF Value) noexcept {
      this->AspectRatio = Value;
      return ERR::Okay;
   }

};

// VectorPattern class definition

#define VER_VECTORPATTERN (1.000000)

class objVectorPattern : public Object {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORPATTERN;
   static constexpr CSTRING CLASS_NAME = "VectorPattern";

   using create = pf::Create<objVectorPattern>;

   DOUBLE  X;                     // X coordinate for the pattern.
   DOUBLE  Y;                     // Y coordinate for the pattern.
   DOUBLE  Width;                 // Width of the pattern tile.
   DOUBLE  Height;                // Height of the pattern tile.
   DOUBLE  Opacity;               // The opacity of the pattern.
   objVectorScene * Scene;        // Refers to the internal @VectorScene that will contain the rendered pattern.
   objVectorPattern * Inherit;    // Inherit attributes from a VectorPattern referenced here.
   VSPREAD SpreadMethod;          // The behaviour to use when the pattern bounds do not match the vector path.
   VUNIT   Units;                 // Defines the coordinate system for fields X, Y, Width and Height.
   VUNIT   ContentUnits;          // Private. Not yet implemented.
   LONG    Dimensions;            // Dimension flags are stored here.

   // Customised field setting

   inline ERR setX(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setY(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[1];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setWidth(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[8];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setHeight(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[2];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setOpacity(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERR setInherit(objVectorPattern * Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, 0x08000301, Value, 1);
   }

   inline ERR setSpreadMethod(const VSPREAD Value) noexcept {
      this->SpreadMethod = Value;
      return ERR::Okay;
   }

   inline ERR setUnits(const VUNIT Value) noexcept {
      this->Units = Value;
      return ERR::Okay;
   }

   inline ERR setContentUnits(const VUNIT Value) noexcept {
      this->ContentUnits = Value;
      return ERR::Okay;
   }

   inline ERR setMatrices(APTR Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[10];
      return field->WriteValue(target, field, 0x08000318, Value, 1);
   }

   template <class T> inline ERR setTransform(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, 0x08800208, to_cstring(Value), 1);
   }

};

// VectorGradient class definition

#define VER_VECTORGRADIENT (1.000000)

class objVectorGradient : public Object {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORGRADIENT;
   static constexpr CSTRING CLASS_NAME = "VectorGradient";

   using create = pf::Create<objVectorGradient>;

   DOUBLE  X1;                     // Initial X coordinate for the gradient.
   DOUBLE  Y1;                     // Initial Y coordinate for the gradient.
   DOUBLE  X2;                     // Final X coordinate for the gradient.
   DOUBLE  Y2;                     // Final Y coordinate for the gradient.
   DOUBLE  CenterX;                // The horizontal center point of the gradient.
   DOUBLE  CenterY;                // The vertical center point of the gradient.
   DOUBLE  FX;                     // The horizontal focal point for radial gradients.
   DOUBLE  FY;                     // The vertical focal point for radial gradients.
   DOUBLE  Radius;                 // The radius of the gradient.
   objVectorGradient * Inherit;    // Inherit attributes from the VectorGradient referenced here.
   VSPREAD SpreadMethod;           // The behaviour to use when the gradient bounds do not match the vector path.
   VUNIT   Units;                  // Defines the coordinate system for #X1, #Y1, #X2 and #Y2.
   VGT     Type;                   // Specifies the type of gradient (e.g. RADIAL, LINEAR)
   VGF     Flags;                  // Dimension flags are stored here.
   VCS     ColourSpace;            // Defines the colour space to use when interpolating gradient colours.
   LONG    TotalStops;             // Total number of stops defined in the Stops array.

   // Action stubs

   inline ERR init() noexcept { return InitObject(this); }

   // Customised field setting

   inline ERR setX1(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[3];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setY1(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setX2(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[4];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setY2(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[6];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setCenterX(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[21];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setCenterY(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[22];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setFX(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setFY(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[1];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setRadius(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setInherit(objVectorGradient * Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[19];
      return field->WriteValue(target, field, 0x08000301, Value, 1);
   }

   inline ERR setSpreadMethod(const VSPREAD Value) noexcept {
      this->SpreadMethod = Value;
      return ERR::Okay;
   }

   inline ERR setUnits(const VUNIT Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Units = Value;
      return ERR::Okay;
   }

   inline ERR setType(const VGT Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Type = Value;
      return ERR::Okay;
   }

   inline ERR setFlags(const VGF Value) noexcept {
      this->Flags = Value;
      return ERR::Okay;
   }

   inline ERR setColourSpace(const VCS Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->ColourSpace = Value;
      return ERR::Okay;
   }

   inline ERR setMatrices(APTR Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[16];
      return field->WriteValue(target, field, 0x08000318, Value, 1);
   }

   inline ERR setNumeric(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[20];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   template <class T> inline ERR setID(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[2];
      return field->WriteValue(target, field, 0x08800308, to_cstring(Value), 1);
   }

   inline ERR setStops(const APTR Value, LONG Elements) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, 0x00001318, Value, Elements);
   }

   template <class T> inline ERR setTransform(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[15];
      return field->WriteValue(target, field, 0x08800208, to_cstring(Value), 1);
   }

};

// FilterEffect class definition

#define VER_FILTEREFFECT (1.000000)

class objFilterEffect : public Object {
   public:
   static constexpr CLASSID CLASS_ID = ID_FILTEREFFECT;
   static constexpr CSTRING CLASS_NAME = "FilterEffect";

   using create = pf::Create<objFilterEffect>;

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
   VSF    SourceType;         // Specifies an input source for the effect algorithm, if required.
   VSF    MixType;            // If a secondary mix input is required for the effect, specify it here.

   // Action stubs

   inline ERR init() noexcept { return InitObject(this); }
   inline ERR moveToBack() noexcept { return Action(AC_MoveToBack, this, NULL); }
   inline ERR moveToFront() noexcept { return Action(AC_MoveToFront, this, NULL); }

   // Customised field setting

   inline ERR setNext(objFilterEffect * Value) noexcept {
      this->Next = Value;
      return ERR::Okay;
   }

   inline ERR setPrev(objFilterEffect * Value) noexcept {
      this->Prev = Value;
      return ERR::Okay;
   }

   inline ERR setTarget(objBitmap * Value) noexcept {
      this->Target = Value;
      return ERR::Okay;
   }

   inline ERR setInput(objFilterEffect * Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, 0x08000301, Value, 1);
   }

   inline ERR setMix(objFilterEffect * Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[3];
      return field->WriteValue(target, field, 0x08000301, Value, 1);
   }

   inline ERR setX(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setY(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[1];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setWidth(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[7];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setHeight(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[2];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setSourceType(const VSF Value) noexcept {
      this->SourceType = Value;
      return ERR::Okay;
   }

   inline ERR setMixType(const VSF Value) noexcept {
      this->MixType = Value;
      return ERR::Okay;
   }

};

struct MergeSource {
   VSF SourceType;              // The type of the required source.
   objFilterEffect * Effect;    // Effect pointer if the SourceType is REFERENCE.
  MergeSource(VSF pType, objFilterEffect *pEffect = NULL) : SourceType(pType), Effect(pEffect) { };
};

// ImageFX class definition

#define VER_IMAGEFX (1.000000)

class objImageFX : public objFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = ID_IMAGEFX;
   static constexpr CSTRING CLASS_NAME = "ImageFX";

   using create = pf::Create<objImageFX>;
};

// SourceFX class definition

#define VER_SOURCEFX (1.000000)

class objSourceFX : public objFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = ID_SOURCEFX;
   static constexpr CSTRING CLASS_NAME = "SourceFX";

   using create = pf::Create<objSourceFX>;
};

// BlurFX class definition

#define VER_BLURFX (1.000000)

class objBlurFX : public objFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = ID_BLURFX;
   static constexpr CSTRING CLASS_NAME = "BlurFX";

   using create = pf::Create<objBlurFX>;
};

// ColourFX class definition

#define VER_COLOURFX (1.000000)

class objColourFX : public objFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = ID_COLOURFX;
   static constexpr CSTRING CLASS_NAME = "ColourFX";

   using create = pf::Create<objColourFX>;
};

// CompositeFX class definition

#define VER_COMPOSITEFX (1.000000)

class objCompositeFX : public objFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = ID_COMPOSITEFX;
   static constexpr CSTRING CLASS_NAME = "CompositeFX";

   using create = pf::Create<objCompositeFX>;
};

// ConvolveFX class definition

#define VER_CONVOLVEFX (1.000000)

class objConvolveFX : public objFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = ID_CONVOLVEFX;
   static constexpr CSTRING CLASS_NAME = "ConvolveFX";

   using create = pf::Create<objConvolveFX>;
};

// DisplacementFX class definition

#define VER_DISPLACEMENTFX (1.000000)

class objDisplacementFX : public objFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = ID_DISPLACEMENTFX;
   static constexpr CSTRING CLASS_NAME = "DisplacementFX";

   using create = pf::Create<objDisplacementFX>;
};

// FloodFX class definition

#define VER_FLOODFX (1.000000)

class objFloodFX : public objFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = ID_FLOODFX;
   static constexpr CSTRING CLASS_NAME = "FloodFX";

   using create = pf::Create<objFloodFX>;
};

// LightingFX class definition

#define VER_LIGHTINGFX (1.000000)

// LightingFX methods

#define MT_LTSetDistantLight -20
#define MT_LTSetPointLight -22
#define MT_LTSetSpotLight -21

struct ltSetDistantLight { DOUBLE Azimuth; DOUBLE Elevation;  };
struct ltSetPointLight { DOUBLE X; DOUBLE Y; DOUBLE Z;  };
struct ltSetSpotLight { DOUBLE X; DOUBLE Y; DOUBLE Z; DOUBLE PX; DOUBLE PY; DOUBLE PZ; DOUBLE Exponent; DOUBLE ConeAngle;  };

inline ERR ltSetDistantLight(APTR Ob, DOUBLE Azimuth, DOUBLE Elevation) noexcept {
   struct ltSetDistantLight args = { Azimuth, Elevation };
   return(Action(MT_LTSetDistantLight, (OBJECTPTR)Ob, &args));
}

inline ERR ltSetPointLight(APTR Ob, DOUBLE X, DOUBLE Y, DOUBLE Z) noexcept {
   struct ltSetPointLight args = { X, Y, Z };
   return(Action(MT_LTSetPointLight, (OBJECTPTR)Ob, &args));
}

inline ERR ltSetSpotLight(APTR Ob, DOUBLE X, DOUBLE Y, DOUBLE Z, DOUBLE PX, DOUBLE PY, DOUBLE PZ, DOUBLE Exponent, DOUBLE ConeAngle) noexcept {
   struct ltSetSpotLight args = { X, Y, Z, PX, PY, PZ, Exponent, ConeAngle };
   return(Action(MT_LTSetSpotLight, (OBJECTPTR)Ob, &args));
}


class objLightingFX : public objFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = ID_LIGHTINGFX;
   static constexpr CSTRING CLASS_NAME = "LightingFX";

   using create = pf::Create<objLightingFX>;
};

// MergeFX class definition

#define VER_MERGEFX (1.000000)

class objMergeFX : public objFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = ID_MERGEFX;
   static constexpr CSTRING CLASS_NAME = "MergeFX";

   using create = pf::Create<objMergeFX>;
};

// MorphologyFX class definition

#define VER_MORPHOLOGYFX (1.000000)

class objMorphologyFX : public objFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = ID_MORPHOLOGYFX;
   static constexpr CSTRING CLASS_NAME = "MorphologyFX";

   using create = pf::Create<objMorphologyFX>;
};

// OffsetFX class definition

#define VER_OFFSETFX (1.000000)

class objOffsetFX : public objFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = ID_OFFSETFX;
   static constexpr CSTRING CLASS_NAME = "OffsetFX";

   using create = pf::Create<objOffsetFX>;
};

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

struct rfSelectGamma { CMP Component; DOUBLE Amplitude; DOUBLE Offset; DOUBLE Exponent;  };
struct rfSelectTable { CMP Component; DOUBLE * Values; LONG Size;  };
struct rfSelectLinear { CMP Component; DOUBLE Slope; DOUBLE Intercept;  };
struct rfSelectIdentity { CMP Component;  };
struct rfSelectDiscrete { CMP Component; DOUBLE * Values; LONG Size;  };
struct rfSelectInvert { CMP Component;  };
struct rfSelectMask { CMP Component; LONG Mask;  };

inline ERR rfSelectGamma(APTR Ob, CMP Component, DOUBLE Amplitude, DOUBLE Offset, DOUBLE Exponent) noexcept {
   struct rfSelectGamma args = { Component, Amplitude, Offset, Exponent };
   return(Action(MT_RFSelectGamma, (OBJECTPTR)Ob, &args));
}

inline ERR rfSelectTable(APTR Ob, CMP Component, DOUBLE * Values, LONG Size) noexcept {
   struct rfSelectTable args = { Component, Values, Size };
   return(Action(MT_RFSelectTable, (OBJECTPTR)Ob, &args));
}

inline ERR rfSelectLinear(APTR Ob, CMP Component, DOUBLE Slope, DOUBLE Intercept) noexcept {
   struct rfSelectLinear args = { Component, Slope, Intercept };
   return(Action(MT_RFSelectLinear, (OBJECTPTR)Ob, &args));
}

inline ERR rfSelectIdentity(APTR Ob, CMP Component) noexcept {
   struct rfSelectIdentity args = { Component };
   return(Action(MT_RFSelectIdentity, (OBJECTPTR)Ob, &args));
}

inline ERR rfSelectDiscrete(APTR Ob, CMP Component, DOUBLE * Values, LONG Size) noexcept {
   struct rfSelectDiscrete args = { Component, Values, Size };
   return(Action(MT_RFSelectDiscrete, (OBJECTPTR)Ob, &args));
}

inline ERR rfSelectInvert(APTR Ob, CMP Component) noexcept {
   struct rfSelectInvert args = { Component };
   return(Action(MT_RFSelectInvert, (OBJECTPTR)Ob, &args));
}

inline ERR rfSelectMask(APTR Ob, CMP Component, LONG Mask) noexcept {
   struct rfSelectMask args = { Component, Mask };
   return(Action(MT_RFSelectMask, (OBJECTPTR)Ob, &args));
}


class objRemapFX : public objFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = ID_REMAPFX;
   static constexpr CSTRING CLASS_NAME = "RemapFX";

   using create = pf::Create<objRemapFX>;
};

// TurbulenceFX class definition

#define VER_TURBULENCEFX (1.000000)

class objTurbulenceFX : public objFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = ID_TURBULENCEFX;
   static constexpr CSTRING CLASS_NAME = "TurbulenceFX";

   using create = pf::Create<objTurbulenceFX>;
};

// VectorClip class definition

#define VER_VECTORCLIP (1.000000)

class objVectorClip : public Object {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORCLIP;
   static constexpr CSTRING CLASS_NAME = "VectorClip";

   using create = pf::Create<objVectorClip>;

   objVectorViewport * Viewport;    // This viewport hosts the Vector objects that will contribute to the clip path.
   VUNIT Units;                     // Defines the coordinate system for fields X, Y, Width and Height.
   VCLF  Flags;                     // Optional flags.

   // Customised field setting

   inline ERR setUnits(const VUNIT Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[3];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setFlags(const VCLF Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[1];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

};

// VectorFilter class definition

#define VER_VECTORFILTER (1.000000)

class objVectorFilter : public Object {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORFILTER;
   static constexpr CSTRING CLASS_NAME = "VectorFilter";

   using create = pf::Create<objVectorFilter>;

   DOUBLE X;                     // X coordinate for the filter.
   DOUBLE Y;                     // Y coordinate for the filter.
   DOUBLE Width;                 // The width of the filter area.  Can be expressed as a fixed or scaled coordinate.
   DOUBLE Height;                // The height of the filter area.  Can be expressed as a fixed or scaled coordinate.
   DOUBLE Opacity;               // The opacity of the filter.
   objVectorFilter * Inherit;    // Inherit attributes from a VectorFilter referenced here.
   LONG   ResX;                  // Width of the intermediate images, measured in pixels.
   LONG   ResY;                  // Height of the intermediate images, measured in pixels.
   VUNIT  Units;                 // Defines the coordinate system for #X, #Y, #Width and #Height.
   VUNIT  PrimitiveUnits;        // Alters the behaviour of some effects that support alternative position calculations.
   LONG   Dimensions;            // Dimension flags define whether individual dimension fields contain fixed or scaled values.
   VCS    ColourSpace;           // The colour space of the filter graphics (SRGB or linear RGB).
   VFA    AspectRatio;           // Aspect ratio to use when scaling X/Y values

   // Action stubs

   inline ERR clear() noexcept { return Action(AC_Clear, this, NULL); }
   inline ERR init() noexcept { return InitObject(this); }

   // Customised field setting

   inline ERR setX(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setY(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[1];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setWidth(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[6];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setHeight(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[2];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setOpacity(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERR setInherit(objVectorFilter * Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, 0x08000301, Value, 1);
   }

   inline ERR setResX(const LONG Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->ResX = Value;
      return ERR::Okay;
   }

   inline ERR setResY(const LONG Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->ResY = Value;
      return ERR::Okay;
   }

   inline ERR setUnits(const VUNIT Value) noexcept {
      this->Units = Value;
      return ERR::Okay;
   }

   inline ERR setPrimitiveUnits(const VUNIT Value) noexcept {
      this->PrimitiveUnits = Value;
      return ERR::Okay;
   }

   inline ERR setColourSpace(const VCS Value) noexcept {
      this->ColourSpace = Value;
      return ERR::Okay;
   }

   inline ERR setAspectRatio(const VFA Value) noexcept {
      this->AspectRatio = Value;
      return ERR::Okay;
   }

};

// Vector class definition

#define VER_VECTOR (1.000000)

// Vector methods

#define MT_VecPush -1
#define MT_VecTrace -2
#define MT_VecGetBoundary -3
#define MT_VecPointInPath -4
#define MT_VecSubscribeInput -5
#define MT_VecSubscribeKeyboard -6
#define MT_VecSubscribeFeedback -7
#define MT_VecDebug -8
#define MT_VecNewMatrix -9
#define MT_VecFreeMatrix -10

struct vecPush { LONG Position;  };
struct vecTrace { FUNCTION * Callback; DOUBLE Scale; LONG Transform;  };
struct vecGetBoundary { VBF Flags; DOUBLE X; DOUBLE Y; DOUBLE Width; DOUBLE Height;  };
struct vecPointInPath { DOUBLE X; DOUBLE Y;  };
struct vecSubscribeInput { JTYPE Mask; FUNCTION * Callback;  };
struct vecSubscribeKeyboard { FUNCTION * Callback;  };
struct vecSubscribeFeedback { FM Mask; FUNCTION * Callback;  };
struct vecNewMatrix { struct VectorMatrix * Transform; LONG End;  };
struct vecFreeMatrix { struct VectorMatrix * Matrix;  };

inline ERR vecPush(APTR Ob, LONG Position) noexcept {
   struct vecPush args = { Position };
   return(Action(MT_VecPush, (OBJECTPTR)Ob, &args));
}

inline ERR vecTrace(APTR Ob, FUNCTION * Callback, DOUBLE Scale, LONG Transform) noexcept {
   struct vecTrace args = { Callback, Scale, Transform };
   return(Action(MT_VecTrace, (OBJECTPTR)Ob, &args));
}

inline ERR vecGetBoundary(APTR Ob, VBF Flags, DOUBLE * X, DOUBLE * Y, DOUBLE * Width, DOUBLE * Height) noexcept {
   struct vecGetBoundary args = { Flags, (DOUBLE)0, (DOUBLE)0, (DOUBLE)0, (DOUBLE)0 };
   ERR error = Action(MT_VecGetBoundary, (OBJECTPTR)Ob, &args);
   if (X) *X = args.X;
   if (Y) *Y = args.Y;
   if (Width) *Width = args.Width;
   if (Height) *Height = args.Height;
   return(error);
}

inline ERR vecPointInPath(APTR Ob, DOUBLE X, DOUBLE Y) noexcept {
   struct vecPointInPath args = { X, Y };
   return(Action(MT_VecPointInPath, (OBJECTPTR)Ob, &args));
}

inline ERR vecSubscribeInput(APTR Ob, JTYPE Mask, FUNCTION * Callback) noexcept {
   struct vecSubscribeInput args = { Mask, Callback };
   return(Action(MT_VecSubscribeInput, (OBJECTPTR)Ob, &args));
}

inline ERR vecSubscribeKeyboard(APTR Ob, FUNCTION * Callback) noexcept {
   struct vecSubscribeKeyboard args = { Callback };
   return(Action(MT_VecSubscribeKeyboard, (OBJECTPTR)Ob, &args));
}

inline ERR vecSubscribeFeedback(APTR Ob, FM Mask, FUNCTION * Callback) noexcept {
   struct vecSubscribeFeedback args = { Mask, Callback };
   return(Action(MT_VecSubscribeFeedback, (OBJECTPTR)Ob, &args));
}

#define vecDebug(obj) Action(MT_VecDebug,(obj),0)

inline ERR vecNewMatrix(APTR Ob, struct VectorMatrix ** Transform, LONG End) noexcept {
   struct vecNewMatrix args = { (struct VectorMatrix *)0, End };
   ERR error = Action(MT_VecNewMatrix, (OBJECTPTR)Ob, &args);
   if (Transform) *Transform = args.Transform;
   return(error);
}

inline ERR vecFreeMatrix(APTR Ob, struct VectorMatrix * Matrix) noexcept {
   struct vecFreeMatrix args = { Matrix };
   return(Action(MT_VecFreeMatrix, (OBJECTPTR)Ob, &args));
}


class objVector : public Object {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTOR;
   static constexpr CSTRING CLASS_NAME = "Vector";

   using create = pf::Create<objVector>;

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
   VIS       Visibility;              // Controls the visibility of a vector and its children.
   VF        Flags;                   // Optional flags.
   PTC       Cursor;                  // The mouse cursor to display when the pointer is within the vector's boundary.
   RQ        PathQuality;             // Defines the quality of a path when it is rendered.
   VCS       ColourSpace;             // Defines the colour space to use when blending the vector with a target bitmap's content.
   LONG      PathTimestamp;           // This counter is modified each time the path is regenerated.

   // Action stubs

   inline ERR disable() noexcept { return Action(AC_Disable, this, NULL); }
   inline ERR draw() noexcept { return Action(AC_Draw, this, NULL); }
   inline ERR drawArea(LONG X, LONG Y, LONG Width, LONG Height) noexcept {
      struct acDraw args = { X, Y, Width, Height };
      return Action(AC_Draw, this, &args);
   }
   inline ERR enable() noexcept { return Action(AC_Enable, this, NULL); }
   inline ERR hide() noexcept { return Action(AC_Hide, this, NULL); }
   inline ERR init() noexcept { return InitObject(this); }
   inline ERR moveToBack() noexcept { return Action(AC_MoveToBack, this, NULL); }
   inline ERR moveToFront() noexcept { return Action(AC_MoveToFront, this, NULL); }
   inline ERR show() noexcept { return Action(AC_Show, this, NULL); }

   // Customised field setting

   inline ERR setNext(objVector * Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[28];
      return field->WriteValue(target, field, 0x08000301, Value, 1);
   }

   inline ERR setPrev(objVector * Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[29];
      return field->WriteValue(target, field, 0x08000301, Value, 1);
   }

   inline ERR setStrokeOpacity(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[36];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERR setFillOpacity(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[43];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERR setOpacity(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[21];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERR setMiterLimit(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERR setInnerMiterLimit(const DOUBLE Value) noexcept {
      this->InnerMiterLimit = Value;
      return ERR::Okay;
   }

   inline ERR setDashOffset(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[20];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERR setVisibility(const VIS Value) noexcept {
      this->Visibility = Value;
      return ERR::Okay;
   }

   inline ERR setFlags(const VF Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Flags = Value;
      return ERR::Okay;
   }

   inline ERR setCursor(const PTC Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[44];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setPathQuality(const RQ Value) noexcept {
      this->PathQuality = Value;
      return ERR::Okay;
   }

   inline ERR setColourSpace(const VCS Value) noexcept {
      this->ColourSpace = Value;
      return ERR::Okay;
   }

   inline ERR setClipRule(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[18];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setDashArray(const DOUBLE * Value, LONG Elements) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[30];
      return field->WriteValue(target, field, 0x80001308, Value, Elements);
   }

   inline ERR setMask(OBJECTPTR Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[26];
      return field->WriteValue(target, field, 0x08000309, Value, 1);
   }

   inline ERR setMorph(OBJECTPTR Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, 0x08000309, Value, 1);
   }

   inline ERR setAppendPath(OBJECTPTR Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[22];
      return field->WriteValue(target, field, 0x08000309, Value, 1);
   }

   inline ERR setMorphFlags(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[17];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setNumeric(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[35];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   template <class T> inline ERR setID(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, 0x08800308, to_cstring(Value), 1);
   }

   inline ERR setResizeEvent(const FUNCTION Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[38];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   template <class T> inline ERR setStroke(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, 0x08800308, to_cstring(Value), 1);
   }

   inline ERR setStrokeColour(const FLOAT * Value, LONG Elements) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      return field->WriteValue(target, field, 0x10001308, Value, Elements);
   }

   inline ERR setStrokeWidth(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[10];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERR setTransition(OBJECTPTR Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[31];
      return field->WriteValue(target, field, 0x08000309, Value, 1);
   }

   inline ERR setEnableBkgd(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[41];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   template <class T> inline ERR setFill(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[25];
      return field->WriteValue(target, field, 0x08800308, to_cstring(Value), 1);
   }

   inline ERR setFillColour(const FLOAT * Value, LONG Elements) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[34];
      return field->WriteValue(target, field, 0x10001308, Value, Elements);
   }

   inline ERR setFillRule(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[15];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   template <class T> inline ERR setFilter(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[45];
      return field->WriteValue(target, field, 0x08800308, to_cstring(Value), 1);
   }

   inline ERR setLineJoin(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[37];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setLineCap(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[24];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setInnerJoin(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[16];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setTabOrder(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[32];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

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

inline ERR vpAddCommand(APTR Ob, struct PathCommand * Commands, LONG Size) noexcept {
   struct vpAddCommand args = { Commands, Size };
   return(Action(MT_VPAddCommand, (OBJECTPTR)Ob, &args));
}

inline ERR vpRemoveCommand(APTR Ob, LONG Index, LONG Total) noexcept {
   struct vpRemoveCommand args = { Index, Total };
   return(Action(MT_VPRemoveCommand, (OBJECTPTR)Ob, &args));
}

inline ERR vpSetCommand(APTR Ob, LONG Index, struct PathCommand * Command, LONG Size) noexcept {
   struct vpSetCommand args = { Index, Command, Size };
   return(Action(MT_VPSetCommand, (OBJECTPTR)Ob, &args));
}

inline ERR vpGetCommand(APTR Ob, LONG Index, struct PathCommand ** Command) noexcept {
   struct vpGetCommand args = { Index, (struct PathCommand *)0 };
   ERR error = Action(MT_VPGetCommand, (OBJECTPTR)Ob, &args);
   if (Command) *Command = args.Command;
   return(error);
}

inline ERR vpSetCommandList(APTR Ob, APTR Commands, LONG Size) noexcept {
   struct vpSetCommandList args = { Commands, Size };
   return(Action(MT_VPSetCommandList, (OBJECTPTR)Ob, &args));
}


class objVectorPath : public objVector {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORPATH;
   static constexpr CSTRING CLASS_NAME = "VectorPath";

   using create = pf::Create<objVectorPath>;
};

// VectorText class definition

#define VER_VECTORTEXT (1.000000)

// VectorText methods

#define MT_VTDeleteLine -30

struct vtDeleteLine { LONG Line;  };

inline ERR vtDeleteLine(APTR Ob, LONG Line) noexcept {
   struct vtDeleteLine args = { Line };
   return(Action(MT_VTDeleteLine, (OBJECTPTR)Ob, &args));
}


class objVectorText : public objVector {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORTEXT;
   static constexpr CSTRING CLASS_NAME = "VectorText";

   using create = pf::Create<objVectorText>;
};

// VectorGroup class definition

#define VER_VECTORGROUP (1.000000)

class objVectorGroup : public objVector {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORGROUP;
   static constexpr CSTRING CLASS_NAME = "VectorGroup";

   using create = pf::Create<objVectorGroup>;
};

// VectorWave class definition

#define VER_VECTORWAVE (1.000000)

class objVectorWave : public objVector {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORWAVE;
   static constexpr CSTRING CLASS_NAME = "VectorWave";

   using create = pf::Create<objVectorWave>;
};

// VectorRectangle class definition

#define VER_VECTORRECTANGLE (1.000000)

class objVectorRectangle : public objVector {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORRECTANGLE;
   static constexpr CSTRING CLASS_NAME = "VectorRectangle";

   using create = pf::Create<objVectorRectangle>;
};

// VectorPolygon class definition

#define VER_VECTORPOLYGON (1.000000)

class objVectorPolygon : public objVector {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORPOLYGON;
   static constexpr CSTRING CLASS_NAME = "VectorPolygon";

   using create = pf::Create<objVectorPolygon>;
};

// VectorShape class definition

#define VER_VECTORSHAPE (1.000000)

class objVectorShape : public objVector {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORSHAPE;
   static constexpr CSTRING CLASS_NAME = "VectorShape";

   using create = pf::Create<objVectorShape>;
};

// VectorSpiral class definition

#define VER_VECTORSPIRAL (1.000000)

class objVectorSpiral : public objVector {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORSPIRAL;
   static constexpr CSTRING CLASS_NAME = "VectorSpiral";

   using create = pf::Create<objVectorSpiral>;
};

// VectorEllipse class definition

#define VER_VECTORELLIPSE (1.000000)

class objVectorEllipse : public objVector {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORELLIPSE;
   static constexpr CSTRING CLASS_NAME = "VectorEllipse";

   using create = pf::Create<objVectorEllipse>;
};

// VectorViewport class definition

#define VER_VECTORVIEWPORT (1.000000)

class objVectorViewport : public objVector {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORVIEWPORT;
   static constexpr CSTRING CLASS_NAME = "VectorViewport";

   using create = pf::Create<objVectorViewport>;
};

#ifdef PARASOL_STATIC
#define JUMPTABLE_VECTOR static struct VectorBase *VectorBase;
#else
#define JUMPTABLE_VECTOR struct VectorBase *VectorBase;
#endif

struct VectorBase {
#ifndef PARASOL_STATIC
   ERR (*_DrawPath)(objBitmap * Bitmap, APTR Path, DOUBLE StrokeWidth, OBJECTPTR StrokeStyle, OBJECTPTR FillStyle);
   ERR (*_GenerateEllipse)(DOUBLE CX, DOUBLE CY, DOUBLE RX, DOUBLE RY, LONG Vertices, APTR Path);
   ERR (*_GeneratePath)(CSTRING Sequence, APTR Path);
   ERR (*_GenerateRectangle)(DOUBLE X, DOUBLE Y, DOUBLE Width, DOUBLE Height, APTR Path);
   ERR (*_ReadPainter)(objVectorScene * Scene, CSTRING IRI, struct VectorPainter * Painter, CSTRING * Result);
   void (*_TranslatePath)(APTR Path, DOUBLE X, DOUBLE Y);
   void (*_MoveTo)(APTR Path, DOUBLE X, DOUBLE Y);
   void (*_LineTo)(APTR Path, DOUBLE X, DOUBLE Y);
   void (*_ArcTo)(APTR Path, DOUBLE RX, DOUBLE RY, DOUBLE Angle, DOUBLE X, DOUBLE Y, ARC Flags);
   void (*_Curve3)(APTR Path, DOUBLE CtrlX, DOUBLE CtrlY, DOUBLE X, DOUBLE Y);
   void (*_Smooth3)(APTR Path, DOUBLE X, DOUBLE Y);
   void (*_Curve4)(APTR Path, DOUBLE CtrlX1, DOUBLE CtrlY1, DOUBLE CtrlX2, DOUBLE CtrlY2, DOUBLE X, DOUBLE Y);
   void (*_Smooth4)(APTR Path, DOUBLE CtrlX, DOUBLE CtrlY, DOUBLE X, DOUBLE Y);
   void (*_ClosePath)(APTR Path);
   void (*_RewindPath)(APTR Path);
   LONG (*_GetVertex)(APTR Path, DOUBLE * X, DOUBLE * Y);
   ERR (*_ApplyPath)(APTR Path, OBJECTPTR VectorPath);
   ERR (*_Rotate)(struct VectorMatrix * Matrix, DOUBLE Angle, DOUBLE CenterX, DOUBLE CenterY);
   ERR (*_Translate)(struct VectorMatrix * Matrix, DOUBLE X, DOUBLE Y);
   ERR (*_Skew)(struct VectorMatrix * Matrix, DOUBLE X, DOUBLE Y);
   ERR (*_Multiply)(struct VectorMatrix * Matrix, DOUBLE ScaleX, DOUBLE ShearY, DOUBLE ShearX, DOUBLE ScaleY, DOUBLE TranslateX, DOUBLE TranslateY);
   ERR (*_MultiplyMatrix)(struct VectorMatrix * Target, struct VectorMatrix * Source);
   ERR (*_Scale)(struct VectorMatrix * Matrix, DOUBLE X, DOUBLE Y);
   ERR (*_ParseTransform)(struct VectorMatrix * Matrix, CSTRING Transform);
   ERR (*_ResetMatrix)(struct VectorMatrix * Matrix);
   ERR (*_GetFontHandle)(CSTRING Family, CSTRING Style, LONG Weight, LONG Size, APTR Handle);
   ERR (*_GetFontMetrics)(APTR Handle, struct FontMetrics * Info);
   DOUBLE (*_CharWidth)(APTR FontHandle, ULONG Char, ULONG KChar, DOUBLE * Kerning);
   DOUBLE (*_StringWidth)(APTR FontHandle, CSTRING String, LONG Chars);
   ERR (*_FlushMatrix)(struct VectorMatrix * Matrix);
   ERR (*_TracePath)(APTR Path, FUNCTION Callback, DOUBLE Scale);
#endif // PARASOL_STATIC
};

#ifndef PRV_VECTOR_MODULE
#ifndef PARASOL_STATIC
extern struct VectorBase *VectorBase;
inline ERR vecDrawPath(objBitmap * Bitmap, APTR Path, DOUBLE StrokeWidth, OBJECTPTR StrokeStyle, OBJECTPTR FillStyle) { return VectorBase->_DrawPath(Bitmap,Path,StrokeWidth,StrokeStyle,FillStyle); }
inline ERR vecGenerateEllipse(DOUBLE CX, DOUBLE CY, DOUBLE RX, DOUBLE RY, LONG Vertices, APTR Path) { return VectorBase->_GenerateEllipse(CX,CY,RX,RY,Vertices,Path); }
inline ERR vecGeneratePath(CSTRING Sequence, APTR Path) { return VectorBase->_GeneratePath(Sequence,Path); }
inline ERR vecGenerateRectangle(DOUBLE X, DOUBLE Y, DOUBLE Width, DOUBLE Height, APTR Path) { return VectorBase->_GenerateRectangle(X,Y,Width,Height,Path); }
inline ERR vecReadPainter(objVectorScene * Scene, CSTRING IRI, struct VectorPainter * Painter, CSTRING * Result) { return VectorBase->_ReadPainter(Scene,IRI,Painter,Result); }
inline void vecTranslatePath(APTR Path, DOUBLE X, DOUBLE Y) { return VectorBase->_TranslatePath(Path,X,Y); }
inline void vecMoveTo(APTR Path, DOUBLE X, DOUBLE Y) { return VectorBase->_MoveTo(Path,X,Y); }
inline void vecLineTo(APTR Path, DOUBLE X, DOUBLE Y) { return VectorBase->_LineTo(Path,X,Y); }
inline void vecArcTo(APTR Path, DOUBLE RX, DOUBLE RY, DOUBLE Angle, DOUBLE X, DOUBLE Y, ARC Flags) { return VectorBase->_ArcTo(Path,RX,RY,Angle,X,Y,Flags); }
inline void vecCurve3(APTR Path, DOUBLE CtrlX, DOUBLE CtrlY, DOUBLE X, DOUBLE Y) { return VectorBase->_Curve3(Path,CtrlX,CtrlY,X,Y); }
inline void vecSmooth3(APTR Path, DOUBLE X, DOUBLE Y) { return VectorBase->_Smooth3(Path,X,Y); }
inline void vecCurve4(APTR Path, DOUBLE CtrlX1, DOUBLE CtrlY1, DOUBLE CtrlX2, DOUBLE CtrlY2, DOUBLE X, DOUBLE Y) { return VectorBase->_Curve4(Path,CtrlX1,CtrlY1,CtrlX2,CtrlY2,X,Y); }
inline void vecSmooth4(APTR Path, DOUBLE CtrlX, DOUBLE CtrlY, DOUBLE X, DOUBLE Y) { return VectorBase->_Smooth4(Path,CtrlX,CtrlY,X,Y); }
inline void vecClosePath(APTR Path) { return VectorBase->_ClosePath(Path); }
inline void vecRewindPath(APTR Path) { return VectorBase->_RewindPath(Path); }
inline LONG vecGetVertex(APTR Path, DOUBLE * X, DOUBLE * Y) { return VectorBase->_GetVertex(Path,X,Y); }
inline ERR vecApplyPath(APTR Path, OBJECTPTR VectorPath) { return VectorBase->_ApplyPath(Path,VectorPath); }
inline ERR vecRotate(struct VectorMatrix * Matrix, DOUBLE Angle, DOUBLE CenterX, DOUBLE CenterY) { return VectorBase->_Rotate(Matrix,Angle,CenterX,CenterY); }
inline ERR vecTranslate(struct VectorMatrix * Matrix, DOUBLE X, DOUBLE Y) { return VectorBase->_Translate(Matrix,X,Y); }
inline ERR vecSkew(struct VectorMatrix * Matrix, DOUBLE X, DOUBLE Y) { return VectorBase->_Skew(Matrix,X,Y); }
inline ERR vecMultiply(struct VectorMatrix * Matrix, DOUBLE ScaleX, DOUBLE ShearY, DOUBLE ShearX, DOUBLE ScaleY, DOUBLE TranslateX, DOUBLE TranslateY) { return VectorBase->_Multiply(Matrix,ScaleX,ShearY,ShearX,ScaleY,TranslateX,TranslateY); }
inline ERR vecMultiplyMatrix(struct VectorMatrix * Target, struct VectorMatrix * Source) { return VectorBase->_MultiplyMatrix(Target,Source); }
inline ERR vecScale(struct VectorMatrix * Matrix, DOUBLE X, DOUBLE Y) { return VectorBase->_Scale(Matrix,X,Y); }
inline ERR vecParseTransform(struct VectorMatrix * Matrix, CSTRING Transform) { return VectorBase->_ParseTransform(Matrix,Transform); }
inline ERR vecResetMatrix(struct VectorMatrix * Matrix) { return VectorBase->_ResetMatrix(Matrix); }
inline ERR vecGetFontHandle(CSTRING Family, CSTRING Style, LONG Weight, LONG Size, APTR Handle) { return VectorBase->_GetFontHandle(Family,Style,Weight,Size,Handle); }
inline ERR vecGetFontMetrics(APTR Handle, struct FontMetrics * Info) { return VectorBase->_GetFontMetrics(Handle,Info); }
inline DOUBLE vecCharWidth(APTR FontHandle, ULONG Char, ULONG KChar, DOUBLE * Kerning) { return VectorBase->_CharWidth(FontHandle,Char,KChar,Kerning); }
inline DOUBLE vecStringWidth(APTR FontHandle, CSTRING String, LONG Chars) { return VectorBase->_StringWidth(FontHandle,String,Chars); }
inline ERR vecFlushMatrix(struct VectorMatrix * Matrix) { return VectorBase->_FlushMatrix(Matrix); }
inline ERR vecTracePath(APTR Path, FUNCTION Callback, DOUBLE Scale) { return VectorBase->_TracePath(Path,Callback,Scale); }
#else
extern "C" {
extern ERR vecDrawPath(objBitmap * Bitmap, APTR Path, DOUBLE StrokeWidth, OBJECTPTR StrokeStyle, OBJECTPTR FillStyle);
extern ERR vecGenerateEllipse(DOUBLE CX, DOUBLE CY, DOUBLE RX, DOUBLE RY, LONG Vertices, APTR Path);
extern ERR vecGeneratePath(CSTRING Sequence, APTR Path);
extern ERR vecGenerateRectangle(DOUBLE X, DOUBLE Y, DOUBLE Width, DOUBLE Height, APTR Path);
extern ERR vecReadPainter(objVectorScene * Scene, CSTRING IRI, struct VectorPainter * Painter, CSTRING * Result);
extern void vecTranslatePath(APTR Path, DOUBLE X, DOUBLE Y);
extern void vecMoveTo(APTR Path, DOUBLE X, DOUBLE Y);
extern void vecLineTo(APTR Path, DOUBLE X, DOUBLE Y);
extern void vecArcTo(APTR Path, DOUBLE RX, DOUBLE RY, DOUBLE Angle, DOUBLE X, DOUBLE Y, ARC Flags);
extern void vecCurve3(APTR Path, DOUBLE CtrlX, DOUBLE CtrlY, DOUBLE X, DOUBLE Y);
extern void vecSmooth3(APTR Path, DOUBLE X, DOUBLE Y);
extern void vecCurve4(APTR Path, DOUBLE CtrlX1, DOUBLE CtrlY1, DOUBLE CtrlX2, DOUBLE CtrlY2, DOUBLE X, DOUBLE Y);
extern void vecSmooth4(APTR Path, DOUBLE CtrlX, DOUBLE CtrlY, DOUBLE X, DOUBLE Y);
extern void vecClosePath(APTR Path);
extern void vecRewindPath(APTR Path);
extern LONG vecGetVertex(APTR Path, DOUBLE * X, DOUBLE * Y);
extern ERR vecApplyPath(APTR Path, OBJECTPTR VectorPath);
extern ERR vecRotate(struct VectorMatrix * Matrix, DOUBLE Angle, DOUBLE CenterX, DOUBLE CenterY);
extern ERR vecTranslate(struct VectorMatrix * Matrix, DOUBLE X, DOUBLE Y);
extern ERR vecSkew(struct VectorMatrix * Matrix, DOUBLE X, DOUBLE Y);
extern ERR vecMultiply(struct VectorMatrix * Matrix, DOUBLE ScaleX, DOUBLE ShearY, DOUBLE ShearX, DOUBLE ScaleY, DOUBLE TranslateX, DOUBLE TranslateY);
extern ERR vecMultiplyMatrix(struct VectorMatrix * Target, struct VectorMatrix * Source);
extern ERR vecScale(struct VectorMatrix * Matrix, DOUBLE X, DOUBLE Y);
extern ERR vecParseTransform(struct VectorMatrix * Matrix, CSTRING Transform);
extern ERR vecResetMatrix(struct VectorMatrix * Matrix);
extern ERR vecGetFontHandle(CSTRING Family, CSTRING Style, LONG Weight, LONG Size, APTR Handle);
extern ERR vecGetFontMetrics(APTR Handle, struct FontMetrics * Info);
extern DOUBLE vecCharWidth(APTR FontHandle, ULONG Char, ULONG KChar, DOUBLE * Kerning);
extern DOUBLE vecStringWidth(APTR FontHandle, CSTRING String, LONG Chars);
extern ERR vecFlushMatrix(struct VectorMatrix * Matrix);
extern ERR vecTracePath(APTR Path, FUNCTION Callback, DOUBLE Scale);
}
#endif // PARASOL_STATIC
#endif


//********************************************************************************************************************

inline void operator*=(VectorMatrix &This, const VectorMatrix &Other)
{
   DOUBLE t0 = This.ScaleX * Other.ScaleX + This.ShearY * Other.ShearX;
   DOUBLE t2 = This.ShearX * Other.ScaleX + This.ScaleY * Other.ShearX;
   DOUBLE t4 = This.TranslateX * Other.ScaleX + This.TranslateY * Other.ShearX + Other.TranslateX;
   This.ShearY     = This.ScaleX * Other.ShearY + This.ShearY * Other.ScaleY;
   This.ScaleY     = This.ShearX * Other.ShearY + This.ScaleY * Other.ScaleY;
   This.TranslateY = This.TranslateX  * Other.ShearY + This.TranslateY * Other.ScaleY + Other.TranslateY;
   This.ScaleX     = t0;
   This.ShearX     = t2;
   This.TranslateX = t4;
}

//********************************************************************************************************************

inline void SET_VECTOR_COLOUR(objVectorColour *Colour, DOUBLE Red, DOUBLE Green, DOUBLE Blue, DOUBLE Alpha) {
   Colour->Class->ClassID = ID_VECTORCOLOUR;
   Colour->Red   = Red;
   Colour->Green = Green;
   Colour->Blue  = Blue;
   Colour->Alpha = Alpha;
}
#define SVF_A 0x0002b606
#define SVF_ACHROMATOMALY 0xc3f37036
#define SVF_ACHROMATOPSIA 0xc3f56170
#define SVF_ADDITIVE 0x035604af
#define SVF_ALIGN 0x0f174e50
#define SVF_ALT_FILL 0x8c3507fa
#define SVF_AMPLITUDE 0x5e60600a
#define SVF_ANIMATE 0x36d195e4
#define SVF_ANIMATECOLOR 0xcd2d1683
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
#define SVF_CLIP 0x7c95326d
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
#define SVF_CROSSORIGIN 0x8e204b17
#define SVF_CX 0x00597780
#define SVF_CY 0x00597781
#define SVF_D 0x0002b609
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
#define SVF_FEDROPSHADOW 0x1c907ecb
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
#define SVF_SET 0x0b88a991
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
#define SVF_X 0x0002b61d
#define SVF_X1 0x005979ee
#define SVF_X2 0x005979ef
#define SVF_XOFFSET 0x23685e64
#define SVF_XLINK_HREF 0x379480aa
#define SVF_XML_SPACE 0x2db612fc
#define SVF_XMLNS 0x10b81bf7
#define SVF_XOR 0x0b88c01e
#define SVF_Y 0x0002b61e
#define SVF_Y1 0x00597a0f
#define SVF_Y2 0x00597a10
#define SVF_YOFFSET 0x70629b25
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
#define SVF_VERTEX_SCALING 0x2363f691
#define SVF_VERTICES 0xd31fda6a
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

#define SVF_APPEND_PATH 0x64cbc017
#define SVF_JOIN_PATH 0x34d6680f
#define SVF_AZIMUTH 0x52cfd287
#define SVF_DARKEN 0xf83e845a
#define SVF_DECAY 0x0f49a6eb
#define SVF_DECODING 0x13246362
#define SVF_DEFS 0x7c95a0a7
#define SVF_ELEVATION 0x0c12538c
#define SVF_FEFUNCR 0xa284a6ae
#define SVF_FEFUNCG 0xa284a6a3
#define SVF_FEFUNCB 0xa284a69e
#define SVF_FEFUNCA 0xa284a69d
#define SVF_LIGHTING_COLOR 0x020fc127
#define SVF_LIGHTING_COLOUR 0x4407e6dc
#define SVF_LIMITINGCONEANGLE 0xbb90036e
#define SVF_LOOP_LIMIT 0xfaf3e6cb
#define SVF_MASKCONTENTUNITS 0x3fe629df
#define SVF_MASKUNITS 0xa68eea04
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
#define SVF_SWITCH 0x1cc53777
#define SVF_XCHANNELSELECTOR 0x57175337
#define SVF_YCHANNELSELECTOR 0x634c7918
#define SVF_ZOOMANDPAN 0xc606dfdc
#define SVF_EXPANDED 0xd353d90e
#define SVF_SEMI_EXPANDED 0xa6ff90c9
#define SVF_EXTRA_EXPANDED 0x8c599b5f
#define SVF_ULTRA_EXPANDED 0x87e8c363
#define SVF_CALCMODE 0x0723eabd
#define SVF_KEYPOINTS 0x47b5578b
#define SVF_ORIGIN 0x1315e3ed
#define SVF_KEYTIMES 0xbc9ffbb0
#define SVF_KEYSPLINES 0x27d7988c
#define SVF_BY 0x00597760
#define SVF_YELLOW 0x297ff6e1
#define SVF_YELLOWGREEN 0xda4a85b2


INLINE ERR vecSubscribeInput(APTR Ob, JTYPE Mask, FUNCTION Callback) {
   struct vecSubscribeInput args = { Mask, &Callback };
   return(Action(MT_VecSubscribeInput, (OBJECTPTR)Ob, &args));
}

INLINE ERR vecSubscribeKeyboard(APTR Ob, FUNCTION Callback) {
   struct vecSubscribeKeyboard args = { &Callback };
   return(Action(MT_VecSubscribeKeyboard, (OBJECTPTR)Ob, &args));
}

INLINE ERR vecSubscribeFeedback(APTR Ob, FM Mask, FUNCTION Callback) {
   struct vecSubscribeFeedback args = { Mask, &Callback };
   return(Action(MT_VecSubscribeFeedback, (OBJECTPTR)Ob, &args));
}

namespace fl {
   using namespace pf;

constexpr FieldValue Flags(VCLF Value) { return FieldValue(FID_Flags, LONG(Value)); }

constexpr FieldValue AppendPath(OBJECTPTR Value) { return FieldValue(FID_AppendPath, Value); }

constexpr FieldValue DragCallback(const FUNCTION &Value) { return FieldValue(FID_DragCallback, &Value); }
constexpr FieldValue DragCallback(const FUNCTION *Value) { return FieldValue(FID_DragCallback, Value); }

constexpr FieldValue TextFlags(VTXF Value) { return FieldValue(FID_TextFlags, LONG(Value)); }
constexpr FieldValue Overflow(VOF Value) { return FieldValue(FID_Overflow, LONG(Value)); }

constexpr FieldValue Sequence(CSTRING Value) { return FieldValue(FID_Sequence, Value); }
inline FieldValue Sequence(std::string &Value) { return FieldValue(FID_Sequence, Value.c_str()); }

constexpr FieldValue FontStyle(CSTRING Value) { return FieldValue(FID_FontStyle, Value); }
inline FieldValue FontStyle(std::string &Value) { return FieldValue(FID_FontStyle, Value.c_str()); }

template <class T> FieldValue RoundX(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_base_of_v<SCALE, T>, "RoundX value must be numeric");
   return FieldValue(FID_RoundX, Value);
}

template <class T> FieldValue RoundY(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_base_of_v<SCALE, T>, "RoundY value must be numeric");
   return FieldValue(FID_RoundY, Value);
}

}

