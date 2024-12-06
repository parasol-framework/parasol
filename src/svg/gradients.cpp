
static ERR gradient_defaults(extSVG *Self, objVectorGradient *Gradient, ULONG Attrib, const std::string Value)
{
   switch (Attrib) {
      case SVF_COLOR_INTERPOLATION:
         if (iequals("auto", Value)) Gradient->setColourSpace(VCS::LINEAR_RGB);
         else if (iequals("sRGB", Value)) Gradient->setColourSpace(VCS::SRGB);
         else if (iequals("linearRGB", Value)) Gradient->setColourSpace(VCS::LINEAR_RGB);
         else if (iequals("inherit", Value)) Gradient->setColourSpace(VCS::INHERIT);
         return ERR::Okay;

      case SVF_HREF:
      case SVF_XLINK_HREF: add_inherit(Self, Gradient, Value);
         return ERR::Okay;
   }

   return ERR::Failed;
}

//********************************************************************************************************************
// Note that all offsets are percentages.

static const std::vector<GradientStop> process_gradient_stops(extSVG *Self, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch();

   std::vector<GradientStop> stops;
   for (auto &scan : Tag.Children) {
      if (iequals("stop", scan.name())) {
         GradientStop stop;
         DOUBLE stop_opacity = 1.0;
         stop.Offset = 0;
         stop.RGB.Red   = 0;
         stop.RGB.Green = 0;
         stop.RGB.Blue  = 0;
         stop.RGB.Alpha = 0;

         for (LONG a=1; a < std::ssize(scan.Attribs); a++) {
            auto &name  = scan.Attribs[a].Name;
            auto &value = scan.Attribs[a].Value;
            if (value.empty()) continue;

            if (iequals("offset", name)) {
               stop.Offset = strtod(value.c_str(), NULL);
               for (LONG j=0; value[j]; j++) {
                  if (value[j] IS '%') {
                     stop.Offset = stop.Offset * 0.01; // Must be in the range of 0 - 1.0
                     break;
                  }
               }

               if (stop.Offset < 0.0) stop.Offset = 0;
               else if (stop.Offset > 1.0) stop.Offset = 1.0;
            }
            else if (iequals("stop-color", name)) {
               VectorPainter painter;
               vec::ReadPainter(Self->Scene, value.c_str(), &painter, NULL);
               stop.RGB = painter.Colour;
            }
            else if (iequals("stop-opacity", name)) {
               stop_opacity = strtod(value.c_str(), NULL);
            }
            else if (iequals("id", name)) {
               log.trace("Use of id attribute in <stop/> ignored.");
            }
            else log.warning("Unable to process stop attribute '%s'", name.c_str());
         }

         stop.RGB.Alpha = ((DOUBLE)stop.RGB.Alpha) * stop_opacity;

         stops.emplace_back(stop);
      }
      else log.warning("Unknown element in gradient, '%s'", scan.name());
   }

   return stops;
}

//********************************************************************************************************************

static ERR xtag_lineargradient(extSVG *Self, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objVectorGradient *gradient;

   std::string id;

   if (NewObject(CLASSID::VECTORGRADIENT, &gradient) IS ERR::Okay) {
      SetOwner(gradient, Self->Scene);
      gradient->setFields(
         fl::Name("SVGLinearGrad"),
         fl::Type(VGT::LINEAR),
         fl::X1(0.0),
         fl::Y1(0.0),
         fl::X2(SCALE(1.0)),
         fl::Y2(0.0));

      // Determine the user coordinate system first.

      gradient->Units = VUNIT::BOUNDING_BOX;
      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
         if (iequals("gradientUnits", Tag.Attribs[a].Name)) {
            if (iequals("userSpaceOnUse", Tag.Attribs[a].Value)) gradient->Units = VUNIT::USERSPACE;
            break;
         }
      }

      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
         auto &val = Tag.Attribs[a].Value;
         if (val.empty()) continue;

         auto attrib = strihash(Tag.Attribs[a].Name);
         switch(attrib) {
            case SVF_GRADIENTUNITS: break; // Already checked gradientUnits earlier
            case SVF_GRADIENTTRANSFORM: gradient->setTransform(val); break;
            case SVF_X1: set_double_units(gradient, FID_X1, val, gradient->Units); break;
            case SVF_Y1: set_double_units(gradient, FID_Y1, val, gradient->Units); break;
            case SVF_X2: set_double_units(gradient, FID_X2, val, gradient->Units); break;
            case SVF_Y2: set_double_units(gradient, FID_Y2, val, gradient->Units); break;

            case SVF_COLOR_INTERPOLATION:
               if (iequals("auto", val)) gradient->setColourSpace(VCS::LINEAR_RGB);
               else if (iequals("sRGB", val)) gradient->setColourSpace(VCS::SRGB);
               else if (iequals("linearRGB", val)) gradient->setColourSpace(VCS::LINEAR_RGB);
               else if (iequals("inherit", val)) gradient->setColourSpace(VCS::INHERIT);
               break;

            case SVF_SPREADMETHOD: {
               if (iequals("pad", val))          gradient->setSpreadMethod(VSPREAD::PAD);
               else if (iequals("reflect", val)) gradient->setSpreadMethod(VSPREAD::REFLECT);
               else if (iequals("repeat", val))  gradient->setSpreadMethod(VSPREAD::REPEAT);
               break;
            }
            case SVF_ID: id = val; break;

            default: {
               if (gradient_defaults(Self, gradient, attrib, val) != ERR::Okay) {
                  if (Tag.Attribs[a].Name.find(':') != std::string::npos) break;
                  log.warning("%s attribute '%s' unrecognised @ line %d", Tag.name(), Tag.Attribs[a].Name.c_str(), Tag.LineNo);
               }
               break;
            }
         }
      }

      auto stops = process_gradient_stops(Self, Tag);
      if (stops.size() >= 2) SetArray(gradient, FID_Stops, stops);

      if (InitObject(gradient) IS ERR::Okay) {
         if (!id.empty()) {
            SetName(gradient, id.c_str());
            add_id(Self, Tag, id);
            track_object(Self, gradient);
            return Self->Scene->addDef(id.c_str(), gradient);
         }
         else return ERR::Okay;
      }
      else return ERR::Init;
   }
   else return ERR::NewObject;
}

//********************************************************************************************************************

static ERR xtag_radialgradient(extSVG *Self, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objVectorGradient *gradient;
   std::string id;

   if (NewObject(CLASSID::VECTORGRADIENT, &gradient) IS ERR::Okay) {
      SetOwner(gradient, Self->Scene);

      gradient->setFields(fl::Name("SVGRadialGrad"), fl::Type(VGT::RADIAL),
         fl::CenterX(SCALE(0.5)), fl::CenterY(SCALE(0.5)), fl::Radius(SCALE(0.5)));

      // Determine the user coordinate system first.

      gradient->Units = VUNIT::BOUNDING_BOX;
      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
         if (iequals("gradientUnits", Tag.Attribs[a].Name)) {
            if (iequals("userSpaceOnUse", Tag.Attribs[a].Value)) gradient->Units = VUNIT::USERSPACE;
            break;
         }
      }

      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
         auto &val = Tag.Attribs[a].Value;
         if (val.empty()) continue;
         log.trace("Processing radial gradient attribute %s = %s", Tag.Attribs[a].Name, val);

         auto attrib = strihash(Tag.Attribs[a].Name);
         switch(attrib) {
            case SVF_CX: set_double_units(gradient, FID_CenterX, val, gradient->Units); break;
            case SVF_CY: set_double_units(gradient, FID_CenterY, val, gradient->Units); break;
            case SVF_FX: set_double_units(gradient, FID_FX, val, gradient->Units); break;
            case SVF_FY: set_double_units(gradient, FID_FY, val, gradient->Units); break;
            case SVF_R:  set_double_units(gradient, FID_Radius, val, gradient->Units); break;
            case SVF_GRADIENTUNITS: break; // Already processed
            case SVF_GRADIENTTRANSFORM: gradient->setTransform(val); break;
            case SVF_ID: id = val; break;
            case SVF_SPREADMETHOD:
               if (iequals("pad", val))          gradient->setSpreadMethod(VSPREAD::PAD);
               else if (iequals("reflect", val)) gradient->setSpreadMethod(VSPREAD::REFLECT);
               else if (iequals("repeat", val))  gradient->setSpreadMethod(VSPREAD::REPEAT);
               break;

            default: {
               if (gradient_defaults(Self, gradient, attrib, val) != ERR::Okay) {
                  if (Tag.Attribs[a].Name.find(':') != std::string::npos) break;
                  log.warning("%s attribute '%s' unrecognised @ line %d", Tag.name(), Tag.Attribs[a].Name.c_str(), Tag.LineNo);
               }
            }
         }
      }

      auto stops = process_gradient_stops(Self, Tag);
      if (stops.size() >= 2) SetArray(gradient, FID_Stops, stops);

      if (InitObject(gradient) IS ERR::Okay) {
         if (!id.empty()) {
            SetName(gradient, id.c_str());
            add_id(Self, Tag, id);
            track_object(Self, gradient);
            return Self->Scene->addDef(id.c_str(), gradient);
         }
         else return ERR::Okay;
      }
      else return ERR::Init;
   }
   else return ERR::NewObject;
}

//********************************************************************************************************************

static ERR xtag_diamondgradient(extSVG *Self, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objVectorGradient *gradient;
   std::string id;

   if (NewObject(CLASSID::VECTORGRADIENT, &gradient) IS ERR::Okay) {
      SetOwner(gradient, Self->Scene);

      gradient->setFields(fl::Name("SVGDiamondGrad"), fl::Type(VGT::DIAMOND),
         fl::CenterX(SCALE(0.5)), fl::CenterY(SCALE(0.5)), fl::Radius(SCALE(0.5)));

      // Determine the user coordinate system first.

      gradient->Units = VUNIT::BOUNDING_BOX;
      for (LONG a=1; a < std::ssize(Tag.Attribs); a++) {
         if (iequals("gradientUnits", Tag.Attribs[a].Name)) {
            if (iequals("userSpaceOnUse", Tag.Attribs[a].Value)) gradient->Units = VUNIT::USERSPACE;
            break;
         }
      }

      for (LONG a=1; a < std::ssize(Tag.Attribs); a++) {
         auto &val = Tag.Attribs[a].Value;
         if (val.empty()) continue;

         log.trace("Processing diamond gradient attribute %s =  %s", Tag.Attribs[a].Name, val);

         auto attrib = strihash(Tag.Attribs[a].Name);
         switch(attrib) {
            case SVF_GRADIENTUNITS: break; // Already processed
            case SVF_GRADIENTTRANSFORM: gradient->setTransform(val); break;
            case SVF_CX: set_double_units(gradient, FID_CenterX, val, gradient->Units); break;
            case SVF_CY: set_double_units(gradient, FID_CenterY, val, gradient->Units); break;
            case SVF_R:  set_double_units(gradient, FID_Radius, val, gradient->Units); break;
            case SVF_SPREADMETHOD: {
               if (iequals("pad", val))          gradient->setSpreadMethod(VSPREAD::PAD);
               else if (iequals("reflect", val)) gradient->setSpreadMethod(VSPREAD::REFLECT);
               else if (iequals("repeat", val))  gradient->setSpreadMethod(VSPREAD::REPEAT);
               break;
            }
            case SVF_ID: id = val; break;
            default: {
               if (gradient_defaults(Self, gradient, attrib, val) != ERR::Okay) {
                  if (Tag.Attribs[a].Name.find(':') != std::string::npos) break;
                  log.warning("%s attribute '%s' unrecognised @ line %d", Tag.name(), Tag.Attribs[a].Name.c_str(), Tag.LineNo);
               }
            }
         }
      }

      auto stops = process_gradient_stops(Self, Tag);
      if (stops.size() >= 2) SetArray(gradient, FID_Stops, stops);

      if (InitObject(gradient) IS ERR::Okay) {
         if (!id.empty()) {
            SetName(gradient, id.c_str());
            add_id(Self, Tag, id);
            track_object(Self, gradient);
            return Self->Scene->addDef(id.c_str(), gradient);
         }
         else return ERR::Okay;
      }
      else return ERR::Init;
   }
   else return ERR::NewObject;
}

//********************************************************************************************************************
// NB: Contour gradients are not part of the SVG standard.

static ERR xtag_contourgradient(extSVG *Self, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objVectorGradient *gradient;
   std::string id;

   if (NewObject(CLASSID::VECTORGRADIENT, &gradient) IS ERR::Okay) {
      SetOwner(gradient, Self->Scene);
      gradient->setFields(fl::Name("SVGContourGrad"), fl::Type(VGT::CONTOUR));

      // Determine the user coordinate system first.

      gradient->Units = VUNIT::BOUNDING_BOX;
      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
         if (iequals("gradientUnits", Tag.Attribs[a].Name)) {
            if (iequals("userSpaceOnUse", Tag.Attribs[a].Value)) gradient->Units = VUNIT::USERSPACE;
            break;
         }
      }

      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
         auto &val = Tag.Attribs[a].Value;
         if (val.empty()) continue;

         log.trace("Processing contour gradient attribute %s =  %s", Tag.Attribs[a].Name, val);

         auto attrib = strihash(Tag.Attribs[a].Name);
         switch(attrib) {
            case SVF_GRADIENTUNITS: break; // Already processed
            case SVF_GRADIENTTRANSFORM: gradient->setTransform(val); break;
            case SVF_X1: set_double_units(gradient, FID_X1, val, gradient->Units); break;
            case SVF_X2: set_double_units(gradient, FID_X2, val, gradient->Units); break;
            case SVF_SPREADMETHOD: {
               if (iequals("pad", val))          gradient->setSpreadMethod(VSPREAD::PAD);
               else if (iequals("reflect", val)) gradient->setSpreadMethod(VSPREAD::REFLECT);
               else if (iequals("repeat", val))  gradient->setSpreadMethod(VSPREAD::REPEAT);
               break;
            }
            case SVF_ID: id = val; break;
            default: {
               if (gradient_defaults(Self, gradient, attrib, val) != ERR::Okay) {
                  if (Tag.Attribs[a].Name.find(':') != std::string::npos) break;
                  log.warning("%s attribute '%s' unrecognised @ line %d", Tag.name(), Tag.Attribs[a].Name.c_str(), Tag.LineNo);
               }
            }
         }
      }

      auto stops = process_gradient_stops(Self, Tag);
      if (stops.size() >= 2) SetArray(gradient, FID_Stops, stops);

      if (InitObject(gradient) IS ERR::Okay) {
         if (!id.empty()) {
            SetName(gradient, id.c_str());
            add_id(Self, Tag, id);
            track_object(Self, gradient);
            return Self->Scene->addDef(id.c_str(), gradient);
         }
         else return ERR::Okay;
      }
      else return ERR::Init;
   }
   else return ERR::NewObject;
}

//********************************************************************************************************************

static ERR xtag_conicgradient(extSVG *Self, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objVectorGradient *gradient;

   if (NewObject(CLASSID::VECTORGRADIENT, &gradient) IS ERR::Okay) {
      SetOwner(gradient, Self->Scene);

      gradient->setFields(fl::Name("SVGConicGrad"), fl::Type(VGT::CONIC),
         fl::CenterX(SCALE(0.5)), fl::CenterY(SCALE(0.5)), fl::Radius(SCALE(0.5)));

      std::string id;

      // Determine the user coordinate system first.

      gradient->Units = VUNIT::BOUNDING_BOX;
      for (LONG a=1; a < std::ssize(Tag.Attribs); a++) {
         if (iequals("gradientUnits", Tag.Attribs[a].Name)) {
            if (iequals("userSpaceOnUse", Tag.Attribs[a].Value)) gradient->Units = VUNIT::USERSPACE;
            break;
         }
      }

      for (LONG a=1; a < std::ssize(Tag.Attribs); a++) {
         auto &val = Tag.Attribs[a].Value;
         if (val.empty()) continue;

         log.trace("Processing diamond gradient attribute %s =  %s", Tag.Attribs[a].Name, val);

         auto attrib = strihash(Tag.Attribs[a].Name);
         switch(attrib) {
            case SVF_GRADIENTUNITS:
               if (iequals("userSpaceOnUse", val)) gradient->setUnits(VUNIT::USERSPACE);
               else if (iequals("objectBoundingBox", val)) gradient->setUnits(VUNIT::BOUNDING_BOX);
               break;
            case SVF_GRADIENTTRANSFORM: gradient->setTransform(val); break;
            case SVF_CX: set_double_units(gradient, FID_CenterX, val, gradient->Units); break;
            case SVF_CY: set_double_units(gradient, FID_CenterY, val, gradient->Units); break;
            case SVF_R:  set_double_units(gradient, FID_Radius, val, gradient->Units); break;
            case SVF_SPREADMETHOD: {
               if (iequals("pad", val))          gradient->setSpreadMethod(VSPREAD::PAD);
               else if (iequals("reflect", val)) gradient->setSpreadMethod(VSPREAD::REFLECT);
               else if (iequals("repeat", val))  gradient->setSpreadMethod(VSPREAD::REPEAT);
               break;
            }
            case SVF_ID: id = val; break;
            default: {
               if (gradient_defaults(Self, gradient, attrib, val) != ERR::Okay) {
                  if (Tag.Attribs[a].Name.find(':') != std::string::npos) break;
                  log.warning("%s attribute '%s' unrecognised @ line %d", Tag.name(), Tag.Attribs[a].Name.c_str(), Tag.LineNo);
               }
            }
         }
      }

      auto stops = process_gradient_stops(Self, Tag);
      if (stops.size() >= 2) SetArray(gradient, FID_Stops, stops);

      if (InitObject(gradient) IS ERR::Okay) {
         if (!id.empty()) {
            SetName(gradient, id.c_str());
            add_id(Self, Tag, id);
            track_object(Self, gradient);
            return Self->Scene->addDef(id.c_str(), gradient);
         }
         else return ERR::Okay;
      }
      else return ERR::Init;
   }
   else return ERR::NewObject;
}
