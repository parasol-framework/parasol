
#define GetChannel(a) &Self->Channels[(a)>>16].Channel[(a) & 0xffff];

#ifdef _WIN32
#define MIX_INTERVAL -(0.02)
#else
#define MIX_INTERVAL -(0.01)
#endif

struct globalaudio {
   FLOAT Volume;        // Current system-wide audio volume
};
