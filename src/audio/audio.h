
#ifdef _WIN32
#define MIX_INTERVAL 0.02
#else
#define MIX_INTERVAL 0.01
#endif

class extAudio : public objAudio {
   public:
   struct ChannelSet Channels[MAX_CHANNELSETS]; // Channels are grouped into sets, which are allocated on a per-task basis
   struct AudioSample  *Samples;
   struct VolumeCtl    *VolumeCtl;
   const MixRoutineSet *MixRoutines;
   APTR  BufferMemory;
   APTR  MixBuffer;
   FLOAT *BFMemory;               // Byte/Float table memory
   APTR  TaskRemovedHandle;
   APTR  UserLoginHandle;
   #ifdef ALSA_ENABLED
   UBYTE *AudioBuffer;
   LONG  AudioBufferSize;
   snd_pcm_t *Handle;
   snd_mixer_t *MixHandle;
   snd_output_t *sndlog;
   #endif
   LONG  MixBufferSize;
   LONG  TotalSamples;
   LONG  MixElements;
   TIMER Timer;
   WORD  SampleBitSize;
   WORD  MixBitSize;
   UBYTE Stereo;                  // TRUE/FALSE for active stereo mode
   BYTE  Mute;
   BYTE  MasterVolume;
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
