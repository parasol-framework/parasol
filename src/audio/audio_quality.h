#pragma once

// Requested processing profiles. High currently uses the completed Standard cubic kernel.
enum class AudioQualityMode { DRAFT, LOW, STANDARD, HIGH };

struct AudioQualityProfile {
   int Minimum;
   int Maximum;
   AudioQualityMode Mode;
   ADF Flags;
   ResamplerMode Resampler;
};

inline constexpr ADF glQualityFlags = ADF::OVER_SAMPLING | ADF::FILTER_LOW | ADF::FILTER_HIGH;
inline constexpr AudioQualityProfile glQualityProfiles[] = {
   { 0, 9, AudioQualityMode::DRAFT, ADF::NIL, ResamplerMode::NEAREST },
   { 10, 32, AudioQualityMode::LOW, ADF::OVER_SAMPLING | ADF::FILTER_LOW, ResamplerMode::LINEAR },
   { 33, 65, AudioQualityMode::STANDARD, ADF::OVER_SAMPLING | ADF::FILTER_HIGH, ResamplerMode::CUBIC },
   { 66, 100, AudioQualityMode::HIGH, ADF::OVER_SAMPLING | ADF::FILTER_HIGH, ResamplerMode::CUBIC }
};

inline constexpr const AudioQualityProfile * audio_quality_profile(int Quality)
{
   for (const auto &profile : glQualityProfiles) {
      if (Quality >= profile.Minimum and Quality <= profile.Maximum) return &profile;
   }
   return nullptr;
}

inline constexpr const AudioQualityProfile * audio_effective_quality_profile(int Quality)
{
   const auto profile = audio_quality_profile(Quality);
   return profile and profile->Mode IS AudioQualityMode::HIGH ? &glQualityProfiles[2] : profile;
}
