
#if defined(DEBUG)
static void debug_tree(CSTRING Header, OBJECTPTR) __attribute__ ((unused));
static void debug_branch(CSTRING Header, OBJECTPTR, LONG *Level) __attribute__ ((unused));

static void debug_branch(CSTRING Header, OBJECTPTR Vector, LONG *Level)
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
         if (((objVectorScene *)Vector)->Viewport) debug_branch(Header, &((objVectorScene *)Vector)->Viewport->Head, Level);
      }
      else if (Vector->ClassID IS ID_VECTOR) {
         objVector *shape = (objVector *)Vector;
         log.msg("%p<-%p->%p Child %p %s%s", shape->Prev, shape, shape->Next, shape->Child, spacing, shape->Head.Class->ClassName);
         if (shape->Child) debug_branch(Header, shape->Child, Level);
         Vector = &shape->Next->Head;
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
      if (Vector->ClassID IS ID_VECTOR) {
         Vector = &(((objVector *)Vector)->Next->Head);
      }
      else break;
   }
}

#endif

//********************************************************************************************************************

static void parse_result(objSVG *Self, objFilterEffect *Effect, CSTRING Value)
{
   if (!Self->Effects.contains(std::string(Value))) {
      Self->Effects.emplace(std::string(Value), Effect);
   }
}

//********************************************************************************************************************

static void parse_input(objSVG *Self, OBJECTPTR Effect, CSTRING Input, FIELD SourceField, FIELD RefField)
{
   switch (StrHash(Input, FALSE)) {
      case SVF_SOURCEGRAPHIC:   SetLong(Effect, SourceField, VSF_GRAPHIC); break;
      case SVF_SOURCEALPHA:     SetLong(Effect, SourceField, VSF_ALPHA); break;
      case SVF_BACKGROUNDIMAGE: SetLong(Effect, SourceField, VSF_BKGD); break;
      case SVF_BACKGROUNDALPHA: SetLong(Effect, SourceField, VSF_BKGD_ALPHA); break;
      case SVF_FILLPAINT:       SetLong(Effect, SourceField, VSF_FILL); break;
      case SVF_STROKEPAINT:     SetLong(Effect, SourceField, VSF_STROKE); break;
      default:  {
         if (Self->Effects.contains(Input)) {
            SetPointer(Effect, RefField, Self->Effects[Input]);
         }
         else {
            parasol::Log log;
            log.warning("Unrecognised input '%s'", Input);
         }
         break;
      }
   }
}

//********************************************************************************************************************

static LONG count_stops(objSVG *Self, const XMLTag *Tag)
{
   LONG stopcount = 0;
   for (auto scan=Tag->Child; scan; scan=scan->Next) {
      if (!StrMatch("stop", scan->Attrib->Name)) stopcount++;
   }

   return stopcount;
}

//********************************************************************************************************************
// Note that all offsets are percentages.

static ERROR process_transition_stops(objSVG *Self, const XMLTag *Tag, Transition *Stops)
{
   parasol::Log log("process_stops");

   log.traceBranch("");

   LONG i = 0;
   for (auto scan=Tag->Child; scan; scan=scan->Next) {
      if (!StrMatch("stop", scan->Attrib->Name)) {
         Stops[i].Offset = 0;
         Stops[i].Transform = NULL;
         for (LONG a=1; a < scan->TotalAttrib; a++) {
            CSTRING name = scan->Attrib[a].Name;
            CSTRING value = scan->Attrib[a].Value;
            if (!value) continue;

            if (!StrMatch("offset", name)) {
               Stops[i].Offset = StrToFloat(value);
               for (LONG j=0; value[j]; j++) {
                  if (value[j] IS '%') {
                     Stops[i].Offset = Stops[i].Offset * 0.01; // Must be in the range of 0 - 1.0
                     break;
                  }
               }

               if (Stops[i].Offset < 0.0) Stops[i].Offset = 0;
               else if (Stops[i].Offset > 1.0) Stops[i].Offset = 1.0;
            }
            else if (!StrMatch("transform", name)) {
               Stops[i].Transform = value;
            }
            else log.warning("Unable to process stop attribute '%s'", name);
         }

         i++;
      }
      else log.warning("Unknown element in transition, '%s'", scan->Attrib->Name);
   }

   return ERR_Okay;
}

//********************************************************************************************************************
// Save an id reference for an SVG element.  The element can be then be found at any time with find_href().

INLINE bool add_id(objSVG *Self, const XMLTag *Tag, CSTRING Name)
{
   if (Self->IDs.contains(std::string(Name))) return false;
   Self->IDs.emplace(std::string(Name), Tag->Index);
   return true;
}

INLINE bool add_id(objSVG *Self, const XMLTag *Tag, const std::string Name)
{
   if (Self->IDs.contains(Name)) return false;
   Self->IDs.emplace(Name, Tag->Index);
   return true;
}

//********************************************************************************************************************

static CSTRING folder(objSVG *Self)
{
   if (Self->Folder) {
      if (Self->Folder[0]) return Self->Folder;
      else return NULL;
   }
   if (!Self->Path) return NULL;

   // Setting a path of "my/house/is/red.svg" results in "my/house/is/"

   STRING folder;
   if (!ResolvePath(Self->Path, RSF_NO_FILE_CHECK, &folder)) {
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

static const std::string uri_name(CSTRING Ref)
{
   while ((*Ref) and (*Ref <= 0x20)) Ref++;

   if (*Ref IS '#') {
      return std::string(Ref+1);
   }
   else if (!StrCompare("url(#", Ref, 5, 0)) {
      WORD i;
      Ref += 5;
      for (i=0; (Ref[i] != ')') and (Ref[i]); i++);
      return std::string(Ref, i);
   }
   else return std::string(Ref);

   return std::string("");
}

//********************************************************************************************************************

static LONG find_href_tag(objSVG *Self, CSTRING Ref)
{
   while ((*Ref) and (*Ref <= 0x20)) Ref++;
   auto ref = uri_name(Ref);
   if ((!ref.empty()) and (Self->IDs.contains(ref))) {
      return Self->IDs[ref].TagIndex;
   }
   return -1;
}

/*****************************************************************************
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

static DOUBLE read_time(CSTRING Value)
{
   DOUBLE units[3];

   while ((*Value) and (*Value <= 0x20)) Value++;
   if ((*Value >= '0') and (*Value <= '9')) {
      units[0] = StrToFloat(Value);
      while ((*Value >= '0') and (*Value <= '9')) Value++;
      if (*Value IS '.') {
         Value++;
         while ((*Value >= '0') and (*Value <= '9')) Value++;
      }

      if (*Value IS ':') {
         Value++;
         units[1] = StrToFloat(Value);
         while ((*Value >= '0') and (*Value <= '9')) Value++;
         if (*Value IS '.') {
            Value++;
            while ((*Value >= '0') and (*Value <= '9')) Value++;
         }

         if (*Value IS ':') {
            Value++;
            units[2] = StrToFloat(Value);
            while ((*Value >= '0') and (*Value <= '9')) Value++;
            if (*Value IS '.') {
               Value++;
               while ((*Value >= '0') and (*Value <= '9')) Value++;
            }

            // hh:nn:ss
            return (units[0] * 60 * 60) + (units[1] * 60) + units[2];
         }
         else { // hh:nn
            return (units[0] * 60 * 60) + (units[1] * 60);
         }
      }
      else if ((Value[0] IS 'h') and (Value[1] <= 0x20)) {
         return units[0] * 60 * 60;
      }
      else if ((Value[0] IS 's') and (Value[1] <= 0x20)) {
         return units[0];
      }
      else if ((Value[0] IS 'm') and (Value[1] IS 'i') and (Value[2] IS 'n') and (Value[3] IS 0)) {
         return units[0] * 60;
      }
      else if ((Value[0] IS 'm') and (Value[1] IS 's') and (Value[2] <= 0x20)) {
         return units[0] / 1000.0;
      }
      else if (Value[0] <= 0x20) return units[0];
      else return 0;
   }
   else return 0;
}

//********************************************************************************************************************
// Designed for reading unit values such as '50%' and '6px'.  The returned value is scaled to pixels.

static DOUBLE read_unit(CSTRING Value, LARGE *FieldID)
{
   BYTE isnumber = TRUE;

   *FieldID |= TDOUBLE;

   while ((*Value) and (*Value <= 0x20)) Value++;

   CSTRING str = Value;
   if ((*str IS '-') or (*str IS '+')) str++;

   if (((*str >= '0') and (*str <= '9')) or (*str IS '.')) {
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
         *FieldID |= TPERCENT;
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

template <class T> static inline void set_double(T Object, FIELD FieldID, CSTRING Value)
{
   LARGE field = FieldID;
   DOUBLE num = read_unit(Value, &field);
   SetField(Object, field, num);
}

//***************************************************************************
// This version forces all coordinates to be interpreted as relative when in BOUNDINGBOX mode.

INLINE void set_double_units(APTR Object, FIELD FieldID, CSTRING Value, LONG Units)
{
   LARGE field = FieldID;
   DOUBLE num = read_unit(Value, &field);
   if (Units IS VUNIT_BOUNDING_BOX) {
      if (!(field & TPERCENT)) {
         num *= 100.0;
         field |= TPERCENT;
      }
   }
   SetField(Object, field, num);
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
         Value++;
      }
      else if ((*Value IS '+') and (Value[1] >= '0') and (Value[1] <= '9')) {
         *result = StrToFloat(Value);
         Value++;
      }
      else if (((*Value >= '0') and (*Value <= '9'))) {
         *result = StrToFloat(Value);
      }
      else if ((*Value IS '.') and (Value[1] >= '0') and (Value[1] <= '9')) {
         *result = StrToFloat(Value);
      }
      else break;

      while ((*Value >= '0') and (*Value <= '9')) Value++;

      if (*Value IS '.') {
         Value++;
         if ((*Value >= '0') and (*Value <= '9')) {
            while ((*Value >= '0') and (*Value <= '9')) Value++;
         }
      }
   }

   va_end(list);
   return Value;
}

//********************************************************************************************************************
// Currently used by gradient functions.

static void add_inherit(objSVG *Self, OBJECTPTR Object, CSTRING ID)
{
   parasol::Log log(__FUNCTION__);
   svgInherit *inherit;
   log.trace("Object: %d, ID: %s", Object->UID, ID);
   if (!AllocMemory(sizeof(svgInherit), MEM_DATA|MEM_NO_CLEAR, &inherit, NULL)) {
      inherit->Object = Object;
      inherit->Next = Self->Inherit;
      while ((*ID) and (*ID IS '#')) ID++;
      StrCopy(ID, inherit->ID, sizeof(inherit->ID));
      Self->Inherit = inherit;
   }
}

//********************************************************************************************************************

static void reset_state(svgState *State)
{
   static const char *fill = "rgb(0,0,0)";
   static const char *family = "Open Sans";
   ClearMemory(State, sizeof(*State));
   State->Fill = fill;
   State->FillOpacity = -1;
   State->Opacity = -1;
   State->FontFamily = family;
   State->PathQuality = RQ_AUTO;
}

//********************************************************************************************************************

static ERROR load_svg(objSVG *Self, CSTRING Path, CSTRING Buffer)
{
   parasol::Log log(__FUNCTION__);

   if (!Path) return ERR_NullArgs;

   log.branch("Path: %s [Log-level reduced]", Path);

#ifndef DEBUG
   AdjustLogLevel(1);
#endif

   objXML *xml;
   ERROR error = ERR_Okay;
   if (!NewObject(ID_XML, NF_INTEGRAL, &xml)) {
      OBJECTPTR task = CurrentTask();
      STRING working_path = NULL;

      if (Path) {
         if (!StrCompare("*.svgz", Path, 0, STR_WILDCARD)) {
            OBJECTPTR file, stream;
            if (!CreateObject(ID_FILE, 0, &file,
                  FID_Owner|TLONG, xml->UID,
                  FID_Path|TSTR,   Path,
                  FID_Flags|TLONG, FL_READ,
                  TAGEND)) {
               if (!CreateObject(ID_COMPRESSEDSTREAM, 0, &stream,
                     FID_Owner|TLONG, file->UID,
                     FID_Input|TPTR,  file,
                     TAGEND)) {
                  SetPointer(xml, FID_Source, stream);
               }
               else {
                  acFree(xml);
                  acFree(file);
                  error = ERR_CreateObject;
                  goto end;
               }
            }
            else {
               acFree(xml);
               error = ERR_CreateObject;
               goto end;
            }
         }
         else SetString(xml, FID_Path, Path);

         if (!GetString(task, FID_Path, &working_path)) working_path = StrClone(working_path);

         // Set a new working path based on the path

         char folder[512];
         WORD last = 0;
         for (LONG i=0; (Path[i]) and ((size_t)i < sizeof(folder)-1); i++) {
            folder[i] = Path[i];
            if ((Path[i] IS '/') or (Path[i] IS '\\') or (Path[i] IS ':')) last = i+1;
         }
         folder[last] = 0;
         if (last) SetString(task, FID_Path, folder);
      }
      else if (Buffer) SetString(xml, FID_Statement, Buffer);

      if (!acInit(xml)) {
         Self->SVGVersion = 1.0;

         convert_styles(xml);

         OBJECTPTR sibling = NULL;
         for (auto tag=xml->Tags[0]; (tag); tag=tag->Next) {
            if (!StrMatch("svg", tag->Attrib->Name)) {
               svgState state;
               reset_state(&state);
               if (Self->Target) xtag_svg(Self, xml, &state, tag, Self->Target, &sibling);
               else xtag_svg(Self, xml, &state, tag, Self->Scene, &sibling);
            }
         }

         // Support for inheritance

         svgInherit *inherit = Self->Inherit;
         while (inherit) {
            OBJECTPTR ref;
            if (!scFindDef(Self->Scene, inherit->ID, &ref)) {
               SetPointer(inherit->Object, FID_Inherit, ref);
            }
            else log.warning("Failed to resolve ID %s for inheritance.", inherit->ID);
            inherit = inherit->Next;
         }

         if (Self->Flags & SVF_AUTOSCALE) {
            // If auto-scale is enabled, access the top-level viewport and set the Width and Height to 100%

            objVector *view = Self->Scene->Viewport;
            while ((view) and (view->SubID != ID_VECTORVIEWPORT)) view = view->Next;
            if (view) SetFields(view, FID_Width|TDOUBLE|TPERCENT, 100.0, FID_Height|TDOUBLE|TPERCENT, 100.0, TAGEND);
         }
      }
      else error = ERR_Init;

      if (working_path) {
         SetString(task, FID_Path, working_path);
         FreeResource(working_path);
      }

      acFree(xml);
   }
   else error = ERR_NewObject;

end:
#ifndef DEBUG
   AdjustLogLevel(-1);
#endif
   return error;
}

//********************************************************************************************************************

static void convert_styles(objXML *XML)
{
   parasol::Log log(__FUNCTION__);

   for (LONG t=0; t < XML->TagCount; t++) {
      char value[1024];
      XMLTag *tag = XML->Tags[t];
      for (LONG a=1; a < tag->TotalAttrib; a++) {
         if (!StrMatch("style", tag->Attrib[a].Name)) {
            // Convert all the style values into real attributes.

            LONG tagindex = tag->Index;
            StrCopy(tag->Attrib[a].Value, value, sizeof(value));
            LONG v = 0;
            while (value[v]) {
               while ((value[v]) and (value[v] <= 0x20)) v++;

               LONG ni = v;
               char c;
               while ((c = value[v]) and (c != ':')) v++;

               if (c IS ':') {
                  value[v++] = 0;
                  while ((value[v]) and (value[v] <= 0x20)) v++;
                  LONG vi = v;
                  while ((value[v]) and (value[v] != ';')) v++;
                  if (value[v] IS ';') value[v++] = 0;
                  if (v > vi) xmlSetAttrib(XML, tagindex, XMS_NEW, value+ni, value+vi);
               }
               else log.warning("Style string missing ':' to denote value: %s", value);
            }

            xmlSetAttrib(XML, tagindex, XMS_UPDATE, "style", NULL); // Remove the style attribute.
            break;
         }
      }
   }
}
