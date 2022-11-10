/*********************************************************************************************************************

-CLASS-
RemapFX: Provides pixel remapping; equivalent to feComponentTransfer in SVG.

The RemapFX class provides an implementation of the feComponentTransfer functionality in SVG.

Internally the pixel rendering process is implemented using pixel lookup tables.  As such this particular effect
carries minimal overhead compared to most other effect classes.

-END-

*********************************************************************************************************************/

#include <algorithm>

enum RFT {
   RFT_IDENTITY=0,
   RFT_DISCRETE,
   RFT_LINEAR,
   RFT_GAMMA,
   RFT_TABLE,
   RFT_INVERT
};

class Component {
   public:

   Component(CSTRING pName) : Name(pName), Type(RFT_IDENTITY) {
      for (size_t i=0; i < sizeof(Lookup); i++) Lookup[i] = i;
   }

   std::string Name;
   std::vector<DOUBLE> Table; // If table; A list of function values.
   DOUBLE Slope;              // If linear; the slope of the linear function.
   DOUBLE Intercept;          // If linear; the intercept of the linear function.
   DOUBLE Amplitude;          // If gamma; the amplitude of the gamma function.
   DOUBLE Exponent;           // If gamma; the exponent of the gamma function.
   DOUBLE Offset;             // If gamma; the offset of the gamma function.
   RFT    Type;               // The type of algorithm to use.
   UBYTE  Lookup[256];
};

class objRemapFX : public extFilterEffect {
   public:
   Component Red;
   Component Green;
   Component Blue;
   Component Alpha;

   objRemapFX() : Red("Red"), Green("Green"), Blue("Blue"), Alpha("Alpha") { }

   Component * getComponent(LONG Component) {
      switch(Component) {
         case CMP_RED:   return &Red;
         case CMP_GREEN: return &Green;
         case CMP_BLUE:  return &Blue;
         case CMP_ALPHA: return &Alpha;
         default: return NULL;
      }
   }
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

   objBitmap *bmp;
   if (get_source_bitmap(Self->Filter, &bmp, Self->SourceType, Self->Input, false)) return ERR_Failed;

   LONG height = Self->Target->Clip.Bottom - Self->Target->Clip.Top;
   LONG width  = Self->Target->Clip.Right - Self->Target->Clip.Left;
   if (bmp->Clip.Right - bmp->Clip.Left < width) width = bmp->Clip.Right - bmp->Clip.Left;
   if (bmp->Clip.Bottom - bmp->Clip.Top < height) height = bmp->Clip.Bottom - bmp->Clip.Top;

   const UBYTE R = Self->Target->ColourFormat->RedPos>>3;
   const UBYTE G = Self->Target->ColourFormat->GreenPos>>3;
   const UBYTE B = Self->Target->ColourFormat->BluePos>>3;
   const UBYTE A = Self->Target->ColourFormat->AlphaPos>>3;

   UBYTE *in = bmp->Data + (bmp->Clip.Left * 4) + (bmp->Clip.Top * bmp->LineWidth);
   UBYTE *dest = Self->Target->Data + (Self->Target->Clip.Left * 4) + (Self->Target->Clip.Top * Self->Target->LineWidth);
   for (LONG y=0; y < height; y++) {
      auto dp = (ULONG *)dest;
      auto sp = in;
      for (LONG x=0; x < width; x++) {
         UBYTE out[4];
         out[R] = Self->Red.Lookup[sp[R]];
         out[G] = Self->Green.Lookup[sp[G]];
         out[B] = Self->Blue.Lookup[sp[B]];
         out[A] = Self->Alpha.Lookup[sp[A]];
         dp[0] = ((ULONG *)out)[0];
         dp++;
         sp += 4;
      }
      dest += Self->Target->LineWidth;
      in   += bmp->LineWidth;
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR REMAPFX_Free(objRemapFX *Self, APTR Void)
{
   Self->~objRemapFX();
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR REMAPFX_NewObject(objRemapFX *Self, APTR Void)
{
   new (Self) objRemapFX;
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
SelectDiscrete: Apply the discrete function to a pixel component.

This method will apply the table function to a selected RGBA pixel component.  A list of values is required with a
minimum size of 1.

-INPUT-
int(CMP) Component: The pixel component to which the discrete function must be applied.
array(double) Values: A list of values for the discrete function.
arraysize Size: Total number of elements in the value list.

-RESULT-
Okay:
NullArgs:

*********************************************************************************************************************/

static ERROR REMAPFX_SelectDiscrete(objRemapFX *Self, struct rfSelectDiscrete *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Values)) return log.warning(ERR_NullArgs);
   if ((Args->Size < 1) or (Args->Size > 1024)) return log.warning(ERR_Args);

   if (auto cmp = Self->getComponent(Args->Component)) {
      cmp->Type = RFT_DISCRETE;
      cmp->Table.insert(cmp->Table.end(), Args->Values, Args->Values + Args->Size);

      ULONG n = cmp->Table.size();
      for (size_t i=0; i < sizeof(cmp->Lookup); i++) {
         auto k = ULONG((i * n) / 255.0);
         k = std::min(k, (ULONG)n - 1);
         auto val = 255.0 * DOUBLE(cmp->Table[k]);
         val = std::max(0.0, std::min(255.0, val));
         cmp->Lookup[i] = (UBYTE)val;
      }

      log.extmsg("%s Values: %d", cmp->Name.c_str(), Args->Size);
      return ERR_Okay;
   }
   else return log.warning(ERR_Args);
}

/*********************************************************************************************************************

-METHOD-
SelectIdentity: Apply the identity function to a pixel component.

Selecting the identity function for a pixel component will render it as its original value.  By default, all pixels
will use this function if no other option is chosen.

-INPUT-
int(CMP) Component: The pixel component to which the identity function must be applied.

-RESULT-
Okay:
NullArgs:

*********************************************************************************************************************/

static ERROR REMAPFX_SelectIdentity(objRemapFX *Self, struct rfSelectIdentity *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if (auto cmp = Self->getComponent(Args->Component)) {
      cmp->Type = RFT_IDENTITY;
      for (size_t i=0; i < sizeof(cmp->Lookup); i++) {
         cmp->Lookup[i] = i;
      }
      log.extmsg("%s", cmp->Name.c_str());
      return ERR_Okay;
   }
   else return log.warning(ERR_Args);
}

/*********************************************************************************************************************

-METHOD-
SelectGamma: Apply the gamma function to a pixel component.

This method will apply the gamma function to a selected RGBA pixel component.  The gamma function is written as
`C' = Amplitude * pow(C, Exponent) + Offset`.

-INPUT-
int(CMP) Component: The pixel component to which the gamma function must be applied.
double Amplitude: The amplitude of the gamma function.
double Offset: The offset of the gamma function.
double Exponent: The exponent of the gamma function.

-RESULT-
Okay:
NullArgs:

*********************************************************************************************************************/

static ERROR REMAPFX_SelectGamma(objRemapFX *Self, struct rfSelectGamma *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if (auto cmp = Self->getComponent(Args->Component)) {
      cmp->Type      = RFT_GAMMA;
      cmp->Amplitude = Args->Amplitude;
      cmp->Exponent  = Args->Exponent;
      cmp->Offset    = Args->Offset;

      for (size_t i=0; i < sizeof(cmp->Lookup); i++) {
         DOUBLE pe = pow(DOUBLE(i) * (1.0/255.0), cmp->Exponent);
         ULONG c = F2T(((cmp->Amplitude * pe) + cmp->Offset) * 255.0);
         cmp->Lookup[i] = (c < 255) ? c : 255;
      }

      log.extmsg("%s Amplitude: %.2f, Exponent: %.2f, Offset: %.2f", cmp->Name.c_str(), cmp->Amplitude, cmp->Exponent, cmp->Offset);
      return ERR_Okay;
   }
   else return log.warning(ERR_Args);
}

/*********************************************************************************************************************

-METHOD-
SelectInvert: Apply the invert function to a pixel component.

This method will apply the invert function to a selected RGBA pixel component.  The linear function is written as
`C' = 1.0 - C`.

This feature is not compatible with SVG.

-INPUT-
int(CMP) Component: The pixel component to which the linear function must be applied.

-RESULT-
Okay:
NullArgs:

*********************************************************************************************************************/

static ERROR REMAPFX_SelectInvert(objRemapFX *Self, struct rfSelectInvert *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if (auto cmp = Self->getComponent(Args->Component)) {
      cmp->Type = RFT_INVERT;

      for (size_t i=0; i < sizeof(cmp->Lookup); i++) {
         cmp->Lookup[i] = 255 - i;
      }

      log.extmsg("%s", cmp->Name.c_str());
      return ERR_Okay;
   }
   else return log.warning(ERR_Args);
}

/*********************************************************************************************************************

-METHOD-
SelectLinear: Apply the linear function to a pixel component.

This method will apply the linear function to a selected RGBA pixel component.  The linear function is written as
`C' = (Slope * C) + Intercept`.

-INPUT-
int(CMP) Component: The pixel component to which the linear function must be applied.
double Slope: The slope of the linear function.
double Intercept: The intercept of the linear function.

-RESULT-
Okay:
NullArgs:

*********************************************************************************************************************/

static ERROR REMAPFX_SelectLinear(objRemapFX *Self, struct rfSelectLinear *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);
   if (Args->Slope < 0) return log.warning(ERR_Args);

   if (auto cmp = Self->getComponent(Args->Component)) {
      cmp->Type      = RFT_LINEAR;
      cmp->Slope     = Args->Slope;
      cmp->Intercept = Args->Intercept;

      for (size_t i=0; i < sizeof(cmp->Lookup); i++) {
         ULONG c = F2T((DOUBLE(i) * cmp->Slope) + cmp->Intercept * 255.0);
         cmp->Lookup[i] = c;
      }

      log.extmsg("%s Slope: %.2f, Intercept: %.2f", cmp->Name.c_str(), cmp->Slope, Args->Intercept);
      return ERR_Okay;
   }
   else return log.warning(ERR_Args);
}

/*********************************************************************************************************************

-METHOD-
SelectTable: Apply the table function to a pixel component.

This method will apply the table function to a selected RGBA pixel component.  A list of values is required with a
minimum size of 1.

-INPUT-
int(CMP) Component: The pixel component to which the table function must be applied.
array(double) Values: A list of values for the table function.
arraysize Size: Total number of elements in the value list.

-RESULT-
Okay:
NullArgs:

*********************************************************************************************************************/

static ERROR REMAPFX_SelectTable(objRemapFX *Self, struct rfSelectTable *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Values)) return log.warning(ERR_NullArgs);
   if ((Args->Size < 1) or (Args->Size > 1024)) return log.warning(ERR_Args);

   if (auto cmp = Self->getComponent(Args->Component)) {
      cmp->Type = RFT_TABLE;
      cmp->Table.insert(cmp->Table.end(), Args->Values, Args->Values + Args->Size);

      ULONG n = cmp->Table.size();
      for (size_t i=0; i < sizeof(cmp->Lookup); i++) {
          DOUBLE c = DOUBLE(i) / 255.0;
          auto k = ULONG(c * (n - 1));
          DOUBLE v = cmp->Table[std::min((k + 1), (n - 1))];
          LONG val = F2T(255.0 * (cmp->Table[k] + (c * (n - 1) - k) * (v - cmp->Table[k])));
          cmp->Lookup[i] = std::max(0, std::min(255, val));
      }

      log.extmsg("%s Values: %d", cmp->Name.c_str(), Args->Size);
      return ERR_Okay;
   }
   else return log.warning(ERR_Args);
}

/*********************************************************************************************************************

-FIELD-
XMLDef: Returns an SVG compliant XML string that describes the filter.
-END-

*********************************************************************************************************************/

static ERROR REMAPFX_GET_XMLDef(objRemapFX *Self, STRING *Value)
{
   std::stringstream stream;

   // TODO
   stream << "<feComponentTransfer>";
   stream << "<feFuncR/>";
   stream << "<feFuncG/>";
   stream << "<feFuncB/>";
   stream << "<feFuncA/>";
   stream << "</feComponentTransfer>";
   *Value = StrClone(stream.str().c_str());
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
      FID_Methods|TARRAY,    clRemapFXMethods,
      FID_Fields|TARRAY,     clRemapFXFields,
      FID_Size|TLONG,        sizeof(objRemapFX),
      FID_Path|TSTR,         MOD_PATH,
      TAGEND));
}
