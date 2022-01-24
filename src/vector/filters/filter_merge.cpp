
//****************************************************************************
// Defines the way in which results will be merged.

static ERROR create_merge(objVectorFilter *Self, XMLTag *Tag)
{
   parasol::Log log(__FUNCTION__);

   auto filter_it = Self->Effects->emplace(Self->Effects->end(), FE_OFFSET);
   auto &filter = *filter_it;

   if (!Self->Merge) Self->Merge = new (std::nothrow) std::vector<VectorEffect *>;
   else Self->Merge->clear();

   filter.Source = VSF_IGNORE;

   // Count the total number of merge nodes.

   for (auto child=Tag->Child; child; child=child->Next) {
      if (!StrMatch("feMergeNode", child->Attrib->Name)) {
         for (LONG a=1; a < child->TotalAttrib; a++) {
            if (!StrMatch("in", child->Attrib[a].Name)) {
               switch (StrHash(child->Attrib[a].Value, FALSE)) {
                  case SVF_SOURCEGRAPHIC:   Self->Merge->push_back(&Self->SrcGraphic); break;
                  case SVF_SOURCEALPHA:     Self->Merge->push_back(&Self->SrcGraphic); break;
                  //case SVF_BACKGROUNDIMAGE: Self->Merge->push_back(Self->BkgdGraphic); break;
                  //case SVF_BACKGROUNDALPHA: Self->Merge->push_back(Self->BkgdGraphic); break;
                  //case SVF_FILLPAINT:       Self->Merge->push_back(VSF_FILL); break;
                  //case SVF_STROKEPAINT:     Self->Merge->push_back(VSF_STROKE); break;
                  default:  {
                     auto ie = find_effect(Self, child->Attrib[a].Value);
                     if (ie) Self->Merge->push_back(ie);
                     else log.warning("Unable to parse 'in' value '%s'", child->Attrib[a].Value);
                     break;
                  }
               }
            }
            else log.warning("Invalid feMergeNode attribute '%s'", child->Attrib[a].Name);
         }
      }
      else log.warning("Invalid merge node '%s'", child->Attrib->Name);
   }

   log.traceBranch("Detected %d merge nodes.", (LONG)Self->Merge->size());

   return ERR_Okay;
}
