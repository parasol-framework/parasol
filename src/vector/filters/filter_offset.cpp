/*********************************************************************************************************************

-CLASS-
OffsetFX: A filter effect that offsets the position of an input source.

This filter offsets the input image relative to its current position in the image space by the specified vector
of `(XOffset,YOffset)`.

-END-

*********************************************************************************************************************/

class extOffsetFX : public extFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = ID_OFFSETFX;
   static constexpr CSTRING CLASS_NAME = "OffsetFX";
   using create = pf::Create<extOffsetFX>;

   LONG XOffset, YOffset;
};

//********************************************************************************************************************

static ERROR OFFSETFX_Draw(extOffsetFX *Self, struct acDraw *Args)
{
   objBitmap *inBmp;
   LONG dx = F2T((DOUBLE)Self->XOffset * Self->Filter->ClientVector->Transform.sx);
   LONG dy = F2T((DOUBLE)Self->YOffset * Self->Filter->ClientVector->Transform.sy);
   if (!get_source_bitmap(Self->Filter, &inBmp, Self->SourceType, Self->Input, false)) {
      gfxCopyArea(inBmp, Self->Target, 0, 0, 0, inBmp->Width, inBmp->Height, dx, dy);
      return ERR_Okay;
   }
   else return ERR_Failed;
}

/*********************************************************************************************************************

-FIELD-
XOffset: The delta X coordinate for the input graphic.

The (XOffset,YOffset) field values define the offset of the input source within the target clipping area.

*********************************************************************************************************************/

static ERROR OFFSETFX_GET_XOffset(extOffsetFX *Self, LONG *Value)
{
   *Value = Self->XOffset;
   return ERR_Okay;
}

static ERROR OFFSETFX_SET_XOffset(extOffsetFX *Self, LONG Value)
{
   Self->XOffset = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
YOffset: The delta Y coordinate for the input graphic.

The (XOffset,YOffset) field values define the offset of the input source within the target clipping area.

*********************************************************************************************************************/

static ERROR OFFSETFX_GET_YOffset(extOffsetFX *Self, LONG *Value)
{
   *Value = Self->YOffset;
   return ERR_Okay;
}

static ERROR OFFSETFX_SET_YOffset(extOffsetFX *Self, LONG Value)
{
   Self->YOffset = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
XMLDef: Returns an SVG compliant XML string that describes the effect.
-END-

*********************************************************************************************************************/

static ERROR OFFSETFX_GET_XMLDef(extOffsetFX *Self, STRING *Value)
{
   std::stringstream stream;
   stream << "feOffset dx=\"" << Self->XOffset << "\" dy=\"" << Self->YOffset << "\"";
   *Value = StrClone(stream.str().c_str());
   return ERR_Okay;
}

//********************************************************************************************************************

#include "filter_offset_def.c"

static const FieldArray clOffsetFXFields[] = {
   { "XOffset", FDF_VIRTUAL|FDF_LONG|FDF_RW, OFFSETFX_GET_XOffset, OFFSETFX_SET_XOffset },
   { "YOffset", FDF_VIRTUAL|FDF_LONG|FDF_RW, OFFSETFX_GET_YOffset, OFFSETFX_SET_YOffset },
   { "XMLDef",  FDF_VIRTUAL|FDF_STRING|FDF_ALLOC|FDF_R, OFFSETFX_GET_XMLDef },
   END_FIELD
};

//********************************************************************************************************************

ERROR init_offsetfx(void)
{
   clOffsetFX = objMetaClass::create::global(
      fl::BaseClassID(ID_FILTEREFFECT),
      fl::ClassID(ID_OFFSETFX),
      fl::Name("OffsetFX"),
      fl::Category(CCF::GRAPHICS),
      fl::Actions(clOffsetFXActions),
      fl::Fields(clOffsetFXFields),
      fl::Size(sizeof(extOffsetFX)),
      fl::Path(MOD_PATH));

   return clOffsetFX ? ERR_Okay : ERR_AddClass;
}
