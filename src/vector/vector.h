
#define PRV_VECTOR
#define PRV_VECTORSCENE
#define PRV_VECTORPATTERN
#define PRV_VECTORGRADIENT
#define PRV_VECTORFILTER
#define PRV_VECTORPATH
#define PRV_VECTOR_MODULE
#define PRV_FILTEREFFECT

#define DBG_TRANSFORM(args...) //log.trace(args)

#define FIXED_DPI 96 // Freetype measurements are based on this DPI.
#define FT_DOWNSIZE 6
#define FT_UPSIZE 6
#define DEG2RAD 0.0174532925 // Multiple any angle by this value to convert to radians

#include <array>
#include <memory>
#include <unordered_set>
#include <sstream>
#include <set>
#include <unordered_map>
#include <mutex>

#include <parasol/main.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/picture.h>
#include <parasol/modules/display.h>
#include <parasol/modules/font.h>

#include "agg_alpha_mask_u8.h"
#include "agg_basics.h"
#include "agg_bounding_rect.h"
#include "agg_conv_dash.h"
#include "agg_conv_stroke.h"
#include "agg_conv_transform.h"
#include "agg_curves.h"
#include "agg_gamma_lut.h"
#include "agg_path_storage.h"
#include "agg_pattern_filters_rgba.h"
#include "agg_pixfmt_gray.h"
#include "agg_pixfmt_rgba.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_renderer_scanline.h"
#include "agg_rendering_buffer.h"
#include "agg_scanline_p.h"
#include "agg_scanline_u.h"
#include "agg_span_allocator.h"
#include "agg_span_converter.h"
#include "agg_span_image_filter_rgba.h"
#include "agg_trans_affine.h"
//#include "agg_conv_marker.h"
//#include "agg_vcgen_markers_term.h"

#include <parasol/linear_rgb.h>

#include <math.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <float.h>

extern OBJECTPTR clVectorScene, clVectorViewport, clVectorGroup, clVectorColour;
extern OBJECTPTR clVectorEllipse, clVectorRectangle, clVectorPath, clVectorWave;
extern OBJECTPTR clVectorFilter, clVectorPolygon, clVectorText, clVectorClip;
extern OBJECTPTR clVectorGradient, clVectorImage, clVectorPattern, clVector;
extern OBJECTPTR clVectorSpiral, clVectorShape, clVectorTransition, clImageFX;
extern OBJECTPTR clBlurFX, clColourFX, clCompositeFX, clConvolveFX, clFilterEffect;
extern OBJECTPTR clFloodFX, clMergeFX, clMorphologyFX, clOffsetFX, clTurbulenceFX;

extern struct DisplayBase *DisplayBase;
extern struct FontBase *FontBase;

typedef agg::pod_auto_array<agg::rgba8, 256> GRADIENT_TABLE;
typedef class plVectorClip objVectorClip;
typedef class plVectorTransition objVectorTransition;
typedef class plVectorText objVectorText;

//****************************************************************************

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

class filter_state {
public:
};

class filter_bitmap {
public:
   objBitmap *Bitmap;
   UBYTE *Data;
   LONG DataSize;

   filter_bitmap() : Bitmap(NULL), Data(NULL), DataSize(0) { };

   ~filter_bitmap() {
      if (Bitmap) { acFree(Bitmap); Bitmap = NULL; }
      if (Data) { FreeResource(Data); Data = NULL; }
   };

   objBitmap * get_bitmap(LONG Width, LONG Height, ClipRectangle &Clip, bool Debug) {
      if (Bitmap) {
         Bitmap->Width = Width;
         Bitmap->Height = Height;
      }
      else {
         // NB: The clip region defines the true size and no data is allocated by the bitmap itself unless in debug mode.
         if (CreateObject(ID_BITMAP, NF_INTEGRAL, &Bitmap,
               FID_Name|TSTR,          "dummy_fx_bitmap",
               FID_Width|TLONG,        Width,
               FID_Height|TLONG,       Height,
               FID_BitsPerPixel|TLONG, 32,
               FID_Flags|TLONG,        Debug ? BMF_ALPHA_CHANNEL : (BMF_ALPHA_CHANNEL|BMF_NO_DATA),
               TAGEND) != ERR_Okay) return NULL;
      }

      Bitmap->Clip = Clip;

      if (!Debug) {
         const LONG canvas_width  = Clip.Right - Clip.Left;
         const LONG canvas_height = Clip.Bottom - Clip.Top;
         Bitmap->LineWidth = canvas_width * Bitmap->BytesPerPixel;

         if ((Data) and (DataSize < Bitmap->LineWidth * canvas_height)) {
            FreeResource(Data);
            Data = NULL;
            Bitmap->Data = NULL;
         }

         if (!Bitmap->Data) {
            if (!AllocMemory(Bitmap->LineWidth * canvas_height, MEM_DATA|MEM_NO_CLEAR, &Data, NULL)) {
               DataSize = Bitmap->LineWidth * canvas_height;
            }
         }

         Bitmap->Data = Data - (Clip.Left * Bitmap->BytesPerPixel) - (Clip.Top * Bitmap->LineWidth);
      }

      return Bitmap;
   }

};

#define TB_NOISE 1

#include <parasol/modules/vector.h>

//****************************************************************************
// NB: Considered a shape (can be transformed).

typedef class plVectorViewport : public objVector {
   public:
   FUNCTION vpDragCallback;
   DOUBLE vpViewX, vpViewY, vpViewWidth, vpViewHeight;     // Viewbox values determine the area of the SVG content that is being sourced.  These values are always fixed pixel units.
   DOUBLE vpTargetX, vpTargetY, vpTargetXO, vpTargetYO, vpTargetWidth, vpTargetHeight; // Target dimensions
   DOUBLE vpXScale, vpYScale;                              // Internal scaling for View -to-> Target.  Does not affect the view itself.
   DOUBLE vpFixedWidth, vpFixedHeight; // Fixed pixel position values, relative to parent viewport
   DOUBLE vpBX1, vpBY1, vpBX2, vpBY2; // Bounding box coordinates relative to (0,0), used for clipping
   DOUBLE vpAlignX, vpAlignY;
   objVectorClip *vpClipMask; // Automatically generated if the viewport is rotated or sheared.  This is in addition to the Vector ClipMask, which can be user-defined.
   LONG  vpDimensions;
   LONG  vpAspectRatio;
   UBYTE vpDragging:1;
   UBYTE vpOverflowX, vpOverflowY;
} objVectorViewport;

//****************************************************************************

typedef class plVectorPoly : public objVector {
   public:
   struct VectorPoint *Points;
   LONG TotalPoints;
   bool Closed:1;      // Polygons are closed (TRUE) and Polylines are open (FALSE)
} objVectorPoly;

//****************************************************************************

typedef class plVectorPath : public objVector {
   public:
   std::vector<PathCommand> Commands;
   agg::path_storage *CustomPath;
} objVectorPath;

//****************************************************************************

typedef class plVectorRectangle : public objVector {
   public:
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

typedef class plVectorTransition : public BaseClass {
   public:
   LONG TotalStops; // Total number of stops registered.

#ifdef PRV_VECTOR
   struct TransitionStop Stops[MAX_TRANSITION_STOPS];
   bool Dirty:1;
#endif
} objVectorTransition;

//****************************************************************************

class GradientColours {
   public:
      GradientColours(objVectorGradient *, DOUBLE);
      GRADIENT_TABLE table;
};

typedef class plVectorClip : public objVector {
   public:
   UBYTE *ClipData;
   agg::path_storage *ClipPath; // Internally generated path
   agg::rendering_buffer ClipRenderer;
   objVector *TargetVector;
   LONG ClipUnits;
   LONG ClipSize;
} objVectorClip;

extern CSTRING get_name(OBJECTPTR);
extern CSTRING read_numseq(CSTRING, ...);
extern DOUBLE read_unit(CSTRING, UBYTE *);
extern ERROR init_blurfx(void);
extern ERROR init_colour(void);
extern ERROR init_colourfx(void);
extern ERROR init_compositefx(void);
extern ERROR init_convolvefx(void);
extern ERROR init_filter(void);
extern ERROR init_filtereffect(void);
extern ERROR init_floodfx(void);
extern ERROR init_gradient(void);
extern ERROR init_image(void);
extern ERROR init_imagefx(void);
extern ERROR init_mergefx(void);
extern ERROR init_morphfx(void);
extern ERROR init_offsetfx(void);
extern ERROR init_pattern(void);
extern ERROR init_transition(void);
extern ERROR init_turbulencefx(void);
extern ERROR init_vectorscene(void);

extern ERROR read_path(std::vector<PathCommand> &, CSTRING);
extern ERROR scene_input_events(const InputEvent *, LONG);
extern GRADIENT_TABLE * get_fill_gradient_table(objVector &, DOUBLE);
extern GRADIENT_TABLE * get_stroke_gradient_table(objVector &);
extern void apply_parent_transforms(objVector *Start, agg::trans_affine &AGGTransform);
extern void apply_transition(objVectorTransition *, DOUBLE, agg::trans_affine &);
extern void apply_transition_xy(objVectorTransition *, DOUBLE, DOUBLE *, DOUBLE *);
extern void calc_aspectratio(CSTRING, LONG, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE *X, DOUBLE *Y, DOUBLE *, DOUBLE *);
extern void calc_full_boundary(objVector *, std::array<DOUBLE, 4> &, bool IncludeSiblings = true, bool IncludeTransforms = true);
extern void convert_to_aggpath(std::vector<PathCommand> &, agg::path_storage *);
extern void gen_vector_path(objVector *);
extern void gen_vector_tree(objVector *);
extern void send_feedback(objVector *, LONG);
extern void setRasterClip(agg::rasterizer_scanline_aa<> &, LONG, LONG, LONG, LONG);
extern void set_filter(agg::image_filter_lut &, UBYTE);
extern ERROR render_filter(objVectorFilter *, objVectorViewport *, objVector *, objBitmap *, objBitmap **);
extern objBitmap * get_source_graphic(objVectorFilter *);

extern const FieldDef clAspectRatio[];
extern std::recursive_mutex glFocusLock;
extern std::vector<objVector *> glFocusList; // The first reference is the most foreground object with the focus

//********************************************************************************************************************
// Mark a vector and all its children as needing some form of recomputation.

template <class T>
inline static void mark_dirty(T *Vector, const UBYTE Flags)
{
   Vector->Dirty |= Flags;
   for (auto scan=(objVector *)Vector->Child; scan; scan=(objVector *)scan->Next) {
      if ((scan->Dirty & Flags) == Flags) continue;
      mark_dirty(scan, Flags);
   }
}

//********************************************************************************************************************

inline static agg::path_storage basic_path(DOUBLE X1, DOUBLE Y1, DOUBLE X2, DOUBLE Y2)
{
   agg::path_storage path;
   path.move_to(X1, Y1);
   path.line_to(X2, Y1);
   path.line_to(X2, Y2);
   path.line_to(X1, Y2);
   path.close_polygon();
   return std::move(path);
}

//********************************************************************************************************************
// Call reset_path() when the shape of the vector requires recalculation.

template <class T>
inline static void reset_path(T *Vector)
{
   Vector->Dirty |= RC_BASE_PATH;
   mark_dirty(Vector, RC_FINAL_PATH);
}

//********************************************************************************************************************
// Call reset_final_path() when the base path is still valid and the vector is affected by a transform or coordinate
// translation.

template <class T>
inline static void reset_final_path(T *Vector)
{
   mark_dirty(Vector, RC_FINAL_PATH);
}

//********************************************************************************************************************

template <class T>
inline static void apply_transforms(const T &Vector, agg::trans_affine &AGGTransform)
{
   for (auto t=Vector.Matrices; t; t=t->Next) {
      AGGTransform.multiply(t->ScaleX, t->ShearY, t->ShearX, t->ScaleY, t->TranslateX, t->TranslateY);
   }
}

//********************************************************************************************************************

#include "pixfmt.h"

namespace agg {

template <class T> static void drawBitmapRender(agg::renderer_base<agg::pixfmt_psl> &RenderBase,
   agg::rasterizer_scanline_aa<> &Raster, T &spangen, DOUBLE Opacity)
{
   class spanconv_image {
      public:
      spanconv_image(DOUBLE Alpha) : alpha(Alpha) { }
      void prepare() { }
      void generate(agg::rgba8 *span, int x, int y, unsigned len) const {
         do {
            span->a = span->a * alpha;
            ++span;
         } while(--len);
      }
      private:
      DOUBLE alpha;
   };

   agg::span_allocator<agg::rgba8> spanalloc;
   agg::scanline_u8 scanline;
   if (Opacity < 1.0) {
      spanconv_image sci(Opacity);
      agg::span_converter<T, spanconv_image> sc(spangen, sci);
      agg::render_scanlines_aa(Raster, scanline, RenderBase, spanalloc, sc);
   }
   else agg::render_scanlines_aa(Raster, scanline, RenderBase, spanalloc, spangen);
};

//****************************************************************************
// This class is used for clipped images (no tiling)

template<class Source> class span_pattern_rkl // Based on span_pattern_rgba
{
private:
   span_pattern_rkl() {}
public:
   typedef typename agg::rgba8::value_type value_type;
   typedef agg::rgba8 color_type;

   span_pattern_rkl(Source & src, unsigned offset_x, unsigned offset_y) :
       m_src(&src),
       m_offset_x(offset_x),
       m_offset_y(offset_y)
   {
      m_bk_buf[0] = m_bk_buf[1] = m_bk_buf[2] = m_bk_buf[3] = 0;
   }

   void prepare() {}

   void generate(agg::rgba8 *s, int x, int y, unsigned len)
   {
      x += m_offset_x;
      y += m_offset_y;
      const value_type* p = (const value_type*)span(x, y, len);
      do {
         s->r = p[m_src->oR];
         s->g = p[m_src->oG];
         s->b = p[m_src->oB];
         s->a = p[m_src->oA];
         p = (const value_type*)next_x();
         ++s;
      } while(--len);
   }

    int8u* span(int x, int y, unsigned len)
   {
      m_x = m_x0 = x;
      m_y = y;
      if ((y >= 0) and (y < (int)m_src->mBitmap->Clip.Bottom) and (x >= 0) and (x+(int)len <= (int)m_src->mBitmap->Clip.Right)) {
         return m_pix_ptr = m_src->row_ptr(y) + (x * m_src->mBitmap->BytesPerPixel);
      }
      m_pix_ptr = 0;
      if ((m_y >= 0) and (m_y < (int)m_src->mBitmap->Clip.Bottom) and (m_x >= 0) and (m_x < (int)m_src->mBitmap->Clip.Right)) {
         return m_src->row_ptr(m_y) + (m_x * m_src->mBitmap->BytesPerPixel);
      }
      return m_bk_buf;
   }

   int8u* next_x()
   {
      if (m_pix_ptr) return m_pix_ptr += m_src->mBitmap->BytesPerPixel;
      ++m_x;
      if ((m_y >= 0) and (m_y < (int)m_src->mBitmap->Clip.Bottom) and (m_x >= 0) and (m_x < (int)m_src->mBitmap->Clip.Right)) {
         return m_src->row_ptr(m_y) + (m_x * m_src->mBitmap->BytesPerPixel);
      }
      return m_bk_buf;
  }

   int8u* next_y()
   {
      ++m_y;
      m_x = m_x0;
      if (m_pix_ptr and m_y >= 0 and m_y < (int)m_src->height()) {
         return m_pix_ptr = m_src->row_ptr(m_y) + (m_x * m_src->mBitmap->BytesPerPixel);
      }
      m_pix_ptr = 0;
      if ((m_y >= 0) and (m_y < (int)m_src->mBitmap->Clip.Bottom) and (m_x >= 0) and (m_x < (int)m_src->mBitmap->Clip.Right)) {
         return m_src->row_ptr(m_y) + (m_x * m_src->mBitmap->BytesPerPixel);
      }
      return m_bk_buf;
   }
   Source *m_src;

private:
   unsigned m_offset_x;
   unsigned m_offset_y;
   UBYTE m_bk_buf[4];
   int m_x, m_x0, m_y;
   UBYTE *m_pix_ptr;
};

} // namespace

class SimpleVector {
public:
   agg::path_storage mPath;
   agg::renderer_base<agg::pixfmt_psl> mRenderer;
   agg::rasterizer_scanline_aa<> mRaster; // For rendering the scene.  Stores a copy of the path, and other values.

   SimpleVector() { }

   // Refer to scene_draw.cpp for DrawPath()
   void DrawPath(objBitmap *, DOUBLE StrokeWidth, OBJECTPTR StrokeStyle, OBJECTPTR FillStyle);
};

//********************************************************************************************************************
// Retrieve the width/height of a vector's nearest viewport or scene object, taking account of relative dimensions
// and offsets.
//
// These functions expect to be called during path generation via gen_vector_path().  If this is not the case, ensure
// that Dirty field markers are cleared beforehand.

template <class T> inline static DOUBLE get_parent_width(const T *Vector)
{
   if (Vector->ParentView) {
      if ((Vector->ParentView->vpDimensions & DMF_WIDTH) or
          ((Vector->ParentView->vpDimensions & DMF_X) and (Vector->ParentView->vpDimensions & DMF_X_OFFSET))) {
         return Vector->ParentView->vpFixedWidth;
      }
      else if (Vector->ParentView->vpViewWidth > 0) return Vector->ParentView->vpViewWidth;
      else return Vector->Scene->PageWidth;
   }
   else if (Vector->Scene) return Vector->Scene->PageWidth;
   else return 0;
}

template <class T> inline static DOUBLE get_parent_height(const T *Vector)
{
   if (Vector->ParentView) {
      if ((Vector->ParentView->vpDimensions & DMF_HEIGHT) or
          ((Vector->ParentView->vpDimensions & DMF_Y) and (Vector->ParentView->vpDimensions & DMF_Y_OFFSET))) {
         return Vector->ParentView->vpFixedHeight;
      }
      else if (Vector->ParentView->vpViewHeight > 0) return Vector->ParentView->vpViewHeight;
      else return Vector->Scene->PageHeight;
   }
   else if (Vector->Scene) return Vector->Scene->PageHeight;
   else return 0;
}

template <class T> inline static void get_parent_size(T *Vector, DOUBLE &Width, DOUBLE &Height)
{
   Width = get_parent_width(Vector);
   Height = get_parent_height(Vector);
}

template <class T> inline static DOUBLE get_parent_diagonal(T *Vector)
{
   DOUBLE width = get_parent_width(Vector);
   DOUBLE height = get_parent_height(Vector);

   if (width > height) std::swap(width, height);
   if ((height / width) <= 1.5) return 5.0 * (width + height) / 7.0; // Fast hypot calculation accurate to within 1% for specific use cases.
   else return std::sqrt((width * width) + (height * height));
}

inline static DOUBLE dist(DOUBLE X1, DOUBLE Y1, DOUBLE X2, DOUBLE Y2)
{
   DOUBLE width = X2 - X1;
   DOUBLE height = Y2 - Y1;
   if (width > height) std::swap(width, height);
   if ((height / width) <= 1.5) return 5.0 * (width + height) / 7.0; // Fast hypot calculation accurate to within 1% for specific use cases.
   else return std::sqrt((width * width) + (height * height));
}

//********************************************************************************************************************

inline static void save_bitmap(objBitmap *Bitmap, std::string Name)
{
   objPicture *pic;
   std::string path = "temp:filter_output_" + Name + ".png";

   if (!CreateObject(ID_PICTURE, 0, &pic,
         FID_Width|TLONG,        Bitmap->Clip.Right - Bitmap->Clip.Left,
         FID_Height|TLONG,       Bitmap->Clip.Bottom - Bitmap->Clip.Top,
         FID_BitsPerPixel|TLONG, 32,
         FID_Flags|TLONG,        PCF_FORCE_ALPHA_32|PCF_NEW,
         FID_Path|TSTR,          path.c_str(),
         FID_ColourSpace|TLONG,  Bitmap->ColourSpace,
         TAGEND)) {

      gfxCopyArea(Bitmap, pic->Bitmap, 0, Bitmap->Clip.Left, Bitmap->Clip.Top, pic->Bitmap->Width, pic->Bitmap->Height, 0, 0);
      acSaveImage(pic, 0, 0);
      acFree(pic);
   }
}

//********************************************************************************************************************
// Find the first parent of the targeted vector.  Returns NULL if no valid parent is found.

inline static objVector * get_parent(const objVector *Vector)
{
   if (Vector->ClassID != ID_VECTOR) return NULL;
   while (Vector) {
      if (!Vector->Parent) Vector = Vector->Prev; // Scan back to the first sibling to find the parent
      else if (Vector->Parent->ClassID IS ID_VECTOR) return (objVector *)(Vector->Parent);
      else return NULL;
   }

   return NULL;
}

//********************************************************************************************************************
// Test if a point is within a rectangle (four points, must be convex)

static DOUBLE is_left(agg::vertex_d A, agg::vertex_d B, agg::vertex_d C)
{
    return ((B.x - A.x) * (C.y - A.y) - (C.x - A.x) * (B.y - A.y));
}

static bool point_in_rectangle(agg::vertex_d X, agg::vertex_d Y, agg::vertex_d Z, agg::vertex_d W, agg::vertex_d P) __attribute__ ((unused));

static bool point_in_rectangle(agg::vertex_d X, agg::vertex_d Y, agg::vertex_d Z, agg::vertex_d W, agg::vertex_d P)
{
    return (is_left(X, Y, P) > 0) and (is_left(Y, Z, P) > 0) and (is_left(Z, W, P) > 0) and (is_left(W, X, P) > 0);
}

//****************************************************************************

inline double fastPow(double a, double b) {
   union {
     double d;
     int x[2];
   } u = { a };
   u.x[1] = (int)(b * (u.x[1] - 1072632447) + 1072632447);
   u.x[0] = 0;
   return u.d;
}

//****************************************************************************

inline int isPow2(ULONG x)
{
   return ((x != 0) and !(x & (x - 1)));
}

//********************************************************************************************************************

template <class T>
void configure_stroke(objVector &Vector, T &Stroke)
{
   Stroke.width(Vector.fixed_stroke_width());

   if (Vector.LineJoin)  Stroke.line_join(Vector.LineJoin); //miter, round, bevel
   if (Vector.LineCap)   Stroke.line_cap(Vector.LineCap); // butt, square, round
   if (Vector.InnerJoin) Stroke.inner_join(Vector.InnerJoin); // miter, round, bevel, jag

   // TODO: AGG seems to have issues with using the correct cap at the end of closed polygons.  For the moment
   // this hack is being used, but it can result in dashed lines being switched to the wrong line cap.  For illustration, use:
   //
   //   <polygon points="100,50 140,50 120,15.36" stroke="darkslategray" stroke-width="5" stroke-dasharray="20 20"
   //     stroke-dashoffset="10" fill="lightslategray" stroke-linejoin="round" />

   if (Vector.LineJoin) {
      if (Vector.SubID IS ID_VECTORPOLYGON) {
         if (((objVectorPoly &)Vector).Closed) {
            switch(Vector.LineJoin) {
               case VLJ_MITER:        Stroke.line_cap(agg::square_cap); break;
               case VLJ_BEVEL:        Stroke.line_cap(agg::square_cap); break;
               case VLJ_MITER_REVERT: Stroke.line_cap(agg::square_cap); break;
               case VLJ_ROUND:        Stroke.line_cap(agg::round_cap); break;
               case VLJ_MITER_ROUND:  Stroke.line_cap(agg::round_cap); break;
               case VLJ_INHERIT: break;
            }
         }
      }
   }

   if (Vector.MiterLimit > 0) Stroke.miter_limit(Vector.MiterLimit);
   if (Vector.InnerMiterLimit > 0) Stroke.inner_miter_limit(Vector.InnerMiterLimit);
}

extern agg::gamma_lut<UBYTE, UWORD, 8, 12> glGamma;

extern void get_text_xy(objVectorText *);
extern void  vecArcTo(class SimpleVector *, DOUBLE RX, DOUBLE RY, DOUBLE Angle, DOUBLE X, DOUBLE Y, LONG Flags);
extern ERROR vecApplyPath(class SimpleVector *, objVectorPath *);
extern void  vecClosePath(class SimpleVector *);
extern void  vecCurve3(class SimpleVector *, DOUBLE CtrlX, DOUBLE CtrlY, DOUBLE X, DOUBLE Y);
extern void  vecCurve4(class SimpleVector *, DOUBLE CtrlX1, DOUBLE CtrlY1, DOUBLE CtrlX2, DOUBLE CtrlY2, DOUBLE X, DOUBLE Y);
extern ERROR vecDrawPath(objBitmap *, class SimpleVector *, DOUBLE StrokeWidth, OBJECTPTR StrokeStyle, OBJECTPTR FillStyle);
extern void  vecFreePath(APTR);
extern ERROR vecGenerateEllipse(DOUBLE, DOUBLE, DOUBLE, DOUBLE, LONG, APTR *);
extern ERROR vecGenerateRectangle(DOUBLE, DOUBLE, DOUBLE, DOUBLE, APTR *);
extern ERROR vecGeneratePath(CSTRING, APTR *);
extern LONG  vecGetVertex(class SimpleVector *, DOUBLE *, DOUBLE *);
extern void  vecLineTo(class SimpleVector *, DOUBLE, DOUBLE);
extern void  vecMoveTo(class SimpleVector *, DOUBLE, DOUBLE);
extern ERROR vecMultiply(struct VectorMatrix *, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE);
extern ERROR vecMultiplyMatrix(struct VectorMatrix *, struct VectorMatrix *);
extern ERROR vecParseTransform(struct VectorMatrix *, CSTRING Commands);
extern void  vecReadPainter(OBJECTPTR, CSTRING, struct FRGB *, objVectorGradient **, objVectorImage **, objVectorPattern **);
extern ERROR vecResetMatrix(struct VectorMatrix *);
extern void  vecRewindPath(class SimpleVector *);
extern ERROR vecRotate(struct VectorMatrix *, DOUBLE, DOUBLE, DOUBLE);
extern ERROR vecScale(struct VectorMatrix *, DOUBLE, DOUBLE);
extern ERROR vecSkew(struct VectorMatrix *, DOUBLE, DOUBLE);
extern void  vecSmooth3(class SimpleVector *, DOUBLE, DOUBLE);
extern void  vecSmooth4(class SimpleVector *, DOUBLE, DOUBLE, DOUBLE, DOUBLE);
extern ERROR vecTranslate(struct VectorMatrix *, DOUBLE, DOUBLE);
extern void  vecTranslatePath(class SimpleVector *, DOUBLE, DOUBLE);
