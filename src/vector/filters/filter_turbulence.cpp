
#define RAND_m 2147483647 // 2**31 - 1
#define RAND_a 16807      // 7**5; primitive root of m
#define RAND_q 127773     // m / a
#define RAND_r 2836       // m % a

#define BSIZE 0x100
#define BM 0xff
#define PerlinN 0x1000
#define NP 12 // 2^PerlinN
#define NM 0xfff

#define s_curve(t) (t * t * (3.0 - 2.0 * t))
#define lerp(t, a, b) (a + t * (b - a))

struct ttable {
   LONG Lattice[BSIZE + BSIZE + 2];
   DOUBLE Gradient[4][BSIZE + BSIZE + 2][2];
};

inline LONG setup_seed(LONG lSeed)
{
  if (lSeed <= 0) lSeed = -(lSeed % (RAND_m - 1)) + 1;
  if (lSeed > RAND_m - 1) lSeed = RAND_m - 1;
  return lSeed;
}

inline LONG random(LONG lSeed)
{
  LONG result;
  result = RAND_a * (lSeed % RAND_q) - RAND_r * (lSeed / RAND_q);
  if (result <= 0) result += RAND_m;
  return result;
}

//****************************************************************************

static DOUBLE noise2(VectorEffect *Effect, UBYTE Channel, DOUBLE vx, DOUBLE vy)
{
   LONG bx0, bx1, by0, by1, b00, b10, b01, b11;
   DOUBLE rx0, rx1, ry0, ry1, *q, sx, sy, a, b, t, u, v;

   t = vx + PerlinN;
   bx0 = (LONG)t;
   bx1 = bx0 + 1;
   rx0 = t - (LONG)t;
   rx1 = rx0 - 1.0;

   t = vy + PerlinN;
   by0 = (LONG)t;
   by1 = by0 + 1;
   ry0 = t - (LONG)t;
   ry1 = ry0 - 1.0;

   // If stitching, adjust lattice points accordingly.

   if (Effect->Turbulence.Stitch) {
      if (bx0 >= Effect->Turbulence.WrapX) bx0 -= Effect->Turbulence.StitchWidth;
      if (bx1 >= Effect->Turbulence.WrapX) bx1 -= Effect->Turbulence.StitchWidth;
      if (by0 >= Effect->Turbulence.WrapY) by0 -= Effect->Turbulence.StitchHeight;
      if (by1 >= Effect->Turbulence.WrapY) by1 -= Effect->Turbulence.StitchHeight;
   }

   bx0 &= BM;
   bx1 &= BM;
   by0 &= BM;
   by1 &= BM;
   LONG i = Effect->Turbulence.Tables->Lattice[bx0];
   LONG j = Effect->Turbulence.Tables->Lattice[bx1];
   b00 = Effect->Turbulence.Tables->Lattice[i + by0];
   b10 = Effect->Turbulence.Tables->Lattice[j + by0];
   b01 = Effect->Turbulence.Tables->Lattice[i + by1];
   b11 = Effect->Turbulence.Tables->Lattice[j + by1];
   sx = s_curve(rx0);
   sy = s_curve(ry0);
   q = Effect->Turbulence.Tables->Gradient[Channel][b00]; u = rx0 * q[0] + ry0 * q[1];
   q = Effect->Turbulence.Tables->Gradient[Channel][b10]; v = rx1 * q[0] + ry0 * q[1];
   a = lerp(sx, u, v);
   q = Effect->Turbulence.Tables->Gradient[Channel][b01]; u = rx0 * q[0] + ry1 * q[1];
   q = Effect->Turbulence.Tables->Gradient[Channel][b11]; v = rx1 * q[0] + ry1 * q[1];
   b = lerp(sx, u, v);
   return lerp(sy, a, b);
}

//****************************************************************************

static DOUBLE turbulence(VectorEffect *Effect, UBYTE Channel, LONG x, LONG y)
{
   DOUBLE sum = 0;
   DOUBLE vx = x * Effect->Turbulence.FX;
   DOUBLE vy = y * Effect->Turbulence.FY;
   DOUBLE ratio = 1;
   for (LONG n=0; n < Effect->Turbulence.Octaves; n++) {
      DOUBLE noise = noise2(Effect, Channel, vx, vy);
      if (Effect->Turbulence.Type IS 1) sum += noise * ratio;
      else sum += fabs(noise) * ratio;
      vx *= 2;
      vy *= 2;
      ratio *= 0.5;
   }

   if (Effect->Turbulence.Type IS 1) return ((sum * 255.0) + 255.0) * 0.5;
   else return sum * 255.0;
}

//****************************************************************************

static DOUBLE turbulence_stitch(VectorEffect *Effect, UBYTE Channel, LONG x, LONG y)
{
   // When stitching tiled turbulence, the frequencies must be adjusted so that the tile borders will be continuous.

   Effect->Turbulence.StitchWidth  = LONG(Effect->Turbulence.TileWidth * Effect->Turbulence.FX + 0.5);
   Effect->Turbulence.WrapX        = (x % Effect->Turbulence.TileWidth) * Effect->Turbulence.FX + PerlinN + Effect->Turbulence.StitchWidth;
   Effect->Turbulence.StitchHeight = LONG(Effect->Turbulence.TileHeight * Effect->Turbulence.FY + 0.5);
   Effect->Turbulence.WrapY        = (y % Effect->Turbulence.TileHeight) * Effect->Turbulence.FY + PerlinN + Effect->Turbulence.StitchHeight;

   DOUBLE sum = 0;
   DOUBLE vx = x * Effect->Turbulence.FX;
   DOUBLE vy = y * Effect->Turbulence.FY;
   DOUBLE ratio = 1;
   for (LONG n=0; n < Effect->Turbulence.Octaves; n++) {
      DOUBLE noise = noise2(Effect, Channel, vx, vy);

      if (Effect->Turbulence.Type IS 1) sum += noise * ratio;
      else sum += fabs(noise) * ratio;

      vx *= 2;
      vy *= 2;
      ratio *= 0.5;
      // Update stitch values. Subtracting PerlinN before the multiplication and adding it afterwards simplifies to
      // subtracting it once.
      Effect->Turbulence.StitchWidth  *= 2;
      Effect->Turbulence.WrapX         = 2 * Effect->Turbulence.WrapX - PerlinN;
      Effect->Turbulence.StitchHeight *= 2;
      Effect->Turbulence.WrapY         = 2 * Effect->Turbulence.WrapY - PerlinN;
   }

   if (Effect->Turbulence.Type IS 1) return ((sum * 255.0) + 255.0) * 0.5;
   else return sum * 255.0;
}

/*****************************************************************************
** Internal: apply_turbulence()
*/

static void apply_turbulence(objVectorFilter *Self, VectorEffect *Effect)
{
   objBitmap *bmp = Effect->Bitmap;
   if (bmp->BytesPerPixel != 4) return;

   const UBYTE A = bmp->ColourFormat->AlphaPos>>3;
   const UBYTE R = bmp->ColourFormat->RedPos>>3;
   const UBYTE G = bmp->ColourFormat->GreenPos>>3;
   const UBYTE B = bmp->ColourFormat->BluePos>>3;

   UBYTE *data = bmp->Data + (bmp->Clip.Left<<2) + (bmp->Clip.Top * bmp->LineWidth);

   const LONG height = bmp->Clip.Bottom - bmp->Clip.Top;
   const LONG width = bmp->Clip.Right - bmp->Clip.Left;

   if (Effect->Turbulence.Stitch) {
      for (LONG y=0; y < height; y++) {
         UBYTE *pixel = data + (bmp->LineWidth * y);
         for (LONG x=0; x < width; x++) {
            DOUBLE r = turbulence_stitch(Effect, 0, x, y);
            DOUBLE g = turbulence_stitch(Effect, 1, x, y);
            DOUBLE b = turbulence_stitch(Effect, 2, x, y);
            DOUBLE a = turbulence_stitch(Effect, 3, x, y);

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
         UBYTE *pixel = data + (bmp->LineWidth * y);
         for (LONG x=0; x < width; x++) {
            DOUBLE r = turbulence(Effect, 0, x, y);
            DOUBLE g = turbulence(Effect, 1, x, y);
            DOUBLE b = turbulence(Effect, 2, x, y);
            DOUBLE a = turbulence(Effect, 3, x, y);

            if (a < 0) pixel[A] = 0; else if (a > 255) pixel[A] = 255; else pixel[A] = a;
            if (r < 0) pixel[R] = 0; else if (r > 255) pixel[R] = 255; else pixel[R] = r;
            if (g < 0) pixel[G] = 0; else if (g > 255) pixel[G] = 255; else pixel[G] = g;
            if (b < 0) pixel[B] = 0; else if (b > 255) pixel[B] = 255; else pixel[B] = b;

            pixel += 4;
         }
      }
   }
}

//****************************************************************************
// Create a new turbulence matrix filter.

static ERROR create_turbulence(objVectorFilter *Self, XMLTag *Tag)
{
   VectorEffect *effect;
   if (!(effect = add_effect(Self, FE_TURBULENCE))) return ERR_AllocMemory;

   effect->Turbulence.Octaves = 1;
   effect->Turbulence.Stitch = FALSE;
   effect->Turbulence.Seed = RandomNumber(1000000);
   effect->Turbulence.Type = 0; // Default type is 'turbulence'.  1 == Noise
   effect->Turbulence.TileWidth = 256;
   effect->Turbulence.TileHeight = 256;

   for (LONG a=1; a < Tag->TotalAttrib; a++) {
      CSTRING val = Tag->Attrib[a].Value;
      if (!val) continue;

      ULONG hash = StrHash(Tag->Attrib[a].Name, FALSE);
      switch(hash) {
         case SVF_BASEFREQUENCY: {
            DOUBLE x = -1, y = -1;
            read_numseq(val, &x, &y, TAGEND);
            if (x >= 0) effect->Turbulence.FX = x;
            else effect->Turbulence.FX = 0;

            if (y >= 0) effect->Turbulence.FY = y;
            else effect->Turbulence.FY = x;
            break;
         }

         case SVF_NUMOCTAVES:
            effect->Turbulence.Octaves = StrToInt(val);
            break;

         case SVF_SEED:
            effect->Turbulence.Seed = StrToInt(val);
            break;

         case SVF_STITCHTILES:
            if (!StrMatch("stitch", val)) effect->Turbulence.Stitch = TRUE;
            else effect->Turbulence.Stitch = FALSE;
            break;

         case SVF_TYPE:
            if (!StrMatch("fractalNoise", val)) effect->Turbulence.Type = 1;
            else effect->Turbulence.Type = 0;
            break;

         default:
            fe_default(Self, effect, hash, val);
            break;
      }
   }

   if (!AllocMemory(sizeof(struct ttable), MEM_DATA|MEM_NO_CLEAR, &effect->Turbulence.Tables, NULL)) {
      LONG i, j, k;
      LONG lSeed = setup_seed(effect->Turbulence.Seed);
      for (k=0; k < 4; k++) {
         for (i=0; i < BSIZE; i++) {
            effect->Turbulence.Tables->Lattice[i] = i;
            for (j=0; j < 2; j++) effect->Turbulence.Tables->Gradient[k][i][j] = (DOUBLE)(((lSeed = random(lSeed)) % (BSIZE + BSIZE)) - BSIZE) / BSIZE;
            DOUBLE s = DOUBLE(sqrt(effect->Turbulence.Tables->Gradient[k][i][0] * effect->Turbulence.Tables->Gradient[k][i][0] + effect->Turbulence.Tables->Gradient[k][i][1] * effect->Turbulence.Tables->Gradient[k][i][1]));
            effect->Turbulence.Tables->Gradient[k][i][0] /= s;
            effect->Turbulence.Tables->Gradient[k][i][1] /= s;
         }
      }

      while (--i) {
        k = effect->Turbulence.Tables->Lattice[i];
        effect->Turbulence.Tables->Lattice[i] = effect->Turbulence.Tables->Lattice[j = (lSeed = random(lSeed)) % BSIZE];
        effect->Turbulence.Tables->Lattice[j] = k;
      }

      for (i=0; i < BSIZE + 2; i++) {
         effect->Turbulence.Tables->Lattice[BSIZE + i] = effect->Turbulence.Tables->Lattice[i];
         for (k = 0; k < 4; k++)
         for (j = 0; j < 2; j++) effect->Turbulence.Tables->Gradient[k][BSIZE + i][j] = effect->Turbulence.Tables->Gradient[k][i][j];
      }

      if (effect->Turbulence.Stitch) {
         if (effect->Turbulence.Stitch) {
            const DOUBLE fx = effect->Turbulence.FX;
            const DOUBLE fy = effect->Turbulence.FY;
            // When stitching tiled turbulence, the frequencies must be adjusted so that the tile borders will be continuous.
            if (fx != 0.0) {
               DOUBLE fLoFreq = DOUBLE(floor(effect->Turbulence.TileWidth * fx)) / effect->Turbulence.TileWidth;
               DOUBLE fHiFreq = DOUBLE(ceil(effect->Turbulence.TileWidth * fx)) / effect->Turbulence.TileWidth;
               if (fx / fLoFreq < fHiFreq / fx) effect->Turbulence.FX = fLoFreq;
               else effect->Turbulence.FX = fHiFreq;
            }

            if (fy != 0.0) {
               DOUBLE fLoFreq = DOUBLE(floor(effect->Turbulence.TileHeight * fy)) / effect->Turbulence.TileHeight;
               DOUBLE fHiFreq = DOUBLE(ceil(effect->Turbulence.TileHeight * fy)) / effect->Turbulence.TileHeight;
               if (fy / fLoFreq < fHiFreq / fy) effect->Turbulence.FY = fLoFreq;
               else effect->Turbulence.FY = fHiFreq;
            }
         }
      }

      return ERR_Okay;
   }
   else {
      remove_effect(Self, effect);
      return ERR_AllocMemory;
   }
}
