/*********************************************************************************************************************

-CLASS-
MergeFX: Combines multiple effects in sequence.

Use MergeFX to composite multiple input sources so that they are rendered on top of each other in a predefined
sequence.

Many effects produce a number of intermediate layers in order to create the final output image.  This filter allows
us to collapse those into a single image.  Although this could be done by using n-1 Composite-filters, it is more
convenient to have  this common operation available in this form, and offers the implementation some additional
flexibility.

-END-

The canonical implementation of feMerge is to render the entire effect into one RGBA layer, and then render the
resulting layer on the output device. In certain cases (in particular if the output device itself is a continuous
tone device), and since merging is associative, it might be a sufficient approximation to evaluate the effect one
layer at a time and render each layer individually onto the output device bottom to top.

If the topmost image input is SourceGraphic and this ‘feMerge’ is the last filter primitive in the filter, the
implementation is encouraged to render the layers up to that point, and then render the SourceGraphic directly from
its vector description on top.

*********************************************************************************************************************/

class objMergeFX : public extFilterEffect {
   public:
   std::vector<MergeSource> List;
};

/*********************************************************************************************************************
-ACTION-
Draw: Render the effect to the target bitmap.
-END-
*********************************************************************************************************************/

static ERROR MERGEFX_Draw(objMergeFX *Self, struct acDraw *Args)
{
   objBitmap *bmp;
   LONG copy_flags = 0;
   for (auto source : Self->List) {
      if (source.Effect) bmp = source.Effect->Target;
      else bmp = get_source_graphic(Self->Filter);
      if (!bmp) continue;

      gfxCopyArea(bmp, Self->Target, copy_flags, 0, 0, bmp->Width, bmp->Height, 0, 0);

      copy_flags = BAF_BLEND|BAF_COPY; // Any subsequent copies are to be blended
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR MERGEFX_NewObject(objMergeFX *Self, APTR Void)
{
   Self->SourceType = VSF_IGNORE;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
SourceList: A list of source types to be processed in the merge.

The list of sources is built as a simple array of MergeSource structures.

Input sources are defined by the SourceType field value.  In the case of `REFERENCE`, it is necessary to provide a
direct pointer to the referenced effect in the Effect field, or an error will be returned.

*********************************************************************************************************************/

static ERROR MERGEFX_SET_SourceList(objMergeFX *Self, MergeSource *Value, LONG Elements)
{
   if ((!Value) or (Elements <= 0)) {
      Self->List.clear();
      return ERR_Okay;
   }

   for (LONG i=0; i < Elements; i++) {
      if (Value[i].SourceType IS VSF_REFERENCE) {
         if (Value[i].Effect) ((extFilterEffect *)Value[i].Effect)->UsageCount++;
         else return ERR_InvalidData;
      }

      Self->List.push_back(Value[i]);
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
XMLDef: Returns an SVG compliant XML string that describes the filter.
-END-

*********************************************************************************************************************/

static ERROR MERGEFX_GET_XMLDef(objMergeFX *Self, STRING *Value)
{
   *Value = StrClone("feMerge");
   return ERR_Okay;
}

//********************************************************************************************************************

#include "filter_merge_def.c"

static const FieldArray clMergeFXFields[] = {
   { "SourceList", FDF_VIRTUAL|FDF_STRUCT|FDF_ARRAY|FDF_RW, (MAXINT)"MergeSource", NULL, (APTR)MERGEFX_SET_SourceList },
   { "XMLDef",     FDF_VIRTUAL|FDF_STRING|FDF_ALLOC|FDF_R, 0, (APTR)MERGEFX_GET_XMLDef, NULL },
   END_FIELD
};

//********************************************************************************************************************

ERROR init_mergefx(void)
{
   return(CreateObject(ID_METACLASS, 0, &clMergeFX,
      FID_BaseClassID|TLONG, ID_FILTEREFFECT,
      FID_SubClassID|TLONG,  ID_MERGEFX,
      FID_Name|TSTRING,      "MergeFX",
      FID_Category|TLONG,    CCF_GRAPHICS,
      FID_Flags|TLONG,       CLF_PRIVATE_ONLY|CLF_PROMOTE_INTEGRAL,
      FID_Actions|TPTR,      clMergeFXActions,
      FID_Fields|TARRAY,     clMergeFXFields,
      FID_Size|TLONG,        sizeof(objMergeFX),
      FID_Path|TSTR,         MOD_PATH,
      TAGEND));
}
