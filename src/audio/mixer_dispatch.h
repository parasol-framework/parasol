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

// Audio configuration structure
struct AudioConfig {
   bool stereo_output;
   bool use_interpolation;

   // Default constructor
   AudioConfig() : stereo_output(false), use_interpolation(false) {}

   AudioConfig(bool stereo_out, bool interpolation)
      : stereo_output(stereo_out), use_interpolation(interpolation) {}
};

// Primary mixing dispatch function
class AudioMixer {
public:
   static int dispatch_mix(const AudioConfig& config, SFM sample_format, const MixingParams& params);
};

// Convenience functions for common mixing scenarios
namespace mixer_helpers {
   // Get sample type information at compile time
   template<SFM format>
   constexpr bool is_stereo_sample() {
      return (format IS SFM::U8_BIT_STEREO) or (format IS SFM::S16_BIT_STEREO);
   }

   template<SFM format>
   constexpr bool is_16bit_sample() {
      return (format IS SFM::S16_BIT_MONO) or (format IS SFM::S16_BIT_STEREO);
   }

   // Runtime type information helpers
   inline bool is_stereo_sample(SFM format) {
      return (format IS SFM::U8_BIT_STEREO) or (format IS SFM::S16_BIT_STEREO);
   }

   inline bool is_16bit_sample(SFM format) {
      return (format IS SFM::S16_BIT_MONO) or (format IS SFM::S16_BIT_STEREO);
   }
}