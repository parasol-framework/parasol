// Create a new offset effect.  Typical usage involves specifying the input and a result that a subsequent filter can
// use for applying an effect.

class OffsetEffect : public VectorEffect {
   LONG XOffset, YOffset;

   void xml(std::stringstream &Stream) {
      Stream << "feOffset";
   }

public:
   OffsetEffect(rkVectorFilter *Filter, XMLTag *Tag) : VectorEffect() {
      EffectName = "feOffset";

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
      get_source_bitmap(Filter, &inBmp, SourceType, InputID, false);
      gfxCopyArea(inBmp, OutBitmap, 0, 0, 0, inBmp->Width, inBmp->Height, XOffset, YOffset);
   }

   virtual ~OffsetEffect() { }
};
