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
   RFT_INVERT,
   RFT_MASK
};

class Component {
   public:

   Component(CSTRING pName) : Name(pName), Type(RFT_IDENTITY) {
      for (size_t i=0; i < sizeof(Lookup); i++) {
         Lookup[i] = i;
         ILookup[i] = glLinearRGB.invert(i);
      }
   }

   std::string Name;
   std::vector<DOUBLE> Table; // If table; A list of function values.
   DOUBLE Slope;              // If linear; the slope of the linear function.
   DOUBLE Intercept;          // If linear; the intercept of the linear function.
   DOUBLE Amplitude;          // If gamma; the amplitude of the gamma function.
   DOUBLE Exponent;           // If gamma; the exponent of the gamma function.
   DOUBLE Offset;             // If gamma; the offset of the gamma function.
   RFT    Type;               // The type of algorithm to use.
   UBYTE  Lookup[256];        // sRGB lookup
   UBYTE  ILookup[256];       // Inverted linear RGB lookup

   void select_invert() {
      Type = RFT_INVERT;
      for (size_t i=0; i < sizeof(Lookup); i++) {
         Lookup[i]  = 255 - i;
         ILookup[i] = glLinearRGB.invert(255 - i);
      }
   }

   void select_identity() {
      Type = RFT_IDENTITY;
      for (size_t i=0; i < sizeof(Lookup); i++) {
         Lookup[i] = i;
         ILookup[i] = glLinearRGB.invert(i);
      }
   }

   void select_mask(UBYTE pMask) {
      Type = RFT_MASK;
      for (size_t i=0; i < sizeof(Lookup); i++) {
         Lookup[i]  = i & pMask;
         ILookup[i] = glLinearRGB.invert(i & pMask);
      }
   }

   void select_linear(const DOUBLE pSlope, const DOUBLE pIntercept) {
      Type      = RFT_LINEAR;
      Slope     = pSlope;
      Intercept = pIntercept;

      for (size_t i=0; i < sizeof(Lookup); i++) {
         ULONG c = F2T((DOUBLE(i) * pSlope) + pIntercept * 255.0);
         Lookup[i] = c;
         ILookup[i] = glLinearRGB.invert(c);
      }
   }

   void select_gamma(const DOUBLE pAmplitude, const DOUBLE pExponent, const DOUBLE pOffset) {
      Type      = RFT_GAMMA;
      Amplitude = pAmplitude;
      Exponent  = pExponent;
      Offset    = pOffset;

      for (size_t i=0; i < sizeof(Lookup); i++) {
         DOUBLE pe = pow(DOUBLE(i) * (1.0/255.0), pExponent);
         ULONG c = F2T(((pAmplitude * pe) + pOffset) * 255.0);
         Lookup[i]  = (c < 255) ? c : 255;
         ILookup[i] = glLinearRGB.invert((c < 255) ? c : 255);
      }
   }

   void select_discrete(const DOUBLE *pValues, const LONG pSize) {
      Type = RFT_DISCRETE;
      Table.insert(Table.end(), pValues, pValues + pSize);

      ULONG n = Table.size();
      for (size_t i=0; i < sizeof(Lookup); i++) {
         auto k = ULONG((i * n) / 255.0);
         k = std::min(k, (ULONG)n - 1);
         auto val = 255.0 * DOUBLE(Table[k]);
         val = std::max(0.0, std::min(255.0, val));
         Lookup[i] = UBYTE(val);
         ILookup[i] = glLinearRGB.invert(UBYTE(val));
      }
   }

   void select_table(const DOUBLE *pValues, const LONG pSize) {
      Type = RFT_TABLE;
      Table.insert(Table.end(), pValues, pValues + pSize);

      ULONG n = Table.size();
      for (size_t i=0; i < sizeof(Lookup); i++) {
          DOUBLE c = DOUBLE(i) / 255.0;
          auto k = ULONG(c * (n - 1));
          DOUBLE v = Table[std::min((k + 1), (n - 1))];
          LONG val = F2T(255.0 * (Table[k] + (c * (n - 1) - k) * (v - Table[k])));
          Lookup[i] = std::max(0, std::min(255, val));
          ILookup[i] = glLinearRGB.invert(Lookup[i]);
      }
   }
};

class extRemapFX : public extFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = ID_REMAPFX;
   static constexpr CSTRING CLASS_NAME = "RemapFX";
   using create = pf::Create<extRemapFX>;

   Component Red;
   Component Green;
   Component Blue;
   Component Alpha;

   extRemapFX() : Red("Red"), Green("Green"), Blue("Blue"), Alpha("Alpha") { }

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

static ERROR REMAPFX_Draw(extRemapFX *Self, struct acDraw *Args)
{
   if (Self->Target->BytesPerPixel != 4) return ERR_InvalidState;

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

      if (Self->Filter->ColourSpace IS VCS_LINEAR_RGB) {
         for (LONG x=0; x < width; x++) {
            if (auto a = sp[A]) {
               UBYTE out[4];
               out[R] = Self->Red.ILookup[glLinearRGB.convert(sp[R])];
               out[G] = Self->Green.ILookup[glLinearRGB.convert(sp[G])];
               out[B] = Self->Blue.ILookup[glLinearRGB.convert(sp[B])];
               out[A] = Self->Alpha.Lookup[a];
               dp[0] = ((ULONG *)out)[0];
            }
            dp++;
            sp += 4;
         }
      }
      else {
         for (LONG x=0; x < width; x++) {
            if (auto a = sp[A]) {
               UBYTE out[4];
               out[R] = Self->Red.Lookup[sp[R]];
               out[G] = Self->Green.Lookup[sp[G]];
               out[B] = Self->Blue.Lookup[sp[B]];
               out[A] = Self->Alpha.Lookup[a];
               dp[0] = ((ULONG *)out)[0];
            }
            dp++;
            sp += 4;
         }
      }

      dest += Self->Target->LineWidth;
      in   += bmp->LineWidth;
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR REMAPFX_Free(extRemapFX *Self, APTR Void)
{
   Self->~extRemapFX();
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR REMAPFX_NewObject(extRemapFX *Self, APTR Void)
{
   new (Self) extRemapFX;
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

static ERROR REMAPFX_SelectDiscrete(extRemapFX *Self, struct rfSelectDiscrete *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Values)) return log.warning(ERR_NullArgs);
   if ((Args->Size < 1) or (Args->Size > 1024)) return log.warning(ERR_Args);

   if (auto cmp = Self->getComponent(Args->Component)) {
      cmp->select_discrete(Args->Values, Args->Size);
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

static ERROR REMAPFX_SelectIdentity(extRemapFX *Self, struct rfSelectIdentity *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if (auto cmp = Self->getComponent(Args->Component)) {
      cmp->select_identity();
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

static ERROR REMAPFX_SelectGamma(extRemapFX *Self, struct rfSelectGamma *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if (auto cmp = Self->getComponent(Args->Component)) {
      cmp->select_gamma(Args->Amplitude, Args->Exponent, Args->Offset);
      log.extmsg("%s Amplitude: %.2f, Exponent: %.2f, Offset: %.2f", cmp->Name.c_str(), cmp->Amplitude, cmp->Exponent, cmp->Offset);
      return ERR_Okay;
   }
   else return log.warning(ERR_Args);
}

/*********************************************************************************************************************

-METHOD-
SelectInvert: Apply the invert function to a pixel component.

This method will apply the invert function to a selected RGBA pixel component.  The function is written as
`C' = 1.0 - C`.

This feature is not compatible with SVG.

-INPUT-
int(CMP) Component: The pixel component to which the function must be applied.

-RESULT-
Okay:
Args:
NullArgs:

*********************************************************************************************************************/

static ERROR REMAPFX_SelectInvert(extRemapFX *Self, struct rfSelectInvert *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if (auto cmp = Self->getComponent(Args->Component)) {
      cmp->select_invert();
      log.extmsg("%s", cmp->Name.c_str());
      return ERR_Okay;
   }
   else return log.warning(ERR_Args);
}

/*********************************************************************************************************************

-METHOD-
SelectLinear: Apply the linear function to a pixel component.

This method will apply the linear function to a selected RGBA pixel component.  The function is written as
`C' = (Slope * C) + Intercept`.

-INPUT-
int(CMP) Component: The pixel component to which the function must be applied.
double Slope: The slope of the linear function.
double Intercept: The intercept of the linear function.

-RESULT-
Okay:
Args:
NullArgs:

*********************************************************************************************************************/

static ERROR REMAPFX_SelectLinear(extRemapFX *Self, struct rfSelectLinear *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs);
   if (Args->Slope < 0) return log.warning(ERR_Args);

   if (auto cmp = Self->getComponent(Args->Component)) {
      cmp->select_linear(Args->Slope, Args->Intercept);
      log.extmsg("%s Slope: %.2f, Intercept: %.2f", cmp->Name.c_str(), cmp->Slope, Args->Intercept);
      return ERR_Okay;
   }
   else return log.warning(ERR_Args);
}

/*********************************************************************************************************************

-METHOD-
SelectMask: Apply the mask function to a pixel component.

This method will apply the mask function to a selected RGBA pixel component.  The function is written as
`C' = C & Mask`.  This algorithm is particularly useful for lowering the bit depth of colours, e.g. a value of `0xf0`
will reduce 8 bit colours to 4 bit.

This feature is not compatible with SVG.

-INPUT-
int(CMP) Component: The pixel component to which the function must be applied.
int Mask: The bit mask to be AND'd with each value.

-RESULT-
Okay:
Args:
NullArgs:

*********************************************************************************************************************/

static ERROR REMAPFX_SelectMask(extRemapFX *Self, struct rfSelectMask *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if (auto cmp = Self->getComponent(Args->Component)) {
      cmp->select_mask(Args->Mask);
      log.extmsg("%s, Mask: $%.2x", cmp->Name.c_str(), Args->Mask);
      return ERR_Okay;
   }
   else return log.warning(ERR_Args);
}

/*********************************************************************************************************************

-METHOD-
SelectTable: Apply the table function to a pixel component.

This method will apply the table function to a selected RGBA pixel component.  A list of values is required with a
minimum size of 1.

If a single table value is supplied then the component will be output as a constant with no interpolation applied.

-INPUT-
int(CMP) Component: The pixel component to which the table function must be applied.
array(double) Values: A list of values for the table function.
arraysize Size: Total number of elements in the value list.

-RESULT-
Okay:
Args:
NullArgs:

*********************************************************************************************************************/

static ERROR REMAPFX_SelectTable(extRemapFX *Self, struct rfSelectTable *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Values)) return log.warning(ERR_NullArgs);
   if ((Args->Size < 1) or (Args->Size > 1024)) return log.warning(ERR_Args);

   if (auto cmp = Self->getComponent(Args->Component)) {
      cmp->select_table(Args->Values, Args->Size);
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

static ERROR REMAPFX_GET_XMLDef(extRemapFX *Self, STRING *Value)
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
   { "XMLDef", FDF_VIRTUAL|FDF_STRING|FDF_ALLOC|FDF_R, REMAPFX_GET_XMLDef },
   END_FIELD
};

//********************************************************************************************************************

ERROR init_remapfx(void)
{
   clRemapFX = objMetaClass::create::global(
      fl::BaseClassID(ID_FILTEREFFECT),
      fl::ClassID(ID_REMAPFX),
      fl::Name("RemapFX"),
      fl::Category(CCF_GRAPHICS),
      fl::Actions(clRemapFXActions),
      fl::Methods(clRemapFXMethods),
      fl::Fields(clRemapFXFields),
      fl::Size(sizeof(extRemapFX)),
      fl::Path(MOD_PATH));

   return clRemapFX ? ERR_Okay : ERR_AddClass;
}
