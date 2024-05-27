/*********************************************************************************************************************

The light-source rendering code is copyright 2012 The Android Open Source Project.  The use of that source code is
governed as follows.

Copyright (c) 2011 Google Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
following conditions are met:

* Redistributions of source code must retain the above copyright  notice, this list of conditions and the following
disclaimer.

* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
following disclaimer in the documentation and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

**********************************************************************************************************************

-CLASS-
LightingFX: Enables the application of lighting effects.

The lighting effect class applies a diffuse or specular lighting effect to the alpha channel of an input bitmap, which
functions as a bump map.  The output is an RGBA representation of the light effect.  If no light #Colour is specified
by the client then the output will be in grey scale.

For diffuse lighting, the resulting RGBA image is computed as follows:

<pre>
Dr = kd * N.L * Lr
Dg = kd * N.L * Lg
Db = kd * N.L * Lb
Da = 1.0
</pre>

where

<pre>
kd = Diffuse lighting constant
N  = Surface normal unit vector, a function of x and y
L  = Unit vector pointing from surface to light, a function of x and y in the point and spot light cases
Lr,Lg,Lb = RGB components of light, a function of x and y in the spot light case
</pre>

For specular lighting, the resulting RGBA image is computed as follows:

<pre>
Sr = ks * pow(N.H, specularExponent) * Lr
Sg = ks * pow(N.H, specularExponent) * Lg
Sb = ks * pow(N.H, specularExponent) * Lb
Sa = max(Sr, Sg, Sb)
</pre>

where

<pre>
ks = Specular lighting constant
N  = Surface normal unit vector, a function of x and y
H  = "Halfway" unit vector between eye unit vector and light unit vector

Lr,Lg,Lb = RGB components of light
</pre>

The definition of `H` reflects our assumption of the constant eye vector `E = (0,0,1)`:

<pre>
H = (L + E) / Norm(L + E)
</pre>

where `L` is the light unit vector.

-END-

*********************************************************************************************************************/

class point3 {
   public:
   DOUBLE  x, y, z;

   point3(DOUBLE X, DOUBLE Y, DOUBLE Z) : x(X), y(Y), z(Z) {};
   point3() {};

   void normalize() {
      DOUBLE scale = 1.0 / (sqrt(dot(*this)));
      x = x * scale;
      y = y * scale;
      z = z * scale;
   }

   friend point3 operator-(const point3& a, const point3& b) {
      return { a.x - b.x, a.y - b.y, a.z - b.z };
   }

   friend point3 operator+(const point3& a, const point3& b) {
      return { a.x + b.x, a.y + b.y, a.z + b.z };
   }

   DOUBLE dot(const point3& vec) const {
      return (this->x * vec.x) + (this->y * vec.y) + (this->z * vec.z);
   }
};

static const DOUBLE ONE_THIRD   = 1.0 / 3.0;
static const DOUBLE TWO_THIRDS  = 2.0 / 3.0;
static const DOUBLE ONE_HALF    = 0.5;
static const DOUBLE ONE_QUARTER = 0.25;

// Shift matrix components to the left, as we advance pixels to the right.

inline void shiftMatrixLeft(UBYTE m[9]) {
    m[0] = m[1];
    m[3] = m[4];
    m[6] = m[7];
    m[1] = m[2];
    m[4] = m[5];
    m[7] = m[8];
}

//********************************************************************************************************************

inline DOUBLE sobel(UBYTE a, UBYTE b, UBYTE c, UBYTE d, UBYTE e, UBYTE f, DOUBLE Scale) {
   return (DOUBLE(-a + b - 2 * c + 2 * d -e + f)) * Scale;
}

inline point3 pointToNormal(DOUBLE x, DOUBLE y, DOUBLE Scale) {
   point3 vector(-x * Scale, -y * Scale, 1.0);
   vector.normalize();
   return vector;
}

inline point3 leftNormal(const UBYTE m[9], const DOUBLE Scale) {
   return pointToNormal(sobel(m[1], m[2], m[4], m[5], m[7], m[8], ONE_HALF),
                        sobel(0, 0, m[1], m[7], m[2], m[8], ONE_THIRD),
                        Scale);
}


inline point3 interiorNormal(const UBYTE m[9], const DOUBLE Scale) {
   return pointToNormal(sobel(m[0], m[2], m[3], m[5], m[6], m[8], ONE_QUARTER),
                        sobel(m[0], m[6], m[1], m[7], m[2], m[8], ONE_QUARTER),
                        Scale);
}

inline point3 rightNormal(const UBYTE m[9], const DOUBLE Scale) {
    return pointToNormal(sobel(m[0], m[1], m[3], m[4], m[6], m[7], ONE_HALF),
                         sobel(m[0], m[6], m[1], m[7], 0, 0, ONE_THIRD),
                         Scale);
}

//********************************************************************************************************************

class extLightingFX : public extFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::LIGHTINGFX;
   static constexpr CSTRING CLASS_NAME = "LightingFX";
   using create = pf::Create<extLightingFX>;

   FRGB   Colour;           // Colour of the light source.
   FRGB   LinearColour;     // Colour of the light source in linear sRGB space.
   DOUBLE SpecularExponent; // Exponent value for specular lighting only.
   DOUBLE Scale;            // Maximum height of the surface for bump map calculations.
   DOUBLE Constant;         // The ks/kd constant value for the light mode.
   DOUBLE UnitX, UnitY;     // SVG kernel unit - scale value for X/Y
   DOUBLE X, Y, Z;          // Position of light source.
   LT     Type;             // Diffuse or Specular light scattering
   LS     LightSource;      // Light source identifier, recorded for SVG output purposes only.

   // DISTANT LIGHT
   DOUBLE Azimuth, Elevation;    // Distant light
   point3 Direction;             // Pre-calculated value for distant light.

   // SPOT LIGHT
   DOUBLE SpotExponent, ConeAngle; // Spot light
   DOUBLE PX, PY, PZ;              // Position of spot light source.
   DOUBLE CosInnerConeAngle;
   DOUBLE CosOuterConeAngle;
   DOUBLE ConeScale;
   point3 SpotDelta;

   extLightingFX() {
      SpecularExponent = 1.0;
      Colour   = { 1.0, 1.0, 1.0, 1.0 };
      LinearColour = { 1.0, 1.0, 1.0, 1.0 };
      Type     = LT::DIFFUSE;
      Constant = 1.0;
      Scale    = 1.0;
      UnitX    = 1.0;
      UnitY    = 1.0;
   }
};

//********************************************************************************************************************
// For point & spot light.

static point3 read_light_delta(extLightingFX *Self, DOUBLE X, DOUBLE Y, DOUBLE SZ, UBYTE Z)
{
   // The incoming Z value is in alpha, so is scaled to 0 - 1.0.
   point3 direction(X, Y, SZ - (DOUBLE(Z) * (1.0 / 255.0) * Self->Scale));
   direction.normalize();
   return direction;
}

//********************************************************************************************************************
// Colour computation for spot light.  Resulting RGB values are 0 - 1.0

static FRGB colour_spot_light(extLightingFX *Self, point3 &Point)
{
   if (Self->ConeAngle) {
      DOUBLE cosAngle = -Point.dot(Self->SpotDelta);
      if (cosAngle < Self->CosOuterConeAngle) return FRGB(0.0, 0.0, 0.0, 1.0);

      DOUBLE scale = pow(cosAngle, Self->SpotExponent);
      if (cosAngle < Self->CosInnerConeAngle) {
         scale = scale * (cosAngle - Self->CosOuterConeAngle);
         scale *= Self->ConeScale;
      }
      FRGB result(Self->LinearColour.Red * scale, Self->LinearColour.Green * scale, Self->LinearColour.Blue * scale, Self->LinearColour.Alpha * scale);
      return result;
   }
   else {
      DOUBLE scale = pow(-Point.dot(Self->SpotDelta), Self->SpotExponent);
      FRGB result(Self->LinearColour.Red * scale, Self->LinearColour.Green * scale, Self->LinearColour.Blue * scale, Self->LinearColour.Alpha * scale);
      return result;
   }
}

//********************************************************************************************************************
// Specular/Diffuse drawing functions.

static void diffuse_light(extLightingFX *Self, const point3 &Normal, const point3 &STL, const FRGB &Colour, UBYTE *Output, UBYTE R, UBYTE G, UBYTE B, UBYTE A)
{
   DOUBLE scale = (Self->Constant * Normal.dot(STL)) * 255.0;
   if (scale < 0) scale = 0;
   else if (scale > 255.0) scale = 255.0;

   Output[R] = glLinearRGB.invert(F2T(Colour.Red * scale));
   Output[G] = glLinearRGB.invert(F2T(Colour.Green * scale));
   Output[B] = glLinearRGB.invert(F2T(Colour.Blue * scale));
   Output[A] = 255;
}

static void specular_light(extLightingFX *Self, const point3 &Normal, const point3 &STL, const FRGB &Colour, UBYTE *Output, UBYTE R, UBYTE G, UBYTE B, UBYTE A)
{
   point3 halfDir(STL);
   halfDir.z += 1.0; // Eye position is always (0, 0, 1)
   halfDir.normalize();

   DOUBLE scale = (Self->Constant * std::pow(Normal.dot(halfDir), Self->SpecularExponent)) * 255.0;
   if (scale < 0) scale = 0;
   else if (scale > 255.0) scale = 255.0;

   const UBYTE r = F2T(Colour.Red * scale);
   const UBYTE g = F2T(Colour.Green * scale);
   const UBYTE b = F2T(Colour.Blue * scale);

   Output[R] = glLinearRGB.invert(r);
   Output[G] = glLinearRGB.invert(g);
   Output[B] = glLinearRGB.invert(b);
   if (Self->LightSource IS LS::DISTANT) {
      // Alpha is chosen from the max of the linear R,G,B light value
      Output[A] = Output[R] > Output[G] ? (Output[R] > Output[B] ? Output[R] : Output[B]) : (Output[G] > Output[B] ? Output[G] : Output[B]);
   }
   else Output[A] = r > g ? (r > b ? r : b) : (g > b ? g : b);
}

/*********************************************************************************************************************
-ACTION-
Draw: Render the effect to the target bitmap.
-END-
*********************************************************************************************************************/

static ERR LIGHTINGFX_Draw(extLightingFX *Self, struct acDraw *Args)
{
   pf::Log log;

   if (Self->Target->BytesPerPixel != 4) return log.warning(ERR::InvalidState);

   auto lt = point3(Self->X, Self->Y, Self->Z); // Light source
   auto pt = point3(Self->PX, Self->PY, Self->PZ); // Target for the light source

   if (Self->Filter->PrimitiveUnits IS VUNIT::BOUNDING_BOX) {
      // Light source coordinates are expressed as relative to the client vector's bounding box in this mode.
      auto &client = Self->Filter->ClientVector;
      const DOUBLE c_width  = client->Bounds.width();
      const DOUBLE c_height = client->Bounds.height();

      lt.x = (lt.x * c_width) + client->Bounds.left;
      lt.y = (lt.y * c_height) + client->Bounds.top;
      lt.z = lt.z * sqrt((c_width * c_width) + (c_height * c_height)) * 0.70710678118654752440084436210485;

      if (Self->LightSource IS LS::SPOT) {
         pt.x = (pt.x * c_width) + client->Bounds.left;
         pt.y = (pt.y * c_height) + client->Bounds.top;
         pt.z = pt.z * sqrt((c_width * c_width) + (c_height * c_height)) * 0.70710678118654752440084436210485;
      }
   }

   auto &t = Self->Filter->ClientVector->Transform;
   t.transform(&lt.x, &lt.y);

   // The Z axis is affected by scaling only.  Compute this according to SVG rules.
   const DOUBLE sz = (t.sx IS t.sy) ? t.sx : sqrt((t.sx*t.sx) + (t.sy*t.sy)) * 0.70710678118654752440084436210485;
   lt.z *= sz;

   // Rendering algorithm requires light source coordinates to be relative to the exposed bitmap.

   lt.x -= Self->Target->Clip.Left;
   lt.y -= Self->Target->Clip.Top;

   if (Self->LightSource IS LS::SPOT) {
      t.transform(&pt.x, &pt.y);
      pt.x -= Self->Target->Clip.Left;
      pt.y -= Self->Target->Clip.Top;
      pt.z *= sz;

      Self->SpotDelta = point3(pt.x, pt.y, pt.z) - point3(lt.x, lt.y, lt.z);
      Self->SpotDelta.normalize();

      if (Self->ConeAngle) {
         Self->CosOuterConeAngle = cos(Self->ConeAngle * DEG2RAD);
         const DOUBLE AA_THRESHOLD = 0.016;
         Self->CosInnerConeAngle = Self->CosOuterConeAngle + AA_THRESHOLD;
         Self->ConeScale = 1.0 / AA_THRESHOLD;
      }
   }

   objBitmap *bmp;
   if (get_source_bitmap(Self->Filter, &bmp, Self->SourceType, Self->Input, false) != ERR::Okay) return ERR::Failed;
   
   // Note! Linear conversion of the source bitmap is unnecessary because only the alpha channel is used.

   // The alpha channel of the source bitmap will function as the Z value for the bump map.  The RGB components
   // are ignored for input purposes.
   
   const UBYTE R = Self->Target->ColourFormat->RedPos>>3;
   const UBYTE G = Self->Target->ColourFormat->GreenPos>>3;
   const UBYTE B = Self->Target->ColourFormat->BluePos>>3;
   const UBYTE A = Self->Target->ColourFormat->AlphaPos>>3;

   LONG width  = Self->Target->Clip.Right - Self->Target->Clip.Left;
   LONG height = Self->Target->Clip.Bottom - Self->Target->Clip.Top;
   if (bmp->Clip.Right - bmp->Clip.Left < width) width = bmp->Clip.Right - bmp->Clip.Left;
   if (bmp->Clip.Bottom - bmp->Clip.Top < height) height = bmp->Clip.Bottom - bmp->Clip.Top;

   const UBYTE bpp = Self->Target->BytesPerPixel;
   UBYTE *in = (UBYTE *)(bmp->Data + (bmp->Clip.Left * bpp) + (bmp->Clip.Top * bmp->LineWidth));
   UBYTE *dest = (UBYTE *)(Self->Target->Data + (Self->Target->Clip.Left * bpp) + (Self->Target->Clip.Top * Self->Target->LineWidth));
   UBYTE *dptr;

   UBYTE m[9];
   const DOUBLE scale = Self->Scale * (1.0 / 255.0); // Adjust to match the scale of alpha values.

   if (Self->Type IS LT::DIFFUSE) {
      for (LONG y=0; y < height; y++) {
         const UBYTE *row0 = (y IS 0) ? in : in - bmp->LineWidth;
         const UBYTE *row1 = in;
         const UBYTE *row2 = (y IS height-1) ? in : in + bmp->LineWidth;

         dptr = dest;
         m[1] = row0[A]; row0 += bpp;
         m[2] = row0[A]; row0 += bpp;
         m[4] = row1[A]; row1 += bpp;
         m[5] = row1[A]; row1 += bpp;
         m[7] = row2[A]; row2 += bpp;
         m[8] = row2[A]; row2 += bpp;

         if (Self->LightSource IS LS::DISTANT) { // Diffuse distant light
            diffuse_light(Self, leftNormal(m, scale), Self->Direction, Self->LinearColour, dptr, R, G, B, A);
            dptr += bpp;

            for (LONG x=1; x < width-1; ++x) {
                shiftMatrixLeft(m);
                m[2] = row0[A]; row0 += bpp;
                m[5] = row1[A]; row1 += bpp;
                m[8] = row2[A]; row2 += bpp;
                diffuse_light(Self, interiorNormal(m, scale), Self->Direction, Self->LinearColour, dptr, R, G, B, A);
                dptr += bpp;
            }

            shiftMatrixLeft(m);
            diffuse_light(Self, rightNormal(m, scale), Self->Direction, Self->LinearColour, dptr, R, G, B, A);
         }
         else if (Self->LightSource IS LS::SPOT) { // Diffuse spot light
            point3 stl = read_light_delta(Self, lt.x, lt.y - DOUBLE(y), lt.z, m[4]);
            diffuse_light(Self, leftNormal(m, scale), stl, colour_spot_light(Self, stl), dptr, R, G, B, A);
            dptr += bpp;

            for (LONG x=1; x < width-1; ++x) {
                shiftMatrixLeft(m);
                m[2] = row0[A]; row0 += bpp;
                m[5] = row1[A]; row1 += bpp;
                m[8] = row2[A]; row2 += bpp;
                stl = read_light_delta(Self, lt.x - DOUBLE(x), lt.y - DOUBLE(y), lt.z, m[4]);
                diffuse_light(Self, interiorNormal(m, scale), stl, colour_spot_light(Self, stl), dptr, R, G, B, A);
                dptr += bpp;
            }

            shiftMatrixLeft(m);
            stl = read_light_delta(Self, lt.x - DOUBLE(width-1), lt.y - DOUBLE(y), lt.z, m[4]);
            diffuse_light(Self, rightNormal(m, scale), stl, colour_spot_light(Self, stl), dptr, R, G, B, A);
         }
         else { // Diffuse point light
            point3 stl = read_light_delta(Self, lt.x, lt.y - DOUBLE(y), lt.z, m[4]);
            diffuse_light(Self, leftNormal(m, scale), stl, Self->LinearColour, dptr, R, G, B, A);
            dptr += bpp;

            for (LONG x=1; x < width-1; ++x) {
                shiftMatrixLeft(m);
                m[2] = row0[A]; row0 += bpp;
                m[5] = row1[A]; row1 += bpp;
                m[8] = row2[A]; row2 += bpp;
                stl = read_light_delta(Self, lt.x - DOUBLE(x), lt.y - DOUBLE(y), lt.z, m[4]);
                diffuse_light(Self, interiorNormal(m, scale), stl, Self->LinearColour, dptr, R, G, B, A);
                dptr += bpp;
            }

            shiftMatrixLeft(m);
            stl = read_light_delta(Self, lt.x - DOUBLE(width-1), lt.y - DOUBLE(y), lt.z, m[4]);
            diffuse_light(Self, rightNormal(m, scale), stl, Self->LinearColour, dptr, R, G, B, A);
         }

         dptr += bpp;

         in   += bmp->LineWidth;
         dest += Self->Target->LineWidth;
      }
   }
   else { // SPECULAR
      for (LONG y=0; y < height; y++) {
         const UBYTE *row0 = (y IS 0) ? in : in - bmp->LineWidth;
         const UBYTE *row1 = in;
         const UBYTE *row2 = (y IS height-1) ? in : in + bmp->LineWidth;

         dptr = dest;
         m[1] = row0[A]; row0 += bpp;
         m[2] = row0[A]; row0 += bpp;
         m[4] = row1[A]; row1 += bpp;
         m[5] = row1[A]; row1 += bpp;
         m[7] = row2[A]; row2 += bpp;
         m[8] = row2[A]; row2 += bpp;

         if (Self->LightSource IS LS::DISTANT) { // Specular distant light
            specular_light(Self, leftNormal(m, scale), Self->Direction, Self->LinearColour, dptr, R, G, B, A);
            dptr += bpp;

            for (LONG x=1; x < width-1; ++x) {
                shiftMatrixLeft(m);
                m[2] = row0[A]; row0 += bpp;
                m[5] = row1[A]; row1 += bpp;
                m[8] = row2[A]; row2 += bpp;
                specular_light(Self, interiorNormal(m, scale), Self->Direction, Self->LinearColour, dptr, R, G, B, A);
                dptr += bpp;
            }

            shiftMatrixLeft(m);
            specular_light(Self, rightNormal(m, scale), Self->Direction, Self->LinearColour, dptr, R, G, B, A);
         }
         else if (Self->LightSource IS LS::SPOT) { // Specular spot light
            point3 stl = read_light_delta(Self, lt.x, lt.y - DOUBLE(y), lt.z, m[4]);
            specular_light(Self, leftNormal(m, scale), stl, colour_spot_light(Self, stl), dptr, R, G, B, A);
            dptr += bpp;

            for (LONG x=1; x < width-1; ++x) {
                shiftMatrixLeft(m);
                m[2] = row0[A]; row0 += bpp;
                m[5] = row1[A]; row1 += bpp;
                m[8] = row2[A]; row2 += bpp;
                stl = read_light_delta(Self, lt.x - DOUBLE(x), lt.y - DOUBLE(y), lt.z, m[4]);
                specular_light(Self, interiorNormal(m, scale), stl, colour_spot_light(Self, stl), dptr, R, G, B, A);
                dptr += bpp;
            }

            shiftMatrixLeft(m);
            stl = read_light_delta(Self, lt.x - DOUBLE(width-1), lt.y - DOUBLE(y), lt.z, m[4]);
            specular_light(Self, rightNormal(m, scale), stl, colour_spot_light(Self, stl), dptr, R, G, B, A);
         }
         else { // LS::POINT Specular point light
            point3 stl = read_light_delta(Self, lt.x, lt.y - DOUBLE(y), lt.z, m[4]);
            specular_light(Self, leftNormal(m, scale), stl, Self->LinearColour, dptr, R, G, B, A);
            dptr += bpp;

            for (LONG x=1; x < width-1; ++x) {
                shiftMatrixLeft(m);
                m[2] = row0[A]; row0 += bpp;
                m[5] = row1[A]; row1 += bpp;
                m[8] = row2[A]; row2 += bpp;
                stl = read_light_delta(Self, lt.x - DOUBLE(x), lt.y - DOUBLE(y), lt.z, m[4]);
                specular_light(Self, interiorNormal(m, scale), stl, Self->LinearColour, dptr, R, G, B, A);
                dptr += bpp;
            }

            shiftMatrixLeft(m);
            stl = read_light_delta(Self, lt.x - DOUBLE(width-1), lt.y - DOUBLE(y), lt.z, m[4]);
            specular_light(Self, rightNormal(m, scale), stl, Self->LinearColour, dptr, R, G, B, A);
         }

         dptr += bpp;

         in   += bmp->LineWidth;
         dest += Self->Target->LineWidth;
      }
   }

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR LIGHTINGFX_Free(extLightingFX *Self)
{
   Self->~extLightingFX();
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR LIGHTINGFX_NewObject(extLightingFX *Self)
{
   new (Self) extLightingFX;
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
SetDistantLight: Configure lighting with a distant light source.

This method applies a distant light configuration to the lighting effect.  It will override any previously defined
light source setting.

A distant light can be thought of as like the light from the sun.  An infinite amount of parallel light rays travel
in the direction that the distant light points to.  Distant lights are handy when you want equal illumination on
objects in a scene.

-INPUT-
double Azimuth: Direction angle for the light source on the XY plane (clockwise), in degrees from the X axis.
double Elevation: Direction angle for the light source from the XY plane towards the Z axis, in degrees.  A positive value points towards the viewer of the content.

-RESULT-
Okay:
Args:
NullArgs:

-END-

*********************************************************************************************************************/

static ERR LIGHTINGFX_SetDistantLight(extLightingFX *Self, struct ltSetDistantLight *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);

   Self->Azimuth     = Args->Azimuth;
   Self->Elevation   = Args->Elevation;
   Self->LightSource = LS::DISTANT;
   Self->Direction   = point3(cos(Self->Azimuth * DEG2RAD) * cos(Self->Elevation * DEG2RAD), sin(Self->Azimuth * DEG2RAD) * cos(Self->Elevation * DEG2RAD), sin(Self->Elevation * DEG2RAD));
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
SetPointLight: Configure lighting with a pointed light source.

This method applies a pointed light configuration to the lighting effect.  It will override any previously defined
light source setting.

A point light sends light out from the specified (X, Y, Z) location equally in all directions.  A light bulb or open
flame is a good example of a point light.  The intensity of the light can be controlled by altering the alpha
component of the light #Colour.

-INPUT-
double X: X location for the light source.
double Y: Y location for the light source.
double Z: Z location for the light source.

-RESULT-
Okay:
Args:
NullArgs:

-END-

*********************************************************************************************************************/

static ERR LIGHTINGFX_SetPointLight(extLightingFX *Self, struct ltSetPointLight *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);

   log.function("Source: %.2fx%.2fx%.2f", Args->X, Args->Y, Args->Z);

   Self->LightSource = LS::POINT;

   Self->X = Args->X;
   Self->Y = Args->Y;
   Self->Z = Args->Z;

   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
SetSpotLight: Configure lighting with a spot light source.

This method applies a spot light configuration to the lighting effect.  It will override any previously defined
light source setting.

A spot light beams light rays from the defined (X, Y, Z) position to the (PX, PY, PZ) position.  The Exponent and
ConeAngle work together to constrain the edge of the light projection.

-INPUT-
double X: X location for the light source.
double Y: Y location for the light source.
double Z: Z location for the light source.  The positive Z-axis comes out towards the person viewing the content and assuming that one unit along the Z-axis equals one unit in X and Y.
double PX: X location of the light source's target.
double PY: Y location of the light source's target.
double PZ: Z location of the light source's target.
double Exponent: Exponent value controlling the focus for the light source.
double ConeAngle: A limiting cone which restricts the region where the light is projected, or 0 to disable.  Specified in degrees.

-RESULT-
Okay:
Args:
NullArgs:

-END-

*********************************************************************************************************************/

static ERR LIGHTINGFX_SetSpotLight(extLightingFX *Self, struct ltSetSpotLight *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);

   log.function("Source: %.2fx%.2fx%.2f, Target: %.2fx%.2fx%.2f, Exp: %.2f, Cone Angle: %.2f", Args->X, Args->Y, Args->Z, Args->PX, Args->PY, Args->PZ, Args->Exponent, Args->ConeAngle);

   Self->LightSource = LS::SPOT;

   Self->X  = Args->X;
   Self->Y  = Args->Y;
   Self->Z  = Args->Z;
   Self->PX = Args->PX;
   Self->PY = Args->PY;
   Self->PZ = Args->PZ;

   Self->SpotExponent = Args->Exponent;
   Self->ConeAngle    = Args->ConeAngle;

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Colour: Defines the colour of the light source.

Set the Colour field to define the colour of the light source.  The colour is defined as an array of four 32-bit
floating point values between 0 and 1.0.  The array elements consist of Red, Green, Blue and Alpha values in that
order.

If the algorithm supports it, the Alpha component defines the intensity of the light source.

The default colour is pure white, `1.0,1.0,1.0,1.0`.

*********************************************************************************************************************/

static ERR LIGHTINGFX_GET_Colour(extLightingFX *Self, FLOAT **Value, LONG *Elements)
{
   *Value = (FLOAT *)&Self->Colour;
   *Elements = 4;
   return ERR::Okay;
}

static ERR LIGHTINGFX_SET_Colour(extLightingFX *Self, FLOAT *Value, LONG Elements)
{
   if (Value) {
      if (Elements >= 1) Self->Colour.Red   = Value[0];
      if (Elements >= 2) Self->Colour.Green = Value[1];
      if (Elements >= 3) Self->Colour.Blue  = Value[2];
      if (Elements >= 4) Self->Colour.Alpha = Value[3];
      else Self->Colour.Alpha = 1;
   }
   else Self->Colour.Alpha = 0;

   Self->LinearColour = Self->Colour;
   glLinearRGB.convert(Self->LinearColour);

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Constant: Specifies the ks/kd value in Phong lighting model.

In the Phong lighting model, this field specifies the kd value in diffuse mode, or ks value in specular mode.

*********************************************************************************************************************/

static ERR LIGHTINGFX_GET_Constant(extLightingFX *Self, DOUBLE *Value)
{
   *Value = Self->Constant;
   return ERR::Okay;
}

static ERR LIGHTINGFX_SET_Constant(extLightingFX *Self, DOUBLE Value)
{
   if (Value >= 0) {
      Self->Constant = Value;
      return ERR::Okay;
   }
   else return ERR::InvalidValue;
}

/*********************************************************************************************************************

-FIELD-
Exponent: Exponent for specular lighting, larger is more "shiny".  Ranges from 1.0 to 128.0.

This field defines the exponent value for specular lighting, within a range of 1.0 to 128.0.  The larger the value,
shinier the end result.

*********************************************************************************************************************/

static ERR LIGHTINGFX_GET_Exponent(extLightingFX *Self, DOUBLE *Value)
{
   *Value = Self->SpecularExponent;
   return ERR::Okay;
}

static ERR LIGHTINGFX_SET_Exponent(extLightingFX *Self, DOUBLE Value)
{
   if ((Value >= 1.0) and (Value <= 128.0)) {
      Self->SpecularExponent = Value;
      return ERR::Okay;
   }
   else return ERR::OutOfRange;
}

/*********************************************************************************************************************

-FIELD-
Scale: The maximum height of the input surface (bump map) when the alpha input is 1.0.

*********************************************************************************************************************/

static ERR LIGHTINGFX_GET_Scale(extLightingFX *Self, DOUBLE *Value)
{
   *Value = Self->Scale;
   return ERR::Okay;
}

static ERR LIGHTINGFX_SET_Scale(extLightingFX *Self, DOUBLE Value)
{
   Self->Scale = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Type: Defines the type of surface light scattering, which can be specular or diffuse.
Lookup: LT

*********************************************************************************************************************/

static ERR LIGHTINGFX_GET_Type(extLightingFX *Self, LT *Value)
{
   *Value = Self->Type;
   return ERR::Okay;
}

static ERR LIGHTINGFX_SET_Type(extLightingFX *Self, LT Value)
{
   Self->Type = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
UnitX: The intended distance in current filter units for dx in the surface normal calculation formulas.

Indicates the intended distance in current filter units (i.e. as determined by the value of PrimitiveUnits)
for dx in the surface normal calculation formulas.

By specifying value(s) for #UnitX, the kernel becomes defined in a scalable, abstract coordinate system.  If #UnitX is
not specified, the default value is one pixel in the offscreen bitmap, which is a pixel-based coordinate system, and
thus potentially not scalable.  For some level of consistency across display media and user agents, it is necessary
that a value be provided for at least one of ResX and #UnitX.

*********************************************************************************************************************/

static ERR LIGHTINGFX_GET_UnitX(extLightingFX *Self, DOUBLE *Value)
{
   *Value = Self->UnitX;
   return ERR::Okay;
}

static ERR LIGHTINGFX_SET_UnitX(extLightingFX *Self, DOUBLE Value)
{
   if (Value < 0) return ERR::InvalidValue;
   Self->UnitX = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
UnitY: The intended distance in current filter units for dy in the surface normal calculation formulas.

Indicates the intended distance in current filter units (i.e. as determined by the value of PrimitiveUnits)
for dy in the surface normal calculation formulas.

By specifying value(s) for #UnitY, the kernel becomes defined in a scalable, abstract coordinate system.  If #UnitY is
not specified, the default value is one pixel in the offscreen bitmap, which is a pixel-based coordinate system, and
thus potentially not scalable.  For some level of consistency across display media and user agents, it is necessary
that a value be provided for at least one of ResY and #UnitY.

*********************************************************************************************************************/

static ERR LIGHTINGFX_GET_UnitY(extLightingFX *Self, DOUBLE *Value)
{
   *Value = Self->UnitY;
   return ERR::Okay;
}

static ERR LIGHTINGFX_SET_UnitY(extLightingFX *Self, DOUBLE Value)
{
   if (Value < 0) return ERR::InvalidValue;
   Self->UnitY = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
XMLDef: Returns an SVG compliant XML string that describes the filter.
-END-

*********************************************************************************************************************/

static ERR LIGHTINGFX_GET_XMLDef(extLightingFX *Self, STRING *Value)
{
   std::stringstream stream;
   std::string type(Self->Type IS LT::DIFFUSE ? "feDiffuseLighting" : "feSpecularLighting");

   // TODO
   stream << "<" << type << ">";
   stream << "</" << type << ">";
   *Value = StrClone(stream.str().c_str());
   return ERR::Okay;
}

//********************************************************************************************************************

static const FieldDef clLightingType[] = {
   { "Diffuse",  LT::DIFFUSE },
   { "Specular", LT::SPECULAR },
   { NULL, 0 }
};

#include "filter_lighting_def.c"

static const FieldArray clLightingFXFields[] = {
   { "Colour",   FDF_VIRTUAL|FD_FLOAT|FDF_ARRAY|FDF_RW,  LIGHTINGFX_GET_Colour, LIGHTINGFX_SET_Colour },
   { "Constant", FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,          LIGHTINGFX_GET_Constant, LIGHTINGFX_SET_Constant },
   { "Exponent", FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,          LIGHTINGFX_GET_Exponent, LIGHTINGFX_SET_Exponent },
   { "Scale",    FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,          LIGHTINGFX_GET_Scale, LIGHTINGFX_SET_Scale },
   { "Type",     FDF_VIRTUAL|FDF_LONG|FDF_LOOKUP|FDF_RW, LIGHTINGFX_GET_Type, LIGHTINGFX_SET_Type, &clLightingType },
   { "UnitX",    FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,          LIGHTINGFX_GET_UnitX, LIGHTINGFX_SET_UnitX },
   { "UnitY",    FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,          LIGHTINGFX_GET_UnitY, LIGHTINGFX_SET_UnitY },
   { "XMLDef",   FDF_VIRTUAL|FDF_STRING|FDF_ALLOC|FDF_R, LIGHTINGFX_GET_XMLDef },
   END_FIELD
};

//********************************************************************************************************************

ERR init_lightingfx(void)
{
   clLightingFX = objMetaClass::create::global(
      fl::BaseClassID(CLASSID::FILTEREFFECT),
      fl::ClassID(CLASSID::LIGHTINGFX),
      fl::Name("LightingFX"),
      fl::Category(CCF::GRAPHICS),
      fl::Actions(clLightingFXActions),
      fl::Methods(clLightingFXMethods),
      fl::Fields(clLightingFXFields),
      fl::Size(sizeof(extLightingFX)),
      fl::Path(MOD_PATH));

   return clLightingFX ? ERR::Okay : ERR::AddClass;
}
