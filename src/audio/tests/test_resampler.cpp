// Numerical contracts use the actual dispatch, independent expected waveforms and partitioned renders.
namespace audio_tests_resampler {

static void kernels(AudioTestContext &Test)
{
   constexpr int count = 128;
   for (int fixture = 0; fixture < 4; ++fixture) {
      int16_t source[count];
      for (int i = 0; i < count; ++i) {
         source[i] = fixture IS 0 ? (i IS 64 ? 30000 : 0) : fixture IS 1 ? 12345 :
            fixture IS 2 ? i * 200 - 12000 : int16_t(25000 * std::sin(i * 0.2));
      }
      for (auto mode : { ResamplerMode::NEAREST, ResamplerMode::LINEAR, ResamplerMode::CUBIC }) {
         for (int phase : { 0, 1, 16384, 32768, 65535 }) {
            float reference[count]{};
            for (int block : { count, 1, 7 }) {
               float output[count]{};
               int position = phase;
               for (int i = 0; i < count;) {
                  auto dest = output + i;
                  const int frames = std::min(block, count - i);
                  MixingParams params{ source, position, frames, 1, 1, &dest, 0, count };
                  set_mix_step(65536);
                  position = AudioMixer::dispatch_mix(AudioConfig(false, mode), PCM::S16_MONO, params);
                  i += frames;
               }
               if (block IS count) std::copy_n(output, count, reference);
               for (int i = 0; i < count; ++i) {
                  AUDIO_CHECK(output[i] IS reference[i]);
                  AUDIO_CHECK(std::isfinite(output[i]) and std::abs(output[i]) <= 1);
                  if (!phase) AUDIO_CHECK(output[i] IS float(source[i]) / 32768);
                  if (fixture IS 1) AUDIO_CHECK(output[i] IS float(source[i]) / 32768);
                  if (fixture IS 2 and i > 0 and i < count - 2 and mode != ResamplerMode::NEAREST) {
                     AUDIO_CHECK(std::abs(output[i] - (source[i] + 200.0 * phase / 65536) / 32768) < 6e-8);
                  }
               }
            }
         }
      }
   }
   // Sweep pass-band frequencies to 0.2 cycles per source frame, with all fractional phases.
   double linear_error = 0, cubic_error = 0;
   for (int frequency = 1; frequency <= 20; ++frequency) for (int i = 2; i < 100; ++i) {
      const double omega = 2 * std::numbers::pi * frequency / 100;
      for (int fraction = 1; fraction < 16; ++fraction) {
         const double phase = fraction / 16.0;
         const double expected = std::sin((i + phase) * omega);
         const double current = std::sin(i * omega), next = std::sin((i + 1) * omega);
         const double linear = current + phase * (next - current);
         const double cubic = resampler_cubic(std::sin((i - 1) * omega), current, next,
            std::sin((i + 2) * omega), phase);
         linear_error += (linear - expected) * (linear - expected);
         cubic_error += (cubic - expected) * (cubic - expected);
      }
   }
   AUDIO_CHECK(cubic_error < linear_error * 0.2);
}

static void transitions(AudioTestContext &Test)
{
   int16_t source[] = { -10000, 20000, -20000, 10000 };
   float reference[96]{};
   for (int block : { 32, 1, 7 }) {
      ResamplerState state;
      float output[96]{};
      set_mix_step(0);
      for (int i = 0; i < 96;) {
         const auto mode = i < 8 ? ResamplerMode::NEAREST : i < 20 ? ResamplerMode::CUBIC : ResamplerMode::LINEAR;
         const int boundary = i < 8 ? 8 : i < 20 ? 20 : 96;
         const int frames = std::min(block, boundary - i);
         auto dest = output + i;
         MixingParams params{source, 98304, frames, 1, 1, &dest, 0, 4, 0, 0, LTYPE::NIL, &state};
         AudioMixer::dispatch_mix(AudioConfig(false, mode), PCM::S16_MONO, params);
         i += frames;
      }
      if (block IS 32) std::copy_n(output, 96, reference);
      AUDIO_CHECK(std::equal(output, output + 96, reference));
      AUDIO_CHECK(state.Remaining IS 0 and state.Weights[1] IS 1);
      AUDIO_CHECK(output[51] IS 0 and output[95] IS 0);
      for (int i = 1; i < 96; ++i) AUDIO_CHECK(std::abs(output[i] - output[i-1]) < 0.03);
   }
}

#ifdef AUDIO_WORKER
static void boundaries(AudioTestContext &Test)
{
   for (auto format : { PCM::U8_MONO, PCM::U8_STEREO, PCM::S16_MONO, PCM::S16_STEREO })
   for (bool stereo : { false, true }) for (int quality : { 0, 10, 33 })
   for (int rate : { 44100, 48000, 96000 }) for (int ratio : { 1, 2, 4 })
   for (auto loop : { LTYPE::NIL, LTYPE::UNIDIRECTIONAL, LTYPE::BIDIRECTIONAL }) {
      extAudio *audio;
      AUDIO_REQUIRE(NewObject(CLASSID::AUDIO, &audio) IS ERR::Okay);
      std::unique_ptr<extAudio, DeleteObject<extAudio>> owner(audio);
      SET_Quality(audio, quality);
      AUDIO_REQUIRE(InitObject(audio) IS ERR::Okay);
      audio->Sets.resize(1);
      audio->OutputRate = rate;
      audio->Stereo = stereo;
      audio->MasterVolume = 1;
      audio->Mute = false;
      audio->snapshot_quality();
      audio->Samples.resize(2);
      auto &sample = audio->Samples[1];
      const bool source_stereo = pcm_stereo(format);
      const bool wide = format IS PCM::S16_MONO or format IS PCM::S16_STEREO;
      const int frame_bytes = (source_stereo ? 2 : 1) * (wide ? 2 : 1);
      sample.FrameBytes = frame_bytes;
      sample.SampleType = format;
      sample.SampleLength = SAMPLE(19);
      sample.Loop1Type = loop;
      sample.Loop1Start = SAMPLE(3);
      sample.Loop1End = SAMPLE(19);
      sample.Data.resize(19 * frame_bytes);
      for (int i = 0; i < 19 * (source_stereo ? 2 : 1); ++i) {
         if (wide) ((int16_t *)sample.Data.data())[i] = int16_t(24000 * std::sin(i * 0.7));
         else sample.Data[i] = uint8_t(128 + 100 * std::sin(i * 0.7));
      }
      const auto resident_data = sample.Data;
      AudioChannel initial{};
      initial.SampleHandle = 1;
      initial.Frequency = rate * ratio / 2;
      initial.State = CHS::PLAYING;
      initial.LVolume = initial.RVolume = 1;
      const int channels = stereo ? 2 : 1;
      std::array<float, 240> reference{};
      AudioChannel expected{};
      for (bool backward : { false, true }) {
         initial.Flags = backward ? CHF::BACKWARD : CHF::NIL;
         initial.Position = backward ? 17 : 0;
         initial.PositionLow = 32768;
         for (int block : { 120, 1, 7 }) {
            auto channel = initial;
            std::array<float, 240> output{};
            for (int i = 0; i < 120; i += block) {
               mix_channel(audio, channel, std::min(block, 120 - i), output.data() + i * channels);
            }
            if (block IS 120) { reference = output; expected = channel; }
            else {
               AUDIO_CHECK(output IS reference);
               AUDIO_CHECK(channel.Position IS expected.Position and channel.PositionLow IS expected.PositionLow);
            }
            for (auto value : output) AUDIO_CHECK(std::isfinite(value) and std::abs(value) <= 1);
         }
         if (backward or loop IS LTYPE::BIDIRECTIONAL) continue; // Stream cursors consume forward only.
         // Materialise logical refill data in a rotated ring: the current frame and future support wrap physically.
         constexpr int available = 256;
         const int stored = loop IS LTYPE::NIL ? 19 : available;
         sample.Data.assign(available * frame_bytes, 0);
         sample.Stream = sample.Prefilled = sample.EndOfSource = true;
         for (int block : { 120, 1, 7 }) {
            sample.Ring.Read = (available - 2) * frame_bytes;
            sample.Ring.Used = stored * frame_bytes;
            sample.HasHistory = false;
            sample.PlayPos = BYTELEN(0);
            for (int i = 0; i < stored; ++i) {
               const int index = i < 19 ? i : 3 + (i - 19) % 16;
               for (int byte = 0; byte < frame_bytes; ++byte) {
                  sample.Data[(sample.Ring.Read + i * frame_bytes + byte) % sample.Data.size()] =
                     resident_data[index * frame_bytes + byte];
               }
            }
            auto channel = initial;
            std::array<float, 240> output{};
            for (int i = 0; i < 120; i += block) {
               mix_channel(audio, channel, std::min(block, 120 - i), output.data() + i * channels);
            }
            AUDIO_CHECK(output IS reference);
         }
         sample.Stream = false;
         sample.Data = resident_data;
      }
   }
}
#endif

static void short_loops(AudioTestContext &Test)
{
   extAudio *audio;
   AUDIO_REQUIRE(NewObject(CLASSID::AUDIO, &audio) IS ERR::Okay);
   std::unique_ptr<extAudio, DeleteObject<extAudio>> owner(audio);
   AUDIO_REQUIRE(InitObject(audio) IS ERR::Okay);
   audio->OutputRate = 48000;
   audio->Stereo = false;
   audio->MasterVolume = 1;
   audio->Mute = false;
   audio->Sets.resize(1);
   audio->Samples.resize(2);
   auto &sample = audio->Samples[1];
   sample.SampleType = PCM::S16_MONO;
   sample.FrameBytes = 2;
   for (int length = 1; length <= 4; ++length)
   for (auto loop : { LTYPE::UNIDIRECTIONAL, LTYPE::BIDIRECTIONAL })
   for (int frequency : { 48000, 504000 }) for (int quality : { 0, 10, 33 }) {
      SET_Quality(audio, quality);
      audio->snapshot_quality();
      sample.SampleLength = sample.Loop1End = SAMPLE(length);
      sample.Loop1Start = SAMPLE(0);
      sample.Loop1Type = loop;
      sample.Data.resize(length * 2);
      for (int i = 0; i < length; ++i) ((int16_t *)sample.Data.data())[i] = int16_t(i * 8000 - 12000);
      int16_t expanded[400];
      for (int i = 0; i < 400; ++i) {
         int index = i % (loop IS LTYPE::BIDIRECTIONAL ? 2 * length : length);
         if (index >= length) index = 2 * length - 1 - index;
         expanded[i] = ((int16_t *)sample.Data.data())[index];
      }
      float expected[32]{}, actual[32]{};
      auto dest = expected;
      set_mix_step(int(int64_t(frequency) * 65536 / 48000));
      MixingParams params{expanded, 0, 32, 1, 1, &dest, 0, 400};
      AudioMixer::dispatch_mix(audio->WindowConfig, PCM::S16_MONO, params);
      AudioChannel channel{};
      channel.SampleHandle = 1;
      channel.State = CHS::PLAYING;
      channel.Frequency = frequency;
      channel.LVolume = channel.RVolume = 1;
      mix_channel(audio, channel, 32, actual);
      for (int i = 0; i < 32; ++i) {
         // Draft retains spatial floor sampling, so fractional reverse positions select the lower spatial
         // sample (the next chronological sample). Linear/cubic are invariant under reversing the support.
         const double logical = double(i) * frequency / 48000;
         if (!quality and loop IS LTYPE::BIDIRECTIONAL and std::fmod(logical, 2 * length) >= length) {
            expected[i] = float(expanded[int(std::ceil(logical))]) / 32768;
         }
         AUDIO_CHECK(std::abs(actual[i] - expected[i]) < 6e-8);
      }
      AUDIO_CHECK(!channel.isStopped());
   }
}

static void run(AudioTestContext &Test)
{
   kernels(Test);
   short_loops(Test);
   transitions(Test);
#ifdef AUDIO_WORKER
   boundaries(Test);
#endif
   set_mix_step(65536);
}
}
