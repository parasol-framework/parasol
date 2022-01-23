
//****************************************************************************
// Defines the way in which results will be merged.

static ERROR create_merge(objVectorFilter *Self, XMLTag *Tag)
{
   parasol::Log log(__FUNCTION__);
   VectorEffect * list[50];
   VectorEffect * filter;

   if (!(filter = add_effect(Self, FE_OFFSET))) return ERR_AllocMemory;

   filter->Source = VSF_IGNORE;

   // Count the total number of merge nodes.

   LONG count = 0;
   for (auto child=Tag->Child; child; child=child->Next) {
      if (!StrMatch("feMergeNode", child->Attrib->Name)) {
         for (LONG a=1; a < child->TotalAttrib; a++) {
            if (!StrMatch("in", child->Attrib[a].Name)) {
               VectorEffect *ie = NULL;
               switch (StrHash(child->Attrib[a].Value, FALSE)) {
                  case SVF_SOURCEGRAPHIC:   ie = &Self->SrcGraphic; break;
                  case SVF_SOURCEALPHA:     ie = &Self->SrcGraphic; break;
                  //case SVF_BACKGROUNDIMAGE: ie = &Self->BkgdGraphic; break;
                  //case SVF_BACKGROUNDALPHA: ie = &Self->BkgdGraphic; break;
                  //case SVF_FILLPAINT:       ie = VSF_FILL; break;
                  //case SVF_STROKEPAINT:     ie = VSF_STROKE; break;
                  default:  {
                     ie = find_effect(Self, child->Attrib[a].Value);
                     if (!ie) log.warning("Unable to parse 'in' value '%s'", child->Attrib[a].Value);
                     break;
                  }
               }

               if (ie) list[count++] = ie;
            }
            else log.warning("Invalid feMergeNode attribute '%s'", child->Attrib[a].Name);
         }
      }
      else log.warning("Invalid merge node '%s'", child->Attrib->Name);
   }

   log.traceBranch("Detected %d merge nodes.", count);

   if (count > 0) {
      if (count > ARRAYSIZE(list)) count = ARRAYSIZE(list);
      if (!AllocMemory(sizeof(Self->Merge[0]) * (count + 1), MEM_DATA|MEM_NO_CLEAR, &Self->Merge, NULL)) {
         CopyMemory(list, Self->Merge, sizeof(Self->Merge[0]) * count);
         Self->Merge[count] = NULL;
         return ERR_Okay;
      }
      else return ERR_AllocMemory;
   }
   else return AllocMemory(sizeof(Self->Merge[0]), MEM_DATA, &Self->Merge, NULL); // Allocate an empty merge list (draws nothing)
}
