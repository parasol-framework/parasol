// Create a new offset effect.  Typical usage involves specifying the input and a result that a subsequent filter can
// use for applying an effect.

class OffsetEffect : public VectorEffect {
public:
   OffsetEffect(struct rkVectorFilter *Filter, XMLTag *Tag) : VectorEffect() {
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
      // This one-off optimisation is used to inherit the offset coordinates and source type from feOffset effects.
      Effect.Source   = Source;
      Effect.XOffset += XOffset;
      Effect.YOffset += YOffset;
      Effect.InputID  = 0;
   }

   void apply(objVectorFilter *Filter) {
   }

   virtual ~OffsetEffect() { }
};
