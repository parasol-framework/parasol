/*****************************************************************************
** Continue: Continues playing a sound sample after it has been stopped earlier.
*/

static ERROR COMMAND_Continue(objAudio *Self, LONG Handle)
{
   parasol::Log log("AudioCommand");

   log.trace("Continue($%.8x)", Handle);

   if (!Handle) return ERR_NullArgs;

   auto channel = GetChannel(Handle);

   // Do nothing if the channel is already active

   if (channel->State IS CHS_PLAYING) return ERR_Okay;

   // Check if the read position is already at the end of the sample

   if ((channel->Sample.StreamID) and (channel->Sample.StreamPos >= channel->Sample.StreamLength)) {
      return ERR_Okay;
   }
   else if (channel->Position >= (ULONG)channel->Sample.SampleLength) return ERR_Okay;

   if (Self->Flags & ADF_OVER_SAMPLING) COMMAND_FadeOut(Self, Handle);

   channel->State = CHS_PLAYING;

   if (Self->Flags & ADF_OVER_SAMPLING) {
      channel += Self->Channels[Handle>>16].Total;
      channel->State = CHS_PLAYING;
   }

   FUNCTION callback;
   SET_FUNCTION_STDC(callback, (APTR)&audio_timer);
   SubscribeTimer(MIX_INTERVAL, &callback, &Self->Timer);

   return ERR_Okay;
}

/*****************************************************************************
** FadeIn: Starts fading in a channel if fadeout channels have been allocated.
*/

static ERROR COMMAND_FadeIn(objAudio *Self, LONG Handle)
{
   parasol::Log log("AudioCommand");

   log.trace("FadeIn($%.8x)", Handle);

   if (Handle) {
      if ((!(Self->Flags & ADF_VOL_RAMPING)) or (!(Self->Flags & ADF_OVER_SAMPLING))) return ERR_Okay;

      // Ramp back up from zero

      AudioChannel *channel = GetChannel(Handle);
      channel->LVolume = 0;
      channel->RVolume = 0;
      return SetInternalVolume(Self, channel);
   }
   else return ERR_Failed;
}

/*****************************************************************************
** FadeOut: Starts fading out a channel if fadeout channels have been allocated and the channel has data being played.
*/

static ERROR COMMAND_FadeOut(objAudio *Self, LONG Handle)
{
   parasol::Log log("AudioCommand");

   log.trace("FadeOut($%.8x)", Handle);

   if (!Handle) return ERR_NullArgs;
   if (!(Self->Flags & ADF_OVER_SAMPLING)) return ERR_Okay;

   auto channel = GetChannel(Handle);
   auto destchannel = channel + (Self->Channels[Handle>>16].Total);

   if ((channel->State IS CHS_STOPPED) or (channel->State IS CHS_FINISHED) or ((channel->LVolume IS 0) and (channel->RVolume IS 0))) return ERR_Okay;
   if (destchannel->State IS CHS_FADE_OUT) return ERR_Okay;

   destchannel->State = CHS_FADE_OUT;

   // Copy the channel information

   LONG oldstatus = channel->State;
   channel->State = CHS_STOPPED;
   CopyMemory(channel, destchannel, sizeof(AudioChannel));
   channel->State = oldstatus;

   // Start ramping

   destchannel->Volume = 0;
   destchannel->State = CHS_FADE_OUT;
   SetInternalVolume(Self, destchannel);
   destchannel->Flags |= CHF_VOL_RAMP;
   return ERR_Okay;
}

/*****************************************************************************
** Mute: Use this method to mute sound channels.
*/

static ERROR COMMAND_Mute(objAudio *Self, LONG Handle, LONG Mute)
{
   parasol::Log log("AudioCommand");

   log.trace("Mute($%.8x, %d)", Handle, Mute);

   if (!Handle) return ERR_NullArgs;

   auto channel = GetChannel(Handle);
   if (Mute IS TRUE) channel->Flags |= CHF_MUTE;
   else channel->Flags &= ~CHF_MUTE;
   SetInternalVolume(Self, channel);
   return ERR_Okay;
}

/*****************************************************************************
** Play
*/

static ERROR COMMAND_Play(objAudio *Self, LONG Handle, LONG Frequency)
{
   parasol::Log log("AudioCommand");

   log.trace("Play($%.8x, %d)", Handle, Frequency);
   if (!Frequency) log.traceWarning("[Play] You should specify a sample frequency.");

   if (!Handle) return ERR_NullArgs;

   if (Self->Flags & ADF_OVER_SAMPLING) COMMAND_FadeOut(Self, Handle);

   auto channel = GetChannel(Handle);
   channel->State     = CHS_FINISHED;    // Turn off previous sound
   channel->Frequency = Frequency;
   COMMAND_SetPosition(Self, Handle, 0); // Set playing position to the beginning of the sample

   if (channel->State IS CHS_PLAYING) {
      FUNCTION callback;
      SET_FUNCTION_STDC(callback, (APTR)&audio_timer);
      SubscribeTimer(MIX_INTERVAL, &callback, &Self->Timer);
   }

   return ERR_Okay;
}

/*****************************************************************************
** SetFrequency: Sets the channel playback rate.
*/

static ERROR COMMAND_SetFrequency(objAudio *Self, LONG Handle, ULONG Frequency)
{
   parasol::Log log("AudioCommand");

   log.trace("SetFrequency($%.8x, %d)", Handle, Frequency);

   if (!Handle) return ERR_NullArgs;

   auto channel = GetChannel(Handle);
   channel->Frequency = Frequency;
   return ERR_Okay;
}

/*****************************************************************************
** SetPan: Sets the panning position of a channel.
*/

static ERROR COMMAND_SetPan(objAudio *Self, LONG Handle, LONG Pan)
{
   parasol::Log log("AudioCommand");

   log.trace("SetPan($%.8x, %d)", Handle, Pan);

   if (!Handle) return ERR_NullArgs;

   auto channel = GetChannel(Handle);

   if (Pan < -100) channel->Pan = -100;
   else if (Pan > 100) channel->Pan = 100;
   else channel->Pan = Pan;

   SetInternalVolume(Self, channel);
   return ERR_Okay;
}

/*****************************************************************************
** SetPosition: Sets the playing position from the beginning of the currently set sample.
**
** This command can only be executed by the task that owns the audio object.
*/

static ERROR COMMAND_SetPosition(objAudio *Self, LONG Handle, LONG Position)
{
   parasol::Log log("SetPosition");
   OBJECTPTR stream;
   AudioSample *sample;
   LONG bitpos;

   log.trace("SetPosition($%.8x, %d)", Handle, Position);

   if (CurrentTaskID() != Self->ownerTask()) {
      log.warning("Illegal attempt to use SetPosition directly (please use command messaging).");
   }

   if (!Handle) return ERR_NullArgs;

   AudioChannel *channel = GetChannel(Handle);

   // Check that sample and playing rate have been set on the channel

   if (!channel->SampleHandle) {
      log.warning("Sample handle not set on channel.");
      return ERR_Failed;
   }

   // If the sample is a stream object, we just need to send a Seek action to have the Stream owner acknowledge the
   // new audio position and have it feed us new audio data.

   if (channel->Sample.StreamID) {
      if (!AccessObject(channel->Sample.StreamID, 5000, &stream)) {
         ActionTags(AC_Seek, stream, (DOUBLE)channel->Sample.SeekStart + Position, SEEK_START);

         // Fill our sample buffer with an initial amount of audio information from the stream

         struct acRead read;
         read.Buffer = channel->Sample.Data;
         read.Length = channel->Sample.BufferLength;
         Action(AC_Read, stream, &read);
         ReleaseObject(stream);
      }

      channel->Sample.StreamPos = channel->Sample.SeekStart + Position;
      Position = 0; // Internally we want to start from byte position zero in our stream buffer
   }

   if (Self->Flags & ADF_OVER_SAMPLING) COMMAND_FadeOut(Self, Handle);

   // Convert position from bytes to samples

   bitpos = Position >> SampleShift(channel->Sample.SampleType);

   // Check if sample has been changed, and if so, set the values to the channel structure

   if (channel->Flags & CHF_CHANGED) {
      sample = &Self->Samples[channel->SampleHandle];
      CopyMemory(sample, channel, sizeof(AudioSample));
      channel->Flags &= ~CHF_CHANGED;

      // If channel status is released and the new sample does not have two loops, end the sample

      if ((channel->Sample.LoopMode != LOOP_SINGLE_RELEASE) and (channel->Sample.LoopMode != LOOP_DOUBLE) and (channel->State IS CHS_RELEASED)) {
         channel->State = CHS_FINISHED;
         return ERR_Okay;
      }
   }

   switch (channel->State) {
      case CHS_FINISHED:
      case CHS_PLAYING:
         // Either playing sample before releasing or playing has ended - check the first loop type.

         channel->LoopIndex = 1;
         switch (channel->Sample.Loop1Type) {
            case 0:
               // No looping - if position is below sample end, set it and start playing there
               if (bitpos < channel->Sample.SampleLength) {
                  channel->Position    = bitpos;
                  channel->PositionLow = 0;
                  channel->State       = CHS_PLAYING;
                  channel->Flags       &= ~CHF_BACKWARD;
               }
               else channel->State = CHS_FINISHED;
               break;

            case LTYPE_UNIDIRECTIONAL:
               // Unidirectional looping - if position is below loop end, set it, otherwise set loop start as the
               // new position. Start playing in any case.
               if ( bitpos < channel->Sample.Loop1End ) channel->Position = bitpos;
               else channel->Position = channel->Sample.Loop1Start;
               channel->PositionLow = 0;
               channel->State       = CHS_PLAYING;
               channel->Flags      &= ~CHF_BACKWARD;
               break;

            case LTYPE_BIDIRECTIONAL:
               // Bidirectional looping - if position is below loop end, set it and start playing forward, otherwise
               // set loop end as the new position and start playing backwards.
               if (bitpos < channel->Sample.Loop1End ) {
                  channel->Position = bitpos;
                  channel->Flags &= ~CHF_BACKWARD;
               }
               else {
                  channel->Position = channel->Sample.Loop1End;
                  channel->Flags |= CHF_BACKWARD;
               }
               channel->PositionLow = 0;
               channel->State = CHS_PLAYING;
         }
         break;

      case CHS_RELEASED: // Playing after sample has been released - check second loop type.
         channel->LoopIndex = 2;
         switch (channel->Sample.Loop2Type) {
            case 0:
               // No looping - if position is below sample end, set it and start playing there.

               if (bitpos < channel->Sample.SampleLength ) {
                  channel->Position    = bitpos;
                  channel->PositionLow = 0;
                  channel->State       = CHS_PLAYING;
                  channel->Flags       &= ~CHF_BACKWARD;
               }
               else channel->State = CHS_FINISHED;
               break;

            case LTYPE_UNIDIRECTIONAL:
               // Unidirectional looping - if position is below loop end, set it, otherwise set loop start as the
               // new position. Start playing in any case.
               if (bitpos < channel->Sample.Loop2End) channel->Position = bitpos;
               else channel->Position = channel->Sample.Loop2Start;
               channel->PositionLow = 0;
               channel->State = CHS_PLAYING;
               channel->Flags &= ~CHF_BACKWARD;
               break;

            case LTYPE_BIDIRECTIONAL:
               // Bidirectional looping - if position is below loop end, set it and start playing forward, otherwise
               // set loop end as the new position and start playing backwards.

               if (bitpos < channel->Sample.Loop2End) {
                  channel->Position = bitpos;
                  channel->Flags &= ~CHF_BACKWARD;
               }
               else {
                  channel->Position = channel->Sample.Loop2End;
                  channel->Flags |= CHF_BACKWARD;
               }
               channel->PositionLow = 0;
               channel->State = CHS_PLAYING;
         }
         break;

      case CHS_STOPPED:
      default:
         // If sound has been stopped do nothing
         break;
   }

   if (Self->Flags & ADF_OVER_SAMPLING) COMMAND_FadeIn(Self, Handle);

   if (channel->State IS CHS_PLAYING) {
      FUNCTION callback;
      SET_FUNCTION_STDC(callback, (APTR)&audio_timer);
      SubscribeTimer(MIX_INTERVAL, &callback, &Self->Timer);
   }

   return ERR_Okay;
}

/*****************************************************************************
** SetRate: Sets a new update rate for buffered channels.
*/

static ERROR COMMAND_SetRate(objAudio *Self, LONG Handle, LONG Rate)
{
   parasol::Log log("AudioCommand");

   log.trace("SetRate($%.8x, %d)", Handle, Rate);
   if (!Handle) return ERR_NullArgs;
   WORD index = Handle>>16;
   Self->Channels[index].UpdateRate = Rate;
   return ERR_Okay;
}

/*****************************************************************************
** SetSample: Sets the sample number on a channel.
*/

ERROR COMMAND_SetSample(objAudio *Self, LONG Handle, LONG SampleHandle)
{
   parasol::Log log("AudioCommand");

   log.trace("SetSample($%.8x, %d)", Handle, SampleHandle);

   if (!Handle) {
      log.warning("[SetSample] No Handle specified.");
      return ERR_NullArgs;
   }

   if (!SampleHandle) {
      log.warning("[SetSample] No SampleHandle specified.");
      return ERR_NullArgs;
   }

   if (SampleHandle > Self->TotalSamples) {
      log.warning("[SetSample] Sample handle %d is out of range ($%.8x max).", SampleHandle, Self->TotalSamples);
      return ERR_Args;
   }

   if (Self->Samples[SampleHandle].Used IS FALSE) {
      log.warning("[SetSample] Sample handle %d refers to a dead sample.", SampleHandle);
      return ERR_Failed;
   }

   auto channel = GetChannel(Handle);

   if (channel->SampleHandle IS SampleHandle) return ERR_Okay;

   channel->SampleHandle = SampleHandle; // Set new sample number to channel
   channel->Flags |= CHF_CHANGED;        // Sample has been changed

   // If the new sample has one Amiga-compatible loop and playing has ended (not released or stopped), set the new
   // sample and start playing from loop start.

   AudioSample *sample = &Self->Samples[SampleHandle];
   if ((sample->LoopMode IS LOOP_AMIGA) and (channel->State IS CHS_FINISHED)) {
      // Set Amiga sample and start playing.  We won't do this with interpolated mixing, as this tends to cause clicks.

      if (!(Self->Flags & ADF_OVER_SAMPLING)) {
         channel->State = CHS_PLAYING;
         COMMAND_SetPosition(Self, Handle, sample->Loop1Start);
      }
   }

   return ERR_Okay;
}

/*****************************************************************************
** SetLength: Sets the byte length of the sample playing in the channel.
*/

static ERROR COMMAND_SetLength(objAudio *Self, LONG Handle, LONG Length)
{
   parasol::Log log("AudioCommand");

   log.trace("SetLength($%.8x, %d)", Handle, Length);

   if (!Handle) return ERR_NullArgs;

   auto channel = GetChannel(Handle);
   if (channel->Sample.StreamID) channel->Sample.StreamLength = Length;
   else channel->Sample.SampleLength = Length;
   return ERR_Okay;
}

/*****************************************************************************
** SetVolume: Sets the volume of a specific channel (0 - 100).
*/

static ERROR COMMAND_SetVolume(objAudio *Self, LONG Handle, LONG Volume)
{
   parasol::Log log("AudioCommand");

   log.trace("SetVolume($%.8x, %d)", Handle, Volume);

   if (!Handle) return ERR_NullArgs;

   auto channel = GetChannel(Handle);
   if (Volume > 1000) channel->Volume = 1000;
   else if (Volume < 0) channel->Volume = 0;
   else channel->Volume = Volume;
   SetInternalVolume(Self, channel);
   return ERR_Okay;
}

/*****************************************************************************
** Stop: Stops a channel's audio playback.
*/

ERROR COMMAND_Stop(objAudio *Self, LONG Handle)
{
   parasol::Log log("AudioCommand");

   log.trace("Stop($%.8x)", Handle);

   if (!Handle) return ERR_NullArgs;

   auto channel = GetChannel(Handle);
   channel->State = CHS_STOPPED;

   if (Self->Flags & ADF_OVER_SAMPLING) {
      channel += Self->Channels[Handle>>16].Total;
      channel->State = CHS_STOPPED;
   }
   return ERR_Okay;
}

/*****************************************************************************
** StopLooping: Stops a sound once it has completed playing.  This method is only useful for stopping sounds that
** continually loop and you want to stop the loop from occurring.
*/

static ERROR COMMAND_StopLooping(objAudio *Self, LONG Handle)
{
   parasol::Log log("AudioCommand");

   log.trace("StopLooping($%.8x)", Handle);

   if (!Handle) return ERR_NullArgs;

   auto channel = GetChannel(Handle);
   if (channel->State != CHS_PLAYING) return ERR_Okay;

   if ((channel->Sample.LoopMode IS LOOP_SINGLE_RELEASE) or (channel->Sample.LoopMode IS LOOP_DOUBLE)) {
      channel->State = CHS_RELEASED;
   }

   return ERR_Okay;
}

