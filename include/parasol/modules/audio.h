#pragma once

// Name:      audio.h
// Copyright: Paul Manias © 2002-2023
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

// Volume control flags

enum class VCF : ULONG {
   NIL = 0,
   PLAYBACK = 0x00000001,
   CAPTURE = 0x00000010,
   JOINED = 0x00000100,
   MONO = 0x00001000,
   MUTE = 0x00010000,
   SYNC = 0x00100000,
};

DEFINE_ENUM_FLAG_OPERATORS(VCF)

// Optional flags for the AudioChannel structure.

enum class CHF : ULONG {
   NIL = 0,
   MUTE = 0x00000001,
   BACKWARD = 0x00000002,
   VOL_RAMP = 0x00000004,
   CHANGED = 0x00000008,
};

DEFINE_ENUM_FLAG_OPERATORS(CHF)

// Flags for the SetVolume() method.

#define SVF_MUTE 0x00000100
#define SVF_UNMUTE 0x00001000
#define SVF_CAPTURE 0x00010000

// Sound flags

#define SDF_LOOP 0x00000001
#define SDF_NEW 0x00000002
#define SDF_STEREO 0x00000004
#define SDF_RESTRICT_PLAY 0x00000008
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

enum class LOOP : WORD {
   NIL = 0,
   SINGLE = 1,
   SINGLE_RELEASE = 2,
   DOUBLE = 3,
   AMIGA_NONE = 4,
   AMIGA = 5,
};

// Loop types for the AudioLoop structure.

enum class LTYPE : BYTE {
   NIL = 0,
   UNIDIRECTIONAL = 1,
   BIDIRECTIONAL = 2,
};

// Streaming options

enum class STREAM : LONG {
   NIL = 0,
   NEVER = 1,
   SMART = 2,
   ALWAYS = 3,
};

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

enum class CHS : BYTE {
   NIL = 0,
   STOPPED = 0,
   FINISHED = 1,
   PLAYING = 2,
   RELEASED = 3,
   FADE_OUT = 4,
};

struct AudioLoop {
   LOOP  LoopMode;   // Loop mode (single, double)
   LTYPE Loop1Type;  // First loop type (unidirectional, bidirectional)
   LTYPE Loop2Type;  // Second loop type (unidirectional, bidirectional)
   LONG  Loop1Start; // Start of the first loop
   LONG  Loop1End;   // End of the first loop
   LONG  Loop2Start; // Start of the second loop
   LONG  Loop2End;   // End of the second loop
};

// Audio class definition

#define VER_AUDIO (1.000000)

// Audio methods

#define MT_SndOpenChannels -1
#define MT_SndCloseChannels -2
#define MT_SndAddSample -3
#define MT_SndRemoveSample -4
#define MT_SndSetSampleLength -5
#define MT_SndAddStream -6
#define MT_SndBeep -7
#define MT_SndSetVolume -8

struct sndOpenChannels { LONG Total; LONG Result;  };
struct sndCloseChannels { LONG Handle;  };
struct sndAddSample { FUNCTION OnStop; LONG SampleFormat; APTR Data; LONG DataSize; struct AudioLoop * Loop; LONG LoopSize; LONG Result;  };
struct sndRemoveSample { LONG Handle;  };
struct sndSetSampleLength { LONG Sample; LARGE Length;  };
struct sndAddStream { FUNCTION Callback; FUNCTION OnStop; LONG SampleFormat; LONG SampleLength; LONG PlayOffset; struct AudioLoop * Loop; LONG LoopSize; LONG Result;  };
struct sndBeep { LONG Pitch; LONG Duration; LONG Volume;  };
struct sndSetVolume { LONG Index; CSTRING Name; LONG Flags; LONG Channel; DOUBLE Volume;  };

INLINE ERROR sndOpenChannels(APTR Ob, LONG Total, LONG * Result) {
   struct sndOpenChannels args = { Total, 0 };
   ERROR error = Action(MT_SndOpenChannels, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

INLINE ERROR sndCloseChannels(APTR Ob, LONG Handle) {
   struct sndCloseChannels args = { Handle };
   return(Action(MT_SndCloseChannels, (OBJECTPTR)Ob, &args));
}

INLINE ERROR sndAddSample(APTR Ob, FUNCTION OnStop, LONG SampleFormat, APTR Data, LONG DataSize, struct AudioLoop * Loop, LONG LoopSize, LONG * Result) {
   struct sndAddSample args = { OnStop, SampleFormat, Data, DataSize, Loop, LoopSize, 0 };
   ERROR error = Action(MT_SndAddSample, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

INLINE ERROR sndRemoveSample(APTR Ob, LONG Handle) {
   struct sndRemoveSample args = { Handle };
   return(Action(MT_SndRemoveSample, (OBJECTPTR)Ob, &args));
}

INLINE ERROR sndSetSampleLength(APTR Ob, LONG Sample, LARGE Length) {
   struct sndSetSampleLength args = { Sample, Length };
   return(Action(MT_SndSetSampleLength, (OBJECTPTR)Ob, &args));
}

INLINE ERROR sndAddStream(APTR Ob, FUNCTION Callback, FUNCTION OnStop, LONG SampleFormat, LONG SampleLength, LONG PlayOffset, struct AudioLoop * Loop, LONG LoopSize, LONG * Result) {
   struct sndAddStream args = { Callback, OnStop, SampleFormat, SampleLength, PlayOffset, Loop, LoopSize, 0 };
   ERROR error = Action(MT_SndAddStream, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

INLINE ERROR sndBeep(APTR Ob, LONG Pitch, LONG Duration, LONG Volume) {
   struct sndBeep args = { Pitch, Duration, Volume };
   return(Action(MT_SndBeep, (OBJECTPTR)Ob, &args));
}

INLINE ERROR sndSetVolume(APTR Ob, LONG Index, CSTRING Name, LONG Flags, LONG Channel, DOUBLE Volume) {
   struct sndSetVolume args = { Index, Name, Flags, Channel, Volume };
   return(Action(MT_SndSetVolume, (OBJECTPTR)Ob, &args));
}


class objAudio : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_AUDIO;
   static constexpr CSTRING CLASS_NAME = "Audio";

   using create = parasol::Create<objAudio>;

   LONG OutputRate;    // Determines the frequency to use for the output of audio data.
   LONG InputRate;     // Determines the frequency to use when recording audio data.
   LONG Quality;       // Determines the quality of the audio mixing.
   LONG Flags;         // Special audio flags can be set here.
   LONG BitDepth;      // The bit depth affects the overall quality of audio input and output.
   LONG Periods;       // Defines the number of periods that make up the internal audio buffer.
   LONG PeriodSize;    // Defines the byte size of each period allocated to the internal audio buffer.

   // Action stubs

   inline ERROR activate() { return Action(AC_Activate, this, NULL); }
   inline ERROR deactivate() { return Action(AC_Deactivate, this, NULL); }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
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
   static constexpr CLASSID CLASS_ID = ID_SOUND;
   static constexpr CSTRING CLASS_NAME = "Sound";

   using create = parasol::Create<objSound>;

   DOUBLE   Volume;        // The volume to use when playing the sound sample.
   DOUBLE   Pan;           // Determines the horizontal position of a sound when played through stereo speakers.
   LARGE    Position;      // The current playback position.
   LONG     Priority;      // The priority of a sound in relation to other sound samples being played.
   LONG     Length;        // Indicates the total byte-length of sample data.
   LONG     Octave;        // The octave to use for sample playback.
   LONG     Flags;         // Optional initialisation flags.
   LONG     Frequency;     // The frequency of a sampled sound is specified here.
   LONG     Playback;      // The playback frequency of the sound sample can be defined here.
   LONG     Compression;   // Determines the amount of compression used when saving an audio sample.
   LONG     BytesPerSecond; // The flow of bytes-per-second when the sample is played at normal frequency.
   LONG     BitsPerSample; // Indicates the sample rate of the audio sample, typically 8 or 16 bit.
   OBJECTID AudioID;       // Refers to the audio object/device to use for playback.
   LONG     LoopStart;     // The byte position at which sample looping begins.
   LONG     LoopEnd;       // The byte position at which sample looping will end.
   STREAM   Stream;        // Defines the preferred streaming method for the sample.
   LONG     Handle;        // Audio handle acquired at the audio object [Private - Available to child classes]
   LONG     ChannelIndex;  // Refers to the channel that the sound is playing through.

   // Action stubs

   inline ERROR activate() { return Action(AC_Activate, this, NULL); }
   inline ERROR deactivate() { return Action(AC_Deactivate, this, NULL); }
   inline ERROR disable() { return Action(AC_Disable, this, NULL); }
   inline ERROR enable() { return Action(AC_Enable, this, NULL); }
   inline ERROR getVar(CSTRING FieldName, STRING Buffer, LONG Size) {
      struct acGetVar args = { FieldName, Buffer, Size };
      ERROR error = Action(AC_GetVar, this, &args);
      if ((error) and (Buffer)) Buffer[0] = 0;
      return error;
   }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
   template <class T> ERROR read(APTR Buffer, T Bytes, LONG *Result) {
      ERROR error;
      const LONG bytes = (Bytes > 0x7fffffff) ? 0x7fffffff : Bytes;
      struct acRead read = { (BYTE *)Buffer, bytes };
      if (!(error = Action(AC_Read, this, &read))) *Result = read.Result;
      else *Result = 0;
      return error;
   }
   template <class T> ERROR read(APTR Buffer, T Bytes) {
      const LONG bytes = (Bytes > 0x7fffffff) ? 0x7fffffff : Bytes;
      struct acRead read = { (BYTE *)Buffer, bytes };
      return Action(AC_Read, this, &read);
   }
   inline ERROR saveToObject(OBJECTID DestID, CLASSID ClassID) {
      struct acSaveToObject args = { { DestID }, { ClassID } };
      return Action(AC_SaveToObject, this, &args);
   }
   inline ERROR seek(DOUBLE Offset, LONG Position = SEEK_CURRENT) {
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

extern struct AudioBase *AudioBase;
struct AudioBase {
   ERROR (*_MixContinue)(objAudio * Audio, LONG Handle);
   ERROR (*_MixFrequency)(objAudio * Audio, LONG Handle, LONG Frequency);
   ERROR (*_MixMute)(objAudio * Audio, LONG Handle, LONG Mute);
   ERROR (*_MixPan)(objAudio * Audio, LONG Handle, DOUBLE Pan);
   ERROR (*_MixPlay)(objAudio * Audio, LONG Handle, LONG Position);
   ERROR (*_MixRate)(objAudio * Audio, LONG Handle, LONG Rate);
   ERROR (*_MixSample)(objAudio * Audio, LONG Handle, LONG Sample);
   ERROR (*_MixStop)(objAudio * Audio, LONG Handle);
   ERROR (*_MixStopLoop)(objAudio * Audio, LONG Handle);
   ERROR (*_MixVolume)(objAudio * Audio, LONG Handle, DOUBLE Volume);
   ERROR (*_MixStartSequence)(objAudio * Audio, LONG Handle);
   ERROR (*_MixEndSequence)(objAudio * Audio, LONG Handle);
};

#ifndef PRV_AUDIO_MODULE
inline ERROR sndMixContinue(objAudio * Audio, LONG Handle) { return AudioBase->_MixContinue(Audio,Handle); }
inline ERROR sndMixFrequency(objAudio * Audio, LONG Handle, LONG Frequency) { return AudioBase->_MixFrequency(Audio,Handle,Frequency); }
inline ERROR sndMixMute(objAudio * Audio, LONG Handle, LONG Mute) { return AudioBase->_MixMute(Audio,Handle,Mute); }
inline ERROR sndMixPan(objAudio * Audio, LONG Handle, DOUBLE Pan) { return AudioBase->_MixPan(Audio,Handle,Pan); }
inline ERROR sndMixPlay(objAudio * Audio, LONG Handle, LONG Position) { return AudioBase->_MixPlay(Audio,Handle,Position); }
inline ERROR sndMixRate(objAudio * Audio, LONG Handle, LONG Rate) { return AudioBase->_MixRate(Audio,Handle,Rate); }
inline ERROR sndMixSample(objAudio * Audio, LONG Handle, LONG Sample) { return AudioBase->_MixSample(Audio,Handle,Sample); }
inline ERROR sndMixStop(objAudio * Audio, LONG Handle) { return AudioBase->_MixStop(Audio,Handle); }
inline ERROR sndMixStopLoop(objAudio * Audio, LONG Handle) { return AudioBase->_MixStopLoop(Audio,Handle); }
inline ERROR sndMixVolume(objAudio * Audio, LONG Handle, DOUBLE Volume) { return AudioBase->_MixVolume(Audio,Handle,Volume); }
inline ERROR sndMixStartSequence(objAudio * Audio, LONG Handle) { return AudioBase->_MixStartSequence(Audio,Handle); }
inline ERROR sndMixEndSequence(objAudio * Audio, LONG Handle) { return AudioBase->_MixEndSequence(Audio,Handle); }
#endif

