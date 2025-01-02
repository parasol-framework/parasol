/*********************************************************************************************************************

-CLASS-
WaveFunctionFX: A filter effect that plots the probability distribution of a quantum wave function.

This filter effect uses a quantum wave function algorithm to generate a plot of electron probability density.
Ignoring its scientific value, the formula can be exploited for its aesthetic qualities.  It can be 
used as an alternative to the radial gradient for generating more interesting shapes for example.

The rendering of the wave function is controlled by its parameters #N, #L and #M.  A #Scale is also provided to deal
with situations where the generated plot would otherwise be too large for its bounds.

The parameter values are clamped according to the rules `N &gt;= 1`, `0 &lt;= L &lt; N`, `-L &lt;= M &lt;= L`.  
Check that the values are assigned and clamped correctly if the wave function is not rendering as expected.

-END-

*********************************************************************************************************************/

#include <complex>

class extWaveFunctionFX : public extFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::WAVEFUNCTIONFX;
   static constexpr CSTRING CLASS_NAME = "WaveFunctionFX";
   using create = pf::Create<extWaveFunctionFX>;

   std::vector<std::vector<double>> psi;
   std::vector<GradientStop> Stops;
   GradientColours *Colours;
   double Scale;
   double N, L, M, Max;
   bool Dirty;

   void compute_wavefunction(LONG);
};

//********************************************************************************************************************
// Wave function generator.  Note that only the top-left quadrant is generated for max efficiency.

inline LONG plot(LONG X, LONG W) { return -W + (X<<1); }

void extWaveFunctionFX::compute_wavefunction(LONG Resolution)
{
   constexpr double BOHR_RADIUS = 5.29177210903e-11 * 1e+12; // in picometers

   Max = 0;
   Dirty = false;
   psi = std::vector<std::vector<double>>(Resolution/2, std::vector<double>(Resolution/2));

   for (int dy = 0; dy < Resolution>>1; ++dy) {
      for (int dx = 0; dx < Resolution>>1; ++dx) {
         // Compute the radius value

         const double r = std::sqrt(plot(dy, Resolution) * plot(dy, Resolution) + plot(dx, Resolution) * plot(dx, Resolution));
         const double p = 2.0 * r / (N * (Scale * BOHR_RADIUS));
         const double constant_factor = std::sqrt(
            std::pow(2.0 / (N * (Scale * BOHR_RADIUS)), 3) * tgamma(N - L) / (2.0 * N * tgamma(N + L + 1))
         );
         const double laguerre = std::assoc_laguerre(N - L - 1, 2.0 * L + 1.0, p);
         const double rv = constant_factor * std::exp(-p * 0.5) * std::pow(p, L) * laguerre;

         // Angular function

         const double theta = std::atan2(plot(dy, Resolution), plot(dx, Resolution)); // Polar angle
         const double af_constant_factor = std::pow(-1, M) * std::sqrt(
            (2 * L + 1) * tgamma(L - std::abs(M) + 1) / (4 * agg::pi * tgamma(L + std::abs(M) + 1))
         );

         psi[dy][dx] = std::abs(rv * af_constant_factor * std::assoc_legendre(L, M, std::cos(theta)));
         if (psi[dy][dx] > Max) Max = psi[dy][dx];
      }
   }
}

/*********************************************************************************************************************
-ACTION-
Draw: Render the effect to the target bitmap.
-END-
*********************************************************************************************************************/

static ERR WAVEFUNCTIONFX_Draw(extWaveFunctionFX *Self, struct acDraw *Args)
{
   if (Self->Target->BytesPerPixel != 4) return ERR::Failed;

   const LONG t_width = Self->Target->Clip.Right - Self->Target->Clip.Left;
   const LONG t_height = Self->Target->Clip.Bottom - Self->Target->Clip.Top;

   unsigned resolution = std::min(t_width, t_height);
   const LONG q_width = resolution>>1;
   const LONG q_height = resolution>>1;

   if (Self->N < 1.0) Self->N = 1.0;
   Self->L = std::clamp(Self->L, 0.0, Self->N - 1.0);
   Self->M = std::clamp(Self->M, -Self->L, Self->L);

   // The wave function is symmetrical on all four corners, so the top-left quadrant is duplicated to the others.
   
   if ((Self->Dirty) or (resolution/2 != Self->psi.size())) Self->compute_wavefunction(resolution);

   auto max = Self->Max;
   auto data = (UBYTE *)(Self->Target->Data + (Self->Target->Clip.Left<<2) + (Self->Target->Clip.Top * Self->Target->LineWidth));
   for (LONG y = 0; y < q_height; ++y) {
      ULONG *top          = (ULONG *)(data + (Self->Target->LineWidth * y));
      ULONG *bottom       = (ULONG *)(data + (((q_height<<1) - y - 1) * Self->Target->LineWidth));
      ULONG *top_right    = (ULONG *)(top + (((q_width<<1) - 1)));
      ULONG *bottom_right = (ULONG *)(bottom + (((q_width<<1) - 1)));

      for (LONG x = 0; x < q_width; ++x, top++, bottom++, top_right--, bottom_right--) {
         UBYTE grey = F2T(Self->psi[x][y] / max * 255.0);
         ULONG col;
         if (Self->Colours) {
            auto &rgb = Self->Colours->table[grey];
            col = Self->Target->packPixel(rgb.r, rgb.g, rgb.b, rgb.a);
         }
         else col = Self->Target->packPixel(grey, grey, grey, 255);

         top[0] = col;
         bottom[0] = col;
         top_right[0] = col;
         bottom_right[0] = col;
      }
   }

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR WAVEFUNCTIONFX_Free(extWaveFunctionFX *Self)
{
   Self->psi.~vector<std::vector<double>>();
   Self->Stops.~vector<GradientStop>();
   if (Self->Colours) { delete Self->Colours; Self->Colours = NULL; }
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR WAVEFUNCTIONFX_Init(extWaveFunctionFX *Self)
{
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR WAVEFUNCTIONFX_NewObject(extWaveFunctionFX *Self)
{
   new (&Self->psi) std::vector<std::vector<double>>;
   new (&Self->Stops) std::vector<GradientStop>;
   Self->N = 1;
   Self->L = 0;
   Self->M = 1;
   Self->Scale = 1.0;
   Self->Dirty = true;
   Self->SourceType = VSF::NONE;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
L: Azimuthal quantum number.

*********************************************************************************************************************/

static ERR WAVEFUNCTIONFX_GET_L(extWaveFunctionFX *Self, LONG *Value)
{
   *Value = Self->L;
   return ERR::Okay;
}

static ERR WAVEFUNCTIONFX_SET_L(extWaveFunctionFX *Self, LONG Value)
{
   if (Value >= 0) {
      Self->L = Value;
      Self->Dirty = true;
      return ERR::Okay;
   }
   else return ERR::InvalidValue;
}

/*********************************************************************************************************************

-FIELD-
M: Magnetic quantum number.

*********************************************************************************************************************/

static ERR WAVEFUNCTIONFX_GET_M(extWaveFunctionFX *Self, LONG *Value)
{
   *Value = Self->M;
   return ERR::Okay;
}

static ERR WAVEFUNCTIONFX_SET_M(extWaveFunctionFX *Self, LONG Value)
{
   if (Value >= 0) {
      Self->M = Value;
      Self->Dirty = true;
      return ERR::Okay;
   }
   else return ERR::InvalidValue;
}

/*********************************************************************************************************************

-FIELD-
N: Principal quantum number.

*********************************************************************************************************************/

static ERR WAVEFUNCTIONFX_GET_N(extWaveFunctionFX *Self, LONG *Value)
{
   *Value = Self->N;
   return ERR::Okay;
}

static ERR WAVEFUNCTIONFX_SET_N(extWaveFunctionFX *Self, LONG Value)
{
   if (Value >= 0) {
      Self->N = Value;
      Self->Dirty = true;
      return ERR::Okay;
   }
   else return ERR::InvalidValue;
}

/*********************************************************************************************************************

-FIELD-
Scale: Multiplier that affects the scale of the plot.

*********************************************************************************************************************/

static ERR WAVEFUNCTIONFX_GET_Scale(extWaveFunctionFX *Self, double *Value)
{
   *Value = Self->Scale;
   return ERR::Okay;
}

static ERR WAVEFUNCTIONFX_SET_Scale(extWaveFunctionFX *Self, double Value)
{
   if (Value >= 0) {
      Self->Scale = Value;
      Self->Dirty = true;
      return ERR::Okay;
   }
   else return ERR::InvalidValue;
}

/*********************************************************************************************************************

-FIELD-
Stops: Defines the colours to use for the wave function.

The colours that will be used for drawing a wave function can be defined by the Stops array.  At least two stops are 
required to define a start and end point for interpolating the gradient colours.

If no stops are defined, the wave function will be drawn in greyscale.
-END-
*********************************************************************************************************************/

static ERR WAVEFUNCTIONFX_GET_Stops(extWaveFunctionFX *Self, GradientStop **Value, LONG *Elements)
{
   *Value    = Self->Stops.data();
   *Elements = Self->Stops.size();
   return ERR::Okay;
}

static ERR WAVEFUNCTIONFX_SET_Stops(extWaveFunctionFX *Self, GradientStop *Value, LONG Elements)
{
   Self->Stops.clear();

   if (Elements >= 2) {
      Self->Stops.insert(Self->Stops.end(), &Value[0], &Value[Elements]);
      if (Self->Colours) delete Self->Colours;
      Self->Colours = new (std::nothrow) GradientColours(Self->Stops, /*Self->Filter->ColourSpace*/ VCS::SRGB, 1.0);
      if (!Self->Colours) return ERR::AllocMemory;
      return ERR::Okay;
   }
   else {
      pf::Log log;
      log.warning("Array size %d < 2", Elements);
      return ERR::InvalidValue;
   }
}

/*********************************************************************************************************************

-FIELD-
XMLDef: Returns an SVG compliant XML string that describes the effect.
-END-

*********************************************************************************************************************/

static ERR WAVEFUNCTIONFX_GET_XMLDef(extWaveFunctionFX *Self, STRING *Value)
{
   std::stringstream stream;

   stream << "feWaveFunction";

   *Value = strclone(stream.str());
   return ERR::Okay;
}

//********************************************************************************************************************

#include "filter_wavefunction_def.c"

static const FieldArray clWaveFunctionFXFields[] = {
   { "N",       FDF_VIRTUAL|FDF_LONG|FDF_RW,             WAVEFUNCTIONFX_GET_N,       WAVEFUNCTIONFX_SET_N },
   { "L",       FDF_VIRTUAL|FDF_LONG|FDF_RW,             WAVEFUNCTIONFX_GET_L,       WAVEFUNCTIONFX_SET_L },
   { "M",       FDF_VIRTUAL|FDF_LONG|FDF_RW,             WAVEFUNCTIONFX_GET_M,       WAVEFUNCTIONFX_SET_M },
   { "Scale",   FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,           WAVEFUNCTIONFX_GET_Scale,   WAVEFUNCTIONFX_SET_Scale },
   { "Stops",   FDF_VIRTUAL|FDF_ARRAY|FDF_STRUCT|FDF_RW, WAVEFUNCTIONFX_GET_Stops, WAVEFUNCTIONFX_SET_Stops, "GradientStop" },
   { "XMLDef",  FDF_VIRTUAL|FDF_STRING|FDF_ALLOC|FDF_R,  WAVEFUNCTIONFX_GET_XMLDef,  NULL },
   END_FIELD
};

//********************************************************************************************************************

ERR init_wavefunctionfx(void)
{
   clWaveFunctionFX = objMetaClass::create::global(
      fl::BaseClassID(CLASSID::FILTEREFFECT),
      fl::ClassID(CLASSID::WAVEFUNCTIONFX),
      fl::Name("WaveFunctionFX"),
      fl::Category(CCF::GRAPHICS),
      fl::Actions(clWaveFunctionFXActions),
      fl::Fields(clWaveFunctionFXFields),
      fl::Size(sizeof(extWaveFunctionFX)),
      fl::Path(MOD_PATH));

   return clWaveFunctionFX ? ERR::Okay : ERR::AddClass;
}
