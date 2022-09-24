
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

struct ttable {
   LONG Lattice[LSIZE];
   DOUBLE Gradient[GSIZE][LSIZE][GSUBSIZE];
};

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

class TurbulenceEffect : public VectorEffect {
   DOUBLE FX, FY;
   struct ttable *Tables;
   LONG Octaves;
   LONG Seed;
   LONG TileWidth, TileHeight;
   LONG StitchWidth, StitchHeight;
   LONG WrapX;
   LONG WrapY;
   UBYTE Type;
   bool Stitch;

   void xml(std::stringstream &Stream) { // TODO: Support exporting attributes
      Stream << "feTurbulence";
   }

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
         if (bx0 >= WrapX) bx0 -= StitchWidth;
         if (bx1 >= WrapX) bx1 -= StitchWidth;
         if (by0 >= WrapY) by0 -= StitchHeight;
         if (by1 >= WrapY) by1 -= StitchHeight;
      }

      bx0 &= BM;
      bx1 &= BM;
      by0 &= BM;
      by1 &= BM;
      LONG i = Tables->Lattice[bx0];
      LONG j = Tables->Lattice[bx1];
      b00 = Tables->Lattice[i + by0];
      b10 = Tables->Lattice[j + by0];
      b01 = Tables->Lattice[i + by1];
      b11 = Tables->Lattice[j + by1];
      sx = s_curve(rx0);
      sy = s_curve(ry0);
      q = Tables->Gradient[Channel][b00]; u = rx0 * q[0] + ry0 * q[1];
      q = Tables->Gradient[Channel][b10]; v = rx1 * q[0] + ry0 * q[1];
      a = lerp(sx, u, v);
      q = Tables->Gradient[Channel][b01]; u = rx0 * q[0] + ry1 * q[1];
      q = Tables->Gradient[Channel][b11]; v = rx1 * q[0] + ry1 * q[1];
      b = lerp(sx, u, v);
      return lerp(sy, a, b);
   }

   //****************************************************************************

   DOUBLE turbulence(UBYTE Channel, LONG x, LONG y) {
      DOUBLE sum = 0.0;
      DOUBLE vx = x * FX;
      DOUBLE vy = y * FY;
      DOUBLE ratio = 1.0;
      for (LONG n=0; n < Octaves; n++) {
         DOUBLE noise = noise2(Channel, vx, vy);
         if (Type IS TB_NOISE) sum += noise * ratio;
         else sum += fabs(noise) * ratio;
         vx *= 2.0;
         vy *= 2.0;
         ratio *= 0.5;
      }

      if (Type IS TB_NOISE) return ((sum * 255.0) + 255.0) * 0.5;
      else return sum * 255.0;
   }

   // When stitching tiled turbulence, the frequencies must be adjusted so that the tile borders will be continuous.

   DOUBLE turbulence_stitch(UBYTE Channel, LONG x, LONG y) {
      StitchWidth  = LONG(TileWidth * FX + 0.5);
      WrapX        = (x % TileWidth) * FX + PerlinN + StitchWidth;
      StitchHeight = LONG(TileHeight * FY + 0.5);
      WrapY        = (y % TileHeight) * FY + PerlinN + StitchHeight;

      DOUBLE sum = 0;
      DOUBLE vx = x * FX;
      DOUBLE vy = y * FY;
      DOUBLE ratio = 1;
      for (LONG n=0; n < Octaves; n++) {
         DOUBLE noise = noise2(Channel, vx, vy);

         if (Type IS TB_NOISE) sum += noise * ratio;
         else sum += fabs(noise) * ratio;

         vx *= 2;
         vy *= 2;
         ratio *= 0.5;
         // Update stitch values. Subtracting PerlinN before the multiplication and adding it
         // afterwards simplifies to subtracting it once.
         StitchWidth  *= 2;
         WrapX         = 2 * WrapX - PerlinN;
         StitchHeight *= 2;
         WrapY         = 2 * WrapY - PerlinN;
      }

      if (Type IS TB_NOISE) return ((sum * 255.0) + 255.0) * 0.5;
      else return sum * 255.0;
   }

public:
   TurbulenceEffect(struct rkVectorFilter *Filter, XMLTag *Tag) : VectorEffect() {
      parasol::Log log(__FUNCTION__);

      Octaves      = 1;
      Stitch       = false;
      Seed         = RandomNumber(1000000);
      Type         = 0; // Default type is 'turbulence'.  1 == Noise
      TileWidth    = 256;
      TileHeight   = 256;
      Tables       = NULL;
      StitchWidth  = 0;
      StitchHeight = 0;
      WrapX        = 0;
      WrapY        = 0;
      FX           = 0;
      FY           = 0;
      EffectName   = "feTurbulence";

      for (LONG a=1; a < Tag->TotalAttrib; a++) {
         CSTRING val = Tag->Attrib[a].Value;
         if (!val) continue;

         ULONG hash = StrHash(Tag->Attrib[a].Name, FALSE);
         switch(hash) {
            case SVF_BASEFREQUENCY: {
               FX = -1;
               FY = -1;
               read_numseq(val, &FX, &FY, TAGEND);
               if (FX < 0) FX = 0;
               if (FY < 0) FY = FX;
               break;
            }

            case SVF_NUMOCTAVES:
               Octaves = StrToInt(val);
               break;

            case SVF_SEED:
               Seed = StrToInt(val);
               break;

            case SVF_STITCHTILES:
               if (!StrMatch("stitch", val)) Stitch = true;
               else Stitch = false;
               break;

            case SVF_TYPE:
               if (!StrMatch("fractalNoise", val)) Type = TB_NOISE;
               else Type = 0;
               break;

            default:
               fe_default(Filter, this, hash, val);
               break;
         }
      }

      if (!AllocMemory(sizeof(struct ttable), MEM_DATA, &Tables, NULL)) {
         LONG i;
         LONG lSeed = setup_seed(Seed);
         auto &gradient = Tables->Gradient;
         auto &lattice  = Tables->Lattice;

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

         if (Stitch) {
            const DOUBLE fx = FX;
            const DOUBLE fy = FY;
            // When stitching tiled turbulence, the frequencies must be adjusted so that the tile borders will be continuous.
            if (fx != 0.0) {
               DOUBLE fLoFreq = DOUBLE(floor(TileWidth * fx)) / TileWidth;
               DOUBLE fHiFreq = DOUBLE(ceil(TileWidth * fx)) / TileWidth;
               if (fx / fLoFreq < fHiFreq / fx) FX = fLoFreq;
               else FX = fHiFreq;
            }

            if (fy != 0.0) {
               DOUBLE fLoFreq = DOUBLE(floor(TileHeight * fy)) / TileHeight;
               DOUBLE fHiFreq = DOUBLE(ceil(TileHeight * fy)) / TileHeight;
               if (fy / fLoFreq < fHiFreq / fy) FY = fLoFreq;
               else FY = fHiFreq;
            }
         }
      }
      else Error = ERR_AllocMemory;
   }

   void apply(objVectorFilter *Filter, filter_state &State) {
      if (OutBitmap->BytesPerPixel != 4) return;

      const UBYTE A = OutBitmap->ColourFormat->AlphaPos>>3;
      const UBYTE R = OutBitmap->ColourFormat->RedPos>>3;
      const UBYTE G = OutBitmap->ColourFormat->GreenPos>>3;
      const UBYTE B = OutBitmap->ColourFormat->BluePos>>3;

      UBYTE *data = OutBitmap->Data + (OutBitmap->Clip.Left<<2) + (OutBitmap->Clip.Top * OutBitmap->LineWidth);

      const LONG height = OutBitmap->Clip.Bottom - OutBitmap->Clip.Top;
      const LONG width = OutBitmap->Clip.Right - OutBitmap->Clip.Left;

      if (Stitch) {
         for (LONG y=0; y < height; y++) {
            UBYTE *pixel = data + (OutBitmap->LineWidth * y);
            for (LONG x=0; x < width; x++) {
               DOUBLE r = turbulence_stitch(0, x, y);
               DOUBLE g = turbulence_stitch(1, x, y);
               DOUBLE b = turbulence_stitch(2, x, y);
               DOUBLE a = turbulence_stitch(3, x, y);

               if (a < 0) pixel[A] = 0; else if (a > 255) pixel[A] = 255; else pixel[A] = a;
               if (r < 0) pixel[R] = 0; else if (r > 255) pixel[R] = 255; else pixel[R] = r;
               if (g < 0) pixel[G] = 0; else if (g > 255) pixel[G] = 255; else pixel[G] = g;
               if (b < 0) pixel[B] = 0; else if (b > 255) pixel[B] = 255; else pixel[B] = b;

               pixel += 4;
            }
         }
      }
      else {
         for (LONG y=0; y < height; y++) {
            UBYTE *pixel = data + (OutBitmap->LineWidth * y);
            for (LONG x=0; x < width; x++) {
               DOUBLE r = turbulence(0, x, y);
               DOUBLE g = turbulence(1, x, y);
               DOUBLE b = turbulence(2, x, y);
               DOUBLE a = turbulence(3, x, y);

               if (a < 0) pixel[A] = 0; else if (a > 255) pixel[A] = 255; else pixel[A] = a;
               if (r < 0) pixel[R] = 0; else if (r > 255) pixel[R] = 255; else pixel[R] = r;
               if (g < 0) pixel[G] = 0; else if (g > 255) pixel[G] = 255; else pixel[G] = g;
               if (b < 0) pixel[B] = 0; else if (b > 255) pixel[B] = 255; else pixel[B] = b;

               pixel += 4;
            }
         }
      }
   }

   virtual ~TurbulenceEffect() {
      if (Tables) FreeResource(Tables);
   }
};
