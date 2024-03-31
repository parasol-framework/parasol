/*********************************************************************************************************************

-CLASS-
TurbulenceFX: A filter effect that utilises the Perlin turbulence function.

This filter effect creates an image using the Perlin turbulence function. It allows the synthesis of artificial
textures like clouds or marble.  For a detailed description the of the Perlin turbulence function, see "Texturing
and Modeling", Ebert et al, AP Professional, 1994. The resulting image will fill the entire filter primitive
subregion for this filter primitive.

It is possible to create bandwidth-limited noise by synthesizing only one octave.

The following order is used for applying the pseudo random numbers.  An initial seed value is computed based on
#Seed.  Then the implementation computes the lattice points for R, then continues getting additional pseudo random
numbers relative to the last generated pseudo random number and computes the lattice points for G, and so on for B
and A.

-END-

*********************************************************************************************************************/

#define RAND_m 2147483647 // 2**31 - 1
#define RAND_a 16807      // 7**5; primitive root of m
#define RAND_q 127773     // m / a
#define RAND_r 2836       // m % a

#define BSIZE 0x100
#define BM 0xff
#define PerlinN 0x1000
#define NP 12 // 2^PerlinN
#define NM 0xfff
#define GSIZE 4
#define GSUBSIZE 2
#define LSIZE (BSIZE + BSIZE + 2)

#define s_curve(t) (t * t * (3.0 - 2.0 * t))
#define lerp(t, a, b) (a + t * (b - a))

class extTurbulenceFX : public extFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = ID_TURBULENCEFX;
   static constexpr CSTRING CLASS_NAME = "TurbulenceFX";
   using create = pf::Create<extTurbulenceFX>;

   DOUBLE Gradient[GSIZE][LSIZE][GSUBSIZE];
   LONG Lattice[LSIZE];
   DOUBLE FX, FY;
   LONG Octaves;
   LONG Seed;
   TB Type;
   bool Stitch;

   private:
   LONG stitch_width, stitch_height;
   LONG wrap_x, wrap_y;

   public:

   DOUBLE noise2(UBYTE Channel, DOUBLE VX, DOUBLE VY) {
      LONG bx0, bx1, by0, by1, b00, b10, b01, b11;
      DOUBLE rx0, rx1, ry0, ry1, *q, sx, sy, a, b, t, u, v;

      t = VX + PerlinN;
      bx0 = (LONG)t;
      bx1 = bx0 + 1;
      rx0 = t - (LONG)t;
      rx1 = rx0 - 1.0;

      t = VY + PerlinN;
      by0 = (LONG)t;
      by1 = by0 + 1;
      ry0 = t - (LONG)t;
      ry1 = ry0 - 1.0;

      // If stitching, adjust lattice points accordingly.

      if (Stitch) {
         if (bx0 >= wrap_x) bx0 -= stitch_width;
         if (bx1 >= wrap_x) bx1 -= stitch_width;
         if (by0 >= wrap_y) by0 -= stitch_height;
         if (by1 >= wrap_y) by1 -= stitch_height;
      }

      bx0 &= BM;
      bx1 &= BM;
      by0 &= BM;
      by1 &= BM;
      LONG i = Lattice[bx0];
      LONG j = Lattice[bx1];
      b00 = Lattice[i + by0];
      b10 = Lattice[j + by0];
      b01 = Lattice[i + by1];
      b11 = Lattice[j + by1];
      sx = s_curve(rx0);
      sy = s_curve(ry0);
      q = Gradient[Channel][b00]; u = rx0 * q[0] + ry0 * q[1];
      q = Gradient[Channel][b10]; v = rx1 * q[0] + ry0 * q[1];
      a = lerp(sx, u, v);
      q = Gradient[Channel][b01]; u = rx0 * q[0] + ry1 * q[1];
      q = Gradient[Channel][b11]; v = rx1 * q[0] + ry1 * q[1];
      b = lerp(sx, u, v);
      return lerp(sy, a, b);
   }

   // Standard turbulence (non-stitched)

   UBYTE turbulence(UBYTE Channel, LONG x, LONG y) {
      DOUBLE sum = 0.0;
      DOUBLE vx = x * FX;
      DOUBLE vy = y * FY;
      DOUBLE ratio = 1.0;
      for (LONG n=0; n < Octaves; n++) {
         DOUBLE noise = noise2(Channel, vx, vy);
         if (Type IS TB::NOISE) sum += noise * ratio;
         else sum += fabs(noise) * ratio;
         vx *= 2.0;
         vy *= 2.0;
         ratio *= 0.5;
      }

      LONG col;
      if (Type IS TB::NOISE) col = ((sum * 255.0) + 255.0) * 0.5;
      else col = sum * 255.0;

      return (col < 0) ? 0 : (col > 255) ? 255 : col;
   }

   // Stitched turbulence

   UBYTE turbulence_stitch(UBYTE Channel, LONG x, LONG y, DOUBLE FX, DOUBLE FY, LONG StitchWidth, LONG StitchHeight) {
      wrap_x = (x % StitchWidth) * FX + PerlinN + stitch_width;
      wrap_y = (y % StitchHeight) * FY + PerlinN + stitch_height;

      DOUBLE sum = 0;
      DOUBLE vx = x * FX;
      DOUBLE vy = y * FY;
      DOUBLE ratio = 1;
      for (LONG n=0; n < Octaves; n++) {
         DOUBLE noise = noise2(Channel, vx, vy);

         if (Type IS TB::NOISE) sum += noise * ratio;
         else sum += fabs(noise) * ratio;

         vx *= 2;
         vy *= 2;
         ratio *= 0.5;
         // Update stitch values. Subtracting PerlinN before the multiplication and adding it
         // afterwards simplifies to subtracting it once.
         stitch_width  *= 2;
         wrap_x         = 2 * wrap_x - PerlinN;
         stitch_height *= 2;
         wrap_y         = 2 * wrap_y - PerlinN;
      }

      LONG col;
      if (Type IS TB::NOISE) col = ((sum * 255.0) + 255.0) * 0.5;
      else col = sum * 255.0;

      return (col < 0) ? 0 : (col > 255) ? 255 : col;
   }
};

//********************************************************************************************************************

inline LONG setup_seed(LONG lSeed)
{
  if (lSeed <= 0) lSeed = -(lSeed % (RAND_m - 1)) + 1;
  if (lSeed > RAND_m - 1) lSeed = RAND_m - 1;
  return lSeed;
}

inline LONG random(LONG lSeed)
{
  LONG result = RAND_a * (lSeed % RAND_q) - RAND_r * (lSeed / RAND_q);
  if (result <= 0) result += RAND_m;
  return result;
}

/*********************************************************************************************************************
-ACTION-
Draw: Render the effect to the target bitmap.
-END-
*********************************************************************************************************************/

static ERR TURBULENCEFX_Draw(extTurbulenceFX *Self, struct acDraw *Args)
{
   if (Self->Target->BytesPerPixel != 4) return ERR::Failed;

   const UBYTE A = Self->Target->ColourFormat->AlphaPos>>3;
   const UBYTE R = Self->Target->ColourFormat->RedPos>>3;
   const UBYTE G = Self->Target->ColourFormat->GreenPos>>3;
   const UBYTE B = Self->Target->ColourFormat->BluePos>>3;

   UBYTE *data = Self->Target->Data + (Self->Target->Clip.Left<<2) + (Self->Target->Clip.Top * Self->Target->LineWidth);

   const LONG height = Self->Target->Clip.Bottom - Self->Target->Clip.Top;
   const LONG width = Self->Target->Clip.Right - Self->Target->Clip.Left;

   if (Self->Stitch) {
      TClipRectangle<DOUBLE> bounds = { Self->Filter->ClientViewport->vpFixedWidth, Self->Filter->ClientViewport->vpFixedHeight, 0, 0 };
      calc_full_boundary(Self->Filter->ClientVector, bounds, false, false);
      const DOUBLE tile_width  = bounds.width();
      const DOUBLE tile_height = bounds.height();

      // When stitching tiled turbulence, the frequencies must be adjusted so that the tile borders will be continuous.

      auto fx = Self->FX;
      auto fy = Self->FY;

      if (fx != 0.0) {
         DOUBLE fLoFreq = DOUBLE(floor(tile_width * fx)) / tile_width;
         DOUBLE fHiFreq = DOUBLE(ceil(tile_width * fx)) / tile_width;
         if (fx / fLoFreq < fHiFreq / fx) fx = fLoFreq;
         else fx = fHiFreq;
      }

      if (fy != 0.0) {
         DOUBLE fLoFreq = DOUBLE(floor(tile_height * fy)) / tile_height;
         DOUBLE fHiFreq = DOUBLE(ceil(tile_height * fy)) / tile_height;
         if (fy / fLoFreq < fHiFreq / fy) fy = fLoFreq;
         else fy = fHiFreq;
      }

      auto stitch_width  = F2I(tile_width * fx);
      auto stitch_height = F2I(tile_height * fy);

      for (LONG y=0; y < height; y++) {
         UBYTE *pixel = data + (Self->Target->LineWidth * y);
         for (LONG x=0; x < width; x++, pixel += 4) {
            pixel[R] = glLinearRGB.invert(Self->turbulence_stitch(0, x, y, fx, fy, stitch_width, stitch_height));
            pixel[G] = glLinearRGB.invert(Self->turbulence_stitch(1, x, y, fx, fy, stitch_width, stitch_height));
            pixel[B] = glLinearRGB.invert(Self->turbulence_stitch(2, x, y, fx, fy, stitch_width, stitch_height));
            pixel[A] = Self->turbulence_stitch(3, x, y, fx, fy, stitch_width, stitch_height);
         }
      }
   }
   else {
      for (LONG y=0; y < height; y++) {
         UBYTE *pixel = data + (Self->Target->LineWidth * y);
         for (LONG x=0; x < width; x++, pixel += 4) {
            pixel[R] = glLinearRGB.invert(Self->turbulence(0, x, y));
            pixel[G] = glLinearRGB.invert(Self->turbulence(1, x, y));
            pixel[B] = glLinearRGB.invert(Self->turbulence(2, x, y));
            pixel[A] = Self->turbulence(3, x, y);
         }
      }
   }

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR TURBULENCEFX_Init(extTurbulenceFX *Self, APTR Void)
{
   LONG lSeed = setup_seed(Self->Seed);
   auto &gradient = Self->Gradient;
   auto &lattice  = Self->Lattice;

   LONG i;
   for (LONG k=0; k < GSIZE; k++) {
      for (i=0; i < BSIZE; i++) {
         lattice[i] = i;
         for (LONG j=0; j < 2; j++) {
            gradient[k][i][j] = (DOUBLE)(((lSeed = random(lSeed)) % (BSIZE + BSIZE)) - BSIZE) / BSIZE;
         }
         DOUBLE s = DOUBLE(sqrt(gradient[k][i][0] * gradient[k][i][0] + gradient[k][i][1] * gradient[k][i][1]));
         gradient[k][i][0] /= s;
         gradient[k][i][1] /= s;
      }
   }

   while (--i) {
      LONG j;
      auto tmp = lattice[i];
      lattice[i] = lattice[j = (lSeed = random(lSeed)) % BSIZE];
      lattice[j] = tmp;
   }

   for (i=0; i < BSIZE + 2; i++) {
      lattice[BSIZE + i] = lattice[i];
      for (LONG k=0; k < GSIZE; k++) {
         for (LONG j=0; j < GSUBSIZE; j++) {
            gradient[k][BSIZE + i][j] = gradient[k][i][j];
         }
      }
   }

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR TURBULENCEFX_NewObject(extTurbulenceFX *Self, APTR Void)
{
   Self->Octaves    = 1;
   Self->Stitch     = false;
   Self->Seed       = 0;
   Self->Type       = TB::TURBULENCE;
   Self->FX         = 0;
   Self->FY         = 0;
   Self->SourceType = VSF::NONE;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
FX: The base frequency for noise on the X axis.

A negative value for base frequency is an error.  The default value is zero.

*********************************************************************************************************************/

static ERR TURBULENCEFX_GET_FX(extTurbulenceFX *Self, DOUBLE *Value)
{
   *Value = Self->FX;
   return ERR::Okay;
}

static ERR TURBULENCEFX_SET_FX(extTurbulenceFX *Self, DOUBLE Value)
{
   if (Value >= 0) {
      Self->FX = Value;
      return ERR::Okay;
   }
   else return ERR::InvalidValue;
}

/*********************************************************************************************************************

-FIELD-
FY: The base frequency for noise on the Y axis.

A negative value for base frequency is an error.  The default value is zero.

*********************************************************************************************************************/

static ERR TURBULENCEFX_GET_FY(extTurbulenceFX *Self, DOUBLE *Value)
{
   *Value = Self->FY;
   return ERR::Okay;
}

static ERR TURBULENCEFX_SET_FY(extTurbulenceFX *Self, DOUBLE Value)
{
   if (Value >= 0) {
      Self->FY = Value;
      return ERR::Okay;
   }
   else return ERR::InvalidValue;
}

/*********************************************************************************************************************

-FIELD-
Octaves: The numOctaves parameter for the noise function.

Defaults to 1 if not specified.

*********************************************************************************************************************/

static ERR TURBULENCEFX_GET_Octaves(extTurbulenceFX *Self, LONG *Value)
{
   *Value = Self->Octaves;
   return ERR::Okay;
}

static ERR TURBULENCEFX_SET_Octaves(extTurbulenceFX *Self, LONG Value)
{
   Self->Octaves = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Seed: The starting number for the pseudo random number generator.

If the value is undefined, the effect is as if a value of 0 were specified.  When the seed number is handed over to
the algorithm it must first be truncated, i.e. rounded to the closest integer value towards zero.

*********************************************************************************************************************/

static ERR TURBULENCEFX_GET_Seed(extTurbulenceFX *Self, LONG *Value)
{
   *Value = Self->Seed;
   return ERR::Okay;
}

static ERR TURBULENCEFX_SET_Seed(extTurbulenceFX *Self, LONG Value)
{
   Self->Seed = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Stitch: If TRUE, stitching will be enabled at the tile's edges.

By default, the turbulence algorithm will sometimes show discontinuities at the tile borders.  If Stitch is set to
TRUE then the algorithm will automatically adjust base frequency values such that the node's width and height
(i.e., the width and height of the current subregion) contains an integral number of the Perlin tile width and height
for the first octave.

The baseFrequency will be adjusted up or down depending on which way has the smallest relative (not absolute) change
as follows:  Given the frequency, calculate `lowFreq = floor(width*frequency) / width` and
`hiFreq = ceil(width * frequency) / width`. If `frequency/lowFreq < hiFreq/frequency` then use lowFreq, else use
hiFreq.  While generating turbulence values, generate lattice vectors as normal for Perlin Noise, except for those
lattice  points that lie on the right or bottom edges of the active area (the size of the resulting tile).  In those
cases, copy the lattice vector from the opposite edge of the active area.

*********************************************************************************************************************/

static ERR TURBULENCEFX_GET_Stitch(extTurbulenceFX *Self, LONG *Value)
{
   *Value = Self->Stitch;
   return ERR::Okay;
}

static ERR TURBULENCEFX_SET_Stitch(extTurbulenceFX *Self, LONG Value)
{
   Self->Stitch = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Type: Can be set to 'noise' or 'turbulence'.


*********************************************************************************************************************/

static ERR TURBULENCEFX_GET_Type(extTurbulenceFX *Self, TB *Value)
{
   *Value = Self->Type;
   return ERR::Okay;
}

static ERR TURBULENCEFX_SET_Type(extTurbulenceFX *Self, TB Value)
{
   Self->Type = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
XMLDef: Returns an SVG compliant XML string that describes the effect.
-END-

*********************************************************************************************************************/

static ERR TURBULENCEFX_GET_XMLDef(extTurbulenceFX *Self, STRING *Value)
{
   std::stringstream stream;

   stream << "feTurbulence";

   *Value = StrClone(stream.str().c_str());
   return ERR::Okay;
}

//********************************************************************************************************************

#include "filter_turbulence_def.c"

static const FieldDef clTurbulenceType[] = {
   { "Turbulence", TB::TURBULENCE },
   { "Noise",      TB::NOISE },
   { NULL, 0 }
};
static const FieldArray clTurbulenceFXFields[] = {
   { "FX",      FDF_VIRTUAL|FDF_DOUBLE|FDF_RI,          TURBULENCEFX_GET_FX,      TURBULENCEFX_SET_FX },
   { "FY",      FDF_VIRTUAL|FDF_DOUBLE|FDF_RI,          TURBULENCEFX_GET_FY,      TURBULENCEFX_SET_FY },
   { "Octaves", FDF_VIRTUAL|FDF_LONG|FDF_RI,            TURBULENCEFX_GET_Octaves, TURBULENCEFX_SET_Octaves },
   { "Seed",    FDF_VIRTUAL|FDF_LONG|FDF_RI,            TURBULENCEFX_GET_Seed,    TURBULENCEFX_SET_Seed },
   { "Stitch",  FDF_VIRTUAL|FDF_LONG|FDF_RI,            TURBULENCEFX_GET_Stitch,  TURBULENCEFX_SET_Stitch },
   { "Type",    FDF_VIRTUAL|FDF_LONG|FDF_LOOKUP|FDF_RI, TURBULENCEFX_GET_Type,    TURBULENCEFX_SET_Type, &clTurbulenceType },
   { "XMLDef",  FDF_VIRTUAL|FDF_STRING|FDF_ALLOC|FDF_R, TURBULENCEFX_GET_XMLDef,  NULL },
   END_FIELD
};

//********************************************************************************************************************

ERR init_turbulencefx(void)
{
   clTurbulenceFX = objMetaClass::create::global(
      fl::BaseClassID(ID_FILTEREFFECT),
      fl::ClassID(ID_TURBULENCEFX),
      fl::Name("TurbulenceFX"),
      fl::Category(CCF::GRAPHICS),
      fl::Actions(clTurbulenceFXActions),
      fl::Fields(clTurbulenceFXFields),
      fl::Size(sizeof(extTurbulenceFX)),
      fl::Path(MOD_PATH));

   return clTurbulenceFX ? ERR::Okay : ERR::AddClass;
}
