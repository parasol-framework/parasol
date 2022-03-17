
class FloodEffect : public VectorEffect {
   struct RGB8 Colour;
   DOUBLE X, Y, Width, Height;
   DOUBLE Opacity;
   LONG Dimensions;

public:
   FloodEffect(struct rkVectorFilter *Filter, XMLTag *Tag) : VectorEffect() {
      parasol::Log log(__FUNCTION__);

      // Dimensions are relative to the VectorFilter's Bound* dimensions.

      Dimensions = DMF_RELATIVE_X|DMF_RELATIVE_Y|DMF_RELATIVE_WIDTH|DMF_RELATIVE_HEIGHT;
      X = 0;
      Y = 0;
      Width   = 1.0;
      Height  = 1.0;
      Opacity = 1.0;

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
               struct DRGB frgb;
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

   // This is the stack flood algorithm originally implemented in AGG.

   void apply(objVectorFilter *Filter) {
      if (Bitmap->BytesPerPixel != 4) return;

      LONG x, y, width, height;

      if (Dimensions & DMF_RELATIVE_X) x = F2I(DOUBLE(Filter->BoundWidth) * X);
      else x = F2I(X);

      if (Dimensions & DMF_RELATIVE_Y) y = F2I(DOUBLE(Filter->BoundHeight) * Y);
      else y = F2I(Y);

      if (Dimensions & DMF_RELATIVE_WIDTH) width = F2I(DOUBLE(Filter->BoundWidth) * Width);
      else width = F2I(Width);

      if (Dimensions & DMF_RELATIVE_HEIGHT) height = F2I(DOUBLE(Filter->BoundHeight) * Height);
      else height = F2I(Height);

      ULONG colour = PackPixelWBA(Bitmap, Colour.Red, Colour.Green, Colour.Blue, Colour.Alpha);
      gfxDrawRectangle(Bitmap, Bitmap->Clip.Left+x, Bitmap->Clip.Top+y, width, height, colour, BAF_FILL);
   }

   virtual ~FloodEffect() { }
};
