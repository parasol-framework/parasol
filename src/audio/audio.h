
#define GetChannel(a) &Self->Channels[(a)>>16].Channel[(a) & 0xffff];

#ifdef _WIN32
#define MIX_INTERVAL -(0.02)
#else
#define MIX_INTERVAL -(0.01)
#endif

struct globalaudio {
   FLOAT Volume;        // Current system-wide audio volume
};

class extAudio : public objAudio {
   public:
   struct ChannelSet Channels[MAX_CHANNELSETS]; // Channels are grouped into sets, which are allocated on a per-task basis
   struct AudioSample *Samples;
   struct VolumeCtl *VolumeCtl;
   const MixRoutineSet *MixRoutines;
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
};

class extSound : public objSound {
   public:
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
};
