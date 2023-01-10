
#define VUBLOCK   128   // VU meter calculation block size in bytes
#define VUBSHIFT  7     // Amount to shift right to convert from bytes to blocks: (log2 VUBLOCK)
#define RAMPSPEED 0.01  // Default ramping speed - volume steps per output sample.  Keeping this value very low prevents clicks from occurring

template <class T> inline DOUBLE b2f(T Value)
{
   return 256 * (Value-128);
}

static void filter_float_mono(extAudio *, void *, LONG, void *);
static void filter_float_stereo(extAudio *, void *, LONG, void *);

static ERROR mix_channel(extAudio *, ChannelSet *, AudioChannel *, LONG, APTR);

//********************************************************************************************************************

static void convert_float8(ULONG numSamples, UBYTE *dest)
{
   FLOAT *buf = (FLOAT *)glMixBuffer;
   while (numSamples) {
      LONG n = ((LONG)(*(buf++)))>>8;
      if (n < -128) n = -128;
      else if (n > 127)  n = 127;
      *dest++ = (UBYTE)(128 + n);
      numSamples--;
   }
}

static void convert_float16(ULONG numSamples, WORD *dest)
{
   FLOAT *buf = (FLOAT *)glMixBuffer;
   while (numSamples) {
      LONG n = (int)(*(buf++));
      if (n < -32768) n = -32768;
      else if (n > 32767) n = 32767;
      *dest++ = (WORD)n;
      numSamples--;
   }
}

//********************************************************************************************************************

static ULONG samples_until_end(extAudio *Self, AudioChannel *Channel, LONG *NextOffset)
{
   ULONG num, lpStart, lpEnd;
   LONG lpType;

   *NextOffset = 1;

   // Return a maximum of 32k-1 samples to prevent overflow problems

   if (Channel->LoopIndex IS 2) {
      lpStart = Channel->Sample.Loop2Start;
      lpEnd   = Channel->Sample.Loop2End;
      lpType  = Channel->Sample.Loop2Type;
   }
   else {
      lpStart = Channel->Sample.Loop1Start;
      lpEnd   = Channel->Sample.Loop1End;
      lpType  = Channel->Sample.Loop1Type;
   }

   // When using interpolating mixing, we'll first mix everything normally until the very last sample of the
   // loop/sample/stream. Then the last sample will be mixed separately, setting *NextOffset to a correct value, to
   // make sure we won't interpolate past the end.  This doesn't make this code exactly pretty, but saves us from
   // quite a bit of headache elsewhere.

   switch (lpType) {
      default:
         if (Self->Flags & ADF_OVER_SAMPLING) {
            if ((Channel->Position + 1) < (ULONG)Channel->Sample.SampleLength) {
               num = (Channel->Sample.SampleLength - 1) - Channel->Position;
            }
            else {
               // The last sample
               *NextOffset = 0;
               num = Channel->Sample.SampleLength - Channel->Position;
            }
         }
         else num = Channel->Sample.SampleLength - Channel->Position;
         break;

      case LTYPE_UNIDIRECTIONAL:
         if (Self->Flags & ADF_OVER_SAMPLING) {
            if ( (Channel->Position + 1) < lpEnd ) num = (lpEnd - 1) - Channel->Position;
            else {
               // The last sample of the loop
               *NextOffset = lpStart - Channel->Position;
               num = lpEnd - Channel->Position;
            }
         }
         else num = lpEnd - Channel->Position;
         break;

      case LTYPE_BIDIRECTIONAL:
         if (Channel->Flags & CHF_BACKWARD) { // Backwards
            if (Self->Flags & ADF_OVER_SAMPLING) {
               if (Channel->Position IS (lpEnd-1)) { // First sample of the loop backwards
                  *NextOffset = 0;
                  num = 1;
               }
               else num = Channel->Position - lpStart;
            }
            else num = Channel->Position - lpStart;
            break;
         }
         else { // Forward
            if (Self->Flags & ADF_OVER_SAMPLING) {
               if ((Channel->Position + 1) < lpEnd) num = (lpEnd - 1) - Channel->Position;
               else { // The last sample of the loop
                  *NextOffset = 0;
                  num = lpEnd - Channel->Position;
               }
            }
            else num = lpEnd - Channel->Position;
            break;
         }
   }

   if (num > 0x7FFF) return 0x7FFF0000;
   return ((num << 16) - Channel->PositionLow);
}

//********************************************************************************************************************

static bool AmigaChange(extAudio *Self, AudioChannel *Channel)
{
   // A sample end or sample loop end has been reached, the sample has been changed, and both old and new samples use
   // Amiga compatible looping - handle Amiga Loop Emulation sample change

   AudioSample *sample = &Self->Samples[Channel->SampleHandle-1];
   Channel->Sample.Data         = sample->Data;
   Channel->Sample.SampleType   = sample->SampleType;
   Channel->Sample.SampleLength = sample->SampleLength;
   Channel->Sample.LoopMode     = sample->LoopMode;
   Channel->Sample.Loop1Start   = sample->Loop1Start;
   Channel->Sample.Loop1End     = sample->Loop1End;
   Channel->Sample.Loop1Type    = sample->Loop1Type;
   Channel->Sample.Loop2Start   = sample->Loop2Start;
   Channel->Sample.Loop2End     = sample->Loop2End;
   Channel->Sample.Loop2Type    = sample->Loop2Type;
   Channel->Flags &= ~CHF_CHANGED;

   if (Channel->Sample.LoopMode IS LOOP_AMIGA) {
      // Looping - start playback from loop beginning
      Channel->Position    = Channel->Sample.Loop1Start;
      Channel->PositionLow = 0;
      return false;
   }

   // Not looping - finish the sample
   Channel->State = CHS_FINISHED;
   return true;
}

//********************************************************************************************************************

static bool handle_sample_end(extAudio *Self, AudioChannel *Channel)
{
   parasol::Log log("Audio");
   struct acRead read;
   ULONG lpStart, lpEnd;
   LONG lpType, n;

   if (!Channel) return false;

   if (Channel->LoopIndex IS 2) {
      lpStart = Channel->Sample.Loop2Start;
      lpEnd   = Channel->Sample.Loop2End;
      lpType  = Channel->Sample.Loop2Type;
   }
   else {
      lpStart = Channel->Sample.Loop1Start;
      lpEnd   = Channel->Sample.Loop1End;
      lpType  = Channel->Sample.Loop1Type;
   }

   if (!lpType) { // No loop - did we reach sample end?
      if (Channel->Position >= (ULONG)Channel->Sample.SampleLength) {
         if ((Channel->Flags & CHF_CHANGED) and
             ((Channel->Sample.LoopMode IS LOOP_AMIGA) or (Channel->Sample.LoopMode IS LOOP_AMIGA_NONE)) and
             ((Self->Samples[Channel->SampleHandle-1].LoopMode IS LOOP_AMIGA) or (Self->Samples[Channel->SampleHandle-1].LoopMode IS LOOP_AMIGA_NONE))) {
            return AmigaChange(Self, Channel);
         }

         // No sample change - we are finished
         Channel->State = CHS_FINISHED;
         return true;
      }
      else return false;
   }

   if (Channel->Flags & CHF_BACKWARD) {
      // Going backwards - did we reach loop start? (signed comparison takes care of possible wraparound)
      if (((LONG)Channel->Position < (LONG)lpStart) or ((Channel->Position IS lpStart) and (Channel->PositionLow IS 0)) ) {
         Channel->Flags &= ~CHF_BACKWARD;
         n = ((((LONG)lpStart) - ((LONG) Channel->Position)) << 16) - Channel->PositionLow - 1;
         // -1 is compensation for the fudge factor at loop end, see below
         Channel->Position = (ULONG) ((LONG)lpStart) + (n>>16);
         Channel->PositionLow = n;

         // Don't die on overshort loops
         if (Channel->Position >= lpEnd) {
            Channel->Position = lpStart;
            return true;
         }
      }
      return false;
   }

   // Going forward - did we reach loop end?

   if (Channel->Position >= lpEnd) { // Stream handling
      if (Channel->Sample.StreamID) {
         parasol::ScopedObjectLock<BaseClass> stream(Channel->Sample.StreamID, 3000);

         if (stream.granted()) {
            // Read the next set of stream data into our sample buffer

            read.Buffer = Channel->Sample.Data;
            read.Length = Channel->Sample.BufferLength;
            if (Action(AC_Read, *stream, &read) IS ERR_Okay) {
               // Increment the known stream position
               Channel->Sample.StreamPos += read.Result;

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

                  if ((Channel->Sample.SampleType IS SFM_U8_BIT_STEREO) or (Channel->Sample.SampleType IS SFM_U8_BIT_MONO)) {
                     for (i=0; i < buffersize; i++) buffer[i] = 0x80;
                  }
                  else ClearMemory(buffer, buffersize);
               }
*/

               // Loop back to the beginning of the stream if necessary

               if ((read.Result <= 0) or (Channel->Sample.StreamPos >= Channel->Sample.StreamLength)) {
                  if (Channel->Sample.Loop2Type) {
                     acSeek(*stream, (DOUBLE)(Channel->Sample.SeekStart + Channel->Sample.Loop2Start), SEEK_START);
                     Channel->Sample.StreamPos = 0;
                  }
                  else Channel->State = CHS_FINISHED;
               }
            }
            else {
               log.warning("Failed to stream data from object #%d.", stream.obj->UID);
               Channel->State = CHS_FINISHED;
            }
         }
         else {
            log.msg("Stream object %d has been lost.", Channel->Sample.StreamID);
            Channel->Sample.StreamID = 0;
         }
      }

      // Check for ALE sample change

      if ((Channel->Flags & CHF_CHANGED) and
          ((Channel->Sample.LoopMode IS LOOP_AMIGA) or (Channel->Sample.LoopMode IS LOOP_AMIGA_NONE)) and
         ((Self->Samples[Channel->SampleHandle-1].LoopMode IS LOOP_AMIGA) or (Self->Samples[Channel->SampleHandle-1].LoopMode IS LOOP_AMIGA_NONE))) {
         return AmigaChange(Self, Channel);
      }

      // Go to the second loop if the sound has been released

      if ((Channel->LoopIndex IS 1) and (Channel->State IS CHS_RELEASED)) {
         Channel->LoopIndex = 2;
         return false;
      }

      if (lpType IS LTYPE_BIDIRECTIONAL ) {
         // Bidirectional loop - change direction
         Channel->Flags |= CHF_BACKWARD;
         n = ((Channel->Position - lpEnd) << 16) + Channel->PositionLow + 1;

         // +1 is a fudge factor to make sure we'll access the correct samples all the time - a similar adjustment is
         // also done at the other end of the loop. This screws up interpolation a little when sample rate IS mixing
         // rate, but little enough that it can't be heard.

         if (lpEnd < 0x10000) {
            Channel->Position    = ((lpEnd << 16) - n)>>16;
            Channel->PositionLow = (lpEnd << 16) - n;
         }
         else {
            Channel->Position    = ((0xFFFF0000 - n)>>16) + (lpEnd - 0xFFFF);
            Channel->PositionLow = 0xFFFF0000 - n;
         }

         if (Channel->Position <= lpStart) { // Don't die on overshort loops
            Channel->Position = lpEnd;
            return true;
         }
         return false;
      }
      else { // Unidirectional loop - just loop to the beginning
         Channel->Position = lpStart + (Channel->Position - lpEnd);

         if (Channel->Position >= lpEnd) { // Don't die on overshort loops
            Channel->Position = lpStart;
            return true;
         }

         return false;
      }
   }

   return false;
}

//********************************************************************************************************************

static void calc_volumes(extAudio *Self, ChannelSet *MasterChannel, AudioChannel *Channel)
{
   DOUBLE mastervol;
   if (Self->Mute) mastervol = 0;
   else if (Self->Flags & ADF_SYSTEM_WIDE) mastervol = 1.0;
   else mastervol = glGlobalVolume;

   MixLeftVolFloat  = mastervol * Channel->LVolume * glTaskVolume;
   MixRightVolFloat = mastervol * Channel->RVolume * glTaskVolume;

   if (Self->Stereo IS FALSE) {
      if ((Channel->Sample.SampleType IS SFM_U8_BIT_STEREO) or (Channel->Sample.SampleType IS SFM_S16_BIT_STEREO)) {
         MixLeftVolFloat  *= (1.0 / 2.0);
         MixRightVolFloat *= (1.0 / 2.0);
      }
   }
}

//********************************************************************************************************************

static void RampVolume(AudioChannel *Channel)
{
   bool cont = false;

   if (Channel->LVolume < Channel->LVolumeTarget) {
      Channel->LVolume += RAMPSPEED;
      if (Channel->LVolume >= Channel->LVolumeTarget) Channel->LVolume = Channel->LVolumeTarget;
      else cont = true;
   }
   else if (Channel->LVolume > Channel->LVolumeTarget) {
      Channel->LVolume -= RAMPSPEED;
      if (Channel->LVolume <= Channel->LVolumeTarget) Channel->LVolume = Channel->LVolumeTarget;
      else cont = true;
   }

   if (Channel->RVolume < Channel->RVolumeTarget) {
      Channel->RVolume += RAMPSPEED;
      if (Channel->RVolume >= Channel->RVolumeTarget) Channel->RVolume = Channel->RVolumeTarget;
      else cont = true;
   }
   else if (Channel->RVolume > Channel->RVolumeTarget) {
      Channel->RVolume -= RAMPSPEED;
      if (Channel->RVolume <= Channel->RVolumeTarget) Channel->RVolume = Channel->RVolumeTarget;
      else cont = true;
   }

   if (cont) Channel->Flags |= CHF_VOL_RAMP;
   else Channel->Flags &= ~CHF_VOL_RAMP;
}

//********************************************************************************************************************
// Mixes sound data to destination

ERROR mix_data(extAudio *Self, LONG Elements, APTR Destination)
{
   parasol::Log log("Audio");

   glMixBuffer = Self->MixBuffer;
   auto glMixDest = (UBYTE *)Destination;
   while (Elements) {
      // Mix only as much as we can fit in our mixing buffer

      auto window = (Elements > Self->MixElements) ? Self->MixElements : Elements;

      // Clear the mix buffer, then mix all channels to the buffer

      ClearMemory(Self->MixBuffer, sizeof(FLOAT) * (Self->Stereo ? (window<<1) : window));

      for (auto n=0; n < ARRAYSIZE(Self->Channels); n++) {
         if (Self->Channels[n].Channel) {
            for (auto i=0; i < Self->Channels[n].Actual; i++) {
               if (mix_channel(Self, Self->Channels+n, Self->Channels[n].Channel + i, window, Self->MixBuffer) != ERR_Okay) {
                  log.warning("Failed to mix channel %d.", i);
               }
            }
         }
      }

      // Do post-processing

      if (Self->Flags & (ADF_FILTER_LOW|ADF_FILTER_HIGH)) {
         if (Self->Stereo) filter_float_stereo(Self, Self->MixBuffer, window, NULL);
         else filter_float_mono(Self, Self->MixBuffer, window, NULL);
      }

      // Convert the floating point data to the correct output format

      if (Self->BitDepth IS 24) {

      }
      else if (Self->BitDepth IS 16) {
         if (Self->Stereo) convert_float16(window<<1, (WORD *)glMixDest);
         else convert_float16(window, (WORD *)glMixDest);
      }
      else {
         if (Self->Stereo) convert_float8(window<<1, (UBYTE *)glMixDest);
         else convert_float8(window, (UBYTE *)glMixDest);
      }

      glMixDest += window * Self->SampleBitSize;
      Elements -= window;
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR mix_channel(extAudio *Self, ChannelSet *MasterChannel, AudioChannel *Channel,
   LONG numSamples, APTR dest)
{
   ULONG sampleSize, prevMix = 1;

   // Check that we have something to mix

   if ((!Channel->Frequency) or (!Channel->Sample.Data) or (Channel->Sample.SampleLength <= 0)) return ERR_Okay;

   // Calculate resampling step (16.16 fixed point)

   LONG step = (((Channel->Frequency / Self->OutputRate) << 16) + ((Channel->Frequency % Self->OutputRate) << 16) / Self->OutputRate);

   glMixDest = (UBYTE *)dest;
   while (numSamples) {
      if (Channel->State IS CHS_STOPPED) return ERR_Okay;
      else if (Channel->State IS CHS_FINISHED) return ERR_Okay;

      switch (Channel->Sample.SampleType) {
         case SFM_U8_BIT_STEREO:
         case SFM_S16_BIT_MONO: sampleSize = 2; break;
         case SFM_S16_BIT_STEREO: sampleSize = 4; break;
         default: sampleSize = 1; break;
      }

      LONG nextoffset;
      ULONG sue = samples_until_end(Self, Channel, &nextoffset);

      // Calculate the number of destination samples (note rounding)

      ULONG mixUntilEnd = sue / step;
      if (sue % step) mixUntilEnd++;

      ULONG mixNow;
      if (mixUntilEnd > (ULONG)numSamples ) mixNow = numSamples;
      else mixNow = mixUntilEnd;
      numSamples -= mixNow;

      // This should never happen, but prevents any chance of an infinite loop.

      if ((mixNow IS 0) and (prevMix IS 0)) return ERR_Okay;

      prevMix = mixNow;

      if (mixNow) {
         MixSrcPos = Channel->PositionLow;
         MixSample = Channel->Sample.Data + (sampleSize * Channel->Position); // source of sample data to mix into destination

         auto mixRoutine = Self->MixRoutines[Channel->Sample.SampleType];

         if (Channel->Flags & CHF_BACKWARD) MixStep = -step;
         else MixStep = step;

         // Do possible volume ramping

         while ((Channel->Flags & CHF_VOL_RAMP) and (mixNow)) {
             calc_volumes(Self, MasterChannel, Channel);
             mixRoutine(1, nextoffset);
             mixNow--;
             RampVolume(Channel);
         }

         if ((Channel->LVolume IS 0) and (Channel->RVolume IS 0)) {
            // If the volume is zero we can just increment the position and not mix anything
            MixSrcPos += mixNow * MixStep;
            if (Channel->State IS CHS_FADE_OUT) {
               Channel->State = CHS_STOPPED;
               Channel->Flags &= ~CHF_VOL_RAMP;
            }
         }
         else {
            calc_volumes(Self, MasterChannel, Channel);

            // Ensure proper alignment and do all mixing if nextoffset != 1

            ULONG num;
            if ((mixNow) and ((nextoffset != 1) or ((((MAXINT)glMixDest) % 1) != 0))) {
               if (nextoffset != 1) num = mixNow;
               else {
                  num = 1 - (((MAXINT)glMixDest) % 1);
                  if (num > mixNow) num = mixNow;
               }
               mixRoutine(num, nextoffset);
               mixNow -= num;
            }

            // Do the main mixing loop
            if (mixNow > 0) {
               num = mixNow;
               mixRoutine(num, 1);
               mixNow -= num;
            }

            // Mix what's left
            if (mixNow) mixRoutine(mixNow, 1);
         }

         // Put changed parts of state back to channel structure
         Channel->PositionLow = MixSrcPos;
         Channel->Position = (MixSrcPos>>16) + ((MixSample - Channel->Sample.Data) / sampleSize);
      }

      // Check if we reached loop/sample/whatever end

      if (handle_sample_end(Self, Channel)) return ERR_Okay;
   }

   return ERR_Okay;
}

//********************************************************************************************************************
// Mono output filtering routines.

static void filter_float_mono(extAudio *Self, void *data, LONG numSamples, void *dummy)
{
   static DOUBLE d1l=0, d2l=0;
   auto p = (FLOAT *)data;
   if (Self->Flags & ADF_FILTER_LOW) {
      while (numSamples) {
         DOUBLE s = (d1l + 2.0 * (*p)) * (1.0 / 3.0);
         d1l = *p;
         *(p++) = s;
         numSamples--;
      }
   }
   else if (Self->Flags & ADF_FILTER_HIGH) {
      while (numSamples) {
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

static void filter_float_stereo(extAudio *Self, void *data, LONG numSamples, void *dummy)
{
   static DOUBLE d1l = 0, d1r = 0, d2l = 0, d2r = 0;

   auto p = (FLOAT *)data;
   if (Self->Flags & ADF_FILTER_LOW) {
      while (numSamples) {
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
      while (numSamples) {
         DOUBLE s = (d1l + 3.0 * d2l + 4.0 * (*p)) * (1.0 / 8.0);
         d1l = d2l;
         d2l = *p;
         *(p++) = s;

         s = (d1r + 3.0*d2r + 4.0*(*p)) * (1.0 / 8.0);
         d1r = d2r;
         d2r = *p;
         *(p++) = s;

         numSamples--;
      }
   }
}

//********************************************************************************************************************
// MONO MIXING

//********************************************************************************************************************
// Mix 8-bit mono samples.

static void mixmf8Mono(unsigned numSamples, LONG nextSampleOffset)
{
   auto dest = (FLOAT *)glMixDest;
   UBYTE *sample = MixSample;
   int  mixPos = MixSrcPos;

   while (numSamples) {
      *(dest++) += MixLeftVolFloat * (256 * (sample[mixPos>>16]-128));
      mixPos += MixStep;
      numSamples--;
   }

   MixSrcPos = mixPos;
   glMixDest = (UBYTE*) dest;
}

// Mix 16-bit mono samples:

static void mixmf16Mono(unsigned numSamples, int nextSampleOffset)
{
    auto dest = (FLOAT *)glMixDest;
    auto sample = (WORD *)MixSample;
    int mixPos = MixSrcPos;

    while (numSamples) {
        *(dest++) += MixLeftVolFloat * ((FLOAT) sample[mixPos>>16]);
        mixPos += MixStep;
        numSamples--;
    }

    MixSrcPos = mixPos;
    glMixDest = (UBYTE*) dest;
}

//********************************************************************************************************************
// Mix 8-bit stereo samples.

static void mixmf8Stereo(unsigned numSamples, LONG nextSampleOffset)
{
   auto dest = (FLOAT *)glMixDest;
   UBYTE *sample = MixSample;
   while (numSamples) {
      *(dest++) += MixLeftVolFloat * (256 * (sample[2 * (MixSrcPos>>16)] - 128) +
         256 * (sample[2 * (MixSrcPos>>16) + 1] - 128) );
      MixSrcPos += MixStep;
      numSamples--;
   }
   glMixDest = (UBYTE*) dest;
}

//********************************************************************************************************************
// Mix 16-bit stereo samples.

static void mixmf16Stereo(unsigned numSamples, LONG nextSampleOffset)
{
   auto dest = (FLOAT *)glMixDest;
   auto sample = (WORD *)MixSample;
   while ( numSamples ) {
       *(dest++) += MixLeftVolFloat * (((FLOAT) sample[2 *(MixSrcPos>>16)]) + ((FLOAT) sample[2 * (MixSrcPos>>16) + 1])) ;
       MixSrcPos += MixStep;
       numSamples--;
   }
   glMixDest = (UBYTE*) dest;
}

MixRoutine MixMonoFloat[5] = {
   NULL,            // no sample
   &mixmf8Mono,     // 8-bit mono
   &mixmf16Mono,    // 16-bit mono
   &mixmf8Stereo,   // 8-bit stereo
   &mixmf16Stereo,  // 16-bit stereo
};

//********************************************************************************************************************
// STEREO MIXING

//********************************************************************************************************************
// Mix 8-bit mono samples.

static void mixsf8Mono(unsigned numSamples, LONG nextSampleOffset)
{
   auto dest = (FLOAT *)glMixDest;
   UBYTE *sample = MixSample;
   LONG mixPos = MixSrcPos;

   while (numSamples) {
      const DOUBLE s = 256 * (sample[mixPos>>16] - 128);
      *dest += MixLeftVolFloat * s;
      *(dest+1) += MixRightVolFloat * s;
      dest += 2;
      mixPos += MixStep;
      numSamples--;
   }

   MixSrcPos = mixPos;
   glMixDest = (UBYTE*) dest;
}

//********************************************************************************************************************
// Mix 16-bit mono samples.

static void mixsf16Mono(unsigned numSamples, LONG nextSampleOffset)
{
   auto dest = (FLOAT *)glMixDest;
   auto sample = (WORD *)MixSample;
   LONG mixPos = MixSrcPos;

   while (numSamples) {
      const DOUBLE s = (DOUBLE)sample[mixPos>>16];
      *dest += MixLeftVolFloat * s;
      *(dest+1) += MixRightVolFloat * s;
      dest += 2;
      mixPos += MixStep;
      numSamples--;
   }

   MixSrcPos = mixPos;
   glMixDest = (UBYTE *)dest;
}

//********************************************************************************************************************
// Mix 8-bit stereo samples.

static void mixsf8Stereo(unsigned numSamples, LONG nextSampleOffset)
{
   auto dest = (FLOAT *)glMixDest;
   UBYTE *sample = MixSample;

   while (numSamples) {
      *dest += MixLeftVolFloat * 256 * (sample[2*(MixSrcPos>>16)] - 128);
      *(dest+1) += MixRightVolFloat * 256 * (sample[2*(MixSrcPos>>16) + 1] - 128);
      dest += 2;
      MixSrcPos += MixStep;
      numSamples--;
   }

   glMixDest = (UBYTE*) dest;
}

// Mix 16-bit stereo samples

static void mixsf16Stereo(unsigned numSamples, int nextSampleOffset)
{
   auto dest = (FLOAT *)glMixDest;
   auto sample = (WORD *)MixSample;
   while (numSamples) {
      *dest += MixLeftVolFloat * ((DOUBLE) sample[2*(MixSrcPos>>16)]);
      *(dest+1) += MixRightVolFloat * ((DOUBLE) sample[2*(MixSrcPos>>16) + 1]);
      dest += 2;
      MixSrcPos += MixStep;
      numSamples--;
   }

   glMixDest = (UBYTE*) dest;
}

MixRoutine MixStereoFloat[5] = {
   NULL,            // no sample
   &mixsf8Mono,     // 8-bit mono
   &mixsf16Mono,    // 16-bit mono
   &mixsf8Stereo,   // 8-bit stereo
   &mixsf16Stereo,  // 16-bit stereo
};

//********************************************************************************************************************
// MONO-INTERPOLATED MIXING

//********************************************************************************************************************
// Mix 8-bit mono samples.

static void mixmi8Mono(unsigned numSamples, LONG nextSampleOffset)
{
   auto dest = (FLOAT *)glMixDest;
   UBYTE *sample = MixSample;
   LONG  mixPos = MixSrcPos;
   DOUBLE volMul = MixLeftVolFloat * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
      while (numSamples) {
         const DOUBLE s0 = b2f(sample[mixPos>>16]);
         const DOUBLE s1 = b2f(sample[(mixPos>>16) + nextSampleOffset]);
         *(dest++) += volMul * (((DOUBLE) (65536 - (mixPos & 0xFFFF))) * s0 + ((DOUBLE) (mixPos & 0xFFFF)) * s1);
         mixPos += MixStep;
         numSamples--;
      }
   }
   else while (numSamples) {
      const DOUBLE s0 = b2f(sample[mixPos>>16]);
      const DOUBLE s1 = b2f(sample[(mixPos>>16) + 1]);
      *(dest++) += volMul * (((DOUBLE) (65536 - (mixPos & 0xFFFF))) * s0 + ((DOUBLE) (mixPos & 0xFFFF)) * s1);
      mixPos += MixStep;
      numSamples--;
   }

   MixSrcPos = mixPos;
   glMixDest = (UBYTE *)dest;
}

//********************************************************************************************************************
// Mix 16-bit mono samples.

static void mixmi16Mono(unsigned numSamples, LONG nextSampleOffset)
{
   auto dest = (FLOAT *)glMixDest;
   auto sample = (WORD *)MixSample;
   LONG mixPos = MixSrcPos;
   const DOUBLE volMul = MixLeftVolFloat * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
      while (numSamples) {
         const DOUBLE s0 = sample[mixPos>>16];
         const DOUBLE s1 = sample[(mixPos>>16) + nextSampleOffset];
         *(dest++) += volMul * (((DOUBLE) (65536 - (mixPos & 0xFFFF))) * s0 + ((DOUBLE) (mixPos & 0xFFFF)) * s1);
         mixPos += MixStep;
         numSamples--;
      }
   }
   else while (numSamples) {
      const DOUBLE s0 = sample[mixPos>>16];
      const DOUBLE s1 = sample[(mixPos>>16) + 1];
      *(dest++) += volMul * (((DOUBLE) (65536 - (mixPos & 0xFFFF))) * s0 + ((DOUBLE) (mixPos & 0xFFFF)) * s1);
      mixPos += MixStep;
      numSamples--;
   }

   MixSrcPos = mixPos;
   glMixDest = (UBYTE *)dest;
}

//********************************************************************************************************************
// Mix 8-bit stereo samples.

static void mixmi8Stereo(unsigned numSamples, LONG nextSampleOffset)
{
   auto dest = (FLOAT *)glMixDest;
   UBYTE *sample = MixSample;
   LONG mixPos = MixSrcPos;
   const DOUBLE volMul = MixLeftVolFloat * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
       while (numSamples) {
         DOUBLE i0 = 65536 - (mixPos & 0xFFFF);
         DOUBLE i1 = mixPos & 0xFFFF;
         *(dest++) += volMul *
            ((i0 * b2f(sample[2 * (mixPos>>16)]) + i1 * b2f(sample[2 * (mixPos>>16) + 2 * nextSampleOffset])) +
             (i0 * b2f(sample[2 * (mixPos>>16) + 1]) + i1 * b2f(sample[2 * (mixPos>>16) + 2 * nextSampleOffset + 1])));
         mixPos += MixStep;
         numSamples--;
      }
   }
   else while (numSamples) {
      DOUBLE i0 = (DOUBLE)(65536 - (mixPos & 0xFFFF));
      DOUBLE i1 = (DOUBLE)(mixPos & 0xFFFF);
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
// Mix 16-bit stereo samples.

static void mixmi16Stereo(unsigned numSamples, LONG nextSampleOffset)
{
   auto dest = (FLOAT *)glMixDest;
   auto sample = (WORD *)MixSample;
   LONG mixPos = MixSrcPos;
   DOUBLE volMul = MixLeftVolFloat * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
      while (numSamples) {
         DOUBLE i0 = (DOUBLE) (65536 - (mixPos & 0xFFFF));
         DOUBLE i1 = (DOUBLE) (mixPos & 0xFFFF);
         *(dest++) += volMul *
             ((i0 * ((DOUBLE) sample[2 * (mixPos>>16)]) + i1 * ((DOUBLE)sample[2 * (mixPos>>16) + 2 * nextSampleOffset])) +
              (i0 * ((DOUBLE) sample[2 * (mixPos>>16) + 1]) + i1 * ((DOUBLE)sample[2 * (mixPos>>16) + 2 * nextSampleOffset + 1])));
         mixPos += MixStep;
         numSamples--;
      }
   }
   else while (numSamples) {
      DOUBLE i0 = (DOUBLE)(65536 - (mixPos & 0xFFFF));
      DOUBLE i1 = (DOUBLE)(mixPos & 0xFFFF);
      *(dest++) += volMul *
          ((i0 * ((DOUBLE)sample[2 * (mixPos>>16)]) + i1 * ((DOUBLE)sample[2 * (mixPos>>16) + 2])) +
           (i0 * ((DOUBLE)sample[2 * (mixPos>>16) + 1]) + i1 * ((DOUBLE)sample[2 * (mixPos>>16) + 3])));
      mixPos += MixStep;
      numSamples--;
   }

   MixSrcPos = mixPos;
   glMixDest = (UBYTE *)dest;
}

MixRoutine MixMonoFloatInterp[5] = {
   NULL,           // no sample
   &mixmi8Mono,    // 8-bit mono
   &mixmi16Mono,   // 16-bit mono
   &mixmi8Stereo,  // 8-bit stereo
   &mixmi16Stereo, // 16-bit stereo
};

//********************************************************************************************************************
// STEREO-INTERPOLATED MIXING

//********************************************************************************************************************
// Mix 8-bit mono samples.

static void mixsi8Mono(unsigned numSamples, LONG nextSampleOffset)
{
   auto dest = (FLOAT *)glMixDest;
   UBYTE *sample = MixSample;
   LONG  mixPos = MixSrcPos;
   DOUBLE volMulL = MixLeftVolFloat * (1.0 / 65536.0);
   DOUBLE volMulR = MixRightVolFloat * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
      while (numSamples) {
         DOUBLE s0 = b2f(sample[mixPos>>16]);
         DOUBLE s1 = b2f(sample[(mixPos>>16) + nextSampleOffset]);
         DOUBLE s = (((DOUBLE) (65536 - (mixPos & 0xFFFF))) * s0 + ((DOUBLE) (mixPos & 0xFFFF)) * s1);
         *dest += volMulL * s;
         *(dest+1) += volMulR * s;
         dest += 2;
         mixPos += MixStep;
         numSamples--;
      }
   }
   else while (numSamples) {
      DOUBLE s0 = b2f(sample[mixPos>>16]);
      DOUBLE s1 = b2f(sample[(mixPos>>16) + 1]);
      DOUBLE s = (((DOUBLE)(65536 - (mixPos & 0xFFFF))) * s0 + ((DOUBLE) (mixPos & 0xFFFF)) * s1);
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
// Mix 16-bit mono samples.

static void mixsi16Mono(unsigned numSamples, LONG nextSampleOffset)
{
   auto dest = (FLOAT *)glMixDest;
   auto sample = (WORD *)MixSample;
   LONG mixPos = MixSrcPos;
   DOUBLE volMulL = MixLeftVolFloat * (1.0 / 65536.0);
   DOUBLE volMulR = MixRightVolFloat * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
      while (numSamples) {
         DOUBLE s0 = (DOUBLE)sample[mixPos>>16];
         DOUBLE s1 = (DOUBLE)sample[(mixPos>>16) + nextSampleOffset];
         DOUBLE s = (((DOUBLE)(65536 - (mixPos & 0xFFFF))) * s0 + ((DOUBLE)(mixPos & 0xFFFF)) * s1);
         *dest += volMulL * s;
         *(dest+1) += volMulR * s;
         dest += 2;
         mixPos += MixStep;
         numSamples--;
      }
   }
   else {
      while (numSamples) {
         DOUBLE s0 = (DOUBLE)sample[mixPos>>16];
         DOUBLE s1 = (DOUBLE)sample[(mixPos>>16) + 1];
         DOUBLE s = (((DOUBLE)(65536 - (mixPos & 0xFFFF))) * s0 + ((DOUBLE) (mixPos & 0xFFFF)) * s1);
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
// Mix 8-bit stereo samples.

static void mixsi8Stereo(unsigned numSamples, LONG nextSampleOffset)
{
   auto dest = (FLOAT *)glMixDest;
   UBYTE *sample = MixSample;
   LONG mixPos = MixSrcPos;
   DOUBLE volMulL = MixLeftVolFloat * (1.0 / 65536.0);
   DOUBLE volMulR = MixRightVolFloat * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
      while (numSamples) {
         DOUBLE i0 = (DOUBLE) (65536 - (mixPos & 0xFFFF));
         DOUBLE i1 = (DOUBLE) (mixPos & 0xFFFF);
         *dest += volMulL * (i0 * b2f(sample[2 * (mixPos>>16)]) + i1 * b2f(sample[2 * (mixPos>>16) + 2 * nextSampleOffset]));
         *(dest+1) += volMulR * (i0 * b2f(sample[2 * (mixPos>>16) + 1]) + i1 * b2f(sample[2 * (mixPos>>16) + 2 * nextSampleOffset + 1]));
         dest += 2;
         mixPos += MixStep;
         numSamples--;
      }
   }
   else {
      while (numSamples)  {
         auto i0 = DOUBLE(65536 - (mixPos & 0xFFFF));
         auto i1 = DOUBLE(mixPos & 0xFFFF);
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
// Mix 16-bit stereo samples.

static void mixsi16Stereo(unsigned numSamples, LONG nextSampleOffset)
{
   auto dest = (FLOAT *)glMixDest;
   auto sample = (WORD *)MixSample;
   LONG mixPos = MixSrcPos;
   const DOUBLE volMulL = MixLeftVolFloat * (1.0 / 65536.0);
   const DOUBLE volMulR = MixRightVolFloat * (1.0 / 65536.0);

   if (nextSampleOffset != 1) {
      while (numSamples) {
         auto i0 = DOUBLE(65536 - (mixPos & 0xFFFF));
         auto i1 = DOUBLE(mixPos & 0xFFFF);
         *dest += volMulL * (i0 * ((DOUBLE) sample[2 * (mixPos>>16)]) + i1 * ((DOUBLE) sample[2 * (mixPos>>16) + 2 * nextSampleOffset]));
         *(dest+1) += volMulR * (i0 * ((DOUBLE) sample[2 * (mixPos>>16) + 1]) +
            i1 * ((DOUBLE) sample[2 * (mixPos>>16) + 2 * nextSampleOffset + 1]));
         dest += 2;
         mixPos += MixStep;
         numSamples--;
      }
   }
   else {
      while (numSamples) {
         auto i0 = DOUBLE(65536 - (mixPos & 0xFFFF));
         auto i1 = DOUBLE(mixPos & 0xFFFF);
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

MixRoutine MixStereoFloatInterp[5] = {
   NULL,            // no sample
   &mixsi8Mono,     // 8-bit mono
   &mixsi16Mono,    // 16-bit mono
   &mixsi8Stereo,   // 8-bit stereo
   &mixsi16Stereo   // 16-bit stereo
};
