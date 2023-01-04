
static ERROR set_dimension(objXML *XML, LONG Index, CSTRING Attrib, DOUBLE Value, LONG Relative)
{
   char buffer[40];
   if (Relative) snprintf(buffer, sizeof(buffer), "%g%%", Value * 100.0);
   else snprintf(buffer, sizeof(buffer), "%g", Value);
   return xmlSetAttrib(XML, Index, XMS_NEW, Attrib, buffer);
}

//*********************************************************************************************************************

static ERROR save_vectorpath(extSVG *Self, objXML *XML, objVector *Vector, LONG Parent)
{
   STRING path;
   ERROR error;

   if (!(error = Vector->get(FID_Sequence, &path))) {
      LONG new_index;
      error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<path/>", &new_index);
      if (!error) error = xmlSetAttrib(XML, new_index, XMS_NEW, "d", path);
      FreeResource(path);

      if (!error) error = save_svg_scan_std(Self, XML, Vector, new_index);
   }

   return error;
}

//*********************************************************************************************************************

static ERROR save_svg_defs(extSVG *Self, objXML *XML, objVectorScene *Scene, LONG Parent)
{
   parasol::Log log(__FUNCTION__);
   KeyStore *keystore;

   if (!Scene->getPtr(FID_Defs, &keystore)) {
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
         log.msg("Processing definition %s (%x %x)", def->Class->ClassName, def->ClassID, def->SubID);

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
            if ((!error) and (!gradient->get(FID_Units, &units))) {
               switch(units) {
                  case VUNIT_USERSPACE:    error = xmlSetAttrib(XML, new_index, XMS_NEW, "gradientUnits", "userSpaceOnUse"); break;
                  case VUNIT_BOUNDING_BOX: error = xmlSetAttrib(XML, new_index, XMS_NEW, "gradientUnits", "objectBoundingBox"); break;
               }
            }

            LONG spread;
            if ((!error) and (!gradient->get(FID_SpreadMethod, &spread))) {
               switch(spread) {
                  case VSPREAD_PAD:     break; // Pad is the default SVG setting
                  case VSPREAD_REFLECT: error = xmlSetAttrib(XML, new_index, XMS_NEW, "spreadMethod", "reflect"); break;
                  case VSPREAD_REPEAT:  error = xmlSetAttrib(XML, new_index, XMS_NEW, "spreadMethod", "repeat"); break;
               }
            }

            if ((gradient->Type IS VGT_LINEAR) or (gradient->Type IS VGT_CONTOUR)) {
               if (!error) error = xmlSetAttrib(XML, new_index, XMS_NEW, "x1", gradient->X1);
               if (!error) error = xmlSetAttrib(XML, new_index, XMS_NEW, "y1", gradient->Y1);
               if (!error) error = xmlSetAttrib(XML, new_index, XMS_NEW, "x2", gradient->X2);
               if (!error) error = xmlSetAttrib(XML, new_index, XMS_NEW, "y2", gradient->Y2);
            }
            else if ((gradient->Type IS VGT_RADIAL) or (gradient->Type IS VGT_DIAMOND) or (gradient->Type IS VGT_CONIC)) {
               if ((!error) and (gradient->Flags & (VGF_FIXED_CX|VGF_RELATIVE_CX)))
                  error = set_dimension(XML, new_index, "cx", gradient->CenterX, gradient->Flags & VGF_RELATIVE_CX);

               if ((!error) and (gradient->Flags & (VGF_FIXED_CY|VGF_RELATIVE_CY)))
                  error = set_dimension(XML, new_index, "cy", gradient->CenterY, gradient->Flags & VGF_RELATIVE_CY);

               if ((!error) and (gradient->Flags & (VGF_FIXED_FX|VGF_RELATIVE_FX)))
                  error = set_dimension(XML, new_index, "fx", gradient->FX, gradient->Flags & VGF_RELATIVE_FX);

               if ((!error) and (gradient->Flags & (VGF_FIXED_FY|VGF_RELATIVE_FY)))
                  error = set_dimension(XML, new_index, "fy", gradient->FY, gradient->Flags & VGF_RELATIVE_FY);

               if ((!error) and (gradient->Flags & (VGF_FIXED_RADIUS|VGF_RELATIVE_RADIUS)))
                  error = set_dimension(XML, new_index, "r", gradient->Radius, gradient->Flags & VGF_RELATIVE_RADIUS);
            }

            VectorMatrix *transform;
            if ((!error) and (!gradient->getPtr(FID_Transforms, &transform)) and (transform)) {
               if (!save_svg_transform(transform, buffer, sizeof(buffer))) {
                  error = xmlSetAttrib(XML, new_index, XMS_NEW, "gradientTransform", buffer);
               }
            }

            if (gradient->TotalStops > 0) {
               GradientStop *stops;
               LONG total_stops, stop_index;
               if (!GetFieldArray(gradient, FID_Stops, &stops, &total_stops)) {
                  for (LONG s=0; (s < total_stops) and (!error); s++) {
                     if (!(error = xmlInsertXML(XML, new_index, XMI_CHILD_END, "<stop/>", &stop_index))) {
                        error = xmlSetAttrib(XML, stop_index, XMS_NEW, "offset", stops[s].Offset);

                        snprintf(buffer, sizeof(buffer), "stop-color:rgb(%g,%g,%g,%g)", stops[s].RGB.Red*255.0, stops[s].RGB.Green*255.0, stops[s].RGB.Blue*255.0, stops[s].RGB.Alpha*255.0);
                        error = xmlSetAttrib(XML, stop_index, XMS_NEW, "style", buffer);
                     }
                  }
               }
            }
         }
         else if (def->ClassID IS ID_VECTORIMAGE) {
            log.warning("VectorImage not supported.");
         }
         else if (def->SubID IS ID_VECTORPATH) {
            error = save_vectorpath(Self, XML, (objVector *)def, def_index);
         }
         else if (def->ClassID IS ID_VECTORPATTERN) {
            log.warning("VectorPattern not supported.");
         }
         else if (def->ClassID IS ID_VECTORFILTER) {
            objVectorFilter *filter = (objVectorFilter *)def;

            error = xmlInsertXML(XML, def_index, XMI_CHILD_END, "<filter/>", &new_index);

            if (!error) error = xmlSetAttrib(XML, new_index, XMS_NEW, "id", key);

            LONG dim;
            if (!error) error = filter->get(FID_Dimensions, &dim);

            if ((!error) and (dim & (DMF_RELATIVE_X|DMF_FIXED_X)))
               error = set_dimension(XML, new_index, "x", filter->X, dim & DMF_RELATIVE_X);

            if ((!error) and (dim & (DMF_RELATIVE_Y|DMF_FIXED_Y)))
               error = set_dimension(XML, new_index, "y", filter->Y, dim & DMF_RELATIVE_Y);

            if ((!error) and (dim & (DMF_RELATIVE_WIDTH|DMF_FIXED_WIDTH)))
               error = set_dimension(XML, new_index, "width", filter->Width, dim & DMF_RELATIVE_WIDTH);

            if ((!error) and (dim & (DMF_RELATIVE_HEIGHT|DMF_FIXED_HEIGHT)))
               error = set_dimension(XML, new_index, "height", filter->Height, dim & DMF_RELATIVE_HEIGHT);

            LONG units;
            if ((!error) and (!filter->get(FID_Units, &units))) {
               switch(units) {
                  default:
                  case VUNIT_BOUNDING_BOX: break; // Default
                  case VUNIT_USERSPACE:    error = xmlSetAttrib(XML, new_index, XMS_NEW, "filterUnits", "userSpaceOnUse"); break;
               }
            }

            if ((!error) and (!filter->get(FID_PrimitiveUnits, &units))) {
               switch(units) {
                  case VUNIT_USERSPACE:    break;
                  case VUNIT_BOUNDING_BOX: error = xmlSetAttrib(XML, new_index, XMS_NEW, "primitiveUnits", "objectBoundingBox"); break;
               }
            }

            STRING effect_xml;
            if ((!error) and (!filter->get(FID_EffectXML, &effect_xml))) {
               error = xmlInsertXML(XML, new_index, XMI_CHILD, effect_xml, NULL);
               FreeResource(effect_xml);
            }
         }
         else if (def->ClassID IS ID_VECTORTRANSITION) {
            log.warning("VectorTransition not supported.");
         }
         else if (def->SubID IS ID_VECTORCLIP) {
            log.warning("VectorClip not supported.");
         }
         else if (def->ClassID IS ID_VECTOR) {
            log.warning("%s not supported.", def->Class->ClassName);
         }
         else log.warning("Unrecognised definition class %x", def->ClassID);
      }

      return ERR_Okay;
   }
   else return ERR_Failed;
}

//*********************************************************************************************************************

static ERROR save_svg_transform(VectorMatrix *Transform, char *Buffer, LONG Size)
{
   Buffer[0] = 0;
   std::vector<VectorMatrix *> list;

   for (auto t=Transform; t; t=t->Next) list.push_back(t);

   LONG pos = 0;
   std::for_each(list.rbegin(), list.rend(), [&](auto t) {
      pos += snprintf(Buffer + pos, Size - pos, "matrix(%f %f %f %f %f %f) ", t->ScaleX, t->ShearY, t->ShearX, t->ScaleY, t->TranslateX, t->TranslateY);
   });

   while ((pos > 0) and (Buffer[pos-1] IS ' ')) pos--;
   Buffer[pos] = 0;
   return ERR_Okay;
}

//*********************************************************************************************************************

static ERROR save_svg_scan_std(extSVG *Self, objXML *XML, objVector *Vector, LONG Tag)
{
   parasol::Log log(__FUNCTION__);
   char buffer[160];
   STRING str;
   FLOAT *colour;
   LONG array_size;
   ERROR error = ERR_Okay;

   if ((!error) and (Vector->Opacity != 1.0))
      error = xmlSetAttrib(XML, Tag, XMS_NEW, "opacity", Vector->Opacity);

   if ((!error) and (Vector->FillOpacity != 1.0))
      error = xmlSetAttrib(XML, Tag, XMS_NEW, "fill-opacity", Vector->FillOpacity);

   if ((!error) and (Vector->StrokeOpacity != 1.0))
      error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-opacity", Vector->StrokeOpacity);

   if ((!Vector->get(FID_Stroke, &str)) and (str)) {
      error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke", str);
   }
   else if ((!GetFieldArray(Vector, FID_StrokeColour, &colour, &array_size)) and (colour[3] != 0)) {
      snprintf(buffer, sizeof(buffer), "rgb(%g,%g,%g,%g)", colour[0], colour[1], colour[2], colour[3]);
      error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-color", buffer);
   }

   LONG line_join;
   if ((!error) and (!Vector->get(FID_LineJoin, &line_join))) {
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
   if ((!error) and (!Vector->get(FID_InnerJoin, &inner_join))) { // Parasol only
      switch (inner_join) {
         default:
         case VIJ_MITER:   break; // Default
         case VIJ_BEVEL:   error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-innerjoin", "bevel"); break;
         case VIJ_JAG:     error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-innerjoin", "jag"); break;
         case VIJ_ROUND:   error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-innerjoin", "round"); break;
         case VIJ_INHERIT: error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-innerjoin", "inherit"); break;
      }
   }

   DOUBLE *dash_array;
   LONG dash_total;
   if ((!error) and (!GetFieldArray(Vector, FID_DashArray, &dash_array, &dash_total)) and (dash_array)) {
      DOUBLE dash_offset;
      if ((!Vector->get(FID_DashOffset, &dash_offset)) and (dash_offset != 0)) {
         error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-dashoffset", Vector->DashOffset);
      }

      LONG pos = 0;
      for (LONG i=0; i < dash_total; i++) {
         if (pos != 0) buffer[pos++] = ',';
         pos += snprintf(buffer+pos, sizeof(buffer)-pos, "%g", dash_array[i]);
         if ((size_t)pos >= sizeof(buffer)-2) return ERR_BufferOverflow;
      }
      error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-dasharray", buffer);
   }

   LONG linecap;
   if ((!error) and (!Vector->get(FID_LineCap, &linecap))) {
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

   STRING stroke_width;
   if ((!error) and (!Vector->get(FID_StrokeWidth, &stroke_width))) {
      if (!stroke_width) stroke_width = "0";
      if ((stroke_width[0] != '1') and (stroke_width[1] != 0)) {
         error = xmlSetAttrib(XML, Tag, XMS_NEW, "stroke-width", stroke_width);
      }
   }

   if ((!error) and (!Vector->get(FID_Fill, &str)) and (str)) {
      if (StrMatch("rgb(0,0,0)", str) != ERR_Okay) {
         error = xmlSetAttrib(XML, Tag, XMS_NEW, "fill", str);
      }
   }
   else if ((!error) and (!GetFieldArray(Vector, FID_FillColour, &colour, &array_size)) and (colour[3] != 0)) {
      snprintf(buffer, sizeof(buffer), "rgb(%g,%g,%g,%g)", colour[0], colour[1], colour[2], colour[3]);
      error = xmlSetAttrib(XML, Tag, XMS_NEW, "fill", buffer);
   }

   LONG fill_rule;
   if ((!error) and (!Vector->get(FID_FillRule, &fill_rule))) {
      if (fill_rule IS VFR_EVEN_ODD) error = xmlSetAttrib(XML, Tag, XMS_NEW, "fill-rule", "evenodd");
   }

   if ((!error) and (!(error = Vector->get(FID_ID, &str))) and (str))
      error = xmlSetAttrib(XML, Tag, XMS_NEW, "id", str);

   if ((!error) and (!Vector->get(FID_Filter, &str)) and (str))
      error = xmlSetAttrib(XML, Tag, XMS_NEW, "filter", str);

   VectorMatrix *transform;
   if ((!error) and (!Vector->getPtr(FID_Transforms, &transform)) and (transform)) {
      if (!(error = save_svg_transform(transform, buffer, sizeof(buffer)))) {
         error = xmlSetAttrib(XML, Tag, XMS_NEW, "transform", buffer);
      }
   }

   OBJECTPTR shape;
   if ((!error) and (!Vector->getPtr(FID_Morph, &shape)) and (shape)) {
      LONG morph_tag, morph_flags;

      error = xmlInsertXML(XML, Tag, XMI_CHILD_END, "<parasol:morph/>", &morph_tag);

      STRING shape_id;
      if ((!error) and (!shape->get(FID_ID, &shape_id)) and (shape_id)) {
         // NB: It is required that the shape has previously been registered as a definition, otherwise the url will refer to a dud tag.
         char shape_ref[120];
         snprintf(shape_ref, sizeof(shape_ref), "url(#%s)", shape_id);
         error = xmlSetAttrib(XML, morph_tag, XMS_NEW, "xlink:href", shape_ref);
      }

      if (!error) error = Vector->get(FID_MorphFlags, &morph_flags);

      if ((!error) and (morph_flags & VMF_STRETCH)) error = xmlSetAttrib(XML, morph_tag, XMS_NEW, "method", "stretch");

      if ((!error) and (morph_flags & VMF_AUTO_SPACING)) error = xmlSetAttrib(XML, morph_tag, XMS_NEW, "spacing", "auto");

      if ((!error) and (morph_flags & (VMF_X_MIN|VMF_X_MID|VMF_X_MAX|VMF_Y_MIN|VMF_Y_MID|VMF_Y_MAX))) {
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
      if ((!error) and (!Vector->getPtr(FID_Transition, &tv))) {

#warning TODO save_svg_scan_std transition support







      }
   }

   return error;
}

//*********************************************************************************************************************

static ERROR save_svg_scan(extSVG *Self, objXML *XML, objVector *Vector, LONG Parent)
{
   parasol::Log log(__FUNCTION__);

   LONG new_index = -1;

   log.branch("%s", Vector->Class->ClassName);

   ERROR error = ERR_Okay;
   if (Vector->SubID IS ID_VECTORRECTANGLE) {
      DOUBLE rx, ry, x, y, width, height;
      LONG dim;

      error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<rect/>", &new_index);

      if (!error) error = Vector->get(FID_Dimensions, &dim);

      if ((!error) and (!Vector->get(FID_RoundX, &rx)) and (rx != 0))
         error = set_dimension(XML, new_index, "rx", rx, FALSE);

      if ((!error) and (!Vector->get(FID_RoundY, &ry)) and (ry != 0))
         error = set_dimension(XML, new_index, "ry", ry, FALSE);

      if ((!error) and (!Vector->get(FID_X, &x)))
         error = set_dimension(XML, new_index, "x", x, dim & DMF_RELATIVE_X);

      if ((!error) and (!Vector->get(FID_Y, &y)))
         error = set_dimension(XML, new_index, "y", y, dim & DMF_RELATIVE_Y);

      if ((!error) and (!Vector->get(FID_Width, &width)))
         error = set_dimension(XML, new_index, "width", width, dim & DMF_RELATIVE_WIDTH);

      if ((!error) and (!Vector->get(FID_Height, &height)))
         error = set_dimension(XML, new_index, "height", height, dim & DMF_RELATIVE_HEIGHT);

      if (!error) save_svg_scan_std(Self, XML, Vector, new_index);
   }
   else if (Vector->SubID IS ID_VECTORELLIPSE) {
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
   else if (Vector->SubID IS ID_VECTORPATH) {
      error = save_vectorpath(Self, XML, Vector, Parent);
   }
   else if (Vector->SubID IS ID_VECTORPOLYGON) { // Serves <polygon>, <line> and <polyline>
      VectorPoint *points;
      LONG total_points, i;
      char buffer[2048];

      if ((!Vector->get(FID_Closed, &i)) and (i IS FALSE)) { // Line or Polyline
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
                     pos += snprintf(buffer+pos, sizeof(buffer)-pos, "%g,%g ", points[i].X, points[i].Y);
                     if ((size_t)pos >= sizeof(buffer)) { error = ERR_BufferOverflow; break; }
                  }
               }
               if (!error) error = xmlSetAttrib(XML, new_index, XMS_NEW, "points", buffer);
            }
         }
      }
      else {
         error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<polygon/>", &new_index);

         if ((!error) and (!GetFieldArray(Vector, FID_PointsArray, &points, &total_points))) {
            WORD pos = 0;
            for (i=0; i < total_points; i++) {
               pos += snprintf(buffer+pos, sizeof(buffer)-pos, "%g,%g ", points[i].X, points[i].Y);
               if ((size_t)pos >= sizeof(buffer)) { error = ERR_BufferOverflow; break; }
            }
            if (!error) error = xmlSetAttrib(XML, new_index, XMS_NEW, "points", buffer);
         }
      }

      DOUBLE path_length;
      if ((!(error = Vector->get(FID_PathLength, &path_length))) and (path_length != 0)) {
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "pathLength", path_length);
      }

      if (!error) error = save_svg_scan_std(Self, XML, Vector, new_index);
   }
   else if (Vector->SubID IS ID_VECTORTEXT) {
      DOUBLE x, y, *dx, *dy, *rotate, text_length;
      LONG total, i, weight;
      STRING str;
      char buffer[1024];

      error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<text/>", &new_index);

      if ((!error) and (!Vector->get(FID_X, &x)))
         error = set_dimension(XML, new_index, "x", x, FALSE);

      if ((!error) and (!Vector->get(FID_Y, &y)))
         error = set_dimension(XML, new_index, "y", y, FALSE);

      if ((!error) and (!(error = GetFieldArray(Vector, FID_DX, &dx, &total))) and (total > 0)) {
         LONG pos = 0;
         for (LONG i=0; i < total; i++) {
            if (pos != 0) buffer[pos++] = ',';
            pos += snprintf(buffer+pos, sizeof(buffer)-pos, "%g", dx[i]);
            if ((size_t)pos >= sizeof(buffer)-2) return ERR_BufferOverflow;
         }
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "dx", buffer);
      }

      if ((!error) and (!(error = GetFieldArray(Vector, FID_DY, &dy, &total))) and (total > 0)) {
         LONG pos = 0;
         for (i=0; i < total; i++) {
            if (pos != 0) buffer[pos++] = ',';
            pos += snprintf(buffer+pos, sizeof(buffer)-pos, "%g", dy[i]);
            if ((size_t)pos >= sizeof(buffer)-2) return ERR_BufferOverflow;
         }
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "dy", buffer);
      }

      if ((!error) and (!(error = Vector->get(FID_FontSize, &str)))) {
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "font-size", str);
         FreeResource(str);
      }

      if ((!error) and (!(error = GetFieldArray(Vector, FID_Rotate, &rotate, &total))) and (total > 0)) {
         LONG pos = 0;
         for (i=0; i < total; i++) {
            if (pos != 0) buffer[pos++] = ',';
            pos += snprintf(buffer+pos, sizeof(buffer)-pos, "%g", rotate[i]);
            if ((size_t)pos >= sizeof(buffer)-2) return ERR_BufferOverflow;
         }
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "rotate", buffer);
      }

      if ((!error) and (!(error = Vector->get(FID_TextLength, &text_length))) and (text_length))
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "textLength", text_length);

      if ((!error) and (!(error = Vector->get(FID_Face, &str))))
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "font-family", str);

      if ((!error) and (!(error = Vector->get(FID_Weight, &weight))) and (weight != 400))
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "font-weight", weight);

      if ((!error) and (!(error = Vector->get(FID_String, &str))))
         error = xmlInsertContent(XML, new_index, XMI_CHILD, str, NULL);

      // TODO: lengthAdjust, font, font-size-adjust, font-stretch, font-style, font-variant, text-anchor, kerning, letter-spacing, path-length, word-spacing, text-decoration

      if (!error) error = save_svg_scan_std(Self, XML, Vector, new_index);
   }
   else if (Vector->SubID IS ID_VECTORGROUP) {
      error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<g/>", &new_index);
      if (!error) error = save_svg_scan_std(Self, XML, Vector, new_index);
   }
   else if (Vector->SubID IS ID_VECTORCLIP) {
      STRING str;
      if ((!(error = Vector->get(FID_ID, &str))) and (str)) { // The id is an essential requirement
         error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<clipPath/>", &new_index);

         LONG units;
         if (!Vector->get(FID_Units, &units)) {
            switch(units) {
               default:
               case VUNIT_USERSPACE:    break; // Default
               case VUNIT_BOUNDING_BOX: xmlSetAttrib(XML, new_index, XMS_NEW, "clipPathUnits", "objectBoundingBox"); break;
            }
         }

         if (!error) error = save_svg_scan_std(Self, XML, Vector, new_index);
      }
   }
   else if (Vector->SubID IS ID_VECTORWAVE) {
      DOUBLE dbl;
      LONG dim;

      error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<parasol:wave/>", &new_index);

      if (!error) error = Vector->get(FID_Dimensions, &dim);

      if ((!error) and (!Vector->get(FID_X, &dbl)))
         error = set_dimension(XML, new_index, "x", dbl, dim & DMF_RELATIVE_X);

      if ((!error) and (!Vector->get(FID_Y, &dbl)))
         error = set_dimension(XML, new_index, "y", dbl, dim & DMF_RELATIVE_Y);

      if ((!error) and (!Vector->get(FID_Width, &dbl)))
         error = set_dimension(XML, new_index, "width", dbl, dim & DMF_RELATIVE_WIDTH);

      if ((!error) and (!Vector->get(FID_Height, &dbl)))
         error = set_dimension(XML, new_index, "height", dbl, dim & DMF_RELATIVE_HEIGHT);

      if ((!error) and (!Vector->get(FID_Amplitude, &dbl)))
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "amplitude", dbl);

      if ((!error) and (!Vector->get(FID_Frequency, &dbl)))
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "frequency", dbl);

      if ((!error) and (!Vector->get(FID_Decay, &dbl)))
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "decay", dbl);

      if ((!error) and (!Vector->get(FID_Degree, &dbl)))
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "degree", dbl);

      LONG close;
      if ((!error) and (!Vector->get(FID_Close, &close)))
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "close", close);

      if ((!error) and (!Vector->get(FID_Thickness, &dbl)))
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "thickness", dbl);

      if (!error) error = save_svg_scan_std(Self, XML, Vector, new_index);
   }
   else if (Vector->SubID IS ID_VECTORSPIRAL) {
      DOUBLE dbl;
      LONG dim, length;

      error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<parasol:spiral/>", &new_index);
      if (error) { return error; }

      if ((error = Vector->get(FID_Dimensions, &dim))) return error;

      if ((!error) and (!Vector->get(FID_CenterX, &dbl)))
         error = set_dimension(XML, new_index, "cx", dbl, dim & DMF_RELATIVE_CENTER_X);

      if ((!error) and (!Vector->get(FID_CenterY, &dbl)))
         error = set_dimension(XML, new_index, "cy", dbl, dim & DMF_RELATIVE_CENTER_Y);

      if ((!error) and (!Vector->get(FID_Width, &dbl)))
         error = set_dimension(XML, new_index, "width", dbl, dim & DMF_RELATIVE_WIDTH);

      if ((!error) and (!Vector->get(FID_Height, &dbl)))
         error = set_dimension(XML, new_index, "height", dbl, dim & DMF_RELATIVE_HEIGHT);

      if ((!error) and (!Vector->get(FID_Offset, &dbl)))
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "offset", dbl);

      if ((!error) and (!Vector->get(FID_PathLength, &length)) and (length != 0))
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "pathLength", length);

      if ((!error) and (!Vector->get(FID_Radius, &dbl)))
         error = set_dimension(XML, new_index, "r", dbl, dim & DMF_RELATIVE_RADIUS);

      if ((!error) and (!Vector->get(FID_Scale, &dbl)))
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "scale", dbl);

      if ((!error) and (!Vector->get(FID_Step, &dbl)))
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "step", dbl);

      if (!error) error = save_svg_scan_std(Self, XML, Vector, new_index);
   }
   else if (Vector->SubID IS ID_VECTORSHAPE) {
      DOUBLE dbl;
      LONG num, dim;

      error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<parasol:shape/>", &new_index);

      if ((error = Vector->get(FID_Dimensions, &dim))) return error;

      if ((!error) and (!Vector->get(FID_CenterX, &dbl)))
         error = set_dimension(XML, new_index, "cx", dbl, dim & DMF_RELATIVE_CENTER_X);

      if ((!error) and (!Vector->get(FID_CenterY, &dbl)))
         error = set_dimension(XML, new_index, "cy", dbl, dim & DMF_RELATIVE_CENTER_Y);

      if ((!error) and (!Vector->get(FID_Radius, &dbl)))
         error = set_dimension(XML, new_index, "r", dbl, dim & DMF_RELATIVE_RADIUS);

      if ((!error) and (!Vector->get(FID_A, &dbl))) error = xmlSetAttrib(XML, new_index, XMS_NEW, "a", dbl);
      if ((!error) and (!Vector->get(FID_B, &dbl))) error = xmlSetAttrib(XML, new_index, XMS_NEW, "b", dbl);
      if ((!error) and (!Vector->get(FID_M, &dbl))) error = xmlSetAttrib(XML, new_index, XMS_NEW, "m", dbl);
      if ((!error) and (!Vector->get(FID_N1, &dbl))) error = xmlSetAttrib(XML, new_index, XMS_NEW, "n1", dbl);
      if ((!error) and (!Vector->get(FID_N2, &dbl))) error = xmlSetAttrib(XML, new_index, XMS_NEW, "n2", dbl);
      if ((!error) and (!Vector->get(FID_N3, &dbl))) error = xmlSetAttrib(XML, new_index, XMS_NEW, "n3", dbl);
      if ((!error) and (!Vector->get(FID_Phi, &dbl))) error = xmlSetAttrib(XML, new_index, XMS_NEW, "phi", dbl);

      if ((!error) and (!Vector->get(FID_Phi, &num))) error = xmlSetAttrib(XML, new_index, XMS_NEW, "phi", num);
      if ((!error) and (!Vector->get(FID_Vertices, &num))) error = xmlSetAttrib(XML, new_index, XMS_NEW, "vertices", num);
      if ((!error) and (!Vector->get(FID_Mod, &num))) error = xmlSetAttrib(XML, new_index, XMS_NEW, "mod", num);
      if ((!error) and (!Vector->get(FID_Spiral, &num))) error = xmlSetAttrib(XML, new_index, XMS_NEW, "spiral", num);
      if ((!error) and (!Vector->get(FID_Repeat, &num))) error = xmlSetAttrib(XML, new_index, XMS_NEW, "repeat", num);
      if ((!error) and (!Vector->get(FID_Close, &num))) error = xmlSetAttrib(XML, new_index, XMS_NEW, "close", num);

      if (!error) error = save_svg_scan_std(Self, XML, Vector, new_index);
   }
   else if (Vector->SubID IS ID_VECTORVIEWPORT) {
      DOUBLE x, y, width, height;
      LONG dim;

      error = xmlInsertXML(XML, Parent, XMI_CHILD_END, "<svg/>", &new_index);

      if ((!error) and (!(error = GetFields(Vector, FID_ViewX|TDOUBLE, &x, FID_ViewY|TDOUBLE, &y, FID_ViewWidth|TDOUBLE, &width, FID_ViewHeight|TDOUBLE, &height, TAGEND)))) {
         char buffer[80];
         snprintf(buffer, sizeof(buffer), "%g %g %g %g", x, y, width, height);
         error = xmlSetAttrib(XML, new_index, XMS_NEW, "viewBox", buffer);
      }

      if ((!error) and (!(error = Vector->get(FID_Dimensions, &dim)))) {
         if ((!error) and (dim & (DMF_RELATIVE_X|DMF_FIXED_X)) and (!Vector->get(FID_X, &x)))
            error = set_dimension(XML, new_index, "x", x, dim & DMF_RELATIVE_X);

         if ((!error) and (dim & (DMF_RELATIVE_Y|DMF_FIXED_Y)) and (!Vector->get(FID_Y, &y)))
            error = set_dimension(XML, new_index, "y", y, dim & DMF_RELATIVE_Y);

         if ((!error) and (dim & (DMF_RELATIVE_WIDTH|DMF_FIXED_WIDTH)) and (!Vector->get(FID_Width, &width)))
            error = set_dimension(XML, new_index, "width", width, dim & DMF_RELATIVE_WIDTH);

         if ((!error) and (dim & (DMF_RELATIVE_HEIGHT|DMF_FIXED_HEIGHT)) and (!Vector->get(FID_Height, &height)))
            error = set_dimension(XML, new_index, "height", height, dim & DMF_RELATIVE_HEIGHT);
      }
   }
   else {
      log.msg("Unrecognised class \"%s\"", Vector->Class->ClassName);
      return ERR_Okay; // Skip objects in the scene graph that we don't recognise
   }

   if (!error) {
      for (auto scan=Vector->Child; scan; scan=scan->Next) {
         save_svg_scan(Self, XML, scan, new_index);
      }
   }

   return error;
}
