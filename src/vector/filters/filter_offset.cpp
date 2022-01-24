
//****************************************************************************
// Create a new offset effect.  Typical usage involves specifying the input and a result that a subsequent filter can
// use for applying an effect.

static ERROR create_offset(objVectorFilter *Self, XMLTag *Tag)
{
   auto filter_it = Self->Effects->emplace(Self->Effects->end(), FE_OFFSET);
   auto &filter = *filter_it;

   for (LONG a=1; a < Tag->TotalAttrib; a++) {
      CSTRING val = Tag->Attrib[a].Value;
      if (!val) continue;
      ULONG hash = StrHash(Tag->Attrib[a].Name, FALSE);
      switch(hash) {
         case SVF_DX: filter.XOffset = StrToInt(val); break;
         case SVF_DY: filter.YOffset = StrToInt(val); break;
         default: fe_default(Self, &filter, hash, val); break;
      }
   }
   return ERR_Okay;
}
