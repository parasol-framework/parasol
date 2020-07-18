
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

static const struct FieldDef clAspectRatio[] = {
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

//****************************************************************************

static FIELD FID_FreetypeFace;

template <class T> static void mark_dirty(T *Vector, UBYTE Flags)
{
   Vector->Dirty |= Flags;
   for (objVector *scan=(objVector *)Vector->Child; scan; scan=(objVector *)scan->Next) {
      if ((scan->Dirty & Flags) == Flags) continue;
      mark_dirty(scan, Flags);
   }
}

//****************************************************************************
// Call reset_path() when the shape of the vector requires recalculation.  If the position of the shape has changed,
// you probably want to mark_dirty() with the RC_TRANSFORM option instead.

template <class T> static void reset_path(T *Vector)
{
   Vector->Dirty |= RC_BASE_PATH;
   mark_dirty(Vector, RC_FINAL_PATH);
}

//****************************************************************************
// Call reset_final_path() when the base path is still valid and the vector is affected by a transform or coordinate
// translation.

template <class T> static void reset_final_path(T *Vector)
{
   mark_dirty(Vector, RC_FINAL_PATH);
}

//****************************************************************************

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

//****************************************************************************
// Calculate the target X/Y for a vector path based on an aspect ratio and source/target dimensions.

static void calc_alignment(CSTRING Caller, LONG AspectRatio,
   DOUBLE TargetWidth, DOUBLE TargetHeight,
   DOUBLE SourceWidth, DOUBLE SourceHeight,
   DOUBLE *X, DOUBLE *Y, DOUBLE *XScale, DOUBLE *YScale)
{
   if (SourceWidth <= 0.000001) SourceWidth = TargetWidth; // Prevent division by zero errors.
   if (SourceHeight <= 0.000001) SourceHeight = TargetHeight;

   if (AspectRatio & ARF_MEET) {
      DOUBLE xScale = TargetWidth / SourceWidth;
      DOUBLE yScale = TargetHeight / SourceHeight;

      // Choose the smaller of the two scaling factors, so that the scaled graphics meet the edge of the viewport and do not exceed it.
      if (yScale > xScale) yScale = xScale;
      else if (xScale > yScale) xScale = yScale;
      *XScale = xScale;
      *YScale = yScale;

      if (AspectRatio & ARF_X_MIN) *X = 0;
      else if (AspectRatio & ARF_X_MID) *X = (TargetWidth - (SourceWidth * xScale)) * 0.5;
      else if (AspectRatio & ARF_X_MAX) *X = TargetWidth - (SourceWidth * xScale);

      if (AspectRatio & ARF_Y_MIN) *Y = 0;
      else if (AspectRatio & ARF_Y_MID) *Y = (TargetHeight - (SourceHeight * yScale)) * 0.5;
      else if (AspectRatio & ARF_Y_MAX) *Y = TargetHeight - (SourceHeight * yScale);
   }
   else if (AspectRatio & ARF_SLICE) {
      DOUBLE xScale = TargetWidth / SourceWidth;
      DOUBLE yScale = TargetHeight / SourceHeight;

      // Choose the larger of the two scaling factors.
      if (yScale < xScale) yScale = xScale;
      else if (xScale < yScale) xScale = yScale;
      *XScale = xScale;
      *YScale = yScale;

      if (AspectRatio & ARF_X_MIN) *X = 0;
      else if (AspectRatio & ARF_X_MID) *X = (TargetWidth - (SourceWidth * xScale)) * 0.5;
      else if (AspectRatio & ARF_X_MAX) *X = TargetWidth - (SourceWidth * xScale);

      if (AspectRatio & ARF_Y_MIN) *Y = 0;
      else if (AspectRatio & ARF_Y_MID) *Y = (TargetHeight - (SourceHeight * yScale)) * 0.5;
      else if (AspectRatio & ARF_Y_MAX) *Y = TargetHeight - (SourceHeight * yScale);
   }
   else {
      *X = 0;
      *Y = 0;
      if ((TargetWidth >= 1.0) and (SourceWidth >= 1.0)) *XScale = TargetWidth / SourceWidth;
      else *XScale = 1.0;

      if ((TargetHeight >= 1.0) and (SourceHeight >= 1.0)) *YScale = TargetHeight / SourceHeight;
      else *YScale = 1.0;
   }

   FMSG(Caller,"Aspect: $%.8x, Target: %.0fx%.0f, View: %.0fx%.0f, AlignXY: %.2fx%.2f, Scale: %.2fx%.2f",
      AspectRatio, TargetWidth, TargetHeight, SourceWidth, SourceHeight, *X, *Y, *XScale, *YScale);
}

//****************************************************************************
// Calculate the boundaries for a branch of the tree and return the combined maximum bound values.

static void calc_full_boundary(objVector *Vector, std::array<DOUBLE, 4> &Bounds)
{
   for (; Vector; Vector=(objVector *)Vector->Next) {
      if ((!Vector->BasePath) and (Vector->Dirty)) {
         gen_vector_path(Vector);
         Vector->Dirty = 0;
      }

      if (Vector->Head.SubID != ID_VECTORVIEWPORT) { // Don't consider viewport sizes when determining content dimensions.
         if (Vector->BasePath) {
            DOUBLE bx1, by1, bx2, by2;
            agg::conv_transform<agg::path_storage, agg::trans_affine> path(*Vector->BasePath, *Vector->Transform);
            bounding_rect_single(path, 0, &bx1, &by1, &bx2, &by2);

            if (bx1 < Bounds[0]) Bounds[0] = bx1;
            if (by1 < Bounds[1]) Bounds[1] = by1;
            if (bx2 > Bounds[2]) Bounds[2] = bx2;
            if (by2 > Bounds[3]) Bounds[3] = by2;
         }
      }

      if (Vector->Child) calc_full_boundary((objVector *)Vector->Child, Bounds);
   }
}

//****************************************************************************

static void debug_branch(CSTRING Header, OBJECTPTR Vector, LONG *Level) __attribute__ ((unused));

static void debug_branch(CSTRING Header, OBJECTPTR Vector, LONG *Level)
{
   UBYTE spacing[*Level + 1];
   LONG i;

   *Level = *Level + 1;
   for (i=0; i < *Level; i++) spacing[i] = ' '; // Indenting
   spacing[i] = 0;

   while (Vector) {
      if (Vector->ClassID IS ID_VECTORSCENE) {
         LogF(Header, "Scene: %p", Vector);
         if (((objVectorScene *)Vector)->Viewport) debug_branch(Header, &(((objVectorScene *)Vector)->Viewport->Head), Level);
         break;
      }
      else if (Vector->ClassID IS ID_VECTOR) {
         objVector *shape = (objVector *)Vector;
         LogF(Header,"%p<-%p->%p Child %p %s%s", shape->Prev, shape, shape->Next, shape->Child, spacing, get_name(shape));
         if (shape->Child) debug_branch(Header, &shape->Child->Head, Level);
         Vector = &shape->Next->Head;
      }
      else break;
   }

   *Level = *Level - 1;
}

//****************************************************************************
// Find the first parent of the targeted vector.  Returns NULL if no valid parent is found.

INLINE OBJECTPTR get_parent(objVector *Vector)
{
   while (Vector) {
      if (Vector->Head.ClassID != ID_VECTOR) break;
      if (!Vector->Parent) Vector = Vector->Prev;
      else return Vector->Parent;
   }

   return NULL;
}

//****************************************************************************
// Creates a VectorTransform entry and attaches it to the target vector.

template <class T> static struct VectorTransform * add_transform(T *Self, LONG Type)
{
   struct VectorTransform *transform;
   if (!AllocMemory(sizeof(struct VectorTransform), MEM_DATA, &transform, NULL)) {
      transform->Prev = NULL;
      transform->Next = Self->Transforms;
      if (Self->Transforms) Self->Transforms->Prev = transform;
      Self->Transforms = transform;
      transform->Type = Type;
      return transform;
   }
   return NULL;
}

//****************************************************************************
// Designed for reading unit values such as '50%' and '6px'.  The returned value is scaled to pixels.

static DOUBLE read_unit(CSTRING Value, UBYTE *Percent)
{
   BYTE isnumber = TRUE;

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
         else isnumber = FALSE;
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

/*****************************************************************************
** The parser will break once the string value terminates, or an invalid character is encountered.  Parsed characters
** include: 0 - 9 , ( ) - + SPACE
*/

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

//****************************************************************************

template <class T> void configure_stroke(objVector &Vector, T &stroke)
{
   stroke.width(Vector.StrokeWidth);

   if (Vector.LineJoin)  stroke.line_join(Vector.LineJoin); //miter, round, bevel
   if (Vector.LineCap)   stroke.line_cap(Vector.LineCap); // butt, square, round
   if (Vector.InnerJoin) stroke.inner_join(Vector.InnerJoin); // miter, round, bevel, jag

   // AGG seems to have issues with using the correct cap at the end of closed polygons.  For the moment
   // this hack is being used, but it can result in dashed lines being switched to the wrong line cap.  For illustration, use:
   //
   //   <polygon points="100,50 140,50 120,15.36" stroke="darkslategray" stroke-width="5" stroke-dasharray="20 20"
   //     stroke-dashoffset="10" fill="lightslategray" stroke-linejoin="round" />

   if (Vector.LineJoin) {
      if (Vector.Head.SubID IS ID_VECTORPOLYGON) {
         if (((objVectorPoly &)Vector).Closed) {
            switch(Vector.LineJoin) {
               case VLJ_MITER:        stroke.line_cap(agg::square_cap); break;
               case VLJ_BEVEL:        stroke.line_cap(agg::square_cap); break;
               case VLJ_MITER_REVERT: stroke.line_cap(agg::square_cap); break;
               case VLJ_ROUND:        stroke.line_cap(agg::round_cap); break;
               case VLJ_MITER_ROUND:  stroke.line_cap(agg::round_cap); break;
               case VLJ_INHERIT: break;
            }
         }
      }
   }

   if (Vector.MiterLimit > 0) stroke.miter_limit(Vector.MiterLimit);
   if (Vector.InnerMiterLimit > 0) stroke.inner_miter_limit(Vector.InnerMiterLimit);
}
