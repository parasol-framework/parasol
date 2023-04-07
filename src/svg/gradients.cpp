
static ERROR gradient_defaults(extSVG *Self, objVectorGradient *Gradient, ULONG Attrib, const std::string Value)
{
   switch (Attrib) {
      case SVF_COLOR_INTERPOLATION:
         if (!StrMatch("auto", Value)) Gradient->setColourSpace(VCS_LINEAR_RGB);
         else if (!StrMatch("sRGB", Value)) Gradient->setColourSpace(VCS_SRGB);
         else if (!StrMatch("linearRGB", Value)) Gradient->setColourSpace(VCS_LINEAR_RGB);
         else if (!StrMatch("inherit", Value)) Gradient->setColourSpace(VCS_INHERIT);
         return ERR_Okay;

      case SVF_XLINK_HREF: add_inherit(Self, Gradient, Value); break;
   }

   return ERR_Failed;
}

//********************************************************************************************************************
// Note that all offsets are percentages.

static const std::vector<GradientStop> process_gradient_stops(extSVG *Self, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("");

   std::vector<GradientStop> stops;
   for (auto &scan : Tag.Children) {
      if (!StrMatch("stop", scan.name())) {
         GradientStop stop;
         DOUBLE stop_opacity = 1.0;
         stop.Offset = 0;
         stop.RGB.Red   = 0;
         stop.RGB.Green = 0;
         stop.RGB.Blue  = 0;
         stop.RGB.Alpha = 0;

         for (LONG a=1; a < LONG(scan.Attribs.size()); a++) {
            auto &name  = scan.Attribs[a].Name;
            auto &value = scan.Attribs[a].Value;
            if (value.empty()) continue;

            if (!StrMatch("offset", name)) {
               stop.Offset = StrToFloat(value);
               for (LONG j=0; value[j]; j++) {
                  if (value[j] IS '%') {
                     stop.Offset = stop.Offset * 0.01; // Must be in the range of 0 - 1.0
                     break;
                  }
               }

               if (stop.Offset < 0.0) stop.Offset = 0;
               else if (stop.Offset > 1.0) stop.Offset = 1.0;
            }
            else if (!StrMatch("stop-color", name)) {
               vecReadPainter(Self->Scene, value.c_str(), &stop.RGB, NULL, NULL, NULL);
            }
            else if (!StrMatch("stop-opacity", name)) {
               stop_opacity = StrToFloat(value);
            }
            else if (!StrMatch("id", name)) {
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

static ERROR xtag_lineargradient(extSVG *Self, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objVectorGradient *gradient;

   std::string id;

   if (!NewObject(ID_VECTORGRADIENT, &gradient)) {
      SetOwner(gradient, Self->Scene);
      gradient->setFields(
         fl::Name("SVGLinearGrad"),
         fl::Type(VGT_LINEAR),
         fl::X1(0.0),
         fl::Y1(0.0),
         fl::X2(PERCENT(1.0)),
         fl::Y2(0.0));

      // Determine the user coordinate system first.

      gradient->Units = VUNIT_BOUNDING_BOX;
      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
         if (!StrMatch("gradientUnits", Tag.Attribs[a].Name)) {
            if (!StrMatch("userSpaceOnUse", Tag.Attribs[a].Value)) gradient->Units = VUNIT_USERSPACE;
            break;
         }
      }

      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
         auto &val = Tag.Attribs[a].Value;
         if (val.empty()) continue;

         auto attrib = StrHash(Tag.Attribs[a].Name);
         switch(attrib) {
            case SVF_GRADIENTUNITS: break; // Already checked gradientUnits earlier
            case SVF_GRADIENTTRANSFORM: gradient->setTransform(val); break;
            case SVF_X1: set_double_units(gradient, FID_X1, val, gradient->Units); break;
            case SVF_Y1: set_double_units(gradient, FID_Y1, val, gradient->Units); break;
            case SVF_X2: set_double_units(gradient, FID_X2, val, gradient->Units); break;
            case SVF_Y2: set_double_units(gradient, FID_Y2, val, gradient->Units); break;

            case SVF_COLOR_INTERPOLATION:
               if (!StrMatch("auto", val)) gradient->setColourSpace(VCS_LINEAR_RGB);
               else if (!StrMatch("sRGB", val)) gradient->setColourSpace(VCS_SRGB);
               else if (!StrMatch("linearRGB", val)) gradient->setColourSpace(VCS_LINEAR_RGB);
               else if (!StrMatch("inherit", val)) gradient->setColourSpace(VCS_INHERIT);
               break;

            case SVF_SPREADMETHOD: {
               if (!StrMatch("pad", val))          gradient->setSpreadMethod(VSPREAD_PAD);
               else if (!StrMatch("reflect", val)) gradient->setSpreadMethod(VSPREAD_REFLECT);
               else if (!StrMatch("repeat", val))  gradient->setSpreadMethod(VSPREAD_REPEAT);
               break;
            }
            case SVF_ID: id = val; break;

            default: {
               if (gradient_defaults(Self, gradient, attrib, val)) {
                  if (Tag.Attribs[a].Name.find(':') != std::string::npos) break;
                  log.warning("%s attribute '%s' unrecognised @ line %d", Tag.name(), Tag.Attribs[a].Name.c_str(), Tag.LineNo);
               }
               break;
            }
         }
      }

      auto stops = process_gradient_stops(Self, Tag);
      if (stops.size() >= 2) SetArray(gradient, FID_Stops, stops);

      if (!InitObject(gradient)) {
         if (!id.empty()) SetName(gradient, id.c_str());
         return scAddDef(Self->Scene, id.c_str(), gradient);
      }
      else return ERR_Init;
   }
   else return ERR_NewObject;
}

//********************************************************************************************************************

static ERROR xtag_radialgradient(extSVG *Self, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objVectorGradient *gradient;
   std::string id;

   if (!NewObject(ID_VECTORGRADIENT, &gradient)) {
      SetOwner(gradient, Self->Scene);

      gradient->setFields(fl::Name("SVGRadialGrad"), fl::Type(VGT_RADIAL),
         fl::CenterX(PERCENT(0.5)), fl::CenterY(PERCENT(0.5)), fl::Radius(PERCENT(0.5)));

      // Determine the user coordinate system first.

      gradient->Units = VUNIT_BOUNDING_BOX;
      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
         if (!StrMatch("gradientUnits", Tag.Attribs[a].Name)) {
            if (!StrMatch("userSpaceOnUse", Tag.Attribs[a].Value)) gradient->Units = VUNIT_USERSPACE;
            break;
         }
      }

      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
         auto &val = Tag.Attribs[a].Value;
         if (val.empty()) continue;
         log.trace("Processing radial gradient attribute %s = %s", Tag.Attribs[a].Name, val);

         auto attrib = StrHash(Tag.Attribs[a].Name);
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
               if (!StrMatch("pad", val))          gradient->setSpreadMethod(VSPREAD_PAD);
               else if (!StrMatch("reflect", val)) gradient->setSpreadMethod(VSPREAD_REFLECT);
               else if (!StrMatch("repeat", val))  gradient->setSpreadMethod(VSPREAD_REPEAT);
               break;

            default: {
               if (gradient_defaults(Self, gradient, attrib, val)) {
                  if (Tag.Attribs[a].Name.find(':') != std::string::npos) break;
                  log.warning("%s attribute '%s' unrecognised @ line %d", Tag.name(), Tag.Attribs[a].Name.c_str(), Tag.LineNo);
               }
            }
         }
      }

      auto stops = process_gradient_stops(Self, Tag);
      if (stops.size() >= 2) SetArray(gradient, FID_Stops, stops);

      if (!InitObject(gradient)) {
         if (!id.empty()) SetName(gradient, id.c_str());
         return scAddDef(Self->Scene, id.c_str(), gradient);
      }
      else return ERR_Init;
   }
   else return ERR_NewObject;
}

//********************************************************************************************************************

static ERROR xtag_diamondgradient(extSVG *Self, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objVectorGradient *gradient;
   std::string id;

   if (!NewObject(ID_VECTORGRADIENT, &gradient)) {
      SetOwner(gradient, Self->Scene);

      gradient->setFields(fl::Name("SVGDiamondGrad"), fl::Type(VGT_DIAMOND),
         fl::CenterX(PERCENT(0.5)), fl::CenterY(PERCENT(0.5)), fl::Radius(PERCENT(0.5)));

      // Determine the user coordinate system first.

      gradient->Units = VUNIT_BOUNDING_BOX;
      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
         if (!StrMatch("gradientUnits", Tag.Attribs[a].Name)) {
            if (!StrMatch("userSpaceOnUse", Tag.Attribs[a].Value)) gradient->Units = VUNIT_USERSPACE;
            break;
         }
      }

      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
         auto &val = Tag.Attribs[a].Value;
         if (val.empty()) continue;

         log.trace("Processing diamond gradient attribute %s =  %s", Tag.Attribs[a].Name, val);

         auto attrib = StrHash(Tag.Attribs[a].Name);
         switch(attrib) {
            case SVF_GRADIENTUNITS: break; // Already processed
            case SVF_GRADIENTTRANSFORM: gradient->setTransform(val); break;
            case SVF_CX: set_double_units(gradient, FID_CenterX, val, gradient->Units); break;
            case SVF_CY: set_double_units(gradient, FID_CenterY, val, gradient->Units); break;
            case SVF_R:  set_double_units(gradient, FID_Radius, val, gradient->Units); break;
            case SVF_SPREADMETHOD: {
               if (!StrMatch("pad", val))          gradient->setSpreadMethod(VSPREAD_PAD);
               else if (!StrMatch("reflect", val)) gradient->setSpreadMethod(VSPREAD_REFLECT);
               else if (!StrMatch("repeat", val))  gradient->setSpreadMethod(VSPREAD_REPEAT);
               break;
            }
            case SVF_ID: id = val; break;
            default: {
               if (gradient_defaults(Self, gradient, attrib, val)) {
                  if (Tag.Attribs[a].Name.find(':') != std::string::npos) break;
                  log.warning("%s attribute '%s' unrecognised @ line %d", Tag.name(), Tag.Attribs[a].Name.c_str(), Tag.LineNo);
               }
            }
         }
      }

      auto stops = process_gradient_stops(Self, Tag);
      if (stops.size() >= 2) SetArray(gradient, FID_Stops, stops);

      if (!InitObject(gradient)) {
         if (!id.empty()) SetName(gradient, id.c_str());
         return scAddDef(Self->Scene, id.c_str(), gradient);
      }
      else return ERR_Init;
   }
   else return ERR_NewObject;
}

//********************************************************************************************************************
// NB: Contour gradients are not part of the SVG standard.

static ERROR xtag_contourgradient(extSVG *Self, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objVectorGradient *gradient;
   std::string id;

   if (!NewObject(ID_VECTORGRADIENT, &gradient)) {
      SetOwner(gradient, Self->Scene);
      gradient->setFields(fl::Name("SVGContourGrad"), fl::Type(VGT_CONTOUR));

      // Determine the user coordinate system first.

      gradient->Units = VUNIT_BOUNDING_BOX;
      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
         if (!StrMatch("gradientUnits", Tag.Attribs[a].Name)) {
            if (!StrMatch("userSpaceOnUse", Tag.Attribs[a].Value)) gradient->Units = VUNIT_USERSPACE;
            break;
         }
      }

      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
         auto &val = Tag.Attribs[a].Value;
         if (val.empty()) continue;

         log.trace("Processing contour gradient attribute %s =  %s", Tag.Attribs[a].Name, val);

         auto attrib = StrHash(Tag.Attribs[a].Name);
         switch(attrib) {
            case SVF_GRADIENTUNITS: break; // Already processed
            case SVF_GRADIENTTRANSFORM: gradient->setTransform(val); break;
            case SVF_X1: set_double_units(gradient, FID_X1, val, gradient->Units); break;
            case SVF_X2: set_double_units(gradient, FID_X2, val, gradient->Units); break;
            case SVF_SPREADMETHOD: {
               if (!StrMatch("pad", val))          gradient->setSpreadMethod(VSPREAD_PAD);
               else if (!StrMatch("reflect", val)) gradient->setSpreadMethod(VSPREAD_REFLECT);
               else if (!StrMatch("repeat", val))  gradient->setSpreadMethod(VSPREAD_REPEAT);
               break;
            }
            case SVF_ID: id = val; break;
            default: {
               if (gradient_defaults(Self, gradient, attrib, val)) {
                  if (Tag.Attribs[a].Name.find(':') != std::string::npos) break;
                  log.warning("%s attribute '%s' unrecognised @ line %d", Tag.name(), Tag.Attribs[a].Name.c_str(), Tag.LineNo);
               }
            }
         }
      }

      auto stops = process_gradient_stops(Self, Tag);
      if (stops.size() >= 2) SetArray(gradient, FID_Stops, stops);

      if (!InitObject(gradient)) {
         if (!id.empty()) SetName(gradient, id.c_str());
         return scAddDef(Self->Scene, id.c_str(), gradient);
      }
      else return ERR_Init;
   }
   else return ERR_NewObject;
}

//********************************************************************************************************************

static ERROR xtag_conicgradient(extSVG *Self, const XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objVectorGradient *gradient;

   if (!NewObject(ID_VECTORGRADIENT, &gradient)) {
      SetOwner(gradient, Self->Scene);

      gradient->setFields(fl::Name("SVGConicGrad"), fl::Type(VGT_CONIC),
         fl::CenterX(PERCENT(0.5)), fl::CenterY(PERCENT(0.5)), fl::Radius(PERCENT(0.5)));

      std::string id;

      // Determine the user coordinate system first.

      gradient->Units = VUNIT_BOUNDING_BOX;
      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
         if (!StrMatch("gradientUnits", Tag.Attribs[a].Name)) {
            if (!StrMatch("userSpaceOnUse", Tag.Attribs[a].Value)) gradient->Units = VUNIT_USERSPACE;
            break;
         }
      }

      for (LONG a=1; a < LONG(Tag.Attribs.size()); a++) {
         auto &val = Tag.Attribs[a].Value;
         if (val.empty()) continue;

         log.trace("Processing diamond gradient attribute %s =  %s", Tag.Attribs[a].Name, val);

         auto attrib = StrHash(Tag.Attribs[a].Name);
         switch(attrib) {
            case SVF_GRADIENTUNITS:
               if (!StrMatch("userSpaceOnUse", val)) gradient->setUnits(VUNIT_USERSPACE);
               else if (!StrMatch("objectBoundingBox", val)) gradient->setUnits(VUNIT_BOUNDING_BOX);
               break;
            case SVF_GRADIENTTRANSFORM: gradient->setTransform(val); break;
            case SVF_CX: set_double_units(gradient, FID_CenterX, val, gradient->Units); break;
            case SVF_CY: set_double_units(gradient, FID_CenterY, val, gradient->Units); break;
            case SVF_R:  set_double_units(gradient, FID_Radius, val, gradient->Units); break;
            case SVF_SPREADMETHOD: {
               if (!StrMatch("pad", val))          gradient->setSpreadMethod(VSPREAD_PAD);
               else if (!StrMatch("reflect", val)) gradient->setSpreadMethod(VSPREAD_REFLECT);
               else if (!StrMatch("repeat", val))  gradient->setSpreadMethod(VSPREAD_REPEAT);
               break;
            }
            case SVF_ID: id = val; break;
            default: {
               if (gradient_defaults(Self, gradient, attrib, val)) {
                  if (Tag.Attribs[a].Name.find(':') != std::string::npos) break;
                  log.warning("%s attribute '%s' unrecognised @ line %d", Tag.name(), Tag.Attribs[a].Name.c_str(), Tag.LineNo);
               }
            }
         }
      }

      auto stops = process_gradient_stops(Self, Tag);
      if (stops.size() >= 2) SetArray(gradient, FID_Stops, stops);

      if (!InitObject(gradient)) {
         if (!id.empty()) SetName(gradient, id.c_str());
         return scAddDef(Self->Scene, id.c_str(), gradient);
      }
      else return ERR_Init;
   }
   else return ERR_NewObject;
}
