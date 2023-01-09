
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
#ifdef ALSA_ENABLED
   UBYTE *AudioBuffer;
   LONG  AudioBufferSize;
   snd_pcm_t *Handle;
   snd_mixer_t *MixHandle;
   snd_output_t *sndlog;
#endif
   LONG VolumeCtlTotal;
   char Device[28];

   inline struct AudioChannel * GetChannel(LONG Handle) {
      return &this->Channels[Handle>>16].Channel[Handle & 0xffff];
   }
};

class extSound : public objSound {
   public:
   struct KeyStore *Fields;
   UBYTE    Header[128];
   LONG     Format;         // The format of the sound data
   LONG     DataOffset;     // Start of raw audio data within the source file
   TIMER    Timer;
   STRING   Path;
   STRING   Description;
   STRING   Disclaimer;
   LONG     Note;               // Note to play back (e.g. C, C#, G...)
   char     NoteString[4];
   struct WAVEFormat *WAVE;
   UBYTE    PlatformData[128];  // Data area for holding platform/hardware specific information
   LONG     Alignment;          // Byte alignment value
};
