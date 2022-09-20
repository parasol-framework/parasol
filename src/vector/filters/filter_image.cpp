
class ImageEffect : public VectorEffect {
   DOUBLE X, Y, Width, Height; // Position & size of the image within the filter's rendering area.
   objPicture *Picture;
   LONG Dimensions;
   LONG AspectRatio;
   UBYTE ResampleMethod;

   void xml(std::stringstream &Stream) { // TODO: Support exporting attributes
      Stream << "feImage";
   }

public:
   ImageEffect(objVectorFilter *Filter, XMLTag *Tag) : VectorEffect() {
      parasol::Log log(__FUNCTION__);

      Dimensions     = 0;
      AspectRatio    = ARF_X_MID|ARF_Y_MID|ARF_MEET;
      ResampleMethod = VSM_BILINEAR;
      Picture        = NULL;
      EffectName     = "feImage";

      bool image_required = false;
      CSTRING path = NULL;

      for (LONG a=1; a < Tag->TotalAttrib; a++) {
         CSTRING val = Tag->Attrib[a].Value;
         if (!val) continue;

         UBYTE percent;
         ULONG hash = StrHash(Tag->Attrib[a].Name, FALSE);
         switch(hash) {
            case SVF_X:
               X = read_unit(val, &percent);
               if (percent) Dimensions = (Dimensions & (~DMF_FIXED_X)) | DMF_RELATIVE_X;
               else Dimensions = (Dimensions & (~DMF_RELATIVE_X)) | DMF_FIXED_X;
               break;

            case SVF_Y:
               Y = read_unit(val, &percent);
               if (percent) Dimensions = (Dimensions & (~DMF_FIXED_Y)) | DMF_RELATIVE_Y;
               else Dimensions = (Dimensions & (~DMF_RELATIVE_Y)) | DMF_FIXED_Y;
               break;

            case SVF_WIDTH:
               Width = read_unit(val, &percent);
               if (percent) Dimensions = (Dimensions & (~DMF_FIXED_WIDTH)) | DMF_RELATIVE_WIDTH;
               else Dimensions = (Dimensions & (~DMF_RELATIVE_WIDTH)) | DMF_FIXED_WIDTH;
               break;

            case SVF_HEIGHT:
               Height = read_unit(val, &percent);
               if (percent) Dimensions = (Dimensions & (~DMF_FIXED_HEIGHT)) | DMF_RELATIVE_HEIGHT;
               else Dimensions = (Dimensions & (~DMF_RELATIVE_HEIGHT)) | DMF_FIXED_HEIGHT;
               break;

            case SVF_IMAGE_RENDERING: {
               if (!StrMatch("optimizeSpeed", val)) ResampleMethod = VSM_BILINEAR;
               else if (!StrMatch("optimizeQuality", val)) ResampleMethod = VSM_LANCZOS3;
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
               AspectRatio = flags;
               break;
            }

            case SVF_XLINK_HREF:
               path = val;
               break;

            case SVF_EXTERNALRESOURCESREQUIRED: // If true and the image cannot be loaded, return a fatal error code.
               if (!StrMatch("true", val)) image_required = true;
               break;

            default: fe_default(Filter, this, hash, val); break;
         }
      }

      if (path) {
         // Check for security risks in the path.

         if ((path[0] IS '/') or ((path[0] IS '.') and (path[1] IS '.') and (path[2] IS '/'))) {
            Error = log.warning(ERR_InvalidValue);
         }
         else {
            for (UWORD i=0; path[i]; i++) {
               if (path[i] IS '/') {
                  while (path[i+1] IS '.') i++;
                  if (path[i+1] IS '/') {
                     Error = log.warning(ERR_InvalidValue);
                     break;
                  }
               }
               else if (path[i] IS ':') {
                  Error = log.warning(ERR_InvalidValue);
                  break;
               }
            }
         }

         if (Filter->Path) { // Use the filter's path (folder) if it has one.
            char comp_path[400];
            LONG i = StrCopy(Filter->Path, comp_path, sizeof(comp_path));
            StrCopy(path, comp_path + i, sizeof(comp_path)-i);
            Error = CreateObject(ID_PICTURE, NF_INTEGRAL, &Picture,
               FID_Path|TSTR,          comp_path,
               FID_BitsPerPixel|TLONG, 32,
               FID_Flags|TLONG,        PCF_FORCE_ALPHA_32,
               TAGEND);
         }
         else Error = CreateObject(ID_PICTURE, NF_INTEGRAL, &Picture,
               FID_Path|TSTR,          path,
               FID_BitsPerPixel|TLONG, 32,
               FID_Flags|TLONG,        PCF_FORCE_ALPHA_32,
               TAGEND);

         if ((Error) and (image_required)) Error = ERR_CreateObject;
         else Error = ERR_Okay;

         if ((Filter->ColourSpace IS CS_LINEAR_RGB) and (!Error) and (Picture)) {
            rgb2linear(*Picture->Bitmap);
         }
      }

      // NB: If no image path is referenced, the instruction to load an image can be ignored safely.
   }

   void apply(objVectorFilter *Filter, filter_state &State) {
      parasol::Log log(__FUNCTION__);

      if (OutBitmap->BytesPerPixel != 4) return;
      if (!Picture) return;

      std::array<DOUBLE, 4> bounds = { Filter->ClientViewport->vpFixedWidth, Filter->ClientViewport->vpFixedHeight, 0, 0 };
      calc_full_boundary((objVector *)Filter->ClientVector, bounds, false, false);
      const DOUBLE b_x = trunc(bounds[0]);
      const DOUBLE b_y = trunc(bounds[1]);
      const DOUBLE b_width  = bounds[2] - bounds[0];
      const DOUBLE b_height = bounds[3] - bounds[1];

      DOUBLE target_x, target_y, target_width, target_height;
      if (Filter->Units IS VUNIT_BOUNDING_BOX) {
         if (Filter->Dimensions & DMF_FIXED_X) target_x = b_x;
         else if (Filter->Dimensions & DMF_RELATIVE_X) target_x = trunc(b_x + (Filter->X * b_width));
         else target_x = b_x;

         if (Filter->Dimensions & DMF_FIXED_Y) target_y = b_y;
         else if (Filter->Dimensions & DMF_RELATIVE_Y) target_y = trunc(b_y + (Filter->Y * b_height));
         else target_y = b_y;

         if (Filter->Dimensions & DMF_FIXED_WIDTH) target_width = b_width;
         else if (Filter->Dimensions & DMF_RELATIVE_WIDTH) target_width = Filter->Width * b_width;
         else target_width = b_width;

         if (Filter->Dimensions & DMF_FIXED_HEIGHT) target_height = Filter->Height;
         else if (Filter->Dimensions & DMF_RELATIVE_HEIGHT) target_height = Filter->Height * b_height;
         else target_height = b_height;
      }
      else { // USERSPACE
         if (Filter->Dimensions & DMF_FIXED_X) target_x = trunc(Filter->X);
         else if (Filter->Dimensions & DMF_RELATIVE_X) target_x = trunc(Filter->X * Filter->ClientViewport->vpFixedWidth);
         else target_x = b_x;

         if (Filter->Dimensions & DMF_FIXED_Y) target_y = trunc(Filter->Y);
         else if (Filter->Dimensions & DMF_RELATIVE_Y) target_y = trunc(Filter->Y * Filter->ClientViewport->vpFixedHeight);
         else target_y = b_y;

         if (Filter->Dimensions & DMF_FIXED_WIDTH) target_width = Filter->Width;
         else if (Filter->Dimensions & DMF_RELATIVE_WIDTH) target_width = Filter->Width * Filter->ClientViewport->vpFixedWidth;
         else target_width = Filter->ClientViewport->vpFixedWidth;

         if (Filter->Dimensions & DMF_FIXED_HEIGHT) target_height = Filter->Height;
         else if (Filter->Dimensions & DMF_RELATIVE_HEIGHT) target_height = Filter->Height * Filter->ClientViewport->vpFixedHeight;
         else target_height = Filter->ClientViewport->vpFixedHeight;
      }

      // The image's x,y,width,height default to (0,0,100%,100%) of the target region.

      DOUBLE img_x = target_x;
      DOUBLE img_y = target_y;
      DOUBLE img_width = target_width;
      DOUBLE img_height = target_height;

      if (Filter->PrimitiveUnits IS VUNIT_BOUNDING_BOX) {
         // In this mode image dimensions typically remain at the default, i.e. (0,0,100%,100%) of the target.
         // If the user does set the XYWH of the image then 'fixed' coordinates act as multipliers, as if they were relative.

         // W3 spec on whether to use the bounds or the filter target region:
         // "Any length values within the filter definitions represent fractions or percentages of the bounding box
         // on the referencing element."

         const DOUBLE container_width = b_width;
         const DOUBLE container_height = b_height;
         if (Dimensions & (DMF_FIXED_X|DMF_RELATIVE_X)) img_x = trunc(target_x + (X * container_width));
         if (Dimensions & (DMF_FIXED_Y|DMF_RELATIVE_Y)) img_y = trunc(target_y + (Y * container_height));
         if (Dimensions & (DMF_FIXED_WIDTH|DMF_RELATIVE_WIDTH)) img_width = Width * container_width;
         if (Dimensions & (DMF_FIXED_HEIGHT|DMF_RELATIVE_HEIGHT)) img_height = Height * container_height;
      }
      else {
         if (Dimensions & DMF_RELATIVE_X) img_x = target_x + (X * target_width);
         else if (Dimensions & DMF_FIXED_X) img_x = X;

         if (Dimensions & DMF_RELATIVE_Y) img_y = target_y + (Y * target_height);
         else if (Dimensions & DMF_FIXED_Y) img_y = Y;

         if (Dimensions & DMF_RELATIVE_WIDTH) img_width = target_width * Width;
         else if (Dimensions & DMF_FIXED_WIDTH) img_width = Width;

         if (Dimensions & DMF_RELATIVE_HEIGHT) img_height = target_height * Height;
         else if (Dimensions & DMF_FIXED_HEIGHT) img_height = Height;
      }

      DOUBLE xScale = 1, yScale = 1, align_x = 0, align_y = 0;
      calc_aspectratio("align_image", AspectRatio, img_width, img_height, Picture->Bitmap->Width, Picture->Bitmap->Height, &align_x, &align_y, &xScale, &yScale);

      img_x += align_x;
      img_y += align_y;

      // Draw to destination

      agg::rasterizer_scanline_aa<> raster;
      agg::renderer_base<agg::pixfmt_rkl> renderBase;
      agg::pixfmt_rkl pixDest(*OutBitmap);
      agg::pixfmt_rkl pixSource(*Picture->Bitmap);

      agg::path_storage path;
      path.move_to(target_x, target_y);
      path.line_to(target_x + target_width, target_y);
      path.line_to(target_x + target_width, target_y + target_height);
      path.line_to(target_x, target_y + target_height);
      path.close_polygon();

      renderBase.attach(pixDest);
      renderBase.clip_box(OutBitmap->Clip.Left, OutBitmap->Clip.Top, OutBitmap->Clip.Right-1, OutBitmap->Clip.Bottom-1);

      agg::conv_transform<agg::path_storage, agg::trans_affine> final_path(path, Filter->ClientVector->Transform);
      raster.add_path(final_path);

      agg::trans_affine img_transform;
      img_transform.scale(xScale, yScale);
      img_transform.translate(img_x, img_y);
      img_transform *= Filter->ClientVector->Transform;
      img_transform.invert();
      agg::span_interpolator_linear<> interpolator(img_transform);

      agg::image_filter_lut filter;
      set_filter(filter, ResampleMethod);

      agg::span_pattern_rkl<agg::pixfmt_rkl> source(pixSource, 0, 0);
      agg::span_image_filter_rgba<agg::span_pattern_rkl<agg::pixfmt_rkl>, agg::span_interpolator_linear<>> spangen(source, interpolator, filter);

      setRasterClip(raster, OutBitmap->Clip.Left, OutBitmap->Clip.Top,
         OutBitmap->Clip.Right - OutBitmap->Clip.Left,
         OutBitmap->Clip.Bottom - OutBitmap->Clip.Top);

      drawBitmapRender(renderBase, raster, spangen, 1.0);
   }

   //****************************************************************************

   virtual ~ImageEffect() {
      if (Picture) acFree(Picture);
   }
};

