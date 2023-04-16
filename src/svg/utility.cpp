
#if defined(DEBUG)
static void debug_tree(CSTRING Header, OBJECTPTR) __attribute__ ((unused));
static void debug_branch(CSTRING Header, OBJECTPTR, LONG *Level) __attribute__ ((unused));

static void debug_branch(CSTRING Header, OBJECTPTR Vector, LONG *Level)
{
   pf::Log log(Header);
   UBYTE spacing[*Level + 1];
   LONG i;

   *Level = *Level + 1;
   for (i=0; i < *Level; i++) spacing[i] = ' '; // Indenting
   spacing[i] = 0;

   while (Vector) {
      if (Vector->Class->ClassID IS ID_VECTORSCENE) {
         log.msg("Scene: %p", Vector);
         if (((objVectorScene *)Vector)->Viewport) debug_branch(Header, ((objVectorScene *)Vector)->Viewport, Level);
      }
      else if (Vector->Class->BaseClassID IS ID_VECTOR) {
         objVector *shape = (objVector *)Vector;
         log.msg("%p<-%p->%p Child %p %s%s", shape->Prev, shape, shape->Next, shape->Child, spacing, shape->className());
         if (shape->Child) debug_branch(Header, shape->Child, Level);
         Vector = shape->Next;
      }
      else break;
   }

   *Level = *Level - 1;
}

static void debug_tree(CSTRING Header, OBJECTPTR Vector)
{
   LONG level = 0;
   while (Vector) {
      debug_branch(Header, Vector, &level);
      if (Vector->Class->BaseClassID IS ID_VECTOR) {
         Vector = (((objVector *)Vector)->Next);
      }
      else break;
   }
}

#endif

//********************************************************************************************************************
// Support for the 'currentColor' colour value.  Finds the first parent with a defined fill colour and returns it.

static ERROR current_colour(extSVG *Self, objVector *Vector, FRGB &RGB)
{
   if (Vector->Class->BaseClassID != ID_VECTOR) return ERR_Failed;

   Vector = (objVector *)Vector->Parent;
   while (Vector) {
      if (Vector->Class->BaseClassID != ID_VECTOR) return ERR_Failed;

      if (!GetFieldArray(Vector, FID_FillColour|TFLOAT, &RGB, NULL)) {
         if (RGB.Alpha != 0) return ERR_Okay;
      }
      Vector = (objVector *)Vector->Parent;
   }

   return ERR_Failed;
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
      case SVF_SOURCEGRAPHIC:   Effect->set(SourceField, VSF_GRAPHIC); break;
      case SVF_SOURCEALPHA:     Effect->set(SourceField, VSF_ALPHA); break;
      case SVF_BACKGROUNDIMAGE: Effect->set(SourceField, VSF_BKGD); break;
      case SVF_BACKGROUNDALPHA: Effect->set(SourceField, VSF_BKGD_ALPHA); break;
      case SVF_FILLPAINT:       Effect->set(SourceField, VSF_FILL); break;
      case SVF_STROKEPAINT:     Effect->set(SourceField, VSF_STROKE); break;
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
      if (!StrMatch("stop", scan.name())) {
         Transition stop;
         stop.Offset = 0;
         stop.Transform = NULL;
         for (unsigned a=1; a < scan.Attribs.size(); a++) {
            auto &name = scan.Attribs[a].Name;
            auto &value = scan.Attribs[a].Value;
            if (value.empty()) continue;

            if (!StrMatch("offset", name)) {
               stop.Offset = StrToFloat(value.c_str());
               if (value.find('%') != std::string::npos) {
                  stop.Offset = stop.Offset * 0.01; // Must be in the range of 0 - 1.0
               }

               if (stop.Offset < 0.0) stop.Offset = 0;
               else if (stop.Offset > 1.0) stop.Offset = 1.0;
            }
            else if (!StrMatch("transform", name)) {
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
// Save an id reference for an SVG element.  The element can be then be found at any time with find_href().  We store
// a copy of the tag data as pointer references are too high a risk.

INLINE bool add_id(extSVG *Self, const XMLTag &Tag, CSTRING Name)
{
   if (Self->IDs.contains(std::string(Name))) return false;
   Self->IDs.emplace(std::string(Name), Tag);
   return true;
}

INLINE bool add_id(extSVG *Self, const XMLTag &Tag, const std::string Name)
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
   if (!ResolvePath(Self->Path, RSF::NO_FILE_CHECK, &folder)) {
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

static const std::string uri_name(const std::string Ref)
{
   LONG skip = 0;
   while ((Ref[skip]) and (Ref[skip] <= 0x20)) skip++;

   if (Ref[skip] IS '#') {
      return Ref.substr(skip+1);
   }
   else if (!StrCompare("url(#", Ref.c_str() + skip, 5)) {
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

static DOUBLE read_time(const std::string Value)
{
   DOUBLE units[3];

   auto v = Value.c_str();
   while ((*v) and (*v <= 0x20)) v++;
   if ((*v >= '0') and (*v <= '9')) {
      units[0] = StrToFloat(v);
      while ((*v >= '0') and (*v <= '9')) v++;
      if (*v IS '.') {
         v++;
         while ((*v >= '0') and (*v <= '9')) v++;
      }

      if (*v IS ':') {
         v++;
         units[1] = StrToFloat(v);
         while ((*v >= '0') and (*v <= '9')) v++;
         if (*v IS '.') {
            v++;
            while ((*v >= '0') and (*v <= '9')) v++;
         }

         if (*v IS ':') {
            v++;
            units[2] = StrToFloat(v);
            while ((*v >= '0') and (*v <= '9')) v++;
            if (*v IS '.') {
               v++;
               while ((*v >= '0') and (*v <= '9')) v++;
            }

            // hh:nn:ss
            return (units[0] * 60 * 60) + (units[1] * 60) + units[2];
         }
         else { // hh:nn
            return (units[0] * 60 * 60) + (units[1] * 60);
         }
      }
      else if ((v[0] IS 'h') and (v[1] <= 0x20)) {
         return units[0] * 60 * 60;
      }
      else if ((v[0] IS 's') and (v[1] <= 0x20)) {
         return units[0];
      }
      else if ((v[0] IS 'm') and (v[1] IS 'i') and (v[2] IS 'n') and (v[3] IS 0)) {
         return units[0] * 60;
      }
      else if ((v[0] IS 'm') and (v[1] IS 's') and (v[2] <= 0x20)) {
         return units[0] / 1000.0;
      }
      else if (v[0] <= 0x20) return units[0];
      else return 0;
   }
   else return 0;
}

//********************************************************************************************************************
// Designed for reading unit values such as '50%' and '6px'.  The returned value is scaled to pixels.

static DOUBLE read_unit(const std::string Value, LARGE *FieldID)
{
   bool isnumber = true;

   *FieldID |= TDOUBLE;

   auto v = Value.c_str();
   while ((*v) and (*v <= 0x20)) v++;

   CSTRING str = v;
   if ((*str IS '-') or (*str IS '+')) str++;

   if (((*str >= '0') and (*str <= '9')) or (*str IS '.')) {
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
         *FieldID |= TPERCENT;
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

      return StrToFloat(v) * multiplier;
   }
   else return 0;
}

//********************************************************************************************************************

template <class T> static inline void set_double(T Object, FIELD FieldID, const std::string Value)
{
   LARGE field = FieldID;
   DOUBLE num = read_unit(Value, &field);
   SetField(Object, field, num);
}

//********************************************************************************************************************
// This version forces all coordinates to be interpreted as relative when in BOUNDINGBOX mode.

INLINE void set_double_units(OBJECTPTR Object, FIELD FieldID, const std::string Value, LONG Units)
{
   LARGE field = FieldID;
   DOUBLE num = read_unit(Value, &field);
   if (Units IS VUNIT_BOUNDING_BOX) field |= TPERCENT;
   SetField(Object, field, num);
}

//********************************************************************************************************************
// The parser will break once the string value terminates, or an invalid character is encountered.  Parsed characters
// include: 0 - 9 , ( ) - + SPACE

static void read_numseq(const std::string Value, ...)
{
   va_list list;
   DOUBLE *result;

   if (Value.empty()) return;

   va_start(list, Value);

   auto v = Value.c_str();
   while ((result = va_arg(list, DOUBLE *))) {
      while ((*v) and ((*v <= 0x20) or (*v IS ',') or (*v IS '(') or (*v IS ')'))) v++;
      if (!v[0]) break;

      STRING next = NULL;
      DOUBLE num = strtod(v, &next);
      if ((!num) and ((!next) or (v IS next))) {  // Invalid character or end-of-stream check.
         v = next;
         break;
      }

      *result = num;
      v = next;
   }

   va_end(list);
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

static ERROR load_svg(extSVG *Self, CSTRING Path, CSTRING Buffer)
{
   pf::Log log(__FUNCTION__);

   if (!Path) return ERR_NullArgs;

   log.branch("Path: %s [Log-level reduced]", Path);

#ifndef DEBUG
   AdjustLogLevel(1);
#endif

   objXML *xml;
   ERROR error = ERR_Okay;
   if (!NewObject(ID_XML, NF::INTEGRAL, &xml)) {
      objTask *task = CurrentTask();
      STRING working_path = NULL;

      if (Path) {
         if (!StrCompare("*.svgz", Path, 0, STR::WILDCARD)) {
            if (auto file = objFile::create::global(fl::Owner(xml->UID), fl::Path(Path), fl::Flags(FL::READ))) {
               if (auto stream = objCompressedStream::create::global(fl::Owner(file->UID), fl::Input(file))) {
                  xml->setSource(stream);
               }
               else {
                  FreeResource(xml);
                  FreeResource(file);
                  error = ERR_CreateObject;
                  goto end;
               }
            }
            else {
               FreeResource(xml);
               error = ERR_CreateObject;
               goto end;
            }
         }
         else xml->setPath(Path);

         if (!task->get(FID_Path, &working_path)) working_path = StrClone(working_path);

         // Set a new working path based on the path

         char folder[512];
         WORD last = 0;
         for (LONG i=0; (Path[i]) and ((size_t)i < sizeof(folder)-1); i++) {
            folder[i] = Path[i];
            if ((Path[i] IS '/') or (Path[i] IS '\\') or (Path[i] IS ':')) last = i+1;
         }
         folder[last] = 0;
         if (last) task->setPath(folder);
      }
      else if (Buffer) xml->setStatement(Buffer);

      if (!InitObject(xml)) {
         Self->SVGVersion = 1.0;

         convert_styles(xml->Tags);

         objVector *sibling = NULL;
         for (auto &scan : xml->Tags) {
            if (!StrMatch("svg", scan.name())) {
               svgState state;
               if (Self->Target) xtag_svg(Self, xml, state, scan, Self->Target, &sibling);
               else xtag_svg(Self, xml, state, scan, Self->Scene, &sibling);
            }
         }

         // Support for inheritance

         for (auto &inherit : Self->Inherit) {
            OBJECTPTR ref;
            if (!scFindDef(Self->Scene, inherit.ID.c_str(), &ref)) {
               inherit.Object->set(FID_Inherit, ref);
            }
            else log.warning("Failed to resolve ID %s for inheritance.", inherit.ID.c_str());
         }

         if ((Self->Flags & SVF::AUTOSCALE) != SVF::NIL) {
            // If auto-scale is enabled, access the top-level viewport and set the Width and Height to 100%

            auto view = Self->Scene->Viewport;
            while ((view) and (view->Class->ClassID != ID_VECTORVIEWPORT)) view = (objVectorViewport *)view->Next;
            if (view) view->setFields(fl::Width(PERCENT(1.0)), fl::Height(PERCENT(1.0)));
         }
      }
      else error = ERR_Init;

      if (working_path) {
         task->setPath(working_path);
         FreeResource(working_path);
      }

      FreeResource(xml);
   }
   else error = ERR_NewObject;

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
      for (unsigned style=1; style < tag.Attribs.size(); style++) {
         if (StrMatch("style", tag.Attribs[style].Name)) continue;

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
