/*********************************************************************************************************************

-CLASS-
Audio: Supports a machine's audio hardware and provides a client-server audio management service.

The Audio class provides a common audio service that works across multiple platforms and follows a client-server
design model.

Supported features include 8/16/32 bit output in stereo or mono, oversampling, streaming, multiple audio channels
and command sequencing.  Audio functionality is simplified in the @Sound class interface, which we recommend when
straight-forward audio playback is sufficient.

Support for audio recording is not currently available.

-END-

*********************************************************************************************************************/

#ifndef ALSA_ENABLED
static ERROR init_audio(extAudio *Self)
{
   Self->BitDepth     = 16;
   Self->Stereo       = true;
   Self->MasterVolume = Self->Volumes[0].Channels[0];
   Self->Volumes[0].Flags |= VCF::MONO;
   for (LONG i=1; i < (LONG)Self->Volumes[0].Channels.size(); i++) Self->Volumes[0].Channels[i] = -1;
   if ((Self->Volumes[0].Flags & VCF::MUTE) != VCF::NIL) Self->Mute = true;
   else Self->Mute = false;

   return ERR_Okay;
}
#endif

/*********************************************************************************************************************
-ACTION-
Activate: Enables access to the audio hardware and initialises the mixer.

An audio object will not play or record until it has been activated.  Activating the object will result in an attempt
to access the device hardware, which on some platforms may lead to failure if another process has permanently locked
the audio device.  The resources and any device locks obtained by this action can be released with a call to
#Deactivate().

An inactive audio object can operate in a limited fashion but is without access to the audio hardware.

*********************************************************************************************************************/

static ERROR AUDIO_Activate(extAudio *Self, APTR Void)
{
   parasol::Log log;

   if (Self->Initialising) return ERR_Okay;

   log.branch();

   Self->Initialising = true;

   ERROR error;
   if ((error = init_audio(Self)) != ERR_Okay) {
      Self->Initialising = false;
      return error;
   }

   // Save the audio settings to disk post-initialisation

   acSaveSettings(Self);

   // Calculate one mixing element size for the hardware driver (not our floating point mixer).

   if (Self->BitDepth IS 16) Self->DriverBitSize = sizeof(WORD);
   else if (Self->BitDepth IS 24) Self->DriverBitSize = 3;
   else if (Self->BitDepth IS 32) Self->DriverBitSize = sizeof(FLOAT);
   else Self->DriverBitSize = sizeof(BYTE);

   if (Self->Stereo) Self->DriverBitSize *= 2;

   // Allocate a floating-point mixing buffer

   const LONG mixbitsize = Self->Stereo ? sizeof(FLOAT) * 2 : sizeof(FLOAT);

   Self->MixBufferSize = BYTELEN((F2T((mixbitsize * Self->OutputRate) * (MIX_INTERVAL * 1.5)) + 15) & (~15));
   Self->MixElements   = SAMPLE(Self->MixBufferSize / mixbitsize);

   if (!AllocMemory(Self->MixBufferSize, MEM_DATA, &Self->MixBuffer)) {
      // Pick the correct mixing routines

      if (Self->Flags & ADF_OVER_SAMPLING) {
         if (Self->Stereo) Self->MixRoutines = MixStereoFloatInterp;
         else Self->MixRoutines = MixMonoFloatInterp;
      }
      else if (Self->Stereo) Self->MixRoutines = MixStereoFloat;
      else Self->MixRoutines = MixMonoFloat;

      #ifdef _WIN32
         WAVEFORMATEX wave = {
            .Format            = WORD((Self->BitDepth IS 32) ? WAVE_FLOAT : WAVE_RAW),
            .Channels          = WORD(Self->Stereo ? 2 : 1),
            .Frequency         = 44100,
            .AvgBytesPerSecond = 44100 * Self->DriverBitSize,
            .BlockAlign        = Self->DriverBitSize,
            .BitsPerSample     = WORD(Self->BitDepth),
            .ExtraLength       = 0
         };

         if (auto strerr = sndCreateBuffer(Self, &wave, Self->MixBufferSize, 0x7fffffff, (PlatformData *)Self->PlatformData, TRUE)) {
            log.warning(strerr);
            Self->Initialising = false;
            return ERR_Failed;
         }

         if (sndPlay((PlatformData *)Self->PlatformData, TRUE, 0)) {
            Self->Initialising = false;
            return log.warning(ERR_Failed);
         }
      #endif

      // Note: The audio feed is managed by audio_timer() and is not started until an audio playback command
      // is executed by the client.

      Self->Initialising = false;
      return ERR_Okay;
   }
   else {
      Self->Initialising = false;
      return log.warning(ERR_AllocMemory);
   }
}

/*********************************************************************************************************************

-METHOD-
AddSample: Adds a new sample to an audio object for channel-based playback.

Audio samples can be loaded into an Audio object for playback via the AddSample() or #AddStream() methods.  For small
samples under 512k we recommend AddSample(), while anything larger should be supported through #AddStream().

When adding a sample, it is essential to select the correct bit format for the sample data.  While it is important to
differentiate between simple attributes such as 8 or 16 bit data, mono or stereo format, you should also be aware of
whether or not the data is little or big endian, and if the sample data consists of signed or unsigned values.  Because
of the possible variations there are a number of sample formats, as illustrated in the following table:

<types lookup="SFM"/>

By default, all samples are assumed to be in little endian format, as supported by Intel CPU's.  If the data is in big
endian format, logical-or the SampleFormat value with `SFM_BIG_ENDIAN`.

It is also possible to supply loop information with the sample data.  This is achieved by configuring the &AudioLoop
structure:

&AudioLoop

The types that can be specified in the LoopMode field are:

&LOOP

The Loop1Type and Loop2Type fields alter the style of the loop.  These can be set to the following:

&LTYPE

-INPUT-
func OnStop: This optional callback function will be called when the stream stops playing.
int(SFM) SampleFormat: Indicates the format of the sample data that you are adding.
buf(ptr) Data: Points to the address of the sample data.
bufsize DataSize: Size of the sample data, in bytes.
struct(*AudioLoop) Loop: Optional sample loop information.
structsize LoopSize: Must be set to sizeof(AudioLoop) if Loop is defined.
&int Result: The resulting sample handle will be returned in this parameter.

-ERRORS-
Okay
Args
NullArgs
AllocMemory: Failed to allocate enough memory to hold the sample data.
-END-

*********************************************************************************************************************/

ERROR AUDIO_AddSample(extAudio *Self, struct sndAddSample *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   log.branch("Data: %p, Length: %d", Args->Data, Args->DataSize);

   // Find an unused sample block.  If there is none, increase the size of the sample management area.

   LONG idx;
   for (idx=1; idx < (LONG)Self->Samples.size(); idx++) {
      if (!Self->Samples[idx].Data) break;
   }

   if (idx >= (LONG)Self->Samples.size()) Self->Samples.resize(Self->Samples.size()+10);

   LONG shift = sample_shift(Args->SampleFormat);

   auto &sample = Self->Samples[idx];
   sample.SampleType   = Args->SampleFormat;
   sample.SampleLength = SAMPLE(Args->DataSize >> shift);
   sample.OnStop       = Args->OnStop;

   if (auto loop = Args->Loop) {
      sample.LoopMode     = loop->LoopMode;
      sample.Loop1Start   = SAMPLE(loop->Loop1Start >> shift);
      sample.Loop1End     = SAMPLE(loop->Loop1End >> shift);
      sample.Loop1Type    = loop->Loop1Type;
      sample.Loop2Start   = SAMPLE(loop->Loop2Start >> shift);
      sample.Loop2End     = SAMPLE(loop->Loop2End >> shift);
      sample.Loop2Type    = loop->Loop2Type;
      // Eliminate zero-byte loops

      if (sample.Loop1Start IS sample.Loop1End) sample.Loop1Type = LTYPE::NIL;
      if (sample.Loop2Start IS sample.Loop2End) sample.Loop2Type = LTYPE::NIL;
   }
   else {
      sample.Loop1Type = LTYPE::NIL;
      sample.Loop2Type = LTYPE::NIL;
   }

   if ((!sample.SampleType) or (Args->DataSize <= 0) or (!Args->Data)) {
      sample.Data = NULL;
   }
   else if (!AllocMemory(Args->DataSize, MEM_DATA|MEM_NO_CLEAR, &sample.Data)) {
      CopyMemory(Args->Data, sample.Data, Args->DataSize);
   }
   else return log.warning(ERR_AllocMemory);

   Args->Result = idx;
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
AddStream: Adds a new sample-stream to an Audio object for channel-based playback.

Use AddStream to load large sound samples to an Audio object, allowing it to play those samples on the client
machine without over-provisioning available resources.  For small samples under 256k consider using AddSample instead.

The data source used for a stream will need to be provided by a client provided Callback function.  The synopsis is
`LONG callback(LONG SampleHandle, LONG Offset, UBYTE *Buffer, LONG BufferSize)`.

The Offset reflects the retrieval point of the decoded data and is measured in bytes.  The Buffer and BufferSize reflect
the target for the decoded data.  The function must return the total number of bytes that were written to the Buffer.
If an error occurs, return zero.

When creating a new stream, pay attention to the audio format that is being used for the sample data.
It is important to differentiate between 8-bit, 16-bit, mono and stereo, but also be aware of whether or not the data
is little or big endian, and if the sample data consists of signed or unsigned values.  Because of the possible
variations there are a number of sample formats, as illustrated in the following table:

<types lookup="SFM"/>

By default, all samples are assumed to be in little endian format, as supported by Intel CPU's.  If the data is in big
endian format, logical-or the SampleFormat value with the flag `SFM_BIG_ENDIAN`.

It is also possible to supply loop information with the stream.  The Audio class supports a number of different looping
formats via the &AudioLoop structure:

&AudioLoop

There are three types of loop modes that can be specified in the LoopMode field:

&LOOP

The Loop1Type and Loop2Type fields normally determine the style of the loop, however only unidirectional looping is
currently supported for streams.  For that reason, set the type variables to either `LTYPE::NIL` or
`LTYPE::UNIDIRECTIONAL`.

-INPUT-
func Callback: This callback function must be able to return raw audio data for streaming.
func OnStop: This optional callback function will be called when the stream stops playing.
int(SFM) SampleFormat: Indicates the format of the sample data that you are adding.
int SampleLength: Total byte-length of the sample data that is being streamed.  May be set to zero if the length is infinite or unknown.
int PlayOffset: Offset the playing position by this byte index.
struct(*AudioLoop) Loop: Refers to sample loop information, or NULL if no loop is required.
structsize LoopSize: Must be set to sizeof(AudioLoop).
&int Result: The resulting sample handle will be returned in this parameter.

-ERRORS-
Okay
Args
NullArgs
AllocMemory: Failed to allocate the stream buffer.
-END-

*********************************************************************************************************************/

static const LONG MAX_STREAM_BUFFER = 16 * 1024; // Max stream buffer length in bytes

static ERROR AUDIO_AddStream(extAudio *Self, struct sndAddStream *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->SampleFormat)) return log.warning(ERR_NullArgs);
   if (!Args->Callback.Type) return log.warning(ERR_NullArgs);

   log.branch("Length: %d", Args->SampleLength);

   // Find an unused sample block.  If there is none, increase the size of the sample management area.

   LONG idx;
   for (idx=1; idx < (LONG)Self->Samples.size(); idx++) {
      if (!Self->Samples[idx].Data) break;
   }

   if (idx >= (LONG)Self->Samples.size()) Self->Samples.resize(Self->Samples.size()+10);

   LONG shift = sample_shift(Args->SampleFormat);

   LONG buffer_len;
   if (Args->SampleLength > 0) {
      buffer_len = Args->SampleLength / 2;
      if (buffer_len > MAX_STREAM_BUFFER) buffer_len = MAX_STREAM_BUFFER;
   }
   else buffer_len = MAX_STREAM_BUFFER; // Use the recommended amount of buffer space

   #ifdef ALSA_ENABLED
      if (buffer_len < Self->AudioBufferSize) log.warning("Warning: Buffer length of %d is less than audio buffer size of %d.", buffer_len, Self->AudioBufferSize);
   #endif

   // Setup the audio sample

   auto &sample = Self->Samples[idx];
   sample.SampleType   = Args->SampleFormat;
   sample.SampleLength = SAMPLE(buffer_len>>shift);
   sample.StreamLength = BYTELEN((Args->SampleLength > 0) ? Args->SampleLength : 0x7fffffff); // 'Infinite' stream length
   sample.Callback     = Args->Callback;
   sample.OnStop       = Args->OnStop;
   sample.Stream       = true;
   sample.PlayPos      = BYTELEN(Args->PlayOffset);

   sample.LoopMode     = LOOP::SINGLE;
   sample.Loop1Type    = LTYPE::UNIDIRECTIONAL;
   sample.Loop1Start   = SAMPLE(0);
   sample.Loop1End     = SAMPLE(buffer_len>>shift);

   if (Args->Loop) {
      sample.Loop2Type    = LTYPE::UNIDIRECTIONAL;
      sample.Loop2Start   = SAMPLE(Args->Loop->Loop1Start);
      sample.Loop2End     = SAMPLE(Args->Loop->Loop1End);
      sample.StreamLength = BYTELEN(sample.Loop2End<<shift);

      if (sample.Loop2Start IS sample.Loop2End) sample.Loop2Type = LTYPE::NIL;
   }

   if (AllocMemory(buffer_len, MEM_DATA|MEM_NO_CLEAR, &sample.Data) != ERR_Okay) {
      return ERR_AllocMemory;
   }

   Args->Result = idx;
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
Beep: Beeps the PC audio speaker.

This method will beep the PC audio speaker, if available.  It is possible to request the specific Pitch, Duration
and Volume for the sound although not all platforms may support the parameters.  In some cases the beep may be
converted to a standard warning sound by the host.

-INPUT-
int Pitch: The pitch of the beep in HZ.
int Duration: The duration of the beep in milliseconds.
int Volume: The volume of the beep, from 0 to 100.

-ERRORS-
Okay
NullArgs
NoSupport: PC speaker support is not available.

*********************************************************************************************************************/

static ERROR AUDIO_Beep(extAudio *Self, struct sndBeep *Args)
{
   if (!Args) return ERR_NullArgs;

#ifdef __linux__
   LONG console;
   if ((console = GetResource(RES_CONSOLE_FD)) != -1) {
      ioctl(console, KDMKTONE, ((1193190 / Args->Pitch) & 0xffff) | ((ULONG)Args->Duration << 16));
      return ERR_Okay;
   }
#endif
   return ERR_NoSupport;
}

/*********************************************************************************************************************

-METHOD-
CloseChannels: Frees audio channels that have been allocated for sample playback.

Use CloseChannels to destroy a group of channels that have previously been allocated through the #OpenChannels()
method.  Any audio commands buffered against the channels will be cleared instantly.  Any audio data that has already
been mixed into the output buffer can continue to play for 1 - 2 seconds.  If this is an issue then the volume should
be muted at the same time.

-INPUT-
int Handle: Must refer to a channel handle returned from the #OpenChannels() method.

-ERRORS-
Okay
NullArgs
Args

*********************************************************************************************************************/

static ERROR AUDIO_CloseChannels(extAudio *Self, struct sndCloseChannels *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   log.branch("Handle: $%.8x", Args->Handle);

   LONG index = Args->Handle>>16;
   if ((index < 0) or (index >= (LONG)Self->Sets.size())) log.warning(ERR_Args);

   Self->Sets[index].clear(); // We can't erase because that would mess up other channel handles.
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Deactivate: Disables the audio mixer and returns device resources to the system.

Deactivating an audio object will switch off the mixer, clear the output buffer and return any allocated device
resources back to the host system.  The audio object will remain in a suspended state until it is reactivated.
-END-
*********************************************************************************************************************/

static ERROR AUDIO_Deactivate(extAudio *Self, APTR Void)
{
   parasol::Log log;

   log.branch();

   if (Self->Initialising) {
      log.msg("Audio is still in the process of initialisation.");
      return ERR_Okay;
   }

   acClear(Self);

#ifdef ALSA_ENABLED
   free_alsa(Self);
   //if (Self->MixHandle) { snd_mixer_close(Self->MixHandle); Self->MixHandle = NULL; }
   //if (Self->Handle) { snd_pcm_close(Self->Handle); Self->Handle = 0; }
#endif

   return ERR_Okay;
}

//********************************************************************************************************************
// Reload the user's audio configuration details.

static void user_login(APTR Reference, APTR Info, LONG InfoSize)
{
   parasol::Log log("Audio");
   extAudio *Self;

   if (!AccessObject((OBJECTID)(MAXINT)Reference, 3000, &Self)) {
      if (!Self->Initialising) {
         log.branch("User login detected - reloading audio configuration.");
         acDeactivate(Self);
         load_config(Self);
         acActivate(Self);
      }
      ReleaseObject(Self);
   }
}

//********************************************************************************************************************

static ERROR AUDIO_Free(extAudio *Self, APTR Void)
{
   if (Self->Flags & ADF_AUTO_SAVE) Self->saveSettings();

   if (Self->Timer) { UpdateTimer(Self->Timer, 0); Self->Timer = NULL; }

   if (Self->TaskRemovedHandle) { UnsubscribeEvent(Self->TaskRemovedHandle); Self->TaskRemovedHandle = NULL; }
   if (Self->UserLoginHandle)   { UnsubscribeEvent(Self->UserLoginHandle);   Self->UserLoginHandle = NULL; }

   acDeactivate(Self);

   if (Self->MixBuffer) { FreeResource(Self->MixBuffer); Self->MixBuffer = NULL; }

#ifdef ALSA_ENABLED

   free_alsa(Self);

#elif _WIN32

   dsCloseDevice();

#endif

   Self->~extAudio();

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR AUDIO_Init(extAudio *Self, APTR Void)
{
   parasol::Log log;

#ifdef _WIN32
   Self->OutputRate = 44100; // Mix rate is forced for direct sound
#endif

   log.msg("Subscribing to events.");

   auto call = make_function_stdc(user_login);
   SubscribeEvent(EVID_USER_STATUS_LOGIN, &call, (APTR)(MAXINT)Self->UID, (APTR)&Self->UserLoginHandle);

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR AUDIO_NewObject(extAudio *Self, APTR Void)
{
   parasol::Log log;

   new (Self) extAudio;

   Self->OutputRate  = 44100;        // Rate for output to speakers
   Self->InputRate   = 44100;        // Input rate for recording
   Self->Quality     = 80;
   Self->BitDepth    = 16;
   Self->Flags       = ADF_OVER_SAMPLING|ADF_FILTER_HIGH|ADF_VOL_RAMPING|ADF_STEREO;
   Self->Periods     = 4;
   Self->PeriodSize  = 2048;
   Self->Device      = "default";
   Self->MaxChannels = 8;

   const SystemState *state = GetSystemState();
   if ((!StrMatch(state->Platform, "Native")) or (!StrMatch(state->Platform, "Linux"))) {
      Self->Flags |= ADF_SYSTEM_WIDE;
   }

   Self->Samples.reserve(32);

#ifdef __linux__
   Self->Volumes.resize(2);
   Self->Volumes[0].Name = "Master";
   for (LONG i=0; i < (LONG)Self->Volumes[0].Channels.size(); i++) Self->Volumes[0].Channels[i] = 1.0;

   Self->Volumes[1].Name = "PCM";
   for (LONG i=0; i < (LONG)Self->Volumes[1].Channels.size(); i++) Self->Volumes[1].Channels[i] = 1.0;
#else
   Self->Volumes.resize(1);
   Self->Volumes[0].Name = "Master";
   Self->Volumes[0].Channels[0] = 1.0;
   for (LONG i=1; i < (LONG)Self->Volumes[0].Channels.size(); i++) Self->Volumes[0].Channels[i] = -1;
#endif

   load_config(Self);

   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
OpenChannels: Allocates audio channels that can be used for sample playback.

Use the OpenChannels method to open audio channels for sample playback.  Channels are allocated in sets with a size
range between 1 and 64.  Channel sets make it easier to segregate playback between users of the same audio object.

The resulting handle returned from this method is an integer consisting of two parts.  The upper word uniquely
identifies the channel set that has been provided to you, while the lower word is used to refer to specific channel
numbers.  If referring to a specific channel is required for a function, use the formula `Channel = Handle | ChannelNo`.

To destroy allocated channels, use the #CloseChannels() method.

-INPUT-
int Total: Total of channels to allocate.
&int Result: The resulting channel handle is returned in this parameter.

-ERRORS-
Okay
NullArgs
OutOfRange: The amount of requested channels or commands is outside of acceptable range.
AllocMemory: Memory for the audio channels could not be allocated.
-END-

*********************************************************************************************************************/

static ERROR AUDIO_OpenChannels(extAudio *Self, struct sndOpenChannels *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   log.branch("Total: %d", Args->Total);

   Args->Result = 0;
   if ((Args->Total < 0) or (Args->Total > 64)) {
      return log.warning(ERR_OutOfRange);
   }

   // Bear in mind that the +1 is for channel set 0 being a dummy entry.

   LONG index = Self->Sets.size() + 1;
   Self->Sets.resize(index+1);

   Self->Sets[index].Channel.resize(Args->Total);

   if (Self->Flags & ADF_OVER_SAMPLING) Self->Sets[index].Shadow.resize(Args->Total);
   else Self->Sets[index].Shadow.clear();

   Self->Sets[index].UpdateRate = 125;  // Default mixer update rate of 125ms
   Self->Sets[index].MixLeft    = Self->MixLeft(Self->Sets[index].UpdateRate);
   Self->Sets[index].Commands.reserve(32);

   Args->Result = index<<16;
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
RemoveSample: Removes a sample from the global sample list and deallocates its resources.

Remove an allocated sample at any time by calling the RemoveSample method.  Once a sample is removed it is
permanently deleted from the audio server and it is not possible to reallocate the sample against the same handle
number.

Sample handles can be reused by the API after being removed.  Clearing any old references to sample handles after use
is therefore recommended.

-INPUT-
int Handle: The handle of the sample that requires removal.

-ERRORS-
Okay
NullArgs
OutOfRange: The provided sample handle is not within the valid range.
-END-

*********************************************************************************************************************/

static ERROR AUDIO_RemoveSample(extAudio *Self, struct sndRemoveSample *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   log.branch("Sample: %d", Args->Handle);

   if ((Args->Handle < 1) or (Args->Handle >= (LONG)Self->Samples.size())) return log.warning(ERR_OutOfRange);

   Self->Samples[Args->Handle].clear();

   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
SaveSettings: Saves the current audio settings.
-END-
*********************************************************************************************************************/

static ERROR AUDIO_SaveSettings(extAudio *Self, APTR Void)
{
   objFile::create file = { fl::Path("user:config/audio.cfg"), fl::Flags(FL_NEW|FL_WRITE) };

   if (file.ok()) {
      return acSaveToObject(Self, file->UID, 0);
   }
   else return ERR_CreateFile;
}

/*********************************************************************************************************************
-ACTION-
SaveToObject: Saves the current audio settings to another object.
-END-
*********************************************************************************************************************/

static ERROR AUDIO_SaveToObject(extAudio *Self, struct acSaveToObject *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->DestID)) return log.warning(ERR_NullArgs);

   objConfig::create config = { };
   if (config.ok()) {
      config->write("AUDIO", "OutputRate", Self->OutputRate);
      config->write("AUDIO", "InputRate", Self->InputRate);
      config->write("AUDIO", "Quality", Self->Quality);
      config->write("AUDIO", "BitDepth", Self->BitDepth);
      config->write("AUDIO", "Periods", Self->Periods);
      config->write("AUDIO", "PeriodSize", Self->PeriodSize);

      if (Self->Flags & ADF_STEREO) config->write("AUDIO", "Stereo", "TRUE");
      else config->write("AUDIO", "Stereo", "FALSE");

#ifdef __linux__
      if (!Self->Device.empty()) config->write("AUDIO", "Device", Self->Device);
      else config->write("AUDIO", "Device", "default");

      if ((!Self->Volumes.empty()) and (Self->Flags & ADF_SYSTEM_WIDE)) {
         for (LONG i=0; i < (LONG)Self->Volumes.size(); i++) {
            std::ostringstream out;
            if ((Self->Volumes[i].Flags & VCF::MUTE) != VCF::NIL) out << "1,[";
            else out << "0,[";

            if ((Self->Volumes[i].Flags & VCF::MONO) != VCF::NIL) {
               out << Self->Volumes[i].Channels[0];
            }
            else for (LONG c=0; c < (LONG)Self->Volumes[i].Channels.size(); c++) {
               if (c > 0) out << ',';
               out << Self->Volumes[i].Channels[c];
            }
            out << ']';

            config->write("MIXER", Self->Volumes[i].Name.c_str(), out.str());
         }
      }
#if 0
   // Commented out because it prevents savetoobject being used by other tasks.

   snd_mixer_selem_id_t *sid;
   snd_mixer_elem_t *elem;
   LONG left, right;
   LONG pmin, pmax;
   LONG mute;
   LONG i;

   snd_mixer_selem_id_alloca(&sid);
   snd_mixer_selem_id_set_index(sid, 0);
   snd_mixer_selem_id_set_name(sid, Self->Volumes[i].Name);

   if ((elem = snd_mixer_find_selem(Self->MixHandle, sid))) {
      if (snd_mixer_selem_has_playback_volume(elem)) {
         snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);
         snd_mixer_selem_get_playback_volume(elem, 0, &left);
         snd_mixer_selem_get_playback_switch(elem, 0, &mute);
         if ((Self->Volumes[i].Flags & VCF::MONO) != VCF::NIL) right = left;
         else snd_mixer_selem_get_playback_volume(elem, 1, &right);
      }
      else if (snd_mixer_selem_has_capture_volume(elem)) {
         snd_mixer_selem_get_capture_volume_range(elem, &pmin, &pmax);
         snd_mixer_selem_get_capture_volume(elem, 0, &left);
         snd_mixer_selem_get_capture_switch(elem, 0, &mute);
         if ((Self->Volumes[i].Flags & VCF::MONO) != VCF::NIL) right = left;
         else snd_mixer_selem_get_capture_volume(elem, 1, &right);
      }
      else continue;

      if (pmin >= pmax) continue;

      std::ostringstream out;
      auto fleft = (DOUBLE)left / (DOUBLE)(pmax - pmin);
      auto fright = (DOUBLE)right / (DOUBLE)(pmax - pmin);
      out << fleft << ',' << fright << ',' << mute ? 0 : 1;

      config->write("MIXER", Self->Volumes[i].Name, out.str());
   }
#endif

#else
      if (!Self->Volumes.empty()) {
         std::string out(((Self->Volumes[0].Flags & VCF::MUTE) != VCF::NIL) ? "1,[" : "0,[");
         out.append(std::to_string(Self->Volumes[0].Channels[0]));
         out.append("]");
         config->write("MIXER", Self->Volumes[0].Name.c_str(), out);
      }
#endif

      config->saveToObject(Args->DestID, 0);
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
SetSampleLength: Sets the byte length of a streaming sample.

This function will update the byte length of a steaming sample.  Although it is possible to manually stop a stream at
any point, setting the length is a preferable means to stop playback as it ensures complete accuracy when a sample's
output is buffered.

Setting a Length of -1 indicates that the stream should be played indefinitely.

-INPUT-
int Sample: A sample handle from AddStream().
large Length: Byte length of the sample stream.

-ERRORS-
Okay
NullArgs
Args
Failed: Sample is not a stream.
-END-

*********************************************************************************************************************/

static ERROR AUDIO_SetSampleLength(extAudio *Self, struct sndSetSampleLength *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   log.msg("Sample: #%d, Length: %" PF64, Args->Sample, Args->Length);

   if ((Args->Sample < 0) or (Args->Sample >= (LONG)Self->Samples.size())) return log.warning(ERR_Args);

   auto &sample = Self->Samples[Args->Sample];

   if (sample.Stream) {
      sample.StreamLength = BYTELEN(Args->Length);
      return ERR_Okay;
   }
   else return log.warning(ERR_Failed);
}

/*********************************************************************************************************************

-METHOD-
SetVolume: Sets the volume for input and output mixers.

To change volume and mixer levels, use the SetVolume method.  It is possible to make adjustments to any of the
available mixers and for different channels per mixer - for instance you may set different volumes for left and right
speakers.  Support is also provided for special options such as muting.

To set the volume for a mixer, use its index or set its name (to change the Master volume, use a name of `Master`).

A target Channel such as the left (0) or right (1) speaker can be specified.  Set the Channel to -1 if all channels
should be the same value.

The new mixer value is set in the Volume field.

Optional flags may be set as follows:

<types lookup="SVF"/>

-INPUT-
int Index: The index of the mixer that you want to set.
cstr Name: If the correct index number is unknown, the name of the mixer may be set here.
int(SVF) Flags: Optional flags.
int Channel: A specific channel to modify (e.g. 0 for left, 1 for right).  If -1, all channels are affected.
double Volume: The volume to set for the mixer, from 0 - 1.0.  If -1, the current volume values are retained.

-ERRORS-
Okay: The new volume was applied successfully.
Args
NullArgs
OutOfRange: The Volume or Index is out of the acceptable range.
-END-

*********************************************************************************************************************/

static ERROR AUDIO_SetVolume(extAudio *Self, struct sndSetVolume *Args)
{
   parasol::Log log;

#ifdef ALSA_ENABLED

   LONG index;
   snd_mixer_selem_id_t *sid;
   snd_mixer_elem_t *elem;
   long pmin, pmax;

   if (!Args) return log.warning(ERR_NullArgs);
   if (((Args->Volume < 0) or (Args->Volume > 1.0)) and (Args->Volume != -1)) {
      return log.warning(ERR_OutOfRange);
   }
   if (Self->Volumes.empty()) return log.warning(ERR_NoSupport);
   if (!Self->MixHandle) return ERR_NotInitialised;

   // Determine what mixer we are going to adjust

   if (Args->Name) {
      for (index=0; index < (LONG)Self->Volumes.size(); index++) {
         if (!StrMatch(Args->Name, Self->Volumes[index].Name.c_str())) break;
      }

      if (index IS (LONG)Self->Volumes.size()) return ERR_Search;
   }
   else {
      index = Args->Index;
      if ((index < 0) or (index >= (LONG)Self->Volumes.size())) return ERR_OutOfRange;
   }

   if (!StrMatch("Master", Self->Volumes[index].Name.c_str())) {
      if (Args->Volume != -1) {
         Self->MasterVolume = Args->Volume;
      }

      if (Args->Flags & SVF::UNMUTE) {
         Self->Volumes[index].Flags &= ~VCF::MUTE;
         Self->Mute = false;
      }
      else if (Args->Flags & SVF::MUTE) {
         Self->Volumes[index].Flags |= VCF::MUTE;
         Self->Mute = true;
      }
   }

   // Apply the volume

   log.branch("%s: %.2f, Flags: $%.8x", Self->Volumes[index].Name.c_str(), Args->Volume, Args->Flags);

   snd_mixer_selem_id_alloca(&sid);
   snd_mixer_selem_id_set_index(sid,0);
   snd_mixer_selem_id_set_name(sid, Self->Volumes[index].Name.c_str());
   if (!(elem = snd_mixer_find_selem(Self->MixHandle, sid))) {
      log.msg("Mixer \"%s\" not found.", Self->Volumes[index].Name.c_str());
      return ERR_Search;
   }

   if (Args->Volume >= 0) {
      if ((Self->Volumes[index].Flags & VCF::CAPTURE) != VCF::NIL) {
         snd_mixer_selem_get_capture_volume_range(elem, &pmin, &pmax);
      }
      else snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);

      pmax = pmax - 1; // -1 because the absolute maximum tends to produce distortion...

      DOUBLE vol = Args->Volume;
      if (vol > 1.0) vol = 1.0;
      LONG lvol = F2T(DOUBLE(pmin) + (DOUBLE(pmax - pmin) * vol));

      if ((Self->Volumes[index].Flags & VCF::CAPTURE) != VCF::NIL) {
         snd_mixer_selem_set_capture_volume_all(elem, lvol);
      }
      else snd_mixer_selem_set_playback_volume_all(elem, lvol);

      if ((Self->Volumes[index].Flags & VCF::MONO) != VCF::NIL) {
         Self->Volumes[index].Channels[0] = vol;
      }
      else {
         if (Args->Channel IS -1) {
            for (LONG channel=0; channel < (LONG)Self->Volumes[index].Channels.size(); channel++) {
               if (Self->Volumes[index].Channels[channel] >= 0) {
                  Self->Volumes[index].Channels[channel] = vol;
               }
            }
         }
         else if ((Args->Channel >= 0) and (Args->Channel < LONG(Self->Volumes[index].Channels.size()))) {
            Self->Volumes[index].Channels[Args->Channel] = vol;
         }
      }
   }

   if ((Args->Flags & SVF::UNMUTE) != SVF::NIL) {
      if ((snd_mixer_selem_has_capture_switch(elem)) and (!snd_mixer_selem_has_playback_switch(elem))) {
         for (LONG chn=0; chn <= SND_MIXER_SCHN_LAST; chn++) {
            snd_mixer_selem_set_capture_switch(elem, (snd_mixer_selem_channel_id_t)chn, 1);
         }
      }
      else if (snd_mixer_selem_has_playback_switch(elem)) {
         for (LONG chn=0; chn <= SND_MIXER_SCHN_LAST; chn++) {
            snd_mixer_selem_set_playback_switch(elem, (snd_mixer_selem_channel_id_t)chn, 1);
         }
      }
      Self->Volumes[index].Flags &= ~VCF::MUTE;
   }
   else if ((Args->Flags & SVF::MUTE) != SVF::NIL) {
      if ((snd_mixer_selem_has_capture_switch(elem)) and (!snd_mixer_selem_has_playback_switch(elem))) {
         for (LONG chn=0; chn <= SND_MIXER_SCHN_LAST; chn++) {
            snd_mixer_selem_set_capture_switch(elem, (snd_mixer_selem_channel_id_t)chn, 0);
         }
      }
      else if (snd_mixer_selem_has_playback_switch(elem)) {
         for (LONG chn=0; chn <= SND_MIXER_SCHN_LAST; chn++) {
            snd_mixer_selem_set_playback_switch(elem, (snd_mixer_selem_channel_id_t)chn, 0);
         }
      }
      Self->Volumes[index].Flags |= VCF::MUTE;
   }

   EVENTID evid = GetEventID(EVG_AUDIO, "volume", Self->Volumes[index].Name.c_str());
   evVolume event_volume = { evid, Args->Volume, ((Self->Volumes[index].Flags & VCF::MUTE) != VCF::NIL) ? true : false };
   BroadcastEvent(&event_volume, sizeof(event_volume));
   return ERR_Okay;

#else

   LONG index;

   if (!Args) return log.warning(ERR_NullArgs);
   if (((Args->Volume < 0) or (Args->Volume > 1.0)) and (Args->Volume != -1)) {
      return log.warning(ERR_OutOfRange);
   }
   if (Self->Volumes.empty()) return log.warning(ERR_NoSupport);

   // Determine what mixer we are going to adjust

   if (Args->Name) {
      for (index=0; index < (LONG)Self->Volumes.size(); index++) {
         if (!StrMatch(Args->Name, Self->Volumes[index].Name.c_str())) break;
      }

      if (index IS (LONG)Self->Volumes.size()) return ERR_Search;
   }
   else {
      index = Args->Index;
      if ((index < 0) or (index >= (LONG)Self->Volumes.size())) return ERR_OutOfRange;
   }

   if (!StrMatch("Master", Self->Volumes[index].Name.c_str())) {
      if (Args->Volume != -1) {
         Self->MasterVolume = Args->Volume;
      }

      if ((Args->Flags & SVF::UNMUTE) != SVF::NIL) {
         Self->Volumes[index].Flags &= ~VCF::MUTE;
         Self->Mute = false;
      }
      else if ((Args->Flags & SVF::MUTE) != SVF::NIL) {
         Self->Volumes[index].Flags |= VCF::MUTE;
         Self->Mute = true;
      }
   }

   // Apply the volume

   log.branch("%s: %.2f, Flags: $%.8x", Self->Volumes[index].Name.c_str(), Args->Volume, (LONG)Args->Flags);

   if ((Args->Volume >= 0) and (Args->Volume <= 1.0)) {
      if ((Self->Volumes[index].Flags & VCF::MONO) != VCF::NIL) {
         Self->Volumes[index].Channels[0] = Args->Volume;
      }
      else {
         if (Args->Channel IS -1) {
            for (LONG channel=0; channel < (LONG)Self->Volumes[0].Channels.size(); channel++) {
               if (Self->Volumes[index].Channels[channel] >= 0) {
                  Self->Volumes[index].Channels[channel] = Args->Volume;
               }
            }
         }
         else if ((Args->Channel >= 0) and (Args->Channel < LONG(Self->Volumes[index].Channels.size()))) {
            Self->Volumes[index].Channels[Args->Channel] = Args->Volume;
         }
      }
   }

   if ((Args->Flags & SVF::UNMUTE) != SVF::NIL) Self->Volumes[index].Flags &= ~VCF::MUTE;
   else if ((Args->Flags & SVF::MUTE) != SVF::NIL) Self->Volumes[index].Flags |= VCF::MUTE;

   EVENTID evid = GetEventID(EVG_AUDIO, "volume", Self->Volumes[index].Name.c_str());
   evVolume event_volume = { evid, Args->Volume, ((Self->Volumes[index].Flags & VCF::MUTE) != VCF::NIL) ? true : false };
   BroadcastEvent(&event_volume, sizeof(event_volume));

   return ERR_Okay;

#endif
}

/*********************************************************************************************************************

-FIELD-
BitDepth: The bit depth affects the overall quality of audio input and output.

This field manages the bit depth for audio mixing and output.  Valid bit depths are 8, 16 and 24, with 16 being the
recommended value for CD quality playback.

*********************************************************************************************************************/

static ERROR SET_BitDepth(extAudio *Self, LONG Value)
{
   if (Value IS 16) Self->BitDepth = 16;
   else if (Value IS 8) Self->BitDepth = 8;
   else if (Value IS 24) Self->BitDepth = 24;
   else return ERR_Failed;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Device: The name of the audio device used by this audio object.

A host platform may have multiple audio devices installed, but a given audio object can represent only one device
at a time.  A new audio object will always represent the default device initially.  Choose a different device by
setting the Device field to a valid alternative.

The default device can always be referenced with a name of `default`.

*********************************************************************************************************************/

static ERROR GET_Device(extAudio *Self, CSTRING *Value)
{
   *Value = Self->Device.c_str();
   return ERR_Okay;
}

static ERROR SET_Device(extAudio *Self, CSTRING Value)
{
   if ((!Value) or (!*Value)) Self->Device = "default";
   else {
      Self->Device = Value;
      std::transform(Self->Device.begin(), Self->Device.end(), Self->Device.begin(), ::tolower);
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Flags: Special audio flags can be set here.
Lookup: ADF

The audio class supports a number of special flags that affect internal behaviour.  The following table illustrates the
publicly available flags:

<types lookup="ADF"/>

-FIELD-
InputRate: Determines the frequency to use when recording audio data.

The InputRate determines the frequency to use when recording audio data from a Line-In connection or microphone.  In
most cases, this value should be set to 44100 for CD quality audio.

The InputRate can only be set prior to initialisation, further attempts to set the field will be ignored.  On some
platforms, it may not be possible to set an InputRate that is different to the #OutputRate.  In such a case, the value
of the InputRate shall be ignored.

-FIELD-
MasterVolume: The master volume to use for audio playback.

The MasterVolume field controls the amount of volume applied to all of the audio channels.  Volume is expressed as
a value between 0 and 1.0.

*********************************************************************************************************************/

static ERROR GET_MasterVolume(extAudio *Self, DOUBLE *Value)
{
   *Value = Self->MasterVolume;
   return ERR_Okay;
}

static ERROR SET_MasterVolume(extAudio *Self, DOUBLE Value)
{
   if (Value < 0) Value = 0;
   else if (Value > 1.0) Value = 1.0;

   struct sndSetVolume setvol;
   setvol.Index   = 0;
   setvol.Name    = "Master";
   setvol.Volume  = Value;
   setvol.Channel = -1;
   setvol.Flags   = SVF::NIL;
   return Action(MT_SndSetVolume, Self, &setvol);
}

/*********************************************************************************************************************

-FIELD-
MixerLag: Returns the lag time of the internal mixer, measured in seconds.

This field will return the worst-case value for latency imposed by the internal audio mixer.  The value is measured
in seconds and will differ between platforms and user configurations.

*********************************************************************************************************************/

static ERROR GET_MixerLag(extAudio *Self, DOUBLE *Value)
{
   *Value = Self->MixerLag();
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Mute:  Mutes all audio output.

Audio output can be muted at any time by setting this value to TRUE.  To restart audio output after muting, set the
field to FALSE.  Muting does not disable the audio system, which is achieved by calling #Deactivate().

*********************************************************************************************************************/

static ERROR GET_Mute(extAudio *Self, LONG *Value)
{
   *Value = FALSE;
   for (LONG i=0; i < (LONG)Self->Volumes.size(); i++) {
      if (!StrMatch("Master", Self->Volumes[i].Name.c_str())) {
         if ((Self->Volumes[i].Flags & VCF::MUTE) != VCF::NIL) *Value = TRUE;
         break;
      }
   }
   return ERR_Okay;
}

static ERROR SET_Mute(extAudio *Self, LONG Value)
{
   struct sndSetVolume setvol = {
      .Index   = 0,
      .Name    = "Master",
      .Volume  = -1
   };
   if (Value) setvol.Flags = SVF::MUTE;
   else setvol.Flags = SVF::UNMUTE;
   return Action(MT_SndSetVolume, Self, &setvol);
}

/*********************************************************************************************************************

-FIELD-
OutputRate: Determines the frequency to use for the output of audio data.

The OutputRate determines the frequency of the audio data that will be output to the audio speakers.  In most cases,
this value should be set to 44100 for CD quality audio.

The OutputRate can only be set prior to initialisation, further attempts to set the field will be ignored.

*********************************************************************************************************************/

static ERROR SET_OutputRate(extAudio *Self, LONG Value)
{
   if (Value < 0) return ERR_OutOfRange;
   else if (Value > 44100) Self->OutputRate = 44100;
   else Self->OutputRate = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Periods: Defines the number of periods that make up the internal audio buffer.

The internal audio buffer is split into periods with each period being a certain byte size.  The minimum period is 2
and the maximum is 16.  This field is supplemented with the #PeriodSize, which indicates the byte size of each
period.  The total size of the audio buffer is calculated as the number of Periods multiplied by the PeriodSize value.

The minimum period size is 1K and maximum 16K.

*********************************************************************************************************************/

static ERROR SET_Periods(extAudio *Self, LONG Value)
{
   Self->Periods = Value;
   if (Self->Periods < 2) Self->Periods = 2;
   else if (Self->Periods > 16) Self->Periods = 16;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
PeriodSize: Defines the byte size of each period allocated to the internal audio buffer.

Refer to the #Periods field for further information.

*********************************************************************************************************************/

static ERROR SET_PeriodSize(extAudio *Self, LONG Value)
{
   Self->PeriodSize = Value;
   if (Self->PeriodSize < 1024) Self->PeriodSize = 1024;
   else if (Self->PeriodSize > 16384) Self->PeriodSize = 16384;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Quality: Determines the quality of the audio mixing.

Alter the quality of internal audio mixing by adjusting the Quality field.  The value range is from 0 (low quality) and
100 (high quality).  A setting between 70 and 80 is recommended.  Setting the Quality field results in the following
flags being automatically adjusted in the audio object: `ADF_FILTER_LOW`, `ADF_FILTER_HIGH` and `ADF_OVER_SAMPLING`.

In general, low quality mixing should only be used when the audio output needs to be raw, or if the audio speaker is
of low quality.

*********************************************************************************************************************/

static ERROR SET_Quality(extAudio *Self, LONG Value)
{
   Self->Quality = Value;

   Self->Flags &= ~(ADF_FILTER_LOW|ADF_FILTER_HIGH|ADF_OVER_SAMPLING);

   if (Self->Quality < 10) return ERR_Okay;
   else if (Self->Quality < 33) Self->Flags |= ADF_FILTER_LOW;
   else if (Self->Quality < 66) Self->Flags |= ADF_FILTER_HIGH;
   else Self->Flags |= ADF_OVER_SAMPLING|ADF_FILTER_HIGH;

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Stereo: Set to TRUE for stereo output and FALSE for mono output.

-END-

*********************************************************************************************************************/

static ERROR GET_Stereo(extAudio *Self, LONG *Value)
{
   if (Self->Flags & ADF_STEREO) *Value = TRUE;
   else *Value = FALSE;
   return ERR_Okay;
}

static ERROR SET_Stereo(extAudio *Self, LONG Value)
{
   if (Value IS TRUE) Self->Flags |= ADF_STEREO;
   else Self->Flags &= ~ADF_STEREO;
   return ERR_Okay;
}

//********************************************************************************************************************

static void load_config(extAudio *Self)
{
   parasol::Log log(__FUNCTION__);

   // Attempt to get the user's preferred pointer settings from the user:config/pointer file.

   objConfig::create config = { fl::Path("user:config/audio.cfg") };

   if (config.ok()) {
      config->read("AUDIO", "OutputRate", &Self->OutputRate);
      config->read("AUDIO", "InputRate", &Self->InputRate);
      config->read("AUDIO", "Quality", &Self->Quality);
      config->read("AUDIO", "BitDepth", &Self->BitDepth);

      LONG value;
      if (!config->read("AUDIO", "Periods", &value)) SET_Periods(Self, value);
      if (!config->read("AUDIO", "PeriodSize", &value)) SET_PeriodSize(Self, value);

      if (config->read("AUDIO", "Device", Self->Device)) Self->Device = "default";

      std::string str;
      Self->Flags |= ADF_STEREO;
      if (!config->read("AUDIO", "Stereo", str)) {
         if (!StrMatch("FALSE", str.c_str())) Self->Flags &= ~ADF_STEREO;
      }

      if ((Self->BitDepth != 8) and (Self->BitDepth != 16) and (Self->BitDepth != 24)) Self->BitDepth = 16;
      SET_Quality(Self, Self->Quality);

      // Find the mixer section, then load the mixer information

      ConfigGroups *groups;
      if (!config->getPtr(FID_Data, &groups)) {
         for (auto& [group, keys] : groups[0]) {
            if (!StrMatch("MIXER", group.c_str())) {
               Self->Volumes.clear();
               Self->Volumes.resize(keys.size());

               LONG j = 0;
               for (auto& [k, v] : keys) {
                  Self->Volumes[j].Name = k;

                  CSTRING str = v.c_str();
                  if (StrToInt(str) IS 1) Self->Volumes[j].Flags |= VCF::MUTE;
                  while ((*str) and (*str != ',')) str++;
                  if (*str IS ',') str++;

                  LONG channel = 0;
                  if (*str IS '[') { // Read channel volumes
                     str++;
                     while ((*str) and (*str != ']')) {
                        Self->Volumes[j].Channels[channel] = StrToInt(str);
                        while ((*str) and (*str != ',') and (*str != ']')) str++;
                        if (*str IS ',') str++;
                        channel++;
                     }
                  }

                  while (channel < (LONG)Self->Volumes[j].Channels.size()) {
                     Self->Volumes[j].Channels[channel] = 0.75;
                     channel++;
                  }
                  j++;
               }
            }
            break;
         }
      }
   }
}

//********************************************************************************************************************

#include "audio_def.c"

static const FieldArray clAudioFields[] = {
   { "OutputRate",    FDF_LONG|FDF_RI,    0, NULL, (APTR)SET_OutputRate },
   { "InputRate",     FDF_LONG|FDF_RI,    0, NULL, NULL },
   { "Quality",       FDF_LONG|FDF_RW,    0, NULL, (APTR)SET_Quality },
   { "Flags",         FDF_LONGFLAGS|FDF_RI, (MAXINT)&clAudioFlags, NULL, NULL },
   { "BitDepth",      FDF_LONG|FDF_RI,    0, NULL, (APTR)SET_BitDepth },
   { "Periods",       FDF_LONG|FDF_RI,    0, NULL, (APTR)SET_Periods },
   { "PeriodSize",    FDF_LONG|FDF_RI,    0, NULL, (APTR)SET_PeriodSize },
   // VIRTUAL FIELDS
   { "Device",        FDF_STRING|FDF_RW,  0, (APTR)GET_Device,       (APTR)SET_Device },
   { "MixerLag",      FDF_DOUBLE|FDF_R,   0, (APTR)GET_MixerLag,     NULL },
   { "MasterVolume",  FDF_DOUBLE|FDF_RW,  0, (APTR)GET_MasterVolume, (APTR)SET_MasterVolume },
   { "Mute",          FDF_LONG|FDF_RW,    0, (APTR)GET_Mute,         (APTR)SET_Mute },
   { "Stereo",        FDF_LONG|FDF_RW,    0, (APTR)GET_Stereo,       (APTR)SET_Stereo },
   END_FIELD
};

//********************************************************************************************************************

ERROR add_audio_class(void)
{
   clAudio = objMetaClass::create::global(
      fl::BaseClassID(ID_AUDIO),
      fl::ClassVersion(1.0),
      fl::Name("Audio"),
      fl::Category(CCF_AUDIO),
      fl::Actions(clAudioActions),
      fl::Methods(clAudioMethods),
      fl::Fields(clAudioFields),
      fl::Size(sizeof(extAudio)),
      fl::Path(MOD_PATH));

   return clAudio ? ERR_Okay : ERR_AddClass;
}

void free_audio_class(void)
{
   if (clAudio) { acFree(clAudio); clAudio = NULL; }
}
