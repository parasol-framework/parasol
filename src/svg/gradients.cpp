
//****************************************************************************
// Note that all offsets are percentages.

static ERROR process_gradient_stops(objSVG *Self, const XMLTag *Tag, GradientStop *Stops)
{
   parasol::Log log(__FUNCTION__);

   log.traceBranch("");

   LONG i = 0;
   for (auto scan=Tag->Child; scan; scan=scan->Next) {
      if (!StrMatch("stop", scan->Attrib->Name)) {
         DOUBLE stop_opacity = 1.0;
         Stops[i].Offset = 0;
         Stops[i].RGB.Red   = 0;
         Stops[i].RGB.Green = 0;
         Stops[i].RGB.Blue  = 0;
         Stops[i].RGB.Alpha = 0;

         for (LONG a=1; a < scan->TotalAttrib; a++) {
            CSTRING name = scan->Attrib[a].Name;
            CSTRING value = scan->Attrib[a].Value;
            if (!value) continue;

            if (!StrMatch("offset", name)) {
               Stops[i].Offset = StrToFloat(value);
               LONG j;
               for (j=0; value[j]; j++) {
                  if (value[j] IS '%') {
                     Stops[i].Offset = Stops[i].Offset * 0.01; // Must be in the range of 0 - 1.0
                     break;
                  }
               }

               if (Stops[i].Offset < 0.0) Stops[i].Offset = 0;
               else if (Stops[i].Offset > 1.0) Stops[i].Offset = 1.0;
            }
            else if (!StrMatch("stop-color", name)) {
               vecReadPainter(Self->Scene, value, &Stops[i].RGB, NULL, NULL, NULL);
            }
            else if (!StrMatch("stop-opacity", name)) {
               stop_opacity = StrToFloat(value);
            }
            else if (!StrMatch("id", name)) {
               log.trace("Use of id attribute in <stop/> ignored.");
            }
            else log.warning("Unable to process stop attribute '%s'", name);
         }

         Stops[i].RGB.Alpha = ((DOUBLE)Stops[i].RGB.Alpha) * stop_opacity;

         i++;
      }
      else log.warning("Unknown element in gradient, '%s'", scan->Attrib->Name);
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR xtag_lineargradient(objSVG *Self, const XMLTag *Tag)
{
   parasol::Log log(__FUNCTION__);
   objVectorGradient *gradient;

   CSTRING id = NULL;

   if (!NewObject(ID_VECTORGRADIENT, 0, &gradient)) {
      SetOwner(gradient, Self->Scene);
      SetFields(gradient,
         FID_Name|TSTR,   "SVGLinearGrad",
         FID_Type|TLONG,  VGT_LINEAR,
         FID_X1|TDOUBLE,  0.0,
         FID_Y1|TDOUBLE,  0.0,
         FID_X2|TDOUBLE|TPERCENT, 100.0,
         FID_Y2|TDOUBLE, 0.0,
         TAGEND);

      // Determine the user coordinate system first.

      gradient->Units = VUNIT_BOUNDING_BOX;

      for (LONG a=1; a < Tag->TotalAttrib; a++) {
         if (!StrMatch("gradientUnits", Tag->Attrib[a].Name)) {
            if (!StrMatch("userSpaceOnUse", Tag->Attrib[a].Value)) gradient->Units = VUNIT_USERSPACE;
            break;
         }
      }

      for (LONG a=1; a < Tag->TotalAttrib; a++) {
         CSTRING val;
         if (!(val = Tag->Attrib[a].Value)) continue;

         switch(StrHash(Tag->Attrib[a].Name, FALSE)) {
            case SVF_GRADIENTUNITS: break; // Already checked gradientUnits earlier
            case SVF_GRADIENTTRANSFORM: SetString(gradient, FID_Transform, val); break;
            case SVF_X1: set_double_units(gradient, FID_X1, val, gradient->Units); break;
            case SVF_Y1: set_double_units(gradient, FID_Y1, val, gradient->Units); break;
            case SVF_X2: set_double_units(gradient, FID_X2, val, gradient->Units); break;
            case SVF_Y2: set_double_units(gradient, FID_Y2, val, gradient->Units); break;

            case SVF_SPREADMETHOD: {
               if (!StrMatch("pad", val))          SetLong(gradient, FID_SpreadMethod, VSPREAD_PAD);
               else if (!StrMatch("reflect", val)) SetLong(gradient, FID_SpreadMethod, VSPREAD_REFLECT);
               else if (!StrMatch("repeat", val))  SetLong(gradient, FID_SpreadMethod, VSPREAD_REPEAT);
               break;
            }
            case SVF_XLINK_HREF: add_inherit(Self, gradient, val); break;
            case SVF_ID: id = val; break;

            default: {
               LONG j;
               for (j=0; Tag->Attrib[a].Name[j] AND (Tag->Attrib[a].Name[j] != ':'); j++);
               if (Tag->Attrib[a].Name[j] IS ':') break;
               log.warning("%s attribute '%s' unrecognised @ line %d", Tag->Attrib->Name, Tag->Attrib[a].Name, Tag->LineNo);
               break;
            }
         }
      }

      LONG stopcount = count_stops(Self, Tag);
      if (stopcount >= 2) {
         GradientStop stops[stopcount];
         process_gradient_stops(Self, Tag, stops);
         SetArray(gradient, FID_Stops, stops, stopcount);
      }

      if (!acInit(gradient)) {
         if (id) SetName(gradient, id);
         return scAddDef(Self->Scene, id, gradient);
      }
      else return ERR_Init;
   }
   else return ERR_NewObject;
}

//****************************************************************************

static ERROR xtag_radialgradient(objSVG *Self, const XMLTag *Tag)
{
   parasol::Log log(__FUNCTION__);
   objVectorGradient *gradient;
   CSTRING id = NULL;

   if (!NewObject(ID_VECTORGRADIENT, 0, &gradient)) {
      SetOwner(gradient, Self->Scene);

      SetFields(gradient,
         FID_Name|TSTR,  "SVGRadialGrad",
         FID_Type|TLONG, VGT_RADIAL,
         FID_CenterX|TDOUBLE|TPERCENT, 50.0,
         FID_CenterY|TDOUBLE|TPERCENT, 50.0,
         FID_Radius|TDOUBLE|TPERCENT,  50.0,
         TAGEND);

      // Determine the user coordinate system first.

      gradient->Units = VUNIT_BOUNDING_BOX;
      for (LONG a=1; a < Tag->TotalAttrib; a++) {
         if (!StrMatch("gradientUnits", Tag->Attrib[a].Name)) {
            if (!StrMatch("userSpaceOnUse", Tag->Attrib[a].Value)) gradient->Units = VUNIT_USERSPACE;
            break;
         }
      }

      for (LONG a=1; a < Tag->TotalAttrib; a++) {
         CSTRING val = Tag->Attrib[a].Value;
         if (!val) continue;
         log.trace("Processing radial gradient attribute %s = %s", Tag->Attrib[a].Name, val);

         switch(StrHash(Tag->Attrib[a].Name, FALSE)) {
            case SVF_CX: set_double_units(gradient, FID_CenterX, val, gradient->Units); break;
            case SVF_CY: set_double_units(gradient, FID_CenterY, val, gradient->Units); break;
            case SVF_FX: set_double_units(gradient, FID_FX, val, gradient->Units); break;
            case SVF_FY: set_double_units(gradient, FID_FY, val, gradient->Units); break;
            case SVF_R:  set_double_units(gradient, FID_Radius, val, gradient->Units); break;
            case SVF_GRADIENTUNITS: break; // Already processed
            case SVF_GRADIENTTRANSFORM: SetString(gradient, FID_Transform, val); break;
            case SVF_ID: id = val; break;
            case SVF_SPREADMETHOD:
               if (!StrMatch("pad", val))          SetLong(gradient, FID_SpreadMethod, VSPREAD_PAD);
               else if (!StrMatch("reflect", val)) SetLong(gradient, FID_SpreadMethod, VSPREAD_REFLECT);
               else if (!StrMatch("repeat", val))  SetLong(gradient, FID_SpreadMethod, VSPREAD_REPEAT);
               break;

            case SVF_XLINK_HREF: add_inherit(Self, gradient, val); break;
            default: {
               LONG j;
               for (j=0; Tag->Attrib[a].Name[j] and (Tag->Attrib[a].Name[j] != ':'); j++);
               if (Tag->Attrib[a].Name[j] IS ':') break;
               log.warning("%s attribute '%s' unrecognised @ line %d", Tag->Attrib->Name, Tag->Attrib[a].Name, Tag->LineNo);
               break;
            }
         }
      }

      LONG stopcount = count_stops(Self, Tag);
      if (stopcount >= 2) {
         GradientStop stops[stopcount];
         process_gradient_stops(Self, Tag, stops);
         SetArray(gradient, FID_Stops, stops, stopcount);
      }

      if (!acInit(gradient)) {
         if (id) SetName(gradient, id);
         return scAddDef(Self->Scene, id, gradient);
      }
      else return ERR_Init;
   }
   else return ERR_NewObject;
}

//****************************************************************************

static ERROR xtag_diamondgradient(objSVG *Self, const XMLTag *Tag)
{
   parasol::Log log(__FUNCTION__);
   objVectorGradient *gradient;
   CSTRING id = NULL;

   if (!NewObject(ID_VECTORGRADIENT, 0, &gradient)) {
      SetOwner(gradient, Self->Scene);

      SetFields(gradient,
         FID_Name|TSTR,  "SVGDiamondGrad",
         FID_Type|TLONG, VGT_DIAMOND,
         FID_CenterX|TDOUBLE|TPERCENT, 50.0,
         FID_CenterY|TDOUBLE|TPERCENT, 50.0,
         FID_Radius|TDOUBLE|TPERCENT,  50.0,
         TAGEND);

      // Determine the user coordinate system first.

      gradient->Units = VUNIT_BOUNDING_BOX;
      for (LONG a=1; a < Tag->TotalAttrib; a++) {
         if (!StrMatch("gradientUnits", Tag->Attrib[a].Name)) {
            if (!StrMatch("userSpaceOnUse", Tag->Attrib[a].Value)) gradient->Units = VUNIT_USERSPACE;
            break;
         }
      }

      for (LONG a=1; a < Tag->TotalAttrib; a++) {
         CSTRING val;
         if (!(val = Tag->Attrib[a].Value)) continue;

         log.trace("Processing diamond gradient attribute %s =  %s", Tag->Attrib[a].Name, val);

         switch(StrHash(Tag->Attrib[a].Name, FALSE)) {
            case SVF_GRADIENTUNITS: break; // Already processed
            case SVF_GRADIENTTRANSFORM: SetString(gradient, FID_Transform, val); break;
            case SVF_CX: set_double_units(gradient, FID_CenterX, val, gradient->Units); break;
            case SVF_CY: set_double_units(gradient, FID_CenterY, val, gradient->Units); break;
            case SVF_R:  set_double_units(gradient, FID_Radius, val, gradient->Units); break;
            case SVF_SPREADMETHOD: {
               if (!StrMatch("pad", val))          SetLong(gradient, FID_SpreadMethod, VSPREAD_PAD);
               else if (!StrMatch("reflect", val)) SetLong(gradient, FID_SpreadMethod, VSPREAD_REFLECT);
               else if (!StrMatch("repeat", val))  SetLong(gradient, FID_SpreadMethod, VSPREAD_REPEAT);
               break;
            }
            case SVF_XLINK_HREF: add_inherit(Self, gradient, val); break;
            case SVF_ID: id = val; break;
            default: {
               LONG j;
               for (j=0; Tag->Attrib[a].Name[j] AND (Tag->Attrib[a].Name[j] != ':'); j++);
               if (Tag->Attrib[a].Name[j] IS ':') break;
               log.warning("%s attribute '%s' unrecognised @ line %d", Tag->Attrib->Name, Tag->Attrib[a].Name, Tag->LineNo);
               break;
            }
         }
      }

      LONG stopcount = count_stops(Self, Tag);
      if (stopcount >= 2) {
         GradientStop stops[stopcount];
         process_gradient_stops(Self, Tag, stops);
         SetArray(gradient, FID_Stops, stops, stopcount);
      }

      if (!acInit(gradient)) {
         if (id) SetName(gradient, id);
         return scAddDef(Self->Scene, id, gradient);
      }
      else return ERR_Init;
   }
   else return ERR_NewObject;
}

//****************************************************************************
// NB: Contour gradients are not part of the SVG standard.

static ERROR xtag_contourgradient(objSVG *Self, const XMLTag *Tag)
{
   parasol::Log log(__FUNCTION__);
   objVectorGradient *gradient;
   CSTRING id = NULL;

   if (!NewObject(ID_VECTORGRADIENT, 0, &gradient)) {
      SetOwner(gradient, Self->Scene);
      SetFields(gradient,
         FID_Name|TSTR,  "SVGContourGrad",
         FID_Type|TLONG, VGT_CONTOUR,
         TAGEND);

      // Determine the user coordinate system first.

      gradient->Units = VUNIT_BOUNDING_BOX;
      for (LONG a=1; a < Tag->TotalAttrib; a++) {
         if (!StrMatch("gradientUnits", Tag->Attrib[a].Name)) {
            if (!StrMatch("userSpaceOnUse", Tag->Attrib[a].Value)) gradient->Units = VUNIT_USERSPACE;
            break;
         }
      }

      for (LONG a=1; a < Tag->TotalAttrib; a++) {
         CSTRING val;
         if (!(val = Tag->Attrib[a].Value)) continue;

         log.trace("Processing contour gradient attribute %s =  %s", Tag->Attrib[a].Name, val);

         switch(StrHash(Tag->Attrib[a].Name, FALSE)) {
            case SVF_GRADIENTUNITS: break; // Already processed
            case SVF_GRADIENTTRANSFORM: SetString(gradient, FID_Transform, val); break;
            case SVF_X1: set_double_units(gradient, FID_X1, val, gradient->Units); break;
            case SVF_X2: set_double_units(gradient, FID_X2, val, gradient->Units); break;
            case SVF_SPREADMETHOD: {
               if (!StrMatch("pad", val))          SetLong(gradient, FID_SpreadMethod, VSPREAD_PAD);
               else if (!StrMatch("reflect", val)) SetLong(gradient, FID_SpreadMethod, VSPREAD_REFLECT);
               else if (!StrMatch("repeat", val))  SetLong(gradient, FID_SpreadMethod, VSPREAD_REPEAT);
               break;
            }
            case SVF_XLINK_HREF: add_inherit(Self, gradient, val); break;
            case SVF_ID: id = val; break;
            default: {
               LONG j;
               for (j=0; Tag->Attrib[a].Name[j] and (Tag->Attrib[a].Name[j] != ':'); j++);
               if (Tag->Attrib[a].Name[j] IS ':') break;
               log.warning("%s attribute '%s' unrecognised @ line %d", Tag->Attrib->Name, Tag->Attrib[a].Name, Tag->LineNo);
               break;
            }
         }
      }

      LONG stopcount = count_stops(Self, Tag);
      if (stopcount >= 2) {
         GradientStop stops[stopcount];
         process_gradient_stops(Self, Tag, stops);
         SetArray(gradient, FID_Stops, stops, stopcount);
      }

      if (!acInit(gradient)) {
         if (id) SetName(gradient, id);
         return scAddDef(Self->Scene, id, gradient);
      }
      else return ERR_Init;
   }
   else return ERR_NewObject;
}

//****************************************************************************

static ERROR xtag_conicgradient(objSVG *Self, const XMLTag *Tag)
{
   parasol::Log log(__FUNCTION__);
   objVectorGradient *gradient;

   if (!NewObject(ID_VECTORGRADIENT, 0, &gradient)) {
      SetOwner(gradient, Self->Scene);

      SetFields(gradient,
         FID_Name|TSTR,  "SVGConicGrad",
         FID_Type|TLONG, VGT_CONIC,
         FID_CenterX|TDOUBLE|TPERCENT, 50.0,
         FID_CenterY|TDOUBLE|TPERCENT, 50.0,
         FID_Radius|TDOUBLE|TPERCENT,  50.0,
         TAGEND);

      CSTRING id = NULL;

      // Determine the user coordinate system first.

      gradient->Units = VUNIT_BOUNDING_BOX;
      for (LONG a=1; a < Tag->TotalAttrib; a++) {
         if (!StrMatch("gradientUnits", Tag->Attrib[a].Name)) {
            if (!StrMatch("userSpaceOnUse", Tag->Attrib[a].Value)) gradient->Units = VUNIT_USERSPACE;
            break;
         }
      }

      for (LONG a=1; a < Tag->TotalAttrib; a++) {
         CSTRING val;
         if (!(val = Tag->Attrib[a].Value)) continue;

         log.trace("Processing diamond gradient attribute %s =  %s", Tag->Attrib[a].Name, val);

         switch(StrHash(Tag->Attrib[a].Name, FALSE)) {
            case SVF_GRADIENTUNITS:
               if (!StrMatch("userSpaceOnUse", val)) SetLong(gradient, FID_Units, VUNIT_USERSPACE);
               else if (!StrMatch("objectBoundingBox", val)) SetLong(gradient, FID_Units, VUNIT_BOUNDING_BOX);
               break;
            case SVF_GRADIENTTRANSFORM: SetString(gradient, FID_Transform, val); break;
            case SVF_CX: set_double_units(gradient, FID_CenterX, val, gradient->Units); break;
            case SVF_CY: set_double_units(gradient, FID_CenterY, val, gradient->Units); break;
            case SVF_R:  set_double_units(gradient, FID_Radius, val, gradient->Units); break;
            case SVF_SPREADMETHOD: {
               if (!StrMatch("pad", val))          SetLong(gradient, FID_SpreadMethod, VSPREAD_PAD);
               else if (!StrMatch("reflect", val)) SetLong(gradient, FID_SpreadMethod, VSPREAD_REFLECT);
               else if (!StrMatch("repeat", val))  SetLong(gradient, FID_SpreadMethod, VSPREAD_REPEAT);
               break;
            }
            case SVF_XLINK_HREF: add_inherit(Self, gradient, val); break;
            case SVF_ID: id = val; break;
            default: {
               LONG j;
               for (j=0; Tag->Attrib[a].Name[j] and (Tag->Attrib[a].Name[j] != ':'); j++);
               if (Tag->Attrib[a].Name[j] IS ':') break;
               log.warning("%s attribute '%s' unrecognised @ line %d", Tag->Attrib->Name, Tag->Attrib[a].Name, Tag->LineNo);
               break;
            }
         }
      }

      LONG stopcount = count_stops(Self, Tag);
      if (stopcount >= 2) {
         GradientStop stops[stopcount];
         process_gradient_stops(Self, Tag, stops);
         SetArray(gradient, FID_Stops, stops, stopcount);
      }

      if (!acInit(gradient)) {
         if (id) SetName(gradient, id);
         return scAddDef(Self->Scene, id, gradient);
      }
      else return ERR_Init;
   }
   else return ERR_NewObject;
}
