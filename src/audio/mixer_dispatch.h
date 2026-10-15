#pragma once

enum class ResamplerMode { NEAREST, LINEAR, CUBIC };

// A retargeted transition starts from the current mixture, including rapid profile writes.
struct ResamplerState {
   bool Initialised = false;
   ResamplerMode Target = ResamplerMode::NEAREST;
   double Weights[3] = { 1, 0, 0 };
   double Start[3] = { 1, 0, 0 };
   int Remaining = 0;
};

struct MixingParams {
   APTR src;
   int src_pos;
   int total_samples;
   float left_vol;
   float right_vol;
   float **mix_dest;
   int64_t origin = 0;
   int64_t length = 0;
   int64_t loop_start = 0, loop_end = 0;
   LTYPE loop_type = LTYPE::NIL;
   ResamplerState *transition = nullptr;
   bool loop_history = false;
};

//********************************************************************************************************************
// Audio configuration structure

struct AudioConfig {
   bool stereo_output;
   ResamplerMode resampler = ResamplerMode::NEAREST;

   AudioConfig() : stereo_output(false) {}
   AudioConfig(bool StereoOutput, ResamplerMode Resampler)
      : stereo_output(StereoOutput), resampler(Resampler) {}

};

//********************************************************************************************************************
// Primary mixing dispatch function

class AudioMixer {
public:
   static int dispatch_mix(const AudioConfig &Config, PCM Format, const MixingParams &Params);
};
