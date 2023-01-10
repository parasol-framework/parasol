
#ifdef _WIN32
#define MIX_INTERVAL 0.02
#else
#define MIX_INTERVAL 0.01
#endif

typedef void (*MixRoutine)(ULONG, LONG, FLOAT, FLOAT);

#define MAX_CHANNELSETS 8
#define DEFAULT_BUFFER_SIZE 8096 // Measured in samples, not bytes

class extAudio : public objAudio {
   public:
   struct ChannelSet Channels[MAX_CHANNELSETS]; // Channels are grouped into sets
   struct AudioSample *Samples;
   struct VolumeCtl *VolumeCtl;
   MixRoutine *MixRoutines;
   APTR  MixBuffer;
   APTR  TaskRemovedHandle;
   APTR  UserLoginHandle;
   #ifdef ALSA_ENABLED
   UBYTE *AudioBuffer;
   snd_pcm_t *Handle;
   snd_mixer_t *MixHandle;
   snd_output_t *sndlog;
   LONG  AudioBufferSize;
   #endif
   DOUBLE MasterVolume;
   LONG  MixBufferSize;
   LONG  TotalSamples;
   LONG  MixElements;
   TIMER Timer;
   WORD  SampleBitSize;
   WORD  MixBitSize;
   bool  Stereo;                  // TRUE/FALSE for active stereo mode
   bool  Mute;
   BYTE  Initialising;
   LONG  VolumeCtlTotal;
   char  Device[28];

   inline struct AudioChannel * GetChannel(LONG Handle) {
      return &this->Channels[Handle>>16].Channel[Handle & 0xffff];
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
   LONG   Format;         // The format of the sound data
   LONG   DataOffset;     // Start of raw audio data within the source file
   LONG   Note;               // Note to play back (e.g. C, C#, G...)
   LONG   Alignment;          // Byte alignment value
   char   NoteString[4];
};
