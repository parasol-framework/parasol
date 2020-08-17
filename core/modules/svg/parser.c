
static ERROR xtag_animatetransform(objSVG *, objXML *, struct XMLTag *, OBJECTPTR);
static ERROR xtag_animatemotion(objSVG *, objXML *, struct XMLTag *, OBJECTPTR Parent);
static ERROR xtag_default(objSVG *, ULONG Hash, objXML *, struct svgState *, struct XMLTag *, OBJECTPTR, OBJECTPTR *);
static void xtag_morph(objSVG *, objXML *, struct XMLTag *, OBJECTPTR Parent);

//****************************************************************************
// Apply the current state values to a vector.

static void apply_state(svgState *State, OBJECTPTR Vector)
{
   FMSG("~apply_state()","%s: Fill: %s, Stroke: %s, Opacity: %.2f, Font: %s %s", Vector->Class->ClassName, State->Fill, State->Stroke, State->Opacity, State->FontFamily, State->FontSize);

   if (State->Fill)         SetString(Vector, FID_Fill, State->Fill);
   if (State->Stroke)       SetString(Vector, FID_Stroke, State->Stroke);
   if (State->StrokeWidth)  SetDouble(Vector, FID_StrokeWidth, State->StrokeWidth);
   if (Vector->SubID IS ID_VECTORTEXT) {
      if (State->FontFamily) SetString(Vector, FID_Face, State->FontFamily);
      if (State->FontSize)   SetString(Vector, FID_FontSize, State->FontSize);
   }
   if (State->FillOpacity >= 0.0) SetDouble(Vector, FID_FillOpacity, State->FillOpacity);
   if (State->Opacity >= 0.0) SetDouble(Vector, FID_Opacity, State->Opacity);

   STEP();
}

//****************************************************************************
// Copy a tag's attributes to the current state.

static void set_state(svgState *State, struct XMLTag *Tag)
{
   FMSG("~set_state()","Total Attributes: %d", Tag->TotalAttrib);

   LONG a;
   for (a=1; a < Tag->TotalAttrib; a++) {
      CSTRING val;
      if (!(val = Tag->Attrib[a].Value)) continue;

      switch (StrHash(Tag->Attrib[a].Name, FALSE)) {
         case SVF_FILL:         State->Fill = val; break;
         case SVF_STROKE:       State->Stroke = val; break;
         case SVF_STROKE_WIDTH: State->StrokeWidth = StrToFloat(val); break;
         case SVF_FONT_FAMILY:  State->FontFamily = val; break;
         case SVF_FONT_SIZE:    State->FontSize = val; break;
         case SVF_FILL_OPACITY: State->FillOpacity = StrToFloat(val); break;
         case SVF_OPACITY:      State->Opacity = StrToFloat(val); break;
      }
   }

   STEP();
}

//****************************************************************************
// Process all child elements that belong to the target Tag.

static void process_children(objSVG *Self, objXML *XML, struct svgState *State, struct XMLTag *Tag, OBJECTPTR Vector)
{
   struct XMLTag *child;
   OBJECTPTR sibling = NULL;
   for (child=Tag; child; child=child->Next) {
      if (child->Attrib->Name) {
         ULONG hash = StrHash(child->Attrib->Name, FALSE);
         xtag_default(Self, hash, XML, State, child, Vector, &sibling);
      }
   }
}

//****************************************************************************

static void xtag_pathtransition(objSVG *Self, objXML *XML, struct XMLTag *Tag)
{
   FMSG("~xtag_pathtransition()","Tag: %p", Tag);

   OBJECTPTR trans;

   if (!NewObject(ID_VECTORTRANSITION, 0, &trans)) {
      SetFields(trans,
         FID_Owner|TLONG, Self->Scene->Head.UniqueID, // All clips belong to the root page to prevent hierarchy issues.
         FID_Name|TSTR,   "SVGTransition",
         TAGEND);

      CSTRING id = NULL;
      LONG a;
      for (a=1; a < Tag->TotalAttrib; a++) {
         CSTRING val = Tag->Attrib[a].Value;
         if (!val) continue;

         switch(StrHash(Tag->Attrib[a].Name, FALSE)) {
            case SVF_ID: id = val; break;
         }
      }

      if (id) {
         LONG stopcount = count_stops(Self, Tag);
         if (stopcount >= 2) {
            struct Transition stops[stopcount];
            process_transition_stops(Self, Tag, stops);
            SetArray((OBJECTPTR)trans, FID_Stops, stops, stopcount);

            if (!acInit(trans)) {
               scAddDef(Self->Scene, id, (OBJECTPTR)trans);
               STEP();
               return;
            }
         }
         else LogErrorMsg("At least two stops are required for <pathTransition> at line %d.", Tag->LineNo);
      }
      else LogErrorMsg("No id attribute specified in <pathTransition> at line %d.", Tag->LineNo);

      acFree(trans);
   }

   STEP();
}

//****************************************************************************

static void xtag_clippath(objSVG *Self, objXML *XML, struct XMLTag *Tag)
{
   FMSG("~xtag_clippath()","Tag: %p", Tag);

   OBJECTPTR clip;
   CSTRING id = NULL;

   if (!NewObject(ID_VECTORCLIP, 0, &clip)) {
      SetFields(clip,
         FID_Owner|TLONG, Self->Scene->Head.UniqueID, // All clips belong to the root page to prevent hierarchy issues.
         FID_Name|TSTR,   "SVGClip",
         FID_Units|TLONG, VUNIT_BOUNDING_BOX,
         TAGEND);

      LONG a;
      for (a=1; a < Tag->TotalAttrib; a++) {
         CSTRING val = Tag->Attrib[a].Value;
         if (!val) continue;

         switch(StrHash(Tag->Attrib[a].Name, FALSE)) {
            case SVF_ID: id = val; break;
            case SVF_TRANSFORM: break;
            case SVF_CLIPPATHUNITS: break;
            case SVF_EXTERNALRESOURCESREQUIRED: break;
         }
      }

      if (id) {
         if (!acInit(clip)) {
            svgState state;
            reset_state(&state);

            // Valid child elements for clip-path are: circle, ellipse, line, path, polygon, polyline, rect, text, use, animate

            process_children(Self, XML, &state, Tag->Child, clip);
            scAddDef(Self->Scene, id, (OBJECTPTR)clip);
         }
         else acFree(clip);
      }
      else {
         LogErrorMsg("No id attribute specified in <clipPath> at line %d.", Tag->LineNo);
         acFree(clip);
      }
   }

   STEP();
}

//****************************************************************************

static void xtag_filter(objSVG *Self, objXML *XML, struct XMLTag *Tag)
{
   objVectorFilter *filter;
   CSTRING id = NULL;

   if (!NewObject(ID_VECTORFILTER, 0, &filter)) {
      SetFields(filter,
         FID_Owner|TLONG,       Self->Scene->Head.UniqueID,
         FID_Name|TSTR,         "SVGFilter",
         FID_Units|TLONG,       VUNIT_BOUNDING_BOX,
         FID_ColourSpace|TLONG, CS_LINEAR_RGB,
         FID_Path|TSTR,         Self->Path,
         TAGEND);

      LONG a;
      for (a=1; a < Tag->TotalAttrib; a++) {
         CSTRING val = Tag->Attrib[a].Value;
         if (!val) continue;

         LONG j;
         for (j=0; Tag->Attrib[a].Name[j] AND (Tag->Attrib[a].Name[j] != ':'); j++);
         if (Tag->Attrib[a].Name[j] IS ':') continue;

         switch(StrHash(Tag->Attrib[a].Name, FALSE)) {
            case SVF_FILTERUNITS:
               if (!StrMatch("userSpaceOnUse", val)) filter->Units = VUNIT_USERSPACE;
               else if (!StrMatch("objectBoundingBox", val)) filter->Units = VUNIT_BOUNDING_BOX;
               break;

            case SVF_ID:
               id = val;
               add_id(Self, Tag, val);
               break;
            case SVF_X:       set_double(filter, FID_X, val); break;
            case SVF_Y:       set_double(filter, FID_Y, val); break;
            case SVF_WIDTH:   set_double(filter, FID_Width, val); break;
            case SVF_HEIGHT:  set_double(filter, FID_Height, val); break;
            case SVF_OPACITY: set_double(filter, FID_Opacity, val); break;
            case SVF_COLOR_INTERPOLATION_FILTERS: // The default is linearRGB
               if (!StrMatch("auto", val)) SetLong(filter, FID_ColourSpace, CS_LINEAR_RGB);
               else if (!StrMatch("sRGB", val)) SetLong(filter, FID_ColourSpace, CS_SRGB);
               else if (!StrMatch("linearRGB", val)) SetLong(filter, FID_ColourSpace, CS_LINEAR_RGB);
               else if (!StrMatch("inherit", val)) SetLong(filter, FID_ColourSpace, CS_INHERIT);
               break;
            case SVF_PRIMITIVEUNITS:
               if (!StrMatch("userSpaceOnUse", val)) filter->PrimitiveUnits = VUNIT_USERSPACE;
               else if (!StrMatch("objectBoundingBox", val)) filter->PrimitiveUnits = VUNIT_BOUNDING_BOX;
               break;

/*
            case SVF_VIEWBOX: {
               DOUBLE x=0, y=0, width=0, height=0;
               read_numseq(val, &x, &y, &width, &height, TAGEND);
               SetFields(filter->Viewport,
                  FID_ViewX|TDOUBLE,      x,
                  FID_ViewY|TDOUBLE,      y,
                  FID_ViewWidth|TDOUBLE,  width,
                  FID_ViewHeight|TDOUBLE, height,
                  TAGEND);
               break;
            }
*/
            default:
               LogErrorMsg("<%s> attribute '%s' unrecognised @ line %d", Tag->Attrib->Name, Tag->Attrib[a].Name, Tag->LineNo);
               break;
         }
      }

      if ((id) AND (!acInit(filter))) {
         SetName(filter, id);
         if (Tag->Child) {
            STRING xml_str;
            if (!xmlGetString(XML, Tag->Child->Index, XMF_INCLUDE_SIBLINGS, &xml_str)) {
               acDataXML(filter, xml_str);
               FreeResource(xml_str);
            }
         }

         scAddDef(Self->Scene, id, (OBJECTPTR)filter);
      }
      else acFree(filter);
   }
}

//****************************************************************************

static void process_pattern(objSVG *Self, objXML *XML, struct XMLTag *Tag)
{
   objVectorPattern *pattern;
   CSTRING id = NULL;

   if (!NewObject(ID_VECTORPATTERN, 0, &pattern)) {
      SetOwner(pattern, Self->Scene);
      SetFields(pattern,
         FID_Name|TSTR,          "SVGPattern",
         FID_Units|TLONG,        VUNIT_BOUNDING_BOX,
         FID_SpreadMethod|TLONG, VSPREAD_REPEAT,
         TAGEND);

      LONG a;
      for (a=1; a < Tag->TotalAttrib; a++) {
         CSTRING val = Tag->Attrib[a].Value;
         if (!val) continue;

         LONG j;
         for (j=0; Tag->Attrib[a].Name[j] AND (Tag->Attrib[a].Name[j] != ':'); j++);
         if (Tag->Attrib[a].Name[j] IS ':') continue;

         switch(StrHash(Tag->Attrib[a].Name, FALSE)) {
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

            case SVF_PATTERNTRANSFORM: SetString(pattern, FID_Transform, val); break;

            case SVF_ID:     id = val; break;
            case SVF_X:      set_double(pattern, FID_X, val); break;
            case SVF_Y:      set_double(pattern, FID_Y, val); break;
            case SVF_WIDTH:  set_double(pattern->Scene, FID_PageWidth, val); break;
            case SVF_HEIGHT: set_double(pattern->Scene, FID_PageHeight, val); break;

            case SVF_OPACITY: set_double(pattern, FID_Opacity, val); break;

            case SVF_VIEWBOX: {
               DOUBLE x=0, y=0, width=0, height=0;
               read_numseq(val, &x, &y, &width, &height, TAGEND);
               SetFields(pattern->Viewport,
                  FID_ViewX|TDOUBLE,      x,
                  FID_ViewY|TDOUBLE,      y,
                  FID_ViewWidth|TDOUBLE,  width,
                  FID_ViewHeight|TDOUBLE, height,
                  TAGEND);
               break;
            }

            default:
               LogErrorMsg("%s attribute '%s' unrecognised @ line %d", Tag->Attrib->Name, Tag->Attrib[a].Name, Tag->LineNo);
               break;
         }
      }

      if (!id) {
         acFree(pattern);
         MSG("Failed to create a valid definition.");
      }

      if (!acInit(pattern)) {
         // Child vectors for the pattern need to be instantiated and belong to the pattern's Viewport.
         svgState state;
         reset_state(&state);
         process_children(Self, XML, &state, Tag->Child, (OBJECTPTR)pattern->Viewport);
         scAddDef(Self->Scene, id, (OBJECTPTR)pattern);
         add_id(Self, Tag, id);
      }
      else {
         acFree(pattern);
         MSG("Pattern initialisation failed.");
      }
   }
}

//****************************************************************************

static ERROR process_shape(objSVG *Self, CLASSID VectorID, objXML *XML, svgState *State, struct XMLTag *Tag,
   OBJECTPTR Parent, OBJECTPTR *Result)
{
   ERROR error;
   OBJECTPTR vector;

   if (!(error = NewObject(VectorID, 0, &vector))) {
      SetOwner(vector, Parent);
      svgState state = *State;
      apply_state(&state, vector);
      if (Tag->Child) set_state(&state, Tag); // Apply all attribute values to the current state.

      process_attrib(Self, XML, Tag, vector);

      if (!acInit(vector)) {
         // Process child tags

         struct XMLTag *child;
         for (child=Tag->Child; child; child=child->Next) {
            if (child->Attrib->Name) {
               switch(StrHash(child->Attrib->Name, FALSE)) {
                  case SVF_ANIMATETRANSFORM: xtag_animatetransform(Self, XML, child, vector); break;
                  case SVF_ANIMATEMOTION:    xtag_animatemotion(Self, XML, child, vector); break;
                  case SVF_PARASOL_MORPH:    xtag_morph(Self, XML, child, vector); break;
                  case SVF_TEXTPATH:
                     if (VectorID IS ID_VECTORTEXT) {
                        if (child->Child) {
                           UBYTE buffer[8192];
                           if (!xmlGetContent(XML, child->Index, buffer, sizeof(buffer))) {
                              LONG ws;
                              for (ws=0; (buffer[ws]) AND (buffer[ws] <= 0x20); ws++); // All leading whitespace is ignored.
                              SetString(vector, FID_String, buffer + ws);
                           }
                           else MSG("Failed to retrieve content for <text> @ line %d", Tag->LineNo);
                        }

                        xtag_morph(Self, XML, child, vector);
                     }
                     break;
                  default:
                     LogErrorMsg("Failed to interpret vector child element <%s/> @ line %d", child->Attrib->Name, child->LineNo);
                     break;
               }
            }
         }

         *Result = vector;
         return error;
      }
      else {
         acFree(vector);
         return ERR_Init;
      }
   }
   else return ERR_CreateObject;
}

//****************************************************************************

static ERROR xtag_default(objSVG *Self, ULONG Hash, objXML *XML, svgState *State, struct XMLTag *Tag, OBJECTPTR Parent, OBJECTPTR *Vector)
{
   switch(Hash) {
      case SVF_USE:     xtag_use(Self, XML, State, Tag, Parent); break;
      case SVF_G:       xtag_group(Self, XML, State, Tag, Parent, Vector); break;
      case SVF_SVG:     xtag_svg(Self, XML, State, Tag, Parent, Vector); break;
      case SVF_RECT:    process_shape(Self, ID_VECTORRECTANGLE, XML, State, Tag, Parent, Vector); break;
      case SVF_ELLIPSE: process_shape(Self, ID_VECTORELLIPSE, XML, State, Tag, Parent, Vector); break;
      case SVF_CIRCLE:  process_shape(Self, ID_VECTORELLIPSE, XML, State, Tag, Parent, Vector); break;
      case SVF_PATH:    process_shape(Self, ID_VECTORPATH, XML, State, Tag, Parent, Vector); break;
      case SVF_POLYGON: process_shape(Self, ID_VECTORPOLYGON, XML, State, Tag, Parent, Vector); break;
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
      case SVF_FILTER:           xtag_filter(Self, XML, Tag); break;
      case SVF_DEFS:             xtag_defs(Self, XML, State, Tag, Parent); break;
      case SVF_CLIPPATH:         xtag_clippath(Self, XML, Tag); break;

      case SVF_TITLE:
         if (Self->Title) { FreeResource(Self->Title); Self->Title = NULL; }
         if (Tag->Child) {
            UBYTE buffer[8192];
            if (!xmlGetContent(XML, Tag->Index, buffer, sizeof(buffer))) {
               LONG ws;
               for (ws=0; buffer[ws] AND (buffer[ws] <= 0x20); ws++); // All leading whitespace is ignored.
               Self->Title = StrClone(buffer+ws);
            }
         }
         break;

      case SVF_LINE:
         process_shape(Self, ID_VECTORPOLYGON, XML, State, Tag, Parent, Vector);
         SetLong(*Vector, FID_Closed, FALSE);
         break;

      case SVF_POLYLINE:
         process_shape(Self, ID_VECTORPOLYGON, XML, State, Tag, Parent, Vector);
         SetLong(*Vector, FID_Closed, FALSE);
         break;

      case SVF_TEXT: {
         if (!process_shape(Self, ID_VECTORTEXT, XML, State, Tag, Parent, Vector)) {
            if (Tag->Child) {
               char buffer[8192];
               STRING str;
               LONG ws = 0;
               if ((!GetString(*Vector, FID_String, &str)) AND (str)) {
                  ws = StrCopy(str, buffer, sizeof(buffer));
               }
               if (!xmlGetContent(XML, Tag->Index, buffer + ws, sizeof(buffer) - ws)) {
                  if (!ws) while (buffer[ws] AND (buffer[ws] <= 0x20)) ws++; // All leading whitespace is ignored.
                  else ws = 0;
                  SetString(*Vector, FID_String, buffer + ws);
               }
               else MSG("Failed to retrieve content for <text> @ line %d", Tag->LineNo);
            }
         }
         break;
      }

      case SVF_DESC: break; // Ignore descriptions

      default: LogErrorMsg("Failed to interpret tag <%s/> ($%.8x) @ line %d", Tag->Attrib->Name, Hash, Tag->LineNo); return ERR_NoSupport;
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR load_pic(objSVG *Self, CSTRING Path, objPicture **Picture)
{
   *Picture = NULL;
   objFile *file = NULL;
   CSTRING val = Path;

   ERROR error = ERR_Okay;
   if (!StrCompare("data:", val, 5, 0)) { // Check for embedded content
      LogF("~load_pic()","Detected embedded source data");
      val += 5;
      if (!StrCompare("image/", val, 6, 0)) { // Has to be an image type
         val += 6;
         while ((*val) AND (*val != ';')) val++;
         if (!StrCompare(";base64", val, 7, 0)) { // Is it base 64?
            val += 7;
            while ((*val) AND (*val != ',')) val++;
            if (*val IS ',') val++;

            struct rkBase64Decode state;
            ClearMemory(&state, sizeof(state));

            UBYTE *output;
            LONG size = StrLength(val);
            if (!AllocMemory(size, MEM_DATA|MEM_NO_CLEAR, &output, NULL)) {
               LONG written;
               if (!(error = StrBase64Decode(&state, val, size, output, &written))) {
                  Path = "temp:svg.img";
                  if (!CreateObject(ID_FILE, NF_INTEGRAL, &file,
                        FID_Path|TSTR,   Path,
                        FID_Flags|TLONG, FL_NEW|FL_WRITE,
                        TAGEND)) {
                     LONG result;
                     acWrite(file, output, written, &result);
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
   else LogF("~load_pic()","%s", Path);

   if (!error) {
      error = CreateObject(ID_PICTURE, 0, Picture,
            FID_Owner|TLONG,        Self->Scene->Head.UniqueID,
            FID_Location|TSTR,      Path,
            FID_BitsPerPixel|TLONG, 32,
            FID_Flags|TLONG,        PCF_FORCE_ALPHA_32,
            TAGEND);
   }

   if (file) {
      flDelete(file, 0);
      acFree(file);
   }

   if (error) PostError(error);
   LogBack();
   return error;
}

//****************************************************************************
// Definition images are stored once, allowing them to be used multiple times via Fill and Stroke references.

static void def_image(objSVG *Self, struct XMLTag *Tag)
{
   objVectorImage *image;
   CSTRING id = NULL;
   objPicture *pic = NULL;

   if (!NewObject(ID_VECTORIMAGE, 0, &image)) {
      SetFields(image,
         FID_Owner|TLONG,        Self->Scene->Head.UniqueID,
         FID_Name|TSTR,          "SVGImage",
         FID_Units|TLONG,        VUNIT_BOUNDING_BOX,
         FID_SpreadMethod|TLONG, VSPREAD_PAD,
         TAGEND);

      LONG a;
      for (a=1; a < Tag->TotalAttrib; a++) {
         CSTRING val = Tag->Attrib[a].Value;
         if (!val) continue;

         switch(StrHash(Tag->Attrib[a].Name, FALSE)) {
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
               for (i=0; val[i] AND (val[i] != ':'); i++);
               if (val[i] != ':') LogErrorMsg("Failed to parse attrib '%s' in <image/> tag @ line %d", Tag->Attrib[a].Name, Tag->LineNo);
               break;
            }
         }
      }

      if (id) {
         if (pic) {
            SetPointer(image, FID_Picture, pic);
            if (!acInit(image)) {
               add_id(Self, Tag, id);
               scAddDef(Self->Scene, id, (OBJECTPTR)image);
            }
            else {
               acFree(image);
               MSG("Picture initialisation failed.");
            }
         }
         else {
            acFree(image);
            MSG("Unable to load a picture for <image/> '%s' at line %d", id, Tag->LineNo);
         }
      }
      else {
         acFree(image);
         MSG("No id specified in <image/> at line %d", Tag->LineNo);
      }
   }
}

//****************************************************************************

static ERROR xtag_image(objSVG *Self, objXML *XML, svgState *State, struct XMLTag *Tag, OBJECTPTR Parent, OBJECTPTR *Vector)
{
   STRING ratio = NULL;
   BYTE width_set = FALSE;
   BYTE height_set = FALSE;
   svgState state = *State;
   objPicture *pic = NULL;

   LONG a;
   for (a=1; a < Tag->TotalAttrib; a++) {
      if (!StrMatch("xlink:href", Tag->Attrib[a].Name)) {
         load_pic(Self, Tag->Attrib[a].Value, &pic);
      }
      else if (!StrMatch("preserveAspectRatio", Tag->Attrib[a].Name)) {
         // none, defer, xMinYMin, xMidyMin, xMaxYMin, xMinYMid, xMidYMid, xMaxYMid, xMinYMax, xMidYMax, xMaxYMax
         ratio = Tag->Attrib[a].Value;
      }
      else if (!StrMatch("width", Tag->Attrib[a].Name)) {
         width_set = TRUE;
      }
      else if (!StrMatch("height", Tag->Attrib[a].Name)) {
         height_set = TRUE;
      }
   }

   // First, load the image and add it to the vector definition

   if (pic) {
      objVectorImage *image;
      if (!CreateObject(ID_VECTORIMAGE, 0, &image,
            FID_Picture|TPTR,       pic,
            FID_SpreadMethod|TLONG, VSPREAD_PAD,
            FID_Units|TLONG,        VUNIT_BOUNDING_BOX,
            TAGEND)) {

         char id[32] = "img";
         IntToStr(image->Head.UniqueID, id+3, sizeof(id)-3);
         SetOwner(pic, image); // It's best if the pic belongs to the image.
         scAddDef(Self->Scene, id, (OBJECTPTR)image);

         UBYTE fillname[256];
         StrFormat(fillname, sizeof(fillname), "url(#%s)", id);

         // Use a rectangle shape to represent the image

         process_shape(Self, ID_VECTORRECTANGLE, XML, &state, Tag, Parent, Vector);
         SetString(*Vector, FID_Fill, "none");

         if (!width_set) SetLong(*Vector, FID_Width, pic->Bitmap->Width);
         if (!height_set) SetLong(*Vector, FID_Height, pic->Bitmap->Height);
         SetString(*Vector, FID_Fill, fillname);
         return ERR_Okay;
      }
      else return ERR_Failed;
   }
   else LogF("@xtag_image","Failed to load picture via xlink:href.");

   return ERR_Failed;
}

//****************************************************************************

static ERROR xtag_defs(objSVG *Self, objXML *XML, svgState *State, struct XMLTag *Tag, OBJECTPTR Parent)
{
   struct XMLTag *child;

   FMSG("~xtag_defs()","Tag: %p", Tag);

   for (child=Tag->Child; child; child=child->Next) {
      switch (StrHash(child->Attrib->Name, FALSE)) {
         case SVF_CONTOURGRADIENT: xtag_contourgradient(Self, child); break;
         case SVF_RADIALGRADIENT:  xtag_radialgradient(Self, child); break;
         case SVF_DIAMONDGRADIENT: xtag_diamondgradient(Self, child); break;
         case SVF_CONICGRADIENT:   xtag_conicgradient(Self, child); break;
         case SVF_LINEARGRADIENT:  xtag_lineargradient(Self, child); break;
         case SVF_PATTERN:         process_pattern(Self, XML, child); break;
         case SVF_IMAGE:           def_image(Self, child); break;
         case SVF_FILTER:          xtag_filter(Self, XML, child); break;
         case SVF_CLIPPATH:        xtag_clippath(Self, XML, child); break;
         case SVF_PARASOL_TRANSITION:  xtag_pathtransition(Self, XML, child); break;

         default: { // Anything not immediately recognised is added if it has an 'id' attribute.
            LONG a;
            for (a=1; a < child->TotalAttrib; a++) {
               if (!StrMatch("id", child->Attrib[a].Name)) {
                  add_id(Self, child, child->Attrib[a].Value);
                  break;
               }
            }
            break;
         }
      }
   }

   STEP();
   return ERR_Okay;
}

//****************************************************************************
// Declare a 'symbol' which is basically a template for inclusion elsewhere through the use of a 'use' element.
//
// When a use element is encountered, it looks for the associated symbol ID and then processes the XML child tags that
// belong to it.

static void xtag_symbol(objSVG *Self, objXML *XML, struct XMLTag *Tag)
{
   FMSG("~xtag_symbol()","Tag: %p", Tag);

   CSTRING id = XMLATTRIB(Tag, "id");
   if (id) {
      add_id(Self, Tag, id);
   }
   else LogErrorMsg("No id attribute specified in <symbol> at line %d.", Tag->LineNo);

   STEP();
}

/*****************************************************************************
** Most vector shapes can be morphed to the path of another vector.
*/

static void xtag_morph(objSVG *Self, objXML *XML, struct XMLTag *Tag, OBJECTPTR Parent)
{
   if ((!Parent) OR (Parent->ClassID != ID_VECTOR)) {
      FMSG("@xtag_morph()","Unable to apply morph to non-vector parent object.");
      return;
   }

   // Find the definition that is being referenced for the morph.

   CSTRING offset = NULL;
   CSTRING ref = NULL;
   CSTRING transition = NULL;
   LONG flags = 0;
   LONG a;
   for (a=1; (a < Tag->TotalAttrib); a++) {
      CSTRING val = Tag->Attrib[a].Value;

      switch(StrHash(Tag->Attrib[a].Name, FALSE)) {
         case SVF_PATH:
         case SVF_XLINK_HREF: ref = val; break;
         case SVF_TRANSITION: transition = val; break;
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
            if (!StrCompare("xMin", val, 4, 0)) { flags |= VMF_X_MIN; val += 4; }
            else if (!StrCompare("xMid", val, 4, 0)) { flags |= VMF_X_MID; val += 4; }
            else if (!StrCompare("xMax", val, 4, 0)) { flags |= VMF_X_MAX; val += 4; }
            while ((*val) AND (*val <= 0x20)) val++;
            if (!StrCompare("yMin", val, 4, 0)) { flags |= VMF_Y_MIN; val += 4; }
            else if (!StrCompare("yMid", val, 4, 0)) { flags |= VMF_Y_MID; val += 4; }
            else if (!StrCompare("yMax", val, 4, 0)) { flags |= VMF_Y_MAX; val += 4; }
            break;
      }
   }

   if (!ref) {
      LogErrorMsg("<morph> element @ line %d is missing a valid xlink:href attribute.", Tag->LineNo);
      return;
   }

   // Find the matching element with matching ID

   struct svgID *id = find_href(Self, ref);
   if (!id) {
      LogErrorMsg("Unable to find element '%s' referenced at line %d", ref, Tag->LineNo);
      return;
   }

   OBJECTPTR transvector = NULL;
   if (transition) {
      if (scFindDef(Self->Scene, transition, &transvector)) {
         LogErrorMsg("Unable to find element '%s' referenced at line %d", transition, Tag->LineNo);
         return;
      }
   }

   struct XMLTag *tagref = XML->Tags[id->TagIndex];

   CLASSID class_id = 0;
   switch (StrHash(tagref->Attrib[0].Name, FALSE)) {
      case SVF_PATH:           class_id = ID_VECTORPATH; break;
      case SVF_RECT:           class_id = ID_VECTORRECTANGLE; break;
      case SVF_ELLIPSE:        class_id = ID_VECTORELLIPSE; break;
      case SVF_CIRCLE:         class_id = ID_VECTORELLIPSE; break;
      case SVF_POLYGON:        class_id = ID_VECTORPOLYGON; break;
      case SVF_PARASOL_SPIRAL: class_id = ID_VECTORSPIRAL; break;
      case SVF_PARASOL_WAVE:   class_id = ID_VECTORWAVE; break;
      case SVF_PARASOL_SHAPE:  class_id = ID_VECTORSHAPE; break;
      default:
         LogErrorMsg("Invalid reference '%s', '%s' is not recognised by <morph>.", ref, tagref->Attrib[0].Name);
   }

   if (!(flags & (VMF_Y_MIN|VMF_Y_MID|VMF_Y_MAX))) {
      if (Parent->SubID IS ID_VECTORTEXT) flags |= VMF_Y_MIN;
      else flags |= VMF_Y_MID;
   }

   if (class_id) {
      OBJECTPTR shape;
      svgState state;
      reset_state(&state);
      process_shape(Self, class_id, XML, &state, tagref, &Self->Scene->Head, &shape);
      SetPointer(Parent, FID_Morph, shape);
      if (transvector) SetPointer(Parent, FID_Transition, transvector);
      SetLong(Parent, FID_MorphFlags, flags);
      scAddDef(Self->Scene, id->ID, shape);
   }
}

//****************************************************************************
// Duplicates a referenced area of the SVG definition.
//
// "The effect of a 'use' element is as if the contents of the referenced element were deeply cloned into a separate
// non-exposed DOM tree which had the 'use' element as its parent and all of the 'use' element's ancestors as its
// higher-level ancestors.

static void xtag_use(objSVG *Self, objXML *XML, svgState *State, struct XMLTag *Tag, OBJECTPTR Parent)
{
   CSTRING ref = NULL;
   LONG a;
   for (a=1; (a < Tag->TotalAttrib) AND (!ref); a++) {
      switch(StrHash(Tag->Attrib[a].Name, FALSE)) {
         case SVF_XLINK_HREF: ref = Tag->Attrib[a].Value; break;
      }
   }

   if (!ref) {
      LogErrorMsg("<use> element @ line %d is missing a valid xlink:href attribute.", Tag->LineNo);
      return;
   }

   // Find the matching element with matching ID

   struct svgID *id = find_href(Self, ref);
   if (!id) {
      LogErrorMsg("Unable to find element '%s'", ref);
      return;
   }

   OBJECTPTR vector = NULL;
   struct XMLTag *tagref = XML->Tags[id->TagIndex];

   svgState state = *State;
   set_state(&state, Tag); // Apply all attribute values to the current state.

   if ((!StrMatch("symbol", tagref->Attrib->Name)) OR (!StrMatch("svg", tagref->Attrib->Name))) {
      // SVG spec requires that we create a VectorGroup and then create a Viewport underneath that.  However if there
      // are no attributes to apply to the group then there is no sense in creating an empty one.

      OBJECTPTR group;
      UBYTE need_group = FALSE;
      for (a=1; (a < Tag->TotalAttrib) AND (!need_group); a++) {
         switch(StrHash(Tag->Attrib[a].Name, FALSE)) {
            case SVF_X: case SVF_Y: case SVF_WIDTH: case SVF_HEIGHT: break;
            default: need_group = TRUE; break;
         }
      }

      if (need_group) {
         if (!NewObject(ID_VECTORGROUP, 0, &group)) {
            SetOwner(group, Parent);
            Parent = group;
            acInit(group);
         }
      }





      if (NewObject(ID_VECTORVIEWPORT, 0, &vector)) return;
      SetOwner(vector, Parent);
      SetFields(vector, FID_Width|TPERCENT|TDOUBLE, 100.0, FID_Height|TPERCENT|TDOUBLE, 100.0, TAGEND); // SVG default

      // Apply attributes from 'use'
      for (a=1; a < Tag->TotalAttrib; a++) {
         CSTRING val;
         if (!(val = Tag->Attrib[a].Value)) continue;
         ULONG hash = StrHash(Tag->Attrib[a].Name, FALSE);
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

      for (a=1; a < tagref->TotalAttrib; a++) {
         CSTRING val;
         if (!(val = tagref->Attrib[a].Value)) continue;

         switch(StrHash(tagref->Attrib[a].Name, FALSE)) {
            case SVF_X:      set_double(vector, FID_X, val); break;
            case SVF_Y:      set_double(vector, FID_Y, val); break;
            case SVF_WIDTH:  set_double(vector, FID_Width, val); break;
            case SVF_HEIGHT: set_double(vector, FID_Height, val); break;
            case SVF_VIEWBOX:  {
               DOUBLE x=0, y=0, width=0, height=0;
               read_numseq(val, &x, &y, &width, &height, TAGEND);
               SetFields(vector,
                  FID_ViewX|TDOUBLE,      x,
                  FID_ViewY|TDOUBLE,      y,
                  FID_ViewWidth|TDOUBLE,  width,
                  FID_ViewHeight|TDOUBLE, height,
                  TAGEND);
               break;
            }
            case SVF_ID: break; // Ignore (already processed).
            default: LogErrorMsg("Not processing attribute '%s'", tagref->Attrib[a].Name); break;
         }
      }

      if (acInit(vector) != ERR_Okay) { acFree(vector); return; }

      // Add all child elements in <symbol> to the viewport.

      if ((id->TagIndex >= 0) AND (id->TagIndex < XML->TagCount)) {
         FMSG("~xtag_use","Processing all child elements within %s", ref);
         process_children(Self, XML, &state, XML->Tags[id->TagIndex]->Child, vector);
         STEP();
      }
      else FMSG("xtag_use","Element TagIndex %d is out of range.", id->TagIndex);
   }
   else { // SVG requires that the 'use' element is converted to a 'g' and that the 'use' attributes are applied to it.
      if (!NewObject(ID_VECTORGROUP, 0, &vector)) {
         SetOwner(vector, Parent);
         apply_state(&state, vector);
         process_attrib(Self, XML, Tag, vector); // Apply 'use' attributes to the group.

         if (acInit(vector) != ERR_Okay) { acFree(vector); return; }

         OBJECTPTR sibling = NULL;
         xtag_default(Self, StrHash(tagref->Attrib->Name, FALSE), XML, &state, tagref, vector, &sibling);
      }
   }
}

//****************************************************************************

static void xtag_group(objSVG *Self, objXML *XML, svgState *State, struct XMLTag *Tag, OBJECTPTR Parent, OBJECTPTR *Vector)
{
   FMSG("~xtag_group()","Tag: %p", Tag);

   svgState state = *State;

   OBJECTPTR group;
   if (NewObject(ID_VECTORGROUP, 0, &group) != ERR_Okay) return;
   SetOwner(group, Parent);
   if (Tag->Child) set_state(&state, Tag); // Apply all group attribute values to the current state.
   process_attrib(Self, XML, Tag, group);

   // Process child tags

   OBJECTPTR sibling = NULL;
   struct XMLTag *child;
   for (child = Tag->Child; child; child=child->Next) {
      if (child->Attrib->Name) {
         ULONG hash = StrHash(child->Attrib->Name, FALSE);
         switch(hash) {
            default: xtag_default(Self, hash, XML, &state, child, group, &sibling);  break;
         }
      }
   }

   if (!acInit(group)) *Vector = group;
   else acFree(group);

   STEP();
}

/*****************************************************************************
** <svg/> tags can be embedded inside <svg/> tags - this establishes a new viewport.
** Refer to section 7.9 of the SVG Specification for more information.
*/

static void xtag_svg(objSVG *Self, objXML *XML, svgState *State, struct XMLTag *Tag, OBJECTPTR Parent, OBJECTPTR *Vector)
{
   LONG a;

   if (!Parent) {
      LogF("@xtag_svg()","A Parent object is required.");
      return;
   }

   // Process <svg> attributes

   OBJECTPTR viewport;
   if (NewObject(ID_VECTORVIEWPORT, 0, &viewport)) return;
   SetOwner(viewport, Parent);

   // The first viewport to be instantiated is stored as a local reference.  This is important if the developer has
   // specified a custom target, in which case there needs to be a way to discover the root of the SVG.

   if (!Self->Viewport) Self->Viewport = viewport;

   svgState state = *State;
   if (Tag->Child) set_state(&state, Tag); // Apply all attribute values to the current state.

   for (a=1; a < Tag->TotalAttrib; a++) {
      CSTRING val;
      if (!(val = Tag->Attrib[a].Value)) continue;

      switch(StrHash(Tag->Attrib[a].Name, FALSE)) {
         // The viewbox determines what area of the vector definition is to be displayed (in a sense, zooming into the document).
         // The individual x, y, width and height values determine the position and clipping of the displayed SVG content.

         case SVF_VIEWBOX:  {
            DOUBLE x, y, width, height;
            read_numseq(val, &x, &y, &width, &height, TAGEND);
            SetFields(viewport,
               FID_ViewX|TDOUBLE,      x,
               FID_ViewY|TDOUBLE,      y,
               FID_ViewWidth|TDOUBLE,  width,
               FID_ViewHeight|TDOUBLE, height,
               TAGEND);
            break;
         }

         case SVF_VERSION: {
            DOUBLE version = StrToFloat(val);
            if (version > Self->SVGVersion) Self->SVGVersion = version;
            break;
         }

         case SVF_X: set_double(viewport, FID_X, val); break;
         case SVF_Y: set_double(viewport, FID_Y, val); break;
         case SVF_WIDTH: set_double(viewport, FID_Width, val); break;
         case SVF_HEIGHT: set_double(viewport, FID_Height, val); break;
         case SVF_PRESERVEASPECTRATIO: {
            LONG flags = 0;
            while ((*val) AND (*val <= 0x20)) val++;
            if (!StrMatch("none", val)) flags = ARF_NONE;
            else {
               if (!StrCompare("xMin", val, 4, 0)) { flags |= ARF_X_MIN; val += 4; }
               else if (!StrCompare("xMid", val, 4, 0)) { flags |= ARF_X_MID; val += 4; }
               else if (!StrCompare("xMax", val, 4, 0)) { flags |= ARF_X_MAX; val += 4; }

               if (!StrCompare("yMin", val, 4, 0)) { flags |= ARF_Y_MIN; val += 4; }
               else if (!StrCompare("yMid", val, 4, 0)) { flags |= ARF_Y_MID; val += 4; }
               else if (!StrCompare("yMax", val, 4, 0)) { flags |= ARF_Y_MAX; val += 4; }

               while ((*val) AND (*val <= 0x20)) val++;

               if (!StrCompare("meet", val, 4, 0)) { flags |= ARF_MEET; }
               else if (!StrCompare("slice", val, 5, 0)) { flags |= ARF_SLICE; }
            }
            SetLong(viewport, FID_AspectRatio, flags);
            break;
         }

         case SVF_ID:
            SetString(viewport, FID_ID, val);
            add_id(Self, Tag, val);
            break;

         case SVF_ENABLE_BACKGROUND:
            if ((!StrMatch("true", val)) OR (!StrMatch("1", val))) SetLong(viewport, FID_EnableBkgd, TRUE);
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
            // Check if this was a reference to some other namespace (ignorable).
            LONG i;
            for (i=0; val[i] AND (val[i] != ':'); i++);
            if (val[i] != ':') {
               LogErrorMsg("Failed to parse attrib '%s' in <svg/> tag @ line %d", Tag->Attrib[a].Name, Tag->LineNo);
            }
         }
      }
   }

   // Process child tags

   OBJECTPTR sibling = NULL;
   struct XMLTag *child;
   for (child=Tag->Child; child; child=child->Next) {
      if (child->Attrib->Name) {
         ULONG hash = StrHash(child->Attrib->Name, FALSE);

         FMSG("~xtag_svg","Processing <%s/>", child->Attrib->Name);

         switch(hash) {
            case SVF_DEFS: xtag_defs(Self, XML, &state, child, viewport); break;
            default:       xtag_default(Self, hash, XML, &state, child, viewport, &sibling);  break;
         }

         STEP();
      }
   }

   if (!acInit(viewport)) *Vector = viewport;
   else acFree(viewport);
}

//****************************************************************************
// <animateTransform attributeType="XML" attributeName="transform" type="rotate" from="0,150,150" to="360,150,150"
//   begin="0s" dur="5s" repeatCount="indefinite"/>

static ERROR xtag_animatetransform(objSVG *Self, objXML *XML, struct XMLTag *Tag, OBJECTPTR Parent)
{
   struct svgAnimation anim;
   LONG a;

   Self->Animated = TRUE;

   ClearMemory(&anim, sizeof(anim));
   anim.Replace = FALSE;
   anim.TargetVector = Parent->UniqueID;

   CSTRING value;
   for (a=1; a < Tag->TotalAttrib; a++) {
      if (!(value = Tag->Attrib[a].Value)) continue;

      switch(StrHash(Tag->Attrib[a].Name, FALSE)) {
         case SVF_ATTRIBUTENAME: // Name of the target attribute affected by the From and To values.
            if (anim.TargetAttribute) FreeResource(anim.TargetAttribute);
            anim.TargetAttribute = StrClone(value);
            break;

         case SVF_ATTRIBUTETYPE: // Namespace of the target attribute: XML, CSS, auto
            if (!StrMatch("XML", value));
            else if (!StrMatch("CSS", value));
            else if (!StrMatch("auto", value));
            break;

         case SVF_ID:
            if (anim.ID) FreeResource(anim.ID);
            anim.ID = StrClone(value);
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
            else LogErrorMsg("Unsupported type '%s'", value);
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
            if (anim.Values[0]) FreeResource(anim.Values[0]);
            anim.Values[0] = StrClone(value);
            if (anim.ValueCount < 1) anim.ValueCount = 1;
            break;
         }

         case SVF_TO: {
            if (anim.Values[1]) FreeResource(anim.Values[1]);
            anim.Values[1] = StrClone(value);
            if (anim.ValueCount < 2) anim.ValueCount = 2;
            break;
         }

         // Similar to from and to, this is a series of values that are interpolated over the time line.
         case SVF_VALUES: {
            LONG s, v = 0;
            while ((*value) AND (v < MAX_VALUES)) {
               STRING copy;
               while ((*value) AND (*value <= 0x20)) value++;
               CSTRING str = value;
               for (s=0; (str[s]) AND (str[s] != ';'); s++);
               if (!AllocMemory(s+1, MEM_STRING, &copy, NULL)) {
                  CopyMemory(str, copy, s);
                  copy[s] = 0;
                  anim.Values[v++] = copy;
               }
               value += s;
               if (*value IS ';') value++;
            }
            anim.ValueCount = v;
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
            if (!StrMatch("freeze", value)) anim.Freeze = TRUE; // Freeze the effect value at the last value of the duration (i.e. keep the last frame).
            else if (!StrMatch("remove", value)) anim.Freeze = TRUE; // The default.  The effect is stopped when the duration is over.
            break;

         case SVF_ADDITIVE: // replace, sum
            if (!StrMatch("replace", value)) anim.Replace = TRUE; // The animation values replace the underlying values of the target vector's attributes.
            else if (!StrMatch("sum", value)) anim.Replace = FALSE; // The animation adds to the underlying values of the target vector.
            break;

         case SVF_ACCUMULATE:
            if (!StrMatch("none", value)) anim.Accumulate = FALSE; // Repeat iterations are not cumulative.  This is the default.
            else if (!StrMatch("sum", value)) anim.Accumulate = TRUE; // Each repeated iteration builds on the last value of the previous iteration.
            break;

         default:
            break;
      }
   }

   struct svgAnimation *newAnim;
   if (!AllocMemory(sizeof(struct svgAnimation), MEM_DATA|MEM_NO_CLEAR, &newAnim, NULL)) {
      if (Self->Animations) anim.Next = Self->Animations;
      CopyMemory(&anim, newAnim, sizeof(struct svgAnimation));
      Self->Animations = newAnim;
      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

//****************************************************************************
// <animateMotion from="0,0" to="100,100" dur="4s" fill="freeze"/>

static ERROR xtag_animatemotion(objSVG *Self, objXML *XML, struct XMLTag *Tag, OBJECTPTR Parent)
{
   LONG a;

   Self->Animated = TRUE;

   for (a=1; a < Tag->TotalAttrib; a++) {
      if (!Tag->Attrib[a].Value) continue;

      switch(StrHash(Tag->Attrib[a].Name, FALSE)) {
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

//****************************************************************************

static void process_attrib(objSVG *Self, objXML *XML, struct XMLTag *Tag, OBJECTPTR Vector)
{
   LONG t;

   for (t=1; t < Tag->TotalAttrib; t++) {
      if (!Tag->Attrib[t].Value) continue;

      // Do not interpret non-SVG attributes, e.g. 'inkscape:dx'

      {
         LONG j;
         for (j=0; Tag->Attrib[t].Name[j] AND (Tag->Attrib[t].Name[j] != ':'); j++);
         if (Tag->Attrib[t].Name[j] IS ':') continue;
      }

      ULONG hash = StrHash(Tag->Attrib[t].Name, FALSE);

      FMSG("process_attrib","%s | %.8x = %.40s", Tag->Attrib[t].Name, hash, Tag->Attrib[t].Value);

      // Analyse the value to determine if it is a string or number

      if (set_property(Self, Vector, hash, XML, Tag, Tag->Attrib[t].Value)) {
         LogF("@process_attrib","Failed to set field '%s' with '%s' of %s", Tag->Attrib[t].Name, Tag->Attrib[t].Value, Vector->Class->ClassName);
      }
   }
}

//****************************************************************************

static ERROR set_property(objSVG *Self, OBJECTPTR Vector, ULONG Hash, objXML *XML, struct XMLTag *Tag, CSTRING StrValue)
{
   DOUBLE num;

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
            case SVF_CLOSE: SetString(Vector, FID_Close, StrValue); return ERR_Okay;
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
               GetDouble(Vector, FID_X, &x);
               num = read_unit(StrValue, &field_id);
               SetDouble(Vector, field_id, ABS(num - x));
               return ERR_Okay;
            }

            case SVF_Y2: {
               DOUBLE y;
               field_id = FID_Height;
               GetDouble(Vector, FID_Y, &y);
               num = read_unit(StrValue, &field_id);
               SetDouble(Vector, field_id, ABS(num - y));
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
            case SVF_POINTS: SetString(Vector, FID_Points, StrValue); return ERR_Okay;
         }
         break;
      }

      case ID_VECTORTEXT: {
         switch (Hash) {
            case SVF_DX: SetString(Vector, FID_DX, StrValue); return ERR_Okay;
            case SVF_DY: SetString(Vector, FID_DY, StrValue); return ERR_Okay;

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
               SetString(Vector, FID_Face, StrValue);
               return ERR_Okay;

            case SVF_FONT_SIZE:
               // A plain numeric font size is interpreted as "a height value corresponding to the current user
               // coordinate system".  Alternatively the user can specify the unit identifier, e.g. '12pt', '10%', '30px'
               SetString(Vector, FID_FontSize, StrValue);
               return ERR_Okay;

            case SVF_FONT_SIZE_ADJUST:
               // Auto-adjust the font height according to the formula "y(a/a') = c" where the value provided is used as 'a'.
               // y = 'font-size' of first-choice font
               // a' = aspect value of available font
               // c = 'font-size' to apply to available font
               return ERR_NoSupport;

            case SVF_FONT_STRETCH:
               switch(StrHash(StrValue, FALSE)) {
                  case SVF_NORMAL:          SetLong(Vector, FID_Stretch, VTS_NORMAL); return ERR_Okay;
                  case SVF_WIDER:           SetLong(Vector, FID_Stretch, VTS_WIDER); return ERR_Okay;
                  case SVF_NARROWER:        SetLong(Vector, FID_Stretch, VTS_NARROWER); return ERR_Okay;
                  case SVF_ULTRA_CONDENSED: SetLong(Vector, FID_Stretch, VTS_ULTRA_CONDENSED); return ERR_Okay;
                  case SVF_EXTRA_CONDENSED: SetLong(Vector, FID_Stretch, VTS_EXTRA_CONDENSED); return ERR_Okay;
                  case SVF_CONDENSED:       SetLong(Vector, FID_Stretch, VTS_CONDENSED); return ERR_Okay;
                  case VTS_SEMI_CONDENSED:  SetLong(Vector, FID_Stretch, VTS_SEMI_CONDENSED); return ERR_Okay;
                  case VTS_EXPANDED:        SetLong(Vector, FID_Stretch, VTS_EXPANDED); return ERR_Okay;
                  case VTS_SEMI_EXPANDED:   SetLong(Vector, FID_Stretch, VTS_SEMI_EXPANDED); return ERR_Okay;
                  case VTS_EXTRA_EXPANDED:  SetLong(Vector, FID_Stretch, VTS_EXTRA_EXPANDED); return ERR_Okay;
                  case VTS_ULTRA_EXPANDED:  SetLong(Vector, FID_Stretch, VTS_ULTRA_EXPANDED); return ERR_Okay;
                  default: LogErrorMsg("no support for font-stretch value '%s'", StrValue);
               }
               break;

            case SVF_FONT_STYLE: return ERR_NoSupport;
            case SVF_FONT_VARIANT: return ERR_NoSupport;

            case SVF_FONT_WEIGHT: { // SVG: normal | bold | bolder | lighter | inherit
               DOUBLE num = StrToFloat(StrValue);
               if (num) SetLong(Vector, FID_Weight, num);
               else switch(StrHash(StrValue, FALSE)) {
                  case SVF_NORMAL:  SetLong(Vector, FID_Weight, 400); return ERR_Okay;
                  case SVF_LIGHTER: SetLong(Vector, FID_Weight, 300); return ERR_Okay; // -100 off the inherited weight
                  case SVF_BOLD:    SetLong(Vector, FID_Weight, 700); return ERR_Okay;
                  case SVF_BOLDER:  SetLong(Vector, FID_Weight, 900); return ERR_Okay; // +100 on the inherited weight
                  case SVF_INHERIT: SetLong(Vector, FID_Weight, 400); return ERR_Okay; // Not supported correctly yet.
                  default: LogErrorMsg("No support for font-weight value '%s'", StrValue); // Non-fatal
               }
               break;
            }

            case SVF_ROTATE: SetString(Vector, FID_Rotate, StrValue); return ERR_Okay;
            case SVF_STRING: SetString(Vector, FID_String, StrValue); return ERR_Okay;

            case SVF_TEXT_ANCHOR:
               switch(StrHash(StrValue, FALSE)) {
                  case SVF_START:   SetLong(Vector, FID_Align, ALIGN_LEFT); return ERR_Okay;
                  case SVF_MIDDLE:  SetLong(Vector, FID_Align, ALIGN_HORIZONTAL); return ERR_Okay;
                  case SVF_END:     SetLong(Vector, FID_Align, ALIGN_RIGHT); return ERR_Okay;
                  case SVF_INHERIT: SetLong(Vector, FID_Align, 0); return ERR_Okay;
                  default: LogErrorMsg("text-anchor: No support for value '%s'", StrValue);
               }
               break;

            case SVF_TEXTLENGTH: SetString(Vector, FID_TextLength, StrValue); return ERR_Okay;
            // TextPath only
            //case SVF_STARTOFFSET: SetString(Vector, FID_StartOffset, StrValue); return ERR_Okay;
            //case SVF_METHOD: // The default is align.  For 'stretch' mode, set VMF_STRETCH in MorphFlags
            //                      SetString(Vector, FID_MorphFlags, StrValue); return ERR_Okay;
            //case SVF_SPACING:     SetString(Vector, FID_Spacing, StrValue); return ERR_Okay;
            //case SVF_XLINK_HREF:  // Used for drawing text along a path.
            //   return ERR_Okay;

            case SVF_KERNING: SetString(Vector, FID_Kerning, StrValue); return ERR_Okay; // Spacing between letters, default=1.0
            case SVF_LETTER_SPACING: SetString(Vector, FID_LetterSpacing, StrValue); return ERR_Okay;
            case SVF_PATHLENGTH: SetString(Vector, FID_PathLength, StrValue); return ERR_Okay;
            case SVF_WORD_SPACING:   SetString(Vector, FID_WordSpacing, StrValue); return ERR_Okay;
            case SVF_TEXT_DECORATION:
               switch(StrHash(StrValue, FALSE)) {
                  case SVF_UNDERLINE:    SetLong(Vector, FID_Flags, VTXF_UNDERLINE); return ERR_Okay;
                  case SVF_OVERLINE:     SetLong(Vector, FID_Flags, VTXF_OVERLINE); return ERR_Okay;
                  case SVF_LINETHROUGH:  SetLong(Vector, FID_Flags, VTXF_LINE_THROUGH); return ERR_Okay;
                  case SVF_BLINK:        SetLong(Vector, FID_Flags, VTXF_BLINK); return ERR_Okay;
                  case SVF_INHERIT:      return ERR_Okay;
                  default: LogErrorMsg("No support for text-decoration value '%s'", StrValue);
               }
               return ERR_Okay;
         }
         break;
      }

      case ID_VECTORSPIRAL: {
         FIELD field_id = 0;
         switch (Hash) {
            case SVF_PATHLENGTH: SetString(Vector, FID_PathLength, StrValue); return ERR_Okay;
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
               if ((!StrMatch("true", StrValue)) OR (!StrMatch("1", StrValue))) SetLong(Vector, FID_Close, TRUE);
               else SetLong(Vector, FID_Close, FALSE);
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
            case SVF_D: SetString(Vector, FID_Sequence, StrValue); return ERR_Okay;
            case SVF_PATHLENGTH: SetString(Vector, FID_PathLength, StrValue); return ERR_Okay;
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
         if (!scFindDef(Self->Scene, StrValue, &trans)) SetPointer(Vector, FID_Transition, trans);
         else LogErrorMsg("Unable to find element '%s' referenced at line %d", StrValue, Tag->LineNo);
         break;
      }

      case SVF_STROKE_LINEJOIN: {
         switch(StrHash(StrValue, FALSE)) {
            case SVF_MITER: SetLong(Vector, FID_LineJoin, VLJ_MITER); break;
            case SVF_ROUND: SetLong(Vector, FID_LineJoin, VLJ_ROUND); break;
            case SVF_BEVEL: SetLong(Vector, FID_LineJoin, VLJ_BEVEL); break;
            case SVF_INHERIT: SetLong(Vector, FID_LineJoin, VLJ_INHERIT); break;
            case SVF_MITER_REVERT: SetLong(Vector, FID_LineJoin, VLJ_MITER_REVERT); break; // Special AGG only join type
            case SVF_MITER_ROUND: SetLong(Vector, FID_LineJoin, VLJ_MITER_ROUND); break; // Special AGG only join type
         }
         break;
      }

      case SVF_STROKE_INNERJOIN: // AGG ONLY
         switch(StrHash(StrValue, FALSE)) {
            case SVF_MITER:   SetLong(Vector, FID_InnerJoin, VIJ_MITER);  break;
            case SVF_ROUND:   SetLong(Vector, FID_InnerJoin, VIJ_ROUND); break;
            case SVF_BEVEL:   SetLong(Vector, FID_InnerJoin, VIJ_BEVEL); break;
            case SVF_INHERIT: SetLong(Vector, FID_InnerJoin, VIJ_INHERIT); break;
            case SVF_JAG:     SetLong(Vector, FID_InnerJoin, VIJ_JAG); break;
         }

      case SVF_STROKE_LINECAP:
         switch(StrHash(StrValue, FALSE)) {
            case SVF_BUTT:    SetLong(Vector, FID_LineCap, VLC_BUTT); break;
            case SVF_SQUARE:  SetLong(Vector, FID_LineCap, VLC_SQUARE); break;
            case SVF_ROUND:   SetLong(Vector, FID_LineCap, VLC_ROUND); break;
            case SVF_INHERIT: SetLong(Vector, FID_LineCap, VLC_INHERIT); break;
         }
         break;

      case SVF_VISIBILITY:
         if (!StrMatch("visible", StrValue))       SetLong(Vector, FID_Visibility, VIS_VISIBLE);
         else if (!StrMatch("hidden", StrValue))   SetLong(Vector, FID_Visibility, VIS_HIDDEN);
         else if (!StrMatch("collapse", StrValue)) SetLong(Vector, FID_Visibility, VIS_COLLAPSE); // Same effect as hidden, kept for SVG compatibility
         else if (!StrMatch("inherit", StrValue))  SetLong(Vector, FID_Visibility, VIS_INHERIT);
         else LogErrorMsg("Unsupported visibility value '%s'", StrValue);
         break;

      case SVF_FILL_RULE:
         if (!StrMatch("nonzero", StrValue)) SetLong(Vector, FID_FillRule, VFR_NON_ZERO);
         else if (!StrMatch("evenodd", StrValue)) SetLong(Vector, FID_FillRule, VFR_EVEN_ODD);
         else if (!StrMatch("inherit", StrValue)) SetLong(Vector, FID_FillRule, VFR_INHERIT);
         else LogErrorMsg("Unsupported fill-rule value '%s'", StrValue);
         break;

      case SVF_CLIP_RULE:
         if (!StrMatch("nonzero", StrValue)) SetLong(Vector, FID_ClipRule, VFR_NON_ZERO);
         else if (!StrMatch("evenodd", StrValue)) SetLong(Vector, FID_ClipRule, VFR_EVEN_ODD);
         else if (!StrMatch("inherit", StrValue)) SetLong(Vector, FID_ClipRule, VFR_INHERIT);
         else LogErrorMsg("Unsupported clip-rule value '%s'", StrValue);
         break;

      case SVF_ENABLE_BACKGROUND:
         if (!StrMatch("new", StrValue)) SetLong(Vector, FID_EnableBkgd, TRUE);
         break;

      case SVF_ID:
         SetString(Vector, FID_ID, StrValue);
         add_id(Self, Tag, StrValue);
         break;

      case SVF_NUMERIC_ID:       SetString(Vector, FID_NumericID, StrValue); break;
      case SVF_DISPLAY:          LogErrorMsg("display is not supported."); break;
      case SVF_OVERFLOW: // visible | hidden | scroll | auto | inherit
         MSG("overflow is not supported.");
         break;
      case SVF_MARKER:           LogErrorMsg("marker is not supported."); break;
      case SVF_MARKER_END:       LogErrorMsg("marker-end is not supported."); break;
      case SVF_MARKER_MID:       LogErrorMsg("marker-mid is not supported."); break;
      case SVF_MARKER_START:     LogErrorMsg("marker-start is not supported."); break;

      case SVF_FILTER:           SetString(Vector, FID_Filter, StrValue); break;
      case SVF_STROKE:           SetString(Vector, FID_Stroke, StrValue); break;
      case SVF_COLOR:            SetString(Vector, FID_Fill, StrValue); break;
      case SVF_FILL:             SetString(Vector, FID_Fill, StrValue); break;
      case SVF_TRANSFORM: {
         if (Vector->ClassID IS ID_VECTOR) vecTransform((objVector *)Vector, StrValue);
         break;
      }
      case SVF_STROKE_DASHARRAY: SetString(Vector, FID_DashArray, StrValue); break;
      case SVF_OPACITY:          SetString(Vector, FID_Opacity, StrValue); break;
      case SVF_FILL_OPACITY:     SetDouble(Vector, FID_FillOpacity, StrToFloat(StrValue)); break;
      case SVF_STROKE_WIDTH:            field_id = FID_StrokeWidth; break;
      case SVF_STROKE_OPACITY:          SetString(Vector, FID_StrokeOpacity, StrValue); break;
      case SVF_STROKE_MITERLIMIT:       SetString(Vector, FID_MiterLimit, StrValue); break;
      case SVF_STROKE_MITERLIMIT_THETA: SetString(Vector, FID_MiterLimitTheta, StrValue); break;
      case SVF_STROKE_INNER_MITERLIMIT: SetString(Vector, FID_InnerMiterLimit, StrValue); break;
      case SVF_STROKE_DASHOFFSET:       field_id = FID_DashOffset; break;

      case SVF_MASK: {
         struct svgID *id = find_href(Self, StrValue);
         if (!id) {
            LogErrorMsg("Unable to find mask '%s'", StrValue);
            return ERR_Search;
         }

         // We need to add code that converts the content of a <mask> tag into a VectorFilter, because masking can be
         // achieved through filters.  There is no need for a dedicated masking class for this task.
         break;
      }

      case SVF_CLIP_PATH: {
         OBJECTPTR clip;
         if (!scFindDef(Self->Scene, StrValue, &clip)) {
            SetPointer(Vector, FID_Mask, clip);
         }
         else {
            LogErrorMsg("Unable to find clip-path '%s'", StrValue);
            return ERR_Search;
         }
         break;
      }

      default: return ERR_Failed;
   }

   if (field_id) {
      num = read_unit(StrValue, &field_id);
      SetField(Vector, field_id, num);
   }

   return ERR_Okay;
}
