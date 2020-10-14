/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
Sound: Plays and records sound samples in a variety of different data formats.

The Sound class provides a simple interface for any program to load and play audio sample files. By default all
loading and saving of sound data is in WAVE format.  Other audio formats can be supported through Sound class
extensions, if available.

Smart, transparent streaming is enabled by default.  If an attempt is made to play an audio file that is
considerably large (relative to system resources), it will be streamed from the source location.  You can alter or
force streaming behaviour through the #Stream field.

The following example illustrates playback of a sound sample one octave higher than its normal frequency.  The
subscription to the Deactivate action will result in the program closing once the sample has finished playback.

<pre>
local snd = obj.new('sound', { path='audio:samples/doorbell.wav', note='C6' })

snd.subscribe("deactivate", function(SoundID)
   mSys.SendMessage(0, MSGID_QUIT)
end)

snd.acActivate()
</pre>

-END-

*****************************************************************************/

struct PlatformData { void *Void; };

#include "windows.h"

static ERROR SOUND_GET_Active(objSound *, LONG *);
static ERROR SOUND_GET_Position(objSound *, LONG *);

static ERROR SOUND_SET_Note(objSound *, CSTRING);
static ERROR SOUND_SET_Pan(objSound *, DOUBLE);
static ERROR SOUND_SET_Playback(objSound *, LONG);
static ERROR SOUND_SET_Position(objSound *, LONG);
static ERROR SOUND_SET_Volume(objSound *, DOUBLE);

static const DOUBLE glScale[NOTE_B+1] = {
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

static const struct ActionArray clActions[];
static const struct FieldArray clFields[];
static const struct FieldDef clStream[];
static const struct FieldDef clFlags[];

static OBJECTPTR clSound = NULL;

static ERROR find_chunk(objSound *, OBJECTPTR, CSTRING);
static ERROR playback_timer(objSound *Self, LARGE Elapsed, LARGE CurrentTime);

#undef GetChannel
#define GetChannel(a,b) &(a)->Channels[(b)>>16].Channel[(b) & 0xffff];

#define KEY_SOUNDCHANNELS 0x3389f93

ERROR add_sound_class(void)
{
   return CreateObject(ID_METACLASS, 0, &clSound,
      FID_BaseClassID|TLONG,    ID_SOUND,
      FID_ClassVersion|TDOUBLE, VER_SOUND,
      FID_FileExtension|TSTR,   "*.wav|*.wave|*.snd",
      FID_FileDescription|TSTR, "Sound Sample",
      FID_FileHeader|TSTR,      "[0:$52494646][8:$57415645]",
      FID_Name|TSTRING,         "Sound",
      FID_Category|TLONG,       CCF_AUDIO,
      FID_Actions|TPTR,         clActions,
      FID_Fields|TARRAY,        clFields,
      FID_Size|TLONG,           sizeof(objSound),
      FID_Path|TSTR,            MOD_PATH,
      TAGEND);
}

void free_sound_class(void)
{
   if (clSound) { acFree(clSound); clSound = 0; }
}

//****************************************************************************
// Stubs.

static LONG ReadLong(OBJECTPTR File)
{
   struct acRead args;
   LONG value;
   args.Buffer = (APTR)&value;
   args.Length = 4;
   Action(AC_Read, File, &args);
   return value;
}

#ifndef _WIN32
static LONG SampleFormat(objSound *Self)
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
#endif

//****************************************************************************

static ERROR SOUND_ActionNotify(objSound *Self, struct acActionNotify *Args)
{
   if (Args->ActionID IS AC_Read) {
      // Streams: When the Audio system calls the Read action, we need to decode more audio information to the stream
      // buffer.

      NotifySubscribers(Self, AC_Read, Args->Args, 0, ERR_Okay);
   }
   else if (Args->ActionID IS AC_Seek) {
      // Streams: If the Audio system calls the Seek action, we need to move our current decode position to the
      // requested area.

      NotifySubscribers(Self, AC_Seek, Args->Args, 0, ERR_Okay);
   }
   else LogMsg("Unrecognised action #%d.", Args->ActionID);

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Activate: Plays the audio sample.
-END-
*****************************************************************************/

static ERROR SOUND_Activate(objSound *Self, APTR Void)
{
   FMSG("~","");

#ifdef _WIN32

   if (Self->prvWAVE) {
      // Set platform dependent playback parameters

      SOUND_SET_Playback(Self, Self->Playback);
      SOUND_SET_Volume(Self, Self->Volume);
      SOUND_SET_Pan(Self, Self->Pan);

      // If streaming is enabled, subscribe to the system timer so that we can regularly fill the audio buffer.
      // 1/4 second checks are fine since we are only going to fill the buffer every 1.5 seconds or more (give
      // consideration to high octave playback).
      //
      // We also need the subscription to fulfil the Deactivate contract.

      FUNCTION callback;
      SET_FUNCTION_STDC(callback, &playback_timer);
      SubscribeTimer(0.25, &callback, &Self->Timer);

      // Play the audio buffer

      if (Self->Flags & SDF_LOOP) sndPlay((struct PlatformData *)Self->prvPlatformData, TRUE, Self->Position);
      else sndPlay((struct PlatformData *)Self->prvPlatformData, FALSE, Self->Position);

      STEP();
      return ERR_Okay;
   }
   else LogMsg("A independent win32 waveform will not be used for this sample.");

#endif

   LONG i;
   objAudio *audio;
   if (!AccessObject(Self->AudioID, 2000, &audio)) {
      // Restricted and streaming audio can only be played on one channel at any given time.  This search will check
      // if the sound object is already active on one of our channels.

      struct AudioChannel *channel = NULL;
      if (Self->Flags & (SDF_RESTRICT_PLAY|SDF_STREAM)) {
         Self->ChannelIndex &= 0xffff0000;
         for (i=0; i < glMaxSoundChannels; i++) {
            channel = GetChannel(audio, Self->ChannelIndex);
            if ((channel) AND (channel->SoundID IS Self->Head.UniqueID)) break;
            Self->ChannelIndex++;
         }
         if (i >= glMaxSoundChannels) channel = NULL;
      }

      if (!channel) {
         // Find an available channel.  If all channels are in use, check the priorities to see if we can push anyone out.
         struct AudioChannel *priority = NULL;
         Self->ChannelIndex &= 0xffff0000;
         for (i=0; i < glMaxSoundChannels; i++) {
            channel = GetChannel(audio, Self->ChannelIndex);
            if (channel) {
               if ((channel->State IS CHS_STOPPED) OR (channel->State IS CHS_FINISHED)) break;
               else if (channel->Priority < Self->Priority) priority = channel;
            }
            Self->ChannelIndex++;
         }

         if (i >= glMaxSoundChannels) {
            if (!(channel = priority)) {
               LogMsg("Audio channel not available for playback.");
               ReleaseObject(audio);
               STEP();
               return ERR_Failed;
            }
         }
      }

      COMMAND_Stop(audio, Self->ChannelIndex);

      if (!COMMAND_SetSample(audio, Self->ChannelIndex, Self->Handle)) {
         struct AudioChannel *channel = GetChannel(audio, Self->ChannelIndex);
         channel->SoundID = Self->Head.UniqueID; // Record our object ID against the channel

         COMMAND_SetVolume(audio, Self->ChannelIndex, Self->Volume * 3.0);
         COMMAND_SetPan(audio, Self->ChannelIndex, Self->Pan);

         ReleaseObject(audio);

         // The Play command must be messaged to the audio object because it needs to be executed by the task that owns
         // the audio memory.

         struct sndBufferCommand command;
         command.Command = CMD_PLAY;
         command.Handle  = Self->ChannelIndex;
         command.Data    = Self->Playback;
         if (!ActionMsg(MT_SndBufferCommand, Self->AudioID, &command)) {
            // If streaming is enabled, subscribe this sound to the system timer so that we can regularly fill the
            // audio buffer.  1/4 second checks are fine since we are only going to fill the buffer every 1.5 seconds
            // or more (give consideration to high octave playback).
            //
            // We also need the subscription to fulfil the Deactivate contract.

            FUNCTION callback;
            SET_FUNCTION_STDC(callback, &playback_timer);
            SubscribeTimer(0.25, NULL, &Self->Timer);

            STEP();
            return ERR_Okay;
         }
         else {
            PostError(ERR_Failed);
            STEP();
            return ERR_Failed;
         }
      }
      else {
         LogErrorMsg("Failed to set sample %d to channel $%.8x", Self->Handle, Self->ChannelIndex);
         ReleaseObject(audio);
         PostError(ERR_Failed);
         STEP();
         return ERR_Failed;
      }
   }
   else {
      PostError(ERR_AccessObject);
      STEP();
      return ERR_AccessObject;
   }
}

/*****************************************************************************
-ACTION-
Deactivate: Stops the audio sample and resets the playback position.
-END-
*****************************************************************************/

static ERROR SOUND_Deactivate(objSound *Self, APTR Void)
{
   LogAction(NULL);

   if (Self->Timer) { UpdateTimer(Self->Timer, 0); Self->Timer = 0; }

   Self->Position = 0;

#ifdef _WIN32
   if (!Self->Handle) {
      sndStop((struct PlatformData *)Self->prvPlatformData);
      return ERR_Okay;
   }
#endif

   objAudio *audio;
   if (!AccessObject(Self->AudioID, 3000, &audio)) {
      // Get a reference to our sound channel, then check if our unique ID is set against it.  If so, our sound is
      // currently playing and we must send a stop signal to the audio system.

      struct AudioChannel *channel = GetChannel(audio, Self->ChannelIndex);

      if (channel) {
         if (channel->SoundID IS Self->Head.UniqueID) {
            COMMAND_Stop(audio, Self->ChannelIndex);
         }
      }

      ReleaseObject(audio);
   }
   else return PostError(ERR_AccessObject);

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Disable: Disable playback of an active audio sample.
-END-
*****************************************************************************/

static ERROR SOUND_Disable(objSound *Self, APTR Void)
{
   LogBranch(NULL);

#ifdef _WIN32

   if (!Self->Handle) {
      Self->Position = sndGetPosition((struct PlatformData *)Self->prvPlatformData);
      LogMsg("Position: %d", Self->Position);
      sndStop((struct PlatformData *)Self->prvPlatformData);
      return ERR_Okay;
   }

#endif

   if (!Self->ChannelIndex) {
      LogBack();
      return ERR_Okay;
   }

   objAudio *audio;
   if (!AccessObject(Self->AudioID, 5000, &audio)) {
      struct AudioChannel *channel = GetChannel(audio, Self->ChannelIndex);
      if ((channel) AND (channel->SoundID IS Self->Head.UniqueID)) {
         COMMAND_Stop(audio, Self->ChannelIndex);
      }
      ReleaseObject(audio);
      LogBack();
      return ERR_Okay;
   }
   else return LogBackError(0, ERR_AccessObject);
}

/*****************************************************************************
-ACTION-
Enable: Continues playing a sound if it has been disabled.
-END-
*****************************************************************************/

static ERROR SOUND_Enable(objSound *Self, APTR Void)
{
   LogBranch(NULL);

#ifdef _WIN32

   if (!Self->Handle) {
      LogMsg("Playing back from position %d.", Self->Position);
      if (Self->Flags & SDF_LOOP) sndPlay((struct PlatformData *)Self->prvPlatformData, TRUE, Self->Position);
      else sndPlay((struct PlatformData *)Self->prvPlatformData, FALSE, Self->Position);
      return ERR_Okay;
   }

#endif

   if (!Self->ChannelIndex) {
      LogBack();
      return ERR_Okay;
   }

   objAudio *audio;
   if (!AccessObject(Self->AudioID, 5000, &audio)) {
      struct AudioChannel *channel = GetChannel(audio, Self->ChannelIndex);
      if ((channel) AND (channel->SoundID IS Self->Head.UniqueID)) {
         COMMAND_Continue(audio, Self->ChannelIndex);
      }
      ReleaseObject(audio);
      LogBack();
      return ERR_Okay;
   }
   else return LogBackError(0, ERR_AccessObject);
}

//****************************************************************************

static ERROR SOUND_Free(objSound *Self, APTR Void)
{
   if (Self->Fields) { FreeResource(Self->Fields); Self->Fields = NULL; }

   if (Self->Flags & SDF_STREAM) {
      if (Self->Timer) { UpdateTimer(Self->Timer, 0); Self->Timer = 0; }
   }

#ifdef _WIN32
   if (!Self->Handle) sndFree((struct PlatformData *)Self->prvPlatformData);
#endif

   acDeactivate(Self);

   if (Self->Handle) {
      struct sndRemoveSample remove = { Self->Handle };
      ActionMsg(MT_SndRemoveSample, Self->AudioID, &remove);
      Self->Handle = 0;
   }

   if (Self->ChannelIndex)   { sndCloseChannelsID(Self->AudioID, Self->ChannelIndex); Self->ChannelIndex = 0; }
   if (Self->prvPath)        { FreeResource(Self->prvPath); Self->prvPath = NULL; }
   if (Self->prvDescription) { FreeResource(Self->prvDescription); Self->prvDescription = NULL; }
   if (Self->prvDisclaimer)  { FreeResource(Self->prvDisclaimer); Self->prvDisclaimer = NULL; }
   if (Self->prvWAVE)        { FreeResource(Self->prvWAVE); Self->prvWAVE = NULL; }
   if (Self->File)           { acFree(Self->File); Self->File = NULL; }
   if (Self->StreamFileID)   { acFreeID(Self->StreamFileID); Self->StreamFileID = 0; }

   return ERR_Okay;
}

/*****************************************************************************
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
</types>

*****************************************************************************/

static ERROR SOUND_GetVar(objSound *Self, struct acGetVar *Args)
{
   if ((!Args) OR (!Args->Field)) return ERR_NullArgs;

   CSTRING val;
   if ((val = VarGetString(Self->Fields, Args->Field))) {
      StrCopy(val, Args->Buffer, Args->Size);
      return ERR_Okay;
   }
   else return ERR_UnsupportedField;
}

/*****************************************************************************
-ACTION-
Init: Prepares a sound object for usage.
-END-
*****************************************************************************/

#define WAVE_RAW    0x0001    // Uncompressed waveform data.
#define WAVE_ADPCM  0x0002    // ADPCM compressed waveform data.

#define SECONDS_STREAM_BUFFER 3

#define SIZE_RIFF_CHUNK 12

#ifdef _WIN32

static ERROR SOUND_Init(objSound *Self, APTR Void)
{
   struct acRead read;
   LONG id, len;
   STRING path, strerr;
   OBJECTPTR audio;
   ERROR error;

   // Find the local audio object.  If none is available, create a new audio object to ease the developer's pain.

   if (!Self->AudioID) {
      if (FastFindObject("SystemAudio", ID_AUDIO, &Self->AudioID, 1, NULL) != ERR_Okay) {
         if (!(error = NewNamedObject(ID_AUDIO, NF_PUBLIC|NF_UNIQUE, &audio, &Self->AudioID, "SystemAudio"))) {
            SetOwner(audio, CurrentTask());

            if (acInit(audio) != ERR_Okay) {
               acFree(audio);
               ReleaseObject(audio);
               if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
               return PostError(ERR_Init);
            }

            acActivate(audio);

            ReleaseObject(audio);
         }
         else if (error != ERR_ObjectExists) {
            if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
            return PostError(ERR_NewObject);
         }
      }
   }

   // Open channels for sound sample playback.  Note that audio channels must be allocated 'locally' so that they
   // can be tracked back to our task.

   if (sndOpenChannelsID(Self->AudioID, glMaxSoundChannels, KEY_SOUNDCHANNELS + CurrentTaskID(), 0, &Self->ChannelIndex) != ERR_Okay) {
      LogErrorMsg("Failed to open channels from Audio device.");
      if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
      return ERR_Failed;
   }

   if ((Self->Flags & SDF_NEW) OR (GetString(Self, FID_Path, &path) != ERR_Okay) OR (!path)) {

      // If the sample is new or no path has been specified, create an audio sample from scratch (e.g. to record
      // audio to disk).

      return ERR_Okay;
   }

   // Load the sound file's header and test it to see if it matches our supported file format.

   if (!CreateObject(ID_FILE, NF_INTEGRAL, &Self->File,
         FID_Path|TSTR,   path,
         FID_Flags|TLONG, FL_READ|FL_APPROXIMATE,
         TAGEND)) {

      read.Buffer = Self->prvHeader;
      read.Length = sizeof(Self->prvHeader);
      Action(AC_Read, Self->File, &read);

      if ((StrCompare(Self->prvHeader, "RIFF", 4, STR_CASE) != ERR_Okay) OR
          (StrCompare(Self->prvHeader + 8, "WAVE", 4, STR_CASE) != ERR_Okay)) {
         if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
         return ERR_NoSupport;
      }
   }
   else {
      if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
      return PostError(ERR_File);
   }

   // Read the RIFF header

   acSeek(Self->File, 12.0, SEEK_START);
   id  = ReadLong(Self->File); // Contains the characters "fmt "
   len = ReadLong(Self->File); // Length of data in this chunk

   if (!AllocMemory(len, MEM_DATA, &Self->prvWAVE, NULL)) {
      read.Buffer = (BYTE *)Self->prvWAVE;
      read.Length = len;
      if ((Action(AC_Read, Self->File, &read) != ERR_Okay) OR (read.Result < len)) {
         if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
         return PostError(ERR_Read);
      }
   }
   else {
      if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
      return ERR_AllocMemory;
   }

   // Check the format of the sound file's data

   if ((Self->prvWAVE->Format != WAVE_ADPCM) AND (Self->prvWAVE->Format != WAVE_RAW)) {
      LogMsg("This file's WAVE data format is not supported (type %d).", Self->prvWAVE->Format);
      if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
      return ERR_InvalidData;
   }

   // Look for the "data" chunk

   if (find_chunk(Self, Self->File, "data") != ERR_Okay) {
      if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
      return PostError(ERR_Read);
   }

   Self->Length = ReadLong(Self->File); // Length of audio data in this chunk

   if (Self->Length & 1) Self->Length++;

   // Setup the sound structure

   GetLong(Self->File, FID_Position, &Self->prvDataOffset);

   Self->prvFormat      = Self->prvWAVE->Format;
   Self->BytesPerSecond = Self->prvWAVE->AvgBytesPerSecond;
   Self->prvAlignment   = Self->prvWAVE->BlockAlign;
   Self->BitsPerSample  = Self->prvWAVE->BitsPerSample;
   if (Self->prvWAVE->Channels IS 2) Self->Flags |= SDF_STEREO;
   if (Self->Frequency <= 0) Self->Frequency = Self->prvWAVE->Frequency;
   if (Self->Playback <= 0)  Self->Playback  = Self->Frequency;

   if (Self->Flags & SDF_NOTE) {
      SetLong(Self, FID_Note, Self->prvNote);
      Self->Flags &= ~SDF_NOTE;
   }

   // If the QUERY flag is set, do not proceed any further as we already have all of the information that we need.

   if (Self->Flags & SDF_QUERY) return ERR_Okay;

   // Determine if we are going to use streaming to play this sample

   if (!Self->BufferLength) {
      if ((Self->Stream IS STREAM_ALWAYS) AND (Self->Length >= 65536)) {
         Self->BufferLength = Self->BytesPerSecond * SECONDS_STREAM_BUFFER;
      }
      else if ((Self->Stream IS STREAM_SMART) AND (Self->Length > 524288)) {
         Self->BufferLength = Self->BytesPerSecond * SECONDS_STREAM_BUFFER;
      }
      else Self->BufferLength = Self->Length;
   }

   if (Self->BufferLength > Self->Length) Self->BufferLength = Self->Length;

   MSG("Bits: %d, Freq: %d, KBPS: %d, BufLen: %d, SmpLen: %d", Self->BitsPerSample, Self->Frequency, Self->BytesPerSecond, Self->BufferLength, Self->Length);

   // Create the audio buffer and fill it with sample data

   if (Self->Length > Self->BufferLength) {
      LogMsg("Streaming enabled for playback.");
      Self->Flags |= SDF_STREAM;
      strerr = sndCreateBuffer(Self, Self->prvWAVE, Self->BufferLength, Self->Length, (struct PlatformData *)Self->prvPlatformData, TRUE);
   }
   else {
      Self->BufferLength = Self->Length;
      strerr = sndCreateBuffer(Self, Self->prvWAVE, Self->BufferLength, Self->Length, (struct PlatformData *)Self->prvPlatformData, FALSE);
   }

   if (strerr) {
      LogErrorMsg("Failed to create audio buffer, reason: %s (buffer length %d, sample length %d)", strerr, Self->BufferLength, Self->Length);
      if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
      return ERR_Failed;
   }

   return ERR_Okay;
}

#else

static ERROR SOUND_Init(objSound *Self, APTR Void)
{
   struct sndAddSample add;
   struct AudioLoop loop;
   OBJECTPTR audio, filestream;
   LONG id, len, sampleformat, result, pos;
   BYTE *buffer;
   ERROR error;

   if (!Self->AudioID) {
      if (FastFindObject("SystemAudio", ID_AUDIO, &Self->AudioID, 1, NULL) != ERR_Okay) {
         if (!(error = NewNamedObject(ID_AUDIO, NF_PUBLIC|NF_UNIQUE, &audio, &Self->AudioID, "SystemAudio"))) {
            SetOwner(audio, CurrentTask());

            if (acInit(audio) != ERR_Okay) {
               acFree(audio);
               ReleaseObject(audio);
               if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
               return PostError(ERR_Init);
            }

            acActivate(audio);

            ReleaseObject(audio);
         }
         else if (error != ERR_ObjectExists) {
            if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
            return PostError(ERR_NewObject);
         }
      }
   }

   // Open channels for sound sample playback.  Note that audio channels must be allocated 'locally' so that they
   // can be tracked back to our task.

   if (!AccessObject(Self->AudioID, 3000, &audio)) {
      error = sndOpenChannels(audio, glMaxSoundChannels, KEY_SOUNDCHANNELS + CurrentTaskID(), 0, &Self->ChannelIndex);
      ReleaseObject(audio);
   }
   else error = ERR_AccessObject;

   if (error) {
      LogErrorMsg("Failed to open channels from Audio device.");
      if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
      return ERR_Failed;
   }

   STRING path = NULL;
   GetString(Self, FID_Path, &path);

   // Set the initial sound title

   if (path) {
      for (len=0; path[len]; len++);
      while ((len > 0) AND (path[len-1] != '/') AND (path[len-1] != ':')) len--;
      acSetVar(Self, "Title", path+len);
   }

   if (Self->Length IS -1) {
      // Enable continuous audio streaming mode

      LogMsg("Enabling continuous audio streaming mode.");

      Self->Stream = STREAM_ALWAYS;
      if (Self->BufferLength <= 0) Self->BufferLength = 32768;
      else if (Self->BufferLength < 256) Self->BufferLength = 256;

      if (!Self->Frequency) Self->Frequency = 44192;
      if (!Self->Playback) Self->Playback = Self->Frequency;

      // Create a public file object that will handle the decoded audio stream

      if (NewLockedObject(ID_FILE, NF_PUBLIC, &filestream, &Self->StreamFileID)) {
         SetFields(filestream,
            FID_Flags|TLONG, FL_BUFFER|FL_LOOP,
            FID_Size|TLONG,  Self->BufferLength,
            TAGEND);

         if (!acInit(filestream)) {
            // Subscribe to the virtual file, so that we can detect when the audio system reads information from it.

            SubscribeActionTags(filestream, AC_Read, AC_Seek, TAGEND);

            error = ERR_Okay;
         }
         else error = ERR_Init;

         if (error) { acFree(filestream); Self->StreamFileID = 0; }

         ReleaseObject(filestream);
      }
      else error = ERR_NewObject;

      if ((error) AND (Self->Flags & SDF_TERMINATE)) {
         DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
         return error;
      }

      // Create the audio stream and activate it

      struct sndAddStream stream = {
         .Loop         = 0,
         .LoopSize     = 0,
         .Path         = NULL,
         .ObjectID     = Self->StreamFileID,
         .SeekStart    = 0,
         .SampleFormat = SampleFormat(Self),
         .SampleLength = -1,
         .BufferLength = Self->BufferLength
      };

      if (WaitMsg(MT_SndAddStream, Self->AudioID, &stream) != ERR_Okay) {
         LogErrorMsg("Failed to add sample to the Audio device.");
         if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
         return ERR_Failed;
      }

      Self->Handle = stream.Result;

      return ERR_Okay;
   }
   else {

      if ((Self->Flags & SDF_NEW) OR (!path)) {
         LogMsg("Sample created as new (without sample data).");

         // If the sample is new or no path has been specified, create an audio sample from scratch (e.g. to
         // record audio to disk).

         return ERR_Okay;
      }

      // Load the sound file's header and test it to see if it matches our supported file format.

      if (!CreateObject(ID_FILE, NF_INTEGRAL, &Self->File,
            FID_Path|TSTR,   path,
            FID_Flags|TLONG, FL_READ|FL_APPROXIMATE,
            TAGEND)) {

         if (!acRead(Self->File, Self->prvHeader, sizeof(Self->prvHeader), NULL)) {
            if ((StrCompare(Self->prvHeader, "RIFF", 4, STR_CASE) != ERR_Okay) OR
                (StrCompare(Self->prvHeader + 8, "WAVE", 4, STR_CASE) != ERR_Okay)) {
               if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
               return ERR_NoSupport;
            }
         }
         else {
            LogErrorMsg("Failed to read file header.");
            return ERR_Read;
         }
      }
      else {
         if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
         return PostError(ERR_File);
      }

      // Read the FMT header

      acSeek(Self->File, 12, SEEK_START);
      id  = ReadLong(Self->File); // Contains the characters "fmt "
      len = ReadLong(Self->File); // Length of data in this chunk

      if (!AllocMemory(len, MEM_DATA, &Self->prvWAVE, NULL)) {
         if ((acRead(Self->File, Self->prvWAVE, len, &result) != ERR_Okay) OR (result < len)) {
            if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
            LogErrorMsg("Failed to read WAVE format header (got %d, expected %d)", result, len);
            return ERR_Read;
         }
      }
      else {
         if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
         return PostError(ERR_AllocMemory);
      }

      // Check the format of the sound file's data

      if ((Self->prvWAVE->Format != WAVE_ADPCM) AND (Self->prvWAVE->Format != WAVE_RAW)) {
         LogErrorMsg("This file's WAVE data format is not supported (type %d).", Self->prvWAVE->Format);
         if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
         return ERR_InvalidData;
      }

      // Look for the cue chunk for loop information

      GetLong(Self->File, FID_Position, &pos);
#if 0
      if (find_chunk(Self, Self->File, "cue ") IS ERR_Okay) {
         data_p += 32;
         info.loopstart = ReadLong();
         // if the next chunk is a LIST chunk, look for a cue length marker
         if (find_chunk(Self, Self->File, "LIST") IS ERR_Okay) {
            if (!strncmp (data_p + 28, "mark", 4)) {
               data_p += 24;
               i = ReadLong();	// samples in loop
               info.samples = info.loopstart + i;
            }
         }
      }
#endif
      acSeek(Self->File, pos, SEEK_START);

      // Look for the "data" chunk

      if (find_chunk(Self, Self->File, "data") != ERR_Okay) {
         if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
         return PostError(ERR_Read);
      }

      // Setup the sound structure

      Self->Length = ReadLong(Self->File); // Length of audio data in this chunk

      GetLong(Self->File, FID_Position, &Self->prvDataOffset);

      Self->prvFormat      = Self->prvWAVE->Format;
      Self->BytesPerSecond = Self->prvWAVE->AvgBytesPerSecond;
      Self->prvAlignment   = Self->prvWAVE->BlockAlign;
      Self->BitsPerSample  = Self->prvWAVE->BitsPerSample;
      if (Self->prvWAVE->Channels IS 2) Self->Flags |= SDF_STEREO;
      if (Self->Frequency <= 0) Self->Frequency = Self->prvWAVE->Frequency;
      if (Self->Playback <= 0)  Self->Playback  = Self->Frequency;

      if (Self->Flags & SDF_NOTE) {
         SOUND_SET_Note(Self, Self->prvNoteString);
         Self->Flags &= ~SDF_NOTE;
      }

      if ((Self->BitsPerSample != 8) AND (Self->BitsPerSample != 16)) {
         LogErrorMsg("Bits-Per-Sample of %d not supported.", Self->BitsPerSample);
         if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
         return ERR_InvalidData;
      }

      // If the QUERY flag is set, do not proceed any further as we already have all of the information that we need.

      if (Self->Flags & SDF_QUERY) return ERR_Okay;

      // Determine the sample type

      sampleformat = 0;
      if ((Self->prvWAVE->Channels IS 1) AND (Self->BitsPerSample IS 8)) sampleformat = SFM_U8_BIT_MONO;
      else if ((Self->prvWAVE->Channels IS 2) AND (Self->BitsPerSample IS 8))  sampleformat = SFM_U8_BIT_STEREO;
      else if ((Self->prvWAVE->Channels IS 1) AND (Self->BitsPerSample IS 16)) sampleformat = SFM_S16_BIT_MONO;
      else if ((Self->prvWAVE->Channels IS 2) AND (Self->BitsPerSample IS 16)) sampleformat = SFM_S16_BIT_STEREO;

      if (!sampleformat) {
         if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
         return PostError(ERR_InvalidData);
      }

      // Determine if we are going to use streaming to play this sample

      if (!Self->BufferLength) {
         if ((Self->Stream IS STREAM_ALWAYS) AND (Self->Length > 32768)) {
            Self->BufferLength = Self->BytesPerSecond * SECONDS_STREAM_BUFFER;
         }
         else if ((Self->Stream IS STREAM_SMART) AND (Self->Length > 524288)) {
            Self->BufferLength = Self->BytesPerSecond * SECONDS_STREAM_BUFFER;
         }
         else Self->BufferLength = Self->Length;
      }

      if (Self->BufferLength > Self->Length) Self->BufferLength = Self->Length;

      // Create the audio buffer and fill it with sample data

      if ((Self->Length > Self->BufferLength) OR (Self->Flags & SDF_STREAM)) {
         LogMsg("Streaming enabled for playback.");
         Self->Flags |= SDF_STREAM;

         struct sndAddStream stream;
         if (Self->Flags & SDF_LOOP) {
            ClearMemory(&loop, sizeof(struct AudioSample));
            loop.LoopMode   = LOOP_SINGLE;
            loop.Loop1Type  = LTYPE_UNIDIRECTIONAL;
            loop.Loop1Start = Self->LoopStart;
            if (Self->LoopEnd) loop.Loop1End = Self->LoopEnd;
            else loop.Loop1End = Self->Length;

            stream.Loop     = &loop;
            stream.LoopSize = sizeof(struct AudioLoop);
         }
         else {
            stream.Loop     = NULL;
            stream.LoopSize = 0;
         }

         stream.Path         = Self->prvPath;
         stream.ObjectID     = 0;
         stream.SeekStart    = Self->prvDataOffset;
         stream.SampleFormat = sampleformat;
         stream.SampleLength = Self->Length;
         stream.BufferLength = Self->BufferLength;
         if (!WaitMsg(MT_SndAddStream, Self->AudioID, &stream)) {
            Self->Handle = stream.Result;
            return ERR_Okay;
         }
         else {
            LogErrorMsg("Failed to add sample to the Audio device.");
            if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
            return ERR_Failed;
         }
      }
      else if (!AllocMemory(Self->Length, MEM_DATA|MEM_NO_CLEAR, &buffer, NULL)) {
         Self->BufferLength = Self->Length;

         if (!acRead(Self->File, buffer, Self->Length, &result)) {
            if (Self->Flags & SDF_LOOP) {
               ClearMemory(&loop, sizeof(struct AudioSample));
               loop.LoopMode   = LOOP_SINGLE;
               loop.Loop1Type  = LTYPE_UNIDIRECTIONAL;
               loop.Loop1Start = Self->LoopStart;
               if (Self->LoopEnd) loop.Loop1End = Self->LoopEnd;
               else loop.Loop1End = Self->Length;

               add.Loop     = &loop;
               add.LoopSize = sizeof(struct AudioLoop);
            }
            else {
               add.Loop     = NULL;
               add.LoopSize = 0;
            }

            add.SampleFormat = sampleformat;
            add.Data         = buffer;
            add.DataSize     = Self->Length;
            add.Result       = 0;
            if (!WaitMsg(MT_SndAddSample, Self->AudioID, &add)) {
               Self->Handle = add.Result;
               FreeResource(buffer);
               return ERR_Okay;
            }
            else {
               FreeResource(buffer);
               LogErrorMsg("Failed to add sample to the Audio device.");
               if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
               return ERR_Failed;
            }
         }
         else {
            FreeResource(buffer);
            if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
            return PostError(ERR_Read);
         }
      }
      else {
         if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
         return PostError(ERR_AllocMemory);
      }
   }
}

#endif

//****************************************************************************

static ERROR SOUND_NewObject(objSound *Self, APTR Void)
{
   Self->Compression = 50;     // 50% compression by default
   Self->Volume      = 100;    // Playback at 100% volume level
   Self->Pan         = 0;
   Self->Playback    = 0;
   Self->prvNote     = NOTE_C; // Standard pitch
   Self->Stream      = STREAM_SMART;
   return ERR_Okay;
}

//****************************************************************************

static ERROR SOUND_ReleaseObject(objSound *Self, APTR Void)
{
   if (Self->prvPath)        { ReleaseMemory(Self->prvPath);        Self->prvPath    = NULL; }
   if (Self->prvDescription) { ReleaseMemory(Self->prvDescription); Self->prvDescription = NULL; }
   if (Self->prvDisclaimer)  { ReleaseMemory(Self->prvDisclaimer);  Self->prvDisclaimer  = NULL; }
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Reset: Stops audio playback, resets configuration details and restores the playback position to the start of the sample.
-END-
*****************************************************************************/

static ERROR SOUND_Reset(objSound *Self, APTR Void)
{
   LogAction(NULL);

   if (!Self->ChannelIndex) return ERR_Okay;

   objAudio *audio;
   if (!AccessObject(Self->AudioID, 2000, &audio)) {
      Self->Position = 0;

      struct AudioChannel *channel = GetChannel(audio, Self->ChannelIndex);

      if ((channel->SoundID != Self->Head.UniqueID) OR (channel->State IS CHS_STOPPED) OR
          (channel->State IS CHS_FINISHED)) {
         ReleaseObject(audio);
         return ERR_Okay;
      }

      COMMAND_Stop(audio, Self->ChannelIndex);

      if (!COMMAND_SetSample(audio, Self->ChannelIndex, Self->Handle)) {
         channel->SoundID = Self->Head.UniqueID;

         COMMAND_SetVolume(audio, Self->ChannelIndex, Self->Volume * 3.0);
         COMMAND_SetPan(audio, Self->ChannelIndex, Self->Pan);
         COMMAND_Play(audio, Self->ChannelIndex, Self->Playback);

         ReleaseObject(audio);
         return ERR_Okay;
      }
      else {
         ReleaseObject(audio);
         return PostError(ERR_Failed);
      }
   }
   else return PostError(ERR_AccessObject);

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
SaveToObject: Saves audio sample data to an object.
-END-
*****************************************************************************/

static ERROR SOUND_SaveToObject(objSound *Self, struct acSaveToObject *Args)
{
   // This routine is used if the developer is trying to save the sound data as a specific subclass type.

   if ((Args->ClassID) AND (Args->ClassID != ID_SOUND)) {
      struct rkMetaClass *mclass = (struct rkMetaClass *)FindClass(Args->ClassID);

      ERROR (**routine)(OBJECTPTR, APTR);
      if ((!GetPointer(mclass, FID_ActionTable, (APTR *)&routine)) AND (routine)) {
         if (routine[AC_SaveToObject]) {
            return routine[AC_SaveToObject]((OBJECTPTR)Self, Args);
         }
         else return PostError(ERR_NoSupport);
      }
      else return PostError(ERR_GetField);
   }

   // Save the sound data as a wave file




   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Seek: Moves sample playback to a new position.
-END-
*****************************************************************************/

static ERROR SOUND_Seek(objSound *Self, struct acSeek *Args)
{
   // Switch off the audio playback if active

   LONG active;
   if ((!SOUND_GET_Active(Self, &active)) AND (active)) {
      acDeactivate(Self);
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
         if (Self->Position > Self->Length) Self->Position = Self->Length;
      }
   }

   // Restart the audio

   if (active IS TRUE) acActivate(Self);

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
SetVar: Define custom tags that will be saved with the sample data.
-END-
*****************************************************************************/

static ERROR SOUND_SetVar(objSound *Self, struct acSetVar *Args)
{
   if ((!Args) OR (!Args->Field) OR (!Args->Field[0])) return ERR_NullArgs;

   if (!Self->Fields) {
      if (!(Self->Fields = VarNew(0, 0))) return ERR_AllocMemory;
   }

   return VarSetString(Self->Fields, Args->Field, Args->Value);
}

/*****************************************************************************
-FIELD-
Active: Returns TRUE if the sound sample is being played back.
-END-
*****************************************************************************/

static ERROR SOUND_GET_Active(objSound *Self, LONG *Value)
{
#ifdef _WIN32

   if (!Self->Handle) {
      WORD status;

      status = sndCheckActivity((struct PlatformData *)Self->prvPlatformData);

      if (status IS 0) *Value = FALSE;
      else if (status > 0) *Value = TRUE;
      else {
         LogErrorMsg("Error retrieving active status.");
         *Value = FALSE;
      }

      return ERR_Okay;
   }

#endif

   *Value = FALSE;

   if (Self->ChannelIndex) {
      objAudio *audio;
      if (!AccessObject(Self->AudioID, 5000, &audio)) {
         struct AudioChannel *channel;
         channel = GetChannel(audio, Self->ChannelIndex);
         if (channel) {
            if (!((channel->State IS CHS_STOPPED) OR (channel->State IS CHS_FINISHED))) {
               *Value = TRUE;
            }
         }
         ReleaseObject(audio);
      }
      else return ERR_AccessObject;
   }

   return ERR_Okay;
}

/*****************************************************************************

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
File: Refers to the file object that contains the audio data for playback.

This field is maintained internally and is defined post-initialisation.  It refers to the file object that contains
audio data for playback or recording purposes.

It is intended for use by child classes only.

-FIELD-
Flags: Optional initialisation flags.

*****************************************************************************/

static ERROR SOUND_SET_Flags(objSound *Self, LONG Value)
{
   Self->Flags = (Self->Flags & 0xffff0000) | (Value & 0x0000ffff);
   return ERR_Okay;
}

/*****************************************************************************

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

*****************************************************************************/

static ERROR SOUND_GET_Header(objSound *Self, BYTE **Value, LONG *Elements)
{
   *Value = Self->prvHeader;
   *Elements = ARRAYSIZE(Self->prvHeader);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Length: Indicates the total byte-length of sample data.

This field specifies the length of the sample data in bytes.  To get the length of the sample in seconds, divide this
value by the #BytesPerSecond field.

*****************************************************************************/

static ERROR SOUND_GET_Path(objSound *Self, STRING *Value)
{
   if ((*Value = Self->prvPath)) return ERR_Okay;
   else return ERR_FieldNotSet;
}

static ERROR SOUND_SET_Path(objSound *Self, CSTRING Value)
{
   if (Self->prvPath) { FreeResource(Self->prvPath); Self->prvPath = NULL; }

   if ((Value) AND (*Value)) {
      LONG i;
      for (i=0; Value[i]; i++);
      if (!AllocMemory(i+1, MEM_STRING|MEM_NO_CLEAR, (void **)&Self->prvPath, NULL)) {
         for (i=0; Value[i]; i++) Self->prvPath[i] = Value[i];
         Self->prvPath[i] = 0;
      }
      else return PostError(ERR_AllocMemory);
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
LoopEnd: The byte position at which sample looping will end.

When using looped samples (via the SDF_LOOP flag), set the LoopEnd field if the sample should
end at a position that is earlier than the sample's actual length.  The LoopEnd value is specified in bytes and must be
less or equal to the length of the sample and greater than the #LoopStart value.

-FIELD-
LoopStart: The byte position at which sample looping begins.

When using looped samples (via the SDF_LOOP flag), set the LoopStart field if the sample should
begin at a position other than zero.  The LoopStart value is specified in bytes and must be less than the length of the
sample and the #LoopEnd value.

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

*****************************************************************************/

static ERROR SOUND_GET_Note(objSound *Self, STRING *Value)
{
   switch(Self->prvNote) {
      case NOTE_C:  Self->prvNoteString[0] = 'C';
                    Self->prvNoteString[1] = '5' + Self->Octave;
                    Self->prvNoteString[2] = 0;
                    break;
      case NOTE_CS: Self->prvNoteString[0] = 'C';
                    Self->prvNoteString[1] = '5' + Self->Octave;
                    Self->prvNoteString[2] = '#';
                    Self->prvNoteString[3] = 0;
                    break;
      case NOTE_D:  Self->prvNoteString[0] = 'D';
                    Self->prvNoteString[1] = '5' + Self->Octave;
                    Self->prvNoteString[2] = 0;
                    break;
      case NOTE_DS: Self->prvNoteString[0] = 'D';
                    Self->prvNoteString[1] = '5' + Self->Octave;
                    Self->prvNoteString[2] = '#';
                    Self->prvNoteString[3] = 0;
                    break;
      case NOTE_E:  Self->prvNoteString[0] = 'E';
                    Self->prvNoteString[1] = '5' + Self->Octave;
                    Self->prvNoteString[2] = 0;
                    break;
      case NOTE_F:  Self->prvNoteString[0] = 'F';
                    Self->prvNoteString[1] = '5' + Self->Octave;
                    Self->prvNoteString[2] = 0;
                    break;
      case NOTE_FS: Self->prvNoteString[0] = 'F';
                    Self->prvNoteString[1] = '5' + Self->Octave;
                    Self->prvNoteString[2] = '#';
                    Self->prvNoteString[3] = 0;
                    break;
      case NOTE_G:  Self->prvNoteString[0] = 'G';
                    Self->prvNoteString[1] = '5' + Self->Octave;
                    Self->prvNoteString[2] = 0;
                    break;
      case NOTE_GS: Self->prvNoteString[0] = 'G';
                    Self->prvNoteString[1] = '5' + Self->Octave;
                    Self->prvNoteString[2] = '#';
                    Self->prvNoteString[3] = 0;
                    break;
      case NOTE_A:  Self->prvNoteString[0] = 'A';
                    Self->prvNoteString[1] = '5' + Self->Octave;
                    Self->prvNoteString[2] = 0;
                    break;
      case NOTE_AS: Self->prvNoteString[0] = 'A';
                    Self->prvNoteString[1] = '5' + Self->Octave;
                    Self->prvNoteString[2] = '#';
                    Self->prvNoteString[3] = 0;
                    break;
      case NOTE_B:  Self->prvNoteString[0] = 'B';
                    Self->prvNoteString[1] = '5' + Self->Octave;
                    Self->prvNoteString[2] = 0;
                    break;
      default:      Self->prvNoteString[0] = 0;
   }
   *Value = Self->prvNoteString;

   return ERR_Okay;
}

static ERROR SOUND_SET_Note(objSound *Self, CSTRING Value)
{
   if (!*Value) return ERR_Okay;

   LONG i, note;
   for (i=0; (Value[i]) AND (i < 3); i++) Self->prvNoteString[i] = Value[i];
   Self->prvNoteString[i] = 0;

   CSTRING str = Value;
   if (((*Value >= '0') AND (*Value <= '9')) OR (*Value IS '-')) {
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
      if ((*str >= '0') AND (*str <= '9')) {
         note += NOTE_OCTAVE * (*str - '5');
         str++;
      }
      if ((*str IS 'S') OR (*str IS 's') OR (*str IS '#')) note++; // Sharp note
   }

   if ((note > NOTE_OCTAVE * 5) OR (note < -(NOTE_OCTAVE * 5))) return PostError(ERR_OutOfRange);

   Self->Flags |= SDF_NOTE;

   // Calculate the note value

   if ((Self->prvNote = note) < 0) Self->prvNote = -Self->prvNote;
   Self->prvNote = Self->prvNote % NOTE_OCTAVE;
   if (Self->prvNote > NOTE_B) Self->prvNote = NOTE_B;

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

   Self->Playback = (LONG)(Self->Playback * glScale[Self->prvNote]);

   // If the sound is playing, set the new playback frequency immediately

#ifdef _WIN32
   if ((!Self->Handle) AND (Self->Head.Flags & NF_INITIALISED)) {
      sndFrequency((struct PlatformData *)Self->prvPlatformData, Self->Playback);
      return ERR_Okay;
   }
#endif

   if (Self->ChannelIndex) {
      objAudio *audio;
      if (!AccessObject(Self->AudioID, 200, &audio)) {
         COMMAND_SetFrequency(audio, Self->ChannelIndex, Self->Playback);
         ReleaseObject(audio);
      }
      else return ERR_AccessObject;
   }

   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Octave: The octave to use for sample playback.

The Octave field determines the octave to use when playing back a sound sample.  The default setting is zero, which
represents the octave at which the sound was sampled.  Setting a negative octave will lower the playback rate, while
positive values raise the playback rate.  The minimum octave setting is -5 and the highest setting is +5.

The octave can also be adjusted by setting the #Note field.  Setting the Octave field directly is useful if
you need to quickly double or halve the playback rate.

*****************************************************************************/

static ERROR SOUND_SET_Octave(objSound *Self, LONG Value)
{
   if ((Value < -10) OR (Value > 10))
   Self->Octave = Value;
   return SetLong(Self, FID_Note, Self->prvNote);
}

/*****************************************************************************
-FIELD-
Pan: Determines the horizontal position of a sound when played through stereo speakers.

The Pan field adjusts the "horizontal position" of a sample that is being played through stereo speakers.
The default value for this field is zero, which plays the sound through both speakers at an equal level.  The minimum
value is -100, which forces play through the left speaker and the maximum value is 100, which forces play through the
right speaker.

*****************************************************************************/

static ERROR SOUND_SET_Pan(objSound *Self, DOUBLE Value)
{
   Self->Pan = Value;

   if (Self->Pan < -100) Self->Pan = -100;
   else if (Self->Pan > 100) Self->Pan = 100;

#ifdef _WIN32
   if ((!Self->Handle) AND (Self->Head.Flags & NF_INITIALISED)) {
      sndPan((struct PlatformData *)Self->prvPlatformData, Self->Pan);
      return ERR_Okay;
   }
#endif

   if (Self->ChannelIndex) {
      objAudio *audio;
      if (!AccessObject(Self->AudioID, 200, &audio)) {
         COMMAND_SetPan(audio, Self->ChannelIndex, (Self->Pan * 64) / 100);
         ReleaseObject(audio);
      }
      else return ERR_AccessObject;
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Path: Location of the audio sample data.

This field must refer to a file that contains the audio data that will be loaded.  If creating a new sample
with the SDF_NEW flag, it is not necessary to define a file source.

-FIELD-
Playback: The playback frequency of the sound sample can be defined here.

Set this field to define the exact frequency of a sample's playback.  The playback frequency can be modified at
any time, including during audio playback if real-time adjustments to a sample's audio output rate is desired.

*****************************************************************************/

static ERROR SOUND_SET_Playback(objSound *Self, LONG Value)
{
   if ((Value < 0) OR (Value > 500000)) return ERR_OutOfRange;

   Self->Playback = Value;
   Self->Flags &= ~SDF_NOTE;

#ifdef _WIN32
   if ((!Self->Handle) AND (Self->Head.Flags & NF_INITIALISED)) {
      sndFrequency((struct PlatformData *)Self->prvPlatformData, Self->Playback);
      return ERR_Okay;
   }
#endif

   if (Self->ChannelIndex) {
      objAudio *audio;
      if (!AccessObject(Self->AudioID, 200, &audio)) {
         COMMAND_SetFrequency(audio, Self->ChannelIndex, Self->Playback);
         ReleaseObject(audio);
      }
      else return PostError(ERR_AccessObject);
   }

   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Position: The current playback position.

The current playback position of the audio sample is indicated by this field.  Writing to the field will alter the
playback position, either when the sample is next played, or immediately if it is currently playing.

*****************************************************************************/

static ERROR SOUND_GET_Position(objSound *Self, LONG *Value)
{
#ifdef _WIN32

   if (!Self->Handle) {
      Self->Position = sndGetPosition((struct PlatformData *)Self->prvPlatformData);
      *Value = Self->Position;
      return ERR_Okay;
   }

#endif

   *Value = Self->Position;
   return ERR_Okay;
}

static ERROR SOUND_SET_Position(objSound *Self, LONG Value)
{
   if (!acSeek(Self, Value, SEEK_START)) return ERR_Okay;
   else {
      LogMsg("Failed to seek to byte position %d.", Value);
      return ERR_Seek;
   }
}

/*****************************************************************************

-FIELD-
Priority: The priority of a sound in relation to other sound samples being played.

The playback priority of the sample is defined here. This helps to determine if the sample should be played when all
available mixing channels are busy. Naturally, higher priorities are played over samples with low priorities.

The minimum priority value allowed is -100, the maximum is 100.

*****************************************************************************/

static ERROR SOUND_SET_Priority(objSound *Self, LONG Value)
{
   Self->Priority = Value;
   if (Self->Priority < -100) Self->Priority = -100;
   else if (Self->Priority > 100) Self->Priority = 100;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Stream: Defines the preferred streaming method for the sample.

-FIELD-
StreamFile: Refers to a File object that is being streamed for playback.

This field is maintained internally and is defined post-initialisation.  It refers to a @File object that
is being streamed.

-FIELD-
Volume: The volume to use when playing the sound sample.

The field specifies the volume of a sound, which lies in the range 0 - 100%.  A volume of zero will not be heard, while
a volume of 100 is the loudest.  Setting the field during sample playback will dynamically alter the volume.
-END-

*****************************************************************************/

static ERROR SOUND_SET_Volume(objSound *Self, DOUBLE Value)
{
   Self->Volume = Value;
   if (Self->Volume < 0) Self->Volume = 0;
   else if (Self->Volume > 100) Self->Volume = 100;

#ifdef _WIN32
   if ((!Self->Handle) AND (Self->Head.Flags & NF_INITIALISED)) {
      sndVolume((struct PlatformData *)Self->prvPlatformData, glAudio->Volume * Self->Volume * (1.0 / 100.0));
      return ERR_Okay;
   }
#endif

   if (Self->ChannelIndex) {
      objAudio *audio;
      if (!AccessObject(Self->AudioID, 200, &audio)) {
         COMMAND_SetVolume(audio, Self->ChannelIndex, Self->Volume);
         ReleaseObject(audio);
      }
      else return ERR_AccessObject;
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR find_chunk(objSound *Self, OBJECTPTR File, CSTRING ChunkName)
{
   while (1) {
      UBYTE chunk[4];
      LONG len;
      if ((acRead(File, chunk, sizeof(chunk), &len) != ERR_Okay) OR (len != sizeof(chunk))) {
         return ERR_Read;
      }

      if (!StrCompare(ChunkName, chunk, 4, STR_CASE)) return ERR_Okay;

      len = ReadLong(Self->File); // Length of data in this chunk
      acSeek(Self->File, len, SEEK_CURRENT);
   }
}

//****************************************************************************

static ERROR playback_timer(objSound *Self, LARGE Elapsed, LARGE CurrentTime)
{
   LONG active;

#ifdef _WIN32
   if ((Self->Flags & SDF_STREAM) AND (!Self->Handle)) {
      // See sndStreamAudio() for further information on streaming in Win32

      if (sndStreamAudio((struct PlatformData *)Self->prvPlatformData)) {
         // We have reached the end of the sample.  If looping is turned off, terminate the timer subscription.

         if (!(Self->Flags & SDF_LOOP)) {
            LogF("6","Sound playback completed.");
            if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
            else DelayMsg(AC_Deactivate, Self->Head.UniqueID, NULL);
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

   if (!(Self->Flags & SDF_LOOP)) {
      if ((!GetLong(Self, FID_Active, &active)) AND (active IS FALSE)) {
         LogF("6","Sound playback completed.");
         if (Self->Flags & SDF_TERMINATE) DelayMsg(AC_Free, Self->Head.UniqueID, NULL);
         else DelayMsg(AC_Deactivate, Self->Head.UniqueID, NULL);
         Self->Timer = 0;
         return ERR_Terminate;
      }
   }

   return ERR_Okay;
}

//****************************************************************************

static const struct FieldDef clFlags[] = {
   { "Loop",         SDF_LOOP },
   { "New",          SDF_NEW },
   { "Query",        SDF_QUERY },
   { "Stereo",       SDF_STEREO },
   { "Terminate",    SDF_TERMINATE },
   { "RestrictPlay", SDF_RESTRICT_PLAY },
   { NULL, 0 }
};

static const struct FieldDef clStream[] = {
   { "Always", STREAM_ALWAYS },
   { "Smart",  STREAM_SMART },
   { "Never",  STREAM_NEVER },
   { NULL, 0 }
};

static const struct FieldArray clFields[] = {
   { "Volume",         FDF_DOUBLE|FDF_RW,    0, NULL, SOUND_SET_Volume },
   { "Pan",            FDF_DOUBLE|FDF_RW,    0, NULL, SOUND_SET_Pan },
   { "Priority",       FDF_LONG|FDF_RW,      0, NULL, SOUND_SET_Priority },
   { "Length",         FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "Octave",         FDF_LONG|FDF_RW,      0, NULL, SOUND_SET_Octave },
   { "Flags",          FDF_LONGFLAGS|FDF_RW, (MAXINT)&clFlags, NULL, SOUND_SET_Flags },
   { "Frequency",      FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "Playback",       FDF_LONG|FDF_RW,      0, NULL, SOUND_SET_Playback },
   { "Compression",    FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "BytesPerSecond", FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "BitsPerSample",  FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "Audio",          FDF_OBJECTID|FDF_RI,  0, NULL, NULL },
   { "LoopStart",      FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "LoopEnd",        FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "Stream",         FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clStream, NULL, NULL },
   { "BufferLength",   FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "StreamFile",     FDF_OBJECTID|FDF_RI,  0, NULL, NULL },
   { "Position",       FDF_LONG|FDF_RW,      0, SOUND_GET_Position, SOUND_SET_Position },
   { "Handle",         FDF_LONG|FDF_SYSTEM|FDF_R, 0, NULL, NULL },
   { "ChannelIndex",   FDF_LONG|FDF_R,       0, NULL, NULL },
   { "File",           FDF_OBJECT|FDF_SYSTEM|FDF_R, ID_FILE, NULL, NULL },
   // Virtual fields
   { "Active",   FDF_LONG|FDF_R,     0, SOUND_GET_Active, NULL },
   { "Header",   FDF_POINTER|FDF_ARRAY|FDF_R, 0, SOUND_GET_Header, NULL },
   { "Path",     FDF_STRING|FDF_RI,  0, SOUND_GET_Path, SOUND_SET_Path },
   { "Note",     FDF_STRING|FDF_RW,  0, SOUND_GET_Note, SOUND_SET_Note },
   END_FIELD
};

static const struct ActionArray clActions[] = {
   { AC_ActionNotify,  SOUND_ActionNotify },
   { AC_Activate,      SOUND_Activate },
   { AC_Deactivate,    SOUND_Deactivate },
   { AC_Disable,       SOUND_Disable },
   { AC_Enable,        SOUND_Enable },
   { AC_Free,          SOUND_Free },
   { AC_GetVar,        SOUND_GetVar },
   { AC_Init,          SOUND_Init },
   { AC_NewObject,     SOUND_NewObject },
   { AC_ReleaseObject, SOUND_ReleaseObject },
   { AC_Reset,         SOUND_Reset },
   { AC_SaveToObject,  SOUND_SaveToObject },
   { AC_Seek,          SOUND_Seek },
   { AC_SetVar,        SOUND_SetVar },
   { 0, NULL }
};
