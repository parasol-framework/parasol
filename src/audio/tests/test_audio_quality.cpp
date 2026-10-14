// Quality contract tests use the real field setters, configuration parser and device-free mixer.
namespace audio_tests_audio_quality {

static ADF legacy_flags(int Quality)
{
   if (Quality < 10) return ADF::NIL;
   if (Quality < 33) return ADF::FILTER_LOW;
   if (Quality < 66) return ADF::FILTER_HIGH;
   return ADF::OVER_SAMPLING | ADF::FILTER_HIGH;
}

static void boundaries(AudioTestContext &Test)
{
   extAudio *audio;
   AUDIO_REQUIRE(NewObject(CLASSID::AUDIO, &audio) IS ERR::Okay);
   std::unique_ptr<extAudio, DeleteObject<extAudio>> owner(audio);
   AUDIO_REQUIRE(SET_Quality(audio, 80) IS ERR::Okay);
   for (int quality : { -1, 0, 9, 10, 32, 33, 65, 66, 100, 101 }) {
      const int previous = audio->Quality;
      const auto previous_flags = audio->Flags;
      const auto previous_mode = audio->RequestedQuality.Mode;
      const auto previous_resampling = audio->MixConfig.resampling;
      const auto error = audio->setQuality(quality);
      if (quality < 0 or quality > 100) {
         AUDIO_CHECK(error IS ERR::OutOfRange);
         AUDIO_CHECK(audio->Quality IS previous and audio->Flags IS previous_flags);
         AUDIO_CHECK(audio->RequestedQuality.Mode IS previous_mode);
         AUDIO_CHECK(audio->MixConfig.resampling IS previous_resampling);
         continue;
      }
      AUDIO_CHECK(error IS ERR::Okay and audio->Quality IS quality);
      AUDIO_CHECK((audio->Flags & glQualityFlags) IS legacy_flags(quality));
      for (int flags : { 0, int(glQualityFlags), int(ADF::AUTO_SAVE | ADF::VOL_RAMPING) }) {
         AUDIO_CHECK(audio->setFlags(ADF(flags)) IS ERR::Okay);
         AUDIO_CHECK((audio->Flags & glQualityFlags) IS legacy_flags(quality));
         AUDIO_CHECK((audio->Flags & ~glQualityFlags) IS (ADF(flags) & ~glQualityFlags));
         std::lock_guard lock(audio->MixerMutex);
         audio->snapshot_quality();
         AUDIO_CHECK(audio->EffectiveQuality.Mode IS audio->RequestedQuality.Mode);
         AUDIO_CHECK(audio->WindowFlags IS audio->Flags);
         AUDIO_CHECK(audio->WindowConfig.resampling IS (quality >= 66));
      }
   }
   // Flags first and Quality first must converge on the same profile, with unrelated flags preserved.
   AUDIO_CHECK(audio->setFlags(ADF::VOL_RAMPING) IS ERR::Okay);
   AUDIO_CHECK(audio->setQuality(10) IS ERR::Okay);
   AUDIO_CHECK(audio->Flags IS (ADF::VOL_RAMPING | ADF::FILTER_LOW));
   AUDIO_CHECK(audio->setFlags(ADF::NIL) IS ERR::Okay);
   AUDIO_CHECK(audio->Flags IS ADF::FILTER_LOW);
   SET_Flags(audio, 0); // Avoid AUTO_SAVE affecting the user's configuration on destruction.
   AUDIO_REQUIRE(InitObject(audio) IS ERR::Okay);
   AUDIO_CHECK(audio->setFlags(ADF::NIL) IS ERR::ImmutableField);
   AUDIO_CHECK(audio->setQuality(100) IS ERR::Okay); // Quality remains writable after initialisation.
}

static void configuration(AudioTestContext &Test)
{
   objConfig::create config = {};
   AUDIO_REQUIRE(config.ok());
   extAudio *audio;
   AUDIO_REQUIRE(NewObject(CLASSID::AUDIO, &audio) IS ERR::Okay);
   std::unique_ptr<extAudio, DeleteObject<extAudio>> owner(audio);
   for (int quality = 0; quality <= 100; ++quality) {
      AUDIO_REQUIRE(audio->setQuality(quality) IS ERR::Okay);
      objFile::create sink = { fl::Size(4096), fl::Flags(FL::BUFFER | FL::READ | FL::WRITE) };
      AUDIO_REQUIRE(sink.ok());
      // Exercise the real persistence action, parse its serialised text, then use the constructor's loading path.
      SET_Flags(audio, 0); // Device-free persistence, without querying the system mixer.
      AUDIO_REQUIRE(audio->saveToObject(*sink) IS ERR::Okay);
      std::span<int8_t> text;
      AUDIO_REQUIRE(sink->getBuffer(text) IS ERR::Okay);
      AUDIO_REQUIRE(config->clear() IS ERR::Okay);
      AUDIO_REQUIRE(config->dataFeed(audio, DATA::TEXT, text.first(size_t(sink->Position))) IS ERR::Okay);
      AUDIO_REQUIRE(audio->setQuality(80) IS ERR::Okay);
      load_audio_quality(audio, *config);
      AUDIO_CHECK(audio->Quality IS quality);
      AUDIO_CHECK((audio->Flags & glQualityFlags) IS legacy_flags(quality));
      AUDIO_CHECK(audio->MixConfig.resampling IS (quality >= 66));
   }
   for (const auto saved : { "-1", "101", "4294967376", "9999999999999999999999", "", "invalid", "80junk" }) {
      AUDIO_REQUIRE(audio->setQuality(80) IS ERR::Okay);
      AUDIO_REQUIRE(config->write("AUDIO", "Quality", saved) IS ERR::Okay);
      load_audio_quality(audio, *config);
      AUDIO_CHECK(audio->Quality IS 80 and (audio->Flags & glQualityFlags) IS legacy_flags(80));
   }
   for (const auto saved : { "0x20", "010", "+66" }) {
      AUDIO_REQUIRE(config->write("AUDIO", "Quality", saved) IS ERR::Okay);
      load_audio_quality(audio, *config);
      AUDIO_CHECK(audio->Quality IS std::strtol(saved, nullptr, 0));
   }
   AUDIO_REQUIRE(audio->setQuality(80) IS ERR::Okay);
   AUDIO_REQUIRE(config->deleteKey("AUDIO", "Quality") IS ERR::Okay);
   load_audio_quality(audio, *config);
   AUDIO_CHECK(audio->Quality IS 80);
}

#ifdef AUDIO_WORKER
// Compare complete renders to the pre-change nearest/linear dispatch and smoothing arithmetic for every valid value.
static void unchanged_output(AudioTestContext &Test)
{
   const int16_t source[] = { 8192, -4096, 16384, 0, -8192, 24576, 4096, -16384, 0 };
   constexpr int frames = 12;
   for (bool stereo : { false, true }) for (bool looped : { false, true })
   for (int quality = 0; quality <= 100; ++quality) {
      extAudio *audio;
      AUDIO_REQUIRE(NewObject(CLASSID::AUDIO, &audio) IS ERR::Okay);
      std::unique_ptr<extAudio, DeleteObject<extAudio>> owner(audio);
      AUDIO_REQUIRE(SET_Quality(audio, quality) IS ERR::Okay);
      AUDIO_REQUIRE(SET_Flags(audio, 0) IS ERR::Okay);
      AUDIO_REQUIRE(InitObject(audio) IS ERR::Okay);
      audio->OutputRate = 1000;
      audio->BitDepth = 32;
      audio->Stereo = stereo;
      audio->MasterVolume = 1;
      audio->Mute = false;
      audio->DriverBitSize = stereo ? 8 : 4;
      audio->MixElements = SAMPLE(5); // Exercise several windows and retained smoothing history.
      audio->MixBuffer.resize(stereo ? 10 : 5);
      audio->Samples.resize(2);
      auto &sample = audio->Samples[1];
      sample.SampleType = PCM::S16_MONO;
      sample.FrameBytes = 2;
      sample.SampleLength = SAMPLE(std::size(source));
      if (looped) {
         sample.LoopMode = LOOP::SINGLE;
         sample.Loop1Type = LTYPE::UNIDIRECTIONAL;
         sample.Loop1Start = SAMPLE(0);
         sample.Loop1End = sample.SampleLength;
      }
      sample.Data.resize(sizeof(source));
      std::memcpy(sample.Data.data(), source, sizeof(source));
      audio->Sets.resize(2);
      audio->Sets[1].Channel.resize(1);
      auto &channel = audio->Sets[1].Channel[0];
      channel.SampleHandle = 1;
      channel.Handle = 1 << 16;
      channel.Frequency = 500;
      channel.State = CHS::PLAYING;
      channel.LVolume = 0.75;
      channel.RVolume = 0.5;
      channel.PositionLow = 16384;

      std::array<float, frames * 2> expected{}, actual{};
      auto dest = expected.data();
      set_mix_step(32768);
      const AudioConfig legacy(stereo, quality >= 66);
      // The legacy no-loop path passes a zero neighbour offset even before the source end.
      const MixingParams params = { (APTR)source, 16384, frames, looped ? 1 : 0, 0.75f, 0.5f, &dest };
      AudioMixer::dispatch_mix(legacy, PCM::S16_MONO, params);
      const auto flags = legacy_flags(quality);
      for (int c = 0; c < (stereo ? 2 : 1); ++c) {
         float d1 = 0, d2 = 0;
         for (int i = 0; i < frames; ++i) {
            auto &value = expected[i * (stereo ? 2 : 1) + c];
            const float input = value;
            if ((flags & ADF::FILTER_LOW) != ADF::NIL) {
               value = stereo ? (d1 + 2.0 * input) * (1.0 / 3.0) :
                  (d1 + 2.0f * input) * (1.0f / 3.0f);
               d1 = input;
            }
            else if ((flags & ADF::FILTER_HIGH) != ADF::NIL) {
               value = stereo ? (d1 + 3.0 * d2 + 4.0 * input) * (1.0 / 8.0) :
                  (d1 + 3.0f * d2 + 4.0f * input) * (1.0f / 8.0f);
               d1 = d2;
               d2 = input;
            }
         }
      }
      AUDIO_REQUIRE(mix_data(audio, frames, actual.data()) IS ERR::Okay);
      AUDIO_CHECK(std::memcmp(actual.data(), expected.data(), frames * (stereo ? 2 : 1) * sizeof(float)) IS 0);
      AUDIO_CHECK(audio->EffectiveQuality.Mode IS audio->RequestedQuality.Mode);
      AUDIO_CHECK(audio->WindowConfig.resampling IS (quality >= 66));
      AUDIO_CHECK(channel.Position IS 6 and channel.PositionLow IS 16384);
   }
}

// A processor writes Quality between source mixing and smoothing: that write must affect only the next window.
class LiveQualityWrite final : public AudioEffectProcessor {
   extAudio *audio;
public:
   explicit LiveQualityWrite(extAudio *Audio) : audio(Audio) { }
   void reset() override { }
   AudioTail tail() const override { return AudioTail::INDEFINITE; }
   bool pending() const override { return true; }
   void process(float *Buffer, int Frames) override {
      SET_Quality(audio, 100);
      std::fill_n(Buffer, Frames, 0.25f);
   }
};

static void live_write(AudioTestContext &Test)
{
   extAudio *audio;
   AUDIO_REQUIRE(NewObject(CLASSID::AUDIO, &audio) IS ERR::Okay);
   std::unique_ptr<extAudio, DeleteObject<extAudio>> owner(audio);
   SET_Quality(audio, 0);
   SET_Flags(audio, 0);
   AUDIO_REQUIRE(InitObject(audio) IS ERR::Okay);
   audio->OutputRate = 1000;
   audio->BitDepth = 32;
   audio->Stereo = false;
   audio->DriverBitSize = 4;
   audio->MixElements = SAMPLE(3);
   audio->MixBuffer.resize(3);
   extAudioEffect effect(nullptr, 0);
   effect.Chain = audio->GlobalEffects;
   effect.OutputRate = audio->GlobalEffects->Rate = 1000;
   audio->GlobalEffects->Layout = { int(SPK::CENTRE) };
   effect.set_layout(glLayoutMono);
   audio->GlobalEffects->Effects.push_back(&effect);
   AUDIO_REQUIRE(effect.set_processor(std::make_unique<LiveQualityWrite>(audio)) IS ERR::Okay);
   effect.idle(); // Complete the initial reset so the synthetic source can report pending output.
   std::array<float, 6> output{};
   const std::array<float, 6> expected = { 0.25f, 0.25f, 0.25f, 0.125f, 0.21875f, 0.25f };
   std::lock_guard lock(audio->MixerMutex);
   AUDIO_REQUIRE(mix_data(audio, output.size(), output.data()) IS ERR::Okay);
   AUDIO_CHECK(output IS expected);
   AUDIO_CHECK(audio->Quality IS 100 and audio->WindowConfig.resampling);
   AUDIO_CHECK(audio->EffectiveQuality.Mode IS AudioQualityMode::HIGH);
   audio->GlobalEffects->Effects.clear();
}

#endif

static void window_boundary(AudioTestContext &Test)
{
   extAudio *audio;
   AUDIO_REQUIRE(NewObject(CLASSID::AUDIO, &audio) IS ERR::Okay);
   std::unique_ptr<extAudio, DeleteObject<extAudio>> owner(audio);
   SET_Quality(audio, 0);
   {
      std::lock_guard lock(audio->MixerMutex);
      audio->snapshot_quality();
      // A recursive write (e.g. from a callback) cannot alter the current window's processing state.
      AUDIO_CHECK(audio->setQuality(100) IS ERR::Okay);
      AUDIO_CHECK(audio->RequestedQuality.Mode IS AudioQualityMode::HIGH);
      AUDIO_CHECK(audio->EffectiveQuality.Mode IS AudioQualityMode::DRAFT);
      AUDIO_CHECK(!audio->WindowConfig.resampling and (audio->WindowFlags & glQualityFlags) IS ADF::NIL);
   }
   std::atomic<bool> started{false};
   std::future<ERR> write;
   {
      std::lock_guard lock(audio->MixerMutex);
      audio->snapshot_quality();
      write = std::async(std::launch::async, [&] {
         started.store(true, std::memory_order_release);
         return SET_Quality(audio, 10);
      });
      while (!started.load(std::memory_order_acquire)) std::this_thread::yield();
      AUDIO_CHECK(write.wait_for(std::chrono::milliseconds(10)) IS std::future_status::timeout);
      AUDIO_CHECK(audio->EffectiveQuality.Mode IS AudioQualityMode::HIGH and audio->WindowConfig.resampling);
   }
   AUDIO_CHECK(write.get() IS ERR::Okay);
   std::lock_guard lock(audio->MixerMutex);
   audio->snapshot_quality();
   AUDIO_CHECK(audio->EffectiveQuality.Mode IS AudioQualityMode::LOW and !audio->WindowConfig.resampling);
   AUDIO_CHECK((audio->WindowFlags & glQualityFlags) IS ADF::FILTER_LOW);
}

static void run(AudioTestContext &Test)
{
   boundaries(Test);
   configuration(Test);
#ifdef AUDIO_WORKER
   unchanged_output(Test);
   live_write(Test);
#endif
   window_boundary(Test);
}

} // namespace audio_tests_audio_quality
