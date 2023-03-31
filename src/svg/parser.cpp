
//********************************************************************************************************************

static LONG parse_aspect_ratio(const std::string Value)
{
   CSTRING v = Value.c_str();
   while ((*v) and (*v <= 0x20)) v++;

   if (!StrMatch("none", v)) return ARF_NONE;
   else {
      LONG flags = 0;
      if (!StrCompare("xMin", v, 4, 0)) { flags |= ARF_X_MIN; v += 4; }
      else if (!StrCompare("xMid", v, 4, 0)) { flags |= ARF_X_MID; v += 4; }
      else if (!StrCompare("xMax", v, 4, 0)) { flags |= ARF_X_MAX; v += 4; }

      if (!StrCompare("yMin", v, 4, 0)) { flags |= ARF_Y_MIN; v += 4; }
      else if (!StrCompare("yMid", v, 4, 0)) { flags |= ARF_Y_MID; v += 4; }
      else if (!StrCompare("yMax", v, 4, 0)) { flags |= ARF_Y_MAX; v += 4; }

      while ((*v) and (*v <= 0x20)) v++;

      if (!StrCompare("meet", v, 4, 0)) { flags |= ARF_MEET; }
      else if (!StrCompare("slice", v, 5, 0)) { flags |= ARF_SLICE; }
      return flags;
   }
}

//********************************************************************************************************************

static LONG shape_rendering_to_render_quality(const std::string Value)
{
   pf::Log log;

   if (!StrMatch("auto", Value)) return RQ_AUTO;
   else if (!StrMatch("optimize-speed", Value)) return RQ_FAST;
   else if (!StrMatch("optimizeSpeed", Value)) return RQ_FAST;
   else if (!StrMatch("crisp-edges", Value)) return RQ_CRISP;
   else if (!StrMatch("crispEdges", Value)) return RQ_CRISP;
   else if (!StrMatch("geometric-precision", Value)) return RQ_PRECISE;
   else if (!StrMatch("geometricPrecision", Value)) return RQ_PRECISE;
   else if (!StrMatch("best", Value)) return RQ_BEST;
   else log.warning("Unknown shape-rendering value '%s'", Value.c_str());

   return RQ_AUTO;
}

//********************************************************************************************************************
// Apply the current state values to a vector.

static void apply_state(svgState &State, OBJECTPTR Vector)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("%s: Fill: %s, Stroke: %s, Opacity: %.2f, Font: %s %s",
      Vector->Class->ClassName, State.Fill.c_str(), State.Stroke.c_str(), State.Opacity, State.FontFamily.c_str(), State.FontSize.c_str());

   if (!State.Fill.empty())   Vector->set(FID_Fill, State.Fill);
   if (!State.Stroke.empty()) Vector->set(FID_Stroke, State.Stroke);
   if (State.StrokeWidth)     Vector->set(FID_StrokeWidth, State.StrokeWidth);
   if (Vector->SubID IS ID_VECTORTEXT) {
      if (!State.FontFamily.empty()) Vector->set(FID_Face, State.FontFamily);
      if (!State.FontSize.empty())   Vector->set(FID_FontSize, State.FontSize);
      if (State.FontWeight) Vector->set(FID_Weight, State.FontWeight);
   }
   if (State.FillOpacity >= 0.0) Vector->set(FID_FillOpacity, State.FillOpacity);
   if (State.Opacity >= 0.0) Vector->set(FID_Opacity, State.Opacity);

   if (Vector->SubID != ID_VECTORTEXT) {
      if (State.PathQuality != RQ_AUTO) Vector->set(FID_PathQuality, State.PathQuality);
   }
}

//********************************************************************************************************************
// Copy a tag's attributes to the current state.

static void set_state(svgState &State, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("Total Attributes: %d", LONG(Tag.Attribs.size()));

   LONG a;
   for (a=1; a < LONG(Tag.Attribs.size()); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch (StrHash(Tag.Attribs[a].Name)) {
         case SVF_FILL:         State.Fill = val; break;
         case SVF_STROKE:       State.Stroke = val; break;
         case SVF_STROKE_WIDTH: State.StrokeWidth = StrToFloat(val); break;
         case SVF_FONT_FAMILY:  State.FontFamily = val; break;
         case SVF_FONT_SIZE:    State.FontSize = val; break;
         case SVF_FONT_WEIGHT: {
            State.FontWeight = StrToFloat(val);
            if (!State.FontWeight) {
               switch(StrHash(val)) {
                  case SVF_NORMAL:  State.FontWeight = 400; break;
                  case SVF_LIGHTER: State.FontWeight = 300; break; // -100 off the inherited weight
                  case SVF_BOLD:    State.FontWeight = 700; break;
                  case SVF_BOLDER:  State.FontWeight = 900; break; // +100 on the inherited weight
                  case SVF_INHERIT: State.FontWeight = 400; break; // Not supported correctly yet.
                  default:
                     log.warning("No support for font-weight value '%s'", val.c_str()); // Non-fatal
                     State.FontWeight = 400;
               }
            }
            break;
         }
         case SVF_FILL_OPACITY: State.FillOpacity = StrToFloat(val); break;
         case SVF_OPACITY:      State.Opacity = StrToFloat(val); break;
         case SVF_SHAPE_RENDERING: State.PathQuality = shape_rendering_to_render_quality(val);
      }
   }
}

//********************************************************************************************************************
// Process all child elements that belong to the target Tag.

static void process_children(extSVG *Self, objXML *XML, svgState &State, const XMLTag &Tag, OBJECTPTR Vector)
{
   objVector *sibling = NULL;
   for (auto &child : Tag.Children) {
      if (child.isTag()) {
         xtag_default(Self, XML, State, child, Vector, &sibling);
      }
   }
}

//********************************************************************************************************************

static void xtag_pathtransition(extSVG *Self, objXML *XML, const XMLTag &Tag)
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
      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
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
               scAddDef(Self->Scene, id.c_str(), trans);
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

static void xtag_clippath(extSVG *Self, objXML *XML, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("Tag: %d", Tag.ID);

   OBJECTPTR clip;
   std::string id;

   if (!NewObject(ID_VECTORCLIP, &clip)) {
      clip->setFields(
         fl::Owner(Self->Scene->UID), // All clips belong to the root page to prevent hierarchy issues.
         fl::Name("SVGClip"),
         fl::Units(VUNIT_BOUNDING_BOX)
      );

      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
         if (Tag.Attribs[a].Value.empty()) continue;

         switch(StrHash(Tag.Attribs[a].Name)) {
            case SVF_ID: id = Tag.Attribs[a].Value; break;
            case SVF_TRANSFORM: break;
            case SVF_CLIPPATHUNITS: break;
            case SVF_EXTERNALRESOURCESREQUIRED: break;
         }
      }

      if (!id.empty()) {
         if (!InitObject(clip)) {
            svgState state;

            // Valid child elements for clip-path are: circle, ellipse, line, path, polygon, polyline, rect, text, use, animate

            process_children(Self, XML, state, Tag, clip);
            scAddDef(Self->Scene, id.c_str(), clip);
         }
         else FreeResource(clip);
      }
      else {
         log.warning("No id attribute specified in <clipPath> at line %d.", Tag.LineNo);
         FreeResource(clip);
      }
   }
}

//********************************************************************************************************************

static ERROR parse_fe_blur(extSVG *Self, objVectorFilter *Filter, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_BLURFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_STDDEVIATION: { // Y is optional, if not set then it is equivalent to X.
            DOUBLE x = -1, y = -1;
            read_numseq(val, &x, &y, TAGEND);
            if (x > 0) fx->set(FID_SX, x);
            if (y > 0) fx->set(FID_SY, y);
            break;
         }

         case SVF_X: set_double(fx, FID_X, val); break;

         case SVF_Y: set_double(fx, FID_Y, val); break;

         case SVF_WIDTH: set_double(fx, FID_Width, val); break;

         case SVF_HEIGHT: set_double(fx, FID_Height, val); break;

         case SVF_IN: parse_input(Self, fx, val, FID_SourceType, FID_Input); break;

         case SVF_RESULT: parse_result(Self, fx, val); break;
      }
   }

   if (!fx->init()) return ERR_Okay;
   else {
      FreeResource(fx);
      return ERR_Init;
   }
}

//********************************************************************************************************************

static ERROR parse_fe_offset(extSVG *Self, objVectorFilter *Filter, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_OFFSETFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_DX: fx->set(FID_XOffset, StrToInt(val)); break;
         case SVF_DY: fx->set(FID_YOffset, StrToInt(val)); break;
         case SVF_IN: parse_input(Self, fx, val, FID_SourceType, FID_Input); break;
         case SVF_RESULT: parse_result(Self, fx, val); break;
      }
   }

   if (!fx->init()) return ERR_Okay;
   else {
      FreeResource(fx);
      return ERR_Init;
   }
}

//********************************************************************************************************************

static ERROR parse_fe_merge(extSVG *Self, objVectorFilter *Filter, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_MERGEFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_X: set_double(fx, FID_X, val); break;
         case SVF_Y: set_double(fx, FID_Y, val); break;
         case SVF_WIDTH: set_double(fx, FID_Width, val); break;
         case SVF_HEIGHT: set_double(fx, FID_Height, val); break;
      }
   }

   std::vector<MergeSource> list;
   for (auto &child : Tag.Children) {
      if (!StrMatch("feMergeNode", child.name())) {
         for (LONG a=1; a < LONG(child.Attribs.size()); a++) {
            if (!StrMatch("in", child.Attribs[a].Name)) {
               switch (StrHash(child.Attribs[a].Value)) {
                  case SVF_SOURCEGRAPHIC:   list.push_back(VSF_GRAPHIC); break;
                  case SVF_SOURCEALPHA:     list.push_back(VSF_ALPHA); break;
                  case SVF_BACKGROUNDIMAGE: list.push_back(VSF_BKGD); break;
                  case SVF_BACKGROUNDALPHA: list.push_back(VSF_BKGD_ALPHA); break;
                  case SVF_FILLPAINT:       list.push_back(VSF_FILL); break;
                  case SVF_STROKEPAINT:     list.push_back(VSF_STROKE); break;
                  default:  {
                     if (auto ref = child.Attribs[a].Value.c_str()) {
                        while ((*ref) and (*ref <= 0x20)) ref++;
                        if (Self->Effects.contains(ref)) {
                           list.emplace_back(VSF_REFERENCE, Self->Effects[ref]);
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

static ERROR parse_fe_colour_matrix(extSVG *Self, objVectorFilter *Filter, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_COLOURFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_TYPE: {
            const DOUBLE *m = NULL;
            LONG mode = 0;
            switch(StrHash(val)) {
               case SVF_NONE:          mode = CM_NONE; break;
               case SVF_MATRIX:        mode = CM_MATRIX; break;
               case SVF_SATURATE:      mode = CM_SATURATE; break;
               case SVF_HUEROTATE:     mode = CM_HUE_ROTATE; break;
               case SVF_LUMINANCETOALPHA: mode = CM_LUMINANCE_ALPHA; break;
               // These are special modes that are not included by SVG
               case SVF_CONTRAST:      mode = CM_CONTRAST; break;
               case SVF_BRIGHTNESS:    mode = CM_BRIGHTNESS; break;
               case SVF_HUE:           mode = CM_HUE; break;
               case SVF_COLOURISE:     mode = CM_COLOURISE; break;
               case SVF_DESATURATE:    mode = CM_DESATURATE; break;
               // Colour blindness modes
               case SVF_PROTANOPIA:    mode = CM_MATRIX; m = glProtanopia; break;
               case SVF_PROTANOMALY:   mode = CM_MATRIX; m = glProtanomaly; break;
               case SVF_DEUTERANOPIA:  mode = CM_MATRIX; m = glDeuteranopia; break;
               case SVF_DEUTERANOMALY: mode = CM_MATRIX; m = glDeuteranomaly; break;
               case SVF_TRITANOPIA:    mode = CM_MATRIX; m = glTritanopia; break;
               case SVF_TRITANOMALY:   mode = CM_MATRIX; m = glTritanomaly; break;
               case SVF_ACHROMATOPSIA: mode = CM_MATRIX; m = glAchromatopsia; break;
               case SVF_ACHROMATOMALY: mode = CM_MATRIX; m = glAchromatomaly; break;

               default:
                  log.warning("Unrecognised colour matrix type '%s'", val.c_str());
                  FreeResource(fx);
                  return ERR_InvalidValue;
            }

            fx->set(FID_Mode, mode);
            if (mode IS CM_MATRIX) SetArray(fx, FID_Values|TDOUBLE, (APTR)m, CM_SIZE);
            break;
         }

         case SVF_VALUES: {
            auto m = read_array<DOUBLE>(val, CM_SIZE);
            SetArray(fx, FID_Values|TDOUBLE, m);
            break;
         }

         case SVF_X: set_double(fx, FID_X, val); break;

         case SVF_Y: set_double(fx, FID_Y, val); break;

         case SVF_WIDTH: set_double(fx, FID_Width, val); break;

         case SVF_HEIGHT: set_double(fx, FID_Height, val); break;

         case SVF_IN: parse_input(Self, fx, val, FID_SourceType, FID_Input); break;

         case SVF_RESULT: parse_result(Self, fx, val); break;
      }
   }

   if (!fx->init()) return ERR_Okay;
   else {
      FreeResource(fx);
      return ERR_Init;
   }
}

//********************************************************************************************************************

static ERROR parse_fe_convolve_matrix(extSVG *Self, objVectorFilter *Filter, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_CONVOLVEFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_ORDER: {
            DOUBLE ox = 0, oy = 0;
            read_numseq(val, &ox, &oy, TAGEND);
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
            read_numseq(val, &divisor, TAGEND);
            fx->set(FID_Divisor, divisor);
            break;
         }

         case SVF_BIAS: {
            DOUBLE bias = 0;
            read_numseq(val, &bias, TAGEND);
            fx->set(FID_Bias, bias);
            break;
         }

         case SVF_TARGETX: fx->set(FID_TargetX, StrToInt(val)); break;

         case SVF_TARGETY: fx->set(FID_TargetY, StrToInt(val)); break;

         case SVF_EDGEMODE:
            if (!StrMatch("duplicate", val)) fx->set(FID_EdgeMode, EM_DUPLICATE);
            else if (!StrMatch("wrap", val)) fx->set(FID_EdgeMode, EM_WRAP);
            else if (!StrMatch("none", val)) fx->set(FID_EdgeMode, EM_NONE);
            break;

         case SVF_KERNELUNITLENGTH: {
            DOUBLE kx = 1, ky = 1;
            read_numseq(val, &kx, &ky, TAGEND);
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

         case SVF_X: set_double(fx, FID_X, val); break;

         case SVF_Y: set_double(fx, FID_Y, val); break;

         case SVF_WIDTH: set_double(fx, FID_Width, val); break;

         case SVF_HEIGHT: set_double(fx, FID_Height, val); break;

         case SVF_IN: parse_input(Self, fx, val, FID_SourceType, FID_Input); break;

         case SVF_RESULT: parse_result(Self, fx, val); break;
      }
   }

   if (!fx->init()) return ERR_Okay;
   else {
      FreeResource(fx);
      return ERR_Init;
   }
}

//********************************************************************************************************************

static ERROR parse_fe_lighting(extSVG *Self, objVectorFilter *Filter, const XMLTag &Tag, LONG Type)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_LIGHTINGFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   fx->set(FID_Type, Type);

   for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_LIGHTING_COLOUR:
         case SVF_LIGHTING_COLOR: {
            FRGB rgb;
            if (!StrMatch("currentColor", val)) {
               if (current_colour(Self, Self->Scene->Viewport, rgb)) break;
            }
            else if (vecReadPainter(NULL, val.c_str(), &rgb, NULL, NULL, NULL)) break;
            SetArray(fx, FID_Colour|TFLOAT, &rgb, 4);
            break;
         }

         case SVF_KERNELUNITLENGTH: {
            DOUBLE kx = 1, ky = 1;
            read_numseq(val, &kx, &ky, TAGEND);
            if (kx < 1) kx = 1;
            if (ky < 1) ky = kx;
            fx->set(FID_UnitX, kx);
            fx->set(FID_UnitY, ky);
            break;
         }

         case SVF_SPECULARCONSTANT:
         case SVF_DIFFUSECONSTANT:  set_double(fx, FID_Constant, val); break;
         case SVF_SURFACESCALE:     set_double(fx, FID_Scale, val); break;
         case SVF_SPECULAREXPONENT: set_double(fx, FID_Exponent, val); break;

         case SVF_X:      set_double(fx, FID_X, val); break;
         case SVF_Y:      set_double(fx, FID_Y, val); break;
         case SVF_WIDTH:  set_double(fx, FID_Width, val); break;
         case SVF_HEIGHT: set_double(fx, FID_Height, val); break;
         case SVF_IN:     parse_input(Self, fx, val, FID_SourceType, FID_Input); break;
         case SVF_RESULT: parse_result(Self, fx, val); break;
         default:         log.warning("Unknown %s attribute %s", Tag.name(), Tag.Attribs[a].Name.c_str());
      }
   }

   // One child tag specifying the light source is required.

   if (!Tag.Children.empty()) {
      ERROR error;
      auto &child = Tag.Children[0];
      if (!StrCompare("feDistantLight", child.name(), 0, STR_WILDCARD)) {
         DOUBLE azimuth = 0, elevation = 0;

         for (LONG a=1; a < LONG(child.Attribs.size()); a++) {
            switch(StrHash(child.Attribs[a].Name)) {
               case SVF_AZIMUTH:   azimuth   = StrToFloat(child.Attribs[a].Value); break;
               case SVF_ELEVATION: elevation = StrToFloat(child.Attribs[a].Value); break;
            }
         }

         error = ltSetDistantLight(fx, azimuth, elevation);
      }
      else if (!StrCompare("fePointLight", child.name(), 0, STR_WILDCARD)) {
         DOUBLE x = 0, y = 0, z = 0;

         for (LONG a=1; a < LONG(child.Attribs.size()); a++) {
            switch(StrHash(child.Attribs[a].Name)) {
               case SVF_X: x = StrToFloat(child.Attribs[a].Value); break;
               case SVF_Y: y = StrToFloat(child.Attribs[a].Value); break;
               case SVF_Z: z = StrToFloat(child.Attribs[a].Value); break;
            }
         }

         error = ltSetPointLight(fx, x, y, z);
      }
      else if (!StrCompare("feSpotLight", child.name(), 0, STR_WILDCARD)) {
         DOUBLE x = 0, y = 0, z = 0, px = 0, py = 0, pz = 0;
         DOUBLE exponent = 1, cone_angle = 0;

         for (LONG a=1; a < LONG(child.Attribs.size()); a++) {
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

   if (!fx->init()) return ERR_Okay;
   else {
      FreeResource(fx);
      return ERR_Init;
   }
}

//********************************************************************************************************************

static ERROR parse_fe_displacement_map(extSVG *Self, objVectorFilter *Filter, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_DISPLACEMENTFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   for (unsigned a=1; a < Tag.Attribs.size(); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_XCHANNELSELECTOR:
            switch(val[0]) {
               case 'r': case 'R': fx->set(FID_XChannel, CMP_RED); break;
               case 'g': case 'G': fx->set(FID_XChannel, CMP_GREEN); break;
               case 'b': case 'B': fx->set(FID_XChannel, CMP_BLUE); break;
               case 'a': case 'A': fx->set(FID_XChannel, CMP_ALPHA); break;
            }
            break;

         case SVF_YCHANNELSELECTOR:
            switch(val[0]) {
               case 'r': case 'R': fx->set(FID_YChannel, CMP_RED); break;
               case 'g': case 'G': fx->set(FID_YChannel, CMP_GREEN); break;
               case 'b': case 'B': fx->set(FID_YChannel, CMP_BLUE); break;
               case 'a': case 'A': fx->set(FID_YChannel, CMP_ALPHA); break;
            }
            break;

         case SVF_SCALE: fx->set(FID_Scale, StrToFloat(val)); break;

         case SVF_X: set_double(fx, FID_X, val); break;
         case SVF_Y: set_double(fx, FID_Y, val); break;
         case SVF_WIDTH: set_double(fx, FID_Width, val); break;
         case SVF_HEIGHT: set_double(fx, FID_Height, val); break;

         case SVF_IN: parse_input(Self, fx, val, FID_SourceType, FID_Input); break;
         case SVF_IN2: parse_input(Self, fx, val, FID_MixType, FID_Mix); break;

         case SVF_RESULT: parse_result(Self, fx, val); break;
      }
   }

   if (!fx->init()) return ERR_Okay;
   else {
      FreeResource(fx);
      return ERR_Init;
   }
}

//********************************************************************************************************************

static ERROR parse_fe_component_xfer(extSVG *Self, objVectorFilter *Filter, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_REMAPFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_X:      set_double(fx, FID_X, val); break;
         case SVF_Y:      set_double(fx, FID_Y, val); break;
         case SVF_WIDTH:  set_double(fx, FID_Width, val); break;
         case SVF_HEIGHT: set_double(fx, FID_Height, val); break;
         case SVF_IN:     parse_input(Self, fx, val, FID_SourceType, FID_Input); break;
         case SVF_RESULT: parse_result(Self, fx, val); break;
      }
   }

   for (auto &child : Tag.Children) {
      if (!StrCompare("feFunc?", child.name(), 0, STR_WILDCARD)) {
         LONG cmp = 0;
         switch(child.name()[6]) {
            case 'R': cmp = CMP_RED; break;
            case 'G': cmp = CMP_GREEN; break;
            case 'B': cmp = CMP_BLUE; break;
            case 'A': cmp = CMP_ALPHA; break;
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
               case SVF_AMPLITUDE:   read_numseq(child.Attribs[a].Value, &amp, TAGEND); break;
               case SVF_INTERCEPT:   read_numseq(child.Attribs[a].Value, &intercept, TAGEND); break;
               case SVF_SLOPE:       read_numseq(child.Attribs[a].Value, &slope, TAGEND); break;
               case SVF_EXPONENT:    read_numseq(child.Attribs[a].Value, &exp, TAGEND); break;
               case SVF_OFFSET:      read_numseq(child.Attribs[a].Value, &offset, TAGEND); break;
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

   if (!fx->init()) return ERR_Okay;
   else {
      FreeResource(fx);
      return ERR_Init;
   }
}

//********************************************************************************************************************

static ERROR parse_fe_composite(extSVG *Self, objVectorFilter *Filter, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_COMPOSITEFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_MODE:
         case SVF_OPERATOR: {
            switch (StrHash(val)) {
               // SVG Operator types
               case SVF_NORMAL:
               case SVF_OVER: fx->set(FID_Operator, OP_OVER); break;
               case SVF_IN:   fx->set(FID_Operator, OP_IN); break;
               case SVF_OUT:  fx->set(FID_Operator, OP_OUT); break;
               case SVF_ATOP: fx->set(FID_Operator, OP_ATOP); break;
               case SVF_XOR:  fx->set(FID_Operator, OP_XOR); break;
               case SVF_ARITHMETIC: fx->set(FID_Operator, OP_ARITHMETIC); break;
               // SVG Mode types
               case SVF_SCREEN:   fx->set(FID_Operator, OP_SCREEN); break;
               case SVF_MULTIPLY: fx->set(FID_Operator, OP_MULTIPLY); break;
               case SVF_LIGHTEN:  fx->set(FID_Operator, OP_LIGHTEN); break;
               case SVF_DARKEN:   fx->set(FID_Operator, OP_DARKEN); break;
               // Parasol modes
               case SVF_INVERTRGB:  fx->set(FID_Operator, OP_INVERT_RGB); break;
               case SVF_INVERT:     fx->set(FID_Operator, OP_INVERT); break;
               case SVF_CONTRAST:   fx->set(FID_Operator, OP_CONTRAST); break;
               case SVF_DODGE:      fx->set(FID_Operator, OP_DODGE); break;
               case SVF_BURN:       fx->set(FID_Operator, OP_BURN); break;
               case SVF_HARDLIGHT:  fx->set(FID_Operator, OP_HARD_LIGHT); break;
               case SVF_SOFTLIGHT:  fx->set(FID_Operator, OP_SOFT_LIGHT); break;
               case SVF_DIFFERENCE: fx->set(FID_Operator, OP_DIFFERENCE); break;
               case SVF_EXCLUSION:  fx->set(FID_Operator, OP_EXCLUSION); break;
               case SVF_PLUS:       fx->set(FID_Operator, OP_PLUS); break;
               case SVF_MINUS:      fx->set(FID_Operator, OP_MINUS); break;
               case SVF_OVERLAY:    fx->set(FID_Operator, OP_OVERLAY); break;
               default:
                  log.warning("Composite operator '%s' not recognised.", val.c_str());
                  FreeResource(fx);
                  return ERR_InvalidValue;
            }
            break;
         }

         case SVF_K1: {
            DOUBLE k1;
            read_numseq(val, &k1, TAGEND);
            fx->set(FID_K1, k1);
            break;
         }

         case SVF_K2: {
            DOUBLE k2;
            read_numseq(val, &k2, TAGEND);
            fx->set(FID_K2, k2);
            break;
         }

         case SVF_K3: {
            DOUBLE k3;
            read_numseq(val, &k3, TAGEND);
            fx->set(FID_K3, k3);
            break;
         }

         case SVF_K4: {
            DOUBLE k4;
            read_numseq(val, &k4, TAGEND);
            fx->set(FID_K4, k4);
            break;
         }

         case SVF_X: set_double(fx, FID_X, val); break;

         case SVF_Y: set_double(fx, FID_Y, val); break;

         case SVF_WIDTH: set_double(fx, FID_Width, val); break;

         case SVF_HEIGHT: set_double(fx, FID_Height, val); break;

         case SVF_IN: parse_input(Self, fx, val, FID_SourceType, FID_Input); break;

         case SVF_IN2: parse_input(Self, fx, val, FID_MixType, FID_Mix); break;

         case SVF_RESULT: parse_result(Self, fx, val); break;
      }
   }

   if (!fx->init()) return ERR_Okay;
   else {
      FreeResource(fx);
      return ERR_Init;
   }
}

//********************************************************************************************************************

static ERROR parse_fe_flood(extSVG *Self, objVectorFilter *Filter, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_FLOODFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   ERROR error = ERR_Okay;
   for (LONG a=1; (a < LONG(Tag.Attribs.size())) and (!error); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_FLOOD_COLOR:
         case SVF_FLOOD_COLOUR: {
            FRGB rgb;
            if (!StrMatch("currentColor", val)) {
               if (current_colour(Self, Self->Scene->Viewport, rgb)) break;
            }
            else if (vecReadPainter(NULL, val.c_str(), &rgb, NULL, NULL, NULL)) break;
            error = SetArray(fx, FID_Colour|TFLOAT, &rgb, 4);
            break;
         }

         case SVF_FLOOD_OPACITY: {
            DOUBLE opacity;
            read_numseq(val, &opacity, TAGEND);
            error = fx->set(FID_Opacity, opacity);
            break;
         }

         case SVF_X: set_double(fx, FID_X, val); break;

         case SVF_Y: set_double(fx, FID_Y, val); break;

         case SVF_WIDTH: set_double(fx, FID_Width, val); break;

         case SVF_HEIGHT: set_double(fx, FID_Height, val); break;

         case SVF_IN: parse_input(Self, fx, val, FID_SourceType, FID_Input); break;

         case SVF_RESULT: parse_result(Self, fx, val); break;
      }
   }

   if (!error) return fx->init();
   else {
      FreeResource(fx);
      return log.warning(error);
   }
}

//********************************************************************************************************************

static ERROR parse_fe_turbulence(extSVG *Self, objVectorFilter *Filter, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_TURBULENCEFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_BASEFREQUENCY: {
            DOUBLE bfx = -1, bfy = -1;
            read_numseq(val, &bfx, &bfy, TAGEND);
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
            if (!StrMatch("fractalNoise", val)) fx->set(FID_Type, TB_NOISE);
            else fx->set(FID_Type, 0);
            break;

         case SVF_X: set_double(fx, FID_X, val); break;

         case SVF_Y: set_double(fx, FID_Y, val); break;

         case SVF_WIDTH: set_double(fx, FID_Width, val); break;

         case SVF_HEIGHT: set_double(fx, FID_Height, val); break;

         case SVF_IN: parse_input(Self, fx, val, FID_SourceType, FID_Input); break;

         case SVF_RESULT: parse_result(Self, fx, val); break;
      }
   }


   if (!fx->init()) return ERR_Okay;
   else {
      FreeResource(fx);
      return ERR_Init;
   }
}

//********************************************************************************************************************

static ERROR parse_fe_morphology(extSVG *Self, objVectorFilter *Filter, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_MORPHOLOGYFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_RADIUS: {
            DOUBLE x = -1, y = -1;
            read_numseq(val, &x, &y, TAGEND);
            if (x > 0) fx->set(FID_RadiusX, F2T(x));
            if (y > 0) fx->set(FID_RadiusY, F2T(y));
            break;
         }

         case SVF_OPERATOR: fx->set(FID_Operator, val); break;

         case SVF_X: set_double(fx, FID_X, val); break;

         case SVF_Y: set_double(fx, FID_Y, val); break;

         case SVF_WIDTH: set_double(fx, FID_Width, val); break;

         case SVF_HEIGHT: set_double(fx, FID_Height, val); break;

         case SVF_IN: parse_input(Self, fx, val, FID_SourceType, FID_Input); break;

         case SVF_RESULT: parse_result(Self, fx, val); break;
      }
   }

   if (!fx->init()) return ERR_Okay;
   else {
      FreeResource(fx);
      return ERR_Init;
   }
}

//********************************************************************************************************************
// This code replaces feImage elements where the href refers to a resource name.

static ERROR parse_fe_source(extSVG *Self, objXML *XML, svgState &State, objVectorFilter *Filter, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objFilterEffect *fx;

   if (NewObject(ID_SOURCEFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   bool required = false;
   std::string ref;

   ERROR error = ERR_Okay;
   for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_X: set_double(fx, FID_X, val); break;
         case SVF_Y: set_double(fx, FID_Y, val); break;
         case SVF_WIDTH: set_double(fx, FID_Width, val); break;
         case SVF_HEIGHT: set_double(fx, FID_Height, val); break;
         case SVF_PRESERVEASPECTRATIO: fx->set(FID_AspectRatio, parse_aspect_ratio(val)); break;
         case SVF_XLINK_HREF: ref = val; break;
         case SVF_EXTERNALRESOURCESREQUIRED: required = StrMatch("true", val) IS ERR_Okay; break;
         case SVF_RESULT: parse_result(Self, fx, val); break;
      }
   }

   objVector *vector = NULL;
   if (!ref.empty()) {
      if (scFindDef(Self->Scene, ref.c_str(), (OBJECTPTR *)&vector) != ERR_Okay) {
         // The reference is not an existing vector but should be a pre-registered declaration that would allow
         // us to create it.  Note that creation only occurs once.  Subsequent use of the ID will result in the
         // live reference being found.

         if (auto tagref = find_href_tag(Self, ref)) {
            xtag_default(Self, XML, State, *tagref, Self->Scene, &vector);
         }
         else log.warning("Element id '%s' not found.", ref.c_str());
      }

      if (vector) {
         fx->set(FID_SourceName, ref);
         if (!(error = fx->init())) return ERR_Okay;
      }
      else error = ERR_Search;
   }
   else error = ERR_UndefinedField;

   FreeResource(fx);
   if (required) return log.warning(error);
   return ERR_Okay; // Default behaviour is not to force a failure despite the error.
}

//********************************************************************************************************************

static ERROR parse_fe_image(extSVG *Self, objXML *XML, svgState &State, objVectorFilter *Filter, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   // Check if the client has specified an href that refers to a pattern name instead of an image file.  In that
   // case we need to divert to the SourceFX parser.

   for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
      if ((!StrMatch("xlink:href", Tag.Attribs[a].Name)) or (!StrMatch("href", Tag.Attribs[a].Name))) {
         if ((Tag.Attribs[a].Value[0] IS '#')) {
            return parse_fe_source(Self, XML, State, Filter, Tag);
         }
         break;
      }
   }

   objFilterEffect *fx;
   if (NewObject(ID_IMAGEFX, &fx) != ERR_Okay) return ERR_NewObject;
   SetOwner(fx, Filter);

   bool image_required = false;
   std::string path;

   for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
      auto &val = Tag.Attribs[a].Value;
      if (val.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_X: set_double(fx, FID_X, val); break;

         case SVF_Y: set_double(fx, FID_Y, val); break;

         case SVF_WIDTH: set_double(fx, FID_Width, val); break;

         case SVF_HEIGHT: set_double(fx, FID_Height, val); break;

         case SVF_IMAGE_RENDERING: {
            if (!StrMatch("optimizeSpeed", val)) fx->set(FID_ResampleMethod, VSM_BILINEAR);
            else if (!StrMatch("optimizeQuality", val)) fx->set(FID_ResampleMethod, VSM_LANCZOS3);
            else if (!StrMatch("auto", val));
            else if (!StrMatch("inherit", val));
            else log.warning("Unrecognised image-rendering option '%s'", val.c_str());
            break;
         }

         case SVF_PRESERVEASPECTRATIO: fx->set(FID_AspectRatio, parse_aspect_ratio(val)); break;

         case SVF_XLINK_HREF: path = val; break;

         case SVF_EXTERNALRESOURCESREQUIRED: // If true and the image cannot be loaded, return a fatal error code.
            if (!StrMatch("true", val)) image_required = true;
            break;

         case SVF_RESULT: parse_result(Self, fx, val); break;
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

         for (UWORD i=0; path[i]; i++) {
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
   else return ERR_Okay;
}

//********************************************************************************************************************

static void xtag_filter(extSVG *Self, objXML *XML, svgState &State, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   objVectorFilter *filter;
   std::string id;

   if (!NewObject(ID_VECTORFILTER, &filter)) {
      filter->setFields(fl::Owner(Self->Scene->UID), fl::Name("SVGFilter"),
         fl::Units(VUNIT_BOUNDING_BOX), fl::ColourSpace(VCS_LINEAR_RGB));

      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
         auto &val = Tag.Attribs[a].Value;
         if (val.empty()) continue;

         LONG j;
         for (j=0; Tag.Attribs[a].Name[j] and (Tag.Attribs[a].Name[j] != ':'); j++);
         if (Tag.Attribs[a].Name[j] IS ':') continue;

         switch(StrHash(Tag.Attribs[a].Name)) {
            case SVF_FILTERUNITS:
               if (!StrMatch("userSpaceOnUse", val)) filter->Units = VUNIT_USERSPACE;
               else if (!StrMatch("objectBoundingBox", val)) filter->Units = VUNIT_BOUNDING_BOX;
               break;

            case SVF_ID:      if (add_id(Self, Tag, val)) id = val; break;

            case SVF_X:       set_double(filter, FID_X, val); break;

            case SVF_Y:       set_double(filter, FID_Y, val); break;

            case SVF_WIDTH:   set_double(filter, FID_Width, val); break;

            case SVF_HEIGHT:  set_double(filter, FID_Height, val); break;

            case SVF_OPACITY: set_double(filter, FID_Opacity, val); break;

            case SVF_FILTERRES: {
               DOUBLE x = 0, y = 0;
               read_numseq(val, &x, &y, TAGEND);
               filter->setFields(fl::ResX(F2T(x)), fl::ResY(F2T(y)));
               break;
            }

            case SVF_COLOR_INTERPOLATION_FILTERS: // The default is linearRGB
               if (!StrMatch("auto", val)) filter->set(FID_ColourSpace, VCS_LINEAR_RGB);
               else if (!StrMatch("sRGB", val)) filter->set(FID_ColourSpace, VCS_SRGB);
               else if (!StrMatch("linearRGB", val)) filter->set(FID_ColourSpace, VCS_LINEAR_RGB);
               else if (!StrMatch("inherit", val)) filter->set(FID_ColourSpace, VCS_INHERIT);
               break;

            case SVF_PRIMITIVEUNITS:
               if (!StrMatch("userSpaceOnUse", val)) filter->PrimitiveUnits = VUNIT_USERSPACE; // Default
               else if (!StrMatch("objectBoundingBox", val)) filter->PrimitiveUnits = VUNIT_BOUNDING_BOX;
               break;

/*
            case SVF_VIEWBOX: {
               DOUBLE x=0, y=0, width=0, height=0;
               read_numseq(val, &x, &y, &width, &height, TAGEND);
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
               case SVF_FEBLEND:             // Blend and composite share the same code.
               case SVF_FECOMPOSITE:         parse_fe_composite(Self, filter, child); break;
               case SVF_FEFLOOD:             parse_fe_flood(Self, filter, child); break;
               case SVF_FETURBULENCE:        parse_fe_turbulence(Self, filter, child); break;
               case SVF_FEMORPHOLOGY:        parse_fe_morphology(Self, filter, child); break;
               case SVF_FEIMAGE:             parse_fe_image(Self, XML, State, filter, child); break;
               case SVF_FECOMPONENTTRANSFER: parse_fe_component_xfer(Self, filter, child); break;
               case SVF_FEDIFFUSELIGHTING:   parse_fe_lighting(Self, filter, child, LT_DIFFUSE); break;
               case SVF_FESPECULARLIGHTING:  parse_fe_lighting(Self, filter, child, LT_SPECULAR); break;
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

         scAddDef(Self->Scene, id.c_str(), filter);
      }
      else FreeResource(filter);
   }
}

//********************************************************************************************************************
// NB: In bounding-box mode, the default view-box is 0 0 1 1, where 1 is equivalent to 100% of the target space.
// If the client sets a custom view-box then the dimensions are fixed, and no scaling will apply.

static void process_pattern(extSVG *Self, objXML *XML, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objVectorPattern *pattern;
   std::string id;

   if (!NewObject(ID_VECTORPATTERN, &pattern)) {
      SetOwner(pattern, Self->Scene);
      pattern->setFields(fl::Name("SVGPattern"),
         fl::Units(VUNIT_BOUNDING_BOX),
         fl::SpreadMethod(VSPREAD_REPEAT),
         fl::HostScene(Self->Scene));

      objVectorViewport *viewport;
      pattern->getPtr(FID_Viewport, &viewport);

      bool client_set_viewbox = false;
      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
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

               if (!StrMatch("userSpaceOnUse", val)) pattern->ContentUnits = VUNIT_USERSPACE;
               else if (!StrMatch("objectBoundingBox", val)) pattern->ContentUnits = VUNIT_BOUNDING_BOX;
               break;

            case SVF_PATTERNUNITS:
               if (!StrMatch("userSpaceOnUse", val)) pattern->Units = VUNIT_USERSPACE;
               else if (!StrMatch("objectBoundingBox", val)) pattern->Units = VUNIT_BOUNDING_BOX;
               break;

            case SVF_PATTERNTRANSFORM: pattern->set(FID_Transform, val); break;

            case SVF_ID:       id = val; break;

            case SVF_OVERFLOW: viewport->set(FID_Overflow, val); break;

            case SVF_OPACITY:  set_double(pattern, FID_Opacity, val); break;

            case SVF_X:        set_double(pattern, FID_X, val); break;

            case SVF_Y:        set_double(pattern, FID_Y, val); break;

            case SVF_WIDTH:    set_double(pattern, FID_Width, val); break;

            case SVF_HEIGHT:   set_double(pattern, FID_Height, val); break;

            case SVF_VIEWBOX: {
               DOUBLE vx=0, vy=0, vwidth=1, vheight=1; // Default view-box for bounding-box mode
               client_set_viewbox = true;
               pattern->ContentUnits = VUNIT_USERSPACE;
               read_numseq(val, &vx, &vy, &vwidth, &vheight, TAGEND);
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
         svgState state;
         process_children(Self, XML, state, Tag, viewport);
         add_id(Self, Tag, id);
         scAddDef(Self->Scene, id.c_str(), pattern);
      }
      else {
         FreeResource(pattern);
         log.trace("Pattern initialisation failed.");
      }
   }
}

//********************************************************************************************************************

static ERROR process_shape(extSVG *Self, CLASSID VectorID, objXML *XML, svgState &State, const XMLTag &Tag,
   OBJECTPTR Parent, objVector **Result)
{
   pf::Log log(__FUNCTION__);
   ERROR error;
   objVector *vector;

   *Result = NULL;
   if (!(error = NewObject(VectorID, &vector))) {
      SetOwner(vector, Parent);
      svgState state = State;
      apply_state(state, vector);
      if (!Tag.Children.empty()) set_state(state, Tag); // Apply all attribute values to the current state.

      process_attrib(Self, XML, Tag, vector);

      if (!vector->init()) {
         // Process child tags, if any

         for (auto &child : Tag.Children) {
            if (child.isTag()) {
               switch(StrHash(child.name())) {
                  case SVF_ANIMATETRANSFORM: xtag_animatetransform(Self, XML, child, vector); break;
                  case SVF_ANIMATEMOTION:    xtag_animatemotion(Self, XML, child, vector); break;
                  case SVF_PARASOL_MORPH:    xtag_morph(Self, XML, child, vector); break;
                  case SVF_TEXTPATH:
                     if (VectorID IS ID_VECTORTEXT) {
                        if (!child.Children.empty()) {
                           char buffer[8192];
                           if (!xmlGetContent(XML, child.ID, buffer, sizeof(buffer))) {
                              LONG ws;
                              for (ws=0; (buffer[ws]) and (buffer[ws] <= 0x20); ws++); // All leading whitespace is ignored.
                              vector->set(FID_String, buffer + ws);
                           }
                           else log.msg("Failed to retrieve content for <text> @ line %d", Tag.LineNo);
                        }

                        xtag_morph(Self, XML, child, vector);
                     }
                     break;
                  default:
                     log.warning("Failed to interpret vector child element <%s/> @ line %d", child.name(), child.LineNo);
                     break;
               }
            }
         }

         *Result = vector;
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

static ERROR xtag_default(extSVG *Self, objXML *XML, svgState &State, const XMLTag &Tag, OBJECTPTR Parent, objVector **Vector)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("%s", Tag.name());

   switch(StrHash(Tag.name())) {
      case SVF_USE:              xtag_use(Self, XML, State, Tag, Parent); break;
      case SVF_G:                xtag_group(Self, XML, State, Tag, Parent, Vector); break;
      case SVF_SVG:              xtag_svg(Self, XML, State, Tag, Parent, Vector); break;
      case SVF_RECT:             process_shape(Self, ID_VECTORRECTANGLE, XML, State, Tag, Parent, Vector); break;
      case SVF_ELLIPSE:          process_shape(Self, ID_VECTORELLIPSE, XML, State, Tag, Parent, Vector); break;
      case SVF_CIRCLE:           process_shape(Self, ID_VECTORELLIPSE, XML, State, Tag, Parent, Vector); break;
      case SVF_PATH:             process_shape(Self, ID_VECTORPATH, XML, State, Tag, Parent, Vector); break;
      case SVF_POLYGON:          process_shape(Self, ID_VECTORPOLYGON, XML, State, Tag, Parent, Vector); break;
      case SVF_PARASOL_SPIRAL:   process_shape(Self, ID_VECTORSPIRAL, XML, State, Tag, Parent, Vector); break;
      case SVF_PARASOL_WAVE:     process_shape(Self, ID_VECTORWAVE, XML, State, Tag, Parent, Vector); break;
      case SVF_PARASOL_SHAPE:    process_shape(Self, ID_VECTORSHAPE, XML, State, Tag, Parent, Vector); break;
      case SVF_IMAGE:            xtag_image(Self, XML, State, Tag, Parent, Vector); break;
      case SVF_CONTOURGRADIENT:  xtag_contourgradient(Self, Tag); break;
      case SVF_RADIALGRADIENT:   xtag_radialgradient(Self, Tag); break;
      case SVF_DIAMONDGRADIENT:  xtag_diamondgradient(Self, Tag); break;
      case SVF_CONICGRADIENT:    xtag_conicgradient(Self, Tag); break;
      case SVF_LINEARGRADIENT:   xtag_lineargradient(Self, Tag); break;
      case SVF_SYMBOL:           xtag_symbol(Self, XML, Tag); break;
      case SVF_ANIMATETRANSFORM: xtag_animatetransform(Self, XML, Tag, Parent); break;
      case SVF_FILTER:           xtag_filter(Self, XML, State, Tag); break;
      case SVF_DEFS:             xtag_defs(Self, XML, State, Tag, Parent); break;
      case SVF_CLIPPATH:         xtag_clippath(Self, XML, Tag); break;
      case SVF_STYLE:            xtag_style(Self, XML, Tag); break;
      case SVF_PATTERN:          process_pattern(Self, XML, Tag); break;

      case SVF_TITLE:
         if (Self->Title) { FreeResource(Self->Title); Self->Title = NULL; }
         if (!Tag.Children.empty()) {
            char buffer[8192];
            if (!xmlGetContent(XML, Tag.ID, buffer, sizeof(buffer))) {
               LONG ws;
               for (ws=0; buffer[ws] and (buffer[ws] <= 0x20); ws++); // All leading whitespace is ignored.
               Self->Title = StrClone(buffer+ws);
            }
         }
         break;

      case SVF_LINE:
         process_shape(Self, ID_VECTORPOLYGON, XML, State, Tag, Parent, Vector);
         Vector[0]->set(FID_Closed, FALSE);
         break;

      case SVF_POLYLINE:
         process_shape(Self, ID_VECTORPOLYGON, XML, State, Tag, Parent, Vector);
         Vector[0]->set(FID_Closed, FALSE);
         break;

      case SVF_TEXT: {
         if (!process_shape(Self, ID_VECTORTEXT, XML, State, Tag, Parent, Vector)) {
            if (!Tag.Children.empty()) {
               char buffer[8192];
               STRING str;
               LONG ws = 0;
               if ((!Vector[0]->get(FID_String, &str)) and (str)) {
                  ws = StrCopy(str, buffer, sizeof(buffer));
               }

               if (!xmlGetContent(XML, Tag.ID, buffer + ws, sizeof(buffer) - ws)) {
                  if (!ws) while (buffer[ws] and (buffer[ws] <= 0x20)) ws++; // All leading whitespace is ignored.
                  else ws = 0;
                  Vector[0]->set(FID_String, buffer + ws);
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

static ERROR load_pic(extSVG *Self, std::string Path, objPicture **Picture)
{
   pf::Log log(__FUNCTION__);

   *Picture = NULL;
   objFile *file = NULL;
   auto val = Path.c_str();

   ERROR error = ERR_Okay;
   if (!StrCompare("data:", val, 5, 0)) { // Check for embedded content
      log.branch("Detected embedded source data");
      val += 5;
      if (!StrCompare("image/", val, 6, 0)) { // Has to be an image type
         val += 6;
         while ((*val) and (*val != ';')) val++;
         if (!StrCompare(";base64", val, 7, 0)) { // Is it base 64?
            val += 7;
            while ((*val) and (*val != ',')) val++;
            if (*val IS ',') val++;

            pfBase64Decode state;
            ClearMemory(&state, sizeof(state));

            UBYTE *output;
            LONG size = strlen(val);
            if (!AllocMemory(size, MEM_DATA|MEM_NO_CLEAR, &output)) {
               LONG written;
               if (!(error = Base64Decode(&state, val, size, output, &written))) {
                  Path = "temp:svg.img";
                  if ((file = objFile::create::integral(fl::Path(Path), fl::Flags(FL_NEW|FL_WRITE)))) {
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
         fl::Flags(PCF_FORCE_ALPHA_32)))) error = ERR_CreateObject;
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

static void def_image(extSVG *Self, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objVectorImage *image;
   std::string id;
   objPicture *pic = NULL;

   if (!NewObject(ID_VECTORIMAGE, &image)) {
      image->setFields(fl::Owner(Self->Scene->UID),
         fl::Name("SVGImage"),
         fl::Units(VUNIT_BOUNDING_BOX),
         fl::SpreadMethod(VSPREAD_PAD));

      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
         auto &val = Tag.Attribs[a].Value;
         if (val.empty()) continue;

         switch(StrHash(Tag.Attribs[a].Name)) {
            case SVF_UNITS:
               if (!StrMatch("userSpaceOnUse", val)) image->Units = VUNIT_USERSPACE;
               else if (!StrMatch("objectBoundingBox", val)) image->Units = VUNIT_BOUNDING_BOX;
               break;

            case SVF_XLINK_HREF: load_pic(Self, val, &pic); break;
            case SVF_ID: id = val; break;
            case SVF_X:  set_double(image, FID_X, val); break;
            case SVF_Y:  set_double(image, FID_Y, val); break;
            default: {
               // Check if this was a reference to some other namespace (ignorable).
               LONG i;
               for (i=0; val[i] and (val[i] != ':'); i++);
               if (val[i] != ':') log.warning("Failed to parse attrib '%s' in <image/> tag @ line %d", Tag.Attribs[a].Name.c_str(), Tag.LineNo);
               break;
            }
         }
      }

      if (!id.empty()) {
         if (pic) {
            image->set(FID_Picture, pic);
            if (!InitObject(image)) {
               add_id(Self, Tag, id);
               scAddDef(Self->Scene, id.c_str(), image);
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
         log.trace("No id specified in <image/> at line %d", Tag.LineNo);
      }
   }
}

//********************************************************************************************************************

static ERROR xtag_image(extSVG *Self, objXML *XML, svgState &State, const XMLTag &Tag, OBJECTPTR Parent, objVector **Vector)
{
   pf::Log log(__FUNCTION__);
   LONG ratio = 0;
   bool width_set = false;
   bool height_set = false;
   svgState state = State;
   objPicture *pic = NULL;

   for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
      if (!StrMatch("xlink:href", Tag.Attribs[a].Name)) {
         load_pic(Self, Tag.Attribs[a].Value, &pic);
      }
      else if (!StrMatch("preserveAspectRatio", Tag.Attribs[a].Name)) {
         ratio = parse_aspect_ratio(Tag.Attribs[a].Value);
      }
      else if (!StrMatch("width", Tag.Attribs[a].Name)) {
         width_set = true;
      }
      else if (!StrMatch("height", Tag.Attribs[a].Name)) {
         height_set = true;
      }
   }

   // Load the image and add it to the vector definition.  It will be rendered as a rectangle within the scene.
   // This may appear a little confusing as an image can be invoked in SVG like a first-class shape, however to
   // treat them as such would be out of step with all other scene graph members being true path-based objects.

   if (pic) {
      if (auto image = objVectorImage::create::global(
            fl::Owner(Self->Scene->UID),
            fl::Picture(pic),
            fl::SpreadMethod(VSPREAD_PAD),
            fl::Units(VUNIT_BOUNDING_BOX),
            fl::AspectRatio(ratio))) {

         SetOwner(pic, image); // It's best if the pic belongs to the image.

         auto id = std::to_string(image->UID);
         id.insert(0, "img");
         scAddDef(Self->Scene, id.c_str(), image);

         std::string fillname("url(#");
         fillname.append(id);
         fillname.append(")");

         // Use a rectangle shape to represent the image

         process_shape(Self, ID_VECTORRECTANGLE, XML, state, Tag, Parent, Vector);
         Vector[0]->set(FID_Fill, "none");

         if (!width_set) Vector[0]->set(FID_Width, pic->Bitmap->Width);
         if (!height_set) Vector[0]->set(FID_Height, pic->Bitmap->Height);
         Vector[0]->set(FID_Fill, fillname);
         return ERR_Okay;
      }
      else return ERR_Failed;
   }
   else log.warning("Failed to load picture via xlink:href.");

   return ERR_Failed;
}

//********************************************************************************************************************

static ERROR xtag_defs(extSVG *Self, objXML *XML, svgState &State, const XMLTag &Tag, OBJECTPTR Parent)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("Tag: %d", Tag.ID);

   for (auto &child : Tag.Children) {
      switch (StrHash(child.name())) {
         case SVF_CONTOURGRADIENT: xtag_contourgradient(Self, child); break;
         case SVF_RADIALGRADIENT:  xtag_radialgradient(Self, child); break;
         case SVF_DIAMONDGRADIENT: xtag_diamondgradient(Self, child); break;
         case SVF_CONICGRADIENT:   xtag_conicgradient(Self, child); break;
         case SVF_LINEARGRADIENT:  xtag_lineargradient(Self, child); break;
         case SVF_PATTERN:         process_pattern(Self, XML, child); break;
         case SVF_IMAGE:           def_image(Self, child); break;
         case SVF_FILTER:          xtag_filter(Self, XML, State, child); break;
         case SVF_CLIPPATH:        xtag_clippath(Self, XML, child); break;
         case SVF_PARASOL_TRANSITION: xtag_pathtransition(Self, XML, child); break;

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

static ERROR xtag_style(extSVG *Self, objXML *XML, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   ERROR error = ERR_Okay;

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

   STRING css_buffer;
   LONG css_size = 256 * 1024;
   if (!AllocMemory(css_size, MEM_DATA|MEM_STRING|MEM_NO_CLEAR, &css_buffer)) {
      if (!(error = xmlGetContent(XML, Tag.ID, css_buffer, css_size))) {
         if (auto css = katana_parse(css_buffer, StrLength(css_buffer), KatanaParserModeStylesheet)) {
            /*#ifdef DEBUG
               Self->CSS->mode = KatanaParserModeStylesheet;
               katana_dump_output(css);
            #endif*/

            // For each rule in the stylesheet, apply them to the loaded XML document by injecting tags and attributes.
            // The stylesheet attributes have precedence over inline tag attributes (therefore we can overwrite matching
            // attribute names) however they are outranked by inline styles.

            KatanaStylesheet *sheet = css->stylesheet;

            log.msg("%d CSS rules will be applied", sheet->imports.length + sheet->rules.length);

            for (unsigned i = 0; i < sheet->imports.length; ++i) {
               if (sheet->imports.data[i])
                  process_rule(Self, XML, XML->Tags, (KatanaRule *)sheet->imports.data[i]);
            }

            for (unsigned i=0; i < sheet->rules.length; ++i) {
               if (sheet->rules.data[i])
                  process_rule(Self, XML, XML->Tags, (KatanaRule *)sheet->rules.data[i]);
            }

            katana_destroy_output(css);
         }
      }
      FreeResource(css_buffer);
   }
   else error = ERR_AllocMemory;

   return error;
}

//********************************************************************************************************************
// Declare a 'symbol' which is basically a template for inclusion elsewhere through the use of a 'use' element.
//
// When a use element is encountered, it looks for the associated symbol ID and then processes the XML child tags that
// belong to it.

static void xtag_symbol(extSVG *Self, objXML *XML, const XMLTag &Tag)
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

/*********************************************************************************************************************
** Most vector shapes can be morphed to the path of another vector.
*/

static void xtag_morph(extSVG *Self, objXML *XML, const XMLTag &Tag, OBJECTPTR Parent)
{
   pf::Log log(__FUNCTION__);

   if ((!Parent) or (Parent->ClassID != ID_VECTOR)) {
      log.traceWarning("Unable to apply morph to non-vector parent object.");
      return;
   }

   // Find the definition that is being referenced for the morph.

   std::string offset;
   std::string ref;
   std::string transition;
   LONG flags = 0;
   for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
      auto &val = Tag.Attribs[a].Value;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_PATH:
         case SVF_XLINK_HREF:  ref = val; break;
         case SVF_TRANSITION:  transition = val; break;
         case SVF_STARTOFFSET: offset = val; break;
         case SVF_METHOD:
            if (!StrMatch("align", val)) flags &= ~VMF_STRETCH;
            else if (!StrMatch("stretch", val)) flags |= VMF_STRETCH;
            break;

         case SVF_SPACING:
            if (!StrMatch("auto", val)) flags |= VMF_AUTO_SPACING;
            else if (!StrMatch("exact", val)) flags &= ~VMF_AUTO_SPACING;
            break;

         case SVF_ALIGN:
            flags |= parse_aspect_ratio(val);
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

   if (!(flags & (VMF_Y_MIN|VMF_Y_MID|VMF_Y_MAX))) {
      if (Parent->SubID IS ID_VECTORTEXT) flags |= VMF_Y_MIN;
      else flags |= VMF_Y_MID;
   }

   if (class_id) {
      objVector *shape;
      svgState state;
      process_shape(Self, class_id, XML, state, tagref, Self->Scene, &shape);
      Parent->set(FID_Morph, shape);
      if (transvector) Parent->set(FID_Transition, transvector);
      Parent->set(FID_MorphFlags, flags);
      scAddDef(Self->Scene, uri.c_str(), shape);
   }
}

//********************************************************************************************************************
// Duplicates a referenced area of the SVG definition.
//
// "The effect of a 'use' element is as if the contents of the referenced element were deeply cloned into a separate
// non-exposed DOM tree which had the 'use' element as its parent and all of the 'use' element's ancestors as its
// higher-level ancestors.

static void xtag_use(extSVG *Self, objXML *XML, svgState &State, const XMLTag &Tag, OBJECTPTR Parent)
{
   pf::Log log(__FUNCTION__);
   std::string ref;

   for (LONG a=1; (a < LONG(Tag.Attribs.size())) and (ref.empty()); a++) {
      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_HREF: // SVG2
         case SVF_XLINK_HREF: ref = Tag.Attribs[a].Value; break;
      }
   }

   if (ref.empty()) {
      log.warning("<use> element @ line %d is missing a valid xlink:href attribute.", Tag.LineNo);
      return;
   }

   // Find the matching element with matching ID

   auto tagref = find_href_tag(Self, ref);
   if (!tagref) {
      log.warning("Unable to find element '%s'", ref.c_str());
      return;
   }

   objVector *vector = NULL;

   auto state = State;
   set_state(state, Tag); // Apply all attribute values to the current state.

   if ((!StrMatch("symbol", tagref->name())) or (!StrMatch("svg", tagref->name()))) {
      // SVG spec requires that we create a VectorGroup and then create a Viewport underneath that.  However if there
      // are no attributes to apply to the group then there is no sense in creating an empty one.

      objVector *group;
      bool need_group = false;
      for (LONG a=1; (a < LONG(Tag.Attribs.size())) and (!need_group); a++) {
         switch(StrHash(Tag.Attribs[a].Name)) {
            case SVF_X: case SVF_Y: case SVF_WIDTH: case SVF_HEIGHT: break;
            default: need_group = TRUE; break;
         }
      }

      if (need_group) {
         if (!NewObject(ID_VECTORGROUP, &group)) {
            SetOwner(group, Parent);
            Parent = group;
            group->init();
         }
      }

      if (NewObject(ID_VECTORVIEWPORT, &vector)) return;
      SetOwner(vector, Parent);
      vector->setFields(fl::Width(PERCENT(1.0)), fl::Height(PERCENT(1.0))); // SVG default

      // Apply attributes from 'use' to the group and/or viewport
      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
         auto &val = Tag.Attribs[a].Value;
         if (val.empty()) continue;

         auto hash = StrHash(Tag.Attribs[a].Name);
         switch(hash) {
            // X,Y,Width,Height are applied to the viewport
            case SVF_X: set_double(vector, FID_X, val); break;
            case SVF_Y: set_double(vector, FID_Y, val); break;
            case SVF_WIDTH:  set_double(vector, FID_Width, val); break;
            case SVF_HEIGHT: set_double(vector, FID_Height, val); break;

            // All other attributes are applied to the 'g' element
            default:
               if (group) set_property(Self, group, hash, XML, Tag, val);
               else set_property(Self, vector, hash, XML, Tag, val);
               break;
         }
      }

      // Apply attributes from the symbol itself to the viewport

      for (unsigned a=1; a < tagref->Attribs.size(); a++) {
         auto &val = tagref->Attribs[a].Value;
         if (val.empty()) continue;

         switch(StrHash(tagref->Attribs[a].Name)) {
            case SVF_X:      set_double(vector, FID_X, val); break;
            case SVF_Y:      set_double(vector, FID_Y, val); break;
            case SVF_WIDTH:  set_double(vector, FID_Width, val); break;
            case SVF_HEIGHT: set_double(vector, FID_Height, val); break;
            case SVF_VIEWBOX:  {
               DOUBLE x=0, y=0, width=0, height=0;
               read_numseq(val, &x, &y, &width, &height, TAGEND);
               vector->setFields(fl::ViewX(x), fl::ViewY(y), fl::ViewWidth(width), fl::ViewHeight(height));
               break;
            }
            case SVF_ID: break; // Ignore (already processed).
            default: log.warning("Not processing attribute '%s'", tagref->Attribs[a].Name.c_str()); break;
         }
      }

      if (vector->init() != ERR_Okay) { FreeResource(vector); return; }

      // Add all child elements in <symbol> to the viewport.

      log.traceBranch("Processing all child elements within %s", ref.c_str());
      process_children(Self, XML, state, *tagref, vector);
   }
   else {
      // Rather than creating a vanilla group with a child viewport, this optimal approach creates the viewport only.
      if (!NewObject(ID_VECTORVIEWPORT, &vector)) {
         SetOwner(vector, Parent);
         apply_state(state, vector);
         process_attrib(Self, XML, Tag, vector); // Apply 'use' attributes to the group.

         if (vector->init() != ERR_Okay) { FreeResource(vector); return; }

         objVector *sibling = NULL;
         xtag_default(Self, XML, state, *tagref, vector, &sibling);
      }
   }
}

//********************************************************************************************************************

static void xtag_group(extSVG *Self, objXML *XML, svgState &State, const XMLTag &Tag, OBJECTPTR Parent, objVector **Vector)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("Tag: %d", Tag.ID);

   auto state = State;

   objVector *group;
   if (NewObject(ID_VECTORGROUP, &group) != ERR_Okay) return;
   SetOwner(group, Parent);
   if (!Tag.Children.empty()) set_state(state, Tag); // Apply all group attribute values to the current state.
   process_attrib(Self, XML, Tag, group);

   // Process child tags

   objVector *sibling = NULL;
   for (auto &child : Tag.Children) {
      if (child.isTag()) {
         xtag_default(Self, XML, state, child, group, &sibling);
      }
   }

   if (!group->init()) *Vector = group;
   else FreeResource(group);
}

/*********************************************************************************************************************
** <svg/> tags can be embedded inside <svg/> tags - this establishes a new viewport.
** Refer to section 7.9 of the SVG Specification for more information.
*/

static void xtag_svg(extSVG *Self, objXML *XML, svgState &State, const XMLTag &Tag, OBJECTPTR Parent, objVector **Vector)
{
   pf::Log log(__FUNCTION__);
   LONG a;

   if (!Parent) {
      log.warning("A Parent object is required.");
      return;
   }

   objVectorViewport *viewport;
   if (NewObject(ID_VECTORVIEWPORT, &viewport)) return;
   SetOwner(viewport, Parent);

   // The first viewport to be instantiated is stored as a local reference.  This is important if the developer has
   // specified a custom target, in which case there needs to be a way to discover the root of the SVG.

   if (!Self->Viewport) Self->Viewport = viewport;
   // Process <svg> attributes

   auto state = State;
   if (!Tag.Children.empty()) set_state(state, Tag); // Apply all attribute values to the current state.

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
            DOUBLE version = StrToFloat(val);
            if (version > Self->SVGVersion) Self->SVGVersion = version;
            break;
         }

         case SVF_X: set_double(viewport, FID_X, val); break;
         case SVF_Y: set_double(viewport, FID_Y, val); break;

         case SVF_WIDTH:
            set_double(viewport, FID_Width, val);
            viewport->set(FID_OverflowX, VOF_HIDDEN);
            break;

         case SVF_HEIGHT:
            set_double(viewport, FID_Height, val);
            viewport->set(FID_OverflowY, VOF_HIDDEN);
            break;

         case SVF_PRESERVEASPECTRATIO:
            viewport->set(FID_AspectRatio, parse_aspect_ratio(val));
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
            case SVF_DEFS: xtag_defs(Self, XML, state, child, viewport); break;
            default:       xtag_default(Self, XML, state, child, viewport, &sibling);  break;
         }
      }
   }

   if (!viewport->init()) *Vector = viewport;
   else FreeResource(viewport);
}

//********************************************************************************************************************
// <animateTransform attributeType="XML" attributeName="transform" type="rotate" from="0,150,150" to="360,150,150"
//   begin="0s" dur="5s" repeatCount="indefinite"/>

static ERROR xtag_animatetransform(extSVG *Self, objXML *XML, const XMLTag &Tag, OBJECTPTR Parent)
{
   pf::Log log(__FUNCTION__);

   Self->Animated = true;

   svgAnimation anim;
   anim.Replace = false;
   anim.TargetVector = Parent->UID;

   for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
      auto &value = Tag.Attribs[a].Value;
      if (value.empty()) continue;

      switch(StrHash(Tag.Attribs[a].Name)) {
         case SVF_ATTRIBUTENAME: // Name of the target attribute affected by the From and To values.
            anim.TargetAttribute = value;
            break;

         case SVF_ATTRIBUTETYPE: // Namespace of the target attribute: XML, CSS, auto
            if (!StrMatch("XML", value));
            else if (!StrMatch("CSS", value));
            else if (!StrMatch("auto", value));
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
            while ((v < LONG(value.size())) and (LONG(anim.Values.size()) < MAX_VALUES)) {
               while ((value[v]) and (value[v] <= 0x20)) v++;
               for (s=v; (value[s]) and (value[s] != ';'); s++);
               anim.Values.push_back(value.substr(s, v-s));
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

static ERROR xtag_animatemotion(extSVG *Self, objXML *XML, const XMLTag &Tag, OBJECTPTR Parent)
{
   Self->Animated = true;

   for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
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

static void process_attrib(extSVG *Self, objXML *XML, const XMLTag &Tag, objVector *Vector)
{
   pf::Log log(__FUNCTION__);

   for (unsigned t=1; t < Tag.Attribs.size(); t++) {
      if (Tag.Attribs[t].Value.empty()) continue;

      // Do not interpret non-SVG attributes, e.g. 'inkscape:dx'

      {
         LONG j;
         for (j=0; Tag.Attribs[t].Name[j] and (Tag.Attribs[t].Name[j] != ':'); j++);
         if (Tag.Attribs[t].Name[j] IS ':') continue;
      }

      log.trace("%s = %.40s", Tag.Attribs[t].Name.c_str(), Tag.Attribs[t].Value.c_str());

      if (auto error = set_property(Self, Vector, StrHash(Tag.Attribs[t].Name), XML, Tag, Tag.Attribs[t].Value)) {
         if (Vector->SubID != ID_VECTORGROUP) {
            log.warning("Failed to set field '%s' with '%s' in %s; Error %s",
               Tag.Attribs[t].Name.c_str(), Tag.Attribs[t].Value.c_str(), Vector->Class->ClassName, GetErrorMsg(error));
         }
      }
   }
}

//********************************************************************************************************************
// Apply all attributes in a rule to a target tag.

static void apply_rule(extSVG *Self, objXML *XML, KatanaArray *Properties, XMLTag &Tag)
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

static void process_rule(extSVG *Self, objXML *XML, objXML::TAGS &Tags, KatanaRule *Rule)
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
                        apply_rule(Self, XML, sr->declarations, tag);
                     }

                     if (!tag.Children.empty()) {
                        process_rule(Self, XML, tag.Children, Rule);
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
                              apply_rule(Self, XML, sr->declarations, tag);
                           }
                           break;
                        }
                     }

                     if (!tag.Children.empty()) {
                        process_rule(Self, XML, tag.Children, Rule);
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

static ERROR set_property(extSVG *Self, objVector *Vector, ULONG Hash, objXML *XML, const XMLTag &Tag, const std::string StrValue)
{
   pf::Log log(__FUNCTION__);
   DOUBLE num;

   // Ignore stylesheet attributes
   if (Hash IS SVF_CLASS) return ERR_Okay;

   switch(Vector->SubID) {
      case ID_VECTORVIEWPORT: {
         FIELD field_id = 0;
         switch (Hash) {
            // The following 'view-*' fields are for defining the SVG view box
            case SVF_VIEW_X:      field_id = FID_ViewX; break;
            case SVF_VIEW_Y:      field_id = FID_ViewY; break;
            case SVF_VIEW_WIDTH:  field_id = FID_ViewWidth; break;
            case SVF_VIEW_HEIGHT: field_id = FID_ViewHeight; break;
            // The following dimension fields are for defining the position and clipping of the vector display
            case SVF_X:      field_id = FID_X; break;
            case SVF_Y:      field_id = FID_Y; break;
            case SVF_WIDTH:  field_id = FID_Width; break;
            case SVF_HEIGHT: field_id = FID_Height; break;
         }

         if (field_id) {
            num = read_unit(StrValue, &field_id);
            SetField(Vector, field_id, num);
            return ERR_Okay;
         }
         break;
      }

      case ID_VECTORELLIPSE: {
         FIELD field_id = 0;
         switch (Hash) {
            case SVF_CX: field_id = FID_CenterX; break;
            case SVF_CY: field_id = FID_CenterY; break;
            case SVF_R:  field_id = FID_Radius; break;
            case SVF_RX: field_id = FID_RadiusX; break;
            case SVF_RY: field_id = FID_RadiusY; break;
            case SVF_VERTICES: field_id = FID_Vertices; break;
         }

         if (field_id) {
            num = read_unit(StrValue, &field_id);
            SetField(Vector, field_id, num);
            return ERR_Okay;
         }
         break;
      }

      case ID_VECTORWAVE: {
         FIELD field_id = 0;
         switch (Hash) {
            case SVF_CLOSE: Vector->set(FID_Close, StrValue); return ERR_Okay;
            case SVF_AMPLITUDE: field_id = FID_Amplitude; break;
            case SVF_DECAY: field_id = FID_Decay; break;
            case SVF_FREQUENCY: field_id = FID_Frequency; break;
            case SVF_THICKNESS: field_id = FID_Thickness; break;
         }

         if (field_id) {
            num = read_unit(StrValue, &field_id);
            SetField(Vector, field_id, num);
            return ERR_Okay;
         }
         break;
      }

      case ID_VECTORRECTANGLE: {
         FIELD field_id = 0;
         switch (Hash) {
            case SVF_X1:
            case SVF_X:      field_id = FID_X; break;
            case SVF_Y1:
            case SVF_Y:      field_id = FID_Y; break;
            case SVF_WIDTH:  field_id = FID_Width; break;
            case SVF_HEIGHT: field_id = FID_Height; break;
            case SVF_RX:     field_id = FID_RoundX; break;
            case SVF_RY:     field_id = FID_RoundY; break;

            case SVF_X2: {
               DOUBLE x;
               field_id = FID_Width;
               Vector->get(FID_X, &x);
               num = read_unit(StrValue, &field_id);
               Vector->set(field_id, std::abs(num - x));
               return ERR_Okay;
            }

            case SVF_Y2: {
               DOUBLE y;
               field_id = FID_Height;
               Vector->get(FID_Y, &y);
               num = read_unit(StrValue, &field_id);
               Vector->set(field_id, std::abs(num - y));
               return ERR_Okay;
            }
         }

         if (field_id) {
            num = read_unit(StrValue, &field_id);
            SetField(Vector, field_id, num);
            return ERR_Okay;
         }

         break;
      }

      // VectorPolygon handles polygon, polyline and line.
      case ID_VECTORPOLYGON: {
         switch (Hash) {
            case SVF_POINTS: Vector->set(FID_Points, StrValue); return ERR_Okay;
         }
         break;
      }

      case ID_VECTORTEXT: {
         switch (Hash) {
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
               #warning Add support for text font attribute
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
                  case SVF_NORMAL:          Vector->set(FID_Stretch, VTS_NORMAL); return ERR_Okay;
                  case SVF_WIDER:           Vector->set(FID_Stretch, VTS_WIDER); return ERR_Okay;
                  case SVF_NARROWER:        Vector->set(FID_Stretch, VTS_NARROWER); return ERR_Okay;
                  case SVF_ULTRA_CONDENSED: Vector->set(FID_Stretch, VTS_ULTRA_CONDENSED); return ERR_Okay;
                  case SVF_EXTRA_CONDENSED: Vector->set(FID_Stretch, VTS_EXTRA_CONDENSED); return ERR_Okay;
                  case SVF_CONDENSED:       Vector->set(FID_Stretch, VTS_CONDENSED); return ERR_Okay;
                  case VTS_SEMI_CONDENSED:  Vector->set(FID_Stretch, VTS_SEMI_CONDENSED); return ERR_Okay;
                  case VTS_EXPANDED:        Vector->set(FID_Stretch, VTS_EXPANDED); return ERR_Okay;
                  case VTS_SEMI_EXPANDED:   Vector->set(FID_Stretch, VTS_SEMI_EXPANDED); return ERR_Okay;
                  case VTS_EXTRA_EXPANDED:  Vector->set(FID_Stretch, VTS_EXTRA_EXPANDED); return ERR_Okay;
                  case VTS_ULTRA_EXPANDED:  Vector->set(FID_Stretch, VTS_ULTRA_EXPANDED); return ERR_Okay;
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
                  case SVF_START:   Vector->set(FID_Align, ALIGN_LEFT); return ERR_Okay;
                  case SVF_MIDDLE:  Vector->set(FID_Align, ALIGN_HORIZONTAL); return ERR_Okay;
                  case SVF_END:     Vector->set(FID_Align, ALIGN_RIGHT); return ERR_Okay;
                  case SVF_INHERIT: Vector->set(FID_Align, 0); return ERR_Okay;
                  default: log.warning("text-anchor: No support for value '%s'", StrValue.c_str());
               }
               break;

            case SVF_TEXTLENGTH: Vector->set(FID_TextLength, StrValue); return ERR_Okay;
            // TextPath only
            //case SVF_STARTOFFSET: Vector->set(FID_StartOffset, StrValue); return ERR_Okay;
            //case SVF_METHOD: // The default is align.  For 'stretch' mode, set VMF_STRETCH in MorphFlags
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
                  case SVF_UNDERLINE:    Vector->set(FID_Flags, VTXF_UNDERLINE); return ERR_Okay;
                  case SVF_OVERLINE:     Vector->set(FID_Flags, VTXF_OVERLINE); return ERR_Okay;
                  case SVF_LINETHROUGH:  Vector->set(FID_Flags, VTXF_LINE_THROUGH); return ERR_Okay;
                  case SVF_BLINK:        Vector->set(FID_Flags, VTXF_BLINK); return ERR_Okay;
                  case SVF_INHERIT:      return ERR_Okay;
                  default: log.warning("No support for text-decoration value '%s'", StrValue.c_str());
               }
               return ERR_Okay;
         }
         break;
      }

      case ID_VECTORSPIRAL: {
         FIELD field_id = 0;
         switch (Hash) {
            case SVF_PATHLENGTH: Vector->set(FID_PathLength, StrValue); return ERR_Okay;
            case SVF_CX: field_id = FID_CenterX; break;
            case SVF_CY: field_id = FID_CenterY; break;
            case SVF_R:  field_id = FID_Radius; break;
            case SVF_SCALE:    field_id = FID_Scale; break;
            case SVF_OFFSET:   field_id = FID_Offset; break;
            case SVF_STEP:     field_id = FID_Step; break;
            case SVF_VERTICES: field_id = FID_Vertices; break;
         }

         if (field_id) {
            num = read_unit(StrValue, &field_id);
            SetField(Vector, field_id, num);
            return ERR_Okay;
         }
         break;
      }

      case ID_VECTORSHAPE: {
         FIELD field_id = 0;
         switch (Hash) {
            case SVF_CX:   field_id = FID_CenterX; break;
            case SVF_CY:   field_id = FID_CenterY; break;
            case SVF_R:    field_id = FID_Radius; break;
            case SVF_N1:   field_id = FID_N1; break;
            case SVF_N2:   field_id = FID_N2; break;
            case SVF_N3:   field_id = FID_N3; break;
            case SVF_M:    field_id = FID_M; break;
            case SVF_A:    field_id = FID_A; break;
            case SVF_B:    field_id = FID_B; break;
            case SVF_PHI:  field_id = FID_Phi; break;
            case SVF_VERTICES: field_id = FID_Vertices; break;
            case SVF_MOD:  field_id = FID_Mod; break;
            case SVF_SPIRAL: field_id = FID_Spiral; break;
            case SVF_REPEAT: field_id = FID_Repeat; break;
            case SVF_CLOSE:
               if ((!StrMatch("true", StrValue)) or (!StrMatch("1", StrValue))) Vector->set(FID_Close, TRUE);
               else Vector->set(FID_Close, FALSE);
               break;
         }

         if (field_id) {
            num = read_unit(StrValue, &field_id);
            SetField(Vector, field_id, num);
            return ERR_Okay;
         }
         break;
      }

      case ID_VECTORPATH: {
         switch (Hash) {
            case SVF_D: Vector->set(FID_Sequence, StrValue); return ERR_Okay;
            case SVF_PATHLENGTH: Vector->set(FID_PathLength, StrValue); return ERR_Okay;
         }
         break;
      }
   }

   // Fall-through to generic attributes.

   FIELD field_id = 0;
   switch (Hash) {
      case SVF_X:  field_id = FID_X; break;
      case SVF_Y:  field_id = FID_Y; break;
      case SVF_X1: field_id = FID_X1; break;
      case SVF_Y1: field_id = FID_Y1; break;
      case SVF_X2: field_id = FID_X2; break;
      case SVF_Y2: field_id = FID_Y2; break;
      case SVF_WIDTH:  field_id = FID_Width; break;
      case SVF_HEIGHT: field_id = FID_Height; break;

      case SVF_TRANSITION: {
         OBJECTPTR trans = NULL;
         if (!scFindDef(Self->Scene, StrValue.c_str(), &trans)) Vector->set(FID_Transition, trans);
         else log.warning("Unable to find element '%s' referenced at line %d", StrValue.c_str(), Tag.LineNo);
         break;
      }

      case SVF_COLOUR_INTERPOLATION:
      case SVF_COLOR_INTERPOLATION: {
         if (!StrMatch("auto", StrValue)) Vector->set(FID_ColourSpace, VCS_SRGB);
         else if (!StrMatch("sRGB", StrValue)) Vector->set(FID_ColourSpace, VCS_SRGB);
         else if (!StrMatch("linearRGB", StrValue)) Vector->set(FID_ColourSpace, VCS_LINEAR_RGB);
         else if (!StrMatch("inherit", StrValue)) Vector->set(FID_ColourSpace, VCS_INHERIT);
         else log.warning("Invalid color-interpolation value '%s' at line %d", StrValue.c_str(), Tag.LineNo);
      }

      case SVF_STROKE_LINEJOIN: {
         switch(StrHash(StrValue)) {
            case SVF_MITER: Vector->set(FID_LineJoin, VLJ_MITER); break;
            case SVF_ROUND: Vector->set(FID_LineJoin, VLJ_ROUND); break;
            case SVF_BEVEL: Vector->set(FID_LineJoin, VLJ_BEVEL); break;
            case SVF_INHERIT: Vector->set(FID_LineJoin, VLJ_INHERIT); break;
            case SVF_MITER_REVERT: Vector->set(FID_LineJoin, VLJ_MITER_REVERT); break; // Special AGG only join type
            case SVF_MITER_ROUND: Vector->set(FID_LineJoin, VLJ_MITER_ROUND); break; // Special AGG only join type
         }
         break;
      }

      case SVF_STROKE_INNERJOIN: // AGG ONLY
         switch(StrHash(StrValue)) {
            case SVF_MITER:   Vector->set(FID_InnerJoin, VIJ_MITER);  break;
            case SVF_ROUND:   Vector->set(FID_InnerJoin, VIJ_ROUND); break;
            case SVF_BEVEL:   Vector->set(FID_InnerJoin, VIJ_BEVEL); break;
            case SVF_INHERIT: Vector->set(FID_InnerJoin, VIJ_INHERIT); break;
            case SVF_JAG:     Vector->set(FID_InnerJoin, VIJ_JAG); break;
         }

      case SVF_STROKE_LINECAP:
         switch(StrHash(StrValue)) {
            case SVF_BUTT:    Vector->set(FID_LineCap, VLC_BUTT); break;
            case SVF_SQUARE:  Vector->set(FID_LineCap, VLC_SQUARE); break;
            case SVF_ROUND:   Vector->set(FID_LineCap, VLC_ROUND); break;
            case SVF_INHERIT: Vector->set(FID_LineCap, VLC_INHERIT); break;
         }
         break;

      case SVF_VISIBILITY:
         if (!StrMatch("visible", StrValue))       Vector->set(FID_Visibility, VIS_VISIBLE);
         else if (!StrMatch("hidden", StrValue))   Vector->set(FID_Visibility, VIS_HIDDEN);
         else if (!StrMatch("collapse", StrValue)) Vector->set(FID_Visibility, VIS_COLLAPSE); // Same effect as hidden, kept for SVG compatibility
         else if (!StrMatch("inherit", StrValue))  Vector->set(FID_Visibility, VIS_INHERIT);
         else log.warning("Unsupported visibility value '%s'", StrValue.c_str());
         break;

      case SVF_FILL_RULE:
         if (!StrMatch("nonzero", StrValue)) Vector->set(FID_FillRule, VFR_NON_ZERO);
         else if (!StrMatch("evenodd", StrValue)) Vector->set(FID_FillRule, VFR_EVEN_ODD);
         else if (!StrMatch("inherit", StrValue)) Vector->set(FID_FillRule, VFR_INHERIT);
         else log.warning("Unsupported fill-rule value '%s'", StrValue.c_str());
         break;

      case SVF_CLIP_RULE:
         if (!StrMatch("nonzero", StrValue)) Vector->set(FID_ClipRule, VFR_NON_ZERO);
         else if (!StrMatch("evenodd", StrValue)) Vector->set(FID_ClipRule, VFR_EVEN_ODD);
         else if (!StrMatch("inherit", StrValue)) Vector->set(FID_ClipRule, VFR_INHERIT);
         else log.warning("Unsupported clip-rule value '%s'", StrValue.c_str());
         break;

      case SVF_ENABLE_BACKGROUND:
         if (!StrMatch("new", StrValue)) Vector->set(FID_EnableBkgd, TRUE);
         break;

      case SVF_ID:
         Vector->set(FID_ID, StrValue);
         add_id(Self, Tag, StrValue);
         scAddDef(Self->Scene, StrValue.c_str(), Vector);
         SetName(Vector, StrValue.c_str());
         break;

      case SVF_NUMERIC_ID:       Vector->set(FID_NumericID, StrValue); break;
      case SVF_DISPLAY:          log.warning("display is not supported."); break;
      case SVF_OVERFLOW: // visible | hidden | scroll | auto | inherit
         log.trace("overflow is not supported.");
         break;
      case SVF_MARKER:           log.warning("marker is not supported."); break;
      case SVF_MARKER_END:       log.warning("marker-end is not supported."); break;
      case SVF_MARKER_MID:       log.warning("marker-mid is not supported."); break;
      case SVF_MARKER_START:     log.warning("marker-start is not supported."); break;

      case SVF_FILTER:           Vector->set(FID_Filter, StrValue); break;
      case SVF_COLOR:            Vector->set(FID_Fill, StrValue); break;

      case SVF_STROKE:
         if (!StrMatch("currentColor", StrValue)) {
            FRGB rgb;
            if (!current_colour(Self, Vector, rgb)) SetArray(Vector, FID_Stroke|TFLOAT, &rgb, 4);
         }
         else Vector->set(FID_Stroke, StrValue);
         break;


      case SVF_FILL:
         if (!StrMatch("currentColor", StrValue)) {
            FRGB rgb;
            if (!current_colour(Self, Vector, rgb)) SetArray(Vector, FID_Fill|TFLOAT, &rgb, 4);
         }
         else Vector->set(FID_Fill, StrValue);
         break;

      case SVF_TRANSFORM: {
         if (Vector->ClassID IS ID_VECTOR) {
            VectorMatrix *matrix;
            if (!vecNewMatrix((objVector *)Vector, &matrix)) {
               vecParseTransform(matrix, StrValue.c_str());
            }
            else log.warning("Failed to create vector transform matrix.");
         }
         break;
      }
      case SVF_STROKE_DASHARRAY: Vector->set(FID_DashArray, StrValue); break;
      case SVF_OPACITY:          Vector->set(FID_Opacity, StrValue); break;
      case SVF_FILL_OPACITY:     Vector->set(FID_FillOpacity, StrToFloat(StrValue)); break;
      case SVF_SHAPE_RENDERING:  Vector->set(FID_PathQuality, shape_rendering_to_render_quality(StrValue)); break;

      case SVF_STROKE_WIDTH:            field_id = FID_StrokeWidth; break;
      case SVF_STROKE_OPACITY:          Vector->set(FID_StrokeOpacity, StrValue); break;
      case SVF_STROKE_MITERLIMIT:       Vector->set(FID_MiterLimit, StrValue); break;
      case SVF_STROKE_MITERLIMIT_THETA: Vector->set(FID_MiterLimitTheta, StrValue); break;
      case SVF_STROKE_INNER_MITERLIMIT: Vector->set(FID_InnerMiterLimit, StrValue); break;
      case SVF_STROKE_DASHOFFSET:       field_id = FID_DashOffset; break;

      case SVF_MASK: {
         auto tagref = find_href_tag(Self, StrValue);
         if (!tagref) {
            log.warning("Unable to find mask '%s'", StrValue.c_str());
            return ERR_Search;
         }

         // TODO: We need to add code that converts the content of a <mask> tag into a VectorFilter, because masking can be
         // achieved through filters.  There is no need for a dedicated masking class for this task.
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

   if (field_id) {
      num = read_unit(StrValue, &field_id);
      SetField(Vector, field_id, num);
   }

   return ERR_Okay;
}
