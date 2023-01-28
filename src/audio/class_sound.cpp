/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
Sound: Plays and records sound samples in a variety of different data formats.

The Sound class provides a simple API for programs to load and play audio sample files. By default all
loading and saving of sound data is in WAVE format.  Other audio formats can be supported through Sound class
extensions, if available.

Automatic streaming is enabled by default.  If an attempt is made to play an audio file that exceeds the maximum
buffer size, it will be streamed from the source location.  Streaming behaviour can be modified via the #Stream
field.

The following example illustrates playback of a sound sample that is one octave higher than its normal frequency.
The subscription to the Deactivate action will result in the program waking once the sample has finished
playback.

<pre>
local snd = obj.new('sound', { path='audio:samples/doorbell.wav', note='C6' })

snd.subscribe('deactivate', function(SoundID)
   proc.signal()
end)

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
static ERROR SOUND_GET_Position(extSound *, LONG *);

static ERROR SOUND_SET_Note(extSound *, CSTRING);

static const auto glScale = std::to_array({
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
});

static OBJECTPTR clSound = NULL;

static ERROR find_chunk(extSound *, objFile *, CSTRING);
static ERROR playback_timer(extSound *, LARGE, LARGE);

//********************************************************************************************************************

static LONG SF_Read(objSound *Self, APTR Buffer, LONG Length)
{
   if (Length > 0) {
      LONG result;
      if (!((extSound *)Self)->File->read(Buffer, Length, &result)) return result;
   }
   return 0;
}

static ERROR SF_Seek(objSound *Self, LONG Offset)
{
   ((extSound *)Self)->File->seekStart(((extSound *)Self)->DataOffset + Offset);
   return ERR_Okay;
}

static SoundFeed glWAVFeed = { SF_Read, SF_Seek };

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

   log.traceBranch("Position: %d", Self->Position);

   if (!Self->Length) return log.warning(ERR_FieldNotSet);

   if (!Self->Active) {
      if (!Self->BufferLength) {
         if ((Self->Stream IS STREAM::ALWAYS) and (Self->Length > 32768)) {
            Self->BufferLength = Self->BytesPerSecond * SECONDS_STREAM_BUFFER;
         }
         else if ((Self->Stream IS STREAM::SMART) and (Self->Length > 524288)) {
            Self->BufferLength = Self->BytesPerSecond * SECONDS_STREAM_BUFFER;
         }
         else Self->BufferLength = Self->Length;
      }

      if (Self->BufferLength > Self->Length) Self->BufferLength = Self->Length;
   }

#ifdef USE_WIN32_PLAYBACK
   // Optimised playback for Windows - this does not use our internal mixer.

   if (!Self->Active) {
      WORD channels = (Self->Flags & SDF_STEREO) ? 2 : 1;
      WAVEFORMATEX wave = {
         .Format            = WAVE_RAW,
         .Channels          = channels,
         .Frequency         = Self->Frequency,
         .AvgBytesPerSecond = Self->BytesPerSecond,
         .BlockAlign        = WORD(channels * (Self->BitsPerSample / 8)),
         .BitsPerSample     = WORD(Self->BitsPerSample),
         .ExtraLength       = 0
      };

      CSTRING strerr;
      if (Self->Length > Self->BufferLength) {
         log.msg("Streaming enabled because sample length %d exceeds buffer size %d.", Self->Length, Self->BufferLength);
         Self->Flags |= SDF_STREAM;
         strerr = sndCreateBuffer(Self, &wave, Self->BufferLength, Self->Length, (PlatformData *)Self->PlatformData, TRUE);
      }
      else {
         Self->BufferLength = Self->Length;
         Self->Flags &= ~SDF_STREAM;
         strerr = sndCreateBuffer(Self, &wave, Self->BufferLength, Self->Length, (PlatformData *)Self->PlatformData, FALSE);
      }

      if (strerr) {
         log.warning("Failed to create audio buffer, reason: %s (buffer length %d, sample length %d)", strerr, Self->BufferLength, Self->Length);
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

   // If streaming is enabled, the timer is used to regularly fill the audio buffer.  1/4 second checks are fine
   // since we are only going to fill the buffer every 1.5 seconds or more (give consideration to high octave
   // playback).
   //
   // We also need the subscription to fulfil the Deactivate contract.

   auto call = make_function_stdc(playback_timer);
   if (!SubscribeTimer(0.25, &call, &Self->Timer)) {
      LONG response;
      if (Self->Flags & SDF_LOOP) response = sndPlay((PlatformData *)Self->PlatformData, TRUE, Self->Position);
      else response = sndPlay((PlatformData *)Self->PlatformData, FALSE, Self->Position);
      return response ? log.warning(ERR_Failed) : ERR_Okay;
   }
   else return log.warning(ERR_Failed);
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

      BYTE *buffer;
      if (Self->Length > Self->BufferLength) {
         log.msg("Streaming enabled for playback in format $%.8x; Length: %d, Buffer: %d", sampleformat, Self->Length, Self->BufferLength);
         Self->Flags |= SDF_STREAM;

         struct sndAddStream stream;
         AudioLoop loop;
         if (Self->Flags & SDF_LOOP) {
            ClearMemory(&loop, sizeof(loop));
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

         stream.Path         = NULL;
         stream.ObjectID     = Self->UID;
         stream.SeekStart    = Self->DataOffset; // Applicable to WAV only
         stream.SampleFormat = sampleformat;
         stream.SampleLength = Self->Length;
         stream.BufferLength = Self->BufferLength;
         if (!ActionMsg(MT_SndAddStream, Self->AudioID, &stream)) {
            Self->Handle = stream.Result;
         }
         else {
            log.warning("Failed to add sample to the Audio device.");
            return ERR_Failed;
         }
      }
      else if (!AllocMemory(Self->Length, MEM_DATA|MEM_NO_CLEAR, &buffer)) {
         Self->BufferLength = Self->Length;

         LONG result;
         if (!Self->File->read(buffer, Self->Length, &result)) {
            struct sndAddSample add;
            AudioLoop loop;
            if (Self->Flags & SDF_LOOP) {
               ClearMemory(&loop, sizeof(loop));
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

            add.SampleFormat = sampleformat;
            add.Data         = buffer;
            add.DataSize     = Self->Length;
            add.Result       = 0;
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
         for (i=0; i < glMaxSoundChannels; i++) {
            channel = audio->GetChannel(Self->ChannelIndex);
            if ((channel) and (channel->SoundID IS Self->UID)) break;
            Self->ChannelIndex++;
         }
         if (i >= glMaxSoundChannels) channel = NULL;
      }

      if (!channel) {
         // Find an available channel.  If all channels are in use, check the priorities to see if we can push anyone out.
         AudioChannel *priority = NULL;
         Self->ChannelIndex &= 0xffff0000;
         LONG i;
         for (i=0; i < glMaxSoundChannels; i++) {
            if (auto channel = audio->GetChannel(Self->ChannelIndex)) {
               if ((channel->State IS CHS::STOPPED) or (channel->State IS CHS::FINISHED)) break;
               else if (channel->Priority < Self->Priority) priority = channel;
            }
            Self->ChannelIndex++;
         }

         if (i >= glMaxSoundChannels) {
            if (!(channel = priority)) {
               log.msg("Audio channel not available for playback.");
               return ERR_Failed;
            }
         }
      }

      sndMixStop(*audio, Self->ChannelIndex);

      if (!sndMixSample(*audio, Self->ChannelIndex, Self->Handle)) {
         auto channel = audio->GetChannel(Self->ChannelIndex);
         channel->SoundID = Self->UID; // Record our object ID against the channel

         if (sndMixVolume(*audio, Self->ChannelIndex, Self->Volume)) return log.warning(ERR_Failed);
         if (sndMixPan(*audio, Self->ChannelIndex, Self->Pan)) return log.warning(ERR_Failed);
         if (sndMixPlay(*audio, Self->ChannelIndex, Self->Playback)) return log.warning(ERR_Failed);

         // Use a timer to fulfil the Deactivate and auto-termination contracts.

         auto call = make_function_stdc(playback_timer);
         return SubscribeTimer(0.25, &call, &Self->Timer);
      }
      else {
         log.warning("Failed to set sample %d to channel $%.8x", Self->Handle, Self->ChannelIndex);
         return ERR_Failed;
      }
   }
   else return log.warning(ERR_AccessObject);
#endif
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

   if (Self->Timer) { UpdateTimer(Self->Timer, 0); Self->Timer = 0; }

   Self->Position = 0;

#ifdef USE_WIN32_PLAYBACK
   sndStop((PlatformData *)Self->PlatformData);
#else
   parasol::ScopedObjectLock<extAudio> audio(Self->AudioID);
   if (audio.granted()) {
      // Get a reference to our sound channel, then check if our unique ID is set against it.  If so, our sound is
      // currently playing and we must send a stop signal to the audio system.

      if (auto channel = audio->GetChannel(Self->ChannelIndex)) {
         if (channel->SoundID IS Self->UID) sndMixStop(*audio, Self->ChannelIndex);
      }
   }
   else return log.warning(ERR_AccessObject);
#endif

   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Disable: Disable playback of an active audio sample.
-END-
*********************************************************************************************************************/

static ERROR SOUND_Disable(extSound *Self, APTR Void)
{
   parasol::Log log;

   log.branch();

#ifdef USE_WIN32_PLAYBACK
   Self->Position = sndGetPosition((PlatformData *)Self->PlatformData);
   sndStop((PlatformData *)Self->PlatformData);
#else
   if (!Self->ChannelIndex) return ERR_Okay;

   parasol::ScopedObjectLock<extAudio> audio(Self->AudioID, 5000);
   if (audio.granted()) {
      if (auto channel = audio->GetChannel(Self->ChannelIndex)) {
         if (channel->SoundID IS Self->UID) sndMixStop(*audio, Self->ChannelIndex);
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
      log.msg("Playing back from position %d.", Self->Position);
      if (Self->Flags & SDF_LOOP) sndPlay((PlatformData *)Self->PlatformData, TRUE, Self->Position);
      else sndPlay((PlatformData *)Self->PlatformData, FALSE, Self->Position);
   }
#else
   if (!Self->ChannelIndex) return ERR_Okay;

   parasol::ScopedObjectLock<extAudio> audio(Self->AudioID, 5000);
   if (audio.granted()) {
      if (auto channel = audio->GetChannel(Self->ChannelIndex)) {
         if (channel->SoundID IS Self->UID) sndMixContinue(*audio, Self->ChannelIndex);
      }
   }
   else return log.warning(ERR_AccessObject);
#endif

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR SOUND_Free(extSound *Self, APTR Void)
{
   if (Self->Flags & SDF_STREAM) {
      if (Self->Timer) { UpdateTimer(Self->Timer, 0); Self->Timer = 0; }
   }

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
         if (!sndOpenChannels(*audio, glMaxSoundChannels, &Self->ChannelIndex)) {
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

   log.trace("Bits: %d, Freq: %d, KBPS: %d, BufLen: %d, ByteLength: %d, DataOffset: %d", Self->BitsPerSample, Self->Frequency, Self->BytesPerSecond, Self->BufferLength, Self->Length, Self->DataOffset);

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
         if (!sndOpenChannels(*audio, glMaxSoundChannels, &Self->ChannelIndex)) {
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
   Self->Feed        = &glWAVFeed;
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Reset: Stops audio playback, resets configuration details and restores the playback position to the start of the sample.
-END-
*********************************************************************************************************************/

static ERROR SOUND_Reset(extSound *Self, APTR Void)
{
   parasol::Log log;
   log.branch();

   if (!Self->ChannelIndex) return ERR_Okay;

   parasol::ScopedObjectLock<extAudio> audio(Self->AudioID, 2000);
   if (audio.granted()) {
      Self->Position = 0;
      auto channel = audio->GetChannel(Self->ChannelIndex);
      if ((channel->SoundID != Self->UID) or (channel->State IS CHS::STOPPED) or
          (channel->State IS CHS::FINISHED)) {
         return ERR_Okay;
      }

      sndMixStop(*audio, Self->ChannelIndex);

      if (!sndMixSample(*audio, Self->ChannelIndex, Self->Handle)) {
         channel->SoundID = Self->UID;

         sndMixVolume(*audio, Self->ChannelIndex, Self->Volume);
         sndMixPan(*audio, Self->ChannelIndex, Self->Pan);
         sndMixPlay(*audio, Self->ChannelIndex, Self->Playback);
         return ERR_Okay;
      }
      else return log.warning(ERR_Failed);
   }
   else return log.warning(ERR_AccessObject);
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
Seek: Moves sample playback to a new position.
-END-
*********************************************************************************************************************/

static ERROR SOUND_Seek(extSound *Self, struct acSeek *Args)
{
   // Switch off the audio playback if active

   LONG active;
   if ((!SOUND_GET_Active(Self, &active)) and (active)) {
      Self->deactivate();
   }
   else active = FALSE;

   // Set the new sample position

   if (Args->Position IS SEEK_START) {
      Self->Position = Args->Offset;
   }
   else if (Args->Position IS SEEK_END) {
      Self->Position = Self->Length - Args->Offset;
   }
   else if (Args->Position IS SEEK_CURRENT) {
      if (!SOUND_GET_Position(Self, &Self->Position)) {
         Self->Position += Args->Offset;
      }
   }
   else if (Args->Position IS SEEK_RELATIVE) {
      Self->Position = Self->Length * Args->Offset;
   }

   if (Self->Position < 0) Self->Position = 0;
   else if (Self->Position > Self->Length) Self->Position = Self->Length;

   // Restart the audio

   if (active) return Self->activate();
   else return ERR_Okay;
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
            if (!((channel->State IS CHS::STOPPED) or (channel->State IS CHS::FINISHED))) {
               *Value = TRUE;
            }
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
BufferLength: Defines the size of the buffer to use when streaming is enabled.

This field fine-tunes the size of the buffer that is used when streaming.  When manually choosing a buffer size, it is
usually best to keep the size between 64 and 128k.  Some systems may ignore the BufferLength field if the audio
driver is incompatible with manually defined buffer lengths.

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

Set the Note field to alter the playback frequency of a sound sample.  By setting this field as opposed
to the #Playback frequency, you can be assured that the sample is played as a correctly scaled note.

The Note field can be set using either string or integer based format.  If you are using the integer format, the number
that you choose reflects on the position on a musical keyboard.  A value of zero refers to the middle C key.  Each
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

static ERROR SOUND_GET_Position(extSound *Self, LONG *Value)
{
#ifdef USE_WIN32_PLAYBACK
   if (Self->initialised()) {
      Self->Position = sndGetPosition((PlatformData *)Self->PlatformData);
      *Value = Self->Position;
      return ERR_Okay;
   }
   else *Value = 0;
#else
   *Value = Self->Position;
#endif

   return ERR_Okay;
}

static ERROR SOUND_SET_Position(extSound *Self, LONG Value)
{
   if (!Self->seek(Value, SEEK_START)) return ERR_Okay;
   else {
      parasol::Log log;
      log.msg("Failed to seek to byte position %d.", Value);
      return ERR_Seek;
   }
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

static ERROR playback_timer(extSound *Self, LARGE Elapsed, LARGE CurrentTime)
{
   parasol::Log log;
   LONG active;

#ifdef USE_WIN32_PLAYBACK
   if (Self->Flags & SDF_STREAM) {
      // See sndStreamAudio() for further information on streaming in Win32

      if (auto response = sndStreamAudio((PlatformData *)Self->PlatformData)) {
         // We have reached the end of the sample.  If looping is turned off, terminate the timer subscription.

         if ((response IS -1) or (!(Self->Flags & SDF_LOOP))) {
            if (response IS -1) log.warning("Sound streaming failed.");
            else log.extmsg("Sound streaming completed.");
            DelayMsg(AC_Deactivate, Self->UID);
            Self->Timer = 0;
            return ERR_Terminate;
         }
      }
      return ERR_Okay;
   }
#endif

   // If the sound has stopped playing and the LOOP flag is not in use, either unsubscribe from the timer (because
   // we don't need timing) or terminate if AUTO_TERMINATE is in use.
   //
   // We used delayed messages here because we don't want to disturb the System Audio object.

   if (Self->Flags & SDF_LOOP) return ERR_Okay;

   if ((!Self->get(FID_Active, &active)) and (!active)) {
      log.extmsg("Sound playback completed.");
      DelayMsg(AC_Deactivate, Self->UID);
      Self->Timer = 0;
      return ERR_Terminate;
   }

   return ERR_Okay;
}

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
   { "Feed",           FDF_POINTER|FDF_SYSTEM|FDF_R, 0, NULL, NULL },
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
   { "BufferLength",   FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "Position",       FDF_LONG|FDF_RW,      0, (APTR)SOUND_GET_Position, (APTR)SOUND_SET_Position },
   { "Handle",         FDF_LONG|FDF_SYSTEM|FDF_R, 0, NULL, NULL },
   { "ChannelIndex",   FDF_LONG|FDF_R,       0, NULL, NULL },
   // Virtual fields
   { "Active",   FDF_LONG|FDF_R,     0, (APTR)SOUND_GET_Active, NULL },
   { "Header",   FDF_BYTE|FDF_ARRAY|FDF_R, 0, (APTR)SOUND_GET_Header, NULL },
   { "Path",     FDF_STRING|FDF_RI,  0, (APTR)SOUND_GET_Path, (APTR)SOUND_SET_Path },
   { "Note",     FDF_STRING|FDF_RW,  0, (APTR)SOUND_GET_Note, (APTR)SOUND_SET_Note },
   END_FIELD
};

static const ActionArray clActions[] = {
   { AC_Activate,      (APTR)SOUND_Activate },
   { AC_Deactivate,    (APTR)SOUND_Deactivate },
   { AC_Disable,       (APTR)SOUND_Disable },
   { AC_Enable,        (APTR)SOUND_Enable },
   { AC_Free,          (APTR)SOUND_Free },
   { AC_GetVar,        (APTR)SOUND_GetVar },
   { AC_Init,          (APTR)SOUND_Init },
   { AC_NewObject,     (APTR)SOUND_NewObject },
   { AC_Reset,         (APTR)SOUND_Reset },
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
