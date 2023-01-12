
#ifdef _WIN32
#define MIX_INTERVAL 0.02
#else
#define MIX_INTERVAL 0.01
#endif

typedef void (*MixRoutine)(LONG, LONG, FLOAT, FLOAT);

#define MAX_CHANNELSETS 8
#define DEFAULT_BUFFER_SIZE 8096 // Measured in samples, not bytes

struct AudioSample {
   UBYTE *  Data;         // Pointer to the sample data.
   OBJECTID StreamID;     // An object to use for streaming
   LONG     SampleLength; // Length of the sample data, in bytes
   LONG     Loop1Start;   // Start of the first loop
   LONG     Loop1End;     // End of the first loop
   LONG     Loop2Start;   // Start of the second loop
   LONG     Loop2End;     // End of the second loop
   LONG     SeekStart;    // Offset to use when seeking to the start of sample data.
   LONG     StreamLength; // Total byte-length of the sample data that is being streamed.
   LONG     BufferLength; // Total byte-length of the stream buffer.
   UBYTE    SampleType;   // Type of sample (bit format)
   BYTE     LoopMode;     // Loop mode (single, double)
   BYTE     Loop1Type;    // First loop type (unidirectional, bidirectional)
   BYTE     Loop2Type;    // Second loop type (unidirectional, bidirectional)
   bool     Free;         // Set to true if the StreamID object should be terminated on sample removal.

   AudioSample() {
      Data     = NULL;
      StreamID = 0;
      Free     = false;
      clear();
   }

   ~AudioSample() {
      clear();
   }

   void clear() {
      if (Data) FreeResource(Data);
      if ((Free) and (StreamID)) acFree(StreamID);

      Data         = NULL;
      StreamID     = 0;
      SampleLength = 0;
      Loop1Start   = 0;
      Loop1End     = 0;
      Loop2Start   = 0;
      Loop2End     = 0;
      SeekStart    = 0;
      StreamLength = 0;
      BufferLength = 0;
      SampleType   = 0;
      LoopMode     = 0;
      Loop1Type    = 0;
      Loop2Type    = 0;
      Free         = false;
   }
};

struct AudioCommand {
   LONG CommandID;    // Command ID
   LONG Handle;       // Channel handle
   LONG Data;         // Special data related to the command ID
};

struct AudioChannel {
   DOUBLE   LVolume;        // Current left speaker volume after applying Pan (0 - 1.0)
   DOUBLE   RVolume;        // Current right speaker volume after applying Pan (0 - 1.0)
   DOUBLE   LVolumeTarget;  // Volume target when fading or ramping
   DOUBLE   RVolumeTarget;  // Volume target when fading or ramping
   DOUBLE   Volume;         // Playing volume (0 - 1.0)
   DOUBLE   Pan;            // Pan value (-1.0 - 1.0)
   OBJECTID SoundID;        // ID of the sound object using this channel, if relevant
   LONG     SampleHandle;   // Sample index, direct lookup into extAudio->Samples
   LONG     Flags;          // Special flags
   LONG     Position;       // Current playing/mixing position
   LONG     Frequency;      // Playback frequency
   LONG     StreamPos;      // Current read position within the referenced audio stream
   LONG     PositionLow;    // Playing position, lower bits
   BYTE     Priority;       // Priority of the sound that has been assigned to this channel
   BYTE     State;          // Channel state
   BYTE     LoopIndex;      // The current active loop (either 0, 1 or 2)

   bool active() {
      return Frequency ? true : false;
   }
};

struct ChannelSet {
   AudioChannel * Channel;    // Array of channel objects
   std::vector<AudioCommand> Commands; // Buffered commands.  Can be empty for immediate command execution
   LONG UpdateRate;           // Update rate, measured in milliseconds
   LONG MixLeft;              // Amount of mix elements left before the next command-update occurs
   WORD Total;                // Total number of base channels
   WORD Actual;               // Total number of channels, including oversampling channels

   ChannelSet() {
      Channel = NULL;
      clear();
   }

   ~ChannelSet() {
      clear();
   }

   void clear() {
      if (Channel)  FreeResource(Channel);
      Channel    = NULL;
      UpdateRate = 0;
      MixLeft    = 0;
      Total      = 0;
      Actual     = 0;
   }
};

struct VolumeCtl {
   std::string Name;     // Name of the mixer
   LONG Flags;           // Special flags identifying the mixer's attributes.
   std::vector<FLOAT> Channels; // A variable length array of channel volumes.

   VolumeCtl() {
      Flags = 0;
      Channels = { -1 }; // A -1 value leaves the current system volume as-is.
   }
};

class extAudio : public objAudio {
   public:
   std::vector<ChannelSet> Sets; // Channels are grouped into sets.  Index 0 is a dummy entry.
   std::vector<AudioSample> Samples; // Buffered samples loaded into the audio object.
   std::vector<VolumeCtl> Volumes;
   MixRoutine *MixRoutines;
   APTR  MixBuffer;
   APTR  TaskRemovedHandle;
   APTR  UserLoginHandle;
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
   char  Device[28];
   BYTE  SampleBitSize;
   bool  Stereo;
   bool  Mute;
   bool  Initialising;

   inline struct AudioChannel * GetChannel(LONG Handle) {
      return &this->Sets[Handle>>16].Channel[Handle & 0xffff];
   }

   inline LONG MixLeft(LONG Value) {
      if (!Value) return 0;
      return (((100 * (LARGE)OutputRate) / (Value * 40)) + 1) & 0xfffffffe;
   }
};

class extSound : public objSound {
   public:
   UBYTE  Header[128];
   UBYTE  PlatformData[128];  // Data area for holding platform/hardware specific information
   struct KeyStore *Fields;
   struct WAVEFormat *WAVE;
   STRING Path;
   STRING Description;
   STRING Disclaimer;
   TIMER  Timer;
   LONG   Format;             // The format of the sound data
   LONG   DataOffset;         // Start of raw audio data within the source file
   LONG   Note;               // Note to play back (e.g. C, C#, G...)
   LONG   Alignment;          // Byte alignment value
   char   NoteString[4];
};

struct BufferCommand {
   WORD CommandID;
   ERROR (*Routine)(extAudio *Self, APTR);
};
