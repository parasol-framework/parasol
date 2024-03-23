
//********************************************************************************************************************

static ARF parse_aspect_ratio(const std::string Value)
{
   CSTRING v = Value.c_str();
   while ((*v) and (*v <= 0x20)) v++;

   if (!StrMatch("none", v)) return ARF::NONE;
   else {
      ARF flags = ARF::NIL;
      if (!StrCompare("xMin", v, 4)) { flags |= ARF::X_MIN; v += 4; }
      else if (!StrCompare("xMid", v, 4)) { flags |= ARF::X_MID; v += 4; }
      else if (!StrCompare("xMax", v, 4)) { flags |= ARF::X_MAX; v += 4; }

      if (!StrCompare("yMin", v, 4)) { flags |= ARF::Y_MIN; v += 4; }
      else if (!StrCompare("yMid", v, 4)) { flags |= ARF::Y_MID; v += 4; }
      else if (!StrCompare("yMax", v, 4)) { flags |= ARF::Y_MAX; v += 4; }

      while ((*v) and (*v <= 0x20)) v++;

      if (!StrCompare("meet", v, 4)) { flags |= ARF::MEET; }
      else if (!StrCompare("slice", v, 5)) { flags |= ARF::SLICE; }
      return flags;
   }
}

//********************************************************************************************************************

static RQ shape_rendering_to_render_quality(const std::string Value)
{
   pf::Log log;

   if (!StrMatch("auto", Value)) return RQ::AUTO;
   else if (!StrMatch("optimize-speed", Value)) return RQ::FAST;
   else if (!StrMatch("optimizeSpeed", Value)) return RQ::FAST;
   else if (!StrMatch("crisp-edges", Value)) return RQ::CRISP;
   else if (!StrMatch("crispEdges", Value)) return RQ::CRISP;
   else if (!StrMatch("geometric-precision", Value)) return RQ::PRECISE;
   else if (!StrMatch("geometricPrecision", Value)) return RQ::PRECISE;
   else if (!StrMatch("best", Value)) return RQ::BEST;
   else log.warning("Unknown shape-rendering value '%s'", Value.c_str());

   return RQ::AUTO;
}

//********************************************************************************************************************
// Apply the current state values to a vector.

void svgState::applyAttribs(OBJECTPTR Vector) const noexcept
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("%s: Fill: %s, Stroke: %s, Opacity: %.2f, Font: %s %s",
      Vector->Class->ClassName, m_fill.c_str(), m_stroke.c_str(), m_opacity, m_font_family.c_str(), m_font_size.c_str());

   if (!m_fill.empty())   Vector->set(FID_Fill, m_fill);
   if (!m_stroke.empty()) Vector->set(FID_Stroke, m_stroke);
   if (m_stroke_width)    Vector->set(FID_StrokeWidth, m_stroke_width);
   if (Vector->Class->ClassID IS ID_VECTORTEXT) {
      if (!m_font_family.empty()) Vector->set(FID_Face, m_font_family);
      if (!m_font_size.empty())   Vector->set(FID_FontSize, m_font_size);
      if (m_font_weight) Vector->set(FID_Weight, m_font_weight);
   }
   if (m_fill_opacity >= 0.0) Vector->set(FID_FillOpacity, m_fill_opacity);
   if (m_opacity >= 0.0) Vector->set(FID_Opacity, m_opacity);

   if (Vector->Class->ClassID != ID_VECTORTEXT) {
      if (m_path_quality != RQ::AUTO) Vector->set(FID_PathQuality, LONG(m_path_quality));
   }
}

//********************************************************************************************************************
// Copy a tag's attributes to the current state.

void svgState::applyTag(XMLTag &Tag) noexcept
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("Total Attributes: %d", LONG(Tag.Attribs.size()));

   for (unsigned a=1; a < Tag.Attribs.size(); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch (StrHash(Tag.Attribs[a].Name)) {
         case SVF_COLOR:  m_color = val; break; // Affects 'currentColor'
         case SVF_FILL:   m_fill = val; break;
         case SVF_STROKE:       
            m_stroke = val; 
            if (!m_stroke_width) m_stroke_width = 1; 
            break;
         case SVF_STROKE_WIDTH: m_stroke_width = StrToFloat(val); break;
         case SVF_FONT_FAMILY:  m_font_family = val; break;
         case SVF_FONT_SIZE:    m_font_size = val; break;
         case SVF_FONT_WEIGHT: {
            m_font_weight = StrToFloat(val);
            if (!m_font_weight) {
               switch(StrHash(val)) {
                  case SVF_NORMAL:  m_font_weight = 400; break;
                  case SVF_LIGHTER: m_font_weight = 300; break; // -100 off the inherited weight
                  case SVF_BOLD:    m_font_weight = 700; break;
                  case SVF_BOLDER:  m_font_weight = 900; break; // +100 on the inherited weight
                  case SVF_INHERIT: m_font_weight = 400; break; // Not supported correctly yet.
                  default:
                     log.warning("No support for font-weight value '%s'", val.c_str()); // Non-fatal
                     m_font_weight = 400;
               }
            }
            break;
         }
         case SVF_FILL_OPACITY: m_fill_opacity = StrToFloat(val); break;
         case SVF_OPACITY:      m_opacity = StrToFloat(val); break;
         case SVF_SHAPE_RENDERING: m_path_quality = shape_rendering_to_render_quality(val); break;
      }
   }
}

//********************************************************************************************************************
// Process all child elements that belong to the target Tag.

static void process_children(extSVG *Self, svgState &State, XMLTag &Tag, OBJECTPTR Vector)
{
   objVector *sibling = NULL;
   for (auto &child : Tag.Children) {
      if (child.isTag()) {
         xtag_default(Self, State, child, Vector, sibling);
      }
   }
}

//********************************************************************************************************************

static void xtag_pathtransition(extSVG *Self, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("Tag: %d", Tag.ID);

   OBJECTPTR trans;
   if (!NewObject(ID_VECTORTRANSITION, &trans)) {
      trans->setFields(
         fl::Owner(Self->Scene->UID), // All clips belong to the root page to prevent hierarchy issues.
         fl::Name("SVGTransition")
      );

      std::string id;
      for (unsigned a=1; a < Tag.Attribs.size(); a++) {
         if (Tag.Attribs[a].Value.empty()) continue;

         switch(StrHash(Tag.Attribs[a].Name)) {
            case SVF_ID: id = Tag.Attribs[a].Value; break;
         }
      }

      if (!id.empty()) {
         auto stops = process_transition_stops(Self, Tag.Children);
         if (stops.size() >= 2) {
            SetArray(trans, FID_Stops, stops);

            if (!InitObject(trans)) {
               if (!Self->Cloning) scAddDef(Self->Scene, id.c_str(), trans);
               return;
            }
         }
         else log.warning("At least two stops are required for <pathTransition> at line %d.", Tag.LineNo);
      }
      else log.warning("No id attribute specified in <pathTransition> at line %d.", Tag.LineNo);

      FreeResource(trans);
   }
}

//********************************************************************************************************************

static void xtag_clippath(extSVG *Self, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("Tag: %d", Tag.ID);

   std::string id, transform, units;
   for (LONG a=1; a < std::ssize(Tag.Attribs); a++) {
      auto &value = Tag.Attribs[a].Value;
      if (value.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_ID:            id        = value; break;
         case SVF_TRANSFORM:     transform = value; break;
         case SVF_CLIPPATHUNITS: units     = value; break;
         case SVF_EXTERNALRESOURCESREQUIRED: break; // Deprecated SVG attribute
         default:
            log.warning("<clipPath> attribute '%s' unrecognised @ line %d", Tag.Attribs[a].Name.c_str(), Tag.LineNo);
            break;
      }
   }
   
   if (id.empty()) {
      // Declaring a clipPath without an id is poor form, but it is valid SVG and likely that at least
      // one child object will specify an id in this case.
      static LONG clip_id = 1;
      id = "auto_clippath_" + std::to_string(clip_id++);
   }

   // A clip-path with an ID can only be added once (important when a clip-path is repeatedly referenced)

   if (add_id(Self, Tag, id)) {
      objVector *clip;
      if (!NewObject(ID_VECTORCLIP, &clip)) {
         clip->setFields(fl::Owner(Self->Scene->UID), fl::Name("SVGClip"));

         if (!transform.empty()) parse_transform(clip, transform);

         if (!units.empty()) {
            if (!StrMatch("userSpaceOnUse", units)) clip->set(FID_Units, LONG(VUNIT::USERSPACE));
            else if (!StrMatch("objectBoundingBox", units)) clip->set(FID_Units, LONG(VUNIT::BOUNDING_BOX));
         }

         if (!InitObject(clip)) {
            svgState state(Self);

            // Valid child elements for clip-path are:
            // Shapes:   circle, ellipse, line, path, polygon, polyline, rect, text, ...
            // Commands: use, animate

            auto vp = clip->get<OBJECTPTR>(FID_Viewport);
            process_children(Self, state, Tag, vp);

            scAddDef(Self->Scene, id.c_str(), clip);
         }
         else FreeResource(clip);
      }
   }
}

//********************************************************************************************************************
// NB: This implementation of mask support uses VectorClip.  An alternative would be to use VectorFilter.
// 
// SVG masks are luminance masks by default (as opposed to masking on a per-channel RGBA basis).
//
// The formula used to get the luminance out of a given RGB value is: .2126R + .7152G + .0722B

static void xtag_mask(extSVG *Self, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("Tag: %d", Tag.ID);
   
   std::string id, transform;
   auto units = VUNIT::USERSPACE;
   for (LONG a=1; a < std::ssize(Tag.Attribs); a++) {
      auto &value = Tag.Attribs[a].Value;
      if (value.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_ID:        id = value; break;
         case SVF_TRANSFORM: transform = value; break;
         case SVF_MASKUNITS:
            if (!StrMatch("userSpaceOnUse", value)) units = VUNIT::USERSPACE;
            else if (!StrMatch("objectBoundingBox", value)) units = VUNIT::BOUNDING_BOX;
            break;
         case SVF_MASKCONTENTUNITS: // TODO
            break;
         case SVF_EXTERNALRESOURCESREQUIRED: // Deprecated SVG attribute
            break;
         case SVF_COLOR_INTERPOLATION:
            break;
         case SVF_FILTER:
            break;
         case SVF_X:
         case SVF_Y:
         case SVF_WIDTH:
         case SVF_HEIGHT:
            break;
         default:
            log.warning("<mask> attribute '%s' unrecognised @ line %d", Tag.Attribs[a].Name.c_str(), Tag.LineNo);
            break;
      }
   }
   
   if (id.empty()) {
      static LONG clip_id = 1;
      id = "auto_mask_" + std::to_string(clip_id++);
   }
   
   // A clip-path with an ID can only be added once (important when a clip-path is repeatedly referenced)

   if (add_id(Self, Tag, id)) {
      objVector *clip;
      if (!NewObject(ID_VECTORCLIP, &clip)) {
         clip->setFields(fl::Owner(Self->Scene->UID), fl::Name("SVGMask"), 
            fl::Flags(VCLF::APPLY_FILLS|VCLF::APPLY_STROKES),
            fl::Units(units));

         if (!transform.empty()) parse_transform(clip, transform); 

         if (!InitObject(clip)) {
            svgState state(Self);
            auto vp = clip->get<OBJECTPTR>(FID_Viewport);
            process_children(Self, state, Tag, vp);

            scAddDef(Self->Scene, id.c_str(), clip);
         }
         else FreeResource(clip);
      }
   }
}

//********************************************************************************************************************

static ERROR parse_fe_blur(extSVG *Self, objVectorFilter *Filter, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_BLURFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   std::string result_name;
   for (unsigned a=1; a < Tag.Attribs.size(); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_STDDEVIATION: { // Y is optional, if not set then it is equivalent to X.
            DOUBLE x = -1, y = -1;
            read_numseq(val, { &x, &y });
            if ((x) and (y IS -1)) y = x;
            if (x > 0) fx->set(FID_SX, x);
            if (y > 0) fx->set(FID_SY, y);
            break;
         }

         case SVF_X: FUNIT(FID_X, val).set(fx); break;

         case SVF_Y: FUNIT(FID_Y, val).set(fx); break;

         case SVF_WIDTH: FUNIT(FID_Width, val).set(fx); break;

         case SVF_HEIGHT: FUNIT(FID_Height, val).set(fx); break;

         case SVF_IN: parse_input(Self, fx, val, FID_SourceType, FID_Input); break;

         case SVF_RESULT: result_name = val; break;
      }
   }

   if (!fx->init()) {
      if (!result_name.empty()) parse_result(Self, fx, result_name);
      return ERR_Okay;
   }
   else {
      FreeResource(fx);
      return ERR_Init;
   }
}

//********************************************************************************************************************

static ERROR parse_fe_offset(extSVG *Self, objVectorFilter *Filter, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_OFFSETFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   std::string result_name;
   for (unsigned a=1; a < Tag.Attribs.size(); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_DX: fx->set(FID_XOffset, StrToInt(val)); break;
         case SVF_DY: fx->set(FID_YOffset, StrToInt(val)); break;
         case SVF_IN: parse_input(Self, fx, val, FID_SourceType, FID_Input); break;
         case SVF_RESULT: result_name = val; break;
      }
   }

   if (!fx->init()) {
      if (!result_name.empty()) parse_result(Self, fx, result_name);
      return ERR_Okay;
   }
   else {
      FreeResource(fx);
      return ERR_Init;
   }
}

//********************************************************************************************************************

static ERROR parse_fe_merge(extSVG *Self, objVectorFilter *Filter, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_MERGEFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   for (unsigned a=1; a < Tag.Attribs.size(); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_X: FUNIT(FID_X, val).set(fx); break;
         case SVF_Y: FUNIT(FID_Y, val).set(fx); break;
         case SVF_WIDTH: FUNIT(FID_Width, val).set(fx); break;
         case SVF_HEIGHT: FUNIT(FID_Height, val).set(fx); break;
      }
   }

   std::vector<MergeSource> list;
   for (auto &child : Tag.Children) {
      if (!StrMatch("feMergeNode", child.name())) {
         for (unsigned a=1; a < child.Attribs.size(); a++) {
            if (!StrMatch("in", child.Attribs[a].Name)) {
               switch (StrHash(child.Attribs[a].Value)) {
                  case SVF_SOURCEGRAPHIC:   list.push_back(VSF::GRAPHIC); break;
                  case SVF_SOURCEALPHA:     list.push_back(VSF::ALPHA); break;
                  case SVF_BACKGROUNDIMAGE: list.push_back(VSF::BKGD); break;
                  case SVF_BACKGROUNDALPHA: list.push_back(VSF::BKGD_ALPHA); break;
                  case SVF_FILLPAINT:       list.push_back(VSF::FILL); break;
                  case SVF_STROKEPAINT:     list.push_back(VSF::STROKE); break;
                  default:  {
                     if (auto ref = child.Attribs[a].Value.c_str()) {
                        while ((*ref) and (*ref <= 0x20)) ref++;
                        if (Self->Effects.contains(ref)) {
                           list.emplace_back(VSF::REFERENCE, Self->Effects[ref]);
                        }
                        else log.warning("Invalid 'in' reference '%s'", ref);
                     }
                     else log.warning("'in' reference is an empty string.");

                     break;
                  }
               }
            }
            else log.warning("Invalid feMergeNode attribute '%s'", child.Attribs[a].Name.c_str());
         }
      }
      else log.warning("Unrecognised feMerge child node '%s'", child.name());
   }

   if (!list.empty()) {
      if (SetArray(fx, FID_SourceList, list)) {
         FreeResource(fx);
         return log.warning(ERR_SetField);
      }
   }

   if (!fx->init()) return ERR_Okay;
   else {
      FreeResource(fx);
      return log.warning(ERR_Init);
   }
}

//********************************************************************************************************************

#define CM_SIZE 20

static const DOUBLE glProtanopia[20] = { 0.567,0.433,0,0,0, 0.558,0.442,0,0,0, 0,0.242,0.758,0,0, 0,0,0,1,0 };
static const DOUBLE glProtanomaly[20] = { 0.817,0.183,0,0,0, 0.333,0.667,0,0,0, 0,0.125,0.875,0,0, 0,0,0,1,0 };
static const DOUBLE glDeuteranopia[20] = { 0.625,0.375,0,0,0, 0.7,0.3,0,0,0, 0,0.3,0.7,0,0, 0,0,0,1,0 };
static const DOUBLE glDeuteranomaly[20] = { 0.8,0.2,0,0,0, 0.258,0.742,0,0,0, 0,0.142,0.858,0,0, 0,0,0,1,0 };
static const DOUBLE glTritanopia[20] = { 0.95,0.05,0,0,0, 0,0.433,0.567,0,0, 0,0.475,0.525,0,0, 0,0,0,1,0 };
static const DOUBLE glTritanomaly[20] = { 0.967,0.033,0,0,0, 0,0.733,0.267,0,0, 0,0.183,0.817,0,0, 0,0,0,1,0 };
static const DOUBLE glAchromatopsia[20] = { 0.299,0.587,0.114,0,0, 0.299,0.587,0.114,0,0, 0.299,0.587,0.114,0,0, 0,0,0,1,0 };
static const DOUBLE glAchromatomaly[20] = { 0.618,0.320,0.062,0,0, 0.163,0.775,0.062,0,0, 0.163,0.320,0.516,0,0, 0,0,0,1,0 };

static ERROR parse_fe_colour_matrix(extSVG *Self, objVectorFilter *Filter, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_COLOURFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   std::string result_name;
   for (unsigned a=1; a < Tag.Attribs.size(); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_TYPE: {
            const DOUBLE *m = NULL;
            CM mode = CM::NIL;
            switch(StrHash(val)) {
               case SVF_NONE:          mode = CM::NONE; break;
               case SVF_MATRIX:        mode = CM::MATRIX; break;
               case SVF_SATURATE:      mode = CM::SATURATE; break;
               case SVF_HUEROTATE:     mode = CM::HUE_ROTATE; break;
               case SVF_LUMINANCETOALPHA: mode = CM::LUMINANCE_ALPHA; break;
               // These are special modes that are not included by SVG
               case SVF_CONTRAST:      mode = CM::CONTRAST; break;
               case SVF_BRIGHTNESS:    mode = CM::BRIGHTNESS; break;
               case SVF_HUE:           mode = CM::HUE; break;
               case SVF_COLOURISE:     mode = CM::COLOURISE; break;
               case SVF_DESATURATE:    mode = CM::DESATURATE; break;
               // Colour blindness modes
               case SVF_PROTANOPIA:    mode = CM::MATRIX; m = glProtanopia; break;
               case SVF_PROTANOMALY:   mode = CM::MATRIX; m = glProtanomaly; break;
               case SVF_DEUTERANOPIA:  mode = CM::MATRIX; m = glDeuteranopia; break;
               case SVF_DEUTERANOMALY: mode = CM::MATRIX; m = glDeuteranomaly; break;
               case SVF_TRITANOPIA:    mode = CM::MATRIX; m = glTritanopia; break;
               case SVF_TRITANOMALY:   mode = CM::MATRIX; m = glTritanomaly; break;
               case SVF_ACHROMATOPSIA: mode = CM::MATRIX; m = glAchromatopsia; break;
               case SVF_ACHROMATOMALY: mode = CM::MATRIX; m = glAchromatomaly; break;

               default:
                  log.warning("Unrecognised colour matrix type '%s'", val.c_str());
                  FreeResource(fx);
                  return ERR_InvalidValue;
            }

            fx->set(FID_Mode, LONG(mode));
            if (mode IS CM::MATRIX) SetArray(fx, FID_Values|TDOUBLE, (APTR)m, CM_SIZE);
            break;
         }

         case SVF_VALUES: {
            auto m = read_array<DOUBLE>(val, CM_SIZE);
            SetArray(fx, FID_Values|TDOUBLE, m);
            break;
         }

         case SVF_X:      FUNIT(FID_X, val).set(fx); break;
         case SVF_Y:      FUNIT(FID_Y, val).set(fx); break;
         case SVF_WIDTH:  FUNIT(FID_Width, val).set(fx); break;
         case SVF_HEIGHT: FUNIT(FID_Height, val).set(fx); break;
         case SVF_IN:     parse_input(Self, fx, val, FID_SourceType, FID_Input); break;
         case SVF_RESULT: result_name = val; break;
      }
   }

   if (!fx->init()) {
      if (!result_name.empty()) parse_result(Self, fx, result_name);
      return ERR_Okay;
   }
   else {
      FreeResource(fx);
      return ERR_Init;
   }
}

//********************************************************************************************************************

static ERROR parse_fe_convolve_matrix(extSVG *Self, objVectorFilter *Filter, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_CONVOLVEFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   std::string result_name;
   for (unsigned a=1; a < Tag.Attribs.size(); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_ORDER: {
            DOUBLE ox = 0, oy = 0;
            read_numseq(val, { &ox, &oy });
            if (ox < 1) ox = 3;
            if (oy < 1) oy = ox;
            fx->setFields(fl::MatrixColumns(F2T(ox)), fl::MatrixRows(F2T(oy)));
            break;
         }

         case SVF_KERNELMATRIX: {
            #define MAX_DIM 9
            auto matrix = read_array<DOUBLE>(val, MAX_DIM * MAX_DIM);
            SetArray(fx, FID_Matrix|TDOUBLE, matrix);
            break;
         }

         case SVF_DIVISOR: {
            DOUBLE divisor = 0;
            read_numseq(val, { &divisor });
            fx->set(FID_Divisor, divisor);
            break;
         }

         case SVF_BIAS: {
            DOUBLE bias = 0;
            read_numseq(val, { &bias });
            fx->set(FID_Bias, bias);
            break;
         }

         case SVF_TARGETX: fx->set(FID_TargetX, StrToInt(val)); break;

         case SVF_TARGETY: fx->set(FID_TargetY, StrToInt(val)); break;

         case SVF_EDGEMODE:
            if (!StrMatch("duplicate", val)) fx->set(FID_EdgeMode, LONG(EM::DUPLICATE));
            else if (!StrMatch("wrap", val)) fx->set(FID_EdgeMode, LONG(EM::WRAP));
            else if (!StrMatch("none", val)) fx->set(FID_EdgeMode, LONG(EM::NONE));
            break;

         case SVF_KERNELUNITLENGTH: {
            DOUBLE kx = 1, ky = 1;
            read_numseq(val, { &kx, &ky });
            if (kx < 1) kx = 1;
            if (ky < 1) ky = kx;
            fx->set(FID_UnitX, kx);
            fx->set(FID_UnitY, ky);
            break;
         }

         // The modifications will apply to R,G,B only when preserveAlpha is true.
         case SVF_PRESERVEALPHA:
            fx->set(FID_PreserveAlpha, (!StrMatch("true", val)) or (!StrMatch("1", val)));
            break;

         case SVF_X:      FUNIT(FID_X, val).set(fx); break;
         case SVF_Y:      FUNIT(FID_Y, val).set(fx); break;
         case SVF_WIDTH:  FUNIT(FID_Width, val).set(fx); break;
         case SVF_HEIGHT: FUNIT(FID_Height, val).set(fx); break;
         case SVF_IN:     parse_input(Self, fx, val, FID_SourceType, FID_Input); break;
         case SVF_RESULT: result_name = val; break;
      }
   }

   if (!fx->init()) {
      if (!result_name.empty()) parse_result(Self, fx, result_name);
      return ERR_Okay;
   }
   else {
      FreeResource(fx);
      return ERR_Init;
   }
}

//********************************************************************************************************************

static ERROR parse_fe_lighting(extSVG *Self, svgState &State, objVectorFilter *Filter, XMLTag &Tag, LT Type)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_LIGHTINGFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   fx->set(FID_Type, LONG(Type));

   std::string result_name;
   for (unsigned a=1; a < Tag.Attribs.size(); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_LIGHTING_COLOUR:
         case SVF_LIGHTING_COLOR: {
            VectorPainter painter;
            if (!StrMatch("currentColor", val)) {
               FRGB rgb;
               if (!current_colour(Self, Self->Scene->Viewport, State, rgb)) SetArray(fx, FID_Colour|TFLOAT, &rgb, 4);
            }
            else if (!vecReadPainter(NULL, val.c_str(), &painter, NULL)) SetArray(fx, FID_Colour|TFLOAT, &painter.Colour, 4);
            break;
         }

         case SVF_KERNELUNITLENGTH: {
            DOUBLE kx = 1, ky = 1;
            read_numseq(val, { &kx, &ky });
            if (kx < 1) kx = 1;
            if (ky < 1) ky = kx;
            fx->set(FID_UnitX, kx);
            fx->set(FID_UnitY, ky);
            break;
         }

         case SVF_SPECULARCONSTANT:
         case SVF_DIFFUSECONSTANT:  FUNIT(FID_Constant, val).set(fx); break;
         case SVF_SURFACESCALE:     FUNIT(FID_Scale, val).set(fx); break;
         case SVF_SPECULAREXPONENT: FUNIT(FID_Exponent, val).set(fx); break;

         case SVF_X:      FUNIT(FID_X, val).set(fx); break;
         case SVF_Y:      FUNIT(FID_Y, val).set(fx); break;
         case SVF_WIDTH:  FUNIT(FID_Width, val).set(fx); break;
         case SVF_HEIGHT: FUNIT(FID_Height, val).set(fx); break;
         case SVF_IN:     parse_input(Self, fx, val, FID_SourceType, FID_Input); break;
         case SVF_RESULT: result_name = val; break;
         default:         log.warning("Unknown %s attribute %s", Tag.name(), Tag.Attribs[a].Name.c_str());
      }
   }

   // One child tag specifying the light source is required.

   if (!Tag.Children.empty()) {
      ERROR error;
      auto &child = Tag.Children[0];
      if (!StrCompare("feDistantLight", child.name(), 0, STR::WILDCARD)) {
         DOUBLE azimuth = 0, elevation = 0;

         for (LONG a=1; a < std::ssize(child.Attribs); a++) {
            switch(StrHash(child.Attribs[a].Name)) {
               case SVF_AZIMUTH:   azimuth   = StrToFloat(child.Attribs[a].Value); break;
               case SVF_ELEVATION: elevation = StrToFloat(child.Attribs[a].Value); break;
            }
         }

         error = ltSetDistantLight(fx, azimuth, elevation);
      }
      else if (!StrCompare("fePointLight", child.name(), 0, STR::WILDCARD)) {
         DOUBLE x = 0, y = 0, z = 0;

         for (LONG a=1; a < std::ssize(child.Attribs); a++) {
            switch(StrHash(child.Attribs[a].Name)) {
               case SVF_X: x = StrToFloat(child.Attribs[a].Value); break;
               case SVF_Y: y = StrToFloat(child.Attribs[a].Value); break;
               case SVF_Z: z = StrToFloat(child.Attribs[a].Value); break;
            }
         }

         error = ltSetPointLight(fx, x, y, z);
      }
      else if (!StrCompare("feSpotLight", child.name(), 0, STR::WILDCARD)) {
         DOUBLE x = 0, y = 0, z = 0, px = 0, py = 0, pz = 0;
         DOUBLE exponent = 1, cone_angle = 0;

         for (LONG a=1; a < std::ssize(child.Attribs); a++) {
            auto &val = child.Attribs[a].Value;
            switch(StrHash(child.Attribs[a].Name)) {
               case SVF_X:                 x = StrToFloat(val); break;
               case SVF_Y:                 y = StrToFloat(val); break;
               case SVF_Z:                 z = StrToFloat(val); break;
               case SVF_POINTSATX:         px = StrToFloat(val); break;
               case SVF_POINTSATY:         py = StrToFloat(val); break;
               case SVF_POINTSATZ:         pz = StrToFloat(val); break;
               case SVF_SPECULAREXPONENT:  exponent   = StrToFloat(val); break;
               case SVF_LIMITINGCONEANGLE: cone_angle = StrToFloat(val); break;
            }
         }

         error = ltSetSpotLight(fx, x, y, z, px, py, pz, exponent, cone_angle);
      }
      else {
         log.warning("Unrecognised %s child node '%s'", Tag.name(), child.name());
         error = ERR_Failed;
      }

      if (error) {
         FreeResource(fx);
         return error;
      }
   }

   if (!fx->init()) {
      if (!result_name.empty()) parse_result(Self, fx, result_name);
      return ERR_Okay;
   }
   else {
      FreeResource(fx);
      return ERR_Init;
   }
}

//********************************************************************************************************************

static ERROR parse_fe_displacement_map(extSVG *Self, objVectorFilter *Filter, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_DISPLACEMENTFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   std::string result_name;
   for (unsigned a=1; a < Tag.Attribs.size(); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_XCHANNELSELECTOR:
            switch(val[0]) {
               case 'r': case 'R': fx->set(FID_XChannel, LONG(CMP::RED)); break;
               case 'g': case 'G': fx->set(FID_XChannel, LONG(CMP::GREEN)); break;
               case 'b': case 'B': fx->set(FID_XChannel, LONG(CMP::BLUE)); break;
               case 'a': case 'A': fx->set(FID_XChannel, LONG(CMP::ALPHA)); break;
            }
            break;

         case SVF_YCHANNELSELECTOR:
            switch(val[0]) {
               case 'r': case 'R': fx->set(FID_YChannel, LONG(CMP::RED)); break;
               case 'g': case 'G': fx->set(FID_YChannel, LONG(CMP::GREEN)); break;
               case 'b': case 'B': fx->set(FID_YChannel, LONG(CMP::BLUE)); break;
               case 'a': case 'A': fx->set(FID_YChannel, LONG(CMP::ALPHA)); break;
            }
            break;

         case SVF_SCALE: fx->set(FID_Scale, StrToFloat(val)); break;

         case SVF_X:      FUNIT(FID_X, val).set(fx); break;
         case SVF_Y:      FUNIT(FID_Y, val).set(fx); break;
         case SVF_WIDTH:  FUNIT(FID_Width, val).set(fx); break;
         case SVF_HEIGHT: FUNIT(FID_Height, val).set(fx); break;

         case SVF_IN: parse_input(Self, fx, val, FID_SourceType, FID_Input); break;
         case SVF_IN2: parse_input(Self, fx, val, FID_MixType, FID_Mix); break;

         case SVF_RESULT: result_name = val; break;
      }
   }

   if (!fx->init()) {
      if (!result_name.empty()) parse_result(Self, fx, result_name);
      return ERR_Okay;
   }
   else {
      FreeResource(fx);
      return ERR_Init;
   }
}

//********************************************************************************************************************

static ERROR parse_fe_component_xfer(extSVG *Self, objVectorFilter *Filter, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_REMAPFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   std::string result_name;
   for (unsigned a=1; a < Tag.Attribs.size(); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_X:      FUNIT(FID_X, val).set(fx); break;
         case SVF_Y:      FUNIT(FID_Y, val).set(fx); break;
         case SVF_WIDTH:  FUNIT(FID_Width, val).set(fx); break;
         case SVF_HEIGHT: FUNIT(FID_Height, val).set(fx); break;
         case SVF_IN:     parse_input(Self, fx, val, FID_SourceType, FID_Input); break;
         case SVF_RESULT: result_name = val; break;
      }
   }

   for (auto &child : Tag.Children) {
      if (!StrCompare("feFunc?", child.name(), 0, STR::WILDCARD)) {
         auto cmp = CMP::NIL;
         switch(child.name()[6]) {
            case 'R': cmp = CMP::RED; break;
            case 'G': cmp = CMP::GREEN; break;
            case 'B': cmp = CMP::BLUE; break;
            case 'A': cmp = CMP::ALPHA; break;
            default:
               log.warning("Invalid feComponentTransfer element %s", child.name());
               return ERR_Failed;
         }

         ULONG type = 0;
         LONG mask = 0xff;
         DOUBLE amp = 1.0, offset = 0, exp = 1.0, slope = 1.0, intercept = 0.0;
         std::vector<DOUBLE> values;
         for (LONG a=1; a < LONG(child.Attribs.size()); a++) {
            switch(StrHash(child.Attribs[a].Name)) {
               case SVF_TYPE:        type = StrHash(child.Attribs[a].Value); break;
               case SVF_AMPLITUDE:   read_numseq(child.Attribs[a].Value, { &amp }); break;
               case SVF_INTERCEPT:   read_numseq(child.Attribs[a].Value, { &intercept }); break;
               case SVF_SLOPE:       read_numseq(child.Attribs[a].Value, { &slope }); break;
               case SVF_EXPONENT:    read_numseq(child.Attribs[a].Value, { &exp }); break;
               case SVF_OFFSET:      read_numseq(child.Attribs[a].Value, { &offset }); break;
               case SVF_MASK:        mask = StrToInt(child.Attribs[a].Value); break;
               case SVF_TABLEVALUES: {
                  values = read_array<DOUBLE>(child.Attribs[a].Value, 64);
                  break;
               }
               default: log.warning("Unknown %s attribute %s", child.name(), child.Attribs[a].Name.c_str()); break;
            }
         }

         switch(type) {
            case SVF_TABLE:    rfSelectTable(fx, cmp, values.data(), values.size()); break;
            case SVF_LINEAR:   rfSelectLinear(fx, cmp, slope, intercept);  break;
            case SVF_GAMMA:    rfSelectGamma(fx, cmp, amp, offset, exp);  break;
            case SVF_DISCRETE: rfSelectDiscrete(fx, cmp, values.data(), values.size());  break;
            case SVF_IDENTITY: rfSelectIdentity(fx, cmp); break;
            // The following additions are specific to Parasol and not SVG compatible.
            case SVF_INVERT:   rfSelectInvert(fx, cmp); break;
            case SVF_MASK:     rfSelectMask(fx, cmp, mask); break;
            default:
               log.warning("feComponentTransfer node failed to specify its type.");
               return ERR_UndefinedField;
         }
      }
      else log.warning("Unrecognised feComponentTransfer child node '%s'", child.name());
   }

   if (!fx->init()) {
      if (!result_name.empty()) parse_result(Self, fx, result_name);
      return ERR_Okay;
   }
   else {
      FreeResource(fx);
      return ERR_Init;
   }
}

//********************************************************************************************************************

static ERROR parse_fe_composite(extSVG *Self, objVectorFilter *Filter, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_COMPOSITEFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   std::string result_name;
   for (unsigned a=1; a < Tag.Attribs.size(); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_MODE:
         case SVF_OPERATOR: {
            switch (StrHash(val)) {
               // SVG Operator types
               case SVF_NORMAL:
               case SVF_OVER: fx->set(FID_Operator, LONG(OP::OVER)); break;
               case SVF_IN:   fx->set(FID_Operator, LONG(OP::IN)); break;
               case SVF_OUT:  fx->set(FID_Operator, LONG(OP::OUT)); break;
               case SVF_ATOP: fx->set(FID_Operator, LONG(OP::ATOP)); break;
               case SVF_XOR:  fx->set(FID_Operator, LONG(OP::XOR)); break;
               case SVF_ARITHMETIC: fx->set(FID_Operator, LONG(OP::ARITHMETIC)); break;
               // SVG Mode types
               case SVF_SCREEN:   fx->set(FID_Operator, LONG(OP::SCREEN)); break;
               case SVF_MULTIPLY: fx->set(FID_Operator, LONG(OP::MULTIPLY)); break;
               case SVF_LIGHTEN:  fx->set(FID_Operator, LONG(OP::LIGHTEN)); break;
               case SVF_DARKEN:   fx->set(FID_Operator, LONG(OP::DARKEN)); break;
               // Parasol modes
               case SVF_INVERTRGB:  fx->set(FID_Operator, LONG(OP::INVERT_RGB)); break;
               case SVF_INVERT:     fx->set(FID_Operator, LONG(OP::INVERT)); break;
               case SVF_CONTRAST:   fx->set(FID_Operator, LONG(OP::CONTRAST)); break;
               case SVF_DODGE:      fx->set(FID_Operator, LONG(OP::DODGE)); break;
               case SVF_BURN:       fx->set(FID_Operator, LONG(OP::BURN)); break;
               case SVF_HARDLIGHT:  fx->set(FID_Operator, LONG(OP::HARD_LIGHT)); break;
               case SVF_SOFTLIGHT:  fx->set(FID_Operator, LONG(OP::SOFT_LIGHT)); break;
               case SVF_DIFFERENCE: fx->set(FID_Operator, LONG(OP::DIFFERENCE)); break;
               case SVF_EXCLUSION:  fx->set(FID_Operator, LONG(OP::EXCLUSION)); break;
               case SVF_PLUS:       fx->set(FID_Operator, LONG(OP::PLUS)); break;
               case SVF_MINUS:      fx->set(FID_Operator, LONG(OP::MINUS)); break;
               case SVF_OVERLAY:    fx->set(FID_Operator, LONG(OP::OVERLAY)); break;
               default:
                  log.warning("Composite operator '%s' not recognised.", val.c_str());
                  FreeResource(fx);
                  return ERR_InvalidValue;
            }
            break;
         }

         case SVF_K1: {
            DOUBLE k1;
            read_numseq(val, { &k1 });
            fx->set(FID_K1, k1);
            break;
         }

         case SVF_K2: {
            DOUBLE k2;
            read_numseq(val, { &k2 });
            fx->set(FID_K2, k2);
            break;
         }

         case SVF_K3: {
            DOUBLE k3;
            read_numseq(val, { &k3 });
            fx->set(FID_K3, k3);
            break;
         }

         case SVF_K4: {
            DOUBLE k4;
            read_numseq(val, { &k4 });
            fx->set(FID_K4, k4);
            break;
         }

         case SVF_X:      FUNIT(FID_X, val).set(fx); break;
         case SVF_Y:      FUNIT(FID_Y, val).set(fx); break;
         case SVF_WIDTH:  FUNIT(FID_Width, val).set(fx); break;
         case SVF_HEIGHT: FUNIT(FID_Height, val).set(fx); break;
         case SVF_IN:     parse_input(Self, fx, val, FID_SourceType, FID_Input); break;
         case SVF_IN2:    parse_input(Self, fx, val, FID_MixType, FID_Mix); break;
         case SVF_RESULT: result_name = val; break;
      }
   }

   if (!fx->init()) {
      if (!result_name.empty()) parse_result(Self, fx, result_name);
      return ERR_Okay;
   }
   else {
      FreeResource(fx);
      return ERR_Init;
   }
}

//********************************************************************************************************************

static ERROR parse_fe_flood(extSVG *Self, svgState &State, objVectorFilter *Filter, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_FLOODFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   ERROR error = ERR_Okay;
   std::string result_name;
   for (unsigned a=1; (a < Tag.Attribs.size()) and (!error); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_FLOOD_COLOR:
         case SVF_FLOOD_COLOUR: {
            VectorPainter painter;
            if (!StrMatch("currentColor", val)) {
               if (!current_colour(Self, Self->Scene->Viewport, State, painter.Colour)) error = SetArray(fx, FID_Colour|TFLOAT, &painter.Colour, 4);
            }
            else if (!vecReadPainter(NULL, val.c_str(), &painter, NULL)) error = SetArray(fx, FID_Colour|TFLOAT, &painter.Colour, 4);
            break;
         }

         case SVF_FLOOD_OPACITY: {
            DOUBLE opacity;
            read_numseq(val, { &opacity });
            error = fx->set(FID_Opacity, opacity);
            break;
         }

         case SVF_X:      FUNIT(FID_X, val).set(fx); break;
         case SVF_Y:      FUNIT(FID_Y, val).set(fx); break;
         case SVF_WIDTH:  FUNIT(FID_Width, val).set(fx); break;
         case SVF_HEIGHT: FUNIT(FID_Height, val).set(fx); break;
         case SVF_IN:     parse_input(Self, fx, val, FID_SourceType, FID_Input); break;
         case SVF_RESULT: result_name = val; break;
      }
   }

   if (!fx->init()) {
      if (!result_name.empty()) parse_result(Self, fx, result_name);
      return ERR_Okay;
   }
   else {
      FreeResource(fx);
      return log.warning(error);
   }
}

//********************************************************************************************************************

static ERROR parse_fe_turbulence(extSVG *Self, objVectorFilter *Filter, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_TURBULENCEFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   std::string result_name;
   for (unsigned a=1; a < Tag.Attribs.size(); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_BASEFREQUENCY: {
            DOUBLE bfx = -1, bfy = -1;
            read_numseq(val, { &bfx, &bfy });
            if (bfx < 0) bfx = 0;
            if (bfy < 0) bfy = bfx;
            fx->setFields(fl::FX(bfx), fl::FY(bfy));
            break;
         }

         case SVF_NUMOCTAVES: fx->set(FID_Octaves, StrToInt(val)); break;

         case SVF_SEED: fx->set(FID_Seed, StrToInt(val)); break;

         case SVF_STITCHTILES:
            if (!StrMatch("stitch", val)) fx->set(FID_Stitch, TRUE);
            else fx->set(FID_Stitch, FALSE);
            break;

         case SVF_TYPE:
            if (!StrMatch("fractalNoise", val)) fx->set(FID_Type, LONG(TB::NOISE));
            else fx->set(FID_Type, 0);
            break;

         case SVF_X:      FUNIT(FID_X, val).set(fx); break;
         case SVF_Y:      FUNIT(FID_Y, val).set(fx); break;
         case SVF_WIDTH:  FUNIT(FID_Width, val).set(fx); break;
         case SVF_HEIGHT: FUNIT(FID_Height, val).set(fx); break;
         case SVF_IN:     parse_input(Self, fx, val, FID_SourceType, FID_Input); break;
         case SVF_RESULT: result_name = val; break;
      }
   }

   if (!fx->init()) {
      if (!result_name.empty()) parse_result(Self, fx, result_name);
      return ERR_Okay;
   }
   else {
      FreeResource(fx);
      return ERR_Init;
   }
}

//********************************************************************************************************************

static ERROR parse_fe_morphology(extSVG *Self, objVectorFilter *Filter, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_MORPHOLOGYFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   std::string result_name;
   for (unsigned a=1; a < Tag.Attribs.size(); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_RADIUS: {
            DOUBLE x = -1, y = -1;
            read_numseq(val, { &x, &y });
            if (x > 0) fx->set(FID_RadiusX, F2T(x));
            if (y > 0) fx->set(FID_RadiusY, F2T(y));
            break;
         }

         case SVF_OPERATOR: fx->set(FID_Operator, val); break;
         case SVF_X:      FUNIT(FID_X, val).set(fx); break;
         case SVF_Y:      FUNIT(FID_Y, val).set(fx); break;
         case SVF_WIDTH:  FUNIT(FID_Width, val).set(fx); break;
         case SVF_HEIGHT: FUNIT(FID_Height, val).set(fx); break;
         case SVF_IN:     parse_input(Self, fx, val, FID_SourceType, FID_Input); break;
         case SVF_RESULT: result_name = val; break;
      }
   }

   if (!fx->init()) {
      if (!result_name.empty()) parse_result(Self, fx, result_name);
      return ERR_Okay;
   }
   else {
      FreeResource(fx);
      return ERR_Init;
   }
}

//********************************************************************************************************************
// This code replaces feImage elements where the href refers to a resource name.

static ERROR parse_fe_source(extSVG *Self, svgState &State, objVectorFilter *Filter, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_SOURCEFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   bool required = false;
   std::string ref, result_name;

   ERROR error = ERR_Okay;
   for (unsigned a=1; a < Tag.Attribs.size(); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_X:      FUNIT(FID_X, val).set(fx); break;
         case SVF_Y:      FUNIT(FID_Y, val).set(fx); break;
         case SVF_WIDTH:  FUNIT(FID_Width, val).set(fx); break;
         case SVF_HEIGHT: FUNIT(FID_Height, val).set(fx); break;
         case SVF_PRESERVEASPECTRATIO: fx->set(FID_AspectRatio, LONG(parse_aspect_ratio(val))); break;
         case SVF_XLINK_HREF: ref = val; break;
         case SVF_EXTERNALRESOURCESREQUIRED: required = StrMatch("true", val) IS ERR_Okay; break;
         case SVF_RESULT: result_name = val; break;
      }
   }

   objVector *vector = NULL;
   if (!ref.empty()) {
      if (scFindDef(Self->Scene, ref.c_str(), (OBJECTPTR *)&vector) != ERR_Okay) {
         // The reference is not an existing vector but should be a pre-registered declaration that would allow
         // us to create it.  Note that creation only occurs once.  Subsequent use of the ID will result in the
         // live reference being found.

         if (auto tagref = find_href_tag(Self, ref)) {
            xtag_default(Self, State, *tagref, Self->Scene, vector);
         }
         else log.warning("Element id '%s' not found.", ref.c_str());
      }

      if (vector) {
         fx->set(FID_SourceName, ref);
         if (!(error = fx->init())) {
            if (!result_name.empty()) parse_result(Self, fx, result_name);
            return ERR_Okay;
         }
      }
      else error = ERR_Search;
   }
   else error = ERR_UndefinedField;

   FreeResource(fx);
   if (required) return log.warning(error);
   return ERR_Okay; // Default behaviour is not to force a failure despite the error.
}

//********************************************************************************************************************

static ERROR parse_fe_image(extSVG *Self, svgState &State, objVectorFilter *Filter, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   // Check if the client has specified an href that refers to a pattern name instead of an image file.  In that
   // case we need to divert to the SourceFX parser.

   for (unsigned a=1; a < Tag.Attribs.size(); a++) {
      if ((!StrMatch("xlink:href", Tag.Attribs[a].Name)) or (!StrMatch("href", Tag.Attribs[a].Name))) {
         if ((Tag.Attribs[a].Value[0] IS '#')) {
            return parse_fe_source(Self, State, Filter, Tag);
         }
         break;
      }
   }

   objFilterEffect *fx;
   if (NewObject(ID_IMAGEFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   bool image_required = false;
   std::string path;
   std::string result_name;

   for (unsigned a=1; a < Tag.Attribs.size(); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_X:      FUNIT(FID_X, val).set(fx); break;
         case SVF_Y:      FUNIT(FID_Y, val).set(fx); break;
         case SVF_WIDTH:  FUNIT(FID_Width, val).set(fx); break;
         case SVF_HEIGHT: FUNIT(FID_Height, val).set(fx); break;

         case SVF_IMAGE_RENDERING: {
            if (!StrMatch("optimizeSpeed", val)) fx->set(FID_ResampleMethod, LONG(VSM::BILINEAR));
            else if (!StrMatch("optimizeQuality", val)) fx->set(FID_ResampleMethod, LONG(VSM::LANCZOS3));
            else if (!StrMatch("auto", val));
            else if (!StrMatch("inherit", val));
            else log.warning("Unrecognised image-rendering option '%s'", val.c_str());
            break;
         }

         case SVF_PRESERVEASPECTRATIO: fx->set(FID_AspectRatio, LONG(parse_aspect_ratio(val))); break;

         case SVF_XLINK_HREF: path = val; break;

         case SVF_EXTERNALRESOURCESREQUIRED: // If true and the image cannot be loaded, return a fatal error code.
            if (!StrMatch("true", val)) image_required = true;
            break;

         case SVF_RESULT: result_name = val; break;
      }
   }

   if (!path.empty()) {
      // Check for security risks in the path.

      if ((path[0] IS '/') or ((path[0] IS '.') and (path[1] IS '.') and (path[2] IS '/'))) {
         FreeResource(fx);
         return log.warning(ERR_InvalidValue);
      }
      else {
         if (path.find(':') != std::string::npos) {
            FreeResource(fx);
            return log.warning(ERR_InvalidValue);
         }

         for (unsigned i=0; path[i]; i++) {
            if (path[i] IS '/') {
               while (path[i+1] IS '.') i++;
               if (path[i+1] IS '/') {
                  return log.warning(ERR_InvalidValue);
               }
            }
         }
      }

      if (auto fl = folder(Self)) {
         std::string comp_path = std::string(fl) + path;
         fx->set(FID_Path, comp_path);
      }
      else fx->set(FID_Path, path);
   }

   if (auto error = fx->init()) {
      FreeResource(fx);
      if (image_required) return error;
      else return ERR_Okay;
   }
   else {
      if (!result_name.empty()) parse_result(Self, fx, result_name);
      return ERR_Okay;
   }
}

//********************************************************************************************************************

static void xtag_filter(extSVG *Self, svgState &State, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   objVectorFilter *filter;
   std::string id;

   if (!NewObject(ID_VECTORFILTER, &filter)) {
      filter->setFields(fl::Owner(Self->Scene->UID), fl::Name("SVGFilter"),
         fl::Units(VUNIT::BOUNDING_BOX), fl::ColourSpace(VCS::LINEAR_RGB));

      for (unsigned a=1; a < Tag.Attribs.size(); a++) {
         auto &val = Tag.Attribs[a].Value;
         if (val.empty()) continue;

         LONG j;
         for (j=0; Tag.Attribs[a].Name[j] and (Tag.Attribs[a].Name[j] != ':'); j++);
         if (Tag.Attribs[a].Name[j] IS ':') continue;

         switch(StrHash(Tag.Attribs[a].Name)) {
            case SVF_FILTERUNITS:
               if (!StrMatch("userSpaceOnUse", val)) filter->Units = VUNIT::USERSPACE;
               else if (!StrMatch("objectBoundingBox", val)) filter->Units = VUNIT::BOUNDING_BOX;
               break;

            case SVF_ID:      if (add_id(Self, Tag, val)) id = val; break;

            case SVF_X:       FUNIT(FID_X, val).set(filter); break;
            case SVF_Y:       FUNIT(FID_Y, val).set(filter); break;
            case SVF_WIDTH:   FUNIT(FID_Width, val).set(filter); break;
            case SVF_HEIGHT:  FUNIT(FID_Height, val).set(filter); break;
            case SVF_OPACITY: FUNIT(FID_Opacity, val).set(filter); break;

            case SVF_FILTERRES: {
               DOUBLE x = 0, y = 0;
               read_numseq(val, { &x, &y });
               filter->setFields(fl::ResX(F2T(x)), fl::ResY(F2T(y)));
               break;
            }

            case SVF_COLOR_INTERPOLATION_FILTERS: // The default is linearRGB
               if (!StrMatch("auto", val)) filter->set(FID_ColourSpace, LONG(VCS::LINEAR_RGB));
               else if (!StrMatch("sRGB", val)) filter->set(FID_ColourSpace, LONG(VCS::SRGB));
               else if (!StrMatch("linearRGB", val)) filter->set(FID_ColourSpace, LONG(VCS::LINEAR_RGB));
               else if (!StrMatch("inherit", val)) filter->set(FID_ColourSpace, LONG(VCS::INHERIT));
               break;

            case SVF_PRIMITIVEUNITS:
               if (!StrMatch("userSpaceOnUse", val)) filter->PrimitiveUnits = VUNIT::USERSPACE; // Default
               else if (!StrMatch("objectBoundingBox", val)) filter->PrimitiveUnits = VUNIT::BOUNDING_BOX;
               break;

/*
            case SVF_VIEWBOX: {
               DOUBLE x=0, y=0, width=0, height=0;
               read_numseq(val, { &x, &y, &width, &height });
               filter->Viewport->setFields(fl::ViewX(x), fl::ViewY(y), fl::ViewWidth(width), fl::ViewHeight(height));
               break;
            }
*/
            default:
               log.warning("<%s> attribute '%s' unrecognised @ line %d", Tag.name(), Tag.Attribs[a].Name.c_str(), Tag.LineNo);
               break;
         }
      }

      if ((!id.empty()) and (!filter->init())) {
         SetName(filter, id.c_str());

         for (auto child : Tag.Children) {
            log.trace("Parsing filter element '%s'", child.name());

            switch(StrHash(child.name())) {
               case SVF_FEBLUR:              parse_fe_blur(Self, filter, child); break;
               case SVF_FEGAUSSIANBLUR:      parse_fe_blur(Self, filter, child); break;
               case SVF_FEOFFSET:            parse_fe_offset(Self, filter, child); break;
               case SVF_FEMERGE:             parse_fe_merge(Self, filter, child); break;
               case SVF_FECOLORMATRIX:       // American spelling
               case SVF_FECOLOURMATRIX:      parse_fe_colour_matrix(Self, filter, child); break;
               case SVF_FECONVOLVEMATRIX:    parse_fe_convolve_matrix(Self, filter, child); break;
               case SVF_FEDROPSHADOW:        log.warning("Support for feDropShadow not yet implemented."); break;
               case SVF_FEBLEND:             // Blend and composite share the same code.
               case SVF_FECOMPOSITE:         parse_fe_composite(Self, filter, child); break;
               case SVF_FEFLOOD:             parse_fe_flood(Self, State, filter, child); break;
               case SVF_FETURBULENCE:        parse_fe_turbulence(Self, filter, child); break;
               case SVF_FEMORPHOLOGY:        parse_fe_morphology(Self, filter, child); break;
               case SVF_FEIMAGE:             parse_fe_image(Self, State, filter, child); break;
               case SVF_FECOMPONENTTRANSFER: parse_fe_component_xfer(Self, filter, child); break;
               case SVF_FEDIFFUSELIGHTING:   parse_fe_lighting(Self, State, filter, child, LT::DIFFUSE); break;
               case SVF_FESPECULARLIGHTING:  parse_fe_lighting(Self, State, filter, child, LT::SPECULAR); break;
               case SVF_FEDISPLACEMENTMAP:   parse_fe_displacement_map(Self, filter, child); break;
               case SVF_FETILE:
                  log.warning("Filter element '%s' is not currently supported.", child.name());
                  break;

               default:
                  log.warning("Filter element '%s' not recognised.", child.name());
                  break;
            }
         }

         Self->Effects.clear();

         if (!Self->Cloning) scAddDef(Self->Scene, id.c_str(), filter);
      }
      else FreeResource(filter);
   }
}

//********************************************************************************************************************
// NB: In bounding-box mode, the default view-box is 0 0 1 1, where 1 is equivalent to 100% of the target space.
// If the client sets a custom view-box then the dimensions are fixed, and no scaling will apply.

static void process_pattern(extSVG *Self, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objVectorPattern *pattern;
   std::string id;

   if (!NewObject(ID_VECTORPATTERN, &pattern)) {
      SetOwner(pattern, Self->Scene);
      pattern->setFields(fl::Name("SVGPattern"),
         fl::Units(VUNIT::BOUNDING_BOX),
         fl::SpreadMethod(VSPREAD::REPEAT),
         fl::HostScene(Self->Scene));

      objVectorViewport *viewport;
      pattern->getPtr(FID_Viewport, &viewport);

      bool client_set_viewbox = false;
      for (unsigned a=1; a < Tag.Attribs.size(); a++) {
         auto &val = Tag.Attribs[a].Value;
         if (val.empty()) continue;

         LONG j;
         for (j=0; Tag.Attribs[a].Name[j] and (Tag.Attribs[a].Name[j] != ':'); j++);
         if (Tag.Attribs[a].Name[j] IS ':') continue;

         switch(StrHash(Tag.Attribs[a].Name)) {
            case SVF_PATTERNCONTENTUNITS:
               // SVG: "This attribute has no effect if viewbox is specified"
               // userSpaceOnUse: The user coordinate system for the contents of the ‘pattern’ element is the coordinate system that results from taking the current user coordinate system in place at the time when the ‘pattern’ element is referenced (i.e., the user coordinate system for the element referencing the ‘pattern’ element via a ‘fill’ or ‘stroke’ property) and then applying the transform specified by attribute ‘patternTransform’.
               // objectBoundingBox: The user coordinate system for the contents of the ‘pattern’ element is established using the bounding box of the element to which the pattern is applied (see Object bounding box units) and then applying the transform specified by attribute ‘patternTransform’.
               // The default is userSpaceOnUse

               if (!StrMatch("userSpaceOnUse", val)) pattern->ContentUnits = VUNIT::USERSPACE;
               else if (!StrMatch("objectBoundingBox", val)) pattern->ContentUnits = VUNIT::BOUNDING_BOX;
               break;

            case SVF_PATTERNUNITS:
               if (!StrMatch("userSpaceOnUse", val)) pattern->Units = VUNIT::USERSPACE;
               else if (!StrMatch("objectBoundingBox", val)) pattern->Units = VUNIT::BOUNDING_BOX;
               break;

            case SVF_PATTERNTRANSFORM: pattern->set(FID_Transform, val); break;

            case SVF_ID:       id = val; break;

            case SVF_OVERFLOW: viewport->set(FID_Overflow, val); break;

            case SVF_OPACITY:  FUNIT(FID_Opacity, val).set(pattern); break;
            case SVF_X:        FUNIT(FID_X, val).set(pattern); break;
            case SVF_Y:        FUNIT(FID_Y, val).set(pattern); break;
            case SVF_WIDTH:    FUNIT(FID_Width, val).set(pattern); break;
            case SVF_HEIGHT:   FUNIT(FID_Height, val).set(pattern); break;

            case SVF_VIEWBOX: {
               DOUBLE vx=0, vy=0, vwidth=1, vheight=1; // Default view-box for bounding-box mode
               client_set_viewbox = true;
               pattern->ContentUnits = VUNIT::USERSPACE;
               read_numseq(val, { &vx, &vy, &vwidth, &vheight });
               viewport->setFields(fl::ViewX(vx), fl::ViewY(vy), fl::ViewWidth(vwidth), fl::ViewHeight(vheight));
               break;
            }

            default:
               log.warning("%s attribute '%s' unrecognised @ line %d", Tag.name(), Tag.Attribs[a].Name.c_str(), Tag.LineNo);
               break;
         }
      }

      if (id.empty()) {
         FreeResource(pattern);
         log.trace("Failed to create a valid definition.");
      }

      /*if (!client_set_viewbox) {
         viewport->setFields(fl::ViewX(0), fl::ViewY(0), fl::ViewWidth(vwidth), fl::ViewHeight(vheight));
      }*/

      if (!InitObject(pattern)) {
         // Child vectors for the pattern need to be instantiated and belong to the pattern's Viewport.
         svgState state(Self);
         process_children(Self, state, Tag, viewport);

         if (!Self->Cloning) {
            add_id(Self, Tag, id);
            scAddDef(Self->Scene, id.c_str(), pattern);
         }
      }
      else {
         FreeResource(pattern);
         log.trace("Pattern initialisation failed.");
      }
   }
}

//********************************************************************************************************************

static ERROR process_shape(extSVG *Self, CLASSID VectorID, svgState &State, XMLTag &Tag,
   OBJECTPTR Parent, objVector * &Result)
{
   pf::Log log(__FUNCTION__);
   objVector *vector;

   Result = NULL;
   if (auto error = NewObject(VectorID, &vector); !error) {
      SetOwner(vector, Parent);
      svgState state = State;
      state.applyAttribs(vector);
      if (!Tag.Children.empty()) state.applyTag(Tag); // Apply all attribute values to the current state.

      process_attrib(Self, Tag, State, vector);

      if (!vector->init()) {
         // Process child tags, if any

         for (auto &child : Tag.Children) {
            if (child.isTag()) {
               switch(StrHash(child.name())) {
                  case SVF_ANIMATETRANSFORM: xtag_animatetransform(Self, child, vector); break;
                  case SVF_ANIMATEMOTION:    xtag_animatemotion(Self, child, vector); break;
                  case SVF_PARASOL_MORPH:    xtag_morph(Self, child, vector); break;
                  case SVF_TEXTPATH:
                     if (VectorID IS ID_VECTORTEXT) {
                        if (!child.Children.empty()) {
                           auto buffer = child.getContent();
                           if (!buffer.empty()) {
                              pf::ltrim(buffer);
                              vector->set(FID_String, buffer);
                           }
                           else log.msg("Failed to retrieve content for <text> @ line %d", Tag.LineNo);
                        }

                        xtag_morph(Self, child, vector);
                     }
                     break;
                  default:
                     log.warning("Failed to interpret vector child element <%s/> @ line %d", child.name(), child.LineNo);
                     break;
               }
            }
         }

         Result = vector;
         return error;
      }
      else {
         FreeResource(vector);
         return ERR_Init;
      }
   }
   else return ERR_CreateObject;
}

//********************************************************************************************************************
// See also process_children()

static ERROR xtag_default(extSVG *Self, svgState &State, XMLTag &Tag, OBJECTPTR Parent, objVector * &Vector)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("%s", Tag.name());

   switch(StrHash(Tag.name())) {
      case SVF_USE:              xtag_use(Self, State, Tag, Parent); break;
      case SVF_G:                xtag_group(Self, State, Tag, Parent, Vector); break;
      case SVF_SVG:              xtag_svg(Self, State, Tag, Parent, Vector); break;
      case SVF_RECT:             process_shape(Self, ID_VECTORRECTANGLE, State, Tag, Parent, Vector); break;
      case SVF_ELLIPSE:          process_shape(Self, ID_VECTORELLIPSE, State, Tag, Parent, Vector); break;
      case SVF_CIRCLE:           process_shape(Self, ID_VECTORELLIPSE, State, Tag, Parent, Vector); break;
      case SVF_PATH:             process_shape(Self, ID_VECTORPATH, State, Tag, Parent, Vector); break;
      case SVF_POLYGON:          process_shape(Self, ID_VECTORPOLYGON, State, Tag, Parent, Vector); break;
      case SVF_PARASOL_SPIRAL:   process_shape(Self, ID_VECTORSPIRAL, State, Tag, Parent, Vector); break;
      case SVF_PARASOL_WAVE:     process_shape(Self, ID_VECTORWAVE, State, Tag, Parent, Vector); break;
      case SVF_PARASOL_SHAPE:    process_shape(Self, ID_VECTORSHAPE, State, Tag, Parent, Vector); break;
      case SVF_IMAGE:            xtag_image(Self, State, Tag, Parent, Vector); break;
      case SVF_CONTOURGRADIENT:  xtag_contourgradient(Self, Tag); break;
      case SVF_RADIALGRADIENT:   xtag_radialgradient(Self, Tag); break;
      case SVF_DIAMONDGRADIENT:  xtag_diamondgradient(Self, Tag); break;
      case SVF_CONICGRADIENT:    xtag_conicgradient(Self, Tag); break;
      case SVF_LINEARGRADIENT:   xtag_lineargradient(Self, Tag); break;
      case SVF_SYMBOL:           xtag_symbol(Self, Tag); break;
      case SVF_ANIMATETRANSFORM: xtag_animatetransform(Self, Tag, Parent); break;
      case SVF_FILTER:           xtag_filter(Self, State, Tag); break;
      case SVF_DEFS:             xtag_defs(Self, State, Tag, Parent); break;
      case SVF_CLIPPATH:         xtag_clippath(Self, Tag); break;
      case SVF_MASK:             xtag_mask(Self, Tag); break;
      case SVF_STYLE:            xtag_style(Self, Tag); break;
      case SVF_PATTERN:          process_pattern(Self, Tag); break;

      case SVF_TITLE:
         if (Self->Title) { FreeResource(Self->Title); Self->Title = NULL; }
         if (!Tag.Children.empty()) {
            if (auto buffer = Tag.getContent(); !buffer.empty()) {
               pf::ltrim(buffer);
               Self->Title = StrClone(buffer.c_str());
            }
         }
         break;

      case SVF_LINE:
         process_shape(Self, ID_VECTORPOLYGON, State, Tag, Parent, Vector);
         Vector->set(FID_Closed, FALSE);
         break;

      case SVF_POLYLINE:
         process_shape(Self, ID_VECTORPOLYGON, State, Tag, Parent, Vector);
         Vector->set(FID_Closed, FALSE);
         break;

      case SVF_TEXT: {
         if (!process_shape(Self, ID_VECTORTEXT, State, Tag, Parent, Vector)) {
            if (!Tag.Children.empty()) {
               STRING existing_str = NULL;
               Vector->get(FID_String, &existing_str);

               if (auto buffer = Tag.getContent(); !buffer.empty()) {
                  pf::ltrim(buffer);
                  if (existing_str) buffer.insert(0, existing_str);
                  Vector->set(FID_String, buffer);
               }
               else log.msg("Failed to retrieve content for <text> @ line %d", Tag.LineNo);
            }
         }
         break;
      }

      case SVF_DESC: break; // Ignore descriptions

      default: log.warning("Failed to interpret tag <%s/> @ line %d", Tag.name(), Tag.LineNo); return ERR_NoSupport;
   }

   return ERR_Okay;
}

//********************************************************************************************************************
// The Width/Height can be zero if the original image dimensions are desired.

static ERROR load_pic(extSVG *Self, std::string Path, objPicture **Picture)
{
   pf::Log log(__FUNCTION__);

   *Picture = NULL;
   objFile *file = NULL;
   auto val = Path.c_str();

   ERROR error = ERR_Okay;
   if (!StrCompare("icons:", val, 5)) {
      // Parasol feature: Load an SVG image from the icon database.  Nothing needs to be done here
      // because the FS volume is built-in.
   }
   else if (!StrCompare("data:", val, 5)) { // Check for embedded content
      log.branch("Detected embedded source data");
      val += 5;
      if (!StrCompare("image/", val, 6)) { // Has to be an image type
         val += 6;
         while ((*val) and (*val != ';')) val++;
         if (!StrCompare(";base64", val, 7)) { // Is it base 64?
            val += 7;
            while ((*val) and (*val != ',')) val++;
            if (*val IS ',') val++;

            pfBase64Decode state;
            ClearMemory(&state, sizeof(state));

            UBYTE *output;
            LONG size = strlen(val);
            if (!AllocMemory(size, MEM::DATA|MEM::NO_CLEAR, &output)) {
               LONG written;
               if (!(error = Base64Decode(&state, val, size, output, &written))) {
                  Path = "temp:svg.img";
                  if ((file = objFile::create::integral(fl::Path(Path), fl::Flags(FL::NEW|FL::WRITE)))) {
                     LONG result;
                     file->write(output, written, &result);
                  }
                  else error = ERR_File;
               }

               FreeResource(output);
            }
            else error = ERR_AllocMemory;
         }
         else error = ERR_StringFormat;
      }
      else error = ERR_StringFormat;
   }
   else log.branch("%s", Path.c_str());

   if (!error) {
      if (!(*Picture = objPicture::create::global(
         fl::Owner(Self->Scene->UID),
         fl::Path(Path),
         fl::BitsPerPixel(32),
         fl::Flags(PCF::FORCE_ALPHA_32)))) error = ERR_CreateObject;
   }

   if (file) {
      flDelete(file, 0);
      FreeResource(file);
   }

   if (error) log.warning(error);
   return error;
}

//********************************************************************************************************************
// Definition images are stored once, allowing them to be used multiple times via Fill and Stroke references.

static void def_image(extSVG *Self, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objVectorImage *image;
   std::string id, src;
   FUNIT width, height;

   if (!NewObject(ID_VECTORIMAGE, &image)) {
      image->setFields(fl::Owner(Self->Scene->UID),
         fl::Name("SVGImage"),
         fl::Units(VUNIT::BOUNDING_BOX),
         fl::SpreadMethod(VSPREAD::PAD));

      for (unsigned a=1; a < Tag.Attribs.size(); a++) {
         auto &val = Tag.Attribs[a].Value;
         if (val.empty()) continue;

         switch(StrHash(Tag.Attribs[a].Name)) {
            case SVF_UNITS:
               if (!StrMatch("userSpaceOnUse", val)) image->Units = VUNIT::USERSPACE;
               else if (!StrMatch("objectBoundingBox", val)) image->Units = VUNIT::BOUNDING_BOX;
               else log.warning("Unknown <image> units reference '%s'", val.c_str());
               break;

            case SVF_XLINK_HREF: src = val; break;
            case SVF_ID:     id = val; break;
            case SVF_X:      FUNIT(FID_X, val).set(image); break;
            case SVF_Y:      FUNIT(FID_Y, val).set(image); break;
            case SVF_WIDTH:  width = FUNIT(val); break;
            case SVF_HEIGHT: height = FUNIT(val); break;
            default: {
               // Check if this was a reference to some other namespace (ignorable).
               LONG i;
               for (i=0; val[i] and (val[i] != ':'); i++);
               if (val[i] != ':') log.warning("Failed to parse attrib '%s' in <image/> tag @ line %d", Tag.Attribs[a].Name.c_str(), Tag.LineNo);
               break;
            }
         }
      }

      if ((!id.empty()) and (!src.empty())) {
         objPicture *pic;
         if (!load_pic(Self, src, &pic)) {
            image->set(FID_Picture, pic);
            if (!InitObject(image)) {
               if (!Self->Cloning) {
                  add_id(Self, Tag, id);
                  scAddDef(Self->Scene, id.c_str(), image);
               }
            }
            else {
               FreeResource(image);
               log.trace("Picture initialisation failed.");
            }
         }
         else {
            FreeResource(image);
            log.trace("Unable to load a picture for <image/> '%s' at line %d", id.c_str(), Tag.LineNo);
         }
      }
      else {
         FreeResource(image);
         log.trace("No id or src specified in <image/> at line %d", Tag.LineNo);
      }
   }
}

//********************************************************************************************************************

static ERROR xtag_image(extSVG *Self, svgState &State, XMLTag &Tag, OBJECTPTR Parent, objVector * &Vector)
{
   pf::Log log(__FUNCTION__);

   std::string src, filter, transform, id;
   ARF ratio = ARF::NIL;
   FUNIT x, y, width, height;

   for (LONG a=1; a < std::ssize(Tag.Attribs); a++) {
      auto &name = Tag.Attribs[a].Name;
      auto &value = Tag.Attribs[a].Value;
      if ((!StrMatch("xlink:href", name)) or (!StrMatch("href", name))) {
         src = value;
      }
      else if (!StrMatch("preserveAspectRatio", name)) {
         ratio = parse_aspect_ratio(value);
      }
      else if (!StrMatch("x", name)) {
         x = FUNIT(FID_X, value);
      }
      else if (!StrMatch("y", name)) {
         y = FUNIT(FID_Y, value);
      }
      else if (!StrMatch("width", name)) {
         width = FUNIT(FID_Width, value);
         if (!width.valid_size()) return log.warning(ERR_InvalidDimension);
      }
      else if (!StrMatch("height", name)) {
         height = FUNIT(FID_Height, value);
         if (!height.valid_size()) return log.warning(ERR_InvalidDimension);
      }
      else if (!StrMatch("id", name)) id = value;
      else if (!StrMatch("transform", name)) transform = value;
      else if (!StrMatch("crossorigin", name)); // Defines the value of the credentials flag for CORS requests.
      else if (!StrMatch("decoding", name)); // Hint as to whether image decoding is synchronous or asynchronous
      else if (!StrMatch("clip", name)); // Deprecated from SVG; allows a rect() to be declared that functions as a clip-path
   }

   if (src.empty()) return ERR_FieldNotSet;

   if (id.empty()) {
      // An image always has an ID; this ensures that if the image bitmap is referenced repeatedly via a <symbol> then
      // we won't keep reloading it into the cache.
      id = "img_" + std::to_string(StrHash(src));
      xmlNewAttrib(Tag, "id", id);
   }

   if (add_id(Self, Tag, id)) {
      // Load the image and add it to the vector definition.  It will be rendered as a rectangle within the scene.
      // This may appear a little confusing because an image can be invoked in SVG like a first-class shape; however to
      // do so would be inconsistent with all other scene graph members being true path-based objects.

      objPicture *pic = NULL;
      load_pic(Self, src, &pic);

      if (pic) {
         if (auto image = objVectorImage::create::global(
               fl::Owner(Self->Scene->UID),
               fl::Picture(pic),
               fl::SpreadMethod(VSPREAD::PAD),
               fl::Units(VUNIT::BOUNDING_BOX),
               fl::AspectRatio(ratio))) {

            SetOwner(pic, image); // It's best if the pic belongs to the image.

            scAddDef(Self->Scene, id.c_str(), image);
         }
         else return ERR_CreateObject;
      }
      else log.warning("Failed to load picture via xlink:href.");
   }
   
   if (auto error = NewObject(ID_VECTORRECTANGLE, &Vector); !error) {
      SetOwner(Vector, Parent);
      State.applyAttribs(Vector);

      // All attributes of <image> will be applied to the rectangle.

      process_attrib(Self, Tag, State, Vector);

      if (!x.empty()) x.set(Vector);
      if (!y.empty()) y.set(Vector);
      if (!width.empty()) width.set(Vector);
      if (!height.empty()) height.set(Vector);

      Vector->set(FID_Fill, "url(#" + id + ")");

      if (!Vector->init()) {
         return ERR_Okay;
      }
      else {
         FreeResource(Vector);
         return ERR_Init;
      }
   }
   else return ERR_CreateObject;
}

//********************************************************************************************************************

static ERROR xtag_defs(extSVG *Self, svgState &State, XMLTag &Tag, OBJECTPTR Parent)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("Tag: %d", Tag.ID);

   auto state = State;
   state.applyTag(Tag); // Apply all attribute values to the current state.

   for (auto &child : Tag.Children) {
      switch (StrHash(child.name())) {
         case SVF_CONTOURGRADIENT: xtag_contourgradient(Self, child); break;
         case SVF_RADIALGRADIENT:  xtag_radialgradient(Self, child); break;
         case SVF_DIAMONDGRADIENT: xtag_diamondgradient(Self, child); break;
         case SVF_CONICGRADIENT:   xtag_conicgradient(Self, child); break;
         case SVF_LINEARGRADIENT:  xtag_lineargradient(Self, child); break;
         case SVF_PATTERN:         process_pattern(Self, child); break;
         case SVF_IMAGE:           def_image(Self, child); break;
         case SVF_FILTER:          xtag_filter(Self, state, child); break;
         case SVF_CLIPPATH:        xtag_clippath(Self, child); break;
         case SVF_MASK:            xtag_mask(Self, child); break;
         case SVF_PARASOL_TRANSITION: xtag_pathtransition(Self, child); break;

         default: {
            // Anything not immediately recognised is added to the dictionary if it has an 'id' attribute.
            // No object is instantiated -- this is left to the referencee.
            for (unsigned a=1; a < child.Attribs.size(); a++) {
               if (!StrMatch("id", child.Attribs[a].Name)) {
                  add_id(Self, child, child.Attribs[a].Value);
                  break;
               }
            }
            break;
         }
      }
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR xtag_style(extSVG *Self, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   ERROR error = ERR_Okay;

   if (!Self->XML) {
      // The application of CSS styles is possible on initial loading of the document, but not in post-processing
      // once the XML object has been abandoned.
      log.warning("Unable to apply CSS style-sheet, XML object already terminated.");
      return ERR_Failed;
   }

   for (auto &a : Tag.Attribs) {
      if (!StrMatch("type", a.Name)) {
         if (StrMatch("text/css", a.Value)) {
            log.warning("Unsupported stylesheet '%s'", a.Value.c_str());
            return ERR_NoSupport;
         }
         break;
      }
   }

   // Parse the CSS using the Katana Parser.

   auto css_buffer = Tag.getContent();
   if (auto css = katana_parse(css_buffer.c_str(), css_buffer.size(), KatanaParserModeStylesheet)) {
      /*#ifdef _DEBUG
         Self->CSS->mode = KatanaParserModeStylesheet;
         katana_dump_output(css);
      #endif*/

      // For each rule in the stylesheet, apply them to the loaded XML document by injecting tags and attributes.
      // The stylesheet attributes have precedence over inline tag attributes (therefore we can overwrite matching
      // attribute names) however they are outranked by inline styles.

      KatanaStylesheet *sheet = css->stylesheet;

      log.msg("%d CSS rules will be applied", sheet->imports.length + sheet->rules.length);

      for (unsigned i=0; i < sheet->imports.length; ++i) {
         if (sheet->imports.data[i])
            process_rule(Self, Self->XML->Tags, (KatanaRule *)sheet->imports.data[i]);
      }

      for (unsigned i=0; i < sheet->rules.length; ++i) {
         if (sheet->rules.data[i])
            process_rule(Self, Self->XML->Tags, (KatanaRule *)sheet->rules.data[i]);
      }

      katana_destroy_output(css);
   }

   return error;
}

//********************************************************************************************************************
// Declare a 'symbol' which is basically a template for inclusion elsewhere through the use of a 'use' element.
//
// When a use element is encountered, it looks for the associated symbol ID and then processes the XML child tags that
// belong to it.

static void xtag_symbol(extSVG *Self, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   log.traceBranch("Tag: %d", Tag.ID);

   for (auto &a : Tag.Attribs) {
      if (!StrMatch("id", a.Name)) {
         add_id(Self, Tag, a.Value);
         return;
      }
   }

   log.warning("No id attribute specified in <symbol> at line %d.", Tag.LineNo);
}

//********************************************************************************************************************
// Most vector shapes can be morphed to the path of another vector.

static void xtag_morph(extSVG *Self, XMLTag &Tag, OBJECTPTR Parent)
{
   pf::Log log(__FUNCTION__);

   if ((!Parent) or (Parent->Class->BaseClassID != ID_VECTOR)) {
      log.traceWarning("Unable to apply morph to non-vector parent object.");
      return;
   }

   // Find the definition that is being referenced for the morph.

   std::string offset;
   std::string ref;
   std::string transition;
   VMF flags = VMF::NIL;
   ARF align = ARF::NIL;
   for (unsigned a=1; a < Tag.Attribs.size(); a++) {
      auto &val = Tag.Attribs[a].Value;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_PATH:
         case SVF_XLINK_HREF:  ref = val; break;
         case SVF_TRANSITION:  transition = val; break;
         case SVF_STARTOFFSET: offset = val; break;
         case SVF_METHOD:
            if (!StrMatch("align", val)) flags &= ~VMF::STRETCH;
            else if (!StrMatch("stretch", val)) flags |= VMF::STRETCH;
            break;

         case SVF_SPACING:
            if (!StrMatch("auto", val)) flags |= VMF::AUTO_SPACING;
            else if (!StrMatch("exact", val)) flags &= ~VMF::AUTO_SPACING;
            break;

         case SVF_ALIGN:
            align |= parse_aspect_ratio(val);
            break;
      }
   }

   if (ref.empty()) {
      log.warning("<morph> element @ line %d is missing a valid xlink:href attribute.", Tag.LineNo);
      return;
   }

   // Find the matching element with matching ID

   auto uri = uri_name(ref);
   if (uri.empty()) {
      log.warning("Invalid URI string '%s' at line %d", ref.c_str(), Tag.LineNo);
      return;
   }

   if (!Self->IDs.contains(uri)) {
      log.warning("Unable to find element '%s' referenced at line %d", ref.c_str(), Tag.LineNo);
      return;
   }

   OBJECTPTR transvector = NULL;
   if (!transition.empty()) {
      if (scFindDef(Self->Scene, transition.c_str(), &transvector)) {
         log.warning("Unable to find element '%s' referenced at line %d", transition.c_str(), Tag.LineNo);
         return;
      }
   }

   auto &tagref = Self->IDs[uri];

   CLASSID class_id = 0;
   switch (StrHash(tagref.name())) {
      case SVF_PATH:           class_id = ID_VECTORPATH; break;
      case SVF_RECT:           class_id = ID_VECTORRECTANGLE; break;
      case SVF_ELLIPSE:        class_id = ID_VECTORELLIPSE; break;
      case SVF_CIRCLE:         class_id = ID_VECTORELLIPSE; break;
      case SVF_POLYGON:        class_id = ID_VECTORPOLYGON; break;
      case SVF_PARASOL_SPIRAL: class_id = ID_VECTORSPIRAL; break;
      case SVF_PARASOL_WAVE:   class_id = ID_VECTORWAVE; break;
      case SVF_PARASOL_SHAPE:  class_id = ID_VECTORSHAPE; break;
      default:
         log.warning("Invalid reference '%s', '%s' is not recognised by <morph>.", ref.c_str(), tagref.name());
   }

   if ((flags & (VMF::Y_MIN|VMF::Y_MID|VMF::Y_MAX)) IS VMF::NIL) {
      if (Parent->Class->ClassID IS ID_VECTORTEXT) flags |= VMF::Y_MIN;
      else flags |= VMF::Y_MID;
   }

   if (class_id) {
      objVector *shape;
      svgState state(Self);
      process_shape(Self, class_id, state, tagref, Self->Scene, shape);
      Parent->set(FID_Morph, shape);
      if (transvector) Parent->set(FID_Transition, transvector);
      Parent->set(FID_MorphFlags, LONG(flags));
      if (!Self->Cloning) scAddDef(Self->Scene, uri.c_str(), shape);
   }
}

//********************************************************************************************************************
// Duplicates a referenced area of the SVG definition.
//
// "The effect of a 'use' element is as if the contents of the referenced element were deeply cloned into a separate
// non-exposed DOM tree which had the 'use' element as its parent and all of the 'use' element's ancestors as its
// higher-level ancestors.

static void xtag_use(extSVG *Self, svgState &State, XMLTag &Tag, OBJECTPTR Parent)
{
   pf::Log log(__FUNCTION__);
   std::string ref;

   for (LONG a=1; (a < std::ssize(Tag.Attribs)) and ref.empty(); a++) {
      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_HREF: // SVG2
         case SVF_XLINK_HREF: ref = Tag.Attribs[a].Value; break;
      }
   }

   if (ref.empty()) {
      log.warning("<use> element @ line %d is missing a valid href attribute.", Tag.LineNo);
      return;
   }

   // Find the matching element with matching ID

   auto tagref = find_href_tag(Self, ref);
   if (!tagref) {
      log.warning("Unable to find element '%s'", ref.c_str());
      return;
   }

   objVector *viewport = NULL;

   auto state = State;
   state.applyTag(Tag); // Apply all attribute values to the current state.

   // Increment the Cloning variable to indicate that we are in a region that is being cloned.
   // This is important for some elements like clip-path, whereby the path only needs to be created
   // once and can then be referenced multiple times.

   Self->Cloning++;
   auto dc = deferred_call([&Self] {
      Self->Cloning--;
   });

   if ((!StrMatch("symbol", tagref->name())) or (!StrMatch("svg", tagref->name()))) {
      // SVG spec requires that we create a VectorGroup and then create a Viewport underneath that.  However if there
      // are no attributes to apply to the group then there is no sense in creating an empty one.

      objVector *group;
      bool need_group = false;
      for (LONG a=1; (a < std::ssize(Tag.Attribs)) and (!need_group); a++) {
         switch(StrHash(Tag.Attribs[a].Name)) {
            case SVF_X: case SVF_Y: case SVF_WIDTH: case SVF_HEIGHT: break;
            default: need_group = true; break;
         }
      }

      if (need_group) {
         if (!NewObject(ID_VECTORGROUP, &group)) {
            SetOwner(group, Parent);
            Parent = group;
            group->init();
         }
      }

      if (NewObject(ID_VECTORVIEWPORT, &viewport)) return;
      SetOwner(viewport, Parent);
      viewport->setFields(fl::Width(SCALE(1.0)), fl::Height(SCALE(1.0))); // SVG default

      // Apply attributes from 'use' to the group and/or viewport
      for (unsigned a=1; a < Tag.Attribs.size(); a++) {
         auto &val = Tag.Attribs[a].Value;
         if (val.empty()) continue;

         auto hash = StrHash(Tag.Attribs[a].Name);
         switch(hash) {
            // X,Y,Width,Height are applied to the viewport
            case SVF_X:      FUNIT(FID_X, val).set(viewport); break;
            case SVF_Y:      FUNIT(FID_Y, val).set(viewport); break;
            case SVF_WIDTH:  FUNIT(FID_Width, val).set(viewport); break;
            case SVF_HEIGHT: FUNIT(FID_Height, val).set(viewport); break;

            // All other attributes are applied to the 'g' element
            default:
               if (group) set_property(Self, group, hash, Tag, State, val);
               else set_property(Self, viewport, hash, Tag, State, val);
               break;
         }
      }

      // Apply attributes from the symbol itself to the viewport

      for (unsigned a=1; a < tagref->Attribs.size(); a++) {
         auto &val = tagref->Attribs[a].Value;
         if (val.empty()) continue;

         switch(StrHash(tagref->Attribs[a].Name)) {
            case SVF_X:      FUNIT(FID_X, val).set(viewport); break;
            case SVF_Y:      FUNIT(FID_Y, val).set(viewport); break;
            case SVF_WIDTH:  FUNIT(FID_Width, val).set(viewport); break;
            case SVF_HEIGHT: FUNIT(FID_Height, val).set(viewport); break;
            case SVF_VIEWBOX:  {
               DOUBLE x=0, y=0, width=0, height=0;
               read_numseq(val, { &x, &y, &width, &height });
               viewport->setFields(fl::ViewX(x), fl::ViewY(y), fl::ViewWidth(width), fl::ViewHeight(height));
               break;
            }
            case SVF_ID: break; // Ignore (already processed).
            default: log.warning("Not processing attribute '%s'", tagref->Attribs[a].Name.c_str()); break;
         }
      }

      if (viewport->init() != ERR_Okay) { FreeResource(viewport); return; }

      // Add all child elements in <symbol> to the viewport.  Some state values have to be reset here because
      // they have already been applied to the viewport and will be inherited via that route.

      state.m_opacity = 1.0;

      log.traceBranch("Processing all child elements within %s", ref.c_str());
      process_children(Self, state, *tagref, viewport);
   }
   else {
      // W3C: In the generated content, the ‘use’ will be replaced by ‘g’, where all attributes from the ‘use’ element 
      // except for ‘x’, ‘y’, ‘width’, ‘height’ and ‘xlink:href’ are transferred to the generated ‘g’ element. An 
      // additional transformation translate(x,y) is appended to the end (i.e., right-side) of the ‘transform’ 
      // attribute on the generated ‘g’, where x and y represent the values of the ‘x’ and ‘y’ attributes on the 
      // ‘use’ element. The referenced object and its contents are deep-cloned into the generated tree.

      objVector *group;
      if (!NewObject(ID_VECTORGROUP, &group)) {
         SetOwner(group, Parent);
         SetName(group, "UseElement");

         state.applyTag(Tag); // Apply supported attribute values to the current state.

         // Apply 'use' attributes to the group.

         FUNIT tx, ty;
         for (unsigned t=1; t < Tag.Attribs.size(); t++) {
            if (Tag.Attribs[t].Value.empty()) continue;

            // Ignore unrecognised namespaces, e.g. 'inkscape:dx'
            if (Tag.Attribs[t].Name.find(':') != std::string::npos) continue;           

            auto hash = StrHash(Tag.Attribs[t].Name);
            switch (hash) {
               case SVF_X: tx = FUNIT(FID_X, Tag.Attribs[t].Value); break;
               case SVF_Y: ty = FUNIT(FID_Y, Tag.Attribs[t].Value); break;
               // SVG states that the following are not to be applied to the group...
               case SVF_WIDTH:
               case SVF_HEIGHT:
               case SVF_XLINK_HREF:
               case SVF_HREF:
                  break;

               default:
                  if (auto error = set_property(Self, group, hash, Tag, State, Tag.Attribs[t].Value)) {
                     log.warning("Failed to apply %s=%s to <use> group: %s", Tag.Attribs[t].Name.c_str(), Tag.Attribs[t].Value.c_str(), GetErrorMsg(error));
                  }
            }
         }

         if ((!tx.empty()) or (!ty.empty())) {
            parse_transform(group, "translate(" + std::to_string(tx) + " " + std::to_string(ty) + ")");
         }

         if (group->init() != ERR_Okay) { FreeResource(group); return; }

         // Perform the deep-clone as stipulated by W3C.  Generated objects will inherit attributes from the group.
         log.branch("Duplicating tags at %s", ref.c_str());
         objVector *sibling = NULL;
         xtag_default(Self, state, *tagref, group, sibling);
      }
   }
}

//********************************************************************************************************************

static void xtag_group(extSVG *Self, svgState &State, XMLTag &Tag, OBJECTPTR Parent, objVector * &Vector)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("Tag: %d", Tag.ID);

   auto state = State;

   objVector *group;
   if (NewObject(ID_VECTORGROUP, &group) != ERR_Okay) return;
   SetOwner(group, Parent);
   if (!Tag.Children.empty()) state.applyTag(Tag); // Apply all group attribute values to the current state.
   process_attrib(Self, Tag, State, group);

   // Process child tags

   objVector *sibling = NULL;
   for (auto &child : Tag.Children) {
      if (child.isTag()) {
         xtag_default(Self, state, child, group, sibling);
      }
   }

   if (!group->init()) Vector = group;
   else FreeResource(group);
}

//********************************************************************************************************************
// <svg/> tags can be embedded inside <svg/> tags - this establishes a new viewport.
// Refer to section 7.9 of the SVG Specification for more information.

static void xtag_svg(extSVG *Self, svgState &State, XMLTag &Tag, OBJECTPTR Parent, objVector * &Vector)
{
   pf::Log log(__FUNCTION__);
   LONG a;

   if (!Parent) {
      log.warning("A Parent object is required.");
      return;
   }

   // If initialising to a VectorScene, prefer to use its existing viewport if there is one.

   objVectorViewport *viewport;
   if ((Parent->Class->ClassID IS ID_VECTORSCENE) and (((objVectorScene *)Parent)->Viewport)) {
      viewport = ((objVectorScene *)Parent)->Viewport;
   }
   else {
      if (NewObject(ID_VECTORVIEWPORT, &viewport)) return;
      SetOwner(viewport, Parent);
   }

   // The first viewport to be instantiated is stored as a local reference.  This is important if the developer has
   // specified a custom target, in which case there needs to be a way to discover the root of the SVG.

   if (!Self->Viewport) Self->Viewport = viewport;
   // Process <svg> attributes

   auto state = State;
   if (!Tag.Children.empty()) state.applyTag(Tag); // Apply all attribute values to the current state.

   for (a=1; a < LONG(Tag.Attribs.size()); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         // The viewbox determines what area of the vector definition is to be displayed (in a sense, zooming into the document).
         // The individual x, y, width and height values determine the position and clipping of the displayed SVG content.

         case SVF_VIEWBOX:  {
            auto dim = read_array(val);
            if (dim.size() >= 4) viewport->setFields(fl::ViewX(dim[0]), fl::ViewY(dim[1]), fl::ViewWidth(dim[2]), fl::ViewHeight(dim[3]));
            break;
         }

         case SVF_VERSION: {
            DOUBLE version = strtod(val.c_str(), NULL);
            if (version > Self->SVGVersion) Self->SVGVersion = version;
            break;
         }

         case SVF_X: FUNIT(FID_X, val).set(viewport); break;
         case SVF_Y: FUNIT(FID_Y, val).set(viewport); break;
            
         case SVF_XOFFSET: FUNIT(FID_XOffset, val).set(viewport); break;
         case SVF_YOFFSET: FUNIT(FID_YOffset, val).set(viewport); break;

         case SVF_WIDTH:
            FUNIT(FID_Width, val).set(viewport);
            viewport->set(FID_OverflowX, LONG(VOF::HIDDEN));
            break;

         case SVF_HEIGHT:
            FUNIT(FID_Height, val).set(viewport);
            viewport->set(FID_OverflowY, LONG(VOF::HIDDEN));
            break;

         case SVF_PRESERVEASPECTRATIO:
            viewport->set(FID_AspectRatio, LONG(parse_aspect_ratio(val)));
            break;

         case SVF_ID:
            viewport->set(FID_ID, val);
            add_id(Self, Tag, val);
            SetName(viewport, val.c_str());
            break;

         case SVF_ENABLE_BACKGROUND:
            if ((!StrMatch("true", val)) or (!StrMatch("1", val))) viewport->set(FID_EnableBkgd, TRUE);
            break;

         case SVF_ZOOMANDPAN:
            if (!StrMatch("magnify", val)) {
               // This option indicates that the scene graph should be scaled to match the size of the client's
               // viewing window.
               log.warning("zoomAndPan not yet supported.");
            }
            break;

         case SVF_XMLNS: break; // Ignored
         case SVF_BASEPROFILE: break; // The minimum required SVG standard that is required for rendering the document.
            
         case SVF_MASK: {
            OBJECTPTR clip;
            if (!scFindDef(Self->Scene, val.c_str(), &clip)) viewport->set(FID_Mask, clip);
            else log.warning("Unable to find mask '%s'", val.c_str());
            break;
         }

         case SVF_CLIP_PATH: {
            OBJECTPTR clip;
            if (!scFindDef(Self->Scene, val.c_str(), &clip)) viewport->set(FID_Mask, clip);
            else log.warning("Unable to find clip-path '%s'", val.c_str());
            break;
         }

         // default - The browser will remove all newline characters. Then it will convert all tab characters into
         // space characters. Then, it will strip off all leading and trailing space characters. Then, all contiguous
         // space characters will be consolidated.
         //
         // preserve - The browser will will convert all newline and tab characters into space characters. Then, it
         // will draw all space characters, including leading, trailing and multiple contiguous space characters. Thus,
         // when drawn with xml:space="preserve", the string "a   b" (three spaces between "a" and "b") will produce a
         // larger separation between "a" and "b" than "a b" (one space between "a" and "b").

         case SVF_XML_SPACE:
            if (!StrMatch("preserve", val)) Self->PreserveWS = TRUE;
            else Self->PreserveWS = FALSE;
            break;

         default: {
            // Print a warning unless this was a reference to some other namespace.
            if (val.find(':') IS std::string::npos) {
               log.warning("Failed to parse attrib '%s' in <svg/> tag @ line %d", Tag.Attribs[a].Name.c_str(), Tag.LineNo);
            }
         }
      }
   }

   // Process child tags

   objVector *sibling = NULL;
   for (auto &child : Tag.Children) {
      if (child.isTag()) {
         log.traceBranch("Processing <%s/>", child.name());

         switch(StrHash(child.name())) {
            case SVF_DEFS: xtag_defs(Self, state, child, viewport); break;
            default:       xtag_default(Self, state, child, viewport, sibling);  break;
         }
      }
   }

   if (viewport->initialised()) Vector = viewport;
   else if (!viewport->init()) Vector = viewport;
   else FreeResource(viewport);
}

//********************************************************************************************************************
// <animateTransform attributeType="XML" attributeName="transform" type="rotate" from="0,150,150" to="360,150,150"
//   begin="0s" dur="5s" repeatCount="indefinite"/>

static ERROR xtag_animatetransform(extSVG *Self, XMLTag &Tag, OBJECTPTR Parent)
{
   pf::Log log(__FUNCTION__);

   Self->Animated = true;

   svgAnimation anim;
   anim.TargetVector = Parent->UID;

   for (unsigned a=1; a < Tag.Attribs.size(); a++) {
      auto &value = Tag.Attribs[a].Value;
      if (value.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_ATTRIBUTENAME: // Name of the target attribute affected by the From and To values.
            anim.TargetAttribute = value;
            break;

         case SVF_ATTRIBUTETYPE: // Namespace of the target attribute: XML, CSS, auto
            //if (!StrMatch("XML", value));
            //else if (!StrMatch("CSS", value));
            //else if (!StrMatch("auto", value));
            break;

         case SVF_ID:
            anim.ID = value;
            add_id(Self, Tag, value);
            break;

         case SVF_BEGIN:
            // Defines when the element should become active.  Specified as a semi-colon list.
            //   offset: A clock-value that is offset from the moment the animation is activated.
            //   id.end/begin: Reference to another animation's begin or end to determine when the animation starts.
            //   event: An event reference like 'focusin' determines that the animation starts when the event is triggered.
            //   id.repeat(value): Reference to another animation, repeat when the given value is reached.
            //   access-key: The animation starts when a keyboard key is pressed.
            //   clock: A real-world clock time (not supported)
            break;

         case SVF_END: // The animation ends when one of the triggers is reached.  Semi-colon list of multiple values permitted.

            break;

         case SVF_DUR: // 4s, 02:33, 12:10:53, 45min, 4ms, 12.93, 1h, 'media', 'indefinite'
            if (!StrMatch("media", value)) anim.Duration = 0; // Does not apply to animation
            else if (!StrMatch("indefinite", value)) anim.Duration = -1;
            else anim.Duration = read_time(value);
            break;

         case SVF_TYPE: // translate, scale, rotate, skewX, skewY
            if (!StrMatch("translate", value))   anim.Transform = AT_TRANSLATE;
            else if (!StrMatch("scale", value))  anim.Transform = AT_SCALE;
            else if (!StrMatch("rotate", value)) anim.Transform = AT_ROTATE;
            else if (!StrMatch("skewX", value))  anim.Transform = AT_SKEW_X;
            else if (!StrMatch("skewY", value))  anim.Transform = AT_SKEW_Y;
            else log.warning("Unsupported type '%s'", value.c_str());
            break;

         case SVF_MIN:
            if (!StrMatch("media", value)) anim.MinDuration = 0; // Does not apply to animation
            else anim.MinDuration = read_time(value);
            break;

         case SVF_MAX:
            if (!StrMatch("media", value)) anim.MaxDuration = 0; // Does not apply to animation
            else anim.MaxDuration = read_time(value);
            break;

         case SVF_FROM: { // The starting value of the animation.
            if (anim.Values.empty()) anim.Values.push_back(value);
            else anim.Values[0] = value;
            break;
         }

         case SVF_TO: {
            if (anim.Values.size() >= 1) anim.Values[1] = value;
            else anim.Values.insert(anim.Values.begin() + 1, value);
            break;
         }

         // Similar to from and to, this is a series of values that are interpolated over the time line.
         case SVF_VALUES: {
            anim.Values.clear();
            LONG s, v = 0;
            while ((v < std::ssize(value)) and (std::ssize(anim.Values) < MAX_VALUES)) {
               while ((value[v]) and (value[v] <= 0x20)) v++;
               for (s=v; (value[s]) and (value[s] != ';'); s++);
               anim.Values.push_back(value.substr(v, s-v));
               v = s;
               if (value[v] IS ';') v++;
            }
            break;
         }

         case SVF_RESTART: // always, whenNotActive, never
            if (!StrMatch("always", value)) anim.Restart = RST_ALWAYS;
            else if (!StrMatch("whenNotActive", value)) anim.Restart = RST_WHEN_NOT_ACTIVE;
            else if (!StrMatch("never", value)) anim.Restart = RST_NEVER;
            break;

         case SVF_REPEATDUR:
            if (!StrMatch("indefinite", value)) anim.RepeatDuration = -1;
            else anim.RepeatDuration = read_time(value);
            break;

         case SVF_REPEATCOUNT: // Integer, 'indefinite'
            if (!StrMatch("indefinite", value)) anim.RepeatCount = -1;
            else anim.RepeatCount = read_time(value);
            break;

         case SVF_FILL: // freeze, remove
            if (!StrMatch("freeze", value)) anim.Freeze = true; // Freeze the effect value at the last value of the duration (i.e. keep the last frame).
            else if (!StrMatch("remove", value)) anim.Freeze = true; // The default.  The effect is stopped when the duration is over.
            break;

         case SVF_ADDITIVE: // replace, sum
            if (!StrMatch("replace", value)) anim.Replace = true; // The animation values replace the underlying values of the target vector's attributes.
            else if (!StrMatch("sum", value)) anim.Replace = false; // The animation adds to the underlying values of the target vector.
            break;

         case SVF_ACCUMULATE:
            if (!StrMatch("none", value)) anim.Accumulate = false; // Repeat iterations are not cumulative.  This is the default.
            else if (!StrMatch("sum", value)) anim.Accumulate = true; // Each repeated iteration builds on the last value of the previous iteration.
            break;

         default:
            break;
      }
   }

   Self->Animations.emplace_back(anim);
   return ERR_Okay;
}

//********************************************************************************************************************
// <animateMotion from="0,0" to="100,100" dur="4s" fill="freeze"/>

static ERROR xtag_animatemotion(extSVG *Self, XMLTag &Tag, OBJECTPTR Parent)
{
   Self->Animated = true;

   for (unsigned a=1; a < Tag.Attribs.size(); a++) {
      if (Tag.Attribs[a].Value.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_FROM:
            break;
         case SVF_TO:
            break;
         case SVF_DUR:
            break;
         case SVF_PATH:
            //path="M 0 0 L 100 100"
            break;
         case SVF_FILL:
            // freeze = The last frame will be displayed at the end of the animation, rather than going back to the first frame.

            break;
         default:
            break;
      }
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static void process_attrib(extSVG *Self, XMLTag &Tag, svgState &State, objVector *Vector)
{
   pf::Log log(__FUNCTION__);

   for (unsigned t=1; t < Tag.Attribs.size(); t++) {
      if (Tag.Attribs[t].Value.empty()) continue;
      auto &name = Tag.Attribs[t].Name;
      auto &value = Tag.Attribs[t].Value;

      if (name.find(':') != std::string::npos) continue; // Do not interpret non-SVG attributes, e.g. 'inkscape:dx'

      log.trace("%s = %.40s", name.c_str(), value.c_str());

      if (auto error = set_property(Self, Vector, StrHash(name), Tag, State, value)) {
         if (Vector->Class->ClassID != ID_VECTORGROUP) {
            log.warning("Failed to set field '%s' with '%s' in %s; Error %s",
               name.c_str(), value.c_str(), Vector->Class->ClassName, GetErrorMsg(error));
         }
      }
   }
}

//********************************************************************************************************************
// Apply all attributes in a rule to a target tag.

static void apply_rule(extSVG *Self, KatanaArray *Properties, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   for (unsigned i=0; i < Properties->length; i++) {
      auto prop = (KatanaDeclaration *)Properties->data[i];

      log.trace("Set property %s with %d values", prop->property, prop->values->length);

      for (unsigned v=0; v < prop->values->length; v++) {
         auto value = (KatanaValue *)prop->values->data[v];

         switch (value->unit) {
            case KATANA_VALUE_NUMBER:
            case KATANA_VALUE_PERCENTAGE:
            case KATANA_VALUE_EMS:
            case KATANA_VALUE_EXS:
            case KATANA_VALUE_REMS:
            case KATANA_VALUE_CHS:
            case KATANA_VALUE_PX:
            case KATANA_VALUE_CM:
            case KATANA_VALUE_DPPX:
            case KATANA_VALUE_DPI:
            case KATANA_VALUE_DPCM:
            case KATANA_VALUE_MM:
            case KATANA_VALUE_IN:
            case KATANA_VALUE_PT:
            case KATANA_VALUE_PC:
            case KATANA_VALUE_DEG:
            case KATANA_VALUE_RAD:
            case KATANA_VALUE_GRAD:
            case KATANA_VALUE_MS:
            case KATANA_VALUE_S:
            case KATANA_VALUE_HZ:
            case KATANA_VALUE_KHZ:
            case KATANA_VALUE_TURN:
               xmlUpdateAttrib(Tag, prop->property, value->raw, true);
               break;

            case KATANA_VALUE_IDENT:
               xmlUpdateAttrib(Tag, prop->property, value->string, true);
               break;

            case KATANA_VALUE_STRING:
               xmlUpdateAttrib(Tag, prop->property, value->string, true);
               break;

            case KATANA_VALUE_PARSER_FUNCTION: {
               //const char* args_str = katana_stringify_value_list(parser, value->function->args);
               //snprintf(str, sizeof(str), "%s%s)", value->function->name, args_str);
               //katana_parser_deallocate(parser, (void*) args_str);
               break;
            }

            case KATANA_VALUE_PARSER_OPERATOR: {
               char str[8];
               if (value->iValue != '=') snprintf(str, sizeof(str), " %c ", value->iValue);
               else snprintf(str, sizeof(str), " %c", value->iValue);
               xmlUpdateAttrib(Tag, prop->property, str, true);
               break;
            }

            case KATANA_VALUE_PARSER_LIST:
               //katana_stringify_value_list(parser, value->list);
               break;

            case KATANA_VALUE_PARSER_HEXCOLOR:
               xmlUpdateAttrib(Tag, prop->property, std::string("#") + value->string, true);
               break;

            case KATANA_VALUE_URI:
               xmlUpdateAttrib(Tag, prop->property, std::string("url(") + value->string + ")", true);
               break;

            default:
               log.warning("Unknown property value.");
               break;
         }
      }
   }
}

//********************************************************************************************************************
// Scan and apply all stylesheet selectors to the loaded XML document.

static void process_rule(extSVG *Self, objXML::TAGS &Tags, KatanaRule *Rule)
{
   pf::Log log(__FUNCTION__);

   if (!Rule) return;

   switch (Rule->type) {
      case KatanaRuleStyle: {
         auto sr = (KatanaStyleRule *)Rule;
         for (unsigned i=0; i < sr->selectors->length; ++i) {
            auto sel = (KatanaSelector *)sr->selectors->data[i];

            switch (sel->match) {
               case KatanaSelectorMatchTag: // Applies to all tags matching this name
                  log.trace("Processing selector: %s", (sel->tag) ? sel->tag->local : "UNNAMED");
                  for (auto &tag : Tags) {
                     if (!StrMatch(sel->tag->local, tag.name())) {
                        apply_rule(Self, sr->declarations, tag);
                     }

                     if (!tag.Children.empty()) {
                        process_rule(Self, tag.Children, Rule);
                     }
                  }
                  break;

               case KatanaSelectorMatchId: // Applies to the first tag expressing this id
                  break;

               case KatanaSelectorMatchClass: // Requires tag to specify a class attribute
                  log.trace("Processing class selector: %s", (sel->data) ? sel->data->value : "UNNAMED");
                  for (auto &tag : Tags) {
                     for (auto &a : tag.Attribs) {
                        if (!StrMatch("class", a.Name)) {
                           if (!StrMatch(sel->data->value, a.Value)) {
                              apply_rule(Self, sr->declarations, tag);
                           }
                           break;
                        }
                     }

                     if (!tag.Children.empty()) {
                        process_rule(Self, tag.Children, Rule);
                     }
                  }
                  break;

               case KatanaSelectorMatchPseudoClass: // E.g. a:link
                  break;

               case KatanaSelectorMatchPseudoElement: break;
               case KatanaSelectorMatchPagePseudoClass: break;
               case KatanaSelectorMatchAttributeExact: break;
               case KatanaSelectorMatchAttributeSet: break;
               case KatanaSelectorMatchAttributeList: break;
               case KatanaSelectorMatchAttributeHyphen: break;
               case KatanaSelectorMatchAttributeContain: break;
               case KatanaSelectorMatchAttributeBegin: break;
               case KatanaSelectorMatchAttributeEnd: break;
               case KatanaSelectorMatchUnknown: break;
            }
         }

         break;
      }

      case KatanaRuleImport: //(KatanaImportRule*)rule
         log.msg("Support required for KatanaRuleImport");
         break;

      case KatanaRuleFontFace: //(KatanaFontFaceRule*)rule
         log.msg("Support required for KatanaRuleFontFace");
         break;

      case KatanaRuleKeyframes: //(KatanaKeyframesRule*)rule
         log.msg("Support required for KatanaRuleKeyframes");
         break;

      case KatanaRuleMedia: //(KatanaMediaRule*)rule
         log.msg("Support required for KatanaRuleMedia");
         break;

      case KatanaRuleUnkown:
      case KatanaRuleSupports:
      case KatanaRuleCharset:
      case KatanaRuleHost:
         break;
   }
}

//********************************************************************************************************************

static ERROR set_property(extSVG *Self, objVector *Vector, ULONG Hash, XMLTag &Tag, svgState &State, const std::string StrValue)
{
   pf::Log log(__FUNCTION__);

   // Ignore stylesheet attributes
   if (Hash IS SVF_CLASS) return ERR_Okay;

   switch(Vector->Class->ClassID) {
      case ID_VECTORVIEWPORT:
         switch (Hash) {
            // The following 'view-*' fields are for defining the SVG view box
            case SVF_VIEW_X:      FUNIT(FID_ViewX, StrValue).set(Vector); return ERR_Okay;
            case SVF_VIEW_Y:      FUNIT(FID_ViewY, StrValue).set(Vector); return ERR_Okay;
            case SVF_VIEW_WIDTH:  FUNIT(FID_ViewWidth, StrValue).set(Vector); return ERR_Okay;
            case SVF_VIEW_HEIGHT: FUNIT(FID_ViewHeight, StrValue).set(Vector); return ERR_Okay;
            // The following dimension fields are for defining the position and clipping of the vector display
            case SVF_X:      FUNIT(FID_X, StrValue).set(Vector); return ERR_Okay;
            case SVF_Y:      FUNIT(FID_Y, StrValue).set(Vector); return ERR_Okay;
            case SVF_WIDTH:  FUNIT(FID_Width, StrValue).set(Vector); return ERR_Okay;
            case SVF_HEIGHT: FUNIT(FID_Height, StrValue).set(Vector); return ERR_Okay;
         }
         break;

      case ID_VECTORELLIPSE:
         switch (Hash) {
            case SVF_CX: FUNIT(FID_CenterX, StrValue).set(Vector); return ERR_Okay;
            case SVF_CY: FUNIT(FID_CenterY, StrValue).set(Vector); return ERR_Okay;
            case SVF_R:  FUNIT(FID_Radius, StrValue).set(Vector); return ERR_Okay;
            case SVF_RX: FUNIT(FID_RadiusX, StrValue).set(Vector); return ERR_Okay;
            case SVF_RY: FUNIT(FID_RadiusY, StrValue).set(Vector); return ERR_Okay;
            case SVF_VERTICES: FUNIT(FID_Vertices, StrValue).set(Vector); return ERR_Okay;
         }
         break;

      case ID_VECTORWAVE:
         switch (Hash) {         
            case SVF_X: FUNIT(FID_X, StrValue).set(Vector); return ERR_Okay;
            case SVF_Y: FUNIT(FID_Y, StrValue).set(Vector); return ERR_Okay;
            case SVF_WIDTH:  FUNIT(FID_Width, StrValue).set(Vector); return ERR_Okay;
            case SVF_HEIGHT: FUNIT(FID_Height, StrValue).set(Vector); return ERR_Okay;
            case SVF_CLOSE:  Vector->set(FID_Close, StrValue); return ERR_Okay;
            case SVF_AMPLITUDE: FUNIT(FID_Amplitude, StrValue).set(Vector); return ERR_Okay;
            case SVF_DECAY:     FUNIT(FID_Decay, StrValue).set(Vector); return ERR_Okay;
            case SVF_FREQUENCY: FUNIT(FID_Frequency, StrValue).set(Vector); return ERR_Okay;
            case SVF_THICKNESS: FUNIT(FID_Thickness, StrValue).set(Vector); return ERR_Okay;
         }
         break;

      case ID_VECTORRECTANGLE:
         switch (Hash) {
            case SVF_X1:
            case SVF_X:  FUNIT(FID_X, StrValue).set(Vector); return ERR_Okay;
            case SVF_Y1:
            case SVF_Y:  FUNIT(FID_Y, StrValue).set(Vector); return ERR_Okay;
            case SVF_WIDTH:  FUNIT(FID_Width, StrValue).set(Vector); return ERR_Okay;
            case SVF_HEIGHT: FUNIT(FID_Height, StrValue).set(Vector); return ERR_Okay;
            case SVF_RX:     FUNIT(FID_RoundX, StrValue).set(Vector); return ERR_Okay;
            case SVF_RY:     FUNIT(FID_RoundY, StrValue).set(Vector); return ERR_Okay;

            case SVF_XOFFSET: FUNIT(FID_XOffset, StrValue).set(Vector); return ERR_Okay; // Parasol only
            case SVF_YOFFSET: FUNIT(FID_YOffset, StrValue).set(Vector); return ERR_Okay; // Parasol only

            case SVF_X2: { 
               // Note: For the time being, VectorRectangle doesn't support X2/Y2 as a concept.  This would
               // cause problems if the client was to specify a scaled value here.
               auto width = FUNIT(FID_Width, StrValue);
               SetField(Vector, FID_Width|TDOUBLE, std::abs(DOUBLE(width) - Vector->get<DOUBLE>(FID_X)));
               return ERR_Okay;
            }

            case SVF_Y2: {
               auto height = FUNIT(FID_Height, StrValue);
               SetField(Vector, FID_Height|TDOUBLE, std::abs(DOUBLE(height) - Vector->get<DOUBLE>(FID_Y)));
               return ERR_Okay;
            }
         }
         break;

      // VectorPolygon handles polygon, polyline and line.
      case ID_VECTORPOLYGON:
         switch (Hash) {
            case SVF_POINTS: Vector->set(FID_Points, StrValue); return ERR_Okay;
            case SVF_X1: FUNIT(FID_X1, StrValue).set(Vector); return ERR_Okay;
            case SVF_Y1: FUNIT(FID_Y1, StrValue).set(Vector); return ERR_Okay;
            case SVF_X2: FUNIT(FID_X2, StrValue).set(Vector); return ERR_Okay;
            case SVF_Y2: FUNIT(FID_Y2, StrValue).set(Vector); return ERR_Okay;
         }
         break;

      case ID_VECTORTEXT:
         switch (Hash) {
            case SVF_X: FUNIT(FID_X, StrValue).set(Vector); return ERR_Okay;
            case SVF_Y: FUNIT(FID_Y, StrValue).set(Vector); return ERR_Okay;
               
            case SVF_DX: Vector->set(FID_DX, StrValue); return ERR_Okay;
            case SVF_DY: Vector->set(FID_DY, StrValue); return ERR_Okay;

            case SVF_LENGTHADJUST: // Can be set to either 'spacing' or 'spacingAndGlyphs'
               //if (!StrMatch("spacingAndGlyphs", va_arg(list, STRING))) Vector->VT.SpacingAndGlyphs = TRUE;
               //else Vector->VT.SpacingAndGlyphs = FALSE;
               return ERR_Okay;

            case SVF_FONT: {
               // Officially accepted examples for the 'font' attribute:
               //
               //    12pt/14pt sans-serif
               //    80% sans-serif
               //    x-large/110% "new century schoolbook", serif
               //    bold italic large Palatino, serif
               //    normal small-caps 120%/120% fantasy
               //    oblique 12pt "Helvetica Nue", serif; font-stretch: condensed
               //
               // [ [ <'font-style'> || <'font-variant'> || <'font-weight'> ]? <'font-size'> [ / <'line-height'> ]? <'font-family'> ] | caption | icon | menu | message-box | small-caption | status-bar | inherit
               // TODO Add support for text font attribute
               return ERR_NoSupport;
            }

            case SVF_FONT_FAMILY:
               Vector->set(FID_Face, StrValue);
               return ERR_Okay;

            case SVF_FONT_SIZE:
               // A plain numeric font size is interpreted as "a height value corresponding to the current user
               // coordinate system".  Alternatively the user can specify the unit identifier, e.g. '12pt', '10%', '30px'
               Vector->set(FID_FontSize, StrValue);
               return ERR_Okay;

            case SVF_FONT_SIZE_ADJUST:
               // Auto-adjust the font height according to the formula "y(a/a') = c" where the value provided is used as 'a'.
               // y = 'font-size' of first-choice font
               // a' = aspect value of available font
               // c = 'font-size' to apply to available font
               return ERR_NoSupport;

            case SVF_FONT_STRETCH:
               switch(StrHash(StrValue)) {
                  case SVF_CONDENSED:       Vector->set(FID_Stretch, LONG(VTS::CONDENSED)); return ERR_Okay;
                  case SVF_EXPANDED:        Vector->set(FID_Stretch, LONG(VTS::EXPANDED)); return ERR_Okay;
                  case SVF_EXTRA_CONDENSED: Vector->set(FID_Stretch, LONG(VTS::EXTRA_CONDENSED)); return ERR_Okay;
                  case SVF_EXTRA_EXPANDED:  Vector->set(FID_Stretch, LONG(VTS::EXTRA_EXPANDED)); return ERR_Okay;
                  case SVF_NARROWER:        Vector->set(FID_Stretch, LONG(VTS::NARROWER)); return ERR_Okay;
                  case SVF_NORMAL:          Vector->set(FID_Stretch, LONG(VTS::NORMAL)); return ERR_Okay;
                  case SVF_SEMI_CONDENSED:  Vector->set(FID_Stretch, LONG(VTS::SEMI_CONDENSED)); return ERR_Okay;
                  case SVF_SEMI_EXPANDED:   Vector->set(FID_Stretch, LONG(VTS::SEMI_EXPANDED)); return ERR_Okay;
                  case SVF_ULTRA_CONDENSED: Vector->set(FID_Stretch, LONG(VTS::ULTRA_CONDENSED)); return ERR_Okay;
                  case SVF_ULTRA_EXPANDED:  Vector->set(FID_Stretch, LONG(VTS::ULTRA_EXPANDED)); return ERR_Okay;
                  case SVF_WIDER:           Vector->set(FID_Stretch, LONG(VTS::WIDER)); return ERR_Okay;
                  default: log.warning("no support for font-stretch value '%s'", StrValue.c_str());
               }
               break;

            case SVF_FONT_STYLE: return ERR_NoSupport;
            case SVF_FONT_VARIANT: return ERR_NoSupport;

            case SVF_FONT_WEIGHT: { // SVG: normal | bold | bolder | lighter | inherit
               DOUBLE num = StrToFloat(StrValue);
               if (num) Vector->set(FID_Weight, num);
               else switch(StrHash(StrValue)) {
                  case SVF_NORMAL:  Vector->set(FID_Weight, 400); return ERR_Okay;
                  case SVF_LIGHTER: Vector->set(FID_Weight, 300); return ERR_Okay; // -100 off the inherited weight
                  case SVF_BOLD:    Vector->set(FID_Weight, 700); return ERR_Okay;
                  case SVF_BOLDER:  Vector->set(FID_Weight, 900); return ERR_Okay; // +100 on the inherited weight
                  case SVF_INHERIT: Vector->set(FID_Weight, 400); return ERR_Okay; // Not supported correctly yet.
                  default: log.warning("No support for font-weight value '%s'", StrValue.c_str()); // Non-fatal
               }
               break;
            }

            case SVF_ROTATE: Vector->set(FID_Rotate, StrValue); return ERR_Okay;
            case SVF_STRING: Vector->set(FID_String, StrValue); return ERR_Okay;

            case SVF_TEXT_ANCHOR:
               switch(StrHash(StrValue)) {
                  case SVF_START:   Vector->set(FID_Align, LONG(ALIGN::LEFT)); return ERR_Okay;
                  case SVF_MIDDLE:  Vector->set(FID_Align, LONG(ALIGN::HORIZONTAL)); return ERR_Okay;
                  case SVF_END:     Vector->set(FID_Align, LONG(ALIGN::RIGHT)); return ERR_Okay;
                  case SVF_INHERIT: Vector->set(FID_Align, LONG(ALIGN::NIL)); return ERR_Okay;
                  default: log.warning("text-anchor: No support for value '%s'", StrValue.c_str());
               }
               break;

            case SVF_TEXTLENGTH: Vector->set(FID_TextLength, StrValue); return ERR_Okay;
            // TextPath only
            //case SVF_STARTOFFSET: Vector->set(FID_StartOffset, StrValue); return ERR_Okay;
            //case SVF_METHOD: // The default is align.  For 'stretch' mode, set VMF::STRETCH in MorphFlags
            //                      Vector->set(FID_MorphFlags, StrValue); return ERR_Okay;
            //case SVF_SPACING:     Vector->set(FID_Spacing, StrValue); return ERR_Okay;
            //case SVF_XLINK_HREF:  // Used for drawing text along a path.
            //   return ERR_Okay;

            case SVF_KERNING: Vector->set(FID_Kerning, StrValue); return ERR_Okay; // Spacing between letters, default=1.0
            case SVF_LETTER_SPACING: Vector->set(FID_LetterSpacing, StrValue); return ERR_Okay;
            case SVF_PATHLENGTH: Vector->set(FID_PathLength, StrValue); return ERR_Okay;
            case SVF_WORD_SPACING:   Vector->set(FID_WordSpacing, StrValue); return ERR_Okay;
            case SVF_TEXT_DECORATION:
               switch(StrHash(StrValue)) {
                  case SVF_UNDERLINE:    Vector->set(FID_Flags, LONG(VTXF::UNDERLINE)); return ERR_Okay;
                  case SVF_OVERLINE:     Vector->set(FID_Flags, LONG(VTXF::OVERLINE)); return ERR_Okay;
                  case SVF_LINETHROUGH:  Vector->set(FID_Flags, LONG(VTXF::LINE_THROUGH)); return ERR_Okay;
                  case SVF_BLINK:        Vector->set(FID_Flags, LONG(VTXF::BLINK)); return ERR_Okay;
                  case SVF_INHERIT:      return ERR_Okay;
                  default: log.warning("No support for text-decoration value '%s'", StrValue.c_str());
               }
               return ERR_Okay;
         }
         break;

      case ID_VECTORSPIRAL:
         switch (Hash) {
            case SVF_PATHLENGTH: Vector->set(FID_PathLength, StrValue); return ERR_Okay;
            case SVF_CX:       FUNIT(FID_CenterX, StrValue).set(Vector); return ERR_Okay;
            case SVF_CY:       FUNIT(FID_CenterY, StrValue).set(Vector); return ERR_Okay;
            case SVF_R:        FUNIT(FID_Radius, StrValue).set(Vector); return ERR_Okay;
            case SVF_OFFSET:   FUNIT(FID_Offset, StrValue).set(Vector); return ERR_Okay;
            case SVF_STEP:     FUNIT(FID_Step, StrValue).set(Vector); return ERR_Okay;
            case SVF_VERTICES: FUNIT(FID_Vertices, StrValue).set(Vector); return ERR_Okay;
            case SVF_SPACING:  FUNIT(FID_Spacing, StrValue).set(Vector); return ERR_Okay;
            case SVF_LOOP_LIMIT: FUNIT(FID_LoopLimit, StrValue).set(Vector); return ERR_Okay;
         }
         break;

      case ID_VECTORSHAPE:
         switch (Hash) {
            case SVF_CX:   FUNIT(FID_CenterX, StrValue).set(Vector); return ERR_Okay;
            case SVF_CY:   FUNIT(FID_CenterY, StrValue).set(Vector); return ERR_Okay;
            case SVF_R:    FUNIT(FID_Radius, StrValue).set(Vector); return ERR_Okay;
            case SVF_N1:   FUNIT(FID_N1, StrValue).set(Vector); return ERR_Okay;
            case SVF_N2:   FUNIT(FID_N2, StrValue).set(Vector); return ERR_Okay;
            case SVF_N3:   FUNIT(FID_N3, StrValue).set(Vector); return ERR_Okay;
            case SVF_M:    FUNIT(FID_M, StrValue).set(Vector); return ERR_Okay;
            case SVF_A:    FUNIT(FID_A, StrValue).set(Vector); return ERR_Okay;
            case SVF_B:    FUNIT(FID_B, StrValue).set(Vector); return ERR_Okay;
            case SVF_PHI:  FUNIT(FID_Phi, StrValue).set(Vector); return ERR_Okay;
            case SVF_VERTICES: FUNIT(FID_Vertices, StrValue).set(Vector); return ERR_Okay;
            case SVF_MOD:      FUNIT(FID_Mod, StrValue).set(Vector); return ERR_Okay;
            case SVF_SPIRAL:   FUNIT(FID_Spiral, StrValue).set(Vector); return ERR_Okay;
            case SVF_REPEAT:   FUNIT(FID_Repeat, StrValue).set(Vector); return ERR_Okay;
            case SVF_CLOSE:
               if ((!StrMatch("true", StrValue)) or (!StrMatch("1", StrValue))) Vector->set(FID_Close, TRUE);
               else Vector->set(FID_Close, FALSE);
               break;
         }
         break;

      case ID_VECTORPATH:
         switch (Hash) {
            case SVF_D: Vector->set(FID_Sequence, StrValue); return ERR_Okay;
            case SVF_PATHLENGTH: Vector->set(FID_PathLength, StrValue); return ERR_Okay;
         }
         break;
   }

   // Fall-through to generic attributes.

   switch (Hash) {
      case SVF_TRANSITION: {
         OBJECTPTR trans = NULL;
         if (!scFindDef(Self->Scene, StrValue.c_str(), &trans)) Vector->set(FID_Transition, trans);
         else log.warning("Unable to find element '%s' referenced at line %d", StrValue.c_str(), Tag.LineNo);
         break;
      }

      case SVF_COLOUR_INTERPOLATION:
      case SVF_COLOR_INTERPOLATION:
         if (!StrMatch("auto", StrValue)) Vector->set(FID_ColourSpace, LONG(VCS::SRGB));
         else if (!StrMatch("sRGB", StrValue)) Vector->set(FID_ColourSpace, LONG(VCS::SRGB));
         else if (!StrMatch("linearRGB", StrValue)) Vector->set(FID_ColourSpace, LONG(VCS::LINEAR_RGB));
         else if (!StrMatch("inherit", StrValue)) Vector->set(FID_ColourSpace, LONG(VCS::INHERIT));
         else log.warning("Invalid color-interpolation value '%s' at line %d", StrValue.c_str(), Tag.LineNo);

      case SVF_STROKE_LINEJOIN:
         switch(StrHash(StrValue)) {
            case SVF_MITER: Vector->set(FID_LineJoin, LONG(VLJ::MITER)); break;
            case SVF_ROUND: Vector->set(FID_LineJoin, LONG(VLJ::ROUND)); break;
            case SVF_BEVEL: Vector->set(FID_LineJoin, LONG(VLJ::BEVEL)); break;
            case SVF_INHERIT: Vector->set(FID_LineJoin, LONG(VLJ::INHERIT)); break;
            case SVF_MITER_REVERT: Vector->set(FID_LineJoin, LONG(VLJ::MITER_REVERT)); break; // Special AGG only join type
            case SVF_MITER_ROUND: Vector->set(FID_LineJoin, LONG(VLJ::MITER_ROUND)); break; // Special AGG only join type
         }
         break;

      case SVF_STROKE_INNERJOIN: // AGG ONLY
         switch(StrHash(StrValue)) {
            case SVF_MITER:   Vector->set(FID_InnerJoin, LONG(VIJ::MITER));  break;
            case SVF_ROUND:   Vector->set(FID_InnerJoin, LONG(VIJ::ROUND)); break;
            case SVF_BEVEL:   Vector->set(FID_InnerJoin, LONG(VIJ::BEVEL)); break;
            case SVF_INHERIT: Vector->set(FID_InnerJoin, LONG(VIJ::INHERIT)); break;
            case SVF_JAG:     Vector->set(FID_InnerJoin, LONG(VIJ::JAG)); break;
         }

      case SVF_STROKE_LINECAP:
         switch(StrHash(StrValue)) {
            case SVF_BUTT:    Vector->set(FID_LineCap, LONG(VLC::BUTT)); break;
            case SVF_SQUARE:  Vector->set(FID_LineCap, LONG(VLC::SQUARE)); break;
            case SVF_ROUND:   Vector->set(FID_LineCap, LONG(VLC::ROUND)); break;
            case SVF_INHERIT: Vector->set(FID_LineCap, LONG(VLC::INHERIT)); break;
         }
         break;

      case SVF_VISIBILITY:
         if (!StrMatch("visible", StrValue))       Vector->set(FID_Visibility, LONG(VIS::VISIBLE));
         else if (!StrMatch("hidden", StrValue))   Vector->set(FID_Visibility, LONG(VIS::HIDDEN));
         else if (!StrMatch("collapse", StrValue)) Vector->set(FID_Visibility, LONG(VIS::COLLAPSE)); // Same effect as hidden, kept for SVG compatibility
         else if (!StrMatch("inherit", StrValue))  Vector->set(FID_Visibility, LONG(VIS::INHERIT));
         else log.warning("Unsupported visibility value '%s'", StrValue.c_str());
         break;

      case SVF_FILL_RULE:
         if (!StrMatch("nonzero", StrValue)) Vector->set(FID_FillRule, LONG(VFR::NON_ZERO));
         else if (!StrMatch("evenodd", StrValue)) Vector->set(FID_FillRule, LONG(VFR::EVEN_ODD));
         else if (!StrMatch("inherit", StrValue)) Vector->set(FID_FillRule, LONG(VFR::INHERIT));
         else log.warning("Unsupported fill-rule value '%s'", StrValue.c_str());
         break;

      case SVF_CLIP_RULE:
         if (!StrMatch("nonzero", StrValue)) Vector->set(FID_ClipRule, LONG(VFR::NON_ZERO));
         else if (!StrMatch("evenodd", StrValue)) Vector->set(FID_ClipRule, LONG(VFR::EVEN_ODD));
         else if (!StrMatch("inherit", StrValue)) Vector->set(FID_ClipRule, LONG(VFR::INHERIT));
         else log.warning("Unsupported clip-rule value '%s'", StrValue.c_str());
         break;

      case SVF_ENABLE_BACKGROUND:
         if (!StrMatch("new", StrValue)) Vector->set(FID_EnableBkgd, TRUE);
         break;

      case SVF_ID:
         if (!Self->Cloning) {
            Vector->set(FID_ID, StrValue);
            add_id(Self, Tag, StrValue);
            scAddDef(Self->Scene, StrValue.c_str(), Vector);
            SetName(Vector, StrValue.c_str());
         }
         break;
         
      case SVF_DISPLAY: 
         // The difference between 'display=none' and 'visibility=hidden' is that visibilility holds its 
         // whitespace in document layout mode.  This has no relevance in our Vector Scene Graph, so 'display' is 
         // treated as an obsolete feature and converted to visibility.

         if (!StrMatch("none", StrValue))          Vector->set(FID_Visibility, LONG(VIS::HIDDEN));
         else if (!StrMatch("inline", StrValue))   Vector->set(FID_Visibility, LONG(VIS::VISIBLE));
         else if (!StrMatch("inherit", StrValue))  Vector->set(FID_Visibility, LONG(VIS::INHERIT));
         break;

      case SVF_NUMERIC_ID: Vector->set(FID_NumericID, StrValue); break;

      case SVF_OVERFLOW: // visible | hidden | scroll | auto | inherit
         log.trace("overflow is not supported.");
         break;

      case SVF_MARKER:       log.warning("marker is not supported."); break;
      case SVF_MARKER_END:   log.warning("marker-end is not supported."); break;
      case SVF_MARKER_MID:   log.warning("marker-mid is not supported."); break;
      case SVF_MARKER_START: log.warning("marker-start is not supported."); break;

      case SVF_FILTER:       Vector->set(FID_Filter, StrValue); break;
      case SVF_COLOR:        Vector->set(FID_Fill, StrValue); break;

      case SVF_STROKE:
         if (!StrMatch("currentColor", StrValue)) {
            FRGB rgb;
            if (!current_colour(Self, Vector, State, rgb)) SetArray(Vector, FID_Stroke|TFLOAT, &rgb, 4);
         }
         else Vector->set(FID_Stroke, StrValue);
         break;

      case SVF_FILL:
         if (!StrMatch("currentColor", StrValue)) {
            FRGB rgb;
            if (!current_colour(Self, Vector, State, rgb)) SetArray(Vector, FID_Fill|TFLOAT, &rgb, 4);
         }
         else Vector->set(FID_Fill, StrValue);
         break;

      case SVF_TRANSFORM: parse_transform(Vector, StrValue); break;

      case SVF_STROKE_DASHARRAY: Vector->set(FID_DashArray, StrValue); break;
      case SVF_OPACITY:          Vector->set(FID_Opacity, StrValue); break;
      case SVF_FILL_OPACITY:     Vector->set(FID_FillOpacity, StrToFloat(StrValue)); break;
      case SVF_SHAPE_RENDERING:  Vector->set(FID_PathQuality, LONG(shape_rendering_to_render_quality(StrValue))); break;

      case SVF_STROKE_WIDTH:            FUNIT(FID_StrokeWidth, StrValue).set(Vector); break;
      case SVF_STROKE_OPACITY:          Vector->set(FID_StrokeOpacity, StrValue); break;
      case SVF_STROKE_MITERLIMIT:       Vector->set(FID_MiterLimit, StrValue); break;
      case SVF_STROKE_MITERLIMIT_THETA: Vector->set(FID_MiterLimitTheta, StrValue); break;
      case SVF_STROKE_INNER_MITERLIMIT: Vector->set(FID_InnerMiterLimit, StrValue); break;
      case SVF_STROKE_DASHOFFSET:       FUNIT(FID_DashOffset, StrValue).set(Vector); break;

      case SVF_MASK: {
         OBJECTPTR clip;
         if (!scFindDef(Self->Scene, StrValue.c_str(), &clip)) {
            Vector->set(FID_Mask, clip);
         }
         else {
            log.warning("Unable to find mask '%s'", StrValue.c_str());
            return ERR_Search;
         }
         break;
      }

      case SVF_CLIP_PATH: {
         OBJECTPTR clip;
         if (!scFindDef(Self->Scene, StrValue.c_str(), &clip)) {
            Vector->set(FID_Mask, clip);
         }
         else {
            log.warning("Unable to find clip-path '%s'", StrValue.c_str());
            return ERR_Search;
         }
         break;
      }

      default: return ERR_UnsupportedField;
   }

   return ERR_Okay;
}
