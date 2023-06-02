
#define RAMPSPEED 0.01  // Default ramping speed - volume steps per output sample.  Keeping this value very low prevents clicks from occurring

static LONG MixStep;
static FLOAT *glMixDest = NULL; // TODO: Global requires deprecation

static void filter_float_mono(extAudio *, FLOAT *, LONG);
static void filter_float_stereo(extAudio *, FLOAT *, LONG);
static void mix_channel(extAudio *, AudioChannel &, LONG, APTR);
static ERROR mix_data(extAudio *, LONG, APTR);
static ERROR process_commands(extAudio *, SAMPLE);

//********************************************************************************************************************

static void audio_stopped_event(extAudio &Audio, LONG SampleHandle)
{
   auto &sample = Audio.Samples[SampleHandle];
   if (sample.OnStop.Type IS CALL_STDC) {
      pf::SwitchContext context(sample.OnStop.StdC.Context);
      auto routine = (void (*)(extAudio *, LONG))sample.OnStop.StdC.Routine;
      routine(&Audio, SampleHandle);
   }
   else if (sample.OnStop.Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = sample.OnStop.Script.Script)) {
         const ScriptArg args[] = {
            { "Audio", FD_OBJECTPTR, { .Address = &Audio } },
            { "Handle", FD_LONG, { .Long = SampleHandle } }
         };
         ERROR error;
         scCallback(script, sample.OnStop.Script.ProcedureID, args, ARRAYSIZE(args), &error);
      }
   }
}

//********************************************************************************************************************
// The callback must return the number of bytes written to the buffer.

static BYTELEN fill_stream_buffer(LONG Handle, AudioSample &Sample, LONG Offset)
{
   if (Sample.Callback.Type IS CALL_STDC) {
      pf::SwitchContext context(Sample.Callback.StdC.Context);
      auto routine = (BYTELEN (*)(LONG, LONG, UBYTE *, LONG))Sample.Callback.StdC.Routine;
      return routine(Handle, Offset, Sample.Data, Sample.SampleLength<<sample_shift(Sample.SampleType));
   }
   else if (Sample.Callback.Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = Sample.Callback.Script.Script)) {
         const ScriptArg args[] = {
            { "Handle", FD_LONG,    { .Long = Handle } },
            { "Offset", FD_LONG,    { .Long = Offset } },
            { "Buffer", FD_BUFFER,  { .Address = Sample.Data } },
            { "Length", FD_BUFSIZE|FD_LONG, { .Long = Sample.SampleLength<<sample_shift(Sample.SampleType) } }
         };

         LONG result = 0;
         ERROR error;
         if (scCallback(script, Sample.Callback.Script.ProcedureID, args, ARRAYSIZE(args), &result)) error = ERR_Failed;
         return BYTELEN(result);
      }
   }

   return BYTELEN(0);
}

//********************************************************************************************************************

static ERROR get_mix_amount(extAudio *Self, SAMPLE *MixLeft)
{
   auto ml = SAMPLE(0x7fffffff);
   for (LONG i=1; i < (LONG)Self->Sets.size(); i++) {
      if ((Self->Sets[i].MixLeft > 0) and (Self->Sets[i].MixLeft < ml)) {
         ml = Self->Sets[i].MixLeft;
      }
   }

   *MixLeft = ml;
   return ERR_Okay;
}

//********************************************************************************************************************
// Functions for use by dsound.cpp

#ifdef _WIN32
extern "C" int dsReadData(BaseClass *Self, void *Buffer, int Length) {
   if (Self->Class->BaseClassID IS ID_SOUND) {
      LONG result;
      if (((objSound *)Self)->read(Buffer, Length, &result)) return 0;
      else return result;
   }
   else if (Self->Class->BaseClassID IS ID_AUDIO) {
      auto space_left = SAMPLE(Length / ((extAudio *)Self)->DriverBitSize); // Convert to number of samples

      SAMPLE mix_left;
      while (space_left > 0) {
         // Scan channels to check if an update rate is going to be met

         get_mix_amount((extAudio *)Self, &mix_left);

         SAMPLE elements = (mix_left < space_left) ? mix_left : space_left;

         if (mix_data((extAudio *)Self, elements, Buffer) != ERR_Okay) break;

         // Drop the mix amount.  This may also update buffered channels for the next round

         process_commands((extAudio *)Self, elements);

         Buffer = (UBYTE *)Buffer + (elements * ((extAudio *)Self)->DriverBitSize);
         space_left -= elements;
      }

      return Length;
   }
   else return 0;
}

extern "C" void dsSeekData(BaseClass *Self, LONG Offset) {
   if (Self->Class->BaseClassID IS ID_SOUND) {
      ((objSound *)Self)->seek(Offset, SEEK::START);
   }
   else return; // Seeking not applicable for the Audio class.
}
#endif

//********************************************************************************************************************
// Defines the L/RVolume and Ramping values for an AudioChannel.  These values are derived from the Volume and Pan.

static ERROR set_channel_volume(extAudio *Self, AudioChannel *Channel)
{
   pf::Log log(__FUNCTION__);

   if ((!Self) or (!Channel)) return log.warning(ERR_NullArgs);

   if (Channel->Volume > 1.0) Channel->Volume = 1.0;
   else if (Channel->Volume < 0) Channel->Volume = 0;

   if (Channel->Pan < -1.0) Channel->Pan = -1.0;
   else if (Channel->Pan > 1.0) Channel->Pan = 1.0;

   // Convert the volume into left/right volume parameters

   DOUBLE leftvol, rightvol;
   if ((Channel->Flags & CHF::MUTE) != CHF::NIL) {
      leftvol  = 0;
      rightvol = 0;
   }
   else {
      leftvol  = Channel->Volume;
      rightvol = Channel->Volume;

      if (!Self->Stereo);
      else if (Channel->Pan < 0) rightvol = (Channel->Volume * (1.0 + Channel->Pan));
      else if (Channel->Pan > 0) leftvol  = (Channel->Volume * (1.0 - Channel->Pan));
   }

   // Start volume ramping if necessary

   Channel->Flags &= ~CHF::VOL_RAMP;
   if (((Self->Flags & ADF::OVER_SAMPLING) != ADF::NIL) and ((Self->Flags & ADF::VOL_RAMPING) != ADF::NIL)) {
      if ((Channel->LVolume != leftvol) or (Channel->LVolumeTarget != leftvol)) {
         Channel->Flags |= CHF::VOL_RAMP;
         Channel->LVolumeTarget = leftvol;
      }

      if ((Channel->RVolume != rightvol) or (Channel->RVolumeTarget != rightvol)) {
         Channel->Flags |= CHF::VOL_RAMP;
         Channel->RVolumeTarget = rightvol;
      }
   }
   else {
      Channel->LVolume       = leftvol;
      Channel->LVolumeTarget = leftvol;
      Channel->RVolume       = rightvol;
      Channel->RVolumeTarget = rightvol;
   }

   return ERR_Okay;
}

//********************************************************************************************************************
// Process as many command batches as possible that will fit within MixLeft.

ERROR process_commands(extAudio *Self, SAMPLE Elements)
{
   pf::Log log(__FUNCTION__);

   for (LONG index=1; index < (LONG)Self->Sets.size(); index++) {
      Self->Sets[index].MixLeft -= Elements;
      if (Self->Sets[index].MixLeft <= 0) {
         // Reset the amount of mixing elements left and execute the next set of channel commands

         Self->Sets[index].MixLeft = Self->MixLeft(Self->Sets[index].UpdateRate);

         if (Self->Sets[index].Commands.empty()) continue;

         LONG i;
         bool stop = false;
         auto &cmds = Self->Sets[index].Commands;
         for (i=0; (i < (LONG)cmds.size()) and (!stop); i++) {
            switch(cmds[i].CommandID) {
               case CMD::CONTINUE:     sndMixContinue(Self, cmds[i].Handle); break;
               case CMD::MUTE:         sndMixMute(Self, cmds[i].Handle, cmds[i].Data); break;
               case CMD::PLAY:         sndMixPlay(Self, cmds[i].Handle, cmds[i].Data); break;
               case CMD::FREQUENCY:    sndMixFrequency(Self, cmds[i].Handle, cmds[i].Data); break;
               case CMD::PAN:          sndMixPan(Self, cmds[i].Handle, cmds[i].Data); break;
               case CMD::RATE:         sndMixRate(Self, cmds[i].Handle, cmds[i].Data); break;
               case CMD::SAMPLE:       sndMixSample(Self, cmds[i].Handle, cmds[i].Data); break;
               case CMD::VOLUME:       sndMixVolume(Self, cmds[i].Handle, cmds[i].Data); break;
               case CMD::STOP:         sndMixStop(Self, cmds[i].Handle); break;
               case CMD::STOP_LOOPING: sndMixStopLoop(Self, cmds[i].Handle); break;
               case CMD::END_SEQUENCE: stop = true; break;

               default:
                  log.warning("Unrecognised command ID #%d at index %d.", LONG(cmds[i].CommandID), i);
                  break;
            }
         }

         if (i IS (LONG)cmds.size()) cmds.clear();
         else cmds.erase(cmds.begin(), cmds.begin()+i);
      }
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR audio_timer(extAudio *Self, LARGE Elapsed, LARGE CurrentTime)
{
#ifdef ALSA_ENABLED

   pf::Log log(__FUNCTION__);
   static WORD errcount = 0;

   // Check if we need to send out any OnStop events

   for (auto it=Self->MixTimers.begin(); it != Self->MixTimers.end(); ) {
      auto &mt = *it;
      if (CurrentTime > mt.Time) {
         audio_stopped_event(*Self, mt.SampleHandle);
         it = Self->MixTimers.erase(it);
      }
      else it++;
   }

   // Get the amount of bytes available for output

   SAMPLE space_left;
   if (Self->Handle) {
      space_left = SAMPLE(snd_pcm_avail_update(Self->Handle)); // Returns available space measured in samples
   }
   else if (Self->AudioBufferSize) { // Run in dummy mode - samples will be processed but not played
      space_left = SAMPLE(Self->AudioBufferSize / Self->DriverBitSize);
   }
   else {
      log.warning("ALSA not in an initialised state.");
      return ERR_Terminate;
   }

   // If the audio system is inactive or in a bad state, try to fix it.

   if (space_left < 0) {
      log.warning("avail_update() %s", snd_strerror(space_left));

      errcount++;
      if (!(errcount % 50)) {
         log.warning("Broken audio - attempting fix...");

         Self->deactivate();

         if (Self->activate() != ERR_Okay) {
            log.warning("Audio error is terminal, self-destructing...");
            SendMessage(MSGID_FREE, MSF::NIL, &Self->UID, sizeof(OBJECTID));
            return ERR_Failed;
         }
      }

      return ERR_Okay;
   }

   if (space_left > SAMPLE(Self->AudioBufferSize / Self->DriverBitSize)) {
      space_left = SAMPLE(Self->AudioBufferSize / Self->DriverBitSize);
   }

   // Fill our entire audio buffer with data to be sent to alsa

   auto space = space_left;
   UBYTE *buffer = Self->AudioBuffer;
   while (space_left) {
      // Scan channels to check if an update rate is going to be met

      SAMPLE mix_left;
      get_mix_amount(Self, &mix_left);

      SAMPLE elements = (mix_left < space_left) ? mix_left : space_left;

      // Produce the audio data

      if (mix_data(Self, elements, buffer) != ERR_Okay) break;

      // Drop the mix amount.  This may also update buffered channels for the next round

      process_commands(Self, elements);

      buffer = buffer + (elements * Self->DriverBitSize);
      space_left -= elements;
   }

   // Write the audio to alsa

   if (Self->Handle) {
      LONG err;
      if ((err = snd_pcm_writei(Self->Handle, Self->AudioBuffer, space)) < 0) {
         // If an EPIPE error is returned, a buffer underrun has probably occurred

         if (err IS -EPIPE) {
            log.msg("A buffer underrun has occurred.");

            snd_pcm_status_t *status;
            snd_pcm_status_alloca(&status);
            if (snd_pcm_status(Self->Handle, status) < 0) {
               return ERR_Okay;
            }

            auto code = snd_pcm_status_get_state(status);
            if (code IS SND_PCM_STATE_XRUN) {
               // Reset the output device
               if ((err = snd_pcm_prepare(Self->Handle)) >= 0) {
                  // Have another try at writing the audio data
                  if (snd_pcm_avail_update(Self->Handle) >= space) {
                     snd_pcm_writei(Self->Handle, Self->AudioBuffer, space);
                  }
               }
               else log.warning("snd_pcm_prepare() %s", snd_strerror(err));
            }
            else if (code IS SND_PCM_STATE_DRAINING) {
               log.msg("Status: Draining");
            }
         }
         else log.warning("snd_pcm_writei() %d %s", err, snd_strerror(err));
      }
   }

   return ERR_Okay;

#elif _WIN32

   sndStreamAudio((PlatformData *)Self->PlatformData);
   return ERR_Okay;

#else

   #warning No audio timer support on this platform.
   return ERR_NoSupport;

#endif
}

//********************************************************************************************************************

static void convert_float8(FLOAT *buf, LONG TotalSamples, UBYTE *dest)
{
   while (TotalSamples) {
      LONG n = ((LONG)(*(buf++)))>>8;
      if (n < -128) n = -128;
      else if (n > 127)  n = 127;
      *dest++ = (UBYTE)(128 + n);
      TotalSamples--;
   }
}

static void convert_float16(FLOAT *buf, LONG TotalSamples, WORD *dest)
{
   while (TotalSamples) {
      LONG n = (LONG)(*(buf++));
      if (n < -32768) n = -32768;
      else if (n > 32767) n = 32767;
      *dest++ = (WORD)n;
      TotalSamples--;
   }
}

// No conversion is necessary if the output is float, but we do ensure that values are clamped.

static void convert_float(FLOAT *buf, LONG TotalSamples, FLOAT *dest)
{
   while (TotalSamples) {
      FLOAT n = buf[0];
      if (n < -1.0) *dest++ = -1.0;
      else if (n > 1.0) *dest++ = 1.0;
      else *dest++ = n;
      buf++;
      TotalSamples--;
   }
}

//********************************************************************************************************************
// Return a maximum of 32k-1 samples to prevent overflow problems

static LONG samples_until_end(extAudio *Self, AudioChannel &Channel, LONG *NextOffset)
{
   pf::Log log(__FUNCTION__);
   LONG num, lp_start, lp_end;
   LTYPE lp_type;

   *NextOffset = 1;

   auto &sample = Self->Samples[Channel.SampleHandle];

   if (Channel.LoopIndex IS 2) {
      lp_start = sample.Loop2Start;
      lp_end   = sample.Loop2End;
      lp_type  = sample.Loop2Type;
   }
   else {
      lp_start = sample.Loop1Start;
      lp_end   = sample.Loop1End;
      lp_type  = sample.Loop1Type;
   }

   // When using interpolating mixing, we'll first mix everything normally until the very last sample of the
   // loop/sample/stream. Then the last sample will be mixed separately, setting *NextOffset to a correct value, to
   // make sure we won't interpolate past the end.  This doesn't make this code exactly pretty, but saves us from
   // quite a bit of headache elsewhere.

   switch (lp_type) {
      default:
         if ((Self->Flags & ADF::OVER_SAMPLING) != ADF::NIL) {
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

      case LTYPE::UNIDIRECTIONAL:
         if ((Self->Flags & ADF::OVER_SAMPLING) != ADF::NIL) {
            if ((Channel.Position + 1) < lp_end) num = (lp_end - 1) - Channel.Position;
            else { // The last sample of the loop
               *NextOffset = lp_start - Channel.Position;
               num = lp_end - Channel.Position;
            }
         }
         else num = lp_end - Channel.Position;
         break;

      case LTYPE::BIDIRECTIONAL:
         if ((Channel.Flags & CHF::BACKWARD) != CHF::NIL) { // Backwards
            if ((Self->Flags & ADF::OVER_SAMPLING) != ADF::NIL) {
               if (Channel.Position IS (lp_end-1)) { // First sample of the loop backwards
                  *NextOffset = 0;
                  num = 1;
               }
               else num = Channel.Position - lp_start;
            }
            else num = Channel.Position - lp_start;
            break;
         }
         else { // Forward
            if ((Self->Flags & ADF::OVER_SAMPLING) != ADF::NIL) {
               if ((Channel.Position + 1) < lp_end) num = (lp_end - 1) - Channel.Position;
               else { // The last sample of the loop
                  *NextOffset = 0;
                  num = lp_end - Channel.Position;
               }
            }
            else num = lp_end - Channel.Position;
            break;
         }
   }

   LONG result;
   if (num > 0x7FFF) result = 0x7FFF0000; // 16.16 fixed point
   else result = ((num << 16) - Channel.PositionLow);

   if (result < 0) {
      log.warning("Computed invalid SUE value of %d", result);
      return 0;
   }
   else return result;
}

//********************************************************************************************************************

static bool amiga_change(extAudio *Self, AudioChannel &Channel)
{
   // A sample end or sample loop end has been reached, the sample has been changed, and both old and new samples use
   // Amiga compatible looping - handle Amiga Loop Emulation sample change

   if (Channel.SampleHandle > 1) Channel.SampleHandle--;
   auto &sample = Self->Samples[Channel.SampleHandle];
   Channel.Flags &= ~CHF::CHANGED;

   if (sample.LoopMode IS LOOP::AMIGA) {
      // Looping - start playback from loop beginning
      Channel.Position    = sample.Loop1Start;
      Channel.PositionLow = 0;
      return false;
   }

   // Not looping - finish the sample
   Self->finish(Channel, true);
   return true;
}

//********************************************************************************************************************

static bool handle_sample_end(extAudio *Self, AudioChannel &Channel)
{
   pf::Log log(__FUNCTION__);
   LONG lp_start, lp_end;
   LTYPE lp_type;

   auto &sample = Self->Samples[Channel.SampleHandle];

   if (Channel.LoopIndex IS 2) {
      lp_start = sample.Loop2Start;
      lp_end   = sample.Loop2End;
      lp_type  = sample.Loop2Type;
   }
   else {
      lp_start = sample.Loop1Start;
      lp_end   = sample.Loop1End;
      lp_type  = sample.Loop1Type;
   }

   if (lp_type IS LTYPE::NIL) { // No loop - did we reach sample end?
      if (Channel.Position >= sample.SampleLength) {
         auto &prev_sample = Self->Samples[Channel.SampleHandle-1];
         if (((Channel.Flags & CHF::CHANGED) != CHF::NIL) and
             ((sample.LoopMode IS LOOP::AMIGA) or (sample.LoopMode IS LOOP::AMIGA_NONE)) and
             ((prev_sample.LoopMode IS LOOP::AMIGA) or (prev_sample.LoopMode IS LOOP::AMIGA_NONE))) {
            return amiga_change(Self, Channel);
         }

         // No sample change - we are finished
         Self->finish(Channel, true);
         return true;
      }
      else return false;
   }

   if ((Channel.Flags & CHF::BACKWARD) != CHF::NIL) {
      // Going backwards - did we reach loop start? (signed comparison takes care of possible wraparound)
      if ((Channel.Position < lp_start) or ((Channel.Position IS lp_start) and (Channel.PositionLow IS 0)) ) {
         Channel.Flags &= ~CHF::BACKWARD;
         LONG n = ((lp_start - Channel.Position) << 16) - Channel.PositionLow - 1;
         // -1 is compensation for the fudge factor at loop end, see below
         Channel.Position = lp_start + (n>>16);
         Channel.PositionLow = n & 0xffff;

         // Don't die on overshort loops
         if (Channel.Position >= lp_end) {
            Channel.Position = lp_start;
            return true;
         }
      }
   }
   else if (Channel.Position >= lp_end) { // Going forward - did we reach loop end?
      if (sample.Stream) {
         // Read the next set of stream data into our sample buffer
         BYTELEN bytes_read = fill_stream_buffer(Channel.SampleHandle, sample, -1);
         auto buffer_len = sample.SampleLength<<sample_shift(sample.SampleType);
         if (bytes_read < buffer_len) {
            ClearMemory(sample.Data + bytes_read, buffer_len - bytes_read);
         }

         if ((bytes_read <= 0) or (sample.PlayPos >= sample.StreamLength)) {
            // Loop back to the beginning if the client has defined a loop.  Otherwise finish.
            if (sample.Loop2Type != LTYPE::NIL) {
               sample.PlayPos = BYTELEN(0);
            }
            else Self->finish(Channel, true);
         }
         else sample.PlayPos += bytes_read;
      }

      // Check for ALE sample change

      auto &prev_sample = Self->Samples[Channel.SampleHandle-1];

      if (((Channel.Flags & CHF::CHANGED) != CHF::NIL) and
          ((sample.LoopMode IS LOOP::AMIGA) or (sample.LoopMode IS LOOP::AMIGA_NONE)) and
          ((prev_sample.LoopMode IS LOOP::AMIGA) or (prev_sample.LoopMode IS LOOP::AMIGA_NONE))) {
         return amiga_change(Self, Channel);
      }

      // Go to the second loop if the sound has been released

      if ((Channel.LoopIndex IS 1) and (Channel.State IS CHS::RELEASED)) {
         Channel.LoopIndex = 2;
         return false;
      }

      if (lp_type IS LTYPE::BIDIRECTIONAL ) {
         // Bidirectional loop - change direction
         Channel.Flags |= CHF::BACKWARD;
         LONG n = ((Channel.Position - lp_end) << 16) + Channel.PositionLow + 1;

         // +1 is a fudge factor to make sure we'll access the correct samples all the time - a similar adjustment is
         // also done at the other end of the loop. This screws up interpolation a little when sample rate IS mixing
         // rate, but little enough that it can't be heard.

         if (lp_end < 0x10000) {
            Channel.Position    = ((lp_end << 16) - n)>>16;
            Channel.PositionLow = ((lp_end << 16) - n) & 0xffff;
         }
         else {
            Channel.Position    = ((0xffff0000 - n)>>16) + (lp_end - 0xffff);
            Channel.PositionLow = (0xffff0000 - n) & 0xffff;
         }

         if (Channel.Position <= lp_start) { // Don't die on overshort loops
            Channel.Position = lp_end;
            return true;
         }
         return false;
      }
      else { // Unidirectional loop - just loop to the beginning
         Channel.Position = lp_start + (Channel.Position - lp_end);

         if (Channel.Position >= lp_end) { // Don't die on overshort loops
            Channel.Position = lp_start;
            return true;
         }

         return false;
      }
   }

   return false;
}

//********************************************************************************************************************
// Main entry point for mixing sound data to destination

static ERROR mix_data(extAudio *Self, LONG Elements, APTR Dest)
{
   pf::Log log(__FUNCTION__);

   while (Elements > 0) {
      // Mix only as much as we can fit in our mixing buffer

      auto window = (Elements > Self->MixElements) ? Self->MixElements : Elements;

      // Clear the mix buffer, then mix all channels to the buffer

      LONG window_size = sizeof(FLOAT) * (Self->Stereo ? (window<<1) : window);
      ClearMemory(Self->MixBuffer, window_size);

      for (auto n=1; n < (LONG)Self->Sets.size(); n++) {
         for (auto &c : Self->Sets[n].Channel) {
            if (c.active()) mix_channel(Self, c, window, Self->MixBuffer);
         }

         for (auto &c : Self->Sets[n].Shadow) {
            if (c.active()) mix_channel(Self, c, window, Self->MixBuffer);
         }
      }

      // Do optional post-processing

      if ((Self->Flags & (ADF::FILTER_LOW|ADF::FILTER_HIGH)) != ADF::NIL) {
         if (Self->Stereo) filter_float_stereo(Self, (FLOAT *)Self->MixBuffer, window);
         else filter_float_mono(Self, (FLOAT *)Self->MixBuffer, window);
      }

      // Convert the floating point data to the correct output format

      if (Self->BitDepth IS 32) { // Presumes a floating point target identical to our own
         convert_float((FLOAT *)Self->MixBuffer, (Self->Stereo) ? window<<1 : window, (FLOAT *)Dest);
      }
      else if (Self->BitDepth IS 24) {

      }
      else if (Self->BitDepth IS 16) {
         convert_float16((FLOAT *)Self->MixBuffer, (Self->Stereo) ? window<<1 : window, (WORD *)Dest);
      }
      else {
         convert_float8((FLOAT *)Self->MixBuffer,  (Self->Stereo) ? window<<1 : window, (UBYTE *)Dest);
      }

      Dest = ((UBYTE *)Dest) + (window * Self->DriverBitSize);
      Elements -= window;
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static void mix_channel(extAudio *Self, AudioChannel &Channel, LONG TotalSamples, APTR Dest)
{
   pf::Log log(__FUNCTION__);

   auto &sample = Self->Samples[Channel.SampleHandle];

   // Check that we have something to mix

   if ((!Channel.Frequency) or (!sample.Data) or (sample.SampleLength <= 0)) return;

   // Calculate resampling step (16.16 fixed point)

   LONG step = ((LARGE(Channel.Frequency / Self->OutputRate) << 16) + (LARGE(Channel.Frequency % Self->OutputRate) << 16) / Self->OutputRate);

   DOUBLE stereo_mul = 1.0;
   if (!Self->Stereo) {
      if ((sample.SampleType IS SFM::U8_BIT_STEREO) or (sample.SampleType IS SFM::S16_BIT_STEREO)) {
         stereo_mul = 0.5;
      }
   }

   DOUBLE mastervol = Self->Mute ? 0 : Self->MasterVolume * stereo_mul;

   // Determine bit size of the sample, not necessarily a match to that of the mixer.

   DOUBLE conversion;
   LONG sample_size;
   switch (sample.SampleType) {
      case SFM::S16_BIT_STEREO: sample_size = sizeof(WORD) * 2; conversion = 1.0 / 32767.0; break;
      case SFM::S16_BIT_MONO:   sample_size = sizeof(WORD); conversion = 1.0 / 32767.0; break;
      case SFM::U8_BIT_STEREO:  sample_size = sizeof(BYTE) * 2; conversion = 1.0 / 127.0; break;
      default:                 sample_size = sizeof(BYTE); conversion = 1.0 / 127.0; break;
   }

   if (Self->BitDepth IS 32) {
      // If our hardware output format is floating point, the values need to range from -1.0 to 1.0.
      // The application of the conversion value to mastervol allows us to achieve this optimally
      // and we won't need to apply any further conversions.
      mastervol *= conversion;
   }

   glMixDest = (FLOAT *)Dest;
   while (TotalSamples > 0) {
      if (Channel.isStopped()) return;

      LONG next_offset;
      LONG sue = samples_until_end(Self, Channel, &next_offset);

      // Calculate the number of destination samples (note rounding)

      LONG mixUntilEnd = sue / step;
      if (sue % step) mixUntilEnd++;

      LONG mix_now = (mixUntilEnd > TotalSamples) ? TotalSamples : mixUntilEnd;
      TotalSamples -= mix_now;

      if (mix_now > 0) {
         if (Channel.PositionLow < 0) { // Sanity check
            log.warning("Detected invalid PositionLow value of %d", Channel.PositionLow);
            return;
         }

         LONG mix_pos = Channel.PositionLow;
         UBYTE *MixSample = sample.Data + (sample_size * Channel.Position); // source of sample data to mix into destination

         auto mix_routine = Self->MixRoutines[LONG(sample.SampleType)];

         if ((Channel.Flags & CHF::BACKWARD) != CHF::NIL) MixStep = -step;
         else MixStep = step;

         // If volume ramping is enabled, mix one sample element at a time and adjust volume by RAMPSPEED.

         while (((Channel.Flags & CHF::VOL_RAMP) != CHF::NIL) and (mix_now > 0)) {
            mix_pos = mix_routine(MixSample, mix_pos, 1, next_offset, mastervol * Channel.LVolume, mastervol * Channel.RVolume);
            mix_now--;

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

            if (!cont) Channel.Flags &= ~CHF::VOL_RAMP;
         }

         if ((Channel.LVolume <= 0.01) and (Channel.RVolume <= 0.01)) {
            // If the volume is zero we can just increment the position and not mix anything
            mix_pos += mix_now * MixStep;
            if (Channel.State IS CHS::FADE_OUT) {
               Self->finish(Channel, true);
               Channel.Flags &= ~CHF::VOL_RAMP;
            }
         }
         else {
            // Ensure proper alignment and do all mixing if next_offset != 1

            LONG num;
            if ((mix_now > 0) and ((next_offset != 1) or ((((MAXINT)glMixDest) % 1) != 0))) {
               if (next_offset IS 1) {
                  num = 1 - (((MAXINT)glMixDest) % 1);
                  if (num > mix_now) num = mix_now;
               }
               else num = mix_now;

               mix_pos = mix_routine(MixSample, mix_pos, num, next_offset, mastervol * Channel.LVolume, mastervol * Channel.RVolume);
               mix_now -= num;
            }

            if (mix_now > 0) { // Main mixing loop
               mix_pos = mix_routine(MixSample, mix_pos, mix_now, 1, mastervol * Channel.LVolume, mastervol * Channel.RVolume);
            }
         }

         // Save state back to channel structure
         Channel.PositionLow = mix_pos & 0xffff;
         Channel.Position = (mix_pos>>16) + ((MixSample - sample.Data) / sample_size);
      }
      else if (mix_now < 0) {
         log.warning("Detected invalid mix values; TotalSamples: %d, MixNow: %d, SUE: %d, NextOffset: %d, Step: %d, ChannelPos: %d", TotalSamples, mix_now, sue, next_offset, step, Channel.Position);
         return;
      }

      // Check if we reached loop/sample/whatever end

      if (handle_sample_end(Self, Channel)) return;
   }
}

//********************************************************************************************************************
// Mono output filtering routines.

static void filter_float_mono(extAudio *Self, FLOAT *Data, LONG TotalSamples)
{
   static FLOAT d1l=0, d2l=0;
   if ((Self->Flags & ADF::FILTER_LOW) != ADF::NIL) {
      while (TotalSamples > 0) {
         FLOAT s = (d1l + 2.0f * Data[0]) * (1.0f / 3.0f);
         d1l = Data[0];
         *(Data++) = s;
         TotalSamples--;
      }
   }
   else if ((Self->Flags & ADF::FILTER_HIGH) != ADF::NIL) {
      while (TotalSamples > 0) {
         FLOAT s = (d1l + 3.0f * d2l + 4.0f * Data[0]) * (1.0f / 8.0f);
         d1l = d2l;
         d2l = Data[0];
         *(Data++) = s;
         TotalSamples--;
      }
   }
}

//********************************************************************************************************************
// Stereo output filtering routines.

static void filter_float_stereo(extAudio *Self, FLOAT *Data, LONG TotalSamples)
{
   static DOUBLE d1l = 0, d1r = 0, d2l = 0, d2r = 0;

   if ((Self->Flags & ADF::FILTER_LOW) != ADF::NIL) {
      while (TotalSamples > 0) {
         FLOAT s = (d1l + 2.0 * Data[0]) * (1.0 / 3.0);
         d1l = Data[0];
         *(Data++) = s;

         s = (d1r + 2.0 * Data[0]) * (1.0 / 3.0);
         d1r = Data[0];
         *(Data++) = s;

         TotalSamples--;
      }
   }
   else if ((Self->Flags & ADF::FILTER_HIGH) != ADF::NIL) {
      while (TotalSamples > 0) {
         FLOAT s = (d1l + 3.0 * d2l + 4.0 * Data[0]) * (1.0 / 8.0);
         d1l = d2l;
         d2l = Data[0];
         *(Data++) = s;

         s = (d1r + 3.0 * d2r + 4.0 * Data[0]) * (1.0 / 8.0);
         d1r = d2r;
         d2r = Data[0];
         *(Data++) = s;

         TotalSamples--;
      }
   }
}
