
agg::gamma_lut<UBYTE, UWORD, 8, 12> glGamma(2.2);
DOUBLE glDisplayHDPI = 96, glDisplayVDPI = 96, glDisplayDPI = 96;

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
   { "XMin",  ARF::X_MIN },
   { "XMid",  ARF::X_MID },
   { "XMax",  ARF::X_MAX },
   { "YMin",  ARF::Y_MIN },
   { "YMid",  ARF::Y_MID },
   { "YMax",  ARF::Y_MAX },
   { "Meet",  ARF::MEET },
   { "Slice", ARF::SLICE },
   { "None",  ARF::NONE },
   { NULL, 0 }
};

//********************************************************************************************************************

CSTRING get_name(OBJECTPTR Vector)
{
   if (!Vector) return "NULL";

   switch(Vector->Class->ClassID) {
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

   switch(Vector->Class->BaseClassID) {
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

static void update_dpi(void)
{
   static LARGE last_update = -0x7fffffff;
   LARGE current_time = PreciseTime();

   if (current_time - last_update > 3000000LL) {
      DISPLAYINFO *display;
      if (!gfxGetDisplayInfo(0, &display)) {
         last_update = PreciseTime();
         if ((display->VDensity >= 72) and (display->HDensity >= 72)) {
            glDisplayVDPI = display->VDensity;
            glDisplayHDPI = display->HDensity;
            glDisplayDPI = (glDisplayVDPI + glDisplayHDPI) * 0.5;
         }
      }
   }
}

//********************************************************************************************************************
// Read a string-based series of vector commands and add them to Path.

ERROR read_path(std::vector<PathCommand> &Path, CSTRING Value)
{
   pf::Log log(__FUNCTION__);

   PathCommand path;

   LONG max_cmds = 8192; // Maximum commands per path - this acts as a safety net in case the parser gets stuck.
   UBYTE cmd = 0;
   while (*Value) {
      if ((*Value >= 'a') and (*Value <= 'z')) cmd = *Value++;
      else if ((*Value >= 'A') and (*Value <= 'Z')) cmd = *Value++;
      else if (((*Value >= '0') and (*Value <= '9')) or (*Value IS '-') or (*Value IS '+')); // Use the previous command
      else { Value++; continue; }

      switch (cmd) {
         case 'M': case 'm': // MoveTo
            read_numseq_zero(Value, { &path.X, &path.Y });
            if (cmd IS 'M') {
               path.Type = PE::Move;
               cmd = 'L'; // This is because the SVG standard requires that uninterrupted coordinate pairs are interpreted as line-to commands.
            }
            else {
               path.Type = PE::MoveRel;
               cmd = 'l';
            }
            break;

         case 'L': case 'l': // LineTo
            read_numseq_zero(Value, { &path.X, &path.Y });
            if (cmd IS 'L') path.Type = PE::Line;
            else path.Type = PE::LineRel;
            break;

         case 'V': case 'v': // Vertical LineTo
            path.X = 0; // Needs to be zero to satisfy any curve instructions that might follow
            read_numseq_zero(Value, { &path.Y });
            if (cmd IS 'V') path.Type = PE::VLine;
            else path.Type = PE::VLineRel;
            break;

         case 'H': case 'h': // Horizontal LineTo
            path.Y = 0; // Needs to be zero to satisfy any curve instructions that might follow
            read_numseq_zero(Value, { &path.X });
            if (cmd IS 'H') path.Type = PE::HLine;
            else path.Type = PE::LineRel;
            break;

         case 'Q': case 'q': // Quadratic Curve To
            read_numseq_zero(Value, { &path.X2, &path.Y2, &path.X, &path.Y });
            if (cmd IS 'Q') path.Type = PE::QuadCurve;
            else path.Type = PE::QuadCurveRel;
            break;

         case 'T': case 't': // Quadratic Smooth Curve To
            read_numseq_zero(Value, { &path.X2, &path.Y2, &path.X, &path.Y });
            if (cmd IS 'T') path.Type = PE::QuadSmooth;
            else path.Type = PE::QuadSmoothRel;
           break;

         case 'C': case 'c': // Curve To
            read_numseq_zero(Value, { &path.X2, &path.Y2, &path.X3, &path.Y3, &path.X, &path.Y });
            if (cmd IS 'C') path.Type = PE::Curve;
            else path.Type = PE::CurveRel;
            break;

         case 'S': case 's': // Smooth Curve To
            read_numseq_zero(Value, { &path.X2, &path.Y2, &path.X, &path.Y });
            if (cmd IS 'S') path.Type = PE::Smooth;
            else path.Type = PE::SmoothRel;
            break;

         case 'A': case 'a': { // Arc
            DOUBLE largearc, sweep;
            read_numseq_zero(Value, { &path.X2, &path.Y2, &path.Angle, &largearc, &sweep, &path.X, &path.Y });
            path.LargeArc = F2T(largearc);
            path.Sweep = F2T(sweep);
            if (cmd IS 'A') path.Type = PE::Arc;
            else path.Type = PE::ArcRel;
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
            path.Type = PE::ClosePath;
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
// Source* defines size of the source area (in SVG, the 'viewbox')
// Target* defines the size of the projection to the display.

void calc_aspectratio(CSTRING Caller, ARF AspectRatio,
   DOUBLE TargetWidth, DOUBLE TargetHeight,
   DOUBLE SourceWidth, DOUBLE SourceHeight,
   DOUBLE *X, DOUBLE *Y, DOUBLE *XScale, DOUBLE *YScale)
{
   pf::Log log(Caller);

   // Prevent division by zero errors.  Note that the client can legitimately set these values to zero, so we cannot
   // treat such situations as an error on the client's part.

   if (TargetWidth <= DBL_MIN) TargetWidth = 0.1;
   if (TargetHeight <= DBL_MIN) TargetHeight = 0.1;

   // A Source size of 0 is acceptable and will be treated as equivalent to the target.

   if (SourceWidth <= DBL_MIN) SourceWidth = TargetWidth;
   if (SourceHeight <= DBL_MIN) SourceHeight = TargetHeight;

   if ((AspectRatio & (ARF::MEET|ARF::SLICE)) != ARF::NIL) {
      DOUBLE xScale = TargetWidth / SourceWidth;
      DOUBLE yScale = TargetHeight / SourceHeight;

      // MEET: Choose the smaller of the two scaling factors, so that the scaled graphics meet the edge of the
      // viewport and do not exceed it.  SLICE: Choose the larger scale, expanding beyond the boundary on one axis.

      if ((AspectRatio & ARF::MEET) != ARF::NIL) {
         if (yScale > xScale) yScale = xScale;
         else if (xScale > yScale) xScale = yScale;
      }
      else if ((AspectRatio & ARF::SLICE) != ARF::NIL) {
         // Choose the larger of the two scaling factors.
         if (yScale < xScale) yScale = xScale;
         else if (xScale < yScale) xScale = yScale;
      }

      *XScale = xScale;
      if ((AspectRatio & ARF::X_MIN) != ARF::NIL) *X = 0;
      else if ((AspectRatio & ARF::X_MID) != ARF::NIL) *X = (TargetWidth - (SourceWidth * xScale)) * 0.5;
      else if ((AspectRatio & ARF::X_MAX) != ARF::NIL) *X = TargetWidth - (SourceWidth * xScale);

      *YScale = yScale;
      if ((AspectRatio & ARF::Y_MIN) != ARF::NIL) *Y = 0;
      else if ((AspectRatio & ARF::Y_MID) != ARF::NIL) *Y = (TargetHeight - (SourceHeight * yScale)) * 0.5;
      else if ((AspectRatio & ARF::Y_MAX) != ARF::NIL) *Y = TargetHeight - (SourceHeight * yScale);
   }
   else { // ARF::NONE
      *X = 0;
      if ((TargetWidth >= 1.0) and (SourceWidth >= 1.0)) *XScale = TargetWidth / SourceWidth;
      else *XScale = 1.0;

      *Y = 0;
      if ((TargetHeight >= 1.0) and (SourceHeight >= 1.0)) *YScale = TargetHeight / SourceHeight;
      else *YScale = 1.0;
   }

   log.trace("ARF Aspect: $%.8x, Target: %.0fx%.0f, View: %.0fx%.0f, AlignXY: %.2fx%.2f, Scale: %.2fx%.2f",
      LONG(AspectRatio), TargetWidth, TargetHeight, SourceWidth, SourceHeight, *X, *Y, *XScale, *YScale);
}

//********************************************************************************************************************
// Calculate the boundaries for a branch of the tree, including transforms, and return the combined maximum bound
// values.  NOTE: This function performs a full traversal (siblings and children) and this may extend beyond the
// viewport's visible boundary.
//
// See also VECTOR_GetBoundary(), for which this function is intended, and set_clip_region() for filters.

void calc_full_boundary(extVector *Vector, TClipRectangle<DOUBLE> &Bounds, bool IncludeSiblings, bool IncludeTransforms, bool IncludeStrokes)
{
   if (!Vector) return;

   for (; Vector; Vector=(extVector *)Vector->Next) {
      if (Vector->dirty()) gen_vector_path(Vector);

      if (Vector->Class->ClassID != ID_VECTORVIEWPORT) { // Don't consider viewport sizes when determining content dimensions.
         if (Vector->BasePath.total_vertices()) {
            DOUBLE stroke = 0;
            if (IncludeTransforms) {
               if ((IncludeStrokes) and (Vector->Stroked)) {
                  stroke = Vector->fixed_stroke_width() * Vector->Transform.scale() * 0.5;
               }

               if (Vector->Transform.is_complex()) {
                  auto simple_path = Vector->Bounds.as_path();
                  agg::conv_transform<agg::path_storage, agg::trans_affine> path(Vector->BasePath, Vector->Transform);
                  auto pb = get_bounds(path);
                  if (pb.left   - stroke < Bounds.left)   Bounds.left   = pb.left - stroke;
                  if (pb.top    - stroke < Bounds.top)    Bounds.top    = pb.top - stroke;
                  if (pb.right  + stroke > Bounds.right)  Bounds.right  = pb.right + stroke;
                  if (pb.bottom + stroke > Bounds.bottom) Bounds.bottom = pb.bottom + stroke;
               }
               else {
                  if (Vector->Bounds.left   - stroke + Vector->Transform.tx < Bounds.left)   Bounds.left   = Vector->Bounds.left + Vector->Transform.tx - stroke;
                  if (Vector->Bounds.top    - stroke + Vector->Transform.ty < Bounds.top)    Bounds.top    = Vector->Bounds.top + Vector->Transform.ty - stroke;
                  if (Vector->Bounds.right  + stroke + Vector->Transform.tx > Bounds.right)  Bounds.right  = Vector->Bounds.right + Vector->Transform.tx + stroke;
                  if (Vector->Bounds.bottom + stroke + Vector->Transform.ty > Bounds.bottom) Bounds.bottom = Vector->Bounds.bottom + Vector->Transform.ty + stroke;
               }
            }
            else {
               if ((IncludeStrokes) and (Vector->Stroked)) stroke = Vector->fixed_stroke_width() * 0.5;

               if (Vector->Bounds.left   - stroke < Bounds.left)   Bounds.left   = Vector->Bounds.left - stroke;
               if (Vector->Bounds.top    - stroke < Bounds.top)    Bounds.top    = Vector->Bounds.top - stroke;
               if (Vector->Bounds.right  + stroke > Bounds.right)  Bounds.right  = Vector->Bounds.right + stroke;
               if (Vector->Bounds.bottom + stroke > Bounds.bottom) Bounds.bottom = Vector->Bounds.bottom + stroke;
            }
         }
      }

      if (Vector->Child) calc_full_boundary((extVector *)Vector->Child, Bounds, true, IncludeTransforms, IncludeStrokes);

      if (!IncludeSiblings) break;
   }
}

//********************************************************************************************************************
// Designed for reading unit values such as '50%' and '6px'.  The returned value is scaled to pixels.

DOUBLE read_unit(CSTRING &Value, bool &Percent)
{
   Percent = false;

   while ((*Value) and (*Value <= 0x20)) Value++;

   CSTRING str = Value;
   if ((*str IS '-') or (*str IS '+')) str++;

   if ((*str >= '0') and (*str <= '9')) {
      while ((*str >= '0') and (*str <= '9')) str++;

      if (*str IS '.') {
         str++;
         if ((*str >= '0') and (*str <= '9')) {
            while ((*str >= '0') and (*str <= '9')) str++;
         }
      }

      DOUBLE multiplier = 1.0;
      DOUBLE dpi = DISPLAY_DPI;

      if (*str IS '%') {
         Percent = true;
         multiplier = 0.01;
         str++;
      }
      else if ((str[0] IS 'p') and (str[1] IS 'x')) str += 2; // Pixel.  This is the default type
      else if ((str[0] IS 'e') and (str[1] IS 'm')) { str += 2; multiplier = (12.0 / 72.0) * dpi; } // Multiply the current font's pixel height by the provided em value
      else if ((str[0] IS 'e') and (str[1] IS 'x')) { str += 2; multiplier = (6.0 / 72.0) * dpi; } // As for em, but multiple by the pixel height of the 'x' character.  If no x character, revert to 0.5em
      else if ((str[0] IS 'i') and (str[1] IS 'n')) { str += 2; multiplier = dpi; } // Inches
      else if ((str[0] IS 'c') and (str[1] IS 'm')) { str += 2; multiplier = (1.0 / 2.56) * dpi; } // Centimetres
      else if ((str[0] IS 'm') and (str[1] IS 'm')) { str += 2; multiplier = (1.0 / 20.56) * dpi; } // Millimetres
      else if ((str[0] IS 'p') and (str[1] IS 't')) { str += 2; multiplier = (1.0 / 72.0) * dpi; } // Points.  A point is 1/72 of an inch
      else if ((str[0] IS 'p') and (str[1] IS 'c')) { str += 2; multiplier = (12.0 / 72.0) * dpi; } // Pica.  1 Pica is equal to 12 Points

      auto result = StrToFloat(Value) * multiplier;

      Value = str;
      return result;
   }
   else return 0;
}

//********************************************************************************************************************

std::string weight_to_style(CSTRING Style, LONG Weight)
{
   std::string weight_name;

   if (Weight >= 700) weight_name = "Extra Bold";
   else if (Weight >= 500) weight_name = "Bold";
   else if (Weight <= 200) weight_name = "Extra Light";
   else if (Weight <= 300) weight_name = "Light";

   if (!StrMatch("Italic", Style)) {
      if (weight_name.empty()) return "Italic";
      else return weight_name + " Italic";
   }
   else if (!weight_name.empty()) return weight_name;
   else return "Regular";
}

//********************************************************************************************************************

ERROR get_font(pf::Log &Log, CSTRING Family, CSTRING Style, LONG Weight, LONG Size, common_font **Handle)
{
   Log.branch("Family: %s, Style: %s, Weight: %d, Size: %d", Family, Style, Weight, Size);

   if (!Style) return Log.warning(ERR_NullArgs);

   const std::lock_guard lock{glFontMutex};

   std::string family(Family ? Family : "*");
   if (!family.ends_with("*")) family.append(",*");
   CSTRING final_name;
   if (!fntResolveFamilyName(family.c_str(), &final_name)) family.assign(final_name);

   std::string style(Style);
   if ((Weight) and (Weight != 400)) {
      // If a weight value is specified and is anything other than "Normal"/400 then it will
      // override the named Style completely.
      style = weight_to_style(Style, Weight);
   }

   const LONG point_size = std::round(Size * (72.0 / DISPLAY_DPI));
   CSTRING location = NULL;
   FMETA meta = FMETA::NIL;
   if (auto error = fntSelectFont(family.c_str(), style.c_str(), &location, &meta); !error) {
      GuardedResource loc(location);

      if ((meta & FMETA::SCALED) IS FMETA::NIL) { // Bitmap font
         auto key = StrHash(style + ":" + std::to_string(point_size) + ":" + location);

         if (glBitmapFonts.contains(key)) {
            *Handle = &glBitmapFonts[key];
            return ERR_Okay;
         }
         else if (auto font = objFont::create::global(fl::Name("vector_cached_font"),
               fl::Owner(glVectorModule->UID),
               fl::Face(family),
               fl::Style(style),
               fl::Point(point_size),
               fl::Path(location))) {
            glBitmapFonts.emplace(key, font);
            *Handle = &glBitmapFonts[key];
            return ERR_Okay;
         }
         else return ERR_CreateObject;
      }
      else {
         // For scalable fonts the key is made from the location only, ensuring that the face file is loaded
         // only once.  If the file is variable, it will contain multiple styles.  Otherwise, assume the file
         // represents one type of style.

         auto key = StrHash(location);

         if (!glFreetypeFonts.contains(key)) {
            STRING resolved;
            if (!ResolvePath(location, RSF::NIL, &resolved)) {
               FT_Face ftface;
               FT_Open_Args openargs = { .flags = FT_OPEN_PATHNAME, .pathname = resolved };
               if (FT_Open_Face(glFTLibrary, &openargs, 0, &ftface)) {
                  Log.warning("Fatal error in attempting to load font \"%s\".", resolved);
                  FreeResource(resolved);
                  return ERR_Failed;
               }

               freetype_font::METRIC_TABLE metrics;
               freetype_font::STYLE_CACHE styles;

               if (FT_HAS_MULTIPLE_MASTERS(ftface)) {
                  FT_MM_Var *mvar;
                  if (!FT_Get_MM_Var(ftface, &mvar)) {
                     FT_UInt index;
                     if (!FT_Get_Default_Named_Instance(ftface, &index)) {
                        auto name_table_size = FT_Get_Sfnt_Name_Count(ftface);
                        for (FT_UInt s=0; s < mvar->num_namedstyles; s++) {
                           for (LONG n=name_table_size-1; n >= 0; n--) {
                              FT_SfntName sft_name;
                              if (!FT_Get_Sfnt_Name(ftface, n, &sft_name)) {
                                 if (sft_name.name_id IS mvar->namedstyle[s].strid) {
                                    // Decode UTF16 Big Endian
                                    char buffer[100];
                                    LONG out = 0;
                                    auto str = (UWORD *)sft_name.string;
                                    UWORD prev_unicode = 0;
                                    for (FT_UInt i=0; (i < sft_name.string_len>>1) and (out < std::ssize(buffer)-8); i++) {
                                       UWORD unicode = (str[i]>>8) | (UBYTE(str[i])<<8);
                                       if ((unicode >= 'A') and (unicode <= 'Z')) {
                                          if ((i > 0) and (prev_unicode >= 'a') and (prev_unicode <= 'z')) {
                                             buffer[out++] = ' ';
                                          }
                                       }
                                       out += UTF8WriteValue(unicode, buffer+out, std::ssize(buffer)-out);
                                       prev_unicode = unicode;
                                    }
                                    buffer[out] = 0;
                                    freetype_font::METRIC_GROUP set;
                                    for (unsigned m=0; m < mvar->num_axis; m++) {
                                       set.push_back(mvar->namedstyle[s].coords[m]);
                                    }
                                    metrics.try_emplace(buffer, set);
                                    styles.try_emplace(buffer);
                                    break;
                                 }
                              }
                           }
                        }
                     }
                     FT_Done_MM_Var(glFTLibrary, mvar);
                  }
               }
               else styles.try_emplace(style);

               glFreetypeFonts.try_emplace(key, ftface, styles, metrics, meta);
               FreeResource(resolved);
            }
            else return Log.warning(ERR_ResolvePath);
         }

         auto &font = glFreetypeFonts[key];
         if (auto cache = font.style_cache.find(style); cache != font.style_cache.end()) {
            freetype_font::SIZE_CACHE &sz = cache->second;
            if (auto size = sz.find(Size); size IS sz.end()) {
               // New font size entry required

               if (font.metrics.contains(style)) {
                  auto new_size = sz.try_emplace(Size, font, font.metrics[style], Size);
                  if (!new_size.first->second.ft_size) return ERR_Failed; // Verify success
                  *Handle = &new_size.first->second;
                  return ERR_Okay;
               }
               else {
                  if (!font.metrics.empty()) Log.warning("Font metrics do not support style '%s'", style.c_str());
                  auto new_size = sz.try_emplace(Size, font, Size);
                  if (!new_size.first->second.ft_size) return ERR_Failed; // Verify success
                  *Handle = &new_size.first->second;
                  return ERR_Okay;
               }
            }
            else {
               *Handle = &size->second;
               return ERR_Okay;
            }
         }
         else return ERR_Search;
      }
   }
   else return error;
}

//********************************************************************************************************************
// The parser will break once the string value terminates, or an invalid character is encountered.
//
// There are two variants - the first aborts if an unparseable value is encountered.  The second will set all
// unparseable result values to zero.
//
// Parsed characters include: 0 - 9 , ( ) - + SPACE

void read_numseq(CSTRING &String, std::initializer_list<DOUBLE *> Value)
{
   for (DOUBLE *v : Value) {
      STRING next = NULL;
      next_value(String);
      DOUBLE num = strtod(String, &next);
      if ((!num) and ((!next) or (String IS next))) {  // Invalid character or end-of-stream check.
         String = next;
         return;
      }
      String = next;
      *v = num;
   }
}

void read_numseq_zero(CSTRING &String, std::initializer_list<DOUBLE *> Value)
{
   for (DOUBLE *v : Value) {
      STRING next = (STRING)String;
      next_value(String);
      *v = strtod(String, &next);
      String = next;
   }
}
