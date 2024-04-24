
//********************************************************************************************************************

void anim_value::perform(extSVG &SVG)
{
   pf::Log log;

   if ((end_time) and (!freeze)) return;
      
   pf::ScopedObjectLock<objVector> vector(target_vector, 1000);
   if (vector.granted()) {
      if (vector->Class->ClassID IS ID_VECTORGROUP) {
         // Groups are a special case because they act as a placeholder and aren't guaranteed to propagate all 
         // attributes to their children.

         // Note that group attributes do not override values that are defined by the client.

         for (auto &child : tag->Children) {            
            if (!child.isTag()) continue;
            // Any tag producing a vector object can theoretically be subject to animation.
            if (auto si = child.attrib("_id")) {
               // We can't override attributes that were defined by the client.
               if (child.attrib(target_attrib)) continue;

               pf::ScopedObjectLock<objVector> cv(std::stoi(*si), 1000);
               if (cv.granted()) set_value(**cv);
            }
         }
      }
      else set_value(**vector);
   }
}

//********************************************************************************************************************

void anim_value::set_value(objVector &Vector)
{
   // Determine the type of the attribute that we're targeting, then interpolate the value and set it.

   switch(StrHash(target_attrib)) {
      case SVF_FONT_SIZE: {
         auto val = get_numeric_value(Vector, FID_FontSize);
         Vector.set(FID_FontSize, val);
         break;
      }

      case SVF_FILL: {
         auto val = get_colour_value(Vector, FID_FillColour);
         Vector.setArray(FID_FillColour, (float *)&val, 4);
         break;
      }

      case SVF_FILL_OPACITY: {
         auto val = get_numeric_value(Vector, FID_FillOpacity);
         Vector.set(FID_FillOpacity, val);
         break;
      }

      case SVF_STROKE: {
         auto val = get_colour_value(Vector, FID_StrokeColour);
         Vector.setArray(FID_StrokeColour, (float *)&val, 4);
         break;
      }

      case SVF_STROKE_WIDTH:
         Vector.set(FID_StrokeWidth, get_numeric_value(Vector, FID_StrokeWidth));
         break;

      case SVF_OPACITY:
         Vector.set(FID_Opacity, get_numeric_value(Vector, FID_Opacity));
         break;

      case SVF_DISPLAY: {
         auto val = get_string();
         if (StrMatch("none", val) IS ERR::Okay)         Vector.set(FID_Visibility, LONG(VIS::HIDDEN));
         else if (StrMatch("inline", val) IS ERR::Okay)  Vector.set(FID_Visibility, LONG(VIS::VISIBLE));
         else if (StrMatch("inherit", val) IS ERR::Okay) Vector.set(FID_Visibility, LONG(VIS::INHERIT));
         break;
      }

      case SVF_VISIBILITY: {
         auto val = get_string();
         Vector.set(FID_Visibility, val);
         break;
      }

      case SVF_R:
         Vector.set(FID_Radius, get_dimension(Vector, FID_Radius));
         break;

      case SVF_RX:
         Vector.set(FID_RadiusX, get_dimension(Vector, FID_RadiusX));
         break;

      case SVF_RY:
         Vector.set(FID_RadiusY, get_dimension(Vector, FID_RadiusY));
         break;

      case SVF_CX:
         Vector.set(FID_CX, get_dimension(Vector, FID_CX));
         break;

      case SVF_CY:
         Vector.set(FID_CY, get_dimension(Vector, FID_CY));
         break;
                    
      case SVF_X1:
         Vector.set(FID_X1, get_dimension(Vector, FID_X1));
         break;

      case SVF_Y1:
         Vector.set(FID_Y1, get_dimension(Vector, FID_Y1));
         break;

      case SVF_X2:
         Vector.set(FID_X2, get_dimension(Vector, FID_X2));
         break;

      case SVF_Y2:
         Vector.set(FID_Y2, get_dimension(Vector, FID_Y2));
         break;

      case SVF_X: {
         if (Vector.Class->ClassID IS ID_VECTORGROUP) {
            // Special case: SVG groups don't have an (x,y) position, but can declare one in the form of a
            // transform.  Refer to xtag_use() for a working example as to why.

            VectorMatrix *m;
            for (m=Vector.Matrices; (m) and (m->Tag != MTAG_SVG_TRANSFORM); m=m->Next);

            if (!m) {
               vecNewMatrix(&Vector, &m, false);
               m->Tag = MTAG_SVG_TRANSFORM;
            }

            if (m) {
               m->TranslateX = get_dimension(Vector, FID_X);
               vecFlushMatrix(m);
            }
         }
         else Vector.set(FID_X, get_dimension(Vector, FID_X));
         break;
      }

      case SVF_Y: {
         if (Vector.Class->ClassID IS ID_VECTORGROUP) {
            VectorMatrix *m;
            for (m=Vector.Matrices; (m) and (m->Tag != MTAG_SVG_TRANSFORM); m=m->Next);

            if (!m) {
               vecNewMatrix(&Vector, &m, false);
               m->Tag = MTAG_SVG_TRANSFORM;
            }

            if (m) {
               m->TranslateY = get_dimension(Vector, FID_Y);
               vecFlushMatrix(m);
            }
         }
         else Vector.set(FID_Y, get_dimension(Vector, FID_Y));
         break;
      }

      case SVF_WIDTH:
         Vector.set(FID_Width, get_dimension(Vector, FID_Width));
         break;

      case SVF_HEIGHT:
         Vector.set(FID_Height, get_dimension(Vector, FID_Height));
         break;
   }
}