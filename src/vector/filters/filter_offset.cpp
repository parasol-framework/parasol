
//****************************************************************************
// Create a new offset effect.  Typical usage involves specifying the input and a result that a subsequent filter can
// use for applying an effect.

static ERROR create_offset(objVectorFilter *Self, XMLTag *Tag)
{
   VectorEffect *filter;

   if (!(filter = add_effect(Self, FE_OFFSET))) return ERR_AllocMemory;

   for (LONG a=1; a < Tag->TotalAttrib; a++) {
      CSTRING val = Tag->Attrib[a].Value;
      if (!val) continue;
      ULONG hash = StrHash(Tag->Attrib[a].Name, FALSE);
      switch(hash) {
         case SVF_DX: filter->XOffset = StrToInt(val); break;
         case SVF_DY: filter->YOffset = StrToInt(val); break;
         default: fe_default(Self, filter, hash, val); break;
      }
   }
   return ERR_Okay;
}
