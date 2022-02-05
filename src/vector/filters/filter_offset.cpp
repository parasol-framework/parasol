
//****************************************************************************
// Create a new offset effect.  Typical usage involves specifying the input and a result that a subsequent filter can
// use for applying an effect.

static ERROR create_offset(objVectorFilter *Self, XMLTag *Tag)
{
   VectorEffect effect(FE_OFFSET);

   for (LONG a=1; a < Tag->TotalAttrib; a++) {
      CSTRING val = Tag->Attrib[a].Value;
      if (!val) continue;
      ULONG hash = StrHash(Tag->Attrib[a].Name, FALSE);
      switch(hash) {
         case SVF_DX: effect.XOffset = StrToInt(val); break;
         case SVF_DY: effect.YOffset = StrToInt(val); break;
         default: fe_default(Self, &effect, hash, val); break;
      }
   }

   Self->Effects->push_back(std::move(effect));
   return ERR_Okay;
}
