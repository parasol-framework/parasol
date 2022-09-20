// Defines the way in which results will be merged.

class MergeSource {
   public:
      LONG SourceType;
      VectorEffect *Effect;
      MergeSource(LONG pSourceType, VectorEffect *pEffect) : SourceType(pSourceType), Effect(pEffect) { };
      MergeSource(LONG pSourceType) : SourceType(pSourceType), Effect(NULL) { };
};

class MergeEffect : public VectorEffect {

   std::vector<MergeSource> List;

   void xml(std::stringstream &Stream) {
      Stream << "feMerge";
   }

public:
   MergeEffect(objVectorFilter *Filter, XMLTag *Tag) : VectorEffect() {
      parasol::Log log(__FUNCTION__);

      SourceType = VSF_IGNORE;
      EffectName = "feMerge";

      for (auto child=Tag->Child; child; child=child->Next) {
         if (!StrMatch("feMergeNode", child->Attrib->Name)) {
            for (LONG a=1; a < child->TotalAttrib; a++) {
               if (!StrMatch("in", child->Attrib[a].Name)) {
                  switch (StrHash(child->Attrib[a].Value, FALSE)) {
                     case SVF_SOURCEGRAPHIC:   List.emplace_back(VSF_GRAPHIC); break;
                     case SVF_SOURCEALPHA:     List.emplace_back(VSF_ALPHA); break;
                     case SVF_BACKGROUNDIMAGE: List.emplace_back(VSF_BKGD); break;
                     case SVF_BACKGROUNDALPHA: List.emplace_back(VSF_BKGD_ALPHA); break;
                     case SVF_FILLPAINT:       List.emplace_back(VSF_FILL); break;
                     case SVF_STROKEPAINT:     List.emplace_back(VSF_STROKE); break;
                     default:  {
                        auto e = find_effect(Filter, child->Attrib[a].Value);
                        if (e) List.emplace_back(VSF_REFERENCE, e);
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

   void apply(objVectorFilter *Filter, filter_state &State) {
      objBitmap *bmp;
      LONG dx, dy;
      LONG copy_flags = 0;
      for (auto source : List) {
         if (source.Effect) {
            if (!(bmp = source.Effect->OutBitmap)) continue;
            dx = source.Effect->DestX;
            dy = source.Effect->DestY;
         }
         else {
            if (!(bmp = get_source_graphic(Filter))) continue;
            dx = 0;
            dy = 0;
         }

         save_bitmap(bmp, "merge_" + std::to_string(bmp->Head.UID));

         gfxCopyArea(bmp, OutBitmap, copy_flags, 0, 0, bmp->Width, bmp->Height, dx, dy);

         copy_flags = BAF_BLEND|BAF_COPY; // Any subsequent copies are to be blended
      }
   }

   virtual ~MergeEffect() {
   }
};
