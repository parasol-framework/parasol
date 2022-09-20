// Create a new offset effect.  Typical usage involves specifying the input and a result that a subsequent filter can
// use for applying an effect.

class OffsetEffect : public VectorEffect {

   void xml(std::stringstream &Stream) {
      Stream << "feOffset";
   }

public:
   OffsetEffect(rkVectorFilter *Filter, XMLTag *Tag) : VectorEffect() {
      EffectName = "feOffset";
      Blank = true;

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

   void applyInput(VectorEffect &Effect) {
      // A child effect has a dependency on this offset.  Apply the offset values permanently, pass on the
      // source type, then disable the dependency.

      Effect.XOffset += XOffset;
      Effect.YOffset += YOffset;

      if (Effect.InputID IS ID) {
         Effect.SourceType = SourceType;
         Effect.InputID    = 0;
      }

      if (Effect.MixID IS ID) {
         Effect.MixType = SourceType;
         Effect.MixID   = 0;
      }
   }

   void apply(objVectorFilter *Filter, filter_state &State) {
   }

   virtual ~OffsetEffect() { }
};
