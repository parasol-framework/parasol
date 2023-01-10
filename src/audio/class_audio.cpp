/*********************************************************************************************************************

-CLASS-
Audio: Supports a machine's audio hardware and provides a client-server audio management service.

The Audio class provides a common audio service that works across multiple platforms and follows a client-server
design model.

Supported features include 8-bit and 16-bit output in stereo or mono, oversampling, streaming, multiple audio channels,
sample sharing and command sequencing.  The Audio class functionality is simplified via the @Sound class interface,
which we recommend in most cases where simplified audio playback is satisfactory.

In some cases the audio server may be managed in a separate process space and allocated with a name of 'SystemAudio'.
In this circumstance all communication with the SystemAudio object will typically be achieved by messaging protocols,
but field values may be read in the normal manner.

Support for audio recording is not currently available.

-END-

*********************************************************************************************************************/

/*********************************************************************************************************************
-ACTION-
Activate: Enables access to the audio hardware and initialises the mixer.

An audio object will not play or record until it has been activated.  Activating the object will result in an attempt
to lock the device hardware, which on some platforms may lead to failure if another process has permanently locked the
device.  The resources and any device locks obtained by this action can be released with a call to
#Deactivate().

An inactive audio object can operate in a limited fashion but will not otherwise interact directly with the audio
hardware.

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

   // Calculate one mixing element size

   if (Self->BitDepth IS 16) Self->SampleBitSize = 2;
   else if (Self->BitDepth IS 24) Self->SampleBitSize = 3;
   else Self->SampleBitSize = 1;

   if (Self->Stereo) Self->SampleBitSize = Self->SampleBitSize<<1;

   // Allocate a floating-point mixing buffer

   #define MIXBUFLEN 20      // mixing buffer length 1/20th of a second
   if (Self->Stereo) Self->MixBitSize = sizeof(FLOAT) * 2;
   else Self->MixBitSize = sizeof(FLOAT);

   Self->MixBufferSize = (((Self->MixBitSize * Self->OutputRate) / MIXBUFLEN) + 15) & 0xfffffff0;
   Self->MixElements   = Self->MixBufferSize / Self->MixBitSize;

   if (!AllocMemory(Self->MixBufferSize + 1024, MEM_DATA, &Self->BufferMemory)) {
      // Align to 1024 bytes
      Self->MixBuffer = (APTR)((((UMAXINT)Self->BufferMemory) + 1023) & (~1023));

      // Pick the correct mixing routines

      if (Self->Flags & ADF_OVER_SAMPLING) {
         if (Self->Stereo) Self->MixRoutines = MixStereoFloatInterp;
         else Self->MixRoutines = MixMonoFloatInterp;
      }
      else if (Self->Stereo) Self->MixRoutines = MixStereoFloat;
      else Self->MixRoutines = MixMonoFloat;

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

Audio samples can be loaded into an Audio object for playback via the AddSample or #AddStream() method.  For small
samples under 512k we recommend AddSample, while anything larger should be supported through AddStream.

When adding a sample, is is essential to select the correct bit format for the sample data.  While it is important to
differentiate between simple attributes such as 8 or 16 bit data, mono or stereo format, you should also be aware of
whether or not the data is little or big endian, and if the sample data consists of signed or unsigned values.  Because
of the possible variations there are a number of sample formats, as illustrated in the following table:

<types lookup="SFM"/>

By default, all samples are assumed to be in little endian format, as supported by Intel CPU's.  If the data is in big
endian format, or the SampleFormat value with `SFM_BIG_ENDIAN`.

It is also possible to supply loop information with the sample data.  The Audio class supports a number of different
looping formats, allowing you to go beyond simple loops that repeat from the beginning of the sample.  The
&AudioLoop structure illustrates your options:

&AudioLoop

The types of that can be specified in the LoopMode field are:

&LOOP

The Loop1Type and Loop2Type fields alter the style of the loop.  These can be set to the following:

&LTYPE

The AddSample method may not be called directly because the audio object is often managed within a separate process.
If you attempt to grab the audio object and call this method, it returns `ERR_IllegalActionAttempt`. The only safe means
for calling this method is through the WaitMsg() function.

-INPUT-
int(SFM) SampleFormat: Indicates the format of the sample data that you are adding.
buf(ptr) Data: Points to the address of the sample data.
bufsize DataSize: Size of the sample data, in bytes.
struct(*AudioLoop) Loop: Points to the sample information that you want to add.
structsize LoopSize: Must be set to sizeof(AudioLoop).
&int Result: The resulting sample handle will be returned in this parameter.

-ERRORS-
Okay
Args
NullArgs
IllegalActionAttempt: You attempted to call this method directly using the Action() function.  Use ActionMsg() instead.
ReallocMemory: The existing sample handle array could not be expanded.
AllocMemory: Failed to allocate enough memory to hold the sample data.
-END-

*********************************************************************************************************************/

ERROR AUDIO_AddSample(extAudio *Self, struct sndAddSample *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   log.branch("Data: %p, Length: %d", Args->Data, Args->DataSize);

   // Find an unused sample block.  If there is none, increase the size of the sample management area.

   LONG handle;
   for (handle=1; handle < Self->TotalSamples; handle++) {
      if (!Self->Samples[handle].Used) break;
   }

   if (handle >= Self->TotalSamples) {
      if (!ReallocMemory(Self->Samples, sizeof(AudioSample) * (Self->TotalSamples + 10), &Self->Samples, NULL)) {
         handle = Self->TotalSamples;
         Self->TotalSamples += 10;
      }
      else return log.warning(ERR_ReallocMemory);
   }

   AudioSample *sample = &Self->Samples[handle];
   ClearMemory(sample, sizeof(AudioSample));

   LONG shift = sample_shift(Args->SampleFormat);

   sample->SampleType   = Args->SampleFormat;
   sample->SampleLength = Args->DataSize >> shift;
   sample->Used         = true;

   if (auto loop = Args->Loop) {
      sample->LoopMode     = loop->LoopMode;
      sample->Loop1Start   = loop->Loop1Start >> shift;
      sample->Loop1End     = loop->Loop1End >> shift;
      sample->Loop1Type    = loop->Loop1Type;
      sample->Loop2Start   = loop->Loop2Start >> shift;
      sample->Loop2End     = loop->Loop2End >> shift;
      sample->Loop2Type    = loop->Loop2Type;
   }

   // Eliminate zero-byte loops

   if (sample->Loop1Start IS sample->Loop1End) sample->Loop1Type = 0;
   if (sample->Loop2Start IS sample->Loop2End) sample->Loop2Type = 0;

   if ((!sample->SampleType) or (Args->DataSize <= 0) or (!Args->Data)) {
      sample->Data = NULL;
   }
   else if (!AllocMemory(Args->DataSize, MEM_DATA|MEM_NO_CLEAR, &sample->Data)) {
      CopyMemory(Args->Data, sample->Data, Args->DataSize);
   }
   else return log.warning(ERR_AllocMemory);

   Args->Result = handle;
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
AddStream: Adds a new sample-stream to an Audio object for channel-based playback.

Use AddStream to load large sound samples to an Audio object, allowing it to play those samples on the client
machine without over-provisioning available resources.  For small samples under 512k consider using AddSample instead.

The data source used for a stream can be located either at an accessible file path (through the Path parameter),
or via an object that has stored the data (through the Object parameter). Set SeekStart to alter the byte position
at which the audio data starts within the stream source.  The SampleLength parameter must also refer to the
byte-length of the entire audio stream.

When creating a new stream, pay attention to the audio format that is being used for the sample data.
While it is important to differentiate between 8-bit, 16-bit, mono and stereo, you should also be
aware of whether or not the data is little or big endian, and if the sample data consists of signed or unsigned values.
Because of the possible variations there are a number of sample formats, as illustrated in the following table:

<types lookup="SFM"/>

By default, all samples are assumed to be in little endian format, as supported by Intel CPU's.  If the data is in big
endian format, logical-or the SampleFormat value with the flag `SFM_BIG_ENDIAN`.

It is also possible to supply loop information with the stream.  The Audio class supports a number of different looping
formats via the &AudioLoop structure:

&AudioLoop

There are three types of loop modes that can be specified in the LoopMode field:

&LOOP

The Loop1Type and Loop2Type fields normally determine the style of the loop, however only unidirectional looping is
currently supported for streams.  For that reason, set the type variables to either NULL or `LTYPE_UNIDIRECTIONAL`.

-INPUT-
cstr Path: Refers to the file that contains the sample data, or NULL if you will supply an ObjectID.
oid ObjectID: Refers to the public object that contains the sample data (if no Path has been specified).  The object must support the Read and Seek actions or it will not be possible to stream data from it.
int SeekStart: Offset to use when seeking to the start of sample data.
int(SFM) SampleFormat: Indicates the format of the sample data that you are adding.
int SampleLength: Total byte-length of the sample data that is being streamed.  May be set to zero if the length is infinite or unknown.
int BufferLength: Total byte-length of the audio stream buffer that you would like to be allocated internally (large buffers affect timing).
struct(*AudioLoop) Loop: Refers to sample loop information, or NULL if no loop is required.
structsize LoopSize: Must be set to sizeof(AudioLoop).
&int Result: The resulting sample handle will be returned in this parameter.

-ERRORS-
Okay
Args
NullArgs
IllegalActionAttempt: You attempted to call this method directly using the Action() function.  Use ActionMsg() instead.
ReallocMemory: The existing sample handle array could not be expanded.
AllocMemory: Failed to allocate the stream buffer.
CreateObject: Failed to create a file object based on the supplied Path.
-END-

*********************************************************************************************************************/

#define MAX_STREAM_BUFFER 32768  // Max stream buffer length in bytes

static ERROR AUDIO_AddStream(extAudio *Self, struct sndAddStream *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->SampleFormat)) return log.warning(ERR_NullArgs);
   if ((!Args->Path) and (!Args->ObjectID)) return log.warning(ERR_NullArgs);

   if (Args->Path) log.branch("Path: %s, Length: %d", Args->Path, Args->SampleLength);
   else log.branch("Object: %d, Length: %d", Args->ObjectID, Args->SampleLength);

   // Find an unused sample block.  If there is none, increase the size of the sample management area.

   LONG handle;
   if (Self->Samples) {
      for (handle=1; handle < Self->TotalSamples; handle++) {
         if (!Self->Samples[handle].Used) break;
      }
   }
   else handle = Self->TotalSamples;

   if (handle >= Self->TotalSamples) {
      log.msg("Reallocating sample list.");
      if (!ReallocMemory(Self->Samples, sizeof(AudioSample) * (Self->TotalSamples + 10), &Self->Samples, NULL)) {
         handle = Self->TotalSamples;
         Self->TotalSamples += 10;
      }
      else return log.warning(ERR_ReallocMemory);
   }

   LONG shift = sample_shift(Args->SampleFormat);
   LONG bufferlength;
   if (!(bufferlength = Args->BufferLength)) {
      if (Args->SampleLength > 0) {
         // Calculate the length of the stream buffer as half of the sample length.
         // (This will be limited by the maximum possible amount of stream space).

         bufferlength = Args->SampleLength / 2;
      }
      else bufferlength = MAX_STREAM_BUFFER; // Use the recommended amount of buffer space
   }

   if (bufferlength > MAX_STREAM_BUFFER) bufferlength = MAX_STREAM_BUFFER;

   if (bufferlength < 256) {
      log.msg("Warning: Buffer length of %d is less than minimum byte size of 256.", bufferlength);
      bufferlength = 256;
   }

   #ifdef ALSA_ENABLED
      if (bufferlength < Self->AudioBufferSize) log.warning("Warning: Buffer length of %d is less than audio buffer size of %d.", bufferlength, Self->AudioBufferSize);
   #endif

   // Setup the audio sample

   AudioSample *sample = &Self->Samples[handle];
   ClearMemory(sample, sizeof(AudioSample));
   sample->Used         = true;
   sample->SampleType   = Args->SampleFormat;
   sample->SampleLength = bufferlength>>shift;
   sample->SeekStart    = Args->SeekStart;
   sample->StreamLength = (Args->SampleLength > 0) ? Args->SampleLength : 0x7fffffff; // 'Infinite' stream length
   sample->BufferLength = bufferlength;
   sample->LoopMode     = LOOP_SINGLE;
   sample->Loop1End     = bufferlength>>shift;
   sample->Loop1Type    = LTYPE_UNIDIRECTIONAL;

   if (Args->Loop) {
      sample->Loop2Type    = LTYPE_UNIDIRECTIONAL;
      sample->Loop2Start   = Args->Loop->Loop1Start;
      sample->Loop2End     = Args->Loop->Loop1End;
      sample->StreamLength = sample->Loop2End;
   }

   if (Args->ObjectID) sample->StreamID = Args->ObjectID;
   else {
      objFile *stream_file;
      ERROR error;
      if (!NewLockedObject(ID_FILE, NF::INTEGRAL, &stream_file, &sample->StreamID)) {
         if (!SetFields(stream_file, FID_Path|TSTR, Args->Path, FID_Flags|TLONG, FL_READ, TAGEND)) {
            if (!acInit(stream_file)) {
               error = ERR_Okay;
            }
            else error = ERR_Init;
         }
         else error = ERR_SetField;

         if (error) { acFree(stream_file); sample->StreamID = 0; }

         ReleaseObject(stream_file);
      }
      else error = ERR_NewObject;

      if (error) return log.warning(error);

      sample->Free = true;
   }

   if (AllocMemory(sample->BufferLength, MEM_DATA, &sample->Data) != ERR_Okay) {
      return ERR_AllocMemory;
   }

   // Fill the buffer with data from the stream object

   parasol::ScopedObjectLock<> stream(sample->StreamID, 5000);
   if (stream.granted()) {
      log.trace("Filling the buffer with sample data from source object #%d.", sample->StreamID);

      acSeek(*stream, sample->SeekStart, SEEK_START);
      acRead(*stream, sample->Data, sample->BufferLength, NULL);
   }
   else log.warning("Failed to access stream source #%d.", sample->StreamID);

   Args->Result = handle;
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
BufferCommand: Sends instructions to the audio mixer.

BufferCommand sends command sequences to an audio object for progressive execution.  This playback method is ideal for
music sequencers or any situation requiring audio commands to be executed at precise intervals.

By default, execution of commands is immediate.  Commands are constructed from a command ID, a target channel, and an
optional parameter dependent on the command type.  The following commands are available:

<types lookup="CMD"/>

Batched sequencing is enabled when a channel set is opened with a large number of command buffers (refer to
OpenChannel for details).  Call the BufferCommand method with `CMD_START_SEQUENCE`, then send the instructions
before terminating with `CMD_END_SEQUENCE`.  Each individual batch of commands will be executed at a predetermined
rate (e.g. every 125 milliseconds).  This allows the audio object to process batched commands at regular intervals.
The client will need to regularly write information to ensure that there are no pauses in the audio playback.

The following code illustrates a basic setup for batching commands:

<pre>
parasol::ScopedObjectLock<objAudio> audio(AudioID, 4);
if (audio.granted()) {
   LONG cycles = 0;
   while ((!sndBufferCommand(*audio, CMD_START_SEQUENCE, Self->Channels, NULL)) and
      (cycles < MAX_CYCLES)) {

       // Add buffered commands in this area.
       // ...

      sndBufferCommand(*audio, CMD_END_SEQUENCE, Self->Channels, NULL);
      cycles++;
   }
}
</pre>

The command sequencing rate is adjusted via the `CMD_SET_RATE` instruction.  The number of milliseconds specified
in the Data parameter will determine the rate at which the command sets are executed.  For instance a rate of 200ms
will execute five batches per second.

-INPUT-
int(CMD) Command: The ID of the command that you want to execute.
int Handle: Refers to the channel that the command is to be executed against (see the OpenChannels method for information).
double Data: Optional data value relevant to the command being executed.

-ERRORS-
Okay: The command was successfully buffered or executed.
Args
NullArgs
BufferOverflow: The command buffer is full.
NoSupport: The Command is not supported.
-END-

*********************************************************************************************************************/

static ERROR AUDIO_BufferCommand(extAudio *Self, struct sndBufferCommand *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Handle) or (!Args->Command)) return log.warning(ERR_NullArgs);

   log.trace("Command: %d, Handle: $%.8x, Data: %d", Args->Command, Args->Handle, Args->Data);

   LONG index = Args->Handle>>16;

   if (index >= ARRAYSIZE(Self->Channels)) {
      log.warning("Bad channel handle $%.8x.", Args->Handle);
      return ERR_Args;
   }

   if (Self->Channels[index].Commands) {
      // If this is the start of a sequence of commands and there is not much space in the command buffer, return an
      // overflow error.

      if ((Args->Command IS CMD_START_SEQUENCE) and
          (Self->Channels[index].Position >= Self->Channels[index].TotalCommands-16)) {
         return ERR_BufferOverflow;
      }

      // If there is not enough space in the buffer for this new command, return an overflow error.

      if (Self->Channels[index].Position >= Self->Channels[index].TotalCommands-1) {
         if (Args->Command IS CMD_END_SEQUENCE) {
            // If the command is an endsequence, roll-back to the CMD_START_SEQUENCE identifier so that all
            // the previous channel alterations are cancelled.
         }

         return ERR_BufferOverflow;
      }

      LONG i = Self->Channels[index].Position++;
      Self->Channels[index].Commands[i].CommandID = Args->Command;
      Self->Channels[index].Commands[i].Handle    = Args->Handle;
      Self->Channels[index].Commands[i].Data      = Args->Data;
      return ERR_Okay;
   }
   else { // Execute the command immediately
      switch (Args->Command) {
         case CMD_START_SEQUENCE: return ERR_Okay;
         case CMD_END_SEQUENCE:   return ERR_Okay;
         case CMD_CONTINUE:       return COMMAND_Continue(Self, Args->Handle);
         case CMD_FADE_IN:        return COMMAND_FadeIn(Self, Args->Handle);
         case CMD_FADE_OUT:       return COMMAND_FadeOut(Self, Args->Handle);
         case CMD_MUTE:           return COMMAND_Mute(Self, Args->Handle, Args->Data);
         case CMD_PLAY:           return COMMAND_Play(Self, Args->Handle, Args->Data);
         case CMD_SET_FREQUENCY:  return COMMAND_SetFrequency(Self, Args->Handle, Args->Data);
         case CMD_SET_LENGTH:     return COMMAND_SetLength(Self, Args->Handle, Args->Data);
         case CMD_SET_PAN:        return COMMAND_SetPan(Self, Args->Handle, Args->Data);
         case CMD_SET_RATE:       return COMMAND_SetRate(Self, Args->Handle, Args->Data);
         case CMD_SET_SAMPLE:     return COMMAND_SetSample(Self, Args->Handle, Args->Data);
         case CMD_SET_VOLUME:     return COMMAND_SetVolume(Self, Args->Handle, Args->Data);
         case CMD_STOP:           return COMMAND_Stop(Self, Args->Handle);
         case CMD_STOP_LOOPING:   return COMMAND_StopLooping(Self, Args->Handle);
         case CMD_SET_POSITION:   return COMMAND_SetPosition(Self, Args->Handle, Args->Data);
      }
   }

   log.warning("Unsupported command ID #%d.", Args->Command);
   return ERR_NoSupport;
}

/*********************************************************************************************************************

-ACTION-
Clear: Clears the audio buffers.

Call this action at any time to clear the internal audio buffers.  This will have the side-effect of temporarily
stopping all output until the next audio update occurs.

*********************************************************************************************************************/

static ERROR AUDIO_Clear(extAudio *Self, APTR Void)
{
   parasol::Log log;

   log.branch();

#ifdef _WIN32
   dsClear();
#else

#endif

   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
CloseChannels: Frees audio channels that have been allocated for sample playback.

Use CloseChannels to destroy a group of channels that have previously been allocated through the #OpenChannels()
method.  Any audio commands buffered against the channels will be cleared instantly.  Any audio data that has already
been mixed into the output buffer will continue to play for 1 - 2 seconds.  If this is an issue then the volume should
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

   LONG index = Args->Handle>>16;

   log.branch("Handle: $%.8x", Args->Handle);

   if ((index < 0) or (index >= ARRAYSIZE(Self->Channels))) log.warning(ERR_Args);

   if (Self->Channels[index].Channel)  FreeResource(Self->Channels[index].Channel);
   if (Self->Channels[index].Commands) FreeResource(Self->Channels[index].Commands);

   Self->TotalChannels -= Self->Channels[index].Total;

   ClearMemory(&Self->Channels[index], sizeof(ChannelSet));

   // If the total number of channels has been reduced to zero, clear the audio buffer output in order to
   // immediately stop all playback.

   log.msg("Total number of channels reduced to %d.", Self->TotalChannels);

   if (Self->TotalChannels <= 0) acClear(Self);

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
   if (Self->Flags & ADF_AUTO_SAVE) {
      Self->saveSettings();
   }

   if (Self->Timer) { UpdateTimer(Self->Timer, 0); Self->Timer = NULL; }

   if (Self->TaskRemovedHandle) { UnsubscribeEvent(Self->TaskRemovedHandle); Self->TaskRemovedHandle = NULL; }
   if (Self->UserLoginHandle)   { UnsubscribeEvent(Self->UserLoginHandle);   Self->UserLoginHandle = NULL; }

   acDeactivate(Self);

   for (LONG i=0; i < ARRAYSIZE(Self->Channels); i++) {
      if (Self->Channels[i].Channel)  { FreeResource(Self->Channels[i].Channel); Self->Channels[i].Channel = NULL; }
      if (Self->Channels[i].Commands) { FreeResource(Self->Channels[i].Commands); Self->Channels[i].Commands = NULL; }
   }

   if (Self->VolumeCtl)    { FreeResource(Self->VolumeCtl); Self->VolumeCtl = NULL; }
   if (Self->BufferMemory) { FreeResource(Self->BufferMemory); Self->BufferMemory = NULL; }

   if (Self->Samples) {
      for (LONG i=0; i < Self->TotalSamples; i++) {
         if (Self->Samples[i].Used) {
            if (Self->Samples[i].Data) FreeResource(Self->Samples[i].Data);
            if (Self->Samples[i].Free) acFree(Self->Samples[i].StreamID);
         }
      }

      FreeResource(Self->Samples);
      Self->Samples = NULL;
   }

#ifdef ALSA_ENABLED

   free_alsa(Self);

#elif _WIN32

   dsCloseDevice();

#endif

   // Destroy our task if we are in service mode

   if (Self->Flags & ADF_SERVICE_MODE) {
      SendMessage(0, MSGID_QUIT, 0, 0, 0);
   }

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

   Self->OutputRate = 44100;        // Rate for output to speakers
   Self->InputRate  = 44100;        // Input rate for recording
   Self->Quality    = 80;
   Self->Bass       = 50;
   Self->Treble     = 50;
   Self->BitDepth   = 16;
   Self->Flags      = ADF_OVER_SAMPLING|ADF_FILTER_HIGH|ADF_VOL_RAMPING|ADF_STEREO;
   Self->Periods    = 4;
   Self->PeriodSize = 2048;

   StrCopy("default", Self->Device, sizeof(Self->Device));

   const SystemState *state = GetSystemState();
   if ((!StrMatch(state->Platform, "Native")) or (!StrMatch(state->Platform, "Linux"))) {
      Self->Flags |= ADF_SYSTEM_WIDE;
   }

   // Allocate sample array

   Self->TotalSamples = 30;
   if (AllocMemory(Self->TotalSamples * sizeof(AudioSample), MEM_DATA, &Self->Samples) != ERR_Okay) {
      return ERR_AllocMemory;
   }

#ifdef __linux__
   if (!AllocMemory(sizeof(VolumeCtl) * 3, MEM_DATA|MEM_NO_CLEAR, &Self->VolumeCtl)) {
      StrCopy("Master", Self->VolumeCtl[0].Name, sizeof(Self->VolumeCtl[0].Name));
      Self->VolumeCtl[0].Flags = 0;
      for (LONG i=0; i < ARRAYSIZE(Self->VolumeCtl[0].Channels); i++) Self->VolumeCtl[0].Channels[i] = 0.75;

      StrCopy("PCM", Self->VolumeCtl[1].Name, sizeof(Self->VolumeCtl[1].Name));
      Self->VolumeCtl[1].Flags = 0;
      for (LONG i=0; i < ARRAYSIZE(Self->VolumeCtl[1].Channels); i++) Self->VolumeCtl[1].Channels[i] = 0.80;

      Self->VolumeCtl[2].Name[0] = 0;
   }
#else
   if (!AllocMemory(sizeof(VolumeCtl) * 2, MEM_DATA|MEM_NO_CLEAR, &Self->VolumeCtl)) {
      StrCopy("Master", Self->VolumeCtl[0].Name, sizeof(Self->VolumeCtl[0].Name));
      Self->VolumeCtl[0].Flags = 0;
      Self->VolumeCtl[0].Channels[0] = 0.75;
      for (LONG i=1; i < ARRAYSIZE(Self->VolumeCtl[0].Channels); i++) Self->VolumeCtl[0].Channels[i] = -1;

      Self->VolumeCtl[1].Name[0] = 0;
   }
#endif

   load_config(Self);

   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
OpenChannels: Allocates audio channels that can be used for sample playback.

Use the OpenChannels method to open audio channels for sample playback.  Channels are allocated in sets with a size
range between 1 and 64.  Channel sets make it easier to segregate playback between users of the same audio object.

You may also indicate to this method how many command sequencing buffers you would like to allocate for your channels.
This is particularly useful if you are writing a digital music sequencer, or if you want to process a number of
real-time channel adjustments with precision timing.  You can allocate a maximum of 1024 command buffers at a cost of
approximately eight bytes each.

The resulting handle returned from this method is an integer consisting of two parts.  The upper word uniquely
identifies the channel set that has been provided to you, while the lower word is used to refer to specific channel
numbers.  To refer to specific channels when using some functions, do so with the formula
`Channel = Handle | ChannelNo`.

To destroy allocated channels, use the #CloseChannels() method.

-INPUT-
int Total: Total of channels to allocate.
int Commands: The total number of command buffers to allocate for real-time processing.
&int Result: The resulting channel handle is returned in this parameter.

-ERRORS-
Okay
NullArgs
OutOfRange: The amount of requested channels or commands is outside of acceptable range.
AllocMemory: Memory for the audio channels could not be allocated.
ArrayFull: The maximum number of available channel sets is currently exhausted.
-END-

*********************************************************************************************************************/

static ERROR AUDIO_OpenChannels(extAudio *Self, struct sndOpenChannels *Args)
{
   parasol::Log log;
   LONG index, total;

   if (!Args) return log.warning(ERR_NullArgs);

   log.branch("Total: %d, Commands: %d", Args->Total, Args->Commands);

   Args->Result = 0;
   if ((Args->Total < 0) or (Args->Total > 64) or (Args->Commands < 0) or (Args->Commands > 1024)) {
      return log.warning(ERR_OutOfRange);
   }

   // Allocate the channels

   for (index=1; index < ARRAYSIZE(Self->Channels); index++) {
      if (!Self->Channels[index].Channel) break;
   }

   if (index >= ARRAYSIZE(Self->Channels)) return log.warning(ERR_ArrayFull);

   // Channels are tracked back to the task responsible for the allocation - this ensures that the channels are
   // deallocated properly in the event that a task crashes or forgets to deallocate its channels.

   log.msg("Allocating %d channels from index %d.", Args->Total, index);

   if (Self->Flags & ADF_OVER_SAMPLING) total = Args->Total * 2;
   else total = Args->Total;

   if (!AllocMemory(sizeof(AudioChannel) * total, MEM_DATA, &Self->Channels[index].Channel)) {
      Self->Channels[index].Total      = Args->Total;
      Self->Channels[index].Actual     = total;

      // Allocate the command buffer

      if (Args->Commands > 0) {
         if (!AllocMemory(sizeof(AudioCommand) * Args->Commands, MEM_DATA|MEM_CALLER, &Self->Channels[index].Commands)) {
            Self->Channels[index].TotalCommands = Args->Commands;
            Self->Channels[index].Position      = 0;
            Self->Channels[index].UpdateRate    = 125;  // Default update rate of 125ms (equates to 5000Hz)
            Self->Channels[index].MixLeft       = MixLeft(Self->Channels[index].UpdateRate);
         }
      }
      else {
         Self->Channels[index].TotalCommands = 0;
         Self->Channels[index].Position      = 0;
         Self->Channels[index].UpdateRate    = 0;
         Self->Channels[index].MixLeft       = 0;
      }

      Self->TotalChannels += Args->Total;
      Args->Result = index<<16;
      return ERR_Okay;
   }
   else return log.warning(ERR_AllocMemory);
}

/*********************************************************************************************************************

-METHOD-
RemoveSample: Removes a sample from the global sample list and deallocates its memory usage.

You can remove an allocated sample at any time by calling the RemoveSample method.  Once a sample is removed it is
permanently deleted from the audio server and it is not possible to reallocate the sample against the same handle
number.

Over time, the continued allocation of audio samples will mean that freed handle numbers will become available again
through the #AddSample() and #AddStream() methods.  For this reason you should clear all references to the sample
handle after removing it.

-INPUT-
int Handle: The handle of the sample that requires removal.

-ERRORS-
Okay
NullArgs
OutOfRange: The provided sample handle is not within the valid range.
IllegalActionAttempt: You attempted to call this method directly using the Action() function.  Use ActionMsg() instead.
-END-

*********************************************************************************************************************/

static ERROR AUDIO_RemoveSample(extAudio *Self, struct sndRemoveSample *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Handle)) return log.warning(ERR_NullArgs);

   log.branch("Sample: %d", Args->Handle);

   if ((Args->Handle < 0) or (Args->Handle >= Self->TotalSamples)) return log.warning(ERR_OutOfRange);

   if (Self->Samples) {
      if (auto sample = &Self->Samples[Args->Handle]) {
         if (!sample->Used) return ERR_Okay;

         sample->Used = false;
         if (sample->Data) { FreeResource(sample->Data); sample->Data = NULL; }
         if (sample->Free) { acFree(sample->StreamID); sample->StreamID = 0; }
      }
   }

   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Reset: Resets the audio settings to default values.
-END-
*********************************************************************************************************************/

static ERROR AUDIO_Reset(extAudio *Self, APTR Void)
{
   Self->Bass   = 50;
   Self->Treble = 50;
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
      config->write("AUDIO", "Bass", Self->Bass);
      config->write("AUDIO", "Treble", Self->Treble);

      if (Self->Flags & ADF_STEREO) config->write("AUDIO", "Stereo", "TRUE");
      else config->write("AUDIO", "Stereo", "FALSE");

#ifdef __linux__
      if (Self->Device[0]) config->write("AUDIO", "Device", Self->Device);
      else config->write("AUDIO", "Device", "default");

      if ((Self->VolumeCtl) and (Self->Flags & ADF_SYSTEM_WIDE)) {
         for (LONG i=0; Self->VolumeCtl[i].Name[0]; i++) {
            std::ostringstream out;
            if (Self->VolumeCtl[i].Flags & VCF_MUTE) out << "1,[";
            else out << "0,[";

            if (Self->VolumeCtl[i].Flags & VCF_MONO) {
               out << Self->VolumeCtl[i].Channels[0];
            }
            else for (LONG c=0; c < ARRAYSIZE(Self->VolumeCtl[i].Channels); c++) {
               if (c > 0) out << ',';
               out << Self->VolumeCtl[i].Channels[c];
            }
            out << ']';

            config->write("MIXER", Self->VolumeCtl[i].Name, out.str());
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
   snd_mixer_selem_id_set_name(sid, Self->VolumeCtl[i].Name);

   if ((elem = snd_mixer_find_selem(Self->MixHandle, sid))) {
      if (snd_mixer_selem_has_playback_volume(elem)) {
         snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);
         snd_mixer_selem_get_playback_volume(elem, 0, &left);
         snd_mixer_selem_get_playback_switch(elem, 0, &mute);
         if (Self->VolumeCtl[i].Flags & VCF_MONO) right = left;
         else snd_mixer_selem_get_playback_volume(elem, 1, &right);
      }
      else if (snd_mixer_selem_has_capture_volume(elem)) {
         snd_mixer_selem_get_capture_volume_range(elem, &pmin, &pmax);
         snd_mixer_selem_get_capture_volume(elem, 0, &left);
         snd_mixer_selem_get_capture_switch(elem, 0, &mute);
         if (Self->VolumeCtl[i].Flags & VCF_MONO) right = left;
         else snd_mixer_selem_get_capture_volume(elem, 1, &right);
      }
      else continue;

      if (pmin >= pmax) continue;

      std::ostringstream out;
      auto fleft = (DOUBLE)left / (DOUBLE)(pmax - pmin);
      auto fright = (DOUBLE)right / (DOUBLE)(pmax - pmin);
      out << fleft << ',' << fright << ',' << mute ? 0 : 1;

      config->write("MIXER", Self->VolumeCtl[i].Name, out.str());
   }
#endif

#else
      if (Self->VolumeCtl) {
         std::string out((Self->VolumeCtl[0].Flags & VCF_MUTE) ? "1,[" : "0,[");
         out.append(std::to_string(Self->VolumeCtl[0].Channels[0]));
         out.append("]");
         config->write("MIXER", Self->VolumeCtl[0].Name, out);
      }
#endif

      config->saveToObject(Args->DestID, 0);
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
SetVolume: Sets the volume for input and output mixers.

To change volume and mixer levels, use the SetVolume method.  It is possible to make adjustments to any of the
available mixers and for different channels per mixer - for instance you may set different volumes for left and right
speakers.  Support is also provided for special options, such as muting.

To set the volume for a mixer, use its index (by scanning the #VolumeCtl field) or set its name (to change the Master
volume, use a name of `Master`).  A channel needs to be specified, or use `CHN_ALL` to synchronise the volume for all
channels.  The new mixer value is set in the Volume field.  Optional flags may be set as follows:

<types lookup="SVF"/>

-INPUT-
int Index: The index of the mixer that you want to set.
cstr Name: If the correct index number is unknown, the name of the mixer may be set here.
int(SVF) Flags: Optional flags.
double Volume: The volume to set for the mixer.  Ranges between 0 - 1.0.  Set to -1 if you do not want to adjust the current volume.

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
   if (!Self->VolumeCtl) return log.warning(ERR_NoSupport);
   if (!Self->MixHandle) return ERR_NotInitialised;

   // Determine what mixer we are going to adjust

   if (Args->Name) {
      for (index=0; Self->VolumeCtl[index].Name[0]; index++) {
         if (!StrMatch(Args->Name, Self->VolumeCtl[index].Name)) break;
      }

      if (!Self->VolumeCtl[index].Name[0]) return ERR_Search;
   }
   else {
      index = Args->Index;
      if ((index < 0) or (index >= Self->VolumeCtlTotal)) return ERR_OutOfRange;
   }

   if (!StrMatch("Master", Self->VolumeCtl[index].Name)) {
      if (Args->Volume != -1) {
         glGlobalVolume = Args->Volume;
         Self->MasterVolume = Args->Volume;
      }

      if (Args->Flags & SVF_UNMUTE) {
         Self->VolumeCtl[index].Flags &= ~VCF_MUTE;
         Self->Mute = false;
      }
      else if (Args->Flags & SVF_MUTE) {
         Self->VolumeCtl[index].Flags |= VCF_MUTE;
         Self->Mute = true;
      }
   }

   // Apply the volume

   log.branch("%s: %.2f, Flags: $%.8x", Self->VolumeCtl[index].Name, Args->Volume, Args->Flags);

   snd_mixer_selem_id_alloca(&sid);
   snd_mixer_selem_id_set_index(sid,0);
   snd_mixer_selem_id_set_name(sid, Self->VolumeCtl[index].Name);
   if (!(elem = snd_mixer_find_selem(Self->MixHandle, sid))) {
      log.msg("Mixer \"%s\" not found.", Self->VolumeCtl[index].Name);
      return ERR_Search;
   }

   if (Args->Volume >= 0) {
      if (Self->VolumeCtl[index].Flags & VCF_CAPTURE) {
         snd_mixer_selem_get_capture_volume_range(elem, &pmin, &pmax);
      }
      else snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);

      pmax = pmax - 1; // -1 because the absolute maximum tends to produce distortion...

      DOUBLE vol = Args->Volume;
      if (vol > 1.0) vol = 1.0;
      LONG lvol = F2T(DOUBLE(pmin) + (DOUBLE(pmax - pmin) * vol));

      if (Self->VolumeCtl[index].Flags & VCF_CAPTURE) {
         snd_mixer_selem_set_capture_volume_all(elem, lvol);
      }
      else snd_mixer_selem_set_playback_volume_all(elem, lvol);

      if (Self->VolumeCtl[index].Flags & VCF_MONO) {
         Self->VolumeCtl[index].Channels[0] = vol;
      }
      else for (LONG channel=0; channel < ARRAYSIZE(Self->VolumeCtl[0].Channels); channel++) {
         if (Self->VolumeCtl[index].Channels[channel] >= 0) {
            Self->VolumeCtl[index].Channels[channel] = vol;
         }
      }
   }

   if (Args->Flags & SVF_UNMUTE) {
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
      Self->VolumeCtl[index].Flags &= ~VCF_MUTE;
   }
   else if (Args->Flags & SVF_MUTE) {
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
      Self->VolumeCtl[index].Flags |= VCF_MUTE;
   }

   if (Args->Flags & SVF_UNSYNC) Self->VolumeCtl[index].Flags &= ~VCF_SYNC;
   else if (Args->Flags & SVF_SYNC) Self->VolumeCtl[index].Flags |= VCF_SYNC;

   EVENTID evid = GetEventID(EVG_AUDIO, "volume", Self->VolumeCtl[index].Name);
   evVolume event_volume = { evid, Args->Volume, (Self->VolumeCtl[index].Flags & VCF_MUTE) ? true : false };
   BroadcastEvent(&event_volume, sizeof(event_volume));
   return ERR_Okay;

#else

   WORD index;

   if (!Args) return log.warning(ERR_NullArgs);
   if (((Args->Volume < 0) or (Args->Volume > 1.0)) and (Args->Volume != -1)) {
      return log.warning(ERR_OutOfRange);
   }
   if (!Self->VolumeCtl) return log.warning(ERR_NoSupport);

   // Determine what mixer we are going to adjust

   if (Args->Name) {
      for (index=0; Self->VolumeCtl[index].Name[0]; index++) {
         if (!StrMatch(Args->Name, Self->VolumeCtl[index].Name)) break;
      }

      if (!Self->VolumeCtl[index].Name[0]) return ERR_Search;
   }
   else {
      index = Args->Index;
      if ((index < 0) or (index >= Self->VolumeCtlTotal)) return ERR_OutOfRange;
   }

   if (!StrMatch("Master", Self->VolumeCtl[index].Name)) {
      if (Args->Volume != -1) {
         glGlobalVolume = Args->Volume;
         Self->MasterVolume = Args->Volume;
      }

      if (Args->Flags & SVF_UNMUTE) {
         Self->VolumeCtl[index].Flags &= ~VCF_MUTE;
         Self->Mute = false;
      }
      else if (Args->Flags & SVF_MUTE) {
         Self->VolumeCtl[index].Flags |= VCF_MUTE;
         Self->Mute = true;
      }
   }

   // Apply the volume

   log.branch("%s: %.2f, Flags: $%.8x", Self->VolumeCtl[index].Name, Args->Volume, Args->Flags);

   if ((Args->Volume >= 0) and (Args->Volume <= 1.0)) {
      if (Self->VolumeCtl[index].Flags & VCF_MONO) {
         Self->VolumeCtl[index].Channels[0] = Args->Volume;
      }
      else for (LONG channel=0; channel < ARRAYSIZE(Self->VolumeCtl[0].Channels); channel++) {
         if (Self->VolumeCtl[index].Channels[channel] >= 0) {
            Self->VolumeCtl[index].Channels[channel] = Args->Volume;
         }
      }
   }

   if (Args->Flags & SVF_UNMUTE) Self->VolumeCtl[index].Flags &= ~VCF_MUTE;
   else if (Args->Flags & SVF_MUTE) Self->VolumeCtl[index].Flags |= VCF_MUTE;

   if (Args->Flags & SVF_UNSYNC) Self->VolumeCtl[index].Flags &= ~VCF_SYNC;
   else if (Args->Flags & SVF_SYNC) Self->VolumeCtl[index].Flags |= VCF_SYNC;

   EVENTID evid = GetEventID(EVG_AUDIO, "volume", Self->VolumeCtl[index].Name);
   evVolume event_volume = { evid, Args->Volume, (Self->VolumeCtl[index].Flags & VCF_MUTE) ? true : false };
   BroadcastEvent(&event_volume, sizeof(event_volume));

   return ERR_Okay;

#endif
}

/*********************************************************************************************************************

-FIELD-
Bass: Sets the amount of bass to use for audio playback.

The Bass field controls the amount of bass that is applied when audio is played back over the user's speakers.  Not all
platforms support bass adjustment.

The bass value ranges between 0 and 100, with 50 being the default setting.

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

A computer system may have multiple audio devices installed, but a given audio object can represent only one device at a
time.  A new audio object will always represent the default device initially.  You can switch to a different device by
setting the Device field to the name of the device that you would like to use.

The default device can always be referenced with a name of `Default`.

*********************************************************************************************************************/

static ERROR GET_Device(extAudio *Self, CSTRING *Value)
{
   *Value = Self->Device;
   return ERR_Okay;
}

static ERROR SET_Device(extAudio *Self, CSTRING Value)
{
   if ((!Value) or (!*Value)) Value = "Default";

   size_t i;
   for (i=0; Value[i] and i < sizeof(Self->Device)-1; i++) {
      if ((Value[i] >= 'A') and (Value[i] <= 'Z')) Self->Device[i] = Value[i] - 'A' + 'a';
      else Self->Device[i] = Value[i];
   }
   Self->Device[i] = 0;

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
   setvol.Index  = 0;
   setvol.Name   = "Master";
   setvol.Volume = Value;
   setvol.Flags  = 0;
   return DelayMsg(MT_SndSetVolume, Self->UID, &setvol);
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
   if (Self->VolumeCtl) {
      for (LONG i=0; Self->VolumeCtl[i].Name[0]; i++) {
         if (!StrMatch("Master", Self->VolumeCtl[i].Name)) {
            if (Self->VolumeCtl[i].Flags & VCF_MUTE) *Value = TRUE;
            break;
         }
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
   if (Value) setvol.Flags = SVF_MUTE;
   else setvol.Flags = SVF_UNMUTE;
   return DelayMsg(MT_SndSetVolume, Self->UID, &setvol);
}

/*********************************************************************************************************************

-FIELD-
OutputRate:  Determines the frequency to use for the output of audio data.

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
flags being automatically adjusted in the audio object: ADF_FILTER_LOW, ADF_FILTER_HIGH and ADF_OVER_SAMPLING.

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
   else Self->Flags |= ADF_OVER_SAMPLING;

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Stereo: Set to TRUE for stereo output and FALSE for mono output.

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

/*********************************************************************************************************************

-FIELD-
TotalChannels: The total number of audio channels allocated by all processes.

-FIELD-
Treble: Sets the amount of treble to use for audio playback.

The Treble field controls the amount of treble that is applied when audio is played back over the user's speakers.  Not
all platforms support treble adjustment.

The normal setting for treble is a value of 50, minimum treble is 0 and maximum treble is 100.

-FIELD-
VolumeCtl: An array of information for all known audio mixers in the system.

The VolumeCtl provides an array of all available mixer controls for the audio hardware.  The information is read-only
and each entry is structured as follows:

&VolumeCtl

Attribute flags include:

&VCF

To scan through the list of controls, search until an entry that uses a Name consisting of a single NULL
byte is found.
-END-

*********************************************************************************************************************/

static ERROR GET_VolumeCtl(extAudio *Self, struct VolumeCtl **Value)
{
#ifdef ALSA_ENABLED
   if (!Self->Handle) return ERR_NotInitialised;
#endif

   if (Self->VolumeCtl) {
      for (WORD i=0; Self->VolumeCtl[i].Name[0]; i++) {
         Self->VolumeCtl[i].Size = sizeof(VolumeCtl);
      }

      *Value = Self->VolumeCtl;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

//********************************************************************************************************************

static ERROR SetInternalVolume(extAudio *Self, AudioChannel *Channel)
{
   parasol::Log log(__FUNCTION__);
   FLOAT leftvol, rightvol;

   if ((!Self) or (!Channel)) return log.warning(ERR_NullArgs);

   if (Channel->Volume > 1.0) Channel->Volume = 1.0;
   else if (Channel->Volume < 0) Channel->Volume = 0;

   if (Channel->Pan < -1.0) Channel->Pan = -1.0;
   else if (Channel->Pan > 1.0) Channel->Pan = 1.0;

   // Convert the volume into left/right volume parameters

   if (Channel->Flags & CHF_MUTE) {
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

   Channel->Flags &= ~CHF_VOL_RAMP;
   if ((Self->Flags & ADF_OVER_SAMPLING) and (Self->Flags & ADF_VOL_RAMPING)) {

      if ((Channel->LVolume != leftvol) or (Channel->LVolumeTarget != leftvol)) {
         Channel->Flags |= CHF_VOL_RAMP;
         Channel->LVolumeTarget = leftvol;
      }

      if ((Channel->RVolume != rightvol) or (Channel->RVolumeTarget != rightvol)) {
         Channel->Flags |= CHF_VOL_RAMP;
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

#ifdef __linx__
static ERROR GetMixAmount(extAudio *Self, LONG *MixLeft)
#else
ERROR GetMixAmount(extAudio *Self, LONG *MixLeft)
#endif
{
   *MixLeft = 0x7fffffff;
   for (LONG i=0; i < ARRAYSIZE(Self->Channels); i++) {
      if ((Self->Channels[i].MixLeft > 0) and (Self->Channels[i].MixLeft < *MixLeft)) {
         *MixLeft = Self->Channels[i].MixLeft;
      }
   }

   return ERR_Okay;
}

//********************************************************************************************************************

#ifdef ALSA_ENABLED
static ERROR DropMixAmount(extAudio *Self, LONG Elements)
#else
ERROR DropMixAmount(extAudio *Self, LONG Elements)
#endif
{
   parasol::Log log(__FUNCTION__);

   for (LONG index=0; index < ARRAYSIZE(Self->Channels); index++) {
      if ((Self->Channels[index].Channel) and (Self->Channels[index].Commands)) {
         Self->Channels[index].MixLeft -= Elements;
         if (Self->Channels[index].MixLeft <= 0) {
            // Reset the amount of mixing elements left and execute the next set of channel commands

            Self->Channels[index].MixLeft = MixLeft(Self->Channels[index].UpdateRate);

            if (Self->Channels[index].Position <= 0) continue;

            LONG total = 0;
            LONG oldpos = Self->Channels[index].Position;

            // Skip start and end signals

            AudioCommand *cmd = Self->Channels[index].Commands;
            while ((cmd->CommandID IS CMD_START_SEQUENCE) or (cmd->CommandID IS CMD_END_SEQUENCE)) {
               total++;
               cmd++;
            }

            // Process commands

            while ((cmd->CommandID) and (cmd->CommandID != CMD_END_SEQUENCE) and (cmd->CommandID != CMD_START_SEQUENCE)) {
               switch(cmd->CommandID) {
                  case CMD_CONTINUE:       COMMAND_Continue(Self, cmd->Handle); break;
                  case CMD_FADE_IN:        COMMAND_FadeIn(Self, cmd->Handle); break;
                  case CMD_FADE_OUT:       COMMAND_FadeOut(Self, cmd->Handle); break;
                  case CMD_MUTE:           COMMAND_Mute(Self, cmd->Handle, cmd->Data); break;
                  case CMD_PLAY:           COMMAND_Play(Self, cmd->Handle, cmd->Data); break;
                  case CMD_SET_FREQUENCY:  COMMAND_SetFrequency(Self, cmd->Handle, cmd->Data); break;
                  case CMD_SET_LENGTH:     COMMAND_SetLength(Self, cmd->Handle, cmd->Data); break;
                  case CMD_SET_PAN:        COMMAND_SetPan(Self, cmd->Handle, cmd->Data); break;
                  case CMD_SET_RATE:       COMMAND_SetRate(Self, cmd->Handle, cmd->Data); break;
                  case CMD_SET_SAMPLE:     COMMAND_SetSample(Self, cmd->Handle, cmd->Data); break;
                  case CMD_SET_VOLUME:     COMMAND_SetVolume(Self, cmd->Handle, cmd->Data); break;
                  case CMD_STOP:           COMMAND_Stop(Self, cmd->Handle); break;
                  case CMD_STOP_LOOPING:   COMMAND_StopLooping(Self, cmd->Handle); break;
                  case CMD_SET_POSITION:   COMMAND_SetPosition(Self, cmd->Handle, cmd->Data); break;

                  default:
                     log.warning("Bad command ID #%d.", cmd->CommandID);
                     Self->Channels[index].Position = 0;
                     Self->Channels[index].Commands->CommandID = 0;
                     return ERR_Failed;
               }

               total++;
               cmd++;
            }

            // Skip start and end signals

            while ((cmd->CommandID IS CMD_START_SEQUENCE) or (cmd->CommandID IS CMD_END_SEQUENCE)) {
               total++;
               cmd++;
            }

            CopyMemory(cmd, Self->Channels[index].Commands, ((oldpos - total) * sizeof(AudioCommand)));

            Self->Channels[index].Position -= total;
            Self->Channels[index].Commands[Self->Channels[index].Position].CommandID = 0;
            Self->Channels[index].Commands[Self->Channels[index].Position].Handle    = 0;
         }
      }
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR audio_timer(extAudio *Self, LARGE Elapsed, LARGE CurrentTime)
{
#ifdef ALSA_ENABLED

   parasol::Log log(__FUNCTION__);
   LONG elements, mixleft, spaceleft, err, space;
   UBYTE *buffer;
   static WORD errcount = 0;

   // Get the amount of bytes available for output

   if (Self->Handle) {
      spaceleft = snd_pcm_avail_update(Self->Handle); // Returns available space in frames (multiply by SampleBitSize for bytes)
   }
   else if (Self->AudioBufferSize) { // Run in dummy mode - samples will be buffered but not played
      spaceleft = Self->AudioBufferSize;
   }
   else {
      log.warning("ALSA not in an initialised state.");
      return ERR_Terminate;
   }

   // If the audio system is inactive or in a bad state, try to fix it.

   if (spaceleft < 0) {
      log.warning("avail_update() %s", snd_strerror(spaceleft));

      errcount++;
      if (!(errcount % 50)) {
         log.warning("Broken audio - attempting fix...");

         acDeactivate(Self);

         if (acActivate(Self) != ERR_Okay) {
            if ((Self->Flags & ADF_SERVICE_MODE) and (errcount < 1000)) {
               log.warning("Audio error unrecoverable - will keep trying.");
            }
            else {
               log.warning("Audio error is terminal, self-destructing...");
               DelayMsg(AC_Free, Self->UID);
               return ERR_Failed;
            }
         }
      }

      return ERR_Okay;
   }

   if (Self->SampleBitSize) {
      if (spaceleft > Self->AudioBufferSize / Self->SampleBitSize) spaceleft = Self->AudioBufferSize / Self->SampleBitSize;
   }

   // Fill our entire audio buffer with data to be sent to alsa

   space = spaceleft;
   buffer = Self->AudioBuffer;
   while (spaceleft) {
      // Scan channels to check if an update rate is going to be met

      GetMixAmount(Self, &mixleft);

      if (mixleft < spaceleft) elements = mixleft;
      else elements = spaceleft;

      // Produce the audio data

      if (mix_data(Self, elements, buffer) != ERR_Okay) break;

      // Drop the mix amount.  This may also update buffered channels for the next round

      DropMixAmount(Self, elements);

      buffer = buffer + (elements * Self->SampleBitSize);
      spaceleft -= elements;
   }

   // Write the audio to alsa

   if (Self->Handle) {
      if ((err = snd_pcm_writei(Self->Handle, Self->AudioBuffer, space)) < 0) {
         // If an EPIPE error is returned, a buffer underrun has probably occurred

         if (err IS -EPIPE) {
            snd_pcm_status_t *status;
            LONG code;

            log.msg("A buffer underrun has occurred.");

            snd_pcm_status_alloca(&status);
            if (snd_pcm_status(Self->Handle, status) < 0) {
               return ERR_Okay;
            }

            code = snd_pcm_status_get_state(status);
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

   dsPlay(Self);

   return ERR_Okay;

#else

   #warning No audio timer support on this platform.
   return ERR_NoSupport;

#endif
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
      config->read("AUDIO", "Bass", &Self->Bass);
      config->read("AUDIO", "Treble", &Self->Treble);
      config->read("AUDIO", "BitDepth", &Self->BitDepth);

      LONG value;
      if (!config->read("AUDIO", "Periods", &value)) SET_Periods(Self, value);
      if (!config->read("AUDIO", "PeriodSize", &value)) SET_PeriodSize(Self, value);

      std::string str;
      if (!config->read("AUDIO", "Device", str)) StrCopy(str.c_str(), Self->Device, sizeof(Self->Device));
      else StrCopy("default", Self->Device, sizeof(Self->Device));

      Self->Flags |= ADF_STEREO;
      if (!config->read("AUDIO", "Stereo", str)) {
         if (!StrMatch("FALSE", str.c_str())) Self->Flags &= ~ADF_STEREO;
      }

      if ((Self->BitDepth != 8) and (Self->BitDepth != 16) and (Self->BitDepth != 24)) Self->BitDepth = 16;
      if ((Self->Treble < 0) or (Self->Treble > 100)) Self->Treble = 50;
      if ((Self->Bass < 0) or (Self->Bass > 100)) Self->Bass = 50;
      SET_Quality(Self, Self->Quality);

      // Find the mixer section, then load the mixer information

      ConfigGroups *groups;
      if (!config->getPtr(FID_Data, &groups)) {
         LONG j = 0;
         for (auto& [group, keys] : groups[0]) {
            if (!StrMatch("MIXER", group.c_str())) {
               if (Self->VolumeCtl) { FreeResource(Self->VolumeCtl); Self->VolumeCtl = 0; }

               if (!AllocMemory(sizeof(VolumeCtl) * (keys.size() + 1), MEM_NO_CLEAR, &Self->VolumeCtl)) {
                  Self->VolumeCtlTotal = keys.size();

                  for (auto& [k, v] : keys) {
                     StrCopy(k.c_str(), Self->VolumeCtl[j].Name, sizeof(Self->VolumeCtl[j].Name));

                     CSTRING str = v.c_str();
                     if (StrToInt(str) IS 1) Self->VolumeCtl[j].Flags |= VCF_MUTE;
                     while ((*str) and (*str != ',')) str++;
                     if (*str IS ',') str++;

                     LONG channel = 0;
                     if (*str IS '[') { // Read channel volumes
                        str++;
                        while ((*str) and (*str != ']')) {
                           Self->VolumeCtl[j].Channels[channel] = StrToInt(str);
                           while ((*str) and (*str != ',') and (*str != ']')) str++;
                           if (*str IS ',') str++;
                           channel++;
                        }
                     }

                     while (channel < ARRAYSIZE(Self->VolumeCtl[j].Channels)) {
                        Self->VolumeCtl[j].Channels[channel] = 0.75;
                        channel++;
                     }
                  }

                  Self->VolumeCtl[j].Name[0] = 0;
               }
            }
            break;
         }
      }
   }
}

//********************************************************************************************************************

#ifdef ALSA_ENABLED
static void free_alsa(extAudio *Self)
{
   if (Self->sndlog) { snd_output_close(Self->sndlog); Self->sndlog = NULL; }
   if (Self->Handle) { snd_pcm_close(Self->Handle); Self->Handle = NULL; }
   if (Self->MixHandle) { snd_mixer_close(Self->MixHandle); Self->MixHandle = NULL; }
   if (Self->AudioBuffer) { FreeResource(Self->AudioBuffer); Self->AudioBuffer = NULL; }
}
#endif

//********************************************************************************************************************

static ERROR init_audio(extAudio *Self)
{
   LONG i;

#ifdef ALSA_ENABLED

   parasol::Log log(__FUNCTION__);
   struct sndSetVolume setvol;
   snd_pcm_hw_params_t *hwparams;
   snd_pcm_stream_t stream;
   snd_ctl_t *ctlhandle;
   snd_pcm_t *pcmhandle;
   snd_mixer_elem_t *elem;
   snd_mixer_selem_id_t *sid;
   snd_pcm_uframes_t periodsize, buffersize;
   snd_ctl_card_info_t *info;
   LONG err, index, flags;
   WORD j, channel;
   long pmin, pmax;
   int dir;
   VolumeCtl *volctl;
   WORD voltotal;
   char name[32], pcm_name[32];

   if (Self->Handle) {
      log.msg("Audio system is already active.");
      return ERR_Okay;
   }

   snd_ctl_card_info_alloca(&info);

   log.msg("Initialising sound card device.");

   // If 'plughw:0,0' is used, we get ALSA's software mixer, which allows us to set any kind of output options.
   // If 'hw:0,0' is used, we get precise hardware information.  Otherwise stick to 'default'.

   if (Self->Device[0]) StrCopy(Self->Device, pcm_name, sizeof(pcm_name));
   else StrCopy("default", pcm_name, sizeof(pcm_name));

   // Convert english pcm_name to the device number

   if (StrMatch("default", pcm_name) != ERR_Okay) {
      STRING cardid, cardname;
      LONG card;

      card = -1;
      if ((snd_card_next(&card) < 0) or (card < 0)) {
         log.warning("There are no sound cards supported by audio drivers.");
         return ERR_NoSupport;
      }

      while (card >= 0) {
         snprintf(name, sizeof(name), "hw:%d", card);

         if ((err = snd_ctl_open(&ctlhandle, name, 0)) >= 0) {
            if ((err = snd_ctl_card_info(ctlhandle, info)) >= 0) {
               cardid = (STRING)snd_ctl_card_info_get_id(info);
               cardname = (STRING)snd_ctl_card_info_get_name(info);

               if (!StrMatch(cardid, pcm_name)) {
                  StrCopy(name, pcm_name, sizeof(pcm_name));
                  snd_ctl_close(ctlhandle);
                  break;
               }
            }
            snd_ctl_close(ctlhandle);
         }
         if (snd_card_next(&card) < 0) card = -1;
      }
   }

   // Check if the default ALSA device is a real sound card.  We don't want to use it if it's a modem or other
   // unexpected device.

   if (!StrMatch("default", pcm_name)) {
      snd_mixer_t *mixhandle;
      STRING cardid, cardname;
      WORD volmax;
      LONG card;

      // If there are no sound devices in the system, abort

      card = -1;
      if ((snd_card_next(&card) < 0) or (card < 0)) {
         log.warning("There are no sound cards supported by audio drivers.");
         return ERR_NoSupport;
      }

      // Check the number of mixer controls for all cards that support output.  We'll choose the card that has the most
      // mixer controls as the default.

      volmax = 0;
      while (card >= 0) {
         snprintf(name, sizeof(name), "hw:%d", card);
         log.msg("Opening card %s", name);

         if ((err = snd_ctl_open(&ctlhandle, name, 0)) >= 0) {
            if ((err = snd_ctl_card_info(ctlhandle, info)) >= 0) {

               cardid = (STRING)snd_ctl_card_info_get_id(info);
               cardname = (STRING)snd_ctl_card_info_get_name(info);

               log.msg("Identified card %s, name %s", cardid, cardname);

               if (!StrMatch("modem", cardid)) goto next_card;

               if ((err = snd_mixer_open(&mixhandle, 0)) >= 0) {
                  if ((err = snd_mixer_attach(mixhandle, name)) >= 0) {
                     if ((err = snd_mixer_selem_register(mixhandle, NULL, NULL)) >= 0) {
                        if ((err = snd_mixer_load(mixhandle)) >= 0) {
                           // Build a list of all available volume controls

                           snd_mixer_selem_id_alloca(&sid);
                           voltotal = 0;
                           for (elem=snd_mixer_first_elem(mixhandle); elem; elem=snd_mixer_elem_next(elem)) voltotal++;

                           log.msg("Card %s has %d mixer controls.", cardid, voltotal);

                           if (voltotal > volmax) {
                              volmax = voltotal;
                              StrCopy(cardid, Self->Device, sizeof(Self->Device));
                              StrCopy(name, pcm_name, sizeof(pcm_name));
                           }
                        }
                        else log.warning("snd_mixer_load() %s", snd_strerror(err));
                     }
                     else log.warning("snd_mixer_selem_register() %s", snd_strerror(err));
                  }
                  else log.warning("snd_mixer_attach() %s", snd_strerror(err));
                  snd_mixer_close(mixhandle);
               }
               else log.warning("snd_mixer_open() %s", snd_strerror(err));
            }
next_card:
            snd_ctl_close(ctlhandle);
         }
         if (snd_card_next(&card) < 0) card = -1;
      }
   }

   snd_output_stdio_attach(&Self->sndlog, stderr, 0);

   // If a mix handle is open from a previous Activate() attempt, close it

   if (Self->MixHandle) {
      snd_mixer_close(Self->MixHandle);
      Self->MixHandle = NULL;
   }

   // Mixer initialisation, for controlling volume

   if ((err = snd_mixer_open(&Self->MixHandle, 0)) < 0) {
      log.warning("snd_mixer_open() %s", snd_strerror(err));
      return ERR_Failed;
   }

   if ((err = snd_mixer_attach(Self->MixHandle, pcm_name)) < 0) {
      log.warning("snd_mixer_attach() %s", snd_strerror(err));
      return ERR_Failed;
   }

   if ((err = snd_mixer_selem_register(Self->MixHandle, NULL, NULL)) < 0) {
      log.warning("snd_mixer_selem_register() %s", snd_strerror(err));
      return ERR_Failed;
   }

   if ((err = snd_mixer_load(Self->MixHandle)) < 0) {
      log.warning("snd_mixer_load() %s", snd_strerror(err));
      return ERR_Failed;
   }

   // Build a list of all available volume controls

   snd_mixer_selem_id_alloca(&sid);
   voltotal = 0;
   for (elem=snd_mixer_first_elem(Self->MixHandle); elem; elem=snd_mixer_elem_next(elem)) voltotal++;

   log.msg("%d mixer controls have been reported by alsa.", voltotal);

   if (voltotal < 1) {
      log.warning("Aborting due to lack of mixers for the sound device.");
      return ERR_NoSupport;
   }

   if (!AllocMemory(sizeof(VolumeCtl) * (voltotal + 1), MEM_NO_CLEAR, &volctl)) {
      index = 0;
      for (elem=snd_mixer_first_elem(Self->MixHandle); elem; elem=snd_mixer_elem_next(elem)) {
         snd_mixer_selem_get_id(elem, sid);
         if (!snd_mixer_selem_is_active(elem)) continue;

         if (volctl[index].Flags & VCF_CAPTURE) {
            snd_mixer_selem_get_capture_volume_range(elem, &pmin, &pmax);
         }
         else snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);

         if (pmin >= pmax) continue; // Ignore mixers with no range

         log.trace("Mixer Control '%s',%i", snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));

         StrCopy((STRING)snd_mixer_selem_id_get_name(sid), volctl[index].Name, sizeof(volctl[index].Name));

         for (channel=0; channel < ARRAYSIZE(volctl[index].Channels); channel++) volctl[index].Channels[channel] = -1;

         flags = 0;
         if (snd_mixer_selem_has_playback_volume(elem))        flags |= VCF_PLAYBACK;
         if (snd_mixer_selem_has_capture_volume(elem))         flags |= VCF_CAPTURE;
         if (snd_mixer_selem_has_capture_volume_joined(elem))  flags |= VCF_JOINED;
         if (snd_mixer_selem_has_playback_volume_joined(elem)) flags |= VCF_JOINED;
         if (snd_mixer_selem_is_capture_mono(elem))            flags |= VCF_MONO;
         if (snd_mixer_selem_is_playback_mono(elem))           flags |= VCF_MONO;

         // Get the current channel volumes

         if (!(flags & VCF_MONO)) {
            for (channel=0; channel < ARRAYSIZE(glAlsaConvert); channel++) {
               if (snd_mixer_selem_has_playback_channel(elem, (snd_mixer_selem_channel_id_t)glAlsaConvert[channel]))   {
                  long vol;
                  snd_mixer_selem_get_playback_volume(elem, (snd_mixer_selem_channel_id_t)glAlsaConvert[channel], &vol);
                  volctl[index].Channels[channel] = vol;
               }
            }
         }
         else volctl[index].Channels[0] = 0;

         // By default, input channels need to be muted.  This is because some rare PC's have been noted to cause high
         // pitched feedback, e.g. when the microphone channel is on.  All playback channels are enabled by default.

         if ((snd_mixer_selem_has_capture_switch(elem)) and (!snd_mixer_selem_has_playback_switch(elem))) {
            for (channel=0; channel < ARRAYSIZE(glAlsaConvert); channel++) {
               flags |= VCF_MUTE;
               snd_mixer_selem_set_capture_switch(elem, (snd_mixer_selem_channel_id_t)channel, 0);
            }
         }
         else if (snd_mixer_selem_has_playback_switch(elem)) {
            for (channel=0; channel < ARRAYSIZE(glAlsaConvert); channel++) {
               snd_mixer_selem_set_capture_switch(elem, (snd_mixer_selem_channel_id_t)channel, 1);
            }
         }

         volctl[index].Flags = flags;

         index++;
      }

      volctl[index].Name[0] = 0;
      volctl[index].Flags = 0;

      log.msg("Configured %d mixer controls.", index);
   }

   snd_pcm_hw_params_alloca(&hwparams); // Stack allocation, no need to free it

   stream = SND_PCM_STREAM_PLAYBACK;
   if ((err = snd_pcm_open(&pcmhandle, pcm_name, stream, 0)) < 0) {
      log.warning("snd_pcm_open(%s) %s", pcm_name, snd_strerror(err));
      FreeResource(volctl);
      return ERR_Failed;
   }

   // Set access type, either SND_PCM_ACCESS_RW_INTERLEAVED or SND_PCM_ACCESS_RW_NONINTERLEAVED.

   if ((err = snd_pcm_hw_params_any(pcmhandle, hwparams)) < 0) {
      log.warning("Broken configuration for this PCM: no configurations available");
      FreeResource(volctl);
      return ERR_Failed;
   }

   if ((err = snd_pcm_hw_params_set_access(pcmhandle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
      log.warning("set_access() %d %s", err, snd_strerror(err));
      FreeResource(volctl);
      return ERR_Failed;
   }

   // Set the preferred audio bit format

   if (Self->BitDepth IS 16) {
      if ((err = snd_pcm_hw_params_set_format(pcmhandle, hwparams, SND_PCM_FORMAT_S16_LE)) < 0) {
         log.warning("set_format(16) %s", snd_strerror(err));
         FreeResource(volctl);
         return ERR_Failed;
      }
   }
   else if ((err = snd_pcm_hw_params_set_format(pcmhandle, hwparams, SND_PCM_FORMAT_U8)) < 0) {
      log.warning("set_format(8) %s", snd_strerror(err));
      FreeResource(volctl);
      return ERR_Failed;
   }

   // Retrieve the bit rate from alsa

   snd_pcm_format_t bitformat;
   snd_pcm_hw_params_get_format(hwparams, &bitformat);

   switch (bitformat) {
      case SND_PCM_FORMAT_S16_LE:
      case SND_PCM_FORMAT_S16_BE:
      case SND_PCM_FORMAT_U16_LE:
      case SND_PCM_FORMAT_U16_BE:
         Self->BitDepth = 16;
         break;

      case SND_PCM_FORMAT_S8:
      case SND_PCM_FORMAT_U8:
         Self->BitDepth = 8;
         break;

      default:
         log.warning("Hardware uses an unsupported audio format.");
         FreeResource(volctl);
         return ERR_Failed;
   }

   log.msg("ALSA bit rate: %d", Self->BitDepth);

   // Set the output rate to the rate that we are using internally.  ALSA will use the nearest possible rate allowed
   // by the hardware.

   dir = 0;
   if ((err = snd_pcm_hw_params_set_rate_near(pcmhandle, hwparams, (ULONG *)&Self->OutputRate, &dir)) < 0) {
      log.warning("set_rate_near() %s", snd_strerror(err));
      FreeResource(volctl);
      return ERR_Failed;
   }

   // Set number of channels

   ULONG channels = (Self->Flags & ADF_STEREO) ? 2 : 1;
   if ((err = snd_pcm_hw_params_set_channels_near(pcmhandle, hwparams, &channels)) < 0) {
      log.warning("set_channels_near(%d) %s", channels, snd_strerror(err));
      FreeResource(volctl);
      return ERR_Failed;
   }

   if (channels IS 2) Self->Stereo = true;
   else Self->Stereo = false;

#if 0
   LONG buffer_time, period_time;
   snd_pcm_uframes_t period_frames, buffer_frames;

   err = snd_pcm_hw_params_get_buffer_time_max(hwparams, &buffer_time, 0);
   if (buffer_time > 500000) buffer_time = 500000;

   period_time = buffer_time / 4;

   log.msg("Using period time of %d, buffer time %d", period_time, buffer_time);

   if ((err = snd_pcm_hw_params_set_period_time_near(handle, hwparams, &period_time, 0)) < 0) {
      log.warning("Period failure: %s", snd_strerror(err));
      FreeResource(volctl);
      return ERR_Failed;
   }

   if ((err = snd_pcm_hw_params_set_buffer_time_near(handle, hwparams, &buffer_time, 0)) < 0) {
      log.warning("Buffer size failure: %s", snd_strerror(err));
      FreeResource(volctl);
      return ERR_Failed;
   }

#else
   snd_pcm_uframes_t periodsize_min, periodsize_max;
   snd_pcm_uframes_t buffersize_min, buffersize_max;

   snd_pcm_hw_params_get_buffer_size_min(hwparams, &buffersize_min);
   snd_pcm_hw_params_get_buffer_size_max(hwparams, &buffersize_max);

   dir = 0;
   snd_pcm_hw_params_get_period_size_min(hwparams, &periodsize_min, &dir);

   dir = 0;
   snd_pcm_hw_params_get_period_size_max(hwparams, &periodsize_max, &dir);

   // NOTE: Audio buffersize is measured in samples, not bytes

   if (!Self->AudioBufferSize) buffersize = DEFAULT_BUFFER_SIZE;
   else buffersize = Self->AudioBufferSize;

   if (buffersize > buffersize_max) buffersize = buffersize_max;
   else if (buffersize < buffersize_min) buffersize = buffersize_min;

   periodsize = buffersize / 4;
   if (periodsize < periodsize_min) periodsize = periodsize_min;
   else if (periodsize > periodsize_max) periodsize = periodsize_max;
   buffersize = periodsize * 4;

   // Set buffer sizes.  Note that we will retrieve the period and buffer sizes AFTER telling ALSA what the audio
   // parameters are.

   log.msg("Using period frame size of %d, buffer size of %d", (LONG)periodsize, (LONG)buffersize);

   if ((err = snd_pcm_hw_params_set_period_size_near(pcmhandle, hwparams, &periodsize, 0)) < 0) {
      log.warning("Period size failure: %s", snd_strerror(err));
      FreeResource(volctl);
      return ERR_Failed;
   }

   if ((err = snd_pcm_hw_params_set_buffer_size_near(pcmhandle, hwparams, &buffersize)) < 0) {
      log.warning("Buffer size failure: %s", snd_strerror(err));
      FreeResource(volctl);
      return ERR_Failed;
   }
#endif

   // ALSA device initialisation

   if ((err = snd_pcm_hw_params(pcmhandle, hwparams)) < 0) {
      log.warning("snd_pcm_hw_params() %s", snd_strerror(err));
      FreeResource(volctl);
      return ERR_Failed;
   }

   if ((err = snd_pcm_prepare(pcmhandle)) < 0) {
      log.warning("snd_pcm_prepare() %s", snd_strerror(err));
      FreeResource(volctl);
      return ERR_Failed;
   }

   // Retrieve ALSA buffer sizes

   err = snd_pcm_hw_params_get_periods(hwparams, (ULONG *)&Self->Periods, &dir);

   snd_pcm_hw_params_get_period_size(hwparams, &periodsize, 0);
   Self->PeriodSize = periodsize;

   // Note that ALSA reports the audio buffer size in samples, not bytes

   snd_pcm_hw_params_get_buffer_size(hwparams, &buffersize);
   Self->AudioBufferSize = buffersize;

   if (Self->Stereo) Self->AudioBufferSize = Self->AudioBufferSize<<1;
   if (Self->BitDepth IS 16) Self->AudioBufferSize = Self->AudioBufferSize<<1;

   log.msg("Total Periods: %d, Period Size: %d, Buffer Size: %d (bytes)", Self->Periods, Self->PeriodSize, Self->AudioBufferSize);

   // Allocate a buffer that we will use for audio output

   if (Self->AudioBuffer) { FreeResource(Self->AudioBuffer); Self->AudioBuffer = NULL; }

   if (!AllocMemory(Self->AudioBufferSize, MEM_DATA, &Self->AudioBuffer)) {
      VolumeCtl *oldctl;

      #ifdef DEBUG
         snd_pcm_hw_params_dump(hwparams, log);
      #endif

      // Apply existing volumes to the alsa mixer if we're system-wide

      oldctl = Self->VolumeCtl;

      Self->VolumeCtlTotal = voltotal;
      Self->VolumeCtl      = volctl;

      if ((oldctl) and (Self->Flags & ADF_SYSTEM_WIDE)) {
         log.msg("Applying preset volumes to alsa.");

         for (i=0; volctl[i].Name[0]; i++) {
            for (j=0; oldctl[j].Name[0]; j++) {
               if (!StrMatch(volctl[i].Name, oldctl[j].Name)) {
                  setvol.Index   = i;
                  setvol.Name    = NULL;
                  setvol.Flags   = 0;
                  setvol.Volume  = oldctl[j].Channels[0];
                  if (oldctl[j].Flags & VCF_MUTE) setvol.Flags |= SVF_MUTE;
                  else setvol.Flags |= SVF_UNMUTE;
                  Action(MT_SndSetVolume, Self, &setvol);
                  break;
               }
            }

            // If the user has not defined a default for the mixer, set our own default.

            if (!oldctl[j].Name[0]) {
               setvol.Index   = i;
               setvol.Name    = NULL;
               setvol.Flags   = 0;
               setvol.Volume  = 0.8;
               Action(MT_SndSetVolume, Self, &setvol);
            }
         }
      }
      else log.msg("Skipping preset volumes.");

      // Free existing volume measurements and apply the information that we read from alsa.

      if (oldctl) FreeResource(oldctl);
      Self->Handle = pcmhandle;
   }
   else {
      FreeResource(volctl);
      return log.warning(ERR_AllocMemory);
   }

#else

   Self->BitDepth = 16;
   Self->Stereo = true;
   Self->MasterVolume = Self->VolumeCtl[0].Channels[0];
   Self->VolumeCtl[0].Flags |= VCF_MONO;
   for (i=1; i < ARRAYSIZE(Self->VolumeCtl[0].Channels); i++) Self->VolumeCtl[0].Channels[i] = -1;
   if (Self->VolumeCtl[0].Flags & VCF_MUTE) Self->Mute = true;
   else Self->Mute = false;

#endif

   // Save the audio settings to disk post-initialisation

   acSaveSettings(Self);
   return ERR_Okay;
}

//********************************************************************************************************************

#include "audio_def.c"

static const FieldArray clAudioFields[] = {
   { "Bass",          FDF_DOUBLE|FDF_RW,  0, NULL, NULL },
   { "Treble",        FDF_DOUBLE|FDF_RW,  0, NULL, NULL },
   { "OutputRate",    FDF_LONG|FDF_RI,    0, NULL, (APTR)SET_OutputRate },
   { "InputRate",     FDF_LONG|FDF_RI,    0, NULL, NULL },
   { "Quality",       FDF_LONG|FDF_RW,    0, NULL, (APTR)SET_Quality },
   { "Flags",         FDF_LONGFLAGS|FDF_RI, (MAXINT)&clAudioFlags, NULL, NULL },
   { "TotalChannels", FDF_LONG|FDF_R,     0, NULL, NULL },
   { "BitDepth",      FDF_LONG|FDF_RI,    0, NULL, (APTR)SET_BitDepth },
   { "Periods",       FDF_LONG|FDF_RI,    0, NULL, (APTR)SET_Periods },
   { "PeriodSize",    FDF_LONG|FDF_RI,    0, NULL, (APTR)SET_PeriodSize },
   // VIRTUAL FIELDS
   { "Device",        FDF_STRING|FDF_RW,  0, (APTR)GET_Device,       (APTR)SET_Device },
   { "MasterVolume",  FDF_DOUBLE|FDF_RW,  0, (APTR)GET_MasterVolume, (APTR)SET_MasterVolume },
   { "Mute",          FDF_LONG|FDF_RW,    0, (APTR)GET_Mute,         (APTR)SET_Mute },
   { "Stereo",        FDF_LONG|FDF_RW,    0, (APTR)GET_Stereo,       (APTR)SET_Stereo },
   { "VolumeCtl",     FDF_POINTER|FDF_R,  0, (APTR)GET_VolumeCtl,    NULL },
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
