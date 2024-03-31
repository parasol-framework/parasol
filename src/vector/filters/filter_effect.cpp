/*********************************************************************************************************************

-CLASS-
FilterEffect: FilterEffect is a support class for managing effects hosted by the VectorFilter class.

The FilterEffect class provides base-class functionality for effect classes.  FilterEffect objects mut not be
instantiated directly by the client.

The documented fields and actions here are integral to all effects that utilise this class.

-END-

*********************************************************************************************************************/

static ERR FILTEREFFECT_Free(extFilterEffect *Self, APTR Void)
{
   if (Self->Filter) {
      for (auto e = Self->Filter->Effects; (e) and (Self->UsageCount > 0); e = (extFilterEffect *)e->Next) {
         if (e->Input IS Self) { e->Input = NULL; Self->UsageCount--; }
         if (e->Mix IS Self) { e->Mix = NULL; Self->UsageCount--; }
      }

      if (Self->Filter->Effects IS Self) Self->Filter->Effects = (extFilterEffect *)Self->Next;
      if (Self->Filter->LastEffect IS Self) Self->Filter->LastEffect = (extFilterEffect *)Self->Prev;
   }

   if (Self->Prev) Self->Prev->Next = Self->Next;
   if (Self->Next) Self->Next->Prev = Self->Prev;

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR FILTEREFFECT_Init(extFilterEffect *Self, APTR Void)
{
   pf::Log log;

   if (!Self->Filter) return log.warning(ERR::UnsupportedOwner);

   // If the client didn't specify a source input, figure out what to use.

   if (Self->SourceType IS VSF::PREVIOUS) {
      if (Self->Prev) {
         Self->SourceType = VSF::REFERENCE;
         Self->Input = Self->Prev;
         ((extFilterEffect *)Self->Input)->UsageCount++;
         log.msg("Using effect %s #%d as an input.", Self->Input->Class->ClassName, Self->Input->UID);
      }
      else {
         Self->SourceType = VSF::GRAPHIC;
         log.msg("Using SourceGraphic as an input.");
      }
   }

   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
MoveToBack: Move an effect to the back of the VectorFilter's list order.
-END-
*********************************************************************************************************************/

static ERR FILTEREFFECT_MoveToBack(extFilterEffect *Self, APTR Void)
{
   if (Self->Filter->Effects != Self) {
      if (Self->Filter->LastEffect IS Self) Self->Filter->LastEffect = (extFilterEffect *)Self->Prev;

      if (Self->Prev) Self->Prev->Next = Self->Next;
      if (Self->Next) Self->Next->Prev = Self->Prev;

      Self->Prev = NULL;
      Self->Next = Self->Filter->Effects;
      Self->Next->Prev = Self;
      Self->Filter->Effects = Self;
   }

   return ERR::Okay;
}


/*********************************************************************************************************************
-ACTION-
MoveToBack: Move an effect to the front of the VectorFilter's list order.
-END-
*********************************************************************************************************************/

static ERR FILTEREFFECT_MoveToFront(extFilterEffect *Self, APTR Void)
{
   if (Self->Next) {
      if (Self->Prev) Self->Prev->Next = Self->Next;
      if (Self->Next) Self->Next->Prev = Self->Prev;

      Self->Next = NULL;
      Self->Prev = Self->Filter->LastEffect;
      Self->Prev->Next = Self;

      Self->Filter->LastEffect = Self;
   }

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR FILTEREFFECT_NewObject(extFilterEffect *Self, APTR Void)
{
   Self->SourceType = VSF::PREVIOUS; // Use previous effect as input, or SourceGraphic if no previous effect.
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR FILTEREFFECT_NewOwner(extFilterEffect *Self, struct acNewOwner *Args)
{
   if (Args->NewOwner->Class->BaseClassID IS ID_VECTORFILTER) {
      Self->Filter = (extVectorFilter *)Args->NewOwner;
   }
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Dimensions: Dimension flags are stored here.

Dimension flags are automatically defined when setting the #X, #Y, #Width and #Height fields.

-FIELD-
Input: Reference to another effect to be used as an input source.

If another effect should be used as a source input, it must be referenced here.  The #SourceType will be automatically
set to `REFERENCE` as a result.

This field is the SVG equivalent to `in`.  If the Input is not defined by the client then it will default to the
previous effect if available, otherwise the source graphic is used.

*********************************************************************************************************************/

static ERR FILTEREFFECT_SET_Input(extFilterEffect *Self, extFilterEffect *Value)
{
   if (Value IS Self) return ERR::InvalidValue;

   if ((Self->SourceType IS VSF::REFERENCE) and (Self->Input)) {
      ((extFilterEffect *)Self->Input)->UsageCount--;
   }

   if (Value) {
      Self->SourceType = VSF::REFERENCE;
      Self->Input      = Value;
      Value->UsageCount++;
   }
   else {
      Self->Input = NULL;
      Self->SourceType = VSF::NIL;
   }
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Height: Primitive height of the effect area.

The (Width,Height) field values define the dimensions of the effect within the target clipping area.

*********************************************************************************************************************/

static ERR FILTEREFFECT_GET_Height(extFilterEffect *Self, Variable *Value)
{
   DOUBLE val = Self->Height;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR::Okay;
}

static ERR FILTEREFFECT_SET_Height(extFilterEffect *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR::FieldTypeMismatch;

   if (Value->Type & FD_SCALED) Self->Dimensions = (Self->Dimensions | DMF_SCALED_HEIGHT) & (~DMF_FIXED_HEIGHT);
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_HEIGHT) & (~DMF_SCALED_HEIGHT);

   Self->Height = val;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Mix: Reference to another effect to be used a mixer with Input.

If another effect should be used as a mixed source input, it must be referenced here.  The #MixType will be
automatically set to `REFERENCE` as a result.

This field is the SVG equivalent to `in2`.  It does nothing if the effect does not supported a mixed source input.

*********************************************************************************************************************/

static ERR FILTEREFFECT_SET_Mix(extFilterEffect *Self, extFilterEffect *Value)
{
   pf::Log log;

   if (Value IS Self) return log.warning(ERR::InvalidValue);

   Self->MixType = VSF::REFERENCE;
   Self->Mix     = Value;
   ((extFilterEffect *)Self->Mix)->UsageCount++;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
MixType: If a secondary mix input is required for the effect, specify it here.
Lookup: VSF

Some effects support a secondary mix input for compositing, such as the @CompositeFX class.  If the mixing source is
a reference to another effect, set the #Mix field instead and this field will be set to `REFERENCE` automatically.

-FIELD-
SourceType: Specifies an input source for the effect algorithm, if required.
Lookup: VSF

If an effect requires an input source for processing, it must be specified here.  If the source is a reference to
another effect, set the #Input field instead and this field will be set to `REFERENCE` automatically.

-FIELD-
Width: Primitive width of the effect area.

The (Width,Height) field values define the dimensions of the effect within the target clipping area.

*********************************************************************************************************************/

static ERR FILTEREFFECT_GET_Width(extFilterEffect *Self, Variable *Value)
{
   DOUBLE val = Self->Width;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR::Okay;
}

static ERR FILTEREFFECT_SET_Width(extFilterEffect *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR::FieldTypeMismatch;

   if (Value->Type & FD_SCALED) Self->Dimensions = (Self->Dimensions | DMF_SCALED_WIDTH) & (~DMF_FIXED_WIDTH);
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_WIDTH) & (~DMF_SCALED_WIDTH);

   Self->Width = val;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
X: Primitive X coordinate for the effect.

The (X,Y) field values define the offset of the effect within the target clipping area.

*********************************************************************************************************************/

static ERR FILTEREFFECT_GET_X(extFilterEffect *Self, Variable *Value)
{
   DOUBLE val = Self->X;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR::Okay;
}

static ERR FILTEREFFECT_SET_X(extFilterEffect *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR::FieldTypeMismatch;

   if (Value->Type & FD_SCALED) Self->Dimensions = (Self->Dimensions | DMF_SCALED_X) & (~DMF_FIXED_X);
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_X) & (~DMF_SCALED_X);

   Self->X = val;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Y: Primitive Y coordinate for the effect.

The (X,Y) field values define the offset of the effect within the target clipping area.
-END-

*********************************************************************************************************************/

static ERR FILTEREFFECT_GET_Y(extFilterEffect *Self, Variable *Value)
{
   DOUBLE val = Self->Y;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR::Okay;
}

static ERR FILTEREFFECT_SET_Y(extFilterEffect *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR::FieldTypeMismatch;

   if (Value->Type & FD_SCALED) Self->Dimensions = (Self->Dimensions | DMF_SCALED_Y) & (~DMF_FIXED_Y);
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_Y) & (~DMF_SCALED_Y);

   Self->Y = val;
   return ERR::Okay;
}

//********************************************************************************************************************

#include "filter_effect_def.c"

static const FieldArray clFilterEffectFields[] = {
   { "Next",       FDF_OBJECT|FDF_RW, NULL, NULL, ID_FILTEREFFECT },
   { "Prev",       FDF_OBJECT|FDF_RW, NULL, NULL, ID_FILTEREFFECT },
   { "Target",     FDF_OBJECT|FDF_RW, NULL, NULL, ID_BITMAP },
   { "Input",      FDF_OBJECT|FDF_RW, NULL, FILTEREFFECT_SET_Input, ID_FILTEREFFECT },
   { "Mix",        FDF_OBJECT|FDF_RW, NULL, FILTEREFFECT_SET_Mix, ID_FILTEREFFECT },
   { "X",          FDF_VARIABLE|FDF_DOUBLE|FDF_SCALED|FDF_RW, FILTEREFFECT_GET_X, FILTEREFFECT_SET_X },
   { "Y",          FDF_VARIABLE|FDF_DOUBLE|FDF_SCALED|FDF_RW, FILTEREFFECT_GET_Y, FILTEREFFECT_SET_Y },
   { "Width",      FDF_VARIABLE|FDF_DOUBLE|FDF_SCALED|FDF_RW, FILTEREFFECT_GET_Width, FILTEREFFECT_SET_Width },
   { "Height",     FDF_VARIABLE|FDF_DOUBLE|FDF_SCALED|FDF_RW, FILTEREFFECT_GET_Height, FILTEREFFECT_SET_Height },
   { "Dimensions", FDF_LONGFLAGS|FDF_R, NULL, NULL, &clFilterEffectDimensions },
   { "SourceType", FDF_LONG|FDF_LOOKUP|FDF_RW, NULL, NULL, &clFilterEffectSourceType },
   { "MixType",    FDF_LONG|FDF_LOOKUP|FDF_RW, NULL, NULL, &clFilterEffectMixType },
   END_FIELD
};

//********************************************************************************************************************

ERR init_filtereffect(void)
{
   clFilterEffect = objMetaClass::create::global(
      fl::ClassVersion(VER_FILTEREFFECT),
      fl::Name("FilterEffect"),
      fl::Category(CCF::GRAPHICS),
      fl::Actions(clFilterEffectActions),
      fl::Fields(clFilterEffectFields),
      fl::Size(sizeof(extFilterEffect)),
      fl::Path(MOD_PATH));

   return clFilterEffect ? ERR::Okay : ERR::AddClass;
}