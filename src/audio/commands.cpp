//********************************************************************************************************************
// Buffered command handling.  The execution of these commands is managed by process_commands()

template <class T> void add_mix_cmd(objAudio *Audio, CMD Command, LONG Handle, T Data)
{
   auto ea = (extAudio *)Audio;
   LONG index = Handle>>16;

   if ((index < 1) or (index >= (LONG)ea->Sets.size())) return;

   if (ea->Sets[index].Commands.capacity() > 0) {
      auto i = ea->Sets[index].Commands.size();
      ea->Sets[index].Commands.resize(i+1);
      ea->Sets[index].Commands[i].CommandID = Command;
      ea->Sets[index].Commands[i].Handle    = Handle;
      ea->Sets[index].Commands[i].Data      = Data;
   }
}

static void add_mix_cmd(objAudio *Audio, CMD Command, LONG Handle)
{
   auto ea = (extAudio *)Audio;
   LONG index = Handle>>16;

   if ((index < 1) or (index >= (LONG)ea->Sets.size())) return;

   if (ea->Sets[index].Commands.capacity() > 0) {
      auto i = ea->Sets[index].Commands.size();
      ea->Sets[index].Commands.resize(i+1);
      ea->Sets[index].Commands[i].CommandID = Command;
      ea->Sets[index].Commands[i].Handle    = Handle;
   }
}

/*********************************************************************************************************************

-FUNCTION-
MixStartSequence: Initiates buffering of mix commands.

Use this function to initiate the buffering of mix commands, up until a call to ~MixEndSequence() is made.  The
buffering of mix commands makes it possible to create batches of commands that are executed at timed intervals
as determined by ~MixRate().

This feature can be used to implement complex sound mixes and digital music players.

-INPUT-
obj(Audio) Audio: The target Audio object.
int Handle: The target channel.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

static ERROR sndMixStartSequence(objAudio *Audio, LONG Handle)
{
   parasol::Log log("Mix:StartSequence");

   if ((!Audio) or (!Handle)) return log.warning(ERR_NullArgs);

   log.trace("Audio: #%d, Channel: $%.8x", Audio->UID, Handle);

   auto channel = ((extAudio *)Audio)->GetChannel(Handle);

   channel->Buffering = true;

   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
MixEndSequence: Ends the buffering of mix commands.

Use this function to end a buffered command sequence that was started by ~MixStartSequence().

-INPUT-
obj(Audio) Audio: The target Audio object.
int Handle: The target channel.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

static ERROR sndMixEndSequence(objAudio *Audio, LONG Handle)
{
   parasol::Log log("Mix:EndSequence");

   if ((!Audio) or (!Handle)) return log.warning(ERR_NullArgs);

   log.trace("Audio: #%d, Channel: $%.8x", Audio->UID, Handle);

   auto channel = ((extAudio *)Audio)->GetChannel(Handle);

   channel->Buffering = false;

   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
MixFadeOut: Fade a channel to zero volume.

This function will fade out a mixer channel if in oversampling mode and the channel has data being played.

-INPUT-
obj(Audio) Audio: The target Audio object.
int Handle: The target channel.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

static ERROR sndMixFadeOut(objAudio *Audio, LONG Handle)
{
   parasol::Log log("Mix:FadeOut");

   if ((!Audio) or (!Handle)) return log.warning(ERR_NullArgs);

   log.trace("Audio: #%d, Channel: $%.8x", Audio->UID, Handle);

   if (!(Audio->Flags & ADF_OVER_SAMPLING)) return ERR_Okay;

   auto channel = ((extAudio *)Audio)->GetChannel(Handle);

   if (channel->Buffering) {
      add_mix_cmd(Audio, CMD::FADE_OUT, Handle);
      return ERR_Okay;
   }

   auto destchannel = channel + ((extAudio *)Audio)->Sets[Handle>>16].Total;

   if ((channel->State IS CHS::STOPPED) or (channel->State IS CHS::FINISHED) or
       (destchannel->State IS CHS::FADE_OUT) or
       ((channel->LVolume < 0.01) and (channel->RVolume < 0.01))) return ERR_Okay;

   destchannel->State = CHS::FADE_OUT;

   // Copy the channel information

   auto oldstatus = channel->State;
   channel->State = CHS::STOPPED;
   destchannel = channel;
   channel->State = oldstatus;

   // Start ramping

   destchannel->Volume = 0;
   destchannel->State = CHS::FADE_OUT;
   set_channel_volume((extAudio *)Audio, destchannel);
   destchannel->Flags |= CHF::VOL_RAMP;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
MixContinue: Continue playing a stopped channel.

This function will continue playback on a channel that has previously been stopped.

-INPUT-
obj(Audio) Audio: The target Audio object.
int Handle: The target channel.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

static ERROR sndMixContinue(objAudio *Audio, LONG Handle)
{
   parasol::Log log("Mix:Continue");

   log.trace("Audio: #%d, Channel: $%.8x", Audio->UID, Handle);

   if ((!Audio) or (!Handle)) return log.warning(ERR_NullArgs);

   auto channel = ((extAudio *)Audio)->GetChannel(Handle);

   if (channel->Buffering) {
      add_mix_cmd(Audio, CMD::CONTINUE, Handle);
      return ERR_Okay;
   }

   // Do nothing if the channel is already active

   if (channel->State IS CHS::PLAYING) return ERR_Okay;

   // Check if the read position is already at the end of the sample

   auto &sample = ((extAudio *)Audio)->Samples[channel->SampleHandle];

   if ((sample.StreamID) and (channel->StreamPos >= sample.StreamLength)) return ERR_Okay;
   else if (channel->Position >= sample.SampleLength) return ERR_Okay;

   if (Audio->Flags & ADF_OVER_SAMPLING) sndMixFadeOut(Audio, Handle);

   channel->State = CHS::PLAYING;

   if (Audio->Flags & ADF_OVER_SAMPLING) {
      channel += ((extAudio *)Audio)->Sets[Handle>>16].Total;
      channel->State = CHS::PLAYING;
   }

   parasol::SwitchContext context(Audio);

   if (((extAudio *)Audio)->Timer) UpdateTimer(((extAudio *)Audio)->Timer, -MIX_INTERVAL);
   else {
      auto call = make_function_stdc(audio_timer);
      SubscribeTimer(MIX_INTERVAL, &call, &((extAudio *)Audio)->Timer);
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
MixFadeIn: Fade in a channel from zero volume.

This function will fade in a mixer channel that has previously been faded out with MixFadeOut.  It is a requirement
that `VOL_RAMPING` or `OVER_SAMPLING` flags have been set in the target Audio object.

-INPUT-
obj(Audio) Audio: The target Audio object.
int Handle: The target channel.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

static ERROR sndMixFadeIn(objAudio *Audio, LONG Handle)
{
   parasol::Log log("Mix:FadeIn");

   log.trace("Audio: #%d, Channel: $%.8x", Audio->UID, Handle);

   if ((!Audio) or (!Handle)) return log.warning(ERR_NullArgs);

   if ((!(Audio->Flags & ADF_VOL_RAMPING)) or (!(Audio->Flags & ADF_OVER_SAMPLING))) return ERR_Okay;

   auto channel = ((extAudio *)Audio)->GetChannel(Handle);

   if (channel->Buffering) {
      add_mix_cmd(Audio, CMD::FADE_IN, Handle);
      return ERR_Okay;
   }

   channel->LVolume = 0;
   channel->RVolume = 0;
   return set_channel_volume((extAudio *)Audio, channel);
}


/*********************************************************************************************************************

-FUNCTION-
MixMute: Mutes the audio of a channel.

Use this function to mute the audio of a mixer channel.

-INPUT-
obj(Audio) Audio: The target Audio object.
int Handle: The target channel.
int Mute: Set to any value to mute the channel.  A value of 0 will undo the mute setting.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

static ERROR sndMixMute(objAudio *Audio, LONG Handle, LONG Mute)
{
   parasol::Log log("Mix:Mute");

   log.trace("Audio: #%d, Channel: $%.8x, Mute: %c", Audio->UID, Handle, Mute ? 'Y' : 'N');

   if ((!Audio) or (!Handle)) return log.warning(ERR_NullArgs);

   auto channel = ((extAudio *)Audio)->GetChannel(Handle);

   if (channel->Buffering) {
      add_mix_cmd(Audio, CMD::MUTE, Handle, Mute);
      return ERR_Okay;
   }

   if (Mute != 0) channel->Flags |= CHF::MUTE;
   else channel->Flags &= ~CHF::MUTE;
   set_channel_volume((extAudio *)Audio, channel);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
MixFrequency: Sets a channel's playback rate.

Use this function to set the playback rate of a mixer channel.

-INPUT-
obj(Audio) Audio: The target Audio object.
int Handle: The target channel.
int Frequency: The desired frequency.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

static ERROR sndMixFrequency(objAudio *Audio, LONG Handle, LONG Frequency)
{
   parasol::Log log("Mix:SetFrequency");

   log.trace("Audio: #%d, Channel: $%.8x, Frequency: %d", Audio->UID, Handle, Frequency);

   if ((!Audio) or (!Handle)) return log.warning(ERR_NullArgs);

   auto channel = ((extAudio *)Audio)->GetChannel(Handle);

   if (channel->Buffering) {
      add_mix_cmd(Audio, CMD::FREQUENCY, Handle, Frequency);
      return ERR_Okay;
   }

   channel->Frequency = Frequency;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
MixPan: Sets a channel's panning value.

Use this function to set a mixer channel's panning value.  Accepted values are between -1.0 (left) and 1.0 (right).

-INPUT-
obj(Audio) Audio: The target Audio object.
int Handle: The target channel.
double Pan: The desired pan value between -1.0 and 1.0.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

static ERROR sndMixPan(objAudio *Audio, LONG Handle, DOUBLE Pan)
{
   parasol::Log log("Mix:SetPan");

   log.trace("Audio: #%d, Channel: $%.8x, Pan: %.2f", Audio->UID, Handle, Pan);

   if ((!Audio) or (!Handle)) return log.warning(ERR_NullArgs);

   auto channel = ((extAudio *)Audio)->GetChannel(Handle);

   if (channel->Buffering) {
      add_mix_cmd(Audio, CMD::PAN, Handle, Pan);
      return ERR_Okay;
   }

   if (Pan < -1.0) channel->Pan = -1.0;
   else if (Pan > 1.0) channel->Pan = 1.0;
   else channel->Pan = Pan;

   set_channel_volume((extAudio *)Audio, channel);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
MixPosition: Sets a channel's playing position relative to the current sample.

This function will change the playing position of a mixer channel for the current sound sample.

-INPUT-
obj(Audio) Audio: The target Audio object.
int Handle: The target channel.
int Position: The new playing position, measured in bytes.

-ERRORS-
Okay
NullArgs
OutOfRange
-END-

*********************************************************************************************************************/

static ERROR sndMixPosition(objAudio *Audio, LONG Handle, LONG Position)
{
   parasol::Log log("Mix:SetPosition");

   log.trace("Audio: #%d, Channel: $%.8x, Position: %d", Audio->UID, Handle, Position);

   if ((!Audio) or (!Handle)) return log.warning(ERR_NullArgs);
   if (Position < 0) return log.warning(ERR_OutOfRange);

   auto channel = ((extAudio *)Audio)->GetChannel(Handle);

   if (channel->Buffering) {
      add_mix_cmd(Audio, CMD::POSITION, Handle, Position);
      return ERR_Okay;
   }

   if (!channel->SampleHandle) { // A sample must be defined for the channel.
      log.warning("Channel not associated with a sample.");
      return ERR_Failed;
   }

   const auto &sample = ((extAudio *)Audio)->Samples[channel->SampleHandle];

   if (!sample.Data) { // The sample reference must be valid and not stale.
      log.warning("On channel %d, referenced sample %d is unconfigured.", Handle, channel->SampleHandle);
      return ERR_Failed;
   }
   else if (Position > sample.SampleLength) return log.warning(ERR_OutOfRange);

   // If the sample is a stream object, we just need to send a Seek action to have the Stream owner acknowledge the
   // new audio position and have it feed us new audio data.

   if (sample.StreamID) {
      parasol::ScopedObjectLock<> stream(sample.StreamID, 5000);
      if (stream.granted()) {
         acSeekStart(*stream, sample.SeekStart + Position);
         acRead(*stream, sample.Data, sample.BufferLength, NULL);
      }

      channel->StreamPos = sample.SeekStart + Position;
      Position = 0; // Internally we want to start from byte position zero in our stream buffer
   }

   if (Audio->Flags & ADF_OVER_SAMPLING) sndMixFadeOut(Audio, Handle);

   // Convert position from bytes to samples

   LONG bitpos = Position >> sample_shift(sample.SampleType);

   // Check if sample has been changed, and if so, set the values to the channel structure

   if ((channel->Flags & CHF::CHANGED) != CHF::NIL) {
      channel->Flags &= ~CHF::CHANGED;

      // If channel status is released and the new sample does not have two loops, end the sample

      if ((sample.LoopMode != LOOP::SINGLE_RELEASE) and (sample.LoopMode != LOOP::DOUBLE) and (channel->State IS CHS::RELEASED)) {
         channel->State = CHS::FINISHED;
         return ERR_Okay;
      }
   }

   switch (channel->State) {
      case CHS::FINISHED:
      case CHS::PLAYING:
         // Either playing sample before releasing or playing has ended - check the first loop type.

         channel->LoopIndex = 1;
         switch (sample.Loop1Type) {
            case LTYPE::NIL:
               // No looping - if position is below sample end, set it and start playing there
               if (bitpos < sample.SampleLength) {
                  channel->Position    = bitpos;
                  channel->PositionLow = 0;
                  channel->State       = CHS::PLAYING;
                  channel->Flags       &= ~CHF::BACKWARD;
               }
               else channel->State = CHS::FINISHED;
               break;

            case LTYPE::UNIDIRECTIONAL:
               // Unidirectional looping - if position is below loop end, set it, otherwise set loop start as the
               // new position. Start playing in any case.
               if ( bitpos < sample.Loop1End ) channel->Position = bitpos;
               else channel->Position = sample.Loop1Start;
               channel->PositionLow = 0;
               channel->State       = CHS::PLAYING;
               channel->Flags      &= ~CHF::BACKWARD;
               break;

            case LTYPE::BIDIRECTIONAL:
               // Bidirectional looping - if position is below loop end, set it and start playing forward, otherwise
               // set loop end as the new position and start playing backwards.
               if (bitpos < sample.Loop1End ) {
                  channel->Position = bitpos;
                  channel->Flags &= ~CHF::BACKWARD;
               }
               else {
                  channel->Position = sample.Loop1End;
                  channel->Flags |= CHF::BACKWARD;
               }
               channel->PositionLow = 0;
               channel->State = CHS::PLAYING;
         }
         break;

      case CHS::RELEASED: // Playing after sample has been released - check second loop type.
         channel->LoopIndex = 2;
         switch (sample.Loop2Type) {
            case LTYPE::NIL:
               // No looping - if position is below sample end, set it and start playing there.

               if (bitpos < sample.SampleLength ) {
                  channel->Position    = bitpos;
                  channel->PositionLow = 0;
                  channel->State       = CHS::PLAYING;
                  channel->Flags       &= ~CHF::BACKWARD;
               }
               else channel->State = CHS::FINISHED;
               break;

            case LTYPE::UNIDIRECTIONAL:
               // Unidirectional looping - if position is below loop end, set it, otherwise set loop start as the
               // new position. Start playing in any case.
               if (bitpos < sample.Loop2End) channel->Position = bitpos;
               else channel->Position = sample.Loop2Start;
               channel->PositionLow = 0;
               channel->State = CHS::PLAYING;
               channel->Flags &= ~CHF::BACKWARD;
               break;

            case LTYPE::BIDIRECTIONAL:
               // Bidirectional looping - if position is below loop end, set it and start playing forward, otherwise
               // set loop end as the new position and start playing backwards.

               if (bitpos < sample.Loop2End) {
                  channel->Position = bitpos;
                  channel->Flags   &= ~CHF::BACKWARD;
               }
               else {
                  channel->Position = sample.Loop2End;
                  channel->Flags   |= CHF::BACKWARD;
               }
               channel->PositionLow = 0;
               channel->State       = CHS::PLAYING;
         }
         break;

      case CHS::STOPPED:
      default:
         // If sound has been stopped do nothing
         break;
   }

   if (Audio->Flags & ADF_OVER_SAMPLING) sndMixFadeIn(Audio, Handle);

   if (channel->State IS CHS::PLAYING) {
      parasol::SwitchContext context(Audio);
      if (((extAudio *)Audio)->Timer) UpdateTimer(((extAudio *)Audio)->Timer, -MIX_INTERVAL);
      else {
         auto call = make_function_stdc(audio_timer);
         SubscribeTimer(MIX_INTERVAL, &call, &((extAudio *)Audio)->Timer);
      }
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
MixPlay: Commences channel playback at a set frequency..

This function will start playback of the sound sample associated with the target mixer channel.  If the channel is
already in playback mode, it will be stopped to facilitate the new playback request.

-INPUT-
obj(Audio) Audio: The target Audio object.
int Handle: The target channel.
int Frequency: The new playing position, measured in bytes.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

static ERROR sndMixPlay(objAudio *Audio, LONG Handle, LONG Frequency)
{
   parasol::Log log("Mix:Play");

   log.trace("Audio: #%d, Channel: $%.8x", Audio->UID, Handle);

   if ((!Audio) or (!Handle)) return log.warning(ERR_NullArgs);
   if (Frequency <= 0) return log.warning(ERR_Args);

   auto channel = ((extAudio *)Audio)->GetChannel(Handle);

   if (channel->Buffering) {
      add_mix_cmd(Audio, CMD::PLAY, Frequency);
      return ERR_Okay;
   }

   if (Audio->Flags & ADF_OVER_SAMPLING) sndMixFadeOut(Audio, Handle);

   channel->State     = CHS::FINISHED; // Turn off previous sound
   channel->Frequency = Frequency; // New frequency

   return sndMixPosition(Audio, Handle, 0); // Setting position to the beginning of the sample also initiates playback
}

/*********************************************************************************************************************

-FUNCTION-
MixRate: Sets a new update rate for a channel.

This function will set a new update rate for all channels, measured in milliseconds.  The default update rate is 125,
which is equivalent to 5000Hz.

-INPUT-
obj(Audio) Audio: The target Audio object.
int Handle: The target channel.
int Rate: The new update rate in milliseconds.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

static ERROR sndMixRate(objAudio *Audio, LONG Handle, LONG Rate)
{
   parasol::Log log("Mix:SetRate");

   log.trace("Audio: #%d, Channel: $%.8x", Audio->UID, Handle);

   if ((!Audio) or (!Handle)) return log.warning(ERR_NullArgs);

   auto channel = ((extAudio *)Audio)->GetChannel(Handle);

   if (channel->Buffering) {
      add_mix_cmd(Audio, CMD::RATE, Handle, Rate);
      return ERR_Okay;
   }

   WORD index = Handle>>16;
   ((extAudio *)Audio)->Sets[index].UpdateRate = Rate;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
MixSample: Associate a sound sample with a mixer channel.

This function will associate a sound sample with a mixer channel.  Configuration should then follow (e.g. volume and
pan values).

The referenced Sample must have been added to the audio server via the @Audio.AddSample() or @Audio.AddStream()
methods.

-INPUT-
obj(Audio) Audio: The target Audio object.
int Handle: The target channel.
int Sample: A sample handle allocated from #Audio.AddSample() or #Audio.AddStream().

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

ERROR sndMixSample(objAudio *Audio, LONG Handle, LONG SampleIndex)
{
   parasol::Log log("Mix:SetSample");

   LONG idx = SampleIndex;

   log.trace("Audio: #%d, Channel: $%.8x, Sample: %d", Audio->UID, Handle, idx);

   if ((!Audio) or (!Handle)) return log.warning(ERR_NullArgs);

   if ((idx <= 0) or (idx >= (LONG)((extAudio *)Audio)->Samples.size())) {
      return log.warning(ERR_OutOfRange);
   }
   else if (!((extAudio *)Audio)->Samples[idx].Data) {
      log.warning("Sample #%d refers to a dead sample.", idx);
      return ERR_Failed;
   }
   else if (((extAudio *)Audio)->Samples[idx].SampleLength <= 0) {
      log.warning("Sample #%d has invalid sample length %d", idx, ((extAudio *)Audio)->Samples[idx].SampleLength);
      return ERR_Failed;
   }

   auto channel = ((extAudio *)Audio)->GetChannel(Handle);

   if (channel->Buffering) {
      add_mix_cmd(Audio, CMD::SAMPLE, Handle, SampleIndex);
      return ERR_Okay;
   }

   if (channel->SampleHandle IS idx) return ERR_Okay; // Already associated?

   channel->SampleHandle = idx;    // Set new sample number to channel
   channel->Flags |= CHF::CHANGED;  // Sample has been changed

   // If the new sample has one Amiga-compatible loop and playing has ended (not released or stopped), set the new
   // sample and start playing from loop start.

   auto &s = ((extAudio *)Audio)->Samples[idx];
   if ((s.LoopMode IS LOOP::AMIGA) and (channel->State IS CHS::FINISHED)) {
      // Set Amiga sample and start playing.  We won't do this with interpolated mixing, as this tends to cause clicks.

      if (!(Audio->Flags & ADF_OVER_SAMPLING)) {
         channel->State = CHS::PLAYING;
         sndMixPosition(Audio, Handle, s.Loop1Start);
      }
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
MixVolume: Changes the volume of a channel.

This function will change the volume of a mixer channel.  Valid values are between 0 and 1.0.

-INPUT-
obj(Audio) Audio: The target Audio object.
int Handle: The target channel.
double Volume: The new volume for the channel.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

static ERROR sndMixVolume(objAudio *Audio, LONG Handle, DOUBLE Volume)
{
   parasol::Log log("Mix:SetVolume");

   log.trace("Audio: #%d, Channel: $%.8x", Audio->UID, Handle);

   if ((!Audio) or (!Handle)) return log.warning(ERR_NullArgs);

   auto channel = ((extAudio *)Audio)->GetChannel(Handle);

   if (channel->Buffering) {
      add_mix_cmd(Audio, CMD::VOLUME, Volume);
      return ERR_Okay;
   }

   if (Volume > 1.0) channel->Volume = 1.0;
   else if (Volume < 0) channel->Volume = 0;
   else channel->Volume = Volume;

   set_channel_volume((extAudio *)Audio, channel);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
MixStop: Stops all playback on a channel.

This function will stop a channel that is currently playing.

-INPUT-
obj(Audio) Audio: The target Audio object.
int Handle: The target channel.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

ERROR sndMixStop(objAudio *Audio, LONG Handle)
{
   parasol::Log log("Mix:Stop");

   log.trace("Audio: #%d, Channel: $%.8x", Audio->UID, Handle);

   if ((!Audio) or (!Handle)) return log.warning(ERR_NullArgs);

   auto channel = ((extAudio *)Audio)->GetChannel(Handle);

   if (channel->Buffering) {
      add_mix_cmd(Audio, CMD::STOP, Handle);
      return ERR_Okay;
   }

   channel->State = CHS::STOPPED;

   if (Audio->Flags & ADF_OVER_SAMPLING) {
      channel += ((extAudio *)Audio)->Sets[Handle>>16].Total;
      channel->State = CHS::STOPPED;
   }
   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
MixStopLoop: Cancels any playback loop configured for a channel.

This function will cancel any loop that is associated with a mixer channel in playback mode.  This does not affect the
loop configuration if playback is restarted for the active sample.

-INPUT-
obj(Audio) Audio: The target Audio object.
int Handle: The target channel.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

static ERROR sndMixStopLoop(objAudio *Audio, LONG Handle)
{
   parasol::Log log("Mix:StopLoop");

   log.trace("Audio: #%d, Channel: $%.8x", Audio->UID, Handle);

   if ((!Audio) or (!Handle)) return log.warning(ERR_NullArgs);

   auto channel = ((extAudio *)Audio)->GetChannel(Handle);

   if (channel->Buffering) {
      add_mix_cmd(Audio, CMD::STOP_LOOPING, Handle);
      return ERR_Okay;
   }

   if (channel->State != CHS::PLAYING) return ERR_Okay;

   auto &sample = ((extAudio *)Audio)->Samples[channel->SampleHandle];

   if ((sample.LoopMode IS LOOP::SINGLE_RELEASE) or (sample.LoopMode IS LOOP::DOUBLE)) {
      channel->State = CHS::RELEASED;
   }

   return ERR_Okay;
}

