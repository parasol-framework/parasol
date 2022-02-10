// Defines the way in which results will be merged.

class MergeSource {
   public:
      objBitmap *Bitmap;
      VectorEffect *Effect;
      MergeSource(objBitmap *pBitmap, VectorEffect *pEffect) : Bitmap(pBitmap), Effect(pEffect) { };
};

class MergeEffect : public VectorEffect {

   std::vector<MergeSource> List;
   objBitmap *MergeBitmap;

public:
   MergeEffect(objVectorFilter *Filter, XMLTag *Tag) : VectorEffect() {
      parasol::Log log(__FUNCTION__);

      Blank = true;
      Source = VSF_IGNORE;
      MergeBitmap = NULL;

      for (auto child=Tag->Child; child; child=child->Next) {
         if (!StrMatch("feMergeNode", child->Attrib->Name)) {
            for (LONG a=1; a < child->TotalAttrib; a++) {
               if (!StrMatch("in", child->Attrib[a].Name)) {
                  switch (StrHash(child->Attrib[a].Value, FALSE)) {
                     case SVF_SOURCEGRAPHIC:
                        if (Filter->SrcBitmap) List.emplace_back(Filter->SrcBitmap, (VectorEffect *)NULL);
                        break;
                     case SVF_SOURCEALPHA:
                        if (Filter->SrcBitmap) List.emplace_back(Filter->SrcBitmap, (VectorEffect *)NULL);
                        break;
                     //case SVF_BACKGROUNDIMAGE: List.emplace_back(Filter->BkgdGraphic); break;
                     //case SVF_BACKGROUNDALPHA: List.emplace_back(Filter->BkgdGraphic); break;
                     //case SVF_FILLPAINT:       List.emplace_back(VSF_FILL); break;
                     //case SVF_STROKEPAINT:     List.emplace_back(VSF_STROKE); break;
                     default:  {
                        auto e = find_effect(Filter, child->Attrib[a].Value);
                        if (e) List.emplace_back((objBitmap *)NULL, e);
                        else log.warning("Unable to parse 'in' value '%s'", child->Attrib[a].Value);
                        break;
                     }
                  }
               }
               else log.warning("Invalid feMergeNode attribute '%s'", child->Attrib[a].Name);
            }
         }
         else log.warning("Invalid merge element '%s'", child->Attrib->Name);
      }
   }

   // Merging overrides the default drawing process of the VectorFilter Draw action.

   void apply(objVectorFilter *Filter) {
      // 1. Merge everything to the scratch bitmap allocated by the filter.
      // 2. Do the linear2RGB conversion on the result.
      // 3. Copy the result to the target.

      Filter->Rendered = true;

      if (!MergeBitmap) {
         if (CreateObject(ID_BITMAP, NF_INTEGRAL, &MergeBitmap,
               FID_Name|TSTR,          "MergeBitmap",
               FID_Width|TLONG,        Filter->BoundWidth,
               FID_Height|TLONG,       Filter->BoundHeight,
               FID_BitsPerPixel|TLONG, 32,
               FID_Flags|TLONG,        BMF_ALPHA_CHANNEL,
               TAGEND)) return;
      }
      else if ((Filter->BoundWidth != MergeBitmap->Width) or (Filter->BoundHeight != MergeBitmap->Height)) {
         acResize(MergeBitmap, Filter->BoundWidth, Filter->BoundHeight, 32);
      }

      gfxDrawRectangle(MergeBitmap, 0, 0, MergeBitmap->Width, MergeBitmap->Height, 0x00000000, BAF_FILL);

      UWORD bmpCount = 0;
      for (auto source : List) {
         objBitmap *bmp;
         LONG dx, dy;
         if (source.Bitmap) {
            bmp = source.Bitmap;
            dx = 0;
            dy = 0;
         }
         else {
            bmp = source.Effect->Bitmap;
            dx = source.Effect->DestX;
            dy = source.Effect->DestY;
         }

         if (++bmpCount IS 1) {
            gfxCopyArea(bmp, MergeBitmap, 0, 0, 0, bmp->Width, bmp->Height, dx - Filter->BoundX, dy - Filter->BoundY);
         }
         else gfxCopyArea(bmp, MergeBitmap, BAF_BLEND|BAF_COPY, 0, 0, bmp->Width, bmp->Height, dx - Filter->BoundX, dy - Filter->BoundY);
      }

      // Final copy to the display.

      if (Filter->ColourSpace IS CS_LINEAR_RGB) linear2RGB(*MergeBitmap);

      if (Filter->Opacity < 1.0) MergeBitmap->Opacity = 255.0 * Filter->Opacity;
      gfxCopyArea(MergeBitmap, Filter->BkgdBitmap, BAF_BLEND|BAF_COPY, 0, 0, Filter->BoundWidth, Filter->BoundHeight, Filter->BoundX, Filter->BoundY);
      MergeBitmap->Opacity = 255;
   }

   virtual ~MergeEffect() {
      if (MergeBitmap) acFree(MergeBitmap);
   }
};
