
static const LONG MIX_BUF_LEN = 4; // Mixing buffer length 1/n of a second

// The mixer interval must trigger more often than the size limit imposed by MIX_BUF_LEN.

#ifdef _WIN32
#define MIX_INTERVAL 0.1
#else
#define MIX_INTERVAL 0.01
#endif

enum SAMPLE : int {};
enum BYTELEN : int {};

inline BYTELEN operator + (BYTELEN a, BYTELEN b) { return BYTELEN(((int)a) + ((int)b)); }
inline BYTELEN &operator += (BYTELEN &a, BYTELEN b) { return (BYTELEN &)(((int &)a) += ((int)b)); }

// Audio channel commands

enum class CMD : LONG {
   START_SEQUENCE=1,
   END_SEQUENCE,
   SAMPLE,
   VOLUME,
   PAN,
   FREQUENCY,
   RATE,
   STOP,
   STOP_LOOPING,
   POSITION,
   PLAY,
   MUTE,
   SET_LENGTH,
   CONTINUE
};

// Sample shift - value used for converting total data size down to samples.

inline const LONG sample_shift(const LONG Type)
{
   switch (Type) {
      case SFM_U8_BIT_STEREO:
      case SFM_S16_BIT_MONO: return 1;
      case SFM_S16_BIT_STEREO: return 2;
   }
   return 0;
}

typedef struct WAVEFormat {
   WORD Format;               // Type of WAVE data in the chunk: RAW or ADPCM
   WORD Channels;             // Number of channels, 1=mono, 2=stereo
   LONG Frequency;            // Playback frequency
   LONG AvgBytesPerSecond;    // Channels * SamplesPerSecond * (BitsPerSample / 8)
   WORD BlockAlign;           // Channels * (BitsPerSample / 8)
   WORD BitsPerSample;        // Bits per sample
   WORD ExtraLength;
} WAVEFORMATEX;

typedef LONG (*MixRoutine)(APTR, LONG, LONG, LONG, FLOAT, FLOAT);

#define WAVE_RAW    0x0001    // Uncompressed waveform data.
#define WAVE_ADPCM  0x0002    // ADPCM compressed waveform data.

const LONG DEFAULT_BUFFER_SIZE = 8096; // Measured in samples, not bytes

struct PlatformData { void *Void; };

struct AudioSample {
   FUNCTION Callback;     // For feeding audio streams.
   FUNCTION OnStop;       // Called when playback stops.
   UBYTE *  Data;         // Pointer to the sample data.
   SAMPLE   Loop1Start;   // Start of the first loop
   SAMPLE   Loop1End;     // End of the first loop
   SAMPLE   Loop2Start;   // Start of the second loop
   SAMPLE   Loop2End;     // End of the second loop
   SAMPLE   SampleLength; // Length of the sample/buffer in Data, measured in samples
   BYTELEN  StreamLength; // Total byte-length of the sample data that is being streamed.
   BYTELEN  PlayPos;      // Current read position relative to StreamLength/SampleLength, measured in bytes
   LOOP     LoopMode;     // Loop mode (single, double)
   UBYTE    SampleType;   // Type of sample (bit format)
   LTYPE    Loop1Type;    // First loop type (unidirectional, bidirectional)
   LTYPE    Loop2Type;    // Second loop type (unidirectional, bidirectional)
   bool     Stream;       // True if this is a stream

   AudioSample() {
      Data     = NULL;
      Stream   = false;
      clear();
   }

   ~AudioSample() {
      clear();
   }

   void clear() {
      if (Data) FreeResource(Data);

      Data         = NULL;
      SampleLength = SAMPLE(0);
      Loop1Start   = SAMPLE(0);
      Loop1End     = SAMPLE(0);
      Loop2Start   = SAMPLE(0);
      Loop2End     = SAMPLE(0);
      StreamLength = BYTELEN(0);
      SampleType   = 0;
      LoopMode     = LOOP::NIL;
      Loop1Type    = LTYPE::NIL;
      Loop2Type    = LTYPE::NIL;
   }
};

struct AudioCommand {
   CMD  CommandID;    // Command ID
   LONG Handle;       // Channel handle
   DOUBLE Data;       // Special data related to the command ID

   AudioCommand(CMD pCommandID, LONG pHandle, LONG pData = 0) :
      CommandID(pCommandID), Handle(pHandle), Data(pData) { }
};

struct AudioChannel {
   DOUBLE   LVolume;        // Current left speaker volume after applying Pan (0 - 1.0)
   DOUBLE   RVolume;        // Current right speaker volume after applying Pan (0 - 1.0)
   DOUBLE   LVolumeTarget;  // Volume target when fading or ramping
   DOUBLE   RVolumeTarget;  // Volume target when fading or ramping
   DOUBLE   Volume;         // Playing volume (0 - 1.0)
   DOUBLE   Pan;            // Pan value (-1.0 - 1.0)
   LARGE    EndTime;        // Anticipated end-time of playing the current sample, if OnStop is defined in the sample.
   LONG     SampleHandle;   // Sample index, direct lookup into extAudio->Samples
   CHF      Flags;          // Special flags
   LONG     Position;       // Current playing/mixing byte position within Sample.
   LONG     Frequency;      // Playback frequency
   LONG     PositionLow;    // Playing position, lower bits
   BYTE     Priority;       // Priority of the sound that has been assigned to this channel
   CHS      State;          // Channel state
   BYTE     LoopIndex;      // The current active loop (either 0, 1 or 2)
   bool     Buffering;

   bool active() {
      return Frequency ? true : false;
   }

   inline bool isStopped() {
      return ((State IS CHS::STOPPED) or (State IS CHS::FINISHED));
   }
};

struct ChannelSet {
   std::vector<AudioChannel> Channel;  // Array of channel objects
   std::vector<AudioChannel> Shadow;   // Array of shadow channels for oversampling
   std::vector<AudioCommand> Commands; // Buffered commands.
   LONG UpdateRate; // Update rate, measured in milliseconds
   LONG MixLeft;    // Amount of mix elements left before the next command-update occurs

   ChannelSet() {
      clear();
   }

   ~ChannelSet() {
      clear();
   }

   void clear() {
      Channel.clear();
      Shadow.clear();
      UpdateRate = 0;
      MixLeft    = 0;
   }
};

struct VolumeCtl {
   std::string Name;     // Name of the mixer
   VCF Flags;            // Special flags identifying the mixer's attributes.
   std::vector<FLOAT> Channels; // A variable length array of channel volumes.

   VolumeCtl() {
      Flags = VCF::NIL;
      Channels = { -1 }; // A -1 value leaves the current system volume as-is.
   }

   VolumeCtl(std::string pName, VCF pFlags = VCF::NIL, DOUBLE pVolume = -1) {
      Name = pName;
      Flags = pFlags;
      Channels = { (FLOAT)pVolume };
   }
};

class extAudio : public objAudio {
   public:
   std::vector<ChannelSet> Sets; // Channels are grouped into sets.  Index 0 is a dummy entry.
   std::vector<AudioSample> Samples; // Buffered samples loaded into the audio object.
   std::vector<VolumeCtl> Volumes;
   std::vector<std::pair<LARGE, LONG>> MixTimers;
   MixRoutine *MixRoutines;
   APTR  MixBuffer;
   APTR  TaskRemovedHandle;
   APTR  UserLoginHandle;
   #ifdef _WIN32
      UBYTE  PlatformData[128];  // Data area for holding platform/hardware specific information
   #endif
   #ifdef ALSA_ENABLED
      UBYTE *AudioBuffer;
      snd_pcm_t    *Handle;
      snd_mixer_t  *MixHandle;
      snd_output_t *sndlog;
      LONG  AudioBufferSize;
   #endif
   DOUBLE MasterVolume;
   TIMER Timer;
   LONG  MixBufferSize;
   LONG  MixElements;
   LONG  MaxChannels;    // Recommended maximum mixing channels for Sound class
   std::string Device;
   BYTE  SampleBitSize;
   bool  Stereo;
   bool  Mute;
   bool  Initialising;

   inline struct AudioChannel * GetChannel(LONG Handle) {
      return &this->Sets[Handle>>16].Channel[Handle & 0xffff];
   }

   inline struct AudioChannel * GetShadow(LONG Handle) {
      return &this->Sets[Handle>>16].Shadow[Handle & 0xffff];
   }

   inline LONG MixLeft(LONG Value) {
      if (!Value) return 0;
      return (((100 * (LARGE)OutputRate) / (Value * 40)) + 1) & 0xfffffffe;
   }

   inline void finish(AudioChannel &Channel, bool Notify) {
      if (!Channel.isStopped()) {
         Channel.State = CHS::FINISHED;
         if ((Channel.SampleHandle) and (Notify)) {
            if (PreciseTime() < Channel.EndTime) {
               this->MixTimers.emplace_back(Channel.EndTime, Channel.SampleHandle);
            }
            else audio_stopped_event(*this, Channel.SampleHandle);
         }
      }
      else Channel.State = CHS::FINISHED;
   }
};

class extSound : public objSound {
   public:
   FUNCTION OnStop;
   UBYTE  Header[32];
   #ifdef _WIN32
   UBYTE  PlatformData[64];   // Data area for holding platform/hardware specific information
   #endif
   std::unordered_map<std::string, std::string> Tags;
   objFile *File;
   STRING Path;
   TIMER  StreamTimer;        // Timer to regularly trigger for provisioning streaming data.
   TIMER  PlaybackTimer;      // Timer to trigger when playback ends.
   LONG   Format;             // The format of the sound data
   LONG   DataOffset;         // Start of raw audio data within the source file
   LONG   Note;               // Note to play back (e.g. C, C#, G...)
   char   NoteString[4];
   bool   Active;             // True once the sound is registered with the audio driver or mixer.
};

struct BufferCommand {
   CMD CommandID;
   ERROR (*Routine)(extAudio *Self, APTR);
};
