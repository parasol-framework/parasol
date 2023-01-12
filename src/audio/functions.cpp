
#define RAMPSPEED 0.01  // Default ramping speed - volume steps per output sample.  Keeping this value very low prevents clicks from occurring

template <class T> inline DOUBLE b2f(T Value)
{
   return 256 * (Value-128);
}

static LONG MixSrcPos, MixStep;
static UBYTE *glMixDest = NULL;
static UBYTE *MixSample = NULL;

static void filter_float_mono(extAudio *, FLOAT *, LONG);
static void filter_float_stereo(extAudio *, FLOAT *, LONG);
static void mix_channel(extAudio *, AudioChannel &, LONG, APTR);

//********************************************************************************************************************

static void convert_float8(FLOAT *buf, LONG numSamples, UBYTE *dest)
{
   while (numSamples) {
      LONG n = ((LONG)(*(buf++)))>>8;
      if (n < -128) n = -128;
      else if (n > 127)  n = 127;
      *dest++ = (UBYTE)(128 + n);
      numSamples--;
   }
}

static void convert_float16(FLOAT *buf, LONG numSamples, WORD *dest)
{
   while (numSamples) {
      LONG n = (LONG)(*(buf++));
      if (n < -32768) n = -32768;
      else if (n > 32767) n = 32767;
      *dest++ = (WORD)n;
      numSamples--;
   }
}

//********************************************************************************************************************

static LONG samples_until_end(extAudio *Self, AudioChannel &Channel, LONG *NextOffset)
{
   LONG num, lpStart, lpEnd, lpType;

   *NextOffset = 1;

   auto &sample = Self->Samples[Channel.SampleHandle];

   // Return a maximum of 32k-1 samples to prevent overflow problems

   if (Channel.LoopIndex IS 2) {
      lpStart = sample.Loop2Start;
      lpEnd   = sample.Loop2End;
      lpType  = sample.Loop2Type;
   }
   else {
      lpStart = sample.Loop1Start;
      lpEnd   = sample.Loop1End;
      lpType  = sample.Loop1Type;
   }

   // When using interpolating mixing, we'll first mix everything normally until the very last sample of the
   // loop/sample/stream. Then the last sample will be mixed separately, setting *NextOffset to a correct value, to
   // make sure we won't interpolate past the end.  This doesn't make this code exactly pretty, but saves us from
   // quite a bit of headache elsewhere.

   switch (lpType) {
      default:
         if (Self->Flags & ADF_OVER_SAMPLING) {
            if ((Channel.Position + 1) < sample.SampleLength) {
               num = (sample.SampleLength - 1) - Channel.Position;
            }
            else { // The last sample
               *NextOffset = 0;
               num = sample.SampleLength - Channel.Position;
            }
         }
         else num = sample.SampleLength - Channel.Position;
         break;

      case LTYPE_UNIDIRECTIONAL:
         if (Self->Flags & ADF_OVER_SAMPLING) {
            if ((Channel.Position + 1) < lpEnd) num = (lpEnd - 1) - Channel.Position;
            else { // The last sample of the loop
               *NextOffset = lpStart - Channel.Position;
               num = lpEnd - Channel.Position;
            }
         }
         else num = lpEnd - Channel.Position;
         break;

      case LTYPE_BIDIRECTIONAL:
         if (Channel.Flags & CHF_BACKWARD) { // Backwards
            if (Self->Flags & ADF_OVER_SAMPLING) {
               if (Channel.Position IS (lpEnd-1)) { // First sample of the loop backwards
                  *NextOffset = 0;
                  num = 1;
               }
               else num = Channel.Position - lpStart;
            }
            else num = Channel.Position - lpStart;
            break;
         }
         else { // Forward
            if (Self->Flags & ADF_OVER_SAMPLING) {
               if ((Channel.Position + 1) < lpEnd) num = (lpEnd - 1) - Channel.Position;
               else { // The last sample of the loop
                  *NextOffset = 0;
                  num = lpEnd - Channel.Position;
               }
            }
            else num = lpEnd - Channel.Position;
            break;
         }
   }

   if (num > 0x7FFF) return 0x7FFF0000;
   else return ((num << 16) - Channel.PositionLow);
}

//********************************************************************************************************************

static bool amiga_change(extAudio *Self, AudioChannel &Channel)
{
   // A sample end or sample loop end has been reached, the sample has been changed, and both old and new samples use
   // Amiga compatible looping - handle Amiga Loop Emulation sample change

   if (Channel.SampleHandle > 1) Channel.SampleHandle--;
   auto &sample = Self->Samples[Channel.SampleHandle];
   Channel.Flags &= ~CHF_CHANGED;

   if (sample.LoopMode IS LOOP_AMIGA) {
      // Looping - start playback from loop beginning
      Channel.Position    = sample.Loop1Start;
      Channel.PositionLow = 0;
      return false;
   }

   // Not looping - finish the sample
   Channel.State = CHS_FINISHED;
   return true;
}

//********************************************************************************************************************

static bool handle_sample_end(extAudio *Self, AudioChannel &Channel)
{
   parasol::Log log(__FUNCTION__);
   LONG lpStart, lpEnd, lpType;

   auto &sample = Self->Samples[Channel.SampleHandle];

   if (Channel.LoopIndex IS 2) {
      lpStart = sample.Loop2Start;
      lpEnd   = sample.Loop2End;
      lpType  = sample.Loop2Type;
   }
   else {
      lpStart = sample.Loop1Start;
      lpEnd   = sample.Loop1End;
      lpType  = sample.Loop1Type;
   }

   if (!lpType) { // No loop - did we reach sample end?
      if (Channel.Position >= sample.SampleLength) {
         auto &prev_sample = Self->Samples[Channel.SampleHandle-1];
         if ((Channel.Flags & CHF_CHANGED) and
             ((sample.LoopMode IS LOOP_AMIGA) or (sample.LoopMode IS LOOP_AMIGA_NONE)) and
             ((prev_sample.LoopMode IS LOOP_AMIGA) or (prev_sample.LoopMode IS LOOP_AMIGA_NONE))) {
            return amiga_change(Self, Channel);
         }

         // No sample change - we are finished
         Channel.State = CHS_FINISHED;
         return true;
      }
      else return false;
   }

   if (Channel.Flags & CHF_BACKWARD) {
      // Going backwards - did we reach loop start? (signed comparison takes care of possible wraparound)
      if ((Channel.Position < lpStart) or ((Channel.Position IS lpStart) and (Channel.PositionLow IS 0)) ) {
         Channel.Flags &= ~CHF_BACKWARD;
         LONG n = ((lpStart - Channel.Position) << 16) - Channel.PositionLow - 1;
         // -1 is compensation for the fudge factor at loop end, see below
         Channel.Position = lpStart + (n>>16);
         Channel.PositionLow = n & 0xffff;

         // Don't die on overshort loops
         if (Channel.Position >= lpEnd) {
            Channel.Position = lpStart;
            return true;
         }
      }
   }
   else if (Channel.Position >= lpEnd) { // Going forward - did we reach loop end?
      if (sample.StreamID) {
         parasol::ScopedObjectLock<BaseClass> stream(sample.StreamID, 3000);

         if (stream.granted()) {
            // Read the next set of stream data into our sample buffer

            LONG bytes_read;
            if (!acRead(*stream, sample.Data, sample.BufferLength, &bytes_read)) {
               // Increment the known stream position
               Channel.StreamPos += bytes_read;

               // If the stream is a virtual file, clear the audio content for the benefit of our hearing (if the
               // buffer is not refilled in time then we will get an ugly loop/feedback distortion because the
               // content will end up repeating over and over).
               //
               // NOTE: This seems to be a bad idea - has a negative effect on the MP3 class.

/*
               if (stream->ClassID IS ID_FILE) {
                  UBYTE *buffer;
                  LONG buffersize, i;

                  GetFields(stream, FID_Buffer|TPTR, &buffer,
                                    FID_Size|TLONG,  &buffersize,
                                    TAGEND);

                  if ((sample.SampleType IS SFM_U8_BIT_STEREO) or (sample.SampleType IS SFM_U8_BIT_MONO)) {
                     for (i=0; i < buffersize; i++) buffer[i] = 0x80;
                  }
                  else ClearMemory(buffer, buffersize);
               }
*/

               // Loop back to the beginning of the stream if necessary

               if ((bytes_read <= 0) or (Channel.StreamPos >= sample.StreamLength)) {
                  if (sample.Loop2Type) {
                     acSeek(*stream, (DOUBLE)(sample.SeekStart + sample.Loop2Start), SEEK_START);
                     Channel.StreamPos = 0;
                  }
                  else Channel.State = CHS_FINISHED;
               }
            }
            else {
               log.warning("Failed to stream data from object #%d.", stream.obj->UID);
               Channel.State = CHS_FINISHED;
            }
         }
         else {
            log.msg("Stream object %d has been lost.", sample.StreamID);
            sample.StreamID = 0;
         }
      }

      // Check for ALE sample change

      auto &prev_sample = Self->Samples[Channel.SampleHandle-1];

      if ((Channel.Flags & CHF_CHANGED) and
          ((sample.LoopMode IS LOOP_AMIGA) or (sample.LoopMode IS LOOP_AMIGA_NONE)) and
          ((prev_sample.LoopMode IS LOOP_AMIGA) or (prev_sample.LoopMode IS LOOP_AMIGA_NONE))) {
         return amiga_change(Self, Channel);
      }

      // Go to the second loop if the sound has been released

      if ((Channel.LoopIndex IS 1) and (Channel.State IS CHS_RELEASED)) {
         Channel.LoopIndex = 2;
         return false;
      }

      if (lpType IS LTYPE_BIDIRECTIONAL ) {
         // Bidirectional loop - change direction
         Channel.Flags |= CHF_BACKWARD;
         LONG n = ((Channel.Position - lpEnd) << 16) + Channel.PositionLow + 1;

         // +1 is a fudge factor to make sure we'll access the correct samples all the time - a similar adjustment is
         // also done at the other end of the loop. This screws up interpolation a little when sample rate IS mixing
         // rate, but little enough that it can't be heard.

         if (lpEnd < 0x10000) {
            Channel.Position    = ((lpEnd << 16) - n)>>16;
            Channel.PositionLow = ((lpEnd << 16) - n) & 0xffff;
         }
         else {
            Channel.Position    = ((0xffff0000 - n)>>16) + (lpEnd - 0xffff);
            Channel.PositionLow = (0xffff0000 - n) & 0xffff;
         }

         if (Channel.Position <= lpStart) { // Don't die on overshort loops
            Channel.Position = lpEnd;
            return true;
         }
         return false;
      }
      else { // Unidirectional loop - just loop to the beginning
         Channel.Position = lpStart + (Channel.Position - lpEnd);

         if (Channel.Position >= lpEnd) { // Don't die on overshort loops
            Channel.Position = lpStart;
            return true;
         }

         return false;
      }
   }

   return false;
}

//********************************************************************************************************************
// Main entry point for mixing sound data to destination

ERROR mix_data(extAudio *Self, LONG Elements, APTR Destination)
{
   parasol::Log log(__FUNCTION__);

   auto dest = (UBYTE *)Destination;
   while (Elements) {
      // Mix only as much as we can fit in our mixing buffer

      auto window = (Elements > Self->MixElements) ? Self->MixElements : Elements;

      // Clear the mix buffer, then mix all channels to the buffer

      ClearMemory(Self->MixBuffer, sizeof(FLOAT) * (Self->Stereo ? (window<<1) : window));

      for (auto n=1; n < (LONG)Self->Sets.size(); n++) {
         if (Self->Sets[n].Channel) {
            for (auto i=0; i < Self->Sets[n].Actual; i++) {
               if (Self->Sets[n].Channel[i].active()) {
                  mix_channel(Self, Self->Sets[n].Channel[i], window, Self->MixBuffer);
               }
            }
         }
      }

      // Do optional post-processing

      if (Self->Flags & (ADF_FILTER_LOW|ADF_FILTER_HIGH)) {
         if (Self->Stereo) filter_float_stereo(Self, (FLOAT *)Self->MixBuffer, window);
         else filter_float_mono(Self, (FLOAT *)Self->MixBuffer, window);
      }

      // Convert the floating point data to the correct output format

      if (Self->BitDepth IS 24) {

      }
      else if (Self->BitDepth IS 16) {
         if (Self->Stereo) convert_float16((FLOAT *)Self->MixBuffer, window<<1, (WORD *)dest);
         else convert_float16((FLOAT *)Self->MixBuffer, window, (WORD *)dest);
      }
      else {
         if (Self->Stereo) convert_float8((FLOAT *)Self->MixBuffer, window<<1, (UBYTE *)dest);
         else convert_float8((FLOAT *)Self->MixBuffer, window, (UBYTE *)dest);
      }

      dest += window * Self->SampleBitSize;
      Elements -= window;
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static void mix_channel(extAudio *Self, AudioChannel &Channel, LONG numSamples, APTR Dest)
{
   parasol::Log log(__FUNCTION__);

   auto &sample = Self->Samples[Channel.SampleHandle];

   // Check that we have something to mix

   if ((!Channel.Frequency) or (!sample.Data) or (sample.SampleLength <= 0)) return;

   // Calculate resampling step (16.16 fixed point)

   LONG step = (((Channel.Frequency / Self->OutputRate) << 16) + ((Channel.Frequency % Self->OutputRate) << 16) / Self->OutputRate);

   DOUBLE stereo_mul = 1.0;
   if (!Self->Stereo) {
      if ((sample.SampleType IS SFM_U8_BIT_STEREO) or (sample.SampleType IS SFM_S16_BIT_STEREO)) {
         stereo_mul = 0.5;
      }
   }

   DOUBLE mastervol = Self->Mute ? 0 : Self->MasterVolume * stereo_mul;

   LONG sample_size;
   switch (sample.SampleType) {
      case SFM_U8_BIT_STEREO:
      case SFM_S16_BIT_MONO: sample_size = 2; break;
      case SFM_S16_BIT_STEREO: sample_size = 4; break;
      default: sample_size = 1; break;
   }

   LONG prev_mix = 1;
   glMixDest = (UBYTE *)Dest;
   while (numSamples) {
      if (Channel.State IS CHS_STOPPED) return;
      else if (Channel.State IS CHS_FINISHED) return;

      LONG nextoffset;
      LONG sue = samples_until_end(Self, Channel, &nextoffset);

      // Calculate the number of destination samples (note rounding)

      LONG mixUntilEnd = sue / step;
      if (sue % step) mixUntilEnd++;

      LONG mixNow;
      if (mixUntilEnd > numSamples) mixNow = numSamples;
      else mixNow = mixUntilEnd;
      numSamples -= mixNow;

      // This should never happen, but prevents any chance of an infinite loop.

      if ((mixNow IS 0) and (prev_mix IS 0)) return;

      prev_mix = mixNow;

      if (mixNow > 0) {
         if (Channel.PositionLow < 0) { // Sanity check
            log.warning("Detected invalid PositionLow value of %d", Channel.PositionLow);
            return;
         }

         MixSrcPos = Channel.PositionLow;
         MixSample = sample.Data + (sample_size * Channel.Position); // source of sample data to mix into destination

         auto mix_routine = Self->MixRoutines[sample.SampleType];

         if (Channel.Flags & CHF_BACKWARD) MixStep = -step;
         else MixStep = step;

         // If volume ramping is enabled, mix one sample element at a time and adjust volume by RAMPSPEED.

         while ((Channel.Flags & CHF_VOL_RAMP) and (mixNow > 0)) {
            mix_routine(1, nextoffset, mastervol * Channel.LVolume, mastervol * Channel.RVolume);
            mixNow--;

            bool cont = false;

            if (Channel.LVolume < Channel.LVolumeTarget) {
               Channel.LVolume += RAMPSPEED;
               if (Channel.LVolume >= Channel.LVolumeTarget) Channel.LVolume = Channel.LVolumeTarget;
               else cont = true;
            }
            else if (Channel.LVolume > Channel.LVolumeTarget) {
               Channel.LVolume -= RAMPSPEED;
               if (Channel.LVolume <= Channel.LVolumeTarget) Channel.LVolume = Channel.LVolumeTarget;
               else cont = true;
            }

            if (Channel.RVolume < Channel.RVolumeTarget) {
               Channel.RVolume += RAMPSPEED;
               if (Channel.RVolume >= Channel.RVolumeTarget) Channel.RVolume = Channel.RVolumeTarget;
               else cont = true;
            }
            else if (Channel.RVolume > Channel.RVolumeTarget) {
               Channel.RVolume -= RAMPSPEED;
               if (Channel.RVolume <= Channel.RVolumeTarget) Channel.RVolume = Channel.RVolumeTarget;
               else cont = true;
            }

            if (!cont) Channel.Flags &= ~CHF_VOL_RAMP;
         }

         if ((Channel.LVolume <= 0.01) and (Channel.RVolume <= 0.01)) {
            // If the volume is zero we can just increment the position and not mix anything
            MixSrcPos += mixNow * MixStep;
            if (Channel.State IS CHS_FADE_OUT) {
               Channel.State = CHS_STOPPED;
               Channel.Flags &= ~CHF_VOL_RAMP;
            }
         }
         else {
            // Ensure proper alignment and do all mixing if nextoffset != 1

            LONG num;
            if ((mixNow) and ((nextoffset != 1) or ((((MAXINT)glMixDest) % 1) != 0))) {
               if (nextoffset != 1) num = mixNow;
               else {
                  num = 1 - (((MAXINT)glMixDest) % 1);
                  if (num > mixNow) num = mixNow;
               }
               mix_routine(num, nextoffset, mastervol * Channel.LVolume, mastervol * Channel.RVolume);
               mixNow -= num;
            }

            // Main mixing loop
            if (mixNow > 0) {
               mix_routine(mixNow, 1, mastervol * Channel.LVolume, mastervol * Channel.RVolume);
               mixNow = 0;
            }
         }

         // Save state back to channel structure
         Channel.PositionLow = MixSrcPos & 0xffff;
         Channel.Position = (MixSrcPos>>16) + ((MixSample - sample.Data) / sample_size);
      }
      else if (mixNow < 0) log.warning("Detected invalid mixNow value of %d.", mixNow);

      // Check if we reached loop/sample/whatever end

      if (handle_sample_end(Self, Channel)) return;
   }
}

//********************************************************************************************************************
// Mono output filtering routines.

static void filter_float_mono(extAudio *Self, FLOAT *data, LONG numSamples)
{
   static DOUBLE d1l=0, d2l=0;
   auto p = data;
   if (Self->Flags & ADF_FILTER_LOW) {
      while (numSamples > 0) {
         DOUBLE s = (d1l + 2.0 * (*p)) * (1.0 / 3.0);
         d1l = *p;
         *(p++) = s;
         numSamples--;
      }
   }
   else if (Self->Flags & ADF_FILTER_HIGH) {
      while (numSamples > 0) {
         DOUBLE s = (d1l + 3.0f * d2l + 4.0f * (*p)) * (1.0f/8.0f);
         d1l = d2l;
         d2l = *p;
         *(p++) = s;
         numSamples--;
      }
   }
}

//********************************************************************************************************************
// Stereo output filtering routines.

static void filter_float_stereo(extAudio *Self, FLOAT *data, LONG numSamples)
{
   static DOUBLE d1l = 0, d1r = 0, d2l = 0, d2r = 0;

   auto p = data;
   if (Self->Flags & ADF_FILTER_LOW) {
      while (numSamples > 0) {
         DOUBLE s = (d1l + 2.0 * (*p)) * (1.0 / 3.0);
         d1l = *p;
         *(p++) = s;

         s = (d1r + 2.0 * (*p)) * (1.0 / 3.0);
         d1r = *p;
         *(p++) = s;

         numSamples--;
      }
   }
   else if (Self->Flags & ADF_FILTER_HIGH) {
      while (numSamples > 0) {
         DOUBLE s = (d1l + 3.0 * d2l + 4.0 * (*p)) * (1.0 / 8.0);
         d1l = d2l;
         d2l = *p;
         *(p++) = s;

         s = (d1r + 3.0 * d2r + 4.0 * (*p)) * (1.0 / 8.0);
         d1r = d2r;
         d2r = *p;
         *(p++) = s;

         numSamples--;
      }
   }
}

//********************************************************************************************************************
// Mix 8-bit mono samples.

static void mixmf8Mono(LONG numSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = (FLOAT *)glMixDest;
   UBYTE *sample = MixSample;
   int  mixPos = MixSrcPos;

   while (numSamples > 0) {
      *(dest++) += LeftVol * (256 * (sample[mixPos>>16]-128));
      mixPos += MixStep;
      numSamples--;
   }

   MixSrcPos = mixPos;
   glMixDest = (UBYTE*) dest;
}

// Mix 16-bit mono samples:

static void mixmf16Mono(LONG numSamples, int nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
    auto dest = (FLOAT *)glMixDest;
    auto sample = (WORD *)MixSample;
    int mixPos = MixSrcPos;

    while (numSamples > 0) {
        *(dest++) += LeftVol * FLOAT(sample[mixPos>>16]);
        mixPos += MixStep;
        numSamples--;
    }

    MixSrcPos = mixPos;
    glMixDest = (UBYTE*) dest;
}

//********************************************************************************************************************
// Mix 8-bit stereo samples.

static void mixmf8Stereo(LONG numSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = (FLOAT *)glMixDest;
   UBYTE *sample = MixSample;
   while (numSamples > 0) {
      *(dest++) += LeftVol * (256 * (sample[2 * (MixSrcPos>>16)] - 128) +
         256 * (sample[2 * (MixSrcPos>>16) + 1] - 128) );
      MixSrcPos += MixStep;
      numSamples--;
   }
   glMixDest = (UBYTE*) dest;
}

//********************************************************************************************************************
// Mix 16-bit stereo samples.

static void mixmf16Stereo(LONG numSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = (FLOAT *)glMixDest;
   auto sample = (WORD *)MixSample;
   while (numSamples > 0) {
       *(dest++) += LeftVol * (((FLOAT) sample[2 *(MixSrcPos>>16)]) + ((FLOAT) sample[2 * (MixSrcPos>>16) + 1])) ;
       MixSrcPos += MixStep;
       numSamples--;
   }
   glMixDest = (UBYTE*) dest;
}

static MixRoutine MixMonoFloat[SFM_END] = { NULL, &mixmf8Mono, &mixmf16Mono, &mixmf8Stereo, &mixmf16Stereo };

//********************************************************************************************************************
// Mix 8-bit mono samples.

static void mixsf8Mono(LONG numSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = (FLOAT *)glMixDest;
   UBYTE *sample = MixSample;
   LONG mixPos = MixSrcPos;

   while (numSamples > 0) {
      const DOUBLE s = 256 * (sample[mixPos>>16] - 128);
      *dest += LeftVol * s;
      *(dest+1) += RightVol * s;
      dest += 2;
      mixPos += MixStep;
      numSamples--;
   }

   MixSrcPos = mixPos;
   glMixDest = (UBYTE*) dest;
}

//********************************************************************************************************************
// Mix 16-bit mono samples.

static void mixsf16Mono(LONG numSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = (FLOAT *)glMixDest;
   auto sample = (WORD *)MixSample;
   LONG mixPos = MixSrcPos;

   while (numSamples > 0) {
      const DOUBLE s = (DOUBLE)sample[mixPos>>16];
      *dest += LeftVol * s;
      *(dest+1) += RightVol * s;
      dest += 2;
      mixPos += MixStep;
      numSamples--;
   }

   MixSrcPos = mixPos;
   glMixDest = (UBYTE *)dest;
}

//********************************************************************************************************************
// Mix 8-bit stereo samples.

static void mixsf8Stereo(LONG numSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = (FLOAT *)glMixDest;
   UBYTE *sample = MixSample;

   while (numSamples > 0) {
      *dest += LeftVol * 256 * (sample[2*(MixSrcPos>>16)] - 128);
      *(dest+1) += RightVol * 256 * (sample[2*(MixSrcPos>>16) + 1] - 128);
      dest += 2;
      MixSrcPos += MixStep;
      numSamples--;
   }

   glMixDest = (UBYTE*) dest;
}

// Mix 16-bit stereo samples

static void mixsf16Stereo(LONG numSamples, int nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = (FLOAT *)glMixDest;
   auto sample = (WORD *)MixSample;
   while (numSamples > 0) {
      *dest += LeftVol * ((DOUBLE) sample[2*(MixSrcPos>>16)]);
      *(dest+1) += RightVol * ((DOUBLE) sample[2*(MixSrcPos>>16) + 1]);
      dest += 2;
      MixSrcPos += MixStep;
      numSamples--;
   }

   glMixDest = (UBYTE*) dest;
}

static MixRoutine MixStereoFloat[SFM_END] = { NULL, &mixsf8Mono, &mixsf16Mono, &mixsf8Stereo, &mixsf16Stereo };

//********************************************************************************************************************
// Mix interploated 8-bit mono samples.

static void mixmi8Mono(LONG numSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = (FLOAT *)glMixDest;
   UBYTE *sample = MixSample;
   LONG  mixPos = MixSrcPos;
   DOUBLE volMul = LeftVol * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
      while (numSamples > 0) {
         const DOUBLE s0 = b2f(sample[mixPos>>16]);
         const DOUBLE s1 = b2f(sample[(mixPos>>16) + nextSampleOffset]);
         *(dest++) += volMul * (((DOUBLE) (65536 - (mixPos & 0xffff))) * s0 + ((DOUBLE) (mixPos & 0xffff)) * s1);
         mixPos += MixStep;
         numSamples--;
      }
   }
   else while (numSamples > 0) {
      const DOUBLE s0 = b2f(sample[mixPos>>16]);
      const DOUBLE s1 = b2f(sample[(mixPos>>16) + 1]);
      *(dest++) += volMul * (((DOUBLE) (65536 - (mixPos & 0xffff))) * s0 + ((DOUBLE) (mixPos & 0xffff)) * s1);
      mixPos += MixStep;
      numSamples--;
   }

   MixSrcPos = mixPos;
   glMixDest = (UBYTE *)dest;
}

//********************************************************************************************************************
// Mix interploated 16-bit mono samples.

static void mixmi16Mono(LONG numSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = (FLOAT *)glMixDest;
   auto sample = (WORD *)MixSample;
   LONG mixPos = MixSrcPos;
   const DOUBLE volMul = LeftVol * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
      while (numSamples > 0) {
         const DOUBLE s0 = sample[mixPos>>16];
         const DOUBLE s1 = sample[(mixPos>>16) + nextSampleOffset];
         *(dest++) += volMul * (((DOUBLE) (65536 - (mixPos & 0xffff))) * s0 + ((DOUBLE) (mixPos & 0xffff)) * s1);
         mixPos += MixStep;
         numSamples--;
      }
   }
   else while (numSamples > 0) {
      const DOUBLE s0 = sample[mixPos>>16];
      const DOUBLE s1 = sample[(mixPos>>16) + 1];
      *(dest++) += volMul * (((DOUBLE) (65536 - (mixPos & 0xffff))) * s0 + ((DOUBLE) (mixPos & 0xffff)) * s1);
      mixPos += MixStep;
      numSamples--;
   }

   MixSrcPos = mixPos;
   glMixDest = (UBYTE *)dest;
}

//********************************************************************************************************************
// Mix interploated 8-bit stereo samples.

static void mixmi8Stereo(LONG numSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = (FLOAT *)glMixDest;
   UBYTE *sample = MixSample;
   LONG mixPos = MixSrcPos;
   const DOUBLE volMul = LeftVol * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
       while (numSamples > 0) {
         auto i0 = DOUBLE(65536 - (mixPos & 0xffff));
         auto i1 = DOUBLE(mixPos & 0xffff);
         *(dest++) += volMul *
            ((i0 * b2f(sample[2 * (mixPos>>16)]) + i1 * b2f(sample[2 * (mixPos>>16) + 2 * nextSampleOffset])) +
             (i0 * b2f(sample[2 * (mixPos>>16) + 1]) + i1 * b2f(sample[2 * (mixPos>>16) + 2 * nextSampleOffset + 1])));
         mixPos += MixStep;
         numSamples--;
      }
   }
   else while (numSamples > 0) {
      auto i0 = DOUBLE(65536 - (mixPos & 0xffff));
      auto i1 = DOUBLE(mixPos & 0xffff);
      *(dest++) += volMul *
         ((i0 * b2f(sample[2 * (mixPos>>16)]) +
           i1 * b2f(sample[2 * (mixPos>>16) + 2])) +
          (i0 * b2f(sample[2 * (mixPos>>16) + 1]) +
           i1 * b2f(sample[2 * (mixPos>>16) + 3])));
      mixPos += MixStep;
      numSamples--;
   }

    MixSrcPos = mixPos;
    glMixDest = (UBYTE *)dest;
}

//********************************************************************************************************************
// Mix interploated 16-bit stereo samples.

static void mixmi16Stereo(LONG numSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = (FLOAT *)glMixDest;
   auto sample = (WORD *)MixSample;
   LONG mixPos = MixSrcPos;
   DOUBLE volMul = LeftVol * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
      while (numSamples > 0) {
         auto i0 = DOUBLE(65536 - (mixPos & 0xffff));
         auto i1 = DOUBLE(mixPos & 0xffff);
         *(dest++) += volMul *
            ((i0 * ((DOUBLE) sample[2 * (mixPos>>16)]) + i1 * ((DOUBLE)sample[2 * (mixPos>>16) + 2 * nextSampleOffset])) +
             (i0 * ((DOUBLE) sample[2 * (mixPos>>16) + 1]) + i1 * ((DOUBLE)sample[2 * (mixPos>>16) + 2 * nextSampleOffset + 1])));
         mixPos += MixStep;
         numSamples--;
      }
   }
   else while (numSamples > 0) {
      auto i0 = DOUBLE(65536 - (mixPos & 0xffff));
      auto i1 = DOUBLE(mixPos & 0xffff);
      *(dest++) += volMul *
         ((i0 * ((DOUBLE)sample[2 * (mixPos>>16)]) + i1 * ((DOUBLE)sample[2 * (mixPos>>16) + 2])) +
          (i0 * ((DOUBLE)sample[2 * (mixPos>>16) + 1]) + i1 * ((DOUBLE)sample[2 * (mixPos>>16) + 3])));
      mixPos += MixStep;
      numSamples--;
   }

   MixSrcPos = mixPos;
   glMixDest = (UBYTE *)dest;
}

static MixRoutine MixMonoFloatInterp[SFM_END] = { NULL, &mixmi8Mono, &mixmi16Mono, &mixmi8Stereo, &mixmi16Stereo };

//********************************************************************************************************************
// Mix interploated 8-bit mono samples.

static void mixsi8Mono(LONG numSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = (FLOAT *)glMixDest;
   UBYTE *sample = MixSample;
   LONG  mixPos = MixSrcPos;
   DOUBLE volMulL = LeftVol * (1.0 / 65536.0);
   DOUBLE volMulR = RightVol * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
      while (numSamples > 0) {
         DOUBLE s0 = b2f(sample[mixPos>>16]);
         DOUBLE s1 = b2f(sample[(mixPos>>16) + nextSampleOffset]);
         DOUBLE s = (((DOUBLE) (65536 - (mixPos & 0xffff))) * s0 + ((DOUBLE) (mixPos & 0xffff)) * s1);
         *dest += volMulL * s;
         *(dest+1) += volMulR * s;
         dest += 2;
         mixPos += MixStep;
         numSamples--;
      }
   }
   else while (numSamples > 0) {
      DOUBLE s0 = b2f(sample[mixPos>>16]);
      DOUBLE s1 = b2f(sample[(mixPos>>16) + 1]);
      DOUBLE s = (((DOUBLE)(65536 - (mixPos & 0xffff))) * s0 + ((DOUBLE) (mixPos & 0xffff)) * s1);
      *dest += volMulL * s;
      *(dest+1) += volMulR * s;
      dest += 2;
      mixPos += MixStep;
      numSamples--;
   }

   MixSrcPos = mixPos;
   glMixDest = (UBYTE *)dest;
}

//********************************************************************************************************************
// Mix interploated 16-bit mono samples.

static void mixsi16Mono(LONG numSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = (FLOAT *)glMixDest;
   auto sample = (WORD *)MixSample;
   LONG mixPos = MixSrcPos;
   DOUBLE volMulL = LeftVol * (1.0 / 65536.0);
   DOUBLE volMulR = RightVol * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
      while (numSamples > 0) {
         DOUBLE s0 = (DOUBLE)sample[mixPos>>16];
         DOUBLE s1 = (DOUBLE)sample[(mixPos>>16) + nextSampleOffset];
         DOUBLE s = (((DOUBLE)(65536 - (mixPos & 0xffff))) * s0 + ((DOUBLE)(mixPos & 0xffff)) * s1);
         *dest += volMulL * s;
         *(dest+1) += volMulR * s;
         dest += 2;
         mixPos += MixStep;
         numSamples--;
      }
   }
   else {
      while (numSamples > 0) {
         DOUBLE s0 = (DOUBLE)sample[mixPos>>16];
         DOUBLE s1 = (DOUBLE)sample[(mixPos>>16) + 1];
         DOUBLE s = (((DOUBLE)(65536 - (mixPos & 0xffff))) * s0 + ((DOUBLE) (mixPos & 0xffff)) * s1);
         *dest += volMulL * s;
         *(dest+1) += volMulR * s;
         dest += 2;
         mixPos += MixStep;
         numSamples--;
      }
   }

   MixSrcPos = mixPos;
   glMixDest = (UBYTE *)dest;
}

//********************************************************************************************************************
// Mix interploated 8-bit stereo samples.

static void mixsi8Stereo(LONG numSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = (FLOAT *)glMixDest;
   UBYTE *sample = MixSample;
   LONG mixPos = MixSrcPos;
   DOUBLE volMulL = LeftVol * (1.0 / 65536.0);
   DOUBLE volMulR = RightVol * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
      while (numSamples > 0) {
         DOUBLE i0 = (DOUBLE) (65536 - (mixPos & 0xffff));
         DOUBLE i1 = (DOUBLE) (mixPos & 0xffff);
         *dest += volMulL * (i0 * b2f(sample[2 * (mixPos>>16)]) + i1 * b2f(sample[2 * (mixPos>>16) + 2 * nextSampleOffset]));
         *(dest+1) += volMulR * (i0 * b2f(sample[2 * (mixPos>>16) + 1]) + i1 * b2f(sample[2 * (mixPos>>16) + 2 * nextSampleOffset + 1]));
         dest += 2;
         mixPos += MixStep;
         numSamples--;
      }
   }
   else {
      while (numSamples > 0)  {
         auto i0 = DOUBLE(65536 - (mixPos & 0xffff));
         auto i1 = DOUBLE(mixPos & 0xffff);
         *dest += volMulL * (i0 * b2f(sample[2 * (mixPos>>16)]) + i1 * b2f(sample[2 * (mixPos>>16) + 2]));
         *(dest+1) += volMulR * (i0 * b2f(sample[2 * (mixPos>>16) + 1]) + i1 * b2f(sample[2 * (mixPos>>16) + 3]));
         dest += 2;
         mixPos += MixStep;
         numSamples--;
      }
   }

   MixSrcPos = mixPos;
   glMixDest = (UBYTE*) dest;
}

//********************************************************************************************************************
// Mix interploated 16-bit stereo samples.

static void mixsi16Stereo(LONG numSamples, LONG nextSampleOffset, FLOAT LeftVol, FLOAT RightVol)
{
   auto dest = (FLOAT *)glMixDest;
   auto sample = (WORD *)MixSample;
   LONG mixPos = MixSrcPos;
   const DOUBLE volMulL = LeftVol * (1.0 / 65536.0);
   const DOUBLE volMulR = RightVol * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
      while (numSamples > 0) {
         auto i0 = DOUBLE(65536 - (mixPos & 0xffff));
         auto i1 = DOUBLE(mixPos & 0xffff);
         *dest += volMulL * (i0 * ((DOUBLE) sample[2 * (mixPos>>16)]) + i1 * ((DOUBLE) sample[2 * (mixPos>>16) + 2 * nextSampleOffset]));
         *(dest+1) += volMulR * (i0 * ((DOUBLE) sample[2 * (mixPos>>16) + 1]) +
            i1 * ((DOUBLE) sample[2 * (mixPos>>16) + 2 * nextSampleOffset + 1]));
         dest += 2;
         mixPos += MixStep;
         numSamples--;
      }
   }
   else {
      while (numSamples > 0) {
         auto i0 = DOUBLE(65536 - (mixPos & 0xffff));
         auto i1 = DOUBLE(mixPos & 0xffff);
         *dest += volMulL *
            (i0 * ((DOUBLE) sample[2 * (mixPos>>16)]) +
             i1 * ((DOUBLE) sample[2 * (mixPos>>16) + 2]));
         *(dest+1) += volMulR *
            (i0 * ((DOUBLE) sample[2 * (mixPos>>16) + 1]) +
             i1 * ((DOUBLE) sample[2 * (mixPos>>16) + 3]));
         dest += 2;
         mixPos += MixStep;
         numSamples--;
      }
   }

   MixSrcPos = mixPos;
   glMixDest = (UBYTE*) dest;
}

static MixRoutine MixStereoFloatInterp[SFM_END] = { NULL, &mixsi8Mono, &mixsi16Mono, &mixsi8Stereo, &mixsi16Stereo };
