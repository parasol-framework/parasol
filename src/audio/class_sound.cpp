/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
Sound: Plays and records sound samples in a variety of different data formats.

The Sound class provides a simple API for programs to load and play audio sample files. By default all
loading and saving of sound data is in WAVE format.  Other audio formats such as MP3 can be supported through Sound
class extensions, if available.

Automatic streaming is enabled by default.  If an attempt is made to play an audio file that exceeds the maximum
buffer size, it will be streamed from the source location.  Streaming behaviour can be modified via the #Stream
field.

The following example illustrates playback of a sound sample that is one octave higher than its normal frequency.
The subscription to the OnStop callback will result in the program waking once the sample has finished
playback.

<pre>
local snd = obj.new('sound', {
   path = 'audio:samples/doorbell.wav',
   note = 'C6',
   onStop = function(Sound)
      proc.signal()
   end
})

snd.acActivate()

proc.sleep()
</pre>

-END-

**********************************************************************************************************************

NOTE: Ideally individual samples should always be played through the host's audio capabilities and not our internal
mixer.  The mixer is buffered and therefore always has a delay, whereas the host drivers should be able to play
individual samples with more immediacy.

*********************************************************************************************************************/

#include <array>

#define SECONDS_STREAM_BUFFER 2

#define SIZE_RIFF_CHUNK 12

static ERROR SOUND_GET_Active(extSound *, LONG *);

static ERROR SOUND_SET_Note(extSound *, CSTRING);

static const std::array<DOUBLE, 12> glScale = {
   1.0,         // C
   1.059435080, // CS
   1.122424798, // D
   1.189198486, // DS
   1.259909032, // E
   1.334823988, // F
   1.414172687, // FS
   1.498299125, // G
   1.587356190, // GS
   1.681764324, // A
   1.781752857, // AS
   1.887704009  // B
};

static OBJECTPTR clSound = NULL;

static ERROR find_chunk(extSound *, objFile *, CSTRING);
#ifdef USE_WIN32_PLAYBACK
static ERROR win32_audio_stream(extSound *, LARGE, LARGE);
#endif

//********************************************************************************************************************
// Send a callback to the client when playback stops.

static void sound_stopped_event(extSound *Self)
{
   if (Self->OnStop.Type IS CALL_STDC) {
      parasol::SwitchContext context(Self->OnStop.StdC.Context);
      auto routine = (void (*)(extSound *))Self->OnStop.StdC.Routine;
      routine(Self);
   }
   else if (Self->OnStop.Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = Self->OnStop.Script.Script)) {
         const ScriptArg args[] = {
            { "Sound",  FD_OBJECTPTR, { .Address = Self } }
         };
         ERROR error;
         scCallback(script, Self->OnStop.Script.ProcedureID, args, ARRAYSIZE(args), &error);
      }
   }
}

//********************************************************************************************************************

#ifndef USE_WIN32_PLAYBACK
static LONG read_stream(LONG Handle, LONG Offset, APTR Buffer, LONG Length)
{
   auto Self = (extSound *)CurrentContext();

   if ((Offset >= 0) and (Self->Position != Offset)) Self->seek(Offset, SEEK_START);

   if (Length > 0) {
      LONG result;
      Self->read(Buffer, Length, &result);
      return result;
   }

   return 0;
}

static void onstop_event(LONG SampleHandle)
{
   sound_stopped_event((extSound *)CurrentContext());
}

#endif

//********************************************************************************************************************
// Called when the estimated time for playback is over.

#ifdef _WIN32
static ERROR timer_playback_ended(extSound *Self, LARGE Elapsed, LARGE CurrentTime)
{
   parasol::Log log;
   log.trace("Sound streaming completed.");
   sound_stopped_event(Self);
   Self->PlaybackTimer = 0;
   // NB: We don't manually stop the audio streamer, it will automatically stop once buffers are clear.
   return ERR_Terminate;
}
#endif

//********************************************************************************************************************
// Configure a timer that will trigger when the playback is finished.  The Position cursor will be taken into account
// in determining playback length.

#ifdef USE_WIN32_PLAYBACK
static ERROR set_playback_trigger(extSound *Self)
{
   if (Self->OnStop.Type) {
      parasol::Log log(__FUNCTION__);
      const LONG bytes_per_sample = (((Self->Flags & SDF_STEREO) ? 2 : 1) * (Self->BitsPerSample>>3));
      const DOUBLE playback_time = DOUBLE((Self->Length - Self->Position) / bytes_per_sample) / DOUBLE(Self->Playback);
      if (playback_time < 0.01) {
         if (Self->PlaybackTimer) { UpdateTimer(Self->PlaybackTimer, 0); Self->PlaybackTimer = 0; }
         timer_playback_ended(Self, 0, 0);
      }
      else {
         log.trace("Playback time period set to %.2fs", playback_time);
         if (Self->PlaybackTimer) return UpdateTimer(Self->PlaybackTimer, playback_time + 0.01);
         else {
            auto call = make_function_stdc(&timer_playback_ended);
            return SubscribeTimer(playback_time + 0.01, &call, &Self->PlaybackTimer);
         }
      }
   }
   return ERR_Okay;
}
#endif

#ifdef _WIN32
void end_of_stream(OBJECTPTR Object, LONG BytesRemaining)
{
   if (Object->ClassID IS ID_SOUND) {
      auto Self = (extSound *)Object;
      if (Self->OnStop.Type) {
         parasol::Log log(__FUNCTION__);
         parasol::SwitchContext context(Object);
         const LONG bytes_per_sample = (((Self->Flags & SDF_STEREO) ? 2 : 1) * (Self->BitsPerSample>>3));
         const DOUBLE playback_time = (DOUBLE(BytesRemaining / bytes_per_sample) / DOUBLE(Self->Playback)) + 0.01;

         if (!Self->PlaybackTimer) {
            if (playback_time < 0.01) {
               timer_playback_ended(Self, 0, 0);
            }
            else {
               log.trace("Remaining time period set to %.2fs", playback_time);
               auto call = make_function_stdc(&timer_playback_ended);
               SubscribeTimer(playback_time, &call, &Self->PlaybackTimer);
            }
         }
      }
   }
}
#endif

//********************************************************************************************************************
// Stubs.

static LONG sample_format(extSound *Self) __attribute__((unused));
static LONG sample_format(extSound *Self)
{
   if (Self->BitsPerSample IS 8) {
      if (Self->Flags & SDF_STEREO) return SFM_U8_BIT_STEREO;
      else return SFM_U8_BIT_MONO;
   }
   else if (Self->BitsPerSample IS 16) {
      if (Self->Flags & SDF_STEREO) return SFM_S16_BIT_STEREO;
      else return SFM_S16_BIT_MONO;
   }
   return 0;
}

static ERROR snd_init_audio(extSound *Self) __attribute__((unused));
static ERROR snd_init_audio(extSound *Self)
{
   parasol::Log log;

   LONG count = 1;
   if (!FindObject("SystemAudio", ID_AUDIO, 0, &Self->AudioID, &count)) return ERR_Okay;

   extAudio *audio;
   ERROR error;
   if (!(error = NewNamedObject(ID_AUDIO, NF::UNIQUE, &audio, &Self->AudioID, "SystemAudio"))) {
      SetOwner(audio, CurrentTask());

      if (!acInit(audio)) {
         error = acActivate(audio);
      }
      else {
         acFree(audio);
         error = ERR_Init;
      }

      ReleaseObject(audio);
   }
   else if (error IS ERR_ObjectExists) return ERR_Okay;
   else error = ERR_NewObject;

   if (error) return log.warning(ERR_CreateObject);

   return error;
}

/*********************************************************************************************************************
-ACTION-
Activate: Plays the audio sample.
-END-
*********************************************************************************************************************/

static ERROR SOUND_Activate(extSound *Self, APTR Void)
{
   parasol::Log log;

   log.traceBranch("Position: %" PF64, Self->Position);

   if (!Self->Length) return log.warning(ERR_FieldNotSet);

#ifdef USE_WIN32_PLAYBACK
   // Optimised playback for Windows - this does not use our internal mixer.

   if (!Self->Active) {
      WORD channels = (Self->Flags & SDF_STEREO) ? 2 : 1;
      WAVEFORMATEX wave = {
         .Format            = WAVE_RAW,
         .Channels          = channels,
         .Frequency         = Self->Frequency,
         .AvgBytesPerSecond = Self->BytesPerSecond,
         .BlockAlign        = WORD(channels * (Self->BitsPerSample>>3)),
         .BitsPerSample     = WORD(Self->BitsPerSample),
         .ExtraLength       = 0
      };

      LONG buffer_len;
      if ((Self->Stream IS STREAM::ALWAYS) and (Self->Length > 16 * 1024)) {
         buffer_len = Self->BytesPerSecond * SECONDS_STREAM_BUFFER;
      }
      else if ((Self->Stream IS STREAM::SMART) and (Self->Length > 256 * 1024)) {
         buffer_len = Self->BytesPerSecond * SECONDS_STREAM_BUFFER;
      }
      else buffer_len = Self->Length;

      if (buffer_len > Self->Length) buffer_len = Self->Length;

      CSTRING strerr;
      if (Self->Length > buffer_len) {
         log.msg("Streaming enabled because sample length %d exceeds buffer size %d.", Self->Length, buffer_len);
         Self->Flags |= SDF_STREAM;
         strerr = sndCreateBuffer(Self, &wave, buffer_len, Self->Length, (PlatformData *)Self->PlatformData, TRUE);
      }
      else {
         // Create the buffer and fill it completely with sample data.
         buffer_len = Self->Length;
         Self->Flags &= ~SDF_STREAM;
         auto client_pos = Self->Position; // Save the seek cursor from pollution
         if (client_pos) Self->seek(0, SEEK_START);
         strerr = sndCreateBuffer(Self, &wave, buffer_len, Self->Length, (PlatformData *)Self->PlatformData, FALSE);
         Self->seek(client_pos, SEEK_START);
      }

      if (strerr) {
         log.warning("Failed to create audio buffer, reason: %s (sample length %d)", strerr, Self->Length);
         return ERR_Failed;
      }

      Self->Active = true;
   }

   if (Self->AudioID) {
      parasol::ScopedObjectLock<extAudio> audio(Self->AudioID, 200);
      if (audio.granted()) {
         sndVolume((PlatformData *)Self->PlatformData, audio->MasterVolume * Self->Volume);
      }
   }
   else sndVolume((PlatformData *)Self->PlatformData, Self->Volume);

   sndFrequency((PlatformData *)Self->PlatformData, Self->Playback);
   sndPan((PlatformData *)Self->PlatformData, Self->Pan);

   if (Self->Flags & SDF_STREAM) {
      auto call = make_function_stdc(win32_audio_stream);
      if (SubscribeTimer(0.25, &call, &Self->StreamTimer)) return log.warning(ERR_Failed);
   }
   else if (set_playback_trigger(Self)) return log.warning(ERR_Failed);

   auto response = sndPlay((PlatformData *)Self->PlatformData, (Self->Flags & SDF_LOOP) ? TRUE : FALSE, Self->Position);
   return response ? log.warning(ERR_Failed) : ERR_Okay;
#else

   if (!Self->Active) {
      // Determine the sample type

      LONG sampleformat = 0;
      if (Self->BitsPerSample IS 8) {
         if (Self->Flags & SDF_STEREO) sampleformat = SFM_U8_BIT_STEREO;
         else sampleformat = SFM_U8_BIT_MONO;
      }
      else if (Self->BitsPerSample IS 16) {
         if (Self->Flags & SDF_STEREO) sampleformat = SFM_S16_BIT_STEREO;
         else sampleformat = SFM_S16_BIT_MONO;
      }

      if (!sampleformat) return log.warning(ERR_InvalidData);

      // Create the audio buffer and fill it with sample data

      if ((Self->Stream IS STREAM::ALWAYS) and (Self->Length > 16 * 1024)) Self->Flags |= SDF_STREAM;
      else if ((Self->Stream IS STREAM::SMART) and (Self->Length > 256 * 1024)) Self->Flags |= SDF_STREAM;

      BYTE *buffer;
      if (Self->Flags & SDF_STREAM) {
         log.msg("Streaming enabled for playback in format $%.8x; Length: %d", sampleformat, Self->Length);

         struct sndAddStream stream;
         AudioLoop loop;
         if (Self->Flags & SDF_LOOP) {
            loop.LoopMode   = LOOP::SINGLE;
            loop.Loop1Type  = LTYPE::UNIDIRECTIONAL;
            loop.Loop1Start = Self->LoopStart;
            if (Self->LoopEnd) loop.Loop1End = Self->LoopEnd;
            else loop.Loop1End = Self->Length;

            stream.Loop     = &loop;
            stream.LoopSize = sizeof(loop);
         }
         else {
            stream.Loop     = NULL;
            stream.LoopSize = 0;
         }

         if (Self->OnStop.Type) stream.OnStop = make_function_stdc(&onstop_event);
         else stream.OnStop.Type = 0;

         stream.PlayOffset   = Self->Position;
         stream.Callback     = make_function_stdc(&read_stream);
         stream.SampleFormat = sampleformat;
         stream.SampleLength = Self->Length;

         if (!ActionMsg(MT_SndAddStream, Self->AudioID, &stream)) {
            Self->Handle = stream.Result;
         }
         else {
            log.warning("Failed to add sample to the Audio device.");
            return ERR_Failed;
         }
      }
      else if (!AllocMemory(Self->Length, MEM_DATA|MEM_NO_CLEAR, &buffer)) {
         auto client_pos = Self->Position;
         if (Self->Position) Self->seek(0, SEEK_START); // Ensure we're reading the entire sample from the start

         LONG result;
         if (!Self->read(buffer, Self->Length, &result)) {
            if (result != Self->Length) log.warning("Expected %d bytes, read %d", Self->Length, result);

            Self->seek(client_pos, SEEK_START);

            struct sndAddSample add;
            AudioLoop loop;

            if (Self->Flags & SDF_LOOP) {
               loop.LoopMode   = LOOP::SINGLE;
               loop.Loop1Type  = LTYPE::UNIDIRECTIONAL;
               loop.Loop1Start = Self->LoopStart;
               if (Self->LoopEnd) loop.Loop1End = Self->LoopEnd;
               else loop.Loop1End = Self->Length;

               add.Loop     = &loop;
               add.LoopSize = sizeof(loop);
            }
            else {
               add.Loop     = NULL;
               add.LoopSize = 0;
            }

            if (Self->OnStop.Type) add.OnStop = make_function_stdc(&onstop_event);
            else add.OnStop.Type = 0;

            add.SampleFormat = sampleformat;
            add.Data         = buffer;
            add.DataSize     = Self->Length;
            if (!ActionMsg(MT_SndAddSample, Self->AudioID, &add)) {
               Self->Handle = add.Result;

               FreeResource(buffer);
            }
            else {
               FreeResource(buffer);
               log.warning("Failed to add sample to the Audio device.");
               return ERR_Failed;
            }
         }
         else {
            FreeResource(buffer);
            return log.warning(ERR_Read);
         }
      }
      else return log.warning(ERR_AllocMemory);
   }

   Self->Active = true;

   parasol::ScopedObjectLock<extAudio> audio(Self->AudioID, 2000);
   if (audio.granted()) {
      // Restricted and streaming audio can be played on only one channel at any given time.  This search will check
      // if the sound object is already active on one of our channels.

      AudioChannel *channel = NULL;
      if (Self->Flags & (SDF_RESTRICT_PLAY|SDF_STREAM)) {
         Self->ChannelIndex &= 0xffff0000;
         LONG i;
         for (i=0; i < audio->MaxChannels; i++) {
            channel = audio->GetChannel(Self->ChannelIndex);
            if ((channel) and (channel->SampleHandle IS Self->Handle)) break;
            Self->ChannelIndex++;
         }
         if (i >= audio->MaxChannels) channel = NULL;
      }

      if (!channel) {
         // Find an available channel.  If all channels are in use, check the priorities to see if we can push anyone out.
         AudioChannel *priority = NULL;
         Self->ChannelIndex &= 0xffff0000;
         LONG i;
         for (i=0; i < audio->MaxChannels; i++) {
            if (auto channel = audio->GetChannel(Self->ChannelIndex)) {
               if (channel->isStopped()) break;
               else if (channel->Priority < Self->Priority) priority = channel;
            }
            Self->ChannelIndex++;
         }

         if (i >= audio->MaxChannels) {
            if (!(channel = priority)) {
               log.msg("Audio channel not available for playback.");
               return ERR_Failed;
            }
         }
      }

      sndMixStop(*audio, Self->ChannelIndex);

      if (!sndMixSample(*audio, Self->ChannelIndex, Self->Handle)) {
         if (sndMixVolume(*audio, Self->ChannelIndex, Self->Volume)) return log.warning(ERR_Failed);
         if (sndMixPan(*audio, Self->ChannelIndex, Self->Pan)) return log.warning(ERR_Failed);
         if (sndMixFrequency(*audio, Self->ChannelIndex, Self->Playback)) return log.warning(ERR_Failed);
         if (sndMixPlay(*audio, Self->ChannelIndex, Self->Position)) return log.warning(ERR_Failed);

         return ERR_Okay;
      }
      else {
         log.warning("Failed to set sample %d to channel $%.8x", Self->Handle, Self->ChannelIndex);
         return ERR_Failed;
      }
   }
   else return log.warning(ERR_AccessObject);
#endif
}

//****************************************************************************

static ERROR SOUND_ActionNotify(extSound *Self, struct acActionNotify *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Args->ActionID IS AC_Free) {
      if ((Self->OnStop.Type IS CALL_SCRIPT) and (Self->OnStop.Script.Script->UID IS Args->ObjectID)) {
         Self->OnStop.Type = CALL_NONE;
      }
   }

   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Deactivate: Stops the audio sample and resets the playback position.
-END-
*********************************************************************************************************************/

static ERROR SOUND_Deactivate(extSound *Self, APTR Void)
{
   parasol::Log log;

   log.branch();

   if (Self->StreamTimer) { UpdateTimer(Self->StreamTimer, 0); Self->StreamTimer = 0; }
   if (Self->PlaybackTimer) { UpdateTimer(Self->PlaybackTimer, 0); Self->PlaybackTimer = 0; }

   Self->Position = 0;

#ifdef USE_WIN32_PLAYBACK
   sndStop((PlatformData *)Self->PlatformData);
#else
   if (Self->ChannelIndex) {
      parasol::ScopedObjectLock<extAudio> audio(Self->AudioID);
      if (audio.granted()) { // Stop the sample if it's live.
         if (auto channel = audio->GetChannel(Self->ChannelIndex)) {
            if (channel->SampleHandle IS Self->Handle) sndMixStop(*audio, Self->ChannelIndex);
         }
      }
      else return log.warning(ERR_AccessObject);
   }
#endif

   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Disable: Disable playback of an active audio sample, equivalent to pausing.
-END-
*********************************************************************************************************************/

static ERROR SOUND_Disable(extSound *Self, APTR Void)
{
   parasol::Log log;

   log.branch();

#ifdef USE_WIN32_PLAYBACK
   sndStop((PlatformData *)Self->PlatformData);
#else
   if (!Self->ChannelIndex) return ERR_Okay;

   parasol::ScopedObjectLock<extAudio> audio(Self->AudioID, 5000);
   if (audio.granted()) {
      if (auto channel = audio->GetChannel(Self->ChannelIndex)) {
         if (channel->SampleHandle IS Self->Handle) sndMixStop(*audio, Self->ChannelIndex);
      }
   }
   else return log.warning(ERR_AccessObject);
#endif

   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Enable: Continues playing a sound if it has been disabled.
-END-
*********************************************************************************************************************/

static ERROR SOUND_Enable(extSound *Self, APTR Void)
{
   parasol::Log log;
   log.branch();

#ifdef USE_WIN32_PLAYBACK
   if (!Self->Handle) {
      log.msg("Playing back from position %" PF64, Self->Position);
      if (Self->Flags & SDF_LOOP) sndPlay((PlatformData *)Self->PlatformData, TRUE, Self->Position);
      else sndPlay((PlatformData *)Self->PlatformData, FALSE, Self->Position);
   }
#else
   if (!Self->ChannelIndex) return ERR_Okay;

   parasol::ScopedObjectLock<extAudio> audio(Self->AudioID, 5000);
   if (audio.granted()) {
      if (auto channel = audio->GetChannel(Self->ChannelIndex)) {
         if (channel->SampleHandle IS Self->Handle) sndMixContinue(*audio, Self->ChannelIndex);
      }
   }
   else return log.warning(ERR_AccessObject);
#endif

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR SOUND_Free(extSound *Self, APTR Void)
{
   if (Self->StreamTimer)   { UpdateTimer(Self->StreamTimer, 0); Self->StreamTimer = 0; }
   if (Self->PlaybackTimer) { UpdateTimer(Self->PlaybackTimer, 0); Self->PlaybackTimer = 0; }

#if defined(USE_WIN32_PLAYBACK)
   if (!Self->Handle) sndFree((PlatformData *)Self->PlatformData);
#endif

   Self->deactivate();

   if ((Self->Handle) and (Self->AudioID)) {
      parasol::ScopedObjectLock<extAudio> audio(Self->AudioID);
      if (audio.granted()) {
         sndRemoveSample(*audio, Self->Handle);
         Self->Handle = 0;
      }
   }

   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }
   if (Self->File) { acFree(Self->File); Self->File = NULL; }

   Self->~extSound();
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
GetVar: Retrieve custom tag values.

The following custom tag values are formally recognised and may be defined automatically when loading sample files:

<types type="Tag">
<type name="Author">The name of the person or organisation that created the sound sample.</type>
<type name="Copyright">Copyright details of an audio sample.</type>
<type name="Description">Long description for an audio sample.</type>
<type name="Disclaimer">The disclaimer associated with an audio sample.</type>
<type name="Software">The name of the application that was used to record the audio sample.</type>
<type name="Title">The title of the audio sample.</type>
<type name="Quality">The compression quality value if the source is an MP3 stream.</type>
</types>

*********************************************************************************************************************/

static ERROR SOUND_GetVar(extSound *Self, struct acGetVar *Args)
{
   if ((!Args) or (!Args->Field)) return ERR_NullArgs;

   std::string name(Args->Field);
   if (Self->Tags.contains(name)) {
      StrCopy(Self->Tags[name].c_str(), Args->Buffer, Args->Size);
      return ERR_Okay;
   }
   else return ERR_UnsupportedField;
}

/*********************************************************************************************************************
-ACTION-
Init: Prepares a sound object for usage.
-END-
*********************************************************************************************************************/

#ifdef USE_WIN32_PLAYBACK

static ERROR SOUND_Init(extSound *Self, APTR Void)
{
   parasol::Log log;
   LONG id, len;
   ERROR error;

   // Find the local audio object or create one to ease the developer's workload.

   if (!Self->AudioID) {
      if ((error = snd_init_audio(Self))) return error;
   }

   // Open channels for sound sample playback.

   if (!(Self->ChannelIndex = glSoundChannels[Self->AudioID])) {
      parasol::ScopedObjectLock<extAudio> audio(Self->AudioID, 3000);
      if (audio.granted()) {
         if (!sndOpenChannels(*audio, audio->MaxChannels, &Self->ChannelIndex)) {
            glSoundChannels[Self->AudioID] = Self->ChannelIndex;
         }
         else {
            log.warning("Failed to open audio channels.");
            return ERR_Failed;
         }
      }
      else return log.warning(ERR_AccessObject);
   }

   STRING path;
   if ((Self->Flags & SDF_NEW) or (Self->get(FID_Path, &path) != ERR_Okay) or (!path)) {
      // If the sample is new or no path has been specified, create an audio sample from scratch (e.g. to record
      // audio to disk).

      return ERR_Okay;
   }

   // Load the sound file's header and test it to see if it matches our supported file format.

   if ((Self->File = objFile::create::integral(fl::Path(path), fl::Flags(FL_READ|FL_APPROXIMATE)))) {
      Self->File->read(Self->Header, (LONG)sizeof(Self->Header));

      if ((StrCompare((CSTRING)Self->Header, "RIFF", 4, STR_CASE) != ERR_Okay) or
          (StrCompare((CSTRING)Self->Header + 8, "WAVE", 4, STR_CASE) != ERR_Okay)) {
         acFree(Self->File);
         Self->File = NULL;
         return ERR_NoSupport;
      }
   }
   else return log.warning(ERR_File);

   // Read the RIFF header

   Self->File->seek(12.0, SEEK_START);
   if (flReadLE(Self->File, &id)) return ERR_Read; // Contains the characters "fmt "
   if (flReadLE(Self->File, &len)) return ERR_Read; // Length of data in this chunk

   WAVEFormat WAVE;
   LONG result;
   if (Self->File->read(&WAVE, len, &result) or (result != len)) {
      return log.warning(ERR_Read);
   }

   // Check the format of the sound file's data

   if ((WAVE.Format != WAVE_ADPCM) and (WAVE.Format != WAVE_RAW)) {
      log.msg("This file's WAVE data format is not supported (type %d).", WAVE.Format);
      return ERR_InvalidData;
   }

   // Look for the "data" chunk

   if (find_chunk(Self, Self->File, "data") != ERR_Okay) {
      return log.warning(ERR_Read);
   }

   if (flReadLE(Self->File, &Self->Length)) return ERR_Read; // Length of audio data in this chunk

   if (Self->Length & 1) Self->Length++;

   // Setup the sound structure

   Self->File->get(FID_Position, &Self->DataOffset);

   Self->Format         = WAVE.Format;
   Self->BytesPerSecond = WAVE.AvgBytesPerSecond;
   Self->BitsPerSample  = WAVE.BitsPerSample;
   if (WAVE.Channels IS 2) Self->Flags |= SDF_STEREO;
   if (Self->Frequency <= 0) Self->Frequency = WAVE.Frequency;
   if (Self->Playback <= 0)  Self->Playback  = Self->Frequency;

   if (Self->Flags & SDF_NOTE) {
      Self->set(FID_Note, Self->Note);
      Self->Flags &= ~SDF_NOTE;
   }

   log.trace("Bits: %d, Freq: %d, KBPS: %d, ByteLength: %d, DataOffset: %d", Self->BitsPerSample, Self->Frequency, Self->BytesPerSecond, Self->Length, Self->DataOffset);

   return ERR_Okay;
}

#else // Use the internal mixer

static ERROR SOUND_Init(extSound *Self, APTR Void)
{
   parasol::Log log;
   LONG id, len, result, pos;
   ERROR error;

   if (!Self->AudioID) {
      if ((error = snd_init_audio(Self))) return error;
   }

   // Open channels for sound sample playback.

   if (!(Self->ChannelIndex = glSoundChannels[Self->AudioID])) {
      parasol::ScopedObjectLock<extAudio> audio(Self->AudioID, 3000);
      if (audio.granted()) {
         if (!sndOpenChannels(*audio, audio->MaxChannels, &Self->ChannelIndex)) {
            glSoundChannels[Self->AudioID] = Self->ChannelIndex;
         }
         else {
            log.warning("Failed to open audio channels.");
            return ERR_Failed;
         }
      }
      else return log.warning(ERR_AccessObject);
   }

   STRING path = NULL;
   Self->get(FID_Path, &path);

   if ((Self->Flags & SDF_NEW) or (!path)) {
      log.msg("Sample created as new (without sample data).");

      // If the sample is new or no path has been specified, create an audio sample from scratch (e.g. to
      // record audio to disk).

      return ERR_Okay;
   }

   // Load the sound file's header and test it to see if it matches our supported file format.

   if ((Self->File = objFile::create::integral(fl::Path(path), fl::Flags(FL_READ|FL_APPROXIMATE)))) {
      if (!Self->File->read(Self->Header, sizeof(Self->Header))) {
         if ((StrCompare((CSTRING)Self->Header, "RIFF", 4, STR_CASE) != ERR_Okay) or
             (StrCompare((CSTRING)Self->Header + 8, "WAVE", 4, STR_CASE) != ERR_Okay)) {
            return ERR_NoSupport;
         }
      }
      else {
         log.warning("Failed to read file header.");
         return ERR_Read;
      }
   }
   else return log.warning(ERR_File);

   // Read the FMT header

   Self->File->seek(12, SEEK_START);
   if (flReadLE(Self->File, &id)) return ERR_Read; // Contains the characters "fmt "
   if (flReadLE(Self->File, &len)) return ERR_Read; // Length of data in this chunk

   WAVEFormat WAVE;
   if (Self->File->read(&WAVE, len, &result) or (result < len)) {
      log.warning("Failed to read WAVE format header (got %d, expected %d)", result, len);
      return ERR_Read;
   }

   // Check the format of the sound file's data

   if ((WAVE.Format != WAVE_ADPCM) and (WAVE.Format != WAVE_RAW)) {
      log.warning("This file's WAVE data format is not supported (type %d).", WAVE.Format);
      return ERR_InvalidData;
   }

   // Look for the cue chunk for loop information

   Self->File->get(FID_Position, &pos);
#if 0
   if (find_chunk(Self, Self->File, "cue ") IS ERR_Okay) {
      data_p += 32;
      flReadLE(Self->File, &info.loopstart);
      // if the next chunk is a LIST chunk, look for a cue length marker
      if (find_chunk(Self, Self->File, "LIST") IS ERR_Okay) {
         if (!strncmp (data_p + 28, "mark", 4)) {
            data_p += 24;
            flReadLE(Self->File, &i);	// samples in loop
            info.samples = info.loopstart + i;
         }
      }
   }
#endif
   Self->File->seekStart(pos);

   // Look for the "data" chunk

   if (find_chunk(Self, Self->File, "data") != ERR_Okay) {
      return log.warning(ERR_Read);
   }

   // Setup the sound structure

   flReadLE(Self->File, &Self->Length); // Length of audio data in this chunk

   Self->File->get(FID_Position, &Self->DataOffset);

   Self->Format         = WAVE.Format;
   Self->BytesPerSecond = WAVE.AvgBytesPerSecond;
   Self->BitsPerSample  = WAVE.BitsPerSample;
   if (WAVE.Channels IS 2)   Self->Flags |= SDF_STEREO;
   if (Self->Frequency <= 0) Self->Frequency = WAVE.Frequency;
   if (Self->Playback <= 0)  Self->Playback  = Self->Frequency;

   if (Self->Flags & SDF_NOTE) {
      SOUND_SET_Note(Self, Self->NoteString);
      Self->Flags &= ~SDF_NOTE;
   }

   if ((Self->BitsPerSample != 8) and (Self->BitsPerSample != 16)) {
      log.warning("Bits-Per-Sample of %d not supported.", Self->BitsPerSample);
      return ERR_InvalidData;
   }

   return ERR_Okay;
}

#endif

//********************************************************************************************************************

static ERROR SOUND_NewObject(extSound *Self, APTR Void)
{
   new (Self) extSound;

   Self->Compression = 50;     // 50% compression by default
   Self->Volume      = 1.0;    // Playback at 100% volume level
   Self->Pan         = 0;
   Self->Playback    = 0;
   Self->Note        = NOTE_C; // Standard pitch
   Self->Stream      = STREAM::SMART;
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Read: Read decoded audio from the sound sample.

This action will read decoded audio from the sound sample.  Decoding is a live process and it may take some time for
all data to be returned if the requested amount of data is considerable.  The starting point for the decoded data
is determined by the #Position value.

-END-
*********************************************************************************************************************/

static ERROR SOUND_Read(extSound *Self, struct acRead *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   log.traceBranch("Length: %d, Offset: %" PF64, Args->Length, Self->Position);

   if (Args->Length <= 0) {
      Args->Result = 0;
      return ERR_Okay;
   }

   // Don't read more than the known raw sample length

   LONG result;
   if (Self->Position + Args->Length > Self->Length) {
      if (auto error = Self->File->read(Args->Buffer, Self->Length - Self->Position, &result)) return error;
   }
   else if (auto error = Self->File->read(Args->Buffer, Args->Length, &result)) return error;

   Self->Position += result;
   Args->Result = result;

   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
SaveToObject: Saves audio sample data to an object.
-END-
*********************************************************************************************************************/

static ERROR SOUND_SaveToObject(extSound *Self, struct acSaveToObject *Args)
{
   parasol::Log log;

   // This routine is used if the developer is trying to save the sound data as a specific subclass type.

   if ((Args->ClassID) and (Args->ClassID != ID_SOUND)) {
      auto mclass = (objMetaClass *)FindClass(Args->ClassID);

      ERROR (**routine)(OBJECTPTR, APTR);
      if ((!mclass->getPtr(FID_ActionTable, (APTR *)&routine)) and (routine)) {
         if (routine[AC_SaveToObject]) {
            return routine[AC_SaveToObject](Self, Args);
         }
         else return log.warning(ERR_NoSupport);
      }
      else return log.warning(ERR_GetField);
   }

   // TODO: Save the sound data as a wave file




   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Seek: Moves the cursor position for reading data.

Use Seek to move the read cursor within the decoded audio stream and update #Position.  This will affect the
Read action.  If the sample is in active playback at the time of the call, the playback position will also be moved.

-END-
*********************************************************************************************************************/

static ERROR SOUND_Seek(extSound *Self, struct acSeek *Args)
{
   parasol::Log log;

   // NB: Sub-classes may divert their functionality to this routine if the sample is fully buffered.

   if (!Args) return log.warning(ERR_NullArgs);
   if (!Self->initialised()) return log.warning(ERR_NotInitialised);

   if (Args->Position IS SEEK_START)         Self->Position = F2T(Args->Offset);
   else if (Args->Position IS SEEK_END)      Self->Position = Self->Length - F2T(Args->Offset);
   else if (Args->Position IS SEEK_CURRENT)  Self->Position += F2T(Args->Offset);
   else if (Args->Position IS SEEK_RELATIVE) Self->Position = Self->Length * Args->Offset;
   else return log.warning(ERR_Args);

   if (Self->Position < 0) Self->Position = 0;
   else if (Self->Position > Self->Length) Self->Position = Self->Length;
   else { // Retain correct byte alignment.
      LONG align = (((Self->Flags & SDF_STEREO) ? 2 : 1) * (Self->BitsPerSample>>3)) - 1;
      Self->Position &= ~align;
   }

   log.traceBranch("Seek to %" PF64 " + %d", Self->Position, Self->DataOffset);

   parasol::ScopedObjectLock<extAudio> audio(Self->AudioID, 2000);
   if (audio.granted()) {
      if ((Self->File) and (!Self->isSubClass())) {
         Self->File->seekStart(Self->DataOffset + Self->Position);
      }

      #ifdef USE_WIN32_PLAYBACK
         if (sndCheckActivity((PlatformData *)Self->PlatformData) > 0) {
            set_playback_trigger(Self);
            sndSetPosition((PlatformData *)Self->PlatformData, Self->Position);
         }
      #else
         if (Self->Handle) {
            audio->Samples[Self->Handle].PlayPos = BYTELEN(Self->Position);

            if ((!audio->Samples[Self->Handle].Stream) and (Self->Active)) {
               // Sample is fully buffered.  Adjust position now if it's in playback.

               if (Self->ChannelIndex) {
                  if (auto channel = audio->GetChannel(Self->ChannelIndex)) {
                     if (!channel->isStopped()) {
                        sndMixPlay(*audio, Self->ChannelIndex, Self->Position);
                     }
                  }
               }
            }
         }
      #endif
   }
   else return log.warning(ERR_AccessObject);

   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
SetVar: Define custom tags that will be saved with the sample data.
-END-
*********************************************************************************************************************/

static ERROR SOUND_SetVar(extSound *Self, struct acSetVar *Args)
{
   if ((!Args) or (!Args->Field) or (!Args->Field[0])) return ERR_NullArgs;

   Self->Tags[std::string(Args->Field)] = Args->Value;
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Active: Returns TRUE if the sound sample is being played back.
-END-
*********************************************************************************************************************/

static ERROR SOUND_GET_Active(extSound *Self, LONG *Value)
{
#ifdef USE_WIN32_PLAYBACK
   parasol::Log log;

   if (Self->Active) {
      WORD status = sndCheckActivity((PlatformData *)Self->PlatformData);

      if (status IS 0) *Value = FALSE;
      else if (status > 0) *Value = TRUE;
      else {
         log.warning("Error retrieving active status.");
         *Value = FALSE;
      }
   }
   else *Value = FALSE;

   return ERR_Okay;
#else
   *Value = FALSE;

   if (Self->ChannelIndex) {
      parasol::ScopedObjectLock<extAudio> audio(Self->AudioID);
      if (audio.granted()) {
         if (auto channel = audio->GetChannel(Self->ChannelIndex)) {
            if (!channel->isStopped()) *Value = TRUE;
         }
      }
      else return ERR_AccessObject;
   }
#endif

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Audio: Refers to the audio object/device to use for playback.

Set this field if a specific @Audio object should be targeted when playing the sound sample.

-FIELD-
BitsPerSample: Indicates the sample rate of the audio sample, typically 8 or 16 bit.

-FIELD-
BytesPerSecond: The flow of bytes-per-second when the sample is played at normal frequency.

This field is set on initialisation.  It indicates the total number of bytes per second that will be played if the
sample is played back at its normal frequency.

-FIELD-
ChannelIndex: Refers to the channel that the sound is playing through.

This field reflects the audio channel index that the sound is currently playing through, or has most recently played
through.

-FIELD-
Compression: Determines the amount of compression used when saving an audio sample.

Setting the Compression field will determine how much compression is applied when saving an audio sample.  The range of
compression is 0 to 100%, with 100% being the strongest level available while 0% is uncompressed and loss-less.  This
field is ignored if the file format does not support compression.

-FIELD-
Duration: Returns the duration of the sample, measured in seconds.

*********************************************************************************************************************/

static ERROR SOUND_GET_Duration(extSound *Self, DOUBLE *Value)
{
   if (Self->Length) {
      const LONG bytes_per_sample = (((Self->Flags & SDF_STEREO) ? 2 : 1) * (Self->BitsPerSample>>3));
      *Value = DOUBLE(Self->Length / bytes_per_sample) / DOUBLE(Self->Playback ? Self->Playback : Self->Frequency);
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

/*********************************************************************************************************************

-FIELD-
Flags: Optional initialisation flags.
Lookup: SDF

*********************************************************************************************************************/

static ERROR SOUND_SET_Flags(extSound *Self, LONG Value)
{
   Self->Flags = (Self->Flags & 0xffff0000) | (Value & 0x0000ffff);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Frequency: The frequency of a sampled sound is specified here.

This field specifies the frequency of the sampled sound data.  If the frequency cannot be determined from the source,
this value will be zero.

Note that if the playback frequency needs to be altered, set the #Playback field.

-FIELD-
Header: Contains the first 128 bytes of data in a sample's file header.

The Header field is a pointer to a 128 byte buffer that contains the first 128 bytes of information read from an audio
file on initialisation.  This special field is considered to be helpful only to developers writing add-on components
for the sound class.

The buffer that is referred to by the Header field is not populated until the Init action is called on the sound object.

*********************************************************************************************************************/

static ERROR SOUND_GET_Header(extSound *Self, BYTE **Value, LONG *Elements)
{
   *Value = (BYTE *)Self->Header;
   *Elements = ARRAYSIZE(Self->Header);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Length: Indicates the total byte-length of sample data.

This field specifies the length of the sample data in bytes.  To get the length of the sample in seconds, divide this
value by the #BytesPerSecond field.

*********************************************************************************************************************/

static ERROR SOUND_SET_Length(extSound *Self, LONG Value)
{
   parasol::Log log;
   if (Value >= 0) {
      Self->Length = Value;

      #ifdef USE_WIN32_PLAYBACK
         if (Self->initialised()) {
            sndLength((PlatformData *)Self->PlatformData, Value);
         }
         return ERR_Okay;
      #else
         if ((Self->Handle) and (Self->AudioID)) {
            parasol::ScopedObjectLock<objAudio> audio(Self->AudioID);
            if (audio.granted()) {
               return sndSetSampleLength(*audio, Self->Handle, Value);
            }
            else return log.warning(ERR_AccessObject);
         }
         else return ERR_Okay;
      #endif
   }
   else return log.warning(ERR_InvalidValue);
}

/*********************************************************************************************************************

-FIELD-
LoopEnd: The byte position at which sample looping will end.

When using looped samples (via the `SDF_LOOP` flag), set the LoopEnd field if the sample should end at a position
that is earlier than the sample's actual length.  The LoopEnd value is specified in bytes and must be less or equal
to the length of the sample and greater than the #LoopStart value.

-FIELD-
LoopStart: The byte position at which sample looping begins.

When using looped samples (via the `SDF_LOOP` flag), set the LoopStart field if the sample should begin at a position
other than zero.  The LoopStart value is specified in bytes and must be less than the length of the sample and the
#LoopEnd value.

Note that the LoopStart variable does not affect the position at which playback occurs for the first time - it only
affects the restart position when the end of the sample is reached.

-FIELD-
Note: The musical note to use when playing a sound sample.
Lookup: NOTE

Set the Note field to alter the playback frequency of a sound sample.  Setting this field as opposed
to the #Playback frequency will assure that the sample is played at a correctly scaled tone.

The Note field can be set using either string or integer based format.  If using the integer format, the chosen
value will reflect the position on a musical keyboard.  A value of zero refers to the middle C key.  Each
octave is measured in sets of 12 notes, so a value of 24 would indicate a C note at 3 times normal playback.  To play
at lower values, simply choose a negative integer to slow down sample playback.

Setting the Note field with the string format is useful if human readability is valuable.  The correct format
is `KEY OCTAVE SHARP`.  Here are some examples: `C5, D7#, G2, E3S`.

The middle C key for this format is `C5`.  The maximum octave that you can achieve for the string format is 9
and the lowest is 0.  Use either the `S` character or the `#` character for referral to a sharp note.

*********************************************************************************************************************/

static ERROR SOUND_GET_Note(extSound *Self, CSTRING *Value)
{
   switch(Self->Note) {
      case NOTE_C:  Self->NoteString[0] = 'C';
                    Self->NoteString[1] = '5' + Self->Octave;
                    Self->NoteString[2] = 0;
                    break;
      case NOTE_CS: Self->NoteString[0] = 'C';
                    Self->NoteString[1] = '5' + Self->Octave;
                    Self->NoteString[2] = '#';
                    Self->NoteString[3] = 0;
                    break;
      case NOTE_D:  Self->NoteString[0] = 'D';
                    Self->NoteString[1] = '5' + Self->Octave;
                    Self->NoteString[2] = 0;
                    break;
      case NOTE_DS: Self->NoteString[0] = 'D';
                    Self->NoteString[1] = '5' + Self->Octave;
                    Self->NoteString[2] = '#';
                    Self->NoteString[3] = 0;
                    break;
      case NOTE_E:  Self->NoteString[0] = 'E';
                    Self->NoteString[1] = '5' + Self->Octave;
                    Self->NoteString[2] = 0;
                    break;
      case NOTE_F:  Self->NoteString[0] = 'F';
                    Self->NoteString[1] = '5' + Self->Octave;
                    Self->NoteString[2] = 0;
                    break;
      case NOTE_FS: Self->NoteString[0] = 'F';
                    Self->NoteString[1] = '5' + Self->Octave;
                    Self->NoteString[2] = '#';
                    Self->NoteString[3] = 0;
                    break;
      case NOTE_G:  Self->NoteString[0] = 'G';
                    Self->NoteString[1] = '5' + Self->Octave;
                    Self->NoteString[2] = 0;
                    break;
      case NOTE_GS: Self->NoteString[0] = 'G';
                    Self->NoteString[1] = '5' + Self->Octave;
                    Self->NoteString[2] = '#';
                    Self->NoteString[3] = 0;
                    break;
      case NOTE_A:  Self->NoteString[0] = 'A';
                    Self->NoteString[1] = '5' + Self->Octave;
                    Self->NoteString[2] = 0;
                    break;
      case NOTE_AS: Self->NoteString[0] = 'A';
                    Self->NoteString[1] = '5' + Self->Octave;
                    Self->NoteString[2] = '#';
                    Self->NoteString[3] = 0;
                    break;
      case NOTE_B:  Self->NoteString[0] = 'B';
                    Self->NoteString[1] = '5' + Self->Octave;
                    Self->NoteString[2] = 0;
                    break;
      default:      Self->NoteString[0] = 0;
   }
   *Value = Self->NoteString;

   return ERR_Okay;
}

static ERROR SOUND_SET_Note(extSound *Self, CSTRING Value)
{
   parasol::Log log;

   if (!*Value) return ERR_Okay;

   LONG i, note;
   for (i=0; (Value[i]) and (i < 3); i++) Self->NoteString[i] = Value[i];
   Self->NoteString[i] = 0;

   CSTRING str = Value;
   if (((*Value >= '0') and (*Value <= '9')) or (*Value IS '-')) {
      note = StrToInt(Value);
   }
   else {
      note = 0;
      switch (*str) {
         case 'C': case 'c': note = NOTE_C; break;
         case 'D': case 'd': note = NOTE_D; break;
         case 'E': case 'e': note = NOTE_E; break;
         case 'F': case 'f': note = NOTE_F; break;
         case 'G': case 'g': note = NOTE_G; break;
         case 'A': case 'a': note = NOTE_A; break;
         case 'B': case 'b': note = NOTE_B; break;
         default:  note = NOTE_C;
      }
      str++;
      if ((*str >= '0') and (*str <= '9')) {
         note += NOTE_OCTAVE * (*str - '5');
         str++;
      }
      if ((*str IS 'S') or (*str IS 's') or (*str IS '#')) note++; // Sharp note
   }

   if ((note > NOTE_OCTAVE * 5) or (note < -(NOTE_OCTAVE * 5))) return log.warning(ERR_OutOfRange);

   Self->Flags |= SDF_NOTE;

   // Calculate the note value

   if ((Self->Note = note) < 0) Self->Note = -Self->Note;
   Self->Note = Self->Note % NOTE_OCTAVE;
   if (Self->Note > NOTE_B) Self->Note = NOTE_B;

   // Calculate the octave value if the note is set outside of the normal range

   if (note < 0) Self->Octave = (note / NOTE_OCTAVE) - 1;
   else if (note > NOTE_B) Self->Octave = note / NOTE_OCTAVE;

   if (Self->Octave < -5) Self->Octave = -5;
   else if (Self->Octave > 5) Self->Octave = 5;

   // Return if there is no frequency setting yet

   if (!Self->Frequency) return ERR_Okay;

   // Get the default frequency and adjust it to suit the requested octave/scale
   Self->Playback = Self->Frequency;
   if (Self->Octave > 0) {
      for (i=0; i < Self->Octave; i++) Self->Playback = Self->Playback<<1;
   }
   else if (Self->Octave < 0) {
      for (i=0; i > Self->Octave; i--) Self->Playback = Self->Playback>>1;
   }

   // Tune the playback frequency to match the requested note

   Self->Playback = (LONG)(Self->Playback * glScale[Self->Note]);

   // If the sound is playing, set the new playback frequency immediately

#ifdef USE_WIN32_PLAYBACK
   if (Self->initialised()) {
      sndFrequency((PlatformData *)Self->PlatformData, Self->Playback);
   }
#else
   if (Self->ChannelIndex) {
      parasol::ScopedObjectLock<extAudio> audio(Self->AudioID, 200);
      if (audio.granted()) {
         sndMixFrequency(*audio, Self->ChannelIndex, Self->Playback);
      }
      else return ERR_AccessObject;
   }
#endif

   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Octave: The octave to use for sample playback.

The Octave field determines the octave to use when playing back a sound sample.  The default setting is zero, which
represents the octave at which the sound was sampled.  Setting a negative octave will lower the playback rate, while
positive values raise the playback rate.  The minimum octave setting is -5 and the highest setting is +5.

The octave can also be adjusted by setting the #Note field.  Setting the Octave field directly is useful if
you need to quickly double or halve the playback rate.

*********************************************************************************************************************/

static ERROR SOUND_SET_Octave(extSound *Self, LONG Value)
{
   if ((Value < -10) or (Value > 10))
   Self->Octave = Value;
   return Self->set(FID_Note, Self->Note);
}

/*********************************************************************************************************************

-FIELD-
OnStop: This callback is triggered when sample playback stops.

Set OnStop to a callback function to receive an event trigger when sample playback stops.  The synopsis for the
function is as follows:

<pre>
void OnStop(*Sound)
</pre>

The timing of this event does not guarantee precision, but should be accurate to approximately 1/100th of a second
in most cases.

*********************************************************************************************************************/

static ERROR SOUND_GET_OnStop(extSound *Self, FUNCTION **Value)
{
   if (Self->OnStop.Type != CALL_NONE) {
      *Value = &Self->OnStop;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SOUND_SET_OnStop(extSound *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->OnStop.Type IS CALL_SCRIPT) UnsubscribeAction(Self->OnStop.Script.Script, AC_Free);
      Self->OnStop = *Value;
      if (Self->OnStop.Type IS CALL_SCRIPT) SubscribeAction(Self->OnStop.Script.Script, AC_Free);
   }
   else Self->OnStop.Type = CALL_NONE;
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Pan: Determines the horizontal position of a sound when played through stereo speakers.

The Pan field adjusts the "horizontal position" of a sample that is being played through stereo speakers.
The default value for this field is zero, which plays the sound through both speakers at an equal level.  The minimum
value is -1.0 to force play through the left speaker and the maximum value is 1.0 for the right speaker.

*********************************************************************************************************************/

static ERROR SOUND_SET_Pan(extSound *Self, DOUBLE Value)
{
   Self->Pan = Value;

   if (Self->Pan < -1.0) Self->Pan = -1.0;
   else if (Self->Pan > 1.0) Self->Pan = 1.0;

#ifdef USE_WIN32_PLAYBACK
   if (Self->initialised()) {
      sndPan((PlatformData *)Self->PlatformData, Self->Pan);
   }
#else
   if (Self->ChannelIndex) {
      parasol::ScopedObjectLock<extAudio> audio(Self->AudioID, 200);
      if (audio.granted()) {
         sndMixPan(*audio, Self->ChannelIndex, Self->Pan);
      }
      else return ERR_AccessObject;
   }
#endif

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Path: Location of the audio sample data.

This field must refer to a file that contains the audio data that will be loaded.  If creating a new sample
with the `SDF_NEW` flag, it is not necessary to define a file source.

*********************************************************************************************************************/

static ERROR SOUND_GET_Path(extSound *Self, STRING *Value)
{
   if ((*Value = Self->Path)) return ERR_Okay;
   else return ERR_FieldNotSet;
}

static ERROR SOUND_SET_Path(extSound *Self, CSTRING Value)
{
   parasol::Log log;

   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }

   if ((Value) and (*Value)) {
      LONG i = strlen(Value);
      if (!AllocMemory(i+1, MEM_STRING|MEM_NO_CLEAR, (void **)&Self->Path)) {
         for (i=0; Value[i]; i++) Self->Path[i] = Value[i];
         Self->Path[i] = 0;
      }
      else return log.warning(ERR_AllocMemory);
   }
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Playback: The playback frequency of the sound sample can be defined here.

Set this field to define the exact frequency of a sample's playback.  The playback frequency can be modified at
any time, including during audio playback if real-time adjustments to a sample's audio output rate is desired.

*********************************************************************************************************************/

static ERROR SOUND_SET_Playback(extSound *Self, LONG Value)
{
   parasol::Log log;

   if ((Value < 0) or (Value > 500000)) return ERR_OutOfRange;

   Self->Playback = Value;
   Self->Flags &= ~SDF_NOTE;

#ifdef USE_WIN32_PLAYBACK
   if (Self->initialised()) {
      sndFrequency((PlatformData *)Self->PlatformData, Self->Playback);
      if (Self->PlaybackTimer) set_playback_trigger(Self);
   }
#else
   if (Self->ChannelIndex) {
      parasol::ScopedObjectLock<extAudio> audio(Self->AudioID, 200);
      if (audio.granted()) {
         sndMixFrequency(*audio, Self->ChannelIndex, Self->Playback);
      }
      else return log.warning(ERR_AccessObject);
   }
#endif

   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Position: The current playback position.

The current playback position of the audio sample is indicated by this field.  Writing to the field will alter the
playback position, either when the sample is next played, or immediately if it is currently playing.

*********************************************************************************************************************/

static ERROR SOUND_SET_Position(extSound *Self, LARGE Value)
{
   return Self->seek(Value, SEEK_START);
}

/*********************************************************************************************************************

-FIELD-
Priority: The priority of a sound in relation to other sound samples being played.

The playback priority of the sample is defined here. This helps to determine if the sample should be played when all
available mixing channels are busy. Naturally, higher priorities are played over samples with low priorities.

The minimum priority value allowed is -100, the maximum is 100.

*********************************************************************************************************************/

static ERROR SOUND_SET_Priority(extSound *Self, LONG Value)
{
   Self->Priority = Value;
   if (Self->Priority < -100) Self->Priority = -100;
   else if (Self->Priority > 100) Self->Priority = 100;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Stream: Defines the preferred streaming method for the sample.
Lookup: STREAM

-FIELD-
Volume: The volume to use when playing the sound sample.

The field specifies the volume of a sound in the range 0 - 1.0 (low to high).  Setting this field during sample
playback will dynamically alter the volume.
-END-

*********************************************************************************************************************/

static ERROR SOUND_SET_Volume(extSound *Self, DOUBLE Value)
{
   if (Value < 0) Value = 0;
   else if (Value > 1.0) Value = 1.0;
   Self->Volume = Value;

#ifdef USE_WIN32_PLAYBACK
   if (Self->initialised()) {
      parasol::ScopedObjectLock<extAudio> audio(Self->AudioID, 200);
      if (audio.granted()) {
         sndVolume((PlatformData *)Self->PlatformData, audio->MasterVolume * Self->Volume);
      }
   }
#else
   if (Self->ChannelIndex) {
      parasol::ScopedObjectLock<extAudio> audio(Self->AudioID, 200);
      if (audio.granted()) {
         sndMixVolume(*audio, Self->ChannelIndex, Self->Volume);
      }
      else return ERR_AccessObject;
   }
#endif

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR find_chunk(extSound *Self, objFile *File, CSTRING ChunkName)
{
   while (1) {
      char chunk[4];
      LONG len;
      if (File->read(chunk, sizeof(chunk), &len) or (len != sizeof(chunk))) {
         return ERR_Read;
      }

      if (!StrCompare(ChunkName, chunk, 4, STR_CASE)) return ERR_Okay;

      flReadLE(Self->File, &len); // Length of data in this chunk
      Self->File->seek(len, SEEK_CURRENT);
   }
}

//********************************************************************************************************************

#ifdef USE_WIN32_PLAYBACK
static ERROR win32_audio_stream(extSound *Self, LARGE Elapsed, LARGE CurrentTime)
{
   parasol::Log log(__FUNCTION__);

   // See sndStreamAudio() for further information on streaming in Win32

   auto response = sndStreamAudio((PlatformData *)Self->PlatformData);
   if (response IS -1) {
      log.warning("Sound streaming failed.");
      sound_stopped_event(Self);
      if (Self->PlaybackTimer) { UpdateTimer(Self->PlaybackTimer, 0); Self->PlaybackTimer = 0; }
      Self->StreamTimer = 0;
      return ERR_Terminate;
   }
   else if (response IS 1) {
      Self->StreamTimer = 0;
      return ERR_Terminate;
   }

   return ERR_Okay;
}
#endif

//********************************************************************************************************************

static const FieldDef clFlags[] = {
   { "Loop",         SDF_LOOP },
   { "New",          SDF_NEW },
   { "Stereo",       SDF_STEREO },
   { "RestrictPlay", SDF_RESTRICT_PLAY },
   { NULL, 0 }
};

static const FieldDef clStream[] = {
   { "Always", (LONG)STREAM::ALWAYS },
   { "Smart",  (LONG)STREAM::SMART },
   { "Never",  (LONG)STREAM::NEVER },
   { NULL, 0 }
};

static const FieldArray clFields[] = {
   { "Volume",         FDF_DOUBLE|FDF_RW,    0, NULL, (APTR)SOUND_SET_Volume },
   { "Pan",            FDF_DOUBLE|FDF_RW,    0, NULL, (APTR)SOUND_SET_Pan },
   { "Position",       FDF_LARGE|FDF_RW,     0, NULL, (APTR)SOUND_SET_Position },
   { "Priority",       FDF_LONG|FDF_RW,      0, NULL, (APTR)SOUND_SET_Priority },
   { "Length",         FDF_LONG|FDF_RW,      0, NULL, (APTR)SOUND_SET_Length },
   { "Octave",         FDF_LONG|FDF_RW,      0, NULL, (APTR)SOUND_SET_Octave },
   { "Flags",          FDF_LONGFLAGS|FDF_RW, (MAXINT)&clFlags, NULL, (APTR)SOUND_SET_Flags },
   { "Frequency",      FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "Playback",       FDF_LONG|FDF_RW,      0, NULL, (APTR)SOUND_SET_Playback },
   { "Compression",    FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "BytesPerSecond", FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "BitsPerSample",  FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "Audio",          FDF_OBJECTID|FDF_RI,  0, NULL, NULL },
   { "LoopStart",      FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "LoopEnd",        FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "Stream",         FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clStream, NULL, NULL },
   { "Handle",         FDF_LONG|FDF_SYSTEM|FDF_R, 0, NULL, NULL },
   { "ChannelIndex",   FDF_LONG|FDF_R,       0, NULL, NULL },
   // Virtual fields
   { "Active",   FDF_LONG|FDF_R,           0, (APTR)SOUND_GET_Active, NULL },
   { "Duration", FDF_DOUBLE|FDF_R,         0, (APTR)SOUND_GET_Duration, NULL },
   { "Header",   FDF_BYTE|FDF_ARRAY|FDF_R, 0, (APTR)SOUND_GET_Header, NULL },
   { "OnStop",   FDF_FUNCTIONPTR|FDF_RW,   0, (APTR)SOUND_GET_OnStop, (APTR)SOUND_SET_OnStop },
   { "Path",     FDF_STRING|FDF_RI,        0, (APTR)SOUND_GET_Path, (APTR)SOUND_SET_Path },
   { "Note",     FDF_STRING|FDF_RW,        0, (APTR)SOUND_GET_Note, (APTR)SOUND_SET_Note },
   END_FIELD
};

static const ActionArray clActions[] = {
   { AC_ActionNotify,  (APTR)SOUND_ActionNotify },
   { AC_Activate,      (APTR)SOUND_Activate },
   { AC_Deactivate,    (APTR)SOUND_Deactivate },
   { AC_Disable,       (APTR)SOUND_Disable },
   { AC_Enable,        (APTR)SOUND_Enable },
   { AC_Free,          (APTR)SOUND_Free },
   { AC_GetVar,        (APTR)SOUND_GetVar },
   { AC_Init,          (APTR)SOUND_Init },
   { AC_NewObject,     (APTR)SOUND_NewObject },
   { AC_Read,          (APTR)SOUND_Read },
   { AC_SaveToObject,  (APTR)SOUND_SaveToObject },
   { AC_Seek,          (APTR)SOUND_Seek },
   { AC_SetVar,        (APTR)SOUND_SetVar },
   { 0, NULL }
};

//********************************************************************************************************************

ERROR add_sound_class(void)
{
   clSound = objMetaClass::create::global(
      fl::BaseClassID(ID_SOUND),
      fl::ClassVersion(VER_SOUND),
      fl::FileExtension("*.wav|*.wave|*.snd"),
      fl::FileDescription("Sound Sample"),
      fl::FileHeader("[0:$52494646][8:$57415645]"),
      fl::Name("Sound"),
      fl::Category(CCF_AUDIO),
      fl::Actions(clActions),
      fl::Fields(clFields),
      fl::Size(sizeof(extSound)),
      fl::Path(MOD_PATH));

   return clSound ? ERR_Okay : ERR_AddClass;
}

void free_sound_class(void)
{
   if (clSound) { acFree(clSound); clSound = 0; }
}
