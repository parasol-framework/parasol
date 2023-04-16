
static void set_dimension(XMLTag *Tag, const std::string Attrib, DOUBLE Value, LONG Relative)
{
   if (Relative) xmlNewAttrib(*Tag, Attrib, std::to_string(Value * 100.0) + "%");
   else xmlNewAttrib(*Tag, Attrib, std::to_string(Value));
}

//*********************************************************************************************************************

static ERROR save_vectorpath(extSVG *Self, objXML *XML, objVector *Vector, LONG Parent)
{
   STRING path;
   ERROR error;

   if (!(error = Vector->get(FID_Sequence, &path))) {
      LONG new_index;
      error = xmlInsertXML(XML, Parent, XMI::CHILD_END, "<path/>", &new_index);
      if (!error) {
         XMLTag *tag;
         error = xmlGetTag(XML, new_index, &tag);
         if (!error) xmlNewAttrib(tag, "d", path);
      }
      FreeResource(path);

      if (!error) error = save_svg_scan_std(Self, XML, Vector, new_index);
   }

   return error;
}

//*********************************************************************************************************************

static ERROR save_svg_defs(extSVG *Self, objXML *XML, objVectorScene *Scene, LONG Parent)
{
   pf::Log log(__FUNCTION__);
   std::unordered_map<std::string, OBJECTPTR> *defs;

   if (!Scene->getPtr(FID_Defs, &defs)) {
      ERROR error;
      LONG def_index = 0;
      for (auto & [ key, def ] : *defs) {
         if (!def_index) {
            if ((error = xmlInsertXML(XML, Parent, XMI::CHILD_END, "<defs/>", &def_index))) return error;
         }

         log.msg("Processing definition %s (%x)", def->Class->ClassName, def->Class->ClassID);

         if (def->Class->ClassID IS ID_VECTORGRADIENT) {
            auto gradient = (objVectorGradient *)def;
            std::string gradient_type;
            switch(gradient->Type) {
               case VGT_RADIAL:  gradient_type = "<radialGradient/>"; break;
               case VGT_CONIC:   gradient_type = "<conicGradient/>"; break;
               case VGT_DIAMOND: gradient_type = "<diamondGradient/>"; break;
               case VGT_CONTOUR: gradient_type = "<contourGradient/>"; break;
               case VGT_LINEAR:
               default:          gradient_type = "<linearGradient/>"; break;
            }
            XMLTag *tag;
            error = xmlInsertXML(XML, def_index, XMI::CHILD_END, gradient_type, &tag);

            if (!error) xmlNewAttrib(tag, "id", key);

            VUNIT units;
            if ((!error) and (!gradient->get(FID_Units, (LONG *)&units))) {
               switch(units) {
                  case VUNIT::USERSPACE:    xmlNewAttrib(tag, "gradientUnits", "userSpaceOnUse"); break;
                  case VUNIT::BOUNDING_BOX: xmlNewAttrib(tag, "gradientUnits", "objectBoundingBox"); break;
                  default: break;
               }
            }

            VSPREAD spread;
            if ((!error) and (!gradient->get(FID_SpreadMethod, (LONG *)&spread))) {
               switch(spread) {
                  default:
                  case VSPREAD::PAD:     break; // Pad is the default SVG setting
                  case VSPREAD::REFLECT: xmlNewAttrib(tag, "spreadMethod", "reflect"); break;
                  case VSPREAD::REPEAT:  xmlNewAttrib(tag, "spreadMethod", "repeat"); break;
               }
            }

            if ((gradient->Type IS VGT_LINEAR) or (gradient->Type IS VGT_CONTOUR)) {
               if (!error) {
                  xmlNewAttrib(tag, "x1", std::to_string(gradient->X1));
                  xmlNewAttrib(tag, "y1", std::to_string(gradient->Y1));
                  xmlNewAttrib(tag, "x2", std::to_string(gradient->X2));
                  xmlNewAttrib(tag, "y2", std::to_string(gradient->Y2));
               }
            }
            else if ((gradient->Type IS VGT_RADIAL) or (gradient->Type IS VGT_DIAMOND) or (gradient->Type IS VGT_CONIC)) {
               if ((!error) and (gradient->Flags & (VGF_FIXED_CX|VGF_RELATIVE_CX)))
                  set_dimension(tag, "cx", gradient->CenterX, gradient->Flags & VGF_RELATIVE_CX);

               if ((!error) and (gradient->Flags & (VGF_FIXED_CY|VGF_RELATIVE_CY)))
                  set_dimension(tag, "cy", gradient->CenterY, gradient->Flags & VGF_RELATIVE_CY);

               if ((!error) and (gradient->Flags & (VGF_FIXED_FX|VGF_RELATIVE_FX)))
                  set_dimension(tag, "fx", gradient->FX, gradient->Flags & VGF_RELATIVE_FX);

               if ((!error) and (gradient->Flags & (VGF_FIXED_FY|VGF_RELATIVE_FY)))
                  set_dimension(tag, "fy", gradient->FY, gradient->Flags & VGF_RELATIVE_FY);

               if ((!error) and (gradient->Flags & (VGF_FIXED_RADIUS|VGF_RELATIVE_RADIUS)))
                  set_dimension(tag, "r", gradient->Radius, gradient->Flags & VGF_RELATIVE_RADIUS);
            }

            VectorMatrix *transform;
            if ((!error) and (!gradient->getPtr(FID_Transforms, &transform)) and (transform)) {
               std::stringstream buffer;
               if (!save_svg_transform(transform, buffer)) {
                  xmlNewAttrib(tag, "gradientTransform", buffer.str());
               }
            }

            if (gradient->TotalStops > 0) {
               GradientStop *stops;
               LONG total_stops, stop_index;
               if (!GetFieldArray(gradient, FID_Stops, &stops, &total_stops)) {
                  for (LONG s=0; (s < total_stops) and (!error); s++) {
                     if (!(error = xmlInsertXML(XML, def_index, XMI::CHILD_END, "<stop/>", &stop_index))) {
                        XMLTag *stop_tag;
                        error = xmlGetTag(XML, stop_index, &stop_tag);
                        if (!error) xmlNewAttrib(stop_tag, "offset", std::to_string(stops[s].Offset));

                        std::stringstream buffer;
                        buffer << "stop-color:rgb(" << stops[s].RGB.Red*255.0 << "," << stops[s].RGB.Green*255.0 << "," << stops[s].RGB.Blue*255.0 << "," << stops[s].RGB.Alpha*255.0 << ")";
                        if (!error) xmlNewAttrib(stop_tag, "style", buffer.str());
                     }
                  }
               }
            }
         }
         else if (def->Class->ClassID IS ID_VECTORIMAGE) {
            log.warning("VectorImage not supported.");
         }
         else if (def->Class->ClassID IS ID_VECTORPATH) {
            error = save_vectorpath(Self, XML, (objVector *)def, def_index);
         }
         else if (def->Class->ClassID IS ID_VECTORPATTERN) {
            log.warning("VectorPattern not supported.");
         }
         else if (def->Class->ClassID IS ID_VECTORFILTER) {
            objVectorFilter *filter = (objVectorFilter *)def;

            XMLTag *tag;
            error = xmlInsertXML(XML, def_index, XMI::CHILD_END, "<filter/>", &tag);

            if (!error) xmlNewAttrib(tag, "id", key);

            LONG dim;
            if (!error) error = filter->get(FID_Dimensions, &dim);

            if ((!error) and (dim & (DMF_RELATIVE_X|DMF_FIXED_X)))
               set_dimension(tag, "x", filter->X, dim & DMF_RELATIVE_X);

            if ((!error) and (dim & (DMF_RELATIVE_Y|DMF_FIXED_Y)))
               set_dimension(tag, "y", filter->Y, dim & DMF_RELATIVE_Y);

            if ((!error) and (dim & (DMF_RELATIVE_WIDTH|DMF_FIXED_WIDTH)))
               set_dimension(tag, "width", filter->Width, dim & DMF_RELATIVE_WIDTH);

            if ((!error) and (dim & (DMF_RELATIVE_HEIGHT|DMF_FIXED_HEIGHT)))
               set_dimension(tag, "height", filter->Height, dim & DMF_RELATIVE_HEIGHT);

            VUNIT units;
            if ((!error) and (!filter->get(FID_Units, (LONG *)&units))) {
               switch(units) {
                  default:
                  case VUNIT::BOUNDING_BOX: break; // Default
                  case VUNIT::USERSPACE:    xmlNewAttrib(tag, "filterUnits", "userSpaceOnUse"); break;
               }
            }

            if ((!error) and (!filter->get(FID_PrimitiveUnits, (LONG *)&units))) {
               switch(units) {
                  default:
                  case VUNIT::USERSPACE:    break;
                  case VUNIT::BOUNDING_BOX: xmlNewAttrib(tag, "primitiveUnits", "objectBoundingBox"); break;
               }
            }

            STRING effect_xml;
            if ((!error) and (!filter->get(FID_EffectXML, &effect_xml))) {
               error = xmlInsertXML(XML, tag->ID, XMI::CHILD, effect_xml, NULL);
               FreeResource(effect_xml);
            }
         }
         else if (def->Class->ClassID IS ID_VECTORTRANSITION) {
            log.warning("VectorTransition not supported.");
         }
         else if (def->Class->ClassID IS ID_VECTORCLIP) {
            log.warning("VectorClip not supported.");
         }
         else if (def->Class->BaseClassID IS ID_VECTOR) {
            log.warning("%s not supported.", def->Class->ClassName);
         }
         else log.warning("Unrecognised definition class %x", def->Class->ClassID);
      }

      return ERR_Okay;
   }
   else return ERR_Failed;
}

//*********************************************************************************************************************

static ERROR save_svg_transform(VectorMatrix *Transform, std::stringstream &Buffer)
{
   std::vector<VectorMatrix *> list;

   for (auto t=Transform; t; t=t->Next) list.push_back(t);

   Buffer.setf(std::ios_base::hex);
   bool need_space = false;
   std::for_each(list.rbegin(), list.rend(), [&](auto t) {
      if (need_space) Buffer << " ";
      else need_space = true;
      Buffer << "matrix(" << t->ScaleX << " " << t->ShearY << " " << t->ShearX << " " << t->ScaleY << " " << t->TranslateX << " " << t->TranslateY << ")";
   });

   return ERR_Okay;
}

//*********************************************************************************************************************

static ERROR save_svg_scan_std(extSVG *Self, objXML *XML, objVector *Vector, LONG TagID)
{
   pf::Log log(__FUNCTION__);
   char buffer[160];
   STRING str;
   FLOAT *colour;
   LONG array_size;
   ERROR error = ERR_Okay;

   XMLTag *tag;
   if ((error = xmlGetTag(XML, TagID, &tag))) return error;

   if (Vector->Opacity != 1.0) xmlNewAttrib(tag, "opacity", std::to_string(Vector->Opacity));
   if (Vector->FillOpacity != 1.0) xmlNewAttrib(tag, "fill-opacity", std::to_string(Vector->FillOpacity));
   if (Vector->StrokeOpacity != 1.0) xmlNewAttrib(tag, "stroke-opacity", std::to_string(Vector->StrokeOpacity));

   if ((!Vector->get(FID_Stroke, &str)) and (str)) {
      xmlNewAttrib(tag, "stroke", str);
   }
   else if ((!GetFieldArray(Vector, FID_StrokeColour, &colour, &array_size)) and (colour[3] != 0)) {
      snprintf(buffer, sizeof(buffer), "rgb(%g,%g,%g,%g)", colour[0], colour[1], colour[2], colour[3]);
      xmlNewAttrib(tag, "stroke-color", buffer);
   }

   LONG line_join;
   if ((!error) and (!Vector->get(FID_LineJoin, &line_join))) {
      switch (line_join) {
         default:
         case VLJ_MITER:        break; // Default
         case VLJ_MITER_REVERT: xmlNewAttrib(tag, "stroke-linejoin", "miter-revert"); break; // Parasol
         case VLJ_ROUND:        xmlNewAttrib(tag, "stroke-linejoin", "round"); break;
         case VLJ_BEVEL:        xmlNewAttrib(tag, "stroke-linejoin", "bevel"); break;
         case VLJ_MITER_ROUND:  xmlNewAttrib(tag, "stroke-linejoin", "arcs"); break; // (SVG2) Not sure if compliant
         case VLJ_INHERIT:      xmlNewAttrib(tag, "stroke-linejoin", "inherit"); break;
      } // "miter-clip" SVG2
   }

   LONG inner_join;
   if ((!error) and (!Vector->get(FID_InnerJoin, &inner_join))) { // Parasol only
      switch (inner_join) {
         default:
         case VIJ_MITER:   break; // Default
         case VIJ_BEVEL:   xmlNewAttrib(tag, "stroke-innerjoin", "bevel"); break;
         case VIJ_JAG:     xmlNewAttrib(tag, "stroke-innerjoin", "jag"); break;
         case VIJ_ROUND:   xmlNewAttrib(tag, "stroke-innerjoin", "round"); break;
         case VIJ_INHERIT: xmlNewAttrib(tag, "stroke-innerjoin", "inherit"); break;
      }
   }

   DOUBLE *dash_array;
   LONG dash_total;
   if ((!error) and (!GetFieldArray(Vector, FID_DashArray, &dash_array, &dash_total)) and (dash_array)) {
      DOUBLE dash_offset;
      if ((!Vector->get(FID_DashOffset, &dash_offset)) and (dash_offset != 0)) {
         xmlNewAttrib(tag, "stroke-dashoffset", std::to_string(Vector->DashOffset));
      }

      LONG pos = 0;
      for (LONG i=0; i < dash_total; i++) {
         if (pos != 0) buffer[pos++] = ',';
         pos += snprintf(buffer+pos, sizeof(buffer)-pos, "%g", dash_array[i]);
         if ((size_t)pos >= sizeof(buffer)-2) return ERR_BufferOverflow;
      }
      xmlNewAttrib(tag, "stroke-dasharray", buffer);
   }

   LONG linecap;
   if ((!error) and (!Vector->get(FID_LineCap, &linecap))) {
      switch (linecap) {
         default:
         case VLC_BUTT:    break; // Default
         case VLC_SQUARE:  xmlNewAttrib(tag, "stroke-linecap", "square"); break;
         case VLC_ROUND:   xmlNewAttrib(tag, "stroke-linecap", "round"); break;
         case VLC_INHERIT: xmlNewAttrib(tag, "stroke-linecap", "inherit"); break;
      }
   }

   if (Vector->Visibility IS VIS_HIDDEN)        xmlNewAttrib(tag, "visibility", "hidden");
   else if (Vector->Visibility IS VIS_COLLAPSE) xmlNewAttrib(tag, "visibility", "collapse");
   else if (Vector->Visibility IS VIS_INHERIT)  xmlNewAttrib(tag, "visibility", "inherit");

   STRING stroke_width;
   if ((!error) and (!Vector->get(FID_StrokeWidth, &stroke_width))) {
      if (!stroke_width) stroke_width = "0";
      if ((stroke_width[0] != '1') and (stroke_width[1] != 0)) {
         xmlNewAttrib(tag, "stroke-width", stroke_width);
      }
   }

   if ((!error) and (!Vector->get(FID_Fill, &str)) and (str)) {
      if (StrMatch("rgb(0,0,0)", str) != ERR_Okay) xmlNewAttrib(tag, "fill", str);
   }
   else if ((!error) and (!GetFieldArray(Vector, FID_FillColour, &colour, &array_size)) and (colour[3] != 0)) {
      snprintf(buffer, sizeof(buffer), "rgb(%g,%g,%g,%g)", colour[0], colour[1], colour[2], colour[3]);
      xmlNewAttrib(tag, "fill", buffer);
   }

   LONG fill_rule;
   if ((!error) and (!Vector->get(FID_FillRule, &fill_rule))) {
      if (fill_rule IS VFR_EVEN_ODD) xmlNewAttrib(tag, "fill-rule", "evenodd");
   }

   if ((!error) and (!(error = Vector->get(FID_ID, &str))) and (str)) xmlNewAttrib(tag, "id", str);

   if ((!error) and (!Vector->get(FID_Filter, &str)) and (str)) xmlNewAttrib(tag, "filter", str);

   VectorMatrix *transform;
   if ((!error) and (!Vector->getPtr(FID_Transforms, &transform)) and (transform)) {
      std::stringstream buffer;
      if (!(error = save_svg_transform(transform, buffer))) {
         xmlNewAttrib(tag, "transform", buffer.str());
      }
   }

   OBJECTPTR shape;
   if ((!error) and (!Vector->getPtr(FID_Morph, &shape)) and (shape)) {
      VMF morph_flags;

      XMLTag *morph_tag;
      error = xmlInsertXML(XML, TagID, XMI::CHILD_END, "<parasol:morph/>", &morph_tag);

      STRING shape_id;
      if ((!error) and (!shape->get(FID_ID, &shape_id)) and (shape_id)) {
         // NB: It is required that the shape has previously been registered as a definition, otherwise the url will refer to a dud tag.
         char shape_ref[120];
         snprintf(shape_ref, sizeof(shape_ref), "url(#%s)", shape_id);
         xmlNewAttrib(morph_tag, "xlink:href", shape_ref);
      }

      if (!error) error = Vector->get(FID_MorphFlags, (LONG *)&morph_flags);

      if ((!error) and ((morph_flags & VMF::STRETCH) != VMF::NIL)) xmlNewAttrib(morph_tag, "method", "stretch");
      if ((!error) and ((morph_flags & VMF::AUTO_SPACING) != VMF::NIL)) xmlNewAttrib(morph_tag, "spacing", "auto");

      if ((!error) and ((morph_flags & (VMF::X_MIN|VMF::X_MID|VMF::X_MAX|VMF::Y_MIN|VMF::Y_MID|VMF::Y_MAX)) != VMF::NIL)) {
         char align[40];
         WORD apos = 0;
         if ((morph_flags & VMF::X_MIN) != VMF::NIL) apos = StrCopy("xMin ", align, sizeof(align));
         else if ((morph_flags & VMF::X_MID) != VMF::NIL) apos = StrCopy("xMid ", align, sizeof(align));
         else if ((morph_flags & VMF::X_MAX) != VMF::NIL) apos = StrCopy("xMax ", align, sizeof(align));

         if ((morph_flags & VMF::Y_MIN) != VMF::NIL) StrCopy("yMin", align+apos, sizeof(align));
         else if ((morph_flags & VMF::Y_MID) != VMF::NIL) StrCopy("yMid", align+apos, sizeof(align));
         else if ((morph_flags & VMF::Y_MAX) != VMF::NIL) StrCopy("yMax", align+apos, sizeof(align));

         xmlNewAttrib(morph_tag, "align", align);
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
   pf::Log log(__FUNCTION__);

   LONG new_index = -1;

   log.branch("%s", Vector->Class->ClassName);

   ERROR error = ERR_Okay;
   if (Vector->Class->ClassID IS ID_VECTORRECTANGLE) {
      XMLTag *tag;
      DOUBLE rx, ry, x, y, width, height;
      LONG dim;

      error = xmlInsertXML(XML, Parent, XMI::CHILD_END, "<rect/>", &new_index);
      if (!error) error = xmlGetTag(XML, new_index, &tag);
      if (!error) error = Vector->get(FID_Dimensions, &dim);

      if (!error) {
         if ((!Vector->get(FID_RoundX, &rx)) and (rx != 0)) set_dimension(tag, "rx", rx, FALSE);
         if ((!Vector->get(FID_RoundY, &ry)) and (ry != 0)) set_dimension(tag, "ry", ry, FALSE);
         if ((!Vector->get(FID_X, &x))) set_dimension(tag, "x", x, dim & DMF_RELATIVE_X);
         if ((!Vector->get(FID_Y, &y))) set_dimension(tag, "y", y, dim & DMF_RELATIVE_Y);
         if ((!Vector->get(FID_Width, &width))) set_dimension(tag, "width", width, dim & DMF_RELATIVE_WIDTH);
         if ((!Vector->get(FID_Height, &height))) set_dimension(tag, "height", height, dim & DMF_RELATIVE_HEIGHT);

         save_svg_scan_std(Self, XML, Vector, new_index);
      }
   }
   else if (Vector->Class->ClassID IS ID_VECTORELLIPSE) {
      XMLTag *tag;
      DOUBLE rx, ry, cx, cy;
      LONG dim;

      error = Vector->get(FID_Dimensions, &dim);
      if (!error) error = Vector->get(FID_RadiusX, &rx);
      if (!error) error = Vector->get(FID_RadiusY, &ry);
      if (!error) error = Vector->get(FID_CenterX, &cx);
      if (!error) error = Vector->get(FID_CenterY, &cy);
      if (!error) error = xmlInsertXML(XML, Parent, XMI::CHILD_END, "<ellipse/>", &tag);

      if (!error) {
         set_dimension(tag, "rx", rx, dim & DMF_RELATIVE_RADIUS_X);
         set_dimension(tag, "ry", ry, dim & DMF_RELATIVE_RADIUS_Y);
         set_dimension(tag, "cx", cx, dim & DMF_RELATIVE_CENTER_X);
         set_dimension(tag, "cy", cy, dim & DMF_RELATIVE_CENTER_Y);
      }

      if (!error) error = save_svg_scan_std(Self, XML, Vector, new_index);
   }
   else if (Vector->Class->ClassID IS ID_VECTORPATH) {
      error = save_vectorpath(Self, XML, Vector, Parent);
   }
   else if (Vector->Class->ClassID IS ID_VECTORPOLYGON) { // Serves <polygon>, <line> and <polyline>
      XMLTag *tag;
      VectorPoint *points;
      LONG total_points, i;

      if ((!Vector->get(FID_Closed, &i)) and (i IS FALSE)) { // Line or Polyline
         if (!(error = GetFieldArray(Vector, FID_PointsArray, &points, &total_points))) {
            if (total_points IS 2) {
               error = xmlInsertXML(XML, Parent, XMI::CHILD_END, "<line/>", &tag);
               if (!error) {
                  set_dimension(tag, "x1", points[0].X, points[0].XRelative);
                  set_dimension(tag, "y1", points[0].Y, points[0].YRelative);
                  set_dimension(tag, "x2", points[1].X, points[1].XRelative);
                  set_dimension(tag, "y2", points[1].Y, points[1].YRelative);
               }
            }
            else {
               std::stringstream buffer;
               error = xmlInsertXML(XML, Parent, XMI::CHILD_END, "<polyline/>", &tag);
               if (!error) {
                  for (i=0; i < total_points; i++) {
                     buffer << points[i].X << "," << points[i].Y << " ";
                  }
                  xmlNewAttrib(tag, "points", buffer.str());
               }
            }
         }
      }
      else {
         std::stringstream buffer;
         error = xmlInsertXML(XML, Parent, XMI::CHILD_END, "<polygon/>", &tag);

         if ((!error) and (!GetFieldArray(Vector, FID_PointsArray, &points, &total_points))) {
            for (i=0; i < total_points; i++) {
               buffer << points[i].X << "," << points[i].Y << " ";
            }
            xmlNewAttrib(tag, "points", buffer.str());
         }
      }

      DOUBLE path_length;
      if ((!(error = Vector->get(FID_PathLength, &path_length))) and (path_length != 0)) {
         xmlNewAttrib(tag, "pathLength", std::to_string(path_length));
      }

      if (!error) error = save_svg_scan_std(Self, XML, Vector, tag->ID);
   }
   else if (Vector->Class->ClassID IS ID_VECTORTEXT) {
      XMLTag *tag;
      DOUBLE x, y, *dx, *dy, *rotate, text_length;
      LONG total, i, weight;
      STRING str;
      char buffer[1024];

      error = xmlInsertXML(XML, Parent, XMI::CHILD_END, "<text/>", &tag);

      if ((!error) and (!Vector->get(FID_X, &x))) set_dimension(tag, "x", x, FALSE);
      if ((!error) and (!Vector->get(FID_Y, &y))) set_dimension(tag, "y", y, FALSE);

      if ((!error) and (!(error = GetFieldArray(Vector, FID_DX, &dx, &total))) and (total > 0)) {
         LONG pos = 0;
         for (LONG i=0; i < total; i++) {
            if (pos != 0) buffer[pos++] = ',';
            pos += snprintf(buffer+pos, sizeof(buffer)-pos, "%g", dx[i]);
            if ((size_t)pos >= sizeof(buffer)-2) return ERR_BufferOverflow;
         }
         xmlNewAttrib(tag, "dx", buffer);
      }

      if ((!error) and (!(error = GetFieldArray(Vector, FID_DY, &dy, &total))) and (total > 0)) {
         LONG pos = 0;
         for (i=0; i < total; i++) {
            if (pos != 0) buffer[pos++] = ',';
            pos += snprintf(buffer+pos, sizeof(buffer)-pos, "%g", dy[i]);
            if ((size_t)pos >= sizeof(buffer)-2) return ERR_BufferOverflow;
         }
         xmlNewAttrib(tag, "dy", buffer);
      }

      if ((!error) and (!(error = Vector->get(FID_FontSize, &str)))) {
         xmlNewAttrib(tag, "font-size", str);
         FreeResource(str);
      }

      if ((!error) and (!(error = GetFieldArray(Vector, FID_Rotate, &rotate, &total))) and (total > 0)) {
         std::stringstream buffer;
         bool comma = false;
         for (i=0; i < total; i++) {
            if (comma) buffer << ',';
            else comma = true;
            buffer << rotate[i];
         }
         xmlNewAttrib(tag, "rotate", buffer.str());
      }

      if ((!error) and (!(error = Vector->get(FID_TextLength, &text_length))) and (text_length))
         xmlNewAttrib(tag, "textLength", std::to_string(text_length));

      if ((!error) and (!(error = Vector->get(FID_Face, &str))))
         xmlNewAttrib(tag, "font-family", str);

      if ((!error) and (!(error = Vector->get(FID_Weight, &weight))) and (weight != 400))
         xmlNewAttrib(tag, "font-weight", std::to_string(weight));

      if ((!error) and (!(error = Vector->get(FID_String, &str))))
         error = xmlInsertContent(XML, tag->ID, XMI::CHILD, str, NULL);

      // TODO: lengthAdjust, font, font-size-adjust, font-stretch, font-style, font-variant, text-anchor, kerning, letter-spacing, path-length, word-spacing, text-decoration

      if (!error) error = save_svg_scan_std(Self, XML, Vector, tag->ID);
   }
   else if (Vector->Class->ClassID IS ID_VECTORGROUP) {
      XMLTag *tag;
      error = xmlInsertXML(XML, Parent, XMI::CHILD_END, "<g/>", &tag);
      if (!error) error = save_svg_scan_std(Self, XML, Vector, tag->ID);
   }
   else if (Vector->Class->ClassID IS ID_VECTORCLIP) {
      XMLTag *tag;
      STRING str;
      if ((!(error = Vector->get(FID_ID, &str))) and (str)) { // The id is an essential requirement
         error = xmlInsertXML(XML, Parent, XMI::CHILD_END, "<clipPath/>", &tag);

         VUNIT units;
         if (!Vector->get(FID_Units, (LONG *)&units)) {
            switch(units) {
               default:
               case VUNIT::USERSPACE:    break; // Default
               case VUNIT::BOUNDING_BOX: xmlNewAttrib(tag, "clipPathUnits", "objectBoundingBox"); break;
            }
         }

         if (!error) error = save_svg_scan_std(Self, XML, Vector, tag->ID);
      }
   }
   else if (Vector->Class->ClassID IS ID_VECTORWAVE) {
      XMLTag *tag;
      DOUBLE dbl;
      LONG dim;

      error = xmlInsertXML(XML, Parent, XMI::CHILD_END, "<parasol:wave/>", &tag);

      if (!error) error = Vector->get(FID_Dimensions, &dim);

      if (!error) {
         if (!Vector->get(FID_X, &dbl)) set_dimension(tag, "x", dbl, dim & DMF_RELATIVE_X);
         if (!Vector->get(FID_Y, &dbl)) set_dimension(tag, "y", dbl, dim & DMF_RELATIVE_Y);
         if (!Vector->get(FID_Width, &dbl)) set_dimension(tag, "width", dbl, dim & DMF_RELATIVE_WIDTH);
         if (!Vector->get(FID_Height, &dbl)) set_dimension(tag, "height", dbl, dim & DMF_RELATIVE_HEIGHT);
         if (!Vector->get(FID_Amplitude, &dbl)) xmlNewAttrib(tag, "amplitude", std::to_string(dbl));
         if (!Vector->get(FID_Frequency, &dbl)) xmlNewAttrib(tag, "frequency", std::to_string(dbl));
         if (!Vector->get(FID_Decay, &dbl)) xmlNewAttrib(tag, "decay", std::to_string(dbl));
         if (!Vector->get(FID_Degree, &dbl)) xmlNewAttrib(tag, "degree", std::to_string(dbl));

         LONG close;
         if (!Vector->get(FID_Close, &close)) xmlNewAttrib(tag, "close", std::to_string(close));
         if (!Vector->get(FID_Thickness, &dbl)) xmlNewAttrib(tag, "thickness", std::to_string(dbl));

         if (!error) error = save_svg_scan_std(Self, XML, Vector, tag->ID);
      }
   }
   else if (Vector->Class->ClassID IS ID_VECTORSPIRAL) {
      XMLTag *tag;
      DOUBLE dbl;
      LONG dim, length;

      error = xmlInsertXML(XML, Parent, XMI::CHILD_END, "<parasol:spiral/>", &tag);
      if (error) return error;

      if ((error = Vector->get(FID_Dimensions, &dim))) return error;

      if (!error) {
         if (!Vector->get(FID_CenterX, &dbl)) set_dimension(tag, "cx", dbl, dim & DMF_RELATIVE_CENTER_X);
         if (!Vector->get(FID_CenterY, &dbl)) set_dimension(tag, "cy", dbl, dim & DMF_RELATIVE_CENTER_Y);
         if (!Vector->get(FID_Width, &dbl)) set_dimension(tag, "width", dbl, dim & DMF_RELATIVE_WIDTH);
         if (!Vector->get(FID_Height, &dbl)) set_dimension(tag, "height", dbl, dim & DMF_RELATIVE_HEIGHT);
         if (!Vector->get(FID_Offset, &dbl)) xmlNewAttrib(tag, "offset", std::to_string(dbl));
         if ((!Vector->get(FID_PathLength, &length)) and (length != 0)) xmlNewAttrib(tag, "pathLength", std::to_string(length));
         if (!Vector->get(FID_Radius, &dbl)) set_dimension(tag, "r", dbl, dim & DMF_RELATIVE_RADIUS);
         if (!Vector->get(FID_Scale, &dbl)) xmlNewAttrib(tag, "scale", std::to_string(dbl));
         if (!Vector->get(FID_Step, &dbl)) xmlNewAttrib(tag, "step", std::to_string(dbl));

         error = save_svg_scan_std(Self, XML, Vector, tag->ID);
      }
   }
   else if (Vector->Class->ClassID IS ID_VECTORSHAPE) {
      XMLTag *tag;
      DOUBLE dbl;
      LONG num, dim;

      error = xmlInsertXML(XML, Parent, XMI::CHILD_END, "<parasol:shape/>", &tag);

      if ((error = Vector->get(FID_Dimensions, &dim))) return error;

      if (!error) {
         if (!Vector->get(FID_CenterX, &dbl)) set_dimension(tag, "cx", dbl, dim & DMF_RELATIVE_CENTER_X);
         if (!Vector->get(FID_CenterY, &dbl)) set_dimension(tag, "cy", dbl, dim & DMF_RELATIVE_CENTER_Y);
         if (!Vector->get(FID_Radius, &dbl)) set_dimension(tag, "r", dbl, dim & DMF_RELATIVE_RADIUS);
         if (!Vector->get(FID_A, &dbl)) xmlNewAttrib(tag, "a", std::to_string(dbl));
         if (!Vector->get(FID_B, &dbl)) xmlNewAttrib(tag, "b", std::to_string(dbl));
         if (!Vector->get(FID_M, &dbl)) xmlNewAttrib(tag, "m", std::to_string(dbl));
         if (!Vector->get(FID_N1, &dbl)) xmlNewAttrib(tag, "n1", std::to_string(dbl));
         if (!Vector->get(FID_N2, &dbl)) xmlNewAttrib(tag, "n2", std::to_string(dbl));
         if (!Vector->get(FID_N3, &dbl)) xmlNewAttrib(tag, "n3", std::to_string(dbl));
         if (!Vector->get(FID_Phi, &dbl)) xmlNewAttrib(tag, "phi", std::to_string(dbl));
         if (!Vector->get(FID_Phi, &num)) xmlNewAttrib(tag, "phi", std::to_string(num));
         if (!Vector->get(FID_Vertices, &num)) xmlNewAttrib(tag, "vertices", std::to_string(num));
         if (!Vector->get(FID_Mod, &num)) xmlNewAttrib(tag, "mod", std::to_string(num));
         if (!Vector->get(FID_Spiral, &num)) xmlNewAttrib(tag, "spiral", std::to_string(num));
         if (!Vector->get(FID_Repeat, &num)) xmlNewAttrib(tag, "repeat", std::to_string(num));
         if (!Vector->get(FID_Close, &num)) xmlNewAttrib(tag, "close", std::to_string(num));

         error = save_svg_scan_std(Self, XML, Vector, tag->ID);
      }
   }
   else if (Vector->Class->ClassID IS ID_VECTORVIEWPORT) {
      XMLTag *tag;
      DOUBLE x, y, width, height;
      LONG dim;

      error = xmlInsertXML(XML, Parent, XMI::CHILD_END, "<svg/>", &tag);

      if (!error) error = Vector->get(FID_ViewX, &x);
      if (!error) error = Vector->get(FID_ViewY, &y);
      if (!error) error = Vector->get(FID_ViewWidth, &width);
      if (!error) error = Vector->get(FID_ViewHeight, &height);

      if (!error) {
         std::stringstream buffer;
         buffer << x << " " << y << " " << width << " " << height;
         xmlNewAttrib(tag, "viewBox", buffer.str());
      }

      if ((!error) and (!(error = Vector->get(FID_Dimensions, &dim)))) {
         if ((!error) and (dim & (DMF_RELATIVE_X|DMF_FIXED_X)) and (!Vector->get(FID_X, &x)))
            set_dimension(tag, "x", x, dim & DMF_RELATIVE_X);

         if ((!error) and (dim & (DMF_RELATIVE_Y|DMF_FIXED_Y)) and (!Vector->get(FID_Y, &y)))
            set_dimension(tag, "y", y, dim & DMF_RELATIVE_Y);

         if ((!error) and (dim & (DMF_RELATIVE_WIDTH|DMF_FIXED_WIDTH)) and (!Vector->get(FID_Width, &width)))
            set_dimension(tag, "width", width, dim & DMF_RELATIVE_WIDTH);

         if ((!error) and (dim & (DMF_RELATIVE_HEIGHT|DMF_FIXED_HEIGHT)) and (!Vector->get(FID_Height, &height)))
            set_dimension(tag, "height", height, dim & DMF_RELATIVE_HEIGHT);
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
