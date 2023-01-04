
agg::gamma_lut<UBYTE, UWORD, 8, 12> glGamma(2.2);
rgb_to_linear glLinearRGB;

//********************************************************************************************************************

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

//********************************************************************************************************************

const FieldDef clAspectRatio[] = {
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

//********************************************************************************************************************

CSTRING get_name(OBJECTPTR Vector)
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

//********************************************************************************************************************
// Read a string-based series of vector commands and add them to Path.

ERROR read_path(std::vector<PathCommand> &Path, CSTRING Value)
{
   parasol::Log log(__FUNCTION__);

   PathCommand path;

   LONG max_cmds = 8192; // Maximum commands per path - this acts as a safety net in case the parser gets stuck.
   UBYTE cmd = 0;
   while (*Value) {
      if ((*Value >= 'a') and (*Value <= 'z')) cmd = *Value++;
      else if ((*Value >= 'A') and (*Value <= 'Z')) cmd = *Value++;
      else if (((*Value >= '0') and (*Value <= '9')) or (*Value IS '-') or (*Value IS '+')); // Use the previous command
      else { Value++; continue; }

      ClearMemory(&path, sizeof(path));

      switch (cmd) {
         case 'M': case 'm': // MoveTo
            Value = read_numseq(Value, &path.X, &path.Y, TAGEND);
            if (cmd IS 'M') {
               path.Type = PE_Move;
               cmd = 'L'; // This is because the SVG standard requires that sequential coordinate pairs will be interpreted as line-to commands.
            }
            else {
               path.Type = PE_MoveRel;
               cmd = 'l';
            }
            path.Curved = FALSE;
            break;

         case 'L': case 'l': // LineTo
            Value = read_numseq(Value, &path.X, &path.Y, TAGEND);
            if (cmd IS 'L') path.Type = PE_Line;
            else path.Type = PE_LineRel;
            path.Curved = FALSE;
            break;

         case 'V': case 'v': // Vertical LineTo
            Value = read_numseq(Value, &path.Y, TAGEND);
            if (cmd IS 'V') path.Type = PE_VLine;
            else path.Type = PE_VLineRel;
            path.Curved = FALSE;
            break;

         case 'H': case 'h': // Horizontal LineTo
            Value = read_numseq(Value, &path.X, TAGEND);
            if (cmd IS 'H') path.Type = PE_HLine;
            else path.Type = PE_LineRel;
            path.Curved = FALSE;
            break;

         case 'Q': case 'q': // Quadratic Curve To
            Value = read_numseq(Value, &path.X2, &path.Y2, &path.X, &path.Y, TAGEND);
            if (cmd IS 'Q') path.Type = PE_QuadCurve;
            else path.Type = PE_QuadCurveRel;
            path.Curved = TRUE;
            break;

         case 'T': case 't': // Quadratic Smooth Curve To
            Value = read_numseq(Value, &path.X2, &path.Y2, &path.X, &path.Y, TAGEND);
            if (cmd IS 'T') path.Type = PE_QuadSmooth;
            else path.Type = PE_QuadSmoothRel;
            path.Curved = TRUE;
           break;

         case 'C': case 'c': // Curve To
            Value = read_numseq(Value, &path.X2, &path.Y2, &path.X3, &path.Y3, &path.X, &path.Y, TAGEND);
            if (cmd IS 'C') path.Type = PE_Curve;
            else path.Type = PE_CurveRel;
            path.Curved = TRUE;
            break;

         case 'S': case 's': // Smooth Curve To
            Value = read_numseq(Value, &path.X2, &path.Y2, &path.X, &path.Y, TAGEND);
            if (cmd IS 'S') path.Type = PE_Smooth;
            else path.Type = PE_SmoothRel;
            path.Curved = TRUE;
            break;

         case 'A': case 'a': { // Arc
            DOUBLE largearc, sweep;
            Value = read_numseq(Value, &path.X2, &path.Y2, &path.Angle, &largearc, &sweep, &path.X, &path.Y, TAGEND);
            path.LargeArc = F2T(largearc);
            path.Sweep = F2T(sweep);
            if (cmd IS 'A') path.Type = PE_Arc;
            else path.Type = PE_ArcRel;
            path.Curved = TRUE;
            break;
         }

         // W3C: When a subpath ends in a "closepath," it differs in behaviour from what happens when "manually" closing
         // a subpath via a "lineto" command in how ‘stroke-linejoin’ and ‘stroke-linecap’ are implemented. With
         // "closepath", the end of the final segment of the subpath is "joined" with the start of the initial segment
         // of the subpath using the current value of ‘stroke-linejoin’. If you instead "manually" close the subpath
         // via a "lineto" command, the start of the first segment and the end of the last segment are not joined but
         // instead are each capped using the current value of ‘stroke-linecap’. At the end of the command, the new
         // current point is set to the initial point of the current subpath.

         case 'Z': case 'z': { // Close Path
            path.Type = PE_ClosePath;
            path.Curved = FALSE;
            break;
         }

         default: {
            log.warning("Invalid path command '%c'", *Value);
            return ERR_Failed;
         }
      }

      if (--max_cmds == 0) {
         Path.clear();
         return log.warning(ERR_BufferOverflow);
      }

      Path.push_back(path);
   }

   return (Path.size() >= 2) ? ERR_Okay : ERR_Failed;
}

//********************************************************************************************************************
// Calculate the target X/Y for a vector path based on an aspect ratio and source/target dimensions.
// Source* defines size of the source area and Target* defines the size of the projection to the display.

void calc_aspectratio(CSTRING Caller, LONG AspectRatio,
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

   log.trace("ARF Aspect: $%.8x, Target: %.0fx%.0f, View: %.0fx%.0f, AlignXY: %.2fx%.2f, Scale: %.2fx%.2f",
      AspectRatio, TargetWidth, TargetHeight, SourceWidth, SourceHeight, *X, *Y, *XScale, *YScale);
}

//********************************************************************************************************************
// Calculate the boundaries for a branch of the tree, including transforms, and return the combined maximum bound
// values.  NOTE: This function performs a full traversal (siblings and children) and this may extend beyond the
// viewport's visible boundary.

void calc_full_boundary(extVector *Vector, std::array<DOUBLE, 4> &Bounds, bool IncludeSiblings, bool IncludeTransforms)
{
   if (!Vector) return;

   for (; Vector; Vector=(extVector *)Vector->Next) {
      if (Vector->Dirty) gen_vector_path(Vector);

      if (Vector->SubID != ID_VECTORVIEWPORT) { // Don't consider viewport sizes when determining content dimensions.
         DOUBLE bx1, by1, bx2, by2;

         if ((Vector->ClipMask) and (Vector->ClipMask->ClipPath)) {
            if (IncludeTransforms) {
               agg::conv_transform<agg::path_storage, agg::trans_affine> path(*Vector->ClipMask->ClipPath, Vector->Transform);
               bounding_rect_single(path, 0, &bx1, &by1, &bx2, &by2);
            }
            else bounding_rect_single(*Vector->ClipMask->ClipPath, 0, &bx1, &by1, &bx2, &by2);

            if (bx1 < Bounds[0]) Bounds[0] = bx1;
            if (by1 < Bounds[1]) Bounds[1] = by1;
            if (bx2 > Bounds[2]) Bounds[2] = bx2;
            if (by2 > Bounds[3]) Bounds[3] = by2;
         }
         else if (Vector->BasePath.total_vertices()) {
            if (IncludeTransforms) {
               if (Vector->Transform.is_complex()) {
                  auto simple_path = basic_path(Vector->BX1, Vector->BY1, Vector->BX2, Vector->BY2);
                  agg::conv_transform<agg::path_storage, agg::trans_affine> path(Vector->BasePath, Vector->Transform);
                  bounding_rect_single(path, 0, &bx1, &by1, &bx2, &by2);

                  if (bx1 < Bounds[0]) Bounds[0] = bx1;
                  if (by1 < Bounds[1]) Bounds[1] = by1;
                  if (bx2 > Bounds[2]) Bounds[2] = bx2;
                  if (by2 > Bounds[3]) Bounds[3] = by2;
               }
               else {
                  if (Vector->BX1 + Vector->Transform.tx < Bounds[0]) Bounds[0] = Vector->BX1 + Vector->Transform.tx;
                  if (Vector->BY1 + Vector->Transform.ty < Bounds[1]) Bounds[1] = Vector->BY1 + Vector->Transform.ty;
                  if (Vector->BX2 + Vector->Transform.tx > Bounds[2]) Bounds[2] = Vector->BX2 + Vector->Transform.tx;
                  if (Vector->BY2 + Vector->Transform.ty > Bounds[3]) Bounds[3] = Vector->BY2 + Vector->Transform.ty;
               }
            }
            else {
               if (Vector->BX1 < Bounds[0]) Bounds[0] = Vector->BX1;
               if (Vector->BY1 < Bounds[1]) Bounds[1] = Vector->BY1;
               if (Vector->BX2 > Bounds[2]) Bounds[2] = Vector->BX2;
               if (Vector->BY2 > Bounds[3]) Bounds[3] = Vector->BY2;
            }
         }
      }

      if (Vector->Child) calc_full_boundary((extVector *)Vector->Child, Bounds, true, IncludeTransforms);

      if (!IncludeSiblings) break;
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
         if (((objVectorScene *)Vector)->Viewport) debug_tree_ptrs(Header, (((objVectorScene *)Vector)->Viewport), Level);
         break;
      }
      else if (Vector->ClassID IS ID_VECTOR) {
         auto shape = (objVector *)Vector;
         log.msg("%p<-%p->%p Child %p %s%s", shape->Prev, shape, shape->Next, shape->Child, spacing, get_name(shape));
         if (shape->Child) debug_tree_ptrs(Header, shape->Child, Level);
         Vector = shape->Next;
      }
      else break;
   }

   *Level = *Level - 1;
}

//********************************************************************************************************************
// Designed for reading unit values such as '50%' and '6px'.  The returned value is scaled to pixels.

DOUBLE read_unit(CSTRING Value, bool &Percent)
{
   bool isnumber = true;

   Percent = false;

   while ((*Value) and (*Value <= 0x20)) Value++;

   CSTRING str = Value;
   if ((*str IS '-') or (*str IS '+')) str++;

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
         Percent = true;
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
// The parser will break once the string value terminates, or an invalid character is encountered.  All unparseable
// result values will be set to zero.
//
// Parsed characters include: 0 - 9 , ( ) - + SPACE

CSTRING read_numseq(CSTRING Value, ...)
{
   va_list list;
   DOUBLE *result;

   if ((!Value) or (!Value[0])) return Value;

   va_start(list, Value);

   while ((result = va_arg(list, DOUBLE *))) {
      while ((*Value) and ((*Value <= 0x20) or (*Value IS ',') or (*Value IS '(') or (*Value IS ')'))) Value++;
      if (!Value[0]) break;

      STRING next = NULL;
      DOUBLE num = strtod(Value, &next);
      if ((!num) and ((!next) or (Value IS next))) {  // Invalid character or end-of-stream check.
         Value = next;
         break;
      }

      *result = num;
      Value = next;
   }

   va_end(list);
   return Value;
}
