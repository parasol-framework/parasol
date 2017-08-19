
#define GetChannel(a) &Self->Channels[(a)>>16].Channel[(a) & 0xffff];

#ifdef __linux__
#define MIX_INTERVAL -(0.01)
#elif _WIN32
#define MIX_INTERVAL -(0.02)
#endif

struct globalaudio {
   FLOAT Volume;        // Current system-wide audio volume
};
