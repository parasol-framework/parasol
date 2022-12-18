#pragma once

// Name:      audio.h
// Copyright: Paul Manias © 2002-2022
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_AUDIO (1)

class objAudio;
class objSound;

// Optional flags for the Audio object.

#define ADF_OVER_SAMPLING 0x00000001
#define ADF_FILTER_LOW 0x00000002
#define ADF_FILTER_HIGH 0x00000004
#define ADF_STEREO 0x00000008
#define ADF_VOL_RAMPING 0x00000010
#define ADF_AUTO_SAVE 0x00000020
#define ADF_SYSTEM_WIDE 0x00000040
#define ADF_SERVICE_MODE 0x00000080

// Volume control flags

#define VCF_PLAYBACK 0x00000001
#define VCF_CAPTURE 0x00000010
#define VCF_JOINED 0x00000100
#define VCF_MONO 0x00001000
#define VCF_MUTE 0x00010000
#define VCF_SYNC 0x00100000

// Optional flags for the AudioChannel structure.

#define CHF_MUTE 0x00000001
#define CHF_BACKWARD 0x00000002
#define CHF_VOL_RAMP 0x00000004
#define CHF_CHANGED 0x00000008

// Flags for the SetVolume() method.

#define SVF_MUTE 0x00000100
#define SVF_UNMUTE 0x00001000
#define SVF_CAPTURE 0x00010000
#define SVF_SYNC 0x00100000
#define SVF_UNSYNC 0x01000000

// Sound flags

#define SDF_LOOP 0x00000001
#define SDF_NEW 0x00000002
#define SDF_QUERY 0x00000004
#define SDF_STEREO 0x00000008
#define SDF_TERMINATE 0x00000010
#define SDF_RESTRICT_PLAY 0x00000020
#define SDF_STREAM 0x40000000
#define SDF_NOTE 0x80000000

// These audio bit formats are supported by AddSample and AddStream.

#define SFM_BIG_ENDIAN 0x80000000
#define SFM_U8_BIT_MONO 1
#define SFM_S16_BIT_MONO 2
#define SFM_U8_BIT_STEREO 3
#define SFM_S16_BIT_STEREO 4
#define SFM_END 5

// Loop modes for the AudioLoop structure.

#define LOOP_SINGLE 1
#define LOOP_SINGLE_RELEASE 2
#define LOOP_DOUBLE 3
#define LOOP_AMIGA_NONE 4
#define LOOP_AMIGA 5

// Loop types for the AudioLoop structure.

#define LTYPE_UNIDIRECTIONAL 1
#define LTYPE_BIDIRECTIONAL 2

// Audio channel commands

#define CMD_START_SEQUENCE 0
#define CMD_END_SEQUENCE 1
#define CMD_SET_SAMPLE 2
#define CMD_SET_VOLUME 3
#define CMD_SET_PAN 4
#define CMD_SET_FREQUENCY 5
#define CMD_SET_RATE 6
#define CMD_STOP 7
#define CMD_STOP_LOOPING 8
#define CMD_SET_POSITION 9
#define CMD_PLAY 10
#define CMD_FADE_IN 11
#define CMD_FADE_OUT 12
#define CMD_MUTE 13
#define CMD_SET_LENGTH 14
#define CMD_CONTINUE 15

// Streaming options

#define STREAM_NEVER 1
#define STREAM_SMART 2
#define STREAM_ALWAYS 3

// Definitions for the Note field.  An 'S' indicates a sharp note.

#define NOTE_C 0
#define NOTE_CS 1
#define NOTE_D 2
#define NOTE_DS 3
#define NOTE_E 4
#define NOTE_F 5
#define NOTE_FS 6
#define NOTE_G 7
#define NOTE_GS 8
#define NOTE_A 9
#define NOTE_AS 10
#define NOTE_B 11
#define NOTE_OCTAVE 12

// Channel status types for the AudioChannel structure.

#define CHS_STOPPED 0
#define CHS_FINISHED 1
#define CHS_PLAYING 2
#define CHS_RELEASED 3
#define CHS_FADE_OUT 4

typedef struct {
   ULONG numCopyBytes;     // number of bytes to copy
   ULONG *relocTable;      // relocation table
   ULONG numRelocEntries;  // number of relocation table entries
} MixLoopRelocInfo;

typedef struct {
   ULONG mainLoopAlign;
   ULONG mainLoopRepeat;
   void (*mixLoop)(ULONG numSamples, LONG nextSampleOffset);
   void (*mainMixLoop)(ULONG numSamples, LONG nextSampleOffset);
} MixRoutine;

typedef struct {
   const MixRoutine routines[5];
} MixRoutineSet;

#define MAX_CHANNELSETS 8
#define DEFAULT_BUFFER_SIZE 8096 // Measured in samples, not bytes
  
struct AudioSample {
   UBYTE *  Data;        // Private.  Pointer to the sample data.
   OBJECTID StreamID;    // Reference to an object to use for streaming
   LONG     SampleLength; // Length of the sample data, in bytes
   LONG     Loop1Start;  // Start of the first loop
   LONG     Loop1End;    // End of the first loop
   LONG     Loop2Start;  // Start of the second loop
   LONG     Loop2End;    // End of the second loop
   LONG     SeekStart;
   LONG     StreamLength;
   LONG     BufferLength;
   LONG     StreamPos;   // Current read position within the audio stream
   UBYTE    SampleType;  // Type of sample (bit format)
   BYTE     LoopMode;    // Loop mode (single, double)
   BYTE     Loop1Type;   // First loop type (unidirectional, bidirectional)
   BYTE     Loop2Type;   // Second loop type (unidirectional, bidirectional)
   BYTE     Used;
   BYTE     Free;
};

struct AudioChannel {
   struct AudioSample Sample;    // Sample structure
   OBJECTID SoundID;             // ID of the sound object set on this channel
   LONG     SampleHandle;        // Internal handle reference
   LONG     Flags;               // Special flags
   ULONG    Position;            // Current playing/mixing position
   ULONG    Frequency;           // Playback frequency
   UWORD    PositionLow;         // Playing position, lower bits
   WORD     LVolume;             // Current left speaker volume (0 - 100)
   WORD     RVolume;             // Current right speaker volume (0 - 100)
   WORD     LVolumeTarget;       // Volume target when fading or ramping
   WORD     RVolumeTarget;       // Volume target when fading or ramping
   WORD     Volume;              // Playing volume (0-100)
   BYTE     Priority;            // Priority of the sound that has been assigned to this channel
   BYTE     State;               // Channel state
   BYTE     LoopIndex;           // The current active loop (either 0, 1 or 2)
   BYTE     Pan;                 // Pan value (-100 to +100)
};

struct AudioCommand {
   LONG CommandID;    // Command ID
   LONG Handle;       // Channel handle
   LONG Data;         // Special data related to the command ID
};

struct ChannelSet {
   struct AudioChannel * Channel;    // Array of channel objects
   struct AudioCommand * Commands;   // Array of buffered commands
   LONG     UpdateRate;              // Update rate, measured in milliseconds
   LONG     MixLeft;                 // Amount of mix elements left before the next command-update occurs
   LONG     Key;
   OBJECTID TaskID;                  // Reference to the task that owns this set of channels
   MEMORYID ChannelMID;              // Private
   MEMORYID CommandMID;              // Private
   DOUBLE   TaskVolume;
   WORD     Total;                   // Total number of base channels
   WORD     Actual;                  // Total number of channels, including oversampling channels
   WORD     TotalCommands;           // Size of the command buffer
   WORD     Position;                // Index to write the next command to
   WORD     OpenCount;
};

struct VolumeCtl {
   WORD  Size;           // The size of the Channels array.
   char  Name[32];       // Name of the mixer
   LONG  Flags;          // Special flags identifying the mixer's attributes.
   FLOAT Channels[1];    // A variable length array of channel volumes.
};

struct AudioLoop {
   WORD LoopMode;    // Loop mode (single, double)
   BYTE Loop1Type;   // First loop type (unidirectional, bidirectional)
   BYTE Loop2Type;   // Second loop type (unidirectional, bidirectional)
   LONG Loop1Start;  // Start of the first loop
   LONG Loop1End;    // End of the first loop
   LONG Loop2Start;  // Start of the second loop
   LONG Loop2End;    // End of the second loop
};

typedef struct WAVEFormat {
   WORD Format;               // Type of WAVE data in the chunk
   WORD Channels;             // Number of channels, 1=mono, 2=stereo
   LONG Frequency;            // Playback frequency
   LONG AvgBytesPerSecond;    // Channels * SamplesPerSecond * (BitsPerSample / 8)
   WORD BlockAlign;           // Channels * (BitsPerSample / 8)
   WORD BitsPerSample;        // Bits per sample
   WORD ExtraLength;
} WAVEFORMATEX;

// Audio class definition

#define VER_AUDIO (1.000000)

// Audio methods

#define MT_SndOpenChannels -1
#define MT_SndCloseChannels -2
#define MT_SndAddSample -3
#define MT_SndRemoveSample -4
#define MT_SndBufferCommand -5
#define MT_SndAddStream -6
#define MT_SndBeep -7
#define MT_SndSetVolume -8

struct sndOpenChannels { LONG Total; LONG Key; LONG Commands; LONG Result;  };
struct sndCloseChannels { LONG Handle;  };
struct sndAddSample { LONG SampleFormat; APTR Data; LONG DataSize; struct AudioLoop * Loop; LONG LoopSize; LONG Result;  };
struct sndRemoveSample { LONG Handle;  };
struct sndBufferCommand { LONG Command; LONG Handle; LONG Data;  };
struct sndAddStream { CSTRING Path; OBJECTID ObjectID; LONG SeekStart; LONG SampleFormat; LONG SampleLength; LONG BufferLength; struct AudioLoop * Loop; LONG LoopSize; LONG Result;  };
struct sndBeep { LONG Pitch; LONG Duration; LONG Volume;  };
struct sndSetVolume { LONG Index; CSTRING Name; LONG Flags; DOUBLE Volume;  };

INLINE ERROR sndOpenChannels(APTR Ob, LONG Total, LONG Key, LONG Commands, LONG * Result) {
   struct sndOpenChannels args = { Total, Key, Commands, 0 };
   ERROR error = Action(MT_SndOpenChannels, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

INLINE ERROR sndCloseChannels(APTR Ob, LONG Handle) {
   struct sndCloseChannels args = { Handle };
   return(Action(MT_SndCloseChannels, (OBJECTPTR)Ob, &args));
}

INLINE ERROR sndAddSample(APTR Ob, LONG SampleFormat, APTR Data, LONG DataSize, struct AudioLoop * Loop, LONG LoopSize, LONG * Result) {
   struct sndAddSample args = { SampleFormat, Data, DataSize, Loop, LoopSize, 0 };
   ERROR error = Action(MT_SndAddSample, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

INLINE ERROR sndRemoveSample(APTR Ob, LONG Handle) {
   struct sndRemoveSample args = { Handle };
   return(Action(MT_SndRemoveSample, (OBJECTPTR)Ob, &args));
}

INLINE ERROR sndBufferCommand(APTR Ob, LONG Command, LONG Handle, LONG Data) {
   struct sndBufferCommand args = { Command, Handle, Data };
   return(Action(MT_SndBufferCommand, (OBJECTPTR)Ob, &args));
}

INLINE ERROR sndAddStream(APTR Ob, CSTRING Path, OBJECTID ObjectID, LONG SeekStart, LONG SampleFormat, LONG SampleLength, LONG BufferLength, struct AudioLoop * Loop, LONG LoopSize, LONG * Result) {
   struct sndAddStream args = { Path, ObjectID, SeekStart, SampleFormat, SampleLength, BufferLength, Loop, LoopSize, 0 };
   ERROR error = Action(MT_SndAddStream, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

INLINE ERROR sndBeep(APTR Ob, LONG Pitch, LONG Duration, LONG Volume) {
   struct sndBeep args = { Pitch, Duration, Volume };
   return(Action(MT_SndBeep, (OBJECTPTR)Ob, &args));
}

INLINE ERROR sndSetVolume(APTR Ob, LONG Index, CSTRING Name, LONG Flags, DOUBLE Volume) {
   struct sndSetVolume args = { Index, Name, Flags, Volume };
   return(Action(MT_SndSetVolume, (OBJECTPTR)Ob, &args));
}


class objAudio : public BaseClass {
   public:
   DOUBLE Bass;           // Sets the amount of bass to use for audio playback.
   DOUBLE Treble;         // Sets the amount of treble to use for audio playback.
   LONG   OutputRate;     // Determines the frequency to use for the output of audio data.
   LONG   InputRate;      // Determines the frequency to use when recording audio data.
   LONG   Quality;        // Determines the quality of the audio mixing.
   LONG   Flags;          // Special audio flags can be set here.
   LONG   TotalChannels;  // The total number of audio channels allocated by all processes.
   LONG   BitDepth;       // The bit depth affects the overall quality of audio input and output.
   LONG   Periods;        // Defines the number of periods that make up the internal audio buffer.
   LONG   PeriodSize;     // Defines the byte size of each period allocated to the internal audio buffer.
   // Action stubs

   inline ERROR activate() { return Action(AC_Activate, this, NULL); }
   inline ERROR clear() { return Action(AC_Clear, this, NULL); }
   inline ERROR deactivate() { return Action(AC_Deactivate, this, NULL); }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
   inline ERROR reset() { return Action(AC_Reset, this, NULL); }
   inline ERROR saveSettings() { return Action(AC_SaveSettings, this, NULL); }
   inline ERROR saveToObject(OBJECTID DestID, CLASSID ClassID) {
      struct acSaveToObject args = { { DestID }, { ClassID } };
      return Action(AC_SaveToObject, this, &args);
   }
};

// Sound class definition

#define VER_SOUND (1.000000)

class objSound : public BaseClass {
   public:
   DOUBLE    Volume;       // The volume to use when playing the sound sample.
   DOUBLE    Pan;          // Determines the horizontal position of a sound when played through stereo speakers.
   LONG      Priority;     // The priority of a sound in relation to other sound samples being played.
   LONG      Length;       // Indicates the total byte-length of sample data.
   LONG      Octave;       // The octave to use for sample playback.
   LONG      Flags;        // Optional initialisation flags.
   LONG      Frequency;    // The frequency of a sampled sound is specified here.
   LONG      Playback;     // The playback frequency of the sound sample can be defined here.
   LONG      Compression;  // Determines the amount of compression used when saving an audio sample.
   LONG      BytesPerSecond; // The flow of bytes-per-second when the sample is played at normal frequency.
   LONG      BitsPerSample; // Indicates the sample rate of the audio sample, typically 8 or 16 bit.
   OBJECTID  AudioID;      // Refers to the audio object/device to use for playback.
   LONG      LoopStart;    // The byte position at which sample looping begins.
   LONG      LoopEnd;      // The byte position at which sample looping will end.
   LONG      Stream;       // Defines the preferred streaming method for the sample.
   LONG      BufferLength; // Defines the size of the buffer to use when streaming is enabled.
   OBJECTID  StreamFileID; // Refers to a File object that is being streamed for playback.
   LONG      Position;     // The current playback position.
   LONG      Handle;       // Audio handle acquired at the audio object [Private - Available to child classes]
   LONG      ChannelIndex; // Refers to the channel that the sound is playing through.
   OBJECTPTR File;         // Refers to the file object that contains the audio data for playback.
   // Action stubs

   inline ERROR activate() { return Action(AC_Activate, this, NULL); }
   inline ERROR deactivate() { return Action(AC_Deactivate, this, NULL); }
   inline ERROR disable() { return Action(AC_Disable, this, NULL); }
   inline ERROR enable() { return Action(AC_Enable, this, NULL); }
   inline ERROR getVar(CSTRING FieldName, STRING Buffer, LONG Size) {
      struct acGetVar args = { FieldName, Buffer, Size };
      ERROR error = Action(AC_GetVar, this, &args);
      if ((error) AND (Buffer)) Buffer[0] = 0;
      return error;
   }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
   inline ERROR reset() { return Action(AC_Reset, this, NULL); }
   inline ERROR saveToObject(OBJECTID DestID, CLASSID ClassID) {
      struct acSaveToObject args = { { DestID }, { ClassID } };
      return Action(AC_SaveToObject, this, &args);
   }
   inline ERROR seek(DOUBLE Offset, LONG Position) {
      struct acSeek args = { Offset, Position };
      return Action(AC_Seek, this, &args);
   }
   inline ERROR seekStart(DOUBLE Offset)   { return seek(Offset, SEEK_START); }
   inline ERROR seekEnd(DOUBLE Offset)     { return seek(Offset, SEEK_END); }
   inline ERROR seekCurrent(DOUBLE Offset) { return seek(Offset, SEEK_CURRENT); }
   inline ERROR acSetVar(CSTRING FieldName, CSTRING Value) {
      struct acSetVar args = { FieldName, Value };
      return Action(AC_SetVar, this, &args);
   }
};

struct AudioBase {
   ERROR (*_StartDrivers)(void);
   ERROR (*_WaitDrivers)(LONG);
   LONG (*_SetChannels)(LONG);
   DOUBLE (*_SetTaskVolume)(DOUBLE);
};

#ifndef PRV_AUDIO_MODULE
#define sndStartDrivers(...) (AudioBase->_StartDrivers)(__VA_ARGS__)
#define sndWaitDrivers(...) (AudioBase->_WaitDrivers)(__VA_ARGS__)
#define sndSetChannels(...) (AudioBase->_SetChannels)(__VA_ARGS__)
#define sndSetTaskVolume(...) (AudioBase->_SetTaskVolume)(__VA_ARGS__)
#endif

