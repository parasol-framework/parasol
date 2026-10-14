#pragma once

struct MixingParams {
   APTR src;
   int src_pos;
   int total_samples;
   int next_sample_offset;
   float left_vol;
   float right_vol;
   float **mix_dest;
};

//********************************************************************************************************************
// Audio configuration structure

struct AudioConfig {
   bool stereo_output;
   bool resampling;

   // Default constructor
   AudioConfig() : stereo_output(false), resampling(false) {}

   AudioConfig(bool StereoOutput, bool Resampling)
      : stereo_output(StereoOutput), resampling(Resampling) {}
};

//********************************************************************************************************************
// Primary mixing dispatch function

class AudioMixer {
public:
   static int dispatch_mix(const AudioConfig& config, PCM sample_format, const MixingParams& params);
};
