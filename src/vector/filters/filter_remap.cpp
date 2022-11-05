/*********************************************************************************************************************

-CLASS-
RemapFX: Provides pixel remapping; equivalent to feComponentTransfer in SVG.

.

-END-

*********************************************************************************************************************/

class Component {
   std::vector<DOUBLE> Table; // If table; A list of function values.
   DOUBLE Slope;      // If linear; the slope of the linear function.
   DOUBLE Intercept;  // If linear; the intercept of the linear function.
   DOUBLE Amplitude;  // If gamma; the amplitude of the gamma function.
   DOUBLE Exponent;   // If gamma; the exponent of the gamma function.
   DOUBLE Offset;     // If gamma; the offset of the gamma function.
   LONG FunctionType; // The type of algorithm to use.
};

class objRemapFX : public extFilterEffect {
   public:
   Component Red;
   Component Green;
   Component Blue;
   Component Alpha;
};

/*********************************************************************************************************************
-ACTION-
Draw: Render the effect to the target bitmap.
-END-
*********************************************************************************************************************/

static ERROR REMAPFX_Draw(objRemapFX *Self, struct acDraw *Args)
{
   parasol::Log log;

   if (Self->Target->BytesPerPixel != 4) return ERR_Failed;

   objBitmap *inBmp;

   UBYTE *dest = Self->Target->Data + (Self->Target->Clip.Left * 4) + (Self->Target->Clip.Top * Self->Target->LineWidth);

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR REMAPFX_Init(objRemapFX *Self, APTR Void)
{
   parasol::Log log;

   if (!Self->MixType) {
      log.warning("A mix input is required.");
      return ERR_FieldNotSet;
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR REMAPFX_NewObject(objRemapFX *Self, APTR Void)
{
   Self->Operator = OP_OVER;
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
SelectGamma: Apply the gamma function to a pixel component.

-INPUT-
int(CMP) Component: The pixel component to which the gamma function must be applied.
double Amplitude: The amplitude of the gamma function.
double Offset: The offset of the gamma function.
double Exponent: The exponent of the gamma function.

-RESULT-
Okay:
NullArgs:

*********************************************************************************************************************/

static ERROR REMAPFX_SelectGamma(objRemapFX *Self, struct rsSelectGamma *Args)
{
   if (!Args) return ERR_NullArgs;

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
XMLDef: Returns an SVG compliant XML string that describes the filter.
-END-

*********************************************************************************************************************/

static ERROR REMAPFX_GET_XMLDef(objRemapFX *Self, STRING *Value)
{
   *Value = StrClone("feComponentTransfer");
   return ERR_Okay;
}

//********************************************************************************************************************

#include "filter_remap_def.c"

static const FieldArray clRemapFXFields[] = {
   { "XMLDef", FDF_VIRTUAL|FDF_STRING|FDF_ALLOC|FDF_R, 0, (APTR)REMAPFX_GET_XMLDef, NULL },
   END_FIELD
};

//********************************************************************************************************************

ERROR init_remapfx(void)
{
   return(CreateObject(ID_METACLASS, 0, &clRemapFX,
      FID_BaseClassID|TLONG, ID_FILTEREFFECT,
      FID_SubClassID|TLONG,  ID_REMAPFX,
      FID_Name|TSTRING,      "RemapFX",
      FID_Category|TLONG,    CCF_GRAPHICS,
      FID_Actions|TPTR,      clRemapFXActions,
      FID_Fields|TARRAY,     clRemapFXFields,
      FID_Size|TLONG,        sizeof(objRemapFX),
      FID_Path|TSTR,         MOD_PATH,
      TAGEND));
}
