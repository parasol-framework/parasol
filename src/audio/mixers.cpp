
template <class T> inline DOUBLE b2f(T Value)
{
   return 256 * (Value-128);
}

//********************************************************************************************************************
// Mix 8-bit mono samples.

static LONG mixmf8Mono(APTR Src, LONG SrcPos, LONG TotalSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = glMixDest;
   auto sample = (UBYTE *)Src;

   while (TotalSamples > 0) {
      *(dest++) += LeftVol * (256 * (sample[SrcPos>>16]-128));
      SrcPos += MixStep;
      TotalSamples--;
   }

   glMixDest = dest;
   return SrcPos;
}

// Mix 16-bit mono samples:

static LONG mixmf16Mono(APTR Src, LONG SrcPos, LONG TotalSamples, int nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
    auto dest = glMixDest;
    auto sample = (WORD *)Src;

    while (TotalSamples > 0) {
        *(dest++) += LeftVol * FLOAT(sample[SrcPos>>16]);
        SrcPos += MixStep;
        TotalSamples--;
    }

    glMixDest = dest;
    return SrcPos;
}

//********************************************************************************************************************
// Mix 8-bit stereo samples.

static LONG mixmf8Stereo(APTR Src, LONG SrcPos, LONG TotalSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = glMixDest;
   auto sample = (UBYTE *)Src;
   while (TotalSamples > 0) {
      *(dest++) += LeftVol * (256 * (sample[2 * (SrcPos>>16)] - 128) +
         256 * (sample[2 * (SrcPos>>16) + 1] - 128) );
      SrcPos += MixStep;
      TotalSamples--;
   }
   glMixDest = dest;
   return SrcPos;
}

//********************************************************************************************************************
// Mix 16-bit stereo samples.

static LONG mixmf16Stereo(APTR Src, LONG SrcPos, LONG TotalSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = glMixDest;
   auto sample = (WORD *)Src;
   while (TotalSamples > 0) {
       *(dest++) += LeftVol * (((FLOAT) sample[2 *(SrcPos>>16)]) + ((FLOAT) sample[2 * (SrcPos>>16) + 1])) ;
       SrcPos += MixStep;
       TotalSamples--;
   }
   glMixDest = dest;
   return SrcPos;
}

static MixRoutine MixMonoFloat[LONG(SFM::END)] = { nullptr, &mixmf8Mono, &mixmf16Mono, &mixmf8Stereo, &mixmf16Stereo };

//********************************************************************************************************************
// Mix 8-bit mono samples.

static LONG mixsf8Mono(APTR Src, LONG SrcPos, LONG TotalSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = glMixDest;
   auto sample = (UBYTE *)Src;

   while (TotalSamples > 0) {
      const DOUBLE s = 256 * (sample[SrcPos>>16] - 128);
      dest[0] += LeftVol * s;
      dest[1] += RightVol * s;
      dest += 2;
      SrcPos += MixStep;
      TotalSamples--;
   }

   glMixDest = dest;
   return SrcPos;
}

//********************************************************************************************************************
// Mix 16-bit mono samples.

static LONG mixsf16Mono(APTR Src, LONG SrcPos, LONG TotalSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = glMixDest;
   auto sample = (WORD *)Src;

   while (TotalSamples > 0) {
      const DOUBLE s = (DOUBLE)sample[SrcPos>>16];
      dest[0] += LeftVol * s;
      dest[1] += RightVol * s;
      dest    += 2;
      SrcPos  += MixStep;
      TotalSamples--;
   }

   glMixDest = dest;
   return SrcPos;
}

//********************************************************************************************************************
// Mix 8-bit stereo samples.

static LONG mixsf8Stereo(APTR Src, LONG SrcPos, LONG TotalSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = glMixDest;
   auto sample = (UBYTE *)Src;

   while (TotalSamples > 0) {
      dest[0] += LeftVol * 256 * (sample[2*(SrcPos>>16)] - 128);
      dest[1] += RightVol * 256 * (sample[2*(SrcPos>>16) + 1] - 128);
      dest += 2;
      SrcPos += MixStep;
      TotalSamples--;
   }

   glMixDest = dest;
   return SrcPos;
}

// Mix 16-bit stereo samples

static LONG mixsf16Stereo(APTR Src, LONG SrcPos, LONG TotalSamples, int nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = glMixDest;
   auto sample = (WORD *)Src;
   while (TotalSamples > 0) {
      dest[0] += LeftVol * ((DOUBLE) sample[2*(SrcPos>>16)]);
      dest[1] += RightVol * ((DOUBLE) sample[2*(SrcPos>>16) + 1]);
      dest    += 2;
      SrcPos  += MixStep;
      TotalSamples--;
   }

   glMixDest = dest;
   return SrcPos;
}

static MixRoutine MixStereoFloat[LONG(SFM::END)] = { nullptr, &mixsf8Mono, &mixsf16Mono, &mixsf8Stereo, &mixsf16Stereo };

//********************************************************************************************************************
// Mix interploated 8-bit mono samples.

static LONG mixmi8Mono(APTR Src, LONG SrcPos, LONG TotalSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = glMixDest;
   auto sample = (UBYTE *)Src;
   DOUBLE volMul = LeftVol * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
      while (TotalSamples > 0) {
         const DOUBLE s0 = b2f(sample[SrcPos>>16]);
         const DOUBLE s1 = b2f(sample[(SrcPos>>16) + nextSampleOffset]);
         *(dest++) += volMul * (((DOUBLE) (65536 - (SrcPos & 0xffff))) * s0 + ((DOUBLE) (SrcPos & 0xffff)) * s1);
         SrcPos += MixStep;
         TotalSamples--;
      }
   }
   else while (TotalSamples > 0) {
      const DOUBLE s0 = b2f(sample[SrcPos>>16]);
      const DOUBLE s1 = b2f(sample[(SrcPos>>16) + 1]);
      *(dest++) += volMul * (((DOUBLE) (65536 - (SrcPos & 0xffff))) * s0 + ((DOUBLE) (SrcPos & 0xffff)) * s1);
      SrcPos += MixStep;
      TotalSamples--;
   }

   glMixDest = dest;
   return SrcPos;
}

//********************************************************************************************************************
// Mix interploated 16-bit mono samples.

static LONG mixmi16Mono(APTR Src, LONG SrcPos, LONG TotalSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = glMixDest;
   auto sample = (WORD *)Src;
   const DOUBLE volMul = LeftVol * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
      while (TotalSamples > 0) {
         const DOUBLE s0 = sample[SrcPos>>16];
         const DOUBLE s1 = sample[(SrcPos>>16) + nextSampleOffset];
         *(dest++) += volMul * (((DOUBLE) (65536 - (SrcPos & 0xffff))) * s0 + ((DOUBLE) (SrcPos & 0xffff)) * s1);
         SrcPos += MixStep;
         TotalSamples--;
      }
   }
   else while (TotalSamples > 0) {
      const DOUBLE s0 = sample[SrcPos>>16];
      const DOUBLE s1 = sample[(SrcPos>>16) + 1];
      *(dest++) += volMul * (((DOUBLE) (65536 - (SrcPos & 0xffff))) * s0 + ((DOUBLE) (SrcPos & 0xffff)) * s1);
      SrcPos += MixStep;
      TotalSamples--;
   }

   glMixDest = dest;
   return SrcPos;
}

//********************************************************************************************************************
// Mix interploated 8-bit stereo samples.

static LONG mixmi8Stereo(APTR Src, LONG SrcPos, LONG TotalSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = glMixDest;
   auto sample = (UBYTE *)Src;
   const DOUBLE volMul = LeftVol * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
       while (TotalSamples > 0) {
         auto i0 = DOUBLE(65536 - (SrcPos & 0xffff));
         auto i1 = DOUBLE(SrcPos & 0xffff);
         *(dest++) += volMul *
            ((i0 * b2f(sample[2 * (SrcPos>>16)]) + i1 * b2f(sample[2 * (SrcPos>>16) + 2 * nextSampleOffset])) +
             (i0 * b2f(sample[2 * (SrcPos>>16) + 1]) + i1 * b2f(sample[2 * (SrcPos>>16) + 2 * nextSampleOffset + 1])));
         SrcPos += MixStep;
         TotalSamples--;
      }
   }
   else while (TotalSamples > 0) {
      auto i0 = DOUBLE(65536 - (SrcPos & 0xffff));
      auto i1 = DOUBLE(SrcPos & 0xffff);
      *(dest++) += volMul *
         ((i0 * b2f(sample[2 * (SrcPos>>16)]) +
           i1 * b2f(sample[2 * (SrcPos>>16) + 2])) +
          (i0 * b2f(sample[2 * (SrcPos>>16) + 1]) +
           i1 * b2f(sample[2 * (SrcPos>>16) + 3])));
      SrcPos += MixStep;
      TotalSamples--;
   }

    glMixDest = dest;
    return SrcPos;
}

//********************************************************************************************************************
// Mix interploated 16-bit stereo samples.

static LONG mixmi16Stereo(APTR Src, LONG SrcPos, LONG TotalSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = glMixDest;
   auto sample = (WORD *)Src;
   DOUBLE volMul = LeftVol * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
      while (TotalSamples > 0) {
         auto i0 = DOUBLE(65536 - (SrcPos & 0xffff));
         auto i1 = DOUBLE(SrcPos & 0xffff);
         *(dest++) += volMul *
            ((i0 * ((DOUBLE) sample[2 * (SrcPos>>16)]) + i1 * ((DOUBLE)sample[2 * (SrcPos>>16) + 2 * nextSampleOffset])) +
             (i0 * ((DOUBLE) sample[2 * (SrcPos>>16) + 1]) + i1 * ((DOUBLE)sample[2 * (SrcPos>>16) + 2 * nextSampleOffset + 1])));
         SrcPos += MixStep;
         TotalSamples--;
      }
   }
   else while (TotalSamples > 0) {
      auto i0 = DOUBLE(65536 - (SrcPos & 0xffff));
      auto i1 = DOUBLE(SrcPos & 0xffff);
      *(dest++) += volMul *
         ((i0 * ((DOUBLE)sample[2 * (SrcPos>>16)]) + i1 * ((DOUBLE)sample[2 * (SrcPos>>16) + 2])) +
          (i0 * ((DOUBLE)sample[2 * (SrcPos>>16) + 1]) + i1 * ((DOUBLE)sample[2 * (SrcPos>>16) + 3])));
      SrcPos += MixStep;
      TotalSamples--;
   }

   glMixDest = dest;
   return SrcPos;
}

static MixRoutine MixMonoFloatInterp[LONG(SFM::END)] = { nullptr, &mixmi8Mono, &mixmi16Mono, &mixmi8Stereo, &mixmi16Stereo };

//********************************************************************************************************************
// Mix interploated 8-bit mono samples.

static LONG mixsi8Mono(APTR Src, LONG SrcPos, LONG TotalSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = glMixDest;
   auto sample = (UBYTE *)Src;
   DOUBLE volMulL = LeftVol * (1.0 / 65536.0);
   DOUBLE volMulR = RightVol * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
      while (TotalSamples > 0) {
         DOUBLE s0 = b2f(sample[SrcPos>>16]);
         DOUBLE s1 = b2f(sample[(SrcPos>>16) + nextSampleOffset]);
         DOUBLE s = (((DOUBLE) (65536 - (SrcPos & 0xffff))) * s0 + ((DOUBLE) (SrcPos & 0xffff)) * s1);
         *dest += volMulL * s;
         *(dest+1) += volMulR * s;
         dest += 2;
         SrcPos += MixStep;
         TotalSamples--;
      }
   }
   else while (TotalSamples > 0) {
      DOUBLE s0 = b2f(sample[SrcPos>>16]);
      DOUBLE s1 = b2f(sample[(SrcPos>>16) + 1]);
      DOUBLE s = (((DOUBLE)(65536 - (SrcPos & 0xffff))) * s0 + ((DOUBLE) (SrcPos & 0xffff)) * s1);
      *dest += volMulL * s;
      *(dest+1) += volMulR * s;
      dest += 2;
      SrcPos += MixStep;
      TotalSamples--;
   }

   glMixDest = dest;
   return SrcPos;
}

//********************************************************************************************************************
// Mix interploated 16-bit mono samples.

static LONG mixsi16Mono(APTR Src, LONG SrcPos, LONG TotalSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = glMixDest;
   auto sample = (WORD *)Src;
   DOUBLE volMulL = LeftVol * (1.0 / 65536.0);
   DOUBLE volMulR = RightVol * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
      while (TotalSamples > 0) {
         auto s0 = (DOUBLE)sample[SrcPos>>16];
         auto s1 = (DOUBLE)sample[(SrcPos>>16) + nextSampleOffset];
         DOUBLE s = (((DOUBLE)(65536 - (SrcPos & 0xffff))) * s0 + ((DOUBLE)(SrcPos & 0xffff)) * s1);
         dest[0] += volMulL * s;
         dest[1] += volMulR * s;
         dest    += 2;
         SrcPos  += MixStep;
         TotalSamples--;
      }
   }
   else {
      while (TotalSamples > 0) {
         auto s0 = DOUBLE(sample[SrcPos>>16]);
         auto s1 = DOUBLE(sample[(SrcPos>>16) + 1]);
         DOUBLE s = (((DOUBLE)(65536 - (SrcPos & 0xffff))) * s0 + ((DOUBLE) (SrcPos & 0xffff)) * s1);
         dest[0] += volMulL * s;
         dest[1] += volMulR * s;
         dest    += 2;
         SrcPos  += MixStep;
         TotalSamples--;
      }
   }

   glMixDest = dest;
   return SrcPos;
}

//********************************************************************************************************************
// Mix interploated 8-bit stereo samples.

static LONG mixsi8Stereo(APTR Src, LONG SrcPos, LONG TotalSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = glMixDest;
   auto sample = (UBYTE *)Src;
   DOUBLE volMulL = LeftVol * (1.0 / 65536.0);
   DOUBLE volMulR = RightVol * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
      while (TotalSamples > 0) {
         auto i0 = DOUBLE(65536 - (SrcPos & 0xffff));
         auto i1 = DOUBLE(SrcPos & 0xffff);
         dest[0] += volMulL * (i0 * b2f(sample[2 * (SrcPos>>16)]) + i1 * b2f(sample[2 * (SrcPos>>16) + 2 * nextSampleOffset]));
         dest[1] += volMulR * (i0 * b2f(sample[2 * (SrcPos>>16) + 1]) + i1 * b2f(sample[2 * (SrcPos>>16) + 2 * nextSampleOffset + 1]));
         dest    += 2;
         SrcPos  += MixStep;
         TotalSamples--;
      }
   }
   else {
      while (TotalSamples > 0)  {
         auto i0 = DOUBLE(65536 - (SrcPos & 0xffff));
         auto i1 = DOUBLE(SrcPos & 0xffff);
         dest[0] += volMulL * (i0 * b2f(sample[2 * (SrcPos>>16)]) + i1 * b2f(sample[2 * (SrcPos>>16) + 2]));
         dest[1] += volMulR * (i0 * b2f(sample[2 * (SrcPos>>16) + 1]) + i1 * b2f(sample[2 * (SrcPos>>16) + 3]));
         dest    += 2;
         SrcPos  += MixStep;
         TotalSamples--;
      }
   }

   glMixDest = dest;
   return SrcPos;
}

//********************************************************************************************************************
// Mix interploated 16-bit stereo samples.

static LONG mixsi16Stereo(APTR Src, LONG SrcPos, LONG TotalSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = glMixDest;
   auto sample = (WORD *)Src;
   const DOUBLE volMulL = LeftVol * (1.0 / 65536.0);
   const DOUBLE volMulR = RightVol * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
      while (TotalSamples > 0) {
         auto i0 = DOUBLE(65536 - (SrcPos & 0xffff));
         auto i1 = DOUBLE(SrcPos & 0xffff);
         dest[0] += volMulL * (i0 * ((DOUBLE) sample[2 * (SrcPos>>16)]) + i1 * ((DOUBLE) sample[2 * (SrcPos>>16) + 2 * nextSampleOffset]));
         dest[1] += volMulR * (i0 * ((DOUBLE) sample[2 * (SrcPos>>16) + 1]) +
                    i1 * ((DOUBLE) sample[2 * (SrcPos>>16) + 2 * nextSampleOffset + 1]));
         dest   += 2;
         SrcPos += MixStep;
         TotalSamples--;
      }
   }
   else {
      while (TotalSamples > 0) {
         auto i0 = DOUBLE(65536 - (SrcPos & 0xffff));
         auto i1 = DOUBLE(SrcPos & 0xffff);
         dest[0] += volMulL * (i0 * ((DOUBLE) sample[2 * (SrcPos>>16)]) + i1 * ((DOUBLE) sample[2 * (SrcPos>>16) + 2]));
         dest[1] += volMulR * (i0 * ((DOUBLE) sample[2 * (SrcPos>>16) + 1]) + i1 * ((DOUBLE) sample[2 * (SrcPos>>16) + 3]));
         dest    += 2;
         SrcPos  += MixStep;
         TotalSamples--;
      }
   }

   glMixDest = dest;
   return SrcPos;
}

static MixRoutine MixStereoFloatInterp[LONG(SFM::END)] = { nullptr, &mixsi8Mono, &mixsi16Mono, &mixsi8Stereo, &mixsi16Stereo };
