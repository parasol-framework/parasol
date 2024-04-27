
static HSV rgb_to_hsl(FRGB Colour) __attribute__((unused));
static FRGB hsl_to_rgb(HSV Colour) __attribute__((unused));

#if defined(DEBUG)
static void debug_tree(CSTRING Header, OBJECTPTR) __attribute__ ((unused));
static void debug_branch(CSTRING Header, OBJECTPTR, LONG &Level) __attribute__ ((unused));

static void debug_branch(CSTRING Header, OBJECTPTR Vector, LONG &Level)
{
   pf::Log log(Header);

   auto spacing = std::string(Level + 1, ' ');
   Level = Level + 1;

   while (Vector) {
      if (Vector->Class->ClassID IS ID_VECTORSCENE) {
         log.msg("Scene: %p", Vector);
         if (((objVectorScene *)Vector)->Viewport) debug_branch(Header, ((objVectorScene *)Vector)->Viewport, Level);
      }
      else if (Vector->Class->BaseClassID IS ID_VECTOR) {
         objVector *shape = (objVector *)Vector;
         log.msg("%p<-%p->%p Child %p %s%s", shape->Prev, shape, shape->Next, shape->Child, spacing.c_str(), shape->className());
         if (shape->Child) debug_branch(Header, shape->Child, Level);
         Vector = shape->Next;
      }
      else break;
   }

   Level = Level - 1;
}

static void debug_tree(CSTRING Header, OBJECTPTR Vector)
{
   LONG level = 0;
   while (Vector) {
      debug_branch(Header, Vector, level);
      if (Vector->Class->BaseClassID IS ID_VECTOR) {
         Vector = (((objVector *)Vector)->Next);
      }
      else break;
   }
}

#endif

//********************************************************************************************************************
// HSV values are from 0 - 1.0

static HSV rgb_to_hsl(FRGB Colour)
{
   DOUBLE vmax = std::ranges::max({ Colour.Red, Colour.Green, Colour.Blue });
   DOUBLE vmin = std::ranges::min({ Colour.Red, Colour.Green, Colour.Blue });
   DOUBLE light = (vmax + vmin) * 0.5;

   if (vmax IS vmin) return HSV { 0, 0, light };

   DOUBLE sat = light, hue = light;
   DOUBLE d = vmax - vmin;
   sat = light > 0.5 ? d / (2 - vmax - vmin) : d / (vmax + vmin);
   if (vmax IS Colour.Red)   hue = (Colour.Green - Colour.Blue) / d + (Colour.Green < Colour.Blue ? 6.0 : 0.0);
   if (vmax IS Colour.Green) hue = (Colour.Blue  - Colour.Red) / d + 2.0;
   if (vmax IS Colour.Blue)  hue = (Colour.Red   - Colour.Green) / d + 4.0;
   hue /= 6.0;

   return HSV { hue, sat, light, Colour.Alpha };
}

//********************************************************************************************************************
// HSV values are from 0 - 1.0

static FRGB hsl_to_rgb(HSV Colour)
{
   auto hueToRgb = [](FLOAT p, FLOAT q, FLOAT t) -> FLOAT {
      if (t < 0) t += 1;
      if (t > 1) t -= 1;
      if (t < 1.0/6.0) return p + (q - p) * 6.0 * t;
      if (t < 1.0/2.0) return q;
      if (t < 2.0/3.0) return p + (q - p) * (2.0/3.0 - t) * 6.0;
      return p;
   };

   if (Colour.Saturation == 0) {
      return { FLOAT(Colour.Value), FLOAT(Colour.Value), FLOAT(Colour.Value), FLOAT(Colour.Alpha) };
   }
   else {
      const DOUBLE q = (Colour.Value < 0.5) ? Colour.Value * (1.0 + Colour.Saturation) : Colour.Value + Colour.Saturation - Colour.Value * Colour.Saturation;
      const DOUBLE p = 2.0 * Colour.Value - q;
      return {
         hueToRgb(p, q, Colour.Hue + 1.0/3.0),
         hueToRgb(p, q, Colour.Hue),
         hueToRgb(p, q, Colour.Hue - 1.0/3.0),
         FLOAT(Colour.Alpha)
      };
   }
}

//********************************************************************************************************************
// Support for the 'currentColor' colour value.  Finds the first parent with a defined fill colour and returns it.

static ERR current_colour(extSVG *Self, objVector *Vector, svgState &State, FRGB &RGB)
{
   if (!State.m_color.empty()) {
      VectorPainter painter;
      if (vecReadPainter(NULL, State.m_color.c_str(), &painter, NULL) IS ERR::Okay) {
         RGB = painter.Colour;
         return ERR::Okay;
      }
   }

   if (Vector->Class->BaseClassID != ID_VECTOR) return ERR::Failed;

   Vector = (objVector *)Vector->Parent;
   while (Vector) {
      if (Vector->Class->BaseClassID != ID_VECTOR) return ERR::Failed;

      if (GetFieldArray(Vector, FID_FillColour|TFLOAT, &RGB, NULL) IS ERR::Okay) {
         if (RGB.Alpha != 0) return ERR::Okay;
      }
      Vector = (objVector *)Vector->Parent;
   }

   return ERR::Failed;
}

//********************************************************************************************************************

static void parse_result(extSVG *Self, objFilterEffect *Effect, std::string Value)
{
   if (!Self->Effects.contains(Value)) {
      Self->Effects.emplace(Value, Effect);
   }
}

//********************************************************************************************************************

static void parse_input(extSVG *Self, OBJECTPTR Effect, const std::string Input, FIELD SourceField, FIELD RefField)
{
   switch (StrHash(Input)) {
      case SVF_SOURCEGRAPHIC:   Effect->set(SourceField, LONG(VSF::GRAPHIC)); break;
      case SVF_SOURCEALPHA:     Effect->set(SourceField, LONG(VSF::ALPHA)); break;
      case SVF_BACKGROUNDIMAGE: Effect->set(SourceField, LONG(VSF::BKGD)); break;
      case SVF_BACKGROUNDALPHA: Effect->set(SourceField, LONG(VSF::BKGD_ALPHA)); break;
      case SVF_FILLPAINT:       Effect->set(SourceField, LONG(VSF::FILL)); break;
      case SVF_STROKEPAINT:     Effect->set(SourceField, LONG(VSF::STROKE)); break;
      default:  {
         if (Self->Effects.contains(Input)) {
            Effect->set(RefField, Self->Effects[Input]);
         }
         else {
            pf::Log log;
            log.warning("Unrecognised input '%s'", Input.c_str());
         }
         break;
      }
   }
}

//********************************************************************************************************************
// Note that all offsets are percentages.

static std::vector<Transition> process_transition_stops(extSVG *Self, const objXML::TAGS &Tags)
{
   pf::Log log("process_stops");

   log.traceBranch("");

   std::vector<Transition> stops;
   for (auto &scan : Tags) {
      if (StrMatch("stop", scan.name()) IS ERR::Okay) {
         Transition stop;
         stop.Offset = 0;
         stop.Transform = NULL;
         for (unsigned a=1; a < scan.Attribs.size(); a++) {
            auto &name = scan.Attribs[a].Name;
            auto &value = scan.Attribs[a].Value;
            if (value.empty()) continue;

            if (StrMatch("offset", name) IS ERR::Okay) {
               char *end;
               stop.Offset = strtod(value.c_str(), &end);
               if (*end IS '%') {
                  stop.Offset = stop.Offset * 0.01; // Must be in the range of 0 - 1.0
               }

               if (stop.Offset < 0.0) stop.Offset = 0;
               else if (stop.Offset > 1.0) stop.Offset = 1.0;
            }
            else if (StrMatch("transform", name) IS ERR::Okay) {
               stop.Transform = value.c_str();
            }
            else log.warning("Unable to process stop attribute '%s'", name.c_str());
         }
         stops.emplace_back(stop);
      }
      else log.warning("Unknown element in transition, '%s'", scan.name());
   }

   return stops;
}

//********************************************************************************************************************
// Save an id reference for an SVG element.  The element can be then be found at any time with find_href_tag().  We store
// a copy of the tag data as pointer references are too high a risk.

inline bool add_id(extSVG *Self, const XMLTag &Tag, const std::string_view Name)
{
   if (Self->IDs.contains(std::string(Name))) return false;
   Self->IDs.emplace(Name, Tag);
   return true;
}

inline bool add_id(extSVG *Self, const XMLTag &Tag, const std::string Name)
{
   if (Self->IDs.contains(Name)) return false;
   Self->IDs.emplace(Name, Tag);
   return true;
}

//********************************************************************************************************************

static CSTRING folder(extSVG *Self)
{
   if (Self->Folder) {
      if (Self->Folder[0]) return Self->Folder;
      else return NULL;
   }
   if (!Self->Path) return NULL;

   // Setting a path of "my/house/is/red.svg" results in "my/house/is/"

   STRING folder;
   if (ResolvePath(Self->Path, RSF::NO_FILE_CHECK, &folder) IS ERR::Okay) {
      WORD last = 0;
      for (WORD i=0; folder[i]; i++) {
         if ((folder[i] IS '/') or (folder[i] IS '\\')) last = i + 1;
      }
      folder[last] = 0;
      Self->Folder = folder;
      if (Self->Folder[0]) return Self->Folder;
      else return NULL;
   }
   else return NULL;
}

//********************************************************************************************************************

static void parse_transform(objVector *Vector, const std::string Value, LONG Tag)
{
   if ((Vector->Class->BaseClassID IS ID_VECTOR) and (!Value.empty())) {
      VectorMatrix *matrix;
      if (vecNewMatrix((objVector *)Vector, &matrix, false) IS ERR::Okay) {
         vecParseTransform(matrix, Value.c_str());
         matrix->Tag = Tag;
      }
      else {
         pf::Log log(__FUNCTION__);
         log.warning("Failed to create vector transform matrix.");
      }
   }
}

//********************************************************************************************************************

static const std::string uri_name(const std::string Ref)
{
   LONG skip = 0;
   while ((Ref[skip]) and (Ref[skip] <= 0x20)) skip++;

   if (Ref[skip] IS '#') {
      return Ref.substr(skip+1);
   }
   else if (StrCompare("url(#", Ref.c_str() + skip, 5) IS ERR::Okay) {
      LONG i;
      skip += 5;
      for (i=0; (Ref[skip+i] != ')') and (skip+i < LONG(Ref.size())); i++);
      return Ref.substr(skip, i);
   }
   else return Ref.substr(skip);

   return std::string("");
}

//********************************************************************************************************************

static XMLTag * find_href_tag(extSVG *Self, std::string Ref)
{
   auto ref = uri_name(Ref);
   if ((!ref.empty()) and (Self->IDs.contains(ref))) {
      return &Self->IDs[ref];
   }
   return NULL;
}

/*********************************************************************************************************************
** Converts a SVG time string into seconds.
**
** Full clock example:  50:00:10.25 = 50 hours, 10 seconds and 250 milliseconds
** Partial clock value: 00:10.5 = 10.5 seconds = 10 seconds and 500 milliseconds
** Time count values:
**  3.2h    = 3.2 hours = 3 hours and 12 minutes
**  45min   = 45 minutes
**  30s     = 30 seconds
**  5ms     = 5 milliseconds
**  12.467  = 12
*/

static DOUBLE read_time(const std::string_view Value)
{
   std::array<DOUBLE, 3> units;

   std::size_t i = 0;
   while ((i < Value.size()) and (unsigned(Value[i]) <= 0x20)) i++;

   double num;
   auto [ v, error ] = std::from_chars(Value.data() + i, Value.data() + Value.size(), num);

   if (error IS std::errc()) {
      units[0] = num;

      if (v[0] IS ':') {
         v++;
         units[1] = strtod(v, (char **)&v);

         if (v[0] IS ':') {
            v++;
            units[2] = strtod(v, (char **)&v);

            // hh:nn:ss
            return (units[0] * 60 * 60) + (units[1] * 60) + units[2];
         }
         else { // hh:nn
            return (units[0] * 60 * 60) + (units[1] * 60);
         }
      }
      else if (Value.ends_with("h")) return units[0] * 60 * 60;
      else if (Value.ends_with("s")) return units[0];
      else if (Value.ends_with("min")) return units[0] * 60;
      else if (Value.ends_with("ms")) return DOUBLE(units[0]) / 1000.0;
      else return units[0];
   }
   else return 0;
}

//********************************************************************************************************************
// Designed for reading unit values such as '50%' and '6px'.  The returned value is scaled to pixels.

static DOUBLE read_unit(const std::string_view Value, LARGE *FieldID)
{
   if (FieldID) *FieldID |= TDOUBLE;

   const DOUBLE dpi = 96.0; // TODO: Needs to be derived from the display

   std::size_t i = 0;
   while ((i < Value.size()) and (unsigned(Value[i]) <= 0x20)) i++;

   DOUBLE fv;
   auto [ ptr, error ] = std::from_chars(Value.data() + i, Value.data() + Value.size(), fv);

   if (error IS std::errc()) {
      if (ptr[0] IS '%') {
         if (FieldID) *FieldID |= TSCALE;
         return fv * 0.01;
      }
      else if ((ptr[0] IS 'e') and (ptr[1] IS 'm')) return fv * 12.0 * (4.0 / 3.0); // Multiply the current font's pixel height by the provided em value
      else if ((ptr[0] IS 'e') and (ptr[1] IS 'x')) return fv * 6.0 * (4.0 / 3.0); // As for em, but multiple by the pixel height of the 'x' character.  If no x character, revert to 0.5em
      else if ((ptr[0] IS 'i') and (ptr[1] IS 'n')) return fv * dpi; // Inches
      else if ((ptr[0] IS 'c') and (ptr[1] IS 'm')) return fv * (1.0 / 2.56) * dpi; // Centimetres
      else if ((ptr[0] IS 'm') and (ptr[1] IS 'm')) return fv * (1.0 / 20.56) * dpi; // Millimetres
      else if ((ptr[0] IS 'p') and (ptr[1] IS 't')) return fv * (4.0 / 3.0); // Points.  A point is 4/3 of a pixel
      else if ((ptr[0] IS 'p') and (ptr[1] IS 'c')) return fv * (4.0 / 3.0) * 12.0; // Pica.  1 Pica is equal to 12 Points
      else return fv; // Default to 'px' / pixel
   }
   else return 0;
}

//********************************************************************************************************************
// This function forces all coordinates to be interpreted as relative when in BOUNDINGBOX mode.
//
// NOTE: It would be possible to deprecate this in future if the viewport host is given a viewbox area of (0 0 1 1)
// as it should be.

inline void set_double_units(OBJECTPTR Object, FIELD FieldID, const std::string_view Value, VUNIT Units)
{
   auto field = FieldID;
   DOUBLE num = read_unit(Value, &field);
   if (Units IS VUNIT::BOUNDING_BOX) field |= TSCALE;
   SetField(Object, field, num);
}

//********************************************************************************************************************

inline std::string_view next_value(const std::string_view Value)
{
   std::size_t i = 0;
   while ((i < Value.size()) and ((Value[i] <= 0x20) or (Value[i] IS ',') or (Value[i] IS '(') or (Value[i] IS ')'))) i++;
   return std::string_view(Value.data() + i, Value.size() - i);
}

//********************************************************************************************************************
// The parser will break once the string value terminates, or an invalid character is encountered.  Parsed characters
// include: 0 - 9 , ( ) - + SPACE

template <class T = double> std::string_view read_numseq(std::string_view String, std::initializer_list<T *> Value)
{
   for (T *v : Value) {
      String = next_value(String);

      T num;
      auto [ next, error ] = std::from_chars(String.data(), String.data() + String.size(), num);
      if (error != std::errc()) return String;
      String = std::string_view(next, String.data() + String.size() - next);
      *v = num;
   }

   return String;
}

//********************************************************************************************************************

template<class T = DOUBLE> std::vector<T> read_array(const std::string Value, LONG Limit = 0x7fffffff)
{
   std::vector<T> result;

   CSTRING v = Value.c_str();
   while ((*v) and (LONG(result.size()) < Limit)) {
      while ((*v) and ((*v <= 0x20) or (*v IS ',') or (*v IS '(') or (*v IS ')'))) v++;
      if (!*v) return result;

      STRING next = NULL;
      DOUBLE num = strtod(v, &next);
      if ((!num) and (!next)) return result;
      result.push_back(num);
      v = next;
   }

   return result;
}

//********************************************************************************************************************
// Currently used by gradient functions.

static void add_inherit(extSVG *Self, OBJECTPTR Object, const std::string ID)
{
   pf::Log log(__FUNCTION__);
   log.trace("Object: %d, ID: %s", Object->UID, ID.c_str());

   auto &inherit = Self->Inherit.emplace_back();
   inherit.Object = Object;

   auto hash = ID.find('#');
   if (hash IS std::string::npos) inherit.ID = ID;
   else inherit.ID = ID.substr(hash);
}

//********************************************************************************************************************
// Parse SVG from a file or string buffer.

static ERR parse_svg(extSVG *Self, CSTRING Path, CSTRING Buffer)
{
   pf::Log log(__FUNCTION__);

   if ((!Path) and (!Buffer)) return ERR::NullArgs;

   log.branch("Path: %s [Log-level reduced]", Path ? Path : "<xml-statement>");

#ifndef DEBUG
   AdjustLogLevel(1);
#endif

   if (Self->XML) { FreeResource(Self->XML); Self->XML = NULL; }

   objXML *xml;
   ERR error = ERR::Okay;
   if (NewObject(ID_XML, NF::INTEGRAL, &xml) IS ERR::Okay) {
      objTask *task = CurrentTask();
      STRING working_path = NULL;

      if (Path) {
         if (StrCompare("*.svgz", Path, 0, STR::WILDCARD) IS ERR::Okay) {
            if (auto file = objFile::create::global(fl::Owner(xml->UID), fl::Path(Path), fl::Flags(FL::READ))) {
               if (auto stream = objCompressedStream::create::global(fl::Owner(file->UID), fl::Input(file))) {
                  xml->setSource(stream);
               }
               else {
                  FreeResource(xml);
                  FreeResource(file);
                  error = ERR::CreateObject;
                  goto end;
               }
            }
            else {
               FreeResource(xml);
               error = ERR::CreateObject;
               goto end;
            }
         }
         else xml->setPath(Path);

         if (task->get(FID_Path, &working_path) IS ERR::Okay) working_path = StrClone(working_path);

         // Set a new working path based on the path

         auto last = std::string::npos;
         for (LONG i=0; Path[i]; i++) {
            if ((Path[i] IS '/') or (Path[i] IS '\\') or (Path[i] IS ':')) last = i+1;
         }
         if (last != std::string::npos) {
            auto folder = std::string(Path, last);
            task->setPath(folder);
         }
      }
      else if (Buffer) xml->setStatement(Buffer);

      if (InitObject(xml) IS ERR::Okay) {
         Self->SVGVersion = 1.0;

         Self->XML = xml;

         convert_styles(xml->Tags);

         objVector *sibling = NULL;
         for (auto &scan : xml->Tags) {
            if (StrMatch("svg", scan.name()) IS ERR::Okay) {
               svgState state(Self);
               if (Self->Target) xtag_svg(Self, state, scan, Self->Target, sibling);
               else xtag_svg(Self, state, scan, Self->Scene, sibling);
            }
         }

         // Support for inheritance

         for (auto &inherit : Self->Inherit) {
            OBJECTPTR ref;
            if (scFindDef(Self->Scene, inherit.ID.c_str(), &ref) IS ERR::Okay) {
               inherit.Object->set(FID_Inherit, ref);
            }
            else log.warning("Failed to resolve ID %s for inheritance.", inherit.ID.c_str());
         }

         if ((Self->Flags & SVF::AUTOSCALE) != SVF::NIL) {
            // If auto-scale is enabled, access the top-level viewport and set the Width and Height to 100%

            auto view = Self->Scene->Viewport;
            while ((view) and (view->Class->ClassID != ID_VECTORVIEWPORT)) view = (objVectorViewport *)view->Next;
            if (view) view->setFields(fl::Width(SCALE(1.0)), fl::Height(SCALE(1.0)));
         }
      }
      else error = ERR::Init;

      if (working_path) {
         task->setPath(working_path);
         FreeResource(working_path);
      }
   }
   else error = ERR::NewObject;

end:
#ifndef DEBUG
   AdjustLogLevel(-1);
#endif
   return error;
}

//********************************************************************************************************************
// Example style string: "fill:rgb(255,0,0);stroke:none;"

static void convert_styles(objXML::TAGS &Tags)
{
   pf::Log log(__FUNCTION__);

   for (auto &tag : Tags) {
      for (LONG style=1; style < std::ssize(tag.Attribs); style++) {
         if (StrMatch("style", tag.Attribs[style].Name) != ERR::Okay) continue;

         // Convert all the style values into real attributes.

         auto value = std::move(tag.Attribs[style].Value);
         tag.Attribs.erase(tag.Attribs.begin() + style);
         for (unsigned v=0; v < value.size(); ) {
            while ((value[v]) and (value[v] <= 0x20)) v++;

            auto n_start = v;
            auto n_end = value.find(':', n_start);
            if (n_end != std::string::npos) {
               auto v_start = n_end + 1;
               while ((value[v_start]) and (value[v_start] <= 0x20)) v_start++;

               auto v_end = value.find(';', v_start);
               if (v_end IS std::string::npos) v_end = value.size();

               tag.Attribs.emplace_back(value.substr(n_start, n_end - n_start), value.substr(v_start, v_end-v_start));

               v = v_end + 1;
            }
            else {
               log.warning("Style string missing ':' to denote value: %s", value.c_str());
               break;
            }
         }

         break;
      }

      if (!tag.Children.empty()) convert_styles(tag.Children);
   }
}
