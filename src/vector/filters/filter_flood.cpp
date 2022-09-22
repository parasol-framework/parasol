
class FloodEffect : public VectorEffect {
   RGB8 Colour;
   DOUBLE X, Y, Width, Height;
   DOUBLE Opacity;
   LONG Dimensions;

   void xml(std::stringstream &Stream) { // TODO: Support exporting attributes
      Stream << "feFlood";
   }

public:
   FloodEffect(rkVectorFilter *Filter, XMLTag *Tag) : VectorEffect() {
      parasol::Log log(__FUNCTION__);

      // Dimensions are relative to the VectorFilter's Bound* dimensions.

      X          = 0;
      Y          = 0;
      Width      = 1.0;
      Height     = 1.0;
      Opacity    = 1.0;
      EffectName = "feFlood";
      Dimensions = DMF_RELATIVE_X|DMF_RELATIVE_Y|DMF_RELATIVE_WIDTH|DMF_RELATIVE_HEIGHT;

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

            case SVF_FLOOD_COLOR:
            case SVF_FLOOD_COLOUR: {
               DRGB frgb;
               vecReadPainter((OBJECTPTR)NULL, val, &frgb, NULL, NULL, NULL);
               Colour.Red   = F2I(frgb.Red * 255.0);
               Colour.Green = F2I(frgb.Green * 255.0);
               Colour.Blue  = F2I(frgb.Blue * 255.0);
               Colour.Alpha = F2I(frgb.Alpha * 255.0);
               break;
            }

            case SVF_FLOOD_OPACITY:
               read_numseq(val, &Opacity, TAGEND);
               break;

            default:
               fe_default(Filter, this, hash, val);
               break;
         }
      }

      Colour.Alpha = F2I((DOUBLE)Colour.Alpha * Opacity);

      if (!Colour.Alpha) {
         log.warning("A valid flood-colour is required.");
         Error = ERR_Failed;
      }
   }

   // Filter flood is implemented in identical fashion to feImage, only difference being that the
   // image is a block of single colour.

   void apply(objVectorFilter *Filter, filter_state &State) {
      if (OutBitmap->BytesPerPixel != 4) return;

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

         if (Filter->Dimensions & DMF_FIXED_WIDTH) target_width = Filter->Width * b_width;
         else if (Filter->Dimensions & DMF_RELATIVE_WIDTH) target_width = Filter->Width * b_width;
         else target_width = b_width;

         if (Filter->Dimensions & DMF_FIXED_HEIGHT) target_height = Filter->Height * b_height;
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

      // Draw to destination.  No anti-aliasing is applied.


      agg::rasterizer_scanline_aa<> raster;
      agg::renderer_base<agg::pixfmt_psl> renderBase;
      agg::scanline_p8 scanline;
      agg::pixfmt_psl format(*OutBitmap);
      renderBase.attach(format);

      agg::path_storage path;
      path.move_to(target_x, target_y);
      path.line_to(target_x + target_width, target_y);
      path.line_to(target_x + target_width, target_y + target_height);
      path.line_to(target_x, target_y + target_height);
      path.close_polygon();

      agg::renderer_scanline_bin_solid< agg::renderer_base<agg::pixfmt_psl> > solid_render(renderBase);
      agg::conv_transform<agg::path_storage, agg::trans_affine> final_path(path, Filter->ClientVector->Transform);
      raster.add_path(final_path);
      renderBase.clip_box(OutBitmap->Clip.Left, OutBitmap->Clip.Top, OutBitmap->Clip.Right - 1, OutBitmap->Clip.Bottom - 1);
      solid_render.color(agg::rgba8(Colour.Red, Colour.Green, Colour.Blue, Colour.Alpha));
      agg::render_scanlines(raster, scanline, solid_render);

   }

   virtual ~FloodEffect() { }
};
