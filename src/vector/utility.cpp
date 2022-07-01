
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

//****************************************************************************

static CSTRING get_effect_name(UBYTE Effect) __attribute__ ((unused));
static CSTRING get_effect_name(UBYTE Effect)
{
   static const CSTRING effects[] = {
      "Null",
      "Blend",
      "ColourMatrix",
      "ComponentTransfer",
      "Composite",
      "ConvolveMatrix",
      "DiffuseLighting",
      "DisplacementMap",
      "Flood",
      "Blur",
      "Image",
      "Merge",
      "Morphology",
      "Offset",
      "SpecularLighting",
      "Tile",
      "Turbulence",
      "DistantLight",
      "PointLight",
      "Spotlight"
   };

   if ((Effect >= 0) and (Effect < ARRAYSIZE(effects))) {
      return effects[Effect];
   }
   else return "Unknown";
}

//****************************************************************************

static const FieldDef clAspectRatio[] = {
   { "XMin",  ARF_X_MIN },
   { "XMid",  ARF_X_MID },
   { "XMax",  ARF_X_MAX },
   { "YMin",  ARF_Y_MIN },
   { "YMid",  ARF_Y_MID },
   { "YMax",  ARF_Y_MAX },
   { "Meet",  ARF_MEET },
   { "Slice", ARF_SLICE },
   { "None",  ARF_NONE },
   { NULL, 0 }
};

static FIELD FID_FreetypeFace;

//********************************************************************************************************************
// Retrieve the width/height of a vector's nearest viewport or scene object, taking account of relative dimensions
// and offsets.
//
// These functions expect to be called during path generation via gen_vector_path().  If this is not the case, ensure
// that Dirty field markers are cleared beforehand.

template <class T> inline static DOUBLE get_parent_width(T *Vector)
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

template <class T> inline static DOUBLE get_parent_height(T *Vector)
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
// Call reset_path() when the shape of the vector requires recalculation.  If the position of the shape has changed,
// you probably want to mark_dirty() with the RC_TRANSFORM option instead.

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
inline static void compile_transforms(const T &Vector, agg::trans_affine &AGGTransform)
{
   for (auto t=Vector.Matrices; t; t=t->Next) {
      AGGTransform.multiply(t->ScaleX, t->ShearY, t->ShearX, t->ScaleY, t->TranslateX, t->TranslateY);
   }
}

//********************************************************************************************************************

static CSTRING get_name(OBJECTPTR) __attribute__ ((unused));
static CSTRING get_name(OBJECTPTR Vector)
{
   if (!Vector) return "NULL";

   switch(Vector->SubID) {
      case ID_VECTORCLIP:      return "Clip";
      case ID_VECTORRECTANGLE: return "Rectangle";
      case ID_VECTORELLIPSE:   return "Ellipse";
      case ID_VECTORPATH:      return "Path";
      case ID_VECTORPOLYGON:   return "Polygon";
      case ID_VECTORTEXT:      return "Text";
      case ID_VECTORFILTER:    return "Filter";
      case ID_VECTORGROUP:     return "Group";
      case ID_VECTORVIEWPORT:  return "Viewport";
      case ID_VECTORWAVE:      return "Wave";
   }

   switch(Vector->ClassID) {
      case ID_VECTORCOLOUR:    return "Colour";
      case ID_VECTORFILTER:    return "Filter";
      case ID_VECTORGRADIENT:  return "Gradient";
      case ID_VECTORPATTERN:   return "Pattern";
      case ID_VECTOR:          return "Vector";
      case ID_VECTORSCENE:     return "Scene";
   }

   return "Unknown";
}

INLINE CSTRING get_name(objVector *Vector) {
   return get_name(&Vector->Head);
}

//********************************************************************************************************************
// Calculate the target X/Y for a vector path based on an aspect ratio and source/target dimensions.
// Source* defines size of the source area and Target* defines the size of the projection to the display.

static void calc_aspectratio(CSTRING Caller, LONG AspectRatio,
   DOUBLE TargetWidth, DOUBLE TargetHeight,
   DOUBLE SourceWidth, DOUBLE SourceHeight,
   DOUBLE *X, DOUBLE *Y, DOUBLE *XScale, DOUBLE *YScale)
{
   parasol::Log log(Caller);

   // Prevent division by zero errors.  Note that the client can legitimately set these values to zero, so we cannot
   // treat such situations as an error on the client's part.

   if (TargetWidth <= 0.000001) TargetWidth = 0.1;
   if (TargetHeight <= 0.000001) TargetHeight = 0.1;

   // A Source size of 0 is acceptable and will be treated as equivalent to the target.

   if (SourceWidth <= 0.000001) SourceWidth = TargetWidth;
   if (SourceHeight <= 0.000001) SourceHeight = TargetHeight;

   if (AspectRatio & (ARF_MEET|ARF_SLICE)) {
      DOUBLE xScale = TargetWidth / SourceWidth;
      DOUBLE yScale = TargetHeight / SourceHeight;

      // MEET: Choose the smaller of the two scaling factors, so that the scaled graphics meet the edge of the
      // viewport and do not exceed it.  SLICE: Choose the larger scale, expanding beyond the boundary on one axis.

      if (AspectRatio & ARF_MEET) {
         if (yScale > xScale) yScale = xScale;
         else if (xScale > yScale) xScale = yScale;
      }
      else if (AspectRatio & ARF_SLICE) {
         // Choose the larger of the two scaling factors.
         if (yScale < xScale) yScale = xScale;
         else if (xScale < yScale) xScale = yScale;
      }

      *XScale = xScale;
      if (AspectRatio & ARF_X_MIN) *X = 0;
      else if (AspectRatio & ARF_X_MID) *X = (TargetWidth - (SourceWidth * xScale)) * 0.5;
      else if (AspectRatio & ARF_X_MAX) *X = TargetWidth - (SourceWidth * xScale);

      *YScale = yScale;
      if (AspectRatio & ARF_Y_MIN) *Y = 0;
      else if (AspectRatio & ARF_Y_MID) *Y = (TargetHeight - (SourceHeight * yScale)) * 0.5;
      else if (AspectRatio & ARF_Y_MAX) *Y = TargetHeight - (SourceHeight * yScale);
   }
   else { // ARF_NONE
      *X = 0;
      if ((TargetWidth >= 1.0) and (SourceWidth >= 1.0)) *XScale = TargetWidth / SourceWidth;
      else *XScale = 1.0;

      *Y = 0;
      if ((TargetHeight >= 1.0) and (SourceHeight >= 1.0)) *YScale = TargetHeight / SourceHeight;
      else *YScale = 1.0;
   }

   log.trace("Aspect: $%.8x, Target: %.0fx%.0f, View: %.0fx%.0f, AlignXY: %.2fx%.2f, Scale: %.2fx%.2f",
      AspectRatio, TargetWidth, TargetHeight, SourceWidth, SourceHeight, *X, *Y, *XScale, *YScale);
}

//********************************************************************************************************************
// Calculate the boundaries for a branch of the tree and return the combined maximum bound values.
// NOTE: This function performs a full traversal (siblings and children) and this may extend beyond the
// viewport's visible boundary.

static void calc_full_boundary(objVector *Vector, std::array<DOUBLE, 4> &Bounds)
{
   if (!Vector) return;

   for (; Vector; Vector=(objVector *)Vector->Next) {
      if (Vector->Dirty) gen_vector_path(Vector);

      if (Vector->Head.SubID != ID_VECTORVIEWPORT) { // Don't consider viewport sizes when determining content dimensions.
         DOUBLE bx1, by1, bx2, by2;

         if (Vector->ClipMask) {
            agg::conv_transform<agg::path_storage, agg::trans_affine> path(*Vector->ClipMask->ClipPath, Vector->Transform);
            bounding_rect_single(path, 0, &bx1, &by1, &bx2, &by2);
         }
         else if (Vector->BasePath.total_vertices()) {
            agg::conv_transform<agg::path_storage, agg::trans_affine> path(Vector->BasePath, Vector->Transform);
            bounding_rect_single(path, 0, &bx1, &by1, &bx2, &by2);
         }

         if (bx1 < Bounds[0]) Bounds[0] = bx1;
         if (by1 < Bounds[1]) Bounds[1] = by1;
         if (bx2 > Bounds[2]) Bounds[2] = bx2;
         if (by2 > Bounds[3]) Bounds[3] = by2;
      }

      if (Vector->Child) calc_full_boundary((objVector *)Vector->Child, Bounds);
   }
}

//********************************************************************************************************************
// For debugging next/prev/child pointers in the scene graph
//
// LONG level = 0;
// debug_branch("Debug", &Self->Head, &level);

static void debug_tree_ptrs(CSTRING Header, OBJECTPTR Vector, LONG *Level) __attribute__ ((unused));

static void debug_tree_ptrs(CSTRING Header, OBJECTPTR Vector, LONG *Level)
{
   parasol::Log log(Header);
   UBYTE spacing[*Level + 1];
   LONG i;

   *Level = *Level + 1;
   for (i=0; i < *Level; i++) spacing[i] = ' '; // Indenting
   spacing[i] = 0;

   while (Vector) {
      if (Vector->ClassID IS ID_VECTORSCENE) {
         log.msg("Scene: %p", Vector);
         if (((objVectorScene *)Vector)->Viewport) debug_tree_ptrs(Header, &(((objVectorScene *)Vector)->Viewport->Head), Level);
         break;
      }
      else if (Vector->ClassID IS ID_VECTOR) {
         objVector *shape = (objVector *)Vector;
         log.msg("%p<-%p->%p Child %p %s%s", shape->Prev, shape, shape->Next, shape->Child, spacing, get_name(shape));
         if (shape->Child) debug_tree_ptrs(Header, &shape->Child->Head, Level);
         Vector = &shape->Next->Head;
      }
      else break;
   }

   *Level = *Level - 1;
}

//********************************************************************************************************************
// Find the first parent of the targeted vector.  Returns NULL if no valid parent is found.

inline static objVector * get_parent(objVector *Vector)
{
   if (Vector->Head.ClassID != ID_VECTOR) return NULL;
   while (Vector) {
      if (!Vector->Parent) Vector = Vector->Prev; // Scan back to the first sibling to find the parent
      else if (Vector->Parent->ClassID IS ID_VECTOR) return (objVector *)(Vector->Parent);
      else return NULL;
   }

   return NULL;
}

//********************************************************************************************************************
// Designed for reading unit values such as '50%' and '6px'.  The returned value is scaled to pixels.

static DOUBLE read_unit(CSTRING Value, UBYTE *Percent)
{
   bool isnumber = true;

   *Percent = 0;

   while ((*Value) and (*Value <= 0x20)) Value++;

   CSTRING str = Value;
   if (*str IS '-') str++;

   if ((((*str >= '0') and (*str <= '9')))) {
      while ((*str >= '0') and (*str <= '9')) str++;

      if (*str IS '.') {
         str++;
         if ((*str >= '0') and (*str <= '9')) {
            while ((*str >= '0') and (*str <= '9')) str++;
         }
         else isnumber = false;
      }

      DOUBLE multiplier = 1.0;
      DOUBLE dpi = 96.0;

      if (*str IS '%') {
         *Percent = 0x01;
         multiplier = 0.01;
         str++;
      }
      else if ((str[0] IS 'p') and (str[1] IS 'x')); // Pixel.  This is the default type
      else if ((str[0] IS 'e') and (str[1] IS 'm')) multiplier = 12.0 * (4.0 / 3.0); // Multiply the current font's pixel height by the provided em value
      else if ((str[0] IS 'e') and (str[1] IS 'x')) multiplier = 6.0 * (4.0 / 3.0); // As for em, but multiple by the pixel height of the 'x' character.  If no x character, revert to 0.5em
      else if ((str[0] IS 'i') and (str[1] IS 'n')) multiplier = dpi; // Inches
      else if ((str[0] IS 'c') and (str[1] IS 'm')) multiplier = (1.0 / 2.56) * dpi; // Centimetres
      else if ((str[0] IS 'm') and (str[1] IS 'm')) multiplier = (1.0 / 20.56) * dpi; // Millimetres
      else if ((str[0] IS 'p') and (str[1] IS 't')) multiplier = (4.0 / 3.0); // Points.  A point is 4/3 of a pixel
      else if ((str[0] IS 'p') and (str[1] IS 'c')) multiplier = (4.0 / 3.0) * 12.0; // Pica.  1 Pica is equal to 12 Points

      return StrToFloat(Value) * multiplier;
   }
   else return 0;
}

//********************************************************************************************************************
// The parser will break once the string value terminates, or an invalid character is encountered.  Parsed characters
// include: 0 - 9 , ( ) - + SPACE

static CSTRING read_numseq(CSTRING Value, ...)
{
   va_list list;
   DOUBLE *result;

   va_start(list, Value);

   while ((result = va_arg(list, DOUBLE *))) {
      while ((*Value) and ((*Value <= 0x20) or (*Value IS ',') or (*Value IS '(') or (*Value IS ')'))) Value++;

      if ((*Value IS '-') and (Value[1] >= '0') and (Value[1] <= '9')) {
         *result = StrToFloat(Value);
         Value++; // Skip '-'
      }
      else if ((*Value IS '+') and (Value[1] >= '0') and (Value[1] <= '9')) {
         *result = StrToFloat(Value);
         Value++; // Skip '+'
      }
      else if ((*Value IS '.') and  (Value[1] >= '0') and (Value[1] <= '9')) {
         *result = StrToFloat(Value);
      }
      else if (((*Value >= '0') and (*Value <= '9'))) {
         *result = StrToFloat(Value);
      }
      else break;

      while ((*Value >= '0') and (*Value <= '9')) Value++;

      if (*Value IS '.') {
         Value++;
         while ((*Value >= '0') and (*Value <= '9')) Value++;
      }
   }

   va_end(list);
   return Value;
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
      if (Vector.Head.SubID IS ID_VECTORPOLYGON) {
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
