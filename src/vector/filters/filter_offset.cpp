
class OffsetEffect : public VectorEffect {
   DOUBLE XOffset, YOffset;

   void xml(std::stringstream &Stream) {
      Stream << "feOffset";
   }

public:
   OffsetEffect(rkVectorFilter *Filter, XMLTag *Tag) : VectorEffect() {
      EffectName = "feOffset";
      XOffset = 0;
      YOffset = 0;

      for (LONG a=1; a < Tag->TotalAttrib; a++) {
         CSTRING val = Tag->Attrib[a].Value;
         if (!val) continue;
         ULONG hash = StrHash(Tag->Attrib[a].Name, FALSE);
         switch(hash) {
            case SVF_DX: XOffset = StrToInt(val); break;
            case SVF_DY: YOffset = StrToInt(val); break;
            default: fe_default(Filter, this, hash, val); break;
         }
      }
   }

   void apply(objVectorFilter *Filter, filter_state &State) {
      objBitmap *inBmp;
      LONG dx = F2T(XOffset * Filter->ClientVector->Transform.sx);
      LONG dy = F2T(YOffset * Filter->ClientVector->Transform.sy);
      get_source_bitmap(Filter, &inBmp, SourceType, InputID, false);
      gfxCopyArea(inBmp, OutBitmap, 0, 0, 0, inBmp->Width, inBmp->Height, dx, dy);
   }

   virtual ~OffsetEffect() { }
};
