#pragma once

// The four legacy processing profiles.  These names do not promise future resampler algorithms.
enum class AudioQualityMode { DRAFT, LOW, STANDARD, HIGH };

struct AudioQualityProfile {
   int Minimum;
   int Maximum;
   AudioQualityMode Mode;
   ADF Flags;
   bool Resampling;
};

inline constexpr ADF glQualityFlags = ADF::OVER_SAMPLING | ADF::FILTER_LOW | ADF::FILTER_HIGH;
inline constexpr AudioQualityProfile glQualityProfiles[] = {
   { 0, 9, AudioQualityMode::DRAFT, ADF::NIL, false },
   { 10, 32, AudioQualityMode::LOW, ADF::FILTER_LOW, false },
   { 33, 65, AudioQualityMode::STANDARD, ADF::FILTER_HIGH, false },
   { 66, 100, AudioQualityMode::HIGH, ADF::OVER_SAMPLING | ADF::FILTER_HIGH, true }
};

inline constexpr const AudioQualityProfile * audio_quality_profile(int Quality)
{
   for (const auto &profile : glQualityProfiles) {
      if (Quality >= profile.Minimum and Quality <= profile.Maximum) return &profile;
   }
   return nullptr;
}
