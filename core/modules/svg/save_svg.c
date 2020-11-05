
static ERROR set_dimension(objXML *XML, LONG Index, CSTRING Attrib, DOUBLE Value, LONG Relative)
{
   char buffer[40];
   if (Relative) StrFormat(buffer, sizeof(buffer), "%g%%", Value * 100.0);
   else StrFormat(buffer, sizeof(buffer), "%g", Value);
   return xmlSetAttrib(XML, Index, XMS_NEW, Attrib, buffer);
}

//*********************************************************************************************************************

static ERROR save_vectorpath(objSVG *Self, objXML *XML, objVector *Vector, LONG Parent)
{
   STRING path;
   ERROR error;

   if (!(error = GetString(Vector, FID_Sequence, &path))) {
      LONG new_index;
      error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<path/>", &new_index);
      if (!error) error = xmlSetAttrib(XML, new_index, XMS_NEW, "d", path);
      FreeResource(path);

      if (!error) error = save_svg_scan_std(Self, XML, Vector, new_index);
   }

   return error;
}

//*********************************************************************************************************************

static ERROR save_svg_defs(objSVG *Self, objXML *XML, objVectorScene *Scene, LONG Parent)
{
   struct KeyStore *keystore;
   if (!GetPointer(Scene, FID_Defs, &keystore)) {
      char buffer[200];
      CSTRING key = NULL;
      OBJECTPTR *value, def;
      ERROR error;
      LONG new_index, def_index = 0;
      while (!VarIterate(keystore, key, &key, &value, NULL)) {
         if (!def_index) {
            if ((error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<defs/>", &def_index))) return error;
         }

         def = value[0];
         LogF("save_svg_defs","Processing definition %s (%x %x)", def->Class->ClassName, def->ClassID, def->SubID);

         if (def->ClassID IS ID_VECTORGRADIENT) {
            objVectorGradient *gradient = (objVectorGradient *)def;
            CSTRING gradient_type;
            switch(gradient->Type) {
               case VGT_RADIAL:  gradient_type = "<radialGradient/>"; break;
               case VGT_CONIC:   gradient_type = "<conicGradient/>"; break;
               case VGT_DIAMOND: gradient_type = "<diamondGradient/>"; break;
               case VGT_CONTOUR: gradient_type = "<contourGradient/>"; break;
               case VGT_LINEAR:
               default:          gradient_type = "<linearGradient/>"; break;
            }
            error = xmlInsertXML(XML, def_index, XMI_CHILD_END, gradient_type, &new_index);

            if (!error) error = xmlSetAttrib(XML, new_index, XMS_NEW, "id", key);

            LONG units;
            if ((!error) AND (!GetLong(gradient, FID_Units, &units))) {
               switch(units) {
                  case VUNIT_USERSPACE:    error = xmlSetAttrib(XML, new_index, XMS_NEW, "gradientUnits", "userSpaceOnUse"); break;
                  case VUNIT_BOUNDING_BOX: error = xmlSetAttrib(XML, new_index, XMS_NEW, "gradientUnits", "objectBoundingBox"); break;
               }
            }

            LONG spread;
            if ((!error) AND (!GetLong(gradient, FID_SpreadMethod, &spread))) {
               switch(spread) {
                  case VSPREAD_PAD:     break; // Pad is the default SVG setting
                  case VSPREAD_REFLECT: error = xmlSetAttrib(XML, new_index, XMS_NEW, "spreadMethod", "reflect"); break;
                  case VSPREAD_REPEAT:  error = xmlSetAttrib(XML, new_index, XMS_NEW, "spreadMethod", "repeat"); break;
               }
            }

            if ((gradient->Type IS VGT_LINEAR) OR (gradient->Type IS VGT_CONTOUR)) {
               if (!error) error = xmlSetAttribDouble(XML, new_index, XMS_NEW, "x1", gradient->X1);
               if (!error) error = xmlSetAttribDouble(XML, new_index, XMS_NEW, "y1", gradient->Y1);
               if (!error) error = xmlSetAttribDouble(XML, new_index, XMS_NEW, "x2", gradient->X2);
               if (!error) error = xmlSetAttribDouble(XML, new_index, XMS_NEW, "y2", gradient->Y2);
            }
            else if ((gradient->Type IS VGT_RADIAL) OR (gradient->Type IS VGT_DIAMOND) OR (gradient->Type IS VGT_CONIC)) {
               if ((!error) AND (gradient->Flags & (VGF_FIXED_CX|VGF_RELATIVE_CX)))
                  error = set_dimension(XML, new_index, "cx", gradient->CenterX, gradient->Flags & VGF_RELATIVE_CX);

               if ((!error) AND (gradient->Flags & (VGF_FIXED_CY|VGF_RELATIVE_CY)))
                  error = set_dimension(XML, new_index, "cy", gradient->CenterY, gradient->Flags & VGF_RELATIVE_CY);

               if ((!error) AND (gradient->Flags & (VGF_FIXED_FX|VGF_RELATIVE_FX)))
                  error = set_dimension(XML, new_index, "fx", gradient->FX, gradient->Flags & VGF_RELATIVE_FX);

               if ((!error) AND (gradient->Flags & (VGF_FIXED_FY|VGF_RELATIVE_FY)))
                  error = set_dimension(XML, new_index, "fy", gradient->FY, gradient->Flags & VGF_RELATIVE_FY);

               if ((!error) AND (gradient->Flags & (VGF_FIXED_RADIUS|VGF_RELATIVE_RADIUS)))
                  error = set_dimension(XML, new_index, "r", gradient->Radius, gradient->Flags & VGF_RELATIVE_RADIUS);
            }

            struct VectorTransform *transform;
            if ((!error) AND (!GetPointer(gradient, FID_Transforms, &transform)) AND (transform)) {
               if (!save_svg_transform(transform, buffer, sizeof(buffer))) {
                  error = xmlSetAttrib(XML, new_index, XMS_NEW, "gradientTransform", buffer);
               }
            }

            if (gradient->TotalStops > 0) {
               struct GradientStop *stops;
               LONG total_stops, s, stop_index;
               if (!GetFieldArray(gradient, FID_Stops, &stops, &total_stops)) {
                  for (s=0; (s < total_stops) AND (!error); s++) {
                     if (!(error = xmlInsertXML(XML, new_index, XMI_CHILD_END, "<stop/>", &stop_index))) {
                        error = xmlSetAttribDouble(XML, stop_index, XMS_NEW, "offset", stops[s].Offset);

                        StrFormat(buffer, sizeof(buffer), "stop-color:rgb(%g,%g,%g,%g)", stops[s].RGB.Red*255.0, stops[s].RGB.Green*255.0, stops[s].RGB.Blue*255.0, stops[s].RGB.Alpha*255.0);
                        error = xmlSetAttrib(XML, stop_index, XMS_NEW, "style", buffer);
                     }
                  }
               }
            }
         }
         else if (def->ClassID IS ID_VECTORIMAGE) {
            LogF("@save_svg_defs","VectorImage not supported.");
         }
         else if (def->SubID IS ID_VECTORPATH) {
            error = save_vectorpath(Self, XML, (objVector *)def, def_index);
         }
         else if (def->ClassID IS ID_VECTORPATTERN) {
            LogF("@save_svg_defs","VectorPattern not supported.");
         }
         else if (def->ClassID IS ID_VECTORFILTER) {
            objVectorFilter *filter = (objVectorFilter *)def;

            error = xmlInsertXML(XML, def_index, XMI_CHILD_END, "<filter/>", &new_index);

            if (!error) error = xmlSetAttrib(XML, new_index, XMS_NEW, "id", key);

            LONG dim;
            if (!error) error = GetLong(filter, FID_Dimensions, &dim);

            if ((!error) AND (dim & (DMF_RELATIVE_X|DMF_FIXED_X)))
               error = set_dimension(XML, new_index, "x", filter->X, dim & DMF_RELATIVE_X);

            if ((!error) AND (dim & (DMF_RELATIVE_Y|DMF_FIXED_Y)))
               error = set_dimension(XML, new_index, "y", filter->Y, dim & DMF_RELATIVE_Y);

            if ((!error) AND (dim & (DMF_RELATIVE_WIDTH|DMF_FIXED_WIDTH)))
               error = set_dimension(XML, new_index, "width", filter->Width, dim & DMF_RELATIVE_WIDTH);

            if ((!error) AND (dim & (DMF_RELATIVE_HEIGHT|DMF_FIXED_HEIGHT)))
               error = set_dimension(XML, new_index, "height", filter->Height, dim & DMF_RELATIVE_HEIGHT);

            LONG units;
            if ((!error) AND (!GetLong(filter, FID_Units, &units))) {
               switch(units) {
                  default:
                  case VUNIT_BOUNDING_BOX: break; // Default
                  case VUNIT_USERSPACE:    error = xmlSetAttrib(XML, new_index, XMS_NEW, "filterUnits", "userSpaceOnUse"); break;
               }
            }

            if ((!error) AND (!GetLong(filter, FID_PrimitiveUnits, &units))) {
               switch(units) {
                  case VUNIT_USERSPACE:    break;
                  case VUNIT_BOUNDING_BOX: error = xmlSetAttrib(XML, new_index, XMS_NEW, "primitiveUnits", "objectBoundingBox"); break;
               }
            }

            objXML *effect_xml;
            if ((!error) AND (!GetPointer(filter, FID_EffectXML, &effect_xml))) {
               STRING effects;
               if (!GetString(effect_xml, FID_Statement, &effects)) {
                  error = xmlInsertXML(XML, new_index, XMI_CHILD, effects, NULL);
                  FreeResource(effects);
               }
            }
         }
         else if (def->ClassID IS ID_VECTORTRANSITION) {
            LogF("@save_svg_defs","VectorTransition not supported.");
         }
         else if (def->SubID IS ID_VECTORCLIP) {
            LogF("@save_svg_defs","VectorClip not supported.");
         }
         else if (def->ClassID IS ID_VECTOR) {
            LogF("@save_svg_defs","%s not supported.", def->Class->ClassName);
         }
         else LogF("@save_svg_defs","Unrecognised definition class %x", def->ClassID);
      }

      return ERR_Okay;
   }
   else return ERR_Failed;
}

//*********************************************************************************************************************

static ERROR save_svg_transform(struct VectorTransform *Transform, char *Buffer, LONG Size)
{
   struct VectorTransform *t = Transform;
   LONG pos = 0;
   Buffer[0] = 0;
   while (t->Next) t = t->Next;
   while (t) {
      if (t->Type IS VTF_MATRIX) {
         pos += StrFormat(Buffer + pos, Size - pos, "matrix(%g %g %g %g %g %g) ", t->Matrix[0], t->Matrix[1], t->Matrix[2], t->Matrix[3], t->Matrix[4], t->Matrix[5]);
      }
      else if (t->Type IS VTF_TRANSLATE) {
         pos += StrFormat(Buffer + pos, Size - pos, "translate(%g %g) ", t->X, t->Y);
      }
      else if (t->Type IS VTF_SCALE) {
         if ((t->X IS t->Y) OR (t->Y IS 0)) pos += StrFormat(Buffer + pos, Size - pos, "scale(%g) ", t->X);
         else pos += StrFormat(Buffer + pos, Size - pos, "scale(%g %g) ", t->X, t->Y);
      }
      else if (t->Type IS VTF_ROTATE) {
         pos += StrFormat(Buffer + pos, Size - pos, "rotate(%g %g %g) ", t->Angle, t->X, t->Y);
      }
      else if (t->Type IS VTF_SKEW) {
         if (!t->Y) pos += StrFormat(Buffer + pos, Size - pos, "skewX(%g) ", t->X);
         else if (!t->X) pos += StrFormat(Buffer + pos, Size - pos, "skewY(%g) ", t->Y);
         else pos += StrFormat(Buffer + pos, Size - pos, "skew(%g %g) ", t->X, t->Y);
      }
      else LogF("@","Unrecognised transform command #%d", t->Type);

      t = t->Prev;
   }
   while ((pos > 0) AND (Buffer[pos-1] IS ' ')) pos--;
   Buffer[pos] = 0;
   return ERR_Okay;
}

//*********************************************************************************************************************

static ERROR save_svg_scan_std(objSVG *Self, objXML *XML, objVector *Vector, LONG Tag)
{
   char buffer[160];
   STRING str;
   FLOAT *colour;
   LONG array_size;
   ERROR error = ERR_Okay;

   if ((!error) AND (Vector->Opacity != 1.0))
      error = xmlSetAttribDouble(XML, Tag, XMS_NEW, "opacity", Vector->Opacity);

   if ((!error) AND (Vector->FillOpacity != 1.0))
      error = xmlSetAttribDouble(XML, Tag, XMS_NEW, "fill-opacity", Vector->FillOpacity);

   if ((!error) AND (Vector->StrokeOpacity != 1.0))
      error = xmlSetAttribDouble(XML, Tag, XMS_NEW, "stroke-opacity", Vector->StrokeOpacity);

   if ((!GetString(Vector, FID_Stroke, &str)) AND (str)) {
      error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke", str);
   }
   else if ((!GetFieldArray(Vector, FID_StrokeColour, &colour, &array_size)) AND (colour[3] != 0)) {
      StrFormat(buffer, sizeof(buffer), "rgb(%g,%g,%g,%g)", colour[0], colour[1], colour[2], colour[3]);
      error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-color", buffer);
   }

   LONG line_join;
   if ((!error) AND (!GetLong(Vector, FID_LineJoin, &line_join))) {
      switch (line_join) {
         default:
         case VLJ_MITER:        break; // Default
         case VLJ_MITER_REVERT: error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-linejoin", "miter-revert"); break; // Parasol
         case VLJ_ROUND:        error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-linejoin", "round"); break;
         case VLJ_BEVEL:        error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-linejoin", "bevel"); break;
         case VLJ_MITER_ROUND:  error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-linejoin", "arcs"); break; // (SVG2) Not sure if compliant
         case VLJ_INHERIT:      error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-linejoin", "inherit"); break;
      } // "miter-clip" SVG2
   }

   LONG inner_join;
   if ((!error) AND (!GetLong(Vector, FID_InnerJoin, &inner_join))) { // Parasol only
      switch (inner_join) {
         default:
         case VIJ_MITER:   break; // Default
         case VIJ_BEVEL:   error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-innerjoin", "bevel"); break;
         case VIJ_JAG:     error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-innerjoin", "jag"); break;
         case VIJ_ROUND:   error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-innerjoin", "round"); break;
         case VIJ_INHERIT: error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-innerjoin", "inherit"); break;
      }
   }

   if ((!error) AND (Vector->DashTotal > 0)) {
      DOUBLE dash_offset;
      if ((!GetDouble(Vector, FID_DashOffset, &dash_offset)) AND (dash_offset != 0)) {
         error = xmlSetAttribDouble(XML, Tag, XMS_NEW, "stroke-dashoffset", Vector->DashOffset);
      }

      DOUBLE *dash_array;
      LONG dash_total;
      if (!GetFieldArray(Vector, FID_DashArray, &dash_array, &dash_total)) {
         LONG i, pos = 0;
         for (i=0; i < dash_total; i++) {
            if (pos != 0) buffer[pos++] = ',';
            pos += StrFormat(buffer+pos, sizeof(buffer)-pos, "%g", dash_array[i]);
            if (pos >= sizeof(buffer)-2) return ERR_BufferOverflow;
         }
         error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-dasharray", buffer);
      }
   }

   LONG linecap;
   if ((!error) AND (!GetLong(Vector, FID_LineCap, &linecap))) {
      switch (linecap) {
         default:
         case VLC_BUTT:    break; // Default
         case VLC_SQUARE:  error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-linecap", "square"); break;
         case VLC_ROUND:   error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-linecap", "round"); break;
         case VLC_INHERIT: error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-linecap", "inherit"); break;
      }
   }

   if (Vector->Visibility IS VIS_HIDDEN)        error = xmlSetAttrib(XML, Tag, XMS_NEW, "visibility", "hidden");
   else if (Vector->Visibility IS VIS_COLLAPSE) error = xmlSetAttrib(XML, Tag, XMS_NEW, "visibility", "collapse");
   else if (Vector->Visibility IS VIS_INHERIT)  error = xmlSetAttrib(XML, Tag, XMS_NEW, "visibility", "inherit");

   if ((!error) AND (Vector->StrokeWidth != 1.0))
      error = xmlSetAttribDouble(XML, Tag, XMS_NEW, "stroke-width", Vector->StrokeWidth);

   if ((!error) AND (!GetString(Vector, FID_Fill, &str)) AND (str)) {
      if (StrMatch("rgb(0,0,0)", str) != ERR_Okay) {
         error = xmlSetAttrib(XML, Tag, XMS_NEW, "fill", str);
      }
   }
   else if ((!error) AND (!GetFieldArray(Vector, FID_FillColour, &colour, &array_size)) AND (colour[3] != 0)) {
      StrFormat(buffer, sizeof(buffer), "rgb(%g,%g,%g,%g)", colour[0], colour[1], colour[2], colour[3]);
      error = xmlSetAttrib(XML, Tag, XMS_NEW, "fill", buffer);
   }

   LONG fill_rule;
   if ((!error) AND (!GetLong(Vector, FID_FillRule, &fill_rule))) {
      if (fill_rule IS VFR_EVEN_ODD) error = xmlSetAttrib(XML, Tag, XMS_NEW, "fill-rule", "evenodd");
   }

   if ((!error) AND (!(error = GetString(Vector, FID_ID, &str))) AND (str))
      error = xmlSetAttrib(XML, Tag, XMS_NEW, "id", str);

   if ((!error) AND (!GetString(Vector, FID_Filter, &str)) AND (str))
      error = xmlSetAttrib(XML, Tag, XMS_NEW, "filter", str);

   struct VectorTransform *transform;
   if ((!error) AND (!GetPointer(Vector, FID_Transforms, &transform)) AND (transform)) {
      if (!(error = save_svg_transform(transform, buffer, sizeof(buffer)))) {
         error = xmlSetAttrib(XML, Tag, XMS_NEW, "transform", buffer);
      }
   }

   OBJECTPTR shape;
   if ((!error) AND (!GetPointer(Vector, FID_Morph, &shape)) AND (shape)) {
      LONG morph_tag, morph_flags;

      error = xmlInsertXML(XML, Tag, XMI_CHILD_END, "<parasol:morph/>", &morph_tag);

      STRING shape_id;
      if ((!error) AND (!GetString(shape, FID_ID, &shape_id)) AND (shape_id)) {
         // NB: It is required that the shape has previously been registered as a definition, otherwise the url will refer to a dud tag.
         char shape_ref[120];
         StrFormat(shape_ref, sizeof(shape_ref), "url(#%s)", shape_id);
         error = xmlSetAttrib(XML, morph_tag, XMS_NEW, "xlink:href", shape_ref);
      }

      if (!error) error = GetLong(Vector, FID_MorphFlags, &morph_flags);

      if ((!error) AND (morph_flags & VMF_STRETCH)) error = xmlSetAttrib(XML, morph_tag, XMS_NEW, "method", "stretch");

      if ((!error) AND (morph_flags & VMF_AUTO_SPACING)) error = xmlSetAttrib(XML, morph_tag, XMS_NEW, "spacing", "auto");

      if ((!error) AND (morph_flags & (VMF_X_MIN|VMF_X_MID|VMF_X_MAX|VMF_Y_MIN|VMF_Y_MID|VMF_Y_MAX))) {
         char align[40];
         WORD apos = 0;
         if (morph_flags & VMF_X_MIN) apos = StrCopy("xMin ", align, sizeof(align));
         else if (morph_flags & VMF_X_MID) apos = StrCopy("xMid ", align, sizeof(align));
         else if (morph_flags & VMF_X_MAX) apos = StrCopy("xMax ", align, sizeof(align));

         if (morph_flags & VMF_Y_MIN) StrCopy("yMin", align+apos, sizeof(align));
         else if (morph_flags & VMF_Y_MID) StrCopy("yMid", align+apos, sizeof(align));
         else if (morph_flags & VMF_Y_MAX) StrCopy("yMax", align+apos, sizeof(align));

         error = xmlSetAttrib(XML, morph_tag, XMS_NEW, "align", align);
      }

      struct rkVectorTransition *tv;
      if ((!error) AND (!GetPointer(Vector, FID_Transition, &tv))) {

#warning TODO save_svg_scan_std transition support







      }
   }

   return error;
}

//*********************************************************************************************************************

static ERROR save_svg_scan(objSVG *Self, objXML *XML, objVector *Vector, LONG Parent)
{
   LONG new_index = -1;

   LogF("~save_scan()","%s", Vector->Head.Class->ClassName);

   ERROR error = ERR_Okay;
   if (Vector->Head.SubID IS ID_VECTORRECTANGLE) {
      DOUBLE rx, ry, x, y, width, height;
      LONG dim;

      error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<rect/>", &new_index);

      if (!error) error = GetLong(Vector, FID_Dimensions, &dim);

      if ((!error) AND (!GetDouble(Vector, FID_RoundX, &rx)) AND (rx != 0))
         error = set_dimension(XML, new_index, "rx", rx, FALSE);

      if ((!error) AND (!GetDouble(Vector, FID_RoundY, &ry)) AND (ry != 0))
         error = set_dimension(XML, new_index, "ry", ry, FALSE);

      if ((!error) AND (!GetDouble(Vector, FID_X, &x)))
         error = set_dimension(XML, new_index, "x", x, dim & DMF_RELATIVE_X);

      if ((!error) AND (!GetDouble(Vector, FID_Y, &y)))
         error = set_dimension(XML, new_index, "y", y, dim & DMF_RELATIVE_Y);

      if ((!error) AND (!GetDouble(Vector, FID_Width, &width)))
         error = set_dimension(XML, new_index, "width", width, dim & DMF_RELATIVE_WIDTH);

      if ((!error) AND (!GetDouble(Vector, FID_Height, &height)))
         error = set_dimension(XML, new_index, "height", height, dim & DMF_RELATIVE_HEIGHT);

      if (!error) save_svg_scan_std(Self, XML, Vector, new_index);
   }
   else if (Vector->Head.SubID IS ID_VECTORELLIPSE) {
      DOUBLE rx, ry, cx, cy;
      LONG dim;

      error = GetFields(Vector, FID_Dimensions|TLONG, &dim,
         FID_RadiusX|TDOUBLE, &rx, FID_RadiusY|TDOUBLE, &ry,
         FID_CenterX|TDOUBLE, &cx, FID_CenterY|TDOUBLE, &cy,
         TAGEND);

      if (!error) error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<ellipse/>", &new_index);
      if (!error) error = set_dimension(XML, new_index, "rx", rx, dim & DMF_RELATIVE_RADIUS_X);
      if (!error) error = set_dimension(XML, new_index, "ry", ry, dim & DMF_RELATIVE_RADIUS_Y);
      if (!error) error = set_dimension(XML, new_index, "cx", cx, dim & DMF_RELATIVE_CENTER_X);
      if (!error) error = set_dimension(XML, new_index, "cy", cy, dim & DMF_RELATIVE_CENTER_Y);

      if (!error) error = save_svg_scan_std(Self, XML, Vector, new_index);
   }
   else if (Vector->Head.SubID IS ID_VECTORPATH) {
      error = save_vectorpath(Self, XML, Vector, Parent);
   }
   else if (Vector->Head.SubID IS ID_VECTORPOLYGON) { // Serves <polygon>, <line> and <polyline>
      struct VectorPoint *points;
      LONG total_points, i;
      char buffer[2048];

      if ((!GetLong(Vector, FID_Closed, &i)) AND (i IS FALSE)) { // Line or Polyline
         if (!(error = GetFieldArray(Vector, FID_PointsArray, &points, &total_points))) {
            if (total_points IS 2) {
               error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<line/>", &new_index);
               if (!error) error = set_dimension(XML, new_index, "x1", points[0].X, points[0].XRelative);
               if (!error) error = set_dimension(XML, new_index, "y1", points[0].Y, points[0].YRelative);
               if (!error) error = set_dimension(XML, new_index, "x2", points[1].X, points[1].XRelative);
               if (!error) error = set_dimension(XML, new_index, "y2", points[1].Y, points[1].YRelative);
            }
            else {
               error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<polyline/>", &new_index);
               if (!error) {
                  WORD pos = 0;
                  for (i=0; i < total_points; i++) {
                     pos += StrFormat(buffer+pos, sizeof(buffer)-pos, "%g,%g ", points[i].X, points[i].Y);
                     if (pos >= sizeof(buffer)) { error = ERR_BufferOverflow; break; }
                  }
               }
               if (!error) error = xmlSetAttrib(XML, new_index, XMS_NEW, "points", buffer);
            }
         }
      }
      else {
         error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<polygon/>", &new_index);

         if ((!error) AND (!GetFieldArray(Vector, FID_PointsArray, &points, &total_points))) {
            WORD pos = 0;
            for (i=0; i < total_points; i++) {
               pos += StrFormat(buffer+pos, sizeof(buffer)-pos, "%g,%g ", points[i].X, points[i].Y);
               if (pos >= sizeof(buffer)) { error = ERR_BufferOverflow; break; }
            }
            if (!error) error = xmlSetAttrib(XML, new_index, XMS_NEW, "points", buffer);
         }
      }

      DOUBLE path_length;
      if ((!(error = GetDouble(Vector, FID_PathLength, &path_length))) AND (path_length != 0)) {
         error = xmlSetAttribDouble(XML, new_index, XMS_NEW, "pathLength", path_length);
      }

      if (!error) error = save_svg_scan_std(Self, XML, Vector, new_index);
   }
   else if (Vector->Head.SubID IS ID_VECTORTEXT) {
      DOUBLE x, y, *dx, *dy, *rotate, text_length;
      LONG total, i, weight;
      STRING str;
      char buffer[1024];

      error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<text/>", &new_index);

      if ((!error) AND (!GetDouble(Vector, FID_X, &x)))
         error = set_dimension(XML, new_index, "x", x, FALSE);

      if ((!error) AND (!GetDouble(Vector, FID_Y, &y)))
         error = set_dimension(XML, new_index, "y", y, FALSE);

      if ((!error) AND (!(error = GetFieldArray(Vector, FID_DX, &dx, &total))) AND (total > 0)) {
         LONG pos = 0;
         for (i=0; i < total; i++) {
            if (pos != 0) buffer[pos++] = ',';
            pos += StrFormat(buffer+pos, sizeof(buffer)-pos, "%g", dx[i]);
            if (pos >= sizeof(buffer)-2) return ERR_BufferOverflow;
         }
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "dx", buffer);
      }

      if ((!error) AND (!(error = GetFieldArray(Vector, FID_DY, &dy, &total))) AND (total > 0)) {
         LONG pos = 0;
         for (i=0; i < total; i++) {
            if (pos != 0) buffer[pos++] = ',';
            pos += StrFormat(buffer+pos, sizeof(buffer)-pos, "%g", dy[i]);
            if (pos >= sizeof(buffer)-2) return ERR_BufferOverflow;
         }
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "dy", buffer);
      }

      if ((!error) AND (!(error = GetString(Vector, FID_FontSize, &str)))) {
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "font-size", str);
         FreeResource(str);
      }

      if ((!error) AND (!(error = GetFieldArray(Vector, FID_Rotate, &rotate, &total))) AND (total > 0)) {
         LONG pos = 0;
         for (i=0; i < total; i++) {
            if (pos != 0) buffer[pos++] = ',';
            pos += StrFormat(buffer+pos, sizeof(buffer)-pos, "%g", rotate[i]);
            if (pos >= sizeof(buffer)-2) return ERR_BufferOverflow;
         }
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "rotate", buffer);
      }

      if ((!error) AND (!(error = GetDouble(Vector, FID_TextLength, &text_length))) AND (text_length))
         error = xmlSetAttribLong(XML, new_index, XMS_NEW, "textLength", text_length);

      if ((!error) AND (!(error = GetString(Vector, FID_Face, &str))))
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "font-family", str);

      if ((!error) AND (!(error = GetLong(Vector, FID_Weight, &weight))) AND (weight != 400))
         error = xmlSetAttribLong(XML, new_index, XMS_NEW, "font-weight", weight);

      if ((!error) AND (!(error = GetString(Vector, FID_String, &str))))
         error = xmlInsertContent(XML, new_index, XMI_CHILD, str, NULL);

      // TODO: lengthAdjust, font, font-size-adjust, font-stretch, font-style, font-variant, text-anchor, kerning, letter-spacing, path-length, word-spacing, text-decoration

      if (!error) error = save_svg_scan_std(Self, XML, Vector, new_index);
   }
   else if (Vector->Head.SubID IS ID_VECTORGROUP) {
      error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<g/>", &new_index);
      if (!error) error = save_svg_scan_std(Self, XML, Vector, new_index);
   }
   else if (Vector->Head.SubID IS ID_VECTORCLIP) {
      STRING str;
      if ((!(error = GetString(Vector, FID_ID, &str))) AND (str)) { // The id is an essential requirement
         error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<clipPath/>", &new_index);

         LONG units;
         if (!GetLong(Vector, FID_Units, &units)) {
            switch(units) {
               default:
               case VUNIT_USERSPACE:    break; // Default
               case VUNIT_BOUNDING_BOX: xmlSetAttrib(XML, new_index, XMS_NEW, "clipPathUnits", "objectBoundingBox"); break;
            }
         }

         if (!error) error = save_svg_scan_std(Self, XML, Vector, new_index);
      }
   }
   else if (Vector->Head.SubID IS ID_VECTORWAVE) {
      DOUBLE dbl;
      LONG dim;

      error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<parasol:wave/>", &new_index);

      if (!error) error = GetLong(Vector, FID_Dimensions, &dim);

      if ((!error) AND (!GetDouble(Vector, FID_X, &dbl)))
         error = set_dimension(XML, new_index, "x", dbl, dim & DMF_RELATIVE_X);

      if ((!error) AND (!GetDouble(Vector, FID_Y, &dbl)))
         error = set_dimension(XML, new_index, "y", dbl, dim & DMF_RELATIVE_Y);

      if ((!error) AND (!GetDouble(Vector, FID_Width, &dbl)))
         error = set_dimension(XML, new_index, "width", dbl, dim & DMF_RELATIVE_WIDTH);

      if ((!error) AND (!GetDouble(Vector, FID_Height, &dbl)))
         error = set_dimension(XML, new_index, "height", dbl, dim & DMF_RELATIVE_HEIGHT);

      if ((!error) AND (!GetDouble(Vector, FID_Amplitude, &dbl)))
         error = xmlSetAttribDouble(XML, new_index, XMS_NEW, "amplitude", dbl);

      if ((!error) AND (!GetDouble(Vector, FID_Frequency, &dbl)))
         error = xmlSetAttribDouble(XML, new_index, XMS_NEW, "frequency", dbl);

      if ((!error) AND (!GetDouble(Vector, FID_Decay, &dbl)))
         error = xmlSetAttribDouble(XML, new_index, XMS_NEW, "decay", dbl);

      if ((!error) AND (!GetDouble(Vector, FID_Degree, &dbl)))
         error = xmlSetAttribDouble(XML, new_index, XMS_NEW, "degree", dbl);

      LONG close;
      if ((!error) AND (!GetLong(Vector, FID_Close, &close)))
         error = xmlSetAttribLong(XML, new_index, XMS_NEW, "close", close);

      if ((!error) AND (!GetDouble(Vector, FID_Thickness, &dbl)))
         error = xmlSetAttribDouble(XML, new_index, XMS_NEW, "thickness", dbl);

      if (!error) error = save_svg_scan_std(Self, XML, Vector, new_index);
   }
   else if (Vector->Head.SubID IS ID_VECTORSPIRAL) {
      DOUBLE dbl;
      LONG dim, length;

      error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<parasol:spiral/>", &new_index);
      if (error) { LogReturn(); return error; }

      if ((error = GetLong(Vector, FID_Dimensions, &dim))) return error;

      if ((!error) AND (!GetDouble(Vector, FID_CenterX, &dbl)))
         error = set_dimension(XML, new_index, "cx", dbl, dim & DMF_RELATIVE_CENTER_X);

      if ((!error) AND (!GetDouble(Vector, FID_CenterY, &dbl)))
         error = set_dimension(XML, new_index, "cy", dbl, dim & DMF_RELATIVE_CENTER_Y);

      if ((!error) AND (!GetDouble(Vector, FID_Width, &dbl)))
         error = set_dimension(XML, new_index, "width", dbl, dim & DMF_RELATIVE_WIDTH);

      if ((!error) AND (!GetDouble(Vector, FID_Height, &dbl)))
         error = set_dimension(XML, new_index, "height", dbl, dim & DMF_RELATIVE_HEIGHT);

      if ((!error) AND (!GetDouble(Vector, FID_Offset, &dbl)))
         error = xmlSetAttribDouble(XML, new_index, XMS_NEW, "offset", dbl);

      if ((!error) AND (!GetLong(Vector, FID_PathLength, &length)) AND (length != 0))
         error = xmlSetAttribLong(XML, new_index, XMS_NEW, "pathLength", length);

      if ((!error) AND (!GetDouble(Vector, FID_Radius, &dbl)))
         error = set_dimension(XML, new_index, "r", dbl, dim & DMF_RELATIVE_RADIUS);

      if ((!error) AND (!GetDouble(Vector, FID_Scale, &dbl)))
         error = xmlSetAttribDouble(XML, new_index, XMS_NEW, "scale", dbl);

      if ((!error) AND (!GetDouble(Vector, FID_Step, &dbl)))
         error = xmlSetAttribDouble(XML, new_index, XMS_NEW, "step", dbl);

      if (!error) error = save_svg_scan_std(Self, XML, Vector, new_index);
   }
   else if (Vector->Head.SubID IS ID_VECTORSHAPE) {
      DOUBLE dbl;
      LONG num, dim;

      error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<parasol:shape/>", &new_index);

      if ((error = GetLong(Vector, FID_Dimensions, &dim))) return error;

      if ((!error) AND (!GetDouble(Vector, FID_CenterX, &dbl)))
         error = set_dimension(XML, new_index, "cx", dbl, dim & DMF_RELATIVE_CENTER_X);

      if ((!error) AND (!GetDouble(Vector, FID_CenterY, &dbl)))
         error = set_dimension(XML, new_index, "cy", dbl, dim & DMF_RELATIVE_CENTER_Y);

      if ((!error) AND (!GetDouble(Vector, FID_Radius, &dbl)))
         error = set_dimension(XML, new_index, "r", dbl, dim & DMF_RELATIVE_RADIUS);

      if ((!error) AND (!GetDouble(Vector, FID_A, &dbl))) error = xmlSetAttribDouble(XML, new_index, XMS_NEW, "a", dbl);
      if ((!error) AND (!GetDouble(Vector, FID_B, &dbl))) error = xmlSetAttribDouble(XML, new_index, XMS_NEW, "b", dbl);
      if ((!error) AND (!GetDouble(Vector, FID_M, &dbl))) error = xmlSetAttribDouble(XML, new_index, XMS_NEW, "m", dbl);
      if ((!error) AND (!GetDouble(Vector, FID_N1, &dbl))) error = xmlSetAttribDouble(XML, new_index, XMS_NEW, "n1", dbl);
      if ((!error) AND (!GetDouble(Vector, FID_N2, &dbl))) error = xmlSetAttribDouble(XML, new_index, XMS_NEW, "n2", dbl);
      if ((!error) AND (!GetDouble(Vector, FID_N3, &dbl))) error = xmlSetAttribDouble(XML, new_index, XMS_NEW, "n3", dbl);
      if ((!error) AND (!GetDouble(Vector, FID_Phi, &dbl))) error = xmlSetAttribDouble(XML, new_index, XMS_NEW, "phi", dbl);

      if ((!error) AND (!GetLong(Vector, FID_Phi, &num))) error = xmlSetAttribLong(XML, new_index, XMS_NEW, "phi", num);
      if ((!error) AND (!GetLong(Vector, FID_Vertices, &num))) error = xmlSetAttribLong(XML, new_index, XMS_NEW, "vertices", num);
      if ((!error) AND (!GetLong(Vector, FID_Mod, &num))) error = xmlSetAttribLong(XML, new_index, XMS_NEW, "mod", num);
      if ((!error) AND (!GetLong(Vector, FID_Spiral, &num))) error = xmlSetAttribLong(XML, new_index, XMS_NEW, "spiral", num);
      if ((!error) AND (!GetLong(Vector, FID_Repeat, &num))) error = xmlSetAttribLong(XML, new_index, XMS_NEW, "repeat", num);
      if ((!error) AND (!GetLong(Vector, FID_Close, &num))) error = xmlSetAttribLong(XML, new_index, XMS_NEW, "close", num);

      if (!error) error = save_svg_scan_std(Self, XML, Vector, new_index);
   }
   else if (Vector->Head.SubID IS ID_VECTORVIEWPORT) {
      DOUBLE x, y, width, height;
      LONG dim;

      error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<svg/>", &new_index);

      if ((!error) AND (!(error = GetFields(Vector, FID_ViewX|TDOUBLE, &x, FID_ViewY|TDOUBLE, &y, FID_ViewWidth|TDOUBLE, &width, FID_ViewHeight|TDOUBLE, &height, TAGEND)))) {
         char buffer[80];
         StrFormat(buffer, sizeof(buffer), "%g %g %g %g", x, y, width, height);
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "viewBox", buffer);
      }

      if ((!error) AND (!(error = GetLong(Vector, FID_Dimensions, &dim)))) {
         if ((!error) AND (dim & (DMF_RELATIVE_X|DMF_FIXED_X)) AND (!GetDouble(Vector, FID_X, &x)))
            error = set_dimension(XML, new_index, "x", x, dim & DMF_RELATIVE_X);

         if ((!error) AND (dim & (DMF_RELATIVE_Y|DMF_FIXED_Y)) AND (!GetDouble(Vector, FID_Y, &y)))
            error = set_dimension(XML, new_index, "y", y, dim & DMF_RELATIVE_Y);

         if ((!error) AND (dim & (DMF_RELATIVE_WIDTH|DMF_FIXED_WIDTH)) AND (!GetDouble(Vector, FID_Width, &width)))
            error = set_dimension(XML, new_index, "width", width, dim & DMF_RELATIVE_WIDTH);

         if ((!error) AND (dim & (DMF_RELATIVE_HEIGHT|DMF_FIXED_HEIGHT)) AND (!GetDouble(Vector, FID_Height, &height)))
            error = set_dimension(XML, new_index, "height", height, dim & DMF_RELATIVE_HEIGHT);
      }
   }
   else {
      LogF("save_scan","Unrecognised class \"%s\"", Vector->Head.Class->ClassName);
      LogReturn();
      return ERR_Okay; // Skip objects in the scene graph that we don't recognise
   }

   if (!error) {
      objVector *scan;
      for (scan=Vector->Child; scan; scan=scan->Next) {
         save_svg_scan(Self, XML, scan, new_index);
      }
   }

   LogReturn();
   return error;
}
