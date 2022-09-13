
class ImageEffect : public VectorEffect {
   DOUBLE X, Y, Width, Height;
   struct rkPicture *Picture;
   LONG Dimensions;
   LONG AspectRatio;
   UBYTE ResampleMethod;

   void xml(std::stringstream &Stream) { // TODO: Support exporting attributes
      Stream << "feImage";
   }

public:
   ImageEffect(struct rkVectorFilter *Filter, XMLTag *Tag) : VectorEffect() {
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

   void apply(objVectorFilter *Filter) {
      if (OutBitmap->BytesPerPixel != 4) return;
      if (!Picture) return;

      DOUBLE img_x, img_y, img_width, img_height;

      if (Filter->PrimitiveUnits IS VUNIT_BOUNDING_BOX) {
         // In this mode, dimensions are forced to (0,0,100%,100%) of the vector's bounding box.
         img_x = Filter->VectorX;
         img_y = Filter->VectorY;
         img_width  = Filter->VectorWidth;
         img_height = Filter->VectorHeight;
      }
      else {
         if (Dimensions & DMF_RELATIVE_X) img_x = (X * (DOUBLE)Filter->BoundWidth);
         else if (Dimensions & DMF_FIXED_X) img_x = X;
         else {
            // If X is undefined then SVG rules that it defaults to the filter's bounds.
            img_x = Filter->BoundX;
         }

         if (Dimensions & DMF_RELATIVE_Y) img_y = (Y * (DOUBLE)Filter->BoundHeight);
         else if (Dimensions & DMF_FIXED_Y) img_y = Y;
         else {
            // If Y is undefined then SVG rules that it defaults to the filter's bounds.
            img_y = Filter->BoundY;
         }

         if (Dimensions & DMF_RELATIVE_WIDTH) img_width = (DOUBLE)Filter->BoundWidth * Width;
         else if (Dimensions & DMF_FIXED_WIDTH) img_width = Width;
         else img_width = Filter->BoundWidth; // 100%

         if (Dimensions & DMF_RELATIVE_HEIGHT) img_height = (DOUBLE)Filter->BoundHeight * Height;
         else if (Dimensions & DMF_FIXED_HEIGHT) img_height = Height;
         else img_height = Filter->BoundHeight; // 100%
      }

      auto pic = Picture->Bitmap;
      DOUBLE xScale, yScale, align_x, align_y;
      calc_aspectratio("align_image", AspectRatio, img_width, img_height, pic->Width, pic->Height,
         &align_x, &align_y, &xScale, &yScale);

      // Configure destination
      agg::renderer_base<agg::pixfmt_rkl> renderBase;
      agg::pixfmt_rkl pixDest(*OutBitmap);
      renderBase.attach(pixDest);
      renderBase.clip_box(OutBitmap->Clip.Left, OutBitmap->Clip.Top, OutBitmap->Clip.Right-1, OutBitmap->Clip.Bottom-1);

      //parasol::Log log(__FUNCTION__);
      //log.trace("Image targeting %.2fx%.2f %.2fx%.2f, Alignment %.2fx%.2f", img_x, img_y, img_width, img_height, align_x, align_y);

      // Configure source

      agg::pixfmt_rkl pixSource(*pic);

      agg::trans_affine transform;
      transform.scale(xScale, yScale);
      transform.translate(img_x + align_x, img_y + align_y);
      transform.invert();
      agg::span_interpolator_linear<> interpolator(transform);

      agg::image_filter_lut filter;
      set_filter(filter, ResampleMethod);

      agg::span_pattern_rkl<agg::pixfmt_rkl> source(pixSource, 0, 0);
      agg::span_image_filter_rgba<agg::span_pattern_rkl<agg::pixfmt_rkl>, agg::span_interpolator_linear<>> spangen(source, interpolator, filter);
      agg::rasterizer_scanline_aa<> raster;
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

