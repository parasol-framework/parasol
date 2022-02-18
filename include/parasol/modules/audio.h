#ifndef MODULES_AUDIO
#define MODULES_AUDIO 1

// Name:      audio.h
// Copyright: Paul Manias © 2002-2022
// Generator: idl-c

#ifndef MAIN_H
#include <parasol/main.h>
#endif

#define MODVERSION_AUDIO (1)

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

#define SFM_S16_BIT_STEREO 4
#define SFM_U8_BIT_MONO 1
#define SFM_END 5
#define SFM_S16_BIT_MONO 2
#define SFM_U8_BIT_STEREO 3
#define SFM_BIG_ENDIAN 0x80000000

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
   MixRoutine routines[5];
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

typedef struct rkAudio {
   OBJECT_HEADER
   DOUBLE Bass;
   DOUBLE Treble;
   LONG   OutputRate;
   LONG   InputRate;
   LONG   Quality;
   LONG   Flags;          // Special flags
   LONG   TotalChannels;  // Total number of channels allocated to the audio object
   LONG   BitDepth;       // Typically 8 or 16 bit, reflects the active bit depth
   LONG   Periods;
   LONG   PeriodSize;

#ifdef PRV_AUDIO
   struct ChannelSet Channels[MAX_CHANNELSETS]; // Channels are grouped into sets, which are allocated on a per-task basis
   struct AudioSample *Samples;
   struct VolumeCtl *VolumeCtl;
   MixRoutineSet      *MixRoutines;
   MEMORYID BFMemoryMID;
   MEMORYID BufferMemoryMID;
   MEMORYID SamplesMID;
   LONG     MixBufferSize;
   LONG     TotalSamples;
   TIMER    Timer;
   APTR     BufferMemory;
   APTR     MixBuffer;
   FLOAT    *BFMemory;               // Byte/Float table memory
   WORD     SampleBitSize;
   WORD     MixBitSize;
   LONG     MixElements;
   UBYTE    Stereo;                  // TRUE/FALSE for active stereo mode
   BYTE     Mute;
   BYTE     MasterVolume;
   BYTE     Initialising;
   APTR     TaskRemovedHandle;
   APTR     UserLoginHandle;
#ifdef __linux__
   UBYTE *AudioBuffer;
   LONG  AudioBufferSize;
   snd_pcm_t *Handle;
   snd_mixer_t *MixHandle;
#endif
   MEMORYID VolumeCtlMID;
   LONG VolumeCtlTotal;
   char prvDevice[28];
  
#endif
} objAudio;

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


// Sound class definition

#define VER_SOUND (1.000000)

typedef struct rkSound {
   OBJECT_HEADER
   DOUBLE    Volume;       // Volume of sample (0 - 100%)
   DOUBLE    Pan;          // Horizontal positioning for playback on stereo speakers (-100 to +100)
   LONG      Priority;     // Priority over other sounds
   LONG      Length;       // Length of sample data in bytes (also refer BufferLength)
   LONG      Octave;       // Current octave to use for playing back notes (defaults to zero)
   LONG      Flags;        // Sound flags
   LONG      Frequency;    // Frequency of sampled sound (nb: does not affect playback - use the Playback field)
   LONG      Playback;     // Frequency to use for sample playback
   LONG      Compression;  // Compression rating (0% none, 100% high)
   LONG      BytesPerSecond; // Bytes per second (Formula: Frequency * BytesPerSample)
   LONG      BitsPerSample; // Usually set to 8 or 16 bit
   OBJECTID  AudioID;      // Reference to an Audio object to use for audio output and input
   LONG      LoopStart;    // Byte position of looping start
   LONG      LoopEnd;      // Byte position of looping end
   LONG      Stream;       // Streaming type (smart by default)
   LONG      BufferLength; // Length of audio buffer in bytes (relevant if streaming)
   OBJECTID  StreamFileID; // Object allocated for streaming
   LONG      Position;     // Byte position to start playing from
   LONG      Handle;       // Audio handle acquired at the audio object [Private - Available to child classes]
   LONG      ChannelIndex; // Channel handle that the sound was last played on
   OBJECTPTR File;         // Private. The file holding the sample data; available to child classes only.

#ifdef PRV_SOUND
    struct KeyStore *Fields;
    UBYTE    prvHeader[128];
    LONG     prvFormat;         // The format of the sound data
    LONG     prvDataOffset;     // Start of raw audio data within the source file
    TIMER    Timer;
    STRING   prvPath;
    STRING   prvDescription;
    STRING   prvDisclaimer;
    LONG     prvNote;               // Note to play back (e.g. C, C#, G...)
    char     prvNoteString[4];
    struct WAVEFormat *prvWAVE;
    UBYTE    prvPlatformData[128];  // Data area for holding platform/hardware specific information
    LONG     prvAlignment;          // Byte alignment value
  
#endif
} objSound;

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

INLINE ERROR sndCloseChannelsID(OBJECTID AudioID, int Handle) {
   extern struct CoreBase *CoreBase;
   OBJECTPTR audio;
   if (!AccessObject(AudioID, 5000, &audio)) {
      struct sndCloseChannels close = { Handle };
      Action(MT_SndCloseChannels, audio, &close);
      ReleaseObject(audio);
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

INLINE ERROR sndOpenChannelsID(OBJECTID AudioID, LONG Total, LONG Key, LONG Commands, LONG *Handle) {
   extern struct CoreBase *CoreBase;
   OBJECTPTR audio;
   if (!AccessObject(AudioID, 5000, &audio)) {
      struct sndOpenChannels open = { Total, Key, Commands };
      Action(MT_SndOpenChannels, audio, &open);
      *Handle = open.Result;
      ReleaseObject(audio);
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}
  
#endif
