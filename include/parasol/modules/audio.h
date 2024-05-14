#pragma once

// Name:      audio.h
// Copyright: Paul Manias © 2002-2024
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_AUDIO (1)

class objAudio;
class objSound;

// Optional flags for the Audio object.

enum class ADF : ULONG {
   NIL = 0,
   OVER_SAMPLING = 0x00000001,
   FILTER_LOW = 0x00000002,
   FILTER_HIGH = 0x00000004,
   STEREO = 0x00000008,
   VOL_RAMPING = 0x00000010,
   AUTO_SAVE = 0x00000020,
   SYSTEM_WIDE = 0x00000040,
};

DEFINE_ENUM_FLAG_OPERATORS(ADF)

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

enum class SVF : ULONG {
   NIL = 0,
   MUTE = 0x00000100,
   UNMUTE = 0x00001000,
   CAPTURE = 0x00010000,
};

DEFINE_ENUM_FLAG_OPERATORS(SVF)

// Sound flags

enum class SDF : ULONG {
   NIL = 0,
   LOOP = 0x00000001,
   NEW = 0x00000002,
   STEREO = 0x00000004,
   RESTRICT_PLAY = 0x00000008,
   STREAM = 0x40000000,
   NOTE = 0x80000000,
};

DEFINE_ENUM_FLAG_OPERATORS(SDF)

// These audio bit formats are supported by AddSample and AddStream.

enum class SFM : ULONG {
   NIL = 0,
   F_BIG_ENDIAN = 0x80000000,
   U8_BIT_MONO = 1,
   S16_BIT_MONO = 2,
   U8_BIT_STEREO = 3,
   S16_BIT_STEREO = 4,
   END = 5,
};

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
struct sndAddSample { FUNCTION OnStop; SFM SampleFormat; APTR Data; LONG DataSize; struct AudioLoop * Loop; LONG LoopSize; LONG Result;  };
struct sndRemoveSample { LONG Handle;  };
struct sndSetSampleLength { LONG Sample; LARGE Length;  };
struct sndAddStream { FUNCTION Callback; FUNCTION OnStop; SFM SampleFormat; LONG SampleLength; LONG PlayOffset; struct AudioLoop * Loop; LONG LoopSize; LONG Result;  };
struct sndBeep { LONG Pitch; LONG Duration; LONG Volume;  };
struct sndSetVolume { LONG Index; CSTRING Name; SVF Flags; LONG Channel; DOUBLE Volume;  };

inline ERR sndOpenChannels(APTR Ob, LONG Total, LONG * Result) noexcept {
   struct sndOpenChannels args = { Total, (LONG)0 };
   ERR error = Action(MT_SndOpenChannels, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

inline ERR sndCloseChannels(APTR Ob, LONG Handle) noexcept {
   struct sndCloseChannels args = { Handle };
   return(Action(MT_SndCloseChannels, (OBJECTPTR)Ob, &args));
}

inline ERR sndAddSample(APTR Ob, FUNCTION OnStop, SFM SampleFormat, APTR Data, LONG DataSize, struct AudioLoop * Loop, LONG LoopSize, LONG * Result) noexcept {
   struct sndAddSample args = { OnStop, SampleFormat, Data, DataSize, Loop, LoopSize, (LONG)0 };
   ERR error = Action(MT_SndAddSample, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

inline ERR sndRemoveSample(APTR Ob, LONG Handle) noexcept {
   struct sndRemoveSample args = { Handle };
   return(Action(MT_SndRemoveSample, (OBJECTPTR)Ob, &args));
}

inline ERR sndSetSampleLength(APTR Ob, LONG Sample, LARGE Length) noexcept {
   struct sndSetSampleLength args = { Sample, Length };
   return(Action(MT_SndSetSampleLength, (OBJECTPTR)Ob, &args));
}

inline ERR sndAddStream(APTR Ob, FUNCTION Callback, FUNCTION OnStop, SFM SampleFormat, LONG SampleLength, LONG PlayOffset, struct AudioLoop * Loop, LONG LoopSize, LONG * Result) noexcept {
   struct sndAddStream args = { Callback, OnStop, SampleFormat, SampleLength, PlayOffset, Loop, LoopSize, (LONG)0 };
   ERR error = Action(MT_SndAddStream, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

inline ERR sndBeep(APTR Ob, LONG Pitch, LONG Duration, LONG Volume) noexcept {
   struct sndBeep args = { Pitch, Duration, Volume };
   return(Action(MT_SndBeep, (OBJECTPTR)Ob, &args));
}

inline ERR sndSetVolume(APTR Ob, LONG Index, CSTRING Name, SVF Flags, LONG Channel, DOUBLE Volume) noexcept {
   struct sndSetVolume args = { Index, Name, Flags, Channel, Volume };
   return(Action(MT_SndSetVolume, (OBJECTPTR)Ob, &args));
}


class objAudio : public Object {
   public:
   static constexpr CLASSID CLASS_ID = ID_AUDIO;
   static constexpr CSTRING CLASS_NAME = "Audio";

   using create = pf::Create<objAudio>;

   LONG OutputRate;    // Determines the frequency to use for the output of audio data.
   LONG InputRate;     // Determines the frequency to use when recording audio data.
   LONG Quality;       // Determines the quality of the audio mixing.
   ADF  Flags;         // Special audio flags can be set here.
   LONG BitDepth;      // The bit depth affects the overall quality of audio input and output.
   LONG Periods;       // Defines the number of periods that make up the internal audio buffer.
   LONG PeriodSize;    // Defines the byte size of each period allocated to the internal audio buffer.

   // Action stubs

   inline ERR activate() noexcept { return Action(AC_Activate, this, NULL); }
   inline ERR deactivate() noexcept { return Action(AC_Deactivate, this, NULL); }
   inline ERR init() noexcept { return InitObject(this); }
   inline ERR saveSettings() noexcept { return Action(AC_SaveSettings, this, NULL); }
   inline ERR saveToObject(OBJECTPTR Dest, CLASSID ClassID = 0) noexcept {
      struct acSaveToObject args = { Dest, { ClassID } };
      return Action(AC_SaveToObject, this, &args);
   }

   // Customised field setting

   inline ERR setOutputRate(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[1];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setInputRate(const LONG Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->InputRate = Value;
      return ERR::Okay;
   }

   inline ERR setQuality(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setFlags(const ADF Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Flags = Value;
      return ERR::Okay;
   }

   inline ERR setBitDepth(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setPeriods(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[10];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setPeriodSize(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   template <class T> inline ERR setDevice(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[15];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERR setMasterVolume(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERR setMute(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[7];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setStereo(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[6];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

};

// Sound class definition

#define VER_SOUND (1.000000)

class objSound : public Object {
   public:
   static constexpr CLASSID CLASS_ID = ID_SOUND;
   static constexpr CSTRING CLASS_NAME = "Sound";

   using create = pf::Create<objSound>;

   DOUBLE   Volume;        // The volume to use when playing the sound sample.
   DOUBLE   Pan;           // Determines the horizontal position of a sound when played through stereo speakers.
   LARGE    Position;      // The current playback position.
   LONG     Priority;      // The priority of a sound in relation to other sound samples being played.
   LONG     Length;        // Indicates the total byte-length of sample data.
   LONG     Octave;        // The octave to use for sample playback.
   SDF      Flags;         // Optional initialisation flags.
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

   inline ERR activate() noexcept { return Action(AC_Activate, this, NULL); }
   inline ERR deactivate() noexcept { return Action(AC_Deactivate, this, NULL); }
   inline ERR disable() noexcept { return Action(AC_Disable, this, NULL); }
   inline ERR enable() noexcept { return Action(AC_Enable, this, NULL); }
   inline ERR getKey(CSTRING Key, STRING Value, LONG Size) noexcept {
      struct acGetKey args = { Key, Value, Size };
      auto error = Action(AC_GetKey, this, &args);
      if ((error != ERR::Okay) and (Value)) Value[0] = 0;
      return error;
   }
   inline ERR init() noexcept { return InitObject(this); }
   template <class T, class U> ERR read(APTR Buffer, T Size, U *Result) noexcept {
      static_assert(std::is_integral<U>::value, "Result value must be an integer type");
      static_assert(std::is_integral<T>::value, "Size value must be an integer type");
      const LONG bytes = (Size > 0x7fffffff) ? 0x7fffffff : Size;
      struct acRead read = { (BYTE *)Buffer, bytes };
      if (auto error = Action(AC_Read, this, &read); error IS ERR::Okay) {
         *Result = static_cast<U>(read.Result);
         return ERR::Okay;
      }
      else { *Result = 0; return error; }
   }
   template <class T> ERR read(APTR Buffer, T Size) noexcept {
      static_assert(std::is_integral<T>::value, "Size value must be an integer type");
      const LONG bytes = (Size > 0x7fffffff) ? 0x7fffffff : Size;
      struct acRead read = { (BYTE *)Buffer, bytes };
      return Action(AC_Read, this, &read);
   }
   inline ERR saveToObject(OBJECTPTR Dest, CLASSID ClassID = 0) noexcept {
      struct acSaveToObject args = { Dest, { ClassID } };
      return Action(AC_SaveToObject, this, &args);
   }
   inline ERR seek(DOUBLE Offset, SEEK Position = SEEK::CURRENT) noexcept {
      struct acSeek args = { Offset, Position };
      return Action(AC_Seek, this, &args);
   }
   inline ERR seekStart(DOUBLE Offset) noexcept { return seek(Offset, SEEK::START); }
   inline ERR seekEnd(DOUBLE Offset) noexcept { return seek(Offset, SEEK::END); }
   inline ERR seekCurrent(DOUBLE Offset) noexcept { return seek(Offset, SEEK::CURRENT); }
   inline ERR acSetKey(CSTRING FieldName, CSTRING Value) noexcept {
      struct acSetKey args = { FieldName, Value };
      return Action(AC_SetKey, this, &args);
   }

   // Customised field setting

   inline ERR setVolume(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERR setPan(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[4];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERR setPosition(const LARGE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[16];
      return field->WriteValue(target, field, FD_LARGE, &Value, 1);
   }

   inline ERR setPriority(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[13];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setLength(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[3];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setOctave(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[10];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setFlags(const SDF Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[8];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setFrequency(const LONG Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Frequency = Value;
      return ERR::Okay;
   }

   inline ERR setPlayback(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[15];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setCompression(const LONG Value) noexcept {
      this->Compression = Value;
      return ERR::Okay;
   }

   inline ERR setBytesPerSecond(const LONG Value) noexcept {
      this->BytesPerSecond = Value;
      return ERR::Okay;
   }

   inline ERR setBitsPerSample(const LONG Value) noexcept {
      this->BitsPerSample = Value;
      return ERR::Okay;
   }

   inline ERR setAudio(OBJECTID Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->AudioID = Value;
      return ERR::Okay;
   }

   inline ERR setLoopStart(const LONG Value) noexcept {
      this->LoopStart = Value;
      return ERR::Okay;
   }

   inline ERR setLoopEnd(const LONG Value) noexcept {
      this->LoopEnd = Value;
      return ERR::Okay;
   }

   inline ERR setStream(const STREAM Value) noexcept {
      this->Stream = Value;
      return ERR::Okay;
   }

   inline ERR setOnStop(FUNCTION Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   template <class T> inline ERR setPath(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[21];
      return field->WriteValue(target, field, 0x08800500, to_cstring(Value), 1);
   }

   template <class T> inline ERR setNote(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[20];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

};

#ifdef PARASOL_STATIC
#define JUMPTABLE_AUDIO static struct AudioBase *AudioBase;
#else
#define JUMPTABLE_AUDIO struct AudioBase *AudioBase;
#endif

struct AudioBase {
#ifndef PARASOL_STATIC
   ERR (*_MixContinue)(objAudio * Audio, LONG Handle);
   ERR (*_MixFrequency)(objAudio * Audio, LONG Handle, LONG Frequency);
   ERR (*_MixMute)(objAudio * Audio, LONG Handle, LONG Mute);
   ERR (*_MixPan)(objAudio * Audio, LONG Handle, DOUBLE Pan);
   ERR (*_MixPlay)(objAudio * Audio, LONG Handle, LONG Position);
   ERR (*_MixRate)(objAudio * Audio, LONG Handle, LONG Rate);
   ERR (*_MixSample)(objAudio * Audio, LONG Handle, LONG Sample);
   ERR (*_MixStop)(objAudio * Audio, LONG Handle);
   ERR (*_MixStopLoop)(objAudio * Audio, LONG Handle);
   ERR (*_MixVolume)(objAudio * Audio, LONG Handle, DOUBLE Volume);
   ERR (*_MixStartSequence)(objAudio * Audio, LONG Handle);
   ERR (*_MixEndSequence)(objAudio * Audio, LONG Handle);
#endif // PARASOL_STATIC
};

#ifndef PRV_AUDIO_MODULE
#ifndef PARASOL_STATIC
extern struct AudioBase *AudioBase;
inline ERR sndMixContinue(objAudio * Audio, LONG Handle) { return AudioBase->_MixContinue(Audio,Handle); }
inline ERR sndMixFrequency(objAudio * Audio, LONG Handle, LONG Frequency) { return AudioBase->_MixFrequency(Audio,Handle,Frequency); }
inline ERR sndMixMute(objAudio * Audio, LONG Handle, LONG Mute) { return AudioBase->_MixMute(Audio,Handle,Mute); }
inline ERR sndMixPan(objAudio * Audio, LONG Handle, DOUBLE Pan) { return AudioBase->_MixPan(Audio,Handle,Pan); }
inline ERR sndMixPlay(objAudio * Audio, LONG Handle, LONG Position) { return AudioBase->_MixPlay(Audio,Handle,Position); }
inline ERR sndMixRate(objAudio * Audio, LONG Handle, LONG Rate) { return AudioBase->_MixRate(Audio,Handle,Rate); }
inline ERR sndMixSample(objAudio * Audio, LONG Handle, LONG Sample) { return AudioBase->_MixSample(Audio,Handle,Sample); }
inline ERR sndMixStop(objAudio * Audio, LONG Handle) { return AudioBase->_MixStop(Audio,Handle); }
inline ERR sndMixStopLoop(objAudio * Audio, LONG Handle) { return AudioBase->_MixStopLoop(Audio,Handle); }
inline ERR sndMixVolume(objAudio * Audio, LONG Handle, DOUBLE Volume) { return AudioBase->_MixVolume(Audio,Handle,Volume); }
inline ERR sndMixStartSequence(objAudio * Audio, LONG Handle) { return AudioBase->_MixStartSequence(Audio,Handle); }
inline ERR sndMixEndSequence(objAudio * Audio, LONG Handle) { return AudioBase->_MixEndSequence(Audio,Handle); }
#else
extern "C" {
extern ERR sndMixContinue(objAudio * Audio, LONG Handle);
extern ERR sndMixFrequency(objAudio * Audio, LONG Handle, LONG Frequency);
extern ERR sndMixMute(objAudio * Audio, LONG Handle, LONG Mute);
extern ERR sndMixPan(objAudio * Audio, LONG Handle, DOUBLE Pan);
extern ERR sndMixPlay(objAudio * Audio, LONG Handle, LONG Position);
extern ERR sndMixRate(objAudio * Audio, LONG Handle, LONG Rate);
extern ERR sndMixSample(objAudio * Audio, LONG Handle, LONG Sample);
extern ERR sndMixStop(objAudio * Audio, LONG Handle);
extern ERR sndMixStopLoop(objAudio * Audio, LONG Handle);
extern ERR sndMixVolume(objAudio * Audio, LONG Handle, DOUBLE Volume);
extern ERR sndMixStartSequence(objAudio * Audio, LONG Handle);
extern ERR sndMixEndSequence(objAudio * Audio, LONG Handle);
}
#endif // PARASOL_STATIC
#endif

