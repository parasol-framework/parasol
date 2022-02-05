
//****************************************************************************
// Create a new image effect.

static ERROR create_image(objVectorFilter *Self, XMLTag *Tag)
{
   parasol::Log log(__FUNCTION__);

   VectorEffect effect(FE_IMAGE);

   // SVG defaults
   effect.Image.Dimensions = 0;
   effect.Image.AspectRatio = ARF_X_MID|ARF_Y_MID|ARF_MEET;
   effect.Image.ResampleMethod = VSM_BILINEAR;
   effect.Image.Picture = NULL;

   UBYTE image_required = FALSE;
   CSTRING path = NULL;

   for (LONG a=1; a < Tag->TotalAttrib; a++) {
      CSTRING val = Tag->Attrib[a].Value;
      if (!val) continue;

      UBYTE percent;
      ULONG hash = StrHash(Tag->Attrib[a].Name, FALSE);
      switch(hash) {
         case SVF_X:
            effect.Image.X = read_unit(val, &percent);
            if (percent) effect.Image.Dimensions = (effect.Image.Dimensions & (~DMF_FIXED_X)) | DMF_RELATIVE_X;
            else effect.Image.Dimensions = (effect.Image.Dimensions & (~DMF_RELATIVE_X)) | DMF_FIXED_X;
            break;
         case SVF_Y:
            effect.Image.Y = read_unit(val, &percent);
            if (percent) effect.Image.Dimensions = (effect.Image.Dimensions & (~DMF_FIXED_Y)) | DMF_RELATIVE_Y;
            else effect.Image.Dimensions = (effect.Image.Dimensions & (~DMF_RELATIVE_Y)) | DMF_FIXED_Y;
            break;
         case SVF_WIDTH:
            effect.Image.Width = read_unit(val, &percent);
            if (percent) effect.Image.Dimensions = (effect.Image.Dimensions & (~DMF_FIXED_WIDTH)) | DMF_RELATIVE_WIDTH;
            else effect.Image.Dimensions = (effect.Image.Dimensions & (~DMF_RELATIVE_WIDTH)) | DMF_FIXED_WIDTH;
            break;
         case SVF_HEIGHT:
            effect.Image.Height = read_unit(val, &percent);
            if (percent) effect.Image.Dimensions = (effect.Image.Dimensions & (~DMF_FIXED_HEIGHT)) | DMF_RELATIVE_HEIGHT;
            else effect.Image.Dimensions = (effect.Image.Dimensions & (~DMF_RELATIVE_HEIGHT)) | DMF_FIXED_HEIGHT;
            break;

         case SVF_IMAGE_RENDERING: {
            if (!StrMatch("optimizeSpeed", val)) effect.Image.ResampleMethod = VSM_BILINEAR;
            else if (!StrMatch("optimizeQuality", val)) effect.Image.ResampleMethod = VSM_LANCZOS3;
            else if (!StrMatch("auto", val));
            else if (!StrMatch("inherit", val));
            else log.warning("Unrecognised image-rendering option '%s'", val);
            break;
         }

         case SVF_PRESERVEASPECTRATIO: {
            LONG flags = 0;
            while ((*val) and (*val <= 0x20)) val++;
            if (!StrMatch("none", val)) flags = ARF_NONE;
            else {
               if (!StrCompare("xMin", val, 4, 0)) { flags |= ARF_X_MIN; val += 4; }
               else if (!StrCompare("xMid", val, 4, 0)) { flags |= ARF_X_MID; val += 4; }
               else if (!StrCompare("xMax", val, 4, 0)) { flags |= ARF_X_MAX; val += 4; }

               if (!StrCompare("yMin", val, 4, 0)) { flags |= ARF_Y_MIN; val += 4; }
               else if (!StrCompare("yMid", val, 4, 0)) { flags |= ARF_Y_MID; val += 4; }
               else if (!StrCompare("yMax", val, 4, 0)) { flags |= ARF_Y_MAX; val += 4; }

               while ((*val) and (*val <= 0x20)) val++;

               if (!StrCompare("meet", val, 4, 0)) { flags |= ARF_MEET; }
               else if (!StrCompare("slice", val, 5, 0)) { flags |= ARF_SLICE; }
            }
            effect.Image.AspectRatio = flags;
            break;
         }

         case SVF_XLINK_HREF:
            path = val;
            break;

         case SVF_EXTERNALRESOURCESREQUIRED: // If true and the image cannot be loaded, return a fatal error code.
            if (!StrMatch("true", val)) image_required = TRUE;
            break;

         default: fe_default(Self, &effect, hash, val); break;
      }
   }

   // The coordinate system is determined according to SVG rules.  The key thing here is that boundingbox and userspace
   // systems are chosen depending on whether or not the user specified an x, y, width and/or height.

   if (Self->PrimitiveUnits != VUNIT_UNDEFINED) effect.Image.Units = Self->PrimitiveUnits;
   else {
      if (effect.Image.Dimensions) effect.Image.Units = VUNIT_USERSPACE;
      else effect.Image.Units = VUNIT_BOUNDING_BOX;
   }

   if (path) {
      // Check for security risks in the path.

      if ((path[0] IS '/') or ((path[0] IS '.') and (path[1] IS '.') and (path[2] IS '/'))) {
         return log.warning(ERR_InvalidValue);
      }

      for (UWORD i=0; path[i]; i++) {
         if (path[i] IS '/') {
            while (path[i+1] IS '.') i++;
            if (path[i+1] IS '/') {
               return log.warning(ERR_InvalidValue);
            }
         }
         else if (path[i] IS ':') {
            return log.warning(ERR_InvalidValue);
         }
      }

      ERROR error;
      if (Self->Path) {
         char comp_path[400];
         LONG i = StrCopy(Self->Path, comp_path, sizeof(comp_path));
         StrCopy(path, comp_path + i, sizeof(comp_path)-i);
         error = CreateObject(ID_PICTURE, NF_INTEGRAL, &effect.Image.Picture,
            FID_Path|TSTR,          comp_path,
            FID_BitsPerPixel|TLONG, 32,
            FID_Flags|TLONG,        PCF_FORCE_ALPHA_32,
            TAGEND);
      }
      else error = CreateObject(ID_PICTURE, NF_INTEGRAL, &effect.Image.Picture,
            FID_Path|TSTR,          path,
            FID_BitsPerPixel|TLONG, 32,
            FID_Flags|TLONG,        PCF_FORCE_ALPHA_32,
            TAGEND);

      if ((error) and (image_required)) return ERR_CreateObject;

      if ((Self->ColourSpace IS CS_LINEAR_RGB) and (!error)) rgb2linear(*effect.Image.Picture->Bitmap);

      Self->Effects->push_back(std::move(effect));
      return ERR_Okay;
   }
   else { // If no image path is referenced, the instruction to load an image can be ignored safely.
      return ERR_Okay;
   }
}

//****************************************************************************

static void apply_image(objVectorFilter *Self, VectorEffect *Effect)
{
   auto bmp = Effect->Bitmap;
   if (bmp->BytesPerPixel != 4) return;
   if (!Effect->Image.Picture) return;

   auto pic = Effect->Image.Picture->Bitmap;

   DOUBLE xScale, yScale, x, y;

   if (Effect->Image.Units IS VUNIT_BOUNDING_BOX) {
      LONG parent_x, parent_y, parent_width, parent_height;
      if (Effect->Image.Dimensions & DMF_RELATIVE_X) parent_x = Self->ViewX + F2I(Effect->Image.X * (DOUBLE)Self->BoundWidth);
      else if (Effect->Image.Dimensions & DMF_FIXED_X) parent_x = Self->ViewX + Effect->Image.X;
      else parent_x = Self->BoundX;

      if (Effect->Image.Dimensions & DMF_RELATIVE_Y) parent_y = Self->ViewY + F2I(Effect->Image.Y * (DOUBLE)Self->BoundHeight);
      else if (Effect->Image.Dimensions & DMF_FIXED_Y) parent_y = Self->ViewY + Effect->Image.Y;
      else parent_y = Self->BoundY;

      if (Effect->Image.Dimensions & DMF_RELATIVE_WIDTH) parent_width = (DOUBLE)Self->ViewWidth * Effect->Image.Width;
      else if (Effect->Image.Dimensions & DMF_FIXED_WIDTH) parent_width = Self->ViewWidth;
      else parent_width = Self->BoundWidth;

      if (Effect->Image.Dimensions & DMF_RELATIVE_HEIGHT) parent_height = (DOUBLE)Self->ViewHeight * Effect->Image.Height;
      else if (Effect->Image.Dimensions & DMF_FIXED_HEIGHT) parent_height = Self->ViewHeight;
      else parent_height = Self->BoundHeight;

      calc_alignment("align_image", Effect->Image.AspectRatio, parent_width, parent_height, pic->Width, pic->Height,
         &x, &y, &xScale, &yScale);

      x += parent_x;
      y += parent_y;
   }
   else {
      // UserSpace (relative to parent).  In this mode, all image coordinates will be computed relative to the parent viewport.
      // Alignment is then calculated as normal relative to the bounding box.  The computed image coordinates are used
      // as a shift for the final (x,y) values.

      LONG parent_x, parent_y, parent_width, parent_height;

      if (Effect->Image.Dimensions & DMF_RELATIVE_X) parent_x = Self->ViewX + F2I(Effect->Image.X * (DOUBLE)Self->Viewport->vpFixedWidth);
      else if (Effect->Image.Dimensions & DMF_FIXED_X) parent_x = Self->ViewX + Effect->Image.X;
      else parent_x = Self->ViewX;

      if (Effect->Image.Dimensions & DMF_RELATIVE_Y) parent_y = Self->ViewY + F2I(Effect->Image.Y * (DOUBLE)Self->Viewport->vpFixedHeight);
      else if (Effect->Image.Dimensions & DMF_FIXED_Y) parent_y = Self->ViewY + Effect->Image.Y;
      else parent_y = Self->ViewY;

      if (Effect->Image.Dimensions & DMF_RELATIVE_WIDTH) parent_width = (DOUBLE)Self->Viewport->vpFixedWidth * Effect->Image.Width;
      else if (Effect->Image.Dimensions & DMF_FIXED_WIDTH) parent_width = Effect->Image.Width;
      else parent_width = Self->BoundWidth;

      if (Effect->Image.Dimensions & DMF_RELATIVE_HEIGHT) parent_height = (DOUBLE)Self->Viewport->vpFixedHeight * Effect->Image.Height;
      else if (Effect->Image.Dimensions & DMF_FIXED_HEIGHT) parent_height = Effect->Image.Height;
      else parent_height = Self->BoundHeight;

      calc_alignment("align_image", Effect->Image.AspectRatio, parent_width, parent_height, pic->Width, pic->Height,
         &x, &y, &xScale, &yScale);

      x += parent_x;
      y += parent_y;
   }

   gfxDrawRectangle(bmp, 0, 0, bmp->Width, bmp->Height, 0x00000000, BAF_FILL);

   // Configure destination
   agg::renderer_base<agg::pixfmt_rkl> renderBase;
   agg::pixfmt_rkl pixDest(*Effect->Bitmap);
   renderBase.attach(pixDest);
   renderBase.clip_box(Effect->Bitmap->Clip.Left, Effect->Bitmap->Clip.Top, Effect->Bitmap->Clip.Right-1, Effect->Bitmap->Clip.Bottom-1);

   // Configure source

   agg::pixfmt_rkl pixSource(*pic);

   agg::trans_affine transform;
   transform.scale(xScale, yScale);
   transform.translate(x, y);
   transform.invert();
   agg::span_interpolator_linear<> interpolator(transform);

   agg::image_filter_lut filter;
   set_filter(filter, Effect->Image.ResampleMethod);

   agg::span_pattern_rkl<agg::pixfmt_rkl> source(pixSource, 0, 0);
   agg::span_image_filter_rgba<agg::span_pattern_rkl<agg::pixfmt_rkl>, agg::span_interpolator_linear<>> spangen(source, interpolator, filter);
   agg::rasterizer_scanline_aa<> raster;
   setRasterClip(raster, Effect->Bitmap->Clip.Left, Effect->Bitmap->Clip.Top,
      Effect->Bitmap->Clip.Right - Effect->Bitmap->Clip.Left,
      Effect->Bitmap->Clip.Bottom - Effect->Bitmap->Clip.Top);
   drawBitmapRender(renderBase, raster, spangen, 1.0);
}


